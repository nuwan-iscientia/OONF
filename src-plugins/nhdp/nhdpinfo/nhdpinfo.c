
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

#include "nhdp/nhdp_domain.h"
#include "nhdp/nhdp_interfaces.h"
#include "nhdp/nhdp_db.h"

#include "nhdpinfo/nhdpinfo.h"

/* Definitions */
#define LOG_NHDPINFO _olsrv2_nhdpinfo_subsystem.logging

/* name of telnet subcommands/JSON nodes */
#define _JSON_NAME_INTERFACE     "interface"
#define _JSON_NAME_IFADDR        "if_addr"
#define _JSON_NAME_LINK          "link"
#define _JSON_NAME_DOMAIN        "domain"
#define _JSON_NAME_LADDR         "link_addr"
#define _JSON_NAME_TWOHOP        "link_twohop"
#define _JSON_NAME_NEIGHBOR      "neighbor"
#define _JSON_NAME_NADDR         "neighbor_addr"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_nhdpinfo(struct oonf_telnet_data *con);
static enum oonf_telnet_result _cb_nhdpinfo_help(struct oonf_telnet_data *con);

static void _initialize_interface_values(struct nhdp_interface *nhdp_if);
static void _initialize_interface_address_values(struct nhdp_interface_addr *if_addr);
static void _initialize_nhdp_link_values(struct nhdp_link *lnk);
static void _initialize_nhdp_domain_metric_values(struct nhdp_domain *domain,
    struct nhdp_metric *metric);
static void _initialize_nhdp_neighbor_mpr_values(struct nhdp_domain *domain,
    struct nhdp_neighbor_domaindata *domaindata);
static void _initialize_nhdp_domain_metric_int_values(struct nhdp_domain *domain,
    struct nhdp_link *lnk);
static void _initialize_nhdp_link_address_values(struct nhdp_laddr *laddr);
static void _initialize_nhdp_link_twohop_values(struct nhdp_l2hop *twohop);
static void _initialize_nhdp_neighbor_values(struct nhdp_neighbor *neigh);
static void _initialize_nhdp_neighbor_address_values(struct nhdp_naddr *naddr);

static int _cb_create_text_interface(struct oonf_viewer_template *);
static int _cb_create_text_if_address(struct oonf_viewer_template *);
static int _cb_create_text_link(struct oonf_viewer_template *);
static int _cb_create_text_link_address(struct oonf_viewer_template *);
static int _cb_create_text_link_twohop(struct oonf_viewer_template *);
static int _cb_create_text_neighbor(struct oonf_viewer_template *);
static int _cb_create_text_neighbor_address(struct oonf_viewer_template *);

/*
 * list of template keys and corresponding buffers for values.
 *
 * The keys are API, so they should not be changed after published
 */
#define KEY_IF                      "if"
#define KEY_IF_BINDTO_V4            "if_bindto_v4"
#define KEY_IF_BINDTO_V6            "if_bindto_v6"
#define KEY_IF_MAC                  "if_mac"
#define KEY_IF_FLOODING_V4          "if_flooding_v4"
#define KEY_IF_FLOODING_V6          "if_flooding_v6"
#define KEY_IF_DUALSTACK_MODE       "if_dualstack_mode"
#define KEY_IF_ADDRESS              "if_address"
#define KEY_IF_ADDRESS_LOST         "if_address_lost"
#define KEY_IF_ADDRESS_LOST_VTIME   "if_address_lost_vtime"

#define KEY_LINK_BINDTO             "link_bindto"
#define KEY_LINK_VTIME_VALUE        "link_vtime_value"
#define KEY_LINK_ITIME_VALUE        "link_itime_value"
#define KEY_LINK_SYMTIME            "link_symtime"
#define KEY_LINK_HEARDTIME          "link_heardtime"
#define KEY_LINK_VTIME              "link_vtime"
#define KEY_LINK_STATUS             "link_status"
#define KEY_LINK_DUALSTACK          "link_dualstack"
#define KEY_LINK_MAC                "link_mac"

#define KEY_LINK_ADDRESS            "link_address"

#define KEY_TWOHOP_ADDRESS          "twohop_address"
#define KEY_TWOHOP_VTIME            "twohop_vtime"

#define KEY_NEIGHBOR_ORIGINATOR     "neighbor_originator"
#define KEY_NEIGHBOR_DUALSTACK      "neighbor_dualstack"
#define KEY_NEIGHBOR_FLOOD_LOCAL    "neighbor_flood_local"
#define KEY_NEIGHBOR_FLOOD_REMOTE   "neighbor_flood_remote"
#define KEY_NEIGHBOR_SYMMETRIC      "neighbor_symmetric"
#define KEY_NEIGHBOR_ADDRESS        "neighbor_address"
#define KEY_NEIGHBOR_ADDRESS_LOST   "neighbor_address_lost"
#define KEY_NEIGHBOR_ADDRESS_VTIME  "neighbor_address_lost_vtime"

#define KEY_DOMAIN                  "domain"
#define KEY_DOMAIN_METRIC           "domain_metric"
#define KEY_DOMAIN_METRIC_IN        "domain_metric_in"
#define KEY_DOMAIN_METRIC_OUT       "domain_metric_out"
#define KEY_DOMAIN_METRIC_IN_RAW    "domain_metric_in_raw"
#define KEY_DOMAIN_METRIC_OUT_RAW   "domain_metric_out_raw"
#define KEY_DOMAIN_METRIC_INTERNAL  "domain_metric_internal"
#define KEY_DOMAIN_MPR              "domain_mpr"
#define KEY_DOMAIN_MPR_LOCAL        "domain_mpr_local"
#define KEY_DOMAIN_MPR_REMOTE       "domain_mpr_remote"

/*
 * buffer space for values that will be assembled
 * into the output of the plugin
 */
static char                       _value_if[IF_NAMESIZE];
static struct netaddr_str         _value_if_bindto_v4;
static struct netaddr_str         _value_if_bindto_v6;
static struct netaddr_str         _value_if_mac;
static char                       _value_if_flooding_v4[TEMPLATE_JSON_BOOL_LENGTH];
static char                       _value_if_flooding_v6[TEMPLATE_JSON_BOOL_LENGTH];
static char                       _value_if_dualstack_mode[5];
static struct netaddr_str         _value_if_address;
static char                       _value_if_address_lost[TEMPLATE_JSON_BOOL_LENGTH];
static struct isonumber_str       _value_if_address_vtime;

static struct netaddr_str         _value_link_bindto;
static struct isonumber_str       _value_link_vtime_value;
static struct isonumber_str       _value_link_itime_value;
static struct isonumber_str       _value_link_symtime;
static struct isonumber_str       _value_link_heardtime;
static struct isonumber_str       _value_link_vtime;
static char                       _value_link_status[NHDP_LINK_STATUS_TXTLENGTH];
static struct netaddr_str         _value_link_dualstack;
static struct netaddr_str         _value_link_mac;

static struct netaddr_str         _value_link_address;

static struct netaddr_str         _value_twohop_address;
static struct isonumber_str       _value_twohop_vtime;

static struct netaddr_str         _value_neighbor_originator;
static struct netaddr_str         _value_neighbor_dualstack;
static char                       _value_neighbor_flood_local[TEMPLATE_JSON_BOOL_LENGTH];
static char                       _value_neighbor_flood_remote[TEMPLATE_JSON_BOOL_LENGTH];
static char                       _value_neighbor_symmetric[TEMPLATE_JSON_BOOL_LENGTH];

static struct netaddr_str         _value_neighbor_address;
static char                       _value_neighbor_address_lost[TEMPLATE_JSON_BOOL_LENGTH];
static struct isonumber_str       _value_neighbor_address_lost_vtime;

static char                       _value_domain[4];
static char                       _value_domain_metric[NHDP_DOMAIN_METRIC_MAXLEN];
static struct nhdp_metric_str     _value_domain_metric_in;
static struct nhdp_metric_str     _value_domain_metric_out;
static char                       _value_domain_metric_in_raw[12];
static char                       _value_domain_metric_out_raw[12];
static struct nhdp_metric_str     _value_domain_metric_internal;
static char                       _value_domain_mpr[NHDP_DOMAIN_MPR_MAXLEN];
static char                       _value_domain_mpr_local[TEMPLATE_JSON_BOOL_LENGTH];
static char                       _value_domain_mpr_remote[TEMPLATE_JSON_BOOL_LENGTH];


/* definition of the template data entries for JSON and table output */
static struct abuf_template_data_entry _tde_if_key[] = {
    { KEY_IF, _value_if, true },
};

static struct abuf_template_data_entry _tde_if[] = {
    { KEY_IF, _value_if, true },
    { KEY_IF_BINDTO_V4, _value_if_bindto_v4.buf, true },
    { KEY_IF_BINDTO_V6, _value_if_bindto_v6.buf, true },
    { KEY_IF_MAC, _value_if_mac.buf, true },
    { KEY_IF_FLOODING_V4, _value_if_flooding_v4, true },
    { KEY_IF_FLOODING_V6, _value_if_flooding_v6, true },
    { KEY_IF_DUALSTACK_MODE, _value_if_dualstack_mode, true },
};

static struct abuf_template_data_entry _tde_if_addr[] = {
    { KEY_IF_ADDRESS, _value_if_address.buf, true },
    { KEY_IF_ADDRESS_LOST, _value_if_address_lost, true },
    { KEY_IF_ADDRESS_LOST_VTIME, _value_if_address_vtime.buf, false },
};

static struct abuf_template_data_entry _tde_link_key[] = {
    { KEY_LINK_BINDTO, _value_link_bindto.buf, true },
    { KEY_NEIGHBOR_ORIGINATOR, _value_neighbor_originator.buf, true },
};

static struct abuf_template_data_entry _tde_link[] = {
    { KEY_LINK_BINDTO, _value_link_bindto.buf, true },
    { KEY_LINK_VTIME_VALUE, _value_link_vtime_value.buf, false },
    { KEY_LINK_ITIME_VALUE, _value_link_itime_value.buf, false },
    { KEY_LINK_SYMTIME, _value_link_symtime.buf, false },
    { KEY_LINK_HEARDTIME, _value_link_heardtime.buf, false },
    { KEY_LINK_VTIME, _value_link_vtime.buf, false },
    { KEY_LINK_STATUS, _value_link_status, true },
    { KEY_LINK_DUALSTACK, _value_link_dualstack.buf, true },
    { KEY_LINK_MAC, _value_link_mac.buf, true },
    { KEY_NEIGHBOR_ORIGINATOR, _value_neighbor_originator.buf, true },
    { KEY_NEIGHBOR_DUALSTACK, _value_neighbor_dualstack.buf, true },
};

static struct abuf_template_data_entry _tde_domain[] = {
    { KEY_DOMAIN, _value_domain, false },
};

static struct abuf_template_data_entry _tde_domain_metric[] = {
    { KEY_DOMAIN_METRIC, _value_domain_metric, true },
    { KEY_DOMAIN_METRIC_IN, _value_domain_metric_in.buf, true },
    { KEY_DOMAIN_METRIC_IN_RAW, _value_domain_metric_in_raw, false },
    { KEY_DOMAIN_METRIC_OUT, _value_domain_metric_out.buf, true },
    { KEY_DOMAIN_METRIC_OUT_RAW, _value_domain_metric_out_raw, false },
};
static struct abuf_template_data_entry _tde_domain_metric_int[] = {
    { KEY_DOMAIN_METRIC_INTERNAL, _value_domain_metric_internal.buf, true },
};

static struct abuf_template_data_entry _tde_domain_mpr[] = {
    { KEY_DOMAIN_MPR, _value_domain_mpr, true },
    { KEY_DOMAIN_MPR_LOCAL, _value_domain_mpr_local, true },
    { KEY_DOMAIN_MPR_REMOTE, _value_domain_mpr_remote, true},
};

static struct abuf_template_data_entry _tde_link_addr[] = {
    { KEY_LINK_ADDRESS, _value_link_address.buf, true },
};

static struct abuf_template_data_entry _tde_twohop_addr[] = {
    { KEY_TWOHOP_ADDRESS, _value_twohop_address.buf, true },
    { KEY_TWOHOP_VTIME, _value_twohop_vtime.buf, false },
};

static struct abuf_template_data_entry _tde_neigh[] = {
    { KEY_NEIGHBOR_ORIGINATOR, _value_neighbor_originator.buf, true },
    { KEY_NEIGHBOR_DUALSTACK, _value_neighbor_dualstack.buf, true },
    { KEY_NEIGHBOR_FLOOD_LOCAL, _value_neighbor_flood_local, true },
    { KEY_NEIGHBOR_FLOOD_REMOTE, _value_neighbor_flood_remote, true },
    { KEY_NEIGHBOR_SYMMETRIC, _value_neighbor_symmetric, true },
};

static struct abuf_template_data_entry _tde_neigh_key[] = {
    { KEY_NEIGHBOR_ORIGINATOR, _value_neighbor_originator.buf, true },
};

static struct abuf_template_data_entry _tde_neigh_addr[] = {
    { KEY_NEIGHBOR_ADDRESS, _value_neighbor_address.buf, true },
    { KEY_NEIGHBOR_ADDRESS_LOST, _value_neighbor_address_lost, true },
    { KEY_NEIGHBOR_ADDRESS_VTIME, _value_neighbor_address_lost_vtime.buf, false },
};

static struct abuf_template_storage _template_storage;

/* Template Data objects (contain one or more Template Data Entries) */
static struct abuf_template_data _td_if[] = {
    { _tde_if, ARRAYSIZE(_tde_if) },
};
static struct abuf_template_data _td_if_addr[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_if_addr, ARRAYSIZE(_tde_if_addr) },
};
static struct abuf_template_data _td_link[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_link, ARRAYSIZE(_tde_link) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric, ARRAYSIZE(_tde_domain_metric) },
    { _tde_domain_metric_int, ARRAYSIZE(_tde_domain_metric_int) },
};
static struct abuf_template_data _td_link_addr[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_link_key, ARRAYSIZE(_tde_link_key) },
    { _tde_link_addr, ARRAYSIZE(_tde_link_addr) },
};
static struct abuf_template_data _td_twohop_addr[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_link_key, ARRAYSIZE(_tde_link_key) },
    { _tde_twohop_addr, ARRAYSIZE(_tde_twohop_addr) },
};
static struct abuf_template_data _td_neigh[] = {
    { _tde_neigh, ARRAYSIZE(_tde_neigh) },
    { _tde_domain, ARRAYSIZE(_tde_domain) },
    { _tde_domain_metric, ARRAYSIZE(_tde_domain_metric) },
    { _tde_domain_mpr, ARRAYSIZE(_tde_domain_mpr) },
};
static struct abuf_template_data _td_neigh_addr[] = {
    { _tde_neigh_key, ARRAYSIZE(_tde_neigh_key) },
    { _tde_neigh_addr, ARRAYSIZE(_tde_neigh_addr) },
};

/* OONF viewer templates (based on Template Data arrays) */
static struct oonf_viewer_template _templates[] = {
    {
        .data = _td_if,
        .data_size = ARRAYSIZE(_td_if),
        .json_name = _JSON_NAME_INTERFACE,
        .cb_function = _cb_create_text_interface,
    },
    {
        .data = _td_if_addr,
        .data_size = ARRAYSIZE(_td_if_addr),
        .json_name = _JSON_NAME_IFADDR,
        .cb_function = _cb_create_text_if_address,
    },
    {
        .data = _td_link,
        .data_size = ARRAYSIZE(_td_link),
        .json_name = _JSON_NAME_LINK,
        .cb_function = _cb_create_text_link,
    },
    {
        .data = _td_link_addr,
        .data_size = ARRAYSIZE(_td_link_addr),
        .json_name = _JSON_NAME_LADDR,
        .cb_function = _cb_create_text_link_address,
    },
    {
        .data = _td_twohop_addr,
        .data_size = ARRAYSIZE(_td_twohop_addr),
        .json_name = _JSON_NAME_TWOHOP,
        .cb_function = _cb_create_text_link_twohop,
    },
    {
        .data = _td_neigh,
        .data_size = ARRAYSIZE(_td_neigh),
        .json_name = _JSON_NAME_NEIGHBOR,
        .cb_function = _cb_create_text_neighbor,
    },
    {
        .data = _td_neigh_addr,
        .data_size = ARRAYSIZE(_td_neigh_addr),
        .json_name = _JSON_NAME_NADDR,
        .cb_function = _cb_create_text_neighbor_address,
    }
};

/* telnet command of this plugin */
static struct oonf_telnet_command _telnet_commands[] = {
    TELNET_CMD(OONF_NHDPINFO_SUBSYSTEM, _cb_nhdpinfo,
        "", .help_handler = _cb_nhdpinfo_help),
};

/* plugin declaration */
static const char *_dependencies[] = {
  OONF_CLOCK_SUBSYSTEM,
  OONF_TELNET_SUBSYSTEM,
  OONF_VIEWER_SUBSYSTEM,
  OONF_NHDP_SUBSYSTEM,
};
static struct oonf_subsystem _olsrv2_nhdpinfo_subsystem = {
  .name = OONF_NHDPINFO_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "NHDPinfo plugin",
  .author = "Henning Rogge",
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(_olsrv2_nhdpinfo_subsystem);

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
_cb_nhdpinfo(struct oonf_telnet_data *con) {
  int result;

  /* call template based subcommands first */
  result = oonf_viewer_call_subcommands(con->out, &_template_storage,
      con->parameter, _templates, ARRAYSIZE(_templates));
  if (result == 0) {
    return TELNET_RESULT_ACTIVE;
  }
  if (result < 0) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  if (con->parameter == NULL || *con->parameter == 0) {
    abuf_puts(con->out, "Error, '" OONF_NHDPINFO_SUBSYSTEM "' needs a parameter\n");
  }
  abuf_appendf(con->out, "Wrong parameter in command: %s\n", con->parameter);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Callback for the help output of this plugin
 * @param con pointer to telnet session data
 * @return telnet result value
 */
static enum oonf_telnet_result
_cb_nhdpinfo_help(struct oonf_telnet_data *con) {
  const char *next;

  /* skip the nhdpinfo command */
  next = str_hasnextword(con->parameter, OONF_NHDPINFO_SUBSYSTEM);

  /* print out own help text */
  abuf_puts(con->out, "NHDP database information command\n");
  oonf_viewer_print_help(con->out, next, _templates, ARRAYSIZE(_templates));

  return TELNET_RESULT_ACTIVE;
}

/**
 * Initialize the value buffers for a NHDP interface
 * @param nhdp_if nhdp interface
 */
static void
_initialize_interface_values(struct nhdp_interface *nhdp_if) {
  struct oonf_interface *core_if;
  struct netaddr temp_addr;

  core_if = nhdp_interface_get_coreif(nhdp_if);

  /* fill output buffers for template engine */
  strscpy(_value_if, nhdp_interface_get_name(nhdp_if), sizeof(_value_if));

  netaddr_from_socket(&temp_addr,
      &nhdp_if->rfc5444_if.interface->_socket.socket_v4.local_socket);
  netaddr_to_string(&_value_if_bindto_v4, &temp_addr);

  netaddr_from_socket(&temp_addr,
      &nhdp_if->rfc5444_if.interface->_socket.socket_v6.local_socket);
  netaddr_to_string(&_value_if_bindto_v6, &temp_addr);

  netaddr_to_string(&_value_if_mac, &core_if->data.mac);

  strscpy(_value_if_flooding_v4,
      abuf_json_getbool(nhdp_if->use_ipv4_for_flooding), TEMPLATE_JSON_BOOL_LENGTH);

  strscpy(_value_if_flooding_v6,
      abuf_json_getbool(nhdp_if->use_ipv6_for_flooding), TEMPLATE_JSON_BOOL_LENGTH);

  if (nhdp_if->dualstack_af_type == AF_INET) {
    strscpy(_value_if_dualstack_mode, "IPv4", sizeof(_value_if_dualstack_mode));
  }
  else if (nhdp_if->dualstack_af_type == AF_INET6) {
    strscpy(_value_if_dualstack_mode, "IPv6", sizeof(_value_if_dualstack_mode));
  }
  else {
    strscpy(_value_if_dualstack_mode, "-", sizeof(_value_if_dualstack_mode));
  }
}

/**
 * Initialize the value buffers for a NHDP interface address
 * @param if_addr interface NHDP address
 */
static void
_initialize_interface_address_values(struct nhdp_interface_addr *if_addr) {
  netaddr_to_string(&_value_if_address, &if_addr->if_addr);

  strscpy(_value_if_address_lost, abuf_json_getbool(if_addr->removed),
      sizeof(_value_if_address_lost));

  if (oonf_timer_is_active(&if_addr->_vtime)) {
    uint64_t due = oonf_timer_get_due(&if_addr->_vtime);
    oonf_clock_toIntervalString(&_value_if_address_vtime, due);
  }
  else {
    strscpy(_value_if_address_vtime.buf, "-1", sizeof(_value_if_address_vtime));
  }
}

/**
 * Initialize the value buffers for a NHDP link
 * @param lnk NHDP link
 */
static void
_initialize_nhdp_link_values(struct nhdp_link *lnk) {
  netaddr_to_string(&_value_link_bindto, &lnk->if_addr);

  oonf_clock_toIntervalString(&_value_link_vtime_value, lnk->vtime_value);
  oonf_clock_toIntervalString(&_value_link_itime_value, lnk->itime_value);

  oonf_clock_toIntervalString(&_value_link_symtime,
      oonf_timer_get_due(&lnk->sym_time));
  oonf_clock_toIntervalString(&_value_link_heardtime,
      oonf_timer_get_due(&lnk->heard_time));
  oonf_clock_toIntervalString(&_value_link_vtime,
      oonf_timer_get_due(&lnk->vtime));

  strscpy(_value_link_status, nhdp_db_link_status_to_string(lnk),
      sizeof(_value_link_status));

  if (lnk->dualstack_partner) {
    netaddr_to_string(
        &_value_link_dualstack, &lnk->dualstack_partner->if_addr);
  }
  else {
    strscpy(_value_link_dualstack.buf, "-", sizeof(_value_link_dualstack));
  }

  netaddr_to_string(&_value_link_mac, &lnk->remote_mac);
}

/**
 * Initialize the value buffers for NHDP domain metric values
 * @param domain NHDP domain
 * @param metric NHDP metric
 */
static void
_initialize_nhdp_domain_metric_values(struct nhdp_domain *domain,
    struct nhdp_metric *metric) {
  snprintf(_value_domain, sizeof(_value_domain), "%u", domain->ext);
  strscpy(_value_domain_metric, domain->metric->name, sizeof(_value_domain_metric));

  nhdp_domain_get_metric_value(&_value_domain_metric_in, domain, metric->in);
  nhdp_domain_get_metric_value(&_value_domain_metric_out, domain, metric->out);

  snprintf(_value_domain_metric_in_raw,
      sizeof(_value_domain_metric_in_raw), "%u", metric->in);
  snprintf(_value_domain_metric_out_raw,
      sizeof(_value_domain_metric_out_raw), "%u", metric->out);
}

/**
 * Initialize the value buffers for a NHDP domain MPR values
 * @param domain NHDP domain
 * @param domaindata NHDP neighbor domain data
 */
static void
_initialize_nhdp_neighbor_mpr_values(struct nhdp_domain *domain,
    struct nhdp_neighbor_domaindata *domaindata) {
  snprintf(_value_domain, sizeof(_value_domain), "%u", domain->ext);
  strscpy(_value_domain_mpr, domain->mpr->name, sizeof(_value_domain_mpr));

  strscpy(_value_domain_mpr_local,
      abuf_json_getbool(domaindata->local_is_mpr),
      sizeof(_value_domain_mpr_local));

  strscpy(_value_domain_mpr_remote,
      abuf_json_getbool(domaindata->neigh_is_mpr),
      sizeof(_value_domain_mpr_remote));

}

static void
_initialize_nhdp_domain_metric_int_values(struct nhdp_domain *domain,
    struct nhdp_link *lnk) {
  nhdp_domain_get_internal_link_metric_value(
      &_value_domain_metric_internal, domain, lnk);
}

/**
 * Initialize the value buffers for a NHDP link address
 * @param laddr NHDP link address
 */
static void
_initialize_nhdp_link_address_values(struct nhdp_laddr *laddr) {
  netaddr_to_string(&_value_link_address, &laddr->link_addr);
}

/**
 * Initialize the value buffers for a NHDP link twohop address
 * @param twohop NHDP twohop address
 */
static void
_initialize_nhdp_link_twohop_values(struct nhdp_l2hop *twohop) {
  netaddr_to_string(&_value_twohop_address, &twohop->twohop_addr);

  oonf_clock_toIntervalString(&_value_twohop_vtime,
      oonf_timer_get_due(&twohop->_vtime));
}

/**
 * Initialize the value buffers for a NHDP neighbor
 * @param neigh NHDP neighbor
 */
static void
_initialize_nhdp_neighbor_values(struct nhdp_neighbor *neigh) {
  netaddr_to_string(&_value_neighbor_originator, &neigh->originator);
  if (neigh->dualstack_partner) {
    netaddr_to_string(
        &_value_neighbor_dualstack, &neigh->dualstack_partner->originator);
  }
  else {
    strscpy(_value_neighbor_dualstack.buf, "-", sizeof(_value_neighbor_dualstack));
  }

  strscpy(_value_neighbor_flood_local,
      abuf_json_getbool(neigh->local_is_flooding_mpr),
      sizeof(_value_neighbor_flood_local));
  strscpy(_value_neighbor_flood_remote,
      abuf_json_getbool(neigh->neigh_is_flooding_mpr),
      sizeof(_value_neighbor_flood_remote));

  strscpy(_value_neighbor_symmetric,
      abuf_json_getbool(neigh->symmetric > 0),
      sizeof(_value_neighbor_symmetric));
}

/**
 * Initialize the value buffers for a NHDP neighbor address
 * @param naddr NHDP neighbor address
 */
static void
_initialize_nhdp_neighbor_address_values(struct nhdp_naddr *naddr) {
  netaddr_to_string(&_value_neighbor_address, &naddr->neigh_addr);

  strscpy(_value_neighbor_address_lost,
      abuf_json_getbool(oonf_timer_is_active(&naddr->_lost_vtime)),
      sizeof(_value_neighbor_address_lost));

  oonf_clock_toIntervalString(&_value_neighbor_address_lost_vtime,
      oonf_timer_get_due(&naddr->_lost_vtime));
}

/**
 * Displays the known data about each NHDP interface.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_interface(struct oonf_viewer_template *template) {
  struct nhdp_interface *nhdpif;

  avl_for_each_element(nhdp_interface_get_tree(), nhdpif, _node) {
    _initialize_interface_values(nhdpif);

    /* generate template output */
    oonf_viewer_output_print_line(template);
  }
  return 0;
}

/**
 * Displays the addresses of a NHDP interface.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_if_address(struct oonf_viewer_template *template) {
  struct nhdp_interface *nhdp_if;
  struct nhdp_interface_addr *nhdp_addr;

  avl_for_each_element(nhdp_interface_get_tree(), nhdp_if, _node) {
    /* fill output buffers for template engine */
    _initialize_interface_values(nhdp_if);

    avl_for_each_element(&nhdp_if->_if_addresses, nhdp_addr, _if_node) {
      /* fill address specific output buffers for template engine */
      _initialize_interface_address_values(nhdp_addr);

      /* generate template output */
      oonf_viewer_output_print_line(template);
    }
  }
  return 0;
}

/**
 * Displays the data of a NHDP link.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_link(struct oonf_viewer_template *template) {
  struct nhdp_interface *nhdp_if;
  struct nhdp_link *nlink;
  struct nhdp_domain *domain;

  avl_for_each_element(nhdp_interface_get_tree(), nhdp_if, _node) {
    /* fill output buffers for template engine */
    _initialize_interface_values(nhdp_if);

    list_for_each_element(&nhdp_if->_links, nlink, _if_node) {
      _initialize_nhdp_link_values(nlink);
      _initialize_nhdp_neighbor_values(nlink->neigh);

      list_for_each_element(nhdp_domain_get_list(), domain, _node) {
        _initialize_nhdp_domain_metric_values(domain,
            &(nhdp_domain_get_linkdata(domain, nlink)->metric));
        _initialize_nhdp_domain_metric_int_values(domain, nlink);

        /* generate template output */
        oonf_viewer_output_print_line(template);
      }
    }
  }
  return 0;
}

/**
 * Displays the addresses of a NHDP link.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_link_address(struct oonf_viewer_template *template) {
  struct nhdp_interface *nhdpif;
  struct nhdp_link *nhdplink;
  struct nhdp_laddr *laddr;

  avl_for_each_element(nhdp_interface_get_tree(), nhdpif, _node) {
    /* fill output buffers for template engine */
    _initialize_interface_values(nhdpif);

    list_for_each_element(&nhdpif->_links, nhdplink, _if_node) {
      _initialize_nhdp_link_values(nhdplink);
      _initialize_nhdp_neighbor_values(nhdplink->neigh);

      avl_for_each_element(&nhdplink->_addresses, laddr, _link_node) {
        _initialize_nhdp_link_address_values(laddr);

        /* generate template output */
        oonf_viewer_output_print_line(template);
      }
    }
  }
  return 0;
}

/**
 * Displays the twohop neighbors of a NHDP link.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_link_twohop(struct oonf_viewer_template *template) {
  struct nhdp_interface *nhdpif;
  struct nhdp_link *nhdplink;
  struct nhdp_l2hop *twohop;
  struct nhdp_domain *domain;

  avl_for_each_element(nhdp_interface_get_tree(), nhdpif, _node) {
    /* fill output buffers for template engine */
    _initialize_interface_values(nhdpif);

    list_for_each_element(&nhdpif->_links, nhdplink, _if_node) {
      _initialize_nhdp_link_values(nhdplink);
      _initialize_nhdp_neighbor_values(nhdplink->neigh);

      avl_for_each_element(&nhdplink->_2hop, twohop, _link_node) {
        _initialize_nhdp_link_twohop_values(twohop);

        list_for_each_element(nhdp_domain_get_list(), domain, _node) {
          _initialize_nhdp_domain_metric_values(domain,
              &nhdp_domain_get_l2hopdata(domain, twohop)->metric);

          /* generate template output */
          oonf_viewer_output_print_line(template);
        }
      }
    }
  }
  return 0;
}

/**
 * Displays the data of a NHDP neighbor.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_neighbor(struct oonf_viewer_template *template) {
  struct nhdp_neighbor *neigh;
  struct nhdp_domain *domain;

  list_for_each_element(nhdp_db_get_neigh_list(), neigh, _global_node) {
    _initialize_nhdp_neighbor_values(neigh);

    list_for_each_element(nhdp_domain_get_list(), domain, _node) {
      struct nhdp_neighbor_domaindata *data;

      data = nhdp_domain_get_neighbordata(domain, neigh);

      _initialize_nhdp_domain_metric_values(domain, &data->metric);
      _initialize_nhdp_neighbor_mpr_values(domain, data);

      /* generate template output */
      oonf_viewer_output_print_line(template);
    }
  }
  return 0;
}

/**
 * Displays the addresses of a NHDP neighbor.
 * @param template oonf viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_neighbor_address(struct oonf_viewer_template *template) {
  struct nhdp_neighbor *neigh;
  struct nhdp_naddr *naddr;

  list_for_each_element(nhdp_db_get_neigh_list(), neigh, _global_node) {
    _initialize_nhdp_neighbor_values(neigh);

    avl_for_each_element(&neigh->_neigh_addresses, naddr, _neigh_node) {
      _initialize_nhdp_neighbor_address_values(naddr);

      /* generate template output */
      oonf_viewer_output_print_line(template);
    }
  }
  return 0;
}
