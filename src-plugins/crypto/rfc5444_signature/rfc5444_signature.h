
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
 * @file src-plugins/crypto/rfc5444_signature/rfc5444_signature.h
 */

#ifndef RFC5444_SIGNATURE_H_
#define RFC5444_SIGNATURE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "subsystems/rfc5444/rfc5444_writer.h"
#include "subsystems/oonf_rfc5444.h"
#include "rfc7182_provider/rfc7182_provider.h"

enum {
  RFC5444_SIG_MAX_HASHSIZE = RFC5444_MAX_PACKET_SIZE,
  RFC5444_SIG_MAX_CRYPTSIZE = RFC5444_MAX_PACKET_SIZE,
};

/**
 * Unique key for a rfc5444 signature
 */
struct rfc5444_signature_key {
  /*! hash function id */
  uint8_t  hash_function;

  /*! crypto function id */
  uint8_t  crypt_function;
};

/**
 * results for signature id check
 */
enum rfc5444_sigid_check {
  /*! signature id is okay */
  RFC5444_SIGID_OKAY,

  /*! ignore signature id */
  RFC5444_SIGID_IGNORE,

  /*! drop signature */
  RFC5444_SIGID_DROP,
};

/**
 * object representing a registered signature
 */
struct rfc5444_signature {
  /*! data that makes the unique key of a signature */
  struct rfc5444_signature_key key;

  /*! true if signature is source specific */
  bool source_specific;

  /**
   * function to check key_id field of incoming signatures
   * @param sig this rfc5444 signature
   * @param id pointer to key id
   * @param len length of key id
   * @return okay, ignore or drop
   */
  enum rfc5444_sigid_check (*verify_id)(
      struct rfc5444_signature *sig, const void *id, size_t len);

  /**
   * checks if signature applies to a message/packet
   * @param sig this rfc5444 signature
   * @param msg_type rfc5444 message type, -1 for packet signature
   * @return true if signature applies to message type, false otherwise
   */
  bool (*is_matching_signature)(struct rfc5444_signature *sig, int msg_type);

  /**
   * @param sig this rfc5444 signature
   * @param length pointer to length field for crypto key length
   * @return pointer to crypto key
   */
  const void *(*getCryptoKey)(struct rfc5444_signature *sig, size_t *length);

  /**
   * @param sig this rfc5444 signature
   * @param length pointer to length field for key-id length
   * @return pointer to key-id
   */
  const void *(*getKeyId)(struct rfc5444_signature *sig, size_t *length);

  /*! true if message/packet should be dropped if signature is invalid */
  bool drop_if_invalid;

  /* the following data will be filled by the API */

  /*! source IP address of packet to be signed/checked */
  const struct netaddr *source;

  /*! pointer to cryptographic hash of signature */
  struct rfc7182_hash *hash;

  /*! pointer to cryptographic function of signature */
  struct rfc7182_crypt *crypt;

  /*! true if signature has been validated */
  bool verified;

  /*! true if signature is essential for the current check */
  bool _must_be_verified;

  /*! rfc5444 postprocessor to add the signature */
  struct rfc5444_writer_postprocessor _postprocessor;

  /*! hook into the registered signature tree */
  struct avl_node _node;
};

/*! subsystem identifier */
#define OONF_RFC5444_SIG_SUBSYSTEM "rfc5444_sig"

EXPORT void rfc5444_sig_add(struct rfc5444_signature *sig);
EXPORT void rfc5444_sig_remove(struct rfc5444_signature *sig);

#endif /* RFC5444_SIGNATURE_H_ */
