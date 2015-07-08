/*
 * dlep_reader.h
 *
 *  Created on: Jun 30, 2015
 *      Author: rogge
 */

#ifndef _DLEP_READER_H_
#define _DLEP_READER_H_

#include "common/common_types.h"
#include "common/netaddr.h"

#include "subsystems/oonf_layer2.h"

#include "dlep/dlep_extension.h"

int dlep_reader_heartbeat_tlv(uint64_t *interval,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_peer_type(char *text, size_t text_length,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_mac_tlv(struct netaddr *mac,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_ipv4_tlv(struct netaddr *ipv4, bool *add,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_ipv6_tlv(struct netaddr *ipv6, bool *add,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_ipv4_conpoint_tlv(struct netaddr *addr, uint16_t *port,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_ipv6_conpoint_tlv(struct netaddr *addr, uint16_t *port,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_uint64(uint64_t *number, uint16_t tlv_id,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_int64(int64_t *number, uint16_t tlv_id,
    struct dlep_session *session, struct dlep_parser_value *value);
int dlep_reader_status(enum dlep_status *status,
    char *text, size_t text_length,
    struct dlep_session *session, struct dlep_parser_value *value);

int dlep_reader_map_identity(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv);
int dlep_reader_map_l2neigh_data(struct oonf_layer2_data *data,
    struct dlep_session *session, struct dlep_extension *ext);

#endif /* _DLEP_READER_H_ */
