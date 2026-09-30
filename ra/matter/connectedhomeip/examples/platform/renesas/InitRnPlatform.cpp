/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
 *    All rights reserved.
 *    Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "AppConfig.h"
#include <lib/support/CHIPPlatformMemory.h>
#include <platform/CHIPDeviceLayer.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "InitRnPlatform.h"
#include "rnDeviceWrapAPIs.h"
#include "rnDevice_system_init.h"

#if defined(__USE_MATTER_DPM_APP__)
#include "rm_pmgr_w_dpm_internal.h"
#include "rm_pmgr_w_instance.h"
#include "rm_wifi_dpm.h"

#define MATTER_RTM_NAME "mat_rtm"

extern pmgr_instance_ctrl_t g_pmgr_w_ctrl;
extern const pmgr_cfg_t g_pmgr_w_cfg;

mat_sess_info *gMatRtmPtr = NULL;
#endif

void init_rnPlatform(void)
{
#if defined(__USE_MATTER_DPM_APP__)
    if (RM_PMGR_W_dpm_is_enabled()) {
        unsigned int ret = 0;
        size_t len = 0;

        len = sizeof(mat_sess_info);
        ret = RM_PMGR_W_user_rtm_get(const_cast<char*>(MATTER_RTM_NAME), (unsigned char **)&gMatRtmPtr);
        if (ret == 0) {
            ret = RM_PMGR_W_user_rtm_pool_alloc(const_cast<char*>(MATTER_RTM_NAME), (void **)&gMatRtmPtr, len, 0);
            if (ret) {
                RENES_LOG("[%s:%d] Failed to allocate memory to save user session(%lu)",
                        __func__, __LINE__, (unsigned long)len);
                return;
            }
            else {
                RENES_LOG("[%s:%d][alloc] Matter RTM size = %d", __func__, __LINE__, len);
            }
        } else if (ret != len) {
            RENES_LOG("[%s] Invalid size(%d,%d)", __func__, ret, len);
            return;
        } else {
            RENES_LOG("[%s:%d] Matter RTM size = %d", __func__, __LINE__, len);
        }

        RM_PMGR_W_rtm_heap_status_print();

        RENES_LOG("[%s:%d][PMGR] PMGR is initilaised", __func__, __LINE__);
        if(RM_PMGR_W_dpm_job_name_set(const_cast<char*>(MATTER_JOB_NAME), CHIP_PORT) != FSP_SUCCESS) {
            RENES_LOG("[%s:%d][PMGR] RM_PMGR_W_dpm_job_name_set: Failure", __func__, __LINE__);
            goto error_exit;
        }

        // DPM parameters:
        //   Keep Alive   = 30 000 ms  – null-frame to AP every 30 s, prevents deauth
        //   TIM Wakeup   = 1 DTIM     – wake every beacon to receive buffered multicast
        //   User Wakeup  = 3 600 000 ms (60 min) – aligned with Matter subscription
        //                               MaxInterval cap (kSubscriptionMaxIntervalPublisherLimit
        //                               = 3600 s) and CHIP_CONFIG_ICD_IDLE_MODE_DURATION_SEC.
        //                               Previously 200 000 ms, which woke the device every
        //                               ~3 min unnecessarily.
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_KEEPALIVE, 30000, 0);
        RM_WIFI_dpm_ptim_wakeup_count_set(1, 0);
        RM_PMGR_W_dpm_user_wakeup_timer_set(3600000);
        RENES_LOG("[%s:%d][PMGR] DPM params: keepalive=30000ms, TIM_wakeup=1dtim, user_wakeup=3600000ms(60min)", __func__, __LINE__);
        RENES_LOG("[%s:%d][PMGR] Wake filter set: UDP %d only (mDNS filters disabled)",
                  __func__, __LINE__, CHIP_PORT);
   }
#endif
    RENES_LOG("[%s:%s:%d] ----", __FILENAME__, __func__, __LINE__);
#if !defined (__RRQ61400__)    
    rnDevice_system_init();
#endif    
    return;

#if defined(__USE_MATTER_DPM_APP__)
error_exit:
    if(!RM_PMGR_W_user_rtm_free(const_cast<char*>(MATTER_RTM_NAME))) {
        RENES_LOG("[%s:%d] Matter RTM pool freed", __func__, __LINE__);
    }
#endif
}

#ifdef __cplusplus
}
#endif

