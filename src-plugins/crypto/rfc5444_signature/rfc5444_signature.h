
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

#ifndef RFC5444_SIGNATURE_H_
#define RFC5444_SIGNATURE_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "subsystems/rfc5444/rfc5444_writer.h"
#include "subsystems/oonf_rfc5444.h"

enum {
  RFC5444_SIG_MAX_HASHSIZE = RFC5444_MAX_PACKET_SIZE,
  RFC5444_SIG_MAX_CRYPTSIZE = RFC5444_MAX_PACKET_SIZE,
};

/* pre-define name of struct */
struct rfc5444_signature;

/* representation of a hash function for signatures */
struct rfc5444_sig_hash {
  /* RFC7182 hash id */
  uint8_t type;

  /* might be used as additional information for the crypto algorithm */
  size_t hash_length;

  /**
   * Callback for a hash function
   * @param sig rfc5444 signature
   * @param dst output buffer for signature
   * @param dst_len pointer to length of output buffer,
   *   will be set to signature length afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @return -1 if an error happened, 0 otherwise
   */
  int (*hash)(struct rfc5444_signature *sig,
      void *dst, size_t *dst_len,
      const void *src, size_t src_len);

  struct avl_node _node;
};

struct rfc5444_sig_crypt {
  /* RFC7182 crypto ID */
  uint8_t type;

  /**
   * @param sig rfc5444 signature
   * @return the maximum number of bytes for the signature
   */
  size_t (*getSize)(struct rfc5444_signature *sig);

  /**
   * Creates a cryptographic signature for a data block. The crypto
   * function normally uses the hash function defined for the signature
   * before encrypting the output.
   * @param sig rfc5444 signature
   * @param dst output buffer for cryptographic signature
   * @param dst_len pointer to length of output buffer, will be set to
   *   length of signature afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @return -1 if an error happened, 0 otherwise
   */
  int (*crypt)(struct rfc5444_signature *sig,
      void *dst, size_t *dst_len,
      const void *src, size_t src_len);

  /**
   * Checks if an encrypted signature is valid.
   *
   * If this function is not defined, the crypt-callback will be
   * used together with a memcmp() call for checking a signature.
   * @param sig rfc5444 signature
   * @param encrypted pointer to encrypted signature
   * @param encrypted_length length of encrypted signature
   * @param src unsigned original data
   * @param src_len length of original data
   * @return true if signature matches, false otherwise
   */
  bool (*check)(struct rfc5444_signature *sig,
      const void *encrypted, size_t encrypted_length,
      const void *src, size_t src_len);

  struct avl_node _node;
};

struct rfc5444_signature_key {
  /* has function id */
  uint8_t  hash_function;

  /* crypto function id */
  uint8_t  crypt_function;
};

enum rfc5444_sigid_check {
  RFC5444_SIGID_OKAY,
  RFC5444_SIGID_IGNORE,
  RFC5444_SIGID_DROP,
};

/* object representing a registered signature */
struct rfc5444_signature {
  /* data that makes the unique key of a signature */
  struct rfc5444_signature_key key;

  /* true if signature is source specific */
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

  /* true if message/packet should be dropped if signature is invalid */
  bool drop_if_invalid;

  /* the following data will be filled by the API */

  /* source IP address of packet to be signed/checked */
  const struct netaddr *source;

  /* pointer to cryptographic hash of signature */
  struct rfc5444_sig_hash *hash;

  /* pointer to cryptographic function of signature */
  struct rfc5444_sig_crypt *crypt;

  /* true if signature has been validated */
  bool verified;

  /* true if signature is essential for the current check */
  bool _must_be_verified;

  struct rfc5444_writer_postprocessor _postprocessor;

  struct avl_node _node;
};

#define OONF_RFC5444_SIG_SUBSYSTEM "rfc5444_sig"

EXPORT void rfc5444_sig_add_hash(struct rfc5444_sig_hash *);
EXPORT void rfc5444_sig_remove_hash(struct rfc5444_sig_hash *);

EXPORT void rfc5444_sig_add_crypt(struct rfc5444_sig_crypt *);
EXPORT void rfc5444_sig_remove_crypt(struct rfc5444_sig_crypt *);

EXPORT void rfc5444_sig_add(struct rfc5444_signature *sig);
EXPORT void rfc5444_sig_remove(struct rfc5444_signature *sig);

#endif /* RFC5444_SIGNATURE_H_ */
