/*
 * os_routing_generic_init_half_route_key.c
 *
 *  Created on: 25.07.2015
 *      Author: rogge
 */

#include "common/common_types.h"
#include "common/netaddr.h"

#include "subsystems/os_routing.h"

/**
 * Copy one address to 'specific', fill the other one with the
 * fitting IP_ANY value
 * @param ipany buffer for IP_ANY
 * @param specific buffer for source
 * @param source source IP value to copy
 */
void
os_route_init_half_os_route_key(struct netaddr *ipany,
    struct netaddr *specific, const struct netaddr *source) {
  memcpy(specific, source, sizeof(*source));
  switch (netaddr_get_address_family(source)) {
    case AF_INET:
      memcpy(ipany, &NETADDR_IPV4_ANY, sizeof(struct netaddr));
      break;
    case AF_INET6:
      memcpy(ipany, &NETADDR_IPV6_ANY, sizeof(struct netaddr));
      break;
    default:
      netaddr_invalidate(ipany);
      break;
  }
}
