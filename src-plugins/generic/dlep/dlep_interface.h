/*
 * dlep_interface.h
 *
 *  Created on: Jul 8, 2015
 *      Author: rogge
 */

#ifndef DLEP_INTERFACE_H_
#define DLEP_INTERFACE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_packet_socket.h"

/**
 * Interface that is used for one or multiple dlep sessions
 */
struct dlep_if {
  /*! pseudo session for UDP communication on interface */
  struct dlep_session session;

  /*! name of layer2 interface */
  char l2_ifname[IF_NAMESIZE];

  /*! UDP socket for discovery */
  struct oonf_packet_managed udp;

  /*! UDP socket configuration */
  struct oonf_packet_managed_config udp_config;

  /*! dynamic buffer for UDP output */
  struct autobuf udp_out;

  /*! true if radio should only accept a single session */
  bool single_session;

  /*! hook into session tree, interface name is the key */
  struct avl_node _node;

  /*! tree of all radio sessions */
  struct avl_tree session_tree;
};

int dlep_if_add(struct dlep_if *interf, const char *ifname,
    uint32_t l2_origin, enum oonf_log_source log_src, bool radio);
void dlep_if_remove(struct dlep_if *interface);

#endif /* DLEP_INTERFACE_H_ */
