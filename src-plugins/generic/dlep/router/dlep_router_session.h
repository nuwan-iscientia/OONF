
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2015, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
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

enum dlep_router_session_state {
  DLEP_ROUTER_SESSION_INIT,
  DLEP_ROUTER_SESSION_ACTIVE,
  DLEP_ROUTER_SESSION_TERMINATE,
};

struct dlep_router_session {
  /* remote socket of radio */
  union netaddr_socket remote_socket;

  /* TCP client socket for session */
  struct oonf_stream_socket tcp;

  /* tcp stream session */
  struct oonf_stream_session *stream;

  /* back pointer to interface session */
  struct dlep_router_if *interface;

  /* state of the DLEP session */
  enum dlep_router_session_state state;

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

void dlep_router_session_init(void);
void dlep_router_session_cleanup(void);

struct dlep_router_session *dlep_router_get_session(
    struct dlep_router_if *interf, union netaddr_socket *remote);
struct dlep_router_session *dlep_router_add_session(
    struct dlep_router_if *interf,
    union netaddr_socket *local, union netaddr_socket *remote);
void dlep_router_remove_session(struct dlep_router_session *);

int dlep_router_send_peer_initialization(struct dlep_router_session *);
void dlep_router_terminate_session(struct dlep_router_session *session);

#endif /* DLEP_ROUTER_SESSION_H_ */
