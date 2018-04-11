
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

/**
 * @file
 */

#include "common/autobuf.h"
#include "common/avl.h"
#include "common/common_types.h"
#include "subsystems/oonf_timer.h"

#include "dlep/dlep_extension.h"
#include "dlep/dlep_iana.h"
#include "dlep/dlep_reader.h"
#include "dlep/dlep_writer.h"

#include "dlep/ext_lid/lid.h"

static int _write_lid_only(struct dlep_extension *ext, struct dlep_session *session, const struct oonf_layer2_neigh_key *neigh);
static void _cb_session_init(struct dlep_session *session);
static void _cb_session_deactivate(struct dlep_session *session);

/* UDP peer discovery */

/* destination up */
static const uint16_t _dst_up_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _dst_up_mandatory[] = {
  DLEP_LID_TLV,
};

/* destination up ack */
static const uint16_t _dst_up_ack_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _dst_up_ack_mandatory[] = {
  DLEP_LID_TLV,
};

/* destination down */
static const uint16_t _dst_down_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _dst_down_mandatory[] = {
  DLEP_LID_TLV,
};

/* destination down ack */
static const uint16_t _dst_down_ack_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _dst_down_ack_mandatory[] = {
  DLEP_LID_TLV,
};

/* destination update */
static const uint16_t _dst_update_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _dst_update_mandatory[] = {
  DLEP_MAC_ADDRESS_TLV,
};

/* link characteristics request */
static const uint16_t _linkchar_req_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _linkchar_req_mandatory[] = {
  DLEP_LID_TLV,
};

/* link characteristics ack */
static const uint16_t _linkchar_ack_tlvs[] = {
  DLEP_LID_TLV,
};
static const uint16_t _linkchar_ack_mandatory[] = {
  DLEP_LID_TLV,
};

/* supported signals of this extension, parsing the LID TLV is done by dlep_extension */
static struct dlep_extension_signal _signals[] = {
  {
    .id = DLEP_DESTINATION_UP,
    .supported_tlvs = _dst_up_tlvs,
    .supported_tlv_count = ARRAYSIZE(_dst_up_tlvs),
    .mandatory_tlvs = _dst_up_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_dst_up_mandatory),
    .add_radio_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_DESTINATION_UP_ACK,
    .supported_tlvs = _dst_up_ack_tlvs,
    .supported_tlv_count = ARRAYSIZE(_dst_up_ack_tlvs),
    .mandatory_tlvs = _dst_up_ack_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_dst_up_ack_mandatory),
    .add_router_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_DESTINATION_DOWN,
    .supported_tlvs = _dst_down_tlvs,
    .supported_tlv_count = ARRAYSIZE(_dst_down_tlvs),
    .mandatory_tlvs = _dst_down_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_dst_down_mandatory),
    .add_radio_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_DESTINATION_DOWN_ACK,
    .supported_tlvs = _dst_down_ack_tlvs,
    .supported_tlv_count = ARRAYSIZE(_dst_down_ack_tlvs),
    .mandatory_tlvs = _dst_down_ack_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_dst_down_ack_mandatory),
    .add_router_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_DESTINATION_UPDATE,
    .supported_tlvs = _dst_update_tlvs,
    .supported_tlv_count = ARRAYSIZE(_dst_update_tlvs),
    .mandatory_tlvs = _dst_update_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_dst_update_mandatory),
    .add_radio_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_LINK_CHARACTERISTICS_REQUEST,
    .supported_tlvs = _linkchar_req_tlvs,
    .supported_tlv_count = ARRAYSIZE(_linkchar_req_tlvs),
    .mandatory_tlvs = _linkchar_req_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_linkchar_req_mandatory),
    .add_router_tlvs = _write_lid_only,
  },
  {
    .id = DLEP_LINK_CHARACTERISTICS_ACK,
    .supported_tlvs = _linkchar_ack_tlvs,
    .supported_tlv_count = ARRAYSIZE(_linkchar_ack_tlvs),
    .mandatory_tlvs = _linkchar_ack_mandatory,
    .mandatory_tlv_count = ARRAYSIZE(_linkchar_ack_mandatory),
    .add_radio_tlvs = _write_lid_only,
  },
};

/* supported TLVs of this extension */
static struct dlep_extension_tlv _tlvs[] = {
  { DLEP_LID_TLV, 1, OONF_LAYER2_MAX_LINK_ID },
};

/* DLEP base extension, radio side */
static struct dlep_extension _lid = {
  .id = DLEP_EXTENSION_LINK_ID,
  .name = "linkid",

  .signals = _signals,
  .signal_count = ARRAYSIZE(_signals),
  .tlvs = _tlvs,
  .tlv_count = ARRAYSIZE(_tlvs),

  .cb_session_init_radio = _cb_session_init,
  .cb_session_init_router = _cb_session_init,
  .cb_session_deactivate_radio = _cb_session_deactivate,
  .cb_session_deactivate_router = _cb_session_deactivate,
};

/**
 * Get link-id DLEP extension
 * @return this extension
 */
struct dlep_extension *
dlep_lid_init(void) {
  dlep_extension_add(&_lid);
  return &_lid;
}

/**
 * Write the link-id TLV into the DLEP message
 * @param ext (this) dlep extension
 * @param session dlep session
 * @param neigh layer2 neighbor key to write into TLV
 * @return -1 if an error happened, 0 otherwise
 */
static int
_write_lid_only(
  struct dlep_extension *ext __attribute__((unused)), struct dlep_session *session, const struct oonf_layer2_neigh_key *neigh) {
  if (dlep_writer_add_lid_tlv(&session->writer, neigh)) {
    return -1;
  }
  return 0;
}

static void
_cb_session_init(struct dlep_session *session) {
  session->allow_lids = true;
}

static void
_cb_session_deactivate(struct dlep_session *session) {
  session->allow_lids = false;
}
