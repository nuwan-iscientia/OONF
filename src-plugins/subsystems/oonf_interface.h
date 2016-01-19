
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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "os_socket.h"

#include "subsystems/oonf_timer.h"

/*! subsystem identifier */
#define OONF_INTERFACE_SUBSYSTEM "interface"

/*! memory class for oonf interfaces */
#define OONF_CLASS_INTERFACE             "oonf_interface"

/*! interface configuration section name */
#define CFG_INTERFACE_SECTION      "interface"

/*! interface configuration section mode */
#define CFG_INTERFACE_SECTION_MODE CFG_SSMODE_NAMED

/*! wildcard name for interfaces */
#define OONF_INTERFACE_WILDCARD "any"

/*! interval after a failed interface change listener should be triggered again */
#define IF_RETRIGGER_INTERVAL 1000ull

/**
 * Status listener for oonf interface
 */
struct oonf_interface_listener {
  /*! name of interface */
  const char *name;

  /**
   * set to true if listener is on a mesh traffic interface.
   * keep this false if in doubt, true will trigger some interface
   * reconfiguration to allow forwarding of user traffic
   */
  bool mesh;

  /**
   * callback for interface change event
   * @param l pointer to this interface listener
   * @return -1 if interface was not ready and
   *   listener should be called again
   */
  int (*process)(struct oonf_interface_listener *l);

  /*! true if process should be triggered again */
  bool trigger_again;

  /*! pointer to the interface this listener is registered to */
  struct os_interface *interface;

  /**
   * pointer to the interface data before the change happened, will be
   * set by the core while process() is called
   */
  struct os_interface_data *old;

  /*! hook into list of listeners */
  struct list_entity _node;
};

EXPORT int oonf_interface_add_listener(struct oonf_interface_listener *);
EXPORT void oonf_interface_remove_listener(struct oonf_interface_listener *);

EXPORT struct os_interface_data *oonf_interface_get_data(
    const char *name, struct os_interface_data *buffer);
EXPORT struct os_interface_data *oonf_interface_get_data_by_ifindex(
    unsigned ifindex);
EXPORT struct os_interface_data *oonf_interface_get_data_by_ifbaseindex(
		unsigned ifindex);

EXPORT void oonf_interface_trigger(struct os_interface *interf);
EXPORT void oonf_interface_trigger_ifindex(unsigned if_index, bool down);
EXPORT void oonf_interface_trigger_handler(struct oonf_interface_listener *listener);

EXPORT const struct netaddr *oonf_interface_get_bindaddress(int af_type,
    struct netaddr_acl *filter, struct os_interface_data *ifdata);
EXPORT const struct netaddr *oonf_interface_get_prefix_from_dst(
    struct netaddr *destination, struct os_interface_data *ifdata);

EXPORT struct avl_tree *oonf_interface_get_tree(void);

#endif /* INTERFACE_H_ */
