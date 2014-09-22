
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

#include <errno.h>
#include <unistd.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "config/cfg_schema.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_router.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static struct dlep_router_session *_add_session(const char *interf);
static void _remove_session(struct dlep_router_session *);

static void _cb_router_send_event(void *);
static void _cb_router_timeout(void *);
static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _router_entries[] = {
  CFG_MAP_NETADDR_V4(dlep_router_session, discovery_config.multicast_v4, "discovery_mc_v4",
    "224.0.0.1", "IPv4 address to send discovery UDP packet to", false, false),
  CFG_MAP_NETADDR_V6(dlep_router_session, discovery_config.multicast_v6, "discovery_mc_v6",
    "ff02::1", "IPv6 address to send discovery UDP packet to", false, false),
  CFG_MAP_INT32_MINMAX(dlep_router_session, discovery_config.multicast_port, "discovery_port",
    "12345", "UDP port to send discovery packets to", 0, false, 1, 65535),

  CFG_MAP_ACL_V46(dlep_router_session, discovery_config.bindto, "discovery_bindto", "fe80::/10",
    "Filter to determine the binding of the multicast discovery socket"),

  CFG_MAP_CLOCK_MIN(dlep_router_session, discovery_interval, "discovery_interval", "1.000",
    "Interval in seconds between two discovery beacons", 1000),
  CFG_MAP_CLOCK_MIN(dlep_router_session, heartbeat_interval, "heartbeat_interval", "1.000",
    "Interval in seconds between two heartbeat signals", 1000),
};

static struct cfg_schema_section _router_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .mode = CFG_SSMODE_NAMED,
  .cb_delta_handler = _cb_config_changed,
  .entries = _router_entries,
  .entry_count = ARRAYSIZE(_router_entries),
};

/* plugin declaration */
struct oonf_subsystem dlep_router_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONF DLEP router plugin",
  .author = "Henning Rogge",

  .cfg_section = &_router_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(dlep_router_subsystem);

/* session objects */
static struct avl_tree _session_tree;

static struct oonf_class _session_class = {
  .name = "DLEP router session",
  .size = sizeof(struct dlep_router_session),
};

static struct oonf_timer_info _session_event_timer_info = {
  .name = "DLEP router event",
  .callback = _cb_router_send_event,
  .periodic = true,
};

static struct oonf_timer_info _session_timeout_info = {
  .name = "DLEP router timeout",
  .callback = _cb_router_timeout,
};

static int
_init(void) {
  oonf_class_add(&_session_class);
  oonf_timer_add(&_session_event_timer_info);
  oonf_timer_add(&_session_timeout_info);

  avl_init(&_session_tree, avl_comp_strcasecmp, false);
  return 0;
}

static void
_cleanup(void) {
  struct dlep_router_session *session, *s_it;

  avl_for_each_element_safe(&_session_tree, session, _node, s_it) {
    _remove_session(session);
  }

  oonf_timer_remove(&_session_timeout_info);
  oonf_timer_remove(&_session_event_timer_info);
  oonf_class_remove(&_session_class);
}

static struct dlep_router_session *
_add_session(const char *interf) {
  struct dlep_router_session *session;

  session = avl_find_element(&_session_tree, interf, session, _node);
  if (session) {
    return session;
  }

  session = oonf_class_malloc(&_session_class);
  if (!session) {
    return NULL;
  }

  /* initialize key */
  strscpy(session->interf, interf, sizeof(session->interf));
  session->_node.key = session->interf;

  /* initialize timer */
  session->event_timer.cb_context = session;
  session->event_timer.info = &_session_event_timer_info;

  session->timeout.cb_context = session;
  session->timeout.info = &_session_timeout_info;

  /* set socket to discovery mode */
  session->state = DLEP_ROUTER_DISCOVERY;

  /* add to global tree of sessions */
  avl_insert(&_session_tree, &session->_node);

  /* initialize discovery socket */
  oonf_packet_add_managed(&session->discovery);

  return session;
}

static void
_remove_session(struct dlep_router_session *session) {
  /* cleanup discovery socket */
  oonf_packet_remove_managed(&session->discovery, true);

  /* stop timers */
  oonf_timer_stop(&session->event_timer);
  oonf_timer_stop(&session->timeout);

  /* remove session */
  avl_remove(&_session_tree, &session->_node);
  oonf_class_free(&_session_class, session);
}

static void
_cb_router_send_event(void *ptr) {
  struct dlep_router_session *session = ptr;

  switch (session->state) {
    case DLEP_ROUTER_DISCOVERY:
      /* TODO: send discovery beacon */
      break;
    case DLEP_ROUTER_ACTIVE:
      /* TODO: send heartbeat */
      break;
    default:
      break;
  }
}

static void
_cb_router_timeout(void *ptr) {
  struct dlep_router_session *session = ptr;

  switch (session->state) {
    case DLEP_ROUTER_CONNECT:
      /* TODO: handle connection timeout */
      break;
    case DLEP_ROUTER_ACTIVE:
      /* handle heartbeat timeout */
      break;
    default:
      break;
  }
}

static void
_cb_config_changed(void) {
  struct dlep_router_session *session;

  if (!_router_section.post) {
    /* remove old session object */
    session = avl_find_element(&_session_tree,
        _router_section.section_name, session, _node);
    if (session) {
      _remove_session(session);
    }
    return;
  }

  /* get session object or create one */
  session = _add_session(_router_section.section_name);
  if (!session) {
    return;
  }

  /* read configuration */
  if (cfg_schema_tobin(session, _router_section.post,
      _router_entries, ARRAYSIZE(_router_entries))) {
    OONF_WARN(LOG_DLEP_ROUTER, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }

  /* apply settings */
  oonf_packet_apply_managed(&session->discovery, &session->discovery_config);

  switch (session->state) {
    case DLEP_ROUTER_DISCOVERY:
      /* (re)start discovery timer */
      oonf_timer_set(&session->event_timer, session->discovery_interval);
      break;
    case DLEP_ROUTER_ACTIVE:
      /* (re)start heartbeat timer */
      oonf_timer_set(&session->event_timer, session->heartbeat_interval);
      break;
    default:
      break;
  }
}
