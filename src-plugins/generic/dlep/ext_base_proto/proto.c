/*
 * dlep_radio_base_extension.c
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#include "common/common_types.h"
#include "common/avl.h"
#include "common/autobuf.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_iana.h"
#include "dlep/dlep_extension.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"

#include "dlep/ext_base_proto/proto.h"

static void _cb_local_heartbeat(void *);
static void _cb_remote_heartbeat(void *);

/* peer discovery */

/* peer offer */
static const uint16_t _peer_offer_tlvs[] = {
    DLEP_PEER_TYPE_TLV,
    DLEP_IPV4_CONPOINT_TLV,
    DLEP_IPV6_CONPOINT_TLV,
};

/* peer initialization */
static const uint16_t _peer_init_tlvs[] = {
    DLEP_HEARTBEAT_INTERVAL_TLV,
    DLEP_PEER_TYPE_TLV,
    DLEP_EXTENSIONS_SUPPORTED_TLV,
};
static const uint16_t _peer_init_mandatory[] = {
    DLEP_HEARTBEAT_INTERVAL_TLV,
};

/* peer initialization ack */
static const uint16_t _peer_initack_tlvs[] = {
    DLEP_HEARTBEAT_INTERVAL_TLV,
    DLEP_STATUS_TLV,
    DLEP_PEER_TYPE_TLV,
    DLEP_EXTENSIONS_SUPPORTED_TLV,
};
static const uint16_t _peer_initack_mandatory[] = {
    DLEP_HEARTBEAT_INTERVAL_TLV,
};

/* peer update */
static const uint16_t _peer_update_tlvs[] = {
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
};
static const uint16_t _peer_update_duplicates[] = {
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
};

/* peer update ack */
static const uint16_t _peer_updateack_tlvs[] = {
    DLEP_STATUS_TLV,
};

/* peer termination */
static const uint16_t _peer_termination_tlvs[] = {
    DLEP_STATUS_TLV,
};

/* peer termination ack */
static const uint16_t _peer_terminationack_tlvs[] = {
    DLEP_STATUS_TLV,
};

/* destination up */
static const uint16_t _dst_up_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
    DLEP_IPV4_SUBNET_TLV,
    DLEP_IPV6_SUBNET_TLV,
};
static const uint16_t _dst_up_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};
static const uint16_t _dst_up_duplicates[] = {
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
    DLEP_IPV4_SUBNET_TLV,
    DLEP_IPV6_SUBNET_TLV,
};

/* destination up ack */
static const uint16_t _dst_up_ack_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_STATUS_TLV,
};
static const uint16_t _dst_up_ack_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* destination down */
static const uint16_t _dst_down_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
};
static const uint16_t _dst_down_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* destination down ack */
static const uint16_t _dst_down_ack_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_STATUS_TLV,
};
static const uint16_t _dst_down_ack_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* destination update */
static const uint16_t _dst_update_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
    DLEP_IPV4_SUBNET_TLV,
    DLEP_IPV6_SUBNET_TLV,
};
static const uint16_t _dst_update_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};
static const uint16_t _dst_update_duplicates[] = {
    DLEP_IPV4_ADDRESS_TLV,
    DLEP_IPV6_ADDRESS_TLV,
    DLEP_IPV4_SUBNET_TLV,
    DLEP_IPV6_SUBNET_TLV,
};

/* link characteristics request */
static const uint16_t _linkchar_req_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
    DLEP_LINK_CHAR_ACK_TIMER_TLV,
};
static const uint16_t _linkchar_req_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* link characteristics ack */
static const uint16_t _linkchar_ack_tlvs[] = {
    DLEP_MAC_ADDRESS_TLV,
    DLEP_MDRR_TLV,
    DLEP_MDRT_TLV,
    DLEP_CDRR_TLV,
    DLEP_CDRT_TLV,
    DLEP_LATENCY_TLV,
    DLEP_RESR_TLV,
    DLEP_REST_TLV,
    DLEP_RLQR_TLV,
    DLEP_RLQT_TLV,
    DLEP_STATUS_TLV,
};
static const uint16_t _linkchar_ack_mandatory[] = {
    DLEP_MAC_ADDRESS_TLV,
};

/* supported signals of this extension */
static struct dlep_extension_signal _signals[] = {
    {
        .id = DLEP_PEER_DISCOVERY,
    },
    {
        .id = DLEP_PEER_OFFER,
        .supported_tlvs = _peer_offer_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_offer_tlvs),
    },
    {
        .id = DLEP_PEER_INITIALIZATION,
        .supported_tlvs = _peer_init_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_init_tlvs),
        .mandatory_tlvs = _peer_init_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_peer_init_mandatory),
    },
    {
        .id = DLEP_PEER_INITIALIZATION_ACK,
        .supported_tlvs = _peer_initack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_initack_tlvs),
        .mandatory_tlvs = _peer_initack_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_peer_initack_mandatory),
    },
    {
        .id = DLEP_PEER_UPDATE,
        .supported_tlvs = _peer_update_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_update_tlvs),
        .duplicate_tlvs = _peer_update_duplicates,
        .duplicate_tlv_count = ARRAYSIZE(_peer_update_duplicates),
    },
    {
        .id = DLEP_PEER_UPDATE_ACK,
        .supported_tlvs = _peer_updateack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_updateack_tlvs),
    },
    {
        .id = DLEP_PEER_TERMINATION,
        .supported_tlvs = _peer_termination_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_termination_tlvs),
    },
    {
        .id = DLEP_PEER_TERMINATION_ACK,
        .supported_tlvs = _peer_terminationack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_peer_terminationack_tlvs),
    },
    {
        .id = DLEP_DESTINATION_UP,
        .supported_tlvs = _dst_up_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_up_tlvs),
        .mandatory_tlvs = _dst_up_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_up_mandatory),
        .duplicate_tlvs = _dst_up_duplicates,
        .duplicate_tlv_count = ARRAYSIZE(_dst_up_duplicates),
    },
    {
        .id = DLEP_DESTINATION_UP_ACK,
        .supported_tlvs = _dst_up_ack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_up_ack_tlvs),
        .mandatory_tlvs = _dst_up_ack_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_up_ack_mandatory),
    },
    {
        .id = DLEP_DESTINATION_DOWN,
        .supported_tlvs = _dst_down_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_down_tlvs),
        .mandatory_tlvs = _dst_down_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_down_mandatory),
    },
    {
        .id = DLEP_DESTINATION_DOWN_ACK,
        .supported_tlvs = _dst_down_ack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_down_ack_tlvs),
        .mandatory_tlvs = _dst_down_ack_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_down_ack_mandatory),
    },
    {
        .id = DLEP_DESTINATION_UPDATE,
        .supported_tlvs = _dst_update_tlvs,
        .supported_tlv_count = ARRAYSIZE(_dst_update_tlvs),
        .mandatory_tlvs = _dst_update_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_dst_update_mandatory),
        .duplicate_tlvs = _dst_update_duplicates,
        .duplicate_tlv_count = ARRAYSIZE(_dst_update_duplicates),
    },
    {
        .id = DLEP_HEARTBEAT,
    },
    {
        .id = DLEP_LINK_CHARACTERISTICS_REQUEST,
        .supported_tlvs = _linkchar_req_tlvs,
        .supported_tlv_count = ARRAYSIZE(_linkchar_req_tlvs),
        .mandatory_tlvs = _linkchar_req_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_linkchar_req_mandatory),
    },
    {
        .id = DLEP_LINK_CHARACTERISTICS_ACK,
        .supported_tlvs = _linkchar_ack_tlvs,
        .supported_tlv_count = ARRAYSIZE(_linkchar_ack_tlvs),
        .mandatory_tlvs = _linkchar_ack_mandatory,
        .mandatory_tlv_count = ARRAYSIZE(_linkchar_ack_mandatory),
    },
};

/* supported TLVs of this extension */
static struct dlep_extension_tlv _tlvs[] = {
    { DLEP_STATUS_TLV, 1,65535 },
    { DLEP_IPV4_CONPOINT_TLV, 6,6 },
    { DLEP_IPV6_CONPOINT_TLV, 18,18 },
    { DLEP_PEER_TYPE_TLV, 1,255 },
    { DLEP_HEARTBEAT_INTERVAL_TLV, 2,2 },
    { DLEP_EXTENSIONS_SUPPORTED_TLV, 2, 65534 },
    { DLEP_MAC_ADDRESS_TLV, 6,8 },
    { DLEP_IPV4_ADDRESS_TLV, 5,5 },
    { DLEP_IPV6_ADDRESS_TLV, 17,17 },
    { DLEP_IPV4_SUBNET_TLV, 5,5 },
    { DLEP_IPV6_SUBNET_TLV, 17,17 },
    { DLEP_MDRR_TLV, 8,8 },
    { DLEP_MDRT_TLV, 8,8 },
    { DLEP_CDRR_TLV, 8,8 },
    { DLEP_CDRT_TLV, 8,8 },
    { DLEP_LATENCY_TLV, 8,8 },
    { DLEP_RESR_TLV, 1,1 },
    { DLEP_REST_TLV, 1,1 },
    { DLEP_RLQR_TLV, 1,1 },
    { DLEP_RLQT_TLV, 1,1 },
    { DLEP_LINK_CHAR_ACK_TIMER_TLV, 1,1 },
};

static struct dlep_neighbor_mapping _neigh_mappings[] = {
    {
        .dlep     = DLEP_MDRR_TLV,
        .layer2   = OONF_LAYER2_NEIGH_RX_MAX_BITRATE,
        .length   = 8,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_MDRT_TLV,
        .layer2   = OONF_LAYER2_NEIGH_TX_MAX_BITRATE,
        .length   = 8,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_CDRR_TLV,
        .layer2   = OONF_LAYER2_NEIGH_RX_BITRATE,
        .length   = 8,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_CDRT_TLV,
        .layer2   = OONF_LAYER2_NEIGH_TX_BITRATE,
        .length   = 8,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_LATENCY_TLV,
        .layer2   = OONF_LAYER2_NEIGH_LATENCY,
        .length   = 8,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_RESR_TLV,
        .layer2   = OONF_LAYER2_NEIGH_RX_RESOURCES,
        .length   = 1,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_REST_TLV,
        .layer2   = OONF_LAYER2_NEIGH_TX_RESOURCES,
        .length   = 1,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_RLQR_TLV,
        .layer2   = OONF_LAYER2_NEIGH_RX_RLQ,
        .length   = 1,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
    {
        .dlep     = DLEP_RLQT_TLV,
        .layer2   = OONF_LAYER2_NEIGH_TX_RLQ,
        .length   = 1,
        .from_tlv = dlep_reader_map_identity,
        .to_tlv   = dlep_writer_map_identity,
    },
};

/* DLEP base extension, radio side */
static struct dlep_extension _base_proto = {
  .id = DLEP_EXTENSION_BASE_PROTO,
  .name = "base",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),
  .neigh_mapping = _neigh_mappings,
  .neigh_mapping_count = ARRAYSIZE(_neigh_mappings),
};

static struct oonf_timer_class _local_heartbeat_class = {
  .name = "dlep local heartbeat",
  .callback = _cb_local_heartbeat,
  .periodic = true,
};
static struct oonf_timer_class _remote_heartbeat_class = {
  .name = "dlep remote heartbeat",
  .callback = _cb_remote_heartbeat,
};

struct dlep_extension *
dlep_base_proto_init(void) {
  if (avl_is_node_added(&_base_proto._node)) {
    return &_base_proto;
  }

  _base_proto._node.key = &_base_proto.id;
  avl_insert(dlep_extension_get_tree(), &_base_proto._node);

  oonf_timer_add(&_local_heartbeat_class);
  oonf_timer_add(&_remote_heartbeat_class);
  return &_base_proto;
}

void
dlep_base_proto_start_local_heartbeat(struct dlep_session *session) {
  /* timer for local heartbeat generation */
  session->local_event_timer.class = &_local_heartbeat_class;
  session->local_event_timer.cb_context = session;
  oonf_timer_set(&session->local_event_timer,
      session->cfg.heartbeat_interval);
}

void
dlep_base_proto_start_remote_heartbeat(struct dlep_session *session) {
  /* timeout for remote heartbeats */
  session->remote_heartbeat_timeout.class = &_remote_heartbeat_class;
  session->remote_heartbeat_timeout.cb_context = session;
  oonf_timer_set(&session->remote_heartbeat_timeout,
      session->remote_heartbeat_interval * 2);
}

void
dlep_base_proto_stop_timers(struct dlep_session *session) {
  OONF_DEBUG(session->log_source, "Cleanup base session");
  oonf_timer_stop(&session->local_event_timer);
  oonf_timer_stop(&session->remote_heartbeat_timeout);
}

enum dlep_status
dlep_base_proto_print_status(struct dlep_session *session) {
  enum dlep_status status;
  char text[256];

  if (!dlep_reader_status(&status, text, sizeof(text), session, NULL)) {
    OONF_DEBUG(session->log_source,
        "Status %d received: %s", status, text);

    return status;
  }
  return DLEP_STATUS_OKAY;
}

void
dlep_base_proto_print_peer_type(struct dlep_session *session) {
  char text[256];

  if (!dlep_reader_peer_type(text, sizeof(text), session, NULL)) {
    OONF_DEBUG(session->log_source,
        "Remote peer type: %s", text);
  }
}

int
dlep_base_proto_process_peer_termination(
    struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session) {
  dlep_base_proto_print_status(session);

  return dlep_session_generate_signal(session, DLEP_PEER_TERMINATION_ACK, NULL);
}

int
dlep_base_proto_process_peer_termination_ack(
    struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session) {
  if (session->cb_end_session) {
    session->cb_end_session(session);
  }
  return 0;
}

int
dlep_base_proto_process_heartbeat(
    struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session) {
  /* just restart the timeout with the same period */
  oonf_timer_set(&session->remote_heartbeat_timeout,
      session->remote_heartbeat_interval * 2);
  return 0;
}

int
dlep_base_proto_write_mac_only(
    struct dlep_extension *ext __attribute__((unused)),
    struct dlep_session *session, const struct netaddr *neigh) {
  if (dlep_writer_add_mac_tlv(&session->writer, neigh)) {
    return -1;
  }
  return 0;
}

static void
_cb_local_heartbeat(void *ptr) {
  struct dlep_session *session = ptr;

  dlep_session_generate_signal(session, DLEP_HEARTBEAT, NULL);
  session->cb_send_buffer(session, 0);
}

static void
_cb_remote_heartbeat(void *ptr) {
  struct dlep_session *session = ptr;

  /* stop local heartbeats */
  oonf_timer_stop(&session->local_event_timer);

  /* terminate session */
  dlep_session_generate_signal(session, DLEP_PEER_TERMINATION, NULL);
  session->next_signal = DLEP_PEER_TERMINATION_ACK;
}
