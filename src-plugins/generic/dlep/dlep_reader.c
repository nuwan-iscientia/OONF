/*
 * dlep_reader.c
 *
 *  Created on: Jun 30, 2015
 *      Author: rogge
 */

#include "common/common_types.h"
#include "common/netaddr.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_session.h"
#include "dlep/dlep_reader.h"

int
dlep_reader_heartbeat_tlv(uint64_t *interval,
    struct dlep_session *session, struct dlep_parser_value *value) {
  uint16_t tmp;
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_HEARTBEAT_INTERVAL_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  memcpy(&tmp, ptr, sizeof(tmp));
  *interval = 1000ull * ntohs(tmp);
  return 0;
}

int
dlep_reader_peer_type(char *text, size_t text_length,
    struct dlep_session *session, struct dlep_parser_value *value) {
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_PEER_TYPE_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);

  if (value->length > 0 && text_length > 0) {
    /* generate a 0 terminated copy of the text */
    if (text_length > value->length) {
      memcpy(text, ptr, value->length);
      text[value->length] = 0;
    }
    else {
      memcpy(text, ptr, text_length-1);
      text[text_length-1] = 0;
    }
  }
  return 0;
}

int
dlep_reader_mac_tlv(struct netaddr *mac,
    struct dlep_session *session, struct dlep_parser_value *value) {
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_MAC_ADDRESS_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  return netaddr_from_binary(mac, ptr, value->length, 0);
}

int
dlep_reader_ipv4_tlv(struct netaddr *ipv4, bool *add,
    struct dlep_session *session, struct dlep_parser_value *value) {
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_IPV4_ADDRESS_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  switch (ptr[0]) {
    case DLEP_IP_ADD:
      *add = true;
      break;
    case DLEP_IP_REMOVE:
      *add = false;
      break;
    default:
      return -1;
  }
  return netaddr_from_binary(ipv4, &ptr[1], 4, AF_INET);
}

int
dlep_reader_ipv6_tlv(struct netaddr *ipv6, bool *add,
    struct dlep_session *session, struct dlep_parser_value *value) {
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_IPV6_ADDRESS_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  switch (ptr[0]) {
    case DLEP_IP_ADD:
      *add = true;
      break;
    case DLEP_IP_REMOVE:
      *add = false;
      break;
    default:
      return -1;
  }
  return netaddr_from_binary(ipv6, &ptr[1], 16, AF_INET6);
}

int
dlep_reader_ipv4_conpoint_tlv(struct netaddr *addr, uint16_t *port,
    struct dlep_session *session, struct dlep_parser_value *value) {
  uint16_t tmp;
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_IPV4_CONPOINT_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  if (value->length == 6) {
    memcpy(&tmp, &ptr[4], sizeof(tmp));
    *port = ntohs(tmp);
  }
  else {
    *port = DLEP_PORT;
  }

  return netaddr_from_binary(addr, ptr, 4, AF_INET);
}

int
dlep_reader_ipv6_conpoint_tlv(struct netaddr *addr, uint16_t *port,
    struct dlep_session *session, struct dlep_parser_value *value) {
  uint16_t tmp;
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_IPV6_CONPOINT_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  if (value->length == 18) {
    memcpy(&tmp, &ptr[16], sizeof(tmp));
    *port = ntohs(tmp);
  }
  else {
    *port = DLEP_PORT;
  }

  return netaddr_from_binary(addr, ptr, 16, AF_INET6);
}

int
dlep_reader_uint64(uint64_t *number, uint16_t tlv_id,
    struct dlep_session *session, struct dlep_parser_value *value) {
  uint64_t tmp;
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, tlv_id);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  memcpy(&tmp, ptr, sizeof(tmp));
  *number = be64toh(tmp);
  return 0;
}

int
dlep_reader_int64(int64_t *number, uint16_t tlv_id,
    struct dlep_session *session, struct dlep_parser_value *value) {
  uint64_t tmp;
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, tlv_id);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  memcpy(&tmp, ptr, sizeof(tmp));
  *number = (int64_t)(be64toh(tmp));
  return 0;
}

int
dlep_reader_status(enum dlep_status *status,
    char *text, size_t text_length,
    struct dlep_session *session, struct dlep_parser_value *value) {
  const uint8_t *ptr;

  if (!value) {
    value = dlep_session_get_tlv_value(session, DLEP_STATUS_TLV);
    if (!value) {
      return -1;
    }
  }

  ptr = dlep_session_get_tlv_binary(session, value);
  *status = ptr[0];

  if (value->length > 1 && text_length > 0) {
    /* generate a 0 terminated copy of the text */
    if (text_length >= value->length) {
      memcpy(text, &ptr[1], value->length-1);
      text[value->length-1] = 0;
    }
    else {
      memcpy(text, &ptr[1], text_length-1);
      text[text_length-1] = 0;
    }
  }
  return 0;
}

int
dlep_reader_map_identity(struct oonf_layer2_data *data,
    struct dlep_session *session, uint16_t dlep_tlv) {
  struct dlep_parser_value *value;
  int64_t l2value;
  uint64_t tmp64;
  uint32_t tmp32;
  uint16_t tmp16;
  uint8_t  tmp8;
  const uint8_t *dlepvalue;

  value = dlep_session_get_tlv_value(session, dlep_tlv);
  if (value) {
    dlepvalue = dlep_parser_get_tlv_binary(&session->parser, value);

    switch (value->length) {
      case 8:
        memcpy(&tmp64, dlepvalue, 8);
        l2value = (int64_t) be64toh(tmp64);
        break;
      case 4:
        memcpy(&tmp32, dlepvalue, 4);
        l2value = (int32_t) ntohl(tmp32);
        break;
      case 2:
        memcpy(&tmp16, dlepvalue, 2);
        l2value = (int16_t) ntohs(tmp16);
        break;
      case 1:
        memcpy(&tmp8, dlepvalue, 1);
        l2value = (int8_t) tmp8;
        break;
      default:
        return -1;
    }

    oonf_layer2_set_value(data, session->l2_origin, l2value);
  }
  return 0;
}

int
dlep_reader_map_l2neigh_data(struct oonf_layer2_data *data,
    struct dlep_session *session, struct dlep_extension *ext) {
  struct dlep_neighbor_mapping *map;
  size_t i;

  for (i=0; i<ext->neigh_mapping_count; i++) {
    map = &ext->neigh_mapping[i];

    if (map->from_tlv(&data[map->layer2], session, map->dlep)) {
      return -(i+1);
    }
  }
  return 0;
}


int
dlep_reader_map_l2net_data(struct oonf_layer2_data *data,
    struct dlep_session *session, struct dlep_extension *ext) {
  struct dlep_network_mapping *map;
  size_t i;

  for (i=0; i<ext->if_mapping_count; i++) {
    map = &ext->if_mapping[i];

    if (map->from_tlv(&data[map->layer2], session, map->dlep)) {
      return -(i+1);
    }
  }
  return 0;
}
