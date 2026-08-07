/**
 ****************************************************************************************
 *
 * @file rm_cli_w_http.c
 *
 * @brief HTTP command functions
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

#include "custom_config_sdk.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "mbedtls/ssl.h"
#include "rm_cli_w_http.h"


/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ENABLE_HTTPC_DEBUG

#if defined (ENABLE_HTTPC_DEBUG)
#define HTTPC_PRINTF            printf
#else
#define HTTPC_PRINTF(...)            do {} while (0)
#endif

/* Debug Log level(Information) for HTTP Client */
/* #undef  ENABLE_HTTPC_DEBUG_INFO */
#define   ENABLE_HTTPC_DEBUG_INFO

#if defined (ENABLE_HTTPC_DEBUG_INFO)
#define HTTPC_DEBUG_INFO(fmt, ...)   HTTPC_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define HTTPC_DEBUG_INFO(...)        do {} while (0)
#endif /* ENABLE_HTTPC_DEBUG_INFO */

#define   ENABLE_HTTPC_DEBUG_DUMP

#if defined (ENABLE_HTTPC_DEBUG_DUMP)
#define HTTPC_DEBUG_DUMP(...)   HTTPC_PRINTF(__VA_ARGS__)
#else
#define HTTPC_DEBUG_DUMP(...)        do {} while (0)
#endif /* ENABLE_HTTPC_DEBUG_INFO */

/* Debug Log level(Error) for HTTP Client */
#define ENABLE_HTTPC_DEBUG_ERR

#if defined (ENABLE_HTTPC_DEBUG_ERR)
#define HTTPC_DEBUG_ERR(fmt, ...)    HTTPC_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define HTTPC_DEBUG_ERR(...)         do {} while (0)
#endif /* ENABLE_HTTPC_DEBUG_ERR */


/***********************************************************************************************************************
 * Function prototypes
 ***********************************************************************************************************************/
#ifdef __SUPPORT_HTTP_CLIENT_FOR_CLI__
static void rm_cli_w_httpc_recv_callback(https_callback_args_t * p_args);
static void dump_http_payload(void * p_args);
#endif //__SUPPORT_HTTP_CLIENT_FOR_CLI__

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
extern const https_api_t g_https_w;
const https_api_t * const p_cli_https = &g_https_w;

https_w_instance_ctrl_t g_cli_https_w0_ctrl;
#ifdef __SUPPORT_HTTP_CLIENT_FOR_CLI__
https_cfg_t g_cli_https_w0_cfg =
{
    .p_callback    = rm_cli_w_httpc_recv_callback,
};
#endif //__SUPPORT_HTTP_CLIENT_FOR_CLI__

static volatile uint32_t client_transfer_complete               = 0;
static volatile uint32_t client_transfer_error                  = 0;
static volatile uint32_t server_transfer_complete               = 0;
static volatile uint32_t server_transfer_error                  = 0;


#ifdef __SUPPORT_HTTP_CLIENT_FOR_CLI__
static http_client_request_t request = {0, };

static void rm_cli_w_http_client_clear_secu_conf(httpc_secure_connection_t * conf)
{
    int idx = 0;

    if (conf->alpn)
    {
        for (idx = 0 ; idx < conf->alpn_cnt ; idx++)
        {
            if (conf->alpn[idx])
            {
                vPortFree(conf->alpn[idx]);
                conf->alpn[idx] = NULL;
            }
        }

        vPortFree(conf->alpn);
        conf->alpn = NULL;
        conf->alpn_cnt = 0;
    }

    if (conf->sni)
    {
        vPortFree(conf->sni);
        conf->sni = NULL;
        conf->sni_len = 0;
    }

    return;
}

static void rm_cli_w_httpc_recv_callback(https_callback_args_t * p_args)
{

    switch (p_args->event)
    {
        case HTTPS_EVENT_SERVER_RECVED:
        {
            err_t err = *(int8_t *) p_args->p_param;

            if (err != ERR_OK || !p_args->len)
            {
                HTTPC_DEBUG_ERR("[%s]:%d err=%d, p->len = %d\n", __func__, __LINE__, err, p_args->len);
                server_transfer_error = 1;
            }

            if (err == ERR_OK)
            {
                server_transfer_complete = 1;
            }

            HTTPC_DEBUG_INFO("\n[%s]:%d err = %d, p->len = %d\n", __func__, __LINE__, err, p_args->len);
            dump_http_payload(p_args);

            break;
        }

        case HTTPS_EVENT_SERVER_ERR_RESULT:
        {
            err_t err = *(int8_t *) p_args->p_param;

            HTTPC_DEBUG_ERR("err: %d\n", err);

            if (err != ERR_OK)
            {
                server_transfer_error = 1;
            }

            break;
        }

        case HTTPS_EVENT_CLIENT_RESULT:
        {
#if defined(ENABLE_HTTPC_DEBUG_INFO)
            uint32_t httpc_result = *(uint32_t *) p_args->payload;
            uint32_t rx_content_len = p_args->len;
#endif
            err_t err = *(int8_t *) p_args->p_param;

            HTTPC_DEBUG_INFO("httpc_result: %ld, received: %d byte, err: %d\n", httpc_result, (int) rx_content_len, err);

            if (err == ERR_OK)
            {
                client_transfer_complete = 1;
            }
            else
            {
                client_transfer_error = 1;
            }

            break;
        }

        case HTTPS_EVENT_CLIENT_RECVED:
        {
#if defined(ENABLE_HTTPC_DEBUG_INFO)
            uint32_t rx_content_len = p_args->len;
            err_t err = *(int8_t *) p_args->p_param;
#endif

            HTTPC_DEBUG_INFO("received: %d byte, err: %d\n", (int) rx_content_len, err);
            dump_http_payload(p_args);
            break;
        }

        case HTTPS_EVENT_CLIENT_RECVED_DECODED:
        {
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_RECVED_DECODED\n");
            break;
        }

        case HTTPS_EVENT_CLIENT_GET_DONE:
        {
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_GET_DONE\n");
            break;
        }

        default:
            break;
    }
}

static fsp_err_t rm_cli_w_http_client_parse_uri(unsigned char * uri, size_t len, http_client_request_t * request)
{
    unsigned char * p = NULL;
    unsigned char * q = NULL;

    if (strlen((const char *) uri) > HTTPC_MAX_PATH_LEN)
    {
        HTTPC_DEBUG_ERR("Invalid URL Length.(max = %d)\n", HTTPC_MAX_PATH_LEN);
        goto error;
    }

    memset(request->path, 0x00, HTTPC_MAX_PATH_LEN);
    memcpy(request->path, uri, strlen((const char *) uri));

    p = uri;
    q = (unsigned char *) "http";

    while (len && *q && tolower(*p) == * q)
    {
        ++p;
        ++q;
        --len;
    }

    if (*q)
    {
        HTTPC_DEBUG_ERR("Invalid prefix(http)\n");
        goto error;
    }

    if (len && (tolower(*p) == 's'))
    {
        ++p;
        --len;
        request->insecure = pdTRUE;
        request->port = HTTPS_SERVER_PORT;
    }
    else
    {
        request->insecure = pdFALSE;
        request->port = HTTP_SERVER_PORT;
    }

    q = (unsigned char *) "://";

    while (len && *q && tolower(*p) == *q)
    {
        ++p;
        ++q;
        --len;
    }

    if (*q)
    {
        HTTPC_DEBUG_ERR("Invalid uri\n");
        goto error;
    }

    /* p points to beginning of Uri-Host */
    q = p;

    if (len && *p == '[')
    {
        /* IPv6 address reference */
#if defined(__SUPPORT_IPV6__)
        ++p;

        while (len && *q != ']')
        {
            ++q;
            --len;
        }

        if (!len || *q != ']' || p == q)
        {
            HTTPC_DEBUG_ERR("Invaild URI\n");
            goto error;
        }

        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, p, q - p);
        ++q;
        --len;
#else
        //not supported ipv6
        HTTPC_DEBUG_ERR("Not supported IPv6\n");
        goto error;
#endif
    }
    else        /* IPv4 address or FQDN */
    {
        if (strstr((const char *) q, "@"))
        {
            p = (unsigned char *) strstr((const char *) q, "@");
            ++p;
            len -= (size_t) (p - q);
            q = p;
        }

        while (len && *q != ':' && *q != '/' && *q != '?')
        {
            *q = (unsigned char) tolower((int) *q);
            ++q;
            --len;
        }

        if (p == q)
        {
            HTTPC_DEBUG_ERR("Invalid hostname\n");
            goto error;
        }

        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, (const char *) p, (size_t) (q - p));
    }

    /* check for Uri-Port */
    if (len && *q == ':')
    {
        p = ++q;
        --len;

        while (len && isdigit(*q))
        {
            ++q;
            --len;
        }

        if (p < q)
        {
            /* explicit port number given */
            int port = 0;

            while (p < q)
            {
                port = port * 10 + (*p++ - '0');
            }

            request->port = (UINT) port;
        }
    }

    return FSP_SUCCESS;

error:
    return FSP_ERR_INVALID_ARGUMENT;
}

static fsp_err_t wait_for_client_transfer_complete (bool allow_error)
{
    uint32_t timeout = 1000000;

    while (timeout > 0)
    {
        R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MICROSECONDS);
        timeout--;

        if (client_transfer_error)
        {
            if (allow_error)
            {
                break;
            }

            client_transfer_error = 0U;
            return FSP_ERR_INVALID_STATE;
        }

        if (client_transfer_complete)
        {
            break;
        }

        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    }

    if (timeout == 0U)
    {
        return FSP_ERR_TIMEOUT;
    }

    client_transfer_complete = 0U;
    client_transfer_error    = 0U;

    return FSP_SUCCESS;
}

static void dump_http_payload(void * p_args)
{
    https_callback_args_t * p = (https_callback_args_t *) p_args;
    uint8_t * payload = (uint8_t *) p->payload;
    int32_t len = p->len;

    for (int i = 0; i < len; i++)
    {
        HTTPC_DEBUG_DUMP("%02x ", payload[i]);

        if ((i + 1) % 16 == 0 || i == len - 1)
        {
            int start = (i + 1) % 16 == 0 ? i - 15 : (i / 16) * 16;
            int end = i;
            int padding = 16 - (end - start + 1);

            HTTPC_DEBUG_DUMP(" ");

            for (int k = 0; k < padding; k++)
            {
                HTTPC_DEBUG_DUMP("   ");
            }

            HTTPC_DEBUG_DUMP("| ");

            for (int j = start; j <= end; j++)
            {
                if (payload[j] >= 32 && payload[j] <= 126)
                {
                    HTTPC_DEBUG_DUMP("%c", payload[j]);
                }
                else
                {
                    HTTPC_DEBUG_DUMP(".");
                }
            }

            HTTPC_DEBUG_DUMP("\n");
        }
    }

    HTTPC_DEBUG_DUMP("\n");
}

static void rm_cli_w_http_client_display_usage(void)
{
    HTTPC_PRINTF("\nUsage: HTTP Client\n");
    HTTPC_PRINTF("\x1b[93mName\x1b[0m\n");
    HTTPC_PRINTF("\thttp-client - HTTP Client\n");
    HTTPC_PRINTF("\x1b[93mSYNOPSIS\x1b[0m\n");
    HTTPC_PRINTF("\thttp-client [OPTION]...URL\n");
    HTTPC_PRINTF("\x1b[93mDESCRIPTION\x1b[0m\n");
    HTTPC_PRINTF("\tRequest client's method to URL\n");

    HTTPC_PRINTF("\t\x1b[93m-i [wlan0|wlan1]\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet interface of HTTP Client\n");
    HTTPC_PRINTF("\t\x1b[93m-status\x1b[0m\n");
    HTTPC_PRINTF("\t\tDisplay status of HTTP Client\n");
    HTTPC_PRINTF("\t\x1b[93m-help\x1b[0m\n");
    HTTPC_PRINTF("\t\tDisplay help\n");

    HTTPC_PRINTF("\t\x1b[93m-head\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest HEAD method to URI\n");
    HTTPC_PRINTF("\t\x1b[93m-get\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest GET method to URI\n");
    HTTPC_PRINTF("\t\x1b[93m-post RESOURCE\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest POST method to URI with RESOURCE\n");
    HTTPC_PRINTF("\t\x1b[93m-put RESOURCE\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest PUT method to URI with RESOURCE\n");
    HTTPC_PRINTF("\t\x1b[93m-message header + body\x1b[0m\n");
    HTTPC_PRINTF("\t\tInput header + body in free form\n");

    HTTPC_PRINTF("\t\x1b[93m-incoming Size\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet incoming buffer size of TLS Contents\n");
    HTTPC_PRINTF("\t\x1b[93m-outgoing Size\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet outgoing buffer size of TLS Contents\n");
    HTTPC_PRINTF("\t\x1b[93m-authmode\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet TLS auth_mode\n");
    HTTPC_PRINTF("\t\x1b[93m-sni <Server Name Indicator>\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet SNI for TLS extension\n");
    HTTPC_PRINTF("\t\x1b[93m-alpn <ALPN Protocols>\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet ALPN for TLS extension\n");

    return ;
}
static fsp_err_t rm_cli_w_http_client_parse_request(int argc, char * argv[], http_client_request_t * request)
{
    fsp_err_t err = FSP_SUCCESS;
    char ** cur_argv = ++argv;
    unsigned int content_len = 0;
    unsigned int sni_len = 0;
    int auth_mode = 0;
    int tls_ver = 0;
    int alpn_cnt = 0;
    int index = 0;

    request->op_code = HTTP_CLIENT_OPCODE_READY;

    for (index = 1 ; index < argc ; index++, cur_argv++)
    {
        if (**cur_argv == '-')
        {
            //Parse options
            if (strcmp("-i", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set interface\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                if (strcasecmp("WLAN0", *cur_argv) == 0)
                {
                    request->iface = WLAN0_IFACE;
                }
                else if (strcasecmp("WLAN1", *cur_argv) == 0)
                {
                    request->iface = WLAN1_IFACE;
                }
                else
                {
                    return FSP_ERR_INVALID_ARGUMENT;
                }
            }
            else if (strcmp("-head", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_HEAD;

            }
            else if (strcmp("-get", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_GET;

            }
            else if ((strcmp("-put", *cur_argv) == 0)
                     || (strcmp("-post", *cur_argv) == 0)
                     || (strcmp("-patch", *cur_argv) == 0))
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set resource\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                if (strcmp("-put", *cur_argv) == 0)
                {
                    request->op_code = HTTP_CLIENT_OPCODE_PUT;
                }
                else if (strcmp("-post", *cur_argv) == 0)
                {
                    request->op_code = HTTP_CLIENT_OPCODE_POST;
                }
                else if (strcmp("-patch", *cur_argv) == 0)
                {
                    request->op_code = HTTP_CLIENT_OPCODE_PATCH;
                }
                else
                {
                    HTTPC_DEBUG_ERR("Failed to set method\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                if (strlen(* cur_argv) > HTTPC_MAX_REQ_DATA)
                {
                    HTTPC_DEBUG_ERR("Request data is too long(%ld)\n", (long int) strlen(*cur_argv));
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                bsp_safe_strcpy((char *) request->data, *cur_argv, sizeof(request->data));

            }
            else if (strcmp("-delete", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_DELETE;

            }
            else if (strcmp("-connect", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_CONNECT;

            }
            else if (strcmp("-trace", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_TRACE;

            }
            else if (strcmp("-options", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_OPTIONS;
#ifdef __SUPPORT_HTTP_CLIENT_USER_MSG__
            }
            else if (strcmp("-message", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;
                ++cur_argv; //url
                bsp_safe_strcpy((char *) request->data, *cur_argv, sizeof(request->data));
                request->op_code = HTTP_CLIENT_OPCODE_MESSAGE;
#endif /* __SUPPORT_HTTP_CLIENT_USER_MSG__ */
            }
            else if (strcmp("-help", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_HELP;

            }
            else if (strcmp("-status", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_STATUS;

            }
            else if (strcmp("-stop", *cur_argv) == 0)
            {
                if (request->op_code != HTTP_CLIENT_OPCODE_READY)
                {
                    HTTPC_DEBUG_ERR("Invalid parameters\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                request->op_code = HTTP_CLIENT_OPCODE_STOP;

            }
            else if (strcmp("-incoming", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set incoming length\r\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                content_len = (unsigned int) atoi(*cur_argv);

                if ((content_len >= HTTPC_MIN_INCOMING_LEN) && (content_len <= HTTPC_MAX_INCOMING_LEN))
                {
                    request->https_conf.incoming_len = content_len;
                }
                else
                {
                    HTTPC_DEBUG_ERR("Invalid buffer length(%d)\r\n", content_len);
                    return FSP_ERR_INVALID_ARGUMENT;
                }

            }
            else if (strcmp("-outgoing", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set outgoing length\r\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                content_len = (unsigned int) atoi(*cur_argv);

                if ((content_len >= HTTPC_MIN_OUTGOING_LEN) && (content_len <= HTTPC_MAX_OUTGOING_LEN))
                {
                    request->https_conf.outgoing_len = content_len;
                }
                else
                {
                    HTTPC_DEBUG_ERR("Invalid buffer length(%d)\r\n", content_len);
                    return FSP_ERR_INVALID_ARGUMENT;
                }
            }
            else if (strcmp("-tlsver", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set authmode\r\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                tls_ver = atoi(*cur_argv);

                if (tls_ver <= 2)
                {
                    if (tls_ver == 0)
                    {
                        //ONLY TLS 1.2
                        request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                        request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
                    }
                    else if (tls_ver == 1)
                    {
                        //ONLY TLS 1.3;
                        request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_3;
                        request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
                    }
                    else if (tls_ver == 2)
                    {
                        //TLS 1.2 and 1.3;
                        request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                        request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
                    }
                    else
                    {
                        //ONLY TLS 1.2
                        request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                        request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
                    }
                }
                else
                {
                    HTTPC_DEBUG_ERR("Invalid tls_ver(%d)\r\n", tls_ver);
                    return FSP_ERR_INVALID_ARGUMENT;
                }
            }
            else if (strcmp("-authmode", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set authmode\r\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                auth_mode = atoi(*cur_argv);

                if (auth_mode <= 2)
                {
                    request->https_conf.auth_mode = auth_mode;
                }
                else
                {
                    HTTPC_DEBUG_ERR("Invalid authmode(%d)\r\n", auth_mode);
                    return FSP_ERR_INVALID_ARGUMENT;
                }
            }
            else if (strcmp("-sni", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set sni\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                sni_len = strlen(*cur_argv);

                if ((sni_len > 0) && (sni_len < HTTPC_MAX_SNI_LEN))
                {
                    if (request->https_conf.sni)
                    {
                        vPortFree(request->https_conf.sni);
                    }

                    request->https_conf.sni = pvPortMalloc(sni_len + 1);

                    if (!request->https_conf.sni)
                    {
                        HTTPC_DEBUG_ERR("Failed to allocate SNI(%ld)\n", (long int) sni_len);
                        return ERR_MEM;
                    }

                    request->https_conf.sni_len = (int) (sni_len + 1);
                    bsp_safe_strcpy(request->https_conf.sni, *cur_argv, (size_t) request->https_conf.sni_len);
                }
                else
                {
                    HTTPC_DEBUG_ERR("Invalid SNI length(%ld)\n", (long int) sni_len);
                    return FSP_ERR_INVALID_ARGUMENT;
                }
            }
            else if (strcmp("-alpn", *cur_argv) == 0)
            {
                if (--argc < 1)
                {
                    HTTPC_DEBUG_ERR("Failed to set alpn\n");
                    return FSP_ERR_INVALID_ARGUMENT;
                }

                ++cur_argv;

                alpn_cnt = atoi(*cur_argv);
                ++cur_argv;

                if ((alpn_cnt > 0) && (strlen(*cur_argv) > 0))
                {
                    rm_cli_w_http_client_clear_secu_conf(&request->https_conf);

                    request->https_conf.alpn = pvPortMalloc((size_t) (alpn_cnt + 1) * sizeof(char *));

                    if (!request->https_conf.alpn)
                    {
                        HTTPC_DEBUG_ERR("Failed to allocate ALPN\n");
                        return FSP_ERR_OUT_OF_MEMORY;
                    }

                    for (index = 0 ; index < alpn_cnt ; index++)
                    {
                        size_t alpn_alloc = strlen(*cur_argv) + 1;

                        request->https_conf.alpn[index] = pvPortMalloc(alpn_alloc);

                        if (!request->https_conf.alpn[index])
                        {
                            HTTPC_DEBUG_ERR("Failed to allocate ALPN#%d\n", index + 1);
                            rm_cli_w_http_client_clear_secu_conf(&request->https_conf);
                            return FSP_ERR_OUT_OF_MEMORY;
                        }

                        bsp_safe_strcpy(request->https_conf.alpn[index], *cur_argv, alpn_alloc);

                        request->https_conf.alpn_cnt++;
                        ++cur_argv;
                    }

                    request->https_conf.alpn[index] = NULL;
                }
            }
            else
            {
                HTTPC_DEBUG_ERR("Invalid parameters(%s)\n", *cur_argv);
                return FSP_ERR_INVALID_ARGUMENT;
            }
        }
        else
        {
            if (   (strncmp(*cur_argv, "http://", strlen("http://")) == 0)
                    || (strncmp(*cur_argv, "https://", strlen("https://")) == 0))
            {
                //Parse URI
                err = rm_cli_w_http_client_parse_uri((UCHAR *) *cur_argv, strlen((char *) *cur_argv), request);

                if (err != FSP_SUCCESS)
                {
                    HTTPC_DEBUG_ERR("Failed to set URI\n");
                    return err;
                }
            }
        }
    }

    if ((request->op_code == HTTP_CLIENT_OPCODE_READY) && (strlen((const char *) (request->hostname)) > 0))
    {
        request->op_code = HTTP_CLIENT_OPCODE_GET; // Default
    }

    if ((request->op_code == HTTP_CLIENT_OPCODE_PUT)
            || (request->op_code == HTTP_CLIENT_OPCODE_POST)
            || (request->op_code == HTTP_CLIENT_OPCODE_PATCH))
    {
        if (strlen((const char *) (request->data)) == 0)
        {
            return FSP_ERR_INVALID_ARGUMENT;
        }
    }

    return err;
}

fsp_err_t rm_cli_w_run_user_http_client(int argc, char * argv[])
{
    fsp_err_t err = FSP_SUCCESS;

    err = p_cli_https->open(&g_cli_https_w0_ctrl, &g_cli_https_w0_cfg);

    if (err == FSP_SUCCESS)
    {
        err = rm_cli_w_http_client_parse_request(argc, argv, &request);

        if (err != FSP_SUCCESS)
        {
            rm_cli_w_http_client_display_usage();
        }
        else
        {
            err = p_cli_https->clientSendRequest(&g_cli_https_w0_ctrl, &request);

            if (err != FSP_SUCCESS)
            {
                HTTPC_DEBUG_ERR("clientSendRequest failed(err = %d)\n", err);
            }
            else
            {
                err = wait_for_client_transfer_complete(false);

                rm_cli_w_http_client_clear_secu_conf(&request.https_conf);
            }
        }

        p_cli_https->close(&g_cli_https_w0_ctrl);
    }

    return err;
}
#endif //__SUPPORT_HTTP_CLIENT_FOR_CLI__

#ifdef __SUPPORT_HTTP_SERVER_FOR_CLI__
fsp_err_t rm_cli_w_run_user_http_server(int argc, char * argv[])
{
    fsp_err_t err = FSP_SUCCESS;

    int index = 0;
    char ** cur_argv = ++argv;

    if (argc < 1)
    {
        HTTPC_DEBUG_ERR("Invaild parameters\n");
        return FSP_ERR_INVALID_ARGUMENT;
    }

    for (index = 1 ; index < argc ; index++, cur_argv++)
    {
        //parse operation
        if (strcmp("start", *cur_argv) == 0)
        {
            err = p_cli_https->open(&g_cli_https_w0_ctrl, &g_cli_https_w0_cfg);
            /* Server Start */
            err = p_cli_https->serverStart(&g_cli_https_w0_ctrl, NULL);

        }
        else if (strcmp("stop", *cur_argv) == 0)
        {
            /* Server Stop */
            err = p_cli_https->serverStop(&g_cli_https_w0_ctrl);
            err = p_cli_https->close(&g_cli_https_w0_ctrl);
        }
        else
        {
            HTTPC_DEBUG_ERR("Invaild paramters(%s)\n", *cur_argv);
            return FSP_ERR_INVALID_ARGUMENT;
        }
    }

    return err;
}
#endif // __SUPPORT_HTTP_SERVER_FOR_CLI__

/* EOF */

