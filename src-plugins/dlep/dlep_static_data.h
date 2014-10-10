/*
 * dlep_tlvdata.h
 *
 *  Created on: Sep 24, 2014
 *      Author: rogge
 */

#ifndef DLEP_TLVDATA_H_
#define DLEP_TLVDATA_H_

#include "common/common_types.h"

#include "dlep/dlep_bitmap.h"
#include "dlep/dlep_iana.h"

struct dlep_tlvdata {
  uint8_t min_length;
  uint8_t max_length;
};

extern struct dlep_bitmap dlep_mandatory_signals;
extern struct dlep_bitmap dlep_mandatory_tlvs;
extern struct dlep_bitmap dlep_mandatory_tlvs_per_signal[DLEP_SIGNAL_COUNT];
extern struct dlep_bitmap dlep_supported_optional_tlvs_per_signal[DLEP_SIGNAL_COUNT];
extern struct dlep_bitmap dlep_supported_optional_signals;
extern struct dlep_bitmap dlep_supported_optional_tlvs;

extern struct dlep_tlvdata dlep_tlv_constraints[DLEP_TLV_COUNT];

#endif /* DLEP_TLVDATA_H_ */
