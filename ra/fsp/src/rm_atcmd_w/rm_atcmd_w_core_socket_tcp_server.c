/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_tcp_server.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"

#include "rm_wifi.h"
#include "rm_lwip_w_helper.h"

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#undef  ENABLE_ATCMD_TCPS_DBG_INFO
#undef  ENABLE_ATCMD_TCPS_DBG_ERR

#define	ATCMD_TCPS_DBG	printf

#if defined (ENABLE_ATCMD_TCPS_DBG_INFO)
#define	ATCMD_TCPS_INFO(fmt, ...)	\
    ATCMD_TCPS_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	ATCMD_TCPS_INFO(...)	do {} while (0)
#endif	// (ENABLE_ATCMD_TCPS_DBG_INFO)

#if defined (ENABLE_ATCMD_TCPS_DBG_ERR)
#define	ATCMD_TCPS_ERR(fmt, ...)	\
    ATCMD_TCPS_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	ATCMD_TCPS_ERR(...)	do {} while (0)
#endif // (ENABLE_ATCMD_TCPS_DBG_ERR)

#define ATCMD_TCPS_MIN_BACKLOG  3

#if (ATCMD_TRANSPORT_SDIO_W == 1)
#define ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#endif

#if defined ( __SUPPORT_IPV6__ )
extern bool isIPv6Address(char * str, struct sockaddr_in6 * valid_ip);
#endif // __SUPPORT_IPV6__

static int atcmd_tcps_ready_to_accept_socket(atcmd_tcps_context * ctx);

static void atcmd_tcps_close_socket(int * sock);

static void atcmd_tcps_task_entry(void * param);

static atcmd_tcps_cli_context * atcmd_tcps_cli_create_context(atcmd_tcps_context * svr_ctx, int cli_sock,
        struct sockaddr_storage * cli_addr);

static int atcmd_tcps_cli_delete_context(atcmd_tcps_cli_context ** ctx);

static int atcmd_tcps_cli_start(atcmd_tcps_cli_context * ctx);

static int atcmd_tcps_cli_stop(atcmd_tcps_cli_context * ctx);

static int atcmd_tcps_cli_add(atcmd_tcps_context * svr_ctx, atcmd_tcps_cli_context * cli_ctx);

#if defined (__ENABLE_UNUSED__)
static atcmd_tcps_cli_context * atcmd_tcps_cli_remove_by_ip(atcmd_tcps_context * svr_ctx, struct sockaddr_in * ip_addr);
#endif // __ENABLE_UNUSED__

static atcmd_tcps_cli_context * atcmd_tcps_cli_remove_by_ctx(atcmd_tcps_context * svr_ctx,
        atcmd_tcps_cli_context * cli_ctx);

static atcmd_tcps_cli_context * atcmd_tcps_cli_find_by_ip(atcmd_tcps_context * svr_ctx, struct sockaddr_in * ip_addr);
static atcmd_tcps_cli_context * atcmd_tcps_cli_find_by_ip6(atcmd_tcps_context * svr_ctx, struct sockaddr_in6 * ip_addr);

static int atcmd_tcps_cli_release(atcmd_tcps_context * svr_ctx, atcmd_tcps_cli_context * cli_ctx);

static void atcmd_tcps_cli_task_entry(void * param);

static int atcmd_tcps_compare_ip_addr(struct sockaddr_in * a, struct sockaddr_in * b);
static int atcmd_tcps_compare_ip_addr6(struct sockaddr_in6 * a, struct sockaddr_in6 * b);

#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
static int tcps_data_mode = 0;

static int is_tcps_data_mode_hexstring(void)
{
    return tcps_data_mode == 1 ? 1 : 0;
}

void set_tcps_data_mode(int mode)
{
    /*
        TCP_DATA_MODE_ASCII     = 0
        TCP_DATA_MODE_HEXSTRING = 1
    */
    tcps_data_mode = mode;
}

static void Convert_Str2HexStr(char * in, char * out, u32_t in_size)
{
    u32_t cnt = 0;
    u32_t i = 0;

    while (cnt < in_size)
    {
        sprintf((char *)(out + i), "%02X", in[cnt]);
        cnt += 1;
        i += 2;
    }
}
#endif

int atcmd_tcps_init_context(atcmd_tcps_context * ctx)
{
    ATCMD_TCPS_INFO("Start\n");
    memset(ctx, 0x00, sizeof(atcmd_tcps_context));
    ctx->socket = ATCMD_TCPS_INIT_SOCKET_FD;
    ctx->state = ATCMD_TCPS_STATE_TERMINATED;
    return 0;
}

int atcmd_tcps_deinit_context(atcmd_tcps_context * ctx)
{
    ATCMD_TCPS_INFO("Start\n");

    if (ctx->state != ATCMD_TCPS_STATE_TERMINATED)
    {
        ATCMD_TCPS_ERR("TCP server is not terminated(%d)\n", ctx->state);
        return -1;
    }

    atcmd_tcps_close_socket(&ctx->socket);

    if (ctx->event)
    {
        vEventGroupDelete(ctx->event);
        ctx->event = NULL;
    }

    atcmd_tcps_init_context(ctx);
    return 0;
}

int atcmd_tcps_init_config(int cid, atcmd_tcps_config * conf, atcmd_tcps_sess_info * sess_info)
{
    ATCMD_TCPS_INFO("Start\n");

    if (!conf || !sess_info)
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        return -1;
    }

    conf->cid = cid;
    conf->task_priority = ATCMD_TCPS_TASK_PRIORITY;
    conf->task_size = (ATCMD_TCPS_TASK_SIZE / 4);
    snprintf((char *)conf->task_name, (ATCMD_TCPS_MAX_TASK_NAME - 1), "%s_%d",
             ATCMD_TCPS_TASK_NAME, cid);
    snprintf(conf->sock_name, (ATCMD_TCPS_MAX_SOCK_NAME - 1), "%s_%d",
             ATCMD_TCPS_SOCK_NAME, cid);
    conf->sess_info = sess_info;
    conf->sess_info->max_allow_client = ATCMD_TCPS_MAX_SESS;
    conf->rx_buflen = ATCMD_TCPS_RECV_BUF_SIZE;
    return 0;
}

int atcmd_tcps_deinit_config(atcmd_tcps_config * conf)
{
    ATCMD_TCPS_INFO("Start\n");
    memset(conf, 0x00, sizeof(atcmd_tcps_config));
    return 0;
}

int atcmd_tcps_set_at_ctrl(atcmd_tcps_context * ctx, void * const p_at_ctrl)
{
    if (!ctx || ctx->p_at_ctrl)
    {
        return -1;
    }

    ctx->p_at_ctrl = p_at_ctrl;
    return 0;
}

int atcmd_tcps_set_local_addr(atcmd_tcps_config * p_conf, int ip_type, char * p_ip, int port)
{
    ATCMD_TCPS_INFO("Start\n");

    if (p_ip)
    {
        /* Not implemented yet. */
        ATCMD_TCPS_ERR("Not allowed to set local IP address\n");
        return -1;
    }

    /* Check range */
    if (port < ATCMD_TCPS_MIN_PORT || port > ATCMD_TCPS_MAX_PORT)
    {
        ATCMD_TCPS_ERR("Invalid port(%d)\n", port);
        return -1;
    }

    if (ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        p_conf->ip_type = ip_type;
        p_conf->local_addr.sin_family = AF_INET;
        p_conf->local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        p_conf->local_addr.sin_port = htons(port);
#endif // __SUPPORT_IPV4__
    }
    else if (ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        p_conf->ip_type = ip_type;
        p_conf->local_addr6.sin6_family = AF_INET6;
        p_conf->local_addr6.sin6_addr = in6addr_any;
        p_conf->local_addr6.sin6_port = htons(port);
#endif // __SUPPORT_IPV6__
    }
    else
    {
        ATCMD_TCPS_ERR("Invalid IP Type(%d)\n", ip_type);
        return -1;
    }

    p_conf->sess_info->local_port = port;
    p_conf->sess_info->ip_type = ip_type;

    return 0;
}

int atcmd_tcps_set_max_allowed_client(atcmd_tcps_config * conf, int max_allowed_client)
{
    ATCMD_TCPS_INFO("Start\n");

    if (max_allowed_client < 1 || max_allowed_client > ATCMD_TCPS_MAX_SESS)
    {
        ATCMD_TCPS_ERR("Invalid max_allowed_client(%d)\n", max_allowed_client);
        return -1;
    }

    conf->sess_info->max_allow_client = max_allowed_client;
    ATCMD_TCPS_INFO("Set max_allow_client(%d)\n", max_allowed_client);
    return 0;
}

int atcmd_tcps_set_config(atcmd_tcps_context * ctx, atcmd_tcps_config * conf)
{
    ATCMD_TCPS_INFO("Start\n");

    if (strlen((const char *)(conf->task_name)) == 0)
    {
        ATCMD_TCPS_ERR("Invalid task name\n");
        return -1;
    }

    if (conf->task_priority == 0)
    {
        ATCMD_TCPS_ERR("Invalid task priority\n");
        return -1;
    }

    if (conf->task_size == 0)
    {
        ATCMD_TCPS_ERR("Invalid task size\n");
        return -1;
    }

    if (conf->rx_buflen == 0)
    {
        ATCMD_TCPS_ERR("Invalid recv buffer size\n");
        return -1;
    }

    if (conf->sess_info->max_allow_client == 0)
    {
        ATCMD_TCPS_ERR("Invalid max client\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        if ((conf->local_addr.sin_family != AF_INET) || (conf->local_addr.sin_port == 0))
        {
            ATCMD_TCPS_ERR("Invali local address\n");
            return -1;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )

        if ((conf->local_addr6.sin6_family != AF_INET6) || (conf->local_addr6.sin6_port == 0))
        {
            ATCMD_TCPS_ERR("Invali local address\n");
            return -1;
        }

        #endif // __SUPPORT_IPV6__
    }

    if (strlen((const char *)(conf->sock_name)) == 0)
    {
        ATCMD_TCPS_ERR("Invalid socket name\n");
        return -1;
    }

    ctx->event = xEventGroupCreate();

    if (ctx->event == NULL)
    {
        ATCMD_TCPS_INFO("Failed to create event\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ATCMD_TCPS_INFO("*%-20s: %s(%d)\n" // task name
                        "*%-20s: %ld\n" // task priority
                        "*%-20s: %d\n" // task size
                        "*%-20s: %d\n" // rx buflen
                        "*%-20s: %d\n" // max allow clients
                        "*%-20s: %ld.%ld.%ld.%ld:%d\n", // local ip address
                        "Task Name", (char *)conf->task_name, strlen((const char *)conf->task_name),
                        "Task Priority", conf->task_priority,
                        "Task Size", conf->task_size,
                        "RX buffer size", conf->rx_buflen,
                        "Max allow connection", conf->sess_info->max_allow_client,
                        "IP Address",
                        (ntohl(conf->local_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(conf->local_addr.sin_port)));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ATCMD_TCPS_INFO("*%-20s: %s(%d)\n" // task name
                        "*%-20s: %ld\n" // task priority
                        "*%-20s: %d\n" // task size
                        "*%-20s: %d\n" // rx buflen
                        "*%-20s: %d\n" // max allow clients
                        "*%-20s:  %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\n", // local ip address
                        "Task Name", (char *)conf->task_name, strlen((const char *)conf->task_name),
                        "Task Priority", conf->task_priority,
                        "Task Size", conf->task_size,
                        "RX buffer size", conf->rx_buflen,
                        "Max allow connection", conf->sess_info->max_allow_client,
                        "IP Address",
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(conf->local_addr6.sin6_port)));
        #endif // __SUPPORT_IPV6__
    }

    ctx->conf = conf;
    return 0;
}

int atcmd_tcps_start(atcmd_tcps_context * ctx)
{
    int ret = 0;
    ATCMD_TCPS_INFO("Start\n");

    if (!ctx || !ctx->conf)
    {
        ATCMD_TCPS_ERR("Invalid parameters\n");
        return -1;
    }

    if (ctx->state != ATCMD_TCPS_STATE_TERMINATED)
    {
        ATCMD_TCPS_ERR("TCP server is not terminated(%d)\n", ctx->state);
        return -1;
    }

    ctx->state = ATCMD_TCPS_STATE_READY;

    ret = xTaskCreate(atcmd_tcps_task_entry,
                      (const char *)(ctx->conf->task_name),
                      ctx->conf->task_size,
                      (void *)ctx,
                      ctx->conf->task_priority,
                      &ctx->task_handler);

    if (ret != pdPASS)
    {
        ATCMD_TCPS_ERR("Failed to create tcp server task(%d)\n", ret);
        ctx->state = ATCMD_TCPS_STATE_TERMINATED;
        return -1;
    }

    return 0;
}

int atcmd_tcps_stop(atcmd_tcps_context * ctx)
{
    int ret = 0;
    const int wait_time = portCONVERT_MS_2_TICKS(100);
    const int max_cnt = 10;
    int cnt = 0;
    unsigned int events = 0x00;
    atcmd_tcps_cli_context * cli_ctx = NULL;
    ATCMD_TCPS_INFO("Start\n");

    if (!ctx)
    {
        ATCMD_TCPS_ERR("Invalid parameters\n");
        return -1;
    }

    //Stop tcp server task
    if (ctx->state == ATCMD_TCPS_STATE_ACCEPT)
    {
        ATCMD_TCPS_INFO("Change tcp server state from %d to %d\n",
                        ctx->state, ATCMD_TCPS_STATE_REQ_TERMINATE);
        ctx->state = ATCMD_TCPS_STATE_REQ_TERMINATE;

        for (cnt = 0 ; cnt < max_cnt ; cnt++)
        {
            if (ctx->event)
            {
                events = xEventGroupWaitBits(ctx->event, ATCMD_TCPS_EVT_CLOSED,
                                             pdTRUE, pdFALSE, wait_time);

                if (events & ATCMD_TCPS_EVT_CLOSED)
                {
                    ATCMD_TCPS_INFO("Closed tcp server task\n");
                    break;
                }
            }
            else
            {
                if (ctx->state == ATCMD_TCPS_STATE_TERMINATED)
                {
                    ATCMD_TCPS_INFO("Closed tcp server task\n");
                    break;
                }

                vTaskDelay(wait_time);
            }

            ATCMD_TCPS_INFO("Waiting for closing task of tcp server(%d,%d,%d)\n",
                            cnt, max_cnt, wait_time);
        }
    }

    if (ctx->state != ATCMD_TCPS_STATE_TERMINATED)
    {
        ATCMD_TCPS_ERR("Failed to stop tcp server task(%s,%d)\n", ctx->conf->task_name, ctx->state);
        return -1;
    }

    //Stop tcp client task
    cli_ctx = ctx->cli_ctx;

    while (cli_ctx)
    {
        ret = atcmd_tcps_cli_release(ctx, cli_ctx);

        if (ret)
        {
            ATCMD_TCPS_ERR("Failed to release TCP client(%d)\n", ret);
            break;
        }

        cli_ctx = ctx->cli_ctx;
    }

    return 0;
}

int atcmd_tcps_stop_cli(atcmd_tcps_context * ctx, const char * ip, const int port)
{
    int ret = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    atcmd_tcps_cli_context * cli_ctx = NULL;

    if (ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        struct sockaddr_in peer_addr = {0x00,};
        ATCMD_TCPS_INFO("Close peer session(%s:%d)\n", ip, port);
        //Convert ip address & port
        peer_addr.sin_addr.s_addr = inet_addr(ip);
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        //Find client context
        cli_ctx = atcmd_tcps_cli_find_by_ip(ctx, &peer_addr);

        if (cli_ctx)
        {
            ret = atcmd_tcps_cli_release(ctx, cli_ctx);

            if (ret)
            {
                ATCMD_TCPS_ERR("Failed to release TCP client(%d)\n", ret);
                return ret;
            }
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        struct sockaddr_in6 peer_addr = {0x00,};
        ATCMD_TCPS_INFO("Close peer session(%s:%d)\n", ip, port);
        //Convert ip address & port
        inet_pton(AF_INET6, ip, &(peer_addr.sin6_addr));
        peer_addr.sin6_family = AF_INET6;
        peer_addr.sin6_port = htons(port);
        //Find client context
        cli_ctx = atcmd_tcps_cli_find_by_ip6(ctx, &peer_addr);

        if (cli_ctx)
        {
            ret = atcmd_tcps_cli_release(ctx, cli_ctx);

            if (ret)
            {
                ATCMD_TCPS_ERR("Failed to release TCP client(%d)\n", ret);
                return ret;
            }
        }

        #endif // __SUPPORT_IPV6__
    }

    return ret;
}

int atcmd_tcps_wait_for_ready(atcmd_tcps_context * ctx)
{
    const int max_wait_cnt = 10;
    const int wait_time = portCONVERT_MS_2_TICKS(100);  // 100 msec
    int wait_cnt = 0;
    unsigned int events = 0x00;

    if (!ctx)
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        return -1;
    }

    for (wait_cnt = 0 ; wait_cnt < max_wait_cnt ; wait_cnt++)
    {
        if (ctx->event)
        {
            events = xEventGroupWaitBits(ctx->event, ATCMD_TCPS_EVT_ANY,
                                         pdTRUE, pdFALSE, wait_time);

            if (events & ATCMD_TCPS_EVT_ACCEPT)
            {
                ATCMD_TCPS_INFO("Got accept event\n");
                break;
            }
            else if (events & ATCMD_TCPS_EVT_CLOSED)
            {
                ATCMD_TCPS_INFO("Got close event\n");
                return -1;
            }
        }
        else
        {
            if (ctx->state == ATCMD_TCPS_STATE_ACCEPT)
            {
                break;
            }
            else if (ctx->state == ATCMD_TCPS_STATE_TERMINATED)
            {
                return -1;
            }

            vTaskDelay(wait_time);
        }
    }

    return 0;
}

int atcmd_tcps_tx(atcmd_tcps_context * ctx, char * data, unsigned int * data_len, char * ip, unsigned int port)
{
    int ret = 0;
    atcmd_tcps_cli_context * cli_ctx = NULL;
    struct addrinfo hints, * addr_list = NULL;
    char str_port[16] = {0x00, };

    int total_sent = 0;
    const int to_send = *data_len;
    int sent_cnt = 0;

    ATCMD_TCPS_INFO("Start\n");

    if ((ctx->state != ATCMD_TCPS_STATE_ACCEPT) || (ctx->cli_cnt == 0))
    {
        ATCMD_TCPS_ERR("Invalid parameters(data_len:%d, state:%d, cli_cnt:%d)\n",
                       *data_len, ctx->state, ctx->cli_cnt);
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    memset(&hints, 0x00, sizeof(struct addrinfo));

    if (ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        struct sockaddr_in cli_addr;
        memset(&cli_addr, 0x00, sizeof(struct sockaddr_in));

        if (!is_in_valid_ip_class(ip))
        {
            hints.ai_family = AF_INET;	//IPv4 only
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            snprintf(str_port, sizeof(str_port), "%d", port);
            ret = getaddrinfo(ip, str_port, &hints, &addr_list);

            if ((ret != 0) || !addr_list)
            {
                ATCMD_TCPS_DBG("Failed to get address info(%d)\n", ret);
                return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;
            }

            //pick 1st address
            memcpy((struct sockaddr *)&cli_addr, addr_list->ai_addr, sizeof(struct sockaddr));
            freeaddrinfo(addr_list);
        }
        else
        {
            cli_addr.sin_addr.s_addr = inet_addr(ip);
        }

        cli_addr.sin_family = AF_INET;
        cli_addr.sin_port = htons(port);
        ATCMD_TCPS_INFO("Client ip address: %ld.%ld.%ld.%ld:%d\n",
                        (ntohl(cli_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(cli_addr.sin_port)));
        //find client context
        cli_ctx = atcmd_tcps_cli_find_by_ip(ctx, &cli_addr);
        #endif // __SUPPORT_IPV4__
    }
    else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        struct sockaddr_in6 cli_addr6;
        struct sockaddr_in6 valid_ipv6;
        memset(&cli_addr6, 0x00, sizeof(struct sockaddr_in6));
        memset(&valid_ipv6, 0x00, sizeof(struct sockaddr_in6));

        if (!isIPv6Address(ip, &valid_ipv6))
        {
            hints.ai_family = AF_INET6;	//IPv4 only
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            snprintf(str_port, sizeof(str_port), "%d", port);
            ret = getaddrinfo(ip, str_port, &hints, &addr_list);

            if ((ret != 0) || !addr_list)
            {
                ATCMD_TCPS_DBG("Failed to get address info(%d)\n", ret);
                return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;
            }

            //pick 1st address
            memcpy((struct sockaddr_in6 *)&cli_addr6, addr_list->ai_addr, sizeof(struct sockaddr_in6));
            freeaddrinfo(addr_list);
        }
        else
        {
            //cli_addr6.sin6_addr.s6_addr = inet_addr(ip);
            inet_pton(AF_INET6, ip, &(cli_addr6.sin6_addr));
        }

        cli_addr6.sin6_family = AF_INET6;
        cli_addr6.sin6_port = htons(port);
        ATCMD_TCPS_INFO("Client ip address: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\n",
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(cli_addr6.sin6_port)));
        //find client context
        cli_ctx = atcmd_tcps_cli_find_by_ip6(ctx, &cli_addr6);
        #endif // __SUPPORT_IPV6__
    }

    if (!cli_ctx)
    {
        ATCMD_TCPS_ERR("Failed to find client ip address in list\n");
        return FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    if (cli_ctx->state != ATCMD_TCPS_CLI_STATE_CONNECTED)
    {
        ATCMD_TCPS_ERR("Client is not connected(%d)\n", cli_ctx->state);
        return FSP_ERR_AT_CMD_ERR_NOT_CONNECTED;
    }

    ATCMD_TCPS_INFO("Send data(%d)\n", *data_len);

    while (total_sent < to_send)
    {
        ret = send(cli_ctx->socket, data + total_sent, to_send - total_sent, 0);
        if (ret < 0)
        {
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                break;
            }
        }
        else
        {
            total_sent += ret;
        }

        sent_cnt++;

        if (sent_cnt >= ATCMD_TCPS_SEND_RETRY_CNT)
        {
            break;
        }

        ATCMD_TCPS_INFO("#%d. TCP Tx:%d/%d/%d/%d\n", sent_cnt, *data_len, total_sent, ret, errno);
    }

    if (total_sent != *data_len)
    {
#if defined(ENABLE_ATCMD_TCPS_DBG_ERR)
        if (ctx->conf->ip_type == IPADDR_TYPE_V4)
        {
#if defined ( __SUPPORT_IPV4__ )
            ATCMD_TCPS_ERR("Failed to send tcp data to tcp client(%ld.%ld.%ld.%ld:%d,%d/%d/%d)\n",
                           (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                           (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                           (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                           (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                           (ntohs(cli_ctx->addr.sin_port)),
                           *data_len, total_sent, errno);
#endif // __SUPPORT_IPV4__
        }
        else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
        {
#if defined ( __SUPPORT_IPV6__ )
            ATCMD_TCPS_ERR("Failed to send tcp data to tcp client(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d,%d/%d/%d)\n",
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                           (ntohs(cli_ctx->addr6.sin6_port)),
                           *data_len, total_sent, errno);
#endif // __SUPPORT_IPV6__
        }
#endif // ENABLE_ATCMD_TCPS_DBG_ERR

        *data_len = total_sent;

        return FSP_ERR_AT_CMD_ERR_DATA_TX;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static int atcmd_tcps_get_backlog(const atcmd_tcps_config * conf, int connected)
{
    int max_backlog = 0;
    int backlog = 0;
    max_backlog = conf->sess_info->max_allow_client;

    if ((max_backlog > ATCMD_TCPS_MIN_BACKLOG) && ((max_backlog / 2) > connected))
    {
        backlog = max_backlog / 2;
    }

    /*
    ATCMD_TCPS_INFO("Max:%d, Min:%d, Conn:%d, backlog:%d\n",
                    max_backlog, ATCMD_TCPS_MIN_BACKLOG, connected, backlog);
    */
    return backlog;
}

static int atcmd_tcps_ready_to_accept_socket(atcmd_tcps_context * ctx)
{
    int ret = 0;
    const atcmd_tcps_config * conf = ctx->conf;
    //socket option
    int sockopt_reuse = 1;
    struct timeval sockopt_timeout = {0x00,};
    //socket information
    #if CFG_PMGR
    const char * svr_sock_name = (const char *)conf->sock_name;
    #endif /* CFG_PMGR */

    ATCMD_TCPS_INFO("Start\n");

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_TCPS_RECV_TIMEOUT * 1000;

    if (ctx->socket >= 0)
    {
        ATCMD_TCPS_ERR("Already assigned socket fd(%d)\n", ctx->socket);
        return -1;
    }

    //create socket
    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        #ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
        #if CFG_PMGR
        ret = socket_dpm((char *)svr_sock_name, PF_INET, SOCK_STREAM, 0);
        #else
        ret = socket(PF_INET, SOCK_STREAM, 0);
        #endif /* CFG_PMGR */
        #else
        ret = socket(PF_INET, SOCK_STREAM, 0);
        #endif

        if (ret < 0)
        {
            ATCMD_TCPS_ERR("Failed to create socket of tcp server(%d)\n", ret);
            goto err;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        #ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
        #if CFG_PMGR
        ret = socket_dpm((char *)svr_sock_name, PF_INET6, SOCK_STREAM, 0);
        #else
        ret = socket(PF_INET6, SOCK_STREAM, 0);
        #endif /* CFG_PMGR */
        #else
        ret = socket(PF_INET6, SOCK_STREAM, 0);
        #endif

        if (ret < 0)
        {
            ATCMD_TCPS_ERR("Failed to create socket of tcp server(%d)\n", ret);
            goto err;
        }

        #endif // __SUPPORT_IPV6__
    }

    ctx->socket = ret;
    ATCMD_TCPS_INFO("Created socket descriptor(%d)\n", ctx->socket);
    //set socket option
    ret = setsockopt(ctx->socket, SOL_SOCKET, SO_REUSEADDR, &sockopt_reuse, sizeof(sockopt_reuse));

    if (ret != 0)
    {
        ATCMD_TCPS_DBG("Failed to set socket option - SO_REUSEADDR(%d)\n", ret);
    }

    ret = setsockopt(ctx->socket, SOL_SOCKET, SO_RCVTIMEO, &sockopt_timeout, sizeof(sockopt_timeout));

    if (ret != 0)
    {
        ATCMD_TCPS_DBG("Failed to set socket option - SO_RCVTIMEOUT(%d)\n", ret);
    }

    ATCMD_TCPS_INFO("bind socket descriptor(%d)\n", ctx->socket);

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        //bind socket
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr, sizeof(struct sockaddr_in));

        if (ret != 0)
        {
            ATCMD_TCPS_DBG("Failed to bind socket of tcp server(%d)\n", ret);
            goto err;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        //bind socket
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr6, sizeof(struct sockaddr_in6));

        if (ret != 0)
        {
            ATCMD_TCPS_DBG("Failed to bind socket of tcp server(%d)\n", ret);
            goto err;
        }

        #endif // __SUPPORT_IPV6__
    }

    ATCMD_TCPS_INFO("listen socket descriptor(%d)\n", ctx->socket);
    //listen socket
    ret = listen(ctx->socket, atcmd_tcps_get_backlog(conf, 0));

    if (ret != 0)
    {
        ATCMD_TCPS_DBG("Failed to listen socket of tcp server(%d)\n", ret);
        goto err;
    }

    return 0;
err:
    atcmd_tcps_close_socket(&ctx->socket);
    return ret;
}

static void atcmd_tcps_close_socket(int * sock)
{
    if (sock)
    {
        if (*sock != ATCMD_TCPS_INIT_SOCKET_FD)
        {
            ATCMD_TCPS_INFO("To close socket(%d)\n", *sock);
            close(*sock);
        }

        *sock = ATCMD_TCPS_INIT_SOCKET_FD;
    }

    return;
}

static void atcmd_tcps_task_entry(void * param)
{
    int ret = 0;
    atcmd_tcps_context * svr_ctx = (atcmd_tcps_context *)param;
    const atcmd_tcps_config * svr_conf = svr_ctx->conf;
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    unsigned int local_port = 0;
#endif /* CFG_PMGR */
#endif
    //tcp client(sub)
    int cli_sock = ATCMD_TCPS_INIT_SOCKET_FD;
    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in cli_addr;
    int cli_addrlen = sizeof(struct sockaddr_in);
    #endif // __SUPPORT_IPV4__
    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 cli_addr6;
    int cli_addrlen6 = sizeof(struct sockaddr_in6);
    #endif // __SUPPORT_IPV6__
    atcmd_tcps_cli_context * cli_ctx = NULL;
    const int max_allow_client = svr_conf->sess_info->max_allow_client;
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    const char * svr_dpm_name = (const char *)svr_conf->task_name;
#endif /* CFG_PMGR */
#endif
    ATCMD_TCPS_INFO("Start\n");

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                           g_wifi_cfg.p_watchdog_service->p_cfg,
                                                           &task_wdog_id);
#endif
#endif

    if (!svr_ctx || !svr_conf)
    {
        ATCMD_TCPS_DBG("[%s] Invalid param\n", __func__);
        goto end;
    }

    svr_ctx->mutex = xSemaphoreCreateMutex();

    if (!svr_ctx->mutex)
    {
        ATCMD_TCPS_DBG("Failed to create mutex to manage tcp client task\n");
        goto end;
    }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    if (svr_conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        local_port = ntohs(svr_conf->local_addr.sin_port);
        #endif // __SUPPORT_IPV4__
    }
    else if (svr_conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        local_port = ntohs(svr_conf->local_addr6.sin6_port);
        #endif // __SUPPORT_IPV6__
    }

    RM_PMGR_W_dpm_job_name_set((char *)svr_dpm_name, local_port);

    RM_WIFI_dpm_tcp_port_filter_set(local_port);

    ATCMD_TCPS_INFO("Reg - DPM Name:%s(%d), Local port(%d)\n",
                    svr_dpm_name, strlen(svr_dpm_name), local_port);
#endif /* CFG_PMGR */
#endif

    ret = atcmd_tcps_ready_to_accept_socket(svr_ctx);

    if (ret)
    {
        ATCMD_TCPS_DBG("Failed to create socket of tcp server(%d)\n", ret);
        goto end;
    }

    svr_ctx->state = ATCMD_TCPS_STATE_ACCEPT;

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_done((char *)svr_dpm_name);

    RM_PMGR_W_dpm_rcv_ready_set((char *)svr_dpm_name);
#endif /* CFG_PMGR */
#endif

    if (svr_ctx->event)
    {
        xEventGroupSetBits(svr_ctx->event, ATCMD_TCPS_EVT_ACCEPT);
    }

    while (svr_ctx->state == ATCMD_TCPS_STATE_ACCEPT)
    {
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         g_wifi_cfg.p_watchdog_service->p_cfg,
                                                         task_wdog_id);
#endif
#endif

        cli_sock = ATCMD_TCPS_INIT_SOCKET_FD;

        if (svr_conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            memset(&cli_addr, 0x00, sizeof(struct sockaddr_in));
            cli_addrlen = sizeof(struct sockaddr_in);
            #endif // __SUPPORT_IPV4__
        }
        else if (svr_conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            memset(&cli_addr6, 0x00, sizeof(struct sockaddr_in6));
            cli_addrlen6 = sizeof(struct sockaddr_in6);
            #endif // __SUPPORT_IPV6__
        }

        cli_ctx = NULL;

        //set backlog again.
        listen(svr_ctx->socket, atcmd_tcps_get_backlog(svr_conf, svr_ctx->cli_cnt));

        //ATCMD_TCPS_INFO("Waiting for client connects(connected clients:%ld)\n", svr_ctx->cli_cnt);
        if (svr_conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            cli_sock = accept(svr_ctx->socket, (struct sockaddr *)&cli_addr, (socklen_t *)&cli_addrlen);
            #endif // __SUPPORT_IPV4__
        }
        else if (svr_conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            cli_sock = accept(svr_ctx->socket, (struct sockaddr *)&cli_addr6, (socklen_t *)&cli_addrlen6);
            #endif // __SUPPORT_IPV6__
        }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
        RM_PMGR_W_dpm_sleep_ready_clear((char *)svr_dpm_name);
#endif /* CFG_PMGR */
#endif
        if (cli_sock < 0)
        {
            //check cli state
            cli_ctx = svr_ctx->cli_ctx;

            while (cli_ctx)
            {
                if ((cli_ctx->state != ATCMD_TCPS_CLI_STATE_READY)
                        && (cli_ctx->state != ATCMD_TCPS_CLI_STATE_CONNECTED))
                {
                    if (svr_conf->ip_type == IPADDR_TYPE_V4)
                    {
                        #if defined ( __SUPPORT_IPV4__ )
                        ATCMD_TCPS_INFO("To remove tcp client(%ld.%ld.%ld.%ld:%d), "
                                        "state(%d), connected client(%d)\n",
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                        ntohs(cli_ctx->addr.sin_port), cli_ctx->state,
                                        svr_ctx->cli_cnt);
                        #endif // __SUPPORT_IPV4__
                    }
                    else if (svr_conf->ip_type == IPADDR_TYPE_V6)
                    {
                        #if defined ( __SUPPORT_IPV6__ )
                        ATCMD_TCPS_INFO("To remove tcp client(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d), "
                                        "state(%d), connected client(%d)\n",
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                        ntohs(cli_ctx->addr6.sin6_port), cli_ctx->state,
                                        svr_ctx->cli_cnt);
                        #endif // __SUPPORT_IPV6__
                    }

                    ret = atcmd_tcps_cli_release(svr_ctx, cli_ctx);

                    if (ret)
                    {
                        ATCMD_TCPS_ERR("Failed to release TCP client(%d)\n", ret);
                        break;
                    }

                    cli_ctx = svr_ctx->cli_ctx;
                }
                else
                {
                    cli_ctx = cli_ctx->next;
                }
            }

            #if 0 //def ENABLE_ATCMD_TCPS_DBG_INFO

            if (svr_ctx->cli_ctx)
            {
                for (cli_ctx = svr_ctx->cli_ctx ; cli_ctx != NULL ; cli_ctx = cli_ctx->next)
                {
                    ATCMD_TCPS_INFO("connecte tcp client(%d.%d.%d.%d:%d), state(%d)\n",
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                    ntohs(cli_ctx->addr.sin_port), cli_ctx->state);
                }
            }
            else
            {
                ATCMD_TCPS_INFO("svr_ctx->cli_ctx is null\n");
            }

            ATCMD_TCPS_INFO("svr_ctx->cli_cnt: %ld\n", svr_ctx->cli_cnt);
            #endif // ENABLE_ATCMD_TCPS_DBG_INFO
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
            RM_PMGR_W_dpm_sleep_ready_set((char *)svr_dpm_name);
#endif /* CFG_PMGR */
#endif
            continue;
        }

        if (svr_conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            ATCMD_TCPS_INFO("Connected %d.client(%ld.%ld.%ld.%ld:%d), current connected(%d + 1)\n",
                            cli_sock,
                            (ntohl(cli_addr.sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(cli_addr.sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(cli_addr.sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(cli_addr.sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(cli_addr.sin_port)),
                            svr_ctx->cli_cnt);
            #endif // __SUPPORT_IPV4__
        }
        else if (svr_conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            ATCMD_TCPS_INFO("Connected %d.client(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d), current connected(%d + 1)\n",
                            cli_sock,
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(cli_addr6.sin6_port)),
                            svr_ctx->cli_cnt);
            #endif // __SUPPORT_IPV6__
        }

        if (max_allow_client <= svr_ctx->cli_cnt)
        {
            ATCMD_TCPS_DBG("Client accept full(max client count:%d)\n", max_allow_client);
            atcmd_tcps_close_socket(&cli_sock);
            continue;
        }

        //Check duplication
        for (cli_ctx = svr_ctx->cli_ctx ; cli_ctx != NULL ; cli_ctx = cli_ctx->next)
        {
            if (svr_conf->ip_type == IPADDR_TYPE_V4)
            {
                #if defined ( __SUPPORT_IPV4__ )

                if (atcmd_tcps_compare_ip_addr(&cli_ctx->addr, &cli_addr) == 0)
                {
                    ATCMD_TCPS_DBG("Duplicated client(%ld.%ld.%ld.%ld:%d)\n",
                           (ntohl(cli_addr.sin_addr.s_addr) >> 24) & 0x0ff,
                           (ntohl(cli_addr.sin_addr.s_addr) >> 16) & 0x0ff,
                           (ntohl(cli_addr.sin_addr.s_addr) >>  8) & 0x0ff,
                           (ntohl(cli_addr.sin_addr.s_addr)      ) & 0x0ff,
                           ntohs(cli_addr.sin_port));
                    atcmd_tcps_close_socket(&cli_sock);
                    break;
                }

                #endif // __SUPPORT_IPV4__
            }
            else if (svr_conf->ip_type == IPADDR_TYPE_V6)
            {
                #if defined ( __SUPPORT_IPV6__ )

                if (atcmd_tcps_compare_ip_addr6(&cli_ctx->addr6, &cli_addr6) == 0)
                {
                    ATCMD_TCPS_DBG("Duplicated client(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                   ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                   ntohs(cli_addr6.sin6_port));
                    atcmd_tcps_close_socket(&cli_sock);
                    break;
                }

                #endif // __SUPPORT_IPV6__
            }
        }

        if (cli_sock < 0)
        {
            continue;
        }

        if (svr_conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            cli_ctx = atcmd_tcps_cli_create_context(svr_ctx, cli_sock, (struct sockaddr_storage *)&cli_addr);
            cli_ctx->ip_type = IPADDR_TYPE_V4;

            if (!cli_ctx)
            {
                ATCMD_TCPS_DBG("Failed to create client context(%ld.%ld.%ld.%ld:%d)\n",
                               (ntohl(cli_addr.sin_addr.s_addr) >> 24) & 0x0ff,
                               (ntohl(cli_addr.sin_addr.s_addr) >> 16) & 0x0ff,
                               (ntohl(cli_addr.sin_addr.s_addr) >>  8) & 0x0ff,
                               (ntohl(cli_addr.sin_addr.s_addr)      ) & 0x0ff,
                               ntohs(cli_addr.sin_port));
                atcmd_tcps_close_socket(&cli_sock);
                continue;
            }

            #endif // __SUPPORT_IPV4__
        }
        else if (svr_conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            cli_ctx = atcmd_tcps_cli_create_context(svr_ctx, cli_sock, (struct sockaddr_storage *)&cli_addr6);
            cli_ctx->ip_type = IPADDR_TYPE_V6;

            if (!cli_ctx)
            {
                ATCMD_TCPS_DBG("Failed to create client context(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                               ((PP_HTONL(cli_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                               ntohs(cli_addr6.sin6_port));
                atcmd_tcps_close_socket(&cli_sock);
                continue;
            }

            #endif // __SUPPORT_IPV6__
        }

        ret = atcmd_tcps_cli_add(svr_ctx, cli_ctx);

        if (ret != 0)
        {
            ATCMD_TCPS_DBG("Failed to add client context(%d)\n", ret);
            atcmd_tcps_cli_delete_context(&cli_ctx);
            atcmd_tcps_close_socket(&cli_sock);
            continue;
        }

        ret = atcmd_tcps_cli_start(cli_ctx);

        if (ret != 0)
        {
            ATCMD_TCPS_DBG("Failed to start tcp client(%d)\n", ret);
            atcmd_tcps_cli_remove_by_ctx(svr_ctx, cli_ctx);
            atcmd_tcps_cli_delete_context(&cli_ctx);
            atcmd_tcps_close_socket(&cli_sock);
            continue;
        }
    }

end:
    atcmd_tcps_close_socket(&svr_ctx->socket);
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR    
    RM_WIFI_dpm_tcp_port_delete(local_port);
    RM_PMGR_W_dpm_job_name_clear((char *)svr_dpm_name);
    ATCMD_TCPS_INFO("Unreg - DPM Name:%s(%d), Local port(%d)\n",
                    svr_dpm_name, strlen(svr_dpm_name), local_port);
#endif /* CFG_PMGR */
#endif

    if (svr_ctx->mutex)
    {
        vSemaphoreDelete(svr_ctx->mutex);
        svr_ctx->mutex = NULL;
    }

    svr_ctx->state = ATCMD_TCPS_STATE_TERMINATED;

    if (svr_ctx->event)
    {
        xEventGroupSetBits(svr_ctx->event, ATCMD_TCPS_EVT_CLOSED);
    }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif
#endif

    ATCMD_TCPS_INFO("End of task(%d)\n", svr_ctx->state);
    svr_ctx->task_handler = NULL;
    vTaskDelete(NULL);
    return ;
}


static atcmd_tcps_cli_context * atcmd_tcps_cli_create_context(atcmd_tcps_context * svr_ctx, int cli_sock,
        struct sockaddr_storage * cli_addr)
{
    static int cli_idx = 0;
    atcmd_tcps_cli_context * cli_ctx = NULL;
    struct timeval sockopt_timeout;
    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_TCPS_RECV_TIMEOUT * 1000;
    ATCMD_TCPS_INFO("Start\n");

    if (setsockopt(cli_sock, SOL_SOCKET, SO_RCVTIMEO, &sockopt_timeout, sizeof(sockopt_timeout)))
    {
        ATCMD_TCPS_DBG("Failed to set socket option - SO_RCVTIMEOUT(%d)\n", ATCMD_TCPS_RECV_TIMEOUT);
        return NULL;
    }

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_TCPS_SEND_TIMEOUT * 1000;

    if (setsockopt(cli_sock, SOL_SOCKET, SO_SNDTIMEO, &sockopt_timeout, sizeof(sockopt_timeout)))
    {
        ATCMD_TCPS_DBG("Failed to set socket option - SO_SNDTIMEO(%d)\n", ATCMD_TCPS_SEND_TIMEOUT);
        return NULL;
    }

    cli_ctx = pvPortMalloc(sizeof(atcmd_tcps_cli_context));

    if (!cli_ctx)
    {
        ATCMD_TCPS_DBG("Failed to allocate memory for tcp client(%d)\n", sizeof(atcmd_tcps_cli_context));
        return NULL;
    }

    memset(cli_ctx, 0x00, sizeof(atcmd_tcps_cli_context));
    cli_ctx->buffer_len = svr_ctx->conf->rx_buflen;
    cli_ctx->buffer = pvPortMalloc(cli_ctx->buffer_len);

    if (!cli_ctx->buffer)
    {
        ATCMD_TCPS_DBG("Failed to allocate memory for tcp client's rx buffer(%d)\n", cli_ctx->buffer_len);
        goto err;
    }

    cli_ctx->event = xEventGroupCreate();

    if (cli_ctx->event == NULL)
    {
        ATCMD_TCPS_ERR("Failed to create event group to close tcp client's task\n");
        goto err;
    }

    //cid
    cli_ctx->cid = svr_ctx->conf->cid;
    //task name
    snprintf((char *)(cli_ctx->task_name), (ATCMD_TCPS_MAX_TASK_NAME - 1), "%s_%d",
             ATCMD_TCPS_CLI_TASK_NAME, cli_idx++);
    //task priority
    cli_ctx->task_priority = ATCMD_TCPS_CLI_TASK_PRIORITY;
    //task size
    cli_ctx->task_size = (ATCMD_TCPS_CLI_TASK_SIZE / 4);
    //state
    cli_ctx->state = ATCMD_TCPS_CLI_STATE_TERMINATED;
    //socket
    cli_ctx->socket = cli_sock;

    //address
    if (svr_ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        memcpy(&cli_ctx->addr, (struct sockaddr_in *)cli_addr, sizeof(struct sockaddr_in));
        #endif // __SUPPORT_IPV4__
    }
    else if (svr_ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        memcpy(&cli_ctx->addr6, (struct sockaddr_in6 *)cli_addr, sizeof(struct sockaddr_in6));
        #endif // __SUPPORT_IPV6__
    }

    cli_ctx->svr_ptr = (void *)svr_ctx;

    if (svr_ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ATCMD_TCPS_INFO("*%-20s: %s(%d)\n" // task name
                        "*%-20s: %ld\n" // task priority
                        "*%-20s: %d\n" // task size
                        "*%-20s: %d\n" // rx buflen
                        "*%-20s: %ld.%ld.%ld.%ld:%d\n", // local ip address
                        "Task Name", cli_ctx->task_name, strlen((const char *)cli_ctx->task_name),
                        "Task Priority", cli_ctx->task_priority,
                        "Task Size", cli_ctx->task_size,
                        "RX buffer size", cli_ctx->buffer_len,
                        "IP Address",
                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(cli_ctx->addr.sin_port)));
        #endif // __SUPPORT_IPV4__
    }
    else if (svr_ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ATCMD_TCPS_INFO("*%-20s: %s(%d)\n" // task name
                        "*%-20s: %ld\n" // task priority
                        "*%-20s: %d\n" // task size
                        "*%-20s: %d\n" // rx buflen
                        "*%-20s: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\n", // local ip address
                        "Task Name", cli_ctx->task_name, strlen((const char *)cli_ctx->task_name),
                        "Task Priority", cli_ctx->task_priority,
                        "Task Size", cli_ctx->task_size,
                        "RX buffer size", cli_ctx->buffer_len,
                        "IP Address",
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(cli_ctx->addr6.sin6_port)));
        #endif // __SUPPORT_IPV4__
    }

    return cli_ctx;
err:

    if (cli_ctx)
    {
        atcmd_tcps_cli_delete_context(&cli_ctx);
    }

    return NULL;
}

static int atcmd_tcps_cli_delete_context(atcmd_tcps_cli_context ** ctx)
{
    ATCMD_TCPS_INFO("Start\n");
    atcmd_tcps_cli_context * ptr = (atcmd_tcps_cli_context *)*ctx;

    if (ptr)
    {
        if (ptr->buffer)
        {
            vPortFree(ptr->buffer);
        }

        if (ptr->event)
        {
            vEventGroupDelete(ptr->event);
        }

        vPortFree(ptr);
    }

    *ctx = NULL;
    return 0;
}

static int atcmd_tcps_cli_start(atcmd_tcps_cli_context * ctx)
{
    int ret = 0;
    ATCMD_TCPS_INFO("Start\n");

    if (!ctx || (ctx->state != ATCMD_TCPS_CLI_STATE_TERMINATED))
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        return -1;
    }

    ctx->state = ATCMD_TCPS_CLI_STATE_READY;

    ret = xTaskCreate(atcmd_tcps_cli_task_entry,
                      (const char *)(ctx->task_name),
                      ctx->task_size,
                      (void *)ctx,
                      ctx->task_priority,
                      &ctx->task_handler);

    if (ret != pdPASS)
    {
        ATCMD_TCPS_ERR("Failed to create tcp client taks(%d)\n", ret);
        return -1;
    }

    return 0;
}

static int atcmd_tcps_cli_stop(atcmd_tcps_cli_context * ctx)
{
    const int wait_time = portCONVERT_MS_2_TICKS(100);
    const int max_cnt = 10;
    int cnt = 0;
    unsigned int events = 0x00;
    ATCMD_TCPS_INFO("Start\n");

    if (!ctx)
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        return -1;
    }

    if ((ctx->state == ATCMD_TCPS_CLI_STATE_CONNECTED)
            || (ctx->state == ATCMD_TCPS_CLI_STATE_REQ_TERMINATE))
    {
        ATCMD_TCPS_INFO("Change tcp client state from %d to %d\n",
                        ctx->state, ATCMD_TCPS_CLI_STATE_REQ_TERMINATE);
        ctx->state = ATCMD_TCPS_CLI_STATE_REQ_TERMINATE;

        for (cnt = 0 ; cnt < max_cnt ; cnt++)
        {
            if (ctx->event)
            {
                events = xEventGroupWaitBits(ctx->event, ATCMD_TCPS_EVT_CLOSED,
                                             pdTRUE, pdFALSE, wait_time);

                if (events & ATCMD_TCPS_EVT_CLOSED)
                {
                    ATCMD_TCPS_INFO("Closed tcp client task\n");
                    break;
                }
            }
            else
            {
                if (ctx->state == ATCMD_TCPS_CLI_STATE_TERMINATED)
                {
                    ATCMD_TCPS_INFO("Closed tcp client task\n");
                    break;
                }

                vTaskDelay(wait_time);
            }
        }
    }

    if (ctx->state != ATCMD_TCPS_CLI_STATE_TERMINATED)
    {
        ATCMD_TCPS_ERR("Failed to stop tcp client task(%d)\n", ctx->state);
        return -1;
    }

    return 0;
}

static int atcmd_tcps_cli_add(atcmd_tcps_context * svr_ctx, atcmd_tcps_cli_context * cli_ctx)
{
    atcmd_tcps_cli_context * last_ptr = NULL;
    const atcmd_tcps_config * svr_conf = svr_ctx->conf;
    const int max_allow_client = svr_conf->sess_info->max_allow_client;

    if (svr_ctx->cli_cnt >= max_allow_client)
    {
        ATCMD_TCPS_ERR("Not allowed tcp client's connection(%d>=%d)\n",
                       svr_ctx->cli_cnt, max_allow_client);
        return -1;
    }

    //increase client count
    svr_ctx->cli_cnt++;

    //link cli_ctx
    if (svr_ctx->cli_ctx)
    {
        //find last pointer
        for (last_ptr = svr_ctx->cli_ctx ; last_ptr->next != NULL ; last_ptr = last_ptr->next);

        last_ptr->next = cli_ctx;
    }
    else
    {
        svr_ctx->cli_ctx = cli_ctx;
    }

    return 0;
}

#if defined (__ENABLE_UNUSED__)
static atcmd_tcps_cli_context * atcmd_tcps_cli_remove_by_ip(atcmd_tcps_context * svr_ctx, struct sockaddr_in * ip_addr)
{
    atcmd_tcps_cli_context * prv_ptr = NULL, * cli_ptr = NULL;
    ATCMD_TCPS_INFO("Start\n");

    for (cli_ptr = svr_ctx->cli_ctx ; cli_ptr != NULL ; cli_ptr = cli_ptr->next)
    {
        if (atcmd_tcps_compare_ip_addr(&cli_ptr->addr, ip_addr) == 0)
        {
            break;
        }

        prv_ptr = cli_ptr;
    }

    if (cli_ptr)
    {
        ATCMD_TCPS_INFO("Found client information(%ld.%ld.%ld.%ld:%d)\n",
                        (ntohl(ip_addr->sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(ip_addr->sin_port)));

        if (cli_ptr == svr_ctx->cli_ctx)
        {
            svr_ctx->cli_ctx = svr_ctx->cli_ctx->next;
        }
        else
        {
            prv_ptr->next = cli_ptr->next;
        }

        cli_ptr->next = NULL;
        //decrease client count
        svr_ctx->cli_cnt--;
    }
    else
    {
        ATCMD_TCPS_INFO("Not found client information(%ld.%ld.%ld.%ld:%d)\n",
                        (ntohl(ip_addr->sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(ip_addr->sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(ip_addr->sin_port)));
    }

    return cli_ptr;
}
#endif // __ENABLE_UNUSED__

static atcmd_tcps_cli_context * atcmd_tcps_cli_remove_by_ctx(atcmd_tcps_context * svr_ctx,
        atcmd_tcps_cli_context * cli_ctx)
{
    atcmd_tcps_cli_context * prv_ptr = NULL, * cur_ptr = NULL;
    ATCMD_TCPS_INFO("Start\n");

    for (cur_ptr = svr_ctx->cli_ctx ; cur_ptr != NULL ; cur_ptr = cur_ptr->next)
    {
        if (cur_ptr == cli_ctx)
        {
            break;
        }

        prv_ptr = cur_ptr;
    }

    if (cur_ptr)
    {
        ATCMD_TCPS_INFO("Found client information(%p)\n", cli_ctx);

        if (cur_ptr == svr_ctx->cli_ctx)
        {
            svr_ctx->cli_ctx = svr_ctx->cli_ctx->next;
        }
        else
        {
            prv_ptr->next = cur_ptr->next;
        }

        cur_ptr->next = NULL;
        //decrease client count
        svr_ctx->cli_cnt--;
    }
    else
    {
        ATCMD_TCPS_INFO("Not found client information\n");
    }

    return cur_ptr;
}

#if defined ( __SUPPORT_IPV4__ )
static atcmd_tcps_cli_context * atcmd_tcps_cli_find_by_ip(atcmd_tcps_context * svr_ctx, struct sockaddr_in * ip_addr)
{
    atcmd_tcps_cli_context * cli_ptr = NULL;

    for (cli_ptr = svr_ctx->cli_ctx ; cli_ptr != NULL ; cli_ptr = cli_ptr->next)
    {
        if (atcmd_tcps_compare_ip_addr(&cli_ptr->addr, ip_addr) == 0)
        {
            ATCMD_TCPS_INFO("Found client information(%ld.%ld.%ld.%ld:%d)\n",
                            (ntohl(ip_addr->sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(ip_addr->sin_port)));
            return cli_ptr;
            #ifdef ENABLE_ATCMD_TCPS_DBG_INFO
        }
        else
        {
            ATCMD_TCPS_INFO("connected client(%ld.%ld.%ld.%ld:%d) vs %ld.%ld.%ld.%ld:%d\n",
                            (ntohl(cli_ptr->addr.sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(cli_ptr->addr.sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(cli_ptr->addr.sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(cli_ptr->addr.sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(cli_ptr->addr.sin_port)),
                            (ntohl(ip_addr->sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(ip_addr->sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(ip_addr->sin_port)));
            #endif // ENABLE_ATCMD_TCPS_DBG_INFO
        }
    }

    ATCMD_TCPS_INFO("Not found client information(%ld.%ld.%ld.%ld:%d)\n",
                    (ntohl(ip_addr->sin_addr.s_addr) >> 24) & 0xFF,
                    (ntohl(ip_addr->sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(ip_addr->sin_addr.s_addr) >>  8) & 0xFF,
                    (ntohl(ip_addr->sin_addr.s_addr)      ) & 0xFF,
                    (ntohs(ip_addr->sin_port)));
    return NULL;
}
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
static atcmd_tcps_cli_context * atcmd_tcps_cli_find_by_ip6(atcmd_tcps_context * svr_ctx, struct sockaddr_in6 * ip_addr)
{
    atcmd_tcps_cli_context * cli_ptr = NULL;

    for (cli_ptr = svr_ctx->cli_ctx ; cli_ptr != NULL ; cli_ptr = cli_ptr->next)
    {
        if (atcmd_tcps_compare_ip_addr6(&cli_ptr->addr6, ip_addr) == 0)
        {
            ATCMD_TCPS_INFO("Found client information(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(ip_addr->sin6_port)));
            return cli_ptr;
            #ifdef ENABLE_ATCMD_TCPS_DBG_INFO
        }
        else
        {
            ATCMD_TCPS_INFO("connected client(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d) vs %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\n",
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(cli_ptr->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(cli_ptr->addr6.sin6_port)),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(ip_addr->sin6_port)));
            #endif // ENABLE_ATCMD_TCPS_DBG_INFO
        }
    }

    ATCMD_TCPS_INFO("Not found client information(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(ip_addr->sin6_addr.un.u32_addr[3])) & 0xffff),
                    (ntohs(ip_addr->sin6_port)));
    return NULL;
}
#endif // __SUPPORT_IPV6__

static int atcmd_tcps_cli_release(atcmd_tcps_context * svr_ctx, atcmd_tcps_cli_context * cli_ctx)
{
    int ret = 0;
    TickType_t wait_time = portMAX_DELAY;
    atcmd_tcps_cli_context * tmp_ctx = NULL;

    if (svr_ctx == NULL || cli_ctx == NULL)
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        return -1;
    }

    if (svr_ctx->mutex)
    {
        xSemaphoreTakeRecursive(svr_ctx->mutex, wait_time);
        ATCMD_TCPS_INFO("Takes semaphore to stop TCP client\n");
    }

    //Check pointer
    for (tmp_ctx = svr_ctx->cli_ctx ; tmp_ctx != NULL ; tmp_ctx = tmp_ctx->next)
    {
        if (tmp_ctx == cli_ctx)
        {
            break;
        }
    }

    //Not found.
    if (tmp_ctx == NULL)
    {
        ATCMD_TCPS_ERR("Not found tcp client task\n");
        ret = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto end;
    }

    ret = atcmd_tcps_cli_stop(cli_ctx);

    if (ret)
    {
        ATCMD_TCPS_ERR("Failed to stop tcp client task(%s,%d)\n",
                       cli_ctx->task_name, cli_ctx->state);
        ret = FSP_ERR_AT_CMD_ERR_TIMEOUT;
        goto end;
    }

    cli_ctx = atcmd_tcps_cli_remove_by_ctx(svr_ctx, cli_ctx);

    if (!cli_ctx)
    {
        ATCMD_TCPS_ERR("Failed to remove tcp client context(%s,%p)\n",
                       cli_ctx->task_name, cli_ctx);
        ret = FSP_ERR_AT_CMD_ERR_UNKNOWN;
        goto end;
    }

    ret = atcmd_tcps_cli_delete_context(&cli_ctx);

    if (ret)
    {
        ATCMD_TCPS_ERR("Failed to delete tcp client context(%d)\n", ret);
        ret = FSP_ERR_AT_CMD_ERR_UNKNOWN;
        goto end;
    }

end:

    if (svr_ctx->mutex)
    {
        ATCMD_TCPS_INFO("Gives semaphore after stoping TCP client\n");
        xSemaphoreGiveRecursive(svr_ctx->mutex);
    }

    return ret;
}

#if defined ( __SUPPORT_IPV4__ )
static int atcmd_tcps_add_cli_addr(atcmd_tcps_context * ctx, struct sockaddr_in addr)
{
    int idx = 0;
    int empty_idx = -1;
    struct sockaddr_in empty_addr = {0x00,};
    const atcmd_tcps_config * conf;
    ATCMD_TCPS_INFO("Start\n");

    if (ctx == NULL)
    {
        ATCMD_TCPS_ERR("Invaild parameter\n");
        return -1;
    }

    conf = ctx->conf;
    #if defined (ENABLE_ATCMD_TCPS_DBG_INFO)
    struct sockaddr_in cli_addr;

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        //cli_addr = {0, };
        memset(&cli_addr, 0, sizeof(struct sockaddr_in));
        memcpy(&cli_addr, &conf->sess_info->cli_addr[idx], sizeof(struct sockaddr_in));
        ATCMD_TCPS_INFO("Saved client ip address(%d - %ld.%ld.%ld.%ld:%d)\n", idx,
                        (ntohl(cli_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(cli_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(cli_addr.sin_port)));
    }

    #endif // ENABLE_ATCMD_TCPS_DBG_INFO

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        //find empty idx
        if ((empty_idx < 0)
                && (memcmp(&conf->sess_info->cli_addr[idx],
                           &empty_addr,
                           sizeof(struct sockaddr_in)) == 0))
        {
            ATCMD_TCPS_INFO("Found Empty idx(%d)\n", idx);
            empty_idx = idx;
        }

        //compare ip address
        if (memcmp(&conf->sess_info->cli_addr[idx], &addr, sizeof(struct sockaddr_in)) == 0)
        {
            ATCMD_TCPS_INFO("Found client ip address(%d - %ld.%ld.%ld.%ld:%d)\n", idx,
                            (ntohl(addr.sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(addr.sin_port)));
            return -1;
        }
    }

    if (empty_idx < 0 || ATCMD_TCPS_MAX_SESS < empty_idx)
    {
        ATCMD_TCPS_ERR("Not found empty idx(%d)\n", empty_idx);
        return -1;
    }

    ATCMD_TCPS_INFO("Added client ip address(%d - %ld.%ld.%ld.%ld:%d)\n", empty_idx,
                    (ntohl(addr.sin_addr.s_addr) >> 24) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr) >>  8) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr)      ) & 0xFF,
                    (ntohs(addr.sin_port)));
    memcpy((struct sockaddr_in *)&conf->sess_info->cli_addr[empty_idx],
           &addr,
           sizeof(struct sockaddr_in));
    return 0;
}
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
static int atcmd_tcps_add_cli_addr6(atcmd_tcps_context * ctx, struct sockaddr_in6 addr)
{
    int idx = 0;
    int empty_idx = -1;
    struct sockaddr_in6 empty_addr = {0x00,};
    const atcmd_tcps_config * conf;
    ATCMD_TCPS_INFO("Start\n");

    if (ctx == NULL)
    {
        ATCMD_TCPS_ERR("Invaild parameter\n");
        return -1;
    }

    conf = ctx->conf;
    #if defined (ENABLE_ATCMD_TCPS_DBG_INFO)
    struct sockaddr_in6 cli_addr;

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        //cli_addr = {0, };
        memset(&cli_addr, 0, sizeof(struct sockaddr_in6));
        memcpy(&cli_addr, &conf->sess_info->cli_addr[idx], sizeof(struct sockaddr_in6));
        ATCMD_TCPS_INFO("Saved client ip address(%d - %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n", idx,
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(cli_addr.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(cli_addr.sin6_port)));
    }

    #endif // ENABLE_ATCMD_TCPS_DBG_INFO

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        //find empty idx
        if ((empty_idx < 0)
                && (memcmp(&conf->sess_info->cli_addr[idx],
                           &empty_addr,
                           sizeof(struct sockaddr_in6)) == 0))
        {
            ATCMD_TCPS_INFO("Found Empty idx(%d)\n", idx);
            empty_idx = idx;
        }

        //compare ip address
        if (memcmp(&conf->sess_info->cli_addr[idx], &addr, sizeof(struct sockaddr_in6)) == 0)
        {
            ATCMD_TCPS_INFO("Found client ip address(%d - %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n", idx,
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(addr.sin6_port)));
            return -1;
        }
    }

    if (empty_idx < 0 || ATCMD_TCPS_MAX_SESS < empty_idx)
    {
        ATCMD_TCPS_ERR("Not found empty idx(%d)\n", empty_idx);
        return -1;
    }

    ATCMD_TCPS_INFO("Added client ip address(%d - %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n", empty_idx,
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[3])) & 0xffff),
                    (ntohs(addr.sin6_port)));
    memcpy((struct sockaddr_in6 *)&conf->sess_info->cli_addr[empty_idx],
           &addr,
           sizeof(struct sockaddr_in6));
    return 0;
}
#endif // __SUPPORT_IPV6__

#if defined ( __SUPPORT_IPV4__ )
static int atcmd_tcps_del_cli_addr(atcmd_tcps_context * ctx, struct sockaddr_in addr)
{
    int idx = 0;
    const atcmd_tcps_config * conf;

    if (ctx == NULL)
    {
        ATCMD_TCPS_ERR("Invaild parameter\n");
        return -1;
    }

    conf = ctx->conf;
    struct sockaddr_in cli_addr;

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        memset(&cli_addr, 0, sizeof(struct sockaddr_in));
        memcpy(&cli_addr, &conf->sess_info->cli_addr[idx], sizeof(struct sockaddr_in));

        //find & delete ip address
        if (memcmp(&cli_addr, &addr, sizeof(struct sockaddr_in)) == 0)
        {
            ATCMD_TCPS_INFO("Found client ip address(%d - %ld.%ld.%ld.%ld:%d)\n", idx,
                            (ntohl(addr.sin_addr.s_addr) >> 24) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr) >> 16) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr) >>  8) & 0xFF,
                            (ntohl(addr.sin_addr.s_addr)      ) & 0xFF,
                            (ntohs(addr.sin_port)));
            memset((struct sockaddr_in *)&conf->sess_info->cli_addr[idx],
                   0x00,
                   sizeof(struct sockaddr_in));
            return 0;
        }
    }

    ATCMD_TCPS_INFO("Not found client ip address(%ld.%ld.%ld.%ld:%d)\n",
                    (ntohl(addr.sin_addr.s_addr) >> 24) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr) >>  8) & 0xFF,
                    (ntohl(addr.sin_addr.s_addr)      ) & 0xFF,
                    (ntohs(addr.sin_port)));
    return -1;
}
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
static int atcmd_tcps_del_cli_addr6(atcmd_tcps_context * ctx, struct sockaddr_in6 addr)
{
    int idx = 0;
    const atcmd_tcps_config * conf;

    if (ctx == NULL)
    {
        ATCMD_TCPS_ERR("Invaild parameter\n");
        return -1;
    }

    conf = ctx->conf;
    struct sockaddr_in6 cli_addr;

    for (idx = 0 ; idx < ATCMD_TCPS_MAX_SESS ; idx++)
    {
        memset(&cli_addr, 0, sizeof(struct sockaddr_in6));
        memcpy(&cli_addr, &conf->sess_info->cli_addr[idx], sizeof(struct sockaddr_in6));

        //find & delete ip address
        if (memcmp(&cli_addr, &addr, sizeof(struct sockaddr_in6)) == 0)
        {
            ATCMD_TCPS_INFO("Found client ip address(%d - %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n", idx,
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[0])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[1])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[2])) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                            ((PP_HTONL(addr.sin6_addr.un.u32_addr[3])) & 0xffff),
                            (ntohs(addr.sin6_port)));
            memset((struct socketaddr_in *)&conf->sess_info->cli_addr[idx],
                   0x00,
                   sizeof(struct sockaddr_in6));
            return 0;
        }
    }

    ATCMD_TCPS_INFO("Not found client ip address(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(addr.sin6_addr.un.u32_addr[3])) & 0xffff),
                    (ntohs(addr.sin6_port)));
    return -1;
}
#endif // __SUPPORT_IPV6__

static void atcmd_tcps_cli_task_entry(void * param)
{
    int ret = 0;
#if CFG_PMGR
    unsigned int local_port = 0;
#endif /* CFG_PMGR */
    atcmd_tcps_cli_context * cli_ctx = (atcmd_tcps_cli_context *)param;
    void * p_at_ctrl = ((atcmd_tcps_context *)cli_ctx->svr_ptr)->p_at_ctrl;
    unsigned char * hdr = NULL;
    size_t hdr_len = 0;
    unsigned char * payload = NULL;
    size_t payload_len = 0;
    size_t tot_len = 0;
    size_t act_hdr_len = 0;
    size_t act_payload_len = 0;
    char conn_info_str[256] = {0x00,};
    int conn_info_str_len = 0;
    const int cid = cli_ctx->cid;
    ATCMD_TCPS_INFO("Start\n");

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                       g_wifi_cfg.p_watchdog_service->p_cfg,
                                                       &task_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     task_wdog_id,
                                                     ATCMD_TCPS_WDOG_LATENCY);
#endif
#endif

    if (!cli_ctx)
    {
        ATCMD_TCPS_ERR("Invalid parameter\n");
        goto end;
    }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    if (cli_ctx->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        local_port = ntohs(cli_ctx->addr.sin_port);
        #endif // __SUPPORT_IPV4__
    }
    else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        local_port = ntohs(cli_ctx->addr6.sin6_port);
        #endif // __SUPPORT_IPV6__
    }

    RM_PMGR_W_dpm_job_name_set((char *)(cli_ctx->task_name), local_port);

    RM_WIFI_dpm_tcp_port_filter_set(local_port);

    ATCMD_TCPS_INFO("Reg - DPM Name:%s(%d), Local port(%d)\n",
                    cli_ctx->task_name, strlen((char *)cli_ctx->task_name), local_port);

    RM_PMGR_W_dpm_wakeup_done((char *)(cli_ctx->task_name));

    RM_PMGR_W_dpm_rcv_ready_set((char *)(cli_ctx->task_name));
#endif /* CFG_PMGR */
#endif

    if (atcmd_transport_get_available_session() < 0)
    {
        goto end;
    }

    if (cli_ctx->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        if (atcmd_tcps_add_cli_addr((atcmd_tcps_context *)cli_ctx->svr_ptr, cli_ctx->addr) == 0)
        {
            conn_info_str_len = sprintf(conn_info_str,
                                        "\r\n" ATCMD_TCPS_CONN_RX_PREFIX ":%d,%ld.%ld.%ld.%ld,%u\r\n",
                                        cid,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                        (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                        (ntohs(cli_ctx->addr.sin_port)));

            if (p_at_ctrl && conn_info_str_len > 0)
            {
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
            }
        }

        #endif //
    }
    else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )

        if (atcmd_tcps_add_cli_addr6((atcmd_tcps_context *)cli_ctx->svr_ptr, cli_ctx->addr6) == 0)
        {
            conn_info_str_len = sprintf(conn_info_str,
                                        "\r\n" ATCMD_TCPS_CONN_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u\r\n",
                                        cid,
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                        ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                        (ntohs(cli_ctx->addr6.sin6_port)));

            if (p_at_ctrl && conn_info_str_len > 0)
            {
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
            }
        }

        #endif // __SUPPORT_IPV6__
    }

    cli_ctx->state = ATCMD_TCPS_CLI_STATE_CONNECTED;

    while (cli_ctx->state == ATCMD_TCPS_CLI_STATE_CONNECTED)
    {
#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     g_wifi_cfg.p_watchdog_service->p_cfg,
                                                     task_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         task_wdog_id,
                                                         ATCMD_TCPS_WDOG_LATENCY);
#endif
#endif

        tot_len = 0;
        act_hdr_len = 0;
        act_payload_len = 0;
        hdr = cli_ctx->buffer;
        hdr_len = ATCMD_TCPS_RECV_HDR_SIZE;
        payload = cli_ctx->buffer + hdr_len;

#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
        if (is_tcps_data_mode_hexstring())
        {
            /* We have only room for half of the payload as a byte is represented by 2 characters. */
            payload_len = ATCMD_TCPS_RECV_PAYLOAD_SIZE / 2;
        }
        else
#endif
        {
            payload_len = ATCMD_TCPS_RECV_PAYLOAD_SIZE;
        }

        ret = recv(cli_ctx->socket, payload, payload_len, 0);

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
        RM_PMGR_W_dpm_sleep_ready_clear((char *)(cli_ctx->task_name));
#endif /* CFG_PMGR */
#endif

        if (ret > 0)
        {
            act_payload_len = ret;

            if (cli_ctx->ip_type == IPADDR_TYPE_V4)
            {
                #if defined ( __SUPPORT_IPV4__ )
                ATCMD_TCPS_INFO("Recv(ip:%ld.%ld.%ld.%ld:%d/len:%d)\n",
                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                (ntohs(cli_ctx->addr.sin_port)), act_payload_len);
                #endif // __SUPPORT_IPV4__
            }
            else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
            {
                #if defined ( __SUPPORT_IPV6__ )
                ATCMD_TCPS_INFO("Recv(ip:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d/len:%d)\n",
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                (ntohs(cli_ctx->addr6.sin6_port)), act_payload_len);
                #endif // __SUPPORT_IPV6__
            }

            #if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)

            if (is_tcps_data_mode_hexstring())
            {
                char * hex_buf = pvPortMalloc(ATCMD_TCPS_RECV_HDR_SIZE + (act_payload_len * 2));

                if (!hex_buf)
                {
                    ATCMD_TCPS_ERR("Failed to allocate buffer to pass recv data(%d)\n",
                                   ATCMD_TCPS_RECV_HDR_SIZE + (act_payload_len * 2));
                    goto end;
                }

                if (cli_ctx->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    act_hdr_len = snprintf((char *)hex_buf, ATCMD_TCPS_RECV_HDR_SIZE,
                                           "\r\n" ATCMD_TCPS_DATA_RX_PREFIX ":%d,%d.%d.%d.%d,%u,%d,", cid,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                           (ntohs(cli_ctx->addr.sin_port)), (act_payload_len * 2));
                    #endif // __SUPPORT_IPV4__
                }
                else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    act_hdr_len = snprintf((char *)hex_buf, ATCMD_TCPS_RECV_HDR_SIZE,
                                           "\r\n" ATCMD_TCPS_DATA_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u,%d,", cid,
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                           (ntohs(cli_ctx->addr6.sin6_port)), (act_payload_len * 2));
                    #endif // __SUPPORT_IPV6__
                }

                tot_len = act_hdr_len;
                Convert_Str2HexStr((char *)payload, hex_buf + tot_len, (u32_t)(act_payload_len));
                tot_len += act_payload_len * 2;
                memcpy(hex_buf + tot_len, "\r\n", 2);
                tot_len += 2;
                hex_buf[tot_len] = '\0';

                if (p_at_ctrl)
                {
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)hex_buf, tot_len);
                }

                vPortFree(hex_buf);
                hex_buf = NULL;
            }
            else
            #endif // __SUPPORT_TCP_RECVDATA_HEX_MODE__
            {
                if (cli_ctx->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    act_hdr_len = snprintf((char *)hdr, hdr_len,
                                           "\r\n" ATCMD_TCPS_DATA_RX_PREFIX ":%d,%d.%d.%d.%d,%u,%d,", cid,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                           (int)(ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                           (ntohs(cli_ctx->addr.sin_port)), act_payload_len);
                    #endif // __SUPPORT_IPV4__
                }
                else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    act_hdr_len = snprintf((char *)hdr, hdr_len,
                                           "\r\n" ATCMD_TCPS_DATA_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u,%d,", cid,
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                           ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                           (ntohs(cli_ctx->addr6.sin6_port)), act_payload_len);
                    #endif // __SUPPORT_IPV6__
                }

                tot_len = act_hdr_len;

                if (!memmove(payload - act_hdr_len, hdr, act_hdr_len))
                {
                    ATCMD_TCPS_ERR("Failed to copy received data(%d)\n", act_payload_len);
                }

                hdr = payload - act_hdr_len;
                tot_len += act_payload_len;
                memcpy(hdr + tot_len, "\r\n", 2);
                tot_len += 2;
#if (ATCMD_TRANSPORT_UART_W == 1)
                hdr[tot_len] = '\0';
#endif

                if (p_at_ctrl)
                {
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)hdr, tot_len);
                }
            }
        }
        else
        {
            if ((errno != EAGAIN))
            {
                if (cli_ctx->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    ATCMD_TCPS_INFO("Disconnected(ip:%ld.%ld.%ld.%ld:%d/%d)\n",
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                    (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                    (ntohs(cli_ctx->addr.sin_port)), errno);
                    #endif // __SUPPORT_IPV4__
                }
                else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    ATCMD_TCPS_INFO("Disconnected(ip:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d/%d)\n",
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                    ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                    (ntohs(cli_ctx->addr6.sin6_port)), errno);
                    #endif // __SUPPORT_IPV6__
                }

                ATCMD_TCPS_ERR("TCP Client disconnected from Server (%d)\n", ret);

                if (cli_ctx->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    conn_info_str_len = sprintf(conn_info_str,
                                                "\r\n" ATCMD_TCPS_DISCONN_RX_PREFIX ":%d,%ld.%ld.%ld.%ld,%u\r\n",
                                                cid,
                                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                                (ntohl(cli_ctx->addr.sin_addr.s_addr) >>  8) & 0xFF,
                                                (ntohl(cli_ctx->addr.sin_addr.s_addr)      ) & 0xFF,
                                                (ntohs(cli_ctx->addr.sin_port)));
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
                    #endif // __SUPPORT_IPV4__
                }
                else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    conn_info_str_len = sprintf(conn_info_str,
                                                "\r\n" ATCMD_TCPS_DISCONN_RX_PREFIX ":%d:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u\r\n",
                                                cid,
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                                ((PP_HTONL(cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                                (ntohs(cli_ctx->addr6.sin6_port)));
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
                    #endif // __SUPPORT_IPV6__
                }

                goto end;
            }
        }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
        if (!RM_PMGR_W_socket_rx_data_is_remaining(cli_ctx->socket))
        {
            RM_PMGR_W_dpm_sleep_ready_set((char *)(cli_ctx->task_name));
        }
#endif /* CFG_PMGR */
#endif
    }

end:

    if (cli_ctx->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        atcmd_tcps_del_cli_addr((atcmd_tcps_context *)cli_ctx->svr_ptr, cli_ctx->addr);
        #endif // __SUPPORT_IPV4__
    }
    else if (cli_ctx->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        atcmd_tcps_del_cli_addr6((atcmd_tcps_context *)cli_ctx->svr_ptr, cli_ctx->addr6);
        #endif // __SUPPORT_IPV6__
    }

    atcmd_tcps_close_socket(&cli_ctx->socket);

    if (cli_ctx->buffer)
    {
        vPortFree(cli_ctx->buffer);
        cli_ctx->buffer = NULL;
    }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if CFG_PMGR
    RM_WIFI_dpm_tcp_port_delete(local_port);
    RM_PMGR_W_dpm_job_name_clear((char *)(cli_ctx->task_name));
    ATCMD_TCPS_INFO("Unreg - DPM Name:%s(%d), Local port(%d)\n",
                    cli_ctx->task_name, strlen((char *)cli_ctx->task_name), local_port);
#endif /* CFG_PMGR */
#endif
    cli_ctx->state = ATCMD_TCPS_CLI_STATE_TERMINATED;

    if (cli_ctx->event)
    {
        xEventGroupSetBits(cli_ctx->event, ATCMD_TCPS_EVT_CLOSED);
    }

#ifndef ATCMD_TCP_SERV_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif
#endif

    ATCMD_TCPS_INFO("End\n");
    cli_ctx->task_handler = NULL;
    vTaskDelete(NULL);
    return ;
}
#if defined ( __SUPPORT_IPV4__ )
static int atcmd_tcps_compare_ip_addr(struct sockaddr_in * a, struct sockaddr_in * b)
{
    if (!a || !b)
    {
        ATCMD_TCPS_INFO("Invalid parameter\n");
        return -2;
    }

    //return memcmp(a, b, sizeof(struct sockaddr_in));
    if ((a->sin_family == b->sin_family)
            && (ntohl(a->sin_addr.s_addr) == ntohl(b->sin_addr.s_addr))
            && (ntohs(a->sin_port) == ntohs(b->sin_port)))
    {
        return 0;
    }

    return -1;
}
#endif // __SUPPORT_IPV4__

#if defined ( __SUPPORT_IPV6__ )
static int atcmd_tcps_compare_ip_addr6(struct sockaddr_in6 * a, struct sockaddr_in6 * b)
{
    if (!a || !b)
    {
        ATCMD_TCPS_INFO("Invalid parameter\n");
        return -2;
    }

    //return memcmp(a, b, sizeof(struct sockaddr_in));
    if ((a->sin6_family == b->sin6_family)
            && (ntohl(a->sin6_addr.un.u32_addr[0]) == ntohl(b->sin6_addr.un.u32_addr[0]))
            && (ntohl(a->sin6_addr.un.u32_addr[1]) == ntohl(b->sin6_addr.un.u32_addr[1]))
            && (ntohl(a->sin6_addr.un.u32_addr[2]) == ntohl(b->sin6_addr.un.u32_addr[2]))
            && (ntohl(a->sin6_addr.un.u32_addr[3]) == ntohl(b->sin6_addr.un.u32_addr[3]))
            && (ntohs(a->sin6_port) == ntohs(b->sin6_port)))
    {
        return 0;
    }

    return -1;
}
#endif // __SUPPORT_IPV6__
#endif /* CFG_WIFI */

/* EOF */
