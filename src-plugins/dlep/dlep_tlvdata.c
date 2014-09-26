/*
 * dlep_tlvdata.c
 *
 *  Created on: Sep 24, 2014
 *      Author: rogge
 */

#include "common/common_types.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_tlvdata.h"

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
    [DLEP_OPTIONAL_SIGNALS_TLV] = { 1,255 },
    [DLEP_OPTIONAL_DATA_ITEMS_TLV] = { 1,255 },
    [DLEP_VENDOR_EXTENSION_TLV] = { 3,255 },
};
