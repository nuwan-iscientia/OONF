
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
 * @file src-plugins/subsystems/oonf_socket.c
 */

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common/avl.h"
#include "common/key_comp.h"
#include "subsystems/oonf_clock.h"
#include "core/oonf_logging.h"
#include "core/oonf_main.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_socket.h"
#include "subsystems/os_clock.h"
#include "subsystems/oonf_socket.h"

/* Definitions */
#define LOG_SOCKET _oonf_socket_subsystem.logging

/* prototypes */
static int _init(void);
static void _cleanup(void);
static void _initiate_shutdown(void);
static int _handle_scheduling(void);

/* time until the scheduler should run */
static uint64_t _scheduler_time_limit;

/* List of all active sockets in scheduler */
static struct list_entity _socket_head;

/* subsystem definition */
static const char *_dependencies[] = {
  OONF_TIMER_SUBSYSTEM,
  OONF_OS_SOCKET_SUBSYSTEM,
};

static struct oonf_subsystem _oonf_socket_subsystem = {
  .name = OONF_SOCKET_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .init = _init,
  .cleanup = _cleanup,
  .initiate_shutdown = _initiate_shutdown,
};
DECLARE_OONF_PLUGIN(_oonf_socket_subsystem);

/**
 * Initialize olsr socket scheduler
 * @return always returns 0
 */
static int
_init(void) {
  if (oonf_main_set_scheduler(_handle_scheduling)) {
    return -1;
  }

  list_init_head(&_socket_head);
  return 0;
}

/**
 * Cleanup olsr socket scheduler.
 * This will close and free all sockets.
 */
static void
_cleanup(void)
{
  struct oonf_socket_entry *entry, *iterator;

  list_for_each_element_safe(&_socket_head, entry, _node, iterator) {
    list_remove(&entry->_node);
    os_socket_close(entry->fd);
  }
}

static void
_initiate_shutdown(void) {
  /* stop within 500 ms */
  _scheduler_time_limit = oonf_clock_get_absolute(500);
  OONF_INFO(LOG_SOCKET, "Stop within 500 ms");
}

/**
 * Add a socket handler to the scheduler
 *
 * @param entry pointer to initialized socket entry
 * @return -1 if an error happened, 0 otherwise
 */
void
oonf_socket_add(struct oonf_socket_entry *entry)
{
  assert (entry->fd >= 0);
  assert (entry->process);

  OONF_DEBUG(LOG_SOCKET, "Adding socket entry %d to scheduler\n", entry->fd);

  list_add_before(&_socket_head, &entry->_node);
}

/**
 * Remove a socket from the socket scheduler
 * @param entry pointer to socket entry
 */
void
oonf_socket_remove(struct oonf_socket_entry *entry)
{
  OONF_DEBUG(LOG_SOCKET, "Removing socket entry %d\n", entry->fd);

  list_remove(&entry->_node);
}

/**
 * Handle all incoming socket events and timer events
 * @return -1 if an error happened, 0 otherwise
 */
int
_handle_scheduling(void)
{
  struct oonf_socket_entry *entry, *iterator;
  uint64_t next_event, stop_time;
  struct timeval tv, *tv_ptr;
  int n = 0;
  bool fd_read;
  bool fd_write;

  while (true) {
    fd_set ibits, obits;
    int hfd = 0;

    /* Update time since this is much used by the parsing functions */
    if (oonf_clock_update()) {
      return -1;
    }

    if (_scheduler_time_limit > 0) {
      stop_time = _scheduler_time_limit;
    }
    else {
      stop_time = ~0ull;
    }

    if (oonf_clock_getNow() >= stop_time) {
      return -1;
    }

    oonf_timer_walk();

    if (!_scheduler_time_limit && oonf_main_shall_stop_scheduler()) {
      return 0;
    }

    /* no event left for now, prepare for select () */
    fd_read = false;
    fd_write = false;

    FD_ZERO(&ibits);
    FD_ZERO(&obits);

    /* Adding file-descriptors to FD set */
    list_for_each_element_safe(&_socket_head, entry, _node, iterator) {
      if (entry->process == NULL) {
        continue;
      }

      if (entry->event_read) {
        fd_read = true;
        FD_SET((unsigned int)entry->fd, &ibits);        /* And we cast here since we get a warning on Win32 */
      }
      if (entry->event_write) {
        fd_write = true;
        FD_SET((unsigned int)entry->fd, &obits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->event_read || entry->event_write) != 0 && entry->fd >= hfd) {
        hfd = entry->fd + 1;
      }
    }

    next_event = oonf_timer_getNextEvent();
    if (next_event > stop_time) {
      next_event = stop_time;
    }

    if (next_event == ~0ull) {
      /* no events waiting */
      tv_ptr = NULL;
    }
    else {
      /* convert time interval until event triggers */
      next_event = oonf_clock_get_relative(next_event);

      tv_ptr = &tv;
      tv.tv_sec = (time_t)(next_event / 1000ull);
      tv.tv_usec = (int)(next_event % 1000) * 1000;
    }

    do {
      if (!_scheduler_time_limit && oonf_main_shall_stop_scheduler()) {
        return 0;
      }
      n = os_socket_select(hfd,
          fd_read ? &ibits : NULL,
          fd_write ? &obits : NULL,
          NULL, tv_ptr);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {               /* timeout! */
      return 0;
    }
    if (n < 0) {              /* Did something go wrong? */
      OONF_WARN(LOG_SOCKET, "select error: %s (%d)", strerror(errno), errno);
      return -1;
    }

    /* Update time since this is much used by the parsing functions */
    if (oonf_clock_update()) {
      return -1;
    }
    list_for_each_element_safe(&_socket_head, entry, _node, iterator) {
      if (entry->process == NULL) {
        continue;
      }

      fd_read = FD_ISSET(entry->fd, &ibits) != 0;
      fd_write = FD_ISSET(entry->fd, &obits) != 0;
      if (fd_read || fd_write) {
        uint64_t start_time, end_time;

        OONF_DEBUG(LOG_SOCKET, "Socket %d triggered (read=%s, write=%s)",
            entry->fd, fd_read ? "true" : "false", fd_write ? "true" : "false");

        os_clock_gettime64(&start_time);
        entry->process(entry->fd, entry->data, fd_read, fd_write);
        os_clock_gettime64(&end_time);

        if (end_time - start_time > OONF_TIMER_SLICE) {
          OONF_WARN(LOG_SOCKET, "Socket %d scheduling took %"PRIu64" ms",
              entry->fd, end_time - start_time);
        }
      }
    }
  }
  return 0;
}
