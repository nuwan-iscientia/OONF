/*
 * dlep_extension.c
 *
 *  Created on: Jun 25, 2015
 *      Author: rogge
 */

#include <stdlib.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"

#include "dlep/dlep_extension.h"

static struct avl_tree _extension_tree;

static uint16_t *_id_array = NULL;
static uint16_t _id_array_length = 0;

void
dlep_extension_init(void) {
  avl_init(&_extension_tree, avl_comp_int32, false);
}

void
dlep_extension_add(struct dlep_extension *ext) {
  uint16_t *ptr;

  if (avl_is_node_added(&ext->_node)) {
    return;
  }

  ptr = realloc(_id_array, sizeof(uint16_t) * _extension_tree.count);
  if (!ptr) {
    return;
  }

  /* add to tree */
  ext->_node.key = &ext->id;
  avl_insert(&_extension_tree, &ext->_node);

  /* refresh id array */
  _id_array_length = 0;
  _id_array = ptr;

  avl_for_each_element(&_extension_tree, ext, _node) {
    if (ext->id >= 0 && ext->id <= 0xffff) {
      ptr[_id_array_length] = ext->id;
      _id_array_length++;
    }
  }
}

struct avl_tree *
dlep_extension_get_tree(void) {
  return &_extension_tree;
}

void
dlep_extension_add_processing(struct dlep_extension *ext, bool radio,
    struct dlep_extension_implementation *processing, size_t proc_count) {
  size_t i,j;

  for (j=0; j<proc_count; j++) {
    for (i=0; i<ext->signal_count; i++) {
      if (ext->signals[i].id == processing[j].id) {
        if (radio) {
          ext->signals[i].process_radio = processing[j].process;
          ext->signals[i].add_radio_tlvs = processing[j].add_tlvs;
        }
        else {
          ext->signals[i].process_router = processing[j].process;
          ext->signals[i].add_router_tlvs = processing[j].add_tlvs;
        }
        break;
      }
    }
  }
}

const uint16_t *
dlep_extension_get_ids(uint16_t *length) {
  *length = _id_array_length;
  return _id_array;
}
