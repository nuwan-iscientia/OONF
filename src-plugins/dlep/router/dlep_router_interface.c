
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

static void _cleanup_interface(struct dlep_router_if *interface);
static void _cb_send_discovery(void *);

static void _cb_receive_udp(struct oonf_packet_socket *,
    union netaddr_socket *from, void *ptr, size_t length);
static void _handle_peer_offer(struct dlep_router_if *interface,
    uint8_t *buffer, size_t length, struct dlep_parser_index *idx);

static void _generate_peer_discovery(struct dlep_router_if *interface);

static struct avl_tree _interface_tree;

static struct oonf_class _router_if_class = {
  .name = "DLEP router interface",
  .size = sizeof(struct dlep_router_if),
};

static struct oonf_timer_class _discovery_timer_class = {
  .name = "DLEP router heartbeat",
  .callback = _cb_send_discovery,
  .periodic = true,
};

static bool _shutting_down;

/**
 * Initialize dlep router interface framework. This will also
 * initialize the dlep router session framework.
 */
void
dlep_router_interface_init(void) {
  oonf_class_add(&_router_if_class);
  oonf_timer_add(&_discovery_timer_class);
  avl_init(&_interface_tree, avl_comp_strcasecmp, false);

  dlep_router_session_init();

  _shutting_down = false;
}

/**
 * Cleanup dlep router interface framework. This will also cleanup
 * all dlep router sessions.
 */
void
dlep_router_interface_cleanup(void) {
  struct dlep_router_if *interf, *it;

  avl_for_each_element_safe(&_interface_tree, interf, _node, it) {
    dlep_router_remove_interface(interf);
  }

  oonf_timer_remove(&_discovery_timer_class);
  oonf_class_remove(&_router_if_class);

  dlep_router_session_cleanup();
}

/**
 * Get a dlep router interface via interface name
 * @param ifname interface name
 * @return dlep router interface, NULL if not found
 */
struct dlep_router_if *
dlep_router_get_interface(const char *ifname) {
  struct dlep_router_if *interface;

  return avl_find_element(&_interface_tree, ifname, interface, _node);
}

/**
 * Add a new dlep interface or get existing one with same name.
 * @param ifname interface name
 * @return dlep router interface, NULL if allocation failed
 */
struct dlep_router_if *
dlep_router_add_interface(const char *ifname) {
  struct dlep_router_if *interface;

  OONF_DEBUG(LOG_DLEP_ROUTER, "Add session %s", ifname);

  interface = dlep_router_get_interface(ifname);
  if (interface) {
    return interface;
  }

  interface = oonf_class_malloc(&_router_if_class);
  if (!interface) {
    return NULL;
  }

  /* initialize key */
  strscpy(interface->name, ifname, sizeof(interface->name));
  interface->_node.key = interface->name;

  /* initialize timer */
  interface->discovery_timer.cb_context = interface;
  interface->discovery_timer.class = &_discovery_timer_class;

  /* add to global tree of sessions */
  avl_insert(&_interface_tree, &interface->_node);

  /* initialize discovery socket */
  interface->udp.config.user = interface;
  interface->udp.config.receive_data = _cb_receive_udp;
  oonf_packet_add_managed(&interface->udp);

  /* initialize stream list */
  avl_init(&interface->session_tree, avl_comp_netaddr_socket, false);

  return interface;
}

/**
 * Remove dlep router interface
 * @param interface dlep router interface
 */
void
dlep_router_remove_interface(struct dlep_router_if *interface) {
  OONF_DEBUG(LOG_DLEP_ROUTER, "remove session %s", interface->name);

  _cleanup_interface(interface);

  /* close UDP interface */
  oonf_packet_remove_managed(&interface->udp, true);

  /* stop timers */
  oonf_timer_stop(&interface->discovery_timer);

  /* remove session */
  avl_remove(&_interface_tree, &interface->_node);
  oonf_class_free(&_router_if_class, interface);
}

/**
 * Apply new settings to dlep router interface. This will close all
 * existing dlep sessions.
 * @param interf dlep router interface
 */
void
dlep_router_apply_interface_settings(struct dlep_router_if *interf) {
  oonf_packet_apply_managed(&interf->udp, &interf->udp_config);

  _cleanup_interface(interf);

  /* reset discovery timers */
  oonf_timer_set(&interf->discovery_timer, interf->local_discovery_interval);
}

/**
 * Send all active sessions a Peer Terminate signal
 */
void
dlep_router_terminate_all_sessions(void) {
  struct dlep_router_if *interf;
  struct dlep_router_session *session;

  _shutting_down = true;

  avl_for_each_element(&_interface_tree, interf, _node) {
    avl_for_each_element(&interf->session_tree, session, _node) {
      dlep_router_terminate_session(session);
    }
  }
}

/**
 * Close all existing dlep sessions of a dlep interface
 * @param interface dlep router interface
 */
static void
_cleanup_interface(struct dlep_router_if *interface) {
  struct dlep_router_session *stream, *it;

  /* close TCP connection and socket */
  avl_for_each_element_safe(&interface->session_tree, stream, _node, it) {
    dlep_router_remove_session(stream);
  }
}

/**
 * Callback triggered to send regular UDP discovery signals
 * @param ptr dlep router interface
 */
static void
_cb_send_discovery(void *ptr) {
  struct dlep_router_if *interface = ptr;

  if (_shutting_down) {
    /* don't send discovery signals during shutdown */
    return;
  }

  if (!interface->single_session
      || avl_is_empty(&interface->session_tree)) {
    OONF_INFO(LOG_DLEP_ROUTER, "Send UDP Peer Discovery");
    _generate_peer_discovery(interface);
  }
}

/**
 * Callback to receive UDP data through oonf_packet_managed API
 * @param pkt
 * @param from
 * @param ptr
 * @param length
 */
static void
_cb_receive_udp(struct oonf_packet_socket *pkt,
    union netaddr_socket *from, void *ptr, size_t length) {
  struct dlep_router_if *interface;
  struct dlep_parser_index idx;
  int signal;
  struct netaddr_str nbuf;

  if (_shutting_down) {
    /* ignore UDP traffic during shutdown */
    return;
  }
  interface = pkt->config.user;

  if ((signal = dlep_parser_read(&idx, ptr, length, NULL)) < 0) {
    OONF_WARN_HEX(LOG_DLEP_ROUTER, ptr, length,
        "Could not parse incoming UDP signal from %s: %d",
        netaddr_socket_to_string(&nbuf, from), signal);
    return;
  }

  if (interface->single_session
      && !avl_is_empty(&interface->session_tree)) {
    /* ignore UDP signal */
    return;
  }

  OONF_INFO(LOG_DLEP_ROUTER, "Received UDP Signal %u from %s",
      signal, netaddr_socket_to_string(&nbuf, from));

  if (signal != DLEP_PEER_OFFER) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Received illegal signal in UDP from %s: %u",
        netaddr_socket_to_string(&nbuf, from), signal);
    return;
  }

  _handle_peer_offer(interface, ptr, length, &idx);
}

/**
 * Get a matching local IP address for a list of remote IP addresses
 * @param remote_addr remote IP address
 * @param ifdata interface required for local IP address
 * @param idx dlep parser index
 * @param buffer dlep signal buffer
 * @param length length of signal buffer
 * @return matching local IP address, NULL if no match
 */
static const struct netaddr *
_get_local_tcp_address(struct netaddr *remote_addr,
    struct oonf_interface_data *ifdata,
    struct dlep_parser_index *idx, uint8_t *buffer, size_t length) {
  const struct netaddr *ipv6 = NULL, *result = NULL;
  struct netaddr_str nbuf;
  uint16_t pos;

  /* start parsing IPv6 */
  pos = idx->idx[DLEP_IPV6_ADDRESS_TLV];
  while (pos) {
    dlep_parser_get_ipv6_addr(remote_addr, NULL, &buffer[pos]);

    OONF_DEBUG(LOG_DLEP_ROUTER, "Router offered %s on interface %s",
        netaddr_to_string(&nbuf, remote_addr), ifdata->name);

    if (netaddr_is_in_subnet(&NETADDR_IPV6_LINKLOCAL, remote_addr)) {
      result = oonf_interface_get_prefix_from_dst(remote_addr, ifdata);

      if (result) {
        /* we prefer IPv6 linklocal */
        return result;
      }
    }
    else if (ipv6 != NULL) {
      ipv6 = oonf_interface_get_prefix_from_dst(remote_addr, NULL);
    }

    pos = dlep_parser_get_next_tlv(buffer, length, pos);
  }

  if (ipv6) {
    /* No linklocal? then we prefer IPv6 */
    return ipv6;
  }

  /* parse all IPv4 addresses */
  pos = idx->idx[DLEP_IPV4_ADDRESS_TLV];
  while (pos) {
    dlep_parser_get_ipv4_addr(remote_addr, NULL, &buffer[pos]);

    OONF_DEBUG(LOG_DLEP_ROUTER, "Router offered %s on interface %s",
        netaddr_to_string(&nbuf, remote_addr), ifdata->name);

    result = oonf_interface_get_prefix_from_dst(remote_addr, NULL);
    if (result) {
      /* at last, take an IPv4 address */
      return result;
    }
  }

  /*
   * no valid address, hit the manufacturers over the head for not
   * supporting IPv6 linklocal addresses
   */
  OONF_WARN(LOG_DLEP_ROUTER, "No compatible address to router on interface %s",
      ifdata->name);

  return NULL;
}

/**
 * Handle dlep Peer Offer signal via UDP
 * @param interface dlep router interface
 * @param buffer dlep signal buffer
 * @param length signal length
 * @param idx dlep parser index
 */
static void
_handle_peer_offer(struct dlep_router_if *interface,
    uint8_t *buffer, size_t length, struct dlep_parser_index *idx) {
  const struct netaddr *local_addr;
  struct oonf_interface_data *ifdata;
  struct netaddr remote_addr;
  union netaddr_socket local_socket, remote_socket;
  uint16_t port, pos;
  char peer[256];
  struct netaddr_str nbuf1;

  if (idx->idx[DLEP_IPV4_ADDRESS_TLV] == 0 && idx->idx[DLEP_IPV6_ADDRESS_TLV] == 0) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Got UDP Peer Offer without IP TLVs");
    return;
  }

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
      &interface->remote_heartbeat_interval, &buffer[pos]);

  /* get dlep port */
  pos = idx->idx[DLEP_PORT_TLV];
  dlep_parser_get_dlep_port(&port, &buffer[pos]);

  /* get interface data for IPv6 LL */
  ifdata = &interface->udp._if_listener.interface->data;

  /* get prefix for local tcp socket */
  local_addr = _get_local_tcp_address(&remote_addr, ifdata, idx, buffer, length);
  if (!local_addr) {
    return;
  }

  /* open TCP session to radio */
  if (netaddr_socket_init(&local_socket, local_addr, 0, ifdata->index)) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Malformed socket data for DLEP session for %s (%u): %s",
        ifdata->name, ifdata->index,
        netaddr_to_string(&nbuf1, local_addr));
    return;
  }

  if (netaddr_socket_init(&remote_socket, &remote_addr, port, ifdata->index)) {
    OONF_WARN(LOG_DLEP_ROUTER,
        "Malformed socket data for DLEP session for %s (%u): %s (%u)",
        ifdata->name, ifdata->index,
        netaddr_to_string(&nbuf1, local_addr), port);
    return;
  }

  dlep_router_add_session(interface, &local_socket, &remote_socket);
}

static void
_generate_peer_discovery(struct dlep_router_if *interface) {
  dlep_writer_start_signal(DLEP_PEER_DISCOVERY, &dlep_mandatory_tlvs);
  dlep_writer_add_heartbeat_tlv(interface->local_heartbeat_interval);

  if (dlep_writer_finish_signal(LOG_DLEP_ROUTER)) {
    return;
  }

  dlep_writer_send_udp_multicast(
      &interface->udp, &dlep_mandatory_signals, LOG_DLEP_ROUTER);
}
