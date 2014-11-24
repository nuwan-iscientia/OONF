
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

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/template.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_telnet.h"
#include "subsystems/oonf_viewer.h"

#include "nhdp/nhdp.h"
#include "nhdp/nhdp_domain.h"
#include "olsrv2/olsrv2.h"
#include "olsrv2/olsrv2_lan.h"
#include "olsrv2/olsrv2_originator.h"
#include "olsrv2/olsrv2_routing.h"
#include "olsrv2/olsrv2_tc.h"

#include "olsrv2info/olsrv2info.h"

/* name of telnet subcommands/JSON nodes */
#define _JSON_NAME_ORIGINATOR         "originator"
#define _JSON_NAME_OLD_ORIGINATOR     "old_originator"
#define _JSON_NAME_LAN                "lan"
#define _JSON_NAME_NODE               "node"
#define _JSON_NAME_ATTACHED_NETWORK   "attached_network"
#define _JSON_NAME_EDGE               "edge"
#define _JSON_NAME_ROUTE              "route"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_olsrv2info(struct oonf_telnet_data *con);
static enum oonf_telnet_result _cb_olsrv2info_help(struct oonf_telnet_data *con);

static void _initialize_originator_values(int af_type);
static void _initialize_old_originator_values(struct olsrv2_originator_set_entry *);
static void _initialize_domain_values(struct nhdp_domain *domain);
static void _initialize_domain_metric_values(struct nhdp_domain *domain, uint32_t);
static void _initialize_domain_distance(uint8_t);
static void _initialize_domain_active(bool);
static void _initialize_lan_values(struct olsrv2_lan_entry *);
static void _initialize_node_values(struct olsrv2_tc_node *);
static void _initialize_attached_network_values(struct olsrv2_tc_attachment *edge);
static void _initialize_edge_values(struct olsrv2_tc_edge *edge);
static void _initialize_route_values(struct olsrv2_routing_entry *route);

static int _cb_create_text_originator(struct oonf_viewer_template *);
static int _cb_create_text_old_originator(struct oonf_viewer_template *);
static int _cb_create_text_lan(struct oonf_viewer_template *);
static int _cb_create_text_node(struct oonf_viewer_template *);
static int _cb_create_text_attached_network(struct oonf_viewer_template *);
static int _cb_create_text_edge(struct oonf_viewer_template *);
static int _cb_create_text_route(struct oonf_viewer_template *);

/*
 * list of template keys and corresponding buffers for values.
 *
 * The keys are API, so they should not be changed after published
 */
#define KEY_ORIGINATOR              "originator"

#define KEY_OLD_ORIGINATOR          "old_originator"
#define KEY_OLD_ORIGINATOR_VTIME    "old_originator_vtime"

#define KEY_DOMAIN                  "domain"
#define KEY_DOMAIN_METRIC           "domain_metric"
#define KEY_DOMAIN_METRIC_IN        "domain_metric_in"
#define KEY_DOMAIN_METRIC_OUT       "domain_metric_out"
#define KEY_DOMAIN_METRIC_IN_RAW    "domain_metric_in_raw"
#define KEY_DOMAIN_METRIC_OUT_RAW   "domain_metric_out_raw"

#define KEY_DOMAIN_DISTANCE         "domain_distance"
#define KEY_DOMAIN_ACTIVE           "domain_active"

#define KEY_LAN                     "lan"

#define KEY_NODE                    "node"
#define KEY_NODE_VTIME              "node_vtime"
#define KEY_NODE_ANSN               "node_ansn"

#define KEY_ATTACHED_NET            "attached_net"
#define KEY_ATTACHED_NET_ANSN       "attached_net_ansn"

#define KEY_EDGE                    "edge"
#define KEY_EDGE_ANSN               "edge_ansn"

#define KEY_ROUTE_SRC_IP            "route_src_ip"
#define KEY_ROUTE_GW                "route_gw"
#define KEY_ROUTE_DST               "route_dst"
#define KEY_ROUTE_SRC_PREFIX        "route_src_prefix"
#define KEY_ROUTE_METRIC            "route_metric"
#define KEY_ROUTE_TABLE             "route_table"
#define KEY_ROUTE_PROTO             "route_proto"
#define KEY_ROUTE_IF                "route_if"
#define KEY_ROUTE_IFINDEX           "route_ifindex"

/*
 * buffer space for values that will be assembled
 * into the output of the plugin
 */
static struct netaddr_str         _value_originator;

static struct netaddr_str         _value_old_originator;
static struct isonumber_str       _value_old_originator_vtime;

static char                       _value_domain[4];
static char                       _value_domain_metric[NHDP_DOMAIN_METRIC_MAXLEN];
static struct nhdp_metric_str     _value_domain_metric_out;
static char                       _value_domain_metric_out_raw[12];
static char                       _value_domain_distance[4];
static char                       _value_domain_active[TEMPLATE_JSON_BOOL_LENGTH];

static struct netaddr_str         _value_lan;

static struct netaddr_str         _value_node;
static struct isonumber_str       _value_node_vtime;
static char                       _value_node_ansn[6];

static struct netaddr_str         _value_attached_net;
static char                       _value_attached_net_ansn[6];

static struct netaddr_str         _value_edge;
static char                       _value_edge_ansn[6];

static struct netaddr_str         _value_route_dst;
static struct netaddr_str         _value_route_gw;
static struct netaddr_str         _value_route_src_ip;
static struct netaddr_str         _value_route_src_prefix;
static char                       _value_route_metric[12];
static char                       _value_route_table[4];
static char                       _value_route_proto[4];
static char                       _value_route_if[IF_NAMESIZE];
static char                       _value_route_ifindex[12];

/* definition of the template data entries for JSON and table output */
static struct abuf_template_data_entry _tde_originator[] = {
    { KEY_ORIGINATOR, _value_originator.buf, true },
};

static struct abuf_template_data_entry _tde_old_originator[] = {
    { KEY_OLD_ORIGINATOR, _value_old_originator.buf, true },
    { KEY_OLD_ORIGINATOR_VTIME, _value_old_originator_vtime.buf, false },
};

static struct abuf_template_data_entry _tde_domain[] = {
    { KEY_DOMAIN, _value_domain, true },
};

static struct abuf_template_data_entry _tde_domain_metric_out[] = {
    { KEY_DOMAIN_METRIC, _value_domain_metric, true },
    { KEY_DOMAIN_METRIC_OUT, _value_domain_metric_out.buf, true },
    { KEY_DOMAIN_METRIC_OUT_RAW, _value_domain_metric_out_raw, false },
};

static struct abuf_template_data_entry _tde_domain_lan_distance[] = {
    { KEY_DOMAIN_DISTANCE, _value_domain_distance, false },
};

static struct abuf_template_data_entry _tde_domain_lan_active[] = {
    { KEY_DOMAIN_ACTIVE, _value_domain_active, true },
};

static struct abuf_template_data_entry _tde_lan[] = {
    { KEY_LAN, _value_lan.buf, true },
};

static struct abuf_template_data_entry _tde_node_key[] = {
    { KEY_NODE, _value_node.buf, true },
};

static struct abuf_template_data_entry _tde_node[] = {
    { KEY_NODE, _value_node.buf, true },
    { KEY_NODE_ANSN, _value_node_ansn, false },
    { KEY_NODE_VTIME, _value_node_vtime.buf, false },
};

static struct abuf_template_data_entry _tde_attached_net[] = {
    { KEY_ATTACHED_NET, _value_attached_net.buf, true },
    { KEY_ATTACHED_NET_ANSN, _value_attached_net_ansn, false },
};

static struct abuf_template_data_entry _tde_edge[] = {
    { KEY_EDGE, _value_edge.buf, true },
    { KEY_EDGE_ANSN, _value_edge_ansn, false },
};

static struct abuf_template_data_entry _tde_route[] = {
    { KEY_ROUTE_DST, _value_route_dst.buf, true },
    { KEY_ROUTE_GW, _value_route_gw.buf, true },
    { KEY_ROUTE_SRC_IP, _value_route_src_ip.buf, true },
    { KEY_ROUTE_SRC_PREFIX, _value_route_src_prefix.buf, true },
    { KEY_ROUTE_METRIC, _value_route_metric, false },
    { KEY_ROUTE_TABLE, _value_route_table, false },
    { KEY_ROUTE_PROTO, _value_route_proto, false },
    { KEY_ROUTE_IF, _value_route_if, true },
    { KEY_ROUTE_IFINDEX, _value_route_ifindex, false },
};

static struct abuf_template_storage _template_storage;

/* Template Data objects (contain one or more Template Data Entries) */
static struct abuf_template_data _td_orig[] = {
    { _tde_originator, ARRAYSIZE(_tde_originator) },
};
static struct abuf_template_data _td_old_orig[] = {
    { _tde_old_originator, ARRAYSIZE(_tde_old_originator) },
};
static struct abuf_template_data _td_lan[] = {
    { _tde_lan, ARRAYSIZE(_tde_lan) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric_out, ARRAYSIZE(_tde_domain_metric_out) },
    { _tde_domain_lan_distance, ARRAYSIZE(_tde_domain_lan_distance) },
    { _tde_domain_lan_active, ARRAYSIZE(_tde_domain_lan_active) },
};
static struct abuf_template_data _td_node[] = {
    { _tde_node, ARRAYSIZE(_tde_node) },
};
static struct abuf_template_data _td_attached_net[] = {
    { _tde_node_key, ARRAYSIZE(_tde_node_key) },
    { _tde_attached_net, ARRAYSIZE(_tde_attached_net) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric_out, ARRAYSIZE(_tde_domain_metric_out) },
    { _tde_domain_lan_distance, ARRAYSIZE(_tde_domain_lan_distance) },
};
static struct abuf_template_data _td_edge[] = {
    { _tde_node_key, ARRAYSIZE(_tde_node_key) },
    { _tde_edge, ARRAYSIZE(_tde_edge) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric_out, ARRAYSIZE(_tde_domain_metric_out) },
};
static struct abuf_template_data _td_route[] = {
    { _tde_route, ARRAYSIZE(_tde_route) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric_out, ARRAYSIZE(_tde_domain_metric_out) },
};

/* OONF viewer templates (based on Template Data arrays) */
static struct oonf_viewer_template _templates[] = {
    {
        .data = _td_orig,
        .data_size = ARRAYSIZE(_td_orig),
        .json_name = _JSON_NAME_ORIGINATOR,
        .cb_function = _cb_create_text_originator,
    },
    {
        .data = _td_old_orig,
        .data_size = ARRAYSIZE(_td_old_orig),
        .json_name = _JSON_NAME_OLD_ORIGINATOR,
        .cb_function = _cb_create_text_old_originator,
    },
    {
        .data = _td_lan,
        .data_size = ARRAYSIZE(_td_lan),
        .json_name = _JSON_NAME_LAN,
        .cb_function = _cb_create_text_lan,
    },
    {
        .data = _td_node,
        .data_size = ARRAYSIZE(_td_node),
        .json_name = _JSON_NAME_NODE,
        .cb_function = _cb_create_text_node,
    },
    {
        .data = _td_attached_net,
        .data_size = ARRAYSIZE(_td_attached_net),
        .json_name = _JSON_NAME_ATTACHED_NETWORK,
        .cb_function = _cb_create_text_attached_network,
    },
    {
        .data = _td_edge,
        .data_size = ARRAYSIZE(_td_edge),
        .json_name = _JSON_NAME_EDGE,
        .cb_function = _cb_create_text_edge,
    },
    {
        .data = _td_route,
        .data_size = ARRAYSIZE(_td_route),
        .json_name = _JSON_NAME_ROUTE,
        .cb_function = _cb_create_text_route,
    }
};

/* telnet command of this plugin */
static struct oonf_telnet_command _telnet_commands[] = {
    TELNET_CMD(OONF_OLSRV2INFO_SUBSYSTEM, _cb_olsrv2info,
        "", .help_handler = _cb_olsrv2info_help),
};

/* plugin declaration */
static const char *_dependencies[] = {
  OONF_NHDP_SUBSYSTEM,
  OONF_OLSRV2_SUBSYSTEM,
};
struct oonf_subsystem olsrv2_olsrv2info_subsystem = {
  .name = OONF_OLSRV2INFO_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "OLSRv2 olsrv2 info plugin",
  .author = "Henning Rogge",
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(olsrv2_olsrv2info_subsystem);

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

/**
 * Callback for the telnet command of this plugin
 * @param con pointer to telnet session data
 * @return telnet result value
 */
static enum oonf_telnet_result
_cb_olsrv2info(struct oonf_telnet_data *con) {
  return oonf_viewer_telnet_handler(con->out, &_template_storage,
      OONF_OLSRV2INFO_SUBSYSTEM, con->parameter,
      _templates, ARRAYSIZE(_templates));
}

/**
 * Callback for the help output of this plugin
 * @param con pointer to telnet session data
 * @return telnet result value
 */
static enum oonf_telnet_result
_cb_olsrv2info_help(struct oonf_telnet_data *con) {
  return oonf_viewer_telnet_help(con->out, OONF_OLSRV2INFO_SUBSYSTEM,
      con->parameter, _templates, ARRAYSIZE(_templates));
}

/**
 * Initialize the value buffers for a NHDP interface address
 * @param if_addr interface NHDP address
 */
static void
_initialize_originator_values(int af_type) {
  netaddr_to_string(&_value_originator,
      olsrv2_originator_get(af_type));
}

/**
 * Initialize the value buffers for a NHDP interface address
 * @param if_addr interface NHDP address
 */
static void
_initialize_old_originator_values(struct olsrv2_originator_set_entry *entry) {
  netaddr_to_string(&_value_old_originator, &entry->originator);

  oonf_clock_toIntervalString(&_value_old_originator_vtime,
      oonf_timer_get_due(&entry->_vtime));
}

/**
 * Initialize the value buffers for a NHDP domain
 * @param domain NHDP domain
 */
static void
_initialize_domain_values(struct nhdp_domain *domain) {
  snprintf(_value_domain, sizeof(_value_domain), "%u", domain->ext);
  strscpy(_value_domain_metric, domain->metric->name, sizeof(_value_domain_metric));
}

/**
 * Initialize the value buffers for a metric value
 * @param domain NHDP domain
 * @param metric raw metric value
 */
static void
_initialize_domain_metric_values(struct nhdp_domain *domain,
    uint32_t metric) {
  nhdp_domain_get_metric_value(&_value_domain_metric_out,
      domain, metric);

  snprintf(_value_domain_metric_out_raw,
      sizeof(_value_domain_metric_out_raw), "%u", metric);

}

/**
 * Initialize the value buffer for the hopcount distance
 * @param distance hopcount distance
 */
static void
_initialize_domain_distance(uint8_t distance) {
  snprintf(_value_domain_distance, sizeof(_value_domain_distance),
      "%u", distance);
}

/**
 * Initialize the value buffer for the 'active' domain flag
 * @param active active domain flag
 */
static void
_initialize_domain_active(bool active) {
  strscpy(_value_domain_active, abuf_json_getbool(active),
      sizeof(_value_domain_active));
}

/**
 * Initialize the value buffer for a LAN entry
 * @param lan OLSRv2 LAN entry
 */
static void
_initialize_lan_values(struct olsrv2_lan_entry *lan) {
  netaddr_to_string(&_value_lan, &lan->prefix);
}

/**
 * Initialize the value buffers for an OLSRv2 node
 * @param node OLSRv2 node
 */
static void
_initialize_node_values(struct olsrv2_tc_node *node) {
  netaddr_to_string(&_value_node, &node->target.addr);

  oonf_clock_toIntervalString(&_value_node_vtime,
      oonf_timer_get_due(&node->_validity_time));

  snprintf(_value_node_ansn, sizeof(_value_node_ansn), "%u", node->ansn);
}

/**
 * Initialize the value buffers for an OLSRv2 attached network
 * @param edge attached network edge
 */
static void
_initialize_attached_network_values(struct olsrv2_tc_attachment *edge) {
  netaddr_to_string(&_value_attached_net, &edge->dst->target.addr);

  snprintf(_value_attached_net_ansn, sizeof(_value_attached_net_ansn),
      "%u", edge->ansn);
}

/**
 * Initialize the value buffers for an OLSRv2 edge
 * @param edge OLSRv2 edge
 */
static void
_initialize_edge_values(struct olsrv2_tc_edge *edge) {
  netaddr_to_string(&_value_edge, &edge->dst->target.addr);

  snprintf(_value_edge_ansn, sizeof(_value_edge_ansn),
      "%u", edge->ansn);
}

/**
 * Initialize the value buffers for a OLSRv2 route
 * @param route OLSRv2 routing entry
 */
static void
_initialize_route_values(struct olsrv2_routing_entry *route) {

  netaddr_to_string(&_value_route_dst, &route->route.dst);
  netaddr_to_string(&_value_route_gw, &route->route.gw);
  netaddr_to_string(&_value_route_src_ip, &route->route.src_ip);
  netaddr_to_string(&_value_route_src_prefix, &route->route.src_prefix);

  snprintf(_value_route_metric, sizeof(_value_route_metric),
      "%u", route->route.metric);
  snprintf(_value_route_table, sizeof(_value_route_table),
      "%u", route->route.table);
  snprintf(_value_route_proto, sizeof(_value_route_proto),
      "%u", route->route.protocol);

  if_indextoname(route->route.if_index, _value_route_if);
  snprintf(_value_route_ifindex, sizeof(_value_route_ifindex),
      "%u", route->route.if_index);
}

/**
 * Displays the known data about each NHDP interface.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_old_originator(struct oonf_viewer_template *template) {
  struct olsrv2_originator_set_entry *entry;

  avl_for_each_element(olsrv2_originator_get_tree(), entry, _node) {
    _initialize_old_originator_values(entry);

    /* generate template output */
    oonf_viewer_output_print_line(template);
  }
  return 0;
}

/**
 * Display the originator addresses of the local node
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_originator(struct oonf_viewer_template *template) {
  /* generate template output */
  _initialize_originator_values(AF_INET);
  oonf_viewer_output_print_line(template);

  /* generate template output */
  _initialize_originator_values(AF_INET6);
  oonf_viewer_output_print_line(template);

  return 0;
}

/**
 * Display all locally attached networks
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_lan(struct oonf_viewer_template *template) {
  struct olsrv2_lan_entry *lan;
  struct nhdp_domain *domain;

  avl_for_each_element(olsrv2_lan_get_tree(), lan, _node) {
    _initialize_lan_values(lan);

    list_for_each_element(nhdp_domain_get_list(), domain, _node) {
      _initialize_domain_values(domain);
      _initialize_domain_metric_values(domain,
          olsrv2_lan_get_domaindata(domain, lan)->outgoing_metric);
      _initialize_domain_distance(
          olsrv2_lan_get_domaindata(domain, lan)->distance);
      _initialize_domain_active(
          olsrv2_lan_get_domaindata(domain, lan)->active);

      oonf_viewer_output_print_line(template);
    }
  }
  return 0;
}

/**
 * Display all known OLSRv2 nodes
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_node(struct oonf_viewer_template *template) {
  struct olsrv2_tc_node *node;

  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    if (olsrv2_tc_is_node_virtual(node)) {
      continue;
    }
    _initialize_node_values(node);

    oonf_viewer_output_print_line(template);
  }
  return 0;
}

/**
 * Display all known OLSRv2 attached networks
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_attached_network(struct oonf_viewer_template *template) {
  struct olsrv2_tc_node *node;
  struct olsrv2_tc_attachment *attached;
  struct nhdp_domain *domain;

  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    _initialize_node_values(node);

    if (olsrv2_tc_is_node_virtual(node)) {
      continue;
    }

    avl_for_each_element(&node->_endpoints, attached, _src_node) {
      _initialize_attached_network_values(attached);

      list_for_each_element(nhdp_domain_get_list(), domain, _node) {
        _initialize_domain_values(domain);
        _initialize_domain_metric_values(domain,
            olsrv2_tc_attachment_get_metric(domain, attached));
        _initialize_domain_distance(
            olsrv2_tc_attachment_get_distance(domain, attached));

        oonf_viewer_output_print_line(template);
      }
    }
  }
  return 0;
}

/**
 * Display all known OLSRv2 edges
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_edge(struct oonf_viewer_template *template) {
  struct olsrv2_tc_node *node;
  struct olsrv2_tc_edge *edge;
  struct nhdp_domain *domain;

  avl_for_each_element(olsrv2_tc_get_tree(), node, _originator_node) {
    _initialize_node_values(node);

    if (olsrv2_tc_is_node_virtual(node)) {
      continue;
    }
    avl_for_each_element(&node->_edges, edge, _node) {
      if (edge->virtual) {
        continue;
      }

      _initialize_edge_values(edge);

      list_for_each_element(nhdp_domain_get_list(), domain, _node) {
        _initialize_domain_values(domain);
        _initialize_domain_metric_values(domain,
            olsrv2_tc_edge_get_metric(domain, edge));

        oonf_viewer_output_print_line(template);
      }
    }
  }
  return 0;
}

/**
 * Display all current entries of the OLSRv2 routing table
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_route(struct oonf_viewer_template *template) {
  struct olsrv2_routing_entry *route;
  struct nhdp_domain *domain;

  list_for_each_element(nhdp_domain_get_list(), domain, _node) {
    _initialize_domain_values(domain);

    avl_for_each_element(olsrv2_routing_get_tree(domain->index),
        route, _node) {
      _initialize_domain_metric_values(domain, route->cost);
      _initialize_route_values(route);

      oonf_viewer_output_print_line(template);
    }
  }
  return 0;
}
