
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

#include "common/common_types.h"
#include "common/netaddr.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_duplicate_set.h"
#include "subsystems/oonf_rfc5444.h"

#include "olsrv2/olsrv2.h"
#include "olsrv2/olsrv2_internal.h"
#include "olsrv2/olsrv2_originator.h"
#include "olsrv2/olsrv2_reader.h"
#include "olsrv2/olsrv2_routing.h"
#include "olsrv2/olsrv2_tc.h"

/* constants and definitions */

/* OLSRv2 message TLV array index */
enum {
  IDX_TLV_ITIME,
  IDX_TLV_VTIME,
  IDX_TLV_CONT_SEQ_NUM,
  IDX_TLV_MPRTYPES,
};

/* OLSRv2 address TLV array index pass 1 */
enum {
  IDX_ADDRTLV_LINK_METRIC,
  IDX_ADDRTLV_NBR_ADDR_TYPE,
  IDX_ADDRTLV_GATEWAY,
};

/* session data during TC parsing */
struct _olsrv2_data {
  struct olsrv2_tc_node *node;
  uint64_t vtime;
  bool complete_tc;
  uint8_t mprtypes[NHDP_MAXIMUM_DOMAINS];
  size_t mprtypes_size;
};

/* Prototypes */
static enum rfc5444_result
_cb_messagetlvs(struct rfc5444_reader_tlvblock_context *context);

static enum rfc5444_result
_cb_addresstlvs(struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _cb_messagetlvs_end(
    struct rfc5444_reader_tlvblock_context *context, bool dropped);

/* definition of the RFC5444 reader components */
static struct rfc5444_reader_tlvblock_consumer _olsrv2_message_consumer = {
  .order = RFC5444_MAIN_PARSER_PRIORITY,
  .msg_id = RFC5444_MSGTYPE_TC,
  .block_callback = _cb_messagetlvs,
  .end_callback = _cb_messagetlvs_end,
};

static struct rfc5444_reader_tlvblock_consumer_entry _olsrv2_message_tlvs[] = {
  [IDX_TLV_ITIME] = { .type = RFC5497_MSGTLV_INTERVAL_TIME, .type_ext = 0, .match_type_ext = true,
      .min_length = 1, .max_length = 511, .match_length = true },
  [IDX_TLV_VTIME] = { .type = RFC5497_MSGTLV_VALIDITY_TIME, .type_ext = 0, .match_type_ext = true,
      .mandatory = true, .min_length = 1, .max_length = 511, .match_length = true },
  [IDX_TLV_CONT_SEQ_NUM] = { .type = RFC7181_MSGTLV_CONT_SEQ_NUM,
      .mandatory = true, .min_length = 2, .max_length = 65535, .match_length = true },
  [IDX_TLV_MPRTYPES] = { .type = DRAFT_MT_MSGTLV_MPR_TYPES,
      .type_ext = DRAFT_MT_MSGTLV_MPR_TYPES_EXT, .match_type_ext = true,
      .min_length = 1, .max_length = NHDP_MAXIMUM_DOMAINS, .match_length = true },
};

static struct rfc5444_reader_tlvblock_consumer _olsrv2_address_consumer = {
  .order = RFC5444_MAIN_PARSER_PRIORITY,
  .msg_id = RFC5444_MSGTYPE_TC,
  .addrblock_consumer = true,
  .block_callback = _cb_addresstlvs,
};

static struct rfc5444_reader_tlvblock_consumer_entry _olsrv2_address_tlvs[] = {
  [IDX_ADDRTLV_LINK_METRIC] = { .type = RFC7181_ADDRTLV_LINK_METRIC,
    .min_length = 2, .max_length = 65535, .match_length = true },
  [IDX_ADDRTLV_NBR_ADDR_TYPE] = { .type = RFC7181_ADDRTLV_NBR_ADDR_TYPE,
    .min_length = 1, .max_length = 65535, .match_length = true },
  [IDX_ADDRTLV_GATEWAY] = { .type = RFC7181_ADDRTLV_GATEWAY,
    .min_length = 1, .max_length = 65535, .match_length = true },
};

/* nhdp multiplexer/protocol */
static struct oonf_rfc5444_protocol *_protocol = NULL;

static struct _olsrv2_data _current;

/**
 * Initialize nhdp reader
 */
void
olsrv2_reader_init(struct oonf_rfc5444_protocol *p) {
  _protocol = p;

  rfc5444_reader_add_message_consumer(
      &_protocol->reader, &_olsrv2_message_consumer,
      _olsrv2_message_tlvs, ARRAYSIZE(_olsrv2_message_tlvs));
  rfc5444_reader_add_message_consumer(
      &_protocol->reader, &_olsrv2_address_consumer,
      _olsrv2_address_tlvs, ARRAYSIZE(_olsrv2_address_tlvs));
}

/**
 * Cleanup nhdp reader
 */
void
olsrv2_reader_cleanup(void) {
  rfc5444_reader_remove_message_consumer(
      &_protocol->reader, &_olsrv2_address_consumer);
  rfc5444_reader_remove_message_consumer(
      &_protocol->reader, &_olsrv2_message_consumer);
}

/**
 * Callback that parses message TLVs of TC
 * @param context
 * @return
 */
static enum rfc5444_result
_cb_messagetlvs(struct rfc5444_reader_tlvblock_context *context) {
  uint64_t itime;
  uint16_t ansn;
  uint8_t tmp;
  int af_type;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf;
#endif

  /*
   * First remove all old session data.
   * Do not put anything that could drop a session before this point,
   * otherwise the cleanup path will run on an outdated session object.
   */
  memset(&_current, 0, sizeof(_current));

  OONF_DEBUG(LOG_OLSRV2_R, "Received TC from %s",
      netaddr_to_string(&buf, _protocol->input_address));

  if (!context->has_origaddr || !context->has_hopcount
      || !context->has_hoplimit || !context->has_seqno) {
    OONF_DEBUG(LOG_OLSRV2_R, "Missing message flag");
    return RFC5444_DROP_MESSAGE;
  }

  if (olsrv2_originator_is_local(&context->orig_addr)) {
    OONF_DEBUG(LOG_OLSRV2_R, "We are hearing ourself");
    return RFC5444_DROP_MESSAGE;
  }

  switch (context->addr_len) {
    case 4:
      af_type = AF_INET;
      break;
    case 16:
      af_type = AF_INET6;
      break;
    default:
      af_type = 0;
      break;
  }

  if (!oonf_rfc5444_is_interface_active(_protocol->input_interface, af_type)) {
    OONF_DEBUG(LOG_OLSRV2_R, "We do not handle address length %u on interface %s",
        context->addr_len, _protocol->input_interface->name);
    return RFC5444_DROP_MESSAGE;
  }

  OONF_DEBUG(LOG_OLSRV2_R, "Originator: %s   Seqno: %u",
      netaddr_to_string(&buf, &context->orig_addr), context->seqno);

  /* get cont_seq_num extension */
  tmp = _olsrv2_message_tlvs[IDX_TLV_CONT_SEQ_NUM].type_ext;
  if (tmp != RFC7181_CONT_SEQ_NUM_COMPLETE
      && tmp != RFC7181_CONT_SEQ_NUM_INCOMPLETE) {
    OONF_DEBUG(LOG_OLSRV2_R, "Illegal extension of CONT_SEQ_NUM TLV: %u",
        tmp);
    return RFC5444_DROP_MESSAGE;
  }
  _current.complete_tc = tmp == RFC7181_CONT_SEQ_NUM_COMPLETE;

  /* get ANSN */
  memcpy(&ansn,
      _olsrv2_message_tlvs[IDX_TLV_CONT_SEQ_NUM].tlv->single_value, 2);
  ansn = ntohs(ansn);

  /* get VTime/ITime */
  tmp = rfc5497_timetlv_get_from_vector(
      _olsrv2_message_tlvs[IDX_TLV_VTIME].tlv->single_value,
      _olsrv2_message_tlvs[IDX_TLV_VTIME].tlv->length,
      context->hopcount);
  _current.vtime = rfc5497_timetlv_decode(tmp);

  if (_olsrv2_message_tlvs[IDX_TLV_ITIME].tlv) {
    tmp = rfc5497_timetlv_get_from_vector(
        _olsrv2_message_tlvs[IDX_TLV_ITIME].tlv->single_value,
        _olsrv2_message_tlvs[IDX_TLV_ITIME].tlv->length,
        context->hopcount);
    itime = rfc5497_timetlv_decode(tmp);
  }
  else {
    itime = 0;
  }

  /* get mprtypes */
  _current.mprtypes_size = nhdp_domain_process_mprtypes_tlv(
      _current.mprtypes, sizeof(_current.mprtypes),
      _olsrv2_message_tlvs[IDX_TLV_MPRTYPES].tlv);

  /* test if we already forwarded the message */
  if (!olsrv2_mpr_shall_forwarding(
      context, _protocol->input_address, _current.vtime)) {
    /* mark message as 'no forward */
    rfc5444_reader_prevent_forwarding(context);
  }

  /* test if we already processed the message */
  if (!olsrv2_mpr_shall_process(context, _current.vtime)) {
    OONF_DEBUG(LOG_OLSRV2_R, "Processing set says 'do not process'");
    return RFC5444_DROP_MSG_BUT_FORWARD;
  }

  /* get tc node */
  _current.node = olsrv2_tc_node_add(
      &context->orig_addr, _current.vtime, ansn);
  if (_current.node == NULL) {
    OONF_DEBUG(LOG_OLSRV2_R, "Cannot create node");
    return RFC5444_DROP_MSG_BUT_FORWARD;
  }

  /* check if the topology information is recent enough */
  if (rfc5444_seqno_is_smaller(ansn, _current.node->ansn)) {
    OONF_DEBUG(LOG_OLSRV2_R, "ANSN %u is smaller than last stored ANSN %u",
        ansn, _current.node->ansn);
    return RFC5444_DROP_MSG_BUT_FORWARD;
  }

  /* overwrite old ansn */
  _current.node->ansn = ansn;

  /* reset validity time and interval time */
  oonf_timer_set(&_current.node->_validity_time, _current.vtime);
  _current.node->interval_time = itime;

  /* continue parsing the message */
  return RFC5444_OKAY;
}

/**
 * Callback that parses address TLVs of TC
 * @param context
 * @return
 */
static enum rfc5444_result
_cb_addresstlvs(struct rfc5444_reader_tlvblock_context *context __attribute__((unused))) {
  struct rfc5444_reader_tlvblock_entry *tlv;
  struct nhdp_domain *domain;
  struct olsrv2_tc_edge *edge;
  struct olsrv2_tc_attachment *end;
  uint32_t cost_in[NHDP_MAXIMUM_DOMAINS];
  uint32_t cost_out[NHDP_MAXIMUM_DOMAINS];
  struct rfc7181_metric_field metric_value;
  struct netaddr truncated;
  size_t i;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf;
#endif

  if (_current.node == NULL) {
    return RFC5444_OKAY;
  }

  for (i=0; i<NHDP_MAXIMUM_DOMAINS; i++) {
    cost_in[i] = RFC7181_METRIC_INFINITE;
    cost_out[i] = RFC7181_METRIC_INFINITE;
  }

  OONF_DEBUG(LOG_OLSRV2_R, "Found address in tc: %s",
      netaddr_to_string(&buf, &context->addr));

  for (tlv = _olsrv2_address_tlvs[IDX_ADDRTLV_LINK_METRIC].tlv;
      tlv; tlv = tlv->next_entry) {
    domain = nhdp_domain_get_by_ext(tlv->type_ext);
    if (domain == NULL) {
      continue;
    }

    memcpy(&metric_value, tlv->single_value, sizeof(metric_value));

    OONF_DEBUG(LOG_OLSRV2_R, "Metric for domain %d: 0x%02x%02x",
        domain->index, metric_value.b[0], metric_value.b[1]);

    if (rfc7181_metric_has_flag(&metric_value, RFC7181_LINKMETRIC_INCOMING_NEIGH)) {
      cost_in[domain->index] = rfc7181_metric_decode(&metric_value);
      OONF_DEBUG(LOG_OLSRV2_R, "Incoming metric: %d", cost_in[domain->index]);
     }

    if (rfc7181_metric_has_flag(&metric_value, RFC7181_LINKMETRIC_OUTGOING_NEIGH)) {
      cost_out[domain->index] = rfc7181_metric_decode(&metric_value);
      OONF_DEBUG(LOG_OLSRV2_R, "Outgoing metric: %d", cost_out[domain->index]);
    }
  }

  for (tlv = _olsrv2_address_tlvs[IDX_ADDRTLV_NBR_ADDR_TYPE].tlv;
      tlv; tlv = tlv->next_entry) {
    /* find routing domain */
    domain = nhdp_domain_get_by_ext(tlv->type_ext);
    if (domain == NULL) {
      continue;
    }

    /* parse originator neighbor */
    if ((tlv->single_value[0] & RFC7181_NBR_ADDR_TYPE_ORIGINATOR) != 0) {
      edge = olsrv2_tc_edge_add(_current.node, &context->addr);
      if (edge) {
        OONF_DEBUG(LOG_OLSRV2_R, "Address is originator");
        edge->ansn = _current.node->ansn;
        edge->cost[domain->index] = cost_out[domain->index];

        if (edge->inverse->virtual) {
          edge->inverse->cost[domain->index] = cost_in[domain->index];
        }
      }
    }
    /* parse routable neighbor (which is not an originator) */
    else if ((tlv->single_value[0] & RFC7181_NBR_ADDR_TYPE_ROUTABLE) != 0) {
      end = olsrv2_tc_endpoint_add(_current.node, &context->addr, true);
      if (end) {
        OONF_DEBUG(LOG_OLSRV2_R, "Address is routable, but not originator");
        end->ansn = _current.node->ansn;
        end->cost[domain->index] = cost_out[domain->index];
      }
    }
  }

  for (tlv = _olsrv2_address_tlvs[IDX_ADDRTLV_GATEWAY].tlv;
      tlv; tlv = tlv->next_entry) {
    /* check length */
    if (tlv->length > 1 && tlv->length < _current.mprtypes_size) {
      /* bad length */
      continue;
    }

    /* truncate address */
    netaddr_truncate(&truncated, &context->addr);

    /* parse attached network */
    end = olsrv2_tc_endpoint_add(_current.node, &truncated, false);
    if (!end) {
      continue;
    }

    end->ansn = _current.node->ansn;

    /* use MT definition of AN tlv */
    for (i=0; i<_current.mprtypes_size; i++) {
      domain = nhdp_domain_get_by_ext(_current.mprtypes[i]);
      if (!domain) {
        /* unknown domain */
        continue;
      }

      if (cost_out[domain->index] >= RFC7181_METRIC_INFINITE) {
        /* metric is missing */
        continue;
      }

      end->cost[domain->index] = cost_out[domain->index];

      if (tlv->length == 1) {
        end->distance[domain->index] = tlv->single_value[0];
      }
      else {
        end->distance[domain->index] = tlv->single_value[i];
      }

      OONF_DEBUG(LOG_OLSRV2_R, "Address is Attached Network: dist=%u",
          end->distance[domain->index]);
    }
  }
  return RFC5444_OKAY;
}

/**
 * Callback that is called when message parsing of TLV is finished
 * @param context
 * @param dropped
 * @return
 */
static enum rfc5444_result
_cb_messagetlvs_end(struct rfc5444_reader_tlvblock_context *context __attribute__((unused)),
    bool dropped) {
  /* cleanup everything that is not the current ANSN */
  struct olsrv2_tc_edge *edge, *edge_it;
  struct olsrv2_tc_attachment *end, *end_it;

  if (dropped || _current.node == NULL) {
    return RFC5444_OKAY;
  }

  avl_for_each_element_safe(&_current.node->_edges, edge, _node, edge_it) {
    if (edge->ansn != _current.node->ansn) {
      olsrv2_tc_edge_remove(edge);
    }
  }

  avl_for_each_element_safe(&_current.node->_attached_networks, end, _src_node, end_it) {
    if (end->ansn != _current.node->ansn) {
      olsrv2_tc_endpoint_remove(end);
    }
  }

  olsrv2_tc_trigger_change(_current.node);
  _current.node = NULL;

  /* recalculate routing table */
  olsrv2_routing_trigger_update();

  return RFC5444_OKAY;
}
