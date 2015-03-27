
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

#ifndef DLEP_ROUTER_INTERFACE_H_
#define DLEP_ROUTER_INTERFACE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_timer.h"

struct dlep_router_if {
  /* name of interface for layer2 database */
  char l2_destination[IF_NAMESIZE];

  /* UDP socket for discovery */
  struct oonf_packet_managed udp;
  struct oonf_packet_managed_config udp_config;

  /* event timer (either discovery or heartbeat) */
  struct oonf_timer_instance discovery_timer;

  /* local timer settings */
  uint64_t local_discovery_interval;
  uint64_t local_heartbeat_interval;

  /* heartbeat settings from the other side of the session */
  uint64_t remote_heartbeat_interval;

  /* true if router should not accept further sessions */
  bool single_session;

  /* hook into session tree, interface name is the key */
  struct avl_node _node;

  /* list of all streams on this interface */
  struct avl_tree session_tree;
};

void dlep_router_interface_init(void);
void dlep_router_interface_cleanup(void);

struct dlep_router_if *dlep_router_get_interface(const char *ifname);
struct dlep_router_if *dlep_router_add_interface(const char *ifname);
void dlep_router_remove_interface(struct dlep_router_if *);

void dlep_router_apply_interface_settings(struct dlep_router_if *);
void dlep_router_terminate_all_sessions(void);

#endif /* DLEP_ROUTER_INTERFACE_H_ */
