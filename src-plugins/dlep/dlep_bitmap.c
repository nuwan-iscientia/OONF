/*
 * dlep_signal.c
 *
 *  Created on: Sep 24, 2014
 *      Author: rogge
 */

#include <arpa/inet.h>

#include "common/common_types.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_bitmap.h"

bool
dlep_bitmap_is_subset(struct dlep_bitmap *set, struct dlep_bitmap *subset) {
  size_t i;

  for (i=0; i<8; i++) {
    if (set->b[i] != (set->b[i] | subset->b[i])) {
      return false;
    }
  }
  return true;
}
