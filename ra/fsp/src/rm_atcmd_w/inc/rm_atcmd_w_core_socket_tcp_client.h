/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_SOCKET_TCP_CLIENT_H
#define RM_ATCMD_W_CORE_SOCKET_TCP_CLIENT_H

#include "FreeRTOS.h"
#include "rm_wifi.h"

#include "task.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "rm_atcmd_w_core_socket_parse.h"
#include "rm_atcmd_w_cfg.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/// TCP Client Disconnection Prefix
#define ATCMD_TCPC_DISCONN_RX_PREFIX     "+TRXTC"

/// Rx TCP Client message Prefix
#define ATCMD_TCPC_DATA_RX_PREFIX        "+TRDTC"

/// TCP client reconnection timeout (sec.)
#define ATCMD_TCPC_RECONNECT_TIMEOUT    3

#define ATCMD_TCPC_MAX_TASK_NAME        configMAX_TASK_NAME_LEN
#define ATCMD_TCPC_MAX_SOCK_NAME        configMAX_TASK_NAME_LEN
#define ATCMD_TCPC_RECV_TIMEOUT         100     //ms
#define ATCMD_TCPC_SEND_TIMEOUT         500
#define ATCMD_TCPC_SEND_RETRY_CNT       5
#define ATCMD_TCPC_RECONN_COUNT         3
#define ATCMD_TCPC_WDOG_LATENCY         4

#define ATCMD_TCPC_DEF_PORT             30000
#define ATCMD_TCPC_MIN_PORT             0
#define ATCMD_TCPC_MAX_PORT             0xFFFF

#define ATCMD_TCPC_TASK_NAME            "atctc_t"
#define ATCMD_TCPC_SOCK_NAME            "atctc_s"
#define ATCMD_TCPC_TASK_PRIORITY        (OS_TASK_PRIORITY_NORMAL + 7) //8
#define ATCMD_TCPC_TASK_SIZE            (1024 * 4)  // bytes
#define ATCMD_TCPC_RECV_HDR_SIZE        65
#if (ATCMD_TRANSPORT_UART_W == 1)
#define ATCMD_TCPC_RECV_PAYLOAD_SIZE    (4096 - ATCMD_TCPC_RECV_HDR_SIZE - 3)
#define ATCMD_TCPC_RECV_BUF_SIZE        (ATCMD_TCPC_RECV_HDR_SIZE + ATCMD_TCPC_RECV_PAYLOAD_SIZE + 3)
#else
#define ATCMD_TCPC_RECV_PAYLOAD_SIZE    (4096 - ATCMD_TCPC_RECV_HDR_SIZE - 2)
#define ATCMD_TCPC_RECV_BUF_SIZE        (ATCMD_TCPC_RECV_HDR_SIZE + ATCMD_TCPC_RECV_PAYLOAD_SIZE + 2)
#endif

#define ATCMD_TCPC_EVT_ANY              0xFF
#define ATCMD_TCPC_EVT_CONNECTION       0x01
#define ATCMD_TCPC_EVT_CLOSED           0x02

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _atcmd_tcpc_state
{
    ATCMD_TCPC_STATE_TERMINATED    = 0,
    ATCMD_TCPC_STATE_DISCONNECTED  = 1,
    ATCMD_TCPC_STATE_CONNECTED     = 2,
    ATCMD_TCPC_STATE_REQ_TERMINATE = 3,
} atcmd_tcpc_state;

typedef struct _atcmd_tcpc_sess_info
{
    int  local_port;
    int  peer_port;
    char peer_ipaddr[ATCMD_NVR_NW_TR_PEER_IPADDR_LEN];
} atcmd_tcpc_sess_info;

typedef struct _atcmd_tcpc_config
{
    int  cid;
    int  ip_type;
    char task_name[ATCMD_TCPC_MAX_TASK_NAME];
    unsigned long task_priority;
    size_t task_size;

    char sock_name[ATCMD_TCPC_MAX_SOCK_NAME];

    size_t rx_buflen;

    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in local_addr;
    struct sockaddr_in svr_addr;
    #endif // __SUPPORT_IPV4__

    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 local_addr6;
    struct sockaddr_in6 svr_addr6;
    #endif // __SUPPORT_IPV4__

    atcmd_tcpc_sess_info * sess_info;
} atcmd_tcpc_config;

typedef struct _atcmd_tcpc_context
{
    TaskHandle_t task_handler;
    atcmd_tcpc_state state;
    EventGroupHandle_t event;

    // Receive buffer
    unsigned char * buffer;
    size_t buffer_len;

    // Socket
    int socket;

    void * p_at_ctrl;

    const atcmd_tcpc_config * conf;
} atcmd_tcpc_context;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
void set_tcpc_data_mode(int mode);
#endif

int atcmd_tcpc_init_context(atcmd_tcpc_context * ctx);
int atcmd_tcpc_deinit_context(atcmd_tcpc_context * ctx);
int atcmd_tcpc_init_config(const int cid, atcmd_tcpc_config * conf, atcmd_tcpc_sess_info * sess_info);
int atcmd_tcpc_deinit_config(atcmd_tcpc_config * conf);
int atcmd_tcpc_set_at_ctrl(atcmd_tcpc_context * ctx, void * const p_at_ctrl);
int atcmd_tcpc_set_local_addr(atcmd_tcpc_config * p_conf, char * p_ip, int port);
int atcmd_tcpc_set_svr_addr(atcmd_tcpc_config * conf, char * ip, int port);
int atcmd_tcpc_set_config(atcmd_tcpc_context * ctx, atcmd_tcpc_config * conf);
int atcmd_tcpc_wait_for_ready(atcmd_tcpc_context * ctx);
int atcmd_tcpc_start(atcmd_tcpc_context * ctx);
int atcmd_tcpc_stop(atcmd_tcpc_context * ctx);
int atcmd_tcpc_tx(atcmd_tcpc_context * ctx, char * data, unsigned int * data_len);
int atcmd_tcpc_tx_with_peer_info(atcmd_tcpc_context * ctx, char * ip, int port, char * data, unsigned int * data_len);

unsigned short atcmd_get_random_value_ushort(void);
unsigned short atcmd_get_random_value_ushort_range(int lower, int upper);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_SOCKET_TCP_CLIENT_H */

