
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
#include "config/cfg_schema.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_router.h"

/* definitions */
struct _router_config {
  int dummy;
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _router_entries[] = {
  CFG_MAP_INT32(_router_config, dummy, "dummy", "0", "help", 0, false),
};

static struct cfg_schema_section _router_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .cb_delta_handler = _cb_config_changed,
  .entries = _router_entries,
  .entry_count = ARRAYSIZE(_router_entries),
};

static struct _router_config _config;

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

static int
_init(void) {
  return 0;
}

static void
_cleanup(void) {
}

static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_config, _router_section.post,
      _router_entries, ARRAYSIZE(_router_entries))) {
    OONF_WARN(LOG_DLEP_ROUTER, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }
}
