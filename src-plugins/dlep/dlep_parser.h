
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

#ifndef DLEP_PARSER_H_
#define DLEP_PARSER_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_tlvmap.h"

struct dlep_parser_index {
  uint16_t idx[DLEP_TLV_COUNT];
};

int dlep_parser_read(struct dlep_parser_index *idx,
    uint8_t *signal, size_t len);
int dlep_parser_check_mandatory_tlvs(struct dlep_parser_index *idx,
    struct dlep_tlvmap *mandatory);
uint16_t dlep_parser_get_next_tlv(uint8_t *buffer, size_t len, size_t offset);

void dlep_parser_get_dlep_port(uint16_t *port, uint8_t *tlv);
void dlep_parser_get_peer_type(char *string, uint8_t *tlv);
void dlep_parser_get_heartbeat_interval(uint64_t *interval, uint8_t *tlv);
void dlep_parser_get_mac_addr(struct netaddr *mac, uint8_t *tlv);
int dlep_parser_get_ipv4_addr(struct netaddr *ipv4, bool *add, uint8_t *tlv);
int dlep_parser_get_ipv6_addr(struct netaddr *ipv6, bool *add, uint8_t *tlv);

#endif /* DLEP_PARSER_H_ */
