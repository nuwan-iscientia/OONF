/*
 * dlep_router_interface.h
 *
 *  Created on: Oct 1, 2014
 *      Author: rogge
 */

#ifndef DLEP_ROUTER_INTERFACE_H_
#define DLEP_ROUTER_INTERFACE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_timer.h"

enum dlep_router_state {
  DLEP_ROUTER_DISCOVERY,
  DLEP_ROUTER_CONNECT,
  DLEP_ROUTER_ACTIVE,
};

struct dlep_router_if {
  /* interface name to talk with DLEP radio */
  char name[IF_NAMESIZE];

  /* state of the DLEP session */
  enum dlep_router_state state;

  /* UDP socket for discovery */
  struct oonf_packet_managed udp;
  struct oonf_packet_managed_config udp_config;

  /* event timer (either discovery or heartbeat) */
  struct oonf_timer_instance discovery_timer;

  /* local timer settings */
  uint64_t local_discovery_interval;
  uint64_t local_heartbeat_interval;

  /* heartbeat settings from the other side of the session */
  uint64_t remote_heartbeat_interval;

  /* hook into session tree, interface name is the key */
  struct avl_node _node;

  /* list of all streams on this interface */
  struct avl_tree stream_tree;
};

EXPORT int dlep_router_interface_init(void);
EXPORT void dlep_router_interface_cleanup(void);

EXPORT struct dlep_router_if *dlep_router_get_interface(const char *ifname);
EXPORT struct dlep_router_if *dlep_router_add_interface(const char *ifname);
EXPORT void dlep_router_remove_interface(struct dlep_router_if *);

EXPORT void dlep_router_apply_interface_settings(struct dlep_router_if *);

#endif /* DLEP_ROUTER_INTERFACE_H_ */
