
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
#include "core/oonf_logging.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

#include "mpr/mpr_internal.h"
#include "mpr/neighbor-graph-routing.h"
#include "mpr/neighbor-graph.h"
#include "mpr/mpr.h"

/* FIXME remove unneeded includes */

static bool _is_allowed_link_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface, struct nhdp_link *lnk);
static uint32_t _calculate_d1_x_of_n2_addr(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct netaddr *addr);
static uint32_t _calculate_d_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y);
static uint32_t _calculate_d2_x_y(const struct nhdp_domain *domain,
    struct n1_node *x, struct addr_node *y);
static uint32_t _get_willingness_n1(const struct nhdp_domain *domain,
    struct n1_node *node);

static uint32_t _calculate_d1_of_y(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct addr_node *y);

static struct neighbor_graph_interface _rt_api_interface = {
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
_is_reachable_neighbor_tuple(struct nhdp_neighbor *neigh) {
  if (neigh->_domaindata[0].metric.in != RFC7181_METRIC_INFINITE
      && neigh->symmetric > 0) {
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
_is_allowed_neighbor_tuple(const struct nhdp_domain *domain __attribute__((unused)),
    struct nhdp_neighbor *neigh) {
  if (_is_reachable_neighbor_tuple(neigh)) {
    // FIXME Willingness handling appears to be broken; routing willingness is always 0
    //      && neigh->_domaindata[0].willingness > RFC5444_WILLINGNESS_NEVER) {
    return true;
  }
  return false;
}

static bool
_is_allowed_link_tuple(const struct nhdp_domain *domain,
    struct nhdp_interface *current_interface __attribute__((unused)),
    struct nhdp_link *lnk) {
  return _is_allowed_neighbor_tuple(domain, lnk->neigh);
}

static bool
_is_allowed_2hop_tuple(const struct nhdp_domain *domain, struct nhdp_l2hop *two_hop) {
  struct nhdp_l2hop_domaindata *neighdata;
  neighdata = nhdp_domain_get_l2hopdata(domain, two_hop);
  if (neighdata->metric.in != RFC7181_METRIC_INFINITE) {
    return true;
  }
  return false;
}

/**
 * Calculate d1(x) according to section 18.2 (draft 19)
 * @param mpr_data
 * @param x
 * @return 
 */
static uint32_t
_calculate_d1_x(const struct nhdp_domain *domain, struct n1_node *x) {
  struct nhdp_neighbor_domaindata *neighdata;

  neighdata = nhdp_domain_get_neighbordata(domain, x->neigh);
  return neighdata->metric.in;
}

/**
 * Calculate d2(x,y) according to section 18.2 (draft 19)
 * @param x
 * @param y
 * @return 
 */
static uint32_t
_calculate_d2_x_y(const struct nhdp_domain *domain, struct n1_node *x, struct addr_node *y) {
  struct nhdp_l2hop *l2hop;
  struct nhdp_link *lnk;
  struct nhdp_l2hop_domaindata *twohopdata;

//  #ifdef OONF_LOG_DEBUG_INFO
//  struct netaddr_str buf1;
//#endif

//  OONF_DEBUG(LOG_MPR, "Calculate d2(x,y), look for address %s", netaddr_to_string(&buf1, &y->addr));
  /* find the corresponding 2-hop entry, if it exists */
  list_for_each_element(&x->neigh->_links, lnk, _neigh_node) {
    l2hop = avl_find_element(&lnk->_2hop,
        &y->addr, l2hop, _link_node);
//    OONF_DEBUG(LOG_MPR, "Addresses of 2hop link");
//    mpr_print_addr_set(&lnk->_2hop);
    if (l2hop) {
      twohopdata = nhdp_domain_get_l2hopdata(domain, l2hop);
//      OONF_DEBUG(LOG_MPR, "Got one with metric %i and address %s", l2hop->_domaindata[0].metric.in, netaddr_to_string(&buf1, &l2hop->twohop_addr));
      return twohopdata->metric.in;
    }
  }
  return RFC7181_METRIC_INFINITE;
}

static uint32_t
_calculate_d_x_y(const struct nhdp_domain *domain, struct n1_node *x, struct addr_node *y) {
  return _calculate_d1_x(domain, x) + _calculate_d2_x_y(domain, x, y);
}

/**
 * Calculate d1(y) according to section 18.2 (draft 19)
 * @param mpr_data
 * @return 
 */
static uint32_t
_calculate_d1_of_y(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct addr_node *y) {
  struct n1_node *node_n1;
  struct nhdp_laddr *laddr;
  struct nhdp_neighbor_domaindata *neighdata;

  /* find the N1 neighbor corresponding to this address, if it exists */
  avl_for_each_element(&graph->set_n1, node_n1, _avl_node) {
    laddr = avl_find_element(&node_n1->neigh->_neigh_addresses, y,
        laddr, _neigh_node);
    if (laddr != NULL) {
      neighdata = nhdp_domain_get_neighbordata(domain, node_n1->neigh);
      return neighdata->metric.in;
    }
  }
  return RFC7181_METRIC_INFINITE;
}

/**
 * Calculate d1(x) according to section 18.2 (draft 19)
 * @param mpr_data
 * @param addr
 * @return 
 */
static uint32_t
_calculate_d1_x_of_n2_addr(const struct nhdp_domain *domain,
    struct neighbor_graph *graph, struct netaddr *addr) {
  struct addr_node *node;
  uint32_t d1_x;

  node = malloc(sizeof (struct addr_node));
  memcpy(&node->addr, addr, sizeof (struct netaddr));

  d1_x = _calculate_d1_of_y(domain, graph, node);
  free(node);

  return d1_x;
}

/**
 * Calculate N1
 * @param interf
 */
static void
_calculate_n1(const struct nhdp_domain *domain, struct neighbor_graph *graph) {
  struct nhdp_neighbor *neigh;

  OONF_DEBUG(LOG_MPR, "Calculate N1 for routing MPRs");
  
  list_for_each_element(nhdp_db_get_neigh_list(), neigh, _global_node) {
    if (_is_allowed_neighbor_tuple(domain, neigh)) {
      mpr_add_n1_node_to_set(&graph->set_n1, neigh, NULL);
    }
  }
}

static void
_calculate_n2(const struct nhdp_domain *domain, struct neighbor_graph *graph) {
  struct n1_node *n1_neigh;
  struct nhdp_link *lnk;
  struct nhdp_l2hop *twohop;
  
#ifdef OONF_LOG_DEBUG_INFO
  struct nhdp_l2hop_domaindata *neighdata;
  struct netaddr_str buf1;
#endif
  
  OONF_DEBUG(LOG_MPR, "Calculate N2 for routing MPRs");

//    list_for_each_element(&nhdp_neigh_list, neigh, _global_node) {
//      list_for_each_element(&neigh->_links, link, _if_node) {
//        OONF_DEBUG(LOG_MPR, "Link status %u", link->neigh->symmetric);
//      }
//    }

  /* iterate over all two-hop neighbor addresses of N1 members */
    avl_for_each_element(&graph->set_n1, n1_neigh, _avl_node) {
      list_for_each_element(&n1_neigh->neigh->_links,
          lnk, _neigh_node) {
        avl_for_each_element(&lnk->_2hop, twohop, _link_node) {
          OONF_DEBUG(LOG_MPR, "Link status %u", lnk->neigh->symmetric);
          if (_is_allowed_2hop_tuple(domain, twohop)) {
#ifdef OONF_LOG_DEBUG_INFO
            neighdata = nhdp_domain_get_l2hopdata(domain, twohop);
            OONF_DEBUG(LOG_MPR, "Add twohop addr %s in: %u out: %u",
                    netaddr_to_string(&buf1, &twohop->twohop_addr),
                       neighdata->metric.in, neighdata->metric.out);
#endif
            mpr_add_addr_node_to_set(&graph->set_n2, twohop->twohop_addr);
          }
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
_get_willingness_n1(const struct nhdp_domain *domain, struct n1_node *node) {
  struct nhdp_neighbor_domaindata *neighdata;

  neighdata = nhdp_domain_get_neighbordata(domain, node->neigh);
  return neighdata->willingness;
}

static struct
neighbor_graph_interface *_get_neighbor_graph_interface_routing(void) {
  return &_rt_api_interface;
}

void
mpr_calculate_neighbor_graph_routing(const struct nhdp_domain *domain,
    struct neighbor_graph *graph) {
  struct neighbor_graph_interface *methods;

  OONF_DEBUG(LOG_MPR, "Calculate neighbor graph for routing MPRs");

  methods = _get_neighbor_graph_interface_routing();

  mpr_init_neighbor_graph(graph, methods);
  _calculate_n1(domain, graph);
  _calculate_n2(domain, graph);
}
