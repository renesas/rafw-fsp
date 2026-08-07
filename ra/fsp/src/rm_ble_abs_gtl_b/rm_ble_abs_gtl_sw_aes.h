/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef HEADER_AES_H
#define HEADER_AES_H

#include <stdint.h>

/// AES encrypt
#define AES_ENCRYPT      1

/// AES decrypt
#define AES_DECRYPT      0

/**************************************************************************
 * AES declarations
 **************************************************************************/

/// AES max rounds
#define AES_MAXROUNDS    14

/// AES block size
#define AES_BLOCKSIZE    16

/// AES IV size
#define AES_IV_SIZE      16

#ifndef htonl

/// htonl helper macro
 #define htonl(a)                 \
    ((((a) >> 24) & 0x000000ff) | \
     (((a) >> 8) & 0x0000ff00) |  \
     (((a) << 8) & 0x00ff0000) |  \
     (((a) << 24) & 0xff000000))
#endif

#ifndef ntohl

/// ntohl helper macro
 #define ntohl(a)    htonl((a))
#endif

/*AES context struct*/
typedef struct aes_key_st
{
    uint16_t rounds;
    uint16_t key_size;
    uint32_t ks[(AES_MAXROUNDS + 1) * 8];
    uint8_t  iv[AES_IV_SIZE];
} AES_CTX;

/*AES key size*/
typedef enum
{
    AES_MODE_128,
    AES_MODE_256
} AES_MODE_KEY_SIZE;

/**
 ****************************************************************************************
 * @brief Set up AES with the key/iv and cipher size.
 * @param[in] ctx           AES context
 * @param[in] key           Key
 * @param[in] iv            Input vector
 * @param[in] mode          AES mode
 ****************************************************************************************
 */
void AES_set_key(AES_CTX * ctx, const uint8_t * key, const uint8_t * iv, AES_MODE_KEY_SIZE mode);

/**
 ****************************************************************************************
 * @brief Encrypt a byte sequence (with a block size 16) using the AES cipher.
 * @param[in] ctx           AES context
 * @param[in] msg           Message buffer
 * @param[out] out          The output data block
 * @param[in] length        Length in bytes
 ****************************************************************************************
 */
void AES_cbc_encrypt(AES_CTX * ctx, const uint8_t * msg, uint8_t * out, int length);

/**
 ****************************************************************************************
 * @brief Decrypt a byte sequence (with a block size 16) using the AES cipher.
 * @param[in] ctx           AES context
 * @param[in] msg           The input data block.
 * @param[out] out          The output data block.
 * @param[in] length        Length
 ****************************************************************************************
 */
void AES_cbc_decrypt(AES_CTX * ctx, const uint8_t * msg, uint8_t * out, int length);

/**
 ****************************************************************************************
 * @brief Change a key for decryption.
 * @param[in] ctx           AES context
 ****************************************************************************************
 */
void AES_convert_key(AES_CTX * ctx);

/**
 ****************************************************************************************
 * @brief Decrypt a single block (16 bytes) of data.
 * @param[in] ctx           AES context
 * @param[out] data         The output data block
 ****************************************************************************************
 */
void AES_decrypt(const AES_CTX * ctx, uint32_t * data);

/**
 ****************************************************************************************
 * @brief Encrypt a single block (16 bytes) of data.
 * @param[in] ctx           AES context
 * @param[out] data         The output data block
 ****************************************************************************************
 */
void AES_encrypt(const AES_CTX * ctx, uint32_t * data);

#endif                                 /* !HEADER_AES_H */

/// @}
/// @}
/// @}
