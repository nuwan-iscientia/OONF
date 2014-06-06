
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
#include "config/cfg.h"
#include "core/oonf_plugins.h"

#include "core/oonf_cfg.h"

#include "cfgio_file/cfgio_file.h"

static void _early_cfg_init(void);
static void _cleanup(void);

static struct cfg_db *_cb_file_load(const char *param, struct autobuf *log);
static int _cb_file_save(const char *param, struct cfg_db *src, struct autobuf *log);

static struct cfg_db *_compact_parse(struct autobuf *input, struct autobuf *log);
static int _compact_serialize(struct autobuf *dst, struct cfg_db *src,
    struct autobuf *log);
static int _parse_line(struct cfg_db *db, char *line, char *section, size_t section_size,
    char *name, size_t name_size, struct autobuf *log);

struct oonf_subsystem oonf_io_file_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONFD file io handler for configuration system",
  .author = "Henning Rogge",

  .cleanup = _cleanup,
  .early_cfg_init = _early_cfg_init,

  .no_logging = true,
};
DECLARE_OONF_PLUGIN(oonf_io_file_subsystem);

struct cfg_io cfg_io_file = {
  .name = "file",
  .load = _cb_file_load,
  .save = _cb_file_save,
  .def = true,
};

/**
 * Callback to hook plugin into configuration system.
 */
static void
_early_cfg_init(void)
{
  cfg_io_add(oonf_cfg_get_instance(), &cfg_io_file);
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void)
{
  cfg_io_remove(oonf_cfg_get_instance(), &cfg_io_file);
}

/*
 * Definition of the file-io handler.
 *
 * This handler can read and write files and use a parser to
 * translate them into a configuration database (and the other way around)
 *
 * The parameter of this parser has to be a filename
 */

/**
 * Reads a file from a filesystem, parse it with the help of a
 * configuration parser and returns a configuration database.
 * @param param file to be read
 * @param log autobuffer for logging purpose
 * @return pointer to configuration database, NULL if an error happened
 */
static struct cfg_db *
_cb_file_load(const char *param, struct autobuf *log) {
  struct autobuf dst;
  struct cfg_db *db;
  char buffer[1024];
  int fd = 0;
  ssize_t bytes;

  fd = open(param, O_RDONLY, 0);
  if (fd == -1) {
    cfg_append_printable_line(log,
        "Cannot open file '%s' to read configuration: %s (%d)",
        param, strerror(errno), errno);
    return NULL;
  }

  bytes = 1;
  if (abuf_init(&dst)) {
    cfg_append_printable_line(log,
        "Out of memory error while allocating io buffer");
    close (fd);
    return NULL;
  }

  /* read file into binary buffer */
  while (bytes > 0) {
    bytes = read(fd, buffer, sizeof(buffer));
    if (bytes < 0 && errno != EINTR) {
      cfg_append_printable_line(log,
          "Error while reading file '%s': %s (%d)",
          param, strerror(errno), errno);
      close(fd);
      abuf_free(&dst);
      return NULL;
    }

    if (bytes > 0) {
      abuf_memcpy(&dst, buffer, (size_t)bytes);
    }
  }
  close(fd);

  db = _compact_parse(&dst, log);
  abuf_free(&dst);
  return db;
}

/**
 * Stores a configuration database into a file. It will use a
 * parser (the serialization part) to translate the database into
 * a storage format.
 * @param param pathname to write configuration file into
 * @param src_db source configuration database
 * @param log autobuffer for logging purpose
 * @return 0 if database was stored sucessfully, -1 otherwise
 */
static int
_cb_file_save(const char *param, struct cfg_db *src_db, struct autobuf *log) {
  int fd = 0;
  ssize_t bytes;
  size_t total;
  struct autobuf abuf;

  if (abuf_init(&abuf)) {
    cfg_append_printable_line(log,
        "Out of memory error while allocating io buffer");
    return -1;
  }
  if (_compact_serialize(&abuf, src_db, log)) {
    abuf_free(&abuf);
    return -1;
  }

  fd = open(param, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    cfg_append_printable_line(log,
        "Cannot open file '%s' for writing configuration: %s (%d)",
        param, strerror(errno), errno);
    return -1;
  }

  total = 0;
  while (total < abuf_getlen(&abuf)) {
    bytes = write(fd, abuf_getptr(&abuf) + total, abuf_getlen(&abuf) - total);
    if (bytes <= 0 && errno != EINTR) {
      cfg_append_printable_line(log,
          "Error while writing to file '%s': %s (%d)",
          param, strerror(errno), errno);
      close(fd);
      return -1;
    }

    if (bytes > 0) {
      total += (size_t)bytes;
    }
  }
  close(fd);
  abuf_free(&abuf);

  return 0;
}

/**
 * Parse a buffer into a configuration database
 * @param input autobuffer with configuration input
 * @param log autobuffer for logging output
 * @return pointer to configuration database, NULL if an error happened
 */
static struct cfg_db *
_compact_parse(struct autobuf *input, struct autobuf *log) {
  char section[128];
  char name[128];
  struct cfg_db *db;
  char *eol, *line;
  char *src;
  size_t len;

  src = abuf_getptr(input);
  len = abuf_getlen(input);

  db = cfg_db_add();
  if (!db) {
    return NULL;
  }

  memset(section, 0, sizeof(section));
  memset(name, 0, sizeof(name));

  line = src;
  while (line < src + len) {
    /* find end of line */
    eol = line;
    while (*eol != 0 && *eol != '\n') {
      eol++;
    }

    /* termiate line with zero byte */
    *eol = 0;
    if (eol > line && eol[-1] == '\r') {
      /* handle \r\n line ending */
      eol[-1] = 0;
    }

    if (_parse_line(db, line, section, sizeof(section),
        name, sizeof(name), log)) {
      cfg_db_remove(db);
      return NULL;
    }

    line = eol+1;
  }
  return db;
}

/**
 * Serialize a configuration database into a buffer
 * @param dst target buffer
 * @param src source configuration database
 * @param log autbuffer for logging
 * @return 0 if database was serialized, -1 otherwise
 */
static int
_compact_serialize(struct autobuf *dst, struct cfg_db *src,
    struct autobuf *log __attribute__ ((unused))) {
  struct cfg_section_type *section, *s_it;
  struct cfg_named_section *name, *n_it;
  struct cfg_entry *entry, *e_it;
  char *ptr;

  CFG_FOR_ALL_SECTION_TYPES(src, section, s_it) {
    CFG_FOR_ALL_SECTION_NAMES(section, name, n_it) {
      if (cfg_db_is_named_section(name)) {
        abuf_appendf(dst, "[%s=%s]\n", section->type, name->name);
      }
      else {
        abuf_appendf(dst, "[%s]\n", section->type);
      }

      CFG_FOR_ALL_ENTRIES(name, entry, e_it) {
        strarray_for_each_element(&entry->val, ptr) {
          abuf_appendf(dst, "\t%s %s\n", entry->name, ptr);
        }
      }
    }
  }
  return 0;
}

/**
 * Parse a single line of the compact format
 * @param db pointer to configuration database
 * @param line pointer to line to be parsed (will be modified
 *   during parsing)
 * @param section pointer to array with current section type
 *   (might be modified during parsing)
 * @param section_size number of bytes for section type
 * @param name pointer to array with current section name
 *   (might be modified during parsing)
 * @param name_size number of bytes for section name
 * @param log autobuffer for logging output
 * @return 0 if line was parsed successfully, -1 otherwise
 */
static int
_parse_line(struct cfg_db *db, char *line,
    char *section, size_t section_size,
    char *name, size_t name_size,
    struct autobuf *log) {
  char *first, *ptr;
  bool dummy;

  /* trim leading and trailing whitespaces */
  first = str_trim(line);

  if (*first == 0 || *first == '#') {
    /* empty line or comment */
    return 0;
  }

  if (*first == '[') {
    first++;
    ptr = strchr(first, ']');
    if (ptr == NULL) {
      cfg_append_printable_line(log,
          "Section syntax error in line: '%s'", line);
      return -1;
    }
    *ptr = 0;

    ptr = strchr(first, '=');
    if (ptr) {
      /* trim section name */
      *ptr++ = 0;
      ptr = str_trim(ptr);
    }

    /* trim section name */
    first = str_trim(first);
    if (*first == 0) {
      cfg_append_printable_line(log,
          "Section syntax error, no section type found");
      return -1;
    }

    /* copy section type */
    strscpy(section, first, section_size);

    /* copy section name */
    if (ptr) {
      strscpy(name, ptr, name_size);
    }
    else {
      *name = 0;
    }

    /* validity of section type (and name) */
    if (!cfg_is_allowed_key(section)) {
      cfg_append_printable_line(log,
          "Illegal section type: '%s'", section);
      return -1;
    }

    if (*name != 0 && !cfg_is_allowed_section_name(name)) {
      cfg_append_printable_line(log,
          "Illegal section name: '%s'", name);
      return -1;
    }

    /* add section to db */
    if (_cfg_db_add_section(db, section, *name ? name : NULL, &dummy) == NULL) {
      return -1;
    }
    return 0;
  }

  if (*section == 0) {
    cfg_append_printable_line(log,
        "Entry before first section is not allowed in this format");
    return -1;
  }

  ptr = first;

  /* look for separator */
  while (!isspace(*ptr)) {
    ptr++;
  }

  *ptr++ = 0;

  /* trim second token */
  ptr = str_trim(ptr);

  if (*ptr == 0) {
    cfg_append_printable_line(log,
        "No second token found in line '%s'",  line);
    return -1;
  }

  if (!cfg_is_allowed_key(first)) {
    cfg_append_printable_line(log,
        "Illegal key type: '%s'", first);
    return -1;
  }

  /* found two tokens */
  if (!cfg_db_add_entry(db, section, *name ? name : NULL, first, ptr)) {
    return -1;
  }
  return 0;
}
