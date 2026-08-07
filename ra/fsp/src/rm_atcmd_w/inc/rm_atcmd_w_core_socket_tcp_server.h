/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_SOCKET_TCP_SERVER_H
#define RM_ATCMD_W_CORE_SOCKET_TCP_SERVER_H

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "rm_atcmd_w_cfg.h"

#include "task.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "rm_atcmd_w_core_socket_parse.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/// TCP Server Connection Prefix
#define ATCMD_TCPS_CONN_RX_PREFIX        "+TRCTS"

/// TCP Server Disconnection Prefix
#define ATCMD_TCPS_DISCONN_RX_PREFIX     "+TRXTS"

/// Rx TCP Server message Prefix
#define ATCMD_TCPS_DATA_RX_PREFIX        "+TRDTS"

#if CFG_PMGR
#define ATCMD_TCPS_MAX_SESS             (ATCMD_NW_TR_MAX_SESSION_CNT_DPM - 1)
#else
#define ATCMD_TCPS_MAX_SESS             ATCMD_NW_TR_MAX_SESSION_CNT
#endif /* CFG_PMGR */

#define ATCMD_TCPS_MAX_TASK_NAME        configMAX_TASK_NAME_LEN
#define ATCMD_TCPS_MAX_SOCK_NAME        configMAX_TASK_NAME_LEN
#define ATCMD_TCPS_CLI_TASK_NAME        "atcts_tc"
#define ATCMD_TCPS_BACKLOG              4
#define ATCMD_TCPS_RECV_TIMEOUT         100     //ms
#define ATCMD_TCPS_SEND_TIMEOUT         500
#define ATCMD_TCPS_SEND_RETRY_CNT       5
#define ATCMD_TCPS_MIN_PORT             1
#define ATCMD_TCPS_MAX_PORT             0xFFFF
#define ATCMD_TCPS_INIT_SOCKET_FD       -1
#define ATCMD_TCPS_WDOG_LATENCY         4

#define ATCMD_TCPS_TASK_NAME            "atcts_t"
#define ATCMD_TCPS_SOCK_NAME            "atcts_s"

#if (ATCMD_TRANSPORT_SDIO_W == 0)
#define ATCMD_TCPS_TASK_PRIORITY        (OS_TASK_PRIORITY_NORMAL + 7) //8
#else
#define ATCMD_TCPS_TASK_PRIORITY        (OS_TASK_PRIORITY_NORMAL + 0) //1
#endif

#define ATCMD_TCPS_TASK_SIZE            (1024 * 2)

#define ATCMD_TCPS_CLI_TASK_PRIORITY    (OS_TASK_PRIORITY_NORMAL + 7) //8
#define ATCMD_TCPS_CLI_TASK_SIZE        (1024 * 4)
#define ATCMD_TCPS_RECV_HDR_SIZE        65

#if (ATCMD_TRANSPORT_UART_W == 1)
#define ATCMD_TCPS_RECV_PAYLOAD_SIZE    (4096 - ATCMD_TCPS_RECV_HDR_SIZE - 3)
#define ATCMD_TCPS_RECV_BUF_SIZE        (ATCMD_TCPS_RECV_HDR_SIZE + ATCMD_TCPS_RECV_PAYLOAD_SIZE + 3)
#else
#define ATCMD_TCPS_RECV_PAYLOAD_SIZE    (4096 - ATCMD_TCPS_RECV_HDR_SIZE - 2)
#define ATCMD_TCPS_RECV_BUF_SIZE        (ATCMD_TCPS_RECV_HDR_SIZE + ATCMD_TCPS_RECV_PAYLOAD_SIZE + 2)
#endif

#define	ATCMD_TCPS_EVT_ANY              0xFF
#define	ATCMD_TCPS_EVT_ACCEPT           0x01
#define	ATCMD_TCPS_EVT_CLOSED           0x02

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _atcmd_tcps_cli_state
{
    ATCMD_TCPS_CLI_STATE_TERMINATED    = 0,
    ATCMD_TCPS_CLI_STATE_READY         = 1,
    ATCMD_TCPS_CLI_STATE_CONNECTED     = 2,
    ATCMD_TCPS_CLI_STATE_DISCONNECTED  = 3,
    ATCMD_TCPS_CLI_STATE_REQ_TERMINATE = 4,
} atcmd_tcps_cli_state;

typedef enum _atcmd_tcps_state
{
    ATCMD_TCPS_STATE_TERMINATED        = 0,
    ATCMD_TCPS_STATE_READY             = 1,
    ATCMD_TCPS_STATE_ACCEPT            = 2,
    ATCMD_TCPS_STATE_REQ_TERMINATE     = 3,
} atcmd_tcps_state;

typedef struct _atcmd_tcps_cli_context
{
    int cid;
    int ip_type;

    TaskHandle_t task_handler;
    char task_name[ATCMD_TCPS_MAX_TASK_NAME];
    unsigned long task_priority;
    size_t task_size;
    EventGroupHandle_t event;

    atcmd_tcps_cli_state state;

    // Recv buffer
    unsigned char * buffer;
    size_t buffer_len;

    int socket;
    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in addr;
    #endif // __SUPPORT_IPV4__
    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 addr6;
    #endif // __SUPPORT_IPV6__

    void * svr_ptr;
    struct _atcmd_tcps_cli_context * next;
} atcmd_tcps_cli_context;

// It's to recover session
typedef struct _atcmd_tcps_sess_info
{
    int local_port;
    int max_allow_client;
    struct sockaddr_storage cli_addr[ATCMD_TCPS_MAX_SESS];
    int ip_type;
} atcmd_tcps_sess_info;

typedef struct _atcmd_tcps_config
{
    int cid;
    int ip_type;

    char task_name[ATCMD_TCPS_MAX_TASK_NAME];
    unsigned long task_priority;
    size_t task_size;

    char sock_name[ATCMD_TCPS_MAX_SOCK_NAME];

    size_t rx_buflen;
    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in local_addr;
    #endif // __SUPPORT_IPV4__
    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 local_addr6;
    #endif // __SUPPORT_IPV6__

    atcmd_tcps_sess_info * sess_info;
} atcmd_tcps_config;

typedef struct _atcmd_tcps_context
{
    TaskHandle_t task_handler;
    atcmd_tcps_state state;
    EventGroupHandle_t event;

    //listen socket
    int socket;

    void * p_at_ctrl;

    const atcmd_tcps_config * conf;

    //client
    int cli_cnt;
    atcmd_tcps_cli_context * cli_ctx;
    SemaphoreHandle_t mutex;
} atcmd_tcps_context;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
void set_tcps_data_mode(int mode);
#endif

int atcmd_tcps_init_context(atcmd_tcps_context * ctx);
int atcmd_tcps_deinit_context(atcmd_tcps_context * ctx);
int atcmd_tcps_init_config(int cid, atcmd_tcps_config * conf, atcmd_tcps_sess_info * sess_info);
int atcmd_tcps_deinit_config(atcmd_tcps_config * conf);
int atcmd_tcps_set_at_ctrl(atcmd_tcps_context * ctx, void * const p_at_ctrl);
int atcmd_tcps_set_local_addr(atcmd_tcps_config * p_conf, int ip_type, char * p_ip, int port);
int atcmd_tcps_set_max_allowed_client(atcmd_tcps_config * conf, int max_allowed_client);
int atcmd_tcps_set_config(atcmd_tcps_context * ctx, atcmd_tcps_config * conf);
int atcmd_tcps_start(atcmd_tcps_context * ctx);
int atcmd_tcps_stop(atcmd_tcps_context * ctx);
int atcmd_tcps_stop_cli(atcmd_tcps_context * ctx, const char * ip, const int port);
int atcmd_tcps_wait_for_ready(atcmd_tcps_context * ctx);
int atcmd_tcps_tx(atcmd_tcps_context * ctx, char * data, unsigned int * data_len, char * ip, unsigned int port);

extern int atcmd_transport_get_available_session(void);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_SOCKET_TCP_SERVER_H */


