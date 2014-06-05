
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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/autobuf.h"
#include "config/cfg_io.h"
#include "config/cfg_parser.h"
#include "config/cfg.h"
#include "core/oonf_plugins.h"

#include "core/oonf_cfg.h"

#include <uci.h>

#include "cfg_uciloader/cfg_uciloader.h"

static void _early_cfg_init(void);
static void _cleanup(void);

static struct cfg_db *_cb_uci_load(struct cfg_instance *instance,
    const char *param, const char *parser, struct autobuf *log);

struct oonf_subsystem oonf_cfg_uciloader_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONF uci handler for configuration system",
  .author = "Henning Rogge",

  .cleanup = _cleanup,
  .early_cfg_init = _early_cfg_init,

  .no_logging = true,
};
DECLARE_OONF_PLUGIN(oonf_cfg_uciloader_subsystem);

struct cfg_io cfg_io_uci = {
  .name = "uci",
  .load = _cb_uci_load,
  .def = false,
};

/**
 * Callback to hook plugin into configuration system.
 */
static void
_early_cfg_init(void)
{
  cfg_io_add(oonf_cfg_get_instance(), &cfg_io_uci);
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void)
{
  cfg_io_remove(oonf_cfg_get_instance(), &cfg_io_uci);
}

/*
 * Definition of the uci-io handler.
 *
 * This handler can read and write files and use a parser to
 * translate them into a configuration database (and the other way around)
 *
 * The parameter of this parser has to be a filename
 */

/**
 * Reads a file from a filesystem, parse it with the help of a
 * configuration parser and returns a configuration database.
 * @param parser parser name, NULL if autodetection should be used
 * @param param file to be read
 * @param log autobuffer for logging purpose
 * @return pointer to configuration database, NULL if an error happened
 */
static struct cfg_db *
_cb_uci_load(struct cfg_instance *instance __attribute__((unused)),
    const char *param, const char *parser __attribute__((unused)),
    struct autobuf *log) {
  struct uci_context *ctx = NULL;
  struct uci_package *p = NULL;
  struct uci_element *s, *o, *i;

  struct cfg_db *db = NULL;
  struct cfg_named_section *db_section;

  char *err = NULL;

  ctx = uci_alloc_context();
  if (!ctx) {
    cfg_append_printable_line(log, "Could not allocate uci context");
    return NULL;
  }

  if (uci_load(ctx, param, &p)) {
    goto uci_error;
  }

  db = cfg_db_add();
  if (!db) {
    goto loading_error;
  }

  uci_foreach_element(&p->sections, s) {
    struct uci_section *sec = uci_to_section(s);

    if (sec->anonymous) {
      db_section = cfg_db_add_unnamedsection(db, sec->type);
    }
    else {
      db_section = cfg_db_add_namedsection(db, sec->type, sec->e.name);
    }
    if (!db_section) {
      cfg_append_printable_line(log, "Could not allocate configuration section");
      goto loading_error;
    }

    uci_foreach_element(&sec->options, o) {
      struct uci_option *opt = uci_to_option(o);
      switch(opt->type) {
        case UCI_TYPE_STRING:
          if (!cfg_db_add_entry(db, db_section->section_type->type,
              db_section->name, opt->e.name, opt->v.string)) {
            cfg_append_printable_line(log, "Could not allocate configuration entry (%s/%s/%s)='%s'",
                db_section->section_type->type, db_section->name, opt->e.name, opt->v.string);
                  goto loading_error;
          };
          break;
        case UCI_TYPE_LIST:
          uci_foreach_element(&opt->v.list, i) {
            if (!cfg_db_add_entry(db, db_section->section_type->type,
                db_section->name, opt->e.name, i->name)) {
              cfg_append_printable_line(log, "Could not allocate configuration entry (%s/%s/%s)='%s'",
                  db_section->section_type->type, db_section->name, opt->e.name, i->name);
                    goto loading_error;
            };
          }
          break;
        default:
          cfg_append_printable_line(log, "# uci-error: unknown type for option '%s'\n", opt->e.name);
          goto loading_error;
      }
    }
  }
  return db;

uci_error:
  uci_get_errorstr(ctx, &err, NULL);
  abuf_puts(log, err);
  free(err);

loading_error:
  if (db) {
    cfg_db_remove(db);
  }

  uci_free_context(ctx);
  return NULL;
}
