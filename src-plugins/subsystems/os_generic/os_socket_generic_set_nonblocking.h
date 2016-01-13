/*
 * os_socket_generic_set_nonblocking.h
 *
 *  Created on: Jan 12, 2016
 *      Author: rogge
 */

#ifndef _OS_SOCKET_GENERIC_SET_NONBLOCKING_H_
#define _OS_SOCKET_GENERIC_SET_NONBLOCKING_H_

#include "common/common_types.h"
#include "subsystems/os_socket.h"

EXPORT int os_socket_generic_set_nonblocking(struct os_socket *sock);

#endif /* _OS_SOCKET_GENERIC_SET_NONBLOCKING_H_ */
