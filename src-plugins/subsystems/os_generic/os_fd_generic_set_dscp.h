/*
 * os_fd_generic_set_dscp.h
 *
 *  Created on: Jan 12, 2016
 *      Author: rogge
 */

#ifndef _OS_FD_GENERIC_SET_DSCP_H_
#define _OS_FD_GENERIC_SET_DSCP_H_

#include "common/common_types.h"
#include "subsystems/os_socket.h"

EXPORT int os_fd_generic_set_dscp(struct os_fd *sock, int dscp, bool ipv6);

#endif /* _OS_FD_GENERIC_SET_DSCP_H_ */
