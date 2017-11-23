
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

/**
 * @file
 */


#include "ip.h"

#include "common/common_types.h"
#include "common/avl.h"
#include "common/autobuf.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_extension.h"
#include "dlep/dlep_interface.h"
#include "dlep/radio/dlep_radio_interface.h"
#include "dlep/radio/dlep_radio_session.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"

struct _prefix_storage {
  struct netaddr prefix;

  bool add;

  struct avl_node _node;
};

static void _cb_session_init(struct dlep_session *session);
static void _cb_session_cleanup(struct dlep_session *session);
static int _radio_write_session_update(struct dlep_extension *ext,
    struct dlep_session *session, const struct netaddr *neigh);
static int _radio_write_destination_update(struct dlep_extension *ext,
    struct dlep_session *session, const struct netaddr *neigh);
static enum dlep_parser_error _router_process_session_update(
    struct dlep_extension *ext, struct dlep_session *session);
static enum dlep_parser_error _router_process_destination_update(
    struct dlep_extension *ext, struct dlep_session *session);
static void _add_prefix(struct avl_tree *tree, struct netaddr *addr, bool add);

static void _cb_add_if_ip(void *ptr);
static void _cb_remove_if_ip(void *ptr);
static void _cb_add_neigh_ip(void *ptr);
static void _cb_remove_neigh_ip(void *ptr);

/* peer initialization ack/peer update/destination update */
static const uint16_t _ip_tlvs[] = {
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV4_SUBNET_TLV,
    DLEP_IPV6_ADDRESS_TLV,
    DLEP_IPV6_SUBNET_TLV,
};

/* supported signals of this extension */
static struct dlep_extension_signal _signals[] = {
    {
        .id = DLEP_SESSION_INITIALIZATION_ACK,
        .supported_tlvs = _ip_tlvs,
        .supported_tlv_count = ARRAYSIZE(_ip_tlvs),
        .add_radio_tlvs = _radio_write_session_update,
        .process_router = _router_process_session_update,
    },
    {
        .id = DLEP_SESSION_UPDATE,
        .supported_tlvs = _ip_tlvs,
        .supported_tlv_count = ARRAYSIZE(_ip_tlvs),
        .add_radio_tlvs = _radio_write_session_update,
        .process_router = _router_process_session_update,
    },
    {
        .id = DLEP_DESTINATION_UPDATE,
        .supported_tlvs = _ip_tlvs,
        .supported_tlv_count = ARRAYSIZE(_ip_tlvs),
        .add_radio_tlvs = _radio_write_destination_update,
        .process_router = _router_process_destination_update,
    },
};

/* supported TLVs of this extension */
static struct dlep_extension_tlv _tlvs[] = {
    { DLEP_MAC_ADDRESS_TLV, 6,8 },
    { DLEP_IPV4_ADDRESS_TLV, 5,5 },
    { DLEP_IPV4_SUBNET_TLV, 6,6 },
    { DLEP_IPV6_ADDRESS_TLV, 17,17 },
    { DLEP_IPV6_SUBNET_TLV, 18,18 },
};

/* DLEP base extension, radio side */
static struct dlep_extension _base_ip = {
  .id = DLEP_EXTENSION_BASE_IP,
  .name = "base metric",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),

  .cb_session_init_radio = _cb_session_init,
  .cb_session_init_router = _cb_session_init,

  .cb_session_cleanup_radio = _cb_session_cleanup,
  .cb_session_cleanup_router = _cb_session_cleanup,
};

static struct oonf_class _prefix_class = {
  .name = "dlep ip prefix",
  .size = sizeof(struct _prefix_storage),
};

static struct oonf_class_extension _l2_interface_ip_listener = {
  .ext_name = "dlep l2 if-ip",
  .class_name = LAYER2_CLASS_NETWORK_ADDRESS,

  .cb_add = _cb_add_if_ip,
  .cb_remove = _cb_remove_if_ip,
};

static struct oonf_class_extension _l2_neighbor_ip_listener = {
  .ext_name = "dlep l2 neigh-ip",
  .class_name = LAYER2_CLASS_NEIGHBOR_ADDRESS,

  .cb_add = _cb_add_neigh_ip,
  .cb_remove = _cb_remove_neigh_ip,
};

/**
 * Initialize the base metric DLEP extension
 * @return this extension
 */
struct dlep_extension *
dlep_base_ip_init(void) {
  dlep_extension_add(&_base_ip);
  oonf_class_add(&_prefix_class);
  oonf_class_extension_add(&_l2_interface_ip_listener);
  oonf_class_extension_add(&_l2_neighbor_ip_listener);

  return &_base_ip;
}

void
dlep_base_ip_cleanup(void) {
  oonf_class_extension_remove(&_l2_neighbor_ip_listener);
  oonf_class_extension_remove(&_l2_interface_ip_listener);
  oonf_class_remove(&_prefix_class);
}

static
void _cb_session_init(struct dlep_session *session) {
  struct oonf_layer2_neighbor_address *l2neigh_ip;
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_peer_address *l2net_ip;
  struct oonf_layer2_net *l2net;
  struct dlep_local_neighbor *dlep_neighbor;

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    return;
  }

  avl_for_each_element(&l2net->local_peer_ips, l2net_ip, _net_node) {
    _add_prefix(&session->_ip_prefix_modification, &l2net_ip->ip, true);
  }

  avl_for_each_element(&l2net->neighbors, l2neigh, _node) {
    dlep_neighbor = dlep_session_add_local_neighbor(session, &l2neigh->addr);
    if (dlep_neighbor) {
      avl_for_each_element(&l2neigh->remote_neighbor_ips, l2neigh_ip, _neigh_node) {
        _add_prefix(&dlep_neighbor->_ip_prefix_modification, &l2neigh_ip->ip, true);
      }
    }
  }
}

static
void _cb_session_cleanup(struct dlep_session *session) {
  struct dlep_local_neighbor *l2neigh;
  struct _prefix_storage *storage, *storage_it;

  /* remove all stored changes for neighbors */
  avl_for_each_element(&session->local_neighbor_tree, l2neigh, _node) {
    avl_for_each_element_safe(&l2neigh->_ip_prefix_modification, storage, _node, storage_it) {
      avl_remove(&l2neigh->_ip_prefix_modification, &storage->_node);
      oonf_class_free(&_prefix_class, storage);
    }
  }

  /* remove all stored changes for the local peer */
  avl_for_each_element_safe(&session->_ip_prefix_modification, storage, _node, storage_it) {
    avl_remove(&session->_ip_prefix_modification, &storage->_node);
    oonf_class_free(&_prefix_class, storage);
  }
}

static int
_radio_write_session_update(struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session, const struct netaddr *neigh __attribute__((unused))) {
  struct _prefix_storage *storage, *storage_it;

  struct netaddr_str nbuf;

  avl_for_each_element(&session->_ip_prefix_modification, storage, _node) {
    OONF_INFO(session->log_source, "Add '%s' (%s) to session update",
        netaddr_to_string(&nbuf, &storage->prefix),
        storage->add ? "add" : "remove");
    if (dlep_writer_add_ip_tlv(&session->writer, &storage->prefix, storage->add)) {
        OONF_WARN(session->log_source, "Cannot add '%s' (%s) to session update",
            netaddr_to_string(&nbuf, &storage->prefix),
            storage->add ? "add" : "remove");
        return -1;
    }
  }

  /* no error, now remove elements from temporary storage */
  avl_for_each_element_safe(&session->_ip_prefix_modification, storage, _node, storage_it) {
    avl_remove(&session->_ip_prefix_modification, &storage->_node);
    oonf_class_free(&_prefix_class, storage);
  }
  return 0;
}

static int
_radio_write_destination_update(struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session, const struct netaddr *neigh) {
  struct dlep_local_neighbor *dlep_neigh;
  struct _prefix_storage *storage, *storage_it;

  struct netaddr_str nbuf1, nbuf2;

  dlep_neigh = dlep_session_get_local_neighbor(session, neigh);
  if (!dlep_neigh) {
    OONF_WARN(session->log_source, "Could not find dlep_neighbor "
        "for neighbor %s", netaddr_to_string(&nbuf1, neigh));
    return -1;
  }

  /* send every attached IP towards the router */
  avl_for_each_element(&dlep_neigh->_ip_prefix_modification, storage, _node) {
    OONF_INFO(session->log_source, "add '%s' (%s) to destination update %s",
        netaddr_to_string(&nbuf1, &storage->prefix),
        storage->add ? "add" : "remove",
        netaddr_to_string(&nbuf2, neigh));
    if (dlep_writer_add_ip_tlv(&session->writer, &storage->prefix, storage->add)) {
      OONF_WARN(session->log_source, "Cannot add '%s' (%s) to destination update %s",
          netaddr_to_string(&nbuf1, &storage->prefix),
          storage->add ? "add" : "remove",
          netaddr_to_string(&nbuf2, neigh));
      return -1;
    }
  }

  /* no error, now remove elements from temporary storage */
  avl_for_each_element_safe(&dlep_neigh->_ip_prefix_modification, storage, _node, storage_it) {
    avl_remove(&dlep_neigh->_ip_prefix_modification, &storage->_node);
    oonf_class_free(&_prefix_class, storage);
  }
  return 0;
}

static void
_process_session_ip_tlvs(const struct oonf_layer2_origin *origin,
    struct oonf_layer2_net *l2net, struct netaddr *ip, bool add) {
  struct oonf_layer2_peer_address *l2addr;

  if (add) {
    oonf_layer2_net_add_ip(l2net, origin, ip);
  }
  else if ((l2addr = oonf_layer2_net_get_local_ip(l2net, ip))){
    oonf_layer2_net_remove_ip(l2addr, origin);
  }
}

static enum dlep_parser_error
_router_process_session_update(struct dlep_extension *ext __attribute((unused)),
    struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct netaddr ip;
  struct dlep_parser_value *value;
  bool add_ip;

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    return 0;
  }

  /* ipv4 address */
  value = dlep_session_get_tlv_value(session, DLEP_IPV4_ADDRESS_TLV);
  while (value) {
    if (dlep_reader_ipv4_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_session_ip_tlvs(session->l2_origin, l2net, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv6 address */
  value = dlep_session_get_tlv_value(session, DLEP_IPV6_ADDRESS_TLV);
  while (value) {
    if (dlep_reader_ipv6_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_session_ip_tlvs(session->l2_origin, l2net, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv4 subnet */
  value = dlep_session_get_tlv_value(session, DLEP_IPV4_SUBNET_TLV);
  while (value) {
    if (dlep_reader_ipv4_subnet_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_session_ip_tlvs(session->l2_origin, l2net, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv6 subnet */
  value = dlep_session_get_tlv_value(session, DLEP_IPV6_SUBNET_TLV);
  while (value) {
    if (dlep_reader_ipv6_subnet_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_session_ip_tlvs(session->l2_origin, l2net, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }
  return 0;
}

static void
_process_destination_ip_tlv(const struct oonf_layer2_origin *origin,
    struct oonf_layer2_neigh *l2neigh, struct netaddr *ip, bool add) {
  struct oonf_layer2_neighbor_address *l2addr;

  if (add) {
    oonf_layer2_neigh_add_ip(l2neigh, origin, ip);
  }
  else if ((l2addr = oonf_layer2_neigh_get_remote_ip(l2neigh, ip))){
    oonf_layer2_neigh_remove_ip(l2addr, origin);
  }
}

static enum dlep_parser_error
_router_process_destination_update(struct dlep_extension *ext __attribute((unused)),
    struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr mac, ip;
  struct dlep_parser_value *value;
  bool add_ip;

  if (dlep_reader_mac_tlv(&mac, session, NULL)) {
    OONF_INFO(session->log_source, "mac tlv missing");
    return -1;
  }

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    return 0;
  }
  l2neigh = oonf_layer2_neigh_add(l2net, &mac);
  if (!l2neigh) {
    return 0;
  }

  /* ipv4 address */
  value = dlep_session_get_tlv_value(session, DLEP_IPV4_ADDRESS_TLV);
  while (value) {
    if (dlep_reader_ipv4_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_destination_ip_tlv(session->l2_origin, l2neigh, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv6 address */
  value = dlep_session_get_tlv_value(session, DLEP_IPV6_ADDRESS_TLV);
  while (value) {
    if (dlep_reader_ipv6_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_destination_ip_tlv(session->l2_origin, l2neigh, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv4 subnet */
  value = dlep_session_get_tlv_value(session, DLEP_IPV4_SUBNET_TLV);
  while (value) {
    if (dlep_reader_ipv4_subnet_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_destination_ip_tlv(session->l2_origin, l2neigh, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }

  /* ipv6 subnet */
  value = dlep_session_get_tlv_value(session, DLEP_IPV6_SUBNET_TLV);
  while (value) {
    if (dlep_reader_ipv6_subnet_tlv(&ip, &add_ip, session, value)) {
      return -1;
    }
    _process_destination_ip_tlv(session->l2_origin, l2neigh, &ip, add_ip);
    value = dlep_session_get_next_tlv_value(session, value);
  }
  return 0;
}

static void
_add_prefix(struct avl_tree *tree, struct netaddr *addr, bool add) {
  struct _prefix_storage *storage;

  storage = avl_find_element(tree, addr, storage, _node);
  if (storage) {
    storage->add = add;
    return;
  }

  storage = oonf_class_malloc(&_prefix_class);
  if (!storage) {
    return;
  }

  /* copy key and put into tree */
  memcpy(&storage->prefix, addr, sizeof(*addr));
  storage->_node.key = &storage->prefix;
  avl_insert(tree, &storage->_node);

  storage->add = add;
}

static void
_modify_if_ip(const char *if_name, struct netaddr *prefix, bool add) {
  struct dlep_radio_if *interf;
  struct dlep_radio_session *radio_session;

  avl_for_each_element(dlep_if_get_tree(true), interf, interf._node) {
    if (strcmp(interf->interf.l2_ifname, if_name) == 0) {
      avl_for_each_element(&interf->interf.session_tree, radio_session, _node) {
        _add_prefix(&radio_session->session._ip_prefix_modification, prefix, add);
      }
    }
  }
}

static void
_cb_add_if_ip(void *ptr){
  struct oonf_layer2_peer_address *peer_ip = ptr;
  _modify_if_ip(peer_ip->l2net->name, &peer_ip->ip, true);
}

static void
_cb_remove_if_ip(void *ptr) {
  struct oonf_layer2_peer_address *peer_ip = ptr;
  _modify_if_ip(peer_ip->l2net->name, &peer_ip->ip, false);
}

static void
_modify_neigh_ip(const char *if_name, struct netaddr *neighbor,
    struct netaddr *prefix, bool add) {
  struct dlep_radio_if *interf;
  struct dlep_local_neighbor *dlep_neighbor;
  struct dlep_radio_session *radio_session;

  avl_for_each_element(dlep_if_get_tree(true), interf, interf._node) {
    if (strcmp(interf->interf.l2_ifname, if_name) == 0) {
      avl_for_each_element(&interf->interf.session_tree, radio_session, _node) {
        dlep_neighbor = dlep_session_get_local_neighbor(&radio_session->session, neighbor);
        if (dlep_neighbor) {
          _add_prefix(&dlep_neighbor->_ip_prefix_modification, prefix, add);
        }
      }
    }
  }
}

static void
_cb_add_neigh_ip(void *ptr) {
  struct oonf_layer2_neighbor_address *neigh_ip = ptr;
  _modify_neigh_ip(neigh_ip->l2neigh->network->name,
      &neigh_ip->l2neigh->addr, &neigh_ip->ip, true);
}

static void
_cb_remove_neigh_ip(void *ptr) {
  struct oonf_layer2_neighbor_address *neigh_ip = ptr;
  _modify_neigh_ip(neigh_ip->l2neigh->network->name,
      &neigh_ip->l2neigh->addr, &neigh_ip->ip, false);
}
