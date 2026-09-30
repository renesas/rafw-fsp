/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_udp_session.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"

#include "rm_wifi.h"
#include "rm_lwip_w_helper.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#undef  ENABLE_ATCMD_UDPS_DBG_INFO
#undef  ENABLE_ATCMD_UDPS_DBG_ERR

#define	ATCMD_UDPS_DBG	printf

#if defined (ENABLE_ATCMD_UDPS_DBG_INFO)
#define	ATCMD_UDPS_INFO(fmt, ...)	\
    ATCMD_UDPS_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	ATCMD_UDPS_INFO(...)	do {} while (0)
#endif	// (ENABLE_ATCMD_TCCS_DBG_INFO)

#if defined (ENABLE_ATCMD_UDPS_DBG_ERR)
#define	ATCMD_UDPS_ERR(fmt, ...)	\
    ATCMD_UDPS_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	ATCMD_UDPS_ERR(...)	do {} while (0)
#endif // (ENABLE_ATCMD_UDPS_DBG_ERR)

#if defined ( __SUPPORT_IPV6__ )
extern bool isIPv6Address(char * str, struct sockaddr_in6 * valid_ip);
#endif // __SUPPORT_IPV6__

#if (ATCMD_TRANSPORT_SDIO_W == 1)
#define ATCMD_UDP_TASK_NO_WDOG_PMGR
#endif

static void atcmd_udps_task_entry(void * param);

int atcmd_udps_init_context(atcmd_udps_context * ctx)
{
    ATCMD_UDPS_INFO("Start\r\n");
    memset(ctx, 0x00, sizeof(atcmd_udps_context));
    ctx->socket = -1;
    ctx->state = ATCMD_UDPS_STATE_TERMINATED;
    return 0;
}

int atcmd_udps_deinit_context(atcmd_udps_context * ctx)
{
    ATCMD_UDPS_INFO("Start\r\n");

    if (ctx->state != ATCMD_UDPS_STATE_TERMINATED)
    {
        ATCMD_UDPS_ERR("udp session is not terminated(%d)\r\n", ctx->state);
        return -1;
    }

    if (ctx->buffer)
    {
        ATCMD_UDPS_INFO("To free udp session's recv buffer\r\n");
        vPortFree(ctx->buffer);
        ctx->buffer = NULL;
    }

    if (ctx->socket > -1)
    {
        ATCMD_UDPS_INFO("To close udp session socket\r\n");
        close(ctx->socket);
        ctx->socket = -1;
    }

    if (ctx->event)
    {
        ATCMD_UDPS_INFO("To delete event\n");
        vEventGroupDelete(ctx->event);
        ctx->event = NULL;
    }

    atcmd_udps_init_context(ctx);
    return 0;
}

int atcmd_udps_init_config(const int cid, atcmd_udps_config * conf, atcmd_udps_sess_info * sess_info)
{
    ATCMD_UDPS_INFO("Start\r\n");
    conf->cid = cid;
    snprintf((char *)conf->task_name, (ATCMD_UDPS_MAX_TASK_NAME - 1), "%s_%d",
             ATCMD_UDPS_TASK_NAME, cid);
    conf->task_priority = ATCMD_UDPS_TASK_PRIORITY;
    conf->task_size = (ATCMD_UDPS_TASK_SIZE / 4);
    snprintf(conf->sock_name, (ATCMD_UDPS_MAX_SOCK_NAME - 1), "%s_%d",
             ATCMD_UDPS_SOCK_NAME, cid);
    conf->rx_buflen = ATCMD_UDPS_RECV_BUF_SIZE;
    conf->sess_info = sess_info;
    return 0;
}

int atcmd_udps_deinit_config(atcmd_udps_config * conf)
{
    ATCMD_UDPS_INFO("Start\r\n");
    memset(conf, 0x00, sizeof(atcmd_udps_config));
    return 0;
}

int atcmd_udps_set_at_ctrl(atcmd_udps_context * ctx, void * const p_at_ctrl)
{
    if (!ctx || ctx->p_at_ctrl)
    {
        return -1;
    }

    ctx->p_at_ctrl = p_at_ctrl;
    return 0;
}

int atcmd_udps_set_local_addr(atcmd_udps_config * p_conf, int ip_type, char * p_ip, int port)
{
    ATCMD_UDPS_INFO("Start\r\n");

    if (p_ip)
    {
        /* Not implemented yet. */
        ATCMD_UDPS_ERR("Not allowed to set local IP address\r\n");
        return -1;
    }

    /* Check range */
    if (port < ATCMD_UDPS_MIN_PORT || port > ATCMD_UDPS_MAX_PORT)
    {
        ATCMD_UDPS_ERR("Invalid port(%d)\n", port);
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
        ATCMD_UDPS_ERR("Invaild IP Type(%d)\n", ip_type);
        return -1;
    }

    p_conf->sess_info->local_port = port;
    p_conf->sess_info->ip_type = ip_type;

    return 0;
}

int atcmd_udps_set_peer_addr(atcmd_udps_config * p_conf, int ip_type, char * p_ip, int port)
{
    int ret = 0;
    struct addrinfo hints, * p_addr_list = NULL;
    char str_port[16] = {0x00, };
#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 valid_ipv6 = {0x00,};
#endif // __SUPPORT_IPV6__

    ATCMD_UDPS_INFO("Start\r\n");

    if (!p_ip)
    {
        ATCMD_UDPS_ERR("Invalid parameters\r\n");
        return -1;
    }

    /* Check range */
    if (port <= 0 || port > 0xFFFF)
    {
        ATCMD_UDPS_ERR("Invalid port(%d)\n", port);
        return -1;
    }

    memset(&hints, 0x00, sizeof(struct addrinfo));

    if (ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        if (!is_in_valid_ip_class(p_ip))
        {
            hints.ai_family = AF_INET;    //IPv4 only
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
            snprintf(str_port, sizeof(str_port), "%d", port);
            ret = getaddrinfo(p_ip, str_port, &hints, &p_addr_list);

            if ((ret != 0) || !p_addr_list)
            {
                printf("Failed to get address info(%d)\r\n", ret);
                return -1;
            }

            memcpy((struct sockaddr *)&p_conf->peer_addr, p_addr_list->ai_addr, sizeof(struct sockaddr));
            freeaddrinfo(p_addr_list);
        }
        else
        {
            p_conf->peer_addr.sin_addr.s_addr = inet_addr(p_ip);
        }

        p_conf->peer_addr.sin_family = AF_INET;
        p_conf->peer_addr.sin_port = htons(port);
        ATCMD_UDPS_INFO("Peer address(%ld.%ld.%ld.%ld:%d)\r\n",
                        (ntohl(p_conf->peer_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(p_conf->peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(p_conf->peer_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(p_conf->peer_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(p_conf->peer_addr.sin_port)));
#endif // __SUPPORT_IPV4__
    }
    else if (ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        if (!isIPv6Address(p_ip, &valid_ipv6))
        {
            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
            snprintf(str_port, sizeof(str_port), "%d", port);
            ret = getaddrinfo(p_ip, str_port, &hints, &p_addr_list);

            if ((ret != 0) || !p_addr_list)
            {
                printf("Failed to get address info(%d)\r\n", ret);
                return -1;
            }

            memcpy((struct sockaddr_in6 *)&p_conf->peer_addr6, p_addr_list->ai_addr, sizeof(struct sockaddr_in6));
            freeaddrinfo(p_addr_list);
        }
        else
        {
            //p_conf->peer_addr6.sin6_addr.s6_addr = inet_addr(p_ip);
            inet_pton(AF_INET6, p_ip, &(p_conf->peer_addr6.sin6_addr));
        }

        p_conf->peer_addr6.sin6_family = AF_INET6;
        p_conf->peer_addr6.sin6_port = htons(port);
        ATCMD_UDPS_INFO("Peer address(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d)\r\n",
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(p_conf->peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(p_conf->peer_addr6.sin6_port)));
#endif // __SUPPORT_IPV6__
    }

    bsp_safe_strcpy(p_conf->sess_info->peer_ipaddr, p_ip, ATCMD_NVR_NW_TR_PEER_IPADDR_LEN);
    p_conf->sess_info->peer_port = port;

    return 0;
}

int atcmd_udps_set_config(atcmd_udps_context * ctx, atcmd_udps_config * conf)
{
    ATCMD_UDPS_INFO("Start\r\n");

    if (strlen((const char *)(conf->task_name)) == 0)
    {
        ATCMD_UDPS_ERR("Invalid task name\r\n");
        return -1;
    }

    if (conf->task_priority == 0)
    {
        ATCMD_UDPS_ERR("Invalid task priority\r\n");
        return -1;
    }

    if (conf->task_size == 0)
    {
        ATCMD_UDPS_ERR("Invalid task size\r\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )

        if (conf->local_addr.sin_family != AF_INET)
        {
            ATCMD_UDPS_ERR("Invalid local address\r\n");
            return -1;
        }

        #endif // IPADDR_TYPE_V4
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )

        if (conf->local_addr6.sin6_family != AF_INET6)
        {
            ATCMD_UDPS_ERR("Invalid local address\r\n");
            return -1;
        }

        #endif // __SUPPORT_IPV6__
    }

    if (conf->rx_buflen == 0)
    {
        ATCMD_UDPS_ERR("Invalid recv buffer size\r\n");
        return -1;
    }

    if (strlen((const char *)(conf->sock_name)) == 0)
    {
        ATCMD_UDPS_ERR("Invalid socket name\n");
        return -1;
    }

    ctx->event = xEventGroupCreate();

    if (ctx->event == NULL)
    {
        ATCMD_UDPS_INFO("Failed to create event\n");
        return -1;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ATCMD_UDPS_INFO("* %-20s: %s(%d)\r\n" // task name
                        "* %-20s: %ld\r\n" // task priority
                        "* %-20s: %d\r\n" // task size
                        "* %-20s: %d\r\n" // rx buflen
                        "* %-20s: %ld.%ld.%ld.%ld:%d\r\n" // local ip address
                        "* %-20s: %ld.%ld.%ld.%ld:%d\r\n", // peer ip address
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
                        "Peer IP address",
                        (ntohl(conf->peer_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(conf->peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(conf->peer_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(conf->peer_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(conf->peer_addr.sin_port)));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ATCMD_UDPS_INFO("* %-20s: %s(%d)\r\n" // task name
                        "* %-20s: %ld\r\n" // task priority
                        "* %-20s: %d\r\n" // task size
                        "* %-20s: %d\r\n" // rx buflen
                        "* %-20s: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\r\n" // local ip address
                        "* %-20s: %lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d\r\n", // peer ip address
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
                        "Peer IP address",
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(conf->peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(conf->peer_addr6.sin6_port)));
        #endif // __SUPPORT_IPV6__
    }

    ctx->conf = conf;
    return 0;
}

int atcmd_udps_wait_for_ready(atcmd_udps_context * ctx)
{
    const int wait_time = portCONVERT_MS_2_TICKS(100); // 100 msec
    const int max_wait_cnt = 10;
    int wait_cnt = 0;
    int ret = 0;
    unsigned int events = 0x00;

    if (!ctx)
    {
        ATCMD_UDPS_ERR("Invalid parameter\n");
        return -1;
    }

    for (wait_cnt = 0 ; wait_cnt < max_wait_cnt ; wait_cnt++)
    {
        if (ctx->event)
        {
            events = xEventGroupWaitBits(ctx->event, ATCMD_UDPS_EVT_ANY,
                                         pdTRUE, pdFALSE, wait_time);

            if (events & ATCMD_UDPS_EVT_ACTIVE)
            {
                ATCMD_UDPS_INFO("Got ready event\n");
                break;
            }
            else if (events & ATCMD_UDPS_EVT_CLOSED)
            {
                ATCMD_UDPS_INFO("Got close event\n");
                return -1;
            }
        }
        else
        {
            if (ctx->state == ATCMD_UDPS_STATE_ACTIVE)
            {
                break;
            }
            else if (ctx->state == ATCMD_UDPS_STATE_TERMINATED)
            {
                ret = -1;
                break;
            }

            vTaskDelay(wait_time);
        }
    }

    return ret;
}

int atcmd_udps_start(atcmd_udps_context * ctx)
{
    int ret = 0;
    ATCMD_UDPS_INFO("Start\r\n");

    if (!ctx || !ctx->conf)
    {
        ATCMD_UDPS_ERR("Invalid parameters\r\n");
        return -1;
    }

    if (ctx->state != ATCMD_UDPS_STATE_TERMINATED)
    {
        ATCMD_UDPS_ERR("UDP session is not terminated(%d)\r\n", ctx->state);
        return -1;
    }

    ctx->state = ATCMD_UDPS_STATE_READY;
    ret = xTaskCreate(atcmd_udps_task_entry,
                      (const char *)(ctx->conf->task_name),
                      ctx->conf->task_size,
                      (void *)ctx,
                      ctx->conf->task_priority,
                      &ctx->task_handler);

    if (ret != pdPASS)
    {
        printf("Failed to create udp session task(%d)\r\n", ret);
        ctx->state = ATCMD_UDPS_STATE_TERMINATED;
        return -1;
    }

    return 0;
}

int atcmd_udps_stop(atcmd_udps_context * ctx)
{
    const int wait_time = portCONVERT_MS_2_TICKS(100);
    const int max_cnt = 10;
    int cnt = 0;
    unsigned int events = 0x00;
    ATCMD_UDPS_INFO("Start\n");

    if (ctx->state == ATCMD_UDPS_STATE_ACTIVE)
    {
        ATCMD_UDPS_INFO("Change udp session state from %d to %d\r\n",
                        ctx->state, ATCMD_UDPS_STATE_REQ_TERMINATE);
        ctx->state = ATCMD_UDPS_STATE_REQ_TERMINATE;

        for (cnt = 0 ; cnt < max_cnt ; cnt++)
        {
            if (ctx->event)
            {
                events = xEventGroupWaitBits(ctx->event, ATCMD_UDPS_EVT_CLOSED,
                                             pdTRUE, pdFALSE, wait_time);

                if (events & ATCMD_UDPS_EVT_CLOSED)
                {
                    ATCMD_UDPS_INFO("Closed udp session task\n");
                    break;
                }
            }
            else
            {
                if (ctx->state == ATCMD_UDPS_STATE_TERMINATED)
                {
                    ATCMD_UDPS_INFO("Closed udp session task\n");
                    break;
                }

                vTaskDelay(wait_time);
            }
        }
    }

    return ((ctx->state == ATCMD_UDPS_STATE_TERMINATED) ? 0 : -1);
}

int atcmd_udps_tx(atcmd_udps_context * ctx, char * data, unsigned int * data_len, char * ip, unsigned int port)
{
    int ret = 0;
    struct addrinfo hints, * p_addr_list = NULL;
    char str_port[16] = {0x00, };
    const int to_send = *data_len;
#if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in peer_addr;
#endif
#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 peer_addr6;
    struct sockaddr_in6 valid_ipv6;
    uint8_t zero_addr[16] = {0};
#endif

    ATCMD_UDPS_INFO("Start\r\n");

    memset(&hints, 0x00, sizeof(hints));

    if (ctx->conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        memset(&peer_addr, 0x00, sizeof(peer_addr));

        if ((ctx->state != ATCMD_UDPS_STATE_ACTIVE) || !(ctx->conf))
        {
            ATCMD_UDPS_ERR("Invalid parameter(data_len:%d, state:%d, ctx->conf:%p)\r\n",
                           to_send, ctx->state, ctx->conf);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if (ip && (strcmp(ip, "0") == 0))
        {
            ip = NULL;
        }

        if (ip && port)
        {
            ATCMD_UDPS_INFO("To find peer ip address(%s:%d)\r\n", ip, port);
            peer_addr.sin_family = AF_INET;
            peer_addr.sin_port = htons(port);

            if (!is_in_valid_ip_class(ip))
            {
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = IPPROTO_UDP;
                snprintf(str_port, sizeof(str_port), "%d", port);
                ret = getaddrinfo(ip, str_port, &hints, &p_addr_list);

                if ((ret != 0) || !p_addr_list)
                {
                    ATCMD_UDPS_ERR("Failed to get peer ip address(%d)\r\n", ret);
                    return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;;
                }

                /* Pick 1st address */
                memcpy((struct sockaddr *) &peer_addr, p_addr_list->ai_addr, sizeof(struct sockaddr));
                freeaddrinfo(p_addr_list);
                p_addr_list = NULL;
            }
            else
            {
                peer_addr.sin_addr.s_addr = inet_addr(ip);
            }
        }
        else
        {
            ATCMD_UDPS_INFO("To use peer ip address of conf\r\n");
            memcpy(&peer_addr, &ctx->conf->peer_addr, sizeof(struct sockaddr_in));
        }

        ATCMD_UDPS_INFO("Peer ip address(%ld.%ld.%ld.%ld:%d), len(%d)\r\n",
                        (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xFF,
                        (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                        (ntohl(peer_addr.sin_addr.s_addr) >>  8) & 0xFF,
                        (ntohl(peer_addr.sin_addr.s_addr)      ) & 0xFF,
                        (ntohs(peer_addr.sin_port)), to_send);

        if ((peer_addr.sin_family == AF_UNSPEC)
                || (peer_addr.sin_addr.s_addr == 0)
                || (peer_addr.sin_port == 0))
        {
            printf("Invalid peer ip address\r\n");
            return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;;
        }

        ret = sendto(ctx->socket, data, to_send, 0, (struct sockaddr *) &peer_addr, sizeof(peer_addr));
#endif // __SUPPORT_IPV4__
    }
    else if (ctx->conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        memset(&peer_addr6, 0x00, sizeof(peer_addr6));
        memset(&valid_ipv6, 0x00, sizeof(struct sockaddr_in6));

        if ((ctx->state != ATCMD_UDPS_STATE_ACTIVE) || !(ctx->conf))
        {
            ATCMD_UDPS_ERR("Invalid parameter(data_len:%d, state:%d, ctx->conf:%p)\r\n",
                           to_send, ctx->state, ctx->conf);
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if (ip && (strcmp(ip, "0") == 0))
        {
            ip = NULL;
        }

        if (ip && port)
        {
            ATCMD_UDPS_INFO("To find peer ip address(%s:%d)\r\n", ip, port);
            peer_addr6.sin6_family = AF_INET6;
            peer_addr6.sin6_port = htons(port);

            if (!isIPv6Address(ip, &valid_ipv6))
            {
                hints.ai_family = AF_INET6;
                hints.ai_socktype = SOCK_DGRAM;
                hints.ai_protocol = IPPROTO_UDP;
                snprintf(str_port, sizeof(str_port), "%d", port);
                ret = getaddrinfo(ip, str_port, &hints, &p_addr_list);

                if ((ret != 0) || !p_addr_list)
                {
                    ATCMD_UDPS_ERR("Failed to get peer ip address(%d)\r\n", ret);
                    return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;;
                }

                /* Pick 1st address */
                memcpy((struct sockaddr_in6 *) &peer_addr6, p_addr_list->ai_addr, sizeof(struct sockaddr_in6));
                freeaddrinfo(p_addr_list);
                p_addr_list = NULL;
            }
            else
            {
                inet_pton(AF_INET6, ip, &(peer_addr6.sin6_addr));
            }
        }
        else
        {
            ATCMD_UDPS_INFO("To use peer ip address of conf\r\n");
            memcpy(&peer_addr6, &ctx->conf->peer_addr6, sizeof(struct sockaddr_in6));
        }

        ATCMD_UDPS_INFO("Peer ip address(%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d), len(%d)\r\n",
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                        ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                        (ntohs(peer_addr6.sin6_port)), to_send);

        if ((peer_addr6.sin6_family == AF_UNSPEC)
                || !memcmp(peer_addr6.sin6_addr.s6_addr, zero_addr, 16)
                || (peer_addr6.sin6_port == 0))
        {
            ATCMD_UDPS_ERR("Invalid peer ip address\r\n");
            return FSP_ERR_AT_CMD_ERR_IP_ADDRESS;;
        }

        ret = sendto(ctx->socket, data, to_send, 0, (struct sockaddr *) &peer_addr6, sizeof(struct sockaddr_in6));
#endif // __SUPPORT_IPV6__
    }

    if (ret != to_send)
    {
        ATCMD_UDPS_ERR("udp socket send fail (%d/%d/%d)\n", to_send, ret, errno);

        if (ret < 0)
        {
            *data_len = 0;
        }
        else
        {
            *data_len = ret;
        }

        return FSP_ERR_AT_CMD_ERR_DATA_TX;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static void atcmd_udps_task_entry(void * param)
{
    int ret = 0;
#if CFG_PMGR
    int res = 0;
#endif
    atcmd_udps_context * ctx = (atcmd_udps_context *)param;
    const atcmd_udps_config * conf = ctx->conf;
#if CFG_PMGR
    unsigned int local_port = 0;
    uint32_t     peer_ip4 = 0;
    uint32_t     peer_ip6[4] = {0, };
    unsigned int peer_port = 0; 
#if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in local_addr_tmp;
#endif // __SUPPORT_IPV4__
#if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 local_addr6_tmp;
#endif // __SUPPORT_IPV6__
#endif /* CFG_PMGR */
    #if defined ( __SUPPORT_IPV4__ )
    struct sockaddr_in peer_addr;
    #endif // __SUPPORT_IPV4__
    #if defined ( __SUPPORT_IPV6__ )
    struct sockaddr_in6 peer_addr6;
    #endif // __SUPPORT_IPV6__
    socklen_t addr_len = 0;
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
    uint8_t zero_addr[16] = {0};
#if CFG_PMGR
    const char * dpm_name = (const char *)conf->task_name;
    const char * sock_name = (const char *)conf->sock_name;
#endif /* CFG_PMGR */
    sockopt_timeout.tv_sec = 0;
    sockopt_timeout.tv_usec = ATCMD_UDPS_RECV_TIMEOUT * 1000;

#ifndef ATCMD_UDP_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                           g_wifi_cfg.p_watchdog_service->p_cfg,
                                                           &task_wdog_id);
#endif
#endif

    if (!ctx)
    {
        printf("[%s] Invalid param\r\n", __func__);
        goto atcmd_udps_term;
    }

    ctx->buffer = pvPortMalloc(conf->rx_buflen);

    if (!ctx->buffer)
    {
        printf("[%s] No FREE memory space for rx buffer(%d)\r\n", __func__, conf->rx_buflen);
        goto atcmd_udps_term;
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        #if CFG_PMGR
        ctx->socket = socket_dpm((char *)sock_name, AF_INET, SOCK_DGRAM, 0);
        #else
        ctx->socket = socket(AF_INET, SOCK_DGRAM, 0);
        #endif /* CFG_PMGR */
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        #if CFG_PMGR
        ctx->socket = socket_dpm((char *)sock_name, AF_INET6, SOCK_DGRAM, 0);
        #else
        ctx->socket = socket(AF_INET6, SOCK_DGRAM, 0);
        #endif /* CFG_PMGR */
        #endif // __SUPPORT_IPV6__
    }

    if (ctx->socket < 0)
    {
        printf("Failed to create udp socket(%d:%d)\r\n", ctx->socket, errno);
        goto atcmd_udps_term;
    }

    ATCMD_UDPS_INFO("UDP Session: socket descriptor(%d)\r\n", ctx->socket);
    ret = setsockopt(ctx->socket, SOL_SOCKET, SO_RCVTIMEO,
                     (const void *)&sockopt_timeout, sizeof(sockopt_timeout));

    if (ret != 0)
    {
        printf("Failed to set socket option - SO_RCVTIMEOUT(%d:%d)\r\n", ret, errno);
    }

    if (conf->ip_type == IPADDR_TYPE_V4)
    {
        #if defined ( __SUPPORT_IPV4__ )
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr, sizeof(struct sockaddr_in));
        #endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
        #if defined ( __SUPPORT_IPV6__ )
        ret = bind(ctx->socket, (struct sockaddr *)&conf->local_addr6, sizeof(struct sockaddr_in6));
        #endif // __SUPPORT_IPV6__
    }

    if (ret != 0)
    {
        printf("Failed to bind udp socket(%d:%d)\r\n", ret, errno);
        goto atcmd_udps_term;
    }

    if (conf->ip_type == IPADDR_TYPE_V4 &&
        conf->peer_addr.sin_port != 0   &&
        conf->peer_addr.sin_addr.s_addr != 0) 
    {
        ret = connect(ctx->socket, (struct sockaddr *)&conf->peer_addr, sizeof(struct sockaddr_in));
    } else if (conf->ip_type == IPADDR_TYPE_V6 &&
        conf->peer_addr6.sin6_port != 0   &&
        memcmp(conf->peer_addr6.sin6_addr.un.u32_addr, zero_addr, 16)) 
    {
        ret = connect(ctx->socket, (struct sockaddr *)&conf->peer_addr6, sizeof(struct sockaddr_in));
    }

    if (ret < 0) 
    {
        printf("Failed to connect udp socket(%d:%d)\r\n", ret, errno);
        goto atcmd_udps_term;
    }

#if CFG_PMGR
    /* Get local addr info bound */
    if (conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        local_addr_tmp = conf->local_addr;
        addr_len = sizeof(struct sockaddr_in);
        getsockname(ctx->socket, (struct sockaddr *)&local_addr_tmp, &addr_len);
#endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        local_addr6_tmp = conf->local_addr6;
        addr_len = sizeof(struct sockaddr_in6);
        getsockname(ctx->socket, (struct sockaddr *)&local_addr6_tmp, &addr_len);
#endif // __SUPPORT_IPV6__
    }

    /* Get local port */
    if (conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined ( __SUPPORT_IPV4__ )
        local_port = ntohs(local_addr_tmp.sin_port);
        peer_port =  ntohs(conf->peer_addr.sin_port);
        peer_ip4 =   ntohl(conf->peer_addr.sin_addr.s_addr);
#endif // __SUPPORT_IPV4__
    }
    else if (conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined ( __SUPPORT_IPV6__ )
        local_port = ntohs(local_addr6_tmp.sin6_port);
        peer_port = ntohs(conf->peer_addr6.sin6_port);
        peer_ip6[0] = conf->peer_addr6.sin6_addr.un.u32_addr[0];
        peer_ip6[1] = conf->peer_addr6.sin6_addr.un.u32_addr[1];
        peer_ip6[2] = conf->peer_addr6.sin6_addr.un.u32_addr[2];
        peer_ip6[3] = conf->peer_addr6.sin6_addr.un.u32_addr[3];
#endif // __SUPPORT_IPV6__
    }

    if ((conf->ip_type == IPADDR_TYPE_V4 || conf->ip_type == IPADDR_TYPE_V6)
                                            && get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
    {
        ATCMD_UDPS_INFO("Reg - DPM Name:%s(%d), IP type(%d), Local port(%d), Peer port (%d), Peer IP (%s) \n",
                      dpm_name, strlen(dpm_name), 
                      conf->ip_type, local_port, peer_port, inet_ntoa(peer_ip4));

        conf->sess_info->local_port = local_port;

        RM_PMGR_W_dpm_job_name_set((char *)dpm_name, local_port);
        RM_WIFI_dpm_udp_port_filter_set(local_port);

        if ((peer_ip4 != 0 || memcmp(peer_ip6, zero_addr, 16)) && peer_port != 0) 
        {
            res = RM_WIFI_dpm_udp_port_hole_punch_set(1, 
                                                        peer_ip4,
                                                        (uint32_t*)peer_ip6,
                                                        (unsigned short)local_port,
                                                        (unsigned short)peer_port);
            if (res == pdFAIL) 
            {
                ATCMD_UDPS_ERR("Adding hole punch config failed! \n");
            }
        }
    }
#endif /* CFG_PMGR */

    ctx->state = ATCMD_UDPS_STATE_ACTIVE;
#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_done((char *)dpm_name);
    RM_PMGR_W_dpm_rcv_ready_set((char *)dpm_name);
#endif /* CFG_PMGR */

    if (ctx->event)
    {
        xEventGroupSetBits(ctx->event, ATCMD_UDPS_EVT_ACTIVE);
    }

    while (ctx->state == ATCMD_UDPS_STATE_ACTIVE)
    {
#ifndef ATCMD_UDP_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         g_wifi_cfg.p_watchdog_service->p_cfg,
                                                         task_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         task_wdog_id,
                                                         ATCMD_UDPS_WDOG_LATENCY);
#endif
#endif

        if (conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            memset(&peer_addr, 0x00, sizeof(struct sockaddr_in));
            #endif // __SUPPORT_IPV4__
        }
        else if (conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            memset(&peer_addr6, 0x00, sizeof(struct sockaddr_in6));
            #endif // __SUPPORT_IPV6__
        }

        tot_len = 0;
        act_hdr_len = 0;
        act_payload_len = 0;
        hdr = ctx->buffer;
        hdr_len = ATCMD_UDPS_RECV_HDR_SIZE;
        payload = ctx->buffer + hdr_len;
        payload_len = ATCMD_UDPS_RECV_PAYLOAD_SIZE;

        if (conf->ip_type == IPADDR_TYPE_V4)
        {
            #if defined ( __SUPPORT_IPV4__ )
            addr_len = sizeof(struct sockaddr_in);
            //ATCMD_UDPS_INFO("Waiting for udp data\r\n");
            ret = recvfrom(ctx->socket, payload, payload_len, 0,
                           (struct sockaddr *)&peer_addr, (socklen_t *)&addr_len);
            #endif // __SUPPORT_IPV4__
        }
        else if (conf->ip_type == IPADDR_TYPE_V6)
        {
            #if defined ( __SUPPORT_IPV6__ )
            addr_len = sizeof(struct sockaddr_in6);
            //ATCMD_UDPS_INFO("Waiting for udp data\r\n");
            ret = recvfrom(ctx->socket, payload, payload_len, 0,
                           (struct sockaddr *)&peer_addr6, (socklen_t *)&addr_len);
            #endif // __SUPPORT_IPV6__
        }

#if CFG_PMGR
        RM_PMGR_W_dpm_sleep_ready_clear((char *)dpm_name);
#endif /* CFG_PMGR */

        if (ret > 0)
        {
            act_payload_len = ret;

            if (conf->ip_type == IPADDR_TYPE_V4)
            {
                #if defined ( __SUPPORT_IPV4__ )
                ATCMD_UDPS_INFO("Recv(ip:%ld.%ld.%ld.%ld:%d/len:%d)\r\n",
                                (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xFF,
                                (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                                (ntohl(peer_addr.sin_addr.s_addr) >>  8) & 0xFF,
                                (ntohl(peer_addr.sin_addr.s_addr)      ) & 0xFF,
                                (ntohs(peer_addr.sin_port)), act_payload_len);
                act_hdr_len = snprintf((char *)hdr, hdr_len,
                                       "\r\n" ATCMD_UDP_DATA_RX_PREFIX ":%d,%ld.%ld.%ld.%ld,%u,%d,", cid,
                                       ((ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xFF),
                                       ((ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xFF),
                                       ((ntohl(peer_addr.sin_addr.s_addr) >>  8) & 0xFF),
                                       ((ntohl(peer_addr.sin_addr.s_addr)      ) & 0xFF),
                                       ntohs(peer_addr.sin_port), act_payload_len);
                #endif // __SUPPORT_IPV4__
            }
            else if (conf->ip_type == IPADDR_TYPE_V6)
            {
                #if defined ( __SUPPORT_IPV6__ )
                ATCMD_UDPS_INFO("Recv(ip:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx:%d/len:%d)\r\n",
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                (ntohs(peer_addr6.sin6_port)), act_payload_len);
                act_hdr_len = snprintf((char *)hdr, hdr_len,
                                       "\r\n" ATCMD_UDP_DATA_RX_PREFIX ":%d,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%u,%d,", cid,
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                       ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                       ntohs(peer_addr6.sin6_port), act_payload_len);
                #endif // __SUPPORT_IPV6__
            }

            tot_len = act_hdr_len;

            if (!memmove(payload - act_hdr_len, hdr, act_hdr_len))
            {
                ATCMD_UDPS_ERR("Failed to copy received data(%d)\n", act_payload_len);
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
                RM_ATCMD_W_CORE_Write((atcmd_w_ctrl_t * const)ctx->p_at_ctrl, (uint8_t *)hdr, tot_len);
            }
        }
#if CFG_PMGR
        if (!RM_PMGR_W_socket_rx_data_is_remaining(ctx->socket))
        {
            RM_PMGR_W_dpm_sleep_ready_set((char *)dpm_name);
        }
#endif /* CFG_PMGR */
    }

atcmd_udps_term:
    close(ctx->socket);
    ctx->socket = -1;

    if (ctx->buffer)
    {
        vPortFree(ctx->buffer);
        ctx->buffer = NULL;
    }

    ctx->buffer_len = 0;
#if CFG_PMGR
    RM_WIFI_dpm_udp_port_filter_delete(local_port);
    RM_PMGR_W_dpm_job_name_clear((char *)dpm_name);
    ATCMD_UDPS_INFO("Unreg - DPM Name:%s(%d), Local port(%d)\n",
                    dpm_name, strlen(dpm_name), local_port);
#endif /* CFG_PMGR */
    ctx->state = ATCMD_UDPS_STATE_TERMINATED;

    if (ctx->event)
    {
        xEventGroupSetBits(ctx->event, ATCMD_UDPS_EVT_CLOSED);
    }

    ATCMD_UDPS_INFO("End of task(state:%d)\r\n", ctx->state);

#ifndef ATCMD_UDP_TASK_NO_WDOG_PMGR
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif
#endif

    ctx->task_handler = NULL;
    vTaskDelete(NULL);
    return ;
}
#endif /* CFG_WIFI */

