/*
 * AWS IoT Device SDK for Embedded C 202211.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* This file configures MbedTLS for the CSDK transport interface and demos. */

#ifndef MBEDTLS_FREERTOS_PORT_ALT_H_
#define MBEDTLS_FREERTOS_PORT_ALT_H_

/* Referring Feature for AWS-IOT-W */
#include "rm_awsiot_w_cfg.h"

//orig SDK[[::
#if !defined(__SUPPORT_AWS_IOT_W__)//[tin aws work]
#include <stddef.h>

/* Generate errors if deprecated functions are used. */
#define MBEDTLS_DEPRECATED_REMOVED

/* Place AES tables in ROM. */
#define MBEDTLS_AES_ROM_TABLES

/* Enable the following cipher modes. */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CFB
#define MBEDTLS_CIPHER_MODE_CTR

/* Enable the following cipher padding modes. */
#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_CIPHER_PADDING_ONE_AND_ZEROS
#define MBEDTLS_CIPHER_PADDING_ZEROS_AND_LEN
#define MBEDTLS_CIPHER_PADDING_ZEROS

/* Cipher suite configuration. */
#define MBEDTLS_REMOVE_ARC4_CIPHERSUITES
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

/* Enable all SSL alert messages. */
#define MBEDTLS_SSL_ALL_ALERT_MESSAGES

/* Enable the following SSL features. */
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_ALPN
#define MBEDTLS_SSL_SERVER_NAME_INDICATION

/* Check certificate key usage. */
#define MBEDTLS_X509_CHECK_KEY_USAGE
#define MBEDTLS_X509_CHECK_EXTENDED_KEY_USAGE

/* Enable PKCS11 features */
#define MBEDTLS_PK_RSA_ALT_SUPPORT
#define MBEDTLS_GENPRIME

/* Enable the following MbedTLS features. */
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_CMAC_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_DEBUG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_THREADING_PTHREAD
#define MBEDTLS_THREADING_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CREATE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_CSR_WRITE_C
#define MBEDTLS_NET_C
#define MBEDTLS_TIMING_C

#define MBEDTLS_ENTROPY_PLATFORM
#define MBEDTLS_FS_IO
#define MBEDTLS_HAVE_TIME_DATE
#define MBEDTLS_HAVE_TIME

#include "mbedtls/check_config.h"
#else
//awsupgradeport[[::
//#include "mbedtls_config.h"
#include "mbedtls/threading.h"

/* Set the memory allocation functions on FreeRTOS. */
void * mbedtls_platform_calloc( size_t nmemb,
                                size_t size );
void mbedtls_platform_free( void * ptr );
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO    mbedtls_platform_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO      mbedtls_platform_free

void mbedtls_platform_mutex_init( mbedtls_threading_mutex_t * pMutex );
void mbedtls_platform_mutex_free( mbedtls_threading_mutex_t * pMutex );
int mbedtls_platform_mutex_lock( mbedtls_threading_mutex_t * pMutex );
int mbedtls_platform_mutex_unlock( mbedtls_threading_mutex_t * pMutex );

/* The network send and receive functions on FreeRTOS. */
int mbedtls_platform_send( void * ctx,
                           const unsigned char * buf,
                           size_t len );
int mbedtls_platform_recv( void * ctx,
                           unsigned char * buf,
                           size_t len );

/* The entropy poll function. */
int mbedtls_platform_entropy_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen );

#endif//]]
#endif /* ifndef MBEDTLS_FREERTOS_PORT_ALT_H_ */
