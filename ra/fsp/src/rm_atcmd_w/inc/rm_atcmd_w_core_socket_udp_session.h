/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_SOCKET_UDP_SESSION_H
#define RM_ATCMD_W_CORE_SOCKET_UDP_SESSION_H

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
/// Rx UDP message Prefix
#define ATCMD_UDP_DATA_RX_PREFIX         "+TRDUS"
/// UDP Session Fail Prefix
#define ATCMD_UDP_SESS_FAIL_PREFIX       "+TRXUS"

#define ATCMD_UDPS_MAX_TASK_NAME		configMAX_TASK_NAME_LEN
#define ATCMD_UDPS_MAX_SOCK_NAME		configMAX_TASK_NAME_LEN
#define ATCMD_UDPS_RECV_TIMEOUT			100 //ms
#define ATCMD_UDPS_MIN_PORT				0
#define ATCMD_UDPS_MAX_PORT				0xFFFF
#define ATCMD_UDPS_WDOG_LATENCY			4

#define ATCMD_UDPS_TASK_NAME			"atcus_t"
#define ATCMD_UDPS_SOCK_NAME			"atcus_s"
#define ATCMD_UDPS_TASK_PRIORITY		(OS_TASK_PRIORITY_NORMAL + 7) //8
#define ATCMD_UDPS_TASK_SIZE			(1024 * 1)
#define ATCMD_UDPS_RECV_HDR_SIZE		65
#if (ATCMD_TRANSPORT_UART_W == 1)
#define ATCMD_UDPS_RECV_PAYLOAD_SIZE	(2048 - ATCMD_UDPS_RECV_HDR_SIZE - 3)
#define ATCMD_UDPS_RECV_BUF_SIZE		(ATCMD_UDPS_RECV_HDR_SIZE + ATCMD_UDPS_RECV_PAYLOAD_SIZE + 3)
#else
#if (ATCMD_TRANSPORT_SDIO_W == 1)
#define ATCMD_UDPS_RECV_PAYLOAD_SIZE	(1536 - ATCMD_UDPS_RECV_HDR_SIZE - 2)
#else
#define ATCMD_UDPS_RECV_PAYLOAD_SIZE	(2048 - ATCMD_UDPS_RECV_HDR_SIZE - 2)
#endif
#define ATCMD_UDPS_RECV_BUF_SIZE		(ATCMD_UDPS_RECV_HDR_SIZE + ATCMD_UDPS_RECV_PAYLOAD_SIZE + 2)
#endif

#define	ATCMD_UDPS_EVT_ANY				0xFF
#define	ATCMD_UDPS_EVT_ACTIVE			0x01
#define	ATCMD_UDPS_EVT_CLOSED			0x02

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _atcmd_udps_state
{
    ATCMD_UDPS_STATE_TERMINATED = 0,

    ATCMD_UDPS_STATE_READY = 1,

    ATCMD_UDPS_STATE_ACTIVE = 2,

    ATCMD_UDPS_STATE_REQ_TERMINATE = 3,
} atcmd_udps_state;

typedef struct _atcmd_udps_sess_info
{
    int local_port;
    uint32_t peer_port;
    char peer_ipaddr[ATCMD_NVR_NW_TR_PEER_IPADDR_LEN];
    int ip_type;
} atcmd_udps_sess_info;

typedef struct _atcmd_udps_config
{
    int cid;
    int ip_type;

    char task_name[ATCMD_UDPS_MAX_TASK_NAME];
    unsigned long task_priority;
    size_t task_size;

    char sock_name[ATCMD_UDPS_MAX_SOCK_NAME];

    size_t rx_buflen;
    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in local_addr;
    struct sockaddr_in peer_addr;
    #endif // __SUPPORT_IPV4__

    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 local_addr6;
    struct sockaddr_in6 peer_addr6;
    #endif // __SUPPORT_IPV6__

    atcmd_udps_sess_info * sess_info;

} atcmd_udps_config;

typedef struct _atcmd_udps_context
{
    TaskHandle_t task_handler;
    atcmd_udps_state state;
    EventGroupHandle_t event;

    //recv buffer
    unsigned char * buffer;
    size_t buffer_len;

    //socket
    int socket;

    void * p_at_ctrl;

    const atcmd_udps_config * conf;
} atcmd_udps_context;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
int atcmd_udps_init_context(atcmd_udps_context * ctx);

int atcmd_udps_deinit_context(atcmd_udps_context * ctx);

int atcmd_udps_init_config(const int cid, atcmd_udps_config * conf, atcmd_udps_sess_info * sess_info);

int atcmd_udps_deinit_config(atcmd_udps_config * conf);

int atcmd_udps_set_at_ctrl(atcmd_udps_context * ctx, void * const p_at_ctrl);

int atcmd_udps_set_local_addr(atcmd_udps_config * p_conf, int ip_type, char * p_ip, int port);

int atcmd_udps_set_peer_addr(atcmd_udps_config * p_conf, int ip_type, char * p_ip, int port);

int atcmd_udps_set_config(atcmd_udps_context * ctx, atcmd_udps_config * conf);

int atcmd_udps_wait_for_ready(atcmd_udps_context * ctx);

int atcmd_udps_start(atcmd_udps_context * ctx);

int atcmd_udps_stop(atcmd_udps_context * ctx);

int atcmd_udps_tx(atcmd_udps_context * ctx, char * data, unsigned int * data_len, char * ip, unsigned int port);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_SOCKET_UDP_SESSION_H */


