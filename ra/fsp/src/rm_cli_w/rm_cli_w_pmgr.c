/**
 ****************************************************************************************
 *
 * @file rm_cli_w_pmgr.c
 *
 * @brief PMGR command functions
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

#include "dpmty_patch.h"
#include "lwip/ip_addr.h"
#include "rm_lwip_w_helper.h"
#include "rm_pmgr_w_instance.h"

#include "net_common.h"
#include "iface_defs.h"
#include "rm_pmgr_w_dpm_socket_internal.h"

#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#include dg_configADNVPARAM_PROJ_FILE
#endif

#include "lwip/sockets.h"

/******************************************************************************
 *
 * Defines
 *
 ******************************************************************************/
#define CONSTRAINTS_LIST_STR  "PMGR_CONSTRAINT_NONE, "             \
                              "PMGR_CONSTRAINT_SLEEP_PROHIBITED, " \
                              "PMGR_CONSTRAINT_POWER_RAM, "        \
                              "PMGR_CONSTRAINT_POWER_MAC_HW, "     \
                              "PMGR_CONSTRAINT_POWER_RETENTION"

#define WAKE_SOURCES_LIST_STR "PMGR_WAKE_SOURCE_RTC, "            \
                              "PMGR_WAKE_SOURCE_GPT, "            \
                              "PMGR_WAKE_SOURCE_ADC, "            \
                              "PMGR_WAKE_SOURCE_WIFI, "           \
                              "PMGR_WAKE_SOURCE_BLE"

#define LLD_STATES_LIST_STR   "PMGR_LLD_POWER_MODE_SLEEP2, "                 \
                              "PMGR_LLD_POWER_MODE_SLEEP3, "                 \
                              "PMGR_LLD_POWER_MODE_SLEEP4, "                 \
                              "PMGR_LLD_POWER_MODE_DPM,    "                 \
                              "PMGR_LLD_POWER_MODE_CPU_WFI, "       \
                              "PMGR_LLD_POWER_MODE_AUTO"

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
static pmgr_constraints_t pmgr_string_to_constraint(char * constraint_str)
{
    if (strcmp(constraint_str, "PMGR_CONSTRAINT_NONE") == 0)
    {
        return PMGR_CONSTRAINT_NONE;
    }
    else if (strcmp(constraint_str, "PMGR_CONSTRAINT_SLEEP_PROHIBITED") == 0)
    {
        return PMGR_CONSTRAINT_SLEEP_PROHIBITED;
    }
    else if (strcmp(constraint_str, "PMGR_CONSTRAINT_POWER_MAC_HW") == 0)
    {
        return PMGR_CONSTRAINT_POWER_MAC_HW;
    }
    else if (strcmp(constraint_str, "PMGR_CONSTRAINT_POWER_RAM") == 0)
    {
        return PMGR_CONSTRAINT_POWER_RAM;
    }
    else if (strcmp(constraint_str, "PMGR_CONSTRAINT_POWER_RETENTION") == 0)
    {
        return PMGR_CONSTRAINT_POWER_RETENTION;
    }

    PRINTF("Unknown constraint, supported constraints: " CONSTRAINTS_LIST_STR "\n");
    return PMGR_CONSTRAINT_MAX_BIT;
}

static const char * pmgr_constraint_to_string(pmgr_constraints_t constraint)
{
    if (constraint == PMGR_CONSTRAINT_NONE)
    {
        return "PMGR_CONSTRAINT_NONE";
    }
    else if (constraint == PMGR_CONSTRAINT_SLEEP_PROHIBITED)
    {
        return "PMGR_CONSTRAINT_SLEEP_PROHIBITED";
    }
    else if (constraint == PMGR_CONSTRAINT_POWER_MAC_HW)
    {
        return "PMGR_CONSTRAINT_POWER_MAC_HW";
    }
    else if (constraint == PMGR_CONSTRAINT_POWER_RAM)
    {
        return "PMGR_CONSTRAINT_POWER_RAM";
    }
    else if (constraint == PMGR_CONSTRAINT_POWER_RETENTION)
    {
        return "PMGR_CONSTRAINT_POWER_RETENTION";
    }

    return "Unknown Constraint";
}

static pmgr_wake_source_t pmgr_string_to_wake_source(char * wake_source_str)
{
    if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_RTC") == 0)
    {
        return PMGR_WAKE_SOURCE_RTC;
    }
    else if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_GPT") == 0)
    {
        return PMGR_WAKE_SOURCE_GPT;
    }
    else if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_GPIO") == 0)
    {
        return PMGR_WAKE_SOURCE_GPIO;
    }
    else if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_ADC") == 0)
    {
        return PMGR_WAKE_SOURCE_ADC;
    }
    else if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_WIFI") == 0)
    {
        return PMGR_WAKE_SOURCE_WIFI;
    }
    else if (strcmp(wake_source_str, "PMGR_WAKE_SOURCE_BLE") == 0)
    {
        return PMGR_WAKE_SOURCE_BLE;
    }

    PRINTF("Unknown wake source, supported wake sources: " WAKE_SOURCES_LIST_STR "\n");
    return PMGR_WAKE_SOURCE_NONE;
}

static pmgr_lld_power_mode_t pmgr_string_to_lld_state(char * lld_state_str)
{
    if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_SLEEP2") == 0)
    {
        return PMGR_LLD_POWER_MODE_SLEEP2;
    }
    else if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_SLEEP3") == 0)
    {
        return PMGR_LLD_POWER_MODE_SLEEP3;
    }
    else if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_SLEEP4") == 0)
    {
        return PMGR_LLD_POWER_MODE_SLEEP4;
    }
    else if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_DPM") == 0)
    {
        return PMGR_LLD_POWER_MODE_DPM;
    }
    else if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_CPU_WFI") == 0)
    {
        return PMGR_LLD_POWER_MODE_CPU_WFI;
    }
    else if (strcmp(lld_state_str, "PMGR_LLD_POWER_MODE_AUTO") == 0)
    {
        return PMGR_LLD_POWER_MODE_AUTO;
    }

    PRINTF("Unknown lld state, supported lld states: " LLD_STATES_LIST_STR "\n");
    return PMGR_LLD_POWER_MODE_INVALID;
}

static const char *pmgr_lld_state_to_string(pmgr_lld_power_mode_t lld_state)
{
    if (lld_state == PMGR_LLD_POWER_MODE_SLEEP2)
    {
        return "PMGR_LLD_POWER_MODE_SLEEP2";
    }
    else if (lld_state == PMGR_LLD_POWER_MODE_SLEEP3)
    {
        return "PMGR_LLD_POWER_MODE_SLEEP3";
    }
    else if (lld_state == PMGR_LLD_POWER_MODE_DPM)
    {
        return "PMGR_LLD_POWER_MODE_DPM";
    }
    else if (lld_state == PMGR_LLD_POWER_MODE_SLEEP4)
    {
        return "PMGR_LLD_POWER_MODE_SLEEP4";
    }
    else if (lld_state == PMGR_LLD_POWER_MODE_CPU_WFI)
    {
        return "PMGR_LLD_POWER_MODE_CPU_WFI";
    }
    else if (lld_state == PMGR_LLD_POWER_MODE_AUTO)
    {
        return "PMGR_LLD_POWER_MODE_AUTO";
    }
    else
    {
        return "Unknown LLD State";
    }
}

static const char * pmgr_dbg_ram_constraint_to_string(pmgr_dbg_ram_constraint_counter_cause_t cause) {
    switch (cause) {
        case PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE:
            return "PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE";
        case PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI:
            return "PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI";
        case PMGR_DBG_RAM_CONSTRAINT_DPM_AT:
            return "PMGR_DBG_RAM_CONSTRAINT_DPM_AT";
        case PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT:
            return "PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT";
        case PMGR_DBG_RAM_CONSTRAINT_NET_IFCONFIG:
            return "PMGR_DBG_RAM_CONSTRAINT_NET_IFCONFIG";
        case PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE:
            return "PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE";
        case PMGR_DBG_RAM_CONSTRAINT_SNTP:
            return "PMGR_DBG_RAM_CONSTRAINT_SNTP";
        case PMGR_DBG_RAM_CONSTRAINT_APP_WEBSOCKET_CLIENT:
            return "PMGR_DBG_RAM_CONSTRAINT_APP_WEBSOCKET_CLIENT";
        case PMGR_DBG_RAM_CONSTRAINT_DPM_SUPPLICANT:
            return "PMGR_DBG_RAM_CONSTRAINT_DPM_SUPPLICANT";
        case PMGR_DBG_RAM_CONSTRAINT_DPM_REG_NAME_TIMER:
            return "PMGR_DBG_RAM_CONSTRAINT_DPM_REG_NAME_TIMER";
        case PMGR_DBG_RAM_CONSTRAINT_UDP_IPV6_DPM:
            return "PMGR_DBG_RAM_CONSTRAINT_UDP_IPV6_DPM";
        case PMGR_DBG_RAM_CONSTRAINT_TCPC_OVER_IPV6_DPM:
            return "PMGR_DBG_RAM_CONSTRAINT_TCPC_OVER_IPV6_DPM";
        case PMGR_DBG_RAM_CONSTRAINT_HTTPC:
            return "PMGR_DBG_RAM_CONSTRAINT_HTTPC";
        case PMGR_DBG_RAM_CONSTRAINT_DPM_LWIP_IPV6:
            return "PMGR_DBG_RAM_CONSTRAINT_DPM_LWIP_IPV6";
        case PMGR_DBG_RAM_CONSTRAINT_DPM_KEY:
            return "PMGR_DBG_RAM_CONSTRAINT_DPM_KEY";
        case PMGR_DBG_RAM_CONSTRAINT_WA_CAUSE:
            return "PMGR_DBG_RAM_CONSTRAINT_WA_CAUSE";
        case PMGR_DBG_RAM_CONSTRAINT_RWNX_DRV_TASK:
            return "PMGR_DBG_RAM_CONSTRAINT_RWNX_DRV_TASK";
        case PMGR_DBG_RAM_CONSTRAINT_RWNX_MAC_TASK:
            return "PMGR_DBG_RAM_CONSTRAINT_RWNX_MAC_TASK";
        case PMGR_DBG_RAM_CONSTRAINT_MQTT_SUB:
            return "PMGR_DBG_RAM_CONSTRAINT_MQTT_SUB";
        default:
            return "UNKNOWN_ENUM_VALUE";
    }
}

static void pmgr_print_info(pmgr_ctrl_t * const p_ctrl)
{
    pmgr_dbg_ram_constraint_counter_cause_t cause;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    int dpm_ble_hibernate;
    pmgr_wake_source_t pmgr_wakesrc;
    notifier_array_entry_t *notifier_array = p_instance_ctrl->notifier_array;

    PRINTF("\n");
    PRINTF("Constraints count:\n");

    for (int i = 0; i < PMGR_CONSTRAINT_MAX; i++)
    {
        uint8_t count = 0;
        pmgr_constraints_t constraint = BIT(i);
        RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), constraint, &count);
        PRINTF("%-33s = %d\n", pmgr_constraint_to_string(constraint), count);
    }

    PRINTF("\n");

    RM_PMGR_W_get_wake_source(p_ctrl, &pmgr_wakesrc);

    PRINTF("Wake source bitmap: 0x%08x\n", pmgr_wakesrc);
    PRINTF("Force mode: %s\n\n", pmgr_lld_state_to_string(p_instance_ctrl->lld_power_mode));

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                               WIFI_PROFILE_DPM_DEBUG_RUNTIME_FLAG, &p_instance_ctrl->debug_runtime_flag);
#else
    p_instance_ctrl->debug_runtime_flag = PMGR_DEBUG_RAM_DISABLED;
#endif

    PRINTF("Debug ram constraints counter cause: %d\n\n", p_instance_ctrl->debug_runtime_flag);

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                               WIFI_PROFILE_DPM_BLE_HIBERNATE, &dpm_ble_hibernate);
    PRINTF("DPM BLE hibernate (combo mode): %d\n\n", dpm_ble_hibernate);
#endif

    if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
    {
        for (cause = 0; cause < PMGR_DBG_RAM_CONSTRAINT_MAX; cause++)
        {
            PRINTF("Cause %s of constraint: %d\n", pmgr_dbg_ram_constraint_to_string(cause),
                p_instance_ctrl->ram_counter_cause[cause]);
        }
    }

    PRINTF("Notifiers:\n");
    /* Execut callback functions by the highest order */
    for (int i = 0; i < PMGR_MAX_NOTIFIER_ARRAY; i++)
    {
        if (notifier_array[i].p_callback)
        {
            PRINTF("notif[%d]: id=%d order=%d callback=0x%08x memory=0x%08x\n", i,
                   (int)notifier_array[i].notify_extend.notifier_id,
                   (int)notifier_array[i].notify_extend.order,
                   (int)notifier_array[i].p_callback,
                   (int)notifier_array[i].p_callback_memory);
        }
    }
}

/******************************************************************************
 *
 * Command
 *
 ******************************************************************************/
static bool cmd_pmgr_info(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    if (!RM_PMGR_W_get_ctrl())
    {
        PRINTF("cmd_pmgr_info: PMGR is not configured\n");
        return false;
    }

    pmgr_print_info(RM_PMGR_W_get_ctrl());

	return pdTRUE;
}

static bool cmd_pmgr_enable_debug(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    if (!p_instance_ctrl)
    {
        PRINTF("cmd_pmgr_enable_debug: PMGR is not configured\n");
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_DPM_DEBUG_RUNTIME_FLAG, PMGR_DEBUG_RAM_ENABLED);

    p_instance_ctrl->debug_runtime_flag = PMGR_DEBUG_RAM_ENABLED;
#endif

	return pdTRUE;
}

static bool cmd_pmgr_disable_debug(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    pmgr_instance_ctrl_t * p_instance_ctrl  = RM_PMGR_W_get_ctrl();

    if (!p_instance_ctrl)
    {
        PRINTF("cmd_pmgr_disable_debug: PMGR is not configured\n");
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_DPM_DEBUG_RUNTIME_FLAG, PMGR_DEBUG_RAM_DISABLED);

    p_instance_ctrl->debug_runtime_flag = PMGR_DEBUG_RAM_DISABLED;
#endif

	return pdTRUE;
}

static bool cmd_pmgr_add_constraint(int argc, char *argv[])
{
    uint8_t err;
    pmgr_constraints_t constraint;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    if (argc < 2)
    {
        PRINTF("-Usage: add_constraint [constraints: " CONSTRAINTS_LIST_STR "]\n");
        return true;
    }

    if (argv[1] == 0)
    {
        PRINTF("-Usage: add_constraint [constraints: " CONSTRAINTS_LIST_STR "]\n");
        return true;
    }

    constraint = pmgr_string_to_constraint(argv[1]);

    if (constraint != PMGR_CONSTRAINT_MAX_BIT)
    {
        if (!RM_PMGR_W_get_ctrl())
        {
            PRINTF("cmd_pmgr_add_constraint: PMGR is not configured\n");
            return true;
        }

        if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI]++;
        }
        err = RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), constraint);
    }
    else
    {
        return false;
    }

    return (err == FSP_SUCCESS)? true : false;
}

static bool cmd_pmgr_remove_constraint(int argc, char *argv[])
{
    uint8_t err;
    pmgr_constraints_t constraint;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    if (argc < 2)
    {
        PRINTF("-Usage: remove_constraint [constraints: " CONSTRAINTS_LIST_STR "]\n");
        return true;
    }

    if (argv[1] == 0)
    {
        PRINTF("-Usage: remove_constraint [constraints: " CONSTRAINTS_LIST_STR "]\n");
        return true;
    }

    constraint = pmgr_string_to_constraint(argv[1]);

    if (constraint != PMGR_CONSTRAINT_MAX_BIT)
    {
        if (!RM_PMGR_W_get_ctrl())
        {
            PRINTF("cmd_pmgr_remove_constraint: PMGR is not configured\n");
            return true;
        }

        err = RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), constraint);
        if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
        {
            p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI]--;
        }
    }
    else
    {
        return false;
    }

    return (err == FSP_SUCCESS)? true : false;
}

static bool cmd_pmgr_set_wake_source(int argc, char *argv[])
{
    uint8_t err;
    pmgr_wake_source_t wake_source;

    if (argc < 2)
    {
        PRINTF("-Usage: set_wake_source [wake_sources: " WAKE_SOURCES_LIST_STR "]\n");
        return true;
    }

    if (argv[1] == 0)
    {
        PRINTF("-Usage: set_wake_source [wake_sources: " WAKE_SOURCES_LIST_STR "]\n");
        return true;
    }

    wake_source = pmgr_string_to_wake_source(argv[1]);

    if (wake_source != PMGR_WAKE_SOURCE_NONE)
    {
        if (!RM_PMGR_W_get_ctrl())
        {
            PRINTF("cmd_pmgr_set_wake_source: PMGR is not configured\n");
            return true;
        }

        err = RM_PMGR_W_set_wake_source(RM_PMGR_W_get_ctrl(), wake_source);
    }
    else
    {
        return false;
    }

    return (err == FSP_SUCCESS)? true : false;
}

static bool cmd_pmgr_clear_wake_source(int argc, char *argv[])
{
    uint8_t err;
    pmgr_wake_source_t wake_source;

    if (argc < 2)
    {
        PRINTF("-Usage: clear_wake_source [wake_sources: " WAKE_SOURCES_LIST_STR "\n");
        return true;
    }

    if (argv[1] == 0)
    {
        PRINTF("-Usage: clear_wake_source [wake_sources: " WAKE_SOURCES_LIST_STR "\n");
        return true;
    }

    wake_source = pmgr_string_to_wake_source(argv[1]);

    if (wake_source != PMGR_WAKE_SOURCE_NONE)
    {
        if (!RM_PMGR_W_get_ctrl())
        {
            PRINTF("cmd_pmgr_clear_wake_source: PMGR is not configured\n");
            return true;
        }

        err = RM_PMGR_W_clr_wake_source(RM_PMGR_W_get_ctrl(), wake_source);
    }
    else
    {
        return false;
    }

    return (err == FSP_SUCCESS)? true : false;
}

static bool cmd_pmgr_force(int argc, char *argv[])
{
    uint8_t err = true;
    pmgr_lld_power_mode_t lld_state;
    uint64_t sleep_time_ms = 0;

    if (argc < 2)
    {
        PRINTF("- Usage: force [lld_states: " LLD_STATES_LIST_STR "]\n");
        return true;
    }

    if (argv[1] == 0)
    {
        PRINTF("- Usage: force [lld_states: " LLD_STATES_LIST_STR "]\n");
        return true;
    }

    if (argc == 3)
    {
        sleep_time_ms = ((uint64_t)atoi(argv[2])) * (1000LL);
    }

    lld_state = pmgr_string_to_lld_state(argv[1]);

    if (lld_state != PMGR_LLD_POWER_MODE_INVALID)
    {
        if (!RM_PMGR_W_get_ctrl())
        {
            PRINTF("cmd_pmgr_force: PMGR is not configured\n");
            return true;
        }

        if (lld_state == PMGR_LLD_POWER_MODE_DPM)
        {
            uint8_t const_cnt = 0, i;          

            if (get_run_mode() != 0 /* station mode */)
            {
                PRINTF("cmd_pmgr_force: PMGR_LLD_POWER_MODE_DPM is only valid in STA only mode \n");
                return true;
            }

            if (chk_network_ready(WLAN0_IFACE) == pdFALSE)
            {
                PRINTF("cmd_pmgr_force: PMGR_LLD_POWER_MODE_DPM is only valid with active wifi connection in place \n");
                return true;
            }

            RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RETENTION, &const_cnt);
            if (const_cnt != 1)
            {
                PRINTF("cmd_pmgr_force: PMGR_LLD_POWER_MODE_DPM is only valid with PMGR_CONSTRAINT_POWER_RETENTION set \n");
                return true;
            }

            if (!RM_PMGR_W_dpm_is_enabled())
            {
                 PRINTF("cmd_pmgr_force: PMGR_LLD_POWER_MODE_DPM is only valid with DPM enabled \n");
                 return true;
            }

            RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM, &const_cnt);

            RM_PMGR_W_dpm_timer_create("cli_force_dpm", "f_d_tmr", NULL, sleep_time_ms, 0);

            for(i = 0; i < const_cnt; i++)
            {
                pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
                RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                if (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED)
                {
                    p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE]--;
                }
            }

            return true;
        }
        
        err = RM_PMGR_W_force(RM_PMGR_W_get_ctrl(), lld_state, MS2US(sleep_time_ms));
    }
    else
    {
        return false;
    }

    return (err == FSP_SUCCESS)? true : false;
}

static bool cmd_dpm_ip_condition(int argc, char *argv[])
{
    fsp_err_t err = FSP_SUCCESS;
    uint8_t usage_en = 0;
    unsigned char ip_condition = PMGR_CONDITION_IPV4_MANDATORY;

    switch (argc) {
    case 1:
        RM_WIFI_dpm_ip_condition_get(&ip_condition);
        break;
    case 2:
        ip_condition = (unsigned char)atoi(argv[1]);

        err = RM_WIFI_dpm_ip_condition_set(ip_condition);
        if (err != FSP_SUCCESS) {
            printf("Failed to set DPM IP Condition(%d)\n", ip_condition);
            usage_en = 1;
        }
        break;
    default:
        usage_en = 1;
        break;
    }

    if (usage_en) {
        printf("Usage: %s [IP address condition] (1:IPv4, 2:IPv6, 3:both)\n", argv[0]);
        return pdTRUE;
    }

    if (err == FSP_SUCCESS) {
        printf("%s::DPM IP Condition: %d \n", argv[0], ip_condition);
    }

    return pdTRUE;
}

#if PMGR_DPM_DEBUG_CLI
#include "r_pm_if.h"

static uint32_t time_old[16], time_new[16];
static uint32_t array_id[16];
static void timer_callback(void *_param)
{
    uint32_t *id = _param;

    time_new[*id] = R_BSP_SystemRtcCountGet();
 
    printf("timer id: %d, rtc time: %ld\n", (int)*id, (time_new[*id] - time_old[*id]) >> 15);
}

static bool cmd_dpm_timer_test(int argc, const char *argv[])
{
    uint32_t id;
    uint64_t time;
    dpm_timer_param_t tparam;
    uint32_t set_tparam_null = false; /* for debug purpose */

    if (argc >= 3) {
        id = atoi(argv[1]);
        time = (uint64_t)atoi(argv[2]);
        time *= 1000000;

        if (argc == 4) {
            set_tparam_null = (atoi(argv[3]) == 1) ? true : false;
        }
    } else {
        return false;
    }

    if (time > 0) {
        array_id[id] = id;
        time_old[id] = (uint32_t)R_BSP_SystemRtcCountGet();

        if (set_tparam_null == true) {
            tparam.callback_func = NULL;
            tparam.callback_param = NULL;
            tparam.booting_offset = NULL;
        } else {
            tparam.callback_func = (void *) timer_callback;
            tparam.callback_param = (void *) &(array_id[id]);
            tparam.booting_offset = (void *) (id | (id << 24));
        }
        
        R_DPM_TIMER_SleepSet(id, time, tparam, false);
    }

    return true;
}

static bool cmd_dpm_sleep_test(int argc, const char *argv[])
{
    uint32_t id;
    uint64_t time;
    dpm_timer_param_t tparam;

    if (argc > 2) {
        id = atoi(argv[1]);
        time = (uint64_t)atoi(argv[2]);
        time *= 1000;
    } else {
        return false;
    }

    if (time > 0) {
        array_id[id] = id;
        time_old[id] = (uint32_t)R_BSP_SystemRtcCountGet();
        tparam.callback_func = NULL;
        tparam.callback_param = NULL;
        tparam.booting_offset = NULL;
        R_DPM_TIMER_SleepSet(id,(time), tparam, true);
    }

    return true;
}

static bool cmd_dpm_udphm(int argc, const char *argv[])
{
    /*  e.g.) udphm show */

    if (argc == 2) {
        if (strcasecmp(argv[1], "udph_tbl") == 0) {
            ptim_udph_conf_status_print();
        } else if (strcasecmp(argv[1], "udp_sock_info") == 0) {
            ptim_udph_sock_status_print();
        }
    } else {
        printf("syntax: udph <option>\n");
		printf("   <option> udph_tbl      : show udph table user UDP app registered \n");
        printf("            udp_sock_info : show active udp sockets in lwip \n");
    }

    return true;
}

static bool cmd_dpm_timer_list(int argc, const char *argv[])
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    R_DPM_TIMER_PrintList();

    return pdTRUE;
}

static bool cmd_dpm_timer_all_test(int argc, const char *argv[])
{
    int i;

    int tmp_argc;
    char *tmp_argv[3] = {0,};
    char tmp_buf[3][10] = {0,};

    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    tmp_argc = 3;

    for (i = 5; i < 16; i++) {
        sprintf(tmp_buf[0], "timer");
        sprintf(tmp_buf[1], "%d", i);
        sprintf(tmp_buf[2], "%d", i+20);
        tmp_argv[0] = tmp_buf[0];
        tmp_argv[1] = tmp_buf[1];
        tmp_argv[2] = tmp_buf[2];

        cmd_dpm_timer_test(tmp_argc, (const char **)tmp_argv);
    }

    return true;
}

static bool cmd_dpm_timer_kill(int argc, const char *argv[])
{
    uint32_t id;

    if (argc > 1) {
        id = atoi(argv[1]);
    } else {
        return pdFALSE;
    }

    R_DPM_TIMER_Kill(id);

    return pdTRUE;
}

static bool cmd_sleep(int argc, const char *argv[])
{
    int mode; // 2 deep sleep, 3 extened sleep, sleep 4, sleep 5
    int sleep_time; // second

    if (argc != 3) {
        return 0;
    }

    mode = atoi(argv[1]);
    sleep_time = atoi(argv[2]);

    if (mode == 2) {
        R_PM_LowPowerModeEnter(PMGR_LLD_POWER_MODE_SLEEP2, SEC_TO_RTC_TICKS(sleep_time));
    } else if (mode == 3) {
        R_PM_LowPowerModeEnter(PMGR_LLD_POWER_MODE_SLEEP3, SEC_TO_RTC_TICKS(sleep_time));
    }

    return pdTRUE;
}
#endif /* PMGR_DPM_DEBUG_CLI */

/**
 * command for multicast address filter in TIM as white list
 */
static bool cmd_set_mc_filter(int argc, char *argv[])
{
    extern int select_ipaddr_type_from_str(char* str, struct sockaddr_storage *ipaddr);

    struct sockaddr_storage valid_ip = {0,};
    ip4_addr_t tmp_addr4;
    ip6_addr_t tmp_addr6;

    unsigned long mc_filter_addr = 0;
    int ip_type = 0;

    if (argc < 2) {
        PRINTF("- Usage : set_mc  [IPv4|IPv6(mc addr)]\n");
        return false;
    }

    ip_type = select_ipaddr_type_from_str(argv[1], &valid_ip);

    /* Common */
    if ((strcmp(argv[1], "0") != 0)
#if defined ( __SUPPORT_IPV4__ )
            && (ip_type != IPADDR_TYPE_V4)
#endif // __SUPPORT_IPV4__
#if defined ( __SUPPORT_IPV6__ )
            && (ip_type != IPADDR_TYPE_V6)
#endif // __SUPPORT_IPV6__
       )
    {
        PRINTF("- Usage : set_mc  [IPv4|IPv6(mc addr)]\n");
        return false;
    }

    if (ip_type == IPADDR_TYPE_V4) {
        ip4addr_aton(argv[1], &tmp_addr4);
        mc_filter_addr = lwip_htonl(ip4_addr_get_u32(&tmp_addr4)) & 0xffffffff;
        RM_WIFI_dpm_ptim_mc_filter_set(mc_filter_addr);
    } else if (ip_type == IPADDR_TYPE_V6) {
        ip6addr_aton(argv[1], &tmp_addr6);
        RM_WIFI_dpm_ptim_mcv6_filter_set((uint16_t *)tmp_addr6.addr);
    }

    return true;
}

static bool cmd_tcpd(int argc, char *argv[])
{
    uint8_t usage_en = 0;
    int32_t no;
    uint16_t dport;

    switch (argc) {
    case 3:
        dport = atoi(argv[2]);
        no = atoi(argv[1]);
        romac4rtos_set_tcport(no, dport);
        break;
    default:
        usage_en = 1;
        break;
    }

    if (usage_en) {
        printf("Usage: %s [no] [dest port]\n", argv[0]);
        return pdTRUE;
    }

    printf("%s::tcp_dest_port%d=%d(%04x)\n", argv[0], (int)no,
           romac4rtos_get_tcport(no), romac4rtos_get_tcport(no));

    return pdTRUE;
}

static bool cmd_udpd(int argc, char *argv[])
{
    uint8_t usage_en = 0;
    int32_t no;
    uint16_t dport;

    switch (argc) {
    case 3:
        dport = atoi(argv[2]);
        no = atoi(argv[1]);
        romac4rtos_set_udport(no, dport);
        break;
    default:
        usage_en = 1;
        break;
    }

    if (usage_en) {
        printf("Usage: %s [no] [dest port]\n", argv[0]);
        return pdTRUE;
    }

    printf("%s::udp_dest_port%d=%d(%04x)\n", argv[0],(int) no,
           romac4rtos_get_udport(no), romac4rtos_get_udport(no));

    return pdTRUE;
}

static const debug_handler_t pmgr_handlers[] = {
    { "status"           ,"PMGR show status",                     (debug_callback_t)cmd_pmgr_info              },
    { "add_constraint"   ,"PMGR add constraint [constraint]",     (debug_callback_t)cmd_pmgr_add_constraint    },
    { "remove_constraint","PMGR remove constraint [constraint]",  (debug_callback_t)cmd_pmgr_remove_constraint },
    { "set_wake_source"  ,"PMGR set wake source [wake_source]",   (debug_callback_t)cmd_pmgr_set_wake_source   },
    { "clear_wake_source","PMGR clear wake source [wake_source]", (debug_callback_t)cmd_pmgr_clear_wake_source },
    { "force"            ,"PMGR force [state] [sleep_time_sec] where [sleep_time_sec] is optional", (debug_callback_t)cmd_pmgr_force},
    { "enable_debug"     ,"PMGR enable debug ram constraints counter cause", (debug_callback_t)cmd_pmgr_enable_debug},
    { "disable_debug"    ,"PMGR disable debug ram constraints counter cause", (debug_callback_t)cmd_pmgr_disable_debug},
    { "ip_cond",         "IPv4/6 condition check for entering PMGR_LLD_POWER_MODE_DPM", (debug_callback_t)cmd_dpm_ip_condition},
#if PMGR_DPM_DEBUG_CLI   
    { "dpm_sleep"        ,"timer [id] [msec]",                    (debug_callback_t)cmd_dpm_sleep_test       },
    { "dpm_timer"        ,"timer [id] [sec]",                     (debug_callback_t)cmd_dpm_timer_test       },
    { "dpm_all_timer"    ,"alltimer, alloc all dpm timer",        (debug_callback_t)cmd_dpm_timer_all_test   },
    { "dpm_timer_list"   ,"timer_list, print out timer list",     (debug_callback_t)cmd_dpm_timer_list       },
    { "dpm_kill_timer"   ,"kill timer [id] ",                     (debug_callback_t)cmd_dpm_timer_kill       },
    { "pm_sleep",        "timer [id] [second]",                   (debug_callback_t)cmd_sleep                },
    { "udphm",           "udp hole punching multi-session debug", (debug_callback_t)cmd_dpm_udphm },
#endif /* PMGR_DPM_DEBUG_CLI */
    { "set_mc",  		"set_mc [mc_ip] ",             			  (debug_callback_t)cmd_set_mc_filter          },
    { "tcpd",    		"TCP Dest Port",               			  (debug_callback_t)cmd_tcpd                   },
    { "udpd",    		"UDP Dest Port",               			  (debug_callback_t)cmd_udpd                   },
    { NULL },
};

bool pmgr_command(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

    return debug_handle_message(argc, argv, pmgr_handlers);
}
#endif /* CFG_PMGR */
