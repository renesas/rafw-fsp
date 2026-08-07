/**
 ****************************************************************************************
 *
 * @file rm_cli_w_task.c
 *
 * @brief BLE CLI task
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

#include <stdio.h>
#include "sdk_defs.h"
#include "rm_wifi_config.h"
#include <FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>
#include "rm_cli_w.h"
#include "rm_cli_w_sys.h"
#include "rm_cli_w_mem.h"
#include "rm_cli_w_nvram.h"
#include "rm_cli_w_net.h"
#include "rm_cli_w_os.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w_lmac.h"
#include "rm_cli_w_sbrom.h"
#if CFG_PMGR
#include "rm_cli_w_dpm.h"
#include "rm_cli_w_pmgr.h"
#endif /* CFG_PMGR */
#if CMD_DPMD
#include "rm_cli_w_dpmd.h"
#endif /* CMD_DPMD */
#include "rm_cli_w_usr_nvram.h"
#if defined (__MATTER_CLI_DEBUG__)
#include "rm_cli_w_matter.h"
#endif
#if WIFI_CFG_BLE_CLI_ENABLE
#include "cli_ble.h"
#endif
#include "rm_cli_w_wifi.h"

/**
 * CLI notification mask.
 */
#define CLI_NOTIF       (uint32_t) (1 << 31)

/**
 * GPIO Wkup notification mask.
 */
#define GPIO_WKUP_NOTIF (1 << 30)

extern void help_root_cmd(void);
#if CFG_WIFI
extern bool mac_command(int argc, const char *argv[], void *user_data);
#endif

# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static const cli_command_t cli_commands[] = {
    { "sys",    sys_command,    NULL },
    { "mem",    mem_command,    NULL },
    { "os",     os_command,     NULL },
    { "nvram",  nvram_command,  NULL },
#if CFG_WIFI
    { "mac",    mac_command,    NULL },
    { "net",    net_command,    NULL },
    { "lmac",   lmac_command,   NULL },
#endif
    { "sbrom",  sbrom_command,  NULL },
#if defined ( __SUPPORT_USR_NVRAM_CMD__)
    { "user",   usr_nv_command, NULL },
#endif /* __SUPPORT_USR_NVRAM_CMD__ */
#if CFG_PMGR
#if PMGR_DPM_DEBUG_CLI
    { "dpm",    dpm_command,    NULL },
#endif /* PMGR_DPM_DEBUG_CLI */
    { "pmgr",   pmgr_command,   NULL },
#endif /* CFG_PMGR */
#if CMD_DPMD
    { "dpmd",   dpmd_command,    NULL },
#endif /* CMD_DPMD */
#if WIFI_CFG_BLE_CLI_ENABLE
    { "ble",    ble_command,   NULL },
#endif
#if CFG_WIFI
    { "wifi",   wifi_command,  NULL },
#endif
#if defined (__MATTER_CLI_DEBUG__)
    { "matter",  matter_command,    NULL },
#endif
    { NULL },
};
#else
static const cli_command_t cli_commands[] = {
#ifndef __SUPPORT_PRODTEST__
    { "mem",    mem_command,    NULL },
#endif
    { "lmac",   lmac_command,   NULL },
    { NULL },
};
#endif

static bool default_handler(int argc, const char *argv[], void *user_data)
{
    (void) argc;
    (void) argv;
    (void) user_data;

    const cli_command_t *command;

    // if(common_handle_message(argc, argv)==0)
    {
        help_root_cmd();

        printf("\ncommand categories:\n");
        for (command = cli_commands; command->name; command++) {
            printf("\t %s\r\n", command->name);
        }
    }

    return true;
}

void cli_task(void *params)
{
    (void) params;

    cli_t cli;

    cli = cli_register(CLI_NOTIF, cli_commands, (cli_handler_t)default_handler);

    for (;;) {
        BaseType_t ret;
        uint32_t notif;

        ret = xTaskNotifyWait(0, (uint32_t) -1, &notif, portMAX_DELAY);
        configASSERT(ret == pdPASS);

        if (notif & CLI_NOTIF) {
            cli_handle_notified(cli);
        }
    }
}
