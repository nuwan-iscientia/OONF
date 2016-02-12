
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

/**
 * @file
 */

#ifndef OS_ROUTING_LINUX_H_
#define OS_ROUTING_LINUX_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"

#include "subsystems/os_generic/os_routing_generic_rt_to_string.h"
#include "subsystems/os_generic/os_routing_generic_init_half_route_key.h"

/**
 * linux specifc data for changing a kernel route
 */
struct os_route_internal {
  /*! hook into list of route change handlers */
  struct avl_node _node;

  /*! netlink sequence number of command sent to the kernel */
  uint32_t nl_seq;
};

/**
 * linux specific data for listening to kernel route changes
 */
struct os_route_listener_internal {
  /*! hook into list of kernel route listeners */
  struct list_entity _node;
};

EXPORT bool os_routing_linux_supports_source_specific(int af_family);
EXPORT int os_routing_linux_set(struct os_route *, bool set, bool del_similar);
EXPORT int os_routing_linux_query(struct os_route *);
EXPORT void os_routing_linux_interrupt(struct os_route *);
EXPORT bool os_routing_linux_is_in_progress(struct os_route *);

EXPORT void os_routing_linux_listener_add(struct os_route_listener *);
EXPORT void os_routing_linux_listener_remove(struct os_route_listener *);

EXPORT const char *os_routing_linux_to_string(
    struct os_route_str *buf, const struct os_route_parameter *route_param);

EXPORT const struct os_route_parameter *os_routing_linux_get_wildcard_route(void);

static INLINE bool
os_routing_supports_source_specific(int af_family) {
  return os_routing_linux_supports_source_specific(af_family);
}

static INLINE int
os_routing_set(struct os_route *route, bool set, bool del_similar) {
  return os_routing_linux_set(route, set, del_similar);
}

static INLINE int
os_routing_query(struct os_route *route) {
  return os_routing_linux_query(route);
}

static INLINE void
os_routing_interrupt(struct os_route *route) {
  os_routing_linux_interrupt(route);
}

static INLINE bool
os_routing_is_in_progress(struct os_route *route) {
  return os_routing_linux_is_in_progress(route);
}

static INLINE void
os_routing_listener_add(struct os_route_listener *l) {
  os_routing_linux_listener_add(l);
}

static INLINE void
os_routing_listener_remove(struct os_route_listener *l) {
  os_routing_linux_listener_remove(l);
}

static INLINE const struct os_route_parameter *
os_routing_get_wildcard_route(void) {
  return os_routing_linux_get_wildcard_route();
}

static INLINE const char *
os_routing_to_string(
    struct os_route_str *buf, const struct os_route_parameter *route_param) {
  return os_routing_generic_rt_to_string(buf, route_param);
}

static INLINE void
os_routing_init_half_os_route_key(
    struct netaddr *any, struct netaddr *specific,
    const struct netaddr *source) {
  return os_routing_generic_init_half_os_route_key(any, specific, source);
}

#endif /* OS_ROUTING_LINUX_H_ */
