/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __RM_CLI_W_HTTP_CLIENT_H__
#define __RM_CLI_W_HTTP_CLIENT_H__

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "custom_config_sdk.h"
#include "FreeRTOS.h"
#include "iface_defs.h"
#if CFG_WIFI
 #include "rm_https_api.h"
 #include "rm_https_w.h"
#endif                                 /* CFG_WIFI */
#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define HTTPC_MIN_INCOMING_LEN             (1024 * 1)
#define HTTPC_MAX_INCOMING_LEN             (1024 * 20)
#define HTTPC_DEF_INCOMING_LEN             (1024 * 4)
#define HTTPC_MIN_OUTGOING_LEN             (1024 * 1)
#define HTTPC_MAX_OUTGOING_LEN             (1024 * 20)
#define HTTPC_DEF_OUTGOING_LEN             (1024 * 4)

#define HTTPC_MAX_ALPN_CNT                 3
#define HTTPC_MAX_ALPN_LEN                 24
#define HTTPC_MAX_SNI_LEN                  64
#define HTTPC_MAX_STOP_TIMEOUT             (300 * HTTPC_DEF_TIMEOUT)

// NVRAM name of HTTP-CLIENT TLS version
#define HTTPC_NVRAM_CONFIG_TLS_VER         "HTTPC_TLS_VER"

// NVRAM name of HTTP-CLIENT TLS auth_mode
#define HTTPC_NVRAM_CONFIG_TLS_AUTH        "HTTPC_TLS_AUTHMODE"

// NVRAM name of HTTP-CLIENT TLS alpn
#define HTTPC_NVRAM_CONFIG_TLS_ALPN        "HTTPC_TLS_ALPN"

// NVRAM name of the number of TLS alpn
#define HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM    "HTTPC_TLS_ALPN_NUM"

// NVRAM name of HTTP-CLIENT TLS SNI
#define HTTPC_NVRAM_CONFIG_TLS_SNI         "HTTPC_TLS_SNI"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum e_atcmd_w_core_https_tls_ver
{
    ATCMD_W_HTTPS_TLS12    = 0,
    ATCMD_W_HTTPS_TLS13    = 1,
    ATCMD_W_HTTPS_TLS12_13 = 2
} atcmd_w_core_https_tls_ver_t;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

err_t http_client_set_alpn(int argc, char * argv[]);
err_t http_client_set_sni(int argc, char * argv[]);
err_t http_client_set_tls_auth_mode(int tls_auth_mode);
err_t http_client_get_tls_auth_mode(int * p_tls_auth_mode);
err_t http_client_set_tls_version(int tls_ver);
err_t http_client_get_tls_version(int * p_tls_ver);

fsp_err_t rm_atcmd_w_run_user_http_client(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[]);
fsp_err_t rm_atcmd_w_run_user_http_server(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[]);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 // (__RM_CLI_W_HTTP_CLIENT_H__)

/* EOF */
