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

#include "dlep/ext_l2_statistics/l2_statistics.h"

/* peer initialization ack */
static const uint16_t _peer_initack_tlvs[] = {
    DLEP_FRAMES_R_TLV,
    DLEP_FRAMES_T_TLV,
    DLEP_FRAMES_RETRIES_TLV,
    DLEP_FRAMES_FAILED_TLV,
};
static const uint16_t _peer_initack_mandatory[] = {
    DLEP_FRAMES_R_TLV,
    DLEP_FRAMES_T_TLV,
};

/* peer update */
static const uint16_t _peer_update_tlvs[] = {
    DLEP_FRAMES_R_TLV,
    DLEP_FRAMES_T_TLV,
    DLEP_FRAMES_RETRIES_TLV,
    DLEP_FRAMES_FAILED_TLV,
};

/* destination up/update */
static const uint16_t _dst_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_FRAMES_R_TLV,
    DLEP_FRAMES_T_TLV,
    DLEP_FRAMES_RETRIES_TLV,
    DLEP_FRAMES_FAILED_TLV,
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
    { DLEP_FRAMES_R_TLV, 8, 8 },
    { DLEP_FRAMES_T_TLV, 8, 8 },
    { DLEP_FRAMES_RETRIES_TLV, 8, 8 },
    { DLEP_FRAMES_FAILED_TLV, 8, 8 },
};

static struct dlep_neighbor_mapping _neigh_mappings[] = {
    {
        .dlep          = DLEP_FRAMES_R_TLV,
        .layer2        = OONF_LAYER2_NEIGH_RX_FRAMES,
        .length        = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_FRAMES_T_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_FRAMES,
        .length        = 8,

        .mandatory     = true,
        .default_value = 0,

        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_FRAMES_RETRIES_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_RETRIES,
        .length        = 8,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
    {
        .dlep          = DLEP_FRAMES_FAILED_TLV,
        .layer2        = OONF_LAYER2_NEIGH_TX_FAILED,
        .length        = 8,
        .from_tlv      = dlep_reader_map_identity,
        .to_tlv        = dlep_writer_map_identity,
    },
};

/* DLEP base extension, radio side */
static struct dlep_extension _l2_stats = {
  .id = DLEP_EXTENSION_L2_STATS,
  .name = "l2 stats",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),
  .neigh_mapping = _neigh_mappings,
  .neigh_mapping_count = ARRAYSIZE(_neigh_mappings),
};

struct dlep_extension *
dlep_l2_statistics_init(void) {
  dlep_extension_add(&_l2_stats);
  return &_l2_stats;
}
