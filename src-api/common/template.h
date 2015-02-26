
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

#ifndef TEMPLATE_H_
#define TEMPLATE_H_

#include "common/common_types.h"
#include "common/autobuf.h"

#define TEMPLATE_JSON_TRUE            "true"
#define TEMPLATE_JSON_FALSE           "false"

enum {
  TEMPLATE_JSON_BOOL_LENGTH = 6,
  TEMPLATE_MAX_KEYS         = 32
};

struct abuf_template_data_entry {
  const char *key;
  const char *value;
  bool string;
};

struct abuf_template_data {
  struct abuf_template_data_entry *data;
  size_t count;
};
struct abuf_template_storage_entry {
  struct abuf_template_data_entry *data;
  size_t start, end;
};

struct abuf_template_storage {
  size_t count;
  const char *format;
  struct abuf_template_storage_entry indices[TEMPLATE_MAX_KEYS];
};

EXPORT void abuf_template_init_ext (struct abuf_template_storage *storage,
    struct abuf_template_data *data, size_t data_count, const char *format);
EXPORT void abuf_add_template(struct autobuf *out,
    struct abuf_template_storage *storage, bool keys);
EXPORT void abuf_add_json_ext(struct autobuf *out, const char *prefix, bool newline,
    struct abuf_template_data *data, size_t data_count);

/**
 * Helper function to initialize a template with
 * a single abuf_template_data_entry array
 * @param storage
 * @param entry
 * @param entry_count
 * @param format
 */
static INLINE void
abuf_template_init(struct abuf_template_storage *storage,
    struct abuf_template_data_entry *entry, size_t entry_count, const char *format) {
  struct abuf_template_data data = { entry, entry_count };
  abuf_template_init_ext(storage, &data, 1, format);
}

/**
 * Helper function to print a json object with
 * a single abuf_template_data_entry array
 * @param out
 * @param prefix
 * @param newline
 * @param entry
 * @param entry_count
 */
static INLINE void
abuf_add_json(struct autobuf *out, const char *prefix, bool newline,
    struct abuf_template_data_entry *entry, size_t entry_count) {
  struct abuf_template_data data = { entry, entry_count };
  abuf_add_json_ext(out, prefix, newline, &data, 1);
}

/**
 * @param b boolean value
 * @return JSON string representation of boolean value
 */
static INLINE const char *
abuf_json_getbool(bool b) {
  return b ? TEMPLATE_JSON_TRUE : TEMPLATE_JSON_FALSE;
}
#endif /* TEMPLATE_H_ */
