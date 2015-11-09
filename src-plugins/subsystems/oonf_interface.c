
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

#include <netinet/in.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"

#include "core/oonf_cfg.h"
#include "core/oonf_logging.h"
#include "core/oonf_main.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_interface.h"
#include "subsystems/os_system.h"

#include "subsystems/oonf_interface.h"

/* Definitions */
#define LOG_INTERFACE _oonf_interface_subsystem.logging

/*! timeinterval to delay change in interface to trigger actions */
#define OONF_INTERFACE_CHANGE_INTERVAL 100

/* prototypes */
static int _init(void);
static void _cleanup(void);
static void _early_cfg_init(void);

static const struct netaddr *_get_fixed_prefix(
    int af_type, struct netaddr_acl *filter);
static const struct netaddr *_get_exact_match_bindaddress(
    int af_type, struct netaddr_acl *filter, struct os_interface_data *ifdata);
static const struct netaddr *_get_matching_bindaddress(
    int af_type, struct netaddr_acl *filter, struct os_interface_data *ifdata);

static struct os_interface *_interface_add(const char *, bool mesh);
static void _interface_remove(struct os_interface *interf, bool mesh);
static int _handle_unused_parameter(const char *);
static void _cb_change_handler(void *);
static void _trigger_ifgeneric_change_timer(void);
static void _trigger_ifspecific_change_timer(struct os_interface *);

/* global tree of known interfaces */
static struct avl_tree _oonf_interface_tree;

/* subsystem definition */
static const char *_dependencies[] = {
  OONF_CLASS_SUBSYSTEM,
  OONF_TIMER_SUBSYSTEM,
  OONF_OS_SYSTEM_SUBSYSTEM,
  OONF_OS_INTERFACE_SUBSYSTEM,
};

static struct oonf_subsystem _oonf_interface_subsystem = {
  .name = OONF_INTERFACE_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .early_cfg_init = _early_cfg_init,
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(_oonf_interface_subsystem);

static struct list_entity _interface_listener;
static struct oonf_timer_class _change_timer_info = {
  .name = "Interface change",
  .callback = _cb_change_handler,
};
static struct oonf_timer_instance _other_if_change_timer = {
  .class = &_change_timer_info,
};
static uint64_t _other_retrigger_timeout;

static struct os_interface_if_listener _iflistener = {
  .if_changed = oonf_interface_trigger_ifindex,
};

static struct oonf_class _if_class = {
  .name = OONF_CLASS_INTERFACE,
  .size = sizeof(struct os_interface),
};

/**
 * Initialize interface subsystem
 * @return always returns 0
 */
static int
_init(void) {
  /* initialize data structures */
  oonf_timer_add(&_change_timer_info);
  oonf_class_add(&_if_class);

  avl_init(&_oonf_interface_tree, avl_comp_strcasecmp, false);
  list_init_head(&_interface_listener);

  os_interface_listener_add(&_iflistener);
  return 0;
}

/**
 * Cleanup interface subsystem
 */
static void
_cleanup(void) {
  struct oonf_interface_listener *listener, *l_it;

  list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {
    oonf_interface_remove_listener(listener);
  }

  os_interface_listener_remove(&_iflistener);
  oonf_class_remove(&_if_class);
  oonf_timer_remove(&_change_timer_info);
}

static
void _early_cfg_init(void) {
  oonf_main_set_parameter_handler(_handle_unused_parameter);
}

/**
 * Add a listener to a specific interface
 * @param listener initialized listener object
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_interface_add_listener(
    struct oonf_interface_listener *listener) {
  if (list_is_node_added(&listener->_node)) {
    return 0;
  }

  OONF_DEBUG(LOG_INTERFACE, "Add listener for interface %s", listener->name);

  if (listener->name && listener->name[0]) {
    listener->interface = _interface_add(listener->name, listener->mesh);
    if (listener->interface == NULL) {
      return -1;
    }
  }

  list_add_tail(&_interface_listener, &listener->_node);
  return 0;
}

/**
 * Removes a listener to an interface object
 * @param listener pointer to listener object
 */
void
oonf_interface_remove_listener(
    struct oonf_interface_listener *listener) {
  if (!list_is_node_added(&listener->_node)) {
    return;
  }

  OONF_DEBUG(LOG_INTERFACE, "Remove listener for interface %s", listener->name);

  if (listener->interface) {
    _interface_remove(listener->interface, listener->mesh);
  }

  list_remove(&listener->_node);
}

/**
 * Trigger a potential change in the interface settings. Normally
 * called by os_system code
 * @param if_index interface index
 * @param down true if interface is going down, false otherwise
 */
void
oonf_interface_trigger_ifindex(unsigned if_index, bool down) {
  char if_name[IF_NAMESIZE];
  struct os_interface *interf = NULL, *if_ptr;

  if (if_indextoname(if_index, if_name) != NULL) {
    interf = avl_find_element(&_oonf_interface_tree, if_name, interf, _node);
  }

  if (interf == NULL) {
    avl_for_each_element(&_oonf_interface_tree, if_ptr, _node) {
      if (if_ptr->data.index == if_index) {
        interf = if_ptr;
      }
    }
  }

  if (interf == NULL) {
    OONF_INFO(LOG_INTERFACE, "Unknown interface update: %d", if_index);
    _trigger_ifgeneric_change_timer();
    return;
  }
  if (down) {
    interf->data.up = false;
  }

  oonf_interface_trigger(interf);
}

/**
 * Trigger the interface change handler after a short waiting period
 * to accumulate multiple change events.
 * @param interf pointer to olsr interface
 */
void
oonf_interface_trigger(struct os_interface *interf) {
  /* trigger interface reload in 100 ms */
  OONF_DEBUG(LOG_INTERFACE, "Change of interface %s was triggered", interf->data.name);

  _trigger_ifspecific_change_timer(interf);
}

/**
 * Trigger the interface change handler after a short waiting period
 * to accumulate multiple change events.
 * @param listener pointer to interface change handler
 */
void
oonf_interface_trigger_handler(struct oonf_interface_listener *listener) {
  /* trigger interface reload in 100 ms */
  OONF_DEBUG(LOG_INTERFACE, "Change of interface listener was triggered");

  listener->trigger_again = true;
  if (listener->interface) {
    listener->interface->retrigger_timeout = IF_RETRIGGER_INTERVAL;
    oonf_timer_set(&listener->interface->_change_timer, IF_RETRIGGER_INTERVAL);
  }
  else {
    _other_retrigger_timeout = IF_RETRIGGER_INTERVAL;
    oonf_timer_set(&_other_if_change_timer, IF_RETRIGGER_INTERVAL);
  }
}

/**
 * @param name interface name
 * @param buf optional pointer to helper buffer, if not NULL and the
 *   interface data wasn't cached, it will be read into the helper
 *   buffer directly.
 * @return pointer to olsr interface data, NULL if not found
 */
struct os_interface_data *
oonf_interface_get_data(const char *name, struct os_interface_data *buf) {
  struct os_interface *interf;

  interf = avl_find_element(&_oonf_interface_tree, name, interf, _node);
  if (interf == NULL) {
    if (buf) {
      if (os_interface_update(buf, name)) {
        return NULL;
      }
      return buf;
    }
    return NULL;
  }

  return &interf->data;
}

/**
 * Search for an interface by its index
 * @param ifindex index of the interface
 * @return interface data, NULL if not found
 */
struct os_interface_data *
oonf_interface_get_data_by_ifindex(unsigned ifindex) {
  struct os_interface *interf;

  avl_for_each_element(&_oonf_interface_tree, interf, _node) {
    if (interf->data.index == ifindex) {
      return &interf->data;
    }
  }
  return NULL;
}

/**
 * Search for an interface by its base-index
 * @param ifindex index of the interface
 * @return first fitting interface data, NULL if not found
 */
struct os_interface_data *
oonf_interface_get_data_by_ifbaseindex(unsigned ifindex) {
  struct os_interface *interf;

  avl_for_each_element(&_oonf_interface_tree, interf, _node) {
    if (interf->data.base_index == ifindex) {
      return &interf->data;
    }
  }
  return NULL;
}

/**
 * Get the prefix of an interface fitting to a destination address
 * @param destination destination address
 * @param ifdata interface data, NULL to search over all interfaces
 * @return network prefix (including full host), NULL if not found
 */
const struct netaddr *
oonf_interface_get_prefix_from_dst(
    struct netaddr *destination, struct os_interface_data *ifdata) {
  struct os_interface *interf;
  const struct netaddr *result;
  size_t i;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf1, nbuf2;
#endif

  if (ifdata == NULL) {
    avl_for_each_element(&_oonf_interface_tree, interf, _node) {
      result = oonf_interface_get_prefix_from_dst(destination, &interf->data);
      if (result) {
        return result;
      }
    }
    return NULL;
  }

  for (i=0; i<ifdata->prefixcount; i++) {
    if (netaddr_is_in_subnet(&ifdata->prefixes[i], destination)) {
      OONF_DEBUG(LOG_INTERFACE, "destination %s query matched if prefix: %s",
          netaddr_to_string(&nbuf1, destination),
          netaddr_to_string(&nbuf2, &ifdata->prefixes[i]));

      return &ifdata->prefixes[i];
    }
  }

  return NULL;
}

/**
 * Calculate the IP address a socket should bind to
 * @param af_type address family for result
 * @param filter filter for IP address to bind on
 * @param ifdata interface to bind to socket on, NULL if not
 *   bound to an interface.
 * @return pointer to address, NULL if no valid address was found
 */
const struct netaddr *
oonf_interface_get_bindaddress(int af_type,
    struct netaddr_acl *filter, struct os_interface_data *ifdata) {
  const struct netaddr *result;
  size_t i;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  OONF_DEBUG(LOG_INTERFACE, "Find bindto (%s) for acl (if=%s):",
      af_type == AF_INET ? "ipv4" : "ipv6",
      !ifdata ? "any" : ifdata->name);

  for (i=0; i<filter->accept_count; i++) {
    OONF_DEBUG_NH(LOG_INTERFACE, "\taccept: %s", netaddr_to_string(&nbuf, &filter->accept[i]));
  }
  for (i=0; i<filter->reject_count; i++) {
    OONF_DEBUG_NH(LOG_INTERFACE, "\treject: %s", netaddr_to_string(&nbuf, &filter->reject[i]));
  }
  OONF_DEBUG_NH(LOG_INTERFACE, "\t%s_first, %s_default",
      filter->reject_first ? "reject" : "accept",
      filter->accept_default ? "accept" : "reject");

  result = NULL;
  if (ifdata == NULL) {
    OONF_DEBUG(LOG_INTERFACE, "Look for fixed prefix");
    result = _get_fixed_prefix(af_type, filter);
  }
  if (!result) {
    OONF_DEBUG(LOG_INTERFACE, "Look for exact match");
    result = _get_exact_match_bindaddress(af_type, filter, ifdata);
  }
  if (!result) {
    OONF_DEBUG(LOG_INTERFACE, "Look for prefix match");
    result = _get_matching_bindaddress(af_type, filter, ifdata);
  }
  OONF_DEBUG(LOG_INTERFACE, "Bind to '%s'", netaddr_to_string(&nbuf, result));
  return result;
}

/**
 * Get tree of OONF managed interfaces
 * @return interface tree
 */
struct avl_tree *
oonf_interface_get_tree(void) {
  return &_oonf_interface_tree;
}

/**
 * Checks if the whole ACL is one maximum length address
 * (or two, one for each possible address type).
 * @param af_type requested address family
 * @param filter filter to parse
 * @return pointer to address to bind socket to, NULL if no match
 */
static const struct netaddr *
_get_fixed_prefix(int af_type, struct netaddr_acl *filter) {
  const struct netaddr *first, *second;
  if (filter->reject_count > 0) {
    return NULL;
  }

  if (filter->accept_count == 0 || filter->accept_count > 2) {
    return NULL;
  }

  first = &filter->accept[0];
  if (netaddr_get_prefix_length(first) != netaddr_get_maxprefix(first)) {
    return NULL;
  }

  if (filter->accept_count == 2) {
    second = &filter->accept[1];

    if (netaddr_get_address_family(first) ==
        netaddr_get_address_family(second)) {
      /* must be two different address families */
      return NULL;
    }

    if (netaddr_get_prefix_length(second) != netaddr_get_maxprefix(second)) {
      return NULL;
    }
    if (netaddr_get_address_family(second) == af_type) {
      return second;
    }
  }

  if (netaddr_get_address_family(first) == af_type) {
    return first;
  }
  return NULL;
}

/**
 * Finds an IP on an/all interfaces that matches an exact (maximum length)
 * filter rule
 *
 * @param af_type address family type to look for
 * @param filter filter that must be matched
 * @param ifdata interface to look through, NULL for all interfaces
 * @return pointer to address to bind socket to, NULL if no match
 */
static const struct netaddr *
_get_exact_match_bindaddress(int af_type, struct netaddr_acl *filter,
    struct os_interface_data *ifdata) {
  struct os_interface *interf;
  const struct netaddr *result;
  size_t i,j;

  /* handle the 'all interfaces' case */
  if (ifdata == NULL) {
    avl_for_each_element(&_oonf_interface_tree, interf, _node) {
      if ((result = _get_exact_match_bindaddress(af_type, filter, &interf->data)) != NULL) {
        return result;
      }
    }
    return NULL;
  }

  /* run through all filters */
  for (i=0; i<filter->accept_count; i++) {
    /* look for maximum prefix length filters */
    if (netaddr_get_prefix_length(&filter->accept[i]) != netaddr_get_af_maxprefix(af_type)) {
      continue;
    }

    /* run through all interface addresses and look for match */
    for (j=0; j<ifdata->addrcount; j++) {
      if (netaddr_cmp(&ifdata->addresses[j], &filter->accept[i]) == 0) {
        return &filter->accept[i];
      }
    }
  }

  /* no exact match found */
  return NULL;
}

/**
 * Finds an IP on an/all interfaces that matches a filter rule
 *
 * @param af_type address family type to look for
 * @param filter filter that must be matched
 * @param ifdata interface to look through, NULL for all interfaces
 * @return pointer to address to bind socket to, NULL if no match
 */
static const struct netaddr *
_get_matching_bindaddress(int af_type, struct netaddr_acl *filter,
    struct os_interface_data *ifdata) {
  struct os_interface *interf;
  const struct netaddr *result;
  size_t i;

  /* handle the 'all interfaces' case */
  if (ifdata == NULL) {
    avl_for_each_element(&_oonf_interface_tree, interf, _node) {
      if ((result = _get_matching_bindaddress(af_type, filter, &interf->data)) != NULL) {
        return result;
      }
    }
    return NULL;
  }

  /* run through interface address list looking for filter match */
  for (i=0; i<ifdata->addrcount; i++) {
    if (netaddr_get_address_family(&ifdata->addresses[i]) != af_type) {
      continue;
    }

    if (netaddr_acl_check_accept(filter, &ifdata->addresses[i])) {
      return &ifdata->addresses[i];
    }
  }
  return NULL;
}

/**
 * Add an interface to the listener system
 * @param if_index index of interface
 * @param mesh true if interface is used for mesh traffic
 * @return pointer to interface struct, NULL if an error happened
 */
static struct os_interface *
_interface_add(const char *name, bool mesh) {
  struct os_interface *interf;

  interf = avl_find_element(&_oonf_interface_tree, name, interf, _node);
  if (!interf) {
    /* allocate new interface */
    interf = oonf_class_malloc(&_if_class);
    if (interf == NULL) {
      return NULL;
    }

    /* hookup */
    strscpy(interf->data.name, name, sizeof(interf->data.name));
    interf->_node.key = interf->data.name;
    avl_insert(&_oonf_interface_tree, &interf->_node);

    interf->data.index = if_nametoindex(name);

    interf->_change_timer.class = &_change_timer_info;
    interf->_change_timer.cb_context = interf;

    /* initialize data of interface */
    os_interface_update(&interf->data, name);
  }

  /* update reference counters */
  interf->usage_counter++;
  if(mesh) {
    if (interf->mesh_counter == 0) {
      os_interface_init_mesh(interf);
    }
    interf->mesh_counter++;
  }

  OONF_INFO(LOG_INTERFACE, "interface '%s' has reference count %u", name, interf->usage_counter);

  /* trigger update */
  if (interf->usage_counter == 1) {
    /* new interface */
    _cb_change_handler(interf);
  }
  else {
    /* existing one, delay update */
    _trigger_ifspecific_change_timer(interf);
  }

  return interf;
}

/**
 * Remove an interface from the listener system. If multiple listeners
 * share an interface, this will only decrease the reference counter.
 * @param interf pointer to os_interface
 */
static void
_interface_remove(struct os_interface *interf, bool mesh) {
  /* handle mesh interface flag */
  if (mesh) {
    interf->mesh_counter--;

    if (interf->mesh_counter < 1) {
      /* no mesh interface anymore, remove routing settings */
      os_interface_cleanup_mesh(interf);
    }
  }

  interf->usage_counter--;
  OONF_INFO(LOG_INTERFACE, "Interface '%s' has reference count %u", interf->data.name, interf->usage_counter);

  if (interf->usage_counter > 0) {
    return;
  }

  if (interf->data.addresses) {
    free(interf->data.addresses);
  }
  avl_remove(&_oonf_interface_tree, &interf->_node);

  oonf_timer_stop(&interf->_change_timer);
  oonf_class_free(&_if_class, interf);
}

static bool
_match_ifgeneric_listener(struct oonf_interface_listener *l) {
  return l->process && (l->name == NULL || l->name[0] == 0);
}
static bool
_should_process_ifgeneric_listener(struct oonf_interface_listener *l) {
  return _match_ifgeneric_listener(l) && l->trigger_again;
}

static bool
_match_ifspecific_listener(struct oonf_interface_listener *l, const char *ifname) {
  return (l->process && l->name != NULL && strcmp(l->name, ifname) == 0)
      || _match_ifgeneric_listener(l);
}

static bool
_should_process_ifspecific_listener(struct oonf_interface_listener *l, const char *ifname) {
  return _match_ifspecific_listener(l, ifname) && l->trigger_again;
}

static bool
_trigger_listener(struct oonf_interface_listener *l, struct os_interface_data *old) {
  bool trouble = false;

  if (l->process) {
    l->old = old;

    if (l->process(l)) {
      trouble = true;
    }
    else {
      l->trigger_again = false;
    }
    l->old = false;
  }
  return trouble;
}

/**
 * Timer callback to handle potential change of data of an interface
 * @param ptr pointer to interface object
 */
static void
_cb_change_handler(void *ptr) {
  struct os_interface_data old_data, new_data;
  struct oonf_interface_listener *listener, *l_it;
  struct os_interface *interf;
  bool trouble = false;

  interf = ptr;

  OONF_DEBUG(LOG_INTERFACE, "Change of interface %s in progress",
      interf == NULL ? "any" : interf->data.name);

  if (!interf) {
    /* call generic listeners */
    list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {

      if (_should_process_ifgeneric_listener(listener)) {
        if (_trigger_listener(listener, NULL)) {
          trouble = true;
        }
      }
    }

    if (trouble) {
      /* trigger callback again after a timeout */
      _other_retrigger_timeout *= 2ull;
      oonf_timer_set(&_other_if_change_timer, _other_retrigger_timeout);
    }
    else {
      _other_retrigger_timeout = IF_RETRIGGER_INTERVAL;
    }
    return;
  }

  /* read interface data */
  memset(&new_data, 0, sizeof(new_data));
  if (os_interface_update(&new_data, interf->data.name)) {
    /* an error happened, try again */
    OONF_INFO(LOG_INTERFACE, "Could not query os network interface %s, trying again soon",
        interf->data.name);
    _trigger_ifspecific_change_timer(interf);
    return;
  }

  /* copy data to interface object, but remember the old data */
  memcpy(&old_data, &interf->data, sizeof(old_data));
  memcpy(&interf->data, &new_data, sizeof(interf->data));

  /* call listeners */
  list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {
    if (_should_process_ifspecific_listener(listener, interf->data.name)) {
      if (_trigger_listener(listener, &old_data)) {
        trouble = true;
      }
    }
  }

  if (trouble) {
    /* trigger callback again after a timeout */
    interf->retrigger_timeout *= 2ull;
    oonf_timer_set(&interf->_change_timer, interf->retrigger_timeout);
  }

  if (old_data.addresses) {
    free (old_data.addresses);
  }
}

/**
 * Activate the change timer of an unknown interface
 */
static void
_trigger_ifgeneric_change_timer(void) {
  struct oonf_interface_listener *listener;

  list_for_each_element(&_interface_listener, listener, _node) {
    if (_match_ifgeneric_listener(listener)) {
      listener->trigger_again = true;
    }
  }
  _other_retrigger_timeout = IF_RETRIGGER_INTERVAL;
  oonf_timer_set(&_other_if_change_timer, OONF_INTERFACE_CHANGE_INTERVAL);
}

/**
 * Activate the change timer of an interface object
 * @param interf pointer to interface object
 */
static void
_trigger_ifspecific_change_timer(struct os_interface *interf) {
  struct oonf_interface_listener *listener;

  list_for_each_element(&_interface_listener, listener, _node) {
    if (_match_ifspecific_listener(listener, interf->data.name)) {
      listener->trigger_again = true;
    }
  }
  interf->retrigger_timeout = IF_RETRIGGER_INTERVAL;
  oonf_timer_set(&interf->_change_timer, OONF_INTERFACE_CHANGE_INTERVAL);
}

static int
_handle_unused_parameter(const char *arg) {
  cfg_db_add_namedsection(oonf_cfg_get_rawdb(), CFG_INTERFACE_SECTION, arg);
  return 0;
}
