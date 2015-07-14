/*
 * dlep_radio_base_extension.c
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#include "common/common_types.h"
#include "common/avl.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_iana.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"

#include "dlep/ext_l1_statistics/l1_statistics.h"

static int dlep_reader_map_array(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv,
    enum oonf_layer2_network_index l2idx);
static int dlep_reader_map_frequency(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv);
static int dlep_reader_map_bandwidth(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv);

static int dlep_writer_map_array(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length,
    enum oonf_layer2_network_index l2idx);
static int dlep_writer_map_frequency(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length);
static int dlep_writer_map_bandwidth(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length);

/* peer initialization ack */
static const uint16_t _peer_initack_tlvs[] = {
    DLEP_FREQUENCY_TLV,
    DLEP_BANDWIDTH_TLV,
    DLEP_NOISE_LEVEL_TLV,
    DLEP_CHANNEL_ACTIVE_TLV,
    DLEP_CHANNEL_BUSY_TLV,
    DLEP_CHANNEL_RX_TLV,
    DLEP_CHANNEL_TX_TLV,
    DLEP_SIGNAL_RX_TLV,
    DLEP_SIGNAL_TX_TLV,
};
static const uint16_t _peer_initack_mandatory[] = {
    DLEP_FREQUENCY_TLV,
    DLEP_BANDWIDTH_TLV,
};

/* peer update */
static const uint16_t _peer_update_tlvs[] = {
    DLEP_FREQUENCY_TLV,
    DLEP_BANDWIDTH_TLV,
    DLEP_NOISE_LEVEL_TLV,
    DLEP_CHANNEL_ACTIVE_TLV,
    DLEP_CHANNEL_BUSY_TLV,
    DLEP_CHANNEL_RX_TLV,
    DLEP_CHANNEL_TX_TLV,
    DLEP_SIGNAL_RX_TLV,
    DLEP_SIGNAL_TX_TLV,
};

/* destination up/update */
static const uint16_t _dst_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_SIGNAL_RX_TLV,
    DLEP_SIGNAL_TX_TLV,
};
static const uint16_t _dst_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* supported signals of this extension */
static struct dlep_extension_signal _signals[] = {
    {
        .id = DLEP_PEER_INITIALIZATION_ACK,
        .supported_tlvs = _peer_initack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_initack_tlvs),
        .mandatory_tlvs = _peer_initack_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_peer_initack_mandatory),
        .add_radio_tlvs = dlep_extension_radio_write_peer_init_ack,
        .process_router = dlep_extension_router_process_peer_init_ack,
    },
    {
        .id = DLEP_PEER_UPDATE,
        .supported_tlvs = _peer_update_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_update_tlvs),
        .add_radio_tlvs = dlep_extension_radio_write_peer_update,
        .process_router = dlep_extension_router_process_peer_update,
    },
    {
        .id = DLEP_DESTINATION_UP,
        .supported_tlvs = _dst_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_tlvs),
        .mandatory_tlvs = _dst_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_mandatory),
        .add_radio_tlvs = dlep_extension_radio_write_destination,
        .process_router = dlep_extension_router_process_destination,
    },
    {
        .id = DLEP_DESTINATION_UPDATE,
        .supported_tlvs = _dst_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_tlvs),
        .mandatory_tlvs = _dst_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_mandatory),
        .add_radio_tlvs = dlep_extension_radio_write_destination,
        .process_router = dlep_extension_router_process_destination,
    },
};

/* supported TLVs of this extension */
static struct dlep_extension_tlv _tlvs[] = {
    { DLEP_FREQUENCY_TLV, 8, 16 },
    { DLEP_BANDWIDTH_TLV, 8, 16 },
    { DLEP_NOISE_LEVEL_TLV, 8, 8 },
    { DLEP_CHANNEL_ACTIVE_TLV, 8, 8 },
    { DLEP_CHANNEL_BUSY_TLV, 8, 8 },
    { DLEP_CHANNEL_RX_TLV, 8, 8 },
    { DLEP_CHANNEL_TX_TLV, 8, 8 },
    { DLEP_SIGNAL_RX_TLV, 8, 8 },
    { DLEP_SIGNAL_TX_TLV, 8, 8 },
};

static struct dlep_neighbor_mapping _neigh_mappings[] = {
    {
        .dlep          = DLEP_SIGNAL_RX_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_SIGNAL,
        .length        = 2,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_SIGNAL_TX_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_SIGNAL,
        .length        = 2,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
};

static struct dlep_network_mapping _net_mappings[] = {
    {
        .dlep          = DLEP_FREQUENCY_TLV,
        .layer2        = OONF_LAYER2_NET_FREQUENCY_1,
        .length        = 8,

        .mandatory     = true,

        .from_tlv      = dlep_reader_map_frequency,
        .to_tlv        = dlep_writer_map_frequency,
    },
    {
        .dlep          = DLEP_BANDWIDTH_TLV,
        .layer2        = OONF_LAYER2_NET_BANDWIDTH_1,
        .length        = 8,

        .mandatory     = true,

        .from_tlv      = dlep_reader_map_bandwidth,
        .to_tlv        = dlep_writer_map_bandwidth,
    },
    {
        .dlep          = DLEP_NOISE_LEVEL_TLV,
        .layer2        = OONF_LAYER2_NET_NOISE,
        .length        = 2,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CHANNEL_ACTIVE_TLV,
        .layer2        = OONF_LAYER2_NET_CHANNEL_ACTIVE,
        .length        = 8,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CHANNEL_BUSY_TLV,
        .layer2        = OONF_LAYER2_NET_CHANNEL_BUSY,
        .length        = 8,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CHANNEL_RX_TLV,
        .layer2        = OONF_LAYER2_NET_CHANNEL_RX,
        .length        = 8,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CHANNEL_TX_TLV,
        .layer2        = OONF_LAYER2_NET_CHANNEL_TX,
        .length        = 8,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
};

/* DLEP base extension, radio side */
static struct dlep_extension _l1_stats = {
  .id = DLEP_EXTENSION_L1_STATS,
  .name = "l1 stats",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),
  .neigh_mapping = _neigh_mappings,
  .neigh_mapping_count = ARRAYSIZE(_neigh_mappings),
  .if_mapping = _net_mappings,
  .if_mapping_count = ARRAYSIZE(_net_mappings),
};

struct dlep_extension *
dlep_l1_statistics_init(void) {
  if (avl_is_node_added(&_l1_stats._node)) {
    return &_l1_stats;
  }

  _l1_stats._node.key = &_l1_stats.id;
  avl_insert(dlep_extension_get_tree(), &_l1_stats._node);
  return &_l1_stats;
}

static int
dlep_reader_map_array(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv,
    enum oonf_layer2_network_index l2idx) {
  struct dlep_parser_value *value;
  int64_t l2value;
  const uint8_t *dlepvalue;
  uint64_t tmp64[2] = { 0,0 };

  value = dlep_session_get_tlv_value(session, dlep_tlv);
  if (!value) {
    return 0;
  }

  if (value->length != 8 && value->length != 16) {
    return -1;
  }

  dlepvalue = dlep_parser_get_tlv_binary(&session->parser, value);

  /* extract dlep TLV values and convert to host representation */
  if (value->length == 16) {
    memcpy(&tmp64[1], &dlepvalue[8], 8);
    tmp64[1] = be64toh(tmp64[1]);
  }
  memcpy(&tmp64[0], dlepvalue, 8);
  tmp64[0] = be64toh(tmp64[0]);

  /* copy into signed integer and set to l2 value */
  memcpy(&l2value, &tmp64[0], 8);
  oonf_layer2_set_value(data, session->l2_origin, l2value);

  if (value->length == 16) {
    switch (l2idx) {
      case OONF_LAYER2_NET_BANDWIDTH_1:
        data += (OONF_LAYER2_NET_BANDWIDTH_2 - OONF_LAYER2_NET_BANDWIDTH_1);
        break;
      case OONF_LAYER2_NET_FREQUENCY_1:
        data += (OONF_LAYER2_NET_FREQUENCY_2 - OONF_LAYER2_NET_FREQUENCY_1);
        break;
      default:
        return -1;
    }

    memcpy(&l2value, &tmp64[1], 8);
    oonf_layer2_set_value(data, session->l2_origin, l2value);
  }
  return 0;
}

static int
dlep_reader_map_frequency(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv) {
  return dlep_reader_map_array(data, session, dlep_tlv,
      OONF_LAYER2_NET_FREQUENCY_1);
}

static int
dlep_reader_map_bandwidth(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv) {
  return dlep_reader_map_array(data, session, dlep_tlv,
      OONF_LAYER2_NET_BANDWIDTH_1);
}

int
dlep_writer_map_array(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length,
    enum oonf_layer2_network_index l2idx) {
  struct oonf_layer2_data *data2;
  int64_t l2value;
  uint64_t tmp64[2];

  if (length != 8 && length != 16) {
    return -1;
  }
  if (length == 16) {
    switch (l2idx) {
      case OONF_LAYER2_NET_FREQUENCY_1:
        data2 = data +
          (OONF_LAYER2_NET_FREQUENCY_2 - OONF_LAYER2_NET_FREQUENCY_1);
        break;
      case OONF_LAYER2_NET_BANDWIDTH_1:
        data2 = data +
          (OONF_LAYER2_NET_BANDWIDTH_2 - OONF_LAYER2_NET_BANDWIDTH_1);
        break;
      default:
        return -1;
    }

    if (oonf_layer2_has_value(data2)) {
      l2value = oonf_layer2_get_value(data2);
      memcpy(&tmp64[1], &l2value, 8);
      tmp64[1] = htobe64(tmp64[1]);
      length = 16;
    }
  }

  l2value = oonf_layer2_get_value(data);
  memcpy(&tmp64[0], &l2value, 8);
  tmp64[0] = htobe64(tmp64[0]);

  dlep_writer_add_tlv(writer, tlv, &tmp64[0], length);
  return 0;
}

static int
dlep_writer_map_frequency(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length) {
  return dlep_writer_map_array(writer, data, tlv, length,
      OONF_LAYER2_NET_FREQUENCY_1);
}

static int
dlep_writer_map_bandwidth(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length) {
  return dlep_writer_map_array(writer, data, tlv, length,
      OONF_LAYER2_NET_BANDWIDTH_1);
}
