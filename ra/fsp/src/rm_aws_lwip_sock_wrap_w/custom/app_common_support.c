/**
 ****************************************************************************************
 *
 * @file app_common_support.c
 *
 * @brief Define the common utility for apps module
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
#if __has_include("rm_awsiot_w_cfg.h")
 #include "rm_awsiot_w_cfg.h"
#endif
#include "lwip/dns.h"
#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "common_utils.h"
#include "common_def.h"
#include "util_api.h"
#include "app_common_support.h"
#if CFG_PMGR
 #include "rm_pmgr_w_instance.h"
 #include "r_pm_if.h"
#endif
#include "rm_lwip_w_helper.h"
#include "net_dns_client.h"
#include "net_sntp_client.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
 #include "ra6w1_platform_nvparam.h"
#endif
#include "rm_aws_lwip_sock_wrap_w_api.h"

// [tin aws work]
#define WAKEUP_SOURCE_EXT_SIGNAL                        (BSP_WAKEUP_SOURCE_GPIO)
#define WAKEUP_EXT_SIG_WITH_RETENTION                   (BSP_WAKEUP_SOURCE_GPIO | BSP_WAKEUP_RETENTION)
#define WAKEUP_EXT_SIG_WAKEUP_COUNTER_WITH_RETENTION    (BSP_WAKEUP_SOURCE_GPIO | BSP_WAKEUP_RETENTION | \
                                                         BSP_WAKEUP_SOURCE_WAKEUP_COUNTER) // WAKEUP_SENSOR_GPIO_COUNTER_WITH_RETENTION
#define WAKEUP_COUNTER_WITH_RETENTION                   (BSP_WAKEUP_SOURCE_WAKEUP_COUNTER | BSP_WAKEUP_RETENTION)
#define WAKEUP_RESET                                    (BSP_WAKEUP_RESET)
#define WAKEUP_SOURCE_POR                               (BSP_WAKEUP_SOURCE_POR)
#define WAKEUP_WATCHDOG                                 (BSP_WAKEUP_SOURCE_WATCHDOG)
#define WAKEUP_SOURCE_WAKEUP_COUNTER                    (BSP_WAKEUP_SOURCE_WAKEUP_COUNTER)

#if CFG_PMGR

// extern int RM_PMGR_W_dpm_is_enabled(void);
extern int RM_PMGR_W_dpm_is_wakeup(void);

#endif

persistant_storage_read_cb_t  persistant_read_cb;
persistant_storage_write_cb_t persistant_write_cb;

// register callback
fsp_err_t persistant_read_callback_set (persistant_storage_read_cb_t cb)
{
    persistant_read_cb = cb;

    return FSP_SUCCESS;
}

fsp_err_t persistant_write_callback_set (persistant_storage_write_cb_t cb)
{
    persistant_write_cb = cb;

    return FSP_SUCCESS;
}

#if (1 == AWS_IOT_DPM_APP_ENABLE)
static UINT32 app_rtm_set_server_ip (char * data_name, char * ip_addr)
{
    UINT32             res  = ER_SUCCESS;
    app_dpm_info_rtm * data = NULL;

    UINT32      status      = 0;
    const ULONG wait_option = 0;
    UINT32      len         = 0;

    if (data_name == NULL)
    {
        printf("[set]rtm data name is null !!\n");

        return ER_NOT_SUCCESSFUL;
    }

 #if CFG_PMGR

    /*if (RM_PMGR_W_dpm_is_enabled())*/ {
        len = RM_PMGR_W_user_rtm_get(data_name, (unsigned char **) &data);

        if (len == 0)
        {
            status = RM_PMGR_W_user_rtm_pool_alloc(data_name, (void **) &data, sizeof(app_dpm_info_rtm), wait_option);

            if (status)
            {
                printf("failed to allocate app_dpm info in rtm(0x%02x)\n", status);
                data = NULL;
            }

            memset(data, 0x00, sizeof(app_dpm_info_rtm));
        }
        else if (len != sizeof(app_dpm_info_rtm))
        {
            printf("invalid size(%u)\n", len);
            data = NULL;
        }

        if (data)
        {
            memset(data->ServrIp, 0, sizeof(data->ServrIp));
            if (isvalidip(ip_addr))
            {
                bsp_safe_strcpy(data->ServrIp, ip_addr, sizeof(data->ServrIp));
            }
            else
            {
                printf("[%s:%d] passed ip (=\"%s\") invalid, and cleared \n", __func__, __LINE__, ip_addr);
            }
        }
        else
        {
            res = ER_NO_MEMORY;
            printf("[set] rtm data: null");
        }
    }
 #endif

    /*else {
     *  res = ER_NOT_SUCCESSFUL;
     * }*/

    return res;
}

static char * app_rtm_get_server_ip (char * data_name)
{
    UINT32      status      = 0;
    const ULONG wait_option = 0;

    app_dpm_info_rtm * data = NULL;
    UINT32             len  = 0;

    if (data_name == NULL)
    {
        printf("[get]rtm data name is null !!\n");

        return NULL;
    }

 #if CFG_PMGR

    /*if (RM_PMGR_W_dpm_is_enabled())*/ {
        len = RM_PMGR_W_user_rtm_get(data_name, (unsigned char **) &data);

        if (len == 0)
        {
            status = RM_PMGR_W_user_rtm_pool_alloc(data_name, (void **) &data, sizeof(app_dpm_info_rtm), wait_option);

            if (status)
            {
                printf("failed to allocate app_dpm info in rtm(0x%02x)\n", status);
                data = NULL;
            }

            memset(data, 0x00, sizeof(app_dpm_info_rtm));
        }
        else if (len != sizeof(app_dpm_info_rtm))
        {
            printf("invalid size(%u)\n", len);
            data = NULL;
        }

        if (data)
        {
            return data->ServrIp;
        }
        else
        {
            printf("[get] rtm data: null\n");

            return NULL;
        }
    }
 #endif

    return NULL;
}

#endif

char * app_common_get_ip_from_normal_dns (char * hostname)
{
    char        * ip4addr_str  = NULL;
    static char   retryCount   = 0;
    unsigned long waitOptQuery = 4000;
    bool          ret_dns_A_Query;

    printf("hostName = \"%s\"\n", hostname);

    ip4addr_str = malloc(IPADDR_LEN * sizeof(char));
    if (ip4addr_str == NULL)
    {
        printf("Memory allocation for ipv4 address failed\n");

        return ip4addr_str;
    }

retryQuery:
    ret_dns_A_Query = dns_A_Query((char *) hostname, ip4addr_str, waitOptQuery);
    if (ret_dns_A_Query && isvalidip(ip4addr_str))
    {
        printf("host IP = \"%s\"\n", ip4addr_str);
    }
    else
    {
        if (++retryCount <= 5)
        {
            waitOptQuery += 1000;
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            printf("dns_A_Query(time to wait: %lu ms) failed : retry (cnt=%d)...\n", waitOptQuery, retryCount);
            goto retryQuery;
        }

        memset(ip4addr_str, 0, IPADDR_LEN);
    }

    retryCount = 0;                    // re-initialize for reentrance

    return ip4addr_str;
}

char * app_common_get_ip_from_fast_dns_with_rtm (char * hostname, char * rtm_data_name, UINT8 is_real_query)
{
#if CFG_PMGR && (1 == AWS_IOT_DPM_APP_ENABLE)
    char * ip_str = NULL;
#endif
    char        * ip4addr_str  = NULL;
    static char   retryCount   = 0;
    unsigned long waitOptQuery = 600;  // 4000;
    bool          ret_dns_A_Query;

    printf("hostName = \"%s\", flag to re-query (=%d)\n", hostname, is_real_query);

    ip4addr_str = malloc(IPADDR_LEN * sizeof(char));
    if (ip4addr_str == NULL)
    {
        printf("Memory allocation for ipv4 address failed\n");

        return ip4addr_str;
    }

    if (is_real_query)
    {
        goto retryQuery;
    }

#if CFG_PMGR && (1 == AWS_IOT_DPM_APP_ENABLE)
    int wakeupSource = WAKEUP_SOURCE_FN();
    int wakeupType   = (enum DPM_WAKEUP_TYPE) RM_PMGR_W_dpm_wakeup_type_get(0);
    if (RM_PMGR_W_dpm_is_wakeup() || ((wakeupType == 0) && (wakeupSource == WAKEUP_EXT_SIG_WITH_RETENTION)) ||
        ((wakeupType == 0) && (wakeupSource == WAKEUP_COUNTER_WITH_RETENTION)))
    {
        ip_str = app_rtm_get_server_ip(rtm_data_name);
        if (ip_str != NULL)
        {
            bsp_safe_strcpy(ip4addr_str, ip_str, IPADDR_LEN);
        }
        else
        {
            printf("Server IP read from RTM failed\n");
            goto retryQuery;
        }

        if (isvalidip(ip4addr_str))
        {
            printf("host IP from RTM = \"%s\"\n", ip4addr_str);
        }
        else
        {
            printf("host IP from RTM: NG\n");
            goto retryQuery;
        }
    }
    else
#endif
    {
retryQuery:
        awsiot_app_print_elapse_time_ms("[%s:%d] call dns_A_Query() directly)", __func__, __LINE__);
        ret_dns_A_Query = dns_A_Query((char *) hostname, ip4addr_str, waitOptQuery);
        if (ret_dns_A_Query && isvalidip(ip4addr_str))
        {
            printf("host IP = \"%s\" \n", ip4addr_str);
#if (1 == AWS_IOT_DPM_APP_ENABLE)
            app_rtm_set_server_ip(rtm_data_name, ip4addr_str);
#endif
        }
        else
        {
            if (++retryCount <= 5)
            {
                waitOptQuery += 100;   // 1000;
                printf("dns_A_Query(time to wait: %lu ms) failed : retry (cnt=%d)...\n", waitOptQuery, retryCount);
                vTaskDelay(portCONVERT_MS_2_TICKS(100));
                goto retryQuery;
            }

            memset(ip4addr_str, 0, IPADDR_LEN);
        }

        retryCount = 0;                // re-initialize for reentrance
    }

    return ip4addr_str;
}

char * app_common_get_ip_from_fast_dns_with_nvram (char * hostname, UINT8 is_real_query)
{
    char        * ip_str       = NULL;
    char        * ip4addr_str  = NULL;
    static char   retryCount   = 0;
    unsigned long waitOptQuery = 4000;
    bool          ret_dns_A_Query;

    printf("hostName = \"%s\", flag to re-query (=%d)\n", hostname, is_real_query);

    ip4addr_str = malloc(IPADDR_LEN * sizeof(char));
    if (ip4addr_str == NULL)
    {
        printf("Memory allocation for ipv4 address failed\n");

        return ip4addr_str;
    }

    if (is_real_query)
    {
        goto retryQuery;
    }

    ip_str = (char *) persistant_read_cb(PROP_SVR_IP);
    if (ip_str != NULL)
    {
        bsp_safe_strcpy(ip4addr_str, ip_str, IPADDR_LEN);
    }
    else
    {
        printf("IP read from persistant storage failed\n");
        goto retryQuery;
    }

    if (isvalidip(ip4addr_str))
    {
        printf("host IP from NVRAM: \"%s\"\n", ip4addr_str);

        return ip4addr_str;
    }
    else
    {
        printf("host IP from NVRAM: NG\n");
    }

retryQuery:
    ret_dns_A_Query = dns_A_Query((char *) hostname, ip4addr_str, waitOptQuery);
    if (ret_dns_A_Query && isvalidip(ip4addr_str))
    {
        printf("host IP = \"%s\"\n", ip4addr_str);
        persistant_write_cb(PROP_SVR_IP, (void *) ip4addr_str);
    }
    else
    {
        if (++retryCount <= 5)
        {
            waitOptQuery += 1000;
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            printf("dns_A_Query(time to wait: %lu ms) failed : retry (cnt=%d)...\n", waitOptQuery, retryCount);
            goto retryQuery;
        }

        memset(ip4addr_str, 0, IPADDR_LEN);
    }

    retryCount = 0;                    // re-initialize for reentrance

    return ip4addr_str;
}

/* EOF */
