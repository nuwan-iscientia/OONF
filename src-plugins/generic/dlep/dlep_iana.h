
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

#ifndef DLEP_IANA_H_
#define DLEP_IANA_H_

/* The well-known multicast address for modem discovery */
#define DLEP_WELL_KNOWN_MULTICAST_ADDRESS "224.0.0.109"
#define DLEP_WELL_KNOWN_MULTICAST_ADDRESS_6 "FF02::6D"
#define DLEP_WELL_KNOWN_MULTICAST_PORT_TXT "22222"

/* The well-known port for modem discovery */
enum {
  DLEP_WELL_KNOWN_MULTICAST_PORT = 22222,
};

enum dlep_signals {
  DLEP_PEER_DISCOVERY               =  0,
  DLEP_PEER_OFFER                   =  1,
  DLEP_PEER_INITIALIZATION          =  2,
  DLEP_PEER_INITIALIZATION_ACK      =  3,
  DLEP_PEER_TERMINATION             =  4,
  DLEP_PEER_TERMINATION_ACK         =  5,
  DLEP_PEER_UPDATE                  =  6,
  DLEP_PEER_UPDATE_ACK              =  7,
  DLEP_DESTINATION_UP               =  8,
  DLEP_DESTINATION_UP_ACK           =  9,
  DLEP_DESTINATION_DOWN             = 10,
  DLEP_DESTINATION_DOWN_ACK         = 11,
  DLEP_DESTINATION_UPDATE           = 12,
  DLEP_LINK_CHARACTERISTICS_REQUEST = 13,
  DLEP_LINK_CHARACTERISTICS_ACK     = 14,
  DLEP_HEARTBEAT                    = 15,

  DLEP_SIGNAL_COUNT,
};

enum dlep_tlvs {
  DLEP_PORT_TLV                =  0,
  DLEP_PEER_TYPE_TLV           =  1,
  DLEP_MAC_ADDRESS_TLV         =  2,
  DLEP_IPV4_ADDRESS_TLV        =  3,
  DLEP_IPV6_ADDRESS_TLV        =  4,
  DLEP_MDRR_TLV                =  5,
  DLEP_MDRT_TLV                =  6,
  DLEP_CDRR_TLV                =  7,
  DLEP_CDRT_TLV                =  8,
  DLEP_LATENCY_TLV             =  9,
  DLEP_RESR_TLV                = 10,
  DLEP_REST_TLV                = 11,
  DLEP_RLQR_TLV                = 12,
  DLEP_RLQT_TLV                = 13,
  DLEP_STATUS_TLV              = 14,
  DLEP_HEARTBEAT_INTERVAL_TLV  = 15,
  DLEP_LINK_CHAR_ACK_TIMER_TLV = 16,
  DLEP_CREDIT_WIN_STATUS_TLV   = 17,
  DLEP_CREDIT_GRANT_REQ_TLV    = 18,
  DLEP_CREDIT_REQUEST_TLV      = 19,
  DLEP_OPTIONAL_SIGNALS_TLV    = 20,
  DLEP_OPTIONAL_DATA_ITEMS_TLV = 21,
  DLEP_VENDOR_EXTENSION_TLV    = 22,
  DLEP_VERSION_TLV             = 23,

  /* custom additions */
  DLEP_FRAMES_R_TLV,
  DLEP_FRAMES_T_TLV,
  DLEP_BYTES_R_TLV,
  DLEP_BYTES_T_TLV,
  DLEP_FRAMES_RETRIES_TLV,
  DLEP_FRAMES_FAILED_TLV,

  DLEP_RX_SIGNAL_TLV,
  DLEP_TX_SIGNAL_TLV,

  /* must be the last entry */
  DLEP_TLV_COUNT,
};

enum dlep_ipaddr_indicator {
  DLEP_IP_ADD    = 1,
  DLEP_IP_REMOVE = 2,
};

enum dlep_status {
  DLEP_STATUS_OKAY  = 0,
  DLEP_STATUS_ERROR = 1,

  DLEP_STATUS_COUNT,
};

#endif /* DLEP_IANA_H_ */
