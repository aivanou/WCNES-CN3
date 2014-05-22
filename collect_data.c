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

#define MAX_REQUESTS_SEND 4

static struct collect_conn tc;
static struct broadcast_conn bc;

static datacom_neighbor_t* neighbor_list = NULL;
static uint8_t neighbor_list_size = 0;

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
    if (strncmp((char*) addr, sink_packet_tag, sink_tag_length) == 0) {
        uint8_t npackets = *((uint8_t*) (addr + current_offset));
        current_offset += sizeof (uint8_t);

        datacom_packet_lost_inf_t* p = (datacom_packet_lost_inf_t*) malloc(sizeof (datacom_packet_lost_inf_t) * npackets);
        printf("Sink received %d packets\n", npackets);
        for (i = 0; i < npackets; ++i) {
            memcpy(&p[i], addr + current_offset, sizeof (datacom_packet_lost_inf_t));
            current_offset += sizeof (datacom_packet_lost_inf_t);
        }

        for (i = 0; i < npackets; ++i) {
            printf("packet information : from %d.%d  to %d.%d  total:  %d  , lost: %d , {%d  %d} hops: %d  \n ",
                   p[i].from.u8[0], p[i].from.u8[1],
                   p[i].to.u8[0], p[i].to.u8[1],
                   p[i].total_packets, p[i].lost_packets,
                   p[i].lqi, p[i].rssi, hops);
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

    uint16_t lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
    uint16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    uint16_t etx = 0;

    struct collect_neighbor* cn = collect_neighbor_list_find(&tc.neighbor_list, from);
    if (cn != NULL) {
        etx = cn->rtmetric;
        printf("etx metric from %d.%d  is %d \n", from->u8[0], from->u8[1], cn->rtmetric);
    }

    datacom_neighbor_t* n = get_neighbor_by_rimeaddr(*from);
    if (n == NULL) {
        add_or_update_neighbor(*from, 0, 0, rmes_id, lqi, rssi, etx);
    }
    else if (n->last_packet_id < rmes_id) {
        n->lost_packets += rmes_id - n->last_packet_id - 1;
        printf("%d.%d total packets: %d  lost packets: %d  %d %d\n", n->sender.u8[0], n->sender.u8[1],
               n->total_packets, n->lost_packets, rmes_id, n->last_packet_id);
        n->total_packets = rmes_id;
        add_or_update_neighbor(n->sender, n->total_packets, n->lost_packets, rmes_id, lqi, rssi, etx);
    }
    else {
        add_or_update_neighbor(n->sender, 0, 0, rmes_id, lqi, rssi, etx);
    }

    print_n();


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

        printf("m number :%d \n", max_total_packet);
        if (max_total_packet >= MAX_REQUESTS_SEND) {
            send_packets_to_sink();
            max_total_packet = 0;
            //clean_list();
        }

    }

    broadcast_close(&bc);

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

static void
send_packets_to_sink()
{
    printf("sending packet lost information to the sink \n");
    int i;
    uint8_t curr_packet_length = 0;

    packetbuf_clear();
    void* addr = packetbuf_dataptr();

    memcpy(addr + curr_packet_length, (void*) sink_packet_tag, sink_tag_length);
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
        p.lqi = curr_el->lqi;
        p.rssi = curr_el->rssi;
        memcpy(addr + curr_packet_length, (void*) &p, sizeof (datacom_packet_lost_inf_t));
        curr_packet_length += sizeof (datacom_packet_lost_inf_t);
        curr_el = curr_el->next;
    }
    packetbuf_set_datalen(curr_packet_length);

    collect_send(&tc, 15);

}

static void
add_or_update_neighbor(rimeaddr_t addr, uint8_t total_packets,
                       uint8_t lost_packets, uint8_t rmes_id,
                       uint16_t lqi, uint16_t rssi, uint16_t etx)
{
    datacom_neighbor_t* curr_element = neighbor_list;
    datacom_neighbor_t* prev_element = NULL;

    if (curr_element == NULL) {
        neighbor_list = init_neighbor(addr, total_packets, lost_packets, rmes_id, lqi, rssi, etx);
        neighbor_list->next = NULL;
        neighbor_list_size += 1;
        return;
    }

    while (curr_element != NULL) {
        if (rimeaddr_cmp(&curr_element->sender, &addr)) {
            curr_element->lost_packets = lost_packets;
            curr_element->total_packets = total_packets;
            curr_element->last_packet_id = rmes_id;
            curr_element->lqi = lqi;
            curr_element->rssi = rssi;
            return;
        }
        prev_element = curr_element;
        curr_element = curr_element->next;
    }

    prev_element->next = init_neighbor(addr, total_packets, lost_packets, rmes_id, lqi, rssi, etx);
    prev_element->next->next = NULL;
    neighbor_list_size += 1;
}

static datacom_neighbor_t*
init_neighbor(rimeaddr_t addr, uint8_t total_packets,
              uint8_t lost_packets, uint8_t rmes_id,
              uint16_t lqi, uint16_t rssi, uint16_t etx)
{
    datacom_neighbor_t* n = (datacom_neighbor_t*) malloc(sizeof (datacom_neighbor_t));
    n->total_packets = total_packets;
    n->lost_packets = lost_packets;
    n->last_packet_id = rmes_id;
    n->lqi = lqi;
    n->rssi = rssi;
    n->etx = etx;
    n->next = NULL;
    n->sender = addr;
    return n;
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

static void
clean_list()
{
    if (neighbor_list == NULL)
        return;
    datacom_neighbor_t* curr_element = neighbor_list;
    datacom_neighbor_t* prev_element = NULL;
    while (curr_element != NULL) {
        prev_element = curr_element;
        free(prev_element);
        curr_element = curr_element->next;
    }
    neighbor_list = NULL;
}

