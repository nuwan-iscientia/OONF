
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
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

#ifndef NL80211_LISTENER_H_
#define NL80211_LISTENER_H_

#include "common/common_types.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"

#define OONF_NL80211_LISTENER_SUBSYSTEM "nl80211_listener"

struct nl80211_if {
  char name[IF_NAMESIZE];

  struct oonf_interface_listener if_listener;

  struct oonf_layer2_net *l2net;

  int phy_if;
  uint64_t max_tx, max_rx;
  bool ifdata_changed;

  bool _remove, _if_section, _nl80211_section;

  struct avl_node _node;
};

struct oonf_layer2_destination *nl80211_add_dst(struct oonf_layer2_neigh *,
    const struct netaddr *dst);
bool nl80211_change_l2net_data(struct oonf_layer2_net *l2net,
    enum oonf_layer2_network_index idx, uint64_t value);
bool nl80211_change_l2net_neighbor_default(struct oonf_layer2_net *l2net,
    enum oonf_layer2_neighbor_index idx, uint64_t value);
void nl80211_cleanup_l2neigh_data(struct oonf_layer2_neigh *l2neigh);
void nl80211_cleanup_l2net_data(struct oonf_layer2_net *l2net);
bool nl80211_change_l2neigh_data(struct oonf_layer2_neigh *l2neigh,
    enum oonf_layer2_neighbor_index idx, uint64_t value);

static INLINE unsigned
nl80211_get_if_index(struct nl80211_if *interf) {
  return interf->if_listener.interface->data.index;
}

#endif /* NL80211_LISTENER_H_ */
