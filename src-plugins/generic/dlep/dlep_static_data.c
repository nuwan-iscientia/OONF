
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

#include "common/common_types.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_static_data.h"

struct bitmap256 dlep_mandatory_signals = { .b = {
  (1ull << DLEP_PEER_DISCOVERY )
  | (1ull << DLEP_PEER_OFFER )
  | (1ull << DLEP_PEER_INITIALIZATION )
  | (1ull << DLEP_PEER_INITIALIZATION_ACK )
  | (1ull << DLEP_PEER_TERMINATION )
  | (1ull << DLEP_PEER_TERMINATION_ACK )
  | (1ull << DLEP_DESTINATION_UP )
  | (1ull << DLEP_DESTINATION_UPDATE )
  | (1ull << DLEP_DESTINATION_DOWN )
  | (1ull << DLEP_HEARTBEAT),
  0,0,0
}};

struct bitmap256 dlep_supported_optional_signals = { .b = {
  (1ull << DLEP_PEER_TERMINATION)
  | (1ull << DLEP_PEER_TERMINATION_ACK)
  | (1ull << DLEP_DESTINATION_UP_ACK)
  | (1ull << DLEP_DESTINATION_DOWN_ACK),
  0,0,0
}};

struct bitmap256 dlep_mandatory_tlvs_per_signal[DLEP_SIGNAL_COUNT] = {
  [DLEP_PEER_DISCOVERY] = { .b = {
      (1ull << DLEP_VERSION_TLV),
      0,0,0
  }},
  [DLEP_PEER_OFFER] = { .b = {
      (1ull << DLEP_VERSION_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION] = { .b = {
      (1ull << DLEP_VERSION_TLV)
      | (1ull << DLEP_HEARTBEAT_INTERVAL_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION_ACK] = { .b = {
      (1ull << DLEP_VERSION_TLV)
      | (1ull << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1ull << DLEP_MDRR_TLV)
      | (1ull << DLEP_MDRT_TLV)
      | (1ull << DLEP_CDRR_TLV)
      | (1ull << DLEP_CDRT_TLV)
      | (1ull << DLEP_LATENCY_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION] = { .b = {
      0,0,0,0
  }},
  [DLEP_PEER_TERMINATION_ACK] = { .b = {
      0,0,0,0
  }},
  [DLEP_DESTINATION_UP] = { .b = {
      (1ull << DLEP_MAC_ADDRESS_TLV)
  }},
  [DLEP_DESTINATION_UP_ACK] = { .b = {
      (1ull << DLEP_MAC_ADDRESS_TLV)
  }},
  [DLEP_DESTINATION_DOWN] = { .b = {
      (1ull << DLEP_MAC_ADDRESS_TLV)
  }},
  [DLEP_DESTINATION_DOWN_ACK] = { .b = {
      (1ull << DLEP_MAC_ADDRESS_TLV)
  }},
  [DLEP_DESTINATION_UPDATE] = { .b = {
      (1ull << DLEP_MAC_ADDRESS_TLV)
  }},
};

struct bitmap256 dlep_supported_optional_tlvs_per_signal[DLEP_SIGNAL_COUNT] = {
  [DLEP_PEER_DISCOVERY] = { .b = {
      (1ull << DLEP_PEER_TYPE_TLV),
      0,0,0
  }},
  [DLEP_PEER_OFFER] = { .b = {
      (1ull << DLEP_PEER_TYPE_TLV)
      | (1ull << DLEP_IPV4_CONPOINT_TLV)
      | (1ull << DLEP_IPV6_CONPOINT_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION] = { .b = {
      (1ull << DLEP_PEER_TYPE_TLV)
      | (1ull << DLEP_EXTENSIONS_SUPPORTED)
      | (1ull << DLEP_EXPERIMENTAL_DEFINITION),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION_ACK] = { .b = {
      (1ull << DLEP_PEER_TYPE_TLV)
      | (1ull << DLEP_EXTENSIONS_SUPPORTED)
      | (1ull << DLEP_STATUS_TLV)
      | (1ull << DLEP_FRAMES_R_TLV)
      | (1ull << DLEP_FRAMES_T_TLV)
      | (1ull << DLEP_BYTES_R_TLV)
      | (1ull << DLEP_BYTES_T_TLV)
      | (1ull << DLEP_FRAMES_RETRIES_TLV)
      | (1ull << DLEP_FRAMES_FAILED_TLV)
      | (1ull << DLEP_RX_SIGNAL_TLV)
      | (1ull << DLEP_TX_SIGNAL_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION] = { .b = {
      (1ull << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_PEER_TERMINATION_ACK] = { .b = {
      (1ull << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_UP] = { .b = {
      (1ull << DLEP_MDRR_TLV)
      | (1ull << DLEP_MDRT_TLV)
      | (1ull << DLEP_CDRR_TLV)
      | (1ull << DLEP_CDRT_TLV)
      | (1ull << DLEP_LATENCY_TLV)
      | (1ull << DLEP_FRAMES_R_TLV)
      | (1ull << DLEP_FRAMES_T_TLV)
      | (1ull << DLEP_BYTES_R_TLV)
      | (1ull << DLEP_BYTES_T_TLV)
      | (1ull << DLEP_FRAMES_RETRIES_TLV)
      | (1ull << DLEP_FRAMES_FAILED_TLV)
      | (1ull << DLEP_TX_SIGNAL_TLV)
      | (1ull << DLEP_RX_SIGNAL_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_UP_ACK] = { .b = {
      (1ull << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_DOWN] = { .b = {
      0,0,0,0
  }},
  [DLEP_DESTINATION_DOWN_ACK] = { .b = {
      (1ull << DLEP_STATUS_TLV),
      0,0,0
  }},
  [DLEP_DESTINATION_UPDATE] = { .b = {
      (1ull << DLEP_MDRR_TLV)
      | (1ull << DLEP_MDRT_TLV)
      | (1ull << DLEP_CDRR_TLV)
      | (1ull << DLEP_CDRT_TLV)
      | (1ull << DLEP_LATENCY_TLV)
      | (1ull << DLEP_FRAMES_R_TLV)
      | (1ull << DLEP_FRAMES_T_TLV)
      | (1ull << DLEP_BYTES_R_TLV)
      | (1ull << DLEP_BYTES_T_TLV)
      | (1ull << DLEP_FRAMES_RETRIES_TLV)
      | (1ull << DLEP_FRAMES_FAILED_TLV)
      | (1ull << DLEP_TX_SIGNAL_TLV)
      | (1ull << DLEP_RX_SIGNAL_TLV),
      0,0,0
  }},
};

struct dlep_tlvdata dlep_tlv_constraints[DLEP_TLV_COUNT] = {
    [DLEP_VERSION_TLV] = { 4,4 },
    [DLEP_STATUS_TLV] = { 1,255 },
    [DLEP_IPV4_CONPOINT_TLV] = { 6,6 },
    [DLEP_IPV6_CONPOINT_TLV] = { 18,18 },
    [DLEP_PEER_TYPE_TLV] = { 1,255 },
    [DLEP_HEARTBEAT_INTERVAL_TLV] = { 2,2 },
    [DLEP_EXTENSIONS_SUPPORTED] = { 1,255 },
    [DLEP_EXPERIMENTAL_DEFINITION] = { 1,255 },
    [DLEP_MAC_ADDRESS_TLV] = { 6,6 },
    [DLEP_IPV4_ADDRESS_TLV] = { 5,5 },
    [DLEP_IPV6_ADDRESS_TLV] = { 17,17 },
    [DLEP_IPV4_SUBNET_TLV] = { 5,5 },
    [DLEP_IPV6_SUBNET_TLV] = { 17,17 },
    [DLEP_MDRR_TLV] = { 8,8 },
    [DLEP_MDRT_TLV] = { 8,8 },
    [DLEP_CDRR_TLV] = { 8,8 },
    [DLEP_CDRT_TLV] = { 8,8 },
    [DLEP_LATENCY_TLV] = { 4,4 },
    [DLEP_RESR_TLV] = { 1,1 },
    [DLEP_REST_TLV] = { 1,1 },
    [DLEP_RLQR_TLV] = { 1,1 },
    [DLEP_RLQT_TLV] = { 1,1 },
    [DLEP_LINK_CHAR_ACK_TIMER_TLV] = { 1,1 },
    [DLEP_CREDIT_GRANT_TLV] = { 8,8 },
    [DLEP_CREDIT_WIN_STATUS_TLV] = { 16,16 },
    [DLEP_CREDIT_REQUEST_TLV] = { 1,1 },

    [DLEP_FRAMES_R_TLV] = { 8,8 },
    [DLEP_FRAMES_T_TLV] = { 8,8 },
    [DLEP_BYTES_R_TLV] = { 8,8 },
    [DLEP_BYTES_T_TLV] = { 8,8 },
    [DLEP_FRAMES_RETRIES_TLV] = { 8,8 },
    [DLEP_FRAMES_FAILED_TLV] = { 8,8 },
    [DLEP_TX_SIGNAL_TLV] = { 4,4 },
    [DLEP_RX_SIGNAL_TLV] = { 4,4 },
};
