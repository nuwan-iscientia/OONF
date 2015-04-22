
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

#ifndef DLEP_PARSER_H_
#define DLEP_PARSER_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"

#include "dlep/dlep_bitmap.h"
#include "dlep/dlep_iana.h"

struct dlep_parser_index {
  uint16_t idx[DLEP_TLV_COUNT];
};

enum dlep_parser_errors {
  DLEP_PARSER_INCOMPLETE_HEADER     = -1,
  DLEP_PARSER_INCOMPLETE_SIGNAL     = -2,
  DLEP_PARSER_INCOMPLETE_TLV_HEADER = -3,
  DLEP_PARSER_INCOMPLETE_TLV        = -4,
  DLEP_PARSER_ILLEGAL_TLV_LENGTH    = -5,
  DLEP_PARSER_MISSING_MANDATORY_TLV = -6,
};

int dlep_parser_read(struct dlep_parser_index *idx,
    const void *signal, size_t len, uint16_t *siglen);
int dlep_parser_check_mandatory_tlvs(const struct dlep_parser_index *idx,
    const struct dlep_bitmap *mandatory);
uint16_t dlep_parser_get_next_tlv(const uint8_t *buffer, size_t len, size_t offset);

void dlep_parser_get_version(uint16_t *major, uint16_t *minor, const uint8_t *tlv);
void dlep_parser_get_peer_type(char *string, const uint8_t *tlv);
void dlep_parser_get_ipv4_conpoint(struct netaddr *ipv4, uint16_t *port, const uint8_t *tlv);
void dlep_parser_get_ipv6_conpoint(struct netaddr *ipv4, uint16_t *port, const uint8_t *tlv);
void dlep_parser_get_heartbeat_interval(uint64_t *interval, const uint8_t *tlv);
void dlep_parser_get_mac_addr(struct netaddr *mac, const uint8_t *tlv);
int dlep_parser_get_ipv4_addr(struct netaddr *ipv4, bool *add, const uint8_t *tlv);
int dlep_parser_get_ipv6_addr(struct netaddr *ipv6, bool *add, const uint8_t *tlv);
void dlep_parser_get_uint64(uint64_t *mdrr, const uint8_t *tlv);
void dlep_parser_get_status(enum dlep_status *status, const uint8_t *tlv);
void dlep_parser_get_extensions_supported(struct dlep_bitmap *bitmap, const uint8_t *tlv);

void dlep_parser_get_tx_signal(int32_t *sig, const uint8_t *tlv);
void dlep_parser_get_rx_signal(int32_t *sig, const uint8_t *tlv);

#endif /* DLEP_PARSER_H_ */
