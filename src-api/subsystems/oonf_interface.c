
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

#include <netinet/in.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_net.h"
#include "subsystems/os_system.h"

/* timeinterval to delay change in interface to trigger actions */
#define OONF_INTERFACE_CHANGE_INTERVAL 100

/* prototypes */
static int _init(void);
static void _cleanup(void);

static const struct netaddr *_get_fixed_prefix(
    int af_type, struct netaddr_acl *filter);
static const struct netaddr *_get_exact_match_bindaddress(
    int af_type, struct netaddr_acl *filter, struct oonf_interface_data *ifdata);
static const struct netaddr *_get_matching_bindaddress(
    int af_type, struct netaddr_acl *filter, struct oonf_interface_data *ifdata);

static struct oonf_interface *_interface_add(const char *, bool mesh);
static void _interface_remove(struct oonf_interface *interf, bool mesh);
static void _cb_change_handler(void *);
static void _trigger_change_timer(struct oonf_interface *);

/* global tree of known interfaces */
struct avl_tree oonf_interface_tree;

/* subsystem definition */
struct oonf_subsystem oonf_interface_subsystem = {
  .name = "interface",
  .init = _init,
  .cleanup = _cleanup,
};

static struct list_entity _interface_listener;
static struct oonf_timer_class _change_timer_info = {
  .name = "Interface change",
  .callback = _cb_change_handler,
};

static struct os_system_if_listener _iflistener = {
  .if_changed = oonf_interface_trigger_change,
};

static struct oonf_class _if_class = {
  .name = OONF_CLASS_INTERFACE,
  .size = sizeof(struct oonf_interface),
};

/**
 * Initialize interface subsystem
 * @return always returns 0
 */
static int
_init(void) {
  oonf_timer_add(&_change_timer_info);
  oonf_class_add(&_if_class);

  avl_init(&oonf_interface_tree, avl_comp_strcasecmp, false);
  list_init_head(&_interface_listener);

  os_system_iflistener_add(&_iflistener);
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

  os_system_iflistener_remove(&_iflistener);
  oonf_class_remove(&_if_class);
  oonf_timer_remove(&_change_timer_info);
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

  if (listener->name) {
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

  if (listener->interface) {
    _interface_remove(listener->interface, listener->mesh);
  }

  list_remove(&listener->_node);
}

/**
 * Trigger a potential change in the interface settings. Normally
 * called by os_system code
 * @param name interface name
 * @param down true if interface is going down
 */
void
oonf_interface_trigger_change(unsigned if_index, bool down) {
  char if_name[IF_NAMESIZE];
  struct oonf_interface *interf = NULL, *if_ptr;

  if (if_indextoname(if_index, if_name) != NULL) {
    interf = avl_find_element(&oonf_interface_tree, if_name, interf, _node);
  }

  if (interf == NULL) {
    avl_for_each_element(&oonf_interface_tree, if_ptr, _node) {
      if (if_ptr->data.index == if_index) {
        interf = if_ptr;
      }
    }
  }

  if (interf == NULL) {
    OONF_INFO(LOG_INTERFACE, "Unknown interface update: %d", if_index);
    return;
  }
  if (down) {
    interf->data.up = false;
  }

  oonf_interface_trigger_handler(interf);
}

/**
 * Trigger the interface change handler after a short waiting period
 * to accumulate multiple change events.
 * @param interf pointer to olsr interface
 */
void
oonf_interface_trigger_handler(struct oonf_interface *interf) {
  /* trigger interface reload in 100 ms */
  OONF_DEBUG(LOG_INTERFACE, "Change of interface %s was triggered", interf->data.name);

  _trigger_change_timer(interf);
}

/**
 * @param name interface name
 * @param buf optional pointer to helper buffer, if not NULL and the
 *   interface data wasn't cached, it will be read into the helper
 *   buffer directly.
 * @return pointer to olsr interface data, NULL if not found
 */
struct oonf_interface_data *
oonf_interface_get_data(const char *name, struct oonf_interface_data *buf) {
  struct oonf_interface *interf;

  interf = avl_find_element(&oonf_interface_tree, name, interf, _node);
  if (interf == NULL) {
    if (buf) {
      if (os_net_update_interface(buf, name)) {
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
struct oonf_interface_data *
oonf_interface_get_data_by_ifindex(unsigned ifindex) {
  struct oonf_interface *interf;

  avl_for_each_element(&oonf_interface_tree, interf, _node) {
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
struct oonf_interface_data *
oonf_interface_get_data_by_ifbaseindex(unsigned ifindex) {
  struct oonf_interface *interf;

  avl_for_each_element(&oonf_interface_tree, interf, _node) {
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
    struct netaddr *destination, struct oonf_interface_data *ifdata) {
  struct oonf_interface *interf;
  const struct netaddr *result;
  size_t i;

  if (ifdata == NULL) {
    avl_for_each_element(&oonf_interface_tree, interf, _node) {
      result = oonf_interface_get_prefix_from_dst(destination, &interf->data);
      if (result) {
        return result;
      }
    }
    return NULL;
  }

  for (i=0; i<ifdata->prefixcount; i++) {
    if (netaddr_is_in_subnet(&ifdata->prefixes[i], destination)) {
      return &ifdata->prefixes[i];
    }
  }

  return NULL;
}

/**
 * Calculate the IP address a socket should bind to
 * @param filter filter for IP address to bind on
 * @param ifdata interface to bind to socket on, NULL if not
 *   bound to an interface.
 * @return 0 if an IP was calculated, -1 otherwise
 */
const struct netaddr *
oonf_interface_get_bindaddress(int af_type,
    struct netaddr_acl *filter, struct oonf_interface_data *ifdata) {
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
  OONF_DEBUG_NH(LOG_INTERFACE, "Bind to '%s'", netaddr_to_string(&nbuf, result));
  return result;
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
    struct oonf_interface_data *ifdata) {
  struct oonf_interface *interf;
  const struct netaddr *result;
  size_t i,j;

  /* handle the 'all interfaces' case */
  if (ifdata == NULL) {
    avl_for_each_element(&oonf_interface_tree, interf, _node) {
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
    struct oonf_interface_data *ifdata) {
  struct oonf_interface *interf;
  const struct netaddr *result;
  size_t i;

  /* handle the 'all interfaces' case */
  if (ifdata == NULL) {
    avl_for_each_element(&oonf_interface_tree, interf, _node) {
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
static struct oonf_interface *
_interface_add(const char *name, bool mesh) {
  struct oonf_interface *interf;

  interf = avl_find_element(&oonf_interface_tree, name, interf, _node);
  if (!interf) {
    /* allocate new interface */
    interf = oonf_class_malloc(&_if_class);
    if (interf == NULL) {
      return NULL;
    }

    /* hookup */
    strscpy(interf->data.name, name, sizeof(interf->data.name));
    interf->_node.key = interf->data.name;
    avl_insert(&oonf_interface_tree, &interf->_node);

    interf->data.index = if_nametoindex(name);

    interf->_change_timer.info = &_change_timer_info;
    interf->_change_timer.cb_context = interf;

    /* initialize data of interface */
    os_net_update_interface(&interf->data, name);
  }

  /* update reference counters */
  interf->usage_counter++;
  if(mesh) {
    if (interf->mesh_counter == 0) {
      os_net_init_mesh_if(interf);
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
    _trigger_change_timer(interf);
  }

  return interf;
}

/**
 * Remove an interface from the listener system. If multiple listeners
 * share an interface, this will only decrease the reference counter.
 * @param interf pointer to oonf_interface
 */
static void
_interface_remove(struct oonf_interface *interf, bool mesh) {
  /* handle mesh interface flag */
  if (mesh) {
    interf->mesh_counter--;

    if (interf->mesh_counter < 1) {
      /* no mesh interface anymore, remove routing settings */
      os_net_cleanup_mesh_if(interf);
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
  avl_remove(&oonf_interface_tree, &interf->_node);

  oonf_timer_stop(&interf->_change_timer);
  oonf_class_free(&_if_class, interf);
}

/**
 * Timer callback to handle potential change of data of an interface
 * @param ptr pointer to interface object
 */
static void
_cb_change_handler(void *ptr) {
  struct oonf_interface_data old_data, new_data;
  struct oonf_interface_listener *listener, *l_it;
  struct oonf_interface *interf;

  interf = ptr;

  OONF_DEBUG(LOG_INTERFACE, "Change of interface %s in progress", interf->data.name);

  /* read interface data */
  memset(&new_data, 0, sizeof(new_data));
  if (os_net_update_interface(&new_data, interf->data.name)) {
    /* an error happened, try again */
    OONF_INFO(LOG_INTERFACE, "Could not query os network interface %s, trying again soon",
        interf->data.name);
    _trigger_change_timer(interf);
    return;
  }

  /* something changed ?
  if (memcmp(&interf->data, &new_data, sizeof(new_data)) == 0) {
    return;
  }
*/
  /* copy data to interface object, but remember the old data */
  memcpy(&old_data, &interf->data, sizeof(old_data));
  memcpy(&interf->data, &new_data, sizeof(interf->data));

  /* call listeners */
  list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {
    if (listener->process != NULL
        && (listener->name == NULL
            || strcasecmp(listener->name, interf->data.name) == 0)) {
      listener->old = &old_data;
      listener->process(listener);
      listener->old = NULL;
    }
  }

  if (old_data.addresses) {
    free (old_data.addresses);
  }
}

/**
 * Activate the change timer of an interface object
 * @param interf pointer to interface object
 */
static void
_trigger_change_timer(struct oonf_interface *interf) {
  oonf_timer_set(&interf->_change_timer, OONF_INTERFACE_CHANGE_INTERVAL);
}
