
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

#include <stdio.h>
#include "nhdp/nhdp.h"
#include "nhdp/nhdp_db.h"
#include "nhdp/nhdp_domain.h"
#include "nhdp/nhdp_interfaces.h"

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/container_of.h"
#include "config/cfg_schema.h"
#include "core/oonf_cfg.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

#include "mpr/mpr_internal.h"
#include "mpr/neighbor-graph-flooding.h"
#include "mpr/neighbor-graph.h"
#include "mpr/mpr.h"

/* FIXME remove unneeded includes */

void
mpr_add_n1_node_to_set(struct avl_tree *set, struct nhdp_neighbor *neigh, struct nhdp_link *lnk) {
  struct n1_node *tmp_n1_neigh;
  tmp_n1_neigh = malloc(sizeof (struct n1_node));
  tmp_n1_neigh->addr = neigh->originator;
  tmp_n1_neigh->_avl_node.key = &tmp_n1_neigh->addr;
  tmp_n1_neigh->neigh = neigh;
  tmp_n1_neigh->link = lnk;
  avl_insert(set, &tmp_n1_neigh->_avl_node);
}

/**
 * Add an address node to a set
 * @param current_mpr_data
 * @param node
 */
void
mpr_add_addr_node_to_set(struct avl_tree *set, const struct netaddr addr) {
  struct addr_node *tmp_node;

  tmp_node = malloc(sizeof (struct addr_node));
  tmp_node->addr = addr;
  tmp_node->_avl_node.key = &tmp_node->addr;
  avl_insert(set, &tmp_node->_avl_node);
}

/**
 * Initialize the MPR data set
 * @param current_mpr_data
 */
void
mpr_init_neighbor_graph(struct neighbor_graph *graph, struct neighbor_graph_interface *methods) {
  avl_init(&graph->set_n, avl_comp_netaddr, false);
  avl_init(&graph->set_n1, avl_comp_netaddr, true);
  avl_init(&graph->set_n2, avl_comp_netaddr, false);
  avl_init(&graph->set_mpr, avl_comp_netaddr, false);
  avl_init(&graph->set_mpr_candidates, avl_comp_netaddr, false);
  graph->methods = methods;
}

/**
 * Clear a set of addresses
 * @param set
 */
void
mpr_clear_addr_set(struct avl_tree *set) {
  struct addr_node *current_node, *node_it;

  avl_for_each_element_safe(set, current_node, _avl_node, node_it) {
    avl_remove(set, &current_node->_avl_node);
    free(current_node);
  }
}

/**
 * Clear set of N1 nodes
 * @param set
 */
void
mpr_clear_n1_set(struct avl_tree *set) {
  struct n1_node *current_node, *node_it;

  avl_for_each_element_safe(set, current_node, _avl_node, node_it) {
    avl_remove(set, &current_node->_avl_node);
    free(current_node);
  }
}

/**
 * Clear the MPR data set
 * @param current_mpr_data
 */
void
mpr_clear_neighbor_graph(struct neighbor_graph *graph) {
  mpr_clear_addr_set(&graph->set_n);
  mpr_clear_addr_set(&graph->set_n2);
  mpr_clear_n1_set(&graph->set_n1);
  mpr_clear_n1_set(&graph->set_mpr);
  mpr_clear_n1_set(&graph->set_mpr_candidates);
}

/**
 * Check if a node was selected as an MPR
 * @param set_mpr
 * @param addr
 * @return 
 */
bool
mpr_is_mpr(struct neighbor_graph *graph, struct netaddr *addr) {
  struct n1_node *tmp_mpr_node;

  tmp_mpr_node = avl_find_element(&graph->set_mpr,
      addr, tmp_mpr_node, _avl_node);
  if (tmp_mpr_node != NULL) {
    return true;
  }
  return false;
}

uint32_t
mpr_calculate_minimal_d_z_y(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct addr_node *y) {
  struct n1_node *z_node;
  uint32_t d_z_y, min_d_z_y;

  min_d_z_y = RFC7181_METRIC_INFINITE;

  avl_for_each_element(&graph->set_n1, z_node, _avl_node) {
    d_z_y = graph->methods->calculate_d_x_y(domain, z_node, y);
    if (d_z_y < min_d_z_y) {
      min_d_z_y = d_z_y;
    }
  }
  return min_d_z_y;
}

/**
 * Print a set of addresses
 * @param set
 */
void
mpr_print_addr_set(struct avl_tree *set) {
  struct addr_node *current_node;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf1;
#endif

  avl_for_each_element(set, current_node, _avl_node) {
    OONF_DEBUG(LOG_MPR, "%s",
        netaddr_to_string(&buf1, &current_node->addr));
  }
}

void
mpr_print_n1_set(struct avl_tree *set) {
  struct n1_node *current_node;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf1;
#endif

  avl_for_each_element(set, current_node, _avl_node) {
    OONF_DEBUG(LOG_MPR, "%s",
        netaddr_to_string(&buf1, &current_node->addr));
  }
}

/**
 * Print the MPR data sets 
 * @param current_mpr_data
 */
void
mpr_print_sets(struct neighbor_graph *graph) {
  OONF_DEBUG(LOG_MPR, "Set N");
  mpr_print_addr_set(&graph->set_n);

  OONF_DEBUG(LOG_MPR, "Set N1");
  mpr_print_n1_set(&graph->set_n1);

  OONF_DEBUG(LOG_MPR, "Set N2");
  mpr_print_addr_set(&graph->set_n2);

  OONF_DEBUG(LOG_MPR, "Set MPR");
  mpr_print_n1_set(&graph->set_mpr);
}
