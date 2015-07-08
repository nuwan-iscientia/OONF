/*
 * dlep_base_radio.c
 *
 *  Created on: Jun 29, 2015
 *      Author: rogge
 */


#include "../dlep_base/dlep_base_router.h"

#include "common/common_types.h"
#include "common/avl.h"
#include "common/autobuf.h"
#include "core/oonf_logging.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_extension.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"
#include "dlep/router/dlep_router_interface.h"
#include "dlep/router/dlep_router_session.h"
#include "dlep/dlep_base/dlep_base.h"
#include "dlep/dlep_base/dlep_base_router.h"

static void _cb_init_router(struct dlep_session *);
static void _cb_apply_router(struct dlep_session *);
static void _cb_cleanup_router(struct dlep_session *);
static void _cb_peer_create_discovery(void *);

static int _router_process_peer_offer(struct dlep_session *);
static int _router_process_peer_init_ack(struct dlep_session *);
static int _router_process_peer_update(struct dlep_session *);
static int _router_process_peer_update_ack(struct dlep_session *);
static int _router_process_destination_up(struct dlep_session *);
static int _router_process_destination_up_ack(struct dlep_session *);
static int _router_process_destination_down(struct dlep_session *);
static int _router_process_destination_down_ack(struct dlep_session *);
static int _router_process_destination_update(struct dlep_session *);
static int _router_process_link_char_ack(struct dlep_session *);

static int _router_write_peer_discovery(
    struct dlep_session *session, const struct netaddr *);
static int _router_write_peer_init(
    struct dlep_session *session, const struct netaddr *);

static struct dlep_extension_implementation _router_signals[] = {
    {
        .id = DLEP_PEER_DISCOVERY,
        .add_tlvs = _router_write_peer_discovery,
    },
    {
        .id = DLEP_PEER_OFFER,
        .process = _router_process_peer_offer,
    },
    {
        .id = DLEP_PEER_INITIALIZATION,
        .add_tlvs = _router_write_peer_init,
    },
    {
        .id = DLEP_PEER_INITIALIZATION_ACK,
        .process = _router_process_peer_init_ack,
    },
    {
        .id = DLEP_PEER_UPDATE,
        .process = _router_process_peer_update,
    },
    {
        .id = DLEP_PEER_UPDATE_ACK,
        .process = _router_process_peer_update_ack,
    },
    {
        .id = DLEP_PEER_TERMINATION,
        .process = dlep_base_process_peer_termination,
    },
    {
        .id = DLEP_PEER_TERMINATION_ACK,
        .process = dlep_base_process_peer_termination_ack,
    },
    {
        .id = DLEP_DESTINATION_UP,
        .process = _router_process_destination_up,
    },
    {
        .id = DLEP_DESTINATION_UP_ACK,
        .process = _router_process_destination_up_ack,
        .add_tlvs = dlep_base_write_mac_only,
    },
    {
        .id = DLEP_DESTINATION_DOWN,
        .process = _router_process_destination_down,
    },
    {
        .id = DLEP_DESTINATION_DOWN_ACK,
        .process = _router_process_destination_down_ack,
        .add_tlvs = dlep_base_write_mac_only,
    },
    {
        .id = DLEP_DESTINATION_UPDATE,
        .process = _router_process_destination_update,
    },
    {
        .id = DLEP_HEARTBEAT,
        .process = dlep_base_process_heartbeat,
    },
    {
        .id = DLEP_LINK_CHARACTERISTICS_ACK,
        .process = _router_process_link_char_ack,
    },
};

static struct oonf_timer_class _peer_discovery_class = {
    .name = "dlep peer discovery",
    .callback = _cb_peer_create_discovery,
    .periodic = true,
};
static struct dlep_extension *_base;

void
dlep_base_router_init(void) {
  _base = dlep_base_init();
  dlep_extension_add_processing(_base, false,
      _router_signals, ARRAYSIZE(_router_signals));

  oonf_timer_add(&_peer_discovery_class);

  _base->cb_session_init_router = _cb_init_router;
  _base->cb_session_apply_router = _cb_apply_router;
  _base->cb_session_cleanup_router = _cb_cleanup_router;
}

static void
_cb_init_router(struct dlep_session *session) {
  if (session->next_signal == DLEP_PEER_INITIALIZATION_ACK) {
    /*
     * we are waiting for a Peer Init Ack,
     * so we need to send a Peer Init
     */
    dlep_session_generate_signal(session, DLEP_PEER_INITIALIZATION, NULL);
    session->cb_send_buffer(session, 0);

    session->remote_heartbeat_interval = session->cfg.heartbeat_interval;
    dlep_base_start_remote_heartbeat(session);
  }
}

static void
_cb_apply_router(struct dlep_session *session) {
  OONF_DEBUG(session->log_source, "Initialize base router session");
  if (session->next_signal == DLEP_PEER_OFFER) {
    /*
     * we are waiting for a Peer Offer,
     * so we need to send Peer Discovery messages
     */
    session->local_event_timer.class = &_peer_discovery_class;
    session->local_event_timer.cb_context = session;

    OONF_DEBUG(session->log_source, "Activate discovery with interval %"
        PRIu64, session->cfg.discovery_interval);

    /* use the "local event" for the discovery timer */
    oonf_timer_start(&session->local_event_timer,
        session->cfg.discovery_interval);
  }
}

static void
_cb_cleanup_router(struct dlep_session *session) {
  struct oonf_layer2_net *l2net;

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (l2net) {
    oonf_layer2_net_remove(l2net, session->l2_origin);
  }

  dlep_base_stop_timers(session);
}

static void
_cb_peer_create_discovery(void *ptr) {
  struct dlep_session *session = ptr;

  OONF_DEBUG(session->log_source, "Generate peer discovery");

  dlep_session_generate_signal(session, DLEP_PEER_DISCOVERY, NULL);
  session->cb_send_buffer(session, AF_INET);

  dlep_session_generate_signal(session, DLEP_PEER_DISCOVERY, NULL);
  session->cb_send_buffer(session, AF_INET6);
}

static int
_router_process_peer_offer(struct dlep_session *session) {
  struct dlep_router_if *router_if;
  union netaddr_socket local, remote;
  struct dlep_parser_value *value;
  const struct netaddr *result = NULL;
  struct netaddr addr;
  uint16_t port;
  struct os_interface_data *ifdata;

  if (session->next_signal != DLEP_PEER_OFFER) {
    /* ignore unless we are in discovery mode */
    return 0;
  }

  /* optional peer type tlv */
  dlep_base_print_peer_type(session);

  /* we are looking for a good address to respond to */
  result = NULL;

  /* remember interface data */
  ifdata = &session->l2_listener.interface->data;

  /* IPv6 offer */
  value = dlep_session_get_tlv_value(session, DLEP_IPV6_CONPOINT_TLV);
  while (value) {
    if (dlep_reader_ipv6_conpoint_tlv(&addr, &port, session, value)) {
      return -1;
    }

    if (netaddr_is_in_subnet(&NETADDR_IPV6_LINKLOCAL, &addr)
        || result == NULL) {
      result = oonf_interface_get_prefix_from_dst(&addr, ifdata);
      if (result) {
        netaddr_socket_init(&remote, &addr, port, ifdata->index);
      }
    }
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* IPv4 offer */
  value = dlep_session_get_tlv_value(session, DLEP_IPV4_CONPOINT_TLV);
  while (value && !result) {
    if (dlep_reader_ipv4_conpoint_tlv(&addr, &port, session, value)) {
      return -1;
    }

    result = oonf_interface_get_prefix_from_dst(&addr, ifdata);
    if (result) {
      netaddr_socket_init(&remote, &addr, port, ifdata->index);
    }
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* remote address of incoming session */
  if (!result) {
    netaddr_from_socket(&addr, &session->remote_socket);
    result = oonf_interface_get_prefix_from_dst(&addr, ifdata);
    if (!result) {
      /* no possible way to communicate */
      return -1;
    }
    netaddr_socket_init(&remote, &addr, port, ifdata->index);
  }

  /* initialize session */
  netaddr_socket_init(&local, result, 0, ifdata->index);

  router_if = dlep_router_get_by_layer2_if(ifdata->name);
  if (router_if && &router_if->interf.session == session) {
    dlep_router_add_session(router_if, &local, &remote);
    return 0;
  }
  /* ignore incoming offer, something is wrong */
  return -1;
}

static int
_router_process_peer_init_ack(struct dlep_session *session) {
  if (session->next_signal != DLEP_PEER_INITIALIZATION_ACK) {
    /* ignore unless we are in initialization mode */
    return 0;
  }

  /* mandatory heartbeat tlv */
  if (dlep_reader_heartbeat_tlv(
      &session->remote_heartbeat_interval, session, NULL)) {
    OONF_WARN(session->log_source, "no heartbeat tlv, should not happen!");
    return -1;
  }

  OONF_DEBUG(session->log_source, "Remote heartbeat interval %"PRIu64,
      session->remote_heartbeat_interval);

  dlep_base_start_local_heartbeat(session);
  dlep_base_start_remote_heartbeat(session);

  dlep_base_print_status(session);

  session->next_signal = DLEP_ALL_SIGNALS;

  return 0;
}

static int
_router_process_peer_update(struct dlep_session *session) {
  struct oonf_layer2_net *l2net;

  l2net = oonf_layer2_net_add(session->l2_listener.name);
  if (!l2net) {
    return -1;
  }
  if (dlep_reader_map_l2neigh_data(l2net->neighdata, session, _base)) {
    return -1;
  }

  // we don't support IP address exchange at the moment
  return 0;
}

static int
_router_process_peer_update_ack(struct dlep_session *session) {
  dlep_base_print_status(session);
  return 0;
}

static int
_router_process_destination_up(struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr mac;

  if (dlep_reader_mac_tlv(&mac, session, NULL)) {
    OONF_DEBUG(session->log_source, "mac tlv missing");
    return -1;
  }

  l2net = oonf_layer2_net_add(session->l2_listener.name);
  if (!l2net) {
    return dlep_session_generate_signal_status(
        session, DLEP_DESTINATION_UP_ACK, &mac,
        DLEP_STATUS_REQUEST_DENIED, "Not enough memory");
  }
  l2neigh = oonf_layer2_neigh_add(l2net, &mac);
  if (!l2neigh) {
    return dlep_session_generate_signal_status(
        session, DLEP_DESTINATION_UP_ACK, &mac,
        DLEP_STATUS_REQUEST_DENIED, "Not enough memory");
  }

  if (dlep_reader_map_l2neigh_data(l2neigh->data, session, _base)) {
    OONF_DEBUG(session->log_source, "tlv mapping failed");
    return -1;
  }

  return dlep_session_generate_signal(
      session, DLEP_DESTINATION_UP_ACK, &mac);
}

static int
_router_process_destination_up_ack(struct dlep_session *session) {
  dlep_base_print_status(session);
  return 0;
}

static int
_router_process_destination_down(struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr mac;

  if (dlep_reader_mac_tlv(&mac, session, NULL)) {
    return -1;
  }

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    return 0;
  }

  l2neigh = oonf_layer2_neigh_get(l2net, &mac);
  if (!l2neigh) {
    return 0;
  }

  /* remove layer2 neighbor */
  oonf_layer2_neigh_remove(l2neigh, session->l2_origin);

  return dlep_session_generate_signal(
      session, DLEP_DESTINATION_UP_ACK, &mac);
}

static int
_router_process_destination_down_ack(struct dlep_session *session) {
  dlep_base_print_status(session);
  return 0;
}

static int
_router_process_destination_update(struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr mac;

  if (dlep_reader_mac_tlv(&mac, session, NULL)) {
    return -1;
  }

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    return 0;
  }

  l2neigh = oonf_layer2_neigh_get(l2net, &mac);
  if (!l2neigh) {
    /* we did not get the destination up signal */
    return 0;
  }

  if (dlep_reader_map_l2neigh_data(l2neigh->data, session, _base)) {
    return -1;
  }

  return 0;
}

static int
_router_process_link_char_ack(struct dlep_session *session) {
  dlep_base_print_status(session);
  return 0;
}

static int
_router_write_peer_discovery(struct dlep_session *session,
    const struct netaddr *addr __attribute__((unused))) {
  if (session->next_signal != DLEP_PEER_OFFER) {
    return -1;
  }
  return 0;
}

static int
_router_write_peer_init(struct dlep_session *session,
    const struct netaddr *addr __attribute__((unused))) {
  const uint16_t *ext_ids;
  uint16_t ext_count;

  /* write supported extensions */
  ext_ids = dlep_extension_get_ids(&ext_count);
  if (ext_count) {
    dlep_writer_add_supported_extensions(
        &session->writer, ext_ids, ext_count);
  }

  dlep_writer_add_heartbeat_tlv(&session->writer,
      session->cfg.heartbeat_interval);

  if (session->cfg.peer_type) {
    dlep_writer_add_peer_type_tlv(
        &session->writer, session->cfg.peer_type);
  }

  return 0;
}
