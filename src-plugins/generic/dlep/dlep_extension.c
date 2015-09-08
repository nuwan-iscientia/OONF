/*
 * dlep_extension.c
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#include <stdlib.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"

#include "subsystems/oonf_layer2.h"

#include "dlep/dlep_reader.h"
#include "dlep/dlep_session.h"
#include "dlep/dlep_writer.h"
#include "dlep/dlep_extension.h"

static struct avl_tree _extension_tree;

static uint16_t *_id_array = NULL;
static uint16_t _id_array_length = 0;

void
dlep_extension_init(void) {
  avl_init(&_extension_tree, avl_comp_int32, false);
}

void
dlep_extension_add(struct dlep_extension *ext) {
  uint16_t *ptr;

  if (avl_is_node_added(&ext->_node)) {
    return;
  }

  ptr = realloc(_id_array, sizeof(uint16_t) * _extension_tree.count);
  if (!ptr) {
    return;
  }

  /* add to tree */
  ext->_node.key = &ext->id;
  avl_insert(&_extension_tree, &ext->_node);

  /* refresh id array */
  _id_array_length = 0;
  _id_array = ptr;

  avl_for_each_element(&_extension_tree, ext, _node) {
    if (ext->id >= 0 && ext->id <= 0xffff) {
      ptr[_id_array_length] = htons(ext->id);
      _id_array_length++;
    }
  }
}

struct avl_tree *
dlep_extension_get_tree(void) {
  return &_extension_tree;
}

void
dlep_extension_add_processing(struct dlep_extension *ext, bool radio,
    struct dlep_extension_implementation *processing, size_t proc_count) {
  size_t i,j;

  for (j=0; j<proc_count; j++) {
    for (i=0; i<ext->signal_count; i++) {
      if (ext->signals[i].id == processing[j].id) {
        if (radio) {
          ext->signals[i].process_radio = processing[j].process;
          ext->signals[i].add_radio_tlvs = processing[j].add_tlvs;
        }
        else {
          ext->signals[i].process_router = processing[j].process;
          ext->signals[i].add_router_tlvs = processing[j].add_tlvs;
        }
        break;
      }
    }
  }
}

const uint16_t *
dlep_extension_get_ids(uint16_t *length) {
  *length = _id_array_length;
  return _id_array;
}

int
dlep_extension_router_process_peer_init_ack(
    struct dlep_extension *ext, struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  int result;

  if (session->restrict_signal != DLEP_PEER_INITIALIZATION_ACK) {
    /* ignore unless we are in initialization mode */
    return 0;
  }

  l2net = oonf_layer2_net_add(session->l2_listener.name);
  if (!l2net) {
    OONF_INFO(session->log_source,
        "Could not find l2net for interface");
    return -1;
  }

  result = dlep_reader_map_l2neigh_data(l2net->neighdata, session, ext);
  if (result) {
    OONF_INFO(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }

  result = dlep_reader_map_l2net_data(l2net->data, session, ext);
  if (result) {
    OONF_INFO(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }
  return 0;
}

int
dlep_extension_router_process_peer_update(
    struct dlep_extension *ext, struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  int result;

  if (session->restrict_signal != DLEP_PEER_INITIALIZATION_ACK) {
    /* ignore unless we are in initialization mode */
    return 0;
  }

  l2net = oonf_layer2_net_add(session->l2_listener.name);
  if (!l2net) {
    OONF_INFO(session->log_source, "Could not add l2net for new interface");
    return -1;
  }

  result = dlep_reader_map_l2neigh_data(l2net->neighdata, session, ext);
  if (result) {
    OONF_INFO(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }

  result = dlep_reader_map_l2net_data(l2net->data, session, ext);
  if (result) {
    OONF_INFO(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }
  return 0;
}

int
dlep_extension_router_process_destination(
    struct dlep_extension *ext, struct dlep_session *session) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr mac;
  int result;

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

  result = dlep_reader_map_l2neigh_data(l2neigh->data, session, ext);
  if (result) {
    OONF_INFO(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }
  return 0;
}

int
dlep_extension_radio_write_peer_init_ack(
    struct dlep_extension *ext, struct dlep_session *session,
    const struct netaddr *neigh __attribute__((unused))) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_data *l2data;
  size_t i;
  int result;

  /* first make sure defaults are set correctly */
  l2net = oonf_layer2_net_add(session->l2_listener.name);
  if (!l2net) {
    OONF_WARN(session->log_source, "Could not add l2net for new interface");
    return -1;
  }

  /* adding default neighbor data for mandatory values */
  for (i=0; i<ext->neigh_mapping_count; i++) {
    if (!ext->neigh_mapping[i].mandatory) {
      continue;
    }

    l2data = &l2net->neighdata[ext->neigh_mapping[i].layer2];

    if (!oonf_layer2_has_value(l2data)) {
      oonf_layer2_set_value(l2data, session->l2_origin,
          ext->neigh_mapping[i].default_value);
    }
  }

  /* adding default interface data for mandatory values */
  for (i=0; i<ext->if_mapping_count; i++) {
    if (!ext->if_mapping[i].mandatory) {
      continue;
    }

    l2data = &l2net->data[ext->if_mapping[i].layer2];

    if (!oonf_layer2_has_value(l2data)) {
      oonf_layer2_set_value(l2data, session->l2_origin,
          ext->if_mapping[i].default_value);
    }
  }

  /* write default metric values */
  result = dlep_writer_map_l2neigh_data(&session->writer, ext,
      l2net->neighdata, NULL);
  if (result) {
    OONF_WARN(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }

  /* write network wide data */
  result = dlep_writer_map_l2net_data(&session->writer, ext, l2net->data);
  if(result) {
    OONF_WARN(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }
  return 0;
}

int
dlep_extension_radio_write_peer_update(
    struct dlep_extension *ext, struct dlep_session *session,
    const struct netaddr *neigh __attribute__((unused))) {
  struct oonf_layer2_net *l2net;
  int result;

  l2net = oonf_layer2_net_get(session->l2_listener.name);
  if (!l2net) {
    OONF_WARN(session->log_source, "Could not find l2net for new interface");
    return -1;
  }

  result = dlep_writer_map_l2neigh_data(&session->writer, ext,
      l2net->neighdata, NULL);
  if (result) {
    OONF_WARN(session->log_source, "tlv mapping for extension %d failed: %d",
        ext->id, result);
    return result;
  }
  return 0;
}

int
dlep_extension_radio_write_destination(struct dlep_extension *ext,
    struct dlep_session *session, const struct netaddr *neigh) {
  struct oonf_layer2_neigh *l2neigh;
  struct netaddr_str nbuf;
  int result;

  l2neigh = dlep_session_get_local_l2_neighbor(session, neigh);
  if (!l2neigh) {
    OONF_WARN(session->log_source, "Could not find l2neigh "
        "for neighbor %s", netaddr_to_string(&nbuf, neigh));
    return -1;
  }

  result = dlep_writer_map_l2neigh_data(&session->writer, ext,
      l2neigh->data, l2neigh->network->neighdata);
  if (result) {
    OONF_WARN(session->log_source, "tlv mapping for extension %d"
        " and neighbor %s failed: %d",
        ext->id, netaddr_to_string(&nbuf, neigh), result);
    return result;
  }
  return 0;
}
