
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
 * @file src-plugins/generic/systeminfo/systeminfo.c
 */

#include <stdio.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"
#include "common/template.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_telnet.h"
#include "subsystems/oonf_viewer.h"

#include "systeminfo/systeminfo.h"

/* definitions */
#define LOG_SYSTEMINFO _oonf_systeminfo_subsystem.logging

/* name of telnet subcommands/JSON nodes */
#define _JSON_NAME_TIME        "time"
#define _JSON_NAME_VERSION     "version"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_telnet_result _cb_systeminfo(struct oonf_telnet_data *con);
static enum oonf_telnet_result _cb_systeminfo_help(struct oonf_telnet_data *con);

static void _initialize_time_values(struct oonf_viewer_template *template);
static void _initialize_version_values(struct oonf_viewer_template *template);

static int _cb_create_text_time(struct oonf_viewer_template *);
static int _cb_create_text_version(struct oonf_viewer_template *);

/*
 * list of template keys and corresponding buffers for values.
 *
 * The keys are API, so they should not be changed after published
 */
#define KEY_TIME_SYSTEM                 "time_system"
#define KEY_TIME_INTERNAL               "time_internal"

#define KEY_VERSION_TEXT                "version_text"
#define KEY_VERSION_COMMIT              "version_commit"

/*
 * buffer space for values that will be assembled
 * into the output of the plugin
 */
static struct oonf_walltime_str         _value_system_time;
static struct isonumber_str             _value_internal_time;

static char                             _value_version_text[256];
static char                             _value_version_commit[21];

/* definition of the template data entries for JSON and table output */
static struct abuf_template_data_entry _tde_time_key[] = {
    { KEY_TIME_SYSTEM, _value_system_time.buf, true },
    { KEY_TIME_INTERNAL, _value_internal_time.buf, false },
};
static struct abuf_template_data_entry _tde_version_key[] = {
    { KEY_VERSION_TEXT, _value_version_text, true },
    { KEY_VERSION_COMMIT, _value_version_commit, true },
};

static struct abuf_template_storage _template_storage;

/* Template Data objects (contain one or more Template Data Entries) */
static struct abuf_template_data _td_time[] = {
    { _tde_time_key, ARRAYSIZE(_tde_time_key) },
};
static struct abuf_template_data _td_version[] = {
    { _tde_version_key, ARRAYSIZE(_tde_version_key) },
};

/* OONF viewer templates (based on Template Data arrays) */
static struct oonf_viewer_template _templates[] = {
    {
        .data = _td_time,
        .data_size = ARRAYSIZE(_td_time),
        .json_name = _JSON_NAME_TIME,
        .cb_function = _cb_create_text_time,
    },
    {
        .data = _td_version,
        .data_size = ARRAYSIZE(_td_version),
        .json_name = _JSON_NAME_VERSION,
        .cb_function = _cb_create_text_version,
    },
};

/* telnet command of this plugin */
static struct oonf_telnet_command _telnet_commands[] = {
    TELNET_CMD(OONF_SYSTEMINFO_SUBSYSTEM, _cb_systeminfo,
        "", .help_handler = _cb_systeminfo_help),
};

/* plugin declaration */
static const char *_dependencies[] = {
  OONF_CLOCK_SUBSYSTEM,
  OONF_TELNET_SUBSYSTEM,
  OONF_VIEWER_SUBSYSTEM,
};

struct oonf_subsystem _olsrv2_systeminfo_subsystem = {
  .name = OONF_SYSTEMINFO_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "OLSRv2 system info plugin",
  .author = "Henning Rogge",
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(_olsrv2_systeminfo_subsystem);

/**
 * Initialize plugin
 * @return -1 if an error happened, 0 otherwise
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
_cb_systeminfo(struct oonf_telnet_data *con) {
  return oonf_viewer_telnet_handler(con->out, &_template_storage,
      OONF_SYSTEMINFO_SUBSYSTEM, con->parameter,
      _templates, ARRAYSIZE(_templates));
}

/**
 * Callback for the help output of this plugin
 * @param con pointer to telnet session data
 * @return telnet result value
 */
static enum oonf_telnet_result
_cb_systeminfo_help(struct oonf_telnet_data *con) {
  return oonf_viewer_telnet_help(con->out, OONF_SYSTEMINFO_SUBSYSTEM,
      con->parameter, _templates, ARRAYSIZE(_templates));
}

/**
 * Initialize the value buffers for the time of the system
 */
static void
_initialize_time_values(struct oonf_viewer_template *template) {
  oonf_log_get_walltime(&_value_system_time);
  isonumber_from_u64(&_value_internal_time, oonf_clock_getNow(),
      "", 3, false, template->create_raw);
}

/**
 * Initialize the value buffers for the version of OONF
 */
static void
_initialize_version_values(
    struct oonf_viewer_template *template __attribute__((unused))) {
  strscpy(_value_version_text, oonf_log_get_libdata()->version,
      sizeof(_value_version_text));
  strscpy(_value_version_commit, oonf_log_get_libdata()->git_commit,
      sizeof(_value_version_commit));
}

/**
 * Callback to generate text/json description of current time
 * @param template viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_time(struct oonf_viewer_template *template) {
  /* initialize values */
  _initialize_time_values(template);

  /* generate template output */
  oonf_viewer_output_print_line(template);
  return 0;
}

/**
 * Callback to generate text/json description of version of OONF
 * @param template viewer template
 * @return -1 if an error happened, 0 otherwise
 */
static int
_cb_create_text_version(struct oonf_viewer_template *template) {
  /* initialize values */
  _initialize_version_values(template);

  /* generate template output */
  oonf_viewer_output_print_line(template);
  return 0;
}
