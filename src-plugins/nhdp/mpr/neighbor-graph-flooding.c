
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
 * @file src-plugins/nhdp/mpr/neighbor-graph-flooding.c
 */

#include "nhdp/nhdp.h"
#include "nhdp/nhdp_db.h"
#include "nhdp/nhdp_domain.h"
#include "nhdp/nhdp_interfaces.h"

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "common/container_of.h"
#include "common/key_comp.h"
#include "config/cfg_schema.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

#include "mpr/mpr_internal.h"
#include "mpr/neighbor-graph-flooding.h"

#include "mpr/neighbor-graph.h"
#include "mpr/mpr.h"

/* FIXME remove unneeded includes */

static uint32_t _calculate_d1_x(const struct nhdp_domain *domain, struct n1_node *x);
static uint32_t _calculate_d2_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y);
static uint32_t _calculate_d_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y);
#if 0
static uint32_t _calculate_d1_of_y(struct mpr_flooding_data *data, struct addr_node *y);
#endif
static uint32_t _calculate_d1_x_of_n2_addr(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct netaddr *addr);
static void _calculate_n1(const struct nhdp_domain *domain, struct mpr_flooding_data *data);
static void _calculate_n2(const struct nhdp_domain *domain, struct mpr_flooding_data *data);

static bool _is_allowed_link_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface, struct nhdp_link *lnk);
static uint32_t _get_willingness_n1(const struct nhdp_domain *, struct n1_node *node);

static struct neighbor_graph_interface _api_interface = {
  .is_allowed_link_tuple     = _is_allowed_link_tuple,
  .calculate_d1_x_of_n2_addr = _calculate_d1_x_of_n2_addr,
  .calculate_d_x_y           = _calculate_d_x_y,
  .calculate_d2_x_y          = _calculate_d2_x_y,
  .get_willingness_n1        = _get_willingness_n1,
};

/**
 * Check if a given tuple is "reachable" according to section 18.4
 * @param mpr_data
 * @param link
 * @return 
 */
static bool
_is_reachable_link_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface, struct nhdp_link *lnk) {
  struct nhdp_link_domaindata *linkdata;

  linkdata = nhdp_domain_get_linkdata(domain, lnk);
  if (lnk->local_if == current_interface
      && linkdata->metric.out != RFC7181_METRIC_INFINITE
      && lnk->status == NHDP_LINK_SYMMETRIC) {
    return true;
  }
  return false;
}

/**
 * Check if a link tuple is "allowed" according to section 18.4
 * @param mpr_data
 * @param link
 * @return 
 */
static bool
_is_allowed_link_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface, struct nhdp_link *lnk) {
  if (_is_reachable_link_tuple(domain, current_interface, lnk)
      && lnk->neigh->flooding_willingness > RFC7181_WILLINGNESS_NEVER) {
    return true;
  }
  return false;
}

static bool
_is_allowed_2hop_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface, struct nhdp_l2hop *two_hop) {
  struct nhdp_l2hop_domaindata *twohopdata;

  twohopdata = nhdp_domain_get_l2hopdata(domain, two_hop);
  if (two_hop->link->local_if == current_interface
      && twohopdata->metric.out != RFC7181_METRIC_INFINITE) {
    return true;
  }
  return false;
}

static uint32_t
_calculate_d1_x(const struct nhdp_domain *domain, struct n1_node *x) {
  struct nhdp_link_domaindata *linkdata;

  linkdata = nhdp_domain_get_linkdata(domain, x->link);
  return linkdata->metric.out;
}

static uint32_t
_calculate_d2_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y) {
  struct nhdp_l2hop *tmp_l2hop;
  struct nhdp_l2hop_domaindata *twohopdata;

  /* find the corresponding 2-hop entry, if it exists */
  tmp_l2hop = avl_find_element(&x->link->_2hop,
      &y->addr, tmp_l2hop, _link_node);
  if (tmp_l2hop) {
    twohopdata = nhdp_domain_get_l2hopdata(domain, tmp_l2hop);
    return twohopdata->metric.out;
  }
  return RFC7181_METRIC_INFINITE;
}

static uint32_t
_calculate_d_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y) {
  return _calculate_d1_x(domain, x) + _calculate_d2_x_y(domain, x, y);
}

#if 0
/**
 * Calculate d1(y) according to section 18.2 (draft 19)
 * @param mpr_data
 * @return 
 */
uint32_t
_calculate_d1_of_y(struct mpr_flooding_data *data, struct addr_node *y) {
  struct n1_node *node_n1;
  struct nhdp_laddr *laddr;

  /* find the N1 neighbor corresponding to this address, if it exists */
  avl_for_each_element(&data->neigh_graph.set_n1, node_n1, _avl_node) {
    laddr = avl_find_element(&node_n1->link->_addresses, y,
        laddr, _link_node);
    if (laddr != NULL) {
      return node_n1->link->_domaindata[0].metric.out;
    }
  }
  return RFC5444_METRIC_INFINITE;
}
#endif

/**
 * Calculate d1(x) according to section 18.2 (draft 19)
 * @param mpr_data
 * @param addr
 * @return 
 */
uint32_t
_calculate_d1_x_of_n2_addr(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct netaddr *addr) {
  struct n1_node *node_n1;
  struct nhdp_naddr *naddr;
  struct nhdp_link_domaindata *linkdata;

  // FIXME Implementation correct?!?!

  avl_for_each_element(&graph->set_n1, node_n1, _avl_node) {
    /* check if the address provided corresponds to this node */
    naddr = avl_find_element(&node_n1->neigh->_neigh_addresses,
        addr, naddr, _neigh_node);
    if (naddr) {
      linkdata = nhdp_domain_get_linkdata(domain, node_n1->link);
      return linkdata->metric.out;
    }
  }

  return RFC7181_METRIC_INFINITE;
}

/**
 * Calculate N1
 * @param interf
 */
static void
_calculate_n1(const struct nhdp_domain *domain, struct mpr_flooding_data *data) {
  struct nhdp_link *lnk;

  OONF_DEBUG(LOG_MPR, "Calculate N1 for interface %s",
      nhdp_interface_get_name(data->current_interface));

  list_for_each_element(nhdp_db_get_link_list(), lnk, _global_node) {
    if (_is_allowed_link_tuple(domain, data->current_interface, lnk)) {
      mpr_add_n1_node_to_set(&data->neigh_graph.set_n1, lnk->neigh, lnk);
    }
  }
}

/**
 * Calculate N2
 * 
 * For every neighbor in N1, N2 contains a unique entry for every neighbor
 * 2-hop neighbor address. The same address may be reachable via multiple
 * 1-hop neighbors, but is only represented once in N2.
 * 
 * Note that N1 is generated per-interface, so we don't need to deal with 
 * multiple links to the same N1 member.
 * 
 * @param set_n1 N1 Set
 * @return 
 */
static void
_calculate_n2(const struct nhdp_domain *domain, struct mpr_flooding_data *data) {
  struct n1_node *n1_neigh;
  struct nhdp_l2hop *twohop;

  OONF_DEBUG(LOG_MPR, "Calculate N2 for flooding MPRs");

  /* iterate over all two-hop neighbor addresses of N1 members */
  avl_for_each_element(&data->neigh_graph.set_n1, n1_neigh, _avl_node) {

    avl_for_each_element(&n1_neigh->link->_2hop, twohop, _link_node) {
      if (_is_allowed_2hop_tuple(domain, data->current_interface, twohop)) {
        mpr_add_addr_node_to_set(&data->neigh_graph.set_n2,
            twohop->twohop_addr);
      }
    }
  }
}

/**
 * Returns the flooding/routing willingness of an N1 neighbor
 * @param not used
 * @param node
 * @return 
 */
static uint32_t
_get_willingness_n1(const struct nhdp_domain *domain __attribute__((unused)),
    struct n1_node *node) {
  return node->neigh->flooding_willingness;
}

void
mpr_calculate_neighbor_graph_flooding(const struct nhdp_domain *domain, struct mpr_flooding_data *data) {
  OONF_DEBUG(LOG_MPR, "Calculate neighbor graph for flooding MPRs");

  mpr_init_neighbor_graph(&data->neigh_graph, &_api_interface);
  _calculate_n1(domain, data);
  _calculate_n2(domain, data);
}

#if 0

/**
 * Calculates N1(x) according to Section 18.2
 * @param addr
 * @return 
 */
struct avl_tree *
_calculate_n1_of_y(struct common_data *mpr_data, struct addr_node *addr) {
  struct n1_node *node_n1;
  struct nhdp_l2hop *two_hop;
  struct avl_tree *n1_of_y;

  n1_of_y = malloc(sizeof (struct avl_tree));
  avl_init(n1_of_y, avl_comp_netaddr, false);

  /* find the subset of N1 through which this N2 node is reachable */
  avl_for_each_element(&mpr_data->set_n1, node_n1, _avl_node) {
    two_hop = avl_find_element(&node_n1->link->_2hop,
        addr,
        two_hop, _link_node);
    if (two_hop != NULL) {
      _add_n1_node_to_set_flooding(n1_of_y, node_n1->link);
    }
  }

  return n1_of_y;
}

/**
 * Calculate d(y,S) according to section 18.2 (draft 19)
 * @param mpr_data
 * @return 
 */
uint32_t
_calculate_d_of_y_s(struct common_data *mpr_data, struct addr_node *y,
    struct avl_tree *subset_s) {
  uint32_t d1_y, d_x_y, min_cost;
  struct avl_tree *n1_y, union_subset;
  struct n1_node *node_n1;

  avl_init(&union_subset, avl_comp_netaddr, false);

  n1_y = _calculate_n1_of_y(mpr_data, y);
  d1_y = _calculate_d1_of_y(mpr_data, y);

  /* calculate union of subset S and N1(y) */
  avl_for_each_element(subset_s, node_n1, _avl_node) {
    _add_n1_node_to_set_flooding(&union_subset, node_n1->link);
  }

  avl_for_each_element(n1_y, node_n1, _avl_node) {
    _add_n1_node_to_set_flooding(&union_subset, node_n1->link);
  }

  /* determine the minimum cost to y over all possible intermediate hops */
  min_cost = d1_y;

  avl_for_each_element(&union_subset, node_n1, _avl_node) {
    d_x_y = _calculate_d_x_y_routing(mpr_data, node_n1, y);
    if (d_x_y < min_cost) {
      min_cost = d_x_y;
    }
  }

  /* free temporary data */
  _clear_n1_set(n1_y);
  _clear_n1_set(&union_subset);
  free(n1_y);

  return min_cost;
}

#endif
