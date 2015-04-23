
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
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_stream_socket.h"

#include "dlep/dlep_bitmap.h"
#include "dlep/dlep_iana.h"
#include "dlep/dlep_static_data.h"
#include "dlep/dlep_writer.h"

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#include <endian.h> /* htobe64 */

static struct autobuf _signal_buf;
static uint8_t _signal_id;

int
dlep_writer_init(void) {
  return abuf_init(&_signal_buf);
}

void
dlep_writer_cleanup(void) {
  abuf_free(&_signal_buf);
}

void
dlep_writer_start_signal(uint8_t signal) {
  _signal_id = signal;
  abuf_clear(&_signal_buf);
  abuf_append_uint8(&_signal_buf, signal);
  abuf_append_uint16(&_signal_buf, 0);
}

void
dlep_writer_add_tlv(uint8_t type, void *data, uint8_t len) {
  if (dlep_bitmap_get(&dlep_mandatory_tlvs_per_signal[_signal_id], type)
      || dlep_bitmap_get(&dlep_supported_optional_tlvs_per_signal[_signal_id], type)) {
    abuf_append_uint8(&_signal_buf, type);
    abuf_append_uint8(&_signal_buf, len);
    abuf_memcpy(&_signal_buf, data, len);
  }
}

int
dlep_writer_finish_signal(enum oonf_log_source source) {
  uint16_t len;
  char *ptr;

  if (abuf_has_failed(&_signal_buf)) {
    OONF_WARN(source, "Could not build signal: %u",
        abuf_getptr(&_signal_buf)[0]);
    return -1;
  }

  if (abuf_getlen(&_signal_buf) > 65535 - 3) {
    OONF_WARN(source, "Signal %u became too long: %" PRINTF_SIZE_T_SPECIFIER,
        abuf_getptr(&_signal_buf)[0], abuf_getlen(&_signal_buf));
    return -1;
  }

  /* calculate network ordered size */
  len = htons(abuf_getlen(&_signal_buf)-3);

  /* put it into the signal */
  ptr = abuf_getptr(&_signal_buf);
  memcpy(&ptr[1], &len, sizeof(len));

  return 0;
}

void
dlep_writer_send_udp_multicast(struct oonf_packet_managed *managed,
    enum oonf_log_source source) {
  OONF_DEBUG_HEX(source, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf),
      "Send signal via UDP multicast");

  if (oonf_packet_send_managed_multicast(
      managed, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf), AF_INET)) {
    OONF_WARN(source, "Could not send ipv4 multicast signal");
  }
  if (oonf_packet_send_managed_multicast(
      managed, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf), AF_INET6)) {
    OONF_WARN(source, "Could not send ipv6 multicast signal");
  }
}

void
dlep_writer_send_udp_unicast(struct oonf_packet_managed *managed,
    union netaddr_socket *dst, enum oonf_log_source source) {
  struct netaddr_str nbuf;

  OONF_DEBUG_HEX(source, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf),
      "Send signal via udp unicast");

  if (oonf_packet_send_managed(
      managed, dst, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf))) {
    OONF_WARN(source, "Could not send udp unicast to %s",
        netaddr_socket_to_string(&nbuf, dst));
  }
}

void
dlep_writer_send_tcp_unicast(struct oonf_stream_session *session, enum oonf_log_source source) {
  OONF_DEBUG_HEX(source, abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf),
      "Send signal via TCP");
  abuf_memcpy(&session->out,
      abuf_getptr(&_signal_buf), abuf_getlen(&_signal_buf));
  oonf_stream_flush(session);
}

void
dlep_writer_add_version_tlv(uint16_t major, uint16_t minor) {
  uint16_t value[2];

  value[0] = htons(major);
  value[1] = htons(minor);

  dlep_writer_add_tlv(DLEP_VERSION_TLV, &value, sizeof(value));
}

void
dlep_writer_add_heartbeat_tlv(uint64_t interval) {
  uint16_t value;

  value = htons(interval / 1000);

  dlep_writer_add_tlv(DLEP_HEARTBEAT_INTERVAL_TLV, &value, sizeof(value));
}

int
dlep_writer_add_mac_tlv(const struct netaddr *mac) {
  uint8_t value[6];

  if (netaddr_get_address_family(mac) != AF_MAC48) {
    return -1;
  }

  netaddr_to_binary(value, mac, 6);

  dlep_writer_add_tlv(DLEP_MAC_ADDRESS_TLV, value, sizeof(value));
  return 0;
}

int
dlep_writer_add_ipv4_tlv(const struct netaddr *ipv4, bool add) {
  uint8_t value[5];

  if (netaddr_get_address_family(ipv4) != AF_INET) {
    return -1;
  }

  value[0] = add ? DLEP_IP_ADD : DLEP_IP_REMOVE;
  netaddr_to_binary(&value[1], ipv4, 4);

  dlep_writer_add_tlv(DLEP_IPV4_ADDRESS_TLV, value, sizeof(value));
  return 0;
}

int
dlep_writer_add_ipv6_tlv(const struct netaddr *ipv6, bool add) {
  uint8_t value[17];

  if (netaddr_get_address_family(ipv6) != AF_INET6) {
    return -1;
  }

  value[0] = add ? DLEP_IP_ADD : DLEP_IP_REMOVE;
  netaddr_to_binary(&value[1], ipv6, 16);

  dlep_writer_add_tlv(DLEP_IPV6_ADDRESS_TLV, value, sizeof(value));
  return 0;
}

void
dlep_writer_add_ipv4_conpoint_tlv(const struct netaddr *addr, uint16_t port) {
  uint8_t value[6];

  if (netaddr_get_address_family(addr) != AF_INET) {
    return;
  }

  /* convert port to network byte order */
  port = htons(port);

  /* copy data into value buffer */
  netaddr_to_binary(&value[0], addr, sizeof(value));
  memcpy(&value[4], &port, sizeof(port));

  dlep_writer_add_tlv(DLEP_IPV4_CONPOINT_TLV, &value, sizeof(value));
}

void
dlep_writer_add_ipv6_conpoint_tlv(const struct netaddr *addr, uint16_t port) {
  uint8_t value[18];

  if (netaddr_get_address_family(addr) != AF_INET6) {
    return;
  }

  /* convert port to network byte order */
  port = htons(port);

  /* copy data into value buffer */
  netaddr_to_binary(&value[0], addr, sizeof(value));
  memcpy(&value[16], &port, sizeof(port));

  dlep_writer_add_tlv(DLEP_IPV6_CONPOINT_TLV, &value, sizeof(value));
}

void
dlep_writer_add_latency(uint32_t latency) {
  latency = htonl(latency);

  dlep_writer_add_tlv(DLEP_LATENCY_TLV, &latency, sizeof(latency));
}

void
dlep_writer_add_uint64(uint64_t number, enum dlep_tlvs tlv) {
  uint64_t value;

  value = be64toh(number);

  dlep_writer_add_tlv(tlv, &value, sizeof(value));
}

void
dlep_writer_add_status(enum dlep_status status) {
  uint8_t value = status;

  dlep_writer_add_tlv(DLEP_STATUS_TLV, &value, sizeof(value));
}

void
dlep_writer_add_extensions_supported(void) {
  uint8_t value[DLEP_SIGNAL_COUNT];
  size_t i,j;

  for (i=0,j=0; i<DLEP_SIGNAL_COUNT; i++) {
    if (dlep_bitmap_get(&dlep_supported_optional_signals, i)) {
      value[j++] = i;
    }
  }

  dlep_writer_add_tlv(DLEP_EXTENSIONS_SUPPORTED, &value[0], j);
}

void
dlep_writer_add_rx_signal(int32_t signal) {
  uint32_t *value = (uint32_t*)(&signal);

  *value = htonl(*value);

  dlep_writer_add_tlv(DLEP_RX_SIGNAL_TLV, value, sizeof(*value));
}

void
dlep_writer_add_tx_signal(int32_t signal) {
  uint32_t *value = (uint32_t*)(&signal);

  *value = htonl(*value);

  dlep_writer_add_tlv(DLEP_TX_SIGNAL_TLV, value, sizeof(*value));
}
