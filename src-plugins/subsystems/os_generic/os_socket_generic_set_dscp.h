/*
 * os_socket_generic_set_dscp.h
 *
 *  Created on: Jan 12, 2016
 *      Author: rogge
 */

#ifndef _OS_SOCKET_GENERIC_SET_DSCP_H_
#define _OS_SOCKET_GENERIC_SET_DSCP_H_

#include "common/common_types.h"
#include "subsystems/os_socket.h"

EXPORT int os_socket_generic_set_dscp(struct os_socket *sock, int dscp, bool ipv6);

#endif /* _OS_SOCKET_GENERIC_SET_DSCP_H_ */
