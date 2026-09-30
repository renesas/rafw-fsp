/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_tls_client.h"
#include "rm_atcmd_w_core.h"
#include "lwip/dns.h"
#include "rm_atcmd_w_core_socket_internal.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "rm_wifi.h"

#undef  ENABLE_ATCMD_TLSC_DBG_INFO
#undef  ENABLE_ATCMD_TLSC_DBG_ERR

#define ATCMD_TLSC_DBG	printf

#if defined (ENABLE_ATCMD_TLSC_DBG_INFO)
#define ATCMD_TLSC_INFO(fmt, ...)   \
    ATCMD_TLSC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_TLSC_INFO(...)    do {} while (0)
#endif  // (ENABLE_ATCMD_TLSC_DBG_INFO)

#if defined (ENABLE_ATCMD_TLSC_DBG_ERR)
#define ATCMD_TLSC_ERR(fmt, ...)    \
    ATCMD_TLSC_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_TLSC_ERR(...) do {} while (0)
#endif // (ENABLE_ATCMD_TLSC_DBG_ERR)

void * (*atcmd_tlsc_malloc)(size_t n) = pvPortMalloc;
void (*atcmd_tlsc_free)(void * ptr) = vPortFree;

static unsigned int g_atcmd_tlsc_dns_cb_done = pdFALSE;
static err_t g_atcmd_tlsc_dns_cb_result = ERR_TIMEOUT;

void atcmd_tlsc_set_malloc_free(void * (*malloc_func)(size_t), void (*free_func)(void *))
{
    atcmd_tlsc_malloc = malloc_func;
    atcmd_tlsc_free = free_func;

    return ;
}

int atcmd_tlsc_send_cb(void * p_ctx, const unsigned char * p_buf, size_t len)
{
    mbedtls_net_context * p_net_ctx = (mbedtls_net_context *) p_ctx;
    const int timeout = ATCMD_TLSC_DATA_TX_WAIT_TIME;
    fd_set writefds;
    struct timeval tv;
    int ret = 0;

    FD_ZERO(&writefds);
    FD_SET(p_net_ctx->fd, &writefds);

    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    ret = select((p_net_ctx->fd + 1), NULL, &writefds, NULL, &tv);
    if (ret == 0)
    {
        ATCMD_TLSC_ERR("Failed to select writefds(%d/%d)\n", ret, errno);
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    else if (ret < 0)
    {
        ATCMD_TLSC_ERR("Failed to select writefds(%d/%d)\n", ret, errno);

        if (errno == EINTR)
        {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    ret = send(p_net_ctx->fd, p_buf, len, 0);
    if (ret < 0)
    {
        ATCMD_TLSC_ERR("Failed to send data(%d/%d)\n", ret, errno);

        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    ATCMD_TLSC_INFO("Data length(%d)\n", ret);

    return ret;
}

int atcmd_tlsc_init_context(atcmd_tlsc_context * ctx)
{
    ATCMD_TLSC_INFO("Start\n");

    memset(ctx, 0x00, sizeof(atcmd_tlsc_context));

    ctx->state = ATCMD_TLSC_STATE_TERMINATED;

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_deinit_context(atcmd_tlsc_context * ctx)
{
    ATCMD_TLSC_INFO("Start\n");

    if (ctx->state != ATCMD_TLSC_STATE_TERMINATED)
    {
        ATCMD_TLSC_ERR("tls client is not terminated(%d)\r\n", ctx->state);
        return RRQ_APP_NOT_CLOSED;
    }

    if (ctx->task_handler)
    {
        ATCMD_TLSC_INFO("To delete tls client task(%s:%d)\r\n",
                        ctx->task_name, strlen((const char *)ctx->task_name));
        vTaskDelete(ctx->task_handler);
    }

    if (ctx->recv_buf)
    {
        ATCMD_TLSC_INFO("To free tls client's recv buffer\r\n");
        atcmd_tlsc_free(ctx->recv_buf);
    }

    atcmd_tlsc_init_context(ctx);

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_set_at_ctrl(atcmd_tlsc_context * ctx, void * const p_at_ctrl)
{
    if (!ctx || ctx->p_at_ctrl)
    {
        return RRQ_APP_INVALID_PARAMETERS;
    }

    ctx->p_at_ctrl = p_at_ctrl;

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_setup_config(atcmd_tlsc_context * ctx, atcmd_tlsc_config * conf)
{
    if (!ctx || !conf)
    {
        ATCMD_TLSC_ERR("Invalid context\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (ctx->state != ATCMD_TLSC_STATE_TERMINATED)
    {
        ATCMD_TLSC_ERR("TLS Client state is not terminated(%d)\n", ctx->state);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    if (!conf->task_priority)
    {
        conf->task_priority = ATCMD_TLSC_TASK_PRIORITY;
    }

    if (!conf->task_size)
    {
        conf->task_size = ATCMD_TLSC_TASK_SIZE;
    }

    if (!conf->recv_buflen)
    {
        conf->recv_buflen = ATCMD_TLSC_DATA_BUF_SIZE;
    }

    ctx->conf = conf;

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_set_incoming_buflen(atcmd_tlsc_config * conf, unsigned int buflen)
{
    conf->incoming_buflen = buflen;
    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_set_outgoing_buflen(atcmd_tlsc_config * conf, unsigned int buflen)
{
    conf->outgoing_buflen = buflen;
    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_set_hostname(atcmd_tlsc_config * conf, char * hostname)
{
    if (strlen(hostname) >= ATCMD_TLSC_MAX_HOSTNAME)
    {
        ATCMD_TLSC_ERR("over hostname length(%d)\n", ATCMD_TLSC_MAX_HOSTNAME);
        return RRQ_APP_INVALID_PARAMETERS;
    }

    bsp_safe_strcpy(conf->hostname, hostname, ATCMD_TLSC_MAX_HOSTNAME);

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_run(atcmd_tlsc_context * ctx, int id)
{
    int status = 0;

    if (!ctx || !ctx->conf)
    {
        ATCMD_TLSC_ERR("Invalid context\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (ctx->state != ATCMD_TLSC_STATE_TERMINATED)
    {
        ATCMD_TLSC_ERR("TLS client is not terminated(%d)\n", ctx->state);
        return RRQ_APP_NOT_CLOSED;
    }

    ctx->cid = id;

    snprintf((char *)(ctx->task_name), sizeof(ctx->task_name), "%s%d",
             ATCMD_TLSC_PREFIX_TASK_NAME, id);

    snprintf(ctx->socket_name, sizeof(ctx->socket_name), "%s%d",
             ATCMD_TLSC_PREFIX_SOCKET_NAME, id);

    snprintf(ctx->tls_name, sizeof(ctx->tls_name), "%s%d",
             ATCMD_TLSC_PREFIX_TLS_NAME, id);

    status = xTaskCreate(atcmd_tlsc_entry_func,
                         (const char *)(ctx->task_name),
                         ctx->conf->task_size,
                         (void *)ctx,
                         ctx->conf->task_priority,
                         &ctx->task_handler);

    if (status != pdPASS)
    {
        ATCMD_TLSC_ERR("Failed to create tcp client task(%d)\n", status);
        return RRQ_APP_NOT_CREATED;
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_stop(atcmd_tlsc_context * ctx, unsigned int wait_option)
{
    unsigned int wait_time = 0;

    if (!ctx)
    {
        ATCMD_TLSC_ERR("Invalid context\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (ctx->state == ATCMD_TLSC_STATE_CONNECTED)
    {
        ATCMD_TLSC_INFO("Change tls client state(%s) from %d to %d\n",
                        ctx->task_name, ctx->state, ATCMD_TLSC_STATE_REQ_TERMINATE);

        ctx->state = ATCMD_TLSC_STATE_REQ_TERMINATE;

        do
        {
            if (ctx->state == ATCMD_TLSC_STATE_TERMINATED)
            {
                return RRQ_APP_SUCCESS;
            }

            ATCMD_TLSC_INFO("sleep_time(%d), wait_time(%d), max(%d)\n",
                            ATCMD_TLSC_SLEEP_TIMEOUT, wait_time, wait_option);

            vTaskDelay(portCONVERT_MS_2_TICKS(ATCMD_TLSC_SLEEP_TIMEOUT * 10));

            wait_time += ATCMD_TLSC_SLEEP_TIMEOUT;
        }
        while (wait_time < wait_option);
    }

    return ((ctx->state == ATCMD_TLSC_STATE_TERMINATED) ? RRQ_APP_SUCCESS : RRQ_APP_NOT_SUCCESSFUL);
}

int atcmd_tlsc_write_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t * data_len)
{
    int status = RRQ_APP_SUCCESS;

    if (ctx->state == ATCMD_TLSC_STATE_CONNECTED)
    {
        status = atcmd_tlsc_send_data(ctx, data, data_len);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to write data(0x%x/%d)\n", -status, *data_len);
            return status;
        }
    }
    else
    {
        status = RRQ_APP_NOT_CREATED;
    }

    return status;
}

int atcmd_tlsc_init_socket(atcmd_tlsc_context * ctx)
{
#if CFG_PMGR
    int status = 0;
#endif /* CFG_PMGR */

    ATCMD_TLSC_INFO("Socket Name: %s(%d)\n", ctx->socket_name, strlen(ctx->socket_name));

    mbedtls_net_init(&ctx->net_ctx);
#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        status = mbedtls_net_set_name(&ctx->net_ctx, ctx->socket_name, strlen(ctx->socket_name) + 1, pdTRUE);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to init net_ctx(0x%x)\n", -status);
            return RRQ_APP_NOT_CREATED;
        }
    }
#endif /* CFG_PMGR */

    ctx->bind_port = 0;

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_get_local_port(atcmd_tlsc_context * ctx, unsigned int * port)
{
    int status = 0;
    struct sockaddr_storage sock;
    unsigned int sock_len = sizeof(struct sockaddr_storage);
    unsigned int local_port = 0;

    *port = 0;

    ATCMD_TLSC_INFO("ctx->bind_port(%d), ctx->conf->local_port(%d)\n",
                    ctx->bind_port, ctx->conf->local_port);

    if (ctx->bind_port)
    {
        *port = ctx->bind_port;
    }
    else if (ctx->conf->local_port)
    {
        *port = ctx->conf->local_port;
    }
    else
    {
        status = getsockname(ctx->net_ctx.fd, (struct sockaddr *)&sock, (socklen_t *)&sock_len);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to get local information(%d)\n", status);
            return RRQ_APP_NOT_SUCCESSFUL;
        }

        #if defined(__SUPPORT_IPV4__)

        if (sock.ss_family == AF_INET)
        {
            local_port = ntohs(((struct sockaddr_in *)&sock)->sin_port);
        }
        else
        #endif // __SUPPORT_IPV4__
        #if defined(__SUPPORT_IPV6__)
            if (sock.ss_family == AF_INET6)
            {
                local_port = ntohs(((struct sockaddr_in6 *)&sock)->sin6_port);
            }

        #endif // __SUPPORT_IPV6__

        if (local_port)
        {
            *port = local_port;
        }
        else
        {
            return RRQ_APP_INVALID_PORT;
        }
    }

    ATCMD_TLSC_INFO("local port(fd:%d,%d)\n", ctx->net_ctx.fd, *port);

    return RRQ_APP_SUCCESS;
}


static void atcmd_tlsc_dns_callback(const char * p_name, const ip_addr_t * p_addr, void * p_arg)
{
    if (p_addr)
    {
        g_atcmd_tlsc_dns_cb_result = ERR_OK;
    }
    else
    {
        g_atcmd_tlsc_dns_cb_result = ERR_VAL;
    }

    g_atcmd_tlsc_dns_cb_done = pdTRUE;

    return ;
}

static err_t atcmd_tlsc_get_hostbyname(const char * p_host, unsigned int timeout)
{
    const unsigned int max_wait_time = timeout;
    unsigned int sleep_time = 100;
    unsigned int wait_time = 0;

    ip_addr_t addr = {0x00,};

    g_atcmd_tlsc_dns_cb_done = pdFALSE;
    g_atcmd_tlsc_dns_cb_result = ERR_TIMEOUT;

    err_t err = dns_gethostbyname(p_host, &addr, atcmd_tlsc_dns_callback, NULL);
    if (err == ERR_OK)
    {
        return ERR_OK;
    }
    else if (err == ERR_INPROGRESS)
    {
        while (!g_atcmd_tlsc_dns_cb_done)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(sleep_time));

            wait_time += sleep_time;

            if (wait_time > max_wait_time)
            {
                ATCMD_TLSC_ERR("Failed to get host by name\n");
                return ERR_TIMEOUT;
            }
        }

        return g_atcmd_tlsc_dns_cb_result;
    }
    else
    {
        ATCMD_TLSC_ERR("Failed to get host by name(%d)\n", err);
        return err;
    }
}

int atcmd_tlsc_connect_socket(atcmd_tlsc_context * ctx, unsigned int wait_option)
{
    int status = RRQ_APP_SUCCESS;
    const atcmd_tlsc_config * p_conf = ctx->conf;
    unsigned int local_port = 0;
    char local_port_str[ATCMD_TLSC_MAX_PORTSTRLEN] = {0x00,};

    ATCMD_TLSC_INFO("Local port(%d), TLS Server(%s:%s)\n",
                    p_conf->local_port, p_conf->svr_addr, p_conf->svr_port);

    /* Get TLS server's IP address in timeout */
    err_t err = atcmd_tlsc_get_hostbyname(p_conf->svr_addr, ATCMD_TLSC_HOST_TIMEOUT);
    if (err != ERR_OK)
    {
        return MBEDTLS_ERR_NET_UNKNOWN_HOST;
    }

    if (p_conf->local_port)
    {
        /* Convert local port */
        snprintf(local_port_str, sizeof(local_port_str), "%d", p_conf->local_port);

        /* Connect to server */
        status = mbedtls_net_connect_with_bind(&ctx->net_ctx,
                                               p_conf->svr_addr, p_conf->svr_port, MBEDTLS_NET_PROTO_TCP,
                                               NULL, local_port_str, wait_option);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to connect to server(0x%x)\n", -status);
            return status;
        }
    }
    else
    {
        status = mbedtls_net_connect_with_bind(&ctx->net_ctx,
                                               p_conf->svr_addr, p_conf->svr_port, MBEDTLS_NET_PROTO_TCP,
                                               NULL, NULL, wait_option);

        /* Connect to server */
        if (status)
        {
            ATCMD_TLSC_ERR("Failed to connect to server(0x%x)\n", -status);
            return status;
        }
    }

    status = atcmd_tlsc_get_local_port(ctx, &local_port);

    if (status == RRQ_APP_SUCCESS)
    {
        ATCMD_TLSC_INFO("set bind port & filter(%d)\n", local_port);
        ctx->bind_port = local_port;
#if CFG_PMGR
        RM_WIFI_dpm_tcp_port_filter_set(local_port);
#endif /* CFG_PMGR */
    }
    else
    {
        ATCMD_TLSC_ERR("Failed to get local port\n");
        return status;
    }

    ATCMD_TLSC_INFO("Assigned IP address(fd:%d, %s:%d)\n",
                    ctx->net_ctx.fd, ctx->conf->svr_addr, ctx->bind_port);

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_disconnect_socket(atcmd_tlsc_context * ctx)
{
    int status = RRQ_APP_SUCCESS;
    unsigned int local_port = 0;

    ATCMD_TLSC_INFO("Socket Name:%s(%d)\n", ctx->socket_name, strlen(ctx->socket_name));

    status = atcmd_tlsc_get_local_port(ctx, &local_port);

    if (status == RRQ_APP_SUCCESS)
    {
        ATCMD_TLSC_INFO("del bind port & filter(%d)\n", local_port);
        ctx->bind_port = 0;
#if CFG_PMGR
        RM_WIFI_dpm_tcp_port_delete(local_port);
#endif /* CFG_PMGR */
    }

    mbedtls_net_free(&ctx->net_ctx);

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_send_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t * data_len)
{
    int ret = 0;
    size_t total_sent = 0;
    const int to_send = *data_len;
    size_t frags = 0;
    int sent_cnt = 0;

    ATCMD_TLSC_ERR("data_len:%d\n", to_send);

    do
    {
        while ((ret = mbedtls_ssl_write(ctx->ssl_ctx, (data + total_sent), (to_send - total_sent))) <= 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ
                && ret != MBEDTLS_ERR_SSL_WANT_WRITE
                && ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
            {
                ATCMD_TLSC_ERR("Failed to send data(0x%x)\n", -ret);
                return RRQ_APP_NOT_SUCCESSFUL;
            }

            /* Incerased & checked retry count */
            if (++sent_cnt >= ATCMD_TLSC_DATA_TX_WAIT_CNT)
            {
                ATCMD_TLSC_ERR("Sent partial data(%d,%d/%d/%d)\n", sent_cnt, to_send, total_sent, frags);
                goto end;
            }
        }

        frags++;
        total_sent += ret;

        ATCMD_TLSC_INFO("data_len:%d, total_sent:%d, ret:%d, flags:%d\n", to_send, total_sent, ret, frags);

    } while (total_sent < to_send);

end:

    ATCMD_TLSC_INFO("%d bytes total_sent in %d fragments\n", total_sent, frags);

#if CFG_PMGR
    atcmd_tlsc_store_ssl(ctx);
#endif /* CFG_PMGR */

    if (total_sent != to_send)
    {
        *data_len = total_sent;
        return RRQ_APP_SSL_PARTIAL_TX;
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_recv(atcmd_tlsc_context * ctx, unsigned char * out, size_t outlen,
                    unsigned int wait_option)
{
    int status = RRQ_APP_SUCCESS;

    mbedtls_ssl_conf_read_timeout(ctx->ssl_conf, wait_option);

    status = mbedtls_ssl_read(ctx->ssl_ctx, out, outlen);

    if (status < 0)
    {
        if ((status == MBEDTLS_ERR_SSL_WANT_WRITE) || (status == MBEDTLS_ERR_SSL_TIMEOUT))
        {
            status = RRQ_APP_NO_PACKET;
        }
        else if (status == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
        {
            status = RRQ_APP_NOT_CONNECTED;
        }
        else if (status == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
        {
            status = RRQ_APP_NO_PACKET;
        }
        else
        {
            ATCMD_TLSC_ERR("Failed to read ssl packet(0x%x)\n", -status);
            status = RRQ_APP_NOT_SUCCESSFUL;
        }
    }
#if CFG_PMGR
    atcmd_tlsc_store_ssl(ctx);
#endif /* CFG_PMGR */

    return status;
}

//related with ssl
int atcmd_tls_rsa_decrypt_func(void * ctx, size_t * olen,
                               const unsigned char * input, unsigned char * output,
                               size_t output_max_len)
{
    return mbedtls_rsa_pkcs1_decrypt((mbedtls_rsa_context *)ctx, NULL, NULL,
                                     olen, input, output, output_max_len);
}

int atcmd_tls_rsa_sign_func(void * ctx,
                            int (*f_rng)(void *, unsigned char *, size_t), void * p_rng,
                            mbedtls_md_type_t md_alg, unsigned int hashlen,
                            const unsigned char * hash, unsigned char * sig)
{
    return mbedtls_rsa_pkcs1_sign((mbedtls_rsa_context *)ctx, f_rng, p_rng,
                                  md_alg, hashlen, hash, sig);
}

size_t atcmd_tls_rsa_key_len_func(void * ctx)
{
    return ((const mbedtls_rsa_context *)ctx)->MBEDTLS_PRIVATE(len);
}

//related with dpm
#if CFG_PMGR
int atcmd_tlsc_store_ssl(atcmd_tlsc_context * ctx)
{
    int status = 0;

    if (RM_PMGR_W_dpm_is_enabled())
    {
        //ATCMD_TLSC_INFO("To store tls session(%s)\n", ctx->tls_name);

        status = RM_PMGR_W_dpm_tls_session_set(ctx->tls_name, ctx->ssl_ctx);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to save tls session(0x%x)\n", status);
            return RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_restore_ssl(atcmd_tlsc_context * ctx)
{
    int status = 0;

    if (RM_PMGR_W_dpm_is_enabled())
    {
        ATCMD_TLSC_INFO("To restore tls session(%s)\n", ctx->tls_name);

        status = RM_PMGR_W_dpm_tls_session_get(ctx->tls_name, ctx->ssl_ctx);

        if (status == ER_NOT_FOUND)
        {
            ATCMD_TLSC_INFO("Not found(%s:%d)\n", ctx->tls_name, strlen(ctx->tls_name));
            return RRQ_APP_SUCCESS;
        }
        else if (status != 0)
        {
            ATCMD_TLSC_ERR("Failed to restore tls session(0x%x)\n", status);
            return RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_clear_ssl(atcmd_tlsc_context * ctx)
{
    int status = 0;

    if (RM_PMGR_W_dpm_is_enabled())
    {
        ATCMD_TLSC_INFO("To clear tls session(%s)\n", ctx->tls_name);

        status = RM_PMGR_W_dpm_tls_session_clear(ctx->tls_name);

        if (status)
        {
            ATCMD_TLSC_INFO("Failed to restore tls session(0x%x)\n", status);
            return RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    return RRQ_APP_SUCCESS;
}
#endif /* CFG_PMGR */

int atcmd_tlsc_init_ssl(atcmd_tlsc_context * ctx)
{
    atcmd_tlsc_config * conf = ctx->conf;

    ATCMD_TLSC_INFO("Init TLS\n");

    if (!ctx->ssl_ctx)
    {
        ctx->ssl_ctx = atcmd_tlsc_malloc(sizeof(mbedtls_ssl_context));

        if (!ctx->ssl_ctx)
        {
            ATCMD_TLSC_ERR("Failed to allocate ssl context\n");
            goto error;
        }

        mbedtls_ssl_init(ctx->ssl_ctx);
    }

    if (!ctx->ssl_conf)
    {
        ctx->ssl_conf = atcmd_tlsc_malloc(sizeof(mbedtls_ssl_config));

        if (!ctx->ssl_conf)
        {
            ATCMD_TLSC_ERR("Failed to allocate ssl config\n");
            goto error;
        }

        mbedtls_ssl_config_init(ctx->ssl_conf);
    }

    if (!ctx->ctr_drbg_ctx)
    {
        ctx->ctr_drbg_ctx = atcmd_tlsc_malloc(sizeof(mbedtls_ctr_drbg_context));

        if (!ctx->ctr_drbg_ctx)
        {
            ATCMD_TLSC_ERR("Failed to allocate ctr-drbg\n");
            goto error;
        }

        mbedtls_ctr_drbg_init(ctx->ctr_drbg_ctx);
    }

    if (!ctx->entropy_ctx)
    {
        ctx->entropy_ctx = atcmd_tlsc_malloc(sizeof(mbedtls_entropy_context));

        if (!ctx->entropy_ctx)
        {
            ATCMD_TLSC_ERR("Failed to allocate entropy\n");
            goto error;
        }

        mbedtls_entropy_init(ctx->entropy_ctx);
    }

    if (strlen(conf->ca_cert_name) > 0)
    {
        if (!ctx->ca_cert_crt)
        {
            ctx->ca_cert_crt = atcmd_tlsc_malloc(sizeof(mbedtls_x509_crt));

            if (!ctx->ca_cert_crt)
            {
                ATCMD_TLSC_ERR("Failed to allocate CA certificate\n");
                goto error;
            }

            mbedtls_x509_crt_init(ctx->ca_cert_crt);
        }
    }

    if (strlen(conf->cert_name) > 0)
    {
        if (!ctx->cert_crt)
        {
            ctx->cert_crt = atcmd_tlsc_malloc(sizeof(mbedtls_x509_crt));

            if (!ctx->cert_crt)
            {
                ATCMD_TLSC_ERR("Failed to allocate certificate\n");
                goto error;
            }

            mbedtls_x509_crt_init(ctx->cert_crt);
        }

        if (!ctx->pkey_ctx)
        {
            ctx->pkey_ctx = atcmd_tlsc_malloc(sizeof(mbedtls_pk_context));

            if (!ctx->pkey_ctx)
            {
                ATCMD_TLSC_ERR("Failed to allocate private key\n");
                goto error;
            }

            mbedtls_pk_init(ctx->pkey_ctx);
        }

        if (!ctx->alt_pkey_ctx)
        {
            ctx->alt_pkey_ctx = atcmd_tlsc_malloc(sizeof(mbedtls_pk_context));

            if (!ctx->alt_pkey_ctx)
            {
                ATCMD_TLSC_ERR("Failed to allocate private key for alt\n");
                goto error;
            }

            mbedtls_pk_init(ctx->alt_pkey_ctx);
        }
    }

    return RRQ_APP_SUCCESS;

error:

    atcmd_tlsc_deinit_ssl(ctx);

    return RRQ_APP_NOT_CREATED;
}

int atcmd_tlsc_setup_ssl(atcmd_tlsc_context * ctx)
{
    int status = RRQ_APP_SUCCESS;
    atcmd_tlsc_config * conf = ctx->conf;
    const char * pers = "atcmd_tls_client";

    char * ca_cert = NULL;
    size_t ca_cert_len = 0;

    char * cert = NULL;
    size_t cert_len = 0;

    char * privkey = NULL;
    size_t privkey_len = 0;

    int preset = MBEDTLS_SSL_PRESET_RRQ;

    ATCMD_TLSC_INFO("Setup TLS\n");

    status = mbedtls_ctr_drbg_seed(ctx->ctr_drbg_ctx,
                                   mbedtls_entropy_func,
                                   ctx->entropy_ctx,
                                   (const unsigned char *)pers,
                                   strlen(pers));

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to set ctr-drbg seed(0x%x)\n", -status);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    status = mbedtls_ssl_config_defaults(ctx->ssl_conf,
                                         MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         preset);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to set default ssl config(0x%x)\n", -status);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    if (strlen(conf->ca_cert_name) > 0)
    {
        status = atcmd_cm_get_cert_len(conf->ca_cert_name, ATCMD_CM_CERT_TYPE_CA_CERT, 0,
                                       &ca_cert_len);

        if (status || ca_cert_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get length of CA cert(0x%x)\n", status);

            if (ca_cert_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_ca_cert;
        }

        ca_cert = atcmd_tlsc_malloc(ca_cert_len);

        if (!ca_cert)
        {
            ATCMD_TLSC_ERR("Failed to allocate memory for CA cert(%d)\n", ca_cert_len);
            status = RRQ_APP_MALLOC_ERROR;
            goto end_of_ca_cert;
        }

        status = atcmd_cm_get_cert(conf->ca_cert_name, ATCMD_CM_CERT_TYPE_CA_CERT, 0,
                                   ca_cert, &ca_cert_len);

        if (status || ca_cert_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get CA cert(0x%x,%d)\n", status, ca_cert_len);

            if (ca_cert_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_ca_cert;
        }

        status = mbedtls_x509_crt_parse(ctx->ca_cert_crt, (const unsigned char *)ca_cert, ca_cert_len);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to parse CA certificate(0x%x)\n", -status);
            status = RRQ_APP_NOT_SUCCESSFUL;

            #if defined(ENABLE_ATCMD_TLSC_DBG_INFO)

            for (unsigned int idx = 0 ; idx < ca_cert_len ; idx++)
            {
                printf("%c", ca_cert[idx]);
            }

            #endif /* ENABLE_ATCMD_TLSC_DBG_INFO */

            goto end_of_ca_cert;
        }

        mbedtls_ssl_conf_ca_chain(ctx->ssl_conf, ctx->ca_cert_crt, NULL);

        ATCMD_TLSC_INFO("Imported CA certificate\n");

end_of_ca_cert:

        if (ca_cert)
        {
            atcmd_tlsc_free(ca_cert);
            ca_cert = NULL;
        }

        if (status)
        {
            return status;
        }
    }

    if (strlen(conf->cert_name) > 0)
    {
        status = atcmd_cm_get_cert_len(conf->cert_name,
                                       ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_CERT,
                                       &cert_len);

        if (status || cert_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get length of cert(0x%x)\n", status);

            if (cert_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_cert;
        }

        status = atcmd_cm_get_cert_len(conf->cert_name,
                                       ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_KEY,
                                       &privkey_len);

        if (status || privkey_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get length of private key(0x%x)\n", status);

            if (privkey_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_cert;
        }

        cert = atcmd_tlsc_malloc(cert_len);

        if (!cert)
        {
            ATCMD_TLSC_ERR("Failed to allocate memory for cert(%d)\n", cert_len);
            status = RRQ_APP_MALLOC_ERROR;
            goto end_of_cert;
        }

        privkey = atcmd_tlsc_malloc(privkey_len);

        if (!privkey)
        {
            ATCMD_TLSC_ERR("Failed to allocate memory for private key(%d)\n", privkey_len);
            status = RRQ_APP_MALLOC_ERROR;
            goto end_of_cert;
        }

        status = atcmd_cm_get_cert(conf->cert_name,
                                   ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_CERT,
                                   cert, &cert_len);

        if (status || cert_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get cert(0x%x)\n", status);

            if (cert_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_cert;
        }

        status = atcmd_cm_get_cert(conf->cert_name,
                                   ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_KEY,
                                   privkey, &privkey_len);

        if (status || privkey_len == 0)
        {
            ATCMD_TLSC_ERR("Failed to get private key(0x%x)\n", status);

            if (privkey_len == 0)
            {
                status = RRQ_APP_NOT_FOUND;
            }

            goto end_of_cert;
        }

        //Import certificate
        status = mbedtls_x509_crt_parse(ctx->cert_crt, (const unsigned char *)cert, cert_len);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to parse cert(0x%x)\n", -status);
            status = RRQ_APP_NOT_SUCCESSFUL;

            #if defined(ENABLE_ATCMD_TLSC_DBG_INFO)

            for (unsigned int idx = 0 ; idx < cert_len ; idx++)
            {
                printf("%c", cert[idx]);
            }

            #endif /* ENABLE_ATCMD_TLSC_DBG_INFO */

            goto end_of_cert;
        }

        ATCMD_TLSC_INFO("Imported certificate\n");

        //Import private key
        status = mbedtls_pk_parse_key(ctx->pkey_ctx, (const unsigned char *)privkey, privkey_len,
                                      NULL, 0, mbedtls_ctr_drbg_random, ctx->ctr_drbg_ctx);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to parse private key(0x%x)\n", -status);
            status = RRQ_APP_NOT_SUCCESSFUL;

            #if defined(ENABLE_ATCMD_TLSC_DBG_INFO)

            for (unsigned int idx = 0 ; idx < privkey_len ; idx++)
            {
                printf("%c", privkey[idx]);
            }

            #endif /* ENABLE_ATCMD_TLSC_DBG_INFO */

            goto end_of_cert;
        }

        ATCMD_TLSC_INFO("Imported private key\n");

        #if defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT)

        if (mbedtls_pk_get_type(ctx->pkey_ctx) == MBEDTLS_PK_RSA)
        {
            status = mbedtls_pk_setup_rsa_alt(ctx->alt_pkey_ctx,
                                              (void *)mbedtls_pk_rsa(*ctx->pkey_ctx),
                                              atcmd_tls_rsa_decrypt_func,
                                              atcmd_tls_rsa_sign_func,
                                              atcmd_tls_rsa_key_len_func);

            if (status)
            {
                ATCMD_TLSC_ERR("Failed to set rsa alt(0x%x)\n", -status);
                status = RRQ_APP_NOT_SUCCESSFUL;
                goto end_of_cert;
            }

            status = mbedtls_ssl_conf_own_cert(ctx->ssl_conf, ctx->cert_crt,
                                               ctx->alt_pkey_ctx);
        }
        else
        {
            status = mbedtls_ssl_conf_own_cert(ctx->ssl_conf, ctx->cert_crt, ctx->pkey_ctx);
        }

        #else
        status = mbedtls_ssl_conf_own_cert(ctx->ssl_conf, ctx->cert_crt, ctx->pkey_ctx);
        #endif /* defined(MBEDTLS_RSA_C) && defined(MBEDTLS_PK_RSA_ALT_SUPPORT) */

end_of_cert:

        if (cert)
        {
            atcmd_tlsc_free(cert);
        }

        if (privkey)
        {
            atcmd_tlsc_free(privkey);
        }

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to set cert & private key(0x%x)\n", -status);
            return status;
        }
    }

    mbedtls_ssl_conf_rng(ctx->ssl_conf, mbedtls_ctr_drbg_random, ctx->ctr_drbg_ctx);

    ATCMD_TLSC_INFO("Auth mode(%d)\n", conf->auth_mode);

    if (conf->auth_mode)
    {
        mbedtls_ssl_conf_authmode(ctx->ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    }
    else
    {
        mbedtls_ssl_conf_authmode(ctx->ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
    }

    if (conf->incoming_buflen < ATCMD_TLSC_MIN_INCOMING_LEN)
    {
        conf->incoming_buflen = ATCMD_TLSC_DEF_INCOMING_LEN;
    }

    if (conf->outgoing_buflen < ATCMD_TLSC_MIN_OUTGOING_LEN)
    {
        conf->outgoing_buflen = ATCMD_TLSC_DEF_OUTGOING_LEN;
    }

    ATCMD_TLSC_INFO("Incoming(%d), outgoing(%d)\n",
                    conf->incoming_buflen, conf->outgoing_buflen);

    status = mbedtls_ssl_conf_content_len(ctx->ssl_conf, conf->incoming_buflen,
                                          conf->outgoing_buflen);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to set content buffer len(0x%x,%d,%d)\n",
                       -status, conf->incoming_buflen, conf->outgoing_buflen);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    if (strlen(conf->hostname))
    {
        ATCMD_TLSC_INFO("Hostname(%s:%d)\n", conf->hostname, strlen(conf->hostname));

        status = mbedtls_ssl_set_hostname(ctx->ssl_ctx, (const char *)conf->hostname);

        if (status)
        {
            ATCMD_TLSC_ERR("Failed to set hostname(0x%x,%s)\n", -status, conf->hostname);
            return RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    ATCMD_TLSC_INFO("TLS ver(%d)\n", conf->tls_ver);

    if (conf->tls_ver == ONLY_TLS12)
    {
        mbedtls_ssl_conf_min_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_max_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_2);
    }
    else if (conf->tls_ver == ONLY_TLS13)
    {
        mbedtls_ssl_conf_min_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_max_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_3);

    }
    else if (conf->tls_ver == TLS12_13)
    {
        mbedtls_ssl_conf_min_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_max_tls_version(ctx->ssl_conf, MBEDTLS_SSL_VERSION_TLS1_3);
    }

    memcpy(&ctx->crt_profile, &mbedtls_x509_crt_profile_default, sizeof(mbedtls_x509_crt_profile));

    ctx->crt_profile.allowed_mds |= MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1);

    mbedtls_ssl_conf_cert_profile(ctx->ssl_conf, &ctx->crt_profile);

    status = mbedtls_ssl_setup(ctx->ssl_ctx, ctx->ssl_conf);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to set ssl config(0x%x)\n", -status);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

#if CFG_PMGR
    status = atcmd_tlsc_restore_ssl(ctx);

    if (status)
    {
        ATCMD_TLSC_ERR(" Failed to restore ssl(0x%x)\n", -status);
        return status;
    }
#endif /* CFG_PMGR */

    mbedtls_ssl_set_bio(ctx->ssl_ctx, (void *)&ctx->net_ctx,
                        atcmd_tlsc_send_cb,
                        mbedtls_net_recv,
                        mbedtls_net_recv_timeout);

    return RRQ_APP_SUCCESS;
}


int atcmd_tlsc_deinit_ssl(atcmd_tlsc_context * ctx)
{
    if (ctx->ssl_ctx)
    {
        mbedtls_ssl_free(ctx->ssl_ctx);
        atcmd_tlsc_free(ctx->ssl_ctx);
    }

    if (ctx->ssl_conf)
    {
        mbedtls_ssl_config_free(ctx->ssl_conf);
        atcmd_tlsc_free(ctx->ssl_conf);
    }

    if (ctx->ctr_drbg_ctx)
    {
        mbedtls_ctr_drbg_free(ctx->ctr_drbg_ctx);
        atcmd_tlsc_free(ctx->ctr_drbg_ctx);
    }

    if (ctx->entropy_ctx)
    {
        mbedtls_entropy_free(ctx->entropy_ctx);
        atcmd_tlsc_free(ctx->entropy_ctx);
    }

    if (ctx->ca_cert_crt)
    {
        mbedtls_x509_crt_free(ctx->ca_cert_crt);
        atcmd_tlsc_free(ctx->ca_cert_crt);
    }

    if (ctx->cert_crt)
    {
        mbedtls_x509_crt_free(ctx->cert_crt);
        atcmd_tlsc_free(ctx->cert_crt);
    }

    if (ctx->pkey_ctx)
    {
        mbedtls_pk_free(ctx->pkey_ctx);
        atcmd_tlsc_free(ctx->pkey_ctx);
    }

    if (ctx->alt_pkey_ctx)
    {
        mbedtls_pk_free(ctx->alt_pkey_ctx);
        atcmd_tlsc_free(ctx->alt_pkey_ctx);
    }

    ctx->ssl_ctx = NULL;
    ctx->ssl_conf = NULL;
    ctx->ctr_drbg_ctx = NULL;
    ctx->entropy_ctx = NULL;
    ctx->ca_cert_crt = NULL;
    ctx->cert_crt = NULL;
    ctx->pkey_ctx = NULL;
    ctx->alt_pkey_ctx = NULL;

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_shutdown_ssl(atcmd_tlsc_context * ctx)
{
    int status = 0;

    status = mbedtls_ssl_session_reset(ctx->ssl_ctx);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to reset session(0x%x)\n", -status);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_tlsc_do_handshake(atcmd_tlsc_context * ctx, unsigned long wait_option)
{
    int status = RRQ_APP_SUCCESS;

    if (ctx->ssl_ctx->MBEDTLS_PRIVATE(state) == MBEDTLS_SSL_HANDSHAKE_OVER)
    {
        ATCMD_TLSC_INFO("Already established tls session\n");
        return RRQ_APP_SUCCESS;
    }

    mbedtls_ssl_conf_read_timeout(ctx->ssl_conf, wait_option);

    while ((status = mbedtls_ssl_handshake(ctx->ssl_ctx)) != 0)
    {
        if (status == MBEDTLS_ERR_NET_CONN_RESET)
        {
            ATCMD_TLSC_ERR("Peer closed the connection(0x%x)\n", -status);
            break;
        }

        if ((status != MBEDTLS_ERR_SSL_WANT_READ)
                && (status != MBEDTLS_ERR_SSL_WANT_WRITE))
        {
            ATCMD_TLSC_ERR("Failed to process tls handshake(0x%x)\n", -status);
            break;
        }
    }

    return status;
}

void atcmd_tlsc_transfer_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t data_len)
{
    fsp_err_t err = FSP_SUCCESS;
    int cnt = 0;

    if (ctx->p_at_ctrl)
    {
        for (cnt = 0; cnt < ATCMD_TLSC_SERIAL_WAIT_CNT; cnt++)
        {
            if (xPortGetFreeHeapSize() > ATCMD_TLSC_MIN_RETAINED_HEAP_MEMORY)
            {
                err = RM_ATCMD_W_CORE_Write(ctx->p_at_ctrl, (uint8_t *) data, data_len);
                if (err != FSP_ERR_QUEUE_FULL)
                {
                    break;
                }
            }

            vTaskDelay(portCONVERT_MS_2_TICKS(ATCMD_TLSC_SERIAL_WAIT_TIME));
        }
    }

    return ;
}

void atcmd_tlsc_transfer_disconn_data(atcmd_tlsc_context * ctx)
{
    unsigned int local_port = 0;

    char resp_str[256] = {0x00,};

    if (ctx->p_at_ctrl)
    {
        atcmd_tlsc_get_local_port(ctx, &local_port);

        ATCMD_TLSC_INFO(ATCMD_TLSC_DISCONN_RX_PREFIX ":%d,%s,%u\n", ctx->cid,
                        ctx->conf->svr_addr, local_port);

        sprintf(resp_str, "\r\n" ATCMD_TLSC_DISCONN_RX_PREFIX ":%d,%s,%u\r\n", ctx->cid,
                ctx->conf->svr_addr, local_port);

        atcmd_tlsc_transfer_data(ctx, (unsigned char *) resp_str, strlen(resp_str));
    }

    return ;
}

void atcmd_tlsc_entry_func(void * pvParameters)
{
    int status = RRQ_APP_SUCCESS;
    atcmd_tlsc_context * ctx = (atcmd_tlsc_context *)pvParameters;

    size_t recv_data_len = 0;
    unsigned int local_port = 0;

    unsigned char * p_recv_msg_buf = NULL;
    size_t recv_msg_buflen = 0;
    size_t recv_msg_len = 0;

    int conn_retry = 0;
    int handshake_retry = 0;

#if CFG_PMGR
    unsigned int svr_port = atoi(ctx->conf->svr_port);
#endif /* CFG_PMGR */

    ATCMD_TLSC_INFO("Start\n");

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                       g_wifi_cfg.p_watchdog_service->p_cfg,
                                                       &task_wdog_id);
#endif

    ctx->state = ATCMD_TLSC_STATE_DISCONNECTED;

    if (!ctx || !ctx->conf)
    {
        ATCMD_TLSC_ERR("Invaild context\n");
        goto terminate;
    }

    recv_msg_buflen = ctx->conf->recv_buflen + ATCMD_TLSC_DATA_RX_PREFIX_LEN;

    p_recv_msg_buf = atcmd_tlsc_malloc(recv_msg_buflen);
    
    if (!p_recv_msg_buf)
    {
        ATCMD_TLSC_ERR("Failed to allocate recv message buffer(%d)\n", recv_msg_buflen);
        goto terminate;
    }

    ctx->recv_buf = atcmd_tlsc_malloc(ctx->conf->recv_buflen);

    if (!ctx->recv_buf)
    {
        ATCMD_TLSC_ERR("Failed to allocate recv buffer(%d)\n", ctx->conf->recv_buflen);
        goto terminate;
    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        ATCMD_TLSC_INFO("Register DPM(%s,%s)\n", ctx->task_name, ctx->conf->svr_port);
        RM_PMGR_W_dpm_job_name_set((char *)(ctx->task_name), svr_port);
    }
#endif /* CFG_PMGR */

    status = atcmd_tlsc_init_socket(ctx);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to init socket(0x%x)\n", -status);
        goto terminate;
    }

    status = atcmd_tlsc_init_ssl(ctx);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to init SSL(0x%x)\n", -status);
        goto terminate;
    }

    status = atcmd_tlsc_setup_ssl(ctx);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to setup SSL(0x%x)\n", -status);
        goto terminate;
    }

#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_done((char *)(ctx->task_name));
#endif /* CFG_PMGR */

connect:

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                  task_wdog_id);
#endif

    ATCMD_TLSC_INFO("#%d. Connecting & waiting for %d\n",
                    conn_retry + 1, ATCMD_TLSC_CONN_TIMEOUT);

    status = atcmd_tlsc_connect_socket(ctx, ATCMD_TLSC_CONN_TIMEOUT);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to connect to TLS server(0x%x)\n", status);

        atcmd_tlsc_disconnect_socket(ctx);

        atcmd_tlsc_init_socket(ctx);

        if (conn_retry < ATCMD_TLSC_MAX_CONN_CNT)
        {
            conn_retry++;
            vTaskDelay(portCONVERT_MS_2_TICKS(ATCMD_TLSC_RECONN_SLEEP_TIMEOUT));

            ATCMD_TLSC_ERR("wait_time(%d), cur_cnt(%d), max_retry_cnt(%d)\n",
                           ATCMD_TLSC_RECONN_SLEEP_TIMEOUT, conn_retry, ATCMD_TLSC_MAX_CONN_CNT);

            goto connect;
        }
        else
        {
            goto terminate;
        }
    }

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                          g_wifi_cfg.p_watchdog_service->p_cfg,
                                                          task_wdog_id);

    g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     task_wdog_id,
                                                     ATCMD_TLSC_WDOG_LATENCY);
#endif

    ATCMD_TLSC_INFO("#%d. TLS Handshake\n", handshake_retry + 1);

    status = atcmd_tlsc_do_handshake(ctx, ATCMD_TLSC_HANDSHAKE_TIMEOUT);

    if (status)
    {
        ATCMD_TLSC_ERR("Failed to establish TLS session(0x%x)\n", -status);

        atcmd_tlsc_shutdown_ssl(ctx);

        atcmd_tlsc_disconnect_socket(ctx);

        atcmd_tlsc_init_socket(ctx);

        if (handshake_retry < ATCMD_TLSC_MAX_CONN_CNT)
        {
            handshake_retry++;
            vTaskDelay(portCONVERT_MS_2_TICKS(ATCMD_TLSC_DEF_TIMEOUT));
            goto connect;
        }
        else
        {
            ATCMD_TLSC_ERR("Terminated TLS client\n");
            goto terminate;
        }
    }

#if CFG_PMGR
    atcmd_tlsc_store_ssl(ctx);
#endif /* CFG_PMGR */

    ATCMD_TLSC_INFO("Estalished TLS session\n");

    ctx->state = ATCMD_TLSC_STATE_CONNECTED;

#if CFG_PMGR
    RM_PMGR_W_dpm_rcv_ready_set((char *)(ctx->task_name));
#endif /* CFG_PMGR */

    atcmd_tlsc_get_local_port(ctx, &local_port);

    while (ctx->state == ATCMD_TLSC_STATE_CONNECTED)
    {
#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     g_wifi_cfg.p_watchdog_service->p_cfg,
                                                     task_wdog_id);
        g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         task_wdog_id,
                                                         ATCMD_TLSC_WDOG_LATENCY);
#endif

        memset(p_recv_msg_buf, 0x00, recv_msg_buflen);
        memset(ctx->recv_buf, 0x00, ctx->conf->recv_buflen);

        status = atcmd_tlsc_recv(ctx, ctx->recv_buf, ctx->conf->recv_buflen, ATCMD_TLSC_RECV_TIMEOUT);

        if (status > 0)
        {
#if CFG_PMGR
            ATCMD_TLSC_INFO("Clear DPM(%s)\n", ctx->task_name);
            RM_PMGR_W_dpm_sleep_ready_clear((char *)(ctx->task_name)); // dpm clear to progress msg
#endif /* CFG_PMGR */

            /* Added Header */
            recv_data_len = status;

            recv_msg_len = snprintf((char *) p_recv_msg_buf, ATCMD_TLSC_DATA_RX_PREFIX_LEN,
                                    "\r\n" ATCMD_TLSC_DATA_RX_PREFIX ":%d,%s,%u,%d,",
                                    ctx->cid, ctx->conf->svr_addr, local_port, recv_data_len);

            memcpy(p_recv_msg_buf + recv_msg_len, ctx->recv_buf, recv_data_len);

            recv_msg_len += recv_data_len;

            memcpy(p_recv_msg_buf + recv_msg_len, "\r\n", 2);

            recv_msg_len += 2;

            p_recv_msg_buf[recv_msg_len] = '\0';

            /* Transfer data over serial interface. */
            atcmd_tlsc_transfer_data(ctx, p_recv_msg_buf, recv_msg_len);
        }
        else if (status == RRQ_APP_NOT_CONNECTED)
        {
            ctx->state = ATCMD_TLSC_STATE_DISCONNECTED;

            atcmd_tlsc_transfer_disconn_data(ctx);

            break;
        }
        else if (status == RRQ_APP_NO_PACKET)
        {
            ;
        }
        else
        {
            ATCMD_TLSC_ERR("Failed to recv packet(0x%x)\n", -status);
            ctx->state = ATCMD_TLSC_STATE_DISCONNECTED;

            atcmd_tlsc_transfer_disconn_data(ctx);

            break;
        }

        if (mbedtls_ssl_get_bytes_avail(ctx->ssl_ctx) == 0)
        {
#if CFG_PMGR
            //ATCMD_TLSC_INFO("Set DPM(%s)\n", ctx->task_name);
            RM_PMGR_W_dpm_sleep_ready_set((char *)(ctx->task_name));
#endif /* CFG_PMGR */
        }
    }

terminate:

#if CFG_PMGR
    ATCMD_TLSC_INFO("Clear DPM(%s)\n", ctx->task_name);
    RM_PMGR_W_dpm_sleep_ready_clear((char *)(ctx->task_name));
#endif /* CFG_PMGR */

    atcmd_tlsc_deinit_ssl(ctx);

#if CFG_PMGR
    atcmd_tlsc_clear_ssl(ctx);
#endif /* CFG_PMGR */

    atcmd_tlsc_disconnect_socket(ctx);

    if (p_recv_msg_buf)
    {
        atcmd_tlsc_free(p_recv_msg_buf);
        p_recv_msg_buf = NULL;
    }

    if (ctx->recv_buf)
    {
        atcmd_tlsc_free(ctx->recv_buf);
        ctx->recv_buf = NULL;
    }

    ctx->state = ATCMD_TLSC_STATE_TERMINATED;

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        ATCMD_TLSC_INFO("deregister DPM(%s,%d)\n", ctx->task_name,
                        ctx->conf->local_port);
        RM_PMGR_W_dpm_job_name_clear((char *)(ctx->task_name));
    }
#endif /* CFG_PMGR */

#if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

    ATCMD_TLSC_INFO("End\n");

    ctx->task_handler = NULL;
    vTaskDelete(NULL);

    return ;
}
#endif /* CFG_WIFI */

