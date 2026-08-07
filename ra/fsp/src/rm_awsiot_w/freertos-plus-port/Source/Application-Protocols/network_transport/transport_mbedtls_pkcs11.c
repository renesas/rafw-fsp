/*
 * FreeRTOS V202411.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/**
 * @file transport_mbedtls_pkcs11.c
 * @brief TLS transport interface implementations. This implementation uses
 * mbedTLS.
 */

/* Standard includes. */
#include <string.h>

#include "logging_levels.h"

#define LIBRARY_LOG_NAME         "PkcsTlsTransport"

#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif /* LIBRARY_LOG_LEVEL */

#include "logging_stack.h"

#ifndef MBEDTLS_ALLOW_PRIVATE_ACCESS
    #define MBEDTLS_ALLOW_PRIVATE_ACCESS
    #include "mbedtls/private_access.h"
#endif /* MBEDTLS_ALLOW_PRIVATE_ACCESS */

/* MBedTLS Includes */
#if !defined( MBEDTLS_CONFIG_FILE )
    #include "mbedtls/config.h"
#else
    #include MBEDTLS_CONFIG_FILE
#endif

#ifdef MBEDTLS_PSA_CRYPTO_C
    /* MbedTLS PSA Includes */
    #include "psa/crypto.h"
    #include "psa/crypto_values.h"
#endif /* MBEDTLS_PSA_CRYPTO_C */

#include "mbedtls/debug.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* MbedTLS Bio TCP sockets wrapper include. */
//awsupgradeport[[:: removed #include "mbedtls_bio_tcp_sockets_wrapper.h"

/* TLS transport header. */
#include "transport_mbedtls_pkcs11.h"
#include "mbedtls_pkcs11.h"
/* PKCS #11 includes. */
#include "core_pkcs11_config.h"
#include "core_pkcs11.h"
#include "pkcs11.h"
#include "core_pki_utils.h"

#include "app_aws_user_conf.h"
#include "app_dpm_interface.h"

/* Referring Feature for AWS-IOT-W */
#include "rm_awsiot_w_cfg.h"
#define MBEDTLS_SSL_MAX_CONTENT_LEN             4096
/*-----------------------------------------------------------*/

/**
 * @brief Each compilation unit that consumes the NetworkContext must define it.
 * It should contain a single pointer as seen below whenever the header file
 * of this transport implementation is included to your project.
 *
 * @note When using multiple transports in the same compilation unit,
 *       define this pointer as void *.
 */
struct NetworkContext
{
    TlsTransportParams_t * pParams;
};

/*-----------------------------------------------------------*/

/**
 * @brief Represents string to be logged when mbedTLS returned error
 * does not contain a high-level code.
 */
static const char * pNoHighLevelMbedTlsCodeStr = "<No-High-Level-Code>";

/**
 * @brief Represents string to be logged when mbedTLS returned error
 * does not contain a low-level code.
 */
static const char * pNoLowLevelMbedTlsCodeStr = "<No-Low-Level-Code>";

/**
 * @brief Utility for converting the high-level code in an mbedTLS error to string,
 * if the code-contains a high-level code; otherwise, using a default string.
 */
#define mbedtlsHighLevelCodeOrDefault( mbedTlsCode )       \
    ( mbedtls_high_level_strerr( mbedTlsCode ) != NULL ) ? \
    mbedtls_high_level_strerr( mbedTlsCode ) : pNoHighLevelMbedTlsCodeStr

/**
 * @brief Utility for converting the level-level code in an mbedTLS error to string,
 * if the code-contains a level-level code; otherwise, using a default string.
 */
#define mbedtlsLowLevelCodeOrDefault( mbedTlsCode )       \
    ( mbedtls_low_level_strerr( mbedTlsCode ) != NULL ) ? \
    mbedtls_low_level_strerr( mbedTlsCode ) : pNoLowLevelMbedTlsCodeStr

/*-----------------------------------------------------------*/

/**
 * @brief Initialize the mbed TLS structures in a network connection.
 *
 * @param[in] pSslContext The SSL context to initialize.
 */
static void sslContextInit_P11( SSLContext_t * pSslContext );

//awsupgradeport[[::without PCKS11
static void sslContextInit( SSLContext_t * pSslContext );
//]]

/**
 * @brief Free the mbed TLS structures in a network connection.
 *
 * @param[in] pSslContext The SSL context to free.
 */
static void sslContextFree( SSLContext_t * pSslContext );

/**
 * @brief Set up TLS on a TCP connection.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INSUFFICIENT_MEMORY, #TLS_TRANSPORT_INVALID_CREDENTIALS,
 * #TLS_TRANSPORT_HANDSHAKE_FAILED, or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t tlsSetup( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials );

/*-----------------------------------------------------------*/

/**
 * @brief Callback that wraps PKCS#11 for pseudo-random number generation.
 *
 * @param[in] pvCtx Caller context.
 * @param[in] pucRandom Byte array to fill with random data.
 * @param[in] xRandomLength Length of byte array.
 *
 * @return Zero on success.
 */
static int generateRandomBytes( void * pvCtx,
                                unsigned char * pucRandom,
                                size_t xRandomLength );

/**
 * @brief Helper for reading the specified certificate object, if present,
 * out of storage, into RAM, and then into an mbedTLS certificate context
 * object.
 *
 * @param[in] pSslContext Caller TLS context.
 * @param[in] pcLabelName PKCS #11 certificate object label.
 * @param[in] xClass PKCS #11 certificate object class.
 * @param[out] pxCertificateContext Certificate context.
 *
 * @return Zero on success.
 */
static CK_RV readCertificateIntoContext( SSLContext_t * pSslContext,
                                         const char * pcLabelName,
                                         CK_OBJECT_CLASS xClass,
                                         mbedtls_x509_crt * pxCertificateContext );

/**
 * @brief Helper for setting up potentially hardware-based cryptographic context
 * for the client TLS certificate and private key.
 *
 * @param[in] Caller context.
 * @param[in] PKCS11 label which contains the desired private key.
 *
 * @return Zero on success.
 */
static CK_RV initializeClientKeys( SSLContext_t * pxCtx,
                                   const char * pcLabelName );

//awsupgradeport[[::without PCKS11
/**
 * @brief Add X509 certificate to the trusted list of root certificates.
 *
 * OpenSSL does not provide a single function for reading and loading certificates
 * from files into stores, so the file API must be called. Start with the
 * root certificate.
 *
 * @param[out] pSslContext SSL context to which the trusted server root CA is to be added.
 * @param[in] pRootCa PEM-encoded string of the trusted server root CA.
 * @param[in] rootCaSize Size of the trusted server root CA.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setRootCa( SSLContext_t * pSslContext,
                          const uint8_t * pRootCa,
                          size_t rootCaSize );

/**
 * @brief Set X509 certificate as client certificate for the server to authenticate.
 *
 * @param[out] pSslContext SSL context to which the client certificate is to be set.
 * @param[in] pClientCert PEM-encoded string of the client certificate.
 * @param[in] clientCertSize Size of the client certificate.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setClientCertificate( SSLContext_t * pSslContext,
                                     const uint8_t * pClientCert,
                                     size_t clientCertSize );

/**
 * @brief Set private key for the client's certificate.
 *
 * @param[out] pSslContext SSL context to which the private key is to be set.
 * @param[in] pPrivateKey PEM-encoded string of the client private key.
 * @param[in] privateKeySize Size of the client private key.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setPrivateKey( SSLContext_t * pSslContext,
                              const uint8_t * pPrivateKey,
                              size_t privateKeySize );

/**
 * @brief Passes TLS credentials to the OpenSSL library.
 *
 * Provides the root CA certificate, client certificate, and private key to the
 * OpenSSL library. If the client certificate or private key is not NULL, mutual
 * authentication is used when performing the TLS handshake.
 *
 * @param[out] pSslContext SSL context to which the credentials are to be imported.
 * @param[in] pNetworkCredentials TLS credentials to be imported.
 *
 * @return 0 on success; otherwise, failure;
 */
static int32_t setCredentials( SSLContext_t * pSslContext,
                               const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Set optional configurations for the TLS connection.
 *
 * This function is used to set SNI and ALPN protocols.
 *
 * @param[in] pSslContext SSL context to which the optional configurations are to be set.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 */
static void setOptionalConfigurations( SSLContext_t * pSslContext,
                                       const char * pHostName,
                                       const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Setup TLS by initializing contexts and setting configurations.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pHostName Remote host name, used for server name indication.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_INSUFFICIENT_MEMORY, #TLS_TRANSPORT_INVALID_CREDENTIALS,
 * or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t tlsSetup( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Perform the TLS handshake on a TCP connection.
 *
 * @param[in] pNetworkContext Network context.
 * @param[in] pNetworkCredentials TLS setup parameters.
 *
 * @return #TLS_TRANSPORT_SUCCESS, #TLS_TRANSPORT_HANDSHAKE_FAILED, or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t tlsHandshake( NetworkContext_t * pNetworkContext,
                                          const NetworkCredentials_t * pNetworkCredentials );

/**
 * @brief Initialize mbedTLS.
 *
 * @param[out] entropyContext mbed TLS entropy context for generation of random numbers.
 * @param[out] ctrDrgbContext mbed TLS CTR DRBG context for generation of random numbers.
 *
 * @return #TLS_TRANSPORT_SUCCESS, or #TLS_TRANSPORT_INTERNAL_ERROR.
 */
static TlsTransportStatus_t initMbedtls( mbedtls_entropy_context * pEntropyContext,
                                         mbedtls_ctr_drbg_context * pCtrDrgbContext );
//]]							 

#if defined(__USE_AWS_TLS_ALT__) //awsupgradeport[[::from rsa.c for running with SW
static int _aws_rsa_decrypt_func( void *ctx, size_t *olen, const unsigned char *input,
                                    unsigned char *output, size_t output_max_len );

static int _aws_rsa_sign_func( void *ctx, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                    mbedtls_md_type_t md_alg, unsigned int hashlen,
                                    const unsigned char *hash, unsigned char *sig );

static size_t _aws_rsa_key_len_func( void *ctx );

static int _aws_rsa_decrypt_func( void *ctx, size_t *olen, const unsigned char *input,
                                    unsigned char *output, size_t output_max_len )
{
    return mbedtls_rsa_pkcs1_decrypt((mbedtls_rsa_context *)ctx, NULL, NULL, olen,
                                    input, output, output_max_len);
}

static int _aws_rsa_sign_func( void *ctx, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                                    mbedtls_md_type_t md_alg, unsigned int hashlen,
                                    const unsigned char *hash, unsigned char *sig )
{
    //ToDo::verification needed with fleet provisioning
    return (mbedtls_rsa_pkcs1_sign((mbedtls_rsa_context*)ctx, f_rng, p_rng, md_alg, hashlen, hash, sig));
}

static size_t _aws_rsa_key_len_func( void *ctx )
{
	return ( ((const mbedtls_rsa_context *) ctx)->len );
}
#endif
/*-----------------------------------------------------------*/

#ifdef MBEDTLS_DEBUG_C
    void mbedtls_string_printf( void * sslContext,
                                int level,
                                const char * file,
                                int line,
                                const char * str )
    {
        if( ( str != NULL ) && ( file != NULL ) )
        {
            LogDebug( ( "%s:%d: [%d] %s", file, line, level, str ) );
        }
    }
#endif /* MBEDTLS_DEBUG_C */

/*-----------------------------------------------------------*/

static void sslContextInit_P11( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    mbedtls_ssl_config_init( &( pSslContext->config ) );
    mbedtls_x509_crt_init( &( pSslContext->rootCa ) );
    mbedtls_x509_crt_init( &( pSslContext->clientCert ) );
    mbedtls_ssl_init( &( pSslContext->context ) );
    #ifdef MBEDTLS_DEBUG_C
        mbedtls_debug_set_threshold( LIBRARY_LOG_LEVEL + 1U );
        mbedtls_ssl_conf_dbg( &( pSslContext->config ),
                              mbedtls_string_printf,
                              NULL );
    #endif /* MBEDTLS_DEBUG_C */

    xInitializePkcs11Session( &( pSslContext->xP11Session ) );
    C_GetFunctionList( &( pSslContext->pxP11FunctionList ) );
}

static void sslContextInit( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    mbedtls_ssl_config_init( &( pSslContext->config ) );
    mbedtls_x509_crt_init( &( pSslContext->rootCa ) );
    mbedtls_pk_init( &( pSslContext->privKey ) );
#if defined(__USE_AWS_TLS_ALT__) //HW RSA used
#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
    mbedtls_pk_init( &( pSslContext->privKeyAlt ) );
#endif
#endif //
    mbedtls_x509_crt_init( &( pSslContext->clientCert ) );
    mbedtls_ssl_init( &( pSslContext->context ) );
}
/*-----------------------------------------------------------*/

static void sslContextFree_P11( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    mbedtls_ssl_free( &( pSslContext->context ) );
    mbedtls_x509_crt_free( &( pSslContext->rootCa ) );
    mbedtls_x509_crt_free( &( pSslContext->clientCert ) );
    mbedtls_ssl_config_free( &( pSslContext->config ) );

    mbedtls_pk_free( &( pSslContext->privKey ) );

    pSslContext->pxP11FunctionList->C_CloseSession( pSslContext->xP11Session );
}

static void sslContextFree( SSLContext_t * pSslContext )
{
    configASSERT( pSslContext != NULL );

    mbedtls_ssl_free( &( pSslContext->context ) );
    mbedtls_x509_crt_free( &( pSslContext->rootCa ) );
    mbedtls_x509_crt_free( &( pSslContext->clientCert ) );
    mbedtls_pk_free( &( pSslContext->privKey ) );
#if defined(__USE_AWS_TLS_ALT__) //HW RSA used
#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
    mbedtls_pk_free( &( pSslContext->privKeyAlt ) );
#endif
#endif //]
    mbedtls_entropy_free( &( pSslContext->entropyContext ) );
    mbedtls_ctr_drbg_free( &( pSslContext->ctrDrgbContext ) );
    mbedtls_ssl_config_free( &( pSslContext->config ) );
}
/*-----------------------------------------------------------*/

static int32_t setRootCa( SSLContext_t * pSslContext,
                          const uint8_t * pRootCa,
                          size_t rootCaSize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pRootCa != NULL );

    /* Parse the server root CA certificate into the SSL context. */
    mbedtlsError = mbedtls_x509_crt_parse( &( pSslContext->rootCa ),
                                           pRootCa,
                                           rootCaSize );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to parse server root CA certificate: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
    }
    else
    {
        mbedtls_ssl_conf_ca_chain( &( pSslContext->config ),
                                   &( pSslContext->rootCa ),
                                   NULL );
    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setClientCertificate( SSLContext_t * pSslContext,
                                     const uint8_t * pClientCert,
                                     size_t clientCertSize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pClientCert != NULL );

    /* Setup the client certificate. */
    mbedtlsError = mbedtls_x509_crt_parse( &( pSslContext->clientCert ),
                                           pClientCert,
                                           clientCertSize );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to parse the client certificate: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setPrivateKey( SSLContext_t * pSslContext,
                              const uint8_t * pPrivateKey,
                              size_t privateKeySize )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pPrivateKey != NULL );

#if (CFG_PMGR==0)
    #if MBEDTLS_VERSION_NUMBER < 0x03000000
        mbedtlsError = mbedtls_pk_parse_key( &( pSslContext->privKey ),
                                             pPrivateKey,
                                             privateKeySize,
                                             NULL, 0 );
    #else
        mbedtlsError = mbedtls_pk_parse_key( &( pSslContext->privKey ),
                                             pPrivateKey,
                                             privateKeySize,
                                             NULL, 0,
                                             mbedtls_ctr_drbg_random,
                                             &( pSslContext->ctrDrgbContext ) );
    #endif /* if MBEDTLS_VERSION_NUMBER < 0x03000000 */
#else //awsupgradeport[[::
	mbedtlsError = 0;
    UINT8 isReconnected;

    app_is_reconnected(&isReconnected);
	if (!RM_PMGR_W_dpm_is_wakeup() ||
		isReconnected ||
		((enum DPM_WAKEUP_TYPE) RM_PMGR_W_dpm_wakeup_type_get(0) != DPM_RTCTIME_WAKEUP &&
		 (enum DPM_WAKEUP_TYPE) RM_PMGR_W_dpm_wakeup_type_get(0) != DPM_PACKET_WAKEUP ) )
	{
    #if MBEDTLS_VERSION_NUMBER < 0x03000000
		mbedtlsError = mbedtls_pk_parse_key( &( pSslContext->privKey ),
											 pPrivateKey,
											 privateKeySize,
											 NULL,
											 0 );
    #else
        mbedtlsError = mbedtls_pk_parse_key( &( pSslContext->privKey ),
                                             pPrivateKey,
                                             privateKeySize,
                                             NULL, 0,
                                             mbedtls_ctr_drbg_random,
                                             &( pSslContext->ctrDrgbContext ) );
    #endif
	}
#endif //]]


    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to parse the client key: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static int32_t setCredentials( SSLContext_t * pSslContext,
                               const NetworkCredentials_t * pNetworkCredentials )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pNetworkCredentials != NULL );

    /* Set up the certificate security profile, starting from the default value. */
    pSslContext->certProfile = mbedtls_x509_crt_profile_default;

    /* Set SSL authmode and the RNG context. */
    mbedtls_ssl_conf_authmode( &( pSslContext->config ),
                               MBEDTLS_SSL_VERIFY_REQUIRED );

//awsupgradeport[[:: [tin aws work]
#if defined(__USE_AWS_TLS_ALT__)
    printf("[%s:%d] MBEDTLS_SSL_MAX_CONTENT_LEN: %u \n", __func__, __LINE__, MBEDTLS_SSL_MAX_CONTENT_LEN);
    if(MBEDTLS_SSL_MAX_CONTENT_LEN < SSL_CONF_CONTENT_LENGTH)
    {
        // mbedtlsError = mbedtls_ssl_conf_content_len(&(pSslContext->config), SSL_CONF_CONTENT_LENGTH, SSL_CONF_CONTENT_LENGTH);
        mbedtlsError = mbedtls_ssl_conf_content_len(&(pSslContext->config), 2*SSL_CONF_CONTENT_LENGTH, SSL_CONF_CONTENT_LENGTH);    // for S3 ota
    }
    printf("[%s:%d] in_content_len: %u, out_content_len: %u \n", __func__, __LINE__, pSslContext->config.in_content_len, pSslContext->config.out_content_len);
#endif
//]]
    
    mbedtls_ssl_conf_rng( &( pSslContext->config ),
                          mbedtls_ctr_drbg_random,
                          &( pSslContext->ctrDrgbContext ) );
    mbedtls_ssl_conf_cert_profile( &( pSslContext->config ),
                                   &( pSslContext->certProfile ) );

    mbedtlsError = setRootCa( pSslContext,
                              pNetworkCredentials->pRootCa,
                              pNetworkCredentials->rootCaSize );

    if( ( pNetworkCredentials->pClientCert != NULL ) &&
        ( pNetworkCredentials->pPrivateKey != NULL ) )
    {
        if( mbedtlsError == 0 )
        {
            mbedtlsError = setClientCertificate( pSslContext,
                                                 pNetworkCredentials->pClientCert,
                                                 pNetworkCredentials->clientCertSize );
        }

        if( mbedtlsError == 0 )
        {
            mbedtlsError = setPrivateKey( pSslContext,
                                          pNetworkCredentials->pPrivateKey,
                                          pNetworkCredentials->privateKeySize );
        }

        if( mbedtlsError == 0 )
        {
#if !defined(__USE_AWS_TLS_ALT__) //org
            mbedtlsError = mbedtls_ssl_conf_own_cert( &( pSslContext->config ),
                                                      &( pSslContext->clientCert ),
                                                      &( pSslContext->privKey ) );

#else //awsupgradeport[[::
#if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
            if (mbedtls_pk_get_type(&pSslContext->privKey) == MBEDTLS_PK_RSA)
            {
                mbedtlsError = mbedtls_pk_setup_rsa_alt(&pSslContext->privKeyAlt,
                                                (void *)mbedtls_pk_rsa(pSslContext->privKey),
                                                _aws_rsa_decrypt_func,
                                                _aws_rsa_sign_func,
                                                _aws_rsa_key_len_func);
                if ( mbedtlsError != 0 )
                {
                    LogError(("Failed to set RSA-alt(0x%lx)", -mbedtlsError));
                }

                mbedtlsError = mbedtls_ssl_conf_own_cert( &( pSslContext->config ),
                                                            &( pSslContext->clientCert ),
                                                            &( pSslContext->privKeyAlt ) );
            } 
            else {
                mbedtlsError = mbedtls_ssl_conf_own_cert( &( pSslContext->config ),
                                                            &( pSslContext->clientCert ),
                                                            &( pSslContext->privKey ) );
            }
#else
            mbedtlsError = mbedtls_ssl_conf_own_cert( &( pSslContext->config ),
                                                      &( pSslContext->clientCert ),
                                                      &( pSslContext->privKey ) );
#endif /* defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT) */
#endif //]
        }

    }

    return mbedtlsError;
}
/*-----------------------------------------------------------*/

static void setOptionalConfigurations( SSLContext_t * pSslContext,
                                       const char * pHostName,
                                       const NetworkCredentials_t * pNetworkCredentials )
{
    int32_t mbedtlsError = -1;

    configASSERT( pSslContext != NULL );
    configASSERT( pHostName != NULL );
    configASSERT( pNetworkCredentials != NULL );

    if( pNetworkCredentials->pAlpnProtos != NULL )
    {
        /* Include an application protocol list in the TLS ClientHello
         * message. */
        mbedtlsError = mbedtls_ssl_conf_alpn_protocols( &( pSslContext->config ),
                                                        pNetworkCredentials->pAlpnProtos );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to configure ALPN protocol in mbed TLS: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
        }
    }

    /* Enable SNI if requested. */
    if( pNetworkCredentials->disableSni == pdFALSE )
    {
        mbedtlsError = mbedtls_ssl_set_hostname( &( pSslContext->context ),
                                                 pHostName );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to set server name: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
        }
    }

    /* Set Maximum Fragment Length if enabled. */
    #ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
        /* Enable the max fragment extension. 4096 bytes is currently the largest fragment size permitted.
         * See RFC 8449 https://tools.ietf.org/html/rfc8449 for more information.
         *
         * Smaller values can be found in "mbedtls/include/ssl.h".
         */
        mbedtlsError = mbedtls_ssl_conf_max_frag_len( &( pSslContext->config ), MBEDTLS_SSL_MAX_FRAG_LEN_4096 );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to maximum fragment length extension: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
        }
    #endif /* ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */
}
/*-----------------------------------------------------------*/

static TlsTransportStatus_t tlsSetup_P11( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;
    CK_RV xResult = CKR_OK;

    configASSERT( pNetworkContext != NULL );
    configASSERT( pNetworkContext->pParams != NULL );
    configASSERT( pHostName != NULL );
    configASSERT( pNetworkCredentials != NULL );
    configASSERT( pNetworkCredentials->pRootCa != NULL );
    configASSERT( pNetworkCredentials->pClientCertLabel != NULL );
    configASSERT( pNetworkCredentials->pPrivateKeyLabel != NULL );

    pTlsTransportParams = pNetworkContext->pParams;

    /* Initialize the mbed TLS context structures. */
    sslContextInit_P11( &( pTlsTransportParams->sslContext ) );

    mbedtlsError = mbedtls_ssl_config_defaults( &( pTlsTransportParams->sslContext.config ),
                                                MBEDTLS_SSL_IS_CLIENT,
                                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                                MBEDTLS_SSL_PRESET_DEFAULT );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to set default SSL configuration: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

        /* Per mbed TLS docs, mbedtls_ssl_config_defaults only fails on memory allocation. */
        returnStatus = TLS_TRANSPORT_INSUFFICIENT_MEMORY;
    }

    #ifdef MBEDTLS_PSA_CRYPTO_C
        mbedtlsError = psa_crypto_init();

        if( mbedtlsError != PSA_SUCCESS )
        {
            LogError( ( "Failed to initialize PSA Crypto implementation: %d", ( int ) mbedtlsError ) );
            returnStatus = TLS_TRANSPORT_INVALID_PARAMETER;
        }
        else
        {
            LogDebug( ( "Initialized the PSA Crypto Engine" ) );
        }
    #endif /* MBEDTLS_PSA_CRYPTO_C */

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Set up the certificate security profile, starting from the default value. */
        pTlsTransportParams->sslContext.certProfile = mbedtls_x509_crt_profile_default;

        /* test.mosquitto.org only provides a 1024-bit RSA certificate, which is
         * not acceptable by the default mbed TLS certificate security profile.
         * For the purposes of this demo, allow the use of 1024-bit RSA certificates.
         * This block should be removed otherwise. */
        if( strncmp( pHostName, "test.mosquitto.org", strlen( pHostName ) ) == 0 )
        {
            pTlsTransportParams->sslContext.certProfile.rsa_min_bitlen = 1024;
        }

        /* Set SSL authmode and the RNG context. */
        mbedtls_ssl_conf_authmode( &( pTlsTransportParams->sslContext.config ),
                                   MBEDTLS_SSL_VERIFY_REQUIRED );
#if defined(__USE_AWS_TLS_ALT__)
        if(MBEDTLS_SSL_MAX_CONTENT_LEN < SSL_CONF_CONTENT_LENGTH)//[tin aws work]
        {
            mbedtlsError = mbedtls_ssl_conf_content_len(&(pTlsTransportParams->sslContext.config), SSL_CONF_CONTENT_LENGTH, SSL_CONF_CONTENT_LENGTH);
        }
#endif
        mbedtls_ssl_conf_rng( &( pTlsTransportParams->sslContext.config ),
                              generateRandomBytes,
                              &pTlsTransportParams->sslContext );
        mbedtls_ssl_conf_cert_profile( &( pTlsTransportParams->sslContext.config ),
                                       &( pTlsTransportParams->sslContext.certProfile ) );

        /* Parse the server root CA certificate into the SSL context. */
        mbedtlsError = mbedtls_x509_crt_parse( &( pTlsTransportParams->sslContext.rootCa ),
                                               pNetworkCredentials->pRootCa,
                                               pNetworkCredentials->rootCaSize );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to parse server root CA certificate: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

            returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
        }
        else
        {
            mbedtls_ssl_conf_ca_chain( &( pTlsTransportParams->sslContext.config ),
                                       &( pTlsTransportParams->sslContext.rootCa ),
                                       NULL );
        }
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Setup the client private key. */
        xResult = initializeClientKeys( &( pTlsTransportParams->sslContext ),
                                        pNetworkCredentials->pPrivateKeyLabel );

        if( xResult != CKR_OK )
        {
            LogError( ( "Failed to setup key handling by PKCS #11." ) );

            returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
        }
        else
        {
            /* Setup the client certificate. */
            xResult = readCertificateIntoContext( &( pTlsTransportParams->sslContext ),
                                                  pNetworkCredentials->pClientCertLabel,
                                                  CKO_CERTIFICATE,
                                                  &( pTlsTransportParams->sslContext.clientCert ) );

            if( xResult != CKR_OK )
            {
                LogError( ( "Failed to get certificate from PKCS #11 module." ) );

                returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
            }
            else
            {
                ( void ) mbedtls_ssl_conf_own_cert( &( pTlsTransportParams->sslContext.config ),
                                                    &( pTlsTransportParams->sslContext.clientCert ),
                                                    &( pTlsTransportParams->sslContext.privKey ) );
            }
        }
    }

    if( ( returnStatus == TLS_TRANSPORT_SUCCESS ) && ( pNetworkCredentials->pAlpnProtos != NULL ) )
    {
        /* Include an application protocol list in the TLS ClientHello
         * message. */
        mbedtlsError = mbedtls_ssl_conf_alpn_protocols( &( pTlsTransportParams->sslContext.config ),
                                                        pNetworkCredentials->pAlpnProtos );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to configure ALPN protocol in mbed TLS: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

            returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
        }
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
#if (1 == AWS_IOT_DPM_APP_ENABLE) //awsupgradeport[[::awsdpmwork
        //Set iv sync functionality (from __TLS_FORCE_SYNC_IV__)
        mbedtls_ssl_set_sync_iv(&( pTlsTransportParams->sslContext.config ), pdTRUE, 10);
#endif //]]
        /* Initialize the mbed TLS secured connection context. */
        mbedtlsError = mbedtls_ssl_setup( &( pTlsTransportParams->sslContext.context ),
                                          &( pTlsTransportParams->sslContext.config ) );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to set up mbed TLS SSL context: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

            returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
        }
        else
        {
            /* Set the underlying IO for the TLS connection. */

            /* MISRA Rule 11.2 flags the following line for casting the second
             * parameter to void *. This rule is suppressed because
             * #mbedtls_ssl_set_bio requires the second parameter as void *.
             */
            /* coverity[misra_c_2012_rule_11_2_violation] */
            //[tin aws work]
            mbedtls_ssl_set_bio( &( pTlsTransportParams->sslContext.context ),
                        ( void * ) pTlsTransportParams->tcpSocket,
                        mbedtls_platform_send,
                        mbedtls_platform_recv,
                        NULL );
        }
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Enable SNI if requested. */
        if( pNetworkCredentials->disableSni == pdFALSE )
        {
            mbedtlsError = mbedtls_ssl_set_hostname( &( pTlsTransportParams->sslContext.context ),
                                                     pHostName );

            if( mbedtlsError != 0 )
            {
                LogError( ( "Failed to set server name: mbedTLSError= %s : %s.",
                            mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                            mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

                returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
            }
        }
    }

    /* Set Maximum Fragment Length if enabled. */
    #ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
        if( returnStatus == TLS_TRANSPORT_SUCCESS )
        {
            /* Enable the max fragment extension. 4096 bytes is currently the largest fragment size permitted.
             * See RFC 8449 https://tools.ietf.org/html/rfc8449 for more information.
             *
             * Smaller values can be found in "mbedtls/include/ssl.h".
             */
            mbedtlsError = mbedtls_ssl_conf_max_frag_len( &( pTlsTransportParams->sslContext.config ), MBEDTLS_SSL_MAX_FRAG_LEN_4096 );

            if( mbedtlsError != 0 )
            {
                LogError( ( "Failed to maximum fragment length extension: mbedTLSError= %s : %s.",
                            mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                            mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
                returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
            }
        }
    #endif /* ifdef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH */

#if CFG_PMGR //awsupgradeport[[::awsdpmwork:: restore TLS session when DPM wakeup
    UINT8 isReconnected;

    app_is_reconnected(&isReconnected);
    if(RM_PMGR_W_dpm_is_wakeup() && !isReconnected && returnStatus == TLS_TRANSPORT_SUCCESS)
    {
        IoT_Error_t dpm_status = app_tls_restore(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));

        if (dpm_status != SUCCESS)
        {
            returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
            app_dpm_set_restoration_flag(0);
            LogError(("app_tls_restore(): NG"));
        }
        else
        {
            app_dpm_set_restoration_flag(1);
            LogInfo(("TLS session restored"));
        }
    }
#endif //]]

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Perform the TLS handshake. */
        do
        {
            mbedtlsError = mbedtls_ssl_handshake( &( pTlsTransportParams->sslContext.context ) );
        } while( ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_READ ) ||
                 ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ) ||
                 ( mbedtlsError == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ) );

        if( mbedtlsError != 0 )
        {
#if defined(__USE_AWS_TLS_ALT__) //awsupgradeport[[::for debug
            uint32_t ret = mbedtls_ssl_get_verify_result(&( pTlsTransportParams->sslContext.context ));
            if (ret)
            {
                LogError( ("failed to verify tls session(0x%lx)", ret) );
            }
#endif //]]

            if( mbedtlsError == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET )
            {
                LogDebug( ( "Received a MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET return code from mbedtls_ssl_handshake." ) );
            }
            else
            {
                LogError( ( "Failed to perform TLS handshake: mbedTLSError= %s : %s.",
                            mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                            mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

                returnStatus = TLS_TRANSPORT_HANDSHAKE_FAILED;
            }
        }
    }
//awsupgradeport[[::awsdpmwork:: save TLS session & set rcv time to DPM_RCV_OK_CONNECT
#if CFG_PMGR
    else
    {
        if (RM_PMGR_W_dpm_is_enabled())
        {
            IoT_Error_t status = 0;
            LogInfo(("TLS handshake successful & rcv timeout set to DPM_RCV_OK_CONNECT"));

            status = app_tls_save(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));
            if (status)
            {
                LogError(("failed to save tls session(0x%02x)\n", status));
            }
        }
        app_dpm_set_recv_timeout_flag(DPM_RCV_OK_CONNECT);
    }
#endif //]]

    if( returnStatus != TLS_TRANSPORT_SUCCESS )
    {
        sslContextFree_P11( &( pTlsTransportParams->sslContext ) );
#if defined(__USE_AWS_TLS_ALT__) //awsupgradework[[::
        /* Clear the mutex functions for mbed TLS thread safety. */
        //prevent memory leak[[::
        mbedtls_threading_free_alt();
        //clear recv timeout flag
        app_dpm_set_recv_timeout_flag(DPM_RCV_NO_CONNECT);
#endif //]]
    }
    else
    {
        LogInfo( ( "(Network connection %p) TLS handshake successful.",
                   pNetworkContext ) );
    }

    return returnStatus;
}

static TlsTransportStatus_t tlsSetup( NetworkContext_t * pNetworkContext,
                                      const char * pHostName,
                                      const NetworkCredentials_t * pNetworkCredentials )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;

    configASSERT( pNetworkContext != NULL );
    configASSERT( pNetworkContext->pParams != NULL );
    configASSERT( pHostName != NULL );
    configASSERT( pNetworkCredentials != NULL );
    configASSERT( pNetworkCredentials->pRootCa != NULL );

    pTlsTransportParams = pNetworkContext->pParams;
    /* Initialize the mbed TLS context structures. */
    sslContextInit( &( pTlsTransportParams->sslContext ) );

    mbedtlsError = mbedtls_ssl_config_defaults( &( pTlsTransportParams->sslContext.config ),
                                                MBEDTLS_SSL_IS_CLIENT,
                                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                                MBEDTLS_SSL_PRESET_DEFAULT );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to set default SSL configuration: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

        /* Per mbed TLS docs, mbedtls_ssl_config_defaults only fails on memory allocation. */
        returnStatus = TLS_TRANSPORT_INSUFFICIENT_MEMORY;
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        mbedtlsError = setCredentials( &( pTlsTransportParams->sslContext ),
                                       pNetworkCredentials );

        if( mbedtlsError != 0 )
        {
            returnStatus = TLS_TRANSPORT_INVALID_CREDENTIALS;
        }
        else
        {
            /* Optionally set SNI and ALPN protocols. */
            setOptionalConfigurations( &( pTlsTransportParams->sslContext ),
                                       pHostName,
                                       pNetworkCredentials );
        }


    }

    return returnStatus;
}

/*-----------------------------------------------------------*/

static int generateRandomBytes( void * pvCtx,
                                unsigned char * pucRandom,
                                size_t xRandomLength )
{
    /* Must cast from void pointer to conform to mbed TLS API. */
    SSLContext_t * pxCtx = ( SSLContext_t * ) pvCtx;
    CK_RV xResult;

    xResult = pxCtx->pxP11FunctionList->C_GenerateRandom( pxCtx->xP11Session, pucRandom, xRandomLength );

    if( xResult != CKR_OK )
    {
        LogError( ( "Failed to generate random bytes from the PKCS #11 module." ) );
    }

    return ( int ) xResult;
}

/*-----------------------------------------------------------*/

static CK_RV readCertificateIntoContext( SSLContext_t * pSslContext,
                                         const char * pcLabelName,
                                         CK_OBJECT_CLASS xClass,
                                         mbedtls_x509_crt * pxCertificateContext )
{
    CK_RV xResult = CKR_OK;
    CK_ATTRIBUTE xTemplate = { 0 };
    CK_OBJECT_HANDLE xCertObj = 0;
    size_t labelLength = strlen( pcLabelName );

    if( labelLength > pkcs11configMAX_LABEL_LENGTH )
    {
        labelLength = pkcs11configMAX_LABEL_LENGTH;
    }

    /* Get the handle of the certificate. */
    xResult = xFindObjectWithLabelAndClass( pSslContext->xP11Session,
                                            ( char * ) pcLabelName,
                                            labelLength,
                                            xClass,
                                            &xCertObj );

    if( ( CKR_OK == xResult ) && ( xCertObj == CK_INVALID_HANDLE ) )
    {
        xResult = CKR_OBJECT_HANDLE_INVALID;
    }

    /* Query the certificate size. */
    if( CKR_OK == xResult )
    {
        xTemplate.type = CKA_VALUE;
        xTemplate.ulValueLen = 0;
        xTemplate.pValue = NULL;
        xResult = pSslContext->pxP11FunctionList->C_GetAttributeValue( pSslContext->xP11Session,
                                                                       xCertObj,
                                                                       &xTemplate,
                                                                       1 );
    }

    /* Create a buffer for the certificate. */
    if( CKR_OK == xResult )
    {
        xTemplate.pValue = pvPortMalloc( xTemplate.ulValueLen );

        if( NULL == xTemplate.pValue )
        {
            xResult = CKR_HOST_MEMORY;
        }
    }

    /* Export the certificate. */
    if( CKR_OK == xResult )
    {
        xResult = pSslContext->pxP11FunctionList->C_GetAttributeValue( pSslContext->xP11Session,
                                                                       xCertObj,
                                                                       &xTemplate,
                                                                       1 );
    }

    /* Decode the certificate. */
    if( CKR_OK == xResult )
    {
        xResult = ( CK_RV ) mbedtls_x509_crt_parse( pxCertificateContext,
                                                    ( const unsigned char * ) xTemplate.pValue,
                                                    xTemplate.ulValueLen );
    }

    /* Free memory. */
    vPortFree( xTemplate.pValue );

    return xResult;
}

/*-----------------------------------------------------------*/
//awsupgradeport[[::without PCKS11
static TlsTransportStatus_t tlsHandshake( NetworkContext_t * pNetworkContext,
                                          const NetworkCredentials_t * pNetworkCredentials )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;

    configASSERT( pNetworkContext != NULL );
    configASSERT( pNetworkContext->pParams != NULL );
    configASSERT( pNetworkCredentials != NULL );

    pTlsTransportParams = pNetworkContext->pParams;

//#if (1 == AWS_IOT_DPM_APP_ENABLE) //awsupgradeport[[::awsdpmwork
	//Set iv sync functionality (from __TLS_FORCE_SYNC_IV__)
	mbedtls_ssl_set_sync_iv(&( pTlsTransportParams->sslContext.config ), pdTRUE, 10);
//#endif //]]

    /* Initialize the mbed TLS secured connection context. */
    mbedtlsError = mbedtls_ssl_setup( &( pTlsTransportParams->sslContext.context ),
                                      &( pTlsTransportParams->sslContext.config ) );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to set up mbed TLS SSL context: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

        returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
    }
    else
    {
        /* Set the underlying IO for the TLS connection. */

        /* MISRA Rule 11.2 flags the following line for casting the second
         * parameter to void *. This rule is suppressed because
         * #mbedtls_ssl_set_bio requires the second parameter as void *.
         */
        /* coverity[misra_c_2012_rule_11_2_violation] */

        /* These two macros MBEDTLS_SSL_SEND and MBEDTLS_SSL_RECV need to be
         * defined in mbedtls_config.h according to which implementation you use.
         */
        //[tin aws work], org
        /*
        mbedtls_ssl_set_bio( &( pTlsTransportParams->sslContext.context ),
                             ( void * ) pTlsTransportParams->tcpSocket,
                             xMbedTLSBioTCPSocketsWrapperSend,
                             xMbedTLSBioTCPSocketsWrapperRecv,
                             NULL );
        */
        mbedtls_ssl_set_bio( &( pTlsTransportParams->sslContext.context ),
                             ( void * ) pTlsTransportParams->tcpSocket,
                             mbedtls_platform_send,
                             mbedtls_platform_recv,
                             NULL );       
    }

#if CFG_PMGR //awsupgradeport[[::awsdpmwork:: restore TLS session when DPM wakeup
    UINT8 isReconnected;

    app_is_reconnected(&isReconnected);
    if(RM_PMGR_W_dpm_is_wakeup() && !isReconnected && returnStatus == TLS_TRANSPORT_SUCCESS)
    {
        IoT_Error_t dpm_status = app_tls_restore(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));

        if (dpm_status != SUCCESS)
        {
            returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
            app_dpm_set_restoration_flag(0);
            LogError(("app_tls_restore(): NG"));
        }
        else
        {
            app_dpm_set_restoration_flag(1);
            LogInfo(("TLS session restored"));
        }
    }
#endif //]]

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Perform the TLS handshake. */
        do
        {
            mbedtlsError = mbedtls_ssl_handshake( &( pTlsTransportParams->sslContext.context ) );
        } while( ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_READ ) ||
                 ( mbedtlsError == MBEDTLS_ERR_SSL_WANT_WRITE ) );

        if( mbedtlsError != 0 )
        {
#if defined(__USE_AWS_TLS_ALT__) //awsupgradeport[[:: for debugging
            uint32_t ret = mbedtls_ssl_get_verify_result(&( pTlsTransportParams->sslContext.context ));
            if (ret)
            {
                LogError( ("failed to verify tls session(0x%lx)", ret) );
            }
#endif //]]
            LogError( ( "Failed to perform TLS handshake: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );

            returnStatus = TLS_TRANSPORT_HANDSHAKE_FAILED;
        }
        else
        {
            LogInfo( ( "(Network connection %p) TLS handshake successful.",
                       pNetworkContext ) );
#if CFG_PMGR //awsupgradeport[[::awsdpmwork:: save TLS session & set rcv time to DPM_RCV_OK_CONNECT
			if (RM_PMGR_W_dpm_is_enabled())
			{
				IoT_Error_t status = 0;
				LogInfo(("TLS handshake successful & rcv timeout set to DPM_RCV_OK_CONNECT"));
	
				status = app_tls_save(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));
				if (status)
				{
					LogError(("failed to save tls session(0x%02x)\n", status));
				}
			}
			app_dpm_set_recv_timeout_flag(DPM_RCV_OK_CONNECT);
#endif //]]
        }
    }
#if (1 == AWS_IOT_DPM_APP_ENABLE) //awsupgradeport[[::awsdpmwork:: clear recv timeout flag
	if (returnStatus != TLS_TRANSPORT_SUCCESS)
	{
		app_dpm_set_recv_timeout_flag(DPM_RCV_NO_CONNECT);
	}
#endif //]]

    return returnStatus;
}
/*-----------------------------------------------------------*/
//]]

/**
 * @brief Helper for setting up potentially hardware-based cryptographic context
 * for the client TLS certificate and private key.
 *
 * @param[in] Caller context.
 * @param[in] PKCS11 label which contains the desired private key.
 *
 * @return Zero on success.
 */
static CK_RV initializeClientKeys( SSLContext_t * pxCtx,
                                   const char * pcLabelName )
{
    CK_RV xResult = CKR_OK;
    CK_SLOT_ID * pxSlotIds = NULL;
    CK_ULONG xCount = 0;
    /*awsupgradeport[[:: fix unused error
    removed: mbedtls_pk_type_t xKeyAlgo = ( mbedtls_pk_type_t ) ~0;
    */

    /* Get the PKCS #11 module/token slot count. */
    if( CKR_OK == xResult )
    {
        xResult = pxCtx->pxP11FunctionList->C_GetSlotList( CK_TRUE,
                                                           NULL,
                                                           &xCount );
    }

    /* Allocate memory to store the token slots. */
    if( CKR_OK == xResult )
    {
        pxSlotIds = ( CK_SLOT_ID * ) pvPortMalloc( sizeof( CK_SLOT_ID ) * xCount );

        if( NULL == pxSlotIds )
        {
            xResult = CKR_HOST_MEMORY;
        }
    }

    /* Get all of the available private key slot identities. */
    if( CKR_OK == xResult )
    {
        xResult = pxCtx->pxP11FunctionList->C_GetSlotList( CK_TRUE,
                                                           pxSlotIds,
                                                           &xCount );
    }

    /* Put the module in authenticated mode. */
    if( CKR_OK == xResult )
    {
        xResult = pxCtx->pxP11FunctionList->C_Login( pxCtx->xP11Session,
                                                     CKU_USER,
                                                     ( CK_UTF8CHAR_PTR ) configPKCS11_DEFAULT_USER_PIN,
                                                     sizeof( configPKCS11_DEFAULT_USER_PIN ) - 1 );
    }

    if( CKR_OK == xResult )
    {
        size_t labelLength = strlen( pcLabelName );

        if( labelLength > pkcs11configMAX_LABEL_LENGTH )
        {
            labelLength = pkcs11configMAX_LABEL_LENGTH;
        }

        /* Get the handle of the device private key. */
        xResult = xFindObjectWithLabelAndClass( pxCtx->xP11Session,
                                                ( char * ) pcLabelName,
                                                labelLength,
                                                CKO_PRIVATE_KEY,
                                                &pxCtx->xP11PrivateKey );
    }

    if( ( CKR_OK == xResult ) && ( pxCtx->xP11PrivateKey == CK_INVALID_HANDLE ) )
    {
        xResult = CK_INVALID_HANDLE;
        LogError( ( "Could not find private key: %s", pcLabelName ) );
    }

    
    if( xResult == CKR_OK )
    {
        xResult = xPKCS11_initMbedtlsPkContext( &( pxCtx->privKey ),
                                                pxCtx->xP11Session,
                                                pxCtx->xP11PrivateKey );
    }

    /* Free memory. */
    vPortFree( pxSlotIds );

    return xResult;
}

/*-----------------------------------------------------------*/
//awsupgradeport[[::without PCKS11
static TlsTransportStatus_t initMbedtls( mbedtls_entropy_context * pEntropyContext,
                                         mbedtls_ctr_drbg_context * pCtrDrgbContext )
{
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    int32_t mbedtlsError = 0;

    #if defined( MBEDTLS_THREADING_ALT )
        /* Set the mutex functions for mbed TLS thread safety. */
        //mbedtls_platform_threading_init();//[tin aws work], build error
    #endif

    /* Initialize contexts for random number generation. */
    mbedtls_entropy_init( pEntropyContext );
    mbedtls_ctr_drbg_init( pCtrDrgbContext );

    if( mbedtlsError != 0 )
    {
        LogError( ( "Failed to add entropy source: mbedTLSError= %s : %s.",
                    mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                    mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
        returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        /* Seed the random number generator. */
        mbedtlsError = mbedtls_ctr_drbg_seed( pCtrDrgbContext,
                                              mbedtls_entropy_func,
                                              pEntropyContext,
                                              NULL,
                                              0 );

        if( mbedtlsError != 0 )
        {
            LogError( ( "Failed to seed PRNG: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( mbedtlsError ),
                        mbedtlsLowLevelCodeOrDefault( mbedtlsError ) ) );
            returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
        }
    }

    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        LogDebug( ( "Successfully initialized mbedTLS." ) );
    }

    return returnStatus;
}
//]]

/*-----------------------------------------------------------*/

TlsTransportStatus_t TLS_FreeRTOS_Connect_P11( NetworkContext_t * pNetworkContext,
                                           const char * pHostName,
                                           uint16_t port,
                                           const NetworkCredentials_t * pNetworkCredentials,
                                           uint32_t receiveTimeoutMs,
                                           uint32_t sendTimeoutMs )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    BaseType_t socketStatus = 0;
    BaseType_t isSocketConnected = pdFALSE;
#if defined(__SUPPORT_AWS_IOT_W__)//awsupgradeport[[::
    TickType_t xRecvTimeout = pdMS_TO_TICKS( receiveTimeoutMs );
    TickType_t xSendTimeout = pdMS_TO_TICKS( sendTimeoutMs );
	UINT32 localPort = AWS_MQTT_PORT;
	(void) xSendTimeout;
    UINT8 isReconnected;

#if CFG_PMGR
	if (RM_PMGR_W_dpm_is_enabled())
	{
		app_dpm_get_client_socket_port(&localPort);
	}
	else
	{
        app_is_reconnected(&isReconnected);
		if (isReconnected)
		{
			//localPort = app_get_random_local_port();
		}
	}
#endif
#endif//]]

    if( ( pNetworkContext == NULL ) ||
        ( pNetworkContext->pParams == NULL ) ||
        ( pHostName == NULL ) ||
        ( pNetworkCredentials == NULL ) )
    {
        LogError( ( "Invalid input parameter(s): Arguments cannot be NULL. pNetworkContext=%p, "
                    "pHostName=%p, pNetworkCredentials=%p.",
                    pNetworkContext,
                    pHostName,
                    pNetworkCredentials ) );
        returnStatus = TLS_TRANSPORT_INVALID_PARAMETER;
    }
    else if( ( pNetworkCredentials->pRootCa == NULL ) )
    {
        LogError( ( "pRootCa cannot be NULL." ) );
        returnStatus = TLS_TRANSPORT_INVALID_PARAMETER;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    /* Establish a TCP connection with the server. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        pTlsTransportParams = pNetworkContext->pParams;

        /* Initialize tcpSocket. */
        pTlsTransportParams->tcpSocket = NULL;

#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
        socketStatus = TCP_Sockets_Connect( &( pTlsTransportParams->tcpSocket ),
                                            pHostName,
                                            port,
                                            receiveTimeoutMs,
                                            sendTimeoutMs );

        if( socketStatus != 0 )
        {
            LogError( ( "Failed to connect to %s with error %d.",
                        pHostName,
                        socketStatus ) );
            returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;
        }
#else//awsupgradeport[[::
		awsiot_app_print_elapse_time_ms("[%s:%d] receiveTimeoutMs=%d", __func__, __LINE__, receiveTimeoutMs);
		if ( ( pTlsTransportParams->tcpSocket = Sockets_Dpm_Open() ) == SOCKETS_INVALID_SOCKET )
		{
			LogError( ( "Failed to open socket." ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;
			goto SKIP;
		}
		else if (( socketStatus = Sockets_SetSockOpt( pTlsTransportParams->tcpSocket, SOCKETS_SO_RCVTIMEO, &xRecvTimeout, sizeof( xRecvTimeout ) ) !=  0 ))
		{
			LogError( ( "Failed to set receive timeout on socket %ld.", socketStatus ) );
			returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
			goto SKIP;
		}
		else if (( socketStatus = Sockets_Dpm_Bind( pTlsTransportParams->tcpSocket , localPort ) ) != 0)
		{
			LogError( ( "Failed to bind to %u with error -%ld.", localPort, -socketStatus ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;			
			goto SKIP;
		}
		else if (( socketStatus = Sockets_Connect( pTlsTransportParams->tcpSocket , pHostName, port ) ) != 0)
		{
			LogError( ( "Failed to connect to server port %u with error -%ld.", port, -socketStatus ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;			
		}
SKIP:
		if( socketStatus == 0 )
		{
			PRINTF("TCP connection OK to \"%s\"\n", pHostName);
		}
#endif//]]
    }

    /* Perform TLS handshake. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        isSocketConnected = pdTRUE;

        returnStatus = tlsSetup_P11( pNetworkContext, pHostName, pNetworkCredentials );
    }

    /* Clean up on failure. */
    if( returnStatus != TLS_TRANSPORT_SUCCESS )
    {
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::   
        if( isSocketConnected == pdTRUE )
        {
            TCP_Sockets_Disconnect( pTlsTransportParams->tcpSocket );
            pTlsTransportParams->tcpSocket = NULL;
        }
#else //awsupgradeport[[::
        if( isSocketConnected == pdTRUE )
        {
            ( void ) Sockets_Close( pTlsTransportParams->tcpSocket );
            pTlsTransportParams->tcpSocket = NULL;
        }
		awsiot_app_print_elapse_time_ms("[%s:%d] (Network connection %p) Connection to %s failed.", __func__, __LINE__, pNetworkContext, pHostName);
#endif//]]

    }
    else
    {
        LogInfo( ( "(Network connection %p) Connection to %s established.",
                   pNetworkContext,
                   pHostName ) );
    }

    return returnStatus;
}

//awsupgradeport[[::without PCKS11
TlsTransportStatus_t TLS_FreeRTOS_Connect( NetworkContext_t * pNetworkContext,
                                           const char * pHostName,
                                           uint16_t port,
                                           const NetworkCredentials_t * pNetworkCredentials,
                                           uint32_t receiveTimeoutMs,
                                           uint32_t sendTimeoutMs )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    TlsTransportStatus_t returnStatus = TLS_TRANSPORT_SUCCESS;
    BaseType_t socketStatus = 0;
    BaseType_t isSocketConnected = pdFALSE, isTlsSetup = pdFALSE;

#if defined(__SUPPORT_AWS_IOT_W__)//awsupgradeport[[::
    TickType_t xRecvTimeout = pdMS_TO_TICKS( receiveTimeoutMs );
    TickType_t xSendTimeout = pdMS_TO_TICKS( sendTimeoutMs );
	UINT32 localPort = AWS_MQTT_PORT;
	(void) xSendTimeout;
    UINT8 isReconnected;

#if CFG_PMGR
	if (RM_PMGR_W_dpm_is_enabled())
	{
		app_dpm_get_client_socket_port(&localPort);
	}
	else
	{
        app_is_reconnected(&isReconnected);
		if (isReconnected)
		{
			//localPort = app_get_random_local_port();
		}
	}
#endif    
#endif//]]

    if( ( pNetworkContext == NULL ) ||
        ( pNetworkContext->pParams == NULL ) ||
        ( pHostName == NULL ) ||
        ( pNetworkCredentials == NULL ) )
    {
        LogError( ( "Invalid input parameter(s): Arguments cannot be NULL. pNetworkContext=%p, "
                    "pHostName=%p, pNetworkCredentials=%p.",
                    pNetworkContext,
                    pHostName,
                    pNetworkCredentials ) );
        returnStatus = TLS_TRANSPORT_INVALID_PARAMETER;
    }
    else if( ( pNetworkCredentials->pRootCa == NULL ) )
    {
        LogError( ( "pRootCa cannot be NULL." ) );
        returnStatus = TLS_TRANSPORT_INVALID_PARAMETER;
    }
    else
    {
        /* Empty else for MISRA 15.7 compliance. */
    }

    /* Establish a TCP connection with the server. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        pTlsTransportParams = pNetworkContext->pParams;

        /* Initialize tcpSocket. */
        pTlsTransportParams->tcpSocket = NULL;

#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::  
        socketStatus = TCP_Sockets_Connect( &( pTlsTransportParams->tcpSocket ),
                                            pHostName,
                                            port,
                                            receiveTimeoutMs,
                                            sendTimeoutMs );

        if( socketStatus != 0 )
        {
            LogError( ( "Failed to connect to %s with error %d.",
                        pHostName,
                        socketStatus ) );
            returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;
        }
#else//awsupgradeport[[::
		awsiot_app_print_elapse_time_ms("[%s:%d] receiveTimeoutMs=%d", __func__, __LINE__, receiveTimeoutMs);
		if ( ( pTlsTransportParams->tcpSocket = Sockets_Dpm_Open() ) == SOCKETS_INVALID_SOCKET )
		{
			LogError( ( "Failed to open socket." ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;
			goto SKIP;
		}
		else if (( socketStatus = Sockets_SetSockOpt( pTlsTransportParams->tcpSocket, SOCKETS_SO_RCVTIMEO, &xRecvTimeout, sizeof( xRecvTimeout ) ) !=  0 ))
		{
			LogError( ( "Failed to set receive timeout on socket %ld.", socketStatus ) );
			returnStatus = TLS_TRANSPORT_INTERNAL_ERROR;
			goto SKIP;
		}
		else if (( socketStatus = Sockets_Dpm_Bind( pTlsTransportParams->tcpSocket , localPort ) ) != 0)
		{
			LogError( ( "Failed to bind to %u with error -%ld.", localPort, -socketStatus ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;			
			goto SKIP;
		}
		else if (( socketStatus = Sockets_Connect( pTlsTransportParams->tcpSocket , pHostName, port ) ) != 0)
		{
			LogError( ( "Failed to connect to server port %u with error -%ld.", port, -socketStatus ) );
			returnStatus = TLS_TRANSPORT_CONNECT_FAILURE;			
		}
SKIP:
		if( socketStatus == 0 )
		{
			PRINTF("TCP connection OK to \"%s\"\n", pHostName);
		}
#endif//]]
    }

    /* Initialize mbedtls. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        isSocketConnected = pdTRUE;

        returnStatus = initMbedtls( &( pTlsTransportParams->sslContext.entropyContext ),
                                    &( pTlsTransportParams->sslContext.ctrDrgbContext ) );
    }

    /* Initialize TLS contexts and set credentials. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        returnStatus = tlsSetup( pNetworkContext, pHostName, pNetworkCredentials );
    }

    /* Perform TLS handshake. */
    if( returnStatus == TLS_TRANSPORT_SUCCESS )
    {
        isTlsSetup = pdTRUE;

        returnStatus = tlsHandshake( pNetworkContext, pNetworkCredentials );
    }

    /* Clean up on failure. */
    if( returnStatus != TLS_TRANSPORT_SUCCESS )
    {
        /* Free SSL context if it's setup. */
        if( isTlsSetup == pdTRUE )
        {
            sslContextFree( &( pTlsTransportParams->sslContext ) );
#if CFG_PMGR//awsupgradeport[[::awsdpmwork:: clear TLS session on DPM mode
            if (RM_PMGR_W_dpm_is_enabled())
            {
                IoT_Error_t dpm_status = app_tls_clear(AWS_TLS_SOCK_NAME);

                if (dpm_status != SUCCESS)
                {
                    LogError(("app_tls_clear(): NG"));
                }
            }
#endif //]]
#if defined(__USE_AWS_TLS_ALT__)//awsupgradeport[[::
            /* Clear the mutex functions for mbed TLS thread safety. */
            //prevent memory leak[[::
#if defined(MBEDTLS_THREADING_ALT)
            //mbedtls_threading_free_alt();
#endif
#endif
//]]
        }

        /* Call Sockets_Disconnect if socket was connected. */
        //[tin aws work], org
/*        
        if( isSocketConnected == pdTRUE )
        {
            TCP_Sockets_Disconnect( pTlsTransportParams->tcpSocket );
            pTlsTransportParams->tcpSocket = NULL;
        }
*/
#if defined(__SUPPORT_AWS_IOT_W__)//awsupgradeport[[::
        if( isSocketConnected == pdTRUE )
        {
            ( void ) Sockets_Close( pTlsTransportParams->tcpSocket );
            pTlsTransportParams->tcpSocket = NULL;
        }
#endif//]]
    }
    else
    {
        LogInfo( ( "(Network connection %p) Connection to %s established.",
                   pNetworkContext,
                   pHostName ) );
    }

    return returnStatus;
}
//]]
/*-----------------------------------------------------------*/

void TLS_FreeRTOS_Disconnect_P11( NetworkContext_t * pNetworkContext )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    BaseType_t tlsStatus = 0;

    if( ( pNetworkContext != NULL ) && ( pNetworkContext->pParams != NULL ) )
    {
        pTlsTransportParams = pNetworkContext->pParams;
        /* Attempting to terminate TLS connection. */
        tlsStatus = ( BaseType_t ) mbedtls_ssl_close_notify( &( pTlsTransportParams->sslContext.context ) );

        /* Ignore the WANT_READ and WANT_WRITE return values. */
        if( ( tlsStatus != ( BaseType_t ) MBEDTLS_ERR_SSL_WANT_READ ) &&
            ( tlsStatus != ( BaseType_t ) MBEDTLS_ERR_SSL_WANT_WRITE ) )
        {
            if( tlsStatus == 0 )
            {
                LogInfo( ( "(Network connection %p) TLS close-notify sent.",
                           pNetworkContext ) );
            }
            else
            {
                LogError( ( "(Network connection %p) Failed to send TLS close-notify: mbedTLSError= %s : %s.",
                            pNetworkContext,
                            mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                            mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );
            }
        }

        /* Call socket shutdown function to close connection. */
        //[tin aws work], org
        //TCP_Sockets_Disconnect( pTlsTransportParams->tcpSocket );
        ( void ) Sockets_Close( pTlsTransportParams->tcpSocket );

        /* Free mbed TLS contexts. */
        sslContextFree_P11( &( pTlsTransportParams->sslContext ) );

#if CFG_PMGR//awsupgradeport[[::awsdpmwork:: clear TLS session on DPM mode
		if (RM_PMGR_W_dpm_is_enabled())
		{				
			IoT_Error_t dpm_status = app_tls_clear(AWS_TLS_SOCK_NAME);
	
			if (dpm_status != SUCCESS)
			{
				LogError(("app_tls_clear(): NG"));
			}
		}
#endif//]]        
    }
}

//awsupgradeport[[::without PCKS11
void TLS_FreeRTOS_Disconnect( NetworkContext_t * pNetworkContext )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    BaseType_t tlsStatus = 0;

    if( ( pNetworkContext != NULL ) && ( pNetworkContext->pParams != NULL ) )
    {
        pTlsTransportParams = pNetworkContext->pParams;
        /* Attempting to terminate TLS connection. */
        tlsStatus = ( BaseType_t ) mbedtls_ssl_close_notify( &( pTlsTransportParams->sslContext.context ) );

        /* Ignore the WANT_READ and WANT_WRITE return values. */
        if( ( tlsStatus != ( BaseType_t ) MBEDTLS_ERR_SSL_WANT_READ ) &&
            ( tlsStatus != ( BaseType_t ) MBEDTLS_ERR_SSL_WANT_WRITE ) )
        {
            if( tlsStatus == 0 )
            {
                LogInfo( ( "(Network connection %p) TLS close-notify sent.",
                           pNetworkContext ) );
            }
            else
            {
                LogError( ( "(Network connection %p) Failed to send TLS close-notify: mbedTLSError= %s : %s.",
                            pNetworkContext,
                            mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                            mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );
            }
        }
        else
        {
            /* WANT_READ and WANT_WRITE can be ignored. Logging for debugging purposes. */
#if !defined(__SUPPORT_AWS_IOT_W__)//[tin aws work], org
            
            LogInfo( ( "(Network connection %p) TLS close-notify sent; "
                       "received %s as the TLS status can be ignored for close-notify.",
                       ( tlsStatus == MBEDTLS_ERR_SSL_WANT_READ ) ? "WANT_READ" : "WANT_WRITE",
                       pNetworkContext ) );
#else//awsupgradeport[[

            LogInfo( ( "(Network connection %p) TLS close-notify sent; "
                       "received %s as the TLS status can be ignored for close-notify.",
                       pNetworkContext,
                       ( tlsStatus == MBEDTLS_ERR_SSL_WANT_READ ) ? "WANT_READ" : "WANT_WRITE" ) );
#endif //]]                    

        }

        /* Call socket shutdown function to close connection. */
#if !defined(__SUPPORT_AWS_IOT_W__)//[tin aws work], org
        TCP_Sockets_Disconnect( pTlsTransportParams->tcpSocket );
#else //awsupgradeport[[        
        ( void ) Sockets_Close( pTlsTransportParams->tcpSocket );
#endif  //]]     

        /* Free mbed TLS contexts. */
        sslContextFree( &( pTlsTransportParams->sslContext ) );

#if CFG_PMGR//awsupgradeport[[::awsdpmwork:: clear TLS session on DPM mode
		if (RM_PMGR_W_dpm_is_enabled())
		{				
			IoT_Error_t dpm_status = app_tls_clear(AWS_TLS_SOCK_NAME);
	
			if (dpm_status != SUCCESS)
			{
				LogError(("app_tls_clear(): NG"));
			}
		}
#endif//]]        
    }
}
//]]
/*-----------------------------------------------------------*/

int32_t TLS_FreeRTOS_recv( NetworkContext_t * pNetworkContext,
                           void * pBuffer,
                           size_t bytesToRecv )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    int32_t tlsStatus = 0;

    if( ( pNetworkContext == NULL ) || ( pNetworkContext->pParams == NULL ) )
    {
        LogError( ( "invalid input, pNetworkContext=%p", pNetworkContext ) );
        tlsStatus = -1;
    }
    else if( pBuffer == NULL )
    {
        LogError( ( "invalid input, pBuffer == NULL" ) );
        tlsStatus = -1;
    }
    else if( bytesToRecv == 0 )
    {
        LogError( ( "invalid input, bytesToRecv == 0" ) );
        tlsStatus = -1;
    }
    else
    {
        pTlsTransportParams = pNetworkContext->pParams;

        tlsStatus = ( int32_t ) mbedtls_ssl_read( &( pTlsTransportParams->sslContext.context ),
                                                  pBuffer,
                                                  bytesToRecv );

        if( ( tlsStatus == MBEDTLS_ERR_SSL_TIMEOUT ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_WANT_READ ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_WANT_WRITE ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ) )
        {
            if( tlsStatus == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET )
            {
                LogDebug( ( "Received a MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET return code from mbedtls_ssl_read." ) );
            }

            LogDebug( ( "Failed to read data. However, a read can be retried on this error. "
                        "mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                        mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );

            /* Mark these set of errors as a timeout. The libraries may retry read
             * on these errors. */
            tlsStatus = 0;
        }
        else if( tlsStatus < 0 )
        {
            LogError( ( "Failed to read data: mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                        mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );
        }
        else
        {
            /* Empty else marker. */
        }
#if CFG_PMGR //awsupgradeport[[::awsdpmwork:: save tls context on RTM
        if((tlsStatus >= 0) || (tlsStatus == MBEDTLS_ERR_SSL_TIMEOUT))
        {
            if (RM_PMGR_W_dpm_is_enabled())
            {
                IoT_Error_t status = 0;
                status = app_tls_save(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));
                if (status)
                {
                    LogError(("failed to save tls session(0x%02x)", status));
                }
            }
        }
#endif //]]
    }

    return tlsStatus;
}

/*-----------------------------------------------------------*/

int32_t TLS_FreeRTOS_send( NetworkContext_t * pNetworkContext,
                           const void * pBuffer,
                           size_t bytesToSend )
{
    TlsTransportParams_t * pTlsTransportParams = NULL;
    int32_t tlsStatus = 0;

    if( ( pNetworkContext == NULL ) || ( pNetworkContext->pParams == NULL ) )
    {
        LogError( ( "invalid input, pNetworkContext=%p", pNetworkContext ) );
        tlsStatus = -1;
    }
    else if( pBuffer == NULL )
    {
        LogError( ( "invalid input, pBuffer == NULL" ) );
        tlsStatus = -1;
    }
    else if( bytesToSend == 0 )
    {
        LogError( ( "invalid input, bytesToSend == 0" ) );
        tlsStatus = -1;
    }
    else
    {
        pTlsTransportParams = pNetworkContext->pParams;
        tlsStatus = ( int32_t ) mbedtls_ssl_write( &( pTlsTransportParams->sslContext.context ),
                                                   pBuffer,
                                                   bytesToSend );

        if( ( tlsStatus == MBEDTLS_ERR_SSL_TIMEOUT ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_WANT_READ ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_WANT_WRITE ) ||
            ( tlsStatus == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET ) )
        {
            if( tlsStatus == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET )
            {
                LogDebug( ( "Received a MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET return code from mbedtls_ssl_write." ) );
            }

            LogDebug( ( "Failed to send data. However, send can be retried on this error. "
                        "mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                        mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );

            /* Mark these set of errors as a timeout. The libraries may retry send
             * on these errors. */
            tlsStatus = 0;
        }
        else if( tlsStatus < 0 )
        {
            LogError( ( "Failed to send data:  mbedTLSError= %s : %s.",
                        mbedtlsHighLevelCodeOrDefault( tlsStatus ),
                        mbedtlsLowLevelCodeOrDefault( tlsStatus ) ) );
        }
        else
        {
            /* Empty else marker. */
        }
#if CFG_PMGR //awsupgradeport[[::awsdpmwork:: save TLS session when read operation completed
        if((tlsStatus >= 0) || (tlsStatus == MBEDTLS_ERR_SSL_TIMEOUT))
        {
            if (RM_PMGR_W_dpm_is_enabled())
            {
                IoT_Error_t status = 0;
                status = app_tls_save(AWS_TLS_SOCK_NAME, &( pTlsTransportParams->sslContext.context ));
                if (status)
                {
                    LogError(("failed to save tls session(0x%02x)", status));
                }
            }
        }
#endif //]]
    }

    return tlsStatus;
}
/*-----------------------------------------------------------*/
