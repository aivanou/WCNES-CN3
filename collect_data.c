/*
 */

#include "contiki.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"

#include "net/netstack.h"

#include <stdio.h>
#include <stdlib.h>

static struct collect_conn tc;

typedef struct {
    rimeaddr_t from;
    rimeaddr_t to;
    uint16_t lqi;
    uint16_t rssi;
} packet_stats_t;

static packet_stats_t packets[128];
static int max_packets = 0;

/*---------------------------------------------------------------------------*/
PROCESS(example_collect_process, "Collect information between nodes");
AUTOSTART_PROCESSES(&example_collect_process);

/*---------------------------------------------------------------------------*/
static void
recv(const rimeaddr_t *originator, uint8_t seqno, uint8_t hops)
{


    void* addr = packetbuf_dataptr();
    int mrp = *((int*) addr);
    packet_stats_t* t = (packet_stats_t*) malloc(sizeof (packet_stats_t) * mrp);
    printf("Sink received: %d packets \n", mrp);
    int i;
    for (i = 0; i < mrp; ++i) {
        packet_stats_t tmp;
        memcpy(&tmp, addr + sizeof (int) + sizeof (packet_stats_t) * i, sizeof (packet_stats_t));
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
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
    printf("broadcast from %d.%d  to %d.%d\n",
           from->u8[0], from->u8[1], rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

    packet_stats_t packet;
    packet.from = rimeaddr_node_addr;
    packet.to = *from;
    packet.rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    packet.lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
    if (max_packets < 128) {
        packets[max_packets] = packet;
        max_packets += 1;
    }
}

/*---------------------------------------------------------------------------*/
static const struct collect_callbacks callbacks = {recv};
static const struct collect_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_collect_process, ev, data)
{
    static struct etimer periodic;
    static struct etimer et;
    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

    PROCESS_BEGIN();

    collect_open(&tc, 130, COLLECT_ROUTER, &callbacks);
    if (rimeaddr_node_addr.u8[0] == 1 &&
            rimeaddr_node_addr.u8[1] == 0) {
        printf("I am sink\n");
        collect_set_sink(&tc, 1);
    }

    broadcast_open(&broadcast, 129, &broadcast_call);

    etimer_set(&et, 10 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("test_message", 12);
    broadcast_send(&broadcast);

    broadcast_close(&broadcast);


    /* Allow some time for the network to settle. */
    etimer_set(&et, 40 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&et));

    //    while (1) {

    /* Send a packet every 30 seconds. */
    if (etimer_expired(&periodic)) {
        etimer_set(&periodic, CLOCK_SECOND * 10);
        etimer_set(&et, random_rand() % (CLOCK_SECOND * 10));
    }

    PROCESS_WAIT_EVENT();


    if (etimer_expired(&et)) {
        static rimeaddr_t oldparent;
        const rimeaddr_t *parent;

        printf("Sending\n");
        packetbuf_clear();

        void* addr = (void*) malloc(sizeof (int) + sizeof (packet_stats_t) * max_packets);
        memcpy(addr, (void*) &max_packets, sizeof (int));
        int i;
        for (i = 0; i < max_packets; ++i) {
            memcpy(addr + sizeof (int) +i * sizeof (packet_stats_t), (void*) &packets[i], sizeof (packet_stats_t));
        }
        packetbuf_copyfrom(addr, sizeof (int) + sizeof (packet_stats_t) * max_packets);

        collect_send(&tc, 15);

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

    //    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
