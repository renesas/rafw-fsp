/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR

 #if CFG_WIFI

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
  #include "FreeRTOS.h"
  #include "custom_config_sdk.h"

  #include "common.h"
  #include "mbedtls/private_access.h"
  #include "mbedtls/platform.h"
  #include "mbedtls/ssl_misc.h"

  #include "lwip/err.h"

  #include "rm_pmgr_w_instance.h"      /* For PMGR_SSL_DPM_SUPPORT of rm_pmgr_cfg.h */

  #pragma GCC diagnostic ignored "-Wsign-compare"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
  #undef  RRQ61X_DPM_TLS_DBG_INFO
  #define RRQ61X_DPM_TLS_DBG_ERR

  #define DPM_TLS_PRINTF    printf

  #ifdef  RRQ61X_DPM_TLS_DBG_INFO
   #define DPM_TLS_DBG_INFO(fmt, ...) \
    DPM_TLS_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
  #else
   #define DPM_TLS_DBG_INFO(...)    do {} while (0);
  #endif                               /* RRQ61X_DPM_TLS_DBG_INFO */

  #ifdef  RRQ61X_DPM_TLS_DBG_ERR
   #define DPM_TLS_DBG_ERR(fmt, ...) \
    DPM_TLS_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
  #else
   #define DPM_TLS_DBG_ERR(...)    do {} while (0);
  #endif                               /* RRQ61X_DPM_TLS_DBG_ERR */

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
  #if defined(PMGR_SSL_DPM_SUPPORT)

// internal function
static int is_supported_tls_dpm ()
{
    if (!RM_PMGR_W_dpm_is_enabled())
    {
        DPM_TLS_DBG_ERR("Only for dpm mode\n");

        return pdFALSE;
    }

    return pdTRUE;
}

static size_t cal_tls_session_size (mbedtls_ssl_context * ssl_ctx)
{
    size_t total_size = 0;

    // calculate ssl context size
    total_size += sizeof(mbedtls_ssl_context);
    total_size += 13;
    total_size += 13;
   #if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
    if (ssl_ctx->cli_id_len)
    {
        total_size += ssl_ctx->cli_id_len;
    }
   #endif                              /* MBEDTLS_SSL_DTLS_HELLO_VERIFY */

    DPM_TLS_DBG_INFO("sizeof(mbedtls_ssl_context):%d, sizeof(mbedtls_ssl_session):%d\n",
                     sizeof(mbedtls_ssl_context),
                     sizeof(mbedtls_ssl_session));

    DPM_TLS_DBG_INFO("total size after adding ssl_context(%d)\n", total_size);

    // calculate ssl session size
    total_size += sizeof(mbedtls_ssl_session);
   #if !defined(RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT)
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    if (ssl_ctx->session->peer_cert)
    {
        total_size += ssl_ctx->session->peer_cert->raw.len;
    }
    total_size += 4;
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */
    #if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    if (ssl_ctx->session->ticket_len)
    {
        total_size += ssl_ctx->session->ticket_len;
    }
    #endif                             /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */
   #endif                              /* RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT */

    DPM_TLS_DBG_INFO("total size after adding mbedtls_ssl_session(%d)\n", total_size);

    // calculate ssl transform size
    if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_2)
    {
        total_size += sizeof(ssl_ctx->transform->keyblk);
    }
    else if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_3)
    {
        total_size += sizeof(ssl_ctx->transform_application->keyblk);
    }

    DPM_TLS_DBG_INFO("total size after adding mbedtls_ssl_transform(%d)\n", total_size);

    DPM_TLS_DBG_INFO("expected tls session size(%d)\n", total_size);

    return total_size;
}

/*
 * @a: rtm memory ptr.
 * @alen: size of parameter a.
 * @b: mbedtls_ssl_context to save.
 */
static int is_duplicated_session (const unsigned char       * start,
                                  const unsigned char       * end,
                                  const mbedtls_ssl_context * ssl_ctx)
{
    const unsigned char * pos = start;

    pos += sizeof(mbedtls_ssl_context);

    // check in contents buffer
    if ((pos + 13 < end) && (memcmp(pos, ssl_ctx->in_buf, 13) != 0))
    {
        DPM_TLS_DBG_INFO("in coming buffer is changed\n");

        return 0;
    }

    pos += 13;

    // check out contents buffer
    if ((pos + 13 < end) && (memcmp(pos, ssl_ctx->out_buf, 13) != 0))
    {
        DPM_TLS_DBG_INFO("out going buffer is changed\n");

        return 0;
    }

    return 1;
}

static int save_ssl_context (mbedtls_ssl_context * ssl_ctx, unsigned char ** msgpos, unsigned char * end)
{
    unsigned char * pos         = *msgpos;
    size_t          ssl_ctx_len = sizeof(mbedtls_ssl_context);

    if (!ssl_ctx)
    {
        DPM_TLS_DBG_ERR("ssl context is null\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (ssl_ctx_len > (unsigned int) (end - pos))
    {
        DPM_TLS_DBG_ERR("buffer is not enough(%d:%zu)\n", end - pos, ssl_ctx_len);

        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    memcpy(pos, ssl_ctx, ssl_ctx_len);
    pos += ssl_ctx_len;

    memcpy(pos, ssl_ctx->in_buf, 13);
    pos += 13;

    memcpy(pos, ssl_ctx->out_buf, 13);
    pos += 13;

   #if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
    if (ssl_ctx->cli_id_len)
    {
        memcpy(pos, ssl_ctx->cli_id, ssl_ctx->cli_id_len);
        pos += ssl_ctx->cli_id_len;
    }
   #endif                              /* MBEDTLS_SSL_DTLS_HELLO_VERIFY */

    DPM_TLS_DBG_INFO("ssl context size(%d)\n", pos - *msgpos);

    *msgpos = pos;

    return 0;
}

static int save_ssl_session (mbedtls_ssl_session * session, unsigned char ** msgpos, unsigned char * end)
{
    unsigned char * pos             = *msgpos;
    size_t          ssl_session_len = sizeof(mbedtls_ssl_session);
   #if !defined(RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT)
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    size_t cert_len = 0;
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */
   #endif                              /* RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT */

    if (!session)
    {
        DPM_TLS_DBG_ERR("session is null\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (ssl_session_len > (unsigned int) (end - pos))
    {
        DPM_TLS_DBG_ERR("buffer is not enough(%d:%zu)\n", end - pos, ssl_session_len);

        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    memcpy(pos, session, ssl_session_len);
    pos += ssl_session_len;

   #if !defined(RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT)
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    if (session->peer_cert)
    {
        cert_len = session->peer_cert->raw.len;
    }
    else
    {
        cert_len = 0;
    }

    if (4 + cert_len > (unsigned int) (end - pos))
    {
        DPM_TLS_DBG_ERR("buffer is not enough(%ld:%ld)\n", end - pos, 3 + cert_len);

        return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    }

    *pos++ = (unsigned char) (cert_len >> 24 & 0xFF);
    *pos++ = (unsigned char) (cert_len >> 16 & 0xFF);
    *pos++ = (unsigned char) (cert_len >> 8 & 0xFF);
    *pos++ = (unsigned char) (cert_len & 0xFF);

    if (session->peer_cert)
    {
        memcpy(pos, session->peer_cert->raw.p, cert_len);
    }
    pos += cert_len;
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */

    // TODO: maybe it's not required. it's setup by user call.
    #if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    if (session->ticket_len)
    {
        memcpy(pos, session->ticket, session->ticket_len);
        pos += session->ticket_len;
    }
    #endif                             /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */
   #endif                              /* RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT */

    DPM_TLS_DBG_INFO("ssl session size(%d)\n", pos - *msgpos);

    *msgpos = pos;

    return 0;
}

static int save_ssl_transform (mbedtls_ssl_transform * transform, unsigned char ** msgpos, unsigned char * end)
{
    unsigned char * pos = *msgpos;
    FSP_PARAMETER_NOT_USED(end);

    if (!transform)
    {
        DPM_TLS_DBG_ERR("transform is null\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    memcpy(pos, &transform->keyblk, sizeof(transform->keyblk));
    pos += sizeof(transform->keyblk);

    DPM_TLS_DBG_INFO("ssl transform size(%d)\n", pos - *msgpos);

    *msgpos = pos;

    return 0;
}

static int restore_ssl_context (mbedtls_ssl_context * dst, unsigned char ** msgpos, unsigned char * end)
{
    unsigned char       * pos = *msgpos;
    mbedtls_ssl_context * src = NULL;

    if (sizeof(mbedtls_ssl_context) > end - pos)
    {
        DPM_TLS_DBG_ERR("failed to resotre ssl context\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    src  = (mbedtls_ssl_context *) pos;
    pos += sizeof(mbedtls_ssl_context);

    if (src->state != MBEDTLS_SSL_HANDSHAKE_OVER)
    {
        DPM_TLS_DBG_ERR("ssl state is not established(%d)\n", src->state);

        return -1;
    }

    dst->state = src->state;
   #if defined(MBEDTLS_SSL_RENEGOTIATION)
    dst->renego_status       = src->renego_status;
    dst->renego_records_seen = src->renego_records_seen;
   #endif                              /* MBEDTLS_SSL_RENEGOTIATION */

    dst->tls_version = src->tls_version;

    dst->badmac_seen_or_in_hsfraglen = src->badmac_seen_or_in_hsfraglen;

    mbedtls_ssl_handshake_free(dst);
    mbedtls_free(dst->handshake);
    dst->handshake = NULL;

    dst->in_msgtype = src->in_msgtype;
    dst->in_msglen  = src->in_msglen;
    dst->in_left    = src->in_left;
   #if defined(MBEDTLS_SSL_PROTO_DTLS)
    dst->in_epoch           = src->in_epoch;
    dst->next_record_offset = src->next_record_offset;
   #endif                              /* MBEDTLS_SSL_PROTO_DTLS */
   #if defined(MBEDTLS_SSL_DTLS_ANTI_REPLAY)
    dst->in_window_top = src->in_window_top;
    dst->in_window     = src->in_window;
   #endif                              /* MBEDTLS_SSL_DTLS_ANTI_REPLAY */
    dst->in_hslen             = src->in_hslen;
    dst->nb_zero              = src->nb_zero;
    dst->keep_current_message = src->keep_current_message;

    dst->out_msgtype = src->out_msgtype;
    dst->out_msglen  = src->out_msglen;
    dst->out_left    = src->out_left;
    memcpy(dst->cur_out_ctr, src->cur_out_ctr, sizeof(dst->cur_out_ctr));
   #if defined(MBEDTLS_SSL_PROTO_DTLS)
    dst->mtu = src->mtu;
   #endif                              /* MBEDTLS_SSL_PROTO_DTLS */

    dst->secure_renegotiation = src->secure_renegotiation;

   #if defined(MBEDTLS_SSL_RENEGOTIATION)
    dst->verify_data_len = src->verify_data_len;
    memcpy(dst->own_verify_data, src->own_verify_data, MBEDTLS_SSL_VERIFY_DATA_MAX_LEN);
    memcpy(dst->peer_verify_data, src->peer_verify_data, MBEDTLS_SSL_VERIFY_DATA_MAX_LEN);
   #endif                              /* MBEDTLS_SSL_RENEGOTIATION */

   #if defined(MBEDTLS_SSL_DTLS_CONNECTION_ID)
    memcpy(dst->own_cid, src->own_cid, MBEDTLS_SSL_CID_IN_LEN_MAX);
    dst->own_cid_len   = src->own_cid_len;
    dst->negotiate_cid = src->negotiate_cid;
   #endif                              /* MBEDTLS_SSL_DTLS_CONNECTION_ID */

    memcpy(dst->prev_in_iv, src->prev_in_iv, sizeof(dst->prev_in_iv));

    memcpy(dst->in_buf, pos, 13);
    pos += 13;

    memcpy(dst->out_buf, pos, 13);
    pos += 13;

   #if defined(MBEDTLS_SSL_DTLS_HELLO_VERIFY)
    if (src->cli_id_len)
    {
        dst->cli_id = mbedtls_calloc(1, src->cli_id_len);
        if (dst->cli_id)
        {
            memcpy(dst->cli_id, pos, src->cli_id_len);
            dst->cli_id_len = src->cli_id_len;
        }
        else
        {
            DPM_TLS_DBG_ERR("Failed to copy client id\n");
        }

        pos += src->cli_id_len;
    }
   #endif                              /* MBEDTLS_SSL_DTLS_HELLO_VERIFY */

    *msgpos = pos;

    return 0;
}

// TODO: Release allocated memory of previous session, dst
static int restore_ssl_session (mbedtls_ssl_session * dst, unsigned char ** msgpos, unsigned char * end)
{
    unsigned char * pos = *msgpos;
   #if !defined(RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT)
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    size_t cert_len = 0;
    int    ret      = 0;
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */
   #endif                              /* RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT */
    size_t ssl_session_len = sizeof(mbedtls_ssl_session);

    if (pos + ssl_session_len > end)
    {
        DPM_TLS_DBG_ERR("Invalid session size\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    memcpy(dst, pos, ssl_session_len);
    pos += ssl_session_len;

   #if !defined(RRQ61X_DPM_TLS_NOT_SAVE_PERR_CERT)
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    if (pos + 4 > end)
    {
        DPM_TLS_DBG_ERR("Invalid cert size\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    cert_len = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
    pos     += 4;

    if (cert_len == 0)
    {
        dst->peer_cert = NULL;
    }
    else
    {
        if (pos + cert_len > end)
        {
            DPM_TLS_DBG_ERR("Invalid cert size\n");

            return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
        }

        dst->peer_cert = mbedtls_calloc(1, sizeof(mbedtls_x509_crt));
        if (!dst->peer_cert)
        {
            DPM_TLS_DBG_ERR("Failed to allocate memory\n");

            return MBEDTLS_ERR_SSL_ALLOC_FAILED;
        }

        mbedtls_x509_crt_init(dst->peer_cert);

        ret = mbedtls_x509_crt_parse_der(dst->peer_cert, pos, cert_len);
        if (ret)
        {
            DPM_TLS_DBG_ERR("Failed to parse peer cert(0x%x)\n", -ret);

            mbedtls_x509_crt_free(dst->peer_cert);
            mbedtls_free(dst->peer_cert);
            dst->peer_cert = NULL;

            return ret;
        }

        pos += cert_len;
    }
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */

    #if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    if (dst->ticket_len)
    {
        dst->ticket = mbedtls_calloc(1, dst->ticket_len);
        if (dst->ticket == NULL)
        {
            DPM_TLS_DBG_ERR("Failed to allocate session ticket\n");

            return MBEDTLS_ERR_SSL_ALLOC_FAILED;
        }

        memcpy(dst->ticket, pos, dst->ticket_len);
        pos += dst->ticket_len;
    }
    #endif                             /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */
   #else /* RRQ61X_DPM_TLS_NOT_SAVE_PEER_CERT */
    #if defined(MBEDTLS_X509_CRT_PARSE_C)
    dst->peer_cert = NULL;
    #endif                             /* MBEDTLS_X509_CRT_PARSE_C */

    #if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    dst->ticket_len = 0;
    dst->ticket     = NULL;
    #endif                             /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */
   #endif                              /* RRQ61X_DPM_TLS_NOT_SAVE_PEER_CERT */

    *msgpos = pos;

    return 0;
}

static int restore_ssl_tls12_transform (mbedtls_ssl_context * dst, unsigned char ** msgpos, unsigned char * end)
{
    int ret = 0;

    mbedtls_ssl_transform * transform             = dst->transform;
    int                               ciphersuite = 0;
    unsigned char                   * keyblk      = NULL;
    unsigned char                   * key1        = NULL;
    unsigned char                   * key2        = NULL;
    unsigned char                   * mac_enc     = NULL;
    unsigned char                   * mac_dec     = NULL;
    size_t                            mac_key_len = 0;
    size_t                            iv_copy_len = 0;
    size_t                            keylen      = 0;
    mbedtls_ssl_mode_t                ssl_mode;
    const mbedtls_ssl_ciphersuite_t * ciphersuite_info;
   #if !defined(MBEDTLS_USE_PSA_CRYPTO)
    const mbedtls_md_info_t     * md_info;
    const mbedtls_cipher_info_t * cipher_info;
   #endif                              /* !MBEDTLS_USE_PSA_CRYPTO */

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_key_type_t       key_type;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t      alg;
    psa_algorithm_t      mac_alg = 0;
    size_t               key_bits;
    psa_status_t         status = PSA_ERROR_CORRUPTION_DETECTED;
   #endif

   #if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM)
    int encrypt_then_mac = dst->session->encrypt_then_mac;
   #endif                              /* MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM */

    unsigned char * pos = *msgpos;

    size_t keyblk_size = sizeof(dst->transform->keyblk);

   #if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM)
    transform->encrypt_then_mac = encrypt_then_mac;
   #endif                              /* MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM */
    transform->tls_version = dst->tls_version;

    ciphersuite = dst->session->ciphersuite;

    DPM_TLS_DBG_INFO("ciphersuite: 0x%x\n", (unsigned int) ciphersuite);

    if ((unsigned int) (end - pos) < keyblk_size)
    {
        DPM_TLS_DBG_ERR("Failed to restore transform\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    keyblk = pos;
    pos   += keyblk_size;

    ciphersuite_info = mbedtls_ssl_ciphersuite_from_id(ciphersuite);
    if (ciphersuite_info == NULL)
    {
        DPM_TLS_DBG_ERR("ciphersuite info for %d not found\n", ciphersuite);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    ssl_mode = mbedtls_ssl_get_mode_from_ciphersuite(
   #if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM)
        encrypt_then_mac,
   #endif                              /* MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM */
        ciphersuite_info);

    if (ssl_mode == MBEDTLS_SSL_MODE_AEAD)
    {
        transform->taglen =
            ciphersuite_info->flags & MBEDTLS_CIPHERSUITE_SHORT_TAG ? 8 : 16;
    }

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    if ((status =
             mbedtls_ssl_cipher_to_psa(ciphersuite_info->cipher, transform->taglen, &alg, &key_type,
                                       &key_bits)) != PSA_SUCCESS)
    {
        ret = psa_ssl_status_to_mbedtls(status);
        DPM_TLS_DBG_ERR("mbedtls_ssl_cipher_to_psa(0x%x)\n", (unsigned int) (-ret));
        goto end;
    }

   #else
    cipher_info = mbedtls_cipher_info_from_type(ciphersuite_info->cipher);
    if (cipher_info == NULL)
    {
        DPM_TLS_DBG_ERR("cipher info for %u not found\n", ciphersuite_info->cipher);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    mac_alg = mbedtls_hash_info_psa_from_md(ciphersuite_info->mac);
    if (mac_alg == 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_hash_info_psa_from_md for %u not found\n", (unsigned int) ciphersuite_info->mac);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

   #else
    md_info = mbedtls_md_info_from_type(ciphersuite_info->mac);
    if (md_info == NULL)
    {
        DPM_TLS_DBG_ERR("mbedtls_md info for %u not found\n", (unsigned int) ciphersuite_info->mac);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

    // Skipped to compute key block using the PRF.

    // Determine the appropriate key, IV and MAC length.
   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    keylen = PSA_BITS_TO_BYTES(key_bits);
   #else
    keylen = mbedtls_cipher_info_get_key_bitlen(cipher_info) / 8;
   #endif

   #if defined(MBEDTLS_GCM_C) || \
    defined(MBEDTLS_CCM_C) ||    \
    defined(MBEDTLS_CHACHAPOLY_C)
    if (ssl_mode == MBEDTLS_SSL_MODE_AEAD)
    {
        size_t explicit_ivlen;

        transform->maclen = 0;
        mac_key_len       = 0;

        transform->ivlen = 12;

        int is_chachapoly = 0;
    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        is_chachapoly = (key_type == PSA_KEY_TYPE_CHACHA20);
    #else
        is_chachapoly = (mbedtls_cipher_info_get_mode(cipher_info) ==
                         MBEDTLS_MODE_CHACHAPOLY);
    #endif                             /* MBEDTLS_USE_PSA_CRYPTO */

        if (is_chachapoly)
        {
            transform->fixed_ivlen = 12;
        }
        else
        {
            transform->fixed_ivlen = 4;
        }

        explicit_ivlen    = transform->ivlen - transform->fixed_ivlen;
        transform->minlen = explicit_ivlen + transform->taglen;
    }
    else
   #endif                              /* MBEDTLS_GCM_C || MBEDTLS_CCM_C || MBEDTLS_CHACHAPOLY_C */
   #if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    if ((ssl_mode == MBEDTLS_SSL_MODE_STREAM) ||
        (ssl_mode == MBEDTLS_SSL_MODE_CBC) ||
        (ssl_mode == MBEDTLS_SSL_MODE_CBC_ETM))
    {
    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        size_t block_size = PSA_BLOCK_CIPHER_BLOCK_LENGTH(key_type);
    #else
        size_t block_size = cipher_info->block_size;
    #endif                             /* MBEDTLS_USE_PSA_CRYPTO */

    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        mac_key_len = PSA_HASH_LENGTH(mac_alg);
    #else
        if (((ret = mbedtls_md_setup(&transform->md_ctx_enc, md_info, 1)) != 0) ||
            ((ret = mbedtls_md_setup(&transform->md_ctx_dec, md_info, 1)) != 0))
        {
            DPM_TLS_DBG_ERR("mbedtls_md_setup(0x%x)\n", (unsigned int) (-ret));
            goto end;
        }
        mac_key_len = mbedtls_md_get_size(md_info);
    #endif                             /* MBEDTLS_USE_PSA_CRYPTO */
        transform->maclen = mac_key_len;

    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        transform->ivlen = PSA_CIPHER_IV_LENGTH(key_type, alg);
    #else
        transform->ivlen = mbedtls_cipher_info_get_iv_size(cipher_info);
    #endif                             /* MBEDTLS_USE_PSA_CRYPTO */

        if (ssl_mode == MBEDTLS_SSL_MODE_STREAM)
        {
            transform->minlen = transform->maclen;
        }
        else
        {
    #if defined(MBEDTLS_SSL_ENCRYPT_THEN_MAC)
            if (ssl_mode == MBEDTLS_SSL_MODE_CBC_ETM)
            {
                transform->minlen = transform->maclen + block_size;
            }
            else
    #endif                             /* MBEDTLS_SSL_ENCRYPT_THEN_MAC */
            {
                transform->minlen = transform->maclen +
                                    block_size -
                                    transform->maclen % block_size;
            }

            transform->minlen += transform->ivlen;
        }
    }
   #endif                              /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

    DPM_TLS_DBG_INFO("keylen: %u, minlen: %u, ivlen: %u, maclen: %u\n",
                     (unsigned int) keylen,
                     (unsigned int) transform->minlen,
                     (unsigned int) transform->ivlen,
                     (unsigned int) transform->maclen);

   #if defined(MBEDTLS_SSL_CLI_C)
    if (dst->conf->endpoint == MBEDTLS_SSL_IS_CLIENT)
    {
        key1 = keyblk + mac_key_len * 2;
        key2 = keyblk + mac_key_len * 2 + keylen;

        mac_enc = keyblk;
        mac_dec = keyblk + mac_key_len;

        iv_copy_len = (transform->fixed_ivlen) ? transform->fixed_ivlen : transform->ivlen;
        memcpy(transform->iv_enc, key2 + keylen, iv_copy_len);
        memcpy(transform->iv_dec, key2 + keylen + iv_copy_len, iv_copy_len);
    }
    else
   #endif                              /* MBEDTLS_SSL_CLI_C */
   #if defined(MBEDTLS_SSL_SRV_C)
    if (dst->conf->endpoint == MBEDTLS_SSL_IS_SERVER)
    {
        key1 = keyblk + mac_key_len * 2 + keylen;
        key2 = keyblk + mac_key_len * 2;

        mac_enc = keyblk + mac_key_len;
        mac_dec = keyblk;

        iv_copy_len = (transform->fixed_ivlen) ? transform->fixed_ivlen : transform->ivlen;
        memcpy(transform->iv_dec, key1 + keylen, iv_copy_len);
        memcpy(transform->iv_enc, key1 + keylen + iv_copy_len, iv_copy_len);
    }
   #endif                              /* MBEDTLS_SSL_SRV_C */

    memcpy(&transform->keyblk, keyblk, keyblk_size);

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    transform->psa_alg = alg;
    if (alg != MBEDTLS_SSL_NULL_CIPHER)
    {
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
        psa_set_key_algorithm(&attributes, alg);
        psa_set_key_type(&attributes, key_type);

        if ((status =
                 psa_import_key(&attributes, key1, PSA_BITS_TO_BYTES(key_bits),
                                &transform->psa_key_enc)) != PSA_SUCCESS)
        {
            DPM_TLS_DBG_ERR("psa_import_key(%d)\n", (int) status);
            ret = psa_ssl_status_to_mbedtls(status);
            DPM_TLS_DBG_ERR("psa_import_key(%d)\n", ret);
            goto end;
        }

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);

        if ((status =
                 psa_import_key(&attributes, key2, PSA_BITS_TO_BYTES(key_bits),
                                &transform->psa_key_dec)) != PSA_SUCCESS)
        {
            ret = psa_ssl_status_to_mbedtls(status);
            DPM_TLS_DBG_ERR("psa_import_key(%d)", ret);
            goto end;
        }
    }

   #else
    ret = mbedtls_cipher_setup(&transform->cipher_ctx_enc, cipher_info);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setup(0x%x)\n", (unsigned int) (-ret));
        goto end;
    }

    ret = mbedtls_cipher_setup(&transform->cipher_ctx_dec, cipher_info);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setup(0x%x)\n", (unsigned int) (-ret));
        goto end;
    }

    ret = mbedtls_cipher_setkey(&transform->cipher_ctx_enc,
                                key1,
                                (int) mbedtls_cipher_info_get_key_bitlen(cipher_info),
                                MBEDTLS_ENCRYPT);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setkey(0x%x)\n", (unsigned int) (-ret));
        goto end;
    }

    ret = mbedtls_cipher_setkey(&transform->cipher_ctx_dec,
                                key2,
                                (int) mbedtls_cipher_info_get_key_bitlen(cipher_info),
                                MBEDTLS_DECRYPT);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setkey(0x%x)", (unsigned int) (-ret));
        goto end;
    }

    #if defined(MBEDTLS_CIPHER_MODE_CBC)
    if (mbedtls_cipher_info_get_mode(cipher_info) == MBEDTLS_MODE_CBC)
    {
        ret = mbedtls_cipher_set_padding_mode(&transform->cipher_ctx_enc, MBEDTLS_PADDING_NONE);
        if (ret != 0)
        {
            DPM_TLS_DBG_ERR("mbedtls_cipher_set_padding_mode(0x%x)\n", (unsigned int) (-ret));
            goto end;
        }

        ret = mbedtls_cipher_set_padding_mode(&transform->cipher_ctx_dec, MBEDTLS_PADDING_NONE);
        if (ret != 0)
        {
            DPM_TLS_DBG_ERR("mbedtls_cipher_set_padding_mode(0x%x)\n", (unsigned int) (-ret));
            goto end;
        }
    }
    #endif                             /* MBEDTLS_CIPHER_MODE_CBC */
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

   #if defined(MBEDTLS_SSL_SOME_SUITES_USE_MAC)
    if (mac_key_len != 0)
    {
    #if defined(MBEDTLS_USE_PSA_CRYPTO)
        transform->psa_mac_alg = PSA_ALG_HMAC(mac_alg);

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(mac_alg));
        psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);

        status = psa_import_key(&attributes, mac_enc, mac_key_len, &transform->psa_mac_enc);
        if (status != PSA_SUCCESS)
        {
            ret = psa_ssl_status_to_mbedtls(status);
            DPM_TLS_DBG_ERR("psa_import_mac_key(0x%x)\n", (unsigned int) (-ret));
            goto end;
        }

        if ((transform->psa_alg == MBEDTLS_SSL_NULL_CIPHER) ||
            ((transform->psa_alg == PSA_ALG_CBC_NO_PADDING)
     #if defined(MBEDTLS_SSL_SOME_SUITES_USE_CBC_ETM)
             && (transform->encrypt_then_mac == MBEDTLS_SSL_ETM_DISABLED)
     #endif
            ))
        {
            /* mbedtls_ct_hmac() requires the key to be exportable */
            psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_VERIFY_HASH);
        }
        else
        {
            psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_VERIFY_HASH);
        }

        status = psa_import_key(&attributes, mac_dec, mac_key_len, &transform->psa_mac_dec);
        if (status != PSA_SUCCESS)
        {
            ret = psa_ssl_status_to_mbedtls(status);
            DPM_TLS_DBG_ERR("psa_import_mac_key(0x%x)\n", (unsigned int) (-ret));
            goto end;
        }

    #else
        ret = mbedtls_md_hmac_starts(&transform->md_ctx_enc, mac_enc, mac_key_len);
        if (ret != 0)
        {
            goto end;
        }

        ret = mbedtls_md_hmac_starts(&transform->md_ctx_dec, mac_dec, mac_key_len);
        if (ret != 0)
        {
            goto end;
        }
    #endif                             /* MBEDTLS_USE_PSA_CRYPTO */
    }
   #endif                              /* MBEDTLS_SSL_SOME_SUITES_USE_MAC */

end:

    // memset(keyblk, 0x00, sizeof(keyblk));

    return 0;
}

static int restore_ssl_tls13_transform (mbedtls_ssl_context * dst, unsigned char ** msgpos, unsigned char * end)
{
    mbedtls_ssl_transform * transform = dst->transform_application;
    int endpoint    = dst->conf->endpoint;
    int ciphersuite = dst->session->ciphersuite;

   #if !defined(MBEDTLS_USE_PSA_CRYPTO)
    int ret;
    mbedtls_cipher_info_t const * cipher_info;
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */
    const mbedtls_ssl_ciphersuite_t * ciphersuite_info;
    unsigned char const             * key_enc = NULL;
    unsigned char const             * iv_enc  = NULL;
    unsigned char const             * key_dec = NULL;
    unsigned char const             * iv_dec  = NULL;

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    psa_key_type_t       key_type;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_algorithm_t      alg;
    size_t               key_bits;
    psa_status_t         status = PSA_SUCCESS;
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

    unsigned char * pos         = *msgpos;
    size_t          keyset_size = sizeof(mbedtls_ssl_key_set);

    if ((unsigned int) (end - pos) < keyset_size)
    {
        DPM_TLS_DBG_ERR("Failed to restore transform\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    if (transform == NULL)
    {
        DPM_TLS_DBG_ERR("Not initialized transform\n");

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    ciphersuite_info = mbedtls_ssl_ciphersuite_from_id(ciphersuite);
    if (ciphersuite_info == NULL)
    {
        DPM_TLS_DBG_ERR("[%s]ciphersuite info for %d not found\n", __func__, ciphersuite);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

   #if !defined(MBEDTLS_USE_PSA_CRYPTO)
    cipher_info = mbedtls_cipher_info_from_type(ciphersuite_info->cipher);
    if (cipher_info == NULL)
    {
        DPM_TLS_DBG_ERR("cipher info for %u not found\n", ciphersuite_info->cipher);

        return MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
    }

    ret = mbedtls_cipher_setup(&transform->cipher_ctx_enc, cipher_info);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setup(0x%x)\n", (unsigned int) (-ret));

        return ret;
    }

    ret = mbedtls_cipher_setup(&transform->cipher_ctx_dec, cipher_info);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setup(0x%x)\n", (unsigned int) (-ret));

        return ret;
    }
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

    memcpy(&transform->keyblk, pos, sizeof(transform->keyblk));
    pos += sizeof(transform->keyblk);

   #if defined(MBEDTLS_SSL_SRV_C)
    if (endpoint == MBEDTLS_SSL_IS_SERVER)
    {
        key_enc = transform->keyblk.tlsv13.server_write_key;
        key_dec = transform->keyblk.tlsv13.client_write_key;
        iv_enc  = transform->keyblk.tlsv13.server_write_iv;
        iv_dec  = transform->keyblk.tlsv13.client_write_iv;
    }
    else
   #endif                              /* MBEDTLS_SSL_SRV_C */
   #if defined(MBEDTLS_SSL_CLI_C)
    if (endpoint == MBEDTLS_SSL_IS_CLIENT)
    {
        key_enc = transform->keyblk.tlsv13.client_write_key;
        key_dec = transform->keyblk.tlsv13.server_write_key;
        iv_enc  = transform->keyblk.tlsv13.client_write_iv;
        iv_dec  = transform->keyblk.tlsv13.server_write_iv;
    }
   #endif                              /* MBEDTLS_SSL_CLI_C */

    memcpy(transform->iv_enc, iv_enc, transform->keyblk.tlsv13.iv_len);
    memcpy(transform->iv_dec, iv_dec, transform->keyblk.tlsv13.iv_len);

   #if !defined(MBEDTLS_USE_PSA_CRYPTO)
    ret = mbedtls_cipher_setkey(&transform->cipher_ctx_enc, key_enc, cipher_info->key_bitlen, MBEDTLS_ENCRYPT);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setkey(0x%x)\n", (unsigned int) (-ret));

        return ret;
    }

    ret = mbedtls_cipher_setkey(&transform->cipher_ctx_dec, key_dec, cipher_info->key_bitlen, MBEDTLS_DECRYPT);
    if (ret != 0)
    {
        DPM_TLS_DBG_ERR("mbedtls_cipher_setkey(0x%x)", (unsigned int) (-ret));

        return ret;
    }
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

    if ((ciphersuite_info->flags & MBEDTLS_CIPHERSUITE_SHORT_TAG) != 0)
    {
        transform->taglen = 8;
    }
    else
    {
        transform->taglen = 16;
    }

    transform->ivlen       = transform->keyblk.tlsv13.iv_len;
    transform->maclen      = 0;
    transform->fixed_ivlen = transform->ivlen;
    transform->tls_version = MBEDTLS_SSL_VERSION_TLS1_3;

    transform->minlen =
        transform->taglen + MBEDTLS_SSL_CID_TLS1_3_PADDING_GRANULARITY;

   #if defined(MBEDTLS_USE_PSA_CRYPTO)
    status = mbedtls_ssl_cipher_to_psa(ciphersuite_info->cipher, transform->taglen, &alg, &key_type, &key_bits);
    if (status != PSA_SUCCESS)
    {
        DPM_TLS_DBG_ERR("mbedtls_ssl_cipher_to_psa(0x%x)\n", psa_ssl_status_to_mbedtls(status));

        return psa_ssl_status_to_mbedtls(status);
    }

    transform->psa_alg = alg;

    if (alg != MBEDTLS_SSL_NULL_CIPHER)
    {
        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
        psa_set_key_algorithm(&attributes, alg);
        psa_set_key_type(&attributes, key_type);

        status = psa_import_key(&attributes, key_enc, PSA_BITS_TO_BYTES(key_bits), &transform->psa_key_enc);
        if (status != PSA_SUCCESS)
        {
            DPM_TLS_DBG_ERR("psa_import_key(0x%x)\n", psa_ssl_status_to_mbedtls(status));

            return psa_ssl_status_to_mbedtls(status);
        }

        psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);

        status = psa_import_key(&attributes, key_dec, PSA_BITS_TO_BYTES(key_bits), &transform->psa_key_dec);
        if (status != PSA_SUCCESS)
        {
            DPM_TLS_DBG_ERR("psa_import_key(0x%x)\n", psa_ssl_status_to_mbedtls(status));

            return psa_ssl_status_to_mbedtls(status);
        }
    }
   #endif                              /* MBEDTLS_USE_PSA_CRYPTO */

    *msgpos = pos;

    return 0;
}

static int restore_ssl_transform (mbedtls_ssl_context * dst, unsigned char ** msgpos, unsigned char * end)
{
    DPM_TLS_DBG_INFO("tls_version(0x%x)\n", dst->tls_version);

    if (dst->tls_version == MBEDTLS_SSL_VERSION_TLS1_2)
    {
        return restore_ssl_tls12_transform(dst, msgpos, end);
    }
    else if (dst->tls_version == MBEDTLS_SSL_VERSION_TLS1_3)
    {
        return restore_ssl_tls13_transform(dst, msgpos, end);
    }

    return -1;
}

// external function
int RM_PMGR_W_dpm_tls_session_set (const char * name, mbedtls_ssl_context * ssl_ctx)
{
    int    ret           = 0;
    size_t expected_size = 0;

    unsigned char * pos = NULL;
    unsigned char * end = NULL;

    unsigned int  status      = ERR_OK;
    unsigned int  stored_size = 0;
    unsigned long wait_option = RRQ61X_DPM_TLS_DEF_TIMEOUT;

    if (!is_supported_tls_dpm())
    {
        DPM_TLS_DBG_ERR("Not supported\n");

        return ER_NOT_SUPPORTED;
    }

    if ((ssl_ctx == NULL) || (ssl_ctx->session == NULL) ||
        ((ssl_ctx->transform == NULL) && (ssl_ctx->transform_application == NULL)))
    {
        DPM_TLS_DBG_ERR("Invalid tls session\n");

        return ER_INVALID_PARAMETERS;
    }

    if (ssl_ctx->state != MBEDTLS_SSL_HANDSHAKE_OVER)
    {
        DPM_TLS_DBG_ERR("tls session is not established(%d)\n", ssl_ctx->state);

        return -1;
    }

    // Calculate expected tls session size
    expected_size = cal_tls_session_size(ssl_ctx);
    if (expected_size == 0)
    {
        DPM_TLS_DBG_ERR("Failed to calculate tls session size\n");

        return -1;
    }

    // allocate rtm memory
    status = RM_PMGR_W_user_rtm_pool_alloc((char *) name, (void **) &pos, expected_size, wait_option);
    if (status == ER_DUPLICATED_ENTRY)
    {
        stored_size = RM_PMGR_W_user_rtm_get((char *) name, &pos);
        if (stored_size != expected_size)
        {
            // release previous one & allocate new one
            status = RM_PMGR_W_user_rtm_free((char *) name);
            if (status)
            {
                DPM_TLS_DBG_ERR("Failed to release rtm memory(0x%x)\n", status);

                return -1;
            }

            status = RM_PMGR_W_user_rtm_pool_alloc((char *) name, (VOID **) &pos, expected_size, wait_option);
            if (status)
            {
                DPM_TLS_DBG_ERR("Failed to allocate rtm memory(0x%x)\n", status);

                return -1;
            }
        }
        else
        {
            if (is_duplicated_session(pos, pos + expected_size, ssl_ctx))
            {
                DPM_TLS_DBG_INFO("No need to save tls session\n");

                return 0;
            }
        }
    }
    else if (status == ERR_OK)
    {
        DPM_TLS_DBG_INFO("Succeed allocated rtm memory(%d)\n", expected_size);
    }
    else
    {
        DPM_TLS_DBG_ERR("Failed to allocate rtm memory(0x%x)\n", status);

        return -1;
    }

    memset(pos, 0x00, expected_size);
    end = pos + expected_size;

    // save ssl context
    ret = save_ssl_context(ssl_ctx, &pos, end);
    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to save ssl context(0x%x)\n", ret);
        goto fail;
    }

    // save ssl session
    ret = save_ssl_session(ssl_ctx->session, &pos, end);
    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to save ssl session(0x%x)\n", ret);
        goto fail;
    }

    // save ssl transform
    if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_2)
    {
        ret = save_ssl_transform(ssl_ctx->transform, &pos, end);
    }
    else if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_3)
    {
        ret = save_ssl_transform(ssl_ctx->transform_application, &pos, end);
    }
    else
    {
        DPM_TLS_DBG_ERR("Invalid version(0x%x)\n", ssl_ctx->tls_version);
        goto fail;
    }

    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to save ssl transform(0x%x)\n", (unsigned int) (-ret));
        goto fail;
    }

    return 0;

fail:

    RM_PMGR_W_user_rtm_free((char *) name);

    return ret;
}

int RM_PMGR_W_dpm_tls_session_get (const char * name, mbedtls_ssl_context * ssl_ctx)
{
    int ret = 0;

    unsigned char * pos = NULL;
    unsigned char * end = NULL;

    unsigned int stored_size = 0;

    if (!is_supported_tls_dpm())
    {
        DPM_TLS_DBG_ERR("Not supported\n");

        return ER_NOT_SUPPORTED;
    }

    // check initialization
    if ((ssl_ctx == NULL) || (ssl_ctx->state != 0))
    {
        DPM_TLS_DBG_ERR("TLS session has to be initialized\n");

        return ER_INVALID_PARAMETERS;
    }

    // get allocated rtm memory
    stored_size = RM_PMGR_W_user_rtm_get((char *) name, &pos);
    if (stored_size == 0)
    {
        DPM_TLS_DBG_INFO("There is no saved data\n");

        return ER_NOT_FOUND;
    }

    end = pos + stored_size;

    // restore ssl context
    ret = restore_ssl_context(ssl_ctx, &pos, end);
    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to restore ssl context(0x%x)\n", -ret);

        return ret;
    }

    // restore ssl session
    ssl_ctx->session           = ssl_ctx->session_negotiate;
    ssl_ctx->session_in        = ssl_ctx->session;
    ssl_ctx->session_out       = ssl_ctx->session;
    ssl_ctx->session_negotiate = NULL;

    ret = restore_ssl_session(ssl_ctx->session, &pos, end);
    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to restore ssl session(0x%x)\n", -ret);

        return ret;
    }

    // restore ssl transform
    if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_2)
    {
        ssl_ctx->transform     = ssl_ctx->transform_negotiate;
        ssl_ctx->transform_in  = ssl_ctx->transform;
        ssl_ctx->transform_out = ssl_ctx->transform;

        ssl_ctx->transform_negotiate = NULL;
    }
    else if (ssl_ctx->tls_version == MBEDTLS_SSL_VERSION_TLS1_3)
    {
        ssl_ctx->transform_application = ssl_ctx->transform_negotiate;
        ssl_ctx->transform_in          = ssl_ctx->transform_application;
        ssl_ctx->transform_out         = ssl_ctx->transform_application;

        ssl_ctx->transform           = NULL;
        ssl_ctx->transform_negotiate = NULL;
    }
    else
    {
        DPM_TLS_DBG_ERR("Invalid version(0x%x)\n", ssl_ctx->tls_version);

        return -1;
    }

    ret = restore_ssl_transform(ssl_ctx, &pos, end);
    if (ret)
    {
        DPM_TLS_DBG_ERR("Failed to restore ssl transform(0x%x)\n", -ret);

        return ret;
    }

    mbedtls_ssl_update_in_pointers(ssl_ctx);

    mbedtls_ssl_update_out_pointers(ssl_ctx, ssl_ctx->transform_out);

    return 0;
}

int RM_PMGR_W_dpm_tls_session_clear (const char * name)
{
    unsigned int status = 0;

    if (!is_supported_tls_dpm())
    {
        DPM_TLS_DBG_ERR("Not supported\n");

        return ER_NOT_SUPPORTED;
    }

    status = RM_PMGR_W_user_rtm_free((char *) name);
    if (status)
    {
        if (status != ER_NOT_FOUND)
        {
            DPM_TLS_DBG_ERR("Failed to release rtm memory(0x%x)\n", status);
        }

        return status;
    }

    return 0;
}

  #else
int RM_PMGR_W_dpm_tls_session_set (const char * name, mbedtls_ssl_context * ssl_ctx)
{
    DPM_TLS_DBG_ERR("Not supported\n");

    return -1;
}

int RM_PMGR_W_dpm_tls_session_get (const char * name, mbedtls_ssl_context * ssl_ctx)
{
    DPM_TLS_DBG_ERR("Not supported\n");

    return -1;
}

int RM_PMGR_W_dpm_tls_session_clear (const char * name)
{
    DPM_TLS_DBG_ERR("Not supported\n");

    return -1;
}

  #endif                               // PMGR_SSL_DPM_SUPPORT

 #endif                                /* CFG_WIFI */

#endif                                 /* CFG_PMGR */
