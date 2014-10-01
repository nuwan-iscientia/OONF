
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
#include "common/netaddr.h"

#include "config/cfg_schema.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_parser.h"
#include "dlep/dlep_static_data.h"
#include "dlep/dlep_writer.h"
#include "dlep/router/dlep_router.h"
#include "dlep/router/dlep_router_interface.h"

/* prototypes */
static int _init(void);
static void _initiate_shutdown(void);
static void _cleanup(void);

static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _router_entries[] = {
  CFG_MAP_NETADDR_V4(dlep_router_if, udp_config.multicast_v4, "discovery_mc_v4",
    "224.0.0.1", "IPv4 address to send discovery UDP packet to", false, false),
  CFG_MAP_NETADDR_V6(dlep_router_if, udp_config.multicast_v6, "discovery_mc_v6",
    "ff02::1", "IPv6 address to send discovery UDP packet to", false, false),
  CFG_MAP_INT32_MINMAX(dlep_router_if, udp_config.multicast_port, "discovery_port",
    "12345", "UDP port for discovery packets", 0, false, 1, 65535),

  CFG_MAP_ACL_V46(dlep_router_if, udp_config.bindto, "discovery_bindto", "fe80::/10",
    "Filter to determine the binding of the UDP discovery socket"),

  CFG_MAP_CLOCK_MIN(dlep_router_if, local_discovery_interval,
    "discovery_interval", "1.000",
    "Interval in seconds between two discovery beacons", 1000),
  CFG_MAP_CLOCK_MINMAX(dlep_router_if, local_heartbeat_interval,
      "heartbeat_interval", "1.000",
    "Interval in seconds between two heartbeat signals", 1000, 65535000),
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
  .initiate_shutdown = _initiate_shutdown,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(dlep_router_subsystem);

/**
 * Plugin constructor for dlep router
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  if (dlep_writer_init()) {
    return -1;
  }

  dlep_router_interface_init();
  return 0;
}

/**
 * Send a clean Peer Terminate before we drop the session to shutdown
 */
static void
_initiate_shutdown(void) {
  dlep_router_terminate_all_sessions();
}

/**
 * Plugin destructor for dlep router
 */
static void
_cleanup(void) {
  dlep_router_interface_cleanup();
  dlep_writer_cleanup();
}

/**
 * Callback for configuration changes
 */
static void
_cb_config_changed(void) {
  struct dlep_router_if *interface;

  if (!_router_section.post) {
    /* remove old session object */
    interface = dlep_router_get_interface(_router_section.section_name);
    if (interface) {
      dlep_router_remove_interface(interface);
    }
    return;
  }

  /* get session object or create one */
  interface = dlep_router_add_interface(_router_section.section_name);
  if (!interface) {
    return;
  }

  /* read configuration */
  if (cfg_schema_tobin(interface, _router_section.post,
      _router_entries, ARRAYSIZE(_router_entries))) {
    OONF_WARN(LOG_DLEP_ROUTER, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }

  /* apply interface name to socket */
  strscpy(interface->udp_config.interface, _router_section.section_name,
      sizeof(interface->udp_config.interface));

  /* apply settings */
  dlep_router_apply_interface_settings(interface);
}
