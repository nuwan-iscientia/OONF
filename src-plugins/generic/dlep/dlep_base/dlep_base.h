/*
 * dlep_base.h
 *
 *  Created on: Jun 29, 2015
 *      Author: rogge
 */

#ifndef _DLEP_BASE_H_
#define _DLEP_BASE_H_

#include "common/common_types.h"
#include "core/oonf_logging.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_session.h"

struct dlep_extension *dlep_base_init(void);
void dlep_base_start_local_heartbeat(struct dlep_session *session);
void dlep_base_start_remote_heartbeat(struct dlep_session *session);
void dlep_base_stop_timers(struct dlep_session *session);
enum dlep_status dlep_base_print_status(struct dlep_session *session);
void dlep_base_print_peer_type(struct dlep_session *session);
int dlep_base_process_peer_termination(struct dlep_session *);
int dlep_base_process_peer_termination_ack(struct dlep_session *);
int dlep_base_process_heartbeat(struct dlep_session *);
int dlep_base_write_mac_only(
    struct dlep_session *session, const struct netaddr *neigh);

#endif /* _DLEP_BASE_H_ */
