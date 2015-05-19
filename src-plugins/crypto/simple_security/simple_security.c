
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

#include <string.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/netaddr.h"
#include "config/cfg_schema.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_interface.h"
#include "crypto/rfc5444_signature/rfc5444_signature.h"

#include "crypto/simple_security/simple_security.h"

/* definitions */
#define LOG_SIMPLE_SECURITY _simple_security_subsystem.logging

struct _sise_config {
  char key[256];
  size_t key_length;

  uint64_t vtime;
  uint64_t trigger_delay;
};

struct timestamp_key {
  struct netaddr src;
  unsigned if_index;
};

struct _timestamp_node {
  struct timestamp_key key;

  uint32_t last_timestamp;

  /* commands for packet generation */
  uint32_t send_query;
  uint32_t send_response;

  struct oonf_rfc5444_target *_target;
  struct oonf_timer_instance _vtime;
  struct oonf_timer_instance _trigger;
  struct avl_node _node;
};
static int _init(void);
static void _cleanup(void);

static bool _cb_is_matching_signature(struct rfc5444_signature *sig, int msg_type);
static const void *_cb_getCryptoKey(struct rfc5444_signature *sig, size_t *length);
static const void *_cb_getKeyId(struct rfc5444_signature *sig, size_t *length);

static struct _timestamp_node *_add_timestamp(struct timestamp_key *);
static void _cb_timestamp_timeout(void *);
static void _cb_query_trigger(void *);

static enum rfc5444_result _cb_timestamp_tlv(struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _cb_timestamp_failed(struct rfc5444_reader_tlvblock_context *context);
static void _cb_addPacketTLVs(struct rfc5444_writer *, struct rfc5444_writer_target *);
static void _cb_finishPacketTLVs(struct rfc5444_writer *, struct rfc5444_writer_target *);

static int _avl_comp_timestamp_keys(const void *, const void *);

static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _sise_entries[] = {
  CFG_MAP_STRING_ARRAY(_sise_config, key, "key", NULL,
      "Key for HMAC signature", 256),
  CFG_MAP_CLOCK_MIN(_sise_config, vtime, "vtime", "60000",
      "Time until replay protection counters are dropped", 60000),
  CFG_MAP_CLOCK_MIN(_sise_config, trigger_delay, "trigger_delay", "10000",
      "Time until a query/response will be generated ", 1000),
};

static struct cfg_schema_section _sise_section = {
  .type = OONF_SIMPLE_SECURITY_SUBSYSTEM,
  .cb_delta_handler = _cb_config_changed,
  .entries = _sise_entries,
  .entry_count = ARRAYSIZE(_sise_entries),
};

/* subsystem */
static const char *_dependencies[] = {
  OONF_CLASS_SUBSYSTEM,
  OONF_TIMER_SUBSYSTEM,
  OONF_RFC5444_SUBSYSTEM,
  OONF_OS_INTERFACE_SUBSYSTEM,
  OONF_RFC5444_SIG_SUBSYSTEM,
};
static struct oonf_subsystem _simple_security_subsystem = {
  .name = OONF_SIMPLE_SECURITY_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .descr = "RFC5444 SHA256-HMAC shared-key security plugin",
  .author = "Henning Rogge",

  .init = _init,
  .cleanup = _cleanup,

  .cfg_section = &_sise_section,
};
DECLARE_OONF_PLUGIN(_simple_security_subsystem);

static struct _sise_config _config;

/* packet signature */
static struct rfc5444_signature _signature = {
  .key = {
      .crypt_function = RFC7182_ICV_CRYPT_HMAC,
      .hash_function = RFC7182_ICV_HASH_SHA_256,
  },

  .is_matching_signature = _cb_is_matching_signature,
  .getCryptoKey = _cb_getCryptoKey,
  .getKeyId = _cb_getKeyId,
  .drop_if_invalid = true,
  .source_specific = true,
};

/* RFC5444 elements */
static struct oonf_rfc5444_protocol *_protocol;

static struct rfc5444_reader_tlvblock_consumer _pkt_consumer = {
  .order = RFC5444_VALIDATOR_PRIORITY + 2,
  .block_callback = _cb_timestamp_tlv,
  .block_callback_failed_constraints = _cb_timestamp_failed,
};

enum {
  IDX_PKTTLV_SEND,
  IDX_PKTTLV_QUERY,
  IDX_PKTTLV_RESPONSE,
};
static struct rfc5444_reader_tlvblock_consumer_entry _pkt_tlvs[] = {
  [IDX_PKTTLV_SEND] =
    { .type = RFC7182_PKTTLV_TIMESTAMP, .mandatory = true,
      .type_ext = RFC7182_TIMESTAMP_EXT_MONOTONIC, .match_type_ext = true,
      .min_length = 4, .max_length = 4, .match_length = 4, },
  [IDX_PKTTLV_QUERY] =
    { .type = RFC5444_PKTTLV_CHALLENGE,
      .type_ext = RFC5444_CHALLENGE_QUERY, .match_type_ext = true,
      .min_length = 4, .max_length = 4, .match_length = 4, },
  [IDX_PKTTLV_RESPONSE] =
    { .type = RFC5444_PKTTLV_CHALLENGE,
      .type_ext = RFC5444_CHALLENGE_RESPONSE, .match_type_ext = true,
      .min_length = 4, .max_length = 4, .match_length = 4, },
};

static struct rfc5444_writer_pkthandler _pkt_handler = {
  .addPacketTLVs = _cb_addPacketTLVs,
  .finishPacketTLVs = _cb_finishPacketTLVs,
};

/* storage for received timestamps */
static struct avl_tree _timestamp_tree;

static struct oonf_class _timestamp_class = {
  .name = "signature timestamps",
  .size = sizeof(struct _timestamp_node),
};

static struct oonf_timer_class _timeout_class = {
  .name = "signature timestamp timeout",
  .callback = _cb_timestamp_timeout,
};

static struct oonf_timer_class _query_trigger_class = {
  .name = "signature query trigger",
  .callback = _cb_query_trigger,
};

/* global "timestamp" for replay protection */
uint32_t _local_timestamp = 1;

static int
_init(void) {
  _protocol = oonf_rfc5444_add_protocol(RFC5444_PROTOCOL, true);
  if (_protocol == NULL) {
    return -1;
  }

  rfc5444_reader_add_packet_consumer(&_protocol->reader,
      &_pkt_consumer, _pkt_tlvs, ARRAYSIZE(_pkt_tlvs));
  rfc5444_writer_register_pkthandler(&_protocol->writer, &_pkt_handler);


  rfc5444_sig_add(&_signature);

  oonf_class_add(&_timestamp_class);
  oonf_timer_add(&_timeout_class);
  oonf_timer_add(&_query_trigger_class);
  avl_init(&_timestamp_tree, _avl_comp_timestamp_keys, false);
  return 0;
}

static void
_cleanup(void) {
  struct _timestamp_node *node, *node_it;

  avl_for_each_element_safe(&_timestamp_tree, node, _node, node_it) {

  }
  rfc5444_sig_remove(&_signature);
  oonf_timer_remove(&_timeout_class);
  oonf_timer_remove(&_query_trigger_class);
  oonf_class_remove(&_timestamp_class);

  rfc5444_reader_remove_packet_consumer(&_protocol->reader, &_pkt_consumer);
  rfc5444_writer_unregister_pkthandler(&_protocol->writer, &_pkt_handler);
  oonf_rfc5444_remove_protocol(_protocol);
}

static bool
_cb_is_matching_signature(
    struct rfc5444_signature *sig __attribute__((unused)), int msg_type) {
  return msg_type == RFC5444_WRITER_PKT_POSTPROCESSOR;
}

static const void *
_cb_getCryptoKey(
    struct rfc5444_signature *sig __attribute__((unused)), size_t *length) {
  *length = _config.key_length;
  return _config.key;
}

static const void *
_cb_getKeyId(
    struct rfc5444_signature *sig __attribute__((unused)), size_t *length) {
  static const char dummy[] = "";
  *length = 0;
  return dummy;
}

static struct _timestamp_node *
_add_timestamp(struct timestamp_key *key) {
  struct _timestamp_node *node;

  node = oonf_class_malloc(&_timestamp_class);
  if (!node) {
    return NULL;
  }

  /* hook into tree */
  memcpy(&node->key, key, sizeof(*key));
  node->_node.key = &node->key;
  avl_insert(&_timestamp_tree, &node->_node);

  /* initialize timer */
  node->_vtime.class = &_timeout_class;
  node->_vtime.cb_context = node;
  oonf_timer_set(&node->_vtime, _config.vtime);

  node->_trigger.class = &_query_trigger_class;
  node->_trigger.cb_context = node;

  return node;
}

static void
_cb_timestamp_timeout(void *ptr) {
  struct _timestamp_node *node = ptr;

  oonf_timer_stop(&node->_vtime);
  oonf_rfc5444_remove_target(node->_target);
  avl_remove(&_timestamp_tree, &node->_node);
  oonf_class_free(&_timestamp_class, node);
}

static void
_cb_query_trigger(void *ptr) {
  struct _timestamp_node *node = ptr;

  rfc5444_writer_flush(&_protocol->writer, &node->_target->rfc5444_target, true);
}

static enum rfc5444_result
_cb_timestamp_tlv(struct rfc5444_reader_tlvblock_context *context __attribute__((unused))) {
  struct oonf_rfc5444_target *target;
  struct os_interface *core_if;
  struct _timestamp_node *node;
  uint32_t timestamp, query, response;
  enum rfc5444_result result;

#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  struct timestamp_key key;

  core_if = oonf_rfc5444_get_core_interface(_protocol->input_interface);

  /* get input-addr/interface combination */
  memset(&key, 0, sizeof(key));
  memcpy(&key.src, _protocol->input_address, sizeof(key.src));
  key.if_index = core_if->data.index;

  /* get timestamp packet TLV */
  memcpy(&timestamp, _pkt_tlvs[IDX_PKTTLV_SEND].tlv->single_value, sizeof(timestamp));
  timestamp = ntohl(timestamp);

  /* get query packet TLV */
  if (_pkt_tlvs[IDX_PKTTLV_QUERY].tlv) {
    memcpy(&query, _pkt_tlvs[IDX_PKTTLV_QUERY].tlv->single_value, sizeof(query));
    query = ntohl(query);
  }
  else {
    query = 0;
  }

  /* get response packet TLV */
  if (_pkt_tlvs[IDX_PKTTLV_RESPONSE].tlv) {
    memcpy(&response, _pkt_tlvs[IDX_PKTTLV_RESPONSE].tlv->single_value, sizeof(response));
    response = ntohl(response);
  }
  else {
    response = 0;
  }

  /* get or create timestamp node */
  node = avl_find_element(&_timestamp_tree, &key, node, _node);
  if (!node) {
    target = oonf_rfc5444_add_target(_protocol->input_interface, _protocol->input_address);
    if (!target) {
      return RFC5444_DROP_PACKET;
    }

    node = _add_timestamp(&key);
    if (!node) {
      oonf_rfc5444_remove_target(target);
      return RFC5444_DROP_PACKET;
    }

    /* remember target */
    node->_target = target;

    /* reset timestamp to force query */
    timestamp = 0;
  }

  OONF_DEBUG(LOG_SIMPLE_SECURITY, "Received new packet from %s/%s(%u): timestamp=%u (was %u), query=%u response=%u",
      netaddr_to_string(&nbuf, &key.src), core_if->data.name, key.if_index,
      timestamp, node->last_timestamp, query, response);

  /* remember querry */
  node->send_response = query;

  /* handle incoming timestamp and query response */
  if ((node->send_query > 0 && response == node->send_query)
      || node->last_timestamp < timestamp) {
    OONF_INFO(LOG_SIMPLE_SECURITY, "Received valid timestamp");

    /* we got a valid query/response or a valid timestamp */
    node->last_timestamp = timestamp;
    node->send_query = 0;

    result = RFC5444_OKAY;

    /* stop trigger, we just received a good packet */
    oonf_timer_stop(&node->_trigger);
  }
  else {
    /* old counter, trigger challenge */
    if (node->send_query == 0) {
      /* generate query */
      node->send_query = ++_local_timestamp;
    }

    /* do not accept a query with a bad counter */
    node->send_query = 0;

    /* and drop packet */
    result = RFC5444_DROP_PACKET;
  }

  if (node->send_query > 0 || node->send_response > 0) {
    OONF_INFO(LOG_SIMPLE_SECURITY, "Trigger challenge message: query=%u response=%u",
        node->send_query, node->send_response);

    if(!oonf_timer_is_active(&node->_trigger)) {
      /* trigger query response */
      oonf_timer_set(&node->_trigger, _config.trigger_delay);
    }
  }

  /* reset validity time */
  oonf_timer_set(&node->_vtime, _config.vtime);

  return result;
}

static enum rfc5444_result
_cb_timestamp_failed(struct rfc5444_reader_tlvblock_context *context __attribute((unused))) {
  /* packet timestamp missing or wrong length */
  return RFC5444_DROP_PACKET;
}

static void
_cb_addPacketTLVs(struct rfc5444_writer *writer, struct rfc5444_writer_target *rfc5444_target) {
  struct oonf_rfc5444_target *target;
  struct os_interface *core_if;
  struct _timestamp_node *node;
  uint32_t query, response;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  struct timestamp_key key;

  /* get OONF rfc5444 target */
  target = oonf_rfc5444_get_target_from_rfc5444_target(rfc5444_target);

  /* get core interface */
  core_if = oonf_rfc5444_get_core_interface(target->interface);

  /* get input-addr/interface combination */
  memset(&key, 0, sizeof(key));
  memcpy(&key.src, &target->dst, sizeof(key.src));
  key.if_index = core_if->data.index;

  /* Challenge query and response are only valid for unicasts */
  node = avl_find_element(&_timestamp_tree, &key, node, _node);
  if (node) {
    if (node->send_query) {
      /* send query */
      query = htonl(node->send_query);

      rfc5444_writer_add_packettlv(writer, rfc5444_target,
          RFC5444_PKTTLV_CHALLENGE, RFC5444_CHALLENGE_QUERY,
          &query, sizeof(query));
    }
    if (node->send_response) {
      /* send response */
      response = htonl(node->send_response);

      rfc5444_writer_add_packettlv(writer, rfc5444_target,
          RFC5444_PKTTLV_CHALLENGE, RFC5444_CHALLENGE_RESPONSE,
          &response, sizeof(response));
    }
    OONF_DEBUG(LOG_SIMPLE_SECURITY, "Add packettvs to %s/%s(%u): query=%u response=%u",
        netaddr_to_string(&nbuf, &key.src), core_if->data.name, key.if_index,
        node->send_query, node->send_response);

    /* clear response */
    node->send_response = 0;
  }

  /* allocate space for timestamp tlv */
  rfc5444_writer_allocate_packettlv(writer, rfc5444_target, true, 4);
}

static void
_cb_finishPacketTLVs(struct rfc5444_writer *writer, struct rfc5444_writer_target *rfc5444_target) {
  uint32_t timestamp;

  /* always send a timestamp */
  timestamp = htonl(++_local_timestamp);

  rfc5444_writer_set_packettlv(writer, rfc5444_target,
      RFC7182_PKTTLV_TIMESTAMP, RFC7182_TIMESTAMP_EXT_MONOTONIC,
      &timestamp, sizeof(timestamp));
}


static int
_avl_comp_timestamp_keys(const void *p1, const void *p2){
  return memcmp(p1, p2, sizeof(struct timestamp_key));
}

static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_config, _sise_section.post, _sise_entries, ARRAYSIZE(_sise_entries))) {
    OONF_WARN(LOG_SIMPLE_SECURITY, "Cannot convert configuration for "
        OONF_SIMPLE_SECURITY_SUBSYSTEM);
    return;
  }

  _config.key_length = strlen(_config.key);
}
