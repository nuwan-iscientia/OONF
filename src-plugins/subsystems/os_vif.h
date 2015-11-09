
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

#ifndef OS_VIF_H_
#define OS_VIF_H_

#include "common/common_types.h"
#include "common/avl.h"

/*! subsystem identifier */
#define OONF_OS_VIF_SUBSYSTEM "os_vif"

/* include os-specific headers */
#if defined(__linux__)
#include "subsystems/os_linux/os_vif_linux.h"
#else
#error "Unknown operation system"
#endif

/**
 * types of virtual interfaces
 */
enum vif_type {
  /*! IP level virtual interface */
  OS_VIF_IP,

  /*! MAC level virtual interface */
  OS_VIF_MAC,
};

/**
 * Definition of a virtual interface
 */
struct os_vif {
  /*! name of virtual interface */
  const char *if_name;

  /*! file descriptor of interface, is being set by API */
  int fd;

  /*! type of virtual interface */
  enum vif_type type;

  /*! hook into global tree of virtual interfaces */
  struct avl_node _vif_node;

  /*! os specific virtual interface data */
  struct os_vif_internal _internal;
};

EXPORT int os_vif_open(struct os_vif *vif);
EXPORT void os_vif_close(struct os_vif *vif);

EXPORT struct avl_tree *os_vif_get_tree(void);

/**
 * Get virtual interface handler
 * @param name name of virtual interface
 * @return handler, NULL if not found
 */
static INLINE struct os_vif *
os_vif_get(const char *name) {
  struct os_vif *vif;
  return avl_find_element(os_vif_get_tree(), name, vif, _vif_node);
}

#endif /* OS_VIF_H_ */
