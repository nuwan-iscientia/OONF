/*
 * dlep_router_session.h
 *
 *  Created on: Oct 1, 2014
 *      Author: rogge
 */

#ifndef DLEP_ROUTER_SESSION_H_
#define DLEP_ROUTER_SESSION_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/netaddr.h"
#include "subsystems/oonf_stream_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_bitmap.h"
#include "dlep/router/dlep_router_interface.h"

struct dlep_router_session {
  /* remote socket of radio */
  union netaddr_socket remote_socket;

  /* TCP client socket for session */
  struct oonf_stream_socket tcp;

  /* tcp stream session */
  struct oonf_stream_session *stream;

  /* back pointer to interface session */
  struct dlep_router_if *interface;

  /* timer to generate heartbeats */
  struct oonf_timer_instance heartbeat_timer;

  /* keep track of various timeouts */
  struct oonf_timer_instance heartbeat_timeout;

  /* supported signals of the other side */
  struct dlep_bitmap supported_signals;

  /* supported optional tlv data items of the other side */
  struct dlep_bitmap supported_tlvs;

  /* heartbeat settings from the other side of the session */
  uint64_t remote_heartbeat_interval;

  /* remember all streams bound to an interface */
  struct avl_node _node;
};

EXPORT int dlep_router_session_init(void);
EXPORT void dlep_router_session_cleanup(void);

EXPORT struct dlep_router_session *dlep_router_get_session(
    struct dlep_router_if *interf, union netaddr_socket *remote);
EXPORT struct dlep_router_session *dlep_router_add_session(
    struct dlep_router_if *interf,
    union netaddr_socket *local, union netaddr_socket *remote);
EXPORT void dlep_router_remove_session(struct dlep_router_session *);

EXPORT int dlep_router_send_peer_initialization(struct dlep_router_session *);

#endif /* DLEP_ROUTER_SESSION_H_ */
