/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_tcp_client.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"

#include "rm_wifi.h"
#include "rm_lwip_w_helper.h"

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#undef  ENABLE_ATCMD_TCPC_DBG_INFO
#undef  ENABLE_ATCMD_TCPC_DBG_ERR

#define ATCMD_TCPC_DBG  printf

#if defined (ENABLE_ATCMD_TCPC_DBG_INFO)
#define ATCMD_TCPC_INFO(fmt, ...)   \
    ATCMD_TCPC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_TCPC_INFO(...)        do {} while (0)
#endif  // (ENABLE_ATCMD_TCPC_DBG_INFO)

#if defined (ENABLE_ATCMD_TCPC_DBG_ERR)
#define ATCMD_TCPC_ERR(fmt, ...)    \
    ATCMD_TCPC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_TCPC_ERR(...)         do {} while (0)
#endif // (ENABLE_ATCMD_TCPC_DBG_ERR)


static void atcmd_tcpc_task_entry(void * param);

#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
static int tcpc_data_mode = 0;

static int is_tcpc_data_mode_hexstring(void)
{
    return tcpc_data_mode == 1 ? 1 : 0;
}

void set_tcpc_data_mode(int mode)
{
    /*
     * TCP_DATA_MODE_ASCII 	= 0
     * TCP_DATA_MODE_HEXSTRING = 1
     */
    tcpc_data_mode = mode;
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
#endif // __SUPPORT_TCP_RECVDATA_HEX_MODE__

//// For create random port APIs /////////////////////////////////////////
static inline unsigned long atcmd_get_random_value(void)
{
    return (unsigned long)rand();
}

/*
 * Get random value ( 16bits )
 */
unsigned short atcmd_get_random_value_ushort(void)
{
    unsigned short    result;

    result = (unsigned short)(atcmd_get_random_value() & 0x0000FFFF);

    return result;
}

unsigned short atcmd_get_random_value_ushort_range(int lower, int upper)
{
    if (lower < 1 || upper < 2 || lower >= upper) {return 0;}

    return (unsigned short)((atcmd_get_random_value_ushort() % (upper - lower + 1)) + lower);
}

int atcmd_tcpc_init_context(atcmd_tcpc_context * ctx)
{
    ATCMD_TCPC_INFO("Start\r\n");
    memset(ctx, 0x00, sizeof(atcmd_tcpc_context));
    ctx->socket = -1;
    ctx->state = ATCMD_TCPC_STATE_TERMINATED;
    return 0;
}

int atcmd_tcpc_deinit_context(atcmd_tcpc_context * ctx)
{
    ATCMD_TCPC_INFO("Start\r\n");

    if (ctx->state != ATCMD_TCPC_STATE_TERMINATED)
    {
        ATCMD_TCPC_ERR("tcp client is not terminated(%d)\r\n", ctx->state);
        return -1;
    }

    if (ctx->task_handler)
    {
        ATCMD_TCPC_INFO("To delete tcp client task\r\n");
        TaskHandle_t handler = ctx->task_handler;
        ctx->task_handler = NULL;

        vTaskDelete(handler);
    }

    if (ctx->buffer)
    {
        ATCMD_TCPC_INFO("To free tcp client's recv buffer\r\n");
        vPortFree(ctx->buffer);
        ctx->buffer = NULL;
    }

    if (ctx->socket > -1)
    {
        ATCMD_TCPC_INFO("To close tcp client socket\r\n");
        close(ctx->socket);
        ctx->socket = -1;
    }

    if (ctx->event)
    {
        ATCMD_TCPC_INFO("To delete event\n");
        vEventGroupDelete(ctx->event);
        ctx->event = NULL;
    }

    atcmd_tcpc_init_context(ctx);

    return 0;
}

int atcmd_tcpc_init_config(const int cid, atcmd_tcpc_config * conf, atcmd_tcpc_sess_info * sess_info)
{
    ATCMD_TCPC_INFO("Start\r\n");

    if (!conf || !sess_info)
    {
        ATCMD_TCPC_ERR("Invalid parameter\n");
        return -1;
    }

    conf->cid = cid;
    snprintf((char *)conf->task_name, (ATCMD_TCPC_MAX_TASK_NAME - 1), "%s_%d",
             ATCMD_TCPC_TASK_NAME, cid);
    conf->task_priority = ATCMD_TCPC_TASK_PRIORITY;
    conf->task_size = (ATCMD_TCPC_TASK_SIZE / 4);
    snprintf(conf->sock_name, (ATCMD_TCPC_MAX_SOCK_NAME - 1), "%s_%d", ATCMD_TCPC_SOCK_NAME, cid);
    conf->rx_buflen = ATCMD_TCPC_RECV_BUF_SIZE;
    conf->sess_info = sess_info;
    return 0;
}

int atcmd_tcpc_deinit_config(atcmd_tcpc_config * conf)
{
    ATCMD_TCPC_INFO("Start\r\n");
    memset(conf, 0x00, sizeof(atcmd_tcpc_config));
    return 0;
}

int atcmd_tcpc_set_at_ctrl(atcmd_tcpc_context * ctx, void * const p_at_ctrl)
{
    if (!ctx || ctx->p_at_ctrl)
    {
        return -1;
    }

    ctx->p_at_ctrl = p_at_ctrl;
    return 0;
}

int atcmd_tcpc_set_local_addr(atcmd_tcpc_config * p_conf, char * p_ip, int port)
{
    ATCMD_TCPC_INFO("Start\r\n");

    if (p_ip)
    {
        /* Not implemented yet. */
        ATCMD_TCPC_ERR("Not allowed to set local IP address\r\n");
        return -1;
    }

    /* Check range */
    if (port < ATCMD_TCPC_MIN_PORT || port > ATCMD_TCPC_MAX_PORT)
    {
        ATCMD_TCPC_ERR("Invalid port(%d)\n", port);
        return -1;
    }

    if (port == 0)
    {
        port = (int)atcmd_get_random_value_ushort_range(ATCMD_TCPC_DEF_PORT, ATCMD_TCPC_DEF_PORT * 2);
    }

    if (p_conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        p_conf->local_addr.sin_family = AF_INET;
        p_conf->local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        p_conf->local_addr.sin_port = htons(port);
#endif // __SUPPORT_IPV4__
    }
    else if (p_conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        p_conf->local_addr6.sin6_family = AF_INET6;
        p_conf->local_addr6.sin6_addr = in6addr_any;
        p_conf->local_addr6.sin6_port = htons(port);
#endif // __SUPPORT_IPV6__
    }

    p_conf->sess_info->local_port = port;

    return 0;
}

int atcmd_tcpc_set_svr_addr(atcmd_tcpc_config * conf, char * ip, int port)
{
    int ret = 0;
    struct addrinfo hints, * addr_list = NULL;
    char str_port[16] = {0x00, };
    ATCMD_TCPC_INFO("Start\r\n");

    if (!ip)
    {
        ATCMD_TCPC_ERR("Invalid parameters\r\n");
        return -1;
    }

    //check range
    if (port <= ATCMD_TCPC_MIN_PORT || port > ATCMD_TCPC_MAX_PORT)
    {
        ATCMD_TCPC_ERR("Invalid port(%d)\n", port);
        return -1;
    }

    memset(&hints, 0x00, sizeof(struct addrinfo));

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        if (!is_in_valid_ip_class(ip))
        {
            hints.ai_family = AF_INET;  //IPv4 only
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            snprintf(str_port, sizeof(str_port), "%d", port);
            ret = getaddrinfo(ip, str_port, &hints, &addr_list);

            if ((ret != 0) || !addr_list)
            {
                printf("Failed to get address info(%d)\r\n", ret);
                return -1;
            }

            //pick 1st address
            memcpy((struct sockaddr *)&conf->svr_addr, addr_list->ai_addr, sizeof(struct sockaddr));
            freeaddrinfo(addr_list);
        }
        else
        {
            conf->svr_addr.sin_addr.s_addr = inet_addr(ip);
        }

        conf->svr_addr.sin_family = AF_INET;
        conf->svr_addr.sin_port = htons(port);
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        char svr_addrArr[16];
        inet_pton(AF_INET6, ip, svr_addrArr);
        memcpy(conf->svr_addr6.sin6_addr.s6_addr, svr_addrArr, 16);
        conf->svr_addr6.sin6_family = AF_INET6;
        conf->svr_addr6.sin6_port = htons(port);
        #endif // __SUPPORT_IPV6__
    }

    conf->sess_info->peer_port = port;
    strncpy(conf->sess_info->peer_ipaddr, ip, sizeof(conf->sess_info->peer_ipaddr));

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ATCMD_TCPC_INFO("TCP server(%ld.%ld.%ld.%ld:%d)\r\n",
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(conf->svr_addr.sin_port)));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ATCMD_TCPC_INFO("TCP server(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\n",
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(conf->svr_addr6.sin6_port)));
        #endif // __SUPPORT_IPV6__
    }

    return 0;
}

int atcmd_tcpc_set_config(atcmd_tcpc_context * ctx, atcmd_tcpc_config * conf)
{
    uint8_t zero_addr[16] = {0};

    ATCMD_TCPC_INFO("Start\r\n");

    if (strlen((const char *)(conf->task_name)) == 0)
    {
        ATCMD_TCPC_ERR("Invalid task name\r\n");
        return -1;
    }

    if (conf->task_priority == 0)
    {
        ATCMD_TCPC_ERR("Invalid task priority\r\n");
        return -1;
    }

    if (conf->task_size == 0)
    {
        ATCMD_TCPC_ERR("Invalid task size\r\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        if (conf->local_addr.sin_family != AF_INET)
        {
            ATCMD_TCPC_ERR("Invalid local address\r\n");
            return -1;
        }

        if ((conf->svr_addr.sin_family != AF_INET)
                || (conf->svr_addr.sin_port == 0)
                || (conf->svr_addr.sin_addr.s_addr == 0))
        {
            ATCMD_TCPC_ERR("Invalid tcp server address\r\n");
            return -1;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )

        if (conf->local_addr6.sin6_family != AF_INET6)
        {
            ATCMD_TCPC_ERR("Invalid local address\r\n");
            return -1;
        }

        if ((conf->svr_addr6.sin6_family != AF_INET6)
                || (conf->svr_addr6.sin6_port == 0)
                || !memcmp(conf->svr_addr6.sin6_addr.s6_addr, zero_addr, 16))
        {
            ATCMD_TCPC_ERR("Invalid tcp server address\r\n");
            return -1;
        }

        #endif // __SUPPORT_IPV6__
    }

    if (conf->rx_buflen == 0)
    {
        ATCMD_TCPC_ERR("Invalid recv buffer size\r\n");
        return -1;
    }

    if (strlen((const char *)(conf->sock_name)) == 0)
    {
        ATCMD_TCPC_ERR("Invalid socket name\n");
        return -1;
    }

    ctx->event = xEventGroupCreate();

    if (ctx->event == NULL)
    {
        ATCMD_TCPC_INFO("Failed to create event\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ATCMD_TCPC_INFO("*%-20s: %s(%d)\r\n" // task name
                        "*%-20s: %ld\r\n" // task priority
                        "*%-20s: %d\r\n" // task size
                        "*%-20s: %d\r\n" // rx buflen
                        "*%-20s: %ld.%ld.%ld.%ld:%d\r\n" // local ip address
                        "*%-20s: %ld.%ld.%ld.%ld:%d\r\n", // tcp server ip address
                        "Task Name", (char *)conf->task_name, strlen((const char *)conf->task_name),
                        "Task Priority", conf->task_priority,
                        "Task Size", conf->task_size,
                        "RX buffer size", conf->rx_buflen,
                        "Local IP address",
                        (ntohl(conf->local_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(conf->local_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(conf->local_addr.sin_port)),
                        "TCP Server IP address",
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(conf->svr_addr.sin_port)));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ATCMD_TCPC_INFO("*%-20s: %s(%d)\r\n" // task name
                        "*%-20s: %ld\r\n" // task priority
                        "*%-20s: %d\r\n" // task size
                        "*%-20s: %d\r\n" // rx buflen
                        "*%-20s: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx::%d\r\n" // local ip address
                        "*%-20s: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx::%d\r\n", // tcp server ip address
                        "Task Name", (char *)conf->task_name, strlen((const char *)conf->task_name),
                        "Task Priority", conf->task_priority,
                        "Task Size", conf->task_size,
                        "RX buffer size", conf->rx_buflen,
                        "Local IP address",
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->local_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(conf->local_addr6.sin6_port)),
                        "TCP Server IP address",
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(conf->svr_addr6.sin6_port)));
        #endif // __SUPPORT_IPV6__
    }

    ctx->conf = conf;
    return 0;
}

int atcmd_tcpc_wait_for_ready(atcmd_tcpc_context * ctx)
{
    const int wait_time = portCONVERT_MS_2_TICKS(100 * 10); // 1sec
    int cnt = 0;
    unsigned int events = 0x00;

    if (!ctx)
    {
        ATCMD_TCPC_ERR("Invalid parameter\n");
        return -1;
    }

    //Check tcp client connects
    for (cnt = 0 ; cnt < (ATCMD_TCPC_RECONN_COUNT + 1) ; cnt++)
    {
        if (ctx->event)
        {
            events = xEventGroupWaitBits(ctx->event, ATCMD_TCPC_EVT_ANY,
                                         pdTRUE, pdFALSE, wait_time);

            if (events & ATCMD_TCPC_EVT_CONNECTION)
            {
                ATCMD_TCPC_INFO("Got connection event\n");
                return 0;
            }
            else if (events & ATCMD_TCPC_EVT_CLOSED)
            {
                ATCMD_TCPC_INFO("Got close event\n");
                return -1;
            }
        }
        else
        {
            if (ctx->state == ATCMD_TCPC_STATE_CONNECTED)
            {
                return 0;
            }
            else if (ctx->state == ATCMD_TCPC_STATE_TERMINATED)
            {
                return -1;
            }

            vTaskDelay(wait_time);
        }
    }

    return -1;
}

int atcmd_tcpc_start(atcmd_tcpc_context * ctx)
{
    int ret = 0;
    ATCMD_TCPC_INFO("Start\r\n");

    if (!ctx->conf)
    {
        ATCMD_TCPC_ERR("Invalid parameters\r\n");
        return -1;
    }

    if (ctx->state != ATCMD_TCPC_STATE_TERMINATED)
    {
        ATCMD_TCPC_ERR("TCP client is not terminated(%d)\r\n", ctx->state);
        return -1;
    }

    ctx->state = ATCMD_TCPC_STATE_DISCONNECTED;
    ret = xTaskCreate(atcmd_tcpc_task_entry,
                      (const char *)(ctx->conf->task_name),
                      ctx->conf->task_size,
                      (void *)ctx,
                      ctx->conf->task_priority,
                      &ctx->task_handler);

    if (ret != pdPASS)
    {
        printf("Failed to create tcp client task(%d)\r\n", ret);
        ctx->state = ATCMD_TCPC_STATE_TERMINATED;
        return -1;
    }

    return 0;
}

int atcmd_tcpc_stop(atcmd_tcpc_context * ctx)
{
    const int wait_time = portCONVERT_MS_2_TICKS(100);
    const int max_cnt = 10;
    int cnt = 0;
    unsigned int events = 0x00;

    if (ctx->state == ATCMD_TCPC_STATE_CONNECTED)
    {
        ATCMD_TCPC_INFO("Change tcp client state from %d to %d\r\n",
                        ctx->state, ATCMD_TCPC_STATE_REQ_TERMINATE);
        ctx->state = ATCMD_TCPC_STATE_REQ_TERMINATE;

        for (cnt = 0 ; cnt < max_cnt ; cnt++)
        {
            if (ctx->event)
            {
                events = xEventGroupWaitBits(ctx->event,
                                             ATCMD_TCPC_EVT_CLOSED,
                                             pdTRUE,
                                             pdFALSE,
                                             wait_time);

                if (events & ATCMD_TCPC_EVT_CLOSED)
                {
                    ATCMD_TCPC_INFO("Closed tcp client task\n");
                    break;
                }
            }
            else
            {
                if (ctx->state == ATCMD_TCPC_STATE_TERMINATED)
                {
                    ATCMD_TCPC_INFO("Closed tcp client task\n");
                    break;
                }

                vTaskDelay(wait_time * 10);
            }

            ATCMD_TCPC_INFO("Waiting for closing task of tcp client(%d,%d,%d)\n", cnt, max_cnt, wait_time);
        }
    }

    return ((ctx->state == ATCMD_TCPC_STATE_TERMINATED) ? 0 : -1);
}

int atcmd_tcpc_tx(atcmd_tcpc_context * ctx, char * data, unsigned int * data_len)
{
    int ret = 0;
    int total_sent = 0;
    const int to_send = *data_len;
    int sent_cnt = 0;

    ATCMD_TCPC_INFO("Start\r\n");

    if (ctx->state != ATCMD_TCPC_STATE_CONNECTED)
    {
        ATCMD_TCPC_ERR("Invalid parameter(data_len:%d, state:%d)\r\n", *data_len, ctx->state);
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    ATCMD_TCPC_INFO("To send data(%d)\r\n", *data_len);

    while (total_sent < to_send)
    {
        ret = send(ctx->socket, data + total_sent, to_send - total_sent, 0);
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

        if (sent_cnt >= ATCMD_TCPC_SEND_RETRY_CNT)
        {
           break;
        }

        ATCMD_TCPC_INFO("#%d. TCP Tx:%d/%d/%d/%d\n", sent_cnt, *data_len, total_sent, ret, errno);
    }

    if (total_sent != *data_len)
    {
        ATCMD_TCPC_ERR("Failed to send TCP data(%d/%d, errno:%d)\n",
                       *data_len, total_sent, errno);

        *data_len = total_sent;

        return FSP_ERR_AT_CMD_ERR_DATA_TX;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

int atcmd_tcpc_tx_with_peer_info(atcmd_tcpc_context * ctx, char * ip, int port,
                                 char * data, unsigned int * data_len)
{
    ATCMD_TCPC_INFO("Start\r\n");

    if (ctx == NULL || ctx->conf == NULL)
    {
        ATCMD_TCPC_ERR("Invalid context\n");
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    if (ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        //check ip address
        if (ip == NULL)
        {
            ATCMD_TCPC_ERR("Invalid IP address\n");
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
        else if (strcmp("0.0.0.0", ip) == 0 || strcmp("0", ip) == 0)
        {
            ;
        }
        else if (!is_in_valid_ip_class(ip))
        {
            ATCMD_TCPC_INFO("Not supported domain yet\n");
            return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
        }
        else if (ctx->conf->svr_addr.sin_addr.s_addr != inet_addr(ip))
        {
            ATCMD_TCPC_ERR("Not matched IP address(%ld.%ld.%ld.%ld vs %s)\n",
                           (ntohl(ctx->conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                           (ntohl(ctx->conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                           (ntohl(ctx->conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                           (ntohl(ctx->conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                           ip);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        char svr_addrArr[16];
        inet_pton(AF_INET6, ip, svr_addrArr);

        //check ip address
        if (ip == NULL)
        {
            ATCMD_TCPC_ERR("Invalid IP address\n");
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
        else if (strcmp("0:0:0:0:0:0:0:0", ip) == 0 || strcmp("0", ip) == 0)
        {
            ;
        }
        else if (memcmp(ctx->conf->svr_addr6.sin6_addr.s6_addr, svr_addrArr, sizeof(svr_addrArr)) != 0)
        {
            ATCMD_TCPC_ERR("Not matched IP address(0x%lx:0x%lx:0x%lx:0x%lx vs %s)\n",
                           (ctx->conf->svr_addr6.sin6_addr.un.u32_addr[0]),
                           (ctx->conf->svr_addr6.sin6_addr.un.u32_addr[1]),
                           (ctx->conf->svr_addr6.sin6_addr.un.u32_addr[2]),
                           (ctx->conf->svr_addr6.sin6_addr.un.u32_addr[3]),
                           ip);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        #endif // __SUPPORT_IPV6__
    }

    if (ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        //check port
        if (port < ATCMD_TCPC_MIN_PORT || port > ATCMD_TCPC_MAX_PORT)
        {
            ATCMD_TCPC_ERR("Invalid port(%d)\n", port);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
        else if (((strcmp("0.0.0.0", ip) == 0) || (strcmp("0", ip) == 0)) && (port == 0))
        {
            ;
        }
        else if (ctx->conf->svr_addr.sin_port != htons(port))
        {
            ATCMD_TCPC_ERR("Not matched port(%d vs %d)\n",
                           ntohs(ctx->conf->svr_addr.sin_port), port);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )

        //check port
        if (port < ATCMD_TCPC_MIN_PORT || port > ATCMD_TCPC_MAX_PORT)
        {
            ATCMD_TCPC_ERR("Invalid port(%d)\n", port);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
        else if (((strcmp("0:0:0:0:0:0:0:0", ip) == 0 || strcmp("0", ip) == 0)) && (port == 0))
        {
            ;
        }
        else if (ctx->conf->svr_addr6.sin6_port != htons(port))
        {
            ATCMD_TCPC_ERR("Not matched port(%d vs %d)\n",
                           ntohs(ctx->conf->svr_addr6.sin6_port), port);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        #endif // __SUPPORT_IPV6__
    }

    return atcmd_tcpc_tx(ctx, data, data_len);
}

static void atcmd_tcpc_task_entry(void * param)
{
    int ret = 0;
    atcmd_tcpc_context * ctx = (atcmd_tcpc_context *)param;
    const atcmd_tcpc_config * conf = ctx->conf;
#if CFG_PMGR
    unsigned int local_port = 0;
#endif /* CFG_PMGR */
    fd_set writablefds;
    int sockfd_flags_before;
    struct timeval conn_timeout;
    //socket option
    int sockopt_reuse = 1;
    struct timeval sockopt_timeout;
    //releated with recv data
    const int cid = conf->cid;
    unsigned char * hdr = NULL;
    size_t hdr_len = 0;
    size_t tot_len = 0;
    size_t act_hdr_len = 0;
    size_t act_payload_len = 0;
    unsigned char * payload = NULL;
    size_t payload_len = 0;
    char conn_info_str[256] = {0x00,};
    int conn_info_str_len = 0;
    //name
#if CFG_PMGR
    const char * dpm_name = (const char *)conf->task_name;
    const char * sock_name = (const char *)conf->sock_name;
#endif /* CFG_PMGR */
    ATCMD_TCPC_INFO("Start\r\n");

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                       g_wifi_cfg.p_watchdog_service->p_cfg,
                                                       &task_wdog_id);
#endif

    ctx->state = ATCMD_TCPC_STATE_DISCONNECTED;
    ctx->buffer = pvPortMalloc(conf->rx_buflen);

    if (!ctx->buffer)
    {
        printf("[%s] No FREE memory space for rx buffer(%d)\r\n", __func__, conf->rx_buflen);
        goto atcmd_tcpc_term;
    }

#if CFG_PMGR
    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        local_port = ntohs(conf->local_addr.sin_port);
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        local_port = ntohs(conf->local_addr6.sin6_port);
        #endif // __SUPPORT_IPV6__
    }
#endif /* CFG_PMGR */

#if CFG_PMGR
    RM_PMGR_W_dpm_job_name_set((char *)dpm_name, local_port);
    RM_WIFI_dpm_tcp_port_filter_set(local_port);
    ATCMD_TCPC_INFO("Reg - DPM Name:%s(%d), Local port(%d)\n",
                    dpm_name, strlen(dpm_name), local_port);
#endif /* CFG_PMGR */

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        #if CFG_PMGR
        ctx->socket = socket_dpm((char *)sock_name, PF_INET, SOCK_STREAM, 0);
        #else
        ctx->socket = socket(PF_INET, SOCK_STREAM, 0);
        #endif /* CFG_PMGR */

        if (ctx->socket < 0)
        {
            printf("Failed to create socket of IPv4 tcp client(%d:%d)\r\n", ctx->socket, errno);
            goto atcmd_tcpc_term;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        #if CFG_PMGR
        ctx->socket = socket_dpm((char *)sock_name, PF_INET6, SOCK_STREAM, 0);
        #else
        ctx->socket = socket(PF_INET6, SOCK_STREAM, 0);
        #endif /* CFG_PMGR */

        if (ctx->socket < 0)
        {
            printf("Failed to create socket of IPv6 tcp client(%d:%d)\r\n", ctx->socket, errno);
            goto atcmd_tcpc_term;
        }

        #endif // __SUPPORT_IPV6__
    }

    ATCMD_TCPC_INFO("TCP Client: socket descriptor(%d)\r\n", ctx->socket);
    ret = setsockopt(ctx->socket, SOL_SOCKET, SO_REUSEADDR, &sockopt_reuse, sizeof(sockopt_reuse));

    if (ret != 0)
    {
        printf("Failed to set socket option - SO_REUSEADDR(%d)\r\n", ret);
    }

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_TCPC_RECV_TIMEOUT * 1000;
    ret = setsockopt(ctx->socket, SOL_SOCKET, SO_RCVTIMEO, &sockopt_timeout, sizeof(sockopt_timeout));

    if (ret != 0)
    {
        printf("Failed to set socket option - SO_RCVTIMEOUT(%d:%d)\r\n", ret, errno);
    }

    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_TCPC_SEND_TIMEOUT * 1000;

    if (setsockopt(ctx->socket, SOL_SOCKET, SO_SNDTIMEO, &sockopt_timeout, sizeof(sockopt_timeout)))
    {
        printf("Failed to set socket option - SO_SNDTIMEO(%d)\n", ATCMD_TCPC_SEND_TIMEOUT);
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr, sizeof(struct sockaddr_in));

        if (ret != 0)
        {
            printf("Failed to bind socket of IPv4 tcp client(%d:%d)\r\n", ret, errno);
            goto atcmd_tcpc_term;
        }

        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr6, sizeof(struct sockaddr_in6));

        if (ret != 0)
        {
            printf("Failed to bind socket of IPv6 tcp client(%d:%d)\r\n", ret, errno);
            goto atcmd_tcpc_term;
        }

        #endif // __SUPPORT_IPV6__
    }

    sockfd_flags_before = fcntl(ctx->socket, F_GETFL, 0);
    fcntl(ctx->socket, F_SETFL, sockfd_flags_before | O_NONBLOCK);

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ret = connect(ctx->socket, (struct sockaddr *)&conf->svr_addr, sizeof(struct sockaddr_in));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ret = connect(ctx->socket, (struct sockaddr *)&conf->svr_addr6, sizeof(struct sockaddr_in6));
        #endif // __SUPPORT_IPV6__
    }

    if (ret == 0)
    {
        ATCMD_TCPC_INFO("Connected, ret=0\r\n");
        fcntl(ctx->socket, F_SETFL, sockfd_flags_before);
    }
    else if ( ret == -1 && errno == EINPROGRESS)
    {
        ATCMD_TCPC_INFO("connection in progress, ret=%d, errno=%d\r\n", ret, errno);
        FD_ZERO(&writablefds);
        FD_SET(ctx->socket, &writablefds);
        conn_timeout.tv_sec = ATCMD_TCPC_RECONNECT_TIMEOUT;
        conn_timeout.tv_usec = 0;

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

        ret = select(ctx->socket + 1, NULL, &writablefds, NULL, &conn_timeout);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                              g_wifi_cfg.p_watchdog_service->p_cfg,
                                                              task_wdog_id);
#endif

        if (ret == -1)
        {
            ATCMD_TCPC_INFO("Select failed !\r\n");
            goto atcmd_tcpc_term;
        }
        else if (ret == 0)
        {
            ATCMD_TCPC_INFO("Select timeout !\r\n");
            goto atcmd_tcpc_term;
        }

        // Socket selected for write ...
        int sock_error;
        socklen_t len = sizeof(sock_error);
        ret = getsockopt(ctx->socket, SOL_SOCKET, SO_ERROR, &sock_error, &len);

        if (ret < 0)
        {
            printf("Error getsockopt (%d:%d)\r\n", ret, errno);
            goto atcmd_tcpc_term;
        }

        // getsockopt success !!
        if (sock_error == 0)
        {
            ATCMD_TCPC_INFO("Connected\r\n");
            fcntl(ctx->socket, F_SETFL, sockfd_flags_before);
        }
        else
        {
            printf("Error in non-blocking connection (sock_error=%d:%d)\r\n",
                   sock_error, errno);
            goto atcmd_tcpc_term;
        }
    }
    else
    {
        // Non EINPROGRESS error!
        printf("Error in connection (%d:%d)\r\n", ret, errno);
        goto atcmd_tcpc_term;
    }

    ctx->state = ATCMD_TCPC_STATE_CONNECTED;
#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_done((char *)dpm_name);
    RM_PMGR_W_dpm_rcv_ready_set((char *)dpm_name);
#endif /* CFG_PMGR */

    if (ctx->event)
    {
        xEventGroupSetBits(ctx->event, ATCMD_TCPC_EVT_CONNECTION);
    }

    ATCMD_TCPC_INFO("Ready to receive packet(cid:%d)\n", cid);

    while (ctx->state == ATCMD_TCPC_STATE_CONNECTED)
    {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     g_wifi_cfg.p_watchdog_service->p_cfg,
                                                     task_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         task_wdog_id,
                                                         ATCMD_TCPC_WDOG_LATENCY);
#endif

        tot_len = 0;
        act_hdr_len = 0;
        act_payload_len = 0;
        hdr = ctx->buffer;
        hdr_len = ATCMD_TCPC_RECV_HDR_SIZE;
        payload = ctx->buffer + hdr_len;
#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
        if (is_tcpc_data_mode_hexstring())
        {
            /* We have only room for half of the payload as a byte is represented by 2 characters. */
            payload_len = ATCMD_TCPC_RECV_PAYLOAD_SIZE / 2;
        }
        else
#endif
        {
            payload_len = ATCMD_TCPC_RECV_PAYLOAD_SIZE;
        }

        ret = recv(ctx->socket, payload, payload_len, 0);

#if CFG_PMGR
        RM_PMGR_W_dpm_sleep_ready_clear((char *)dpm_name);
#endif /* CFG_PMGR */

        if (ret > 0)
        {
            act_payload_len = ret;

            if (conf->ip_type == IPADDR_TYPE_V4)
            {
                #if defined ( __SUPPORT_IPV4__ )
                ATCMD_TCPC_INFO("Recv(ip:%ld.%ld.%ld.%ld:%d/len:%d)\r\n",
                                (ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                (ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                (ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                (ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                                (ntohs(conf->svr_addr.sin_port)), act_payload_len);
                #endif // __SUPPORT_IPV4__
            }
            else if (conf->ip_type == IPADDR_TYPE_V6)
            {
                #if defined ( __SUPPORT_IPV6__ )
                ATCMD_TCPC_INFO("Recv(ip:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d/len:%d)\r\n",
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                (ntohs(conf->svr_addr6.sin6_port)), act_payload_len);
                #endif // __SUPPORT_IPV6__
            }

            #if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)

            if (is_tcpc_data_mode_hexstring())
            {
                char * hex_buf = pvPortMalloc(ATCMD_TCPC_RECV_HDR_SIZE + (act_payload_len * 2));

                if (!hex_buf)
                {
                    ATCMD_TCPC_ERR("Failed to allocate buffer to pass recv data(%d)\n",
                                   ATCMD_TCPC_RECV_HDR_SIZE + (act_payload_len * 2));
                    goto atcmd_tcpc_term;
                }

                if (conf->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    act_hdr_len = snprintf((char *)hex_buf, ATCMD_TCPC_RECV_HDR_SIZE,
                                           "\r\n" ATCMD_TCPC_DATA_RX_PREFIX ":%d,%d.%d.%d.%d,%u,%d,", cid,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                                           (ntohs(conf->svr_addr.sin_port)), (act_payload_len * 2));
                    #endif // __SUPPORT_IPV4__
                }
                else if (conf->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    act_hdr_len = snprintf((char *)hex_buf, ATCMD_TCPC_RECV_HDR_SIZE,
                                           "\r\n" ATCMD_TCPC_DATA_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u,%d,", cid,
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                           (ntohs(conf->svr_addr6.sin6_port)), (act_payload_len * 2));
                    #endif // __SUPPORT_IPV6__
                }

                tot_len = act_hdr_len;
                Convert_Str2HexStr((char *)payload, hex_buf + tot_len, (u32_t)(act_payload_len));
                tot_len += (act_payload_len * 2);
                memcpy(hex_buf + tot_len, "\r\n", 2);
                tot_len += 2;
                hex_buf[tot_len] = '\0';

                if (ctx->p_at_ctrl)
                {
                    RM_ATCMD_W_CORE_Write((atcmd_w_ctrl_t * const) ctx->p_at_ctrl, (uint8_t *) hex_buf, tot_len);
                }

                vPortFree(hex_buf);
                hex_buf = NULL;
            }
            else
            #endif // __SUPPORT_TCP_RECVDATA_HEX_MODE__
            {
                if (conf->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    act_hdr_len = snprintf((char *)hdr, hdr_len,
                                           "\r\n" ATCMD_TCPC_DATA_RX_PREFIX ":%d,%d.%d.%d.%d,%u,%d,", cid,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                           (int)(ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                                           (ntohs(conf->svr_addr.sin_port)), act_payload_len);
                    #endif // __SUPPORT_IPV4__
                }
                else if (conf->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    act_hdr_len = snprintf((char *)hdr, hdr_len,
                                           "\r\n" ATCMD_TCPC_DATA_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u,%d,", cid,
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                           ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                           (ntohs(conf->svr_addr6.sin6_port)), act_payload_len);
                    #endif // __SUPPORT_IPV6__
                }

                tot_len = act_hdr_len;

                if (!memmove(payload - act_hdr_len, hdr, act_hdr_len))
                {
                    ATCMD_TCPC_ERR("Failed to copy received data(%d)\n", act_payload_len);
                }

                hdr = payload - act_hdr_len;
                tot_len += act_payload_len;
                memcpy(hdr + tot_len, "\r\n", 2);
                tot_len += 2;
#if (ATCMD_TRANSPORT_UART_W == 1)
                hdr[tot_len] = '\0';
#endif
                if (ctx->p_at_ctrl)
                {
                    RM_ATCMD_W_CORE_Write((atcmd_w_ctrl_t * const) ctx->p_at_ctrl, (uint8_t *) hdr, tot_len);
                }
            }
        }
        else
        {
            if (errno != EAGAIN)
            {
                if (conf->ip_type == IPADDR_TYPE_V4)
                {
                    #if defined ( __SUPPORT_IPV4__ )
                    ATCMD_TCPC_INFO("Disconnected(ip:%ld.%ld.%ld.%ld:%d/errno:%d)\r\n",
                                    (ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                    (ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                    (ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                    (ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                                    (ntohs(conf->svr_addr.sin_port)), errno);
                    printf("[%s] TCP Client disconnected from Server (%d)\n", __func__, ret);
                    conn_info_str_len = sprintf(conn_info_str,
                                                "\r\n" ATCMD_TCPC_DISCONN_RX_PREFIX ":%d,%ld.%ld.%ld.%ld,%u\r\n",
                                                cid,
                                                (ntohl(conf->svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                                (ntohl(conf->svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                                (ntohl(conf->svr_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                                (ntohl(conf->svr_addr.sin_addr.s_addr)      ) & 0xFF,
                                                (ntohs(conf->svr_addr.sin_port)));

                    if (ctx->p_at_ctrl && conn_info_str_len > 0)
                    {
                        RM_ATCMD_W_CORE_Write((atcmd_w_ctrl_t * const)ctx->p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
                    }

                    #endif // __SUPPORT_IPV4__
                }
                else if (conf->ip_type == IPADDR_TYPE_V6)
                {
                    #if defined ( __SUPPORT_IPV6__ )
                    ATCMD_TCPC_INFO("Disconnected(ip:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d/errno:%d)\r\n",
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                    ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                    (ntohs(conf->svr_addr6.sin6_port)), errno);
                    printf("[%s] TCP Client disconnected from Server (%d)\n", __func__, ret);
                    conn_info_str_len = sprintf(conn_info_str,
                                                "\r\n" ATCMD_TCPC_DISCONN_RX_PREFIX
                                                ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u\r\n",
                                                cid,
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                                ((PP_HTONL(conf->svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                                (ntohs(conf->svr_addr6.sin6_port)));

                    if (ctx->p_at_ctrl && conn_info_str_len > 0)
                    {
                        RM_ATCMD_W_CORE_Write((atcmd_w_ctrl_t * const)ctx->p_at_ctrl, (uint8_t *)conn_info_str, conn_info_str_len);
                    }

                    #endif // __SUPPORT_IPV6__
                }

                goto atcmd_tcpc_term;
            }
        }

#if CFG_PMGR
        if (!RM_PMGR_W_socket_rx_data_is_remaining(ctx->socket))
        {
            RM_PMGR_W_dpm_sleep_ready_set((char *)dpm_name);
        }
#endif /* CFG_PMGR */
    }

atcmd_tcpc_term:
    close(ctx->socket);
    ctx->socket = -1;

    if (ctx->buffer)
    {
        vPortFree(ctx->buffer);
        ctx->buffer = NULL;
    }

#if CFG_PMGR
    RM_WIFI_dpm_tcp_port_delete(local_port);
    RM_PMGR_W_dpm_job_name_clear((char *)dpm_name);
    ATCMD_TCPC_INFO("Unreg - DPM Name:%s(%d), Local port(%d)\n", dpm_name, strlen(dpm_name), local_port);
#endif /* CFG_PMGR */
    ctx->state = ATCMD_TCPC_STATE_TERMINATED;

    ATCMD_TCPC_INFO("End of task(state:%d)\r\n", ctx->state);

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

    if (ctx->event)
    {
        xEventGroupSetBits(ctx->event, ATCMD_TCPC_EVT_CLOSED);
    }

    if (ctx->task_handler)
    {
        ctx->task_handler = NULL;
        vTaskDelete(NULL);
    }

    return ;
}
#endif /* CFG_WIFI */

