/*
 * dlep_base.h
 *
 *  Created on: Jun 29, 2015
 *      Author: rogge
 */

#ifndef _PROTO_H_
#define _PROTO_H_

#include "common/common_types.h"
#include "common/netaddr.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_session.h"

struct dlep_extension *dlep_base_proto_init(void);
void dlep_base_proto_start_local_heartbeat(struct dlep_session *session);
void dlep_base_proto_start_remote_heartbeat(struct dlep_session *session);
void dlep_base_proto_stop_timers(struct dlep_session *session);
enum dlep_status dlep_base_proto_print_status(struct dlep_session *session);
void dlep_base_proto_print_peer_type(struct dlep_session *session);
int dlep_base_proto_process_peer_termination(
    struct dlep_extension *, struct dlep_session *);
int dlep_base_proto_process_peer_termination_ack(
    struct dlep_extension *, struct dlep_session *);
int dlep_base_proto_process_heartbeat(
    struct dlep_extension *, struct dlep_session *);
int dlep_base_proto_write_mac_only(struct dlep_extension *,
    struct dlep_session *session, const struct netaddr *neigh);

#endif /* _PROTO_H_ */
