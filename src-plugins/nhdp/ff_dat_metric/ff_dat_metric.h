
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

#ifndef FF_DAT_METRIC_H_
#define FF_DAT_METRIC_H_

#include "common/common_types.h"
#include "core/oonf_subsystem.h"

#define OONF_FF_DAT_METRIC_SUBSYSTEM "ff_dat_metric"

/* definitions and constants */
enum {
  /* frame transmission success bewteen 1/8 and 8/8 */
  DATFF_FRAME_SUCCESS_RANGE = 1<<3,

  /*
   * linkspeed between 1 kbit/s and 2 gbit/s
   * (linkspeed_range * frame_success_range = 1<<24)
   */
  DATFF_LINKSPEED_MINIMUM = 1<<10,
  DATFF_LINKSPEED_RANGE = 1<<21,

  /* basic statistics of the metric */
  DATFF_LINKCOST_START    = RFC7181_METRIC_MAX,
  DATFF_LINKCOST_MINIMUM  = RFC7181_METRIC_MIN,
  DATFF_LINKCOST_MAXIMUM  = RFC7181_METRIC_MAX,
};

/* Configuration settings of DATFF Metric */
struct ff_dat_config {
  /* Interval between two updates of the metric */
  uint64_t interval;

  /* length of history in 'interval sized' memory cells */
  int32_t window;

  /* true if metric should include link speed */
  bool ett;

  /* selects how loss should be scaled */
  int loss_exponent;

  /* true if MIC factor should be applied to metric */
  bool mic;

#ifdef COLLECT_RAW_DATA
  /* filename to store raw data into */
  char *rawdata_file;

  /* true if metric should collect raw data */
  bool rawdata_start;

  /* time in milliseconds until measurement stops */
  uint64_t rawdata_maxtime;

  /* maxmimum number of measured packets until measurement stops */
  int rawdata_maxpackets;
#endif
};

/* a single history memory cell */
struct link_datff_bucket {
  /* number of RFC5444 packets received in time interval */
  int received;

  /* sum of received and lost RFC5444 packets in time interval */
  int total;

  /* link speed scaled to "minimum speed = 1" */
  int scaled_speed;
};

/* Additional data for a nhdp_link for metric calculation */
struct link_datff_data {
  /* current position in history ringbuffer */
  int activePtr;

  /* number of missed hellos based on timeouts since last received packet */
  int missed_hellos;

  /* last received packet sequence number */
  uint16_t last_seq_nr;

  /* remember the last transmitted packet loss for hysteresis */
  uint32_t last_packet_success_rate;

  /* timer for measuring lost hellos when no further packets are received */
  struct oonf_timer_instance hello_lost_timer;

  /* last known hello interval */
  uint64_t hello_interval;

  /* estimated number of neighbors of this link */
  uint32_t link_neigborhood;

  /* history ringbuffer */
  struct link_datff_bucket buckets[0];
};

#endif /* FF_DAT_METRIC_H_ */
