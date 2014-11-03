
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

#include "dlep/dlep_bitmap.h"
#include "dlep/dlep_iana.h"
#include "dlep/dlep_parser.h"
#include "dlep/dlep_static_data.h"

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h> /* be64toh */


static int _check_tlv_length(uint8_t type, uint8_t length);
static int _check_mandatory_tlvs(struct dlep_parser_index *idx,
    uint8_t signal);
static void _remove_unknown_tlvs(struct dlep_parser_index *idx,
    uint8_t signal);

int
dlep_parser_read(struct dlep_parser_index *idx,
    const void *ptr, size_t total_len, uint16_t *siglen) {
  uint8_t tlv_type, tlv_length, signal_type;
  const uint8_t *signal;
  uint16_t signal_length;
  size_t pos;

  if (total_len < 3) {
    /* signal header not complete */
    return DLEP_PARSER_INCOMPLETE_HEADER;
  }

  /* get byte-wise pointer */
  signal = ptr;

  /* get signal length */
  memcpy(&signal_length, &signal[1], 2);
  signal_length = ntohs(signal_length) + 3;
  pos = 0;

  if (total_len < signal_length) {
    /* signal not complete */
    return DLEP_PARSER_INCOMPLETE_SIGNAL;
  }

  /* get signal type */
  signal_type = signal[0];

  /* store signal length */
  if (siglen) {
    *siglen = signal_length;
  }

  /* prepare index */
  memset(idx, 0, sizeof(*idx));

  /* jump to first TLV */
  pos += 3;

  while (pos < signal_length) {
    if (pos + 2 > signal_length) {
      /* tlv header broken */
      return DLEP_PARSER_INCOMPLETE_TLV_HEADER;
    }

    /* get tlv header */
    tlv_type = signal[pos];
    tlv_length = signal[pos+1];

    if (pos + 2 + tlv_length > signal_length) {
      /* tlv too long */
      return DLEP_PARSER_INCOMPLETE_TLV;
    }

    if (_check_tlv_length(tlv_type, tlv_length)) {
      /* length of TLV is incorrect */
      return DLEP_PARSER_ILLEGAL_TLV_LENGTH;
    }

    /* remember index of first tlv */
    if (tlv_type < DLEP_TLV_COUNT
        && idx->idx[tlv_type] == 0) {
      idx->idx[tlv_type] = pos;
    }

    /* jump to next signal */
    pos += tlv_length + 2;
  }

  if (_check_mandatory_tlvs(idx, signal_type)) {
    /* mandatory TLV is missing */
    return DLEP_PARSER_MISSING_MANDATORY_TLV;
  }

  _remove_unknown_tlvs(idx, signal_type);

  return signal[0];
}

uint16_t
dlep_parser_get_next_tlv(const uint8_t *buffer, size_t len, size_t offset) {
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
dlep_parser_get_dlep_port(uint16_t *port, const uint8_t *tlv) {
  uint16_t tmp;
  memcpy(&tmp, &tlv[2], 2);
  *port = ntohs(tmp);
}

void
dlep_parser_get_peer_type(char *string, const uint8_t *tlv) {
  memcpy(string, &tlv[2], tlv[1]);
  string[tlv[2]] = 0;
}

void
dlep_parser_get_heartbeat_interval(uint64_t *interval, const uint8_t *tlv) {
  uint16_t tmp;
  memcpy(&tmp, &tlv[2], tlv[1]);

  *interval = ntohs(tmp) * 1000;
}

void
dlep_parser_get_mac_addr(struct netaddr *mac, const uint8_t *tlv) {
  /* length was already checked */
  netaddr_from_binary(mac, &tlv[2], tlv[1], AF_MAC48);
}

int
dlep_parser_get_ipv4_addr(struct netaddr *ipv4, bool *add, const uint8_t *tlv) {
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
dlep_parser_get_ipv6_addr(struct netaddr *ipv6, bool *add, const uint8_t *tlv) {
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

void
dlep_parser_get_uint64(uint64_t *number, const uint8_t *tlv) {
  uint64_t value;

  memcpy(&value, &tlv[2], sizeof(value));
  *number= be64toh(value);
}

void
dlep_parser_get_status(enum dlep_status *status, const uint8_t *tlv) {
  if (tlv[2] >= DLEP_STATUS_COUNT) {
    /* normalize status code */
    *status = DLEP_STATUS_ERROR;
  }
  else {
    *status = tlv[2];
  }
}

void
dlep_parser_get_optional_signal(struct dlep_bitmap *bitmap, const uint8_t *tlv) {
  unsigned i;

  memset(bitmap, 0, sizeof(*bitmap));
  for (i=0; i<tlv[1]; i++) {
    dlep_bitmap_set(bitmap, tlv[2+i]);
  }
}

void
dlep_parser_get_optional_tlv(struct dlep_bitmap *bitmap, const uint8_t *tlv) {
  unsigned i;

  memset(bitmap, 0, sizeof(*bitmap));
  for (i=0; i<tlv[1]; i++) {
    dlep_bitmap_set(bitmap, tlv[2+i]);
  }
}

void
dlep_parser_get_tx_signal(int32_t *sig, const uint8_t *tlv) {
  uint32_t value;

  memcpy(&value, &tlv[2], sizeof(value));
  value = ntohl(value);
  memcpy(sig, &value, sizeof(value));
}

void
dlep_parser_get_rx_signal(int32_t *sig, const uint8_t *tlv) {
  uint32_t value;

  memcpy(&value, &tlv[2], sizeof(value));
  value = ntohl(value);
  memcpy(sig, &value, sizeof(value));
}

static int
_check_tlv_length(uint8_t type, uint8_t length) {
  uint8_t min, max;
  if (type >= DLEP_TLV_COUNT) {
    /* unsupported custom TLV, no check necessary */
    return 0;
  }

  /* check length */
  min = dlep_tlv_constraints[type].min_length;
  if (min > 0 && length < min) {
    return -1;
  }
  max = dlep_tlv_constraints[type].max_length;
  if (max < 255 && length > max) {
    return -1;
  }
  return 0;
}

static int
_check_mandatory_tlvs(struct dlep_parser_index *idx,
    uint8_t signal) {
  struct dlep_bitmap *mandatory;
  int i;

  if (signal >= DLEP_SIGNAL_COUNT) {
    /* unsupported custom signal, no check necessary */
    return 0;
  }

  mandatory = &dlep_mandatory_tlvs_per_signal[signal];
  for (i=0; i<DLEP_TLV_COUNT; i++) {
    if (dlep_bitmap_get(mandatory, i)
        && idx->idx[i] == 0) {
      return -1;
    }
  }
  return 0;
}

static void
_remove_unknown_tlvs(struct dlep_parser_index *idx,
    uint8_t signal) {
  struct dlep_bitmap *mandatory, *optional;
  int i;

  if (signal >= DLEP_SIGNAL_COUNT) {
    /* unsupported custom signal */
    memset(idx, 0, sizeof(*idx));

    return;
  }

  mandatory = &dlep_mandatory_tlvs_per_signal[signal];
  optional = &dlep_supported_optional_tlvs_per_signal[signal];

  for (i=0; i<DLEP_TLV_COUNT; i++) {
    if (!dlep_bitmap_get(mandatory, i)
        && !dlep_bitmap_get(optional, i)) {
      idx->idx[i] = 0;
    }
  }
  return;
}
