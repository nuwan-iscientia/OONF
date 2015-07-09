/*
 * dlep_radio_base_extension.c
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#include "common/common_types.h"
#include "common/avl.h"
#include "common/autobuf.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_extension.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"

#include "dlep/ext_base_metric/metric.h"

/* peer initialization ack */
static const uint16_t _peer_initack_tlvs[] = {
    DLEP_MDRR_TLV,
    DLEP_MDRT_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
    DLEP_RESR_TLV,
    DLEP_REST_TLV,
    DLEP_RLQR_TLV,
    DLEP_RLQT_TLV,
};
static const uint16_t _peer_initack_mandatory[] = {
    DLEP_MDRR_TLV,
    DLEP_MDRT_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
};

/* peer update */
static const uint16_t _peer_update_tlvs[] = {
    DLEP_MDRR_TLV,
    DLEP_MDRT_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
    DLEP_RESR_TLV,
    DLEP_REST_TLV,
    DLEP_RLQR_TLV,
    DLEP_RLQT_TLV,
};

/* destination up/update */
static const uint16_t _dst_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_MDRR_TLV,
    DLEP_MDRT_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
    DLEP_RESR_TLV,
    DLEP_REST_TLV,
    DLEP_RLQR_TLV,
    DLEP_RLQT_TLV,
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
    { DLEP_MAC_ADDRESS_TLV, 6,8 },
    { DLEP_MDRR_TLV, 8,8 },
    { DLEP_MDRT_TLV, 8,8 },
    { DLEP_CDRR_TLV, 8,8 },
    { DLEP_CDRT_TLV, 8,8 },
    { DLEP_LATENCY_TLV, 8,8 },
    { DLEP_RESR_TLV, 1,1 },
    { DLEP_REST_TLV, 1,1 },
    { DLEP_RLQR_TLV, 1,1 },
    { DLEP_RLQT_TLV, 1,1 },
};

static struct dlep_neighbor_mapping _neigh_mappings[] = {
    {
        .dlep          = DLEP_MDRR_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_MAX_BITRATE,
        .length        = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_MDRT_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_MAX_BITRATE,
        .length        = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CDRR_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_BITRATE,
        .length        = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_CDRT_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_BITRATE,
        .length        = 8,

        .mandatory     = true,
        .default_value = 1000000,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_LATENCY_TLV,
        .layer2   = OONF_LAYER2_NEIGH_LATENCY,
        .length   = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_RESR_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_RESOURCES,
        .length        = 1,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_REST_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_RESOURCES,
        .length        = 1,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_RLQR_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_RLQ,
        .length        = 1,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_RLQT_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_RLQ,
        .length        = 1,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
};

/* DLEP base extension, radio side */
static struct dlep_extension _base_metric = {
  .id = DLEP_EXTENSION_BASE_METRIC,
  .name = "base metric",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),
  .neigh_mapping = _neigh_mappings,
  .neigh_mapping_count = ARRAYSIZE(_neigh_mappings),

};

struct dlep_extension *
dlep_base_metric_init(void) {
  if (avl_is_node_added(&_base_metric._node)) {
    return &_base_metric;
  }

  _base_metric._node.key = &_base_metric.id;
  avl_insert(dlep_extension_get_tree(), &_base_metric._node);
  return &_base_metric;
}
