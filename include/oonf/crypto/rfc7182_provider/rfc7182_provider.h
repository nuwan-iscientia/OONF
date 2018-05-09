
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

#ifndef RFC7182_PROVIDER_H_
#define RFC7182_PROVIDER_H_

#include <oonf/libcommon/avl.h>
#include <oonf/oonf.h>

/**
 * representation of a hash function for signatures
 */
struct rfc7182_hash {
  /*! RFC7182 hash id */
  uint8_t type;

  /*! might be used as additional information for the crypto algorithm */
  size_t hash_length;

  /**
   * Callback for a hash function
   * @param hash pointer to this definition
   * @param dst output buffer for signature
   * @param dst_len pointer to length of output buffer,
   *   will be set to signature length afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @return -1 if an error happened, 0 otherwise
   */
  int (*hash)(struct rfc7182_hash *hash, void *dst, size_t *dst_len, const void *src, size_t src_len);

  /*! hook into the tree of registered hashes */
  struct avl_node _node;
};

/**
 * representation of a crypto function for signatures
 */
struct rfc7182_crypt {
  /*! RFC7182 crypto ID */
  uint8_t type;

  /**
   * @param crypt this definition
   * @param hash hash function
   * @param src_len length of source data
   * @return the maximum number of bytes for the signature
   */
  size_t (*getSignSize)(struct rfc7182_crypt *crypt, struct rfc7182_hash *hash);

  /**
   * Creates a cryptographic signature for a data block. The crypto
   * function normally uses the hash function
   * before creating the signature.
   * @param crypt this crypto definition
   * @param hash the definition of the hash
   * @param dst output buffer for cryptographic signature
   * @param dst_len pointer to length of output buffer, will be set to
   *   length of signature afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @param key key material for signature
   * @param key_len length of key material
   * @return -1 if an error happened, 0 otherwise
   */
  int (*sign)(struct rfc7182_crypt *crypt, struct rfc7182_hash *hash, void *dst, size_t *dst_len, const void *src,
    size_t src_len, const void *key, size_t key_len);

  /**
   * Checks if an encrypted signature is valid.
   * @param crypt this crypto definition
   * @param hash the definition of the hash
   * @param encrypted pointer to encrypted signature
   * @param encrypted_length length of encrypted signature
   * @param src unsigned original data
   * @param src_len length of original data
   * @param key key material for signature
   * @param key_len length of key material
   * @return true if signature matches, false otherwise
   */
  bool (*validate)(struct rfc7182_crypt *crypt, struct rfc7182_hash *hash, const void *encrypted,
    size_t encrypted_length, const void *src, size_t src_len, const void *key, size_t key_len);

  /**
   * Encrypts a data block.
   * @param crypt this crypto definition
   * @param dst output buffer for encrypted data
   * @param dst_len pointer to length of output buffer, will be set to
   *   length of encrypted data afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @param key key material for encryption
   * @param key_len length of key material
   * @return -1 if an error happened, 0 otherwise
   */
  int (*encrypt)(struct rfc7182_crypt *crypt, void *dst, size_t *dst_len, const void *src, size_t src_len,
    const void *key, size_t key_len);

  /**
   * Decrypts a data block.
   * @param crypt this crypto definition
   * @param dst output buffer for decrypted data
   * @param dst_len pointer to length of output buffer, will be set to
   *   length of decrypted data afterwards
   * @param src unsigned original data
   * @param src_len length of original data
   * @param key key material for decryption
   * @param key_len length of key material
   * @return -1 if an error happened, 0 otherwise
   */
  int (*decrypt)(struct rfc7182_crypt *crypt, void *dst, size_t *dst_len, const void *src, size_t src_len,
    const void *key, size_t key_len);

  /*! hook into the tree of registered crypto functions */
  struct avl_node _node;
};

/*! subsystem identifier */
#define OONF_RFC7182_PROVIDER_SUBSYSTEM "rfc7182_provider"

/*! rfc7182 hash class identifier */
#define OONF_RFC7182_HASH_CLASS "rfc7182_hash"

/*! rfc7182 crypto class identifier */
#define OONF_RFC7182_CRYPTO_CLASS "rfc7182_crypto"

EXPORT void rfc7182_add_hash(struct rfc7182_hash *);
EXPORT void rfc7182_remove_hash(struct rfc7182_hash *);
EXPORT struct avl_tree *rfc7182_get_hash_tree(void);

EXPORT void rfc7182_add_crypt(struct rfc7182_crypt *);
EXPORT void rfc7182_remove_crypt(struct rfc7182_crypt *);
EXPORT struct avl_tree *rfc7182_get_crypt_tree(void);

/**
 * @param id RFC7182 hash id
 * @return hash provider, NULL if unregistered id
 */
static INLINE struct rfc7182_hash *
rfc7182_get_hash(uint8_t id) {
  struct rfc7182_hash *hash;
  return avl_find_element(rfc7182_get_hash_tree(), &id, hash, _node);
}

/**
 * @param id RDC7182 crypto id
 * @return crypto provider, NULL if unregistered id
 */
static INLINE struct rfc7182_crypt *
rfc7182_get_crypt(uint8_t id) {
  struct rfc7182_crypt *crypt;
  return avl_find_element(rfc7182_get_crypt_tree(), &id, crypt, _node);
}
#endif /* RFC7182_PROVIDER_H_ */
