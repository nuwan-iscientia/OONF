
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

#include <arpa/inet.h>

#include "common/common_types.h"
#include "common/autobuf.h"

#include "core/oonf_logging.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_iana.h"
#include "dlep/dlep_writer.h"

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h> /* htobe64 */

void
dlep_writer_start_signal(struct dlep_writer *writer, uint16_t signal_type) {

  writer->signal_type = signal_type;
  writer->signal_start_ptr =
      abuf_getptr(writer->out) + abuf_getlen(writer->out);

  abuf_append_uint16(writer->out, htons(signal_type));
  abuf_append_uint16(writer->out, 0);
}

void
dlep_writer_add_tlv(struct dlep_writer *writer,
    uint16_t type, const void *data, uint16_t len) {
  abuf_append_uint16(writer->out, htons(type));
  abuf_append_uint16(writer->out, htons(len));
  abuf_memcpy(writer->out, data, len);
}

void
dlep_writer_add_tlv2(struct dlep_writer *writer,
    uint16_t type, const void *data1, uint16_t len1,
    const void *data2, uint16_t len2) {
  abuf_append_uint16(writer->out, htons(type));
  abuf_append_uint16(writer->out, htons(len1 + len2));
  abuf_memcpy(writer->out, data1, len1);
  abuf_memcpy(writer->out, data2, len2);
}

int
dlep_writer_finish_signal(struct dlep_writer *writer,
    enum oonf_log_source source) {
  size_t length;
  uint16_t buffer;

  if (abuf_has_failed(writer->out)) {
    OONF_WARN(source, "Could not build signal: %u",
        writer->signal_type);
    return -1;
  }

  length = (abuf_getptr(writer->out) + abuf_getlen(writer->out))
      - writer->signal_start_ptr;
  if (length > 65535 + 4) {
    OONF_WARN(source, "Signal %u became too long: %" PRINTF_SIZE_T_SPECIFIER,
        writer->signal_type, abuf_getlen(writer->out));
    return -1;
  }

  /* calculate network ordered size */
  buffer = htons(length - 4);

  /* put it into the signal */
  memcpy(&writer->signal_start_ptr[2], &buffer, sizeof(buffer));

  return 0;
}

void
dlep_writer_add_heartbeat_tlv(struct dlep_writer *writer, uint64_t interval) {
  uint16_t value;

  value = htons(interval / 1000);

  dlep_writer_add_tlv(writer, DLEP_HEARTBEAT_INTERVAL_TLV,
      &value, sizeof(value));
}

void
dlep_writer_add_peer_type_tlv(struct dlep_writer *writer,
    const char *peer_type) {
  dlep_writer_add_tlv(writer, DLEP_PEER_TYPE_TLV,
      peer_type, strlen(peer_type));
}

int
dlep_writer_add_mac_tlv(struct dlep_writer *writer,
    const struct netaddr *mac) {
  uint8_t value[8];

  switch (netaddr_get_address_family(mac)) {
    case AF_MAC48:
    case AF_EUI64:
      break;
    default:
      return -1;
  }

  netaddr_to_binary(value, mac, 8);

  dlep_writer_add_tlv(writer,
      DLEP_MAC_ADDRESS_TLV, value, netaddr_get_binlength(mac));
  return 0;
}

int
dlep_writer_add_ipv4_tlv(struct dlep_writer *writer,
    const struct netaddr *ipv4, bool add) {
  uint8_t value[5];

  if (netaddr_get_address_family(ipv4) != AF_INET) {
    return -1;
  }

  value[0] = add ? DLEP_IP_ADD : DLEP_IP_REMOVE;
  netaddr_to_binary(&value[1], ipv4, 4);

  dlep_writer_add_tlv(writer,
      DLEP_IPV4_ADDRESS_TLV, value, sizeof(value));
  return 0;
}

int
dlep_writer_add_ipv6_tlv(struct dlep_writer *writer,
    const struct netaddr *ipv6, bool add) {
  uint8_t value[17];

  if (netaddr_get_address_family(ipv6) != AF_INET6) {
    return -1;
  }

  value[0] = add ? DLEP_IP_ADD : DLEP_IP_REMOVE;
  netaddr_to_binary(&value[1], ipv6, 16);

  dlep_writer_add_tlv(writer,
      DLEP_IPV6_ADDRESS_TLV, value, sizeof(value));
  return 0;
}

void
dlep_writer_add_ipv4_conpoint_tlv(struct dlep_writer *writer,
    const struct netaddr *addr, uint16_t port) {
  uint8_t value[6];

  if (netaddr_get_address_family(addr) != AF_INET) {
    return;
  }

  /* convert port to network byte order */
  port = htons(port);

  /* copy data into value buffer */
  netaddr_to_binary(&value[0], addr, sizeof(value));
  memcpy(&value[4], &port, sizeof(port));

  dlep_writer_add_tlv(writer,
      DLEP_IPV4_CONPOINT_TLV, &value, sizeof(value));
}

void
dlep_writer_add_ipv6_conpoint_tlv(struct dlep_writer *writer,
    const struct netaddr *addr, uint16_t port) {
  uint8_t value[18];

  if (netaddr_get_address_family(addr) != AF_INET6) {
    return;
  }

  /* convert port to network byte order */
  port = htons(port);

  /* copy data into value buffer */
  netaddr_to_binary(&value[0], addr, sizeof(value));
  memcpy(&value[16], &port, sizeof(port));

  dlep_writer_add_tlv(writer,
      DLEP_IPV6_CONPOINT_TLV, &value, sizeof(value));
}

void
dlep_writer_add_uint64(struct dlep_writer *writer,
    uint64_t number, enum dlep_tlvs tlv) {
  uint64_t value;

  value = be64toh(number);

  dlep_writer_add_tlv(writer, tlv, &value, sizeof(value));
}

void
dlep_writer_add_int64(struct dlep_writer *writer,
    int64_t number, enum dlep_tlvs tlv) {
  uint64_t *value = (uint64_t*)(&number);

  *value = htonl(*value);

  dlep_writer_add_tlv(writer, tlv, value, sizeof(*value));
}

int
dlep_writer_add_status(struct dlep_writer *writer,
    enum dlep_status status, const char *text) {
  uint8_t value;
  size_t txtlen;

  value = status;
  txtlen = strlen(text);
  if (txtlen > 65534) {
    return -1;
  }

  dlep_writer_add_tlv2(writer, DLEP_STATUS_TLV,
      &value, sizeof(value), text, txtlen);
  return 0;
}

void
dlep_writer_add_supported_extensions(struct dlep_writer *writer,
    const uint16_t *extensions, uint16_t ext_count) {
  dlep_writer_add_tlv(writer, DLEP_EXTENSIONS_SUPPORTED_TLV,
      extensions, ext_count * 2);
}

int
dlep_writer_map_identity(struct dlep_writer *writer,
    struct oonf_layer2_data *data, uint16_t tlv, uint16_t length) {
  int64_t l2value;
  uint64_t tmp64;
  uint32_t tmp32;
  uint16_t tmp16;
  uint8_t tmp8;
  void *value;

  l2value = oonf_layer2_get_value(data);
  memcpy(&tmp64, &l2value, 8);

  switch (length) {
    case 8:
      tmp64 = htobe64(tmp64);
      value = &tmp64;
      break;
    case 4:
      tmp32 = htonl(tmp64);
      value = &tmp32;
      break;
    case 2:
      tmp16 = htons(tmp64);
      value = &tmp16;
      break;
    case 1:
      tmp8 = tmp64;
      value = &tmp8;
      break;
    default:
      return -1;
  }

  dlep_writer_add_tlv(writer, tlv, value, length);
  return 0;
}

int
dlep_writer_map_l2neigh_data(struct dlep_writer *writer,
    struct dlep_extension *ext, struct oonf_layer2_data *data) {
  struct dlep_neighbor_mapping *map;
  size_t i;

  for (i=0; i<ext->neigh_mapping_count; i++) {
    map = &ext->neigh_mapping[i];

    if (map->to_tlv(writer, &data[map->layer2],
        map->dlep, map->length)) {
      return -1;
    }
  }
  return 0;
}

int
dlep_writer_map_l2net_data(struct dlep_writer *writer,
    struct dlep_extension *ext, struct oonf_layer2_data *data) {
  struct dlep_network_mapping *map;
  size_t i;

  for (i=0; i<ext->if_mapping_count; i++) {
    map = &ext->if_mapping[i];

    if (map->to_tlv(writer, &data[map->layer2],
        map->dlep, map->length)) {
      return -1;
    }
  }
  return 0;
}
