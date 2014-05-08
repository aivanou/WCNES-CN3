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
#define MAX_PACKET_LOST 20

static struct collect_conn tc;
static struct broadcast_conn bc;

static packet_stats_t packets[MAX_PACKETS];
static datacom_neighbor_t* neighbor_list = NULL;
static uint8_t neighbor_list_size = 0;

static uint8_t packets_received = 0;
static uint8_t current_message_id = 0;

static uint8_t max_total_packet = 0;

/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Collect information between nodes");
AUTOSTART_PROCESSES(&example_collect_process);

/*---------------------------------------------------------------------------*/
static void
recv(const rimeaddr_t *originator, uint8_t seqno, uint8_t hops)
{

    int i;
    void* addr = packetbuf_dataptr();
    uint8_t current_offset = sink_tag_length;
    printf("AAA %s \n", (char*) addr);
    if (strncmp((char*) addr, sink_packet_tag, sink_tag_length) == 0) {
        uint8_t mrp = *((uint8_t*) (addr + current_offset));
        current_offset += sizeof (uint8_t);

        packet_stats_t* t = (packet_stats_t*) malloc(sizeof (packet_stats_t) * mrp);
        printf("Sink received: %d packets \n", mrp);
        for (i = 0; i < mrp; ++i) {
            packet_stats_t tmp;
            memcpy(&tmp, addr + current_offset, sizeof (packet_stats_t));
            t[i] = tmp;
            current_offset += sizeof (packet_stats_t);
        }

        for (i = 0; i < mrp; ++i) {
            printf("received from %d.%d  data: : from %d.%d  to %d.%d  { %d  %d  }\n",
                   originator->u8[0], originator->u8[1],
                   t[i].from.u8[0], t[i].from.u8[1],
                   t[i].to.u8[0], t[i].to.u8[1], t[i].lqi, t[i].rssi);
        }

        free(t);
    }
    else if (strncmp((char*) addr, sink_packet_lost_tag, sink_tag_length) == 0) {
        uint8_t npackets = *((uint8_t*) (addr + current_offset));
        current_offset += sizeof (uint8_t);

        datacom_packet_lost_inf_t* p = (datacom_packet_lost_inf_t*) malloc(sizeof (datacom_packet_lost_inf_t) * npackets);
        printf("Sink received %d lost packet information \n", npackets);
        for (i = 0; i < npackets; ++i) {
            memcpy(&p[i], addr + current_offset, sizeof (datacom_packet_lost_inf_t));
            current_offset += sizeof (datacom_packet_lost_inf_t);
        }

        for (i = 0; i < npackets; ++i) {
            printf("packet lost information : from %d.%d  to %d.%d  total:  %d  , lost: %d \n ",
                   p[i].from.u8[0], p[i].from.u8[1],
                   p[i].to.u8[0], p[i].to.u8[1],
                   p[i].total_packets, p[i].lost_packets);
        }

        free(p);
    }
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
        max_total_packet += 1;

        if (packets_received >= MAX_PACKETS) {
            send_packets_to_sink();
            packets_received = 0;

        }

        if (max_total_packet >= MAX_PACKET_LOST) {
            send_packet_lost_inf_to_sink();
            max_total_packet = 0;
        }

    }

    broadcast_close(&bc);

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

static void
send_packet_lost_inf_to_sink()
{
    printf("sending packet lost information to the sink \n");
    int i;
    uint8_t curr_packet_length = 0;

    packetbuf_clear();
    void* addr = packetbuf_dataptr();

    memcpy(addr + curr_packet_length, (void*) sink_packet_lost_tag, sink_tag_length);
    curr_packet_length += sink_tag_length;
    memcpy(addr + curr_packet_length, (void*) &neighbor_list_size, sizeof (uint8_t));
    curr_packet_length += sizeof (uint8_t);

    datacom_neighbor_t* curr_el = neighbor_list;
    for (i = 0; i < neighbor_list_size; ++i) {
        datacom_packet_lost_inf_t p;
        p.from = curr_el->sender;
        p.to = rimeaddr_node_addr;
        p.lost_packets = curr_el->lost_packets;
        p.total_packets = curr_el->total_packets;
        memcpy(addr + curr_packet_length, (void*) &p, sizeof (datacom_packet_lost_inf_t));
        curr_packet_length += sizeof (datacom_packet_lost_inf_t);
    }
    packetbuf_set_datalen(curr_packet_length);

    collect_send(&tc, 15);

}

static void
send_packets_to_sink()
{
    static rimeaddr_t oldparent;
    const rimeaddr_t *parent;
    uint8_t curr_packet_length = 0;

    printf("%d.%d:  sending data to the sink\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    if (packets_received <= 0)
        return;

    packetbuf_clear();

    void* addr = packetbuf_dataptr();

    memcpy(addr + curr_packet_length, (void*) sink_packet_tag, sink_tag_length);
    curr_packet_length += sink_tag_length;
    memcpy(addr + curr_packet_length, (void*) &packets_received, sizeof (uint8_t));
    curr_packet_length += sizeof (uint8_t);

    int i;
    for (i = 0; i < packets_received; ++i) {
        memcpy(addr + curr_packet_length, (void*) &packets[i], sizeof (packet_stats_t));
        curr_packet_length += sizeof (packet_stats_t);
    }
    packetbuf_set_datalen(sink_tag_length + sizeof (uint8_t) + sizeof (packet_stats_t) * packets_received);

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
        neighbor_list_size += 1;
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
    neighbor_list_size += 1;
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

