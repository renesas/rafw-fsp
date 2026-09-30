/**
 ****************************************************************************************
 *
 * @file app_dpm_wrapper.c
 *
 * @brief Define DPM related APIs used to operate device on platform.
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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

#include "FreeRTOS.h"
#include "rm_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBEDTLS_ALLOW_PRIVATE_ACCESS//[tin aws work]
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "user_dpm.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif
#include "util_api.h"
#include "app_dpm_interface.h"
#include "common_utils.h"
#include "app_aws_user_conf.h"

#define malloc	pvPortMalloc
#define free	vPortFree

dpmAppThreadInfo *pAppDpmThread = NULL;

UINT8 flagSleepDPM = 0;

/*! timestamp at last time   */
uint32_t previous_ts = 0;

/*! timestamp at first time   */
uint32_t first_ts = 0;

char flagRegisteredThing = 0;
char gRegisteredThingName[128] = {0, };

extern uint8_t *pGenPublicKey;
extern size_t lenGenPublicKey;

UINT8 flagRcvTimeout = 0; // flag for setting recv timeout on DPM mode
UINT8 flagReconnected = 0; // reconnection flag on DPM mode
UINT8 flagTlsRestored = 0; // flag whether TLS restored or not
UINT16 secondsKeepalive = 0; // wakup timer interval for KA (seconds)
UINT8 flagPersistentSession = 0; // flag for persistent session
UINT8 flagUnknownUC = 0; // flag for Unknown UC on DPM wakeup

static UINT32 countReconTry = 0;
static char pubSendFlag = 0;
static UINT8 waitNextJobFlag = 0;

static char peerIPStr[16] = {0, };
static char aws_rtm_name[20] = AWS_RTM_NAME;
static APPSleepMode appSleepMode = SLEEP_MODE_3;
#if CFG_PMGR
UINT32 app_dpm_info_release(void)
{
    UINT32 status = 0;

    if (RM_PMGR_W_dpm_is_enabled()) {
        status = RM_PMGR_W_user_rtm_free(AWS_RTM_NAME);
        if (status) {
            IOT_ERROR("failed to release dpm info in rtm(0x%02x)", status);
        }
    }

    return status;
}

fsp_err_t app_dpm_info_get(char *_name, app_dpm_info_rtm** rtm_info)
{
    UINT32 status = 0;
    const ULONG wait_option = 0;

    app_dpm_info_rtm *data = NULL;
    UINT32 len = 0;

    if (_name == NULL) {
        len = RM_PMGR_W_user_rtm_get(aws_rtm_name, (unsigned char**)&data);
    } else {
        len = RM_PMGR_W_user_rtm_get(_name, (unsigned char**)&data);
    }

    if (len == 0) {
        if (_name == NULL) {
            status = RM_PMGR_W_user_rtm_pool_alloc(aws_rtm_name, (void**)&data, sizeof(app_dpm_info_rtm), wait_option);
        } else {
            status = RM_PMGR_W_user_rtm_pool_alloc(_name, (void**)&data, sizeof(app_dpm_info_rtm), wait_option);
        }

        if (status) {
            IOT_ERROR("failed to allocate app_dpm info in rtm(0x%02x)", status);
            rtm_info = NULL;
            return FSP_SUCCESS;
        }

        memset(data, 0x00, sizeof(app_dpm_info_rtm));

    } else if (len != sizeof(app_dpm_info_rtm)) {
        IOT_ERROR("invalid size(%u)", len);
        rtm_info = NULL;
        return FSP_SUCCESS;
    }

    if (_name != NULL) {
        memset(aws_rtm_name, 0, sizeof(aws_rtm_name));
        bsp_safe_strcpy(aws_rtm_name, _name, sizeof(aws_rtm_name));
    }
    //IOT_INFO("\"%s\" got DPM's RTM data size (%ld)\n", aws_rtm_name, sizeof(app_dpm_info_rtm))
    *rtm_info = data;
    return FSP_SUCCESS;
}


fsp_err_t app_dpm_set_sleep_flag(UINT8 _flag)
{
    flagSleepDPM = _flag;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_get_sleep_flag(UINT8 *p_flag)
{
    if (p_flag == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *p_flag = flagSleepDPM;
    return FSP_SUCCESS;
}

UINT32 app_dpm_set_client_socket_port(UINT32 _port)
{
    UINT32 status = ER_SUCCESS;
    app_dpm_info_rtm *data = NULL;

    if (RM_PMGR_W_dpm_is_enabled()) {
        app_dpm_info_get(NULL, &data);
        if (data) {
            data->bindPort = (int)_port;
        } else {
            status = ER_NO_MEMORY;
            IOT_ERROR("[set] rtm binded port: null");
        }
    }

    return status;
}

fsp_err_t app_dpm_get_client_socket_port(UINT32 *p_port)
{
    if (RM_PMGR_W_dpm_is_enabled()) {
        app_dpm_info_rtm *data = NULL;

        app_dpm_info_get(NULL, &data);
        if (data) {
            *p_port = (UINT32)data->bindPort;
            IOT_INFO("[get] rtm binded port: %d", data->bindPort);
            return FSP_SUCCESS;
        } else {
            *p_port = 0; //return port 0 if not found
            IOT_ERROR("[get] rtm binded port: null");
        }
    }

    return FSP_ERR_NOT_FOUND; 
}

fsp_err_t app_dpm_set_send_pub_flag(UINT8 _flag)
{
    if (_flag) {
        pubSendFlag++;
    } else {
        pubSendFlag--;
    }
}

fsp_err_t app_dpm_get_send_pub_flag(UINT8 *p_flag)
{
    if (pubSendFlag < 0) {
        APRINTF_E("[%s:%d] invalid publish flag (=%d)\n", __func__, __LINE__, pubSendFlag);
        pubSendFlag = 0;
    }
    *p_flag = (UINT8)pubSendFlag;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_set_wait_job_next_flag(UINT8 _flag)
{
    waitNextJobFlag = _flag;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_get_wait_job_next_flag(UINT8 *p_flag)
{
    if (p_flag == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *p_flag = waitNextJobFlag;
    return FSP_SUCCESS;
}
#endif

fsp_err_t app_get_thread_info(dpmAppThreadInfo ** _threadInfo)
{
    *_threadInfo = pAppDpmThread;
    return FSP_SUCCESS;
}

fsp_err_t app_set_thread_info(dpmAppThreadInfo * _threadInfo)
{
    pAppDpmThread = _threadInfo;
    return FSP_SUCCESS;
}

void app_set_peer_ip_str(char *_ipStr)
{
    memset(peerIPStr, 0, sizeof(peerIPStr));
    if (_ipStr != NULL) {
        bsp_safe_strcpy(peerIPStr, _ipStr, sizeof(peerIPStr));
    }
}

fsp_err_t app_get_peer_ip_str(char** p_ipStr)
{
    if (p_ipStr == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *p_ipStr = peerIPStr;
    return FSP_SUCCESS;
}

fsp_err_t app_get_random_local_port(UINT32 *p_port)
{
    if (p_port == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    static UINT32 appDummyCount = 0;
    UINT32 port;

    generate_port: ++appDummyCount;

    port = 30000 + (((UINT32)rand() * xTaskGetTickCount()) % 10000) + appDummyCount;

    if (appDummyCount > 25000) {
        appDummyCount = 0;
        goto generate_port;
    }

    APRINTF("port re-generated: #%d, retry count: %d\n", port, countReconTry + 1);
    countReconTry++;

    *p_port = port;
    return FSP_SUCCESS;
}

fsp_err_t app_get_count_of_reconnection(UINT32 *p_count)
{
    if (p_count == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *p_count = countReconTry;
    return FSP_SUCCESS;
}

fsp_err_t app_socket_set_port_n_filter(UINT32 _defLocalPort)
{
    uint32_t localPort;
    uint8_t isReconnected;

    app_dpm_get_client_socket_port(&localPort);
    app_is_reconnected(&isReconnected);
    if (isReconnected) {
        if (localPort != UNDEF_PORT) {
            //delete local port
            RM_WIFI_dpm_tcp_port_delete((uint16_t)localPort);
        }
        //generate ramdom port
        app_get_random_local_port(&localPort);
        if (!app_dpm_set_client_socket_port(localPort)) {
            APRINTF("saving binded port num=%lu\n", localPort);
        } else {
            APRINTF_E("saving binded port num=%lu: error\n", localPort);
        }
        app_dpm_get_client_socket_port(&localPort);
    }

    if (localPort == UNDEF_PORT) {
        localPort = _defLocalPort;
    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled()) {
        dpmAppThreadInfo * _threadInfo = NULL;
        char dpmMainJobName[20] = {0, };
        int status = 0;
        fsp_err_t rcode;

        app_get_thread_info(&_threadInfo);
        configASSERT(_threadInfo != NULL);

        if (_threadInfo == NULL)
        {
            sprintf(dpmMainJobName, "%s", APP_AWS_SHADOW);
        }
        else
        {
            sprintf(dpmMainJobName, "%s", _threadInfo->DPMRegeditName);
        }

        configASSERT(strlen(dpmMainJobName) < REG_NAME_DPM_MAX_LEN);
        configASSERT(strlen(dpmMainJobName) > 0);

        status = RM_PMGR_W_dpm_job_name_is_set(dpmMainJobName);
        if (status == DPM_NOT_REGISTERED) 
        {
            APRINTF("\n\"%s\" is not registered. Will be registered with %lu local port\n", dpmMainJobName, localPort);
            rcode = RM_PMGR_W_dpm_job_name_set(dpmMainJobName, localPort);
            APRINTF("[%s:%d] RM_PMGR_W_dpm_job_name_set(\"%s\")=0x%x\n", __func__, __LINE__, dpmMainJobName, rcode);
        } else {
            UINT32 save_port;
            app_dpm_get_client_socket_port(&save_port);

            if (save_port != UNDEF_PORT) {
                char *reg_name = RM_PMGR_W_dpm_port_is_set(save_port);

                if (strncmp(reg_name, dpmMainJobName, strlen(dpmMainJobName))) {
                    RM_PMGR_W_dpm_job_name_clear(dpmMainJobName);
                    RM_PMGR_W_dpm_job_name_set(dpmMainJobName, save_port);

                    APRINTF_S("\n\"%s\" re-registered to port #%d for DPM process\n", dpmMainJobName, save_port);
                } else {
                    APRINTF("\n\"%s\" already registered to port #%d for DPM process\n", dpmMainJobName, save_port);
                }
            } else {
                char *reg_name = RM_PMGR_W_dpm_port_is_set(localPort);
                if (strncmp(reg_name, dpmMainJobName, strlen(dpmMainJobName))) {
                    APRINTF_E("\n[fatal] port #%lu registered to \"%s\" (wrong).\n", localPort, reg_name);
                } else {
                    APRINTF("\n\"%s\" already registered to port #%lu for DPM process\n", dpmMainJobName, localPort);
                }
            }
        }
        app_dpm_set_client_socket_port(localPort);
        APRINTF("[%s:%d] app_dpm_set_client_socket_port(localPort=%lu) called\n", __func__, __LINE__, localPort);
        /* Set tcp client' binding port to filter in DPM */
        RM_WIFI_dpm_tcp_port_filter_set((uint16_t)localPort);
        APRINTF("[%s:%d] RM_WIFI_dpm_tcp_port_filter_set(localPort=%lu) called\n", __func__, __LINE__, localPort);
    }
#endif
    return FSP_SUCCESS;
}

fsp_err_t app_is_reconnected(UINT8 *p_value)
{
    if (p_value == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_value = flagReconnected;
    return FSP_SUCCESS;
}

fsp_err_t app_set_reconnect_flag(UINT8 _value)
{
    flagReconnected = _value;
    if (flagReconnected) {
        //clear other flags (pub and job)
        pubSendFlag = 0;
        waitNextJobFlag = 0;
    }
    return FSP_SUCCESS;
}

fsp_err_t app_is_persistent_session(UINT8 *p_value)
{
    if (p_value == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_value = flagPersistentSession;
    return FSP_SUCCESS;
}

fsp_err_t app_set_persistent_session(UINT8 _value)
{
    flagPersistentSession = _value;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_get_recv_timeout_flag(UINT8 *p_value)
{
    if (p_value == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_value = flagRcvTimeout;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_set_recv_timeout_flag(UINT8 _value)
{
    if (flagRegisteredThing == 1) { //only used in case of thing being registerd
        flagRcvTimeout = _value;
        if (_value == DPM_RCV_OK_SLEEP) {
            awsiot_app_print_elapse_time_ms("[%s:%d] DPM_RCV_OK_SLEEP set", __func__, __LINE__);
        } else {
            awsiot_app_print_elapse_time_ms("[%s:%d] state:%d set", __func__, __LINE__, _value);
        }
    } else {
        awsiot_app_print_elapse_time_ms("[%s:%d] thing not yet registered!!!", __func__, __LINE__);
    }
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_get_unknown_uc_flag(UINT8 *p_value)
{
    if (p_value == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_value = flagUnknownUC;
    return FSP_SUCCESS;
}

fsp_err_t app_dpm_set_unknown_uc_flag(UINT8 _value)
{
    flagUnknownUC = _value;
    return FSP_SUCCESS;
}

fsp_err_t app_set_ka_value(UINT16 _kaVal)
{
    secondsKeepalive = _kaVal;
    awsiot_app_print_elapse_time_ms("[%s:%d] KA set to %d", __func__, __LINE__, secondsKeepalive);
    return FSP_SUCCESS;
}

fsp_err_t app_get_ka_value(UINT16 *p_kaVal)
{
    if (p_kaVal == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    *p_kaVal = secondsKeepalive;
    return FSP_SUCCESS;
}

#if CFG_PMGR
fsp_err_t app_dpm_set_rcv_ready(void)
{
    RM_PMGR_W_dpm_rcv_ready_set(APP_AWS_SHADOW);
    awsiot_app_print_elapse_time_ms("[%s:%d] DPM rcv ready & subscription completed", __func__, __LINE__);
    return FSP_SUCCESS;
}

void app_dpm_set_restoration_flag(UINT8 _flag)
{
    flagTlsRestored = _flag;
}

UINT8 app_dpm_get_tls_restoration_flag(void)
{
    return flagTlsRestored;
}

IoT_Error_t app_tls_save(const char *name, mbedtls_ssl_context *sslCtx)
{
    INT32 ret = 0;

    if (RM_PMGR_W_dpm_is_enabled()) {
        if ((strlen(name) == 0) || (strlen(name) > REG_NAME_DPM_MAX_LEN)) {
            APRINTF_E("[%s:%d] user rtm name's length has to be less than %d\n", __func__, __LINE__, REG_NAME_DPM_MAX_LEN);
            return FAILURE;
        }

        ret = RM_PMGR_W_dpm_tls_session_set(name, sslCtx);
        if (ret) {
            APRINTF_E("[%s:%d] Failed to save tls session(0x%x)\n",__func__, __LINE__, -ret);
            return FAILURE;
        }
    }

    return SUCCESS;
}

IoT_Error_t app_tls_restore(const char *name, mbedtls_ssl_context *sslCtx)
{
    INT32 ret = 0;

    if (RM_PMGR_W_dpm_is_enabled()) {
        if ((strlen(name) == 0) || (strlen(name) > REG_NAME_DPM_MAX_LEN)) {
            APRINTF_E("[%s:%d] user rtm name's length has to be less than %d\n", __func__, __LINE__, REG_NAME_DPM_MAX_LEN);
            return FAILURE;
        }

        ret = RM_PMGR_W_dpm_tls_session_get(name, sslCtx);
        if (ret == ER_NOT_FOUND) {
            APRINTF_E("[%s:%d] Not found(%s)\n", __func__, __LINE__, name);
            return NETWORK_SSL_UNKNOWN_ERROR;
        } else if (ret != 0) {
            if (ret == ER_INVALID_PARAMETERS) {
                //APRINTF("%s:%d ----ssl_ctx=0x%x, ssl_ctx->state=%d\n", __func__, __LINE__, tlsDataParams->ssl_ctx, tlsDataParams->ssl_ctx->state);
                if (sslCtx) {
                    if (sslCtx->state == MBEDTLS_SSL_HANDSHAKE_OVER) {
                        goto NEXT_STEP;
                    }
                }
            }

            APRINTF_E("[%s:%d] Failed to restore tls session(0x%x)\n", __func__, __LINE__, ret);
            return FAILURE;
        } else {
            //APRINTF("[%s:%d] restore tls session OK, state = %d\n", __func__, __LINE__, sslCtx->state);
            awsiot_app_print_elapse_time_ms("[%s:%d] restore tls session OK, state = %d", __func__, __LINE__, sslCtx->state);
        }

        NEXT_STEP: if (RM_PMGR_W_dpm_is_wakeup()) {
            //RM_PMGR_W_dpm_rcv_ready_set(APP_AWS_SHADOW);

            /*
             if (WAKEUP_SOURCE_FN() == WAKEUP_SENSOR_EXT_SIGNAL ||
             WAKEUP_TYPE_FN() != DPM_PACKET_WAKEUP) {
             if (WAKEUP_SOURCE_FN() == WAKEUP_SENSOR_EXT_SIGNAL) {
             APRINTF("[%s] sensor wakeup mode\n", __func__);
             }
             else {
             APRINTF("[%s] not the mode of receiving packet\n", __func__);
             }
             return NULL_VALUE_ERROR;
             }
             */

            return SUCCESS;
        } else {
            return FAILURE;
        }
    } else {
        return SUCCESS;
    }
}

IoT_Error_t app_tls_clear(const char *name)
{
    INT32 ret = 0;

    if ((strlen(name) == 0) || (strlen(name) > REG_NAME_DPM_MAX_LEN)) {
        APRINTF_E("[%s:%d] user rtm name's length has to be less than %d\n",
            __func__, __LINE__, REG_NAME_DPM_MAX_LEN);
        return FAILURE;
    }

    //first check rtm data
    mbedtls_ssl_context *tmpSslCtx = NULL;
    if (RM_PMGR_W_dpm_tls_session_get(name, tmpSslCtx))
    {
        if (ret == ER_NOT_FOUND)
        {
            APRINTF_I("[%s:%d] already release\n", __func__, __LINE__);
            return SUCCESS;
        }
    }

    ret = RM_PMGR_W_dpm_tls_session_clear(name);
    if (ret) {
        APRINTF_E("[%s:%d] Failed to clear tls session(0x%x)\n", __func__, __LINE__, ret);
        return FAILURE;
    }

    return SUCCESS;
}
#endif

char* app_get_registered_thing_name(void)
{
    if (flagRegisteredThing) {
        return gRegisteredThingName;
    } else {
        return NULL;
    }
}

fsp_err_t app_set_registered_thing_name(char *_pcThingName)
{
    if (_pcThingName == NULL) {
        APRINTF_E("[%s:%d] passed thing name is NULL\n", __func__, __LINE__);
        flagRegisteredThing = 0;
        return -1;
    } else {
        memset(gRegisteredThingName, 0, sizeof(gRegisteredThingName));
        memcpy(gRegisteredThingName, _pcThingName, strlen(_pcThingName));
        flagRegisteredThing = 1;
        return 0;
    }
}
//]]

UINT8 flagNeedSubscription = 1;
fsp_err_t app_get_subscription_need_to_server(UINT8 *p_flag)
{
    if (p_flag == NULL) {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    *p_flag = flagNeedSubscription;
    return FSP_SUCCESS;
}   

fsp_err_t app_set_subscription_need_to_server(UINT8 _enable)
{
    flagNeedSubscription = _enable;
    return FSP_SUCCESS;
}

#ifdef __cplusplus
}
#endif

static UINT32 app_get_ms(void)
{
    uint32_t ulTimeMs = 0UL;
    TickType_t xTickCount = 0;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = pdTICKS_TO_MS( xTickCount );

    return ulTimeMs;
}

fsp_err_t awsiot_app_print_elapse_time_ms(const char *fmt, ...)
{
    UINT32 currentTS = app_get_ms();
#if defined(TIME_CHECK_DBG)
    UINT32 followingTS = 0;
#endif  // TIME_CHECK_DBG
    va_list ap;
    char buf[160] = {0, };

    if (first_ts == 0) {
        first_ts = currentTS;
    }

#if defined(TIME_CHECK_DBG)
    if (previous_ts != 0) {
        followingTS = currentTS - previous_ts;
    }
#endif  // TIME_CHECK_DBG

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), (const char*)fmt, ap);
    va_end(ap);

#if defined(TIME_CHECK_DBG)
    APRINTF("\n%s ==> elapsed time: %u ms, total time: %lu ms\n", buf, followingTS, currentTS - first_ts);
#endif

    previous_ts = currentTS;
}


fsp_err_t appSetSleepMode(APPSleepMode _mode)
{
    appSleepMode = _mode;
    return FSP_SUCCESS;
}

fsp_err_t appGetSleepMode(APPSleepMode *p_mode)
{
    *p_mode = appSleepMode;
    return FSP_SUCCESS;
}
