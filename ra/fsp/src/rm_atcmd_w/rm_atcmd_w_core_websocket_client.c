/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_websocket_client.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include <string.h>

#if defined(__SUPPORT_WEBSOCKET_CLIENT_FOR_ATCMD__)

 #include "net_network_main.h"
 #include "iface_defs.h"
 #include "net_dhcp_client.h"
 #include "rm_wifi.h"
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
 #endif                                /* CFG_PMGR */

 #define RM_ATCMD_W_CORE_WEBSOCKET_CLIENT_ERROR(fmt, ...)    printf("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)

extern websocketParamForRtm * rtmwebsocketParamPtr;
extern websocketParamForRtm   websocketParams;

 #if CFG_PMGR
extern UINT websocket_client_assign_dpm_user_mem(void);
extern int  websocket_client_read_from_dpm_user_mem(void);
extern int  websocket_client_send_text(websocket_client_handle_t client, const char * data, int len,
                                       TickType_t timeout);
extern bool websocket_client_is_connected(websocket_client_handle_t client);

 #endif                                /* CFG_PMGR */

websocket_client_handle_t client;

void websocket_event_handler (websocket_client_event_id_t event_id, websocket_client_event_data_t * event_data)
{
    websocket_client_event_data_t * data;
    atcmd_w_ctrl_t                * p_at_ctrl = NULL;
    char resp_str[32] = {0x00, };

    if (event_data == NULL)
    {
        WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "event data is NULL\n");

        return;
    }

    data = (websocket_client_event_data_t *) event_data;
    if (data->user_context)
    {
        p_at_ctrl = (atcmd_w_ctrl_t *) data->user_context;
    }

    switch (event_id)
    {
        case WEBSOCKET_CLIENT_EVENT_CONNECTED:
        {
            WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Connected\n");
            if (p_at_ctrl)
            {
                bsp_safe_strcpy(resp_str, "\r\n+NWWSC:1\r\n\r\nOK\r\n", sizeof(resp_str));
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
            }

            break;
        }

        case WEBSOCKET_CLIENT_EVENT_DISCONNECTED:
        {
            WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Disconnected\n");
            if (p_at_ctrl)
            {
                bsp_safe_strcpy(resp_str, "\r\n+NWWSC:0\r\n", sizeof(resp_str));
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
            }

            break;
        }

        case WEBSOCKET_CLIENT_EVENT_ADD_HEADER:
        {
            WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_client_add_header()\n");
            break;
        }

        case WEBSOCKET_CLIENT_EVENT_DATA:
        {
            WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Data\n");
            WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", "Received opcode=%d\n", data->op_code);

            if ((data->op_code == WS_TRANSPORT_OPCODES_CLOSE) && (data->data_len == 2))
            {
                WS_LOGW("WEBSOCKET_CLIENT_FOR_ATCMD",
                        "Received closed message with code=%d\n",
                        256 * data->data_ptr[0] + data->data_ptr[1]);
            }
            else
            {
                WS_LOGW("WEBSOCKET_CLIENT_FOR_ATCMD", "Received=%.*s\n", data->data_len, (char *) data->data_ptr);
            }

            WS_LOGW("WEBSOCKET_CLIENT_FOR_ATCMD",
                    "Total payload length=%d, data_len=%d, current payload offset=%d\r\n",
                    data->payload_len,
                    data->data_len,
                    data->payload_offset);

            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT)
            {
                const int prefix_data_resp_len = 32;
                char    * p_data_resp          = pvPortMalloc(prefix_data_resp_len + data->data_len);

                if (p_data_resp == NULL)
                {
                    break;
                }

                memset(p_data_resp, 0x00, (prefix_data_resp_len + data->data_len));

                if (p_at_ctrl)
                {
                    sprintf(p_data_resp, "\r\n+NWWSC:1,%d,%d,%.*s\r\n", data->op_code, data->data_len, data->data_len,
                            (char *) data->data_ptr);
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_data_resp, strlen(p_data_resp));
                }

                vPortFree(p_data_resp);
                p_data_resp = NULL;
            }

            break;
        }

        case WEBSOCKET_CLIENT_EVENT_CLOSED:
        {
            WS_LOGW("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Closed\n");
            break;
        }

        case WEBSOCKET_CLIENT_EVENT_FAIL_TO_CONNECT:
        {
            WS_LOGE("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Failed to connect to server\n");
            if (p_at_ctrl)
            {
                rm_atcmd_w_core_common_print_error_code(p_at_ctrl, FSP_ERR_AT_CMD_ERR_NW_WSC_SESS_NOT_CONNECTED);
            }

            break;
        }

        case WEBSOCKET_CLIENT_EVENT_ERROR:
        {
            WS_LOGE("WEBSOCKET_CLIENT_FOR_ATCMD", ">> Websocket Error\n");
            if (p_at_ctrl)
            {
                rm_atcmd_w_core_common_print_error_code(p_at_ctrl, FSP_ERR_AT_CMD_ERR_UNKNOWN);
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

#if CFG_PMGR
static void websocket_auto_start (void * pvParameters)
{
    RA6W1_UNUSED_ARG(pvParameters);

    /* Wait for connection */
    if (chk_network_ready(WLAN0_IFACE) == pdFALSE)
    {
        while (chk_network_ready(WLAN0_IFACE) == pdFALSE)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }
    }

    if (!RM_PMGR_W_dpm_is_enabled())
    {
        vTaskDelete(NULL);

        return;
    }

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t task_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                       g_wifi_cfg.p_watchdog_service->p_cfg,
                                                       &task_wdog_id);
    g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     task_wdog_id,
                                                     ATCMD_WEBSOCKET_CLIENT_WDOG_LATENCY);
 #endif

    /* Use Null URI to indicate try load form RTM */
    websocket_client_connect(NULL, "\0");

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, task_wdog_id);
 #endif

    vTaskDelete(NULL);
}

TaskHandle_t websocket_auto_start_handler = NULL;
void websocket_auto_start_begin (void)
{
    BaseType_t       ret;
    WIFIDeviceMode_t wifi_mode;

    ret = xTaskCreate(websocket_auto_start,
                      APP_WEBSOCKET_CLIENT,
                      (320),
                      (void *) NULL,
                      (OS_TASK_PRIORITY_USER + 6),
                      &websocket_auto_start_handler);
    if (ret != pdPASS)
    {
        RM_ATCMD_W_CORE_WEBSOCKET_CLIENT_ERROR("ERROR(%ld)\n", ret);
    }

    if (WIFI_GetMode(&wifi_mode) == eWiFiSuccess)
    {
        if ((wifi_mode == eWiFiModeStation) && (RM_PMGR_W_dpm_is_enabled()))
        {
            RM_PMGR_W_dpm_job_name_set(APP_WEBSOCKET_CLIENT, 0);
        }
    }
}
#endif /* CFG_PMGR */

ws_err_t websocket_client_connect (void * p_at_ctrl, char * uri)
{
    int  ret              = -1;
    bool uri_is_allocated = false;

    WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_client_connect()\n");
 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        if (websocket_client_assign_dpm_user_mem() == pdFALSE)
        {
            WS_LOGD("WEBSOCKET_CLIENT", "Rtm assign ERROR \n");
            RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);

            return pdFALSE;
        }
    }

    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_wakeup_done((char *) APP_WEBSOCKET_CLIENT);
    }
 #endif                                /* CFG_PMGR */

    if (websocket_client_is_connected(client))
    {
        WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", "Websocket Client is already connected.\n");
 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);
        }
 #endif                                /* CFG_PMGR */
        return WS_ERR_CONN_ALREADY_EXIST;
    }

 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        if (strlen(uri) != 0)
        {
            memcpy(websocketParams.uri, uri, sizeof(websocketParams.uri));
        }
        else
        {
            if (RM_PMGR_W_dpm_is_wakeup())
            {
                websocket_client_read_from_dpm_user_mem();
                if (strlen(websocketParams.uri) == 0)
                {
                    RM_PMGR_W_dpm_job_name_clear(APP_WEBSOCKET_CLIENT);

                    return pdFALSE;
                }
                else
                {
                    uri              = _ws_strdup(websocketParams.uri);
                    uri_is_allocated = true;
                }
            }
            else
            {
                RM_PMGR_W_dpm_job_name_clear(APP_WEBSOCKET_CLIENT);

                return pdFALSE;
            }
        }
    }
 #endif                                /* CFG_PMGR */

    if ((uri == NULL) || (strlen(uri) == 0))
    {
        WS_LOGE("WEBSOCKET_CLIENT", "\nInvalid or missing URI for Websocket — aborting connect.\n");
 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            RM_PMGR_W_dpm_sleep_ready_set(APP_WEBSOCKET_CLIENT);
        }
 #endif
        if (uri_is_allocated)
        {
            free(uri);
        }

        return WS_ERR_INVALID_ARG;
    }

    websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = uri;
    websocket_cfg.disable_auto_reconnect = true;
    websocket_cfg.user_context           = p_at_ctrl;

    WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", "Connecting to %s...\n", websocket_cfg.uri);

    client = websocket_client_init(&websocket_cfg);
    ret    = websocket_client_start(client, websocket_event_handler);
    if (uri_is_allocated)
    {
        free(uri);
    }

    return ret;
}

ws_err_t websocket_client_config (char * ping_intv_sec, char * ping_timeout_sec, char * buffer_size)
{
    int temp;

    WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_set_config()\n");

    temp = atoi(ping_intv_sec);
    if (temp > WEBSOCKET_PING_INTERVAL_MAX)
    {
        return WS_ERR_INVALID_ARG;
    }

    client->config->ping_interval_sec = temp;

    temp = atoi(ping_timeout_sec);
    if (temp > WEBSOCKET_PINGPONG_TIMEOUT_MAX)
    {
        return WS_ERR_INVALID_ARG;
    }

    client->config->pingpong_timeout_sec = temp;

    if (buffer_size)
    {
        temp = atoi(buffer_size);
        if (temp > WEBSOCKET_BUFFER_SIZE_BYTE)
        {
            return WS_ERR_INVALID_ARG;
        }

        client->buffer_size = temp;
    }

    return WS_OK;
}

ws_err_t websocket_client_disconnect (void)
{
    WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_client_disconnect()\n");

    if (websocket_client_is_connected(client) == false)
    {
        WS_LOGE("WEBSOCKET_CLIENT_FOR_ATCMD", "Websocket Client already disconnected.\n");

        return WS_ERR_INVALID_STATE;
    }

    if (websocket_client_close_with_code(client, WS_CLOSE_STATUS_NORMAL, NULL, 0,
                                         client->config->network_timeout_ms) != WS_OK)
    {
        WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "Failed to send Close Frame.\n");

        return WS_FAIL;
    }

    return WS_OK;
}

ws_err_t websocket_client_send_msg (char * msg)
{
    WS_LOGD("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_client_send_msg()\n");

    if (websocket_client_is_connected(client))
    {
        if (NULL == msg)
        {
            return WS_FAIL;
        }

        WS_LOGI("WEBSOCKET_CLIENT_FOR_ATCMD", "Sending %s, len=%d\n", msg, strlen(msg));

        return websocket_client_send_text(client, msg, strlen(msg), portMAX_DELAY);
    }

    WS_LOGE("WEBSOCKET_CLIENT_FOR_ATCMD", "websocket_client is not connected.\n");

    return WS_FAIL;
}

ws_err_t websocket_client_add_header (char * len, char * buffer)
{
    int length = atoi(len);

    if (length < 0)
    {
        return WS_ERR_INVALID_ARG;
    }
    else if (length == 0)
    {
        if (client->config->headers)
        {
            vPortFree(client->config->headers);
            client->config->headers = NULL;
        }
    }
    else
    {
        if (strlen(buffer) > length)
        {
            return WS_ERR_INVALID_SIZE;
        }

        size_t headers_alloc = (size_t) (length + 1);

        vPortFree(client->config->headers);
        client->config->headers = pvPortMalloc(headers_alloc);

        if (!client->config->headers)
        {
            return WS_FAIL;
        }

        bsp_safe_strcpy(client->config->headers, buffer, headers_alloc);
    }

    return WS_OK;
}

#endif
#endif                                 /* CFG_WIFI */
