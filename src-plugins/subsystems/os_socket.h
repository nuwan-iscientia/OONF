
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
 * @file
 */

#ifndef OS_SOCKET_H_
#define OS_SOCKET_H_

#include <unistd.h>
#include <sys/select.h>

#include "common/avl.h"
#include "common/common_types.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_interface.h"

/*! subsystem identifier */
#define OONF_OS_SOCKET_SUBSYSTEM "os_socket"

/* pre-definition of structs */
struct os_socket;
struct os_socket_select;

/* pre-declare inlines */
static INLINE int os_socket_init(struct os_socket *, int fd);
static INLINE int os_socket_copy(struct os_socket *dst, struct os_socket *from);
static INLINE int os_socket_get_fd(struct os_socket *);
static INLINE int os_socket_invalidate(struct os_socket *);
static INLINE bool os_socket_is_initialized(struct os_socket *);

static INLINE int os_socket_bindto_interface(struct os_socket *, struct os_interface_data *data);
static INLINE int os_socket_close(struct os_socket *);
static INLINE int os_socket_listen(struct os_socket *, int n);

static INLINE int os_socket_event_add(struct os_socket_select *);
static INLINE int os_socket_event_socket_add(struct os_socket_select *, struct os_socket *);
static INLINE int os_socket_event_socket_read(struct os_socket_select *,
    struct os_socket *, bool want_read);
static INLINE int os_socket_event_is_read(struct os_socket *);
static INLINE int os_socket_event_socket_write(struct os_socket_select *,
    struct os_socket *, bool want_write);
static INLINE int os_socket_event_is_write(struct os_socket *);
static INLINE int os_socket_event_socket_remove(struct os_socket_select *, struct os_socket *);
static INLINE int os_socket_event_set_deadline(struct os_socket_select *, uint64_t deadline);
static INLINE uint64_t os_socket_event_get_deadline(struct os_socket_select *);
static INLINE int os_socket_event_wait(struct os_socket_select *);
static INLINE struct os_socket *os_socket_event_get(struct os_socket_select *, int idx);
static INLINE int os_socket_event_remove(struct os_socket_select *);

static INLINE int os_socket_connect(struct os_socket *, const union netaddr_socket *remote);
static INLINE int os_socket_accept(struct os_socket *client,
    struct os_socket *server, union netaddr_socket *incoming);
static INLINE int os_socket_get_socket_error(struct os_socket *, int *value);
static INLINE ssize_t os_socket_sendto(struct os_socket *, const void *buf, size_t length,
    const union netaddr_socket *dst, bool dont_route);
static INLINE ssize_t os_socket_recvfrom(struct os_socket *, void *buf, size_t length,
    union netaddr_socket *source, const struct os_interface_data *interf);
static INLINE const char *os_socket_get_loopback_name(void);
static INLINE ssize_t os_socket_sendfile(struct os_socket *, struct os_socket *,
    size_t offset, size_t count);

static INLINE int os_socket_getsocket(struct os_socket *, const union netaddr_socket *bindto, bool tcp,
    size_t recvbuf, const struct os_interface_data *, enum oonf_log_source log_src);
static INLINE int os_socket_getrawsocket(struct os_socket *, const union netaddr_socket *bindto, int protocol,
    size_t recvbuf, const struct os_interface_data *, enum oonf_log_source log_src);
static INLINE int os_socket_configsocket(struct os_socket *, const union netaddr_socket *bindto,
    size_t recvbuf, bool rawip, const struct os_interface_data *, enum oonf_log_source log_src);
static INLINE int os_socket_set_nonblocking(struct os_socket *);
static INLINE int os_socket_join_mcast_recv(struct os_socket *, const struct netaddr *multicast,
    const struct os_interface_data *oif, enum oonf_log_source log_src);
static INLINE int os_socket_join_mcast_send(struct os_socket *, const struct netaddr *multicast,
    const struct os_interface_data *oif, bool loop, enum oonf_log_source log_src);
static INLINE int os_socket_set_dscp(struct os_socket *, int dscp, bool ipv6);
static INLINE uint8_t *os_socket_skip_rawsocket_prefix(uint8_t *ptr, ssize_t *len, int af_type);

/* include os-specific headers */
#if defined(__linux__)
#include "os_linux/os_socket_linux.h"
#elif defined (BSD)
#include "subsystems/os_bsd/os_socket_bsd.h"
#elif defined (_WIN32)
#include "subsystems/os_win32/os_socket_win32.h"
#else
#error "Unknown operation system"
#endif

#endif /* OS_SOCKET_H_ */
