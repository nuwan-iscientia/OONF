/*
 * os_routing_generic_rtkey_avlcomp.c
 *
 *  Created on: Jul 21, 2015
 *      Author: henning
 */

#include "common/common_types.h"

#include "subsystems/os_routing.h"

int
os_route_avl_cmp_route_key(const void *p1, const void *p2) {
  const struct os_route_key *ss1, *ss2;

  ss1 = p1;
  ss2 = p2;

  return memcmp(ss1, ss2, sizeof(*ss1));
}

