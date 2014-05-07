/* 
 * File:   collect_data.h
 * Author: tierex
 *
 * Created on May 7, 2014, 6:44 PM
 */

#ifndef COLLECT_DATA_H
#define	COLLECT_DATA_H

#ifdef	__cplusplus
extern "C" {
#endif

    typedef struct {
        rimeaddr_t from;
        rimeaddr_t to;
        uint16_t lqi;
        uint16_t rssi;
    } packet_stats_t;

    typedef struct _node {
        rimeaddr_t sender;
        uint8_t total_packets;
        uint8_t lost_packets;
        uint8_t last_packet_id;
        struct _node* next;
    } datacom_neighbor_t;


    static void send_packets_to_sink();

    static datacom_neighbor_t* get_neighbor_by_rimeaddr(rimeaddr_t addr);
    static void add_or_update_neighbor(rimeaddr_t addr, uint8_t total_packets, uint8_t lost_packets, uint8_t rmes_id);
    static void print_n();


#ifdef	__cplusplus
}
#endif

#endif	/* COLLECT_DATA_H */

