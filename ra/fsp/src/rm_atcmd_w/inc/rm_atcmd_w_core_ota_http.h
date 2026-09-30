/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_OTA_HTTP_H
#define RM_ATCMD_W_CORE_OTA_HTTP_H

#include "FreeRTOS.h"
#include "rm_wifi.h"

#include "rm_atcmd_w_core_ota_update.h"
#include "rm_atcmd_w_core_ota_common.h"
#include "rm_atcmd_w_core_http.h"
#include "mbedtls/ssl.h"

/// Receive buffer size.
#define ATCMD_W_OTA_HTTP_RX_BUF_SZ                     ATCMD_W_OTA_SFLASH_BUF_SZ

#define ATCMD_W_OTA_HTTPC_MIN_INCOMING_LEN             (1024 * 1)
#define ATCMD_W_OTA_HTTPC_MAX_INCOMING_LEN             (1024 * 20)
#define ATCMD_W_OTA_HTTPC_DEF_INCOMING_LEN             (1024 * 4)
#define ATCMD_W_OTA_HTTPC_MIN_OUTGOING_LEN             (1024 * 1)
#define ATCMD_W_OTA_HTTPC_MAX_OUTGOING_LEN             (1024 * 20)
#define ATCMD_W_OTA_HTTPC_DEF_OUTGOING_LEN             (1024 * 4)

#define ATCMD_W_OTA_HTTPC_MAX_ALPN_CNT                 3
#define ATCMD_W_OTA_HTTPC_MAX_ALPN_LEN                 24
#define ATCMD_W_OTA_HTTPC_MAX_SNI_LEN                  64
#define ATCMD_W_OTA_HTTPC_MAX_STOP_TIMEOUT             (300 * HTTPC_DEF_TIMEOUT)

// Default auth mode value
#define ATCMD_W_OTA_HTTPC_TLS_AUTHMODE_DEF             MBEDTLS_SSL_VERIFY_NONE

// Default tls version
#define ATCMD_W_OTA_HTTPC_TLS_VERSION_DEF              0 // ONLY_TLS12

// NVRAM name of HTTP-CLIENT TLS version
#define ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_VER         "OTA_TLS_VER"

// NVRAM name of HTTP-CLIENT TLS auth_mode
#define ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH        "OTA_TLS_AUTHMODE"

// NVRAM name of HTTP-CLIENT TLS alpn
#define ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN        "OTA_TLS_ALPN"

// NVRAM name of the number of TLS alpn
#define ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM    "OTA_TLS_ALPN_NUM"

// NVRAM name of HTTP-CLIENT TLS SNI
#define ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI         "OTA_TLS_SNI"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum e_atcmd_w_core_ota_https_tls_ver
{
    ATCMD_W_OTA_HTTPS_TLS12    = 0,
    ATCMD_W_OTA_HTTPS_TLS13    = 1,
    ATCMD_W_OTA_HTTPS_TLS12_13 = 2
} atcmd_w_core_ota_https_tls_ver_t;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

UINT atcmd_w_ota_update_http_client_request(atcmd_w_ota_update_proc_t * at_ota_proc);
UINT atcmd_w_ota_http_client_get_download_status(void);
void atcmd_w_ota_http_client_set_download_status(UINT status);
UINT atcmd_w_ota_http_client_get_result(void);

UINT atcmd_w_ota_http_client_set_tls_auth_mode(int tls_auth_mode);
UINT atcmd_w_ota_http_client_set_tls_version(int tls_ver);
UINT atcmd_w_ota_http_client_set_sni(char * sni);
UINT atcmd_w_ota_http_client_set_alpn(char * alpn0, char * alpn1, char * alpn2);

UINT   atcmd_w_ota_http_client_get_tls_auth_mode(void);
UINT   atcmd_w_ota_http_client_get_tls_version(void);
size_t atcmd_w_ota_http_client_get_sni(char * sni, size_t buf_len);
size_t atcmd_w_ota_http_client_get_alpn0(char * alpn0, size_t buf_len);
size_t atcmd_w_ota_http_client_get_alpn1(char * alpn1, size_t buf_len);
size_t atcmd_w_ota_http_client_get_alpn2(char * alpn2, size_t buf_len);
void   atcmd_w_ota_http_client_del_all_alpn(void);

err_t atcmd_w_ota_update_httpc_cb_headers_done_fn(httpc_state_t * connection,
                                                  void          * arg,
                                                  struct pbuf   * hdr,
                                                  u16_t           hdr_len,
                                                  u32_t           content_len);
void atcmd_w_ota_update_httpc_cb_result_fn(void         * arg,
                                           httpc_result_t httpc_result,
                                           u32_t          rx_content_len,
                                           u32_t          srv_res,
                                           err_t          err);

#endif                                 /* RM_ATCMD_W_CORE_OTA_HTTP_H */

/* EOF */
