
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

#include <oonf/libcommon/avl.h>
#include <oonf/oonf.h>
#include <oonf/libcore/oonf_subsystem.h>
#include <oonf/base/os_interface.h>

/*! subsystem identifier */
#define OONF_LAYER2_SUBSYSTEM "layer2"

/*! memory class for layer2 neighbor */
#define LAYER2_CLASS_NEIGHBOR "layer2_neighbor"

/*! memory class for layer2 network */
#define LAYER2_CLASS_NETWORK "layer2_network"

/*! memory class for layer2 destination */
#define LAYER2_CLASS_DESTINATION "layer2_destination"

/*! memory class for layer2 network address */
#define LAYER2_CLASS_NETWORK_ADDRESS "layer2_network_address"

/*! memory class for layer2 neighbor address */
#define LAYER2_CLASS_NEIGHBOR_ADDRESS "layer2_neighbor_address"

/*! memory class for layer2 tracking of next link-id per neighbor */
#define LAYER2_CLASS_LID "layer2_lid"

enum {
  /*! maximum length of link id for layer2 neighbors */
  OONF_LAYER2_MAX_LINK_ID = 16,
};

/* configuration Macros for Layer2 keys */

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 interface keys
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_VALIDATE_LAYER2_NET_KEY(p_name, p_def, p_help, args...)                                                    \
  CFG_VALIDATE_CHOICE_CB_ARG(p_name, p_def, p_help, oonf_layer2_cfg_get_l2net_key, OONF_LAYER2_NET_COUNT, NULL, ##args)

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 interface keys
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_VALIDATE_LAYER2_NEIGH_KEY(p_name, p_def, p_help, args...)                                                  \
  CFG_VALIDATE_CHOICE_CB_ARG(                                                                                          \
    p_name, p_def, p_help, oonf_layer2_cfg_get_l2neigh_key, OONF_LAYER2_NEIGH_COUNT, NULL, ##args)

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 data comparators
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_VALIDATE_LAYER2_COMP(p_name, p_def, p_help, args...)                                                       \
  CFG_VALIDATE_CHOICE_CB_ARG(                                                                                          \
    p_name, p_def, p_help, oonf_layer2_cfg_get_l2comp, OONF_LAYER2_DATA_CMP_COUNT, NULL, ##args)

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 interface keys
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_MAP_CHOICE_L2NET(p_reference, p_field, p_name, p_def, p_help, args...)                                     \
  CFG_MAP_CHOICE_CB_ARG(                                                                                               \
    p_reference, p_field, p_name, p_def, p_help, oonf_layer2_cfg_get_l2net_key, OONF_LAYER2_NET_COUNT, NULL, ##args)

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 interface keys
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_MAP_CHOICE_L2NEIGH(p_reference, p_field, p_name, p_def, p_help, args...)                                   \
  CFG_MAP_CHOICE_CB_ARG(p_reference, p_field, p_name, p_def, p_help, oonf_layer2_cfg_get_l2neigh_key,                  \
    OONF_LAYER2_NEIGH_COUNT, NULL, ##args)

/**
 * Creates a cfg_schema_entry for a parameter that can be choosen
 * from the layer2 data comparators
 * @param p_name parameter name
 * @param p_def parameter default value
 * @param p_help help text for configuration entry
 * @param args variable list of additional arguments
 */
#define CFG_MAP_CHOICE_L2COMP(p_reference, p_field, p_name, p_def, p_help, args...)                                    \
  CFG_MAP_CHOICE_CB_ARG(                                                                                               \
    p_reference, p_field, p_name, p_def, p_help, oonf_layer2_cfg_get_l2comp, OONF_LAYER2_DATA_CMP_COUNT, NULL, ##args)

/**
 * priorities of layer2 originators
 */
enum oonf_layer2_origin_priority
{
  OONF_LAYER2_ORIGIN_UNKNOWN = 0,
  OONF_LAYER2_ORIGIN_UNRELIABLE = 10,
  OONF_LAYER2_ORIGIN_CONFIGURED = 20,
  OONF_LAYER2_ORIGIN_RELIABLE = 30,
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

  /*! true if this originator creates l2neighbor LID entries */
  bool lid;

  /* index number of the originator for LID creation */
  uint32_t lid_index;

  /*! node for tree of originators */
  struct avl_node _node;
};

enum oonf_layer2_data_type
{
  OONF_LAYER2_NO_DATA,
  OONF_LAYER2_INTEGER_DATA,
  OONF_LAYER2_BOOLEAN_DATA,
  OONF_LAYER2_NETWORK_DATA,

  OONF_LAYER2_DATA_TYPE_COUNT,
};

union oonf_layer2_value {
  int64_t integer;
  bool boolean;
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
};

/**
 * Comparator options for layer2 data
 */
enum oonf_layer2_data_comparator_type
{
  OONF_LAYER2_DATA_CMP_EQUALS,
  OONF_LAYER2_DATA_CMP_NOT_EQUALS,
  OONF_LAYER2_DATA_CMP_LESSER,
  OONF_LAYER2_DATA_CMP_LESSER_OR_EQUALS,
  OONF_LAYER2_DATA_CMP_GREATER,
  OONF_LAYER2_DATA_CMP_GREATER_OR_EQUALS,

  OONF_LAYER2_DATA_CMP_COUNT,

  OONF_LAYER2_DATA_CMP_ILLEGAL = -1,
};

/**
 * list of layer2 network metrics
 */
enum oonf_layer2_network_index
{
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

  /*! outgoing broadcast bitrate in bit/s */
  OONF_LAYER2_NET_TX_BC_BITRATE,

  /*! maixmum size of an ethernet/IP packet the router is allowed to send */
  OONF_LAYER2_NET_MTU,

  /*! true if unicast traffic is necessary for ratecontrol */
  OONF_LAYER2_NET_MCS_BY_PROBING,

  /*! true if interface does not support incoming broadcast/multicast */
  OONF_LAYER2_NET_RX_ONLY_UNICAST,

  /*! true if interface does not support incoming broadcast/multicast */
  OONF_LAYER2_NET_TX_ONLY_UNICAST,

  /*! true if radio provides multihop forwarding transparently */
  OONF_LAYER2_NET_RADIO_MULTIHOP,

  /*!
   * true if first frequency is uplink, second is downlink.
   * false if reported frequencies can be used for both reception/transmission
   */
  OONF_LAYER2_NET_BAND_UP_DOWN,

  /*! number of layer2 network metrics */
  OONF_LAYER2_NET_COUNT,
};

/**
 * list with types of layer2 networks
 */
enum oonf_layer2_network_type
{
  OONF_LAYER2_TYPE_UNDEFINED,
  OONF_LAYER2_TYPE_WIRELESS,
  OONF_LAYER2_TYPE_ETHERNET,
  OONF_LAYER2_TYPE_TUNNEL,

  OONF_LAYER2_TYPE_COUNT,
};

/**
 * list of layer2 neighbor metrics
 */
enum oonf_layer2_neighbor_index
{
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
  OONF_LAYER2_NEIGH_RX_THROUGHPUT,

  /*! average incoming throughput in bit/s */
  OONF_LAYER2_NEIGH_TX_THROUGHPUT,

  /*! total number of frame retransmission of other radio*/
  OONF_LAYER2_NEIGH_RX_RETRIES,

  /*! total number of frame retransmission */
  OONF_LAYER2_NEIGH_TX_RETRIES,

  /*! total number of failed frame receptions */
  OONF_LAYER2_NEIGH_RX_FAILED,

  /*! total number of failed frame transmissions */
  OONF_LAYER2_NEIGH_TX_FAILED,

  /*! relative transmission link quality (0-100) */
  OONF_LAYER2_NEIGH_TX_RLQ,

  /*! relative receiver link quality (0-100) */
  OONF_LAYER2_NEIGH_RX_RLQ,

  /*! incoming broadcast bitrate in bit/s */
  OONF_LAYER2_NEIGH_RX_BC_BITRATE,

  /*! incoming broadcast loss in 1/1000 */
  OONF_LAYER2_NEIGH_RX_BC_LOSS,

  /*! latency to neighbor in microseconds */
  OONF_LAYER2_NEIGH_LATENCY,

  /*! available resources of radio (0-100) */
  OONF_LAYER2_NEIGH_RESOURCES,

  /*! number of radio hops to neighbor, only available for multihop capable radios */
  OONF_LAYER2_NEIGH_RADIO_HOPCOUNT,

  /*!
   *IP hopcount (including ethernet between radio and router) to neighbor router,
   * only available for multihop capable radios
   */
  OONF_LAYER2_NEIGH_IP_HOPCOUNT,

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

  /*! global tree of all remote neighbor IPs */
  struct avl_tree remote_neighbor_ips;

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

  /*! node for global tree of network IP addresses */
  struct avl_node _global_node;

  /*! node for tree of ip addresses in network */
  struct avl_node _net_node;
};

/**
 * unique key of a layer2 neighbor
 */
struct oonf_layer2_neigh_key {
  /*! mac address of the neighbor */
  struct netaddr addr;

  /*! length of link id in octets */
  uint8_t link_id_length;

  /*! stored link id of neighbor (padded with zero bytes) */
  uint8_t link_id[OONF_LAYER2_MAX_LINK_ID];
};

/**
 * String buffer for text representation of neighbor key
 */
union oonf_layer2_neigh_key_str {
  struct netaddr_str nbuf;
  char buf[sizeof(struct netaddr_str) + 5 + 16*2];
};

/**
 * representation of a remote layer2 neighbor
 */
struct oonf_layer2_neigh {
  /*! remote mac address of neighbor */
  struct oonf_layer2_neigh_key key;

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

  /*! (interface) global tree of neighbor IP addresses */
  struct avl_node _net_node;

  /*! node for tree of ip addresses */
  struct avl_node _neigh_node;
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

struct oonf_layer2_lid {
  struct netaddr mac;

  uint32_t next_id;

  struct avl_node _node;
};

EXPORT void oonf_layer2_origin_add(struct oonf_layer2_origin *origin);
EXPORT void oonf_layer2_origin_remove(struct oonf_layer2_origin *origin);

EXPORT int oonf_layer2_data_parse_string(
  union oonf_layer2_value *value, const struct oonf_layer2_metadata *meta, const char *input);
EXPORT const char *oonf_layer2_data_to_string(
  char *buffer, size_t length, const struct oonf_layer2_data *data, const struct oonf_layer2_metadata *meta, bool raw);
EXPORT bool oonf_layer2_data_set(struct oonf_layer2_data *data, const struct oonf_layer2_origin *origin,
  enum oonf_layer2_data_type type, const union oonf_layer2_value *input);
EXPORT bool oonf_layer2_data_compare(const union oonf_layer2_value *left, const union oonf_layer2_value *right,
  enum oonf_layer2_data_comparator_type comparator, enum oonf_layer2_data_type data_type);
EXPORT enum oonf_layer2_data_comparator_type oonf_layer2_data_get_comparator(const char *);
EXPORT const char *oonf_layer2_data_get_comparator_string(enum oonf_layer2_data_comparator_type type);
EXPORT const char *oonf_layer2_data_get_type_string(enum oonf_layer2_data_type type);

EXPORT struct oonf_layer2_net *oonf_layer2_net_add(const char *ifname);
EXPORT bool oonf_layer2_net_remove(struct oonf_layer2_net *, const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_net_cleanup(
  struct oonf_layer2_net *l2net, const struct oonf_layer2_origin *origin, bool cleanup_neigh);
EXPORT bool oonf_layer2_net_commit(struct oonf_layer2_net *);
EXPORT void oonf_layer2_net_relabel(struct oonf_layer2_net *l2net, const struct oonf_layer2_origin *new_origin,
  const struct oonf_layer2_origin *old_origin);
EXPORT struct oonf_layer2_peer_address *oonf_layer2_net_add_ip(
  struct oonf_layer2_net *l2net, const struct oonf_layer2_origin *origin, const struct netaddr *ip);
EXPORT int oonf_layer2_net_remove_ip(struct oonf_layer2_peer_address *ip, const struct oonf_layer2_origin *origin);
EXPORT struct oonf_layer2_neighbor_address *oonf_layer2_net_get_best_neighbor_match(const struct netaddr *addr);
EXPORT struct avl_tree *oonf_layer2_net_get_remote_ip_tree(void);

EXPORT int oonf_layer2_neigh_generate_lid(struct oonf_layer2_neigh_key *, struct oonf_layer2_origin *origin, const struct netaddr *mac);
EXPORT struct oonf_layer2_neigh *oonf_layer2_neigh_add_lid(struct oonf_layer2_net *, const struct oonf_layer2_neigh_key *key);
EXPORT bool oonf_layer2_neigh_cleanup(struct oonf_layer2_neigh *l2neigh, const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_neigh_remove(struct oonf_layer2_neigh *l2neigh, const struct oonf_layer2_origin *origin);
EXPORT bool oonf_layer2_neigh_commit(struct oonf_layer2_neigh *l2neigh);
EXPORT void oonf_layer2_neigh_relabel(struct oonf_layer2_neigh *l2neigh, const struct oonf_layer2_origin *new_origin,
  const struct oonf_layer2_origin *old_origin);
EXPORT struct oonf_layer2_neighbor_address *oonf_layer2_neigh_add_ip(
  struct oonf_layer2_neigh *l2neigh, const struct oonf_layer2_origin *origin, const struct netaddr *ip);
EXPORT int oonf_layer2_neigh_remove_ip(
  struct oonf_layer2_neighbor_address *ip, const struct oonf_layer2_origin *origin);

EXPORT struct oonf_layer2_destination *oonf_layer2_destination_add(
  struct oonf_layer2_neigh *l2neigh, const struct netaddr *destination, const struct oonf_layer2_origin *origin);
EXPORT void oonf_layer2_destination_remove(struct oonf_layer2_destination *);

EXPORT struct oonf_layer2_data *oonf_layer2_neigh_query(
  const char *ifname, const struct netaddr *l2neigh, enum oonf_layer2_neighbor_index idx, bool get_default);
EXPORT struct oonf_layer2_data *oonf_layer2_neigh_add_path(
  const char *ifname, const struct netaddr *l2neigh_addr, enum oonf_layer2_neighbor_index idx);
EXPORT struct oonf_layer2_data *oonf_layer2_neigh_get_data(
  struct oonf_layer2_neigh *l2neigh, enum oonf_layer2_neighbor_index idx);

EXPORT const struct oonf_layer2_metadata *oonf_layer2_neigh_metadata_get(enum oonf_layer2_neighbor_index);
EXPORT const struct oonf_layer2_metadata *oonf_layer2_net_metadata_get(enum oonf_layer2_network_index);
EXPORT const char *oonf_layer2_cfg_get_l2net_key(size_t index, const void *unused);
EXPORT const char *oonf_layer2_cfg_get_l2neigh_key(size_t index, const void *unused);
EXPORT const char *oonf_layer2_cfg_get_l2comp(size_t index, const void *unused);

EXPORT const char *oonf_layer2_net_get_type_name(enum oonf_layer2_network_type);

EXPORT struct avl_tree *oonf_layer2_get_net_tree(void);
EXPORT struct avl_tree *oonf_layer2_get_origin_tree(void);
EXPORT int oonf_layer2_avlcmp_neigh_key(const void *p1, const void *p2);
EXPORT const char *oonf_layer2_neigh_key_to_string(union oonf_layer2_neigh_key_str *buf,
    const struct oonf_layer2_neigh_key *key, bool show_mac);

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
  return avl_find_element(oonf_layer2_get_net_tree(), ifname, l2net, _node);
}

/**
 * Get a layer-2 local peer ip address object from the database
 * @param l2net layer-2 network/interface object
 * @param addr ip address of local radio/modem
 * @return layer-2 ip address object, NULL if not found
 */
static INLINE struct oonf_layer2_peer_address *
oonf_layer2_net_get_local_ip(const struct oonf_layer2_net *l2net, const struct netaddr *addr) {
  struct oonf_layer2_peer_address *l2ip;
  return avl_find_element(&l2net->local_peer_ips, addr, l2ip, _net_node);
}

/**
 * Get a layer-2 ip address object from the database
 * @param l2net layer-2 network object
 * @param addr ip address of remote router
 * @return layer-2 ip address object, NULL if not found
 */
static INLINE struct oonf_layer2_neighbor_address *
oonf_layer2_net_get_remote_ip(const struct oonf_layer2_net *l2net, const struct netaddr *addr) {
  struct oonf_layer2_neighbor_address *l2ip;
  return avl_find_element(&l2net->remote_neighbor_ips, addr, l2ip, _net_node);
}

static INLINE struct oonf_layer2_neigh *
oonf_layer2_neigh_add(struct oonf_layer2_net *net, const struct netaddr *l2neigh) {
  struct oonf_layer2_neigh_key key;

  memset(&key, 0, sizeof(key));
  memcpy(&key.addr, l2neigh, sizeof(*l2neigh));
  return oonf_layer2_neigh_add_lid(net, &key);
}

/**
 * Get a layer-2 neighbor object from the database
 * @param l2net layer-2 network/interface object
 * @param addr remote mac address of neighbor
 * @return layer-2 neighbor object, NULL if not found
 */
static INLINE struct oonf_layer2_neigh *
oonf_layer2_neigh_get(const struct oonf_layer2_net *l2net, const struct netaddr *addr) {
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_neigh_key key;

  memset(&key, 0, sizeof(key));
  memcpy(&key.addr, addr, sizeof(*addr));
  return avl_find_element(&l2net->neighbors, &key, l2neigh, _node);
}

/**
 * Get a layer-2 neighbor object from the database
 * @param l2net layer-2 network/interface object
 * @param key unique key of remote neighbor
 * @return layer-2 neighbor object, NULL if not found
 */
static INLINE struct oonf_layer2_neigh *
oonf_layer2_neigh_get_lid(const struct oonf_layer2_net *l2net, const struct oonf_layer2_neigh_key *key) {
  struct oonf_layer2_neigh *l2neigh;
  return avl_find_element(&l2net->neighbors, key, l2neigh, _node);
}

/**
 * Get a layer-2 ip address object from the database
 * @param l2neigh layer-2 neighbor object
 * @param addr ip address of remote router
 * @return layer-2 ip address object, NULL if not found
 */
static INLINE struct oonf_layer2_neighbor_address *
oonf_layer2_neigh_get_remote_ip(const struct oonf_layer2_neigh *l2neigh, const struct netaddr *addr) {
  struct oonf_layer2_neighbor_address *l2ip;
  return avl_find_element(&l2neigh->remote_neighbor_ips, addr, l2ip, _neigh_node);
}

/**
 * Get a layer-2 destination (secondary MAC) for a neighbor
 * @param l2neigh layer-2 neighbor object
 * @param destination mac address of destination
 * @return layer-2 destination object, NULL if not found
 */
static INLINE struct oonf_layer2_destination *
oonf_layer2_destination_get(const struct oonf_layer2_neigh *l2neigh, const struct netaddr *destination) {
  struct oonf_layer2_destination *l2dst;
  return avl_find_element(&l2neigh->destinations, destination, l2dst, _node);
}

/**
 * @param l2data layer-2 data object
 * @return true if object contains a value, false otherwise
 */
static INLINE bool
oonf_layer2_data_has_value(const struct oonf_layer2_data *l2data) {
  return l2data->_type != OONF_LAYER2_NO_DATA;
}

/**
 * @param l2data layer-2 data object
 * @return type of data in object
 */
static INLINE enum oonf_layer2_data_type
oonf_layer2_data_get_type(const struct oonf_layer2_data *l2data) {
  return l2data->_type;
}

/**
 * @param l2data layer-2 data object
 * @param def default value to return
 * @return value of data object, default value if not net
 */
static INLINE int64_t
oonf_layer2_data_get_int64(const struct oonf_layer2_data *l2data, int64_t def) {
  if (l2data->_type != OONF_LAYER2_INTEGER_DATA) {
    return def;
  }
  return l2data->_value.integer;
}

/**
 * @param l2data layer-2 data object
 * @param def default value to return
 * @return value of data object, default value if not net
 */
static INLINE bool
oonf_layer2_data_get_boolean(const struct oonf_layer2_data *l2data, bool def) {
  if (l2data->_type != OONF_LAYER2_BOOLEAN_DATA) {
    return def;
  }
  return l2data->_value.boolean;
}

/**
 * @param buffer pointer to int64 data storage
 * @param l2data layer-2 data object
 * @return 0 if value was read, -1 if it was the wrong type
 */
static INLINE int
oonf_layer2_data_read_int64(int64_t *buffer, const struct oonf_layer2_data *l2data) {
  if (l2data->_type != OONF_LAYER2_INTEGER_DATA) {
    return -1;
  }
  *buffer = l2data->_value.integer;
  return 0;
}

/**
 * @param buffer pointer to boolean data storage
 * @param l2data layer-2 data object
 * @return 0 if value was read, -1 if it was the wrong type
 */
static INLINE int
oonf_layer2_data_read_boolean(bool *buffer, const struct oonf_layer2_data *l2data) {
  if (l2data->_type != OONF_LAYER2_BOOLEAN_DATA) {
    return -1;
  }
  *buffer = l2data->_value.boolean;
  return 0;
}

/**
 * @param l2data layer-2 data object
 * @return originator of data value
 */
static INLINE const struct oonf_layer2_origin *
oonf_layer2_data_get_origin(const struct oonf_layer2_data *l2data) {
  return l2data->_origin;
}

/**
 * Sets the originator of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of data value
 */
static INLINE void
oonf_layer2_data_set_origin(struct oonf_layer2_data *l2data, const struct oonf_layer2_origin *origin) {
  l2data->_origin = origin;
}

static INLINE bool
oonf_layer2_data_from_string(struct oonf_layer2_data *data, const struct oonf_layer2_origin *origin,
  const struct oonf_layer2_metadata *meta, const char *input) {
  union oonf_layer2_value value;

  if (oonf_layer2_data_parse_string(&value, meta, input)) {
    return false;
  }
  return oonf_layer2_data_set(data, origin, meta->type, &value);
}

static INLINE const char *
oonf_layer2_net_data_to_string(
  char *buffer, size_t length, const struct oonf_layer2_data *data, enum oonf_layer2_network_index idx, bool raw) {
  return oonf_layer2_data_to_string(buffer, length, data, oonf_layer2_net_metadata_get(idx), raw);
}

static INLINE const char *
oonf_layer2_neigh_data_to_string(
  char *buffer, size_t length, const struct oonf_layer2_data *data, enum oonf_layer2_neighbor_index idx, bool raw) {
  return oonf_layer2_data_to_string(buffer, length, data, oonf_layer2_neigh_metadata_get(idx), raw);
}

/**
 * Set the value of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of value
 * @param integer new value for data object
 * @return true if value was overwrite, false otherwise
 */
static INLINE bool
oonf_layer2_data_set_int64(struct oonf_layer2_data *l2data, const struct oonf_layer2_origin *origin, int64_t integer) {
  union oonf_layer2_value value = { 0 };
  value.integer = integer;

  return oonf_layer2_data_set(l2data, origin, OONF_LAYER2_INTEGER_DATA, &value);
}

/**
 * Set the value of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of value
 * @param boolean new value for data object
 * @return true if value was overwrite, false otherwise
 */
static INLINE bool
oonf_layer2_data_set_bool(struct oonf_layer2_data *l2data, const struct oonf_layer2_origin *origin, bool boolean) {
  union oonf_layer2_value value = { 0 };
  value.boolean = boolean;
  return oonf_layer2_data_set(l2data, origin, OONF_LAYER2_BOOLEAN_DATA, &value);
}

static INLINE int
oonf_layer2_net_data_from_string(struct oonf_layer2_data *data, enum oonf_layer2_network_index idx,
  struct oonf_layer2_origin *origin, const char *input) {
  return oonf_layer2_data_from_string(data, origin, oonf_layer2_net_metadata_get(idx), input);
}

static INLINE int
oonf_layer2_neigh_data_from_string(struct oonf_layer2_data *data, enum oonf_layer2_neighbor_index idx,
  struct oonf_layer2_origin *origin, const char *input) {
  return oonf_layer2_data_from_string(data, origin, oonf_layer2_neigh_metadata_get(idx), input);
}

/**
 * Removes the value of a layer-2 data object
 * @param l2data layer-2 data object
 */
static INLINE void
oonf_layer2_data_reset(struct oonf_layer2_data *l2data) {
  l2data->_type = OONF_LAYER2_NO_DATA;
  l2data->_origin = NULL;
}

#endif /* OONF_LAYER2_H_ */
