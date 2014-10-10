
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
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

#ifndef DLEP_RADIO_SESSION_H_
#define DLEP_RADIO_SESSION_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_stream_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_bitmap.h"
#include "dlep/radio/dlep_radio_session.h"

enum dlep_radio_session_state {
  DLEP_RADIO_SESSION_INIT,
  DLEP_RADIO_SESSION_ACTIVE,
  DLEP_RADIO_SESSION_TERMINATE,
};

struct dlep_radio_session {
  /* basic content for tcp stream */
  struct oonf_stream_session stream;

  /* back pointer to interface session */
  struct dlep_radio_if *interface;

  /* state of the DLEP session */
  enum dlep_radio_session_state state;

  /* heartbeat timer */
  struct oonf_timer_instance heartbeat_timer;

  /* heartbeat timeout */
  struct oonf_timer_instance heartbeat_timeout;

  /* heartbeat settings from the other side of the session */
  uint64_t remote_heartbeat_interval;

  /* supported signals of the other side */
  struct dlep_bitmap supported_signals;

  /* supported tlv data items of the other side */
  struct dlep_bitmap supported_tlvs;

  /* node for session tree of interface */
  struct avl_node _node;
};

void dlep_radio_session_init(void);
void dlep_radio_session_cleanup(void);

void dlep_radio_session_initialize_tcp_callbacks(
    struct oonf_stream_config *config);

void dlep_radio_terminate_session(struct dlep_radio_session *session);
#endif /* DLEP_RADIO_SESSION_H_ */
