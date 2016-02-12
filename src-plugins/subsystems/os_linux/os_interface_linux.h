
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

#ifndef OS_INTERFACE_LINUX_H_
#define OS_INTERFACE_LINUX_H_

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "common/common_types.h"
#include "subsystems/os_interface.h"

/**
 * define scope of address on interface
 */
enum os_addr_scope {
  /* linklocal scope */
  OS_ADDR_SCOPE_LINK = RT_SCOPE_LINK,
  /*! global scope */
  OS_ADDR_SCOPE_GLOBAL = RT_SCOPE_UNIVERSE,
};

/**
 * linux specifc data for changing an interface address
 */
struct os_interface_address_internal {
  /*! hook into list of IP address change handlers */
  struct list_entity _node;

  /*! netlink sequence number of command sent to the kernel */
  uint32_t nl_seq;
};

EXPORT void os_interface_linux_listener_add(struct os_interface_if_listener *);
EXPORT void os_interface_linux_listener_remove(struct os_interface_if_listener *);
EXPORT int os_interface_linux_state_set(const char *dev, bool up);
EXPORT int os_interface_linux_address_set(struct os_interface_address *addr);
EXPORT void os_interface_linux_address_interrupt(struct os_interface_address *addr);
EXPORT int os_interface_linux_mac_set_by_name(const char *, struct netaddr *mac);

EXPORT int os_interface_linux_update(struct os_interface_data *, const char *);
EXPORT int os_interface_linux_init_mesh(struct os_interface *);
EXPORT void os_interface_linux_cleanup_mesh(struct os_interface *);

static INLINE void
os_interface_listener_add(struct os_interface_if_listener *l) {
  os_interface_linux_listener_add(l);
}

static INLINE void
os_interface_listener_remove(struct os_interface_if_listener *l) {
  os_interface_linux_listener_remove(l);
}

static INLINE int
os_interface_state_set(const char *dev, bool up) {
  return os_interface_linux_state_set(dev, up);
}

static INLINE int
os_interface_address_set(struct os_interface_address *addr) {
  return os_interface_linux_address_set(addr);
}

static INLINE void
os_interface_address_interrupt(struct os_interface_address *addr) {
  os_interface_linux_address_interrupt(addr);
}

static INLINE int
os_interface_mac_set_by_name(const char *ifname, struct netaddr *mac) {
  return os_interface_linux_mac_set_by_name(ifname, mac);
}

static INLINE int
os_interface_update(struct os_interface_data *ifdata, const char *name) {
  return os_interface_linux_update(ifdata, name);
}

static INLINE int
os_interface_init_mesh(struct os_interface *interf) {
  return os_interface_linux_init_mesh(interf);
}

static INLINE void
os_interface_cleanup_mesh(struct os_interface *interf) {
  os_interface_linux_cleanup_mesh(interf);
}

/**
 * Set mac address of interface
 * @param ifdata interface data object
 * @param mac new mac address
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_interface_mac_set(struct os_interface_data *ifdata, struct netaddr *mac) {
  return os_interface_linux_mac_set_by_name(ifdata->name, mac);
}

#endif /* OS_INTERFACE_LINUX_H_ */
