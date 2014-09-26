
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

#include <arpa/inet.h>
#include <string.h>

#include "common/common_types.h"
#include "common/netaddr.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_parser.h"
#include "dlep/dlep_tlvdata.h"
#include "dlep/dlep_tlvmap.h"

static int _check_tlv_length(uint8_t type, uint8_t length);
static int _check_mandatory_tlvs(struct dlep_parser_index *idx,
    uint8_t signal);

int
dlep_parser_read(struct dlep_parser_index *idx,
    uint8_t *signal, size_t len) {
  uint8_t tlv_type, tlv_length, signal_type;
  uint16_t signal_length;
  size_t pos;

  if (len < 3) {
    /* signal header not complete */
    return -1;
  }

  /* get signal length */
  memcpy(&signal_length, &signal[1], 2);
  signal_length = ntohs(signal_length);
  pos = 0;

  if (len - 3 < signal_length) {
    /* signal not complete */
    return -2;
  }

  /* get signal type */
  signal_type = signal[0];

  /* prepare index */
  memset(idx, 0, sizeof(*idx));

  /* jump to first TLV */
  pos += 3;

  while (pos < signal_length) {
    if (pos + 2 < signal_length) {
      /* tlv header broken */
      return -3;
    }

    /* get tlv header */
    tlv_type = signal[pos];
    tlv_length = signal[pos+1];

    if (pos + 2 + tlv_length < signal_length) {
      /* tlv too long */
      return -4;
    }

    if (_check_tlv_length(tlv_type, tlv_length)) {
      /* length of TLV is incorrect */
      return -5;
    }

    /* remember index of first tlv */
    if (tlv_type < DLEP_TLV_COUNT && idx->idx[tlv_type] == 0) {
      idx->idx[tlv_type] = pos;
    }

    /* jump to next signal */
    pos += tlv_length + 2;
  }

  if (_check_mandatory_tlvs(idx, signal_type)) {
    /* mandatory TLV is missing */
    return -6;
  }
  return 0;
}

uint16_t
dlep_parser_get_next_tlv(uint8_t *buffer, size_t len, size_t offset) {
  uint8_t type;

  /* remember TLV type */
  type = buffer[offset];

  /* skip current tlv */
  offset += buffer[offset + 1];

  /* look for another one of the same type */
  while (offset < len) {
    if (buffer[offset] == type) {
      return offset;
    }

    offset += buffer[offset + 1];
  }
  return 0;
}

void
dlep_parser_get_dlep_port(uint16_t *port, uint8_t *tlv) {
  uint16_t tmp;
  memcpy(&tmp, &tlv[2], 2);
  *port = ntohs(tmp);
}

void
dlep_parser_get_peer_type(char *string, uint8_t *tlv) {
  memcpy(string, &tlv[2], tlv[1]);
  string[tlv[2]] = 0;
}

void
dlep_parser_get_heartbeat_interval(uint64_t *interval, uint8_t *tlv) {
  uint16_t tmp;
  memcpy(&tmp, &tlv[2], tlv[1]);

  *interval = ntohs(tmp) * 1000;
}

void
dlep_parser_get_mac_addr(struct netaddr *mac, uint8_t *tlv) {
  /* length was already checked */
  netaddr_from_binary(mac, &tlv[2], tlv[1], AF_MAC48);
}

int
dlep_parser_get_ipv4_addr(struct netaddr *ipv4, bool *add, uint8_t *tlv) {
  if (add) {
    switch (tlv[2]) {
      case DLEP_IP_ADD:
        *add = true;
        break;
      case DLEP_IP_REMOVE:
        *add = false;
        break;
      default:
        /* bad indicator field */
        return -1;
    }
  }
  /* length was already checked */
  netaddr_from_binary(ipv4, &tlv[3], tlv[1]-1, AF_INET);
  return 0;
}

int
dlep_parser_get_ipv6_addr(struct netaddr *ipv6, bool *add, uint8_t *tlv) {
  if (add) {
    switch (tlv[2]) {
      case DLEP_IP_ADD:
        *add = true;
        break;
      case DLEP_IP_REMOVE:
        *add = false;
        break;
      default:
        /* bad indicator field */
        return -1;
    }
  }

  /* length was already checked */
  netaddr_from_binary(ipv6, &tlv[3], tlv[1]-1, AF_INET6);
  return 0;
}

static int
_check_tlv_length(uint8_t type, uint8_t length) {
  if (type >= DLEP_TLV_COUNT) {
    /* unsupported custom TLV, no check necessary */
    return 0;
  }

  /* check length */
  if (length < dlep_tlv_constraints[type].min_length) {
    return -1;
  }
  if (length > dlep_tlv_constraints[type].min_length) {
    return -1;
  }
  return 0;
}

static int
_check_mandatory_tlvs(struct dlep_parser_index *idx,
    uint8_t signal) {
  struct dlep_tlvmap *mandatory;
  int i;

  if (signal >= DLEP_SIGNAL_COUNT) {
    /* unsupported custom signal, no check necessary */
    return 0;
  }

  mandatory = &dlep_mandatory_tlvs[signal];
  for (i=0; i<DLEP_TLV_COUNT; i++) {
    if (dlep_tlvmap_get(mandatory, i)
        && idx->idx[i] == 0) {
      return -1;
    }
  }
  return 0;
}
