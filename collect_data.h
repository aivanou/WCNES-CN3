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

    typedef struct _node {
        rimeaddr_t sender;
        uint8_t total_packets;
        uint8_t lost_packets;
        uint8_t last_packet_id;
        uint16_t lqi;
        uint16_t rssi;
        uint16_t etx;
        uint32_t etx_accumulator;
        struct _node* next;
    } datacom_neighbor_t;

    typedef struct {
        rimeaddr_t from;
        rimeaddr_t to;
        uint8_t total_packets;
        uint8_t lost_packets;
        uint16_t lqi;
        uint16_t rssi;
        uint16_t etx;
        uint32_t etx_accumulator;
    } datacom_packet_lost_inf_t;


    static const char* broadcast_tag = "bdc_m\0";
    static const int broadcast_tag_length = 5;

    static const char* sink_packet_tag = "splt";
    static const int sink_tag_length = 4;

    static void send_packets_to_sink();
    static void clean_list();

    static datacom_neighbor_t* get_neighbor_by_rimeaddr(rimeaddr_t addr);
    static void add_or_update_neighbor(rimeaddr_t addr, uint8_t total_packets, uint8_t lost_packets,
            uint8_t rmes_id, uint16_t lqi, uint16_t rssi, uint16_t etx, uint32_t etx_accumulator);

    static datacom_neighbor_t init_neighbor(rimeaddr_t addr, uint8_t total_packets, uint8_t lost_packets,
            uint8_t rmes_id, uint16_t lqi, uint16_t rssi, uint16_t etx, uint32_t etx_accumulator);

    static void print_n();


#ifdef	__cplusplus
}
#endif

#endif	/* COLLECT_DATA_H */

