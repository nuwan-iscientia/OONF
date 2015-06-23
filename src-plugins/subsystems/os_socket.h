
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

#define OONF_OS_SOCKET_SUBSYSTEM "os_socket"

/* pre-declare inlines */
static INLINE int os_socket_bindto_interface(int, struct os_interface_data *data);
static INLINE int os_socket_close(int fd);
static INLINE int os_socket_listen(int fd, int n);
static INLINE int os_socket_select(
    int num, fd_set *r,fd_set *w,fd_set *e, struct timeval *timeout);
static INLINE int os_socket_connect(int sockfd, const union netaddr_socket *remote);
static INLINE int os_socket_accept(int sockfd, union netaddr_socket *incoming);
static INLINE int os_socket_get_socket_error(int sockfd, int *value);
static INLINE ssize_t os_socket_sendto(int fd, const void *buf, size_t length,
    const union netaddr_socket *dst, bool dont_route);
static INLINE ssize_t os_socket_recvfrom(int fd, void *buf, size_t length,
    union netaddr_socket *source, const struct os_interface_data *interf);
static INLINE const char *os_socket_get_loopback_name(void);
static INLINE ssize_t os_socket_sendfile(int outfd, int infd, size_t offset, size_t count);

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

/* prototypes for all os_net functions */
EXPORT int os_socket_getsocket(const union netaddr_socket *bindto, bool tcp,
    size_t recvbuf, const struct os_interface_data *, enum oonf_log_source log_src);
EXPORT int os_socket_getrawsocket(const union netaddr_socket *bindto, int protocol,
    size_t recvbuf, const struct os_interface_data *, enum oonf_log_source log_src);
EXPORT int os_socket_configsocket(int sock, const union netaddr_socket *bindto,
    size_t recvbuf, bool rawip, const struct os_interface_data *, enum oonf_log_source log_src);
EXPORT int os_socket_set_nonblocking(int sock);
EXPORT int os_socket_join_mcast_recv(int sock, const struct netaddr *multicast,
    const struct os_interface_data *oif, enum oonf_log_source log_src);
EXPORT int os_socket_join_mcast_send(int sock, const struct netaddr *multicast,
    const struct os_interface_data *oif, bool loop, enum oonf_log_source log_src);
EXPORT int os_socket_set_dscp(int sock, int dscp, bool ipv6);
EXPORT uint8_t *os_socket_skip_rawsocket_prefix(uint8_t *ptr, ssize_t *len, int af_type);

#endif /* OS_SOCKET_H_ */
