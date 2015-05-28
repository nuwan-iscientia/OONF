
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

#include "common/common_types.h"
#include "common/autobuf.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_telnet.h"
#include "subsystems/oonf_viewer.h"

#include "nhdp/nhdp.h"
#include "nhdp/nhdp_db.h"
#include "nhdp/nhdp_domain.h"
#include "nhdp/nhdp_interfaces.h"
#include "olsrv2/olsrv2.h"
#include "olsrv2/olsrv2_lan.h"
#include "olsrv2/olsrv2_originator.h"
#include "olsrv2/olsrv2_routing.h"
#include "olsrv2/olsrv2_tc.h"

#include "json_for_networks/json_for_networks.h"

/* definitions */
#define LOG_JSON_FOR_NET olsrv2_jsonfornet.logging

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_jsonfornet(struct oonf_telnet_data *con);
static void _print_json_string(
    struct oonf_viewer_json_session *session, const char *key, const char *value);
static void _print_json_number(
    struct oonf_viewer_json_session *session, const char *key, uint64_t value);
static void _print_json_netaddr(
    struct oonf_viewer_json_session *session, const char *key, const struct netaddr *addr);

/* telnet command of this plugin */
static struct oonf_telnet_command _telnet_commands[] = {
    TELNET_CMD(OONF_JSONFORNETWORKS_SUBSYSTEM, _cb_jsonfornet,
        ""),
};

/* plugin declaration */
static const char *_dependencies[] = {
  OONF_NHDP_SUBSYSTEM,
  OONF_OLSRV2_SUBSYSTEM,
  OONF_TELNET_SUBSYSTEM,
  OONF_VIEWER_SUBSYSTEM,
};
struct oonf_subsystem olsrv2_jsonfornet = {
  .name = OONF_JSONFORNETWORKS_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "OLSRv2 JSON for networks generator plugin",
  .author = "Henning Rogge",
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(olsrv2_jsonfornet);

/**
 * Initialize plugin
 * @return always returns 0
 */
static int
_init(void) {
  oonf_telnet_add(&_telnet_commands[0]);
  return 0;
}

/**
 * Cleanup plugin
 */
static void
_cleanup(void) {
  oonf_telnet_remove(&_telnet_commands[0]);
}

static void
_print_graph_node(struct oonf_viewer_json_session *session, const struct netaddr *id) {
  oonf_viewer_json_start_object(session, NULL);
  _print_json_netaddr(session, "id", id);
  oonf_viewer_json_end_object(session);
}

static void
_print_graph_edge(struct oonf_viewer_json_session *session,
    struct nhdp_domain *domain,
    const struct netaddr *src, const struct netaddr *dst,
    uint32_t out, uint32_t in) {
  struct nhdp_metric_str mbuf;

  oonf_viewer_json_start_object(session, NULL);
  _print_json_netaddr(session, "source", src);
  _print_json_netaddr(session, "target", dst);
  _print_json_number(session, "weight", out);
  if (in) {
    oonf_viewer_json_start_object(session, "properties");
    _print_json_string(session, "weight-txt",
        nhdp_domain_get_metric_value(&mbuf, domain, out));
    _print_json_number(session, "in", in);
    _print_json_string(session, "in-txt",
        nhdp_domain_get_metric_value(&mbuf, domain, in));
    oonf_viewer_json_end_object(session);
  }
  oonf_viewer_json_end_object(session);
}

static void
_print_graph_end(struct oonf_viewer_json_session *session,
    struct nhdp_domain *domain,
    const struct netaddr *src, const struct netaddr *prefix,
    uint32_t out, uint8_t hopcount) {
  struct nhdp_metric_str mbuf;

  oonf_viewer_json_start_object(session, NULL);
  _print_json_netaddr(session, "source", src);
  _print_json_netaddr(session, "prefix", prefix);
  _print_json_number(session, "weight", out);

  oonf_viewer_json_start_object(session, "properties");
  _print_json_string(session, "weight-txt",
      nhdp_domain_get_metric_value(&mbuf, domain, out));
  if (hopcount) {
    _print_json_number(session, "hopcount", hopcount);
  }
  oonf_viewer_json_end_object(session);
  oonf_viewer_json_end_object(session);
}

static void
_print_graph(struct oonf_viewer_json_session *session,
    struct nhdp_domain *domain, int af_type) {
  const struct netaddr *originator;
  struct nhdp_neighbor *neigh;
  struct olsrv2_tc_node *node;
  struct olsrv2_tc_edge *edge;
  struct olsrv2_tc_attachment *attached;
  struct olsrv2_lan_entry *lan;

  originator = olsrv2_originator_get(af_type);
  if (netaddr_get_address_family(originator) != af_type) {
    return;
  }
  oonf_viewer_json_start_object(session, NULL);

  _print_json_netaddr(session, "router_id", originator);
  _print_json_string(session, "metric", domain->metric->name);

  oonf_viewer_json_start_array(session, "nodes");
  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    if (netaddr_get_address_family(&node->target.addr) == af_type) {
      _print_graph_node(session, &node->target.addr);
    }
  }
  oonf_viewer_json_end_array(session);

  oonf_viewer_json_start_array(session, "links");

  /* print local links to neighbors */
  avl_for_each_element(nhdp_db_get_neigh_originator_tree(), neigh, _originator_node) {
    if (netaddr_get_address_family(&neigh->originator) == af_type
        && neigh->symmetric > 0) {
      _print_graph_edge(session, domain,
          originator, &neigh->originator,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.out,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.in);
    }
  }

  /* print remote node links to neighbors and prefixes */
  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    if (netaddr_get_address_family(&node->target.addr) == af_type) {
      avl_for_each_element(&node->_edges, edge, _node) {
        if (!edge->virtual) {
          _print_graph_edge(session, domain,
              &node->target.addr, &edge->dst->target.addr,
              edge->cost[domain->index],
              edge->inverse->cost[domain->index]);
        }
      }
    }
  }
  oonf_viewer_json_end_array(session);

  oonf_viewer_json_start_array(session, "endpoints");

  /* print local endpoints */
  avl_for_each_element(olsrv2_lan_get_tree(), lan, _node) {
    if (netaddr_get_address_family(&lan->prefix) == af_type
        && olsrv2_lan_get_domaindata(domain, lan)->active) {
      _print_graph_end(session, domain,
          originator, &lan->prefix,
          olsrv2_lan_get_domaindata(domain, lan)->outgoing_metric,
          olsrv2_lan_get_domaindata(domain, lan)->distance);
    }
  }

  /* print remote nodes neighbors */
  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    if (netaddr_get_address_family(&node->target.addr) == af_type) {
      avl_for_each_element(&node->_endpoints, attached, _src_node) {
        _print_graph_end(session, domain,
            &node->target.addr, &attached->dst->target.addr,
            attached->cost[domain->index],
            attached->distance[domain->index]);
      }
    }
  }
  oonf_viewer_json_end_array(session);

  oonf_viewer_json_end_object(session);
}

static void
_create_graph_json(struct oonf_viewer_json_session *session) {
  struct nhdp_domain *domain;
  oonf_viewer_json_start_object(session, NULL);

  _print_json_string(session, "type", "NetworkRoutes");
  _print_json_string(session, "protocol", "olsrv2");
  _print_json_string(session, "version", oonf_log_get_libdata()->version);
  _print_json_string(session, "revision", oonf_log_get_libdata()->git_commit);

  oonf_viewer_json_start_array(session, "topologies");
  list_for_each_element(nhdp_domain_get_list(), domain, _node) {
    _print_graph(session, domain, AF_INET);
    _print_graph(session, domain, AF_INET6);
  }
  oonf_viewer_json_end_array(session);
  oonf_viewer_json_end_object(session);
}

static void
_print_routing_tree(struct oonf_viewer_json_session *session,
    struct nhdp_domain *domain, int af_type) {
  struct olsrv2_routing_entry *rtentry;
  const struct netaddr *originator;
  char ibuf[IF_NAMESIZE];

  originator = olsrv2_originator_get(af_type);
  if (netaddr_get_address_family(originator) != af_type) {
    return;
  }

  oonf_viewer_json_start_object(session, NULL);

  _print_json_netaddr(session, "router_id", originator);
  _print_json_string(session, "metric", domain->metric->name);

  oonf_viewer_json_start_array(session, "routes");

  avl_for_each_element(olsrv2_routing_get_tree(domain), rtentry, _node) {
    if (rtentry->route.family == af_type) {
      oonf_viewer_json_start_object(session, NULL);
      _print_json_netaddr(session, "destination", &rtentry->route.dst);
      if (netaddr_get_prefix_length(&rtentry->route.src_prefix) > 0) {
        _print_json_netaddr(session, "source", &rtentry->route.src_prefix);
      }
      _print_json_netaddr(session, "next", &rtentry->route.gw);
      _print_json_netaddr(session, "next-id", &rtentry->next_originator);

      _print_json_string(session, "device", if_indextoname(rtentry->route.if_index, ibuf));
      _print_json_number(session, "cost", rtentry->cost);
      oonf_viewer_json_end_object(session);
    }
  }

  oonf_viewer_json_end_array(session);
  oonf_viewer_json_end_object(session);
}

static void
_create_routes_json(struct oonf_viewer_json_session *session) {
  struct nhdp_domain *domain;
  oonf_viewer_json_start_object(session, NULL);

  _print_json_string(session, "type", "NetworkRoutes");
  _print_json_string(session, "protocol", "olsrv2");
  _print_json_string(session, "version", oonf_log_get_libdata()->version);
  _print_json_string(session, "revision", oonf_log_get_libdata()->git_commit);

  oonf_viewer_json_start_array(session, "topologies");
  list_for_each_element(nhdp_domain_get_list(), domain, _node) {
    _print_routing_tree(session, domain, AF_INET);
    _print_routing_tree(session, domain, AF_INET6);
  }
  oonf_viewer_json_end_array(session);
  oonf_viewer_json_end_object(session);
}

/**
 * Callback for jsonfornet telnet command
 * @param con telnet connection
 * @return active or internal_error
 */
static enum oonf_telnet_result
_cb_jsonfornet(struct oonf_telnet_data *con) {
  struct oonf_viewer_json_session session;
  struct autobuf out;
  const char *error;

  if (abuf_init(&out)) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  oonf_viewer_json_init_session(&session, &out);

  error = NULL;

  if (!con->parameter || con->parameter[0] == 0) {
    error = "use network or routes subcommand";
  }
  else if (strcmp(con->parameter, "graph") == 0) {
    _create_graph_json(&session);
  }
  else if (strcmp(con->parameter, "routes") == 0) {
    _create_routes_json(&session);
  }
  else {
    error = "unknown sub-command";
  }

  if (error == NULL && abuf_has_failed(&out)) {
    error = "internal error";
  }

  if (error) {
    /* create error */
    oonf_viewer_json_init_session(&session, con->out);

    oonf_viewer_json_start_object(&session, NULL);
    _print_json_string(&session, "error", error);
    oonf_viewer_json_end_object(&session);

    return TELNET_RESULT_ACTIVE;
  }

  /* copy output into telnet buffer */
  abuf_memcpy(con->out, abuf_getptr(&out), abuf_getlen(&out));
  abuf_free(&out);
  return TELNET_RESULT_ACTIVE;
}

static void
_print_json_string(struct oonf_viewer_json_session *session, const char *key, const char *value) {
  oonf_viewer_json_element(session, key, true, value);
}

static void
_print_json_number(struct oonf_viewer_json_session *session, const char *key, uint64_t value) {
  oonf_viewer_json_elementf(session, key, false, "%" PRIu64, value);
}

static void
_print_json_netaddr(struct oonf_viewer_json_session *session, const char *key, const struct netaddr *addr) {
  struct netaddr_str nbuf;

  oonf_viewer_json_element(session, key, true, netaddr_to_string(&nbuf, addr));
}

