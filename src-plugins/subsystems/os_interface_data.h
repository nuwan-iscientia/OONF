
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

#ifndef OS_INTERFACE_DATA_H_
#define OS_INTERFACE_DATA_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/netaddr.h"

#include "subsystems/oonf_timer.h"

#if 0

/**
 * Representation of interface knowledge of the operation system
 */
struct os_interface {
  /*! interface name */
  char name[IF_NAMESIZE];

  /*! interface index */
  unsigned index;

  /**
   * interface index of base interface (for vlan),
   * same for normal interface
   */
  unsigned base_index;

  /*! mac address of interface */
  struct netaddr mac;

  /*! true if the interface exists and is up */
  bool up;

  /*! true if this is a loopback interface */
  bool loopback;

  /*! tree of all addresses/prefixes of this interface */
  struct avl_tree addresses;

  /*!
   * tree to store all new entries before they are
   * committed to the address tree
   */
  struct avl_tree _new_entries;

  /*! listeners to be informed when an interface changes */
  struct list_entity _listeners;

  /*! timer to delay commits of interface address changes */
  struct oonf_timer_instance _commit_timer;
};

/**
 * Representation of an IP address/prefix of a network interface of
 * the operation system
 */
struct os_interface_ip {
  struct avl_node _node;

  struct netaddr address;
  struct netaddr prefix;

  struct os_interface *interf;
};
#endif

#endif /* OS_INTERFACE_DATA_H_ */
