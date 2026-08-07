/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/

#ifndef RM_ATCMD_W_CORE_WEBSOCKET_CLIENT_H
#define RM_ATCMD_W_CORE_WEBSOCKET_CLIENT_H

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "task.h"

#include "websocket_client.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ATCMD_WEBSOCKET_CLIENT_WDOG_LATENCY 4

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
#if defined(__SUPPORT_WEBSOCKET_CLIENT_FOR_ATCMD__)
void websocket_event_handler (websocket_client_event_id_t event_id, websocket_client_event_data_t *event_data);
ws_err_t websocket_client_add_header(char * len, char * buffer);
ws_err_t websocket_client_connect(void * p_at_ctrl, char * uri);
ws_err_t websocket_client_disconnect();
ws_err_t websocket_client_send_msg(char *msg);
ws_err_t websocket_client_config(char * ping_intv_sec, char * ping_timeout_sec, char * buffer_size);
#if CFG_PMGR
void websocket_auto_start_begin(void);
#endif /* CFG_PMGR */
#endif

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif // RM_ATCMD_W_CORE_WEBSOCKET_CLIENT_H


