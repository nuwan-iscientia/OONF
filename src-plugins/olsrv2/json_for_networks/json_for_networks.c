
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
#include "common/json.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_telnet.h"

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

#define COMMAND_GRAPH  "graph"
#define COMMAND_ROUTES "routes"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_jsonfornet(struct oonf_telnet_data *con);
static void _print_json_string(
    struct json_session *session, const char *key, const char *value);
static void _print_json_number(
    struct json_session *session, const char *key, uint64_t value);
static void _print_json_netaddr(
    struct json_session *session, const char *key, const struct netaddr *addr);

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
_print_graph_node(struct json_session *session, const struct netaddr *id) {
  json_start_object(session, NULL);
  _print_json_netaddr(session, "id", id);
  json_end_object(session);
}

static void
_print_graph_edge(struct json_session *session,
    struct nhdp_domain *domain,
    const struct netaddr *src, const struct netaddr *dst,
    uint32_t out, uint32_t in, bool outgoing_tree) {
  struct nhdp_metric_str mbuf;

  json_start_object(session, NULL);
  _print_json_netaddr(session, "source", src);
  _print_json_netaddr(session, "target", dst);
  _print_json_number(session, "weight", out);
  if (in) {
    json_start_object(session, "properties");
    _print_json_string(session, "weight_txt",
        nhdp_domain_get_metric_value(&mbuf, domain, out));
    _print_json_number(session, "in", in);
    _print_json_string(session, "in_txt",
        nhdp_domain_get_metric_value(&mbuf, domain, in));
    _print_json_string(session, "outgoing_tree",
        json_getbool(outgoing_tree));
    json_end_object(session);
  }
  json_end_object(session);
}

static void
_print_graph_end(struct json_session *session,
    struct nhdp_domain *domain,
    const struct netaddr *src, const struct netaddr *prefix,
    uint32_t out, uint8_t hopcount) {
  struct nhdp_metric_str mbuf;

  json_start_object(session, NULL);
  _print_json_netaddr(session, "source", src);
  _print_json_netaddr(session, "target", prefix);
  _print_json_number(session, "weight", out);

  json_start_object(session, "properties");
  _print_json_string(session, "weight_txt",
      nhdp_domain_get_metric_value(&mbuf, domain, out));
  if (hopcount) {
    _print_json_number(session, "hopcount", hopcount);
  }
  json_end_object(session);
  json_end_object(session);
}

static void
_print_graph(struct json_session *session,
    struct nhdp_domain *domain, int af_type) {
  const struct netaddr *originator;
  struct nhdp_neighbor *neigh;
  struct olsrv2_tc_node *node;
  struct olsrv2_tc_edge *edge;
  struct olsrv2_tc_attachment *attached;
  struct olsrv2_lan_entry *lan;
  struct avl_tree *rt_tree;
  struct olsrv2_routing_entry *rt_entry;

  originator = olsrv2_originator_get(af_type);
  if (netaddr_get_address_family(originator) != af_type) {
    return;
  }
  json_start_object(session, NULL);

  _print_json_string(session, "type", "NetworkGraph");
  _print_json_string(session, "protocol", "olsrv2");
  _print_json_string(session, "version", oonf_log_get_libdata()->version);
  _print_json_string(session, "revision", oonf_log_get_libdata()->git_commit);
  _print_json_netaddr(session, "router_id", originator);
  _print_json_string(session, "metric", domain->metric->name);

  json_start_array(session, "nodes");
  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    if (netaddr_get_address_family(&node->target.addr) == af_type) {
      _print_graph_node(session, &node->target.addr);
    }
  }
  json_end_array(session);

  json_start_array(session, "links");

  rt_tree = olsrv2_routing_get_tree(domain);

  /* print local links to neighbors */
  avl_for_each_element(nhdp_db_get_neigh_originator_tree(), neigh, _originator_node) {
    if (netaddr_get_address_family(&neigh->originator) == af_type
        && neigh->symmetric > 0) {
      rt_entry = avl_find_element(rt_tree, &neigh->originator, rt_entry, _node);

      _print_graph_edge(session, domain,
          originator, &neigh->originator,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.out,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.in,
          rt_entry != NULL && rt_entry->path_hops == 1);

      _print_graph_edge(session, domain,
          &neigh->originator, originator,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.in,
          nhdp_domain_get_neighbordata(domain, neigh)->metric.out,
          false);
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
              edge->inverse->cost[domain->index],
              edge->outgoing_tree[domain->index]);
        }
      }
    }
  }
  json_end_array(session);

  json_start_array(session, "endpoints");

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
      avl_for_each_element(&node->_attached_networks, attached, _src_node) {
        _print_graph_end(session, domain,
            &node->target.addr, &attached->dst->target.addr,
            attached->cost[domain->index],
            attached->distance[domain->index]);
      }
    }
  }
  json_end_array(session);

  json_end_object(session);
}

static void
_create_graph_json(struct json_session *session) {
  struct nhdp_domain *domain;
  list_for_each_element(nhdp_domain_get_list(), domain, _node) {
    _print_graph(session, domain, AF_INET);
    _print_graph(session, domain, AF_INET6);
  }
}

static void
_print_routing_tree(struct json_session *session,
    struct nhdp_domain *domain, int af_type) {
  struct olsrv2_routing_entry *rtentry;
  const struct netaddr *originator;
  char ibuf[IF_NAMESIZE];

  originator = olsrv2_originator_get(af_type);
  if (netaddr_get_address_family(originator) != af_type) {
    return;
  }

  json_start_object(session, NULL);

  _print_json_string(session, "type", "NetworkRoutes");
  _print_json_string(session, "protocol", "olsrv2");
  _print_json_string(session, "version", oonf_log_get_libdata()->version);
  _print_json_string(session, "revision", oonf_log_get_libdata()->git_commit);
  _print_json_netaddr(session, "router_id", originator);
  _print_json_string(session, "metric", domain->metric->name);

  json_start_array(session, COMMAND_ROUTES);

  avl_for_each_element(olsrv2_routing_get_tree(domain), rtentry, _node) {
    if (rtentry->route.family == af_type) {
      json_start_object(session, NULL);
      _print_json_netaddr(session, "destination", &rtentry->route.dst);
      if (netaddr_get_prefix_length(&rtentry->route.src_prefix) > 0) {
        _print_json_netaddr(session, "source", &rtentry->route.src_prefix);
      }
      _print_json_netaddr(session, "next", &rtentry->route.gw);
      _print_json_netaddr(session, "next_id", &rtentry->next_originator);

      _print_json_string(session, "device", if_indextoname(rtentry->route.if_index, ibuf));
      _print_json_number(session, "cost", rtentry->path_cost);

      json_start_object(session, "properties");
      _print_json_number(session, "hops", rtentry->path_hops);
      json_end_object(session);

      json_end_object(session);
    }
  }

  json_end_array(session);
  json_end_object(session);
}

static void
_create_routes_json(struct json_session *session) {
  struct nhdp_domain *domain;
  list_for_each_element(nhdp_domain_get_list(), domain, _node) {
    _print_routing_tree(session, domain, AF_INET);
    _print_routing_tree(session, domain, AF_INET6);
  }
}

static void
_create_error_json(struct json_session *session,
    const char *message, const char *parameter) {
  json_start_object(session, NULL);

  _print_json_string(session, "type", "Error");
  _print_json_string(session, "message", message);
  _print_json_string(session, "parameter", parameter);

  json_end_object(session);
}
/**
 * Callback for jsonfornet telnet command
 * @param con telnet connection
 * @return active or internal_error
 */
static enum oonf_telnet_result
_cb_jsonfornet(struct oonf_telnet_data *con) {
  struct json_session session;
  struct autobuf out;
  const char *ptr, *next;
  bool error;

  if (abuf_init(&out)) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  json_init_session(&session, &out);

  json_start_object(&session, NULL);

  _print_json_string(&session, "type", "NetworkCollection");
  json_start_array(&session, "collection");

  error = false;
  next = con->parameter;
  while (next && *next) {
    if ((ptr = str_hasnextword(next, COMMAND_GRAPH))) {
      _create_graph_json(&session);
    }
    else if ((ptr = str_hasnextword(next, COMMAND_ROUTES))) {
      _create_routes_json(&session);
    }
    else {
      ptr = str_skipnextword(next);
      error = true;
    }
    next = ptr;
  }

  if (error) {
    _create_error_json(&session, "unknown sub-command, use "
        COMMAND_GRAPH " or " COMMAND_ROUTES " subcommand",
        con->parameter);
  }
  json_end_array(&session);
  json_end_object(&session);

  /* copy output into telnet buffer */
  abuf_memcpy(con->out, abuf_getptr(&out), abuf_getlen(&out));
  abuf_free(&out);
  return TELNET_RESULT_ACTIVE;
}

static void
_print_json_string(struct json_session *session, const char *key, const char *value) {
  json_print(session, key, true, value);
}

static void
_print_json_number(struct json_session *session, const char *key, uint64_t value) {
  char buffer[21];

  snprintf(buffer, sizeof(buffer), "%" PRIu64, value);
  json_print(session, key, false, buffer);
}

static void
_print_json_netaddr(struct json_session *session, const char *key, const struct netaddr *addr) {
  struct netaddr_str nbuf;

  json_print(session, key, true, netaddr_to_string(&nbuf, addr));
}

