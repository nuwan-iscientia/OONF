
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

#ifndef OONF_LAYER2_H_
#define OONF_LAYER2_H_

#include "common/avl.h"
#include "common/common_types.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_interface.h"

/*! subsystem identifier */
#define OONF_LAYER2_SUBSYSTEM "layer2"

/*! memory class for layer2 neighbor */
#define LAYER2_CLASS_NEIGHBOR    "layer2_neighbor"

/*! memory class for layer2 network */
#define LAYER2_CLASS_NETWORK     "layer2_network"

/*! memory class for layer2 destination */
#define LAYER2_CLASS_DESTINATION "layer2_destination"

/*! memory class for layer2 network address */
#define LAYER2_CLASS_NETWORK_ADDRESS "layer2_network_address"

/*! memory class for layer2 neighbor address */
#define LAYER2_CLASS_NEIGHBOR_ADDRESS "layer2_neighbor_address"

/**
 * priorities of layer2 originators
 */
enum oonf_layer2_origin_priority {
  OONF_LAYER2_ORIGIN_UNKNOWN,
  OONF_LAYER2_ORIGIN_UNRELIABLE,
  OONF_LAYER2_ORIGIN_CONFIGURED,
  OONF_LAYER2_ORIGIN_RELIABLE,
};

/**
 * Origin for layer2 data
 */
struct oonf_layer2_origin {
  const char *name;

  /*! true if data will be constantly updated by a plugin */
  bool proactive;

  /*! priority of this originator */
  enum oonf_layer2_origin_priority priority;

  /*! node for tree of originators */
  struct avl_node _node;
};

enum oonf_layer2_data_type {
  OONF_LAYER2_NO_DATA,
  OONF_LAYER2_INTEGER_DATA,
  OONF_LAYER2_BOOLEAN_DATA,
};

union oonf_layer2_value {
  int64_t        integer;
  bool           boolean;
  struct netaddr addr;
};

/**
 * Single data entry of layer2 network or neighbor
 */
struct oonf_layer2_data {
  /*! data value */
  union oonf_layer2_value _value;

  /*! type of data contained in this element */
  enum oonf_layer2_data_type _type;

  /*! layer2 originator id */
  const struct oonf_layer2_origin *_origin;
};

/**
 * Metadata of layer2 data entry for automatic processing
 */
struct oonf_layer2_metadata {
  /*! type of data */
  const char key[16];

  /*! data type */
  enum oonf_layer2_data_type type;

  /*! unit (bit/s, byte, ...) */
  const char unit[8];

  /*! number of fractional digits of the data */
  const int fraction;

  /*! true if data is "base" 1024 instead of "base" 1000 */
  const bool binary;
};

/**
 * list of layer2 network metrics
 */
enum oonf_layer2_network_index {
  /*! primary center frequency */
  OONF_LAYER2_NET_FREQUENCY_1,

  /*! optional secondary center frequency */
  OONF_LAYER2_NET_FREQUENCY_2,

  /*! primary bandwidth */
  OONF_LAYER2_NET_BANDWIDTH_1,

  /*! optional secondary bandwidth */
  OONF_LAYER2_NET_BANDWIDTH_2,

  /*! noise level in dBm */
  OONF_LAYER2_NET_NOISE,

  /*! total time in ns the channel was active */
  OONF_LAYER2_NET_CHANNEL_ACTIVE,

  /*! total time in ns the channel was busy */
  OONF_LAYER2_NET_CHANNEL_BUSY,

  /*! total time in ns the channel was receiving */
  OONF_LAYER2_NET_CHANNEL_RX,

  /*! total time in ns the channel was transmitting */
  OONF_LAYER2_NET_CHANNEL_TX,

  /*! maixmum size of an IP packet for this interface */
  OONF_LAYER2_NET_MTU,

  /*! true if unicast traffic is necessary for ratecontrol */
  OONF_LAYER2_NET_MCS_BY_PROBING,

  /*! true if interface does not support incoming broadcast/multicast */
  OONF_LAYER2_NET_RX_ONLY_UNICAST,

  /*! true if interface does not support incoming broadcast/multicast */
  OONF_LAYER2_NET_TX_ONLY_UNICAST,

  /*! number of layer2 network metrics */
  OONF_LAYER2_NET_COUNT,
};

/**
 * list with types of layer2 networks
 */
enum oonf_layer2_network_type {
  OONF_LAYER2_TYPE_UNDEFINED,
  OONF_LAYER2_TYPE_WIRELESS,
  OONF_LAYER2_TYPE_ETHERNET,
  OONF_LAYER2_TYPE_TUNNEL,

  OONF_LAYER2_TYPE_COUNT,
};

/**
 * list of layer2 neighbor metrics
 */
enum oonf_layer2_neighbor_index {
  /*! outgoing signal in milli dBm */
  OONF_LAYER2_NEIGH_TX_SIGNAL,

  /*! incoming signal in milli dBm */
  OONF_LAYER2_NEIGH_RX_SIGNAL,

  /*! outgoing bitrate in bit/s */
  OONF_LAYER2_NEIGH_TX_BITRATE,

  /*! incoming bitrate in bit/s */
  OONF_LAYER2_NEIGH_RX_BITRATE,

  /*! maximum possible outgoing bitrate in bit/s */
  OONF_LAYER2_NEIGH_TX_MAX_BITRATE,

  /*! maximum possible incoming bitrate in bit/s */
  OONF_LAYER2_NEIGH_RX_MAX_BITRATE,

  /*! total number of transmitted bytes */
  OONF_LAYER2_NEIGH_TX_BYTES,

  /*! total number of received bytes */
  OONF_LAYER2_NEIGH_RX_BYTES,

  /*! total number of transmitted frames */
  OONF_LAYER2_NEIGH_TX_FRAMES,

  /*! total number of received frames */
  OONF_LAYER2_NEIGH_RX_FRAMES,

  /*! average outgoing throughput in bit/s */
  OONF_LAYER2_NEIGH_TX_THROUGHPUT,

  /*! total number of frame retransmission */
  OONF_LAYER2_NEIGH_TX_RETRIES,

  /*! total number of failed frame transmissions */
  OONF_LAYER2_NEIGH_TX_FAILED,

  /*! latency to neighbor in microseconds */
  OONF_LAYER2_NEIGH_LATENCY,

  /*! available resources of radio (0-100) */
  OONF_LAYER2_NEIGH_RESOURCES,

  /*! relative transmission link quality (0-100) */
  OONF_LAYER2_NEIGH_TX_RLQ,

  /*! relative receiver link quality (0-100) */
  OONF_LAYER2_NEIGH_RX_RLQ,

  /*! number of neighbor metrics */
  OONF_LAYER2_NEIGH_COUNT,
};

/**
 * representation of a layer2 interface
 */
struct oonf_layer2_net {
  /*! name of local interface */
  char name[IF_NAMESIZE];

  /*! optional identification string */
  char if_ident[64];

  /*! interface type */
  enum oonf_layer2_network_type if_type;

  /*! interface data is delivered by DLEP */
  bool if_dlep;

  /*! interface listener to keep track of events and local mac address */
  struct os_interface_listener if_listener;

  /*! tree of remote neighbors */
  struct avl_tree neighbors;

  /*! tree of IP addresses/prefixes of local radio/modem */
  struct avl_tree local_peer_ips;

  /*! absolute timestamp when network has been active last */
  uint64_t last_seen;

  /*! network wide layer 2 data */
  struct oonf_layer2_data data[OONF_LAYER2_NET_COUNT];

  /*! default values of neighbor layer2 data */
  struct oonf_layer2_data neighdata[OONF_LAYER2_NEIGH_COUNT];

  /*! node to hook into global l2network tree */
  struct avl_node _node;
};

/**
 * IP addresses that are attached to a local radio/modem
 */
struct oonf_layer2_peer_address {
  /*! ip address attached to a local radio/modem */
  struct netaddr ip;

  /*! backlink to layer2 network */
  struct oonf_layer2_net *l2net;

  /*! origin of this address */
  const struct oonf_layer2_origin *origin;

  /*! node for tree of ip addresses */
  struct avl_node _node;
};

/**
 * representation of a remote layer2 neighbor
 */
struct oonf_layer2_neigh {
  /*! remote mac address of neighbor */
  struct netaddr addr;

  /*! back pointer to layer2 network */
  struct oonf_layer2_net *network;

  /*! tree of proxied destinations */
  struct avl_tree destinations;

  /*! tree of IP addresses/prefixes of remote neighbor router */
  struct avl_tree remote_neighbor_ips;

  /*! absolute timestamp when neighbor has been active last */
  uint64_t last_seen;

  /*! neigbor layer 2 data */
  struct oonf_layer2_data data[OONF_LAYER2_NEIGH_COUNT];

  /*! node to hook into tree of layer2 network */
  struct avl_node _node;
};

/**
 * IP addresses that are attached to a remote router
 */
struct oonf_layer2_neighbor_address {
  /*! ip address attached to a remote router */
  struct netaddr ip;

  /*! backlink to layer2 neighbor*/
  struct oonf_layer2_neigh *l2neigh;

  /*! origin of this address */
  const struct oonf_layer2_origin *origin;

  /*! node for tree of ip addresses */
  struct avl_node _node;
};

/**
 * representation of a bridged MAC address behind a layer2 neighbor
 */
struct oonf_layer2_destination {
  /*! proxied mac address behind a layer2 neighbor */
  struct netaddr destination;

  /*! back pointer to layer2 neighbor */
  struct oonf_layer2_neigh *neighbor;

  /*! origin of this proxied address */
  const struct oonf_layer2_origin *origin;

  /*! node to hook into tree of layer2 neighbor */
  struct avl_node _node;
};

EXPORT void oonf_layer2_add_origin(struct oonf_layer2_origin *origin);
EXPORT void oonf_layer2_remove_origin(struct oonf_layer2_origin *origin);

EXPORT int oonf_layer2_data_parse_string(union oonf_layer2_value *value,
    const struct oonf_layer2_metadata *meta,
    const char *input);
EXPORT int oonf_layer2_data_to_string(char *buffer, size_t length,
    const struct oonf_layer2_data *data,
    const struct oonf_layer2_metadata *meta, bool raw);
EXPORT bool oonf_layer2_data_set(struct oonf_layer2_data *data,
    const struct oonf_layer2_origin *origin,
    const struct oonf_layer2_metadata *meta,
    const union oonf_layer2_value *input);

EXPORT struct oonf_layer2_net *oonf_layer2_net_add(const char *ifname);
EXPORT bool oonf_layer2_net_remove(
    struct oonf_layer2_net *, const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_net_cleanup(struct oonf_layer2_net *l2net,
    const struct oonf_layer2_origin *origin, bool cleanup_neigh);
EXPORT bool oonf_layer2_net_commit(struct oonf_layer2_net *);
EXPORT void oonf_layer2_net_relabel(struct oonf_layer2_net *l2net,
    const struct oonf_layer2_origin *new_origin,
    const struct oonf_layer2_origin *old_origin);
EXPORT struct oonf_layer2_peer_address *oonf_layer2_net_add_ip(
    struct oonf_layer2_net *l2net,
    const struct oonf_layer2_origin *origin, const struct netaddr *ip);
EXPORT int oonf_layer2_net_remove_ip(
    struct oonf_layer2_peer_address *ip, const struct oonf_layer2_origin *origin);
EXPORT struct oonf_layer2_neighbor_address *oonf_layer2_net_get_best_neighbor_match(
    const struct netaddr *addr);

EXPORT struct oonf_layer2_neigh *oonf_layer2_neigh_add(
    struct oonf_layer2_net *, struct netaddr *l2neigh);
EXPORT bool oonf_layer2_neigh_cleanup(struct oonf_layer2_neigh *l2neigh,
    const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_neigh_remove(
    struct oonf_layer2_neigh *l2neigh,
    const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_neigh_commit(struct oonf_layer2_neigh *l2neigh);
EXPORT void oonf_layer2_neigh_relabel(struct oonf_layer2_neigh *l2neigh,
    const struct oonf_layer2_origin *new_origin,
    const struct oonf_layer2_origin *old_origin);
EXPORT struct oonf_layer2_neighbor_address *oonf_layer2_neigh_add_ip(
    struct oonf_layer2_neigh *l2neigh,
    const struct oonf_layer2_origin *origin, const struct netaddr *ip);
EXPORT int oonf_layer2_neigh_remove_ip(
    struct oonf_layer2_neighbor_address *ip, const struct oonf_layer2_origin *origin);

EXPORT struct oonf_layer2_destination *oonf_layer2_destination_add(
    struct oonf_layer2_neigh *l2neigh, const struct netaddr *destination,
    const struct oonf_layer2_origin *origin);
EXPORT void oonf_layer2_destination_remove(struct oonf_layer2_destination *);

EXPORT const struct oonf_layer2_data *oonf_layer2_neigh_query(
    const char *ifname, const struct netaddr *l2neigh,
    enum oonf_layer2_neighbor_index idx);
EXPORT const struct oonf_layer2_data *oonf_layer2_neigh_get_value(
    const struct oonf_layer2_neigh *l2neigh, enum oonf_layer2_neighbor_index idx);

EXPORT const struct oonf_layer2_metadata *oonf_layer2_get_neigh_metadata(
    enum oonf_layer2_neighbor_index);
EXPORT const struct oonf_layer2_metadata *oonf_layer2_get_net_metadata(
    enum oonf_layer2_network_index);
EXPORT const char *oonf_layer2_get_network_type(enum oonf_layer2_network_type);
EXPORT struct avl_tree *oonf_layer2_get_network_tree(void);
EXPORT struct avl_tree *oonf_layer2_get_origin_tree(void);

/**
 * Checks if a layer2 originator is registered
 * @param origin originator
 * @return true if registered, false otherwise
 */
static INLINE bool
oonf_layer2_origin_is_added(const struct oonf_layer2_origin *origin) {
  return avl_is_node_added(&origin->_node);
}

/**
 * Get a layer-2 interface object from the database
 * @param ifname name of interface
 * @return layer-2 addr object, NULL if not found
 */
static INLINE struct oonf_layer2_net *
oonf_layer2_net_get(const char *ifname) {
  struct oonf_layer2_net *l2net;
  return avl_find_element(oonf_layer2_get_network_tree(), ifname, l2net, _node);
}

/**
 * Get a layer-2 ip address object from the database
 * @param l2net layer-2 network/interface object
 * @param ip ip address of local radio/modem
 * @return layer-2 ip address object, NULL if not found
 */
static INLINE struct oonf_layer2_peer_address *
oonf_layer2_net_get_ip(const struct oonf_layer2_net *l2net,
    const struct netaddr *addr) {
  struct oonf_layer2_peer_address *l2ip;
  return avl_find_element(&l2net->local_peer_ips, addr, l2ip, _node);
}

/**
 * Get a layer-2 neighbor object from the database
 * @param l2net layer-2 network/interface object
 * @param addr remote mac address of neighbor
 * @return layer-2 neighbor object, NULL if not found
 */
static INLINE struct oonf_layer2_neigh *
oonf_layer2_neigh_get(const struct oonf_layer2_net *l2net,
    const struct netaddr *addr) {
  struct oonf_layer2_neigh *l2neigh;
  return avl_find_element(&l2net->neighbors, addr, l2neigh, _node);
}

/**
 * Get a layer-2 destination (secondary MAC) for a neighbor
 * @param l2neigh layer-2 neighbor object
 * @param destination mac address of destination
 * @return layer-2 destination object, NULL if not found
 */
static INLINE struct oonf_layer2_destination *
oonf_layer2_destination_get(const struct oonf_layer2_neigh *l2neigh,
    const struct netaddr *destination) {
  struct oonf_layer2_destination *l2dst;
  return avl_find_element(&l2neigh->destinations, destination, l2dst, _node);
}

/**
 * Get a layer-2 ip address object from the database
 * @param l2neigh layer-2 neighbor object
 * @param ip ip address of remote router
 * @return layer-2 ip address object, NULL if not found
 */
static INLINE struct oonf_layer2_neighbor_address *
oonf_layer2_neigh_get_ip(const struct oonf_layer2_neigh *l2neigh,
    const struct netaddr *addr) {
  struct oonf_layer2_neighbor_address *l2ip;
  return avl_find_element(&l2neigh->remote_neighbor_ips, addr, l2ip, _node);
}

/**
 * @param l2data layer-2 data object
 * @return true if object contains a value, false otherwise
 */
static INLINE bool
oonf_layer2_has_value(const struct oonf_layer2_data *l2data) {
  return l2data->_type != OONF_LAYER2_NO_DATA;
}

static INLINE enum oonf_layer2_data_type
oonf_layer2_data_get_type(const struct oonf_layer2_data *l2data) {
  return l2data->_type;
}

/**
 * @param l2data layer-2 data object
 * @return value of data object
 */
static INLINE int64_t
oonf_layer2_get_int64(const struct oonf_layer2_data *l2data) {
  assert (l2data->_type == OONF_LAYER2_INTEGER_DATA);
  return l2data->_value.integer;
}

/**
 * @param l2data layer-2 data object
 * @return value of data object
 */
static INLINE bool
oonf_layer2_get_boolean(const struct oonf_layer2_data *l2data) {
  assert (l2data->_type == OONF_LAYER2_BOOLEAN_DATA);
  return l2data->_value.boolean;
}

/**
 * @param l2data layer-2 data object
 * @return originator of data value
 */
static INLINE const struct oonf_layer2_origin *
oonf_layer2_get_origin(const struct oonf_layer2_data *l2data) {
  return l2data->_origin;
}

/**
 * Sets the originator of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of data value
 */
static INLINE void
oonf_layer2_set_origin(struct oonf_layer2_data *l2data,
    const struct oonf_layer2_origin *origin) {
  l2data->_origin = origin;
}

static INLINE bool
oonf_layer2_data_from_string(struct oonf_layer2_data *data,
    const struct oonf_layer2_origin *origin,
    const struct oonf_layer2_metadata *meta,
    const char *input) {
  union oonf_layer2_value value;

  if (oonf_layer2_data_parse_string(&value, meta, input)) {
    return false;
  }
  return oonf_layer2_data_set(data, origin, meta, &value);
}

static INLINE int
oonf_layer2_net_data_to_string(char *buffer, size_t length,
    const struct oonf_layer2_data *data, enum oonf_layer2_network_index idx, bool raw) {
  return oonf_layer2_data_to_string(buffer, length, data,
      oonf_layer2_get_net_metadata(idx), raw);
}

static INLINE int
oonf_layer2_neigh_data_to_string(char *buffer, size_t length,
    const struct oonf_layer2_data *data, enum oonf_layer2_neighbor_index idx, bool raw) {
  return oonf_layer2_data_to_string(buffer, length, data,
      oonf_layer2_get_neigh_metadata(idx), raw);
}

/**
 * Set the value of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of value
 * @param integer new value for data object
 * @return true if value was overwrite, false otherwise
 */
static INLINE bool
oonf_layer2_data_set_int64(struct oonf_layer2_data *l2data,
    const struct oonf_layer2_origin *origin,
    const struct oonf_layer2_metadata *meta, int64_t integer) {
  union oonf_layer2_value value = {0};
  value.integer = integer;

  return oonf_layer2_data_set(l2data, origin, meta, &value);
}

/**
 * Set the value of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of value
 * @param boolean new value for data object
 * @return true if value was overwrite, false otherwise
 */
static INLINE bool
oonf_layer2_data_set_bool(struct oonf_layer2_data *l2data,
    const struct oonf_layer2_origin *origin,
    const struct oonf_layer2_metadata *meta, bool boolean) {
  union oonf_layer2_value value = {0};
  value.boolean = boolean;
  return oonf_layer2_data_set(l2data, origin, meta, &value);
}

static INLINE int
oonf_layer2_net_data_from_string(
    struct oonf_layer2_data *data, enum oonf_layer2_network_index idx,
    struct oonf_layer2_origin *origin, const char *input) {
  return oonf_layer2_data_from_string(
      data, origin, oonf_layer2_get_net_metadata(idx), input);
}

static INLINE int
oonf_layer2_neigh_data_from_string(
    struct oonf_layer2_data *data, enum oonf_layer2_neighbor_index idx,
    struct oonf_layer2_origin *origin, const char *input) {
  return oonf_layer2_data_from_string(
      data, origin, oonf_layer2_get_neigh_metadata(idx), input);
}

/**
 * Removes the value of a layer-2 data object
 * @param l2data layer-2 data object
 */
static INLINE void
oonf_layer2_reset_value(struct oonf_layer2_data *l2data) {
  l2data->_type = OONF_LAYER2_NO_DATA;
  l2data->_origin = NULL;
}

#endif /* OONF_LAYER2_H_ */
