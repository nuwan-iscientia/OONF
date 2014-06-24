
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

#include <stdio.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"
#include "common/template.h"

#include "core/oonf_logging.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_telnet.h"
#include "subsystems/oonf_viewer.h"

#include "layer2info/layer2info.h"

/* name of telnet subcommands/JSON nodes */
#define _JSON_NAME_INTERFACE   "interface"
#define _JSON_NAME_NEIGHBOR    "neighbor"
#define _JSON_NAME_DEFAULT     "default"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_layer2info(struct oonf_telnet_data *con);
static enum oonf_telnet_result _cb_layer2info_help(struct oonf_telnet_data *con);

static void _initialize_interface_values(struct oonf_layer2_net *net);
static void _initialize_data_values(struct isonumber_str *dst,
    struct oonf_layer2_data *data,
    const struct oonf_layer2_metadata *metadata, size_t count);
static void _initialize_neighbor_values(struct oonf_layer2_neigh *neigh);

static int _cb_create_text_interface(struct oonf_viewer_template *);
static int _cb_create_text_neighbor(struct oonf_viewer_template *);
static int _cb_create_text_default(struct oonf_viewer_template *);

/*
 * list of template keys and corresponding buffers for values.
 *
 * The keys are API, so they should not be changed after published
 */
#define KEY_IF                          "if"
#define KEY_IF_INDEX                    "if_index"
#define KEY_IF_TYPE                     "if_type"
#define KEY_IF_IDENT                    "if_ident"
#define KEY_IF_IDENT_ADDR               "if_ident_addr"
#define KEY_IF_LOCAL_ADDR               "if_local_addr"
#define KEY_IF_LASTSEEN                 "if_lastseen"
#define KEY_IF_PREFIX                   "if_"

#define KEY_NEIGH_ADDR                  "neigh_addr"
#define KEY_NEIGH_LASTSEEN              "neigh_lastseen"
#define KEY_NEIGH_PREFIX                "neigh_"

/*
 * buffer space for values that will be assembled
 * into the output of the plugin
 */
static char                             _value_if[IF_NAMESIZE];
static char                             _value_if_index[12];
static char                             _value_if_type[16];
static char                             _value_if_ident[33];
static struct netaddr_str               _value_if_ident_addr;
static struct netaddr_str               _value_if_local_addr;
static struct isonumber_str             _value_if_lastseen;
static struct isonumber_str             _value_if_data[OONF_LAYER2_NET_COUNT];

static struct netaddr_str               _value_neigh_addr;
static struct isonumber_str             _value_neigh_lastseen;
static struct isonumber_str             _value_neigh_data[OONF_LAYER2_NEIGH_COUNT];

/* definition of the template data entries for JSON and table output */
static struct abuf_template_data_entry _tde_if_key[] = {
    { KEY_IF, _value_if, true },
    { KEY_IF_INDEX, _value_if_index, false },
    { KEY_IF_LOCAL_ADDR, _value_if_local_addr.buf, true },
};

static struct abuf_template_data_entry _tde_if[] = {
    { KEY_IF, _value_if, true },
    { KEY_IF_INDEX, _value_if_index, false },
    { KEY_IF_TYPE, _value_if_type, true },
    { KEY_IF_IDENT, _value_if_ident, true },
    { KEY_IF_IDENT_ADDR, _value_if_ident_addr.buf, true },
    { KEY_IF_LOCAL_ADDR, _value_if_local_addr.buf, true },
    { KEY_IF_LASTSEEN, _value_if_lastseen.buf, false },
};

static struct abuf_template_data_entry _tde_if_data[OONF_LAYER2_NET_COUNT];

static struct abuf_template_data_entry _tde_neigh[] = {
    { KEY_NEIGH_ADDR, _value_neigh_addr.buf, true },
    { KEY_NEIGH_LASTSEEN, _value_neigh_lastseen.buf, false },
};

static struct abuf_template_data_entry _tde_neigh_data[OONF_LAYER2_NEIGH_COUNT];

static struct abuf_template_storage _template_storage;
static struct autobuf _key_storage;

/* Template Data objects (contain one or more Template Data Entries) */
static struct abuf_template_data _td_if[] = {
    { _tde_if, ARRAYSIZE(_tde_if) },
    { _tde_if_data, ARRAYSIZE(_tde_if_data) },
};
static struct abuf_template_data _td_neigh[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_neigh, ARRAYSIZE(_tde_neigh) },
    { _tde_neigh_data, ARRAYSIZE(_tde_neigh_data) },
};
static struct abuf_template_data _td_default[] = {
    { _tde_if_key, ARRAYSIZE(_tde_if_key) },
    { _tde_neigh_data, ARRAYSIZE(_tde_neigh_data) },
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
        .data = _td_neigh,
        .data_size = ARRAYSIZE(_td_neigh),
        .json_name = _JSON_NAME_NEIGHBOR,
        .cb_function = _cb_create_text_neighbor,
    },
    {
        .data = _td_default,
        .data_size = ARRAYSIZE(_td_default),
        .json_name = _JSON_NAME_DEFAULT,
        .cb_function = _cb_create_text_default,
    },
};

/* telnet command of this plugin */
static struct oonf_telnet_command _telnet_commands[] = {
    TELNET_CMD(OONF_PLUGIN_GET_NAME(), _cb_layer2info,
        "", .help_handler = _cb_layer2info_help),
};

/* plugin declaration */
struct oonf_subsystem olsrv2_layer2info_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OLSRv2 layer2 info plugin",
  .author = "Henning Rogge",
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(olsrv2_layer2info_subsystem);

/**
 * Initialize plugin
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  size_t i;

  abuf_init(&_key_storage);

  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    _tde_if_data[i].key =
        abuf_getptr(&_key_storage) + abuf_getlen(&_key_storage);
    _tde_if_data[i].value = _value_if_data[i].buf;
    _tde_if_data[i].string = true;

    abuf_puts(&_key_storage, KEY_IF_PREFIX);
    abuf_puts(&_key_storage, oonf_layer2_metadata_net[i].key);
    abuf_memcpy(&_key_storage, "\0", 1);
  }

  for (i=0; i<OONF_LAYER2_NEIGH_COUNT; i++) {
    _tde_neigh_data[i].key =
        abuf_getptr(&_key_storage) + abuf_getlen(&_key_storage);
    _tde_neigh_data[i].value = _value_neigh_data[i].buf;
    _tde_neigh_data[i].string = true;

    abuf_puts(&_key_storage, KEY_NEIGH_PREFIX);
    abuf_puts(&_key_storage, oonf_layer2_metadata_neigh[i].key);
    abuf_memcpy(&_key_storage, "\0", 1);
  }

  oonf_telnet_add(&_telnet_commands[0]);

  return abuf_has_failed(&_key_storage) ? -1 : 0;
}

/**
 * Cleanup plugin
 */
static void
_cleanup(void) {
  oonf_telnet_remove(&_telnet_commands[0]);
  abuf_free(&_key_storage);
}

/**
 * Callback for the telnet command of this plugin
 * @param con pointer to telnet session data
 * @return telnet result value
 */
static enum oonf_telnet_result
_cb_layer2info(struct oonf_telnet_data *con) {
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
    abuf_puts(con->out, "Error, '" OONF_PLUGIN_GET_NAME() "' needs a parameter\n");
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
_cb_layer2info_help(struct oonf_telnet_data *con) {
  const char *next;

  /* skip the layer2info command */
  next = str_hasnextword(con->parameter, OONF_PLUGIN_GET_NAME());

  /* print out own help text */
  abuf_puts(con->out, "Layer2 database information command\n");
  oonf_viewer_print_help(con->out, next, _templates, ARRAYSIZE(_templates));

  return TELNET_RESULT_ACTIVE;
}

static void
_initialize_interface_values(struct oonf_layer2_net *net) {
  strscpy(_value_if, net->if_name, sizeof(_value_if));
  snprintf(_value_if_index, sizeof(_value_if_index), "%u", net->if_index);
  strscpy(_value_if_ident, net->if_ident, sizeof(_value_if_ident));
  netaddr_to_string(&_value_if_ident_addr, &net->if_idaddr);
  netaddr_to_string(&_value_if_local_addr, &net->addr);

  if (net->last_seen) {
    oonf_clock_toIntervalString(&_value_if_lastseen,
        -oonf_clock_get_relative(net->last_seen));
  }
  else {
    _value_if_lastseen.buf[0] = 0;
  }
}

static void
_initialize_data_values(struct isonumber_str *dst,
    struct oonf_layer2_data *data,
    const struct oonf_layer2_metadata *meta, size_t count) {
  size_t i;

  memset(dst, 0, sizeof(*dst) * count);

  for (i=0; i<count; i++) {
    if (oonf_layer2_has_value(&data[i])) {
      isonumber_from_s64(&dst[i], oonf_layer2_get_value(data),
          meta[i].unit, meta[i].fraction, meta[i].binary, true);
    }
  }
}

static void
_initialize_neighbor_values(struct oonf_layer2_neigh *neigh) {
  netaddr_to_string(&_value_neigh_addr, &neigh->addr);
  if (neigh->last_seen) {
    oonf_clock_toIntervalString(&_value_neigh_lastseen,
        -oonf_clock_get_relative(neigh->last_seen));
  }
  else {
    _value_neigh_lastseen.buf[0] = 0;
  }
}

static int
_cb_create_text_interface(struct oonf_viewer_template *template) {
  struct oonf_layer2_net *net;

  avl_for_each_element(&oonf_layer2_net_tree, net, _node) {
    _initialize_interface_values(net);
    _initialize_data_values(_value_if_data, net->data,
        oonf_layer2_metadata_net, OONF_LAYER2_NET_COUNT);

    /* generate template output */
    oonf_viewer_print_output_line(template);
  }
  return 0;
}

static int
_cb_create_text_neighbor(struct oonf_viewer_template *template) {
  struct oonf_layer2_neigh *neigh;
  struct oonf_layer2_net *net;

  avl_for_each_element(&oonf_layer2_net_tree, net, _node) {
    _initialize_interface_values(net);

    avl_for_each_element(&net->neighbors, neigh, _node) {
      _initialize_neighbor_values(neigh);
      _initialize_data_values(_value_neigh_data, neigh->data,
          oonf_layer2_metadata_neigh, OONF_LAYER2_NEIGH_COUNT);

      /* generate template output */
      oonf_viewer_print_output_line(template);
    }
  }
  return 0;
}

static int
_cb_create_text_default(struct oonf_viewer_template *template) {
  struct oonf_layer2_net *net;

  avl_for_each_element(&oonf_layer2_net_tree, net, _node) {
    _initialize_interface_values(net);
    _initialize_data_values(_value_neigh_data, net->neighdata,
        oonf_layer2_metadata_neigh, OONF_LAYER2_NEIGH_COUNT);

    /* generate template output */
    oonf_viewer_print_output_line(template);
  }
  return 0;
}
