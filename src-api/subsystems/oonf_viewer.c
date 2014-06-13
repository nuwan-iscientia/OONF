
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

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/template.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_viewer.h"

/* static function prototypes */
static int _init(void);
static void _cleanup(void);

/* subsystem definition */
struct oonf_subsystem oonf_viewer_subsystem = {
  .name = "viewer",
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize telnet subsystem
 * @return always returns 0
 */
static int
_init(void) {
  return 0;
}

/**
 * Cleanup all allocated data of telnet subsystem
 */
static void
_cleanup(void) {
}

int
oonf_viewer_prepare_output(struct oonf_viewer_template *template,
    struct abuf_template_storage *storage,
    struct autobuf *out, const char *format) {
  template->out = out;

  if (format && strcmp(format, OONF_VIEWER_JSON_FORMAT) == 0) {
    /* JSON format */
    template->_storage = NULL;
    oonf_viewer_init_json_session(&template->_json, out);

    /* start wrapper object */
    oonf_viewer_start_json_object(&template->_json);

    /* start object with array */
    oonf_viewer_start_json_array(&template->_json, template->json_name);
  }
  else {
    if (format && *format == 0) {
      format = NULL;
    }

    /* no JSON format, generate template entries */
    template->_storage = storage;
    abuf_template_init_ext(template->_storage,
        template->data, template->data_size, format);
  }

  return 0;
}

void
oonf_viewer_print_output_line(struct oonf_viewer_template *template) {
  if (template->_storage) {
    abuf_add_template(template->out, template->_storage, false);
    abuf_puts(template->out, "\n");
  }

  /* JSON output */
  oonf_viewer_start_json_object(&template->_json);
  oonf_viewer_fill_json_object_ext(&template->_json, template->data, template->data_size);
  oonf_viewer_end_json_object(&template->_json);
}

void
oonf_viewer_finish_output(struct oonf_viewer_template *template) {
  if (!template->_storage) {
    oonf_viewer_end_json_array(&template->_json);
    oonf_viewer_end_json_object(&template->_json);
  }
}

static void
_print_help_parameters(struct autobuf *out) {
  abuf_puts(out, "\nAdd the additional parameter '" OONF_VIEWER_JSON_FORMAT
      "' to generate JSON output of all keys/value pairs.\n"
      "Add the additional parameter '" OONF_VIEWER_HEAD_FORMAT "' to"
      " generate a headline for the table.\n"
      "You can also add a custom template (text with keys inside)"
      " as a parameter.\n");
}
void
oonf_viewer_print_help(struct autobuf *out, const char *parameter,
    struct oonf_viewer_template *template, size_t count) {
  size_t i,j,k;

  if (parameter == NULL || *parameter == 0) {
    abuf_puts(out, "Available subcommands:\n");

    for (i=0; i<count; i++) {
      if (template[i].help_line) {
        abuf_appendf(out, "\t%s: %s\n", template[i].json_name, template[i].help_line);
      }
      else {
        abuf_appendf(out, "\t%s\n", template[i].json_name);
      }
    }
    _print_help_parameters(out);
    abuf_puts(out, "Use 'help <command> <subcommand>' to get help about a subcommand\n");
    return;
  }
  for (i=0; i<count; i++) {
    if (strcmp(parameter, template[i].json_name) == 0) {
      if (template[i].help) {
        abuf_puts(out, template[i].help);
      }
      abuf_appendf(out, "The subcommand '%s' has the following keys:\n",
          template[i].json_name);

      for (j=0; j<template[i].data_size; j++) {
        for (k=0; k<template[i].data[j].count; k++) {
          abuf_appendf(out, "\t%%%s%%\n", template[i].data[j].data[k].key);
        }
      }

      _print_help_parameters(out);
      return;
    }
  }

  abuf_appendf(out, "Unknown subcommand %s\n", parameter);
}

int
oonf_viewer_call_subcommands(struct autobuf *out,
    struct abuf_template_storage *storage, const char *param,
    struct oonf_viewer_template *templates, size_t count) {
  const char *next = NULL, *ptr = NULL;
  int result = 0;
  size_t i;
  bool head = false;

  for (i=0; i<count; i++) {
    if ((next = str_hasnextword(param, templates[i].json_name))) {
      if ((ptr = str_hasnextword(next, OONF_VIEWER_HEAD_FORMAT))) {
        head = true;
        next = ptr;
      }

      if (oonf_viewer_prepare_output(&templates[i], storage, out, next)) {
        return -1;
      }

      if (head) {
        abuf_add_template(out, templates[i]._storage, true);
        abuf_puts(out, "\n");
      }
      else {
        result = templates[i].cb_function(&templates[i]);
      }

      oonf_viewer_finish_output(&templates[i]);

      return result;
    }
  }
  return 1;
}

void
oonf_viewer_init_json_session(struct oonf_viewer_json_session *session,
    struct autobuf *out) {
  memset(session, 0, sizeof(*session));
  session->out = out;
  session->empty = true;
}

void
oonf_viewer_start_json_array(struct oonf_viewer_json_session *session,
    const char *name) {
  if (!session->empty) {
    abuf_puts(session->out, ",");
    session->empty = true;
  }

  if (session->level > 0) {
    abuf_puts(session->out, "\n");
  }
  abuf_appendf(session->out, "%s\"%s\": [", session->prefix, name);

  session->prefix[session->level] = '\t';
  session->level++;
  session->prefix[session->level] = 0;
}

void
oonf_viewer_end_json_array(struct oonf_viewer_json_session *session) {
  /* close session */
  session->empty = false;
  session->level--;
  session->prefix[session->level] = 0;

  abuf_appendf(session->out, "\n%s]", session->prefix);
}

void
oonf_viewer_start_json_object(struct oonf_viewer_json_session *session) {
  /* open new session */
  if (!session->empty) {
    abuf_puts(session->out, ",");
    session->empty = true;
  }

  if (session->level > 0) {
    abuf_puts(session->out, "\n");
  }
  abuf_appendf(session->out, "%s{", session->prefix);

  session->prefix[session->level] = '\t';
  session->level++;
  session->prefix[session->level] = 0;
}

void
oonf_viewer_end_json_object(struct oonf_viewer_json_session *session) {
  /* close session */
  session->empty = false;
  session->level--;
  session->prefix[session->level] = 0;

  abuf_appendf(session->out, "\n%s}", session->prefix);

  if (session->level == 0) {
    abuf_puts(session->out, "\n");
  }
}

void
oonf_viewer_fill_json_object_ext(struct oonf_viewer_json_session *session,
    struct abuf_template_data *data, size_t count) {
  if (session->empty) {
    session->empty = false;
    abuf_puts(session->out, "\n");
  }
  else {
    abuf_puts(session->out, ",\n");
  }

  /* temporary remove one level */
  session->prefix[session->level-1] = 0;
  abuf_add_json_ext(session->out, session->prefix, false, data, count);
  session->prefix[session->level-1] = '\t';
}
