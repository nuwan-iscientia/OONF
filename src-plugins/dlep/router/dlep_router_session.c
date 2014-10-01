
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

#include <errno.h>
#include <unistd.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/netaddr.h"

#include "config/cfg_schema.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_parser.h"
#include "dlep/dlep_static_data.h"
#include "dlep/dlep_writer.h"
#include "dlep/router/dlep_router.h"
#include "dlep/router/dlep_router_interface.h"
#include "dlep/router/dlep_router_session.h"

static int _send_peer_initialization(struct dlep_router_session *session);

static void _cb_tcp_lost(struct oonf_stream_session *);
static enum oonf_stream_session_state _cb_tcp_receive_data(struct oonf_stream_session *);

static void _cb_send_heartbeat(void *);
static void _cb_heartbeat_timeout(void *);

static void _handle_peer_initialization_ack(struct dlep_router_session *session,
    void *ptr, struct dlep_parser_index *idx);

/* session objects */
static struct oonf_class _router_stream_class = {
  .name = "DLEP router stream",
  .size = sizeof(struct dlep_router_session),
};

static struct oonf_timer_class _heartbeat_timer_class = {
  .name = "DLEP router heartbeat",
  .callback = _cb_send_heartbeat,
  .periodic = true,
};

static struct oonf_timer_class _heartbeat_timeout_class = {
  .name = "DLEP router timeout",
  .callback = _cb_heartbeat_timeout,
};

/**
 * Initialize dlep router session framework
 */
void
dlep_router_session_init(void) {
  oonf_class_add(&_router_stream_class);
  oonf_timer_add(&_heartbeat_timer_class);
  oonf_timer_add(&_heartbeat_timeout_class);
}

/**
 * Cleanup dlep router session framework
 */
void
dlep_router_session_cleanup(void) {
  oonf_timer_remove(&_heartbeat_timeout_class);
  oonf_timer_remove(&_heartbeat_timer_class);
  oonf_class_remove(&_router_stream_class);
}

/**
 * Get dlep router session based on interface and remote socket
 * @param interf dlep router interface
 * @param remote remote IP socket
 * @return dlep router session, NULL if not found
 */
struct dlep_router_session *
dlep_router_get_session(struct dlep_router_if *interf,
    union netaddr_socket *remote) {
  struct dlep_router_session *session;

  return avl_find_element(&interf->stream_tree, remote, session, _node);
}

/**
 * Add new dlep router session or return existing one
 * @param interf dlep router interface
 * @param local local IP socket
 * @param remote remote IP socket
 * @return dlep router session, NULL if not found
 */
struct dlep_router_session *
dlep_router_add_session(struct dlep_router_if *interf,
    union netaddr_socket *local, union netaddr_socket *remote) {
  struct dlep_router_session *session;
  struct netaddr_str nbuf1, nbuf2;

  session = dlep_router_get_session(interf, remote);
  if (session) {
    return session;
  }

  /* initialize tcp session instance */
  session = oonf_class_malloc(&_router_stream_class);
  if (!session) {
    return NULL;
  }

  /* initialize tree node */
  memcpy(&session->remote_socket, remote, sizeof(*remote));
  session->_node.key = &session->remote_socket;

  /* configure and open TCP session */
  session->tcp.config.session_timeout = 120000; /* 120 seconds */
  session->tcp.config.maximum_input_buffer = 4096;
  session->tcp.config.allowed_sessions = 3;
  session->tcp.config.cleanup = _cb_tcp_lost;
  session->tcp.config.receive_data = _cb_tcp_receive_data;

  if (oonf_stream_add(&session->tcp, local)) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Could not open TCP client on %s for %s",
        interf->name, netaddr_socket_to_string(&nbuf1, local));
    oonf_class_free(&_router_stream_class, session);
    return NULL;
  }

  session->stream = oonf_stream_connect_to(&session->tcp, remote);
  if (!session->stream) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Could not open TCP client on %s for %s to %s",
        interf->name,
        netaddr_socket_to_string(&nbuf1, local),
        netaddr_socket_to_string(&nbuf2, remote));
    oonf_stream_remove(&session->tcp, true);
    oonf_class_free(&_router_stream_class, session);
    return NULL;
  }

  /* initialize back pointer */
  session->interface = interf;

  /* copy timer remote interval */
  OONF_DEBUG(LOG_DLEP_ROUTER, "Heartbeat interval is %"PRIu64,
      interf->remote_heartbeat_interval);
  session->remote_heartbeat_interval = interf->remote_heartbeat_interval;

  /* initialize timers */
  session->heartbeat_timer.cb_context = session;
  session->heartbeat_timer.class = &_heartbeat_timer_class;

  session->heartbeat_timeout.cb_context = session;
  session->heartbeat_timeout.class = &_heartbeat_timeout_class;

  if (_send_peer_initialization(session)) {
    OONF_WARN(LOG_DLEP_ROUTER, "Could not send peer initialization to %s",
        netaddr_socket_to_string(&nbuf1, remote));
    oonf_stream_remove(&session->tcp, true);
    oonf_class_free(&_router_stream_class, session);
    return NULL;
  }

  /* start heartbeat timeout */
  oonf_timer_set(&session->heartbeat_timeout,
      interf->remote_heartbeat_interval * 2);

  /* start heartbeat */
  oonf_timer_set(&session->heartbeat_timer,
      interf->local_heartbeat_interval);

  /* add session to interface */
  avl_insert(&interf->stream_tree, &session->_node);

  return session;
}

/**
 * Remove existing dlep router session
 * @param session dlep router session
 */
void
dlep_router_remove_session(struct dlep_router_session *session) {
  if (session->stream) {
    oonf_stream_close(session->stream);
  }
  oonf_stream_remove(&session->tcp, false);

  oonf_timer_stop(&session->heartbeat_timeout);
  oonf_timer_stop(&session->heartbeat_timer);
  avl_remove(&session->interface->stream_tree, &session->_node);
  oonf_class_free(&_router_stream_class, session);
}

/**
 * Send a peer initialization signal
 * @param session dlep router session
 * @return -1 if an error happened, 0 otherwise
 */
static int
_send_peer_initialization(struct dlep_router_session *session) {
  /* create Peer Initialization */
  OONF_INFO(LOG_DLEP_ROUTER, "Send Peer Initialization");

  dlep_writer_start_signal(DLEP_PEER_INITIALIZATION, &dlep_mandatory_tlvs);

  /* add tlvs */
  dlep_writer_add_heartbeat_tlv(session->interface->local_heartbeat_interval);
  dlep_writer_add_optional_signals();
  dlep_writer_add_optional_data_items();

  if (dlep_writer_finish_signal(LOG_DLEP_ROUTER)) {
    OONF_DEBUG(LOG_DLEP_ROUTER, "bad peer discovery, do not send");
    return -1;
  }

  dlep_writer_send_tcp_unicast(session->stream, &dlep_mandatory_signals);
  return 0;
}

/**
 * Receive tcp data via oonf_stream_socket
 * @param tcp_session
 * @return
 */
static enum oonf_stream_session_state
_cb_tcp_receive_data(struct oonf_stream_session *tcp_session) {
  struct dlep_router_session *stream;
  struct dlep_parser_index idx;
  uint16_t siglen;
  int signal;
  struct netaddr_str nbuf;

  stream = container_of(tcp_session->comport, struct dlep_router_session, tcp);

  if ((signal = dlep_parser_read(&idx, abuf_getptr(&tcp_session->in),
      abuf_getlen(&tcp_session->in), &siglen)) < 0) {
    if (signal != DLEP_PARSER_INCOMPLETE_HEADER
        && signal != DLEP_PARSER_INCOMPLETE_SIGNAL) {
      OONF_WARN_HEX(LOG_DLEP_ROUTER,
          abuf_getptr(&tcp_session->in), abuf_getlen(&tcp_session->in),
          "Could not parse incoming TCP signal %d from %s",
          signal, netaddr_socket_to_string(&nbuf, &tcp_session->remote_socket));
      return STREAM_SESSION_CLEANUP;
    }
  }

  if (!stream->session_active && signal != DLEP_PEER_INITIALIZATION_ACK) {
    OONF_WARN(LOG_DLEP_ROUTER, "Received TCP signal %d before Peer Initialization ACK",
        signal);
    return STREAM_SESSION_CLEANUP;
  }

  OONF_INFO(LOG_DLEP_ROUTER, "Received TCP signal %d", signal);

  switch (signal) {
    case DLEP_PEER_INITIALIZATION_ACK:
      _handle_peer_initialization_ack(
          stream, abuf_getptr(&tcp_session->in), &idx);
      break;
    case DLEP_HEARTBEAT:
      OONF_DEBUG(LOG_DLEP_ROUTER, "Received TCP heartbeat, reset interval to %"PRIu64,
          stream->remote_heartbeat_interval * 2);
      oonf_timer_set(&stream->heartbeat_timeout, stream->remote_heartbeat_interval * 2);
      break;
    default:
      OONF_WARN(LOG_DLEP_ROUTER,
          "Received illegal signal in TCP from %s: %u",
          netaddr_to_string(&nbuf, &tcp_session->remote_address), signal);
      return STREAM_SESSION_CLEANUP;
  }

  /* remove signal from input buffer */
  abuf_pull(&tcp_session->in, siglen);

  return STREAM_SESSION_ACTIVE;
}

/**
 * Callback triggered when tcp session was lost and will be removed
 * @param tcp_session tcp session
 */
static void
_cb_tcp_lost(struct oonf_stream_session *tcp_session) {
  struct dlep_router_session *session;

  session = container_of(tcp_session->comport, struct dlep_router_session, tcp);

  OONF_DEBUG(LOG_DLEP_ROUTER, "tcp lost");

  /* no heartbeats anymore */
  oonf_timer_stop(&session->heartbeat_timer);

  /* trigger lazy cleanup */
  oonf_timer_set(&session->heartbeat_timeout, 1);
}

/**
 * Callback triggered to send regular heartbeats via TCP
 * @param ptr dlep router session
 */
static void
_cb_send_heartbeat(void *ptr) {
  struct dlep_router_session *session = ptr;

  OONF_DEBUG(LOG_DLEP_ROUTER, "Send Heartbeat (%"PRIu64")",
      session->interface->local_heartbeat_interval);

  dlep_writer_start_signal(DLEP_HEARTBEAT, &session->supported_tlvs);

  if (dlep_writer_finish_signal(LOG_DLEP_ROUTER)) {
    return;
  }

  dlep_writer_send_tcp_unicast(session->stream, &session->supported_signals);
}

/**
 * Callback triggered when remote heartbeat times out
 * @param ptr dlep router session
 */
static void
_cb_heartbeat_timeout(void *ptr) {
  struct dlep_router_session *session = ptr;

  OONF_DEBUG(LOG_DLEP_ROUTER, "Heartbeat timeout");

  /* close session */
  dlep_router_remove_session(session);
}

/**
 * Handle incoming peer initialization ack signal
 * @param session dlep router session
 * @param ptr begin of signal
 * @param idx dlep parser index
 */
static void
_handle_peer_initialization_ack(struct dlep_router_session *session,
    void *ptr, struct dlep_parser_index *idx) {
  uint8_t *buffer;
  char peer[256];
  int pos;
  uint64_t data;

  buffer = ptr;

  /* activate session */
  session->session_active = true;

  /* get peer type */
  peer[0] = 0;
  pos = idx->idx[DLEP_PEER_TYPE_TLV];
  if (pos) {
    dlep_parser_get_peer_type(peer, &buffer[pos]);

    OONF_INFO(LOG_DLEP_ROUTER, "Radio peer type: %s", peer);
  }

  /* get heartbeat interval */
  pos = idx->idx[DLEP_HEARTBEAT_INTERVAL_TLV];
  dlep_parser_get_heartbeat_interval(
      &session->remote_heartbeat_interval, &buffer[pos]);

  OONF_DEBUG(LOG_DLEP_ROUTER, "Heartbeat interval is %"PRIu64,
      session->remote_heartbeat_interval);

  /* reset heartbeat timeout */
  oonf_timer_set(&session->heartbeat_timeout, session->remote_heartbeat_interval*2);

  /* add supported signals */
  pos = idx->idx[DLEP_OPTIONAL_SIGNALS_TLV];
  dlep_parser_get_optional_signal(&session->supported_signals, &buffer[pos]);

  /* add supported tlvs */
  pos = idx->idx[DLEP_OPTIONAL_DATA_ITEMS_TLV];
  dlep_parser_get_optional_tlv(&session->supported_tlvs, &buffer[pos]);

  /* get default values for interface */
  pos = idx->idx[DLEP_MDRR_TLV];
  if (pos) {
    dlep_parser_get_mdrr(&data, &buffer[pos]);
    OONF_DEBUG(LOG_DLEP_ROUTER, "Received default for mdrr: %" PRIu64, data);
  }

  pos = idx->idx[DLEP_MDRT_TLV];
  if (pos) {
    dlep_parser_get_mdrt(&data, &buffer[pos]);
    OONF_DEBUG(LOG_DLEP_ROUTER, "Received default for mdrt: %" PRIu64, data);
  }

  pos = idx->idx[DLEP_CDRR_TLV];
  if (pos) {
    dlep_parser_get_cdrr(&data, &buffer[pos]);
    OONF_DEBUG(LOG_DLEP_ROUTER, "Received default for cdrr: %" PRIu64, data);
  }

  pos = idx->idx[DLEP_CDRT_TLV];
  if (pos) {
    dlep_parser_get_cdrt(&data, &buffer[pos]);
    OONF_DEBUG(LOG_DLEP_ROUTER, "Received default for cdrt: %" PRIu64, data);
  }
}
