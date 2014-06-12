
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

#ifndef OONF_VIEWER_H_
#define OONF_VIEWER_H_

#include "common/common_types.h"
#include "common/template.h"

#include "core/oonf_subsystem.h"

#define OONF_VIEWER_JSON_FORMAT "json"
#define OONF_VIEWER_HEAD_FORMAT "head"

/* This struct contains the session variables for JSON generation */
struct oonf_viewer_json_session {
  /* pointer to output buffer */
  struct autobuf *out;

  /* prefix to handle indentation of output */
  char prefix[32];

  /* current level of indentation */
  int level;

  /* true if we just started a new object/array */
  bool empty;
};

/*
 * This struct defines a template engine command that can output both
 * table and JSON.
 */
struct oonf_viewer_template {
  /* output buffer */
  struct autobuf *out;

  /* pointer to template data array to get key/value pairs */
  struct abuf_template_data *data;

  /* size of template data array */
  size_t data_size;

  /* name of the JSON object which contains the key*/
  const char *json_name;

  /* default format for table view */
  const char *def_format;

  /* single line help text for overview about command */
  const char *help_line;

  /* full help text about this template output */
  const char *help;

  /* callback to generate content */
  int (*cb_function)(struct oonf_viewer_template *);

  /* internal variable for template engine storage array */
  struct abuf_template_storage *_storage;

  /* internal variable for current table format */
  const char *_format;

  /* internal variable for JSON generation */
  struct oonf_viewer_json_session _json;
};

#define LOG_VIEWER oonf_viewer_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_viewer_subsystem;

EXPORT int oonf_viewer_generate_default_format(
    struct oonf_viewer_template *);
EXPORT int oonf_viewer_concat_templates(struct oonf_viewer_template *dst,
    struct oonf_viewer_template *src1, struct oonf_viewer_template *src2);
EXPORT int oonf_viewer_concat_3_templates(struct oonf_viewer_template *dst,
    struct oonf_viewer_template *src1, struct oonf_viewer_template *src2,
    struct oonf_viewer_template *src3);
EXPORT void oonf_viewer_free_concat_template(
    struct oonf_viewer_template *template);

EXPORT int oonf_viewer_prepare_output(struct oonf_viewer_template *template,
    struct autobuf *out, const char *format);
EXPORT int oonf_viewer_print_line(struct oonf_viewer_template *template);
EXPORT void oonf_viewer_finish_output(struct oonf_viewer_template *template);

EXPORT void oonf_viewer_print_help(struct autobuf *out,
    const char *parameter, struct oonf_viewer_template *template, size_t count);
EXPORT int oonf_viewer_call_subcommands(struct autobuf *out, const char *param,
    struct oonf_viewer_template *templates, size_t count);

EXPORT void oonf_viewer_init_json_session(struct oonf_viewer_json_session *,
    struct autobuf *out);
EXPORT void oonf_viewer_start_json_array(struct oonf_viewer_json_session *,
    const char *name);
EXPORT void oonf_viewer_end_json_array(struct oonf_viewer_json_session *);
EXPORT void oonf_viewer_start_json_object(struct oonf_viewer_json_session *);
EXPORT void oonf_viewer_end_json_object(struct oonf_viewer_json_session *);
EXPORT int oonf_viewer_fill_json_object(struct oonf_viewer_json_session *,
    struct abuf_template_data *data, size_t count);

#endif /* OONF_VIEWER_H_ */
