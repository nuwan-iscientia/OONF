
#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_stream_socket.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_writer.h"
#include "dlep/dlep_session.h"

enum {
  SESSION_VALUE_STEP = 128,
};

static int _update_allowed_tlvs(struct dlep_session_parser *parser);
static enum dlep_parser_error _parse_tlvstream(
    struct dlep_session_parser *parser, const uint8_t *buffer, size_t length);
static enum dlep_parser_error _check_mandatory(
    struct dlep_session_parser *parser, uint16_t signal_type);
static enum dlep_parser_error _check_duplicate(
    struct dlep_session_parser *parser, uint16_t signal_type);
static enum dlep_parser_error _call_extension_processing(
    struct dlep_session *parser, uint16_t signal_type);
static struct dlep_parser_tlv *_add_session_tlv(
    struct dlep_session_parser *parser, uint16_t id);
static enum dlep_parser_error _process_tlvs(struct dlep_session *,
    uint16_t signal_type, uint16_t signal_length, const uint8_t *tlvs);
static void _send_terminate(struct dlep_session *session);
static void _cb_destination_timeout(void *);

static struct oonf_class _tlv_class = {
    .name = "dlep reader tlv",
    .size = sizeof(struct dlep_parser_tlv),
};

static struct oonf_class _local_neighbor_class = {
    .name = "dlep neighbor",
    .size = sizeof(struct dlep_local_neighbor),
};

static struct oonf_timer_class _destination_ack_class = {
    .name = "dlep destination ack",
    .callback = _cb_destination_timeout,
};

void
dlep_session_init(void) {
  oonf_class_add(&_tlv_class);
  oonf_class_add(&_local_neighbor_class);
  oonf_timer_add(&_destination_ack_class);
}

/**
 * Initialize a session, will hook in the base extension
 * @param session
 * @return
 */
int
dlep_session_add(struct dlep_session *session, const char *l2_ifname,
    uint32_t l2_origin, struct autobuf *out, bool radio,
    enum oonf_log_source log_source) {
  struct dlep_session_parser *parser;
  int32_t i;

  parser = &session->parser;

  avl_init(&parser->allowed_tlvs, avl_comp_uint16, false);
  avl_init(&session->local_neighbor_tree, avl_comp_netaddr, false);

  session->log_source = log_source;
  session->l2_origin = l2_origin;
  session->radio = radio;
  session->writer.out = out;

  /* remember interface name */
  session->l2_listener.name = l2_ifname;

  /* get interface listener to lock interface */
  if ((oonf_interface_add_listener(&session->l2_listener))) {
    OONF_WARN(session->log_source,
        "Cannot activate interface listener for %s", l2_ifname);
    dlep_session_remove(session);
    return -1;
  }

  /* allocate memory for the pointers */
  parser->extensions = calloc(
      DLEP_EXTENSION_BASE_COUNT, sizeof(struct dlep_extension *));
  if (!parser->extensions) {
    OONF_WARN(session->log_source,
        "Cannot allocate extension buffer for %s", l2_ifname);
    dlep_session_remove(session);
    return -1;
  }

  /* remember the sessions */
  parser->extension_count = DLEP_EXTENSION_BASE_COUNT;

  parser->values = calloc(SESSION_VALUE_STEP,
      sizeof(struct dlep_parser_value));
  if (!parser->values) {
    OONF_WARN(session->log_source,
        "Cannot allocate values buffer for %s", l2_ifname);
    dlep_session_remove(session);
    return -1;
  }

  for (i=0; i<DLEP_EXTENSION_BASE_COUNT; i++) {
    parser->extensions[i] = dlep_extension_get(-(i+1));
    if (!parser->extensions[i]) {
      OONF_WARN(session->log_source,
          "default extension not found");
      dlep_session_remove(session);
      return -1;
    }
  }

  if (_update_allowed_tlvs(parser)) {
    OONF_WARN(session->log_source,
        "Could not update allowed TLVs for %s", l2_ifname);
    dlep_session_remove(session);
    return -1;
  }

  OONF_INFO(session->log_source, "Add session on %s",
      session->l2_listener.name);
  return 0;
}

void
dlep_session_remove(struct dlep_session *session) {
  struct dlep_parser_tlv *tlv, *tlv_it;
  struct dlep_session_parser *parser;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif

  OONF_DEBUG(session->log_source, "Remove session if %s to %s",
      session->l2_listener.name,
      netaddr_socket_to_string(&nbuf, &session->remote_socket));

  oonf_interface_remove_listener(&session->l2_listener);

  parser = &session->parser;
  avl_for_each_element_safe(&parser->allowed_tlvs, tlv, _node, tlv_it) {
    avl_remove(&parser->allowed_tlvs, &tlv->_node);
    oonf_class_free(&_tlv_class, tlv);
  }

  oonf_timer_stop(&session->local_event_timer);
  oonf_timer_stop(&session->remote_heartbeat_timeout);

  free (parser->extensions);
  parser->extensions = NULL;

  free (parser->values);
  parser->values = NULL;
}

/**
 * Send peer termination
 * @param session dlep session
 */
void
dlep_session_terminate(struct dlep_session *session) {
  if (session->next_signal != DLEP_ALL_SIGNALS) {
    return;
  }

  dlep_session_generate_signal(
      session, DLEP_PEER_TERMINATION, NULL);
  session->cb_send_buffer(session, 0);
  session->next_signal = DLEP_PEER_TERMINATION_ACK;
}

int
dlep_session_update_extensions(struct dlep_session *session,
    const uint8_t *extvalues, size_t extcount) {
  struct dlep_extension **ext_array, *ext;
  size_t count, i;
  uint16_t extid;

  /* keep entry 0 untouched, that is the base extension */
  count = 1;
  for (i=0; i<extcount; i++) {
    memcpy(&extid, &extvalues[i*2], sizeof(extid));

    if (dlep_extension_get(ntohs(extid))) {
      count++;
    }
  }

  if (count != session->parser.extension_count) {
    ext_array = realloc(session->parser.extensions,
        sizeof(struct dlep_extension *) * count);
    if (!ext_array) {
      return -1;
    }

    session->parser.extensions = ext_array;
    session->parser.extension_count = count;
  }

  count = 1;
  for (i=1; i<extcount; i++) {
    memcpy(&extid, &extvalues[i*2], sizeof(extid));
    if ((ext = dlep_extension_get(ntohs(extid)))) {
      session->parser.extensions[count] = ext;
      count++;
    }
  }

  _update_allowed_tlvs(&session->parser);
  return 0;
}

enum oonf_stream_session_state
dlep_session_process_tcp(struct oonf_stream_session *tcp_session,
    struct dlep_session *session) {
  ssize_t processed;

  OONF_DEBUG(session->log_source,
      "Process TCP buffer of %" PRINTF_SIZE_T_SPECIFIER " bytes",
      abuf_getlen(&tcp_session->in));

  processed = dlep_session_process_buffer(session,
      abuf_getptr(&tcp_session->in),
      abuf_getlen(&tcp_session->in));

  OONF_DEBUG(session->log_source,
      "Processed %" PRINTF_SSIZE_T_SPECIFIER " bytes", processed);
  if (processed < 0) {
    return STREAM_SESSION_CLEANUP;
  }

  abuf_pull(&tcp_session->in, processed);

  if (abuf_getlen(session->writer.out) > 0) {
    OONF_DEBUG(session->log_source,
        "Trigger sending %" PRINTF_SIZE_T_SPECIFIER " bytes",
        abuf_getlen(session->writer.out));

    /* send answer */
    oonf_stream_flush(tcp_session);
  }
  if (session->next_signal == DLEP_KILL_SESSION) {
    return STREAM_SESSION_CLEANUP;
  }
  return STREAM_SESSION_ACTIVE;
}

ssize_t
dlep_session_process_buffer(
    struct dlep_session *session, const void *buffer, size_t length) {
  ssize_t result, offset;
  const char *ptr;

  offset = 0;
  ptr = buffer;

  OONF_DEBUG(session->log_source, "Processing buffer of"
      " %" PRINTF_SIZE_T_SPECIFIER " bytes", length);
  while (length > 0) {
    OONF_DEBUG(session->log_source, "Processing message at offset"
        " %" PRINTF_SSIZE_T_SPECIFIER, offset);

    if ((result = dlep_session_process_signal(
        session, &ptr[offset], length)) <= 0){
      if (result < 0) {
        return result;
      }
      break;
    }

    length -= result;
    offset += result;
  }
  return offset;
}

size_t
dlep_session_process_signal(struct dlep_session *session,
    const void *ptr, size_t length) {
  enum dlep_parser_error result;
  uint16_t signal_type;
  uint16_t signal_length;
  const uint8_t *buffer;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf;
#endif
  if (length < 4) {
    /* not enough data for a signal type */
    OONF_DEBUG(session->log_source, "Not enough data to process"
        " signal from %s (%" PRINTF_SIZE_T_SPECIFIER" bytes)",
        netaddr_socket_to_string(&nbuf, &session->remote_socket),
        length);

    return 0;
  }

  buffer = ptr;

  /* copy data */
  memcpy(&signal_type, &buffer[0], sizeof(signal_type));
  memcpy(&signal_length, &buffer[2], sizeof(signal_length));
  signal_type = ntohs(signal_type);
  signal_length = ntohs(signal_length);

  if (length < (size_t)signal_length + 4u) {
    /* not enough data for signal */
    OONF_DEBUG(session->log_source, "Not enough data to process"
        " signal %u (length %u) from %s"
        " (%" PRINTF_SIZE_T_SPECIFIER" bytes)",
        signal_type, signal_length,
        netaddr_socket_to_string(&nbuf, &session->remote_socket),
        length);
    return 0;
  }

  OONF_DEBUG(session->log_source, "Process signal %u (length %u)"
      " from %s (%" PRINTF_SIZE_T_SPECIFIER" bytes)",
      signal_type, signal_length,
      netaddr_socket_to_string(&nbuf, &session->remote_socket),
      length);

  if (session->next_signal != DLEP_ALL_SIGNALS
      && session->next_signal != signal_type) {
    OONF_DEBUG(session->log_source, "Signal should have been %u,"
        " drop session", session->next_signal);
    /* we only accept a single type and we got the wrong one */
    return -1;
  }

  result = _process_tlvs(session,
      signal_type, signal_length, &buffer[4]);
  if (result != DLEP_NEW_PARSER_OKAY) {
    OONF_WARN(session->log_source, "Parser error: %d", result);
    _send_terminate(session);
  }

  /* skip forward */
  return signal_length + 4;
}

struct dlep_local_neighbor *
dlep_session_add_local_neighbor(struct dlep_session *session,
    const struct netaddr *neigh) {
  struct dlep_local_neighbor *local;
  if ((local = dlep_session_get_local_neighbor(session, neigh))) {
    return local;
  }

  local = oonf_class_malloc(&_local_neighbor_class);
  if (!local) {
    return NULL;
  }

  /* hook into tree */
  memcpy(&local->addr, neigh, sizeof(local->addr));
  local->_node.key = &local->addr;
  avl_insert(&session->local_neighbor_tree, &local->_node);

  /* initialize timer */
  local->_ack_timeout.class = &_destination_ack_class;
  local->_ack_timeout.cb_context = local;

  /* initialize backpointer */
  local->session = session;

  return local;
}

void
dlep_session_remove_local_neighbor(struct dlep_session *session,
    struct dlep_local_neighbor *local) {
  avl_remove(&session->local_neighbor_tree, &local->_node);
  oonf_timer_stop(&local->_ack_timeout);
  oonf_class_free(&_local_neighbor_class, local);
}

struct dlep_local_neighbor *
dlep_session_get_local_neighbor(struct dlep_session *session,
    const struct netaddr *neigh) {
  struct dlep_local_neighbor *local;
  return avl_find_element(&session->local_neighbor_tree,
      neigh, local, _node);
}

static int
_generate_signal(struct dlep_session *session, uint16_t signal,
    const struct netaddr *neighbor) {
  struct dlep_extension *ext;
  size_t e,s;

#ifdef OONF_LOG_DEBUG_INFO
  size_t len;
  struct netaddr_str nbuf1, nbuf2;
#endif

  OONF_DEBUG(session->log_source, "Generate signal %u for %s on %s (%s)",
      signal, netaddr_to_string(&nbuf1, neighbor),
      session->l2_listener.name,
      netaddr_socket_to_string(&nbuf2, &session->remote_socket));

#ifdef OONF_LOG_DEBUG_INFO
  len = abuf_getlen(session->writer.out);
#endif
  /* generate signal */
  dlep_writer_start_signal(&session->writer, signal);
  for (e=0; e<session->parser.extension_count; e++) {
    ext = session->parser.extensions[e];

    for (s=0; s<ext->signal_count; s++) {
      if (ext->signals[s].id != signal) {
        continue;
      }

      if (session->radio && ext->signals[s].add_radio_tlvs) {
        OONF_DEBUG(session->log_source,
            "Add tlvs for radio extension %d", ext->id);
        if (ext->signals[s].add_radio_tlvs(ext, session, neighbor)) {
          return -1;
        }
      }
      else if (!session->radio && ext->signals[s].add_router_tlvs) {
        OONF_DEBUG(session->log_source,
            "Add tlvs for router extension %d", ext->id);
        if (ext->signals[s].add_router_tlvs(ext, session, neighbor)) {
          return -1;
        }
      }
      break;
    }
  }

  OONF_DEBUG(session->log_source, "generated %"
      PRINTF_SIZE_T_SPECIFIER " bytes",
      abuf_getlen(session->writer.out) - len);
  return 0;
}

int
dlep_session_generate_signal(struct dlep_session *session, uint16_t signal,
    const struct netaddr *neighbor) {
  if (_generate_signal(session, signal, neighbor)) {
    OONF_DEBUG(session->log_source, "Could not generate signal");
    return -1;
  }
  return dlep_writer_finish_signal(&session->writer, session->log_source);
}

int
dlep_session_generate_signal_status(struct dlep_session *session,
    uint16_t signal, const struct netaddr *neighbor,
    enum dlep_status status, const char *msg) {
  if (_generate_signal(session, signal, neighbor)) {
    return -1;
  }
  if (dlep_writer_add_status(&session->writer, status, msg)) {
    return -1;
  }
  return dlep_writer_finish_signal(&session->writer, session->log_source);
}

struct dlep_parser_value *
dlep_session_get_tlv_value(
    struct dlep_session *session, uint16_t tlvtype) {
  struct dlep_parser_tlv *tlv;

  tlv = dlep_parser_get_tlv(&session->parser, tlvtype);
  if (tlv) {
    return dlep_session_get_tlv_first_value(session, tlv);
  }
  return NULL;
}

static int
_update_allowed_tlvs(struct dlep_session_parser *parser) {
  struct dlep_parser_tlv *tlv, *tlv_it;
  struct dlep_extension *ext;
  size_t e, t;
  uint16_t id;

  /* remove all existing allowed tlvs */
  avl_for_each_element_safe(&parser->allowed_tlvs, tlv, _node, tlv_it) {
    avl_remove(&parser->allowed_tlvs, &tlv->_node);
    oonf_class_free(&_tlv_class, tlv);
  }

  /* allocate new allowed tlvs structures */
  for (e = 0; e < parser->extension_count; e++) {
    ext = parser->extensions[e];

    /* for all extensions */
    for (t = 0; t < ext->tlv_count; t++) {
      /* for all tlvs */
      id = ext->tlvs[t].id;
      tlv = dlep_parser_get_tlv(parser, id);
      if (!tlv) {
        /* new tlv found! */
        if (!(tlv = _add_session_tlv(parser, id))) {
          return -1;
        }
        tlv->length_min = ext->tlvs[t].length_min;
        tlv->length_max = ext->tlvs[t].length_max;
      }
      else if (
          tlv->length_min != ext->tlvs[t].length_min
          || tlv->length_max != ext->tlvs[t].length_max) {
        return -1;
      }
    }
  }
  return 0;
}

static enum dlep_parser_error
_process_tlvs(struct dlep_session *session,
    uint16_t signal_type, uint16_t signal_length, const uint8_t *tlvs) {
  struct dlep_session_parser *parser;
  enum dlep_parser_error result;

  parser = &session->parser;

  /* start at the beginning of the tlvs */
  if ((result = _parse_tlvstream(parser, tlvs, signal_length))) {
    OONF_DEBUG(session->log_source, "parse_tlvstream result: %d", result);
    return result;
  }
  if ((result = _check_mandatory(parser, signal_type))) {
    OONF_DEBUG(session->log_source, "check_mandatory result: %d", result);
    return result;
  }
  if ((result = _check_duplicate(parser, signal_type))) {
    OONF_DEBUG(session->log_source, "check_duplicate result: %d", result);
    return result;
  }

  if ((result = _call_extension_processing(session, signal_type))) {
    OONF_DEBUG(session->log_source,
        "extension processing failed: %d", result);
    return result;
  }

  return DLEP_NEW_PARSER_OKAY;
}

static void
_send_terminate(struct dlep_session *session) {
  if (session->next_signal != DLEP_PEER_DISCOVERY
      && session->next_signal != DLEP_PEER_OFFER) {
    dlep_session_generate_signal(session, DLEP_PEER_TERMINATION, NULL);

    session->next_signal = DLEP_PEER_TERMINATION_ACK;
  }
}

static void
_cb_destination_timeout(void *ptr) {
  struct dlep_local_neighbor *local;

  local = ptr;
  if (local->session->cb_destination_timeout) {
    local->session->cb_destination_timeout(local->session, local);
  }
}

static enum dlep_parser_error
_parse_tlvstream(struct dlep_session_parser *parser,
    const uint8_t *buffer, size_t length) {
  struct dlep_parser_tlv *tlv;
  struct dlep_parser_value *value;
  uint16_t tlv_type;
  uint16_t tlv_length;
  size_t tlv_count, idx;
  int i;

  parser->tlv_ptr = buffer;
  tlv_count = 0;
  idx = 0;

  avl_for_each_element(&parser->allowed_tlvs, tlv, _node) {
    tlv->tlv_first = -1;
    tlv->tlv_last = -1;
  }

  while (idx < length) {
    if (length - idx < 4) {
      /* too short for a TLV, end parsing */
      return DLEP_NEW_PARSER_INCOMPLETE_TLV_HEADER;
    }

    /* copy header */
    memcpy(&tlv_type, &buffer[idx], sizeof(tlv_type));
    idx += sizeof(tlv_type);
    tlv_type = ntohs(tlv_type);

    memcpy(&tlv_length, &buffer[idx], sizeof(tlv_length));
    idx += sizeof(tlv_length);
    tlv_length = ntohs(tlv_length);

    if (idx + tlv_length > length) {
      return DLEP_NEW_PARSER_INCOMPLETE_TLV;
    }

    /* check if tlv is supported */
    tlv = dlep_parser_get_tlv(parser, tlv_type);
    if (!tlv) {
      return DLEP_NEW_PARSER_UNSUPPORTED_TLV;
    }

    /* check length */
    if (tlv->length_max < tlv_length || tlv->length_min > tlv_length) {
      return DLEP_NEW_PARSER_ILLEGAL_TLV_LENGTH;
    }

    /* check if we need to allocate more space for value pointers */
    if (parser->value_max_count == tlv_count) {
      /* allocate more */
      value = realloc(parser->values,
          sizeof(*value) * tlv_count + SESSION_VALUE_STEP);
      if (!value) {
        return DLEP_NEW_PARSER_OUT_OF_MEMORY;
      }
      parser->value_max_count += SESSION_VALUE_STEP;
      parser->values = value;
    }

    /* remember tlv value */
    value = &parser->values[tlv_count];
    value->tlv_next = -1;
    value->index = idx;
    value->length = tlv_length;

    if (tlv->tlv_last == -1) {
      /* first tlv */
      tlv->tlv_first = tlv_count;
    }
    else {
      /* one more */
      value = &parser->values[tlv->tlv_last];
      value->tlv_next = tlv_count;
    }
    tlv->tlv_last  = tlv_count;
    tlv_count++;

    idx += tlv_length;
  }

  avl_for_each_element(&parser->allowed_tlvs, tlv, _node) {
    i = tlv->tlv_first;
    while (i != -1) {
      value = &parser->values[i];

      i = value->tlv_next;
    }
  }
  return DLEP_NEW_PARSER_OKAY;
}

static enum dlep_parser_error
_check_mandatory(struct dlep_session_parser *parser, uint16_t signal_type) {
  struct dlep_parser_tlv *tlv;
  struct dlep_extension_signal *extsig;
  struct dlep_extension *ext;
  size_t e,s,t;

  for (e = 0; e < parser->extension_count; e++) {
    ext = parser->extensions[e];

    extsig = NULL;
    for (s = 0; s < ext->signal_count; s++) {
      if (ext->signals[s].id == signal_type) {
        extsig = &ext->signals[s];
        break;
      }
    }

    if (extsig) {
      for (t = 0; t < extsig->mandatory_tlv_count; t++) {
        tlv = dlep_parser_get_tlv(parser, extsig->mandatory_tlvs[t]);
        if (!tlv) {
          return DLEP_NEW_PARSER_INTERNAL_ERROR;
        }

        if (tlv->tlv_first == -1) {
          return DLEP_NEW_PARSER_MISSING_MANDATORY_TLV;
        }
      }
    }
  }
  return DLEP_NEW_PARSER_OKAY;
}

static enum dlep_parser_error
_check_duplicate(struct dlep_session_parser *parser, uint16_t signal_type) {
  struct dlep_parser_tlv *tlv;
  struct dlep_extension_signal *extsig;
  struct dlep_extension *ext;
  size_t e, s, t;
  bool okay;

  avl_for_each_element(&parser->allowed_tlvs, tlv, _node) {
    if (tlv->tlv_first == tlv->tlv_last) {
      continue;
    }

    /* multiple tlvs of the same kind */
    okay = false;
    for (e = 0; e < parser->extension_count && !okay; e++) {
      ext = parser->extensions[e];

      extsig = NULL;
      for (s = 0; s < ext->signal_count; s++) {
        extsig = &ext->signals[s];
        if (ext->signals[s].id == signal_type) {
          extsig = &ext->signals[s];
          break;
        }
      }

      if (extsig) {
        for (t = 0; t < extsig->duplicate_tlv_count; t++) {
          if (extsig->duplicate_tlvs[t] == tlv->id) {
            okay = true;
            break;
          }
        }
      }
    }
    if (!okay) {
      return DLEP_NEW_PARSER_DUPLICATE_TLV;
    }
  }
  return DLEP_NEW_PARSER_OKAY;
}

static enum dlep_parser_error
_call_extension_processing(struct dlep_session *session, uint16_t signal_type) {
  struct dlep_extension *ext;
  size_t e, s;

  for (e=0; e<session->parser.extension_count; e++) {
    ext = session->parser.extensions[e];

    for (s=0; s<ext->signal_count; s++) {
      if (ext->signals[s].id != signal_type) {
        continue;
      }

      if (session->radio) {
        if (ext->signals[s].process_radio(ext, session)) {
          OONF_DEBUG(session->log_source,
              "Error in radio signal processing of extension '%s'", ext->name);
          return -1;
        }
      }
      else {
        if (ext->signals[s].process_router(ext, session)) {
          OONF_DEBUG(session->log_source,
              "Error in router signal processing of extension '%s'", ext->name);
          return -1;
        }
      }
      break;
    }
  }
  return DLEP_NEW_PARSER_OKAY;
}

static struct dlep_parser_tlv *
_add_session_tlv(struct dlep_session_parser *parser, uint16_t id) {
  struct dlep_parser_tlv *tlv;

  tlv = oonf_class_malloc(&_tlv_class);
  if (!tlv) {
    return NULL;
  }

  tlv->id = id;
  tlv->_node.key = &tlv->id;
  tlv->tlv_first = -1;
  tlv->tlv_last  = -1;

  avl_insert(&parser->allowed_tlvs, &tlv->_node);
  return tlv;
}
