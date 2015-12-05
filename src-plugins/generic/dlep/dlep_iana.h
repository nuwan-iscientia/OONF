
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

#ifndef DLEP_IANA_H_
#define DLEP_IANA_H_

/*! IPv4 address for DLEP multicast discovery */
#define DLEP_WELL_KNOWN_MULTICAST_ADDRESS "224.0.0.109"

/*! IPv6 address for DLEP multicast discovery */
#define DLEP_WELL_KNOWN_MULTICAST_ADDRESS_6 "FF02::6D"

/*! UDP port for DLEP multicast discovery */
#define DLEP_WELL_KNOWN_MULTICAST_PORT_TXT "22222"

/**
 * Generic DLEP Constant
 */
enum {
  /*! DLEP IANA_PORT */
  DLEP_PORT = 22222,//!< DLEP_PORT
};

/*! magic word for DLEP session */
#define DLEP_DRAFT_16_PREFIX "DLEP"

/**
 * Predefined DLEP extension IDs
 */
enum dlep_extensions {
  /*! number of DLEP base extensions */
  DLEP_EXTENSION_BASE_COUNT      =  2,

  /*! DLEP protocol/session handling */
  DLEP_EXTENSION_BASE_PROTO      = -2,

  /*! DLEP metrics defined by base RFC */
  DLEP_EXTENSION_BASE_METRIC     = -1,

  /*! Additional DLEP physical layer statistics */
  DLEP_EXTENSION_L1_STATS        = 256,

  /*! Additional DLEP link layer statistics */
  DLEP_EXTENSION_L2_STATS        = 257,
};

/**
 * DLEP signal IDs
 */
enum dlep_signals {
  /*! control numbers to keep session restriction the same */
  DLEP_KEEP_RESTRICTION             = -3,

  /*! control number to kill DLEP session */
  DLEP_KILL_SESSION                 = -2,

  /*! control number to allow all DLEP signals */
  DLEP_ALL_SIGNALS                  = -1,

  /*! Router announces its presence (UDP) */
  DLEP_PEER_DISCOVERY               =  1,

  /*! Radio announces the open DLEP session port(s) (UDP) */
  DLEP_PEER_OFFER                   =  2,

  /*! Router announcing the supported extensions */
  DLEP_PEER_INITIALIZATION          =  3,

  /*! Radio announcing the supported extensions and default metrics */
  DLEP_PEER_INITIALIZATION_ACK      =  4,

  /*! Radio update interface scope data */
  DLEP_PEER_UPDATE                  =  5,

  /*! Router acknowledges interface data update */
  DLEP_PEER_UPDATE_ACK              =  6,

  /*! Radio/Router terminates DLEP session */
  DLEP_PEER_TERMINATION             =  7,

  /*! Radio/Router acknowledge end of DLEP session */
  DLEP_PEER_TERMINATION_ACK         =  8,

  /*! Radio announces a new neighbor with metrics */
  DLEP_DESTINATION_UP               =  9,

  /*! Router acknowledges new neighbor */
  DLEP_DESTINATION_UP_ACK           = 10,

  /*! Radio announces a lost neighbor */
  DLEP_DESTINATION_DOWN             = 11,

  /*! Router acknowledges lost neighbor */
  DLEP_DESTINATION_DOWN_ACK         = 12,

  /*! Radio announces new metrics for a neighbor */
  DLEP_DESTINATION_UPDATE           = 13,

  /*! Radio/Router is still active */
  DLEP_HEARTBEAT                    = 14,

  /*! Router demands a special link characteristic to a neighbor */
  DLEP_LINK_CHARACTERISTICS_REQUEST = 15,

  /*! Radio acknowledges the changed link characterestics */
  DLEP_LINK_CHARACTERISTICS_ACK     = 16,
};

/**
 * DLEP TLV ids
 */
enum dlep_tlvs {
  /*! did an operation succeed or failed */
  DLEP_STATUS_TLV                  =  1,

  /*! IPv4 address, port and TLS flag for a Radio DLEP port */
  DLEP_IPV4_CONPOINT_TLV           =  2,

  /*! IPv6 address, port and TLS flag for a Radio DLEP port */
  DLEP_IPV6_CONPOINT_TLV           =  3,

  /*! string identifier of a DLEP Radio/Router */
  DLEP_PEER_TYPE_TLV               =  4,

  /*! interval between two Heartbeats a Radio/Router will send */
  DLEP_HEARTBEAT_INTERVAL_TLV      =  5,

  /*! list of supported extensions of a DLEP Radio/Router */
  DLEP_EXTENSIONS_SUPPORTED_TLV    =  6,

  /*! MAC address of a neighbor a DLEP message will refer to */
  DLEP_MAC_ADDRESS_TLV             =  7,

  /*! IPv4 address of a DLEP neighbor */
  DLEP_IPV4_ADDRESS_TLV            =  8,

  /*! IPv6 address of a DLEP neighbor */
  DLEP_IPV6_ADDRESS_TLV            =  9,

  /*! IPv4 subnet of a DLEP neighbor */
  DLEP_IPV4_SUBNET_TLV             = 10,

  /*! IPv6 subnet of a DLEP neigbor */
  DLEP_IPV6_SUBNET_TLV             = 11,

  /*! maximum data rate (receive) in bit/s */
  DLEP_MDRR_TLV                    = 12,

  /*! maximum data rate (transmit) in bit/s */
  DLEP_MDRT_TLV                    = 13,

  /*! current data rate (receive) in bit/s */
  DLEP_CDRR_TLV                    = 14,

  /*! current data rate (transmit) in bit/s */
  DLEP_CDRT_TLV                    = 15,

  /*! latency in microseconds */
  DLEP_LATENCY_TLV                 = 16,

  /*! resources (receive) */
  DLEP_RESR_TLV                    = 17,

  /*! resources (transmit) */
  DLEP_REST_TLV                    = 18,

  /*! relative link quality (receive) */
  DLEP_RLQR_TLV                    = 19,

  /*! relative link quality (transmit) */
  DLEP_RLQT_TLV                    = 20,

  /*! timeout for link characteristic acknowledgement */
  DLEP_LINK_CHAR_ACK_TIMER_TLV     = 21,

  /*! grant more traffic credit to DLEP endpoint in octets */
  DLEP_CREDIT_GRANT_TLV            = 22,

  /*! current traffic credit window value in octets */
  DLEP_CREDIT_WIN_STATUS_TLV       = 23,

  /*! request more traffic credit */
  DLEP_CREDIT_REQUEST_TLV          = 24,

  /* l1 statistics */

  /*! channel center frequency in Hz */
  DLEP_FREQUENCY_TLV,

  /*! channel bandwidth in Hz */
  DLEP_BANDWIDTH_TLV,

  /*! noise level in milli dBm */
  DLEP_NOISE_LEVEL_TLV,

  /*! total channel active time in ns */
  DLEP_CHANNEL_ACTIVE_TLV,

  /*! total channel busy time in ns */
  DLEP_CHANNEL_BUSY_TLV,

  /*! total channel receiver time in ns */
  DLEP_CHANNEL_RX_TLV,

  /*! total channel transmission time in ns */
  DLEP_CHANNEL_TX_TLV,

  /*! signal strength (receive) in milli dBm */
  DLEP_SIGNAL_RX_TLV,

  /*! signal strength (transmit) in milli dBm */
  DLEP_SIGNAL_TX_TLV,

  /* l2 statistics */

  /*! total number of frames (receive) */
  DLEP_FRAMES_R_TLV,

  /*! total number of frames (transmit) */
  DLEP_FRAMES_T_TLV,

  /*! total number of bytes (receive) */
  DLEP_BYTES_R_TLV,

  /*! total number of bytes (transmit) */
  DLEP_BYTES_T_TLV,

  /*! outgoing throughput in bit/s */
  DLEP_THROUGHPUT_T_TLV,

  /*! total number of frame retransmissions */
  DLEP_FRAMES_RETRIES_TLV,

  /*! total number of failed transmissions */
  DLEP_FRAMES_FAILED_TLV,
};

/**
 * Flags for IP address TLVs
 */
enum dlep_ipaddr_indicator {
  /*! Add an IP address to a DLEP neighbor */
  DLEP_IP_ADD    = 1,

  /*! Remove an IP address from a DLEP neighbor */
  DLEP_IP_REMOVE = 0,
};

/**
 * Flags for DLEP Connection Point
 */
enum dlep_connection_point_flags {
  /*! Connection needs TLS */
  DLEP_CONNECTION_TLS   = 1,

  /*! Connection needs no TLS */
  DLEP_CONNECTION_PLAIN = 0,
};

/**
 * DLEP status codes
 */
enum dlep_status {
  /*! internal value for "no status set" */
  DLEP_STATUS_NONE                = -1,

  /*! operation succeeded */
  DLEP_STATUS_OKAY                = 0,

  /*! message ID is unknown */
  DLEP_STATUS_UNKNOWN_MESSAGE     = 1,

  /*! message happened at an unexpected time */
  DLEP_STATUS_UNEXPECTED_MESSAGE  = 2,

  /*! unknown or invalid TLV in signal */
  DLEP_STATUS_INVALID_DATA        = 3,

  /*! unknown MAC address destination */
  DLEP_STATUS_INVALID_DESTINATION = 4,

  /*! receiver does not need messages about the destination */
  DLEP_STATUS_NOT_INTERESTED      = 100,

  /*! operation is refused at the moment */
  DLEP_STATUS_REQUEST_DENIED      = 101,

  /*! operation could not be done in the allocated time */
  DLEP_STATUS_TIMED_OUT           = 102,
};

#endif /* DLEP_IANA_H_ */
