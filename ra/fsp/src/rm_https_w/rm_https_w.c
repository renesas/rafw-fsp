/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "FreeRTOS.h"
#include "portmacro.h"
#include "projdefs.h"
#include "semphr.h"
#include "task.h"
#include "mbedtls/ssl.h"
#include "altcp_tls.h"

#include "lwip/priv/tcpip_priv.h"
#include "rm_https_w.h"
#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
#include "rm_watchdog_service_w.h"
#endif

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#include "rm_pmgr_w_rtm_internal.h"
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#include "rm_wifi.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** "HTTP" in ASCII, used to determine if the HTTPS module is open. */
#define HTTPS_OPEN                      (0x48545450ULL)

/* Name of HTTP Server */
#define HTTP_SERVER_TASK_NAME           "HttpServer"
#define HTTP_SERVER_DPM_REG_NAME        HTTP_SERVER_TASK_NAME
#define HTTPS_SERVER_DPM_REG_NAME       "HttpsServer"

/* Event */
#define HTTPS_REV_START                 BIT0
#define HTTPS_REV_FINISH                BIT1

/* Stack size of HTTP Server */
#define HTTP_SERVER_TASK_SIZE           ((1024 * 8) / 4) /* WORD */
#define HTTP_SERVER_TASK_PRI            (OS_TASK_PRIORITY_USER + 3)

/* Debug Log level(Hex Dump) for HTTP Client */
#undef  ENABLE_HTTPC_DEBUG_DUMP

/* Name of HTTP Client */
#define HTTPC_XTASK_NAME                "HttpClient"
#define HTTPC_DPM_REG_NAME              HTTPC_XTASK_NAME

#define HTTPC_BUF_SNI_STR_SIZE          (63)
#define HTTPC_ALPN_CNT_WO_NVRAM         (1)

#define HTTP_SLEEP_CNT                  (30)
#define HTTP_SERVER_WAIT_TICK_CNT       (100)

#define HTTPC_MIN_INCOMING_LEN          (1024 * 1)
#define HTTPC_MAX_INCOMING_LEN          (1024 * 20)
#define HTTPC_DEF_INCOMING_LEN          (1024 * 4)
#define HTTPC_MIN_OUTGOING_LEN          (1024 * 1)
#define HTTPC_MAX_OUTGOING_LEN          (1024 * 20)
#define HTTPC_DEF_OUTGOING_LEN          (1024 * 4)

#define HTTPC_MAX_ALPN_CNT              (3)
#define HTTPC_MAX_ALPN_LEN              (24)
#define HTTPC_MAX_SNI_LEN               (64)
#define HTTPC_MAX_STOP_TIMEOUT          (300 * HTTPC_DEF_TIMEOUT)

#define EVENT_HTTPC_FINISH              (0x01)
#define EVENT_HTTPC_STOP                (0x02)
#define EVENT_HTTPC_RECV                (0x04)
#define EVENT_HTTPC_ALL                 (0xFF)

#define WLAN0_IFACE                     (0)
#define WLAN1_IFACE                     (1)
#define ETH0_IFACE                      (2)
#define NONE_IFACE                      (9)

/* Name of HTTP Server NVRAM config */
#define HTTPS_NVRAM_CONFIG_ENABLE       "HTTPS_ENABLE"

/* NVRAM name of HTTP-CLIENT TLS version */
#define HTTPC_NVRAM_CONFIG_TLS_VER      "HTTPC_TLS_VER"

/* NVRAM name of HTTP-CLIENT TLS auth_mode */
#define HTTPC_NVRAM_CONFIG_TLS_AUTH     "HTTPC_TLS_AUTHMODE"

/* NVRAM name of HTTP-CLIENT TLS alpn */
#define HTTPC_NVRAM_CONFIG_TLS_ALPN     "HTTPC_TLS_ALPN"

/* NVRAM name of the number of TLS alpn */
#define HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM "HTTPC_TLS_ALPN_NUM"

/* NVRAM name of HTTP-CLIENT TLS SNI */
#define HTTPC_NVRAM_CONFIG_TLS_SNI      "HTTPC_TLS_SNI"

/* #define ENABLE_HTTP_DEBUG */

#if defined (ENABLE_HTTP_DEBUG)
  #define HTTP_PRINTF            printf
#else
  #define HTTP_PRINTF(...)        do {} while (0)
#endif

/* Debug Log level(Information) for HTTP Client */
/* #undef  ENABLE_HTTP_DEBUG_INFO */
/* #define   ENABLE_HTTP_DEBUG_INFO */

#if defined (ENABLE_HTTP_DEBUG_INFO)
  #define HTTP_DEBUG_INFO(fmt, ...)   HTTP_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
  #define HTTP_DEBUG_INFO(...)        do {} while (0)
#endif /* ENABLE_HTTP_DEBUG_INFO */

/* Debug Log level(Error) for HTTP Client */
/* #define ENABLE_HTTP_DEBUG_ERR */

#if defined (ENABLE_HTTP_DEBUG_ERR)
  #define HTTP_DEBUG_ERR(fmt, ...)    HTTP_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
  #define HTTP_DEBUG_ERR(...)         do {} while (0)
#endif /* ENABLE_HTTP_DEBUG_ERR */


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (defined in other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
#if HTTPS_W_CFG_SERVER_ENABLE
static void      http_server_task(void * params);
static fsp_err_t http_server_set_state(https_w_instance_ctrl_t * p_ctrl, int32_t enable);
static err_t     http_server_cb_recv_fn_wrap(struct pbuf * p, err_t err);
static fsp_err_t http_server_cb_recv_fn(struct pbuf * p, err_t err);
#if CFG_PMGR
static void http_server_register(void);
static void http_server_unregister(void);
static void http_server_sleep_clear(void);
static void http_server_sleep_set(void);
#endif /* CFG_PMGR */
#endif

#if HTTPS_W_CFG_CLIENT_ENABLE
static fsp_err_t run_user_http_client(https_w_instance_ctrl_t * p_ctrl);
static void      http_client_display_usage(void);
static err_t     httpc_get_file_dns_on_tcpip_thread(struct tcpip_api_call_data * call);
static void      http_client_clear_alpn(httpc_secure_connection_t * conf);
static fsp_err_t http_client_clear_request(http_client_request_t * request);
static fsp_err_t http_client_init_conf(http_client_conf_t * config);
static void      http_client_display_request(http_client_conf_t * config, http_client_request_t * request);
static err_t     httpc_cb_recv_fn_wrap(void * arg, struct altcp_pcb * conn, struct pbuf * p, err_t err);
static fsp_err_t httpc_cb_recv_fn(void * arg, struct tcp_pcb * tpcb, struct pbuf * p, fsp_err_t err);
#endif

#if HTTPS_W_CFG_CLIENT_ENABLE
/*******************************************************************************
 * Argument block for dispatching httpc_get_file_dns() to the tcpip_thread
 * via tcpip_api_call().  All parameters that the caller would normally pass
 * directly are stored here so the on-tcpip callback can unpack them.
 ******************************************************************************/
typedef struct httpc_get_file_dns_api_msg
{
    struct tcpip_api_call_data call;     /* must be the first member */
    const char *               server_name;
    u16_t                      port;
    const char *               uri;
    const httpc_connection_t * settings;
    altcp_recv_fn              recv_fn;
    void *                     callback_arg;
    httpc_state_t **           connection;
} httpc_get_file_dns_api_msg_t;

/*******************************************************************************
 * tcpip_api_call callback — runs on the tcpip_thread.
 * Unpacks arguments from the message block and calls httpc_get_file_dns().
 ******************************************************************************/
static err_t httpc_get_file_dns_on_tcpip_thread (struct tcpip_api_call_data * call)
{
    httpc_get_file_dns_api_msg_t * msg =
        (httpc_get_file_dns_api_msg_t *) call;

    return httpc_get_file_dns(msg->server_name,
                              msg->port,
                              msg->uri,
                              msg->settings,
                              msg->recv_fn,
                              msg->callback_arg,
                              msg->connection);
}
#endif /* HTTPS_W_CFG_CLIENT_ENABLE */

#if (HTTPS_W_CFG_SERVER_ENABLE | HTTPS_W_CFG_CLIENT_ENABLE)
static void rm_https_call_callback (https_w_instance_ctrl_t * p_ctrl, https_callback_args_t * p_args);
extern void httpd_inits(struct altcp_tls_config * conf, altcp_user_recv_fn recv_fn);
#endif

#if CFG_PMGR
extern void dpm_http_server_register(void);
extern void dpm_http_server_unregister(void);
extern void dpm_http_server_sleep_clear(void);
extern void dpm_http_server_sleep_set(void);
extern int  RM_PMGR_W_dpm_sleep_ready_set(char * mod_name);
#endif /* CFG_PMGR */

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/** HTTPS function pointer  */
const https_api_t g_https_w =
{
    .open                = RM_HTTPS_W_Open,
    .serverStart         = RM_HTTPS_W_ServerStart,
    .serverStop          = RM_HTTPS_W_ServerStop,
    .serverGetStatus     = RM_HTTPS_W_ServerGetStatus,
    .callbackSet         = RM_HTTPS_W_CallbackSet,
    .clientSendRequest   = RM_HTTPS_W_ClientSendRequest,
    .close               = RM_HTTPS_W_Close,
};

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
static https_w_instance_ctrl_t * gp_https_instance_ctrl;

#if HTTPS_W_CFG_SERVER_ENABLE
#if LWIP_ALTCP_TLS
static struct altcp_tls_config * tls_srv_config = NULL;
#endif /* LWIP_ALTCP_TLS */
static TaskHandle_t             server_task_handle	= NULL;
static EventGroupHandle_t       server_event_group = NULL;
#endif /* HTTPS_W_CFG_SERVER_ENABLE */

#if HTTPS_W_CFG_CLIENT_ENABLE
static httpc_connection_t httpc_conn_settings = {0, };
static httpc_state_t *    httpc_connection    = NULL;
static TaskHandle_t       client_task_handle  = NULL;
static EventGroupHandle_t client_event_group;
#endif /* HTTPS_W_CFG_CLIENT_ENABLE */

/*******************************************************************************************************************//**
 * @addtogroup HTTPS_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
BSP_WEAK_REFERENCE void g_https0_callback(https_callback_args_t *p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    HTTP_DEBUG_INFO("[%s]\n", __func__);
}

/*******************************************************************************************************************//**
 * Open Https module.
 *
 * @retval FSP_SUCCESS                    HTTPS module was opened successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_ALREADY_OPEN           HTTPS module is already opened.
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_Open (https_ctrl_t * const p_api_ctrl, https_cfg_t const * const p_cfg)
{
    https_w_instance_ctrl_t * p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ERROR_RETURN(HTTPS_OPEN != p_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif
    p_ctrl->p_cfg   = p_cfg;

    /* Set callback and context pointers, if configured */
    p_ctrl->p_callback        = p_cfg->p_callback;
    p_ctrl->p_context         = p_cfg->p_context;
    p_ctrl->p_callback_memory = NULL;

    p_ctrl->open    = HTTPS_OPEN;

#if HTTPS_W_CFG_CLIENT_ENABLE
    memset(&p_ctrl->http_client_conf, 0x00, sizeof(http_client_conf_t));
#endif
#if (HTTPS_W_CFG_SERVER_ENABLE && CFG_PMGR)
    if (RM_PMGR_W_dpm_is_wakeup())
    {
        unsigned long long server_en = 0;

        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_HTTP_SVR_ENABLE_FLAG, NULL, &server_en, NULL);

        if (server_en)
            RM_HTTPS_W_ServerStart(p_ctrl, NULL);
    }
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Close Https module.
 *
 * @retval FSP_SUCCESS                    HTTPS module was closed successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_Close (https_ctrl_t * const p_api_ctrl)
{
    https_w_instance_ctrl_t * p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    HTTP_DEBUG_INFO("[%s]\n", __func__);

#if HTTPS_W_CFG_CLIENT_ENABLE
    memset(&p_ctrl->http_client_conf, 0x00, sizeof(http_client_conf_t));
#endif

    p_ctrl->open           = 0U;
    gp_https_instance_ctrl = NULL;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Start HTTP server.
 *
 * @retval FSP_SUCCESS                    HTTPS server was started successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 * @retval FSP_ERR_INVALID_STATE          Task was not created successfully.
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_ServerStart (https_ctrl_t * const p_api_ctrl, https_server_sec_t * p_sec)
{
#if HTTPS_W_CFG_SERVER_ENABLE
    BaseType_t                  xReturned;
    TaskHandle_t                task_handle;
    https_w_instance_ctrl_t *   p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

#if CFG_PMGR
    http_server_register();
#endif /* CFG_PMGR */

    if (server_task_handle)
    {
        task_handle         = server_task_handle;
        server_task_handle  = NULL;
        vTaskDelete(task_handle);
    }

    gp_https_instance_ctrl = p_ctrl;

    xReturned = xTaskCreate(http_server_task,
                            HTTP_SERVER_TASK_NAME,
                            HTTP_SERVER_TASK_SIZE,
                            (void *)p_sec,
                            HTTP_SERVER_TASK_PRI,
                            &server_task_handle);

    if (pdPASS != xReturned)
    {
        HTTP_DEBUG_ERR("[%s] Failed to create task(%s)\n", __func__, HTTP_SERVER_TASK_NAME);
#if CFG_PMGR
        http_server_unregister();
#endif /* CFG_PMGR */

        http_server_set_state(p_ctrl, 0);
        FSP_RETURN(FSP_ERR_INVALID_STATE);
    }
 
    HTTP_DEBUG_INFO("[%s] HTTP-Server Start!! \n", __func__);
    http_server_set_state(p_ctrl, 1);

    return FSP_SUCCESS;
#else /* HTTPS_W_CFG_SERVER_ENABLE */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
    FSP_PARAMETER_NOT_USED(p_sec);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
#endif /* HTTPS_W_CFG_SERVER_ENABLE */
}

/*******************************************************************************************************************//**
 * Stop HTTP server.
 *
 * @retval FSP_SUCCESS                    HTTPS server was stopped successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_ServerStop (https_ctrl_t * const p_api_ctrl)
{
#if HTTPS_W_CFG_SERVER_ENABLE
    TaskHandle_t              task_handle;
    https_w_instance_ctrl_t * p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    HTTP_DEBUG_INFO("[%s]\n", __func__);
    HTTP_DEBUG_INFO("HTTP-Server Stop!! \n");
    http_server_stop();
#if LWIP_ALTCP_TLS
    if (NULL != tls_srv_config)
    {
        altcp_tls_free_config(tls_srv_config);
        tls_srv_config = NULL;
    }
#endif /* LWIP_ALTCP_TLS */
#if CFG_PMGR
    http_server_unregister();
#endif /* CFG_PMGR */
    http_server_set_state(p_ctrl, 0);

    if (NULL != server_event_group)
    {
        vEventGroupDelete(server_event_group);
        server_event_group = NULL;
    }

    if (server_task_handle)
    {
        task_handle         = server_task_handle;
        server_task_handle  = NULL;
        vTaskDelete(task_handle);
    }

    return FSP_SUCCESS;
#else /* HTTPS_W_CFG_SERVER_ENABLE */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
#endif /* HTTPS_W_CFG_SERVER_ENABLE */
}

/*******************************************************************************************************************//**
 * Updates the user callback with the option to provide memory for the callback argument structure.
 *
 * @retval  FSP_SUCCESS                   Callback updated successfully.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_CallbackSet (https_ctrl_t * const p_api_ctrl,
                                      void (* p_callback)(https_callback_args_t *),
                                      void const * const p_context,
                                      https_callback_args_t * const p_callback_memory)
{
    https_w_instance_ctrl_t * p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_callback);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Store callback and context */
    p_ctrl->p_callback        = p_callback;
    p_ctrl->p_context         = p_context;
    p_ctrl->p_callback_memory = p_callback_memory;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Get HTTP server status.
 *
 * @retval FSP_SUCCESS                    Successfully get the server status.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_ServerGetStatus (https_ctrl_t * const p_api_ctrl, https_server_status_t * p_status)
{
#if HTTPS_W_CFG_SERVER_ENABLE
    https_w_instance_ctrl_t * p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;
#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_status);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    *p_status = (https_server_status_t)p_ctrl->server_status;

    return FSP_SUCCESS;
#else /* HTTPS_W_CFG_SERVER_ENABLE */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
    FSP_PARAMETER_NOT_USED(p_status);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
#endif /* HTTPS_W_CFG_SERVER_ENABLE */
}

/*******************************************************************************************************************//**
 * Send HTTP request to HTTP server.
 *
 * @retval FSP_SUCCESS                    Successfully send a HTTP request.
 * @retval FSP_ERR_ASSERTION              NULL pointer to parameters in argument.
 * @retval FSP_ERR_NOT_OPEN               HTTPS module has not been opened
 * @retval FSP_ERR_INVALID_ARGUMENT       Parameter passed into function was invalid.
 * @retval FSP_ERR_INVALID_STATE          Mutex was not created successfully.
 **********************************************************************************************************************/
fsp_err_t RM_HTTPS_W_ClientSendRequest (https_ctrl_t * const p_api_ctrl, http_client_request_t * p_request)
{
#if HTTPS_W_CFG_CLIENT_ENABLE
    fsp_err_t                   ret;
    https_w_instance_ctrl_t *   p_ctrl = (https_w_instance_ctrl_t *) p_api_ctrl;

#if HTTPS_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_request);
    FSP_ERROR_RETURN(HTTPS_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    gp_https_instance_ctrl = p_ctrl;

    if (p_ctrl->http_client_conf.status == HTTP_CLIENT_STATUS_READY)
    {
        http_client_init_conf(&p_ctrl->http_client_conf);
    }

    memcpy(&p_ctrl->http_client_conf.request, p_request, sizeof(http_client_request_t));

    ret = run_user_http_client(p_ctrl);
    if (FSP_SUCCESS != ret)
    {
        HTTP_DEBUG_ERR("run_user_http_client fail \n");
        http_client_clear_request(p_request);
#if CFG_PMGR
        RM_PMGR_W_dpm_job_name_clear(HTTPC_DPM_REG_NAME);
#endif /* CFG_PMGR */

        return ret;
    }

    HTTP_DEBUG_INFO("run_user_http_client success \n");

    return FSP_SUCCESS;
#else /* HTTPS_W_CFG_CLIENT_ENABLE */
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
    FSP_PARAMETER_NOT_USED(p_request);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
#endif /* HTTPS_W_CFG_CLIENT_ENABLE */
}

/*******************************************************************************************************************//**
 * @} (end addtogroup HTTPS_W)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

#if (HTTPS_W_CFG_SERVER_ENABLE | HTTPS_W_CFG_CLIENT_ENABLE)
/*******************************************************************************************************************//**
 * Calls user callback
 *
 * @param[in]     p_ctrl    Pointer to the instance control block.
 * @param[in]     p_args    Pointer to callback arguments with event and response set.
 **********************************************************************************************************************/
static void rm_https_call_callback (https_w_instance_ctrl_t * p_ctrl, https_callback_args_t * p_args)
{
    /* Call user callback if provided, if an event was determined, and if the driver is initialized. */
    if (NULL != p_ctrl->p_callback)
    {
        https_callback_args_t   args;

        /* Store callback arguments in memory provided by user if available.  This allows callback arguments to be
         * stored in non-secure memory so they can be accessed by a non-secure callback function. */
        https_callback_args_t * p_args_memory = p_ctrl->p_callback_memory;

        if (NULL == p_args_memory)
        {
            /* Use provided args struct on stack */
            p_args_memory  = p_args;
        }
        else
        {
            /* Save current arguments on the stack in case this is a nested interrupt. */
            args = *p_args_memory;

            /* Copy the stacked args to callback memory */
            *p_args_memory = *p_args;
        }

        p_args_memory->p_context = p_ctrl->p_context;

        /* If the project is not Trustzone Secure,
         * then it will never need to change security state in order to call the callback. */
        p_ctrl->p_callback(p_args_memory);

        if (NULL != p_ctrl->p_callback_memory)
        {
            /* Restore callback memory in case this is a nested interrupt. */
            *p_ctrl->p_callback_memory = args;
        }
    }
}
#endif /* (HTTPS_W_CFG_SERVER_ENABLE | HTTPS_W_CFG_CLIENT_ENABLE) */

#if HTTPS_W_CFG_SERVER_ENABLE
static fsp_err_t http_server_set_state(https_w_instance_ctrl_t * p_ctrl, int32_t enable)
{
#if CFG_PMGR
    RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_HTTP_SVR_ENABLE_FLAG, 0, enable);
#endif /* CFG_PMGR */
    p_ctrl->server_status = enable;

    return FSP_SUCCESS;
}

static err_t http_server_cb_recv_fn_wrap(struct pbuf * p, err_t err)
{
    fsp_err_t error;

    error = http_server_cb_recv_fn(p, err);

    return (err_t) error;
}

static fsp_err_t http_server_cb_recv_fn(struct pbuf *p, err_t err)
{
    fsp_err_t error = FSP_SUCCESS;

    xEventGroupSetBits(server_event_group, HTTPS_REV_START);

    while (NULL != p)
    {
        if ((NULL != p->payload) && (p->len > 0))
        {
            /* Call user callback */
            https_callback_args_t    args;
            args.payload    = p->payload;
            args.len        = p->len;
            args.p_param    = &err;
            args.event      = HTTPS_EVENT_SERVER_RECVED;
            rm_https_call_callback(gp_https_instance_ctrl, &args);
        }
        else
        {
            HTTP_DEBUG_ERR("\nReceive data is NULL !! \n");
            error = FSP_ERR_BUFFER_EMPTY;
            break;
        }
        p = p->next;
    }

    /* xEventGroupSetBits(server_event_group, HTTPS_REV_FINISH); */

    return error;
}

static void http_server_cb_result_fn(void *arg, err_t err)
{
    FSP_PARAMETER_NOT_USED(arg);

    if (server_event_group)
    {
        HTTP_DEBUG_INFO("event set: EVENT_HTTPC_STOP\n");
        xEventGroupSetBits(server_event_group, HTTPS_REV_FINISH);
    }

    /* Call user callback */
    https_callback_args_t    args;
    args.payload    = &err;
    args.len        = sizeof(uint32_t);
    args.p_param    = &err;
    args.event      = HTTPS_EVENT_SERVER_ERR_RESULT;
    rm_https_call_callback(gp_https_instance_ctrl, &args);

    HTTP_DEBUG_INFO("\nerr: %d\n", err);
}

static void http_server_task(void * params)
{
    TaskHandle_t         task_handle;
    EventBits_t          events;
    int32_t              timeout_sleep  = 0;
    int32_t              supend_sleep   = 0;
#if LWIP_ALTCP_TLS
    https_server_sec_t * p_sec = (https_server_sec_t *) params;
#endif /* LWIP_ALTCP_TLS */

    if (NULL != server_event_group)
    {
        vEventGroupDelete(server_event_group);
        server_event_group = NULL;
    }

    server_event_group = xEventGroupCreate();

#if LWIP_ALTCP_TLS
    if (NULL != p_sec)
    {
        tls_srv_config = altcp_tls_create_config_server_privkey_cert((const uint8_t *) p_sec->p_tls_srv_key,
                                                                    p_sec->tls_srv_key_len,
                                                                    (const uint8_t *) p_sec->p_priv_pass,
                                                                    p_sec->priv_pass_len,
                                                                    (const uint8_t *) p_sec->p_tls_srv_cert,
                                                                    p_sec->tls_srv_cert_len);
    }
    if ((p_sec && !tls_srv_config) || !server_event_group)
#else
    if (!server_event_group)
#endif /* LWIP_ALTCP_TLS */
    {
        HTTP_DEBUG_ERR("[%s] Server initialization failed\n", __func__);
        http_server_cb_result_fn(NULL, ER_INVALID_PARAMETERS);
        goto end_of_task;
    }

    httpd_init(http_server_cb_recv_fn_wrap);

#if LWIP_ALTCP_TLS
    if (NULL != p_sec)
    {
        if ((MBEDTLS_SSL_VERSION_TLS1_2 != p_sec->tls_ver_min) && 
            (MBEDTLS_SSL_VERSION_TLS1_3 != p_sec->tls_ver_min))
        {
            p_sec->tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        if ((MBEDTLS_SSL_VERSION_TLS1_2 != p_sec->tls_ver_max) && 
            (MBEDTLS_SSL_VERSION_TLS1_3 != p_sec->tls_ver_max))
        {
            p_sec->tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        if (p_sec->tls_ver_max < p_sec->tls_ver_min)
        {
            p_sec->tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        altcp_tls_config_min_tls_version(tls_srv_config, p_sec->tls_ver_min);
        altcp_tls_config_max_tls_version(tls_srv_config, p_sec->tls_ver_max);
        httpd_inits(tls_srv_config, http_server_cb_recv_fn_wrap);
    }
#endif /* LWIP_ALTCP_TLS */

#if CFG_PMGR
    RM_PMGR_W_dpm_wakeup_done(HTTP_SERVER_DPM_REG_NAME);
    RM_PMGR_W_dpm_rcv_ready_set(HTTP_SERVER_DPM_REG_NAME);
#endif /* CFG_PMGR */

    while (1)
    {
        events = xEventGroupWaitBits(server_event_group,
                                     HTTPS_REV_START | HTTPS_REV_FINISH,
                                     pdTRUE,
                                     pdFALSE,
                                     portCONVERT_MS_2_TICKS(HTTP_SERVER_WAIT_TICK_CNT));

        if (events == HTTPS_REV_START)
        {
#if CFG_PMGR
            http_server_sleep_clear();
#endif /* CFG_PMGR */
        }
        else if (events == HTTPS_REV_FINISH)
        {
            if (supend_sleep++ > HTTP_SLEEP_CNT)
            {  /* 3sec */
                supend_sleep = 0;
#if CFG_PMGR
                http_server_sleep_set();
#endif /* CFG_PMGR */
            }
        }
        else
        {
            if (timeout_sleep++ > HTTP_SLEEP_CNT)
            {   /* 3sec */
                timeout_sleep = 0;
#if CFG_PMGR
                http_server_sleep_set();
#endif /* CFG_PMGR */
            }
        }
    }

end_of_task:
    HTTP_DEBUG_INFO("server end_of_task \n");

#if LWIP_ALTCP_TLS
    if (NULL != tls_srv_config)
    {
        altcp_tls_free_config(tls_srv_config);
        tls_srv_config = NULL;
    }
#endif /* LWIP_ALTCP_TLS */

    if (NULL != server_event_group)
    {
        vEventGroupDelete(server_event_group);
        server_event_group = NULL;
    }

    if (NULL != server_task_handle)
    {
        task_handle         = server_task_handle;
        server_task_handle  = NULL;
        vTaskDelete(task_handle);
    }

    return;
}

#if CFG_PMGR
static void http_server_register(void)
{
    RM_PMGR_W_dpm_job_name_set(HTTP_SERVER_DPM_REG_NAME, HTTP_SVR_PORT);
    RM_PMGR_W_dpm_job_name_set(HTTPS_SERVER_DPM_REG_NAME, HTTPS_SVR_PORT);

    RM_WIFI_dpm_tcp_port_filter_set(HTTP_SVR_PORT);
    RM_WIFI_dpm_tcp_port_filter_set(HTTPS_SVR_PORT);

}

static void http_server_unregister(void)
{
    RM_PMGR_W_dpm_job_name_clear(HTTP_SERVER_DPM_REG_NAME);
    RM_PMGR_W_dpm_job_name_clear(HTTPS_SERVER_DPM_REG_NAME);

    RM_WIFI_dpm_tcp_port_delete(HTTP_SVR_PORT);
    RM_WIFI_dpm_tcp_port_delete(HTTPS_SVR_PORT);
}

static void http_server_sleep_clear(void)
{
    RM_PMGR_W_dpm_sleep_ready_clear(HTTP_SERVER_DPM_REG_NAME);
    RM_PMGR_W_dpm_sleep_ready_clear(HTTPS_SERVER_DPM_REG_NAME);
}

static void http_server_sleep_set(void)
{
    RM_PMGR_W_dpm_sleep_ready_set(HTTP_SERVER_DPM_REG_NAME);
    RM_PMGR_W_dpm_sleep_ready_set(HTTPS_SERVER_DPM_REG_NAME);
}
#endif /* CFG_PMGR */
#endif /* HTTPS_W_CFG_SERVER_ENABLE */

#if HTTPS_W_CFG_CLIENT_ENABLE

static void http_client_clear_alpn(httpc_secure_connection_t *conf)
{
    (void)conf;

    return;
}

static void http_client_clear_https_conf(httpc_secure_connection_t *conf)
{
    if (conf)
    {
        http_client_clear_alpn(conf);

        conf->auth_mode     = MBEDTLS_SSL_VERIFY_NONE;
        conf->incoming_len  = HTTPC_DEF_INCOMING_LEN;
        conf->outgoing_len  = HTTPC_DEF_OUTGOING_LEN;
        conf->tls_ver_min   = MBEDTLS_SSL_VERSION_TLS1_2;
        conf->tls_ver_max   = MBEDTLS_SSL_VERSION_TLS1_2;
    }

    return;
}

static fsp_err_t http_client_clear_request(http_client_request_t *request)
{
    request->op_code    = HTTP_CLIENT_OPCODE_READY;
    request->iface      = WLAN0_IFACE;
    request->port       = HTTP_SERVER_PORT;
    request->insecure   = pdFALSE;

    memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
    memset(request->path, 0x00, HTTPC_MAX_PATH_LEN);
    memset(request->data, 0x00, HTTPC_MAX_REQ_DATA);
    memset(request->username, 0x00, HTTPC_MAX_NAME);
    memset(request->password, 0x00, HTTPC_MAX_PASSWORD);

    http_client_clear_https_conf(&request->https_conf);

    return FSP_SUCCESS;
}

static fsp_err_t http_client_init_conf(http_client_conf_t *config)
{
    HTTP_DEBUG_INFO("Init http client configuration\n");

    config->status          = HTTP_CLIENT_STATUS_READY;

    return http_client_clear_request(&config->request);
}

static fsp_err_t http_client_execute_request(http_client_conf_t *config, http_client_request_t *request)
{
    if (request->op_code == HTTP_CLIENT_OPCODE_READY)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    switch (request->op_code)
    {
        case HTTP_CLIENT_OPCODE_STATUS:
            http_client_display_request(config, &config->request);
            break;

        case HTTP_CLIENT_OPCODE_HELP:
            http_client_display_usage();
            break;

        default:
            if (HTTP_CLIENT_STATUS_PROGRESS == config->status)
            {
                HTTP_DEBUG_INFO("Http client is progressing previous request\n");
                return FSP_ERR_INVALID_STATE;
            }

            break;
    }

    return FSP_SUCCESS;
}

static int32_t httpc_ascii_to_num(int32_t ret_base, char *src, int32_t src_len)
{
    int32_t base    = 1;
    int32_t num     = 0;

    for (int32_t idx = src_len - 1 ; idx >= 0; idx--)
    {
        if ((src[idx] >= '0') && (src[idx] <= '9'))
        {
            num += (src[idx] - '0') * base;
            base = base * ret_base;
        }
        else if ((src[idx] >= 'A') && (src[idx] <= 'F'))
        {
            num += (src[idx] - 'A' + 10) * base;
            base = base * ret_base;
        }
        else if ((src[idx] >= 'a') && (src[idx] <= 'f'))
        {
            num += (src[idx] - 'a' + 10) * base;
            base = base * ret_base;
        }
    }

    return num;
}

static err_t httpc_cb_headers_done_fn(httpc_state_t *connection, void *arg, struct pbuf *hdr, u16_t hdr_len, u32_t content_len)
{
    FSP_PARAMETER_NOT_USED(connection);
    FSP_PARAMETER_NOT_USED(arg);
    FSP_PARAMETER_NOT_USED(content_len);

    err_t                   error     = ERR_OK;
    http_client_receive_t * p_receive = &gp_https_instance_ctrl->http_client_conf.receive;

    if (client_event_group)
    {
        HTTP_DEBUG_INFO("event set: EVENT_HTTPC_RECV\n");
        xEventGroupSetBits(client_event_group, EVENT_HTTPC_RECV);
    }

    if ((NULL != hdr->payload) && (hdr_len > 0))
    {
        if (strstr(hdr->payload, "Transfer-Encoding: chunked") != NULL)
        {
            p_receive->chunked = pdTRUE;
        }
        else
        {
            p_receive->chunked = pdFALSE;
        }
        p_receive->chunked_len        = 0;
        p_receive->chunked_remain_len = 0;

        /* Call user callback */
        https_callback_args_t    args;
        args.payload    = hdr->payload;
        args.len        = hdr_len;
        args.p_param    = NULL;
        args.event      = HTTPS_EVENT_CLIENT_GET_DONE;
        rm_https_call_callback(gp_https_instance_ctrl, &args);

#if defined (ENABLE_HTTPC_DEBUG_DUMP)
        hex_dump((UCHAR*)hdr->payload, (UINT)hdr_len);
#endif /* ENABLE_HTTPC_DEBUG_DUMP */

        HTTP_DEBUG_INFO("\nhdr_len : %d, content_len : %d\n", hdr_len, (int)content_len);
    }
    else
    {
        HTTP_DEBUG_ERR("\nFailed to receive http header!! \n");
        error = ERR_UNKNOWN;
    }

    return error;
}

static void httpc_cb_result_fn(void *arg, httpc_result_t httpc_result,
                               uint32_t rx_content_len, uint32_t srv_res, err_t err)
{
    FSP_PARAMETER_NOT_USED(arg);
    FSP_PARAMETER_NOT_USED(srv_res);

    if (client_event_group)
    {
        HTTP_DEBUG_INFO("event set: EVENT_HTTPC_STOP\n");
        xEventGroupSetBits(client_event_group, EVENT_HTTPC_STOP);
    }

    /* Call user callback */
    https_callback_args_t   args;

    uint32_t result = httpc_result;

    args.payload    = &result;
    args.len        = rx_content_len;
    args.p_param    = &err;
    args.event      = HTTPS_EVENT_CLIENT_RESULT;
    rm_https_call_callback(gp_https_instance_ctrl, &args);

    HTTP_DEBUG_INFO("\nhttpc_result: %d, received: %d byte, err: %d\n", httpc_result, (int)rx_content_len, err);

#if CFG_PMGR
    /* DPM mode */
    RM_PMGR_W_dpm_sleep_ready_set(HTTPC_DPM_REG_NAME);
#endif /* CFG_PMGR */

    return;
}

static fsp_err_t httpc_parse_transfer_encoding_chunked(http_client_conf_t *config, char *payload, int32_t len)
{
    int32_t cnt             = 0;
    int32_t pure_data_len   = 0;
    char *  p_payload       = NULL;
    char *  offset          = NULL;

    https_callback_args_t   args;

    http_client_receive_t * receive = &config->receive;

    p_payload   = payload;
    offset      = payload;

    if (receive->chunked_remain_len > 0)
    {
        if (receive->chunked_remain_len > len)
        {
            cnt = len;
        }
        else
        {
            cnt = receive->chunked_remain_len;
        }

        receive->chunked_remain_len -= cnt;
        pure_data_len                = cnt;

        /* Users can copy the parsed chunked payload from here. */
        HTTP_DEBUG_INFO("Decoded chunked payload size : %ld\n", pure_data_len);

        /* Call user callback */
        args.payload    = payload;
        args.len        = pure_data_len;
        args.p_param    = NULL;
        args.event      = HTTPS_EVENT_CLIENT_RECVED_DECODED;
        rm_https_call_callback(gp_https_instance_ctrl, &args);

#if defined (ENABLE_HTTPC_DEBUG_DUMP)
        hex_dump((UCHAR*)payload, (UINT)pure_data_len);
#endif /* (ENABLE_HTTPC_DEBUG_DUMP) */
    }

    p_payload += cnt;
    offset    += cnt;

    if ((0x0d == *offset) && (0x0a == *(offset + 1)))
    {
        cnt      += 2;   /* 0x0d 0x0a */
        offset   += 2;   /* 0x0d 0x0a */
        p_payload = offset;
    }

    while (len > cnt)
    {
        if ((0x0d == *offset) && (0x0a == *(offset + 1)))
        {
            receive->chunked_len = httpc_ascii_to_num(16, p_payload, (offset - p_payload));
            if (receive->chunked_len == 0)
            {
                if ((0x0d == *(offset + 2)) && (0x0a == *(offset + 3)))
                {
                    /* finish */
                    return FSP_SUCCESS;
                }
                else
                {
                    HTTP_DEBUG_ERR("\nEnd of chunked data is unknown !! \n");
                    return FSP_ERR_INVALID_DATA;
                }
            }

            cnt         += 2;   /* 0x0d 0x0a */
            offset      += 2;   /*0x0d 0x0a */
            p_payload    = offset;

            if (receive->chunked_len > (len - cnt))
            {
                receive->chunked_remain_len = receive->chunked_len - (len - cnt);
                pure_data_len               = (len - cnt);
            }
            else
            {
                pure_data_len = receive->chunked_len;
            }

            /* Users can copy the parsed chunked payload from here. */
            HTTP_DEBUG_INFO("Decoded chunked payload size : %ld\n", pure_data_len);

            /* Call user callback */
            args.payload    = p_payload;
            args.len        = pure_data_len;
            args.p_param    = NULL;
            args.event      = HTTPS_EVENT_CLIENT_RECVED_DECODED;
            rm_https_call_callback(gp_https_instance_ctrl, &args);

#if defined (ENABLE_HTTPC_DEBUG_DUMP)
            hex_dump((UCHAR*)p_payload, (UINT)pure_data_len);
#endif /* (ENABLE_HTTPC_DEBUG_DUMP) */

            cnt      += receive->chunked_len;
            offset   += receive->chunked_len;
            p_payload = offset;

            /* end of data */
            if ((0x0d == *offset) && (0x0a == *(offset+1)))
            {
                cnt      += 2;   /* 0x0d 0x0a */
                offset   += 2;   /* 0x0d 0x0a */
                p_payload = offset;
                /* next data */
                continue;
            }
            else
            {
                if (0 == receive->chunked_remain_len)
                {
                    HTTP_DEBUG_ERR("End of data unknown(CRLF)\n");
                    return FSP_ERR_INVALID_DATA;
                }
            }
        }

        cnt ++;
        offset++;
    }

    return FSP_SUCCESS;
}

static err_t httpc_cb_recv_fn_wrap(void * arg, struct altcp_pcb * conn, struct pbuf * p, err_t err)
{
    fsp_err_t error;

    error = httpc_cb_recv_fn(arg, (struct tcp_pcb *) conn, p, err);

    return (err_t) error;
}

static fsp_err_t httpc_cb_recv_fn(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, fsp_err_t err)
{
    FSP_PARAMETER_NOT_USED(arg);
    FSP_PARAMETER_NOT_USED(tpcb);

    fsp_err_t               error   = FSP_SUCCESS;

    http_client_receive_t * receive = &gp_https_instance_ctrl->http_client_conf.receive;

    if (client_event_group)
    {
        if (gp_https_instance_ctrl->http_client_conf.request.op_code == HTTP_CLIENT_OPCODE_STOP)
        {
            HTTP_DEBUG_ERR("event set: EVENT_HTTPC_STOP\n");
            xEventGroupSetBits(client_event_group, EVENT_HTTPC_STOP);
            HTTP_DEBUG_ERR("\nStop by command. \n");
            return FSP_ERR_ABORTED;
        }

        HTTP_DEBUG_INFO("event set: EVENT_HTTPC_RECV\n");
        xEventGroupSetBits(client_event_group, EVENT_HTTPC_RECV);
    }

    while (NULL != p)
    {
        if ((NULL != p->payload) && (p->len > 0))
        {
            /* Transfer-Encoding : chunked */
            if (pdTRUE == receive->chunked)
            {
                error = httpc_parse_transfer_encoding_chunked(&gp_https_instance_ctrl->http_client_conf, p->payload, p->len);
                if (FSP_SUCCESS != error)
                {
                    return error;
                }
            }
            else
            {
                /* Users can copy the payload from here. */
                /* Call user callback */
                https_callback_args_t    args;

                args.payload    = p->payload;
                args.len        = p->len;
                args.p_param    = &err;
                args.event      = HTTPS_EVENT_CLIENT_RECVED;
                rm_https_call_callback(gp_https_instance_ctrl, &args);

#if defined (ENABLE_HTTPC_DEBUG_DUMP)
                hex_dump((UCHAR*)p->payload, (UINT)p->len);
#endif /* (ENABLE_HTTPC_DEBUG_DUMP) */
            }

            error = err;
            HTTP_DEBUG_INFO("Receive length: %d\n", p->tot_len);
        }
        else
        {
            HTTP_DEBUG_ERR("\nReceive data is NULL !! \n");
            error = FSP_ERR_BUFFER_EMPTY;
            break;
        }

        p = p->next;
    }

    return error;
}

static void http_client_process_request(void *arg)
{
    err_t         error         = ERR_OK;
    const int32_t max_timeout   = HTTPC_MAX_STOP_TIMEOUT;
    const int32_t timeout       = HTTPC_DEF_TIMEOUT;
    int32_t       cur_timeout   = 0;

    http_client_conf_t *    conf    = (http_client_conf_t *)arg;
    http_client_request_t * request = &(conf->request);

    ULONG        events;
    TaskHandle_t task_handle;

    int32_t index = 0;

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
#endif

    HTTP_DEBUG_INFO("Start of Task\r\n");

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, &task_wdog_id);
    if (task_wdog_id == 0xFFU)
    {
        HTTP_DEBUG_ERR("Failed to register watchdog service\n");
    }
#endif

    /* Initialize ... */
    memset(&httpc_conn_settings, 0, sizeof(httpc_connection_t));
    httpc_connection = NULL;

    conf->status = HTTP_CLIENT_STATUS_PROGRESS;

    if (!client_event_group)
    {
        client_event_group = xEventGroupCreate();
        if (NULL == client_event_group)
        {
            goto finish;
        }
    }

    switch (request->op_code)
    {
        case HTTP_CLIENT_OPCODE_HEAD:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "HEAD", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_GET:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "GET", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_PUT:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "PUT", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_POST:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "POST", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_PATCH:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "PATCH", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_DELETE:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "DELETE", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_CONNECT:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "CONNECT", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_TRACE:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "TRACE", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_OPTIONS:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "OPTIONS", sizeof(httpc_conn_settings.method));
            break;

        case HTTP_CLIENT_OPCODE_MESSAGE:
            bsp_safe_strcpy((char *)httpc_conn_settings.method, "MESSAGE", sizeof(httpc_conn_settings.method));
            break;

        default:
            HTTP_DEBUG_ERR("Unknown Method\n");
            goto finish;
    }

    httpc_conn_settings.use_proxy       = 0;
    httpc_conn_settings.altcp_allocator = NULL;

    httpc_conn_settings.headers_done_fn = httpc_cb_headers_done_fn;
    httpc_conn_settings.result_fn       = httpc_cb_result_fn;
    httpc_conn_settings.insecure        = (uint8_t)request->insecure;

    if ((request->op_code == HTTP_CLIENT_OPCODE_PUT)
        || (request->op_code == HTTP_CLIENT_OPCODE_POST)
        || (request->op_code == HTTP_CLIENT_OPCODE_PATCH)
        || (request->op_code == HTTP_CLIENT_OPCODE_MESSAGE))
    {
        memset(httpc_conn_settings.post_msg, 0x00, strlen((char *)httpc_conn_settings.post_msg));
        if (strlen((char *) request->data) > 0) {
            memcpy(httpc_conn_settings.post_msg, request->data, strlen((char *) request->data));
        }
    }

    if (httpc_conn_settings.insecure)
    {
        memset(&httpc_conn_settings.tls_settings, 0x00, sizeof(httpc_secure_connection_t));
        memcpy(&httpc_conn_settings.tls_settings, &request->https_conf,
               sizeof(httpc_secure_connection_t));

        if (0 == httpc_conn_settings.tls_settings.incoming_len)
        {
            httpc_conn_settings.tls_settings.incoming_len = HTTPC_MAX_INCOMING_LEN;
        }

        if (0 == httpc_conn_settings.tls_settings.outgoing_len)
        {
            httpc_conn_settings.tls_settings.outgoing_len = HTTPC_DEF_OUTGOING_LEN;
        }

        if (httpc_conn_settings.tls_settings.auth_mode > MBEDTLS_SSL_VERIFY_UNSET)
        {
            httpc_conn_settings.tls_settings.auth_mode = MBEDTLS_SSL_VERIFY_NONE;
        }

        if ((httpc_conn_settings.tls_settings.tls_ver_min != MBEDTLS_SSL_VERSION_TLS1_2) && 
            (httpc_conn_settings.tls_settings.tls_ver_min != MBEDTLS_SSL_VERSION_TLS1_3))
        {
            httpc_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        if ((httpc_conn_settings.tls_settings.tls_ver_max != MBEDTLS_SSL_VERSION_TLS1_2) && 
            (httpc_conn_settings.tls_settings.tls_ver_max != MBEDTLS_SSL_VERSION_TLS1_3))
        {
            httpc_conn_settings.tls_settings.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        if (httpc_conn_settings.tls_settings.tls_ver_max < httpc_conn_settings.tls_settings.tls_ver_min)
        {
            httpc_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        if (NULL == httpc_conn_settings.tls_settings.sni)
        {
            httpc_conn_settings.tls_settings.sni_len = 0;
        }

        if (NULL == httpc_conn_settings.tls_settings.alpn)
        {
            httpc_conn_settings.tls_settings.alpn_cnt = 0;
        }

        if (HTTPC_MAX_ALPN_CNT < httpc_conn_settings.tls_settings.alpn_cnt)
        {
            httpc_conn_settings.tls_settings.alpn_cnt = HTTPC_MAX_ALPN_CNT;
        }

        if (httpc_conn_settings.tls_settings.alpn_cnt > 0)
        {
            for (index = 0 ; index < httpc_conn_settings.tls_settings.alpn_cnt ; index++)
            {
                if (NULL == httpc_conn_settings.tls_settings.alpn[index])
                {
                    HTTP_DEBUG_ERR("Failed to pointer to ALPN#%ld\n", index + 1);
                    goto finish;
                }
            }
        }
    }

    HTTP_DEBUG_INFO("HTTP-Client request to %s\n", request->path);

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->suspend(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
#endif

    /* Marshal httpc_get_file_dns() onto the tcpip_thread so that all raw TCP
     * API calls (tcp_connect, tcp_output, etc.) run on the correct thread.
     * See: lwIP threading contract. */
    {
        httpc_get_file_dns_api_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.server_name  = (const char *) (&request->hostname[0]);
        msg.port         = (u16_t) request->port;
        msg.uri          = (const char *) (&request->path[0]);
        msg.settings     = &httpc_conn_settings;
        msg.recv_fn      = httpc_cb_recv_fn_wrap;
        msg.callback_arg = NULL;
        msg.connection   = &httpc_connection;

        error = tcpip_api_call(httpc_get_file_dns_on_tcpip_thread, &msg.call);
    }

    HTTP_DEBUG_INFO("[httpc_get_file_dns] error:%d\n", error);

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->resumeAndNotify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
#endif

    if (error != ERR_OK)
    {
        HTTP_DEBUG_ERR("Request Error (%d)\r\n", error);
        //xEventGroupSetBits(client_event_group, EVENT_HTTPC_FINISH);
        httpc_cb_result_fn(NULL, HTTPC_RESULT_LOCAL_ABORT, sizeof(uint32_t), 0, ERR_UNKNOWN);
    }

    while (cur_timeout < max_timeout)
    {
        events = xEventGroupWaitBits(client_event_group,
                                    EVENT_HTTPC_ALL,
                                    pdTRUE,
                                    pdFALSE,
                                    timeout);

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl, g_wifi_cfg.p_watchdog_service->p_cfg, task_wdog_id);
#endif

        if (events)
        {
            HTTP_DEBUG_INFO("Recevied Event(0x%lx)\r\n", events);
        }

        if (events & EVENT_HTTPC_FINISH)
        {
            break;
        }
        else if (events & EVENT_HTTPC_STOP)
        {
            break;
        }
        else 
        {
            if (events & EVENT_HTTPC_RECV)
            {
                cur_timeout = 0;
            }
        }

        cur_timeout += timeout;
    }

finish:

    conf->status = HTTP_CLIENT_STATUS_WAIT;

    HTTP_DEBUG_INFO("End of Task\r\n");

    http_client_clear_https_conf(&httpc_conn_settings.tls_settings);

    if (client_event_group)
    {
        HTTP_DEBUG_INFO("event delete: client_event_group\n");
        vEventGroupDelete(client_event_group);
        client_event_group = NULL;
    }

#if HTTPS_W_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
    task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
#endif

    if (client_task_handle)
    {
        task_handle         = client_task_handle;
        client_task_handle  = NULL;
        vTaskDelete(task_handle);
    }
}

static fsp_err_t run_user_http_client(https_w_instance_ctrl_t * p_ctrl)
{
    fsp_err_t   err = FSP_SUCCESS;

    BaseType_t  xReturned;

    err = http_client_execute_request(&p_ctrl->http_client_conf, &p_ctrl->http_client_conf.request);

    if ((FSP_ERR_ABORTED != err) && (p_ctrl->http_client_conf.request.op_code == HTTP_CLIENT_OPCODE_STOP))
    {
#if CFG_PMGR
        /* DPM mode */
        RM_PMGR_W_dpm_job_name_clear(HTTPC_DPM_REG_NAME);
#endif /* CFG_PMGR */

        return FSP_SUCCESS;
    }
    else
    {
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }

    xReturned = xTaskCreate(http_client_process_request,
                            HTTPC_XTASK_NAME,
                            HTTPC_STACK_SZ,
                            &p_ctrl->http_client_conf,
                            HTTPC_TASK_PRI,
                            &client_task_handle);
    if (pdPASS != xReturned)
    {
        HTTP_DEBUG_ERR(RED_COLOR "Failed task create %s\r\n" CLEAR_COLOR, "HttpClient");
        return FSP_ERR_INVALID_STATE;
    }

    HTTP_DEBUG_INFO("HTTP-Client Start!! \n");

    return FSP_SUCCESS;
}

static void http_client_display_usage(void)
{
    HTTP_PRINTF("\nUsage: HTTP Client\n");
    HTTP_PRINTF("\x1b[93mName\x1b[0m\n");
    HTTP_PRINTF("\thttp-client - HTTP Client\n");
    HTTP_PRINTF("\x1b[93mSYNOPSIS\x1b[0m\n");
    HTTP_PRINTF("\thttp-client [OPTION]...URL\n");
    HTTP_PRINTF("\x1b[93mDESCRIPTION\x1b[0m\n");
    HTTP_PRINTF("\tRequest client's method to URL\n");

    HTTP_PRINTF("\t\x1b[93m-i [wlan0|wlan1]\x1b[0m\n");
    HTTP_PRINTF("\t\tSet interface of HTTP Client\n");
    HTTP_PRINTF("\t\x1b[93m-status\x1b[0m\n");
    HTTP_PRINTF("\t\tDisplay status of HTTP Client\n");
    HTTP_PRINTF("\t\x1b[93m-help\x1b[0m\n");
    HTTP_PRINTF("\t\tDisplay help\n");

    HTTP_PRINTF("\t\x1b[93m-head\x1b[0m\n");
    HTTP_PRINTF("\t\tRequest HEAD method to URI\n");
    HTTP_PRINTF("\t\x1b[93m-get\x1b[0m\n");
    HTTP_PRINTF("\t\tRequest GET method to URI\n");
    HTTP_PRINTF("\t\x1b[93m-post RESOURCE\x1b[0m\n");
    HTTP_PRINTF("\t\tRequest POST method to URI with RESOURCE\n");
    HTTP_PRINTF("\t\x1b[93m-put RESOURCE\x1b[0m\n");
    HTTP_PRINTF("\t\tRequest PUT method to URI with RESOURCE\n");
    HTTP_PRINTF("\t\x1b[93m-message header + body\x1b[0m\n");
    HTTP_PRINTF("\t\tInput header + body in free form\n");

    HTTP_PRINTF("\t\x1b[93m-incoming Size\x1b[0m\n");
    HTTP_PRINTF("\t\tSet incoming buffer size of TLS Contents\n");
    HTTP_PRINTF("\t\x1b[93m-outgoing Size\x1b[0m\n");
    HTTP_PRINTF("\t\tSet outgoing buffer size of TLS Contents\n");
    HTTP_PRINTF("\t\x1b[93m-authmode\x1b[0m\n");
    HTTP_PRINTF("\t\tSet TLS auth_mode\n");
    HTTP_PRINTF("\t\x1b[93m-sni <Server Name Indicator>\x1b[0m\n");
    HTTP_PRINTF("\t\tSet SNI for TLS extension\n");
    HTTP_PRINTF("\t\x1b[93m-alpn <ALPN Protocols>\x1b[0m\n");
    HTTP_PRINTF("\t\tSet ALPN for TLS extension\n");

    return ;
}

static void http_client_display_request(http_client_conf_t *config,
                                 http_client_request_t *request)
{
    HTTP_PRINTF("\n%-30s\n", "***** HTTP Client Requst *****");

    HTTP_PRINTF("\n%-30s:", "HTTP Client Status");
    if (config->status == HTTP_CLIENT_STATUS_READY)
    {
        HTTP_PRINTF("\t%s\n", "Ready");
    }
    else if (config->status == HTTP_CLIENT_STATUS_WAIT)
    {
        HTTP_PRINTF("\t%s\n", "Wait");
    }
    else if (config->status == HTTP_CLIENT_STATUS_PROGRESS)
    {
        HTTP_PRINTF("\t%s\n", "Progress");
    }
    else
    {
        HTTP_PRINTF("\t%s(%d)\n", "Unknown", config->status);
    }

    HTTP_PRINTF("%-30s:", "Interface");
    if (WLAN0_IFACE == request->iface)
    {
        HTTP_PRINTF("\t%s\n", "wlan0");
    }
    else if (WLAN1_IFACE == request->iface)
    {
        HTTP_PRINTF("\t%s\n", "wlan1");
    }
    else
    {
        HTTP_PRINTF("\t%s(%ld)\n", "Unknown", request->iface);
    }

    if (strlen((char *)request->hostname))
    {
        HTTP_PRINTF("%-30s:\t%s\n", "Host Name", request->hostname);
    }

    if (strlen((char *)request->username))
    {
        HTTP_PRINTF("%-30s:\t%s\n", "User Name", request->username);
    }

    if (strlen((char *)request->password) > 0)
    {
        HTTP_PRINTF("%-30s:\t%s\n", "User Password", request->password);
    }

    if (strlen((char *)request->path))
    {
        HTTP_PRINTF("%-30s:\t%s\n", "Path", request->path);
    }

    if (strlen((char *)request->data))
    {
        HTTP_PRINTF("%-30s:\t%s\n", "Data", request->data);
    }

    HTTP_PRINTF("%-30s:\t%s\n", "Secure", request->insecure ? "Yes" : "No");
    HTTP_PRINTF("%-30s:\t%d\n", "Incoming buffer length", (int) request->https_conf.incoming_len);
    HTTP_PRINTF("%-30s:\t%d\n", "Outgoing buffer length", (int) request->https_conf.outgoing_len);
    HTTP_PRINTF("%-30s:\t%d\n", "Auth Mode", (int) request->https_conf.auth_mode);
    if (request->https_conf.sni_len)
    {
        HTTP_PRINTF("%-30s:\t%s(%ld)\n", "SNI", request->https_conf.sni,
                     (int32_t)strlen(request->https_conf.sni));
    }

    if (request->https_conf.alpn_cnt)
    {
        HTTP_PRINTF("%-30s:\t%ld\n", "ALPN", (int32_t) request->https_conf.alpn_cnt);
        for (int32_t idx = 0 ; idx < request->https_conf.alpn_cnt ; idx++)
        {
            HTTP_PRINTF("\t* %s(%ld)\n", request->https_conf.alpn[idx],
                         (int32_t)strlen(request->https_conf.alpn[idx]));
        }
    }

    HTTP_PRINTF("%-30s:", "Op code");
    switch (request->op_code)
    {
        case HTTP_CLIENT_OPCODE_READY:
            HTTP_PRINTF("\t%s\n", "READY");
            break;

        case HTTP_CLIENT_OPCODE_HEAD:
            HTTP_PRINTF("\t%s\n", "HEAD");
            break;

        case HTTP_CLIENT_OPCODE_GET:
            HTTP_PRINTF("\t%s\n", "GET");
            break;

        case HTTP_CLIENT_OPCODE_PUT:
            HTTP_PRINTF("\t%s\n", "PUT");
            break;

        case HTTP_CLIENT_OPCODE_POST:
            HTTP_PRINTF("\t%s\n", "POST");
            break;

        case HTTP_CLIENT_OPCODE_PATCH:
            HTTP_PRINTF("\t%s\n", "PATCH");
            break;

        case HTTP_CLIENT_OPCODE_DELETE:
            HTTP_PRINTF("\t%s\n", "DELETE");
            break;

        case HTTP_CLIENT_OPCODE_CONNECT:
            HTTP_PRINTF("\t%s\n", "CONNECT");
            break;

        case HTTP_CLIENT_OPCODE_TRACE:
            HTTP_PRINTF("\t%s\n", "TRACE");
            break;

        case HTTP_CLIENT_OPCODE_OPTIONS:
            HTTP_PRINTF("\t%s\n", "OPTIONS");
            break;

        case HTTP_CLIENT_OPCODE_MESSAGE:
            HTTP_PRINTF("\t%s\n", "MESSAGE");
            break;

        case HTTP_CLIENT_OPCODE_STATUS:
            HTTP_PRINTF("\t%s\n", "STATUS");
            break;

        case HTTP_CLIENT_OPCODE_HELP:
            HTTP_PRINTF("\t%s\n", "HELP");
            break;

        case HTTP_CLIENT_OPCODE_STOP:
            HTTP_PRINTF("\t%s\n", "STOP");
            break;

        default:
            HTTP_PRINTF("\t%s(%d)\n", "Unknown", request->op_code);
            break;
    }

    return;
}

#endif /* HTTPS_W_CFG_CLIENT_ENABLE */

