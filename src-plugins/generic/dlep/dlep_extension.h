/*
 * dlep_extension.h
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#ifndef _DLEP_EXTENSION_H_
#define _DLEP_EXTENSION_H_

struct dlep_extension;

#include "common/common_types.h"
#include "common/avl.h"
#include "common/autobuf.h"

#include "subsystems/oonf_layer2.h"

#include "dlep/dlep_session.h"

enum {
  DLEP_EXTENSION_BASE      = -1,
};

struct dlep_extension_signal {
    /* signal id */
    uint16_t id;

    /* array of supported tlv ids */
    const uint16_t *supported_tlvs;
    size_t supported_tlv_count;

    /* array of mandatory tlv ids */
    const uint16_t *mandatory_tlvs;
    size_t mandatory_tlv_count;

    /* array of tlvs that are allowed multiple times */
    const uint16_t *duplicate_tlvs;
    size_t duplicate_tlv_count;

    enum dlep_parser_error (*process_radio)(struct dlep_session *session);
    enum dlep_parser_error (*process_router)(struct dlep_session *session);
    int (*add_radio_tlvs)(struct dlep_session *session, const struct netaddr *);
    int (*add_router_tlvs)(struct dlep_session *session, const struct netaddr *);
};

struct dlep_extension_tlv {
    uint16_t id;
    uint16_t length_min;
    uint16_t length_max;
};

struct dlep_extension_implementation {
    uint16_t id;

    enum dlep_parser_error (*process)(struct dlep_session *session);
    int (*add_tlvs)(struct dlep_session *session, const struct netaddr *);
};

struct dlep_neighbor_mapping {
    uint16_t dlep;
    uint16_t length;

    enum oonf_layer2_neighbor_index layer2;

    int (*from_tlv)(struct oonf_layer2_data *, struct dlep_session *,
        uint16_t tlv);
    int (*to_tlv)(struct dlep_writer *, struct oonf_layer2_data *,
        uint16_t tlv, uint16_t length);
};

struct dlep_network_mapping {
    uint16_t dlep;
    uint16_t length;

    enum oonf_layer2_network_index layer2;

    int (*from_tlv)(struct oonf_layer2_data *, struct dlep_session *,
        uint16_t tlv);
    int (*to_tlv)(struct dlep_writer *, struct oonf_layer2_data *,
        uint16_t tlv, uint16_t length);
};

struct dlep_extension {
    /* id of dlep extension, -1 for base protocol */
    int id;

    /* name of extension for debugging purpose */
    const char *name;

    /* array of dlep signals used by this extension */
    struct dlep_extension_signal *signals;
    size_t signal_count;

    /* array of dlep tlvs used by this extension */
    struct dlep_extension_tlv *tlvs;
    size_t tlv_count;

    /*
     * array of id mappings between DLEP tlvs
     * and oonf-layer2 neighbor data
     */
    struct dlep_neighbor_mapping *neigh_mapping;
    size_t neigh_mapping_count;

    /*
     * array of id mappings between DLEP tlvs
     * and oonf-layer2 network data
     */
    struct dlep_network_mapping *if_mapping;
    size_t if_mapping_count;

    /* callbacks for session creation and teardown */
    void (*cb_session_init_radio)(struct dlep_session *);
    void (*cb_session_init_router)(struct dlep_session *);
    void (*cb_session_apply_radio)(struct dlep_session *);
    void (*cb_session_apply_router)(struct dlep_session *);
    void (*cb_session_cleanup_radio)(struct dlep_session *);
    void (*cb_session_cleanup_router)(struct dlep_session *);

    /* node for global tree of extensions */
    struct avl_node _node;
};

EXPORT void dlep_extension_init(void);
EXPORT void dlep_extension_add(struct dlep_extension *);
EXPORT struct avl_tree *dlep_extension_get_tree(void);
EXPORT void dlep_extension_add_processing(struct dlep_extension *, bool radio,
    struct dlep_extension_implementation *proc, size_t proc_count);
EXPORT const uint16_t *dlep_extension_get_ids(uint16_t *length);

static INLINE struct dlep_extension *
dlep_extension_get(int32_t id) {
  struct dlep_extension *ext;
  return avl_find_element(dlep_extension_get_tree(), &id, ext, _node);
}

#endif /* _DLEP_EXTENSION_H_ */
