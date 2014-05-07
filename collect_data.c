/*
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "net/rime/collect-neighbor.h"
#include "collect_data.h"
#include "net/netstack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PACKETS 4
#define MAX_PACKET_LOST 5

static struct collect_conn tc;
static struct broadcast_conn bc;

static packet_stats_t packets[MAX_PACKETS];
static datacom_neighbor_t* neighbor_list = NULL;

static uint8_t packets_received = 0;
static uint8_t current_message_id = 0;

static uint8_t max_total_packet = 0;

static char* broadcast_tag = "bdc_m\0";
static int broadcast_tag_length = 5;



/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Collect information between nodes");
AUTOSTART_PROCESSES(&example_collect_process);

/*---------------------------------------------------------------------------*/
static void
recv(const rimeaddr_t *originator, uint8_t seqno, uint8_t hops)
{

    void* addr = packetbuf_dataptr();
    uint8_t mrp = *((uint8_t*) addr);
    packet_stats_t* t = (packet_stats_t*) malloc(sizeof (packet_stats_t) * mrp);
    printf("Sink received: %d packets \n", mrp);
    int i;
    for (i = 0; i < mrp; ++i) {
        packet_stats_t tmp;
        memcpy(&tmp, addr + sizeof (uint8_t) + sizeof (packet_stats_t) * i, sizeof (packet_stats_t));
        t[i] = tmp;
    }

    for (i = 0; i < mrp; ++i) {
        printf("received from %d.%d  data: : from %d.%d  to %d.%d  { %d  %d  }\n",
               originator->u8[0], originator->u8[1],
               t[i].from.u8[0], t[i].from.u8[1],
               t[i].to.u8[0], t[i].to.u8[1], t[i].lqi, t[i].rssi);
    }

    free(t);
}

static void
broadcast_receive(struct broadcast_conn* c, const rimeaddr_t* from)
{
    uint8_t rmes_id = 0;
    if (strncmp((char*) packetbuf_dataptr(), broadcast_tag, strlen(broadcast_tag)) == 0) {
        rmes_id = ((uint8_t*) packetbuf_dataptr() + strlen(broadcast_tag))[0];
    }

    printf("%d.%d:  broadcast from %d.%d  to me, message id: %d \n",
           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
           from->u8[0], from->u8[1], rmes_id);

    datacom_neighbor_t* n = get_neighbor_by_rimeaddr(*from);
    if (n == NULL) {
        add_or_update_neighbor(*from, 0, 0, rmes_id);
    }
    else if (n->last_packet_id < rmes_id) {
        n->lost_packets += rmes_id - n->last_packet_id - 1;
        printf("%d.%d total packets: %d  lost packets: %d  %d %d\n", n->sender.u8[0], n->sender.u8[1],
               n->total_packets, n->lost_packets, rmes_id, n->last_packet_id);
        n->total_packets = rmes_id;
        add_or_update_neighbor(n->sender, n->total_packets, n->lost_packets, rmes_id);
    }
    else {
        add_or_update_neighbor(n->sender, 0, 0, rmes_id);
    }

    print_n();

    packet_stats_t packet;
    packet.from = *from;
    packet.to = rimeaddr_node_addr;
    packet.rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    packet.lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
    if (packets_received < MAX_PACKETS) {
        packets[packets_received] = packet;
        packets_received += 1;
    }

}

/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = {recv};
static const struct broadcast_callbacks broactcast_calls = {broadcast_receive};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_collect_process, ev, data)
{
    static struct etimer periodic;
    static struct etimer et;
    PROCESS_EXITHANDLER(broadcast_close(&bc);)

    PROCESS_BEGIN();

    collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);
    if (rimeaddr_node_addr.u8[0] == 1 &&
            rimeaddr_node_addr.u8[1] == 0) {
        printf("I am sink\n");
        collect_set_sink(&tc, 1);
    }

    broadcast_open(&bc, 129, &broactcast_calls);

    /* Allow some time for the network to settle. */
    etimer_set(&et, 40 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    while (1) {

        current_message_id += 1;
        etimer_set(&et, 15 * CLOCK_SECOND);
        PROCESS_WAIT_UNTIL(etimer_expired(&et));

        packetbuf_clear();
        void* addr = packetbuf_dataptr();
        memcpy(addr, broadcast_tag, strlen(broadcast_tag));
        memcpy(addr + strlen(broadcast_tag), &current_message_id, sizeof (uint8_t));
        packetbuf_set_datalen(strlen(broadcast_tag) + sizeof (uint8_t));

        broadcast_send(&bc);

        if (packets_received >= MAX_PACKETS) {
            send_packets_to_sink();
            packets_received = 0;

        }

    }

    broadcast_close(&bc);

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

static void
send_packets_to_sink()
{
    static rimeaddr_t oldparent;
    const rimeaddr_t *parent;

    printf("%d.%d:  sending data to the sink\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    if (packets_received <= 0)
        return;

    packetbuf_clear();

    void* addr = packetbuf_dataptr();
    memcpy(addr, (void*) &packets_received, sizeof (uint8_t));
    int i;
    for (i = 0; i < packets_received; ++i) {
        memcpy(addr + sizeof (uint8_t) + i * sizeof (packet_stats_t), (void*) &packets[i], sizeof (packet_stats_t));
    }
    packetbuf_set_datalen(sizeof (uint8_t) + sizeof (packet_stats_t) * packets_received);

    collect_send(&tc, 10);

    parent = collect_parent(&tc);
    if (!rimeaddr_cmp(parent, &oldparent)) {
        if (!rimeaddr_cmp(&oldparent, &rimeaddr_null)) {
            printf("#L %d 0\n", oldparent.u8[0]);
        }
        if (!rimeaddr_cmp(parent, &rimeaddr_null)) {
            printf("#L %d 1\n", parent->u8[0]);
        }
        rimeaddr_copy(&oldparent, parent);
    }

}

static void
add_or_update_neighbor(rimeaddr_t addr, uint8_t total_packets, uint8_t lost_packets, uint8_t rmes_id)
{
    datacom_neighbor_t* n = (datacom_neighbor_t*) malloc(sizeof (datacom_neighbor_t));
    n->total_packets = total_packets;
    n->lost_packets = lost_packets;
    n->last_packet_id = rmes_id;
    n->next = NULL;
    n->sender = addr;

    datacom_neighbor_t* curr_element = neighbor_list;
    datacom_neighbor_t* prev_element = NULL;

    if (curr_element == NULL) {
        neighbor_list = n;
        neighbor_list->next = NULL;
        return;
    }

    while (curr_element != NULL) {
        if (rimeaddr_cmp(&curr_element->sender, &addr)) {
            curr_element->lost_packets = lost_packets;
            curr_element->total_packets = total_packets;
            curr_element->last_packet_id = rmes_id;
            return;
        }
        prev_element = curr_element;
        curr_element = curr_element->next;
    }

    prev_element->next = n;
    prev_element->next->next = NULL;
    n->next = NULL;
}

static datacom_neighbor_t*
get_neighbor_by_rimeaddr(rimeaddr_t addr)
{
    datacom_neighbor_t* head = neighbor_list;
    while (head != NULL) {
        if (rimeaddr_cmp(&head->sender, &addr)) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

static void
print_n()
{
    datacom_neighbor_t* c = neighbor_list;
    printf("nodes:  ");
    while (c != NULL) {
        printf("%d.%d   ", c->sender.u8[0], c->sender.u8[1]);
        c = c->next;
    }
    printf("\n");
}

