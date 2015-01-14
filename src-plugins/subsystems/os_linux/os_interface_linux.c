
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
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

/* must be first because of a problem with linux/rtnetlink.h */
#include <sys/socket.h>

/* and now the rest of the includes */
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/socket.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "common/common_types.h"
#include "common/string.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_system.h"

#include "subsystems/os_interface.h"

/* Definitions */
#define LOG_OS_INTERFACE _oonf_os_interface_subsystem.logging

/* ip forwarding */
#define PROC_IPFORWARD_V4 "/proc/sys/net/ipv4/ip_forward"
#define PROC_IPFORWARD_V6 "/proc/sys/net/ipv6/conf/all/forwarding"

/* Redirect proc entry */
#define PROC_IF_REDIRECT "/proc/sys/net/ipv4/conf/%s/send_redirects"
#define PROC_ALL_REDIRECT "/proc/sys/net/ipv4/conf/all/send_redirects"

/* IP spoof proc entry */
#define PROC_IF_SPOOF "/proc/sys/net/ipv4/conf/%s/rp_filter"
#define PROC_ALL_SPOOF "/proc/sys/net/ipv4/conf/all/rp_filter"

/* Interface base index */
#define SYSFS_BASE_IFINDEX "/sys/class/net/%s/iflink"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_rtnetlink_message(struct nlmsghdr *hdr);
static void _cb_rtnetlink_error(uint32_t seq, int error);
static void _cb_rtnetlink_done(uint32_t seq);
static void _cb_rtnetlink_timeout(void);
static void _address_finished(struct os_interface_address *addr, int error);

static void _activate_if_routing(void);
static void _deactivate_if_routing(void);
static bool _is_at_least_linuxkernel_2_6_31(void);
static int _os_linux_writeToFile(const char *file, char *old, char value);
static unsigned _os_linux_get_base_ifindex(const char *interf);

/* ioctl socket */
static int _ioctl_fd = -1;

/* list of interface change listeners */
static struct list_entity _ifchange_listener;

/* subsystem definition */
static const char *_dependencies[] = {
  OONF_OS_SYSTEM_SUBSYSTEM,
};

static struct oonf_subsystem _oonf_os_interface_subsystem = {
  .name = OONF_OS_INTERFACE_SUBSYSTEM,
  .dependencies = _dependencies,
  .dependencies_count = ARRAYSIZE(_dependencies),
  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(_oonf_os_interface_subsystem);

/* built in rtnetlink receiver */
static struct os_system_netlink _rtnetlink_receiver = {
  .used_by = &_oonf_os_interface_subsystem,
  .cb_message = _cb_rtnetlink_message,
  .cb_error = _cb_rtnetlink_error,
  .cb_done = _cb_rtnetlink_done,
  .cb_timeout = _cb_rtnetlink_timeout,
};

static struct list_entity _rtnetlink_feedback;

static const uint32_t _rtnetlink_mcast[] = {
  RTNLGRP_LINK, RTNLGRP_IPV4_IFADDR, RTNLGRP_IPV6_IFADDR
};

/* global procfile state before initialization */
static char _original_rp_filter;
static char _original_icmp_redirect;
static char _original_ipv4_forward;
static char _original_ipv6_forward;

/* counter of mesh interfaces for ip_forward configuration */
static int _mesh_count = 0;

/**
 * Initialize os-specific subsystem
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  _ioctl_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_fd == -1) {
    OONF_WARN(LOG_OS_INTERFACE, "Cannot open ioctl socket: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  if (os_system_netlink_add(&_rtnetlink_receiver, NETLINK_ROUTE)) {
    close(_ioctl_fd);
    return -1;
  }

  if (os_system_netlink_add_mc(&_rtnetlink_receiver, _rtnetlink_mcast, ARRAYSIZE(_rtnetlink_mcast))) {
    os_system_netlink_remove(&_rtnetlink_receiver);
    close(_ioctl_fd);
    return -1;
  }

  list_init_head(&_ifchange_listener);
  return 0;
}

/**
 * Cleanup os-specific subsystem
 */
static void
_cleanup(void) {
  os_system_netlink_remove(&_rtnetlink_receiver);
  close(_ioctl_fd);
}

/**
 * Set interface up or down
 * @param dev pointer to name of interface
 * @param up true if interface should be up, false if down
 * @return -1 if an error happened, 0 otherwise
 */
int
os_interface_state_set(const char *dev, bool up) {
  int oldflags;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, dev, IFNAMSIZ);

  if (ioctl(_ioctl_fd, SIOCGIFFLAGS, &ifr) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
        "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        dev, strerror(errno), errno);
    return -1;
  }

  oldflags = ifr.ifr_flags;
  if (up) {
    ifr.ifr_flags |= IFF_UP;
  }
  else {
    ifr.ifr_flags &= ~IFF_UP;
  }

  if (oldflags == ifr.ifr_flags) {
    /* interface is already up/down */
    return 0;
  }

  if (ioctl(_ioctl_fd, SIOCSIFFLAGS, &ifr) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
        "ioctl SIOCSIFFLAGS (set flags %s) error on device %s: %s (%d)\n",
        up ? "up" : "down", dev, strerror(errno), errno);
    return -1;
  }
  return 0;
}

void
os_interface_listener_add(struct os_interface_if_listener *listener) {
  list_add_tail(&_ifchange_listener, &listener->_node);
}

void
os_interface_listener_remove(struct os_interface_if_listener *listener) {
  list_remove(&listener->_node);
}

int
os_interface_address_set(struct os_interface_address *addr) {
  uint8_t buffer[UIO_MAXIOV];
  struct nlmsghdr *msg;
  struct ifaddrmsg *ifaddrreq;
  int seq;
#if defined(OONF_LOG_DEBUG_INFO)
  struct netaddr_str nbuf;
#endif

  memset(buffer, 0, sizeof(buffer));

  /* get pointers for netlink message */
  msg = (void *)&buffer[0];

  if (addr->set) {
    msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_REPLACE | NLM_F_ACK;
    msg->nlmsg_type = RTM_NEWADDR;
  }
  else {
    msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    msg->nlmsg_type = RTM_DELADDR;
  }

  /* set length of netlink message with ifaddrmsg payload */
  msg->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));

  OONF_DEBUG(LOG_OS_INTERFACE, "%sset address on if %d: %s",
      addr->set ? "" : "re", addr->if_index,
      netaddr_to_string(&nbuf, &addr->address));

  ifaddrreq = NLMSG_DATA(msg);
  ifaddrreq->ifa_family = netaddr_get_address_family(&addr->address);
  ifaddrreq->ifa_prefixlen = netaddr_get_prefix_length(&addr->address);
  ifaddrreq->ifa_index= addr->if_index;
  ifaddrreq->ifa_scope = addr->scope;

  if (os_system_netlink_addnetaddr(msg, IFA_LOCAL, &addr->address)) {
    return -1;
  }

  /* cannot fail */
  seq = os_system_netlink_send(&_rtnetlink_receiver, msg);

  if (addr->cb_finished) {
    list_add_tail(&_rtnetlink_feedback, &addr->_internal._node);
    addr->_internal.nl_seq = seq;
  }
  return 0;
}

void
os_interface_address_interrupt(struct os_interface_address *addr) {
  if (list_is_node_added(&addr->_internal._node)) {
    /* remove first to prevent any kind of recursive cleanup */
    list_remove(&addr->_internal._node);

    if (addr->cb_finished) {
      addr->cb_finished(addr, -1);
    }
  }
}

/**
 * Updates the data of an interface.
 * The interface data object will be completely overwritten
 * @param ifdata pointer to an interface data object
 * @param name name of interface
 * @return -1 if an error happened, 0 otherwise
 */
int
os_interface_update(struct os_interface_data *ifdata,
    const char *name) {
  struct ifreq ifr;
  struct ifaddrs *ifaddrs;
  struct ifaddrs *ifa;
  size_t addrcount;
  union netaddr_socket *sock;
  struct netaddr *addr, *prefix, netmask;
#ifdef OONF_LOG_INFO
  struct netaddr_str nbuf1;
#endif
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str nbuf2, nbuf3;
#endif

  /* cleanup data structure */
  if (ifdata->addresses) {
    free(ifdata->addresses);
  }

  memset(ifdata, 0, sizeof(*ifdata));
  strscpy(ifdata->name, name, sizeof(ifdata->name));

  /* get interface index */
  ifdata->index = if_nametoindex(name);
  if (ifdata->index == 0) {
    /* interface is not there at the moment */
    return 0;
  }

  ifdata->base_index = _os_linux_get_base_ifindex(name);

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, ifdata->name, IF_NAMESIZE);

  if (ioctl(os_system_linux_get_ioctl_fd(AF_INET), SIOCGIFFLAGS, &ifr) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
        "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        ifdata->name, strerror(errno), errno);
    return -1;
  }

  ifdata->up = (ifr.ifr_flags & IFF_UP) == IFF_UP;
  ifdata->loopback = (ifr.ifr_flags & IFF_LOOPBACK) != 0;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, ifdata->name, IF_NAMESIZE);

  if (ioctl(os_system_linux_get_ioctl_fd(AF_INET), SIOCGIFHWADDR, &ifr) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
        "ioctl SIOCGIFHWADDR (get flags) error on device %s: %s (%d)\n",
        ifdata->name, strerror(errno), errno);
    return -1;
  }

  netaddr_from_binary(&ifdata->mac, ifr.ifr_hwaddr.sa_data, 6, AF_MAC48);
  OONF_INFO(LOG_OS_INTERFACE, "Interface %s has mac address %s",
      ifdata->name, netaddr_to_string(&nbuf1, &ifdata->mac));

  /* get ip addresses */
  ifaddrs = NULL;
  addrcount = 0;

  if (getifaddrs(&ifaddrs)) {
    OONF_WARN(LOG_OS_INTERFACE,
        "getifaddrs() failed: %s (%d)", strerror(errno), errno);
    return -1;
  }

  for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (strcmp(ifdata->name, ifa->ifa_name) == 0 &&
        (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
      addrcount++;
    }
  }

  ifdata->addresses = calloc(addrcount*2, sizeof(struct netaddr));
  if (ifdata->addresses == NULL) {
    OONF_WARN(LOG_OS_INTERFACE,
        "Cannot allocate memory for interface %s with %"PRINTF_SIZE_T_SPECIFIER" prefixes",
        ifdata->name, addrcount);
    freeifaddrs(ifaddrs);
    return -1;
  }

  ifdata->prefixes = &ifdata->addresses[addrcount];

  ifdata->if_v4 = &NETADDR_UNSPEC;
  ifdata->if_v6 = &NETADDR_UNSPEC;
  ifdata->linklocal_v6_ptr = &NETADDR_UNSPEC;

  for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (strcmp(ifdata->name, ifa->ifa_name) == 0 &&
        (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
      sock = (union netaddr_socket *)ifa->ifa_addr;
      addr = &ifdata->addresses[ifdata->addrcount];

      /* get address of interface */
      if (netaddr_from_socket(addr, sock) == 0) {
        ifdata->addrcount++;

        sock = (union netaddr_socket *)ifa->ifa_netmask;
        prefix = &ifdata->prefixes[ifdata->prefixcount];

        /* get corresponding prefix if possible */
        if (!netaddr_from_socket(&netmask, sock)) {
          if (!netaddr_create_prefix(prefix, addr, &netmask, false)) {
            OONF_DEBUG(LOG_OS_INTERFACE, "Address %s and Netmask %s produce prefix %s",
                netaddr_to_string(&nbuf1, addr),
                netaddr_to_string(&nbuf2, &netmask),
                netaddr_to_string(&nbuf3, prefix));
            ifdata->prefixcount++;
          }
        }

        if (netaddr_get_address_family(addr) == AF_INET) {
          if (!netaddr_is_in_subnet(&NETADDR_IPV4_MULTICAST, addr)) {
            ifdata->if_v4 = addr;
          }
        }
        else if (netaddr_get_address_family(addr) == AF_INET6) {
          if (netaddr_is_in_subnet(&NETADDR_IPV6_LINKLOCAL, addr)) {
            ifdata->linklocal_v6_ptr = addr;
          }
          else if (!(netaddr_is_in_subnet(&NETADDR_IPV6_MULTICAST, addr)
              || netaddr_is_in_subnet(&NETADDR_IPV6_IPV4COMPATIBLE, addr)
              || netaddr_is_in_subnet(&NETADDR_IPV6_IPV4MAPPED, addr))) {
            ifdata->if_v6 = addr;
          }
        }
      }
    }
  }

  freeifaddrs(ifaddrs);
  return 0;
}

/**
 * Initialize interface for mesh usage
 * @param interf pointer to interface object
 * @return -1 if an error happened, 0 otherwise
 */
int
os_interface_init_mesh(struct os_interface *interf) {
  char procfile[FILENAME_MAX];
  char old_redirect = 0, old_spoof = 0;

  if (interf->data.loopback) {
    /* ignore loopback */
    return 0;
  }

  /* handle global ip_forward setting */
  _mesh_count++;
  if (_mesh_count == 1) {
    _activate_if_routing();
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (_os_linux_writeToFile(procfile, &old_redirect, '0')) {
    OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (_os_linux_writeToFile(procfile, &old_spoof, '0')) {
    OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not disable the IP spoof filter! "
        "You should mannually ensure that IP spoof filtering is disabled!");
  }

  interf->_original_state = (old_redirect << 8) | (old_spoof);
  return 0;
}

/**
 * Cleanup interface after mesh usage
 * @param interf pointer to interface object
 */
void
os_interface_cleanup_mesh(struct os_interface *interf) {
  char restore_redirect, restore_spoof;
  char procfile[FILENAME_MAX];

  if (interf->data.loopback) {
    /* ignore loopback */
    return;
  }

  restore_redirect = (interf->_original_state >> 8) & 255;
  restore_spoof = (interf->_original_state & 255);

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (_os_linux_writeToFile(procfile, NULL, restore_redirect) != 0) {
    OONF_WARN(LOG_OS_INTERFACE, "Could not restore ICMP redirect flag %s to %c",
        procfile, restore_redirect);
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (_os_linux_writeToFile(procfile, NULL, restore_spoof) != 0) {
    OONF_WARN(LOG_OS_INTERFACE, "Could not restore IP spoof flag %s to %c",
        procfile, restore_spoof);
  }

  /* handle global ip_forward setting */
  _mesh_count--;
  if (_mesh_count == 0) {
    _deactivate_if_routing();
  }

  interf->_original_state = 0;
  return;
}

/**
 * Set the mac address of an interface
 * @param name name of interface
 * @param mac mac address
 * @return -1 if an error happened, 0 otherwise
 */
int
os_interface_mac_set_by_name(const char *name, struct netaddr *mac) {
  struct ifreq if_req;
  struct netaddr_str nbuf;

  if (netaddr_get_address_family(mac) != AF_MAC48) {
    OONF_WARN(LOG_OS_INTERFACE, "Interface MAC must mac48, not %s",
        netaddr_to_string(&nbuf, mac));
    return -1;
  }

  memset(&if_req, 0, sizeof(if_req));
  strscpy(if_req.ifr_name, name, IF_NAMESIZE);

  if_req.ifr_addr.sa_family = ARPHRD_ETHER;
  netaddr_to_binary(&if_req.ifr_addr.sa_data, mac, 6);

  if (ioctl(os_system_linux_get_ioctl_fd(AF_INET), SIOCSIFHWADDR, &if_req) < 0) {
    OONF_WARN(LOG_OS_INTERFACE, "Could not set mac address of '%s': %s (%d)",
        name, strerror(errno), errno);
    return -1;
  }
  return 0;
}

static unsigned
_os_linux_get_base_ifindex(const char *interf) {
  char sysfile[FILENAME_MAX];
  char ifnumber[11];
  int fd;
  ssize_t len;

  /* Generate the sysfs name */
  snprintf(sysfile, sizeof(sysfile), SYSFS_BASE_IFINDEX, interf);

  if ((fd = open(sysfile, O_RDONLY)) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
      "Error, cannot open sysfs entry %s: %s (%d)\n",
      sysfile, strerror(errno), errno);
    return 0;
  }

  if ((len = read(fd, &ifnumber, sizeof(ifnumber))) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
      "Error, cannot read proc entry %s: %s (%d)\n",
      sysfile, strerror(errno), errno);
    close(fd);
    return 0;
  }

  if (len >= (ssize_t)sizeof(ifnumber)) {
    OONF_WARN(LOG_OS_INTERFACE, "Content of %s too long", sysfile);
    close(fd);
    return 0;
  }

  ifnumber[len] = 0;
  close(fd);
  return atoi(ifnumber);
}

/**
 * Set the required settings to allow multihop mesh routing
 */
static void
_activate_if_routing(void) {
  if (_os_linux_writeToFile(PROC_IPFORWARD_V4, &_original_ipv4_forward, '1')) {
    OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not activate ip_forward for ipv4! "
        "You should manually ensure that ip_forward for ipv4 is activated!");
  }
  if (os_system_is_ipv6_supported()) {
    if(_os_linux_writeToFile(PROC_IPFORWARD_V6, &_original_ipv6_forward, '1')) {
      OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not activate ip_forward for ipv6! "
          "You should manually ensure that ip_forward for ipv6 is activated!");
    }
  }

  if (_os_linux_writeToFile(PROC_ALL_REDIRECT, &_original_icmp_redirect, '0')) {
    OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* check kernel version and disable global rp_filter */
  if (_is_at_least_linuxkernel_2_6_31()) {
    if (_os_linux_writeToFile(PROC_ALL_SPOOF, &_original_rp_filter, '0')) {
      OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not disable global rp_filter "
          "(necessary for kernel 2.6.31 and newer)! You should manually "
          "ensure that rp_filter is disabled!");
    }
  }
}

/**
 * Reset the multihop mesh routing settings to default
 */
static void
_deactivate_if_routing(void) {
  if (_os_linux_writeToFile(PROC_ALL_REDIRECT, NULL, _original_icmp_redirect) != 0) {
    OONF_WARN(LOG_OS_INTERFACE,
        "WARNING! Could not restore ICMP redirect flag %s to %c!",
        PROC_ALL_REDIRECT, _original_icmp_redirect);
  }

  if (_os_linux_writeToFile(PROC_ALL_SPOOF, NULL, _original_rp_filter)) {
    OONF_WARN(LOG_OS_INTERFACE,
        "WARNING! Could not restore global rp_filter flag %s to %c!",
        PROC_ALL_SPOOF, _original_rp_filter);
  }

  if (_os_linux_writeToFile(PROC_IPFORWARD_V4, NULL, _original_ipv4_forward)) {
    OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not restore %s to %c!",
        PROC_IPFORWARD_V4, _original_ipv4_forward);
  }
  if (os_system_is_ipv6_supported()) {
    if (_os_linux_writeToFile(PROC_IPFORWARD_V6, NULL, _original_ipv6_forward)) {
      OONF_WARN(LOG_OS_INTERFACE, "WARNING! Could not restore %s to %c",
          PROC_IPFORWARD_V6, _original_ipv6_forward);
    }
  }
}


/**
 * Overwrite a numeric entry in the procfile system and keep the old
 * value.
 * @param file pointer to filename (including full path)
 * @param old pointer to memory to store old value
 * @param value new value
 * @return -1 if an error happened, 0 otherwise
 */
static int
_os_linux_writeToFile(const char *file, char *old, char value) {
  int fd;
  char rv;

  if (value == 0) {
    /* ignore */
    return 0;
  }

  if ((fd = open(file, O_RDWR)) < 0) {
    OONF_WARN(LOG_OS_INTERFACE,
      "Error, cannot open proc entry %s: %s (%d)\n",
      file, strerror(errno), errno);
    return -1;
  }

  if (read(fd, &rv, 1) != 1) {
    OONF_WARN(LOG_OS_INTERFACE,
      "Error, cannot read proc entry %s: %s (%d)\n",
      file, strerror(errno), errno);
    close(fd);
    return -1;
  }

  if (rv != value) {
    if (lseek(fd, SEEK_SET, 0) == -1) {
      OONF_WARN(LOG_OS_INTERFACE,
        "Error, cannot rewind to start on proc entry %s: %s (%d)\n",
        file, strerror(errno), errno);
      close(fd);
      return -1;
    }

    if (write(fd, &value, 1) != 1) {
      OONF_WARN(LOG_OS_INTERFACE,
        "Error, cannot write '%c' to proc entry %s: %s (%d)\n",
        value, file, strerror(errno), errno);
    }

    OONF_DEBUG(LOG_OS_INTERFACE, "Writing '%c' (was %c) to %s", value, rv, file);
  }

  close(fd);

  if (old && rv != value) {
    *old = rv;
  }

  return 0;
}

/**
 * @return true if linux kernel is at least 2.6.31
 */
static bool
_is_at_least_linuxkernel_2_6_31(void) {
  struct utsname uts;
  char *next;
  int first = 0, second = 0, third = 0;

  memset(&uts, 0, sizeof(uts));
  if (uname(&uts)) {
    OONF_WARN(LOG_OS_INTERFACE,
        "Error, could not read kernel version: %s (%d)\n",
        strerror(errno), errno);
    return false;
  }

  first = strtol(uts.release, &next, 10);
  /* check for linux 3.x */
  if (first >= 3) {
    return true;
  }

  if (*next != '.') {
    goto kernel_parse_error;
  }

  second = strtol(next+1, &next, 10);
  if (*next != '.') {
    goto kernel_parse_error;
  }

  third = strtol(next+1, NULL, 10);

  /* better or equal than linux 2.6.31 ? */
  return first == 2 && second == 6 && third >= 31;

kernel_parse_error:
  OONF_WARN(LOG_OS_INTERFACE,
      "Error, cannot parse kernel version: %s\n", uts.release);
  return false;
}

/**
 * Handle incoming rtnetlink multicast messages for interface listeners
 * @param hdr pointer to netlink message
 */
static void
_cb_rtnetlink_message(struct nlmsghdr *hdr) {
  struct ifinfomsg *ifi;
  struct ifaddrmsg *ifa;

  struct os_interface_if_listener *listener;

  if (hdr->nlmsg_type == RTM_NEWLINK || hdr->nlmsg_type == RTM_DELLINK) {
    ifi = (struct ifinfomsg *) NLMSG_DATA(hdr);

    OONF_DEBUG(LOG_OS_INTERFACE, "Linkstatus of interface %d changed", ifi->ifi_index);
    list_for_each_element(&_ifchange_listener, listener, _node) {
      listener->if_changed(ifi->ifi_index, (ifi->ifi_flags & IFF_UP) == 0);
    }
  }

  else if (hdr->nlmsg_type == RTM_NEWADDR || hdr->nlmsg_type == RTM_DELADDR) {
    ifa = (struct ifaddrmsg *) NLMSG_DATA(hdr);

    OONF_DEBUG(LOG_OS_INTERFACE, "Address of interface %u changed", ifa->ifa_index);
    list_for_each_element(&_ifchange_listener, listener, _node) {
      listener->if_changed(ifa->ifa_index, (ifa->ifa_flags & IFF_UP) == 0);
    }
  }
}

/**
 * Handle feedback from netlink socket
 * @param seq
 * @param error
 */
static void
_cb_rtnetlink_error(uint32_t seq, int error) {
  struct os_interface_address *addr;

  OONF_INFO(LOG_OS_INTERFACE, "Netlink socket provided feedback: %d %d", seq, error);

  /* transform into errno number */
  list_for_each_element(&_rtnetlink_feedback, addr, _internal._node) {
    if (seq == addr->_internal.nl_seq) {
      _address_finished(addr, error);
      break;
    }
  }
}

/**
 * Handle ack timeout from netlink socket
 */
static void
_cb_rtnetlink_timeout(void) {
  struct os_interface_address *addr;

  OONF_INFO(LOG_OS_INTERFACE, "Netlink socket timed out");

  list_for_each_element(&_rtnetlink_feedback, addr, _internal._node) {
    _address_finished(addr, -1);
  }
}

/**
 * Handle done from multipart netlink messages
 * @param seq
 */
static void
_cb_rtnetlink_done(uint32_t seq) {
  struct os_interface_address *addr;

  OONF_INFO(LOG_OS_INTERFACE, "Netlink operation finished: %u", seq);

  list_for_each_element(&_rtnetlink_feedback, addr, _internal._node) {
    if (seq == addr->_internal.nl_seq) {
      _address_finished(addr, 0);
      break;
    }
  }
}

/**
 * Stop processing of an ip address command and set error code
 * for callback
 * @param addr pointer to os_system_address
 * @param error error code, 0 if no error
 */
static void
_address_finished(struct os_interface_address *addr, int error) {
  if (list_is_node_added(&addr->_internal._node)) {
    /* remove first to prevent any kind of recursive cleanup */
    list_remove(&addr->_internal._node);

    if (addr->cb_finished) {
      addr->cb_finished(addr, error);
    }
  }
}
