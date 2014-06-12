
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

/* shared buffer for default patterns */
static struct autobuf _patterns;

/**
 * Initialize telnet subsystem
 * @return always returns 0
 */
static int
_init(void) {
  abuf_init(&_patterns);
  return 0;
}

/**
 * Cleanup all allocated data of telnet subsystem
 */
static void
_cleanup(void) {
  abuf_free(&_patterns);
}

/**
 * Generate a new default pattern for the template engine as a list of
 * tab separated keys.
 * @param data pointer to template data array
 * @param count number of elements in template data array
 * @return pointer to pattern, NULL if an error happened
 */
int oonf_viewer_generate_default_format(
    struct oonf_viewer_template *template) {
  const char *ptr;
  bool first = true;
  size_t i;

  ptr = abuf_getptr(&_patterns) + abuf_getlen(&_patterns);

  for (i=0; i<template->data_size; i++) {
    if (first) {
      first = false;
      abuf_puts(&_patterns, "%");
    }
    else {
      abuf_puts(&_patterns, "%\t%");
    }

    abuf_puts(&_patterns, template->data[i].key);
  }

  /* add an additional '0' byte to split patterns */
  abuf_memcpy(&_patterns, "%", 2);

  if (abuf_has_failed(&_patterns)) {
    return -1;
  }

  template->def_format = ptr;
  return 0;
}

/**
 * Generate a common template from two sources and generate a
 * default pattern for the new template.
 * @param dst pointer to destination viewer template
 * @param src1 pointer to first viewer template
 * @param src2 pointer to second viewer template
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_viewer_concat_templates(struct oonf_viewer_template *dst,
    struct oonf_viewer_template *src1, struct oonf_viewer_template *src2) {
  dst->data = calloc(src1->data_size + src2->data_size,
      sizeof(struct abuf_template_data));
  if (!dst->data) {
    return -1;
  }

  memcpy(dst->data, src1->data, src1->data_size * sizeof(struct abuf_template_data));
  memcpy(&dst->data[src1->data_size],
      src2->data, src2->data_size * sizeof(struct abuf_template_data));

  dst->data_size = src1->data_size + src2->data_size;
  return 0;
}

/**
 * Generate a common template from two sources and generate a
 * default pattern for the new template.
 * @param dst pointer to destination viewer template
 * @param src1 pointer to first viewer template
 * @param src2 pointer to second viewer template
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_viewer_concat_3_templates(struct oonf_viewer_template *dst,
    struct oonf_viewer_template *src1, struct oonf_viewer_template *src2,
    struct oonf_viewer_template *src3) {
  dst->data = calloc(src1->data_size + src2->data_size + src3->data_size,
      sizeof(struct abuf_template_data));
  if (!dst->data) {
    return -1;
  }

  memcpy(dst->data, src1->data, src1->data_size * sizeof(struct abuf_template_data));
  memcpy(&dst->data[src1->data_size],
      src2->data, src2->data_size * sizeof(struct abuf_template_data));
  memcpy(&dst->data[src1->data_size + src2->data_size],
      src3->data, src3->data_size * sizeof(struct abuf_template_data));

  dst->data_size = src1->data_size + src2->data_size + src3->data_size;
  return 0;
}

void
oonf_viewer_free_concat_template(struct oonf_viewer_template *template) {
  free(template->data);
}

int
oonf_viewer_prepare_output(struct oonf_viewer_template *template,
    struct autobuf *out, const char *format) {
  template->out = out;

  if (format == NULL || *format == 0) {
    format = template->def_format;
  }
  if (strcmp(format, OONF_VIEWER_JSON_FORMAT) != 0) {
    /* no JSON format, generate template entries */
    template->_storage = abuf_template_init(
        template->data, template->data_size, format);
    if (!template->_storage) {
      return -1;
    }
    template->_format = format;
  }
  else {
    /* JSON output doesn't need storage preparation */
    template->_storage = NULL;

    oonf_viewer_init_json_session(&template->_json, out);

    /* start wrapper object */
    oonf_viewer_start_json_object(&template->_json);

    /* start object with array */
    oonf_viewer_start_json_array(&template->_json, template->json_name);
  }

  return 0;
}

int
oonf_viewer_print_line(struct oonf_viewer_template *template) {
  if (template->_storage) {
    if (abuf_add_template(template->out, template->_format,
        template->_storage, false)) {
      return -1;
    }
    abuf_puts(template->out, "\n");
    return 0;
  }

  /* JSON output */
  oonf_viewer_start_json_object(&template->_json);
  if (oonf_viewer_fill_json_object(&template->_json, template->data, template->data_size)) {
    return -1;
  }
  oonf_viewer_end_json_object(&template->_json);
  return 0;
}

void
oonf_viewer_finish_output(struct oonf_viewer_template *template) {
  if (!template->_storage) {
    oonf_viewer_end_json_array(&template->_json);
    oonf_viewer_end_json_object(&template->_json);
  }
  else {
    free (template->_storage);
    template->_storage = NULL;
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
  size_t i,j;

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
        abuf_appendf(out, "\t%%%s%%\n", template[i].data[j].key);
      }

      _print_help_parameters(out);
      return;
    }
  }

  abuf_appendf(out, "Unknown subcommand %s\n", parameter);
}

int
oonf_viewer_call_subcommands(struct autobuf *out, const char *param,
    struct oonf_viewer_template *templates, size_t count) {
  const char *next = NULL, *ptr = NULL;
  int result;
  size_t i;
  bool head = false;

  for (i=0; i<count; i++) {
    if ((next = str_hasnextword(param, templates[i].json_name))) {
      if ((ptr = str_hasnextword(next, OONF_VIEWER_HEAD_FORMAT))) {
        head = true;
        next = ptr;
      }

      if (oonf_viewer_prepare_output(&templates[i], out, next)) {
        return -1;
      }

      if (head) {
        result = abuf_add_template(out, templates[i]._format,
            templates[i]._storage, true);
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

int
oonf_viewer_fill_json_object(struct oonf_viewer_json_session *session,
    struct abuf_template_data *data, size_t count) {
  int result;

  if (session->empty) {
    session->empty = false;
    abuf_puts(session->out, "\n");
  }
  else {
    abuf_puts(session->out, ",\n");
  }

  /* temporary remove one level */
  session->prefix[session->level-1] = 0;
  result = abuf_add_json(session->out, session->prefix, false, data, count);
  session->prefix[session->level-1] = '\t';

  return result;
}
