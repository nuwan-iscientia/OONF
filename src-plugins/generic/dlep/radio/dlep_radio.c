
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

#include <errno.h>
#include <unistd.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/netaddr.h"

#include "config/cfg_schema.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_stream_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_parser.h"
#include "dlep/dlep_static_data.h"
#include "dlep/dlep_writer.h"
#include "dlep/radio/dlep_radio.h"
#include "dlep/radio/dlep_radio_interface.h"
#include "dlep/radio/dlep_radio_internal.h"

/* prototypes */
static void _early_cfg_init(void);
static int _init(void);
static void _cleanup(void);
static void _initiate_shutdown(void);

static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _radio_entries[] = {
  CFG_MAP_STRING_ARRAY(dlep_radio_if, udp_config.interface, "datapath_if", NULL,
     "Name of interface to talk to dlep router", IF_NAMESIZE),

  CFG_MAP_NETADDR_V4(dlep_radio_if, udp_config.multicast_v4, "discovery_mc_v4",
    DLEP_WELL_KNOWN_MULTICAST_ADDRESS, "IPv4 address to send discovery UDP packet to", false, false),
  CFG_MAP_NETADDR_V6(dlep_radio_if, udp_config.multicast_v6, "discovery_mc_v6",
    DLEP_WELL_KNOWN_MULTICAST_ADDRESS_6, "IPv6 address to send discovery UDP packet to", false, false),
  CFG_MAP_INT32_MINMAX(dlep_radio_if, udp_config.port, "discovery_port",
    DLEP_WELL_KNOWN_MULTICAST_PORT_TXT, "UDP port for discovery packets", 0, false, 1, 65535),
  CFG_MAP_ACL_V46(dlep_radio_if, udp_config.bindto, "discovery_bindto", "fe80::/10",
    "Filter to determine the binding of the UDP discovery socket"),

  CFG_MAP_INT32_MINMAX(dlep_radio_if, tcp_config.port, "session_port",
    "12345", "Server port for DLEP tcp sessions", 0, false, 1, 65535),
  CFG_MAP_ACL_V46(dlep_radio_if, tcp_config.bindto, "session_bindto", "fe80::/10",
      "Filter to determine the binding of the TCP server socket"),
  CFG_MAP_CLOCK_MINMAX(dlep_radio_if, local_heartbeat_interval,
      "heartbeat_interval", "1.000",
      "Interval in seconds between two heartbeat signals", 1000, 65535 * 1000),

  CFG_MAP_BOOL(dlep_radio_if, use_proxied_dst, "proxied", "false",
      "Report 802.11s proxied mac address for neighbors"),
  CFG_MAP_BOOL(dlep_radio_if, use_nonproxied_dst, "not_proxied", "true",
      "Report direct neighbors"),
};

static struct cfg_schema_section _radio_section = {
  .type = OONF_DLEP_RADIO_SUBSYSTEM,
  .mode = CFG_SSMODE_NAMED,

  .help = "name of the layer2 interface DLEP radio will take its data from",

  .cb_delta_handler = _cb_config_changed,

  .entries = _radio_entries,
  .entry_count = ARRAYSIZE(_radio_entries),
};

/* subsystem declaration */
static const char *_dependencies[] = {
  OONF_CLASS_SUBSYSTEM,
  OONF_LAYER2_SUBSYSTEM,
  OONF_PACKET_SUBSYSTEM,
  OONF_STREAM_SUBSYSTEM,
  OONF_TIMER_SUBSYSTEM,
};
static struct oonf_subsystem _dlep_radio_subsystem = {
  .name = OONF_DLEP_RADIO_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "OONF DLEP radio plugin",
  .author = "Henning Rogge",

  .cfg_section = &_radio_section,

  .early_cfg_init = _early_cfg_init,
  .init = _init,
  .initiate_shutdown = _initiate_shutdown,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(_dlep_radio_subsystem);

/* logging */
enum oonf_log_source LOG_DLEP_RADIO;

static void
_early_cfg_init(void) {
  LOG_DLEP_RADIO = _dlep_radio_subsystem.logging;
}

/**
 * Plugin constructor for dlep radio
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  if (dlep_writer_init()) {
    return -1;
  }

  dlep_radio_interface_init();
  return 0;
}

/**
 * Send a clean Peer Terminate before we drop the session to shutdown
 */
static void
_initiate_shutdown(void) {
  dlep_radio_terminate_all_sessions();
}

/**
 * Plugin destructor for dlep radio
 */
static void
_cleanup(void) {
  dlep_radio_interface_cleanup();
  dlep_writer_cleanup();
}

/**
 * Callback for configuration changes
 */
static void
_cb_config_changed(void) {
  struct dlep_radio_if *interface;

  if (!_radio_section.post) {
    /* remove old interface object */
    interface = dlep_radio_get_interface(_radio_section.section_name);
    if (interface) {
      dlep_radio_remove_interface(interface);
    }
    return;
  }

  /* get interface object or create one */
  interface = dlep_radio_add_interface(_radio_section.section_name);
  if (!interface) {
    return;
  }

  /* read configuration */
  if (cfg_schema_tobin(interface, _radio_section.post,
      _radio_entries, ARRAYSIZE(_radio_entries))) {
    OONF_WARN(LOG_DLEP_RADIO, "Could not convert "
        OONF_DLEP_RADIO_SUBSYSTEM " config to bin");
    return;
  }

  /* apply interface name also to TCP socket */
  strscpy(interface->tcp_config.interface, interface->udp_config.interface,
      sizeof(interface->tcp_config.interface));

  /* apply settings */
  dlep_radio_apply_interface_settings(interface);
}
