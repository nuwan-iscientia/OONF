/*
 * dlep_signal.c
 *
 *  Created on: Sep 24, 2014
 *      Author: rogge
 */

#include <arpa/inet.h>

#include "common/common_types.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_tlvmap.h"

struct dlep_tlvmap dlep_mandatory_tlvs[DLEP_SIGNAL_COUNT] = {
  [DLEP_PEER_DISCOVERY] = { .map = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV),
      0,0,0
  }},
  [DLEP_PEER_OFFER] = { .map = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_PORT_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION] = { .map = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_OPTIONAL_SIGNALS_TLV)
      | (1 << DLEP_OPTIONAL_DATA_ITEMS_TLV),
      0,0,0
  }},
  [DLEP_PEER_INITIALIZATION_ACK] = { .map = {
      (1 << DLEP_HEARTBEAT_INTERVAL_TLV)
      | (1 << DLEP_MDRR_TLV)
      | (1 << DLEP_MDRT_TLV)
      | (1 << DLEP_CDRR_TLV)
      | (1 << DLEP_CDRT_TLV)
      | (1 << DLEP_OPTIONAL_SIGNALS_TLV)
      | (1 << DLEP_OPTIONAL_DATA_ITEMS_TLV),
      0,0,0
  }},
};

bool
dlep_tlvmap_is_subset(struct dlep_tlvmap *set, struct dlep_tlvmap *subset) {
  size_t i;

  for (i=0; i<8; i++) {
    if (set->map[i] != (set->map[i] | subset->map[i])) {
      return false;
    }
  }
  return true;
}
