
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

#include <errno.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "config/cfg_schema.h"
#include "core/oonf_cfg.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "core/os_core.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_interface.h"

#include "nhdp/nhdp.h"
#include "nhdp/nhdp_domain.h"
#include "nhdp/nhdp_interfaces.h"

#include "auto_ll4/auto_ll4.h"

/* constants and definitions */
#define LOG_AUTO_LL4 _olsrv2_auto_ll4_subsystem.logging

/**
 * Configuration for auto-ll4 plugin
 */
struct _config {
  /*! delay until plugin starts generating interface addresses */
  uint64_t startup_delay;
};

/**
 * NHDP interface class extension of autoll4 plugin
 */
struct _nhdp_if_autoll4 {
  /*! timer until next update of autogenerated ip */
  struct oonf_timer_instance update_timer;

  /*! back pointer to NHDP interface */
  struct nhdp_interface *nhdp_if;

  /*! true if interface LL4 was generated by this plugin */
  bool active;

  /*! true if current LL4 was generated by the plugin */
  bool plugin_generated;

  /*! data structure for setting and resetting auto-configured address */
  struct os_interface_ip_change os_addr;

  /*! currently configured address */
  struct netaddr auto_ll4_addr;
};

/* prototypes */
static int _init(void);
static void _initiate_shutdown(void);
static void _cleanup(void);

static void _cb_add_nhdp_interface(void *);
static void _cb_remove_nhdp_interface(void *);
static void _cb_address_finished(struct os_interface_ip_change *, int);
static void _cb_update_timer(struct oonf_timer_instance *);
static int _get_current_if_ipv4_addresscount(
    struct os_interface *os_if,
    struct netaddr *ll4_addr, struct netaddr *current_ll4);
static void _generate_default_address(
    struct _nhdp_if_autoll4 *auto_ll4, const struct netaddr *ipv6_ll);

static void _commit_address(struct _nhdp_if_autoll4 *auto_ll4
    , struct netaddr *addr, bool set);
uint16_t _calculate_host_part(const struct netaddr *addr);
static bool _is_address_collision(
    struct netaddr *auto_ll4, struct netaddr *addr);
static bool _nhdp_if_has_collision(
    struct nhdp_interface *nhdp_if, struct netaddr *addr);
static void _cb_ifaddr_change(void *);
static void _cb_laddr_change(void *);
static void _cb_2hop_change(void *);
static void _cb_ll4_cfg_changed(void);
static void _cb_if_cfg_changed(void);

/* plugin declaration */
static struct cfg_schema_entry _interface_entries[] = {
  CFG_MAP_BOOL(_nhdp_if_autoll4, active, "auto_ll4", "true",
      "Controls autogeneration of IPv4 linklocal IPs on interface."),
};

static struct cfg_schema_section _interface_section = {
  .type = CFG_INTERFACE_SECTION,
  .mode = CFG_INTERFACE_SECTION_MODE,
  .cb_delta_handler = _cb_if_cfg_changed,
  .entries = _interface_entries,
  .entry_count = ARRAYSIZE(_interface_entries),
};

static struct cfg_schema_entry _auto_ll4_entries[] = {
  CFG_MAP_CLOCK(_config, startup_delay, "startup", "10",
      "Startup time until first auto-configured IPv4 linklocal should be selected."),
};

static struct cfg_schema_section _auto_ll4_section = {
  .type = OONF_AUTO_LL4_SUBSYSTEM,
  .mode = CFG_SSMODE_UNNAMED,
  .cb_delta_handler = _cb_ll4_cfg_changed,
  .entries = _auto_ll4_entries,
  .entry_count = ARRAYSIZE(_auto_ll4_entries),
  .next_section = &_interface_section,
};

static const char *_dependencies[] = {
  OONF_CLASS_SUBSYSTEM,
  OONF_TIMER_SUBSYSTEM,
  OONF_OS_INTERFACE_SUBSYSTEM,
  OONF_NHDP_SUBSYSTEM,
};
static struct oonf_subsystem _olsrv2_auto_ll4_subsystem = {
  .name = OONF_AUTO_LL4_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "OLSRv2 Automatic IPv4 Linklayer IP generation plugin",
  .author = "Henning Rogge",

  .cfg_section = &_auto_ll4_section,

  .init = _init,
  .cleanup = _cleanup,
  .initiate_shutdown = _initiate_shutdown,
};
DECLARE_OONF_PLUGIN(_olsrv2_auto_ll4_subsystem);

/* timer for handling new NHDP neighbors */
static struct oonf_timer_class _startup_timer_info = {
  .name = "Initial delay until first IPv4 linklocal IPs are generated",
  .callback = _cb_update_timer,
  .periodic = false,
};

/* NHDP interface extension/listener */
static struct oonf_class_extension _nhdp_if_extenstion = {
  .ext_name = "auto ll4 generation",
  .class_name = NHDP_CLASS_INTERFACE,
  .size = sizeof(struct _nhdp_if_autoll4),

  .cb_add = _cb_add_nhdp_interface,
  .cb_remove = _cb_remove_nhdp_interface,
};

/* NHDP interface address listener */
static struct oonf_class_extension _nhdp_ifaddr_listener = {
  .ext_name = "auto ll4 generation",
  .class_name = NHDP_CLASS_INTERFACE_ADDRESS,

  .cb_add = _cb_ifaddr_change,
  .cb_remove = _cb_ifaddr_change,
};

/* NHDP link address listener */
static struct oonf_class_extension _nhdp_laddr_listener = {
  .ext_name = "auto ll4 laddr listener",
  .class_name = NHDP_CLASS_LINK_ADDRESS,

  .cb_add = _cb_laddr_change,
  .cb_remove = _cb_laddr_change,
};

/* NHDP twohop listener */
static struct oonf_class_extension _nhdp_2hop_listener = {
  .ext_name = "auto ll4 twohop listener",
  .class_name = NHDP_CLASS_LINK_2HOP,

  .cb_add = _cb_2hop_change,
  .cb_remove = _cb_2hop_change,
};

/* global variables */
static uint64_t _ll4_startup_delay = 10*1000;

/**
 * Initialize plugin
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  if (oonf_class_extension_add(&_nhdp_if_extenstion)) {
    OONF_WARN(LOG_AUTO_LL4, "Cannot allocate extension for NHDP interface data");
    return -1;
  }

  oonf_class_extension_add(&_nhdp_ifaddr_listener);
  oonf_class_extension_add(&_nhdp_laddr_listener);
  oonf_class_extension_add(&_nhdp_2hop_listener);
  oonf_timer_add(&_startup_timer_info);
  return 0;
}

/**
 * Initiate cleanup of plugin.
 */
static void
_initiate_shutdown(void) {
  struct nhdp_interface *nhdp_if;

  avl_for_each_element(nhdp_interface_get_tree(), nhdp_if, _node) {
    OONF_DEBUG(LOG_AUTO_LL4, "initiate cleanup if: %s",
        nhdp_interface_get_if_listener(nhdp_if)->data->name);
    _cb_remove_nhdp_interface(nhdp_if);
  }
  oonf_class_extension_remove(&_nhdp_if_extenstion);
  oonf_class_extension_remove(&_nhdp_2hop_listener);
  oonf_class_extension_remove(&_nhdp_laddr_listener);
  oonf_class_extension_remove(&_nhdp_ifaddr_listener);
}

/**
 * Cleanup plugin
 */
static void
_cleanup(void) {
  oonf_timer_remove(&_startup_timer_info);
}

/**
 * Callback triggered when a new NHDP interface is added to the database
 * @param ptr pointer to NHDP interface
 */
static void
_cb_add_nhdp_interface(void *ptr) {
  struct nhdp_interface *nhdp_if = ptr;
  struct _nhdp_if_autoll4 *auto_ll4;

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);
  auto_ll4->nhdp_if = nhdp_if;

  /* initialize static part of routing data */
  auto_ll4->os_addr.cb_finished = _cb_address_finished;
  auto_ll4->os_addr.if_index = nhdp_interface_get_if_listener(nhdp_if)->data->index;
  auto_ll4->os_addr.scope = OS_ADDR_SCOPE_LINK;


  /* activate update_timer delay timer */
  auto_ll4->update_timer.class = &_startup_timer_info;
  oonf_timer_set(&auto_ll4->update_timer, _ll4_startup_delay);
}

/**
 * Callback triggered when a NHDP interface is removed from the database
 * @param ptr pointer to NHDP interface
 */
static void
_cb_remove_nhdp_interface(void *ptr) {
  struct nhdp_interface *nhdp_if = ptr;
  struct _nhdp_if_autoll4 *auto_ll4;

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

  /* stop running address setting feedback */
  auto_ll4->os_addr.cb_finished = NULL;
  os_interface_address_interrupt(&auto_ll4->os_addr);

  /* cleanup address if necessary */
  auto_ll4->active = false;
  _cb_update_timer(&auto_ll4->update_timer);

  /* stop update timer */
  oonf_timer_stop(&auto_ll4->update_timer);
}

/**
 * Callback triggered when the kernel acknowledged that an address has
 * been set on an interface (or not, because an error happened)
 * @param os_addr pointer to address parameters
 * @param error 0 if address was set, otherwise an error happened
 */
static void
_cb_address_finished(struct os_interface_ip_change *os_addr, int error) {
  struct _nhdp_if_autoll4 *auto_ll4;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
  char ibuf[IF_NAMESIZE];
#endif

  /* get auto linklayer extension */
  auto_ll4 = container_of(os_addr, typeof(*auto_ll4), os_addr);

  OONF_DEBUG(LOG_AUTO_LL4, "Got feedback from netlink for %s address %s on if %s: %s (%d)",
      os_addr->set ? "setting" : "resetting",
          netaddr_to_string(&nbuf, &os_addr->address),
          if_indextoname(os_addr->if_index, ibuf),
          strerror(error), error);

  if (error) {
    if ((os_addr->set && error != EEXIST)
        || (!os_addr->set && error != EADDRNOTAVAIL)) {
      /* try again */
      oonf_timer_set(&auto_ll4->update_timer, 1000);
      return;
    }
  }
}

/**
 * Callback triggered when an address changes on a NHDP interface
 * @param ptr pointer to NHDP interface
 */
static void
_cb_ifaddr_change(void *ptr) {
  struct nhdp_interface_addr *ifaddr = ptr;
  struct nhdp_interface *nhdp_if;
  struct _nhdp_if_autoll4 *auto_ll4;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  nhdp_if = ifaddr->interf;

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

  if (!oonf_timer_is_active(&auto_ll4->update_timer)) {
    /* request delayed address check */
    oonf_timer_set(&auto_ll4->update_timer, 1);

    OONF_DEBUG(LOG_AUTO_LL4, "Interface address changed: %s",
        netaddr_to_string(&nbuf, &ifaddr->if_addr));
  }
}

/**
 * Callback triggered when a link address of a NHDP neighbor changes
 * @param ptr pointer to NHDP link address
 */
static void
_cb_laddr_change(void *ptr) {
  struct nhdp_laddr *laddr = ptr;
  struct nhdp_interface *nhdp_if;
  struct _nhdp_if_autoll4 *auto_ll4;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  nhdp_if = laddr->link->local_if;

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

  if (!oonf_timer_is_active(&auto_ll4->update_timer)) {
    /* request delayed address check */
    oonf_timer_set(&auto_ll4->update_timer, 1);

    OONF_DEBUG(LOG_AUTO_LL4, "Link address changed: %s",
        netaddr_to_string(&nbuf, &laddr->link_addr));
  }
}

/**
 * Callback triggered when a two-hop address of a NHDP neighbor changes
 * @param ptr pointer to NHDP two-hop address
 */
static void
_cb_2hop_change(void *ptr) {
  struct nhdp_l2hop *l2hop = ptr;
  struct nhdp_interface *nhdp_if;
  struct _nhdp_if_autoll4 *auto_ll4;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  nhdp_if = l2hop->link->local_if;

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

  if (!oonf_timer_is_active(&auto_ll4->update_timer)) {
    /* request delayed address check */
    oonf_timer_set(&auto_ll4->update_timer, 1);

    OONF_DEBUG(LOG_AUTO_LL4, "2Hop address changed: %s",
        netaddr_to_string(&nbuf, &l2hop->twohop_addr));
  }
}

/**
 * Callback triggered when the plugin should check if the autoconfigured
 * address is still okay.
 * @param ptr timer instance that fired
 */
static void
_cb_update_timer(struct oonf_timer_instance *ptr) {
  struct nhdp_interface *nhdp_if;
  struct os_interface *os_if;
  struct _nhdp_if_autoll4 *auto_ll4;
  struct netaddr current_ll4;
  int count;
  uint32_t rnd;
  uint16_t hash;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  auto_ll4 = container_of(ptr, struct _nhdp_if_autoll4, update_timer);
  nhdp_if = auto_ll4->nhdp_if;

  /* get pointer to interface data */
  os_if = nhdp_interface_get_if_listener(nhdp_if)->data;

  /* ignore loopback */
  if (os_if->loopback || !os_if->up) {
    OONF_DEBUG(LOG_AUTO_LL4, "Ignore interface %s: its loopback or down",
        os_if->name);
    return;
  }

  /* query current interface status */
  count = _get_current_if_ipv4_addresscount(os_if, &current_ll4, &auto_ll4->auto_ll4_addr);

  if (!oonf_rfc5444_is_interface_active(nhdp_if->rfc5444_if.interface, AF_INET6)) {
    if (auto_ll4->plugin_generated) {
      /* remove our configured address, this interface does not support dualstack */
      _commit_address(auto_ll4, &current_ll4, false);
      OONF_DEBUG(LOG_AUTO_LL4,
          "Remove LL4 address, interface is not using NHDP on IPv6");
    }
    OONF_DEBUG(LOG_AUTO_LL4,
        "Done (interface %s is not using NHDP on IPv6)", os_if->name);
    return;
  }

  if (!auto_ll4->active) {
    if (auto_ll4->plugin_generated && count == 1
        && netaddr_cmp(&current_ll4, &auto_ll4->auto_ll4_addr) == 0) {
      /* remove our configured address, the user set a different one */
      _commit_address(auto_ll4, &current_ll4, false);
      OONF_DEBUG(LOG_AUTO_LL4, "Remove LL4, user has selected his own address");
    }
    OONF_DEBUG(LOG_AUTO_LL4, "Done (interface %s is not active)", os_if->name);
    return;
  }

  if (count > 1) {
    if (auto_ll4->plugin_generated &&
        netaddr_cmp(&current_ll4, &auto_ll4->auto_ll4_addr) == 0) {
      /* remove our configured address, the user set a different one */
      _commit_address(auto_ll4, &current_ll4, false);
      OONF_DEBUG(LOG_AUTO_LL4, "Remove LL4, user has selected his own address");
    }
    OONF_DEBUG(LOG_AUTO_LL4, "Done (interface %s has additional addresses)", os_if->name);
    return;
  }

  if (count == 1) {
    if (netaddr_get_address_family(&current_ll4) == AF_UNSPEC) {
      /* do nothing, user set a non-LL interface IP */
      OONF_DEBUG(LOG_AUTO_LL4, "Done (interface %s has non-ll ipv4)", os_if->name);
      return;
    }

    /* copy the current IP to the setting variable */
    memcpy(&auto_ll4->auto_ll4_addr, &current_ll4, sizeof(current_ll4));
  }

  if (netaddr_get_address_family(&auto_ll4->auto_ll4_addr) == AF_UNSPEC) {
    /* try our default IP first */
    _generate_default_address(auto_ll4, os_if->if_linklocal_v6);
  }

  while (_nhdp_if_has_collision(nhdp_if, &auto_ll4->auto_ll4_addr)) {
    /* roll up a random address */
    if (os_core_get_random(&rnd, sizeof(rnd))) {
      OONF_WARN(LOG_AUTO_LL4, "Could not get random data");
      return;
    }
    hash = htons((rnd % (256 * 254)) + 256);
    netaddr_create_host_bin(&auto_ll4->auto_ll4_addr,
        &NETADDR_IPV4_LINKLOCAL, &hash, sizeof(hash));
  }

  if (netaddr_cmp(&auto_ll4->auto_ll4_addr, &current_ll4) == 0) {
    /* nothing to do */
    OONF_DEBUG(LOG_AUTO_LL4, "Done (interface %s already has ll %s)",
        os_if->name, netaddr_to_string(&nbuf, &current_ll4));
    return;
  }

  if (netaddr_get_address_family(&current_ll4) != AF_UNSPEC) {
    /* remove current ipv4 linklocal address */
    OONF_DEBUG(LOG_AUTO_LL4, "Remove old LL4 %s",
        netaddr_to_string(&nbuf, &current_ll4));
    _commit_address(auto_ll4, &current_ll4, false);
  }
  else {
    /* set new ipv4 linklocal address */
    OONF_DEBUG(LOG_AUTO_LL4, "Set new LL4 %s",
        netaddr_to_string(&nbuf, &auto_ll4->auto_ll4_addr));
    _commit_address(auto_ll4, &auto_ll4->auto_ll4_addr, true);
  }
}

/**
 * Generate a new auto-configured address on an interface
 * @param nhdp_if pointer to NHDP interface
 */
static void
_generate_default_address(struct _nhdp_if_autoll4 *auto_ll4, const struct netaddr *ipv6_ll) {
  uint16_t host_part;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf1, nbuf2;
#endif

  if (netaddr_get_address_family(ipv6_ll) == AF_UNSPEC) {
    /* no ipv6 linklocal address */
    netaddr_invalidate(&auto_ll4->auto_ll4_addr);
  }

  host_part = _calculate_host_part(ipv6_ll);

  /*
   * generate the address between
   * 169.254.1.0 and 169.254.254.255
   */
  netaddr_create_host_bin(&auto_ll4->auto_ll4_addr, &NETADDR_IPV4_LINKLOCAL,
      &host_part, sizeof(host_part));

  OONF_DEBUG(LOG_AUTO_LL4, "IPv6 ll %s => IPv4 ll %s",
      netaddr_to_string(&nbuf1, ipv6_ll),
      netaddr_to_string(&nbuf2, &auto_ll4->auto_ll4_addr));
}

/**
 * Get the current number of IPv4 addresses of an interface and
 * copy an IPv4 link-local address if set.
 * @param ifdata pointer to interface data
 * @param ll4_addr return buffer for link-local address
 * @param current_ll4 current link-local address, this will be returned
 *   if set, even if multiple ipv4 link-local addresses are present
 * @return number of IPv4 addresses on interface
 */
static int
_get_current_if_ipv4_addresscount(struct os_interface *os_if,
    struct netaddr *ll4_addr, struct netaddr *current_ll4) {
  struct os_interface_ip *ip;
  bool match;
  int count;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  /* reset counter */
  count = 0;
  netaddr_invalidate(ll4_addr);
  match = false;

  avl_for_each_element(&os_if->addresses, ip, _node) {
    OONF_DEBUG(LOG_AUTO_LL4, "Interface %s has address %s",
        os_if->name, netaddr_to_string(&nbuf, &ip->address));

    if (netaddr_get_address_family(&ip->address) == AF_INET) {
      /* count IPv4 addresses */
      count++;

      /* copy one IPv4 link-local address, if possible the current one */
      if (!match
          && netaddr_is_in_subnet(&NETADDR_IPV4_LINKLOCAL, &ip->address)) {
        memcpy(ll4_addr, &ip->address, sizeof(*ll4_addr));

        if (netaddr_cmp(&ip->address, current_ll4) == 0) {
          match = true;
        }
      }
    }
  }

  return count;
}

/**
 * Set/reset an address on a NHDP interface
 * @param auto_ll4 pointer to data structure for auto-configured
 * addresses on a NHDP interface
 */
static void
_commit_address(struct _nhdp_if_autoll4 *auto_ll4, struct netaddr *addr, bool set) {
#ifdef OONF_LOG_INFO
  struct netaddr_str nbuf;
  char ibuf[IF_NAMESIZE];
#endif

  memcpy(&auto_ll4->os_addr.address, addr, sizeof(*addr));
  auto_ll4->os_addr.set = set;

  OONF_INFO(LOG_AUTO_LL4, "%s address %s on interface %s",
      auto_ll4->os_addr.set ? "Set" : "Remove",
      netaddr_to_string(&nbuf, &auto_ll4->os_addr.address),
      if_indextoname(auto_ll4->os_addr.if_index, ibuf));

  /* remember if the plugin set/reset the address */
  auto_ll4->plugin_generated = set;

  /* set prefix length */
  netaddr_set_prefix_length(&auto_ll4->os_addr.address, 16);

  /* call operation system */
  os_interface_address_set(&auto_ll4->os_addr);
}

/**
 * Checks if an address would collide with any neighbor on a
 * NHDP interface, both one- and two-hop.
 * @param nhdp_if pointer to NHDP interface
 * @param addr pointer to address that might collide
 * @return true if address or hash collides with known neighbor,
 *   false otherwise
 */
static bool
_nhdp_if_has_collision(struct nhdp_interface *nhdp_if, struct netaddr *addr) {
  struct nhdp_link *lnk;
  struct nhdp_laddr *laddr;
  struct nhdp_l2hop *l2hop;

  list_for_each_element(&nhdp_if->_links, lnk, _if_node) {
    /* check for collision with one-hop neighbor */
    avl_for_each_element(&lnk->_addresses, laddr, _link_node) {
      if (_is_address_collision(addr, &laddr->link_addr)) {
        return true;
      }
    }

    avl_for_each_element(&lnk->_2hop, l2hop, _link_node) {
      if (_is_address_collision(addr, &l2hop->twohop_addr)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Check if an auto-configured address has a collision
 * @param auto_ll4 pointer to data structure for auto-configured
 * @param addr address that could collide with auto-configured IP
 * @return true if address collides, false otherwise
 */
static bool
_is_address_collision(struct netaddr *auto_ll4, struct netaddr *addr) {
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf1, nbuf2;
#endif
  uint16_t hostpart;
  const char *ptr;

  OONF_DEBUG(LOG_AUTO_LL4, "Check %s for collision with address %s",
      netaddr_to_string(&nbuf1, auto_ll4), netaddr_to_string(&nbuf2, addr));

  if (netaddr_get_address_family(addr) == AF_INET) {
    if (netaddr_cmp(auto_ll4, addr) == 0) {
      OONF_DEBUG(LOG_AUTO_LL4, "Collision with address");
      return true;
    }
  }
  else {
    hostpart = _calculate_host_part(addr);
    ptr = netaddr_get_binptr(auto_ll4);
    if (memcmp(ptr+2, &hostpart, 2) == 0) {
      OONF_DEBUG(LOG_AUTO_LL4, "Collision with hashed IPv6!");
      return true;
    }
  }

  return false;
}

/**
 * Calculates host part of an auto-configured address
 * based on a hash value
 * @param addr address that should be hashed
 * @return number between 256 (.1.0) and 65279 (.254.255)
 */
uint16_t
_calculate_host_part(const struct netaddr *addr)
{
  uint32_t hash, i;
  const char *key;
  size_t len;

  key = netaddr_get_binptr(addr);
  len = netaddr_get_binlength(addr);

  /*
   * step 1: calculate Jenkins hash
   *
   * This is no cryptographic secure has, it doesn't need
   * to be. Its just to make sure all 6 byte of the MAC
   * address are the source of the two-byte host part
   * of the auto-configuredIP.
   */
  for(hash = i = 0; i < len; ++i)
  {
    hash += key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  /* step 2: calculate host part of linklocal address */
  return htons((hash % (254 * 256)) + 256);
}

/**
 * Callback triggered when plugin configuration changes
 */
static void
_cb_ll4_cfg_changed(void) {
  struct nhdp_interface *nhdp_if;
  struct _nhdp_if_autoll4 *auto_ll4;
  struct _config cfg;

  memset(&cfg, 0, sizeof(cfg));
  if (cfg_schema_tobin(&cfg, _auto_ll4_section.post,
      _auto_ll4_entries, ARRAYSIZE(_auto_ll4_entries))) {
    OONF_WARN(LOG_AUTO_LL4, "Cannot convert " OONF_AUTO_LL4_SUBSYSTEM " configuration.");
    return;
  }

  if (cfg.startup_delay == _ll4_startup_delay) {
    return;
  }

  avl_for_each_element(nhdp_interface_get_tree(), nhdp_if, _node) {
    /* get auto linklayer extension */
    auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

    /* fix update_timer timer if still running */
    if (!oonf_timer_is_active(&auto_ll4->update_timer)) {
      oonf_timer_set(&auto_ll4->update_timer, _ll4_startup_delay);
    }
  }
}

/**
 * Plugin triggered when interface auto-configuration parameter changes
 */
static void
_cb_if_cfg_changed(void) {
  struct _nhdp_if_autoll4 *auto_ll4;
  struct nhdp_interface *nhdp_if;

  /* get interface */
  nhdp_if = nhdp_interface_get(_interface_section.section_name);

  if (_interface_section.post == NULL) {
    /* section was removed */
    if (nhdp_if != NULL) {
      /* decrease nhdp_interface refcount */
      nhdp_interface_remove(nhdp_if);
    }
    return;
  }

  if (nhdp_if == NULL) {
    /* increase nhdp_interface refcount */
    nhdp_if = nhdp_interface_add(_interface_section.section_name);
  }

  /* get auto linklayer extension */
  auto_ll4 = oonf_class_get_extension(&_nhdp_if_extenstion, nhdp_if);

  /* get configuration */
  if (cfg_schema_tobin(auto_ll4, _interface_section.post,
      _interface_entries, ARRAYSIZE(_interface_entries))) {
    return;
  }

  if (!oonf_timer_is_active(&auto_ll4->update_timer)) {
    /* activate delayed update */
    oonf_timer_set(&auto_ll4->update_timer, 1);
  }
}
