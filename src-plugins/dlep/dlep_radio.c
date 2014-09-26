
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
#include "dlep/dlep_radio.h"
#include "dlep/dlep_writer.h"


/* prototypes */
static int _init(void);
static void _cleanup(void);

static struct dlep_radio_session *_add_session(const char *interf);
static void _remove_session(struct dlep_radio_session *);

static void _cb_send_heartbeat(void *);
static void _cb_heartbeat_timeout(void *);
static void _cb_receive_udp(struct oonf_packet_socket *,
    union netaddr_socket *from, void *ptr, size_t length);

static void _handle_peer_discovery(struct dlep_radio_session *session,
    union netaddr_socket *dst, uint8_t *buffer, struct dlep_parser_index *idx);

static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _radio_entries[] = {
  CFG_MAP_NETADDR_V4(dlep_radio_session, discovery_config.multicast_v4, "discovery_mc_v4",
    "224.0.0.1", "IPv4 address to send discovery UDP packet to", false, false),
  CFG_MAP_NETADDR_V6(dlep_radio_session, discovery_config.multicast_v6, "discovery_mc_v6",
    "ff02::1", "IPv6 address to send discovery UDP packet to", false, false),
  CFG_MAP_INT32_MINMAX(dlep_radio_session, discovery_config.port, "discovery_port",
    "12345", "UDP port for discovery packets", 0, false, 1, 65535),
  CFG_MAP_ACL_V46(dlep_radio_session, discovery_config.bindto, "discovery_bindto", "fe80::/10",
    "Filter to determine the binding of the UDP discovery socket"),

  CFG_MAP_INT32_MINMAX(dlep_radio_session, session_config.port, "session_port",
      DLEP_WELL_KNOWN_MULTICAST_PORT_TXT, "Server port for DLEP tcp sessions",
      0, false, 1, 65535),
  CFG_MAP_ACL_V46(dlep_radio_session, session_config.bindto, "session_bindto", "fe80::/10",
      "Filter to determine the binding of the TCP server socket"),
  CFG_MAP_CLOCK_MINMAX(dlep_radio_session, local_heartbeat_interval,
      "heartbeat_interval", "1.000",
      "Interval in seconds between two heartbeat signals", 1000, 65535000),
};

static struct cfg_schema_section _radio_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .mode = CFG_SSMODE_NAMED,
  .cb_delta_handler = _cb_config_changed,
  .entries = _radio_entries,
  .entry_count = ARRAYSIZE(_radio_entries),
};

/* plugin declaration */
struct oonf_subsystem dlep_radio_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONF DLEP radio plugin",
  .author = "Henning Rogge",

  .cfg_section = &_radio_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(dlep_radio_subsystem);

/* session objects */
static struct avl_tree _session_tree;

static struct oonf_class _session_class = {
  .name = "DLEP radio session",
  .size = sizeof(struct dlep_radio_session),
};

static struct oonf_timer_class _heartbeat_timer_class = {
  .name = "DLEP radio heartbeat",
  .callback = _cb_send_heartbeat,
  .periodic = true,
};

static struct oonf_timer_class _heartbeat_timeout_class = {
  .name = "DLEP radio heartbeat timeout",
  .callback = _cb_heartbeat_timeout,
};

static int
_init(void) {
  /* add classes for session handling */
  oonf_class_add(&_session_class);
  oonf_timer_add(&_heartbeat_timer_class);
  oonf_timer_add(&_heartbeat_timeout_class);

  avl_init(&_session_tree, avl_comp_strcasecmp, false);
  return 0;
}

static void
_cleanup(void) {
  struct dlep_radio_session *session, *s_it;

  avl_for_each_element_safe(&_session_tree, session, _node, s_it) {
    _remove_session(session);
  }

  oonf_timer_remove(&_heartbeat_timeout_class);
  oonf_timer_remove(&_heartbeat_timer_class);
  oonf_class_remove(&_session_class);
}

static struct dlep_radio_session *
_add_session(const char *interf) {
  struct dlep_radio_session *session;

  session = avl_find_element(&_session_tree, interf, session, _node);
  if (session) {
    return session;
  }

  session = oonf_class_malloc(&_session_class);
  if (!session) {
    return NULL;
  }

  /* initialize key */
  strscpy(session->interf, interf, sizeof(session->interf));
  session->_node.key = session->interf;

  /* initialize timer */
  session->heartbeat_timer.cb_context = session;
  session->heartbeat_timer.info = &_heartbeat_timer_class;

  session->heartbeat_timeout.cb_context = session;
  session->heartbeat_timeout.info = &_heartbeat_timeout_class;

  /* set socket to discovery mode */
  session->state = DLEP_RADIO_DISCOVERY;

  /* add to global tree of sessions */
  avl_insert(&_session_tree, &session->_node);

  /* initialize discovery socket */
  session->discovery.config.user = session;
  session->discovery.config.receive_data = _cb_receive_udp;
  oonf_packet_add_managed(&session->discovery);

  //TODO: configure TCP socket
#if 0
  static struct oonf_stream_managed _telnet_managed = {
    .config = {
      .session_timeout = 120000, /* 120 seconds */
      .maximum_input_buffer = 4096,
      .allowed_sessions = 3,
      .memcookie = &_telnet_memcookie,
      .init = _cb_telnet_init,
      .cleanup = _cb_telnet_cleanup,
      .receive_data = _cb_telnet_receive_data,
      .create_error = _cb_telnet_create_error,
    },
  };
#endif
  return session;
}

static void
_remove_session(struct dlep_radio_session *session) {
  /* cleanup discovery socket */
  oonf_packet_remove_managed(&session->discovery, true);

  /* cleanup session socket */
  if (session->stream) {
    oonf_stream_close(session->stream, true);
    session->stream = NULL;
  }
  oonf_stream_remove_managed(&session->session, true);

  /* stop timers */
  oonf_timer_stop(&session->heartbeat_timer);
  oonf_timer_stop(&session->heartbeat_timeout);

  /* remove session */
  avl_remove(&_session_tree, &session->_node);
  oonf_class_free(&_session_class, session);
}

static void
_restart_session(struct dlep_radio_session *session) {
  /* reset timers */
  oonf_timer_stop(&session->heartbeat_timer);
  oonf_timer_stop(&session->heartbeat_timeout);

  /* close TCP connection and socket */
  if (session->stream) {
    oonf_stream_close(session->stream, true);
    session->stream = NULL;
  }
  oonf_stream_remove_managed(&session->session, true);

  /* reset session state to discovery */
  session->state = DLEP_RADIO_DISCOVERY;
}

static void
_cb_send_heartbeat(void *ptr) {
  struct dlep_radio_session *session = ptr;

  dlep_writer_start_signal(DLEP_HEARTBEAT);
  dlep_writer_add_heartbeat_tlv(session->local_heartbeat_interval);
  if (dlep_writer_finish_signal(LOG_DLEP_RADIO)) {
    return;
  }

  dlep_writer_send_tcp_unicast(session->stream);
}

static void
_cb_heartbeat_timeout(void *ptr) {
  struct dlep_radio_session *session = ptr;

  _restart_session(session);
}

static void
_cb_receive_udp(struct oonf_packet_socket *pkt,
    union netaddr_socket *from, void *ptr, size_t length) {
  struct dlep_radio_session *session;
  struct dlep_parser_index idx;
  uint8_t *buffer;
  int result;
  struct netaddr_str nbuf;

  buffer = ptr;
  session = pkt->config.user;

  if (session->state != DLEP_RADIO_DISCOVERY) {
    /* ignore all traffic unless we are in discovery phase */
    return;
  }

  if ((result = dlep_parser_read(&idx, ptr, length)) != 0) {
    OONF_WARN(LOG_DLEP_RADIO,
        "Could not parse incoming UDP signal from %s: %d",
        netaddr_socket_to_string(&nbuf, from), result);
    return;
  }

  if (buffer[0] != DLEP_PEER_DISCOVERY) {
    OONF_WARN(LOG_DLEP_RADIO,
        "Received illegal signal in UDP from %s: %u",
        netaddr_socket_to_string(&nbuf, from), buffer[0]);
    return;
  }

  _handle_peer_discovery(session, from, buffer, &idx);
}

static void
_handle_peer_discovery(struct dlep_radio_session *session,
    union netaddr_socket *dst, uint8_t *buffer, struct dlep_parser_index *idx) {
  struct netaddr addr;
  int pos;

  /* get heartbeat interval */
  pos = idx->idx[DLEP_HEARTBEAT_INTERVAL_TLV];
  dlep_parser_get_heartbeat_interval(
      &session->remote_heartbeat_interval, &buffer[pos]);

  /* (re)start heartbeat timeout */
  oonf_timer_set(&session->heartbeat_timeout, session->remote_heartbeat_interval * 2);

  /* create Peer Offer */
  dlep_writer_start_signal(DLEP_PEER_OFFER);
  dlep_writer_add_heartbeat_tlv(session->local_heartbeat_interval);
  dlep_writer_add_port_tlv(session->session_config.port);

  netaddr_from_socket(&addr, &session->session.socket_v4.local_socket);
  dlep_writer_add_ipv4_tlv(&addr, true);

  netaddr_from_socket(&addr, &session->session.socket_v6.local_socket);
  dlep_writer_add_ipv6_tlv(&addr, true);

  if (dlep_writer_finish_signal(LOG_DLEP_RADIO)) {
    return;
  }
  dlep_writer_send_udp_unicast(&session->discovery, dst, LOG_DLEP_RADIO);
}

static void
_cb_config_changed(void) {
  struct dlep_radio_session *session;

  if (!_radio_section.post) {
    /* remove old session object */
    session = avl_find_element(&_session_tree,
        _radio_section.section_name, session, _node);
    if (session) {
      _remove_session(session);
    }
    return;
  }

  /* get session object or create one */
  session = _add_session(_radio_section.section_name);
  if (!session) {
    return;
  }

  /* read configuration */
  if (cfg_schema_tobin(session, _radio_section.post,
      _radio_entries, ARRAYSIZE(_radio_entries))) {
    OONF_WARN(LOG_DLEP_RADIO, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }

  /* apply settings */
  oonf_packet_apply_managed(&session->discovery, &session->discovery_config);

  _restart_session(session);
}
