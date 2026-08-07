/**
 ****************************************************************************************
 *
 * @file rm_cli_w_http.h
 *
 * @brief HTTP feature.
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#ifndef __RM_CLI_W_HTTP_CLIENT_H__
#define __RM_CLI_W_HTTP_CLIENT_H__

#include "custom_config_sdk.h"
#include "FreeRTOS.h"
#include "iface_defs.h"
#include "rm_https_api.h"
#include "rm_https_w.h"

#define HTTPC_MIN_INCOMING_LEN        (1024 * 1)
#define HTTPC_MAX_INCOMING_LEN        (1024 * 20)
#define HTTPC_DEF_INCOMING_LEN        (1024 * 4)
#define HTTPC_MIN_OUTGOING_LEN        (1024 * 1)
#define HTTPC_MAX_OUTGOING_LEN        (1024 * 20)
#define HTTPC_DEF_OUTGOING_LEN        (1024 * 4)

#define HTTPC_MAX_ALPN_CNT            3
#define HTTPC_MAX_ALPN_LEN            24
#define HTTPC_MAX_SNI_LEN             64
#define HTTPC_MAX_STOP_TIMEOUT        (300 * HTTPC_DEF_TIMEOUT)

fsp_err_t rm_cli_w_run_user_http_client(int argc, char * argv[]);
fsp_err_t rm_cli_w_run_user_http_server(int argc, char * argv[]);

#endif // (__RM_CLI_W_HTTP_CLIENT_H__)

/* EOF */
