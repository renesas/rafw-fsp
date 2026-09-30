/**
 ****************************************************************************************
 *
 * @file rm_cli_w_dpm.c
 *
 * @brief DPM command functions
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

#include "bsp_api.h"

#if CFG_PMGR
#include "FreeRTOS.h"
#include "rm_wifi.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "task.h"
#include "sdk_defs.h"

#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w_dpm.h"

#include "rm_pmgr_w_instance.h"
#include "dpmty_patch.h"
#include "lwip/ip_addr.h"
#include "rm_lwip_w_helper.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif
#include "lwip/sockets.h"

#if PMGR_DPM_DEBUG_CLI
extern bool RM_PMGR_W_rtm_exist(void);


/******************************************************************************
 *
 * Variables
 *
 ******************************************************************************/


/******************************************************************************
 *
 * Internal functions
 *
 ******************************************************************************/
static int chk_ssid_chg_state(void)
{
    dpm_supp_conn_info_t    * dpm_supp_conn_info_loc;
    char    *nvram_ssid = NULL;
    char    *nvram_psk = NULL;
    int    rtn;

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_CONN_INFO_PTR, NULL, NULL, (void**)(&dpm_supp_conn_info_loc));
    
    if (dpm_supp_conn_info_loc == NULL) {
        return 0;
    }

    /* Compare the status of ssid changing */
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, (char *)NVR_KEY_SSID_0, &nvram_ssid);
#endif

    if (nvram_ssid == NULL && strlen((char const *)(dpm_supp_conn_info_loc->ssid)) > 0) {
        rtn = -1;
    } else if (nvram_ssid != NULL && strcmp(nvram_ssid, (char const *)(dpm_supp_conn_info_loc->ssid)) == 0) {
        rtn = 0;
    } else {
        rtn = -1;
    }
#if 0
    if (nvram_ssid) {
        vPortFree(nvram_ssid);
    }
#endif /* 0 */

    /* Compare the status of psk changing */
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, (char *)NVR_KEY_ENCKEY_0, &nvram_psk);
#endif

    if (nvram_psk == NULL && strlen((char const *) (dpm_supp_conn_info_loc->psk)) > 0) {
        rtn = -1;
    } else if (nvram_psk != NULL
             && strcmp(nvram_psk, (char const *) (dpm_supp_conn_info_loc->psk)) == 0) {
        rtn = 0;
    } else {
        rtn = -1;
    }
#if 0
    if (nvram_psk) {
        vPortFree(nvram_psk);
    }
#endif /* 0 */
    
    return rtn;
}

/******************************************************************************
 *
 * Command
 *
 ******************************************************************************/
static bool cmd_dpm_onoff(int argc, char *argv[])
{
    (void) argc;

    if (strcasecmp(argv[0], "ON") == 0) {
        if (!RM_PMGR_W_rtm_exist()) {
            printf("FAIL\n - RTM doesn't exist.\n");
            return pdFALSE;
        }
        //OAL_DPM_FULL_BOOT. DPM Mode on.
        vTaskDelay(portCONVERT_MS_2_TICKS(50));
        RM_PMGR_W_dpm_enable();
        vTaskDelay(portCONVERT_MS_2_TICKS(100));

        /* If connection config was changed,
         * need to reboot to start as normal mode
         */
        if (!RM_PMGR_W_dpm_is_wakeup() || chk_ssid_chg_state() == -1) {
            reset();
        }
    } else if (strcasecmp(argv[0], "OFF") == 0) {

        RM_PMGR_W_dpm_hold();

        //OAL_DPM_NORMAL_BOOT. DPM Mode off.
        vTaskDelay(portCONVERT_MS_2_TICKS(5));

        /* Disable DPM running mode */
        RM_PMGR_W_dpm_disable();
        vTaskDelay(portCONVERT_MS_2_TICKS(100));

        reset();
    }
    return pdTRUE;
}

static bool cmd_dpm_hold(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

	RM_PMGR_W_dpm_hold();

	return pdTRUE;
}

static bool cmd_dpm_resume(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

	RM_PMGR_W_dpm_sleep_resume();

	return pdTRUE;
}

static bool cmd_dpm_info(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

	RM_PMGR_W_dpm_dbg_info_get();

	return pdTRUE;
}

static const debug_handler_t dpm_handlers[] = {
    { "on",      "DPM On",                      (debug_callback_t)cmd_dpm_onoff       },
    { "off",     "DPM Off",                     (debug_callback_t)cmd_dpm_onoff       },
    { "hold",    "DPM Hold",                    (debug_callback_t)cmd_dpm_hold        },
    { "resume",  "DPM Resume",                  (debug_callback_t)cmd_dpm_resume      },
    { "status",  "DPM Status",                  (debug_callback_t)cmd_dpm_info        },
    { NULL },
};

bool dpm_command(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

    return debug_handle_message(argc, argv, dpm_handlers);
}
#endif /* PMGR_DPM_DEBUG_CLI */
#endif /* CFG_PMGR */
