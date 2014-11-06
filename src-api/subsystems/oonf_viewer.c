
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

/* Template call help text for telnet */
static const char _telnet_help[] =
    "\n"
    "Use '" OONF_VIEWER_JSON_FORMAT "' as the first parameter"
    " ' to generate JSON output of all keys/value pairs.\n"
    "Use '" OONF_VIEWER_JSON_RAW_FORMAT "' as the first parameter"
    " to generate JSON output of all keys/value pairs"
    "  without isoprefixes for numbers.\n"
    "Use '" OONF_VIEWER_HEAD_FORMAT "' as the first parameter to"
    " generate a headline for the table.\n"
    "Use '" OONF_VIEWER_RAW_FORMAT "' as the first parameter to"
    " generate a headline for the table without isoprefixes for numbers.\n"
    "You can also add a custom template (text with keys inside)"
    " as the last parameter instead.\n";

/* subsystem definition */
struct oonf_subsystem oonf_viewer_subsystem = {
  .name = OONF_VIEWER_SUBSYSTEM,
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_viewer_subsystem);

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

/**
 * Prepare a viewer template for output. The create_json and
 * create_raw variable should be initialized before calling this
 * function.
 * @param template pointer to viewer template
 * @param storage pointer to autobuffer template storage that should
 *     be printed
 * @param out pointer to output buffer
 * @param format pointer to template for output, not used for JSON output
 * @return
 */
int
oonf_viewer_output_prepare(struct oonf_viewer_template *template,
    struct abuf_template_storage *storage,
    struct autobuf *out, const char *format) {
  template->out = out;

  if (template->create_json) {
    /* JSON format */
    template->_storage = NULL;
    oonf_viewer_json_init_session(&template->_json, out);

    /* start wrapper object */
    oonf_viewer_json_start_object(&template->_json);

    /* start object with array */
    oonf_viewer_json_start_array(&template->_json, template->json_name);
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

/**
 * Print a link of output as a text table or JSON object. The data
 * for the output is collected from the value buffers of the template
 * storage array stored in the template.
 * @param template pointer to viewer template
 */
void
oonf_viewer_output_print_line(struct oonf_viewer_template *template) {
  if (!template->create_json) {
    abuf_add_template(template->out, template->_storage, false);
    abuf_puts(template->out, "\n");
  }
  else {
    /* JSON output */
    oonf_viewer_json_start_object(&template->_json);
    oonf_viewer_json_print_object_ext(&template->_json, template->data, template->data_size);
    oonf_viewer_json_end_object(&template->_json);
  }
}

/**
 * Finalize the output of a text table or JSON object
 * @param template pointer to viewer template
 */
void
oonf_viewer_output_finish(struct oonf_viewer_template *template) {
  if (template->create_json) {
    oonf_viewer_json_end_array(&template->_json);
    oonf_viewer_json_end_object(&template->_json);
  }
}

/**
 * Print telnet help text for array of templates
 * @param out output buffer
 * @param parameter parameter of help command
 * @param template pointer to template array
 * @param count number of elements in template array
 */
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

    abuf_puts(out, _telnet_help);
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

      abuf_puts(out, _telnet_help);
      return;
    }
  }

  abuf_appendf(out, "Unknown subcommand %s\n", parameter);
}

/**
 * Parse the parameter of a telnet call to run the callback of the
 * corresponding template command. This function both prepares and
 * finishes a viewer template.
 * @param out pointer to output buffer
 * @param storage pointer to autobuffer template storage
 * @param param parameter of telnet call
 * @param templates pointer to array of viewer templates
 * @param count number of elements in viewer template array
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_viewer_call_subcommands(struct autobuf *out,
    struct abuf_template_storage *storage, const char *param,
    struct oonf_viewer_template *templates, size_t count) {
  const char *next = NULL, *ptr = NULL;
  int result = 0;
  size_t i;
  bool head = false;
  bool json = false;
  bool raw = false;

  if ((next = str_hasnextword(param, OONF_VIEWER_HEAD_FORMAT))) {
    head = true;
  }
  else if ((next = str_hasnextword(param, OONF_VIEWER_JSON_FORMAT))) {
    json = true;
  }
  else if ((next = str_hasnextword(param, OONF_VIEWER_RAW_FORMAT))) {
    raw = true;
  }
  else if ((next = str_hasnextword(param, OONF_VIEWER_JSON_RAW_FORMAT))) {
    json = true;
    raw = true;
  }
  else {
    next = param;
  }

  for (i=0; i<count; i++) {
    if ((ptr = str_hasnextword(next, templates[i].json_name))) {
      templates[i].create_json = json;
      templates[i].create_raw = raw;

      if (oonf_viewer_output_prepare(&templates[i], storage, out, ptr)) {
        return -1;
      }

      if (head) {
        abuf_add_template(out, templates[i]._storage, true);
        abuf_puts(out, "\n");
      }
      else {
        result = templates[i].cb_function(&templates[i]);
      }

      oonf_viewer_output_finish(&templates[i]);

      return result;
    }
  }
  return 1;
}

/**
 * Initialize the JSON session object for creating a nested JSON
 * string output.
 * @param session JSON session
 * @param out output buffer
 */
void
oonf_viewer_json_init_session(struct oonf_viewer_json_session *session,
    struct autobuf *out) {
  memset(session, 0, sizeof(*session));
  session->out = out;
  session->empty = true;
}

/**
 * Starts a new JSON array
 * @param session JSON session
 * @param name name of JSON array
 */
void
oonf_viewer_json_start_array(struct oonf_viewer_json_session *session,
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

/**
 * Ends a JSON array, should be paired with corresponding _start_array
 * call.
 * @param session JSON session
 */
void
oonf_viewer_json_end_array(struct oonf_viewer_json_session *session) {
  /* close session */
  session->empty = false;
  session->level--;
  session->prefix[session->level] = 0;

  abuf_appendf(session->out, "\n%s]", session->prefix);
}

/**
 * Starts a new JSON object.
 * @param session JSON session
 */
void
oonf_viewer_json_start_object(struct oonf_viewer_json_session *session) {
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

/**
 * Ends a JSON object, should be paired with corresponding _start_object
 * call.
 * @param session JSON session
 */
void
oonf_viewer_json_end_object(struct oonf_viewer_json_session *session) {
  /* close session */
  session->empty = false;
  session->level--;
  session->prefix[session->level] = 0;

  abuf_appendf(session->out, "\n%s}", session->prefix);

  if (session->level == 0) {
    abuf_puts(session->out, "\n");
  }
}

/**
 * Print the contect of an autobuffer template as a list of JSON
 * key/value pairs.
 * @param session JSON session
 * @param data autobuffer template data array
 * @param count number of elements in data array
 */
void
oonf_viewer_json_print_object_ext(struct oonf_viewer_json_session *session,
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
