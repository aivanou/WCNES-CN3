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

// PA_LEVEL, 7 = -15dBm
#define DEFAULT_TX_POWER 7


static int SINK_LOW_VALUE = 1;
static int SINK_HIGH_VALUE = 0;
static const int MAX_NEIGHBORS = 8;

static struct collect_conn tc;
static struct broadcast_conn bc;

static datacom_neighbor_t neighbor_list[8];
static int nlist_count = 0;

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
        printf("received pakcets\n");
        for (i = 0; i < npackets; ++i) {
            printf("%d.%d   %d.%d  %d %d %d  %d   %d  %d  %d  \n ",
                   p[i].from.u8[0], p[i].from.u8[1],
                   p[i].to.u8[0], p[i].to.u8[1],
                   p[i].total_packets, p[i].lost_packets,
                   p[i].lqi, p[i].rssi, p[i].etx, p[i].etx_accumulator, hops);
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

    //    printf("%d.%d:  broadcast from %d.%d  to me, message id: %d \n",
    //           rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
    //           from->u8[0], from->u8[1], rmes_id);

    uint16_t lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
    uint16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    uint16_t etx = 0;
    uint32_t etx_accumulator = 0;

    struct collect_neighbor* cn = collect_neighbor_list_find(&tc.neighbor_list, from);
    if (cn != NULL) {
        etx = cn->rtmetric;
        etx_accumulator = cn->le.etx_accumulator;
        printf("my rtmetric:  %d   etx_accumulator: %d  with link %d.%d\n", etx, etx_accumulator, cn->addr.u8[0], cn->addr.u8[1]);
    }

    datacom_neighbor_t* n = get_neighbor_by_rimeaddr(*from);
    if (n == NULL) {
        add_or_update_neighbor(*from, 0, 0, rmes_id, lqi, rssi, etx, etx_accumulator);
    }
    else if (n->last_packet_id < rmes_id) {
        n->lost_packets += rmes_id - n->last_packet_id - 1;
        //        printf("%d.%d total packets: %d  lost packets: %d  %d %d\n", n->sender.u8[0], n->sender.u8[1],
        //               n->total_packets, n->lost_packets, rmes_id, n->last_packet_id);
        n->total_packets = rmes_id;
        add_or_update_neighbor(n->sender, n->total_packets, n->lost_packets, rmes_id, lqi, rssi, etx, etx_accumulator);
    }
    else {
        add_or_update_neighbor(n->sender, 0, 0, rmes_id, lqi, rssi, etx, etx_accumulator);
    }

    //    print_n();


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

    // Set TX power
    //    cc2420_set_txpower(DEFAULT_TX_POWER);

    //    printf("%d.%d: packetbuf txpower=%d\n", 
    //        rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
    //        packetbuf_attr(PACKETBUF_ATTR_RADIO_TXPOWER));


    collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);
    if (rimeaddr_node_addr.u8[0] == SINK_LOW_VALUE &&
        rimeaddr_node_addr.u8[1] == SINK_HIGH_VALUE) {
        printf("I am sink ID: %d.%d\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
        collect_set_sink(&tc, 1);
    }
    else {
        printf("My address: %d.%d\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    }

    broadcast_open(&bc, 129, &broactcast_calls);

    /* Allow some time for the network to settle. */
    etimer_set(&et, 5 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    if (rimeaddr_node_addr.u8[0] == SINK_LOW_VALUE &&
        rimeaddr_node_addr.u8[1] == SINK_HIGH_VALUE) {
        //do something
    }
    else {
        while (1) {

            current_message_id += 1;
            etimer_set(&et, 5 * CLOCK_SECOND);
            PROCESS_WAIT_UNTIL(etimer_expired(&et));

            packetbuf_clear();
            void* addr = packetbuf_dataptr();
            memcpy(addr, broadcast_tag, strlen(broadcast_tag));
            memcpy(addr + strlen(broadcast_tag), &current_message_id, sizeof (uint8_t));
            packetbuf_set_datalen(strlen(broadcast_tag) + sizeof (uint8_t));

            broadcast_send(&bc);
            max_total_packet += 1;

            //        printf("m number :%d \n", max_total_packet);
            if (max_total_packet >= MAX_REQUESTS_SEND) {
                send_packets_to_sink();
                max_total_packet = 0;
                clean_list();
            }

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
    memcpy(addr + curr_packet_length, (void*) &nlist_count, sizeof (uint8_t));
    curr_packet_length += sizeof (uint8_t);

    for (i = 0; i < nlist_count; ++i) {

        datacom_neighbor_t curr_el = neighbor_list[i];
        datacom_packet_lost_inf_t p;
        p.from = curr_el.sender;
        p.to = rimeaddr_node_addr;
        p.lost_packets = curr_el.lost_packets;
        p.total_packets = curr_el.total_packets;
        p.lqi = curr_el.lqi;
        p.rssi = curr_el.rssi;
        p.etx = curr_el.etx;
        p.etx_accumulator = curr_el.etx_accumulator;
        printf("from: %d.%d to %d.%d %d %d  ;", p.from.u8[0], p.from.u8[1], p.to.u8[0], p.to.u8[1], p.lqi, p.rssi);
        memcpy(addr + curr_packet_length, (void*) &p, sizeof (datacom_packet_lost_inf_t));
        curr_packet_length += sizeof (datacom_packet_lost_inf_t);
    }
    printf("\n");
    packetbuf_set_datalen(curr_packet_length);

    collect_send(&tc, 4);

}

static void
add_or_update_neighbor(rimeaddr_t addr, uint8_t total_packets,
                       uint8_t lost_packets, uint8_t rmes_id,
                       uint16_t lqi, uint16_t rssi, uint16_t etx, uint32_t etx_accumulator)
{
    if (nlist_count == 0) {
        datacom_neighbor_t neighbor = init_neighbor(addr, total_packets, lost_packets, rmes_id, lqi, rssi, etx, etx_accumulator);
        neighbor_list[nlist_count] = neighbor;
        nlist_count += 1;
        return;
    }

    if (nlist_count > 8)
        return;
    int i;
    for (i = 0; i < nlist_count; ++i) {
        if (rimeaddr_cmp(&neighbor_list[i].sender, &addr)) {
            neighbor_list[i].lost_packets = lost_packets;
            neighbor_list[i].total_packets = total_packets;
            neighbor_list[i].last_packet_id = rmes_id;
            neighbor_list[i].lqi = lqi;
            neighbor_list[i].rssi = rssi;
            neighbor_list[i].etx = etx;
            neighbor_list[i].etx_accumulator = etx_accumulator;
            return;
        }
    }

    datacom_neighbor_t neighbor = init_neighbor(addr, total_packets, lost_packets, rmes_id, lqi, rssi, etx, etx_accumulator);
    neighbor_list[nlist_count] = neighbor;
    nlist_count += 1;
}

static datacom_neighbor_t
init_neighbor(rimeaddr_t addr, uint8_t total_packets,
              uint8_t lost_packets, uint8_t rmes_id,
              uint16_t lqi, uint16_t rssi, uint16_t etx, uint32_t etx_accumulator)
{
    datacom_neighbor_t n;
    n.total_packets = total_packets;
    n.lost_packets = lost_packets;
    n.last_packet_id = rmes_id;
    n.lqi = lqi;
    n.rssi = rssi;
    n.etx = etx;
    n.etx_accumulator = etx_accumulator;
    n.next = NULL;
    n.sender = addr;
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
    nlist_count = 0;
}

