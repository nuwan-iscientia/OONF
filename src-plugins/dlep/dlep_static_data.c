/*
 * dlep_tlvdata.c
 *
 *  Created on: Sep 24, 2014
 *      Author: rogge
 */

#include "common/common_types.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_static_data.h"

struct dlep_bitmap dlep_mandatory_signals = { .b = {
  (1 << DLEP_PEER_DISCOVERY )
  | (1 << DLEP_PEER_OFFER )
  | (1 << DLEP_PEER_INITIALIZATION )
  | (1 << DLEP_PEER_INITIALIZATION_ACK )
  | (1 << DLEP_PEER_TERMINATION )
  | (1 << DLEP_PEER_TERMINATION_ACK )
  | (1 << DLEP_DESTINATION_UP )
  | (1 << DLEP_DESTINATION_UPDATE )
  | (1 << DLEP_DESTINATION_DOWN )
  | (1 << DLEP_HEARTBEAT),
  0,0,0
}};

struct dlep_bitmap dlep_mandatory_tlvs = { .b = {
  (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
  | (1 << DLEP_IPV4_ADDRESS_TLV )
  | (1 << DLEP_IPV6_ADDRESS_TLV )
  | (1 << DLEP_PORT_TLV )
  | (1 << DLEP_MDRR_TLV )
  | (1 << DLEP_MDRT_TLV )
  | (1 << DLEP_CDRR_TLV )
  | (1 << DLEP_CDRT_TLV )
  | (1 << DLEP_OPTIONAL_DATA_ITEMS_TLV )
  | (1 << DLEP_OPTIONAL_SIGNALS_TLV )
  | (1 << DLEP_STATUS_TLV )
  | (1 << DLEP_MAC_ADDRESS_TLV )
  | (1 << DLEP_PEER_TYPE_TLV),
  0,0,0
}};

struct dlep_bitmap dlep_supported_optional_signals = { .b = {
  (1 << DLEP_PEER_TERMINATION)
  | (1 << DLEP_PEER_TERMINATION_ACK)
  | (1 << DLEP_DESTINATION_UP_ACK)
  | (1 << DLEP_DESTINATION_DOWN_ACK),
  0,0,0
}};

struct dlep_bitmap dlep_supported_optional_tlvs = { .b = {
  (1 << DLEP_FRAMES_R_TLV)
  | (1 << DLEP_FRAMES_T_TLV)
  | (1 << DLEP_BYTES_R_TLV)
  | (1 << DLEP_BYTES_T_TLV)
  | (1 << DLEP_FRAMES_RETRIES_TLV)
  | (1 << DLEP_FRAMES_FAILED_TLV)
  | (1 << DLEP_SIGNAL_TLV),
  0,0,0
}};

struct dlep_bitmap dlep_mandatory_tlvs_per_signal[DLEP_SIGNAL_COUNT] = {
  [DLEP_PEER_DISCOVERY] = { .b = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV),
      0,0,0
  }},
  [DLEP_PEER_OFFER] = { .b = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_PORT_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION] = { .b = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_OPTIONAL_SIGNALS_TLV)
      | (1 << DLEP_OPTIONAL_DATA_ITEMS_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION_ACK] = { .b = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_MDRR_TLV)
      | (1 << DLEP_MDRT_TLV)
      | (1 << DLEP_CDRR_TLV)
      | (1 << DLEP_CDRT_TLV)
      | (1 << DLEP_OPTIONAL_SIGNALS_TLV)
      | (1 << DLEP_OPTIONAL_DATA_ITEMS_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION] = { .b = {
      0,0,0,0
  }},
  [DLEP_PEER_TERMINATION_ACK] = { .b = {
      0,0,0,0
  }},
  [DLEP_DESTINATION_UP] = { .b = {
      (1 << DLEP_MAC_ADDRESS_TLV)
      | (1 << DLEP_MDRR_TLV)
      | (1 << DLEP_MDRT_TLV)
      | (1 << DLEP_CDRR_TLV)
      | (1 << DLEP_CDRT_TLV)
  }},
  [DLEP_DESTINATION_UPDATE] = { .b = {
      (1 << DLEP_MAC_ADDRESS_TLV)
      | (1 << DLEP_MDRR_TLV)
      | (1 << DLEP_MDRT_TLV)
      | (1 << DLEP_CDRR_TLV)
      | (1 << DLEP_CDRT_TLV)
  }},
  [DLEP_DESTINATION_DOWN] = { .b = {
      (1 << DLEP_MAC_ADDRESS_TLV)
  }},
};

struct dlep_bitmap dlep_supported_optional_tlvs_per_signal[DLEP_SIGNAL_COUNT] = {
  [DLEP_PEER_DISCOVERY] = { .b = {
      0,0,0,0
  }},
  [DLEP_PEER_OFFER] = { .b = {
      (1 << DLEP_IPV4_ADDRESS_TLV)
      | (1 << DLEP_IPV6_ADDRESS_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION] = { .b = {
      (1 << DLEP_PEER_TYPE_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION_ACK] = { .b = {
      (1 << DLEP_PEER_TYPE_TLV)
      | (1 << DLEP_VENDOR_EXTENSION_TLV)
      | (1 << DLEP_FRAMES_R_TLV)
      | (1 << DLEP_FRAMES_T_TLV)
      | (1 << DLEP_BYTES_R_TLV)
      | (1 << DLEP_BYTES_T_TLV)
      | (1 << DLEP_FRAMES_RETRIES_TLV)
      | (1 << DLEP_FRAMES_FAILED_TLV)
      | (1 << DLEP_SIGNAL_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION] = { .b = {
      (1 << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION_ACK] = { .b = {
      (1 << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_UP] = { .b = {
      (1 << DLEP_FRAMES_R_TLV)
      | (1 << DLEP_FRAMES_T_TLV)
      | (1 << DLEP_BYTES_R_TLV)
      | (1 << DLEP_BYTES_T_TLV)
      | (1 << DLEP_FRAMES_RETRIES_TLV)
      | (1 << DLEP_FRAMES_FAILED_TLV)
      | (1 << DLEP_SIGNAL_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_UPDATE] = { .b = {
      (1 << DLEP_FRAMES_R_TLV)
      | (1 << DLEP_FRAMES_T_TLV)
      | (1 << DLEP_BYTES_R_TLV)
      | (1 << DLEP_BYTES_T_TLV)
      | (1 << DLEP_FRAMES_RETRIES_TLV)
      | (1 << DLEP_FRAMES_FAILED_TLV)
      | (1 << DLEP_SIGNAL_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_DOWN] = { .b = {
      0,0,0,0
  }},
};

struct dlep_tlvdata dlep_tlv_constraints[DLEP_TLV_COUNT] = {
    [DLEP_PORT_TLV] = { 2,2 },
    [DLEP_PEER_TYPE_TLV] = { 1,255 },
    [DLEP_MAC_ADDRESS_TLV] = { 6,6 },
    [DLEP_IPV4_ADDRESS_TLV] = { 5,5 },
    [DLEP_IPV6_ADDRESS_TLV] = { 17,17 },
    [DLEP_MDRR_TLV] = { 8,8 },
    [DLEP_MDRT_TLV] = { 8,8 },
    [DLEP_CDRR_TLV] = { 8,8 },
    [DLEP_CDRT_TLV] = { 8,8 },
    [DLEP_LATENCY_TLV] = { 4,4 },
    [DLEP_RESR_TLV] = { 1,1 },
    [DLEP_REST_TLV] = { 1,1 },
    [DLEP_RLQR_TLV] = { 1,1 },
    [DLEP_RLQT_TLV] = { 1,1 },
    [DLEP_STATUS_TLV] = { 1,1 },
    [DLEP_HEARTBEAT_INTERVAL_TLV] = { 2,2 },
    [DLEP_LINK_CHAR_ACK_TIMER_TLV] = { 1,1 },
    [DLEP_CREDIT_WIN_STATUS_TLV] = { 16,16 },
    [DLEP_CREDIT_GRANT_REQ_TLV] = { 8,8 },
    [DLEP_CREDIT_REQUEST_TLV] = { 1,1 },
    [DLEP_OPTIONAL_SIGNALS_TLV] = { 0,255 },
    [DLEP_OPTIONAL_DATA_ITEMS_TLV] = { 0,255 },
    [DLEP_VENDOR_EXTENSION_TLV] = { 3,255 },

    [DLEP_FRAMES_R_TLV] = { 8,8 },
    [DLEP_FRAMES_T_TLV] = { 8,8 },
    [DLEP_BYTES_R_TLV] = { 8,8 },
    [DLEP_BYTES_T_TLV] = { 8,8 },
    [DLEP_FRAMES_RETRIES_TLV] = { 8,8 },
    [DLEP_FRAMES_FAILED_TLV] = { 8,8 },
    [DLEP_SIGNAL_TLV] = { 4,4 },
};
