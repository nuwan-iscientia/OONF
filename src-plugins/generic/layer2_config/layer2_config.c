
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

#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/common_types.h"
#include "config/cfg_schema.h"
#include "config/cfg_tobin.h"
#include "config/cfg_validate.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_timer.h"

#include "layer2_config/layer2_config.h"

/* definitions and constants */
#define LOG_LAYER2_CONFIG _oonf_layer2_config_subsystem.logging

enum { _MAX_L2_VALUE_LEN = 64 };

enum l2_data_type {
  L2_NET,
  L2_NET_IP,
  L2_DEF,
  L2_NEIGH,
  L2_NEIGH_IP,
  L2_DST,

  L2_TYPE_COUNT,
};

/* one layer2 configuration option for an interface */
struct l2_config_data {
  enum l2_data_type config_type;
  struct netaddr mac;

  int data_idx;

  enum oonf_layer2_data_type data_type;
  union oonf_layer2_value data;
  char txt_value[_MAX_L2_VALUE_LEN];
};

/* all configuration options for an interface */
struct l2_config_if_data {
  char interf[IF_NAMESIZE];

  struct oonf_timer_instance _reconfigure_timer;
  struct avl_node _node;

  size_t count;
  struct l2_config_data d[0];
};

/* Prototypes */
static int _init(void);
static void _cleanup(void);

static struct l2_config_if_data *_add_if_data(
    const char *ifname, size_t data_count);
static void _remove_if_data(struct l2_config_if_data *);

static int _cb_if_value_validator(
    struct autobuf *out, const char *section_name,
    const char *entry_name, const char *value,
    struct cfg_schema_entry *entries, size_t entry_count);
static int _cb_if_value_tobin(
    struct cfg_schema_entry *entries, size_t entry_count,
      const char *value, void *ptr);
static int _cb_neigh_value_validator(
    struct autobuf *out, const char *section_name,
    const char *entry_name, const char *value,
    struct cfg_schema_entry *entries, size_t entry_count);
static int _cb_neigh_value_tobin(
    struct cfg_schema_entry *entries, size_t entry_count,
      const char *value, void *ptr);

static void _cb_update_l2net(void *);
static void _cb_update_l2neigh(void *);
static void _cb_reconfigure(struct oonf_timer_instance *);

static void _configure_if_data(struct l2_config_if_data *if_data);
static void _cb_config_changed(void);

/* define configuration entries */

/*! configuration for linkdata */
static struct cfg_schema_entry _l2net_entries[] = {
  CFG_MAP_CHOICE_L2NET(l2_config_data, data_idx, "l2net_key", "", "Layer2 network key for configuration"),
  CFG_MAP_STRING_ARRAY(l2_config_data, txt_value, "l2net_value", "", "Layer2 network value", _MAX_L2_VALUE_LEN),
};
static struct cfg_schema_entry _l2net_def_entries[] = {
  CFG_MAP_CHOICE_L2NEIGH(l2_config_data, data_idx, "l2net_key", "", "Layer2 neighbor key for configuration"),
  CFG_MAP_STRING_ARRAY(l2_config_data, txt_value, "l2net_value", "", "Layer2 neighbor value for default neighbor data", _MAX_L2_VALUE_LEN),
};
static struct cfg_schema_entry _l2neigh_entries[] = {
  CFG_MAP_CHOICE_L2NET(l2_config_data, data_idx, "l2neigh_key", "", "Layer2 neighbor key for configuration"),
  CFG_MAP_NETADDR_MAC48(l2_config_data, mac, "l2neigh_mac", "", "MAC address of neighbor", false, false),
  CFG_MAP_STRING_ARRAY(l2_config_data, txt_value, "l2neigh_value", "", "Layer2 neighbor value", _MAX_L2_VALUE_LEN),
};
static struct cfg_schema_entry _l2neigh_ip_entries[] = {
  CFG_MAP_NETADDR_MAC48(l2_config_data, mac, "l2neigh_mac", "", "MAC address of neighbor", false, false),
  CFG_MAP_NETADDR_V46(l2_config_data, data.addr, "l2neigh_ip", "", "IP address to neighbor", false, false),
};
static struct cfg_schema_entry _l2neigh_dst_entries[] = {
  CFG_MAP_NETADDR_MAC48(l2_config_data, mac, "l2neigh_mac", "", "MAC address of neighbor", false, false),
  CFG_MAP_NETADDR_MAC48(l2_config_data, data.addr, "l2neigh_dst", "", "Secondary MAC address of neighbor", false, false),
};

static struct cfg_schema_token_customizer _if_value_customizer = {
  .cb_validator = _cb_if_value_validator,
  .cb_tobin = _cb_if_value_tobin,
};

static struct cfg_schema_token_customizer _neigh_value_customizer = {
  .cb_validator = _cb_neigh_value_validator,
  .cb_tobin = _cb_neigh_value_tobin,
};

static struct cfg_schema_entry _l2_config_if_entries[] = {
  [L2_NET]    = CFG_VALIDATE_TOKENS_CUSTOM("l2net", "",
    "Sets an interface wide layer2 entry into the database."
    " Parameters are the key of the interface data followed by the data.",
    _l2net_entries, _if_value_customizer, .list = true),
  [L2_NET_IP] = CFG_VALIDATE_NETADDR_V46("l2net_ip", "",
    "Sets an ip address/prefix for the local radio in the database", true, false, .list = true),
  [L2_DEF] = CFG_VALIDATE_TOKENS_CUSTOM("l2default", "",
    "Sets an interface wide default neighbor layer2 entry into the database."
    " Parameters are the key of the neighbor data followed by the data.",
    _l2net_def_entries, _neigh_value_customizer, .list = true),
  [L2_NEIGH] = CFG_VALIDATE_TOKENS_CUSTOM("l2neighbor", "",
    "Sets an neighbor specific layer2 entry into the database."
    " Parameters are the key of the neighbor data followed by the data and the mac address of the neighbor.",
    _l2neigh_entries, _neigh_value_customizer, .list = true),
  [L2_NEIGH_IP] = CFG_VALIDATE_TOKENS("l2neighbor_ip", "",
    "Sets an neighbor specific ip address/prefix into the database."
    " Parameters are the mac address and then the ip address/prefix.",
    _l2neigh_ip_entries, .list = true),
  [L2_DST] = CFG_VALIDATE_TOKENS("l2destination", "",
    "Sets an neighbor specific bridged MAC destination into the database."
    " Parameters are the mac address of the neighbor and then the proxied mac address.",
    _l2neigh_dst_entries, .list = true),
};

static struct cfg_schema_section _l2_config_section = {
  CFG_OSIF_SCHEMA_INTERFACE_SECTION_INIT,

  .cb_delta_handler = _cb_config_changed,
  .entries = _l2_config_if_entries,
  .entry_count = ARRAYSIZE(_l2_config_if_entries),
};

/* declare subsystem */
static const char *_dependencies[] = {
  OONF_LAYER2_SUBSYSTEM,
  OONF_TIMER_SUBSYSTEM,
};
static struct oonf_subsystem _oonf_layer2_config_subsystem = {
  .name = OONF_LAYER2_CONFIG_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .init = _init,
  .cleanup = _cleanup,

  .cfg_section = &_l2_config_section,
};
DECLARE_OONF_PLUGIN(_oonf_layer2_config_subsystem);

/* originator for smooth set/remove of configured layer2 values */
static struct oonf_layer2_origin _l2_origin_current = {
  .name = "layer2 config",
  .priority = OONF_LAYER2_ORIGIN_CONFIGURED,
};
static struct oonf_layer2_origin _l2_origin_old = {
  .name = "layer2 config old",
  .priority = OONF_LAYER2_ORIGIN_CONFIGURED,
};

/* listener for removal of layer2 data */
static struct oonf_class_extension _l2net_listener = {
  .ext_name = "link config listener",
  .class_name = LAYER2_CLASS_NETWORK,

  .cb_remove = _cb_update_l2net,
  .cb_change = _cb_update_l2net,
};
static struct oonf_class_extension _l2neigh_listener = {
  .ext_name = "link config listener",
  .class_name = LAYER2_CLASS_NEIGHBOR,

  .cb_remove = _cb_update_l2neigh,
  .cb_change = _cb_update_l2neigh,
};

/* interface data */
static struct avl_tree _if_data_tree;

/* interface reconfiguration timer */
static struct oonf_timer_class _reconfigure_timer = {
  .name = "layer2 reconfiguration",
  .callback = _cb_reconfigure,
};

/**
 * Subsystem constructor
 * @return always returns 0
 */
static int
_init(void) {
  oonf_layer2_origin_add(&_l2_origin_current);
  oonf_layer2_origin_add(&_l2_origin_old);

  oonf_class_extension_add(&_l2net_listener);
  oonf_class_extension_add(&_l2neigh_listener);

  oonf_timer_add(&_reconfigure_timer);

  avl_init(&_if_data_tree, avl_comp_strcasecmp, false);
  return 0;
}

/**
 * Subsystem destructor
 */
static void
_cleanup(void) {
  struct l2_config_if_data *if_data, *if_data_it;

  avl_for_each_element_safe(&_if_data_tree, if_data, _node, if_data_it) {
    _remove_if_data(if_data);
  }
  oonf_timer_remove(&_reconfigure_timer);

  oonf_class_extension_remove(&_l2net_listener);
  oonf_class_extension_remove(&_l2neigh_listener);

  oonf_layer2_origin_remove(&_l2_origin_current);
  oonf_layer2_origin_remove(&_l2_origin_old);
}

/**
 * Add a new layer2 config interface data
 * @param ifname name of interface
 * @param data_count number of data entries
 * @return interface entry, NULL if out of memory
 */
static struct l2_config_if_data *
_add_if_data(const char *ifname, size_t data_count) {
  struct l2_config_if_data *if_data;

  if_data = avl_find_element(&_if_data_tree, ifname, if_data, _node);
  if (if_data) {
    _remove_if_data(if_data);
  }

  if_data = calloc(1, sizeof(struct l2_config_if_data)
      + data_count * sizeof(struct l2_config_data));
  if (!if_data) {
    OONF_WARN(LOG_LAYER2_CONFIG, "Out of memory for %"PRINTF_SIZE_T_SPECIFIER
        " ifdata entries for interface %s", data_count, _l2_config_section.section_name);
    return NULL;
  }

  /* hook into tree */
  strscpy(if_data->interf, ifname, IF_NAMESIZE);
  if_data->_node.key = if_data->interf;

  avl_insert(&_if_data_tree, &if_data->_node);

  /* initialize timer */
  if_data->_reconfigure_timer.class = &_reconfigure_timer;

  return if_data;
}

/**
 * removes a layer2 config interface data
 * @param if_data interface data
 */
static void
_remove_if_data(struct l2_config_if_data *if_data) {
  if (!avl_is_node_added(&if_data->_node)) {
    return;
  }

  oonf_timer_stop(&if_data->_reconfigure_timer);
  avl_remove(&_if_data_tree, &if_data->_node);
  free (if_data);
}

/**
 * Validate interface setting for layer2 data
 * @param out buffer for validation errors
 * @param section_name name of configuration section
 * @param entry_name name of configuration entries
 * @param value value to be validated
 * @param entries subentries of token config options
 * @param entry_count number of subentries
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_if_value_validator(struct autobuf *out, const char *section_name,
    const char *entry_name, const char *value,
    struct cfg_schema_entry *entries, size_t entry_count) {
  struct l2_config_data l2_data;
  const struct oonf_layer2_metadata *meta;
  union oonf_layer2_value dst;

  if (cfg_tobin_tokens(&l2_data, value, entries, entry_count, NULL)) {
    return -1;
  }

  meta = oonf_layer2_net_metadata_get(l2_data.data_idx);

  if (oonf_layer2_data_parse_string(&dst, meta, l2_data.txt_value)) {
    cfg_append_printable_line(out,
        "Value '%s' for entry '%s' in section %s does not use the data"
        " type %s for layer2 network key %s",
        value, entry_name, section_name,
        oonf_layer2_data_get_type_string(meta->type), meta->key);
  }
  return 0;
}

/**
 * Finalize binary conversion of layer2 interface config entry
 * @param entries configuration subentries
 * @param entry_count number of subentries
 * @param value (non-tokenized) value of tokens
 * @param ptr pointer to binary destination
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_if_value_tobin(
    struct cfg_schema_entry *entries __attribute__((unused)),
    size_t entry_count __attribute__((unused)),
    const char *value __attribute__((unused)), void *ptr) {
  struct l2_config_data *l2_data;
  const struct oonf_layer2_metadata *meta;

  l2_data = ptr;

  meta = oonf_layer2_net_metadata_get(l2_data->data_idx);
  if (oonf_layer2_data_parse_string(&l2_data->data, meta, l2_data->txt_value)) {
    return -1;
  }
  l2_data->data_type = meta->type;
  return 0;
}

/**
 * Validate neighbor setting for layer2 data
 * @param out buffer for validation errors
 * @param section_name name of configuration section
 * @param entry_name name of configuration entries
 * @param value value to be validated
 * @param entries subentries of token config options
 * @param entry_count number of subentries
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_neigh_value_validator(struct autobuf *out, const char *section_name,
    const char *entry_name, const char *value,
    struct cfg_schema_entry *entries, size_t entry_count) {
  struct l2_config_data l2_data;
  const struct oonf_layer2_metadata *meta;
  union oonf_layer2_value dst;

  if (cfg_tobin_tokens(&l2_data, value, entries, entry_count, NULL)) {
    return -1;
  }

  meta = oonf_layer2_neigh_metadata_get(l2_data.data_idx);

  if (oonf_layer2_data_parse_string(&dst, meta, l2_data.txt_value)) {
    cfg_append_printable_line(out,
        "Value '%s' for entry '%s' in section %s does not use the data"
        " type %s for layer2 neighbor key %s",
        value, entry_name, section_name,
        oonf_layer2_data_get_type_string(meta->type), meta->key);
  }
  return 0;
}

/**
 * Finalize binary conversion of layer2 network config entry
 * @param entries configuration subentries
 * @param entry_count number of subentries
 * @param value (non-tokenized) value of tokens
 * @param ptr pointer to binary destination
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_neigh_value_tobin(
    struct cfg_schema_entry *entries __attribute__((unused)),
    size_t entry_count __attribute__((unused)),
    const char *value __attribute__((unused)), void *ptr) {
  struct l2_config_data *l2_data;
  const struct oonf_layer2_metadata *meta;

  l2_data = ptr;

  meta = oonf_layer2_neigh_metadata_get(l2_data->data_idx);
  if (oonf_layer2_data_parse_string(&l2_data->data, meta, l2_data->txt_value)) {
    return -1;
  }
  l2_data->data_type = meta->type;
  return 0;
}

/**
 * Callback when a layer2 network entry is changed/removed
 * @param ptr l2net entry
 */
static void
_cb_update_l2net(void *ptr) {
  struct oonf_layer2_net *l2net;
  struct l2_config_if_data *if_data;

  l2net = ptr;

  if_data = avl_find_element(&_if_data_tree, l2net->name, if_data, _node);
  if (if_data && !oonf_timer_is_active(&if_data->_reconfigure_timer)) {
    OONF_DEBUG(LOG_LAYER2_CONFIG, "Received update for l2net: %s", l2net->name);
    oonf_timer_set(&if_data->_reconfigure_timer, LAYER2_RECONFIG_DELAY);
  }
}

/**
 * Callback when a layer2 neighbor entry is changed/removed
 * @param ptr l2net entry
 */
static void
_cb_update_l2neigh(void *ptr) {
  struct oonf_layer2_neigh *l2neigh;
  struct l2_config_if_data *if_data;

  l2neigh = ptr;

  if_data = avl_find_element(&_if_data_tree, l2neigh->network->name, if_data, _node);
  if (if_data && !oonf_timer_is_active(&if_data->_reconfigure_timer)) {
    OONF_DEBUG(LOG_LAYER2_CONFIG, "Received update for l2neigh: %s", l2neigh->network->name);
    oonf_timer_set(&if_data->_reconfigure_timer, LAYER2_RECONFIG_DELAY);
  }
}

/**
 * Timer called for delayed layer2 config update
 * @param timer timer entry
 */
static void
_cb_reconfigure(struct oonf_timer_instance *timer) {
  struct l2_config_if_data *if_data;

  if_data = container_of(timer, typeof(*if_data), _reconfigure_timer);
  _configure_if_data(if_data);
}

/**
 * Apply a layer2 config interface to the l2 database
 * @param if_data interface data
 */
static void
_configure_if_data(struct l2_config_if_data *if_data) {
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_net *l2net;
  struct l2_config_data *entry;
  size_t i;


  l2net = oonf_layer2_net_get(if_data->interf);
  if (!l2net && if_data->count > 0) {
    l2net = oonf_layer2_net_add(if_data->interf);
    if (!l2net) {
      return;
    }
  }

  /* relabel old entries */
  if (l2net) {
    oonf_layer2_net_relabel(l2net, &_l2_origin_old, &_l2_origin_current);

    for (i=0; i<if_data->count; i++) {
      entry = &if_data->d[i];

      switch (entry->config_type) {
        case L2_NET:
          oonf_layer2_data_set(&l2net->data[entry->data_idx],
              &_l2_origin_current, entry->data_type, &entry->data);
          break;
        case L2_NET_IP:
          oonf_layer2_net_add_ip(l2net,
              &_l2_origin_current, &entry->data.addr);
          break;
        case L2_DEF:
          oonf_layer2_data_set(&l2net->neighdata[entry->data_idx],
              &_l2_origin_current, entry->data_type, &entry->data);
          break;
        case L2_NEIGH:
          l2neigh = oonf_layer2_neigh_add(l2net, &entry->mac);
          if (l2neigh) {
            oonf_layer2_data_set(&l2neigh->data[entry->data_idx],
                &_l2_origin_current, entry->data_type, &entry->data);
          }
          break;
        case L2_NEIGH_IP:
          l2neigh = oonf_layer2_neigh_add(l2net, &entry->mac);
          if (l2neigh) {
            oonf_layer2_neigh_add_ip(l2neigh,
                &_l2_origin_current, &entry->data.addr);
          }
          break;
        case L2_DST:
          l2neigh = oonf_layer2_neigh_add(l2net, &entry->mac);
          if (l2neigh) {
            oonf_layer2_destination_add(l2neigh,
                &entry->data.addr, &_l2_origin_current);
          }
          break;
        default:
          break;
      }
    }

    /* remove old data */
    oonf_layer2_net_remove(l2net, &_l2_origin_old);
  }

  /* stop update timer */
  oonf_timer_stop(&if_data->_reconfigure_timer);
}

/**
 * Parse configuration change
 */
static void
_cb_config_changed(void) {
  struct l2_config_if_data *if_data;
  struct cfg_entry *entry;
  char ifbuf[IF_NAMESIZE];
  const char *txt_value;
  const char *ifname;
  size_t i, total;

  ifname = cfg_get_phy_if(ifbuf, _l2_config_section.section_name);
  if_data = avl_find_element(&_if_data_tree, ifname, if_data, _node);
  if (if_data) {
    _remove_if_data(if_data);
  }

  if (!_l2_config_section.post) {
    /* section was removed */
     return;
  }

  total = 0;
  for (i=0; i<L2_TYPE_COUNT; i++) {
    entry = cfg_db_get_entry(_l2_config_section.post,
        _l2_config_if_entries[i].key.entry);
    if (entry) {
      total += strarray_get_count(&entry->val);
    }
  }

  if_data = _add_if_data(_l2_config_section.section_name, total);
  if (!if_data) {
    OONF_WARN(LOG_LAYER2_CONFIG, "Out of memory for %"PRINTF_SIZE_T_SPECIFIER
        " ifdata entries for interface %s", total, _l2_config_section.section_name);
    return;
  }

  /* initialize header */
  strscpy(if_data->interf, _l2_config_section.section_name, IF_NAMESIZE);
  if_data->_node.key = if_data->interf;

  avl_insert(&_if_data_tree, &if_data->_node);

  /* initialize data */
  total = 0;
  for (i=0; i<L2_TYPE_COUNT; i++) {
    entry = cfg_db_get_entry(_l2_config_section.post,
        _l2_config_if_entries[i].key.entry);
    if (entry) {
      strarray_for_each_element(&entry->val, txt_value) {

        /*
         * assume that the data type is "address", we will overwrite it for
         * other variants in the next function call.
         */
        if_data->d[total].data_type = OONF_LAYER2_NETWORK_DATA;
        if(!cfg_tobin_tokens(&if_data->d[total], txt_value,
            _l2_config_if_entries[i].validate_param[0].ptr,
            _l2_config_if_entries[i].validate_param[1].s,
            _l2_config_if_entries[i].validate_param[2].ptr)) {
          if_data->d[total].config_type = i;
          total++;
        }
      }
    }
  }

  if_data->count = total;

  /* reconfigure layer2 database */
  _configure_if_data(if_data);
}
