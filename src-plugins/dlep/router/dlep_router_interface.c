/*
 * dlep_router_interface.c
 *
 *  Created on: Oct 1, 2014
 *      Author: rogge
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
static void _restart_interface(struct dlep_router_if *interface);

static void _cb_send_discovery(void *);

static void _cb_receive_udp(struct oonf_packet_socket *,
    union netaddr_socket *from, void *ptr, size_t length);
static void _handle_peer_offer(struct dlep_router_if *interface,
    uint8_t *buffer, size_t length, struct dlep_parser_index *idx);

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

int
dlep_router_interface_init(void) {
  if (dlep_router_session_init()) {
    return -1;
  }
  oonf_class_add(&_router_if_class);
  oonf_timer_add(&_discovery_timer_class);
  avl_init(&_interface_tree, avl_comp_strcasecmp, false);

  return 0;
}

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

struct dlep_router_if *
dlep_router_get_interface(const char *ifname) {
  struct dlep_router_if *interface;

  return avl_find_element(&_interface_tree, ifname, interface, _node);
}

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

  /* set socket to discovery mode */
  interface->state = DLEP_ROUTER_DISCOVERY;

  /* add to global tree of sessions */
  avl_insert(&_interface_tree, &interface->_node);

  /* initialize discovery socket */
  interface->udp.config.user = interface;
  interface->udp.config.receive_data = _cb_receive_udp;
  oonf_packet_add_managed(&interface->udp);

  /* initialize stream list */
  avl_init(&interface->stream_tree, avl_comp_netaddr_socket, false);

  return interface;
}

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

void
dlep_router_apply_interface_settings(struct dlep_router_if *interf) {
  oonf_packet_apply_managed(&interf->udp, &interf->udp_config);
  _restart_interface(interf);
}

static void
_cleanup_interface(struct dlep_router_if *interface) {
  struct dlep_router_session *stream, *it;

  /* close TCP connection and socket */
  avl_for_each_element_safe(&interface->stream_tree, stream, _node, it) {
    dlep_router_remove_session(stream);
  }
}

static void
_restart_interface(struct dlep_router_if *interface) {
  OONF_DEBUG(LOG_DLEP_ROUTER, "Restart session %s", interface->name);

  _cleanup_interface(interface);

  /* reset timers */
  oonf_timer_set(&interface->discovery_timer, interface->local_discovery_interval);

  /* reset session state to discovery */
  interface->state = DLEP_ROUTER_DISCOVERY;
}

static void
_cb_send_discovery(void *ptr) {
  struct dlep_router_if *interface = ptr;

  OONF_INFO(LOG_DLEP_ROUTER, "Send UDP Peer Discovery");

  dlep_writer_start_signal(DLEP_PEER_DISCOVERY, &dlep_mandatory_tlvs);
  dlep_writer_add_heartbeat_tlv(interface->local_heartbeat_interval);

  if (dlep_writer_finish_signal(LOG_DLEP_ROUTER)) {
    return;
  }

  dlep_writer_send_udp_multicast(
      &interface->udp, &dlep_mandatory_signals, LOG_DLEP_ROUTER);
}

static void
_cb_receive_udp(struct oonf_packet_socket *pkt,
    union netaddr_socket *from, void *ptr, size_t length) {
  struct dlep_router_if *interface;
  struct dlep_parser_index idx;
  int signal;
  struct netaddr_str nbuf;

  interface = pkt->config.user;

  if (interface->state != DLEP_ROUTER_DISCOVERY) {
    /* ignore all traffic unless we are in discovery phase */
    return;
  }

  if ((signal = dlep_parser_read(&idx, ptr, length, NULL)) < 0) {
    OONF_WARN_HEX(LOG_DLEP_ROUTER, ptr, length,
        "Could not parse incoming UDP signal from %s: %d",
        netaddr_socket_to_string(&nbuf, from), signal);
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
