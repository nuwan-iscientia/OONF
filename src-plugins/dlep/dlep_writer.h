
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

#ifndef DLEP_WRITER_H_
#define DLEP_WRITER_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"

#include "core/oonf_logging.h"

int dlep_writer_init(void);
void dlep_writer_cleanup(void);

void dlep_writer_start_signal(uint8_t signal);
void dlep_writer_add_tlv(uint8_t type, void *data,
    uint8_t len);
int dlep_writer_finish_signal(enum oonf_log_source);

void dlep_writer_send_udp_multicast(struct oonf_packet_managed *managed,
    enum oonf_log_source source);
void dlep_writer_send_udp_unicast(struct oonf_packet_managed *managed,
    union netaddr_socket *dst, enum oonf_log_source source);
void dlep_writer_send_tcp_unicast(struct oonf_stream_session *session);

void dlep_writer_add_heartbeat_tlv(uint64_t interval);
void dlep_writer_add_ipv4_tlv(struct netaddr *, bool add);
void dlep_writer_add_ipv6_tlv(struct netaddr *, bool add);
void dlep_writer_add_port_tlv(uint16_t);

#endif /* DLEP_WRITER_H_ */
