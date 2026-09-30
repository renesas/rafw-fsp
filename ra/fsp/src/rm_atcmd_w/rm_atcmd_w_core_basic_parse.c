/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "rm_atcmd_w_core_basic_parse.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "rm_atcmd_w_core_utils.h"
#include "ctype.h"
#include "strings.h"
#include "FreeRTOS.h"
#include "task.h"

#include "rm_wifi.h"

#include "fw_version.h"
#if CFG_WIFI
 #include "iface_defs.h"
 #include "common_def.h"
 #include "supp_config.h"
 #include "lwip/ip_addr.h"
 #include "rm_lwip_w_helper.h"
 #include "rm_wifi.h"
 #include "rm_wifi_helper.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "dhcpserver.h"
#endif                                 /* CFG_WIFI */
#include "r_rtc_w.h"
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
 #include dg_configADNVPARAM_PROJ_FILE
#endif

#if defined(__SUPPORT_OTA__)
 #if CFG_WIFI
  #include "ota_update_common.h"
  #include "ra6w1_image.h"
 #endif                                /* CFG_WIFI */
#endif                                 /* (__SUPPORT_OTA__) */
#if (ATCMD_TRANSPORT_UART_W == 1)
 #include "rm_atcmd_transport_uart_w.h"
#endif
#if (SUPPORT_FSP_RM_OTA_W == 1)
 #include "rm_ota_w.h"
#endif                                 /* SUPPORT_FSP_RM_OTA_W */
#if defined(__OTA_UPDATE_MCU_FW__)
 #include "ota_update_mcu_fw.h"
#endif                                 /* __OTA_UPDATE_MCU_FW__ */
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
 #include dg_configADNVPARAM_PROJ_FILE
#endif

#define RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(atcmd)     # atcmd
#define RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(atcmd)    "AT+" # atcmd

#define RM_ATCMD_W_CORE_BASIC_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(atcmd) \
    RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(atcmd) \
    RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(atcmd) \
    RM_ATCMD_W_CORE_BASIC_ ## atcmd ## _brief_cb

#if (ATCMD_TRANSPORT_UART_W == 1)

// temporary, to cover whole help texts of all commands
 #define RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN    (1024 * 15)
 #define RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT      (0)
#else
 #define RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN    (255)
 #define RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT      (1)
#endif

#if (ATCMD_BLE_BRG == 1)
 #define RM_ATCMD_W_CORE_BASICE_ATBLE_ALL          "ALL"
 #define RM_ATCMD_W_CORE_BASICE_ATBLE_RESET        "RESET"
 #define RM_ATCMD_W_CORE_BASICE_ATBLE_BRG          "BRG"
 #define RM_ATCMD_W_CORE_BASICE_ATBLE_PINS         "PINS"
#endif

#if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
#endif                                 /* SUPPORT_FSP_RM_OTA_W */

#if (ATCMD_TRANSPORT_UART_W == 1)
uart_cfg_t            g_uart_conf_atb         = {0};
uart_w_extended_cfg_t g_uart_conf_extend_atb  = {0};
uart_w_baud_setting_t g_uart_baud_setting_atb = {0};
uint32_t              g_uart_baud_atb         = 0xFFFFFFFF;
#endif

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(BRIEF);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(BRIEF);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(BRIEF);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(HELP);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(HELP);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(HELP);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(AT);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(AT);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(AT);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(CMD_LIST);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(CMD_LIST);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(CMD_LIST);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATZ);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATZ);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATZ);

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATF);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATF);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATF);
#endif                                 /* CFG_WIFI */

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATE);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATE);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATE);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATQ);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATQ);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATQ);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATB);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATB);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATB);

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(RESTART);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(RESTART);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(RESTART);
#endif                                 /* CFG_WIFI */

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(CHIPNAME);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(CHIPNAME);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(CHIPNAME);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(VER);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(VER);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(VER);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(SDKVER);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(SDKVER);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(SDKVER);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(TIME);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(TIME);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(TIME);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(RLT);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(RLT);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(RLT);

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(TZONE);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(TZONE);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(TZONE);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(DEFAP);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(DEFAP);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(DEFAP);

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(DEFCCRNT);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(DEFCCRNT);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(DEFCCRNT);
#endif                                 /* CFG_WIFI */

#if (ATCMD_BLE_BRG == 1)
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(BLEBRG);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(BLEBRG);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(BLEBRG);
#endif

#if (ATCMD_SECURE_CHANNEL == 1)

/* AT command secure channel */
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATS);
RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATS);
RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATS);
#endif

const atcmd_w_core_module_t at_core_basic_module[] =
{
    {
        "?",
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(BRIEF),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(BRIEF),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(BRIEF),
    },
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(HELP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(HELP),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(HELP),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(HELP),
    },
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(AT),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(AT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(AT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(AT),
    },
    {
        "AT+",
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(CMD_LIST),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(CMD_LIST),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(CMD_LIST),
    },
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATZ),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATZ),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATZ),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATZ),
    },
#if CFG_WIFI
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATF),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATF),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATF),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATF),
    },
#endif                                 /* CFG_WIFI */
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATE),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATE),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATE),
    },
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATQ),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATQ),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATQ),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATQ),
    },
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATB),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATB),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATB),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATB),
    },
#if CFG_WIFI
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(RESTART),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(RESTART),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(RESTART),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(RESTART),
    },
#endif                                 /* CFG_WIFI */
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(CHIPNAME),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(CHIPNAME),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(CHIPNAME),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(CHIPNAME),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(VER),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(VER),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(VER),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(VER),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(SDKVER),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(SDKVER),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(SDKVER),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(SDKVER),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(TIME),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(TIME),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(TIME),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(TIME),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(RLT),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(RLT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(RLT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(RLT),
    },
#if CFG_WIFI
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(TZONE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(TZONE),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(TZONE),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(TZONE),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(DEFAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(DEFAP),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(DEFAP),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(DEFAP),
    },
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(DEFCCRNT),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(DEFCCRNT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(DEFCCRNT),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(DEFCCRNT),
    },
#endif                                 /* CFG_WIFI */

#if (ATCMD_BLE_BRG == 1)
    {
        RM_ATCMD_W_CORE_BASIC2_ATCMD_CODE(BLEBRG),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(BLEBRG),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(BLEBRG),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(BLEBRG),
    },
#endif
#if (ATCMD_SECURE_CHANNEL == 1)
    {
        RM_ATCMD_W_CORE_BASIC_ATCMD_CODE(ATS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_BASIC_ATCMD_CB_P(ATS),
        RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB_P(ATS),
        RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB_P(ATS),
    },
#endif
    {
        NULL,
        ATCMD_W_TYPE_MAX,
        0,
        0,
        NULL,
        NULL,
        NULL
    },
};

#if (ATCMD_SECURE_CHANNEL == 1)
static bool is_all_zero (uint8_t * arr, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (0 != arr[i])
        {
            return false;
        }
    }

    return true;
}

static int is_valid_iv_hex_arg (const char * s)
{
    size_t i;

    if (!s)
    {
        return 0;
    }

    for (i = 0; i < IV_HEX_LEN_AT; i++)
    {
        if (!is_hex_char(s[i]))
        {
            return 0;
        }
    }

    return 1;
}

#endif                                 /* ATCMD_SECURE_CHANNEL */

static void rm_atcmd_w_core_basic_help_concat_and_update_params (char * p_in,
                                                                 int  * p_inlen,
                                                                 char * p_out,
                                                                 int  * p_outlen)
{
    *p_inlen = strlen(p_in);
    sprintf(p_out + (*p_outlen), "%s", p_in);
    *p_outlen += *p_inlen;
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_basic_help_cmd (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_atcmd_err_code                     err                   = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t             * p_ctrl                = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    atcmd_w_core_module_list_t               * p_list                = &p_ctrl->list;
    const atcmd_w_core_module_t              * p_module              = NULL;
    const atcmd_w_core_module_node_t         * p_module_node         = NULL;
    const atcmd_w_core_unfixed_module_t      * p_unfixed_module      = NULL;
    const atcmd_w_core_unfixed_module_node_t * p_unfixed_module_node = NULL;
    int          is_esc_at_cmd       = false;
    const char * unfixed_at_cmd_str  = NULL;
    char       * input_at_cmd_str    = NULL;
    char       * p_help_result_buf   = NULL;
    int          help_result_str_len = 0;
    char         temp_str[256]       = {0, };
    char         temp_str_2[32]      = {0, };
    int          temp_str_len        = 0;

    switch (argc)
    {
        case 0:
        case 1:
        {
            /* help or ?*/
            p_help_result_buf = pvPortMalloc(RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);

            if (p_help_result_buf == NULL)
            {
                printf("- [%s] Failed to allocate the print buffer\n", __func__);

                return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            }

            memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);

LIST_ALL:
            bsp_safe_strcpy(temp_str, "\r\nAT Commands:\r\n", sizeof(temp_str));
            rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                &temp_str_len,
                                                                p_help_result_buf,
                                                                &help_result_str_len);

            /* Core modules */
            for (p_module_node = p_list->p_module_head; p_module_node != NULL; p_module_node = p_module_node->next)
            {
                for (p_module = p_module_node->module; p_module->p_at_cmd != NULL; p_module++)
                {
                    if ((p_module->p_format_callback != NULL) && (strcmp(p_module->p_format_callback(), "") != 0))
                    {
                        sprintf(temp_str, "    %s%s%s\r\n", p_module->p_at_cmd,
                                (p_module->input_var > 0 ? AT_CMD_CLASS_BC_EXT : " "), p_module->p_format_callback());
                    }
                    else
                    {
                        sprintf(temp_str, "    %s\r\n", p_module->p_at_cmd);
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    if ((strlen(temp_str) + help_result_str_len + 1) > RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN)
#else
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                        memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                        help_result_str_len = 0;
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif

                    if ((p_module->p_brief_callback != NULL) && (strcmp(p_module->p_brief_callback(), "") != 0))
                    {
                        sprintf(temp_str, "        - %s\r\n", p_module->p_brief_callback());
                    }
                    else
                    {
                        sprintf(temp_str, "        - No example for %s\r\n", p_module->p_at_cmd);
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    if ((strlen(temp_str) + help_result_str_len + 1) > RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN)
#else
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                        memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                        help_result_str_len = 0;
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif
                }
            }

            /* Core unfixed_modules */
            for (p_unfixed_module_node = p_list->p_unfixed_module_head;
                 p_unfixed_module_node != NULL;
                 p_unfixed_module_node = p_unfixed_module_node->next)
            {
                for (p_unfixed_module = p_unfixed_module_node->module;
                     p_unfixed_module->at_cmd_len != 0;
                     p_unfixed_module++)
                {
                    unfixed_at_cmd_str = NULL;

                    if (p_unfixed_module->at_cmd[0] == AT_CMD_ESC_KEY_CHAR)
                    {
                        unfixed_at_cmd_str = (char *) (&(p_unfixed_module->at_cmd[1]));
                        is_esc_at_cmd      = true;
                    }
                    else
                    {
                        unfixed_at_cmd_str = p_unfixed_module->at_cmd;
#if defined(__OTA_UPDATE_MCU_FW__)
                        if (strcasecmp(unfixed_at_cmd_str, OTA_BY_MCU) == 0)
                        {
                            continue;
                        }
#endif                                 /* __OTA_UPDATE_MCU_FW__ */
                        is_esc_at_cmd = false;
                    }

                    memset(temp_str_2, 0x00, 32);

                    if (is_esc_at_cmd)
                    {
                        sprintf(temp_str_2, "%s%s", AT_CMD_ESC_HELP_STR, unfixed_at_cmd_str);
                    }

                    if ((p_unfixed_module->p_format_callback != NULL) &&
                        (strcmp(p_unfixed_module->p_format_callback(), "") != 0))
                    {
                        sprintf(temp_str,
                                "    %s%s%s\r\n",
                                (is_esc_at_cmd ? "" : unfixed_at_cmd_str),
                                is_esc_at_cmd ? "" : AT_CMD_CLASS_BC_EXT,
                                p_unfixed_module->p_format_callback());
                    }
                    else
                    {
                        sprintf(temp_str, "    %s\r\n", (is_esc_at_cmd ? temp_str_2 : unfixed_at_cmd_str));
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    if ((strlen(temp_str) + help_result_str_len + 1) > RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN)
#else
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif                                 /* RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0 */
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                        memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                        help_result_str_len = 0;
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif                                 /* RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0 */

                    if ((p_unfixed_module->p_brief_callback != NULL) &&
                        (strcmp(p_unfixed_module->p_brief_callback(), "") != 0))
                    {
                        sprintf(temp_str, "        - %s\r\n", p_unfixed_module->p_brief_callback());
                    }
                    else
                    {
                        sprintf(temp_str, "        - No example for %s\r\n",
                                (is_esc_at_cmd ? temp_str_2 : unfixed_at_cmd_str));
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    if ((strlen(temp_str) + help_result_str_len + 1) > RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN)
#else
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif                                 /* RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0 */
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                        memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                        help_result_str_len = 0;
                    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
                    rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                        &temp_str_len,
                                                                        p_help_result_buf,
                                                                        &help_result_str_len);
#endif                                 /* RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0 */
                }
            }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
#endif

            if (p_help_result_buf)
            {
                vPortFree(p_help_result_buf);
                p_help_result_buf = NULL;
            }

            break;
        }

        case 2:
        {
            /* help <cmd>, ?=? */
            if (strcasecmp(argv[0], "?") == 0)
            {
                if (strcasecmp(argv[1], "?") == 0)
                {
                    goto LIST_ALL;
                }
                else
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    break;
                }
            }

            err = FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD;

            /* Core module */
            for (p_module_node = p_list->p_module_head; p_module_node != NULL; p_module_node = p_module_node->next)
            {
                for (p_module = p_module_node->module; p_module->p_at_cmd != NULL; p_module++)
                {
                    if (strcasecmp(argv[1], p_module->p_at_cmd) == 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_CMD_OK;

                        if (((p_module->p_format_callback() == NULL) ||
                             (strcmp(p_module->p_format_callback(), "") == 0)) &&
                            ((p_module->p_brief_callback() == NULL) || (strcmp(p_module->p_brief_callback(), "") == 0)))
                        {
                            temp_str_len = sprintf(temp_str, "\r\n        - No example for %s\r\n", argv[1]);
                            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) temp_str, temp_str_len);
                        }
                        else
                        {
                            sprintf(temp_str, "\r\n    %s%s%s\r\n", p_module->p_at_cmd,
                                    (p_module->input_var > 0 ? AT_CMD_CLASS_BC_EXT : " "),
                                    ((p_module->p_format_callback() != NULL) ? p_module->p_format_callback() : ""));

                            if ((p_module->p_brief_callback() != NULL) &&
                                (strcmp(p_module->p_brief_callback(), "") != 0))
                            {
                                temp_str_len = sprintf(temp_str, "          - %s\r\n", p_module->p_brief_callback());
                                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) temp_str, temp_str_len);
                            }
                        }

                        return err;
                    }
                }
            }

            err = FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD;

            /* Core unfixed module */
            for (p_unfixed_module_node = p_list->p_unfixed_module_head;
                 p_unfixed_module_node != NULL;
                 p_unfixed_module_node = p_unfixed_module_node->next)
            {
                for (p_unfixed_module = p_unfixed_module_node->module;
                     p_unfixed_module->at_cmd_len != 0;
                     p_unfixed_module++)
                {
                    if (strncasecmp(argv[1], AT_CMD_ESC_HELP_STR, 5) == 0)
                    {
                        if (p_unfixed_module->at_cmd[0] == AT_CMD_ESC_KEY_CHAR)
                        {
                            unfixed_at_cmd_str = ((char *) (&(p_unfixed_module->at_cmd[1])));
                            input_at_cmd_str   = (argv[1] + 5);
                            is_esc_at_cmd      = true;
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        unfixed_at_cmd_str = p_unfixed_module->at_cmd;
                        input_at_cmd_str   = argv[1];
                        is_esc_at_cmd      = false;
                    }

                    if (strcasecmp(input_at_cmd_str, unfixed_at_cmd_str) == 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_CMD_OK;

                        if (((p_unfixed_module->p_format_callback() == NULL) ||
                             (strcmp(p_unfixed_module->p_format_callback(), "") == 0)) &&
                            ((p_unfixed_module->p_brief_callback() == NULL) ||
                             (strcmp(p_unfixed_module->p_brief_callback(), "") == 0)))
                        {
                            temp_str_len = sprintf(temp_str, "\r\n        - No example for %s\r\n", input_at_cmd_str);
                            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) temp_str, temp_str_len);
                        }
                        else
                        {
                            sprintf(temp_str,
                                    "\r\n    %s%s%s\r\n",
                                    is_esc_at_cmd ? "" : unfixed_at_cmd_str,
                                    is_esc_at_cmd ? "" : AT_CMD_CLASS_BC_EXT,
                                    ((p_unfixed_module->p_format_callback() != NULL) ?
                                     p_unfixed_module->p_format_callback() : ""));

                            if ((p_unfixed_module->p_brief_callback() != NULL) &&
                                (strcmp(p_unfixed_module->p_brief_callback(), "") != 0))
                            {
                                temp_str_len = sprintf(temp_str,
                                                       "          - %s\r\n",
                                                       p_unfixed_module->p_brief_callback());
                                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) temp_str, temp_str_len);
                            }
                        }

                        return err;
                    }
                }
            }

            break;
        }
    }

    return err;
}

uint32_t RM_ATCMD_W_CORE_BASIC_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_basic_module);
}

uint32_t RM_ATCMD_W_CORE_BASIC_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_basic_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(BRIEF)
{
    return rm_atcmd_w_core_basic_help_cmd(p_at_ctrl, argc, argv);
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(BRIEF)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(BRIEF)
{
    const char * p_description = "Commands list with brief and usage";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(HELP)
{
    return rm_atcmd_w_core_basic_help_cmd(p_at_ctrl, argc, argv);
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(HELP)
{
    const char * p_usage = "<command>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(HELP)
{
    const char * p_description = "Print help message.";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(AT)
{
    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(AT)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(AT)
{
    const char * p_description = "Attention command";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(CMD_LIST)
{
    fsp_err_atcmd_err_code                     err                   = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t             * p_ctrl                = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    atcmd_w_core_module_list_t               * p_list                = &p_ctrl->list;
    const atcmd_w_core_module_t              * p_module              = NULL;
    const atcmd_w_core_module_node_t         * p_module_node         = NULL;
    const atcmd_w_core_unfixed_module_t      * p_unfixed_module      = NULL;
    const atcmd_w_core_unfixed_module_node_t * p_unfixed_module_node = NULL;
    char * p_help_result_buf   = NULL;
    int    help_result_str_len = 0;
    char   temp_str[256]       = {0, };
    int    temp_str_len        = 0;

    if ((argc > 1) && (strcasecmp(argv[1], "?") != 0))
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
    p_help_result_buf = pvPortMalloc((RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN / 2));
#else
    p_help_result_buf = pvPortMalloc(RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
#endif

    if (p_help_result_buf == NULL)
    {
        printf("- [%s] Failed to allocate the print buffer\n", __func__);

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    /* Core module */
    for (p_module_node = p_list->p_module_head; p_module_node != NULL; p_module_node = p_module_node->next)
    {
        for (p_module = p_module_node->module; p_module->p_at_cmd != NULL; p_module++)
        {
            if (strncasecmp(p_module->p_at_cmd, "AT", 2) == 0)
            {
                sprintf(temp_str, "\r\n%s", p_module->p_at_cmd);
                rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                    &temp_str_len,
                                                                    p_help_result_buf,
                                                                    &help_result_str_len);

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 1)
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                help_result_str_len = 0;
#endif
            }
        }
    }

    /* Core unfixed module */
    for (p_unfixed_module_node = p_list->p_unfixed_module_head;
         p_unfixed_module_node != NULL;
         p_unfixed_module_node = p_unfixed_module_node->next)
    {
        for (p_unfixed_module = p_unfixed_module_node->module; p_unfixed_module->at_cmd_len != 0; p_unfixed_module++)
        {
            int need_to_add = 0;

            if (strncasecmp(p_unfixed_module->at_cmd, "AT", 2) == 0)
            {
                sprintf(temp_str, "\r\n%s", p_unfixed_module->at_cmd);
                need_to_add = true;
            }
            else if (p_unfixed_module->at_cmd[0] == AT_CMD_ESC_KEY_CHAR)
            {
                sprintf(temp_str, "\r\n%s%s", AT_CMD_ESC_HELP_STR, (char *) (&(p_unfixed_module->at_cmd[1])));
                need_to_add = true;
            }

            if (need_to_add)
            {
                rm_atcmd_w_core_basic_help_concat_and_update_params(temp_str,
                                                                    &temp_str_len,
                                                                    p_help_result_buf,
                                                                    &help_result_str_len);

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 1)
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
                memset(p_help_result_buf, 0x00, RM_ATCMD_W_CORE_BASIC_HELP_CMD_BUF_LEN);
                help_result_str_len = 0;
#endif
            }
        }
    }

#if (RM_ATCMD_W_CORE_BASIC_CHUNKED_OUTPUT == 0)
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_help_result_buf, strlen(p_help_result_buf));
#endif

    if (p_help_result_buf)
    {
        vPortFree(p_help_result_buf);
        p_help_result_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(CMD_LIST)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(CMD_LIST)
{
    const char * p_description = "List available commands";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATZ)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    char result_str[64] = {0x00, };
    p_ctrl->q_result     = 1;
    p_ctrl->uart_echo_on = 0;
    p_ctrl->echo_on      = 0;
    sprintf(result_str, "%s\r\n", "Display result on\nEcho off");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATZ)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATZ)
{
    const char * p_description = "AT command initialize";

    return p_description;
}

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATF)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
 #if 0                                 //! defined(RM_ATCMD_W_CORE_BASIC_NOT_IMPLEMETED)
  #if defined(__SUPPORT_ATCMD_TLS__)
    atcmd_transport_ssl_reboot();
  #endif                               // (__SUPPORT_ATCMD_TLS__)
 #endif
    factory_reset(1);

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATF)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATF)
{
    const char * p_description = "Restore to Factory mode (NVRAM clean)";

    return p_description;
}
#endif                                 /* CFG_WIFI */

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATE)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    char result_str[32] = {0x00, };

    if ((argc != 1) && !rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    if (argc == 1)
    {
        if (p_ctrl->uart_echo_on)
        {
            p_ctrl->uart_echo_on = 0;
        }
        else
        {
#if (ATCMD_SECURE_CHANNEL == 1)
            if (p_ctrl->secure_channel == 1)
            {
                p_ctrl->uart_echo_on = 0; /* disable echo in secure channel */
            }
            else
#endif /* ATCMD_SECURE_CHANNEL */
            {
                p_ctrl->uart_echo_on = 1;
            }
        }

        p_ctrl->echo_on = p_ctrl->uart_echo_on;
    }

    if (p_ctrl->uart_echo_on)
    {
        sprintf(result_str, "%s\r\n", "Echo on");
    }
    else
    {
        sprintf(result_str, "%s\r\n", "Echo off");
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATE)
{
    const char * p_usage = "[<?>]";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATE)
{
    const char * p_description = "Command echo";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATQ)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    char result_str[64] = {0x00, };

    if ((argc != 1) && !rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    if (argc == 1)
    {
        if (p_ctrl->q_result)
        {
            p_ctrl->q_result = 0;
        }
        else
        {
            p_ctrl->q_result = 1;
        }
    }

    if (p_ctrl->q_result)
    {
        sprintf(result_str, "%s\r\n", "Display result on");
    }
    else
    {
        sprintf(result_str, "%s\r\n", "Display result off");
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATQ)
{
    const char * p_usage = "[<?>]";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATQ)
{
    const char * p_description = "Result Codes On/Off";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATB)
{
#if (ATCMD_TRANSPORT_UART_W == 1)
    atcmd_w_core_instance_ctrl_t           * p_ctrl          = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl->
                                                               p_transport_instance->p_ctrl;
    uart_instance_t      * p_uart   = NULL;
    fsp_err_atcmd_err_code err      = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err  = FSP_SUCCESS;
    uint32_t               baudrate = 0;
    int  databits       = 0;
    int  parity         = 0;
    int  stopbits       = 0;
    int  flow_control   = 0;
    char ret_msg[20]    = {0x00, };
    char result_str[64] = {0x00, };

    p_ctrl->q_result = 1;

 #if (ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_instance_ctrl);
 #endif

    p_uart = (uart_instance_t *) p_instance_ctrl->uart_instance_objects[0];

 #if (ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_uart);
 #endif

    if ((argc == 2) || (argc == 5) || (argc == 3) || (argc == 6))
    {
        /* ATB=? */
        if (*argv[1] == 0x3F)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_DEVCFG,
                                         "ATCMD_UART_BAUDRATE",
                                         (int *) &baudrate);

            /* non-volatile area does not store UART configuration */
            if ((int) baudrate == -1)
            {
                /* 1st time to use ATB command */
                if ((int) g_uart_baud_atb == -1)
                {
                    /* Re-calculate baudrate from configuration values */
                    uint64_t div_fraction_part = ((uart_w_extended_cfg_t *) (p_uart->p_cfg->p_extend))->
                        p_baud_setting->fra_baud;

                    uint64_t div_integer_part  = ((uart_w_extended_cfg_t *) (p_uart->p_cfg->p_extend))->
                        p_baud_setting->int_baud;

                    uint64_t freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_UART);

                    baudrate = (uint32_t) (((4 * freq_hz) / ((div_fraction_part - 0.5) + 64 * div_integer_part) +
                                            50) / 100) * 100;
                    sprintf(result_str, "%d \r\n", (int) baudrate);
                }
                else
                {
                    sprintf(result_str, "%d\r\n", (int) g_uart_baud_atb);
                }
            }
            else
            {
                /* 1st time to use ATB command */
                if ((int) g_uart_baud_atb == -1)
                {
                    sprintf(result_str, "%d\r\n", (int) baudrate);
                }
                else
                {
                    sprintf(result_str, "%d\r\n", (int) g_uart_baud_atb);
                }
            }

 #else
            if ((int) g_uart_baud_atb == -1)
            {
                /* Re-calculate baudrate from configuration values */
                uint64_t div_fraction_part = ((uart_w_extended_cfg_t *) (p_uart->p_cfg->p_extend))->
                    p_baud_setting->fra_baud;

                uint64_t div_integer_part  = ((uart_w_extended_cfg_t *) (p_uart->p_cfg->p_extend))->
                    p_baud_setting->int_baud;

                uint64_t freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_UART);

                baudrate = (uint32_t) (((4 * freq_hz) / ((div_fraction_part - 0.5) + 64 * div_integer_part) +
                                        50) / 100) * 100;
                sprintf(result_str, "%d \r\n", (int) baudrate);
            }
            else
            {
                sprintf(result_str, "%d\r\n", (int) g_uart_baud_atb);
            }
 #endif
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
            bsp_safe_strcpy(ret_msg, "\r\nOK\r\n", sizeof(ret_msg));
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t const *) ret_msg, strlen(ret_msg));

            return FSP_ERR_AT_CMD_ERR_CMD_OK;
        }

        baudrate = atoi(argv[1]);
        switch (baudrate)
        {
            case (9600):
            case (14400):
            case (19200):
            case (38400):
            case (57600):
            case (115200):
            case (230400):
            case (460800):
            case (921600):
            {
                memcpy((void *) &g_uart_conf_atb, (void *) p_uart->p_cfg, sizeof(uart_cfg_t));
                memcpy((void *) &g_uart_conf_extend_atb,
                       (void *) p_uart->p_cfg->p_extend,
                       sizeof(uart_w_extended_cfg_t));
                g_uart_conf_atb.p_extend = &g_uart_conf_extend_atb;

                fsp_err = R_UART_W_BaudCalculate(baudrate, &g_uart_baud_setting_atb);
                g_uart_conf_extend_atb.p_baud_setting = &g_uart_baud_setting_atb;
                g_uart_baud_atb = baudrate;

                if (FSP_SUCCESS != fsp_err)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    break;
                }

                if ((argc == 5) || (argc == 6))
                {
                    /* Convert databits option into uart_data_bits_t or uart_w_data_bits_t type */
                    switch (atoi(argv[2]))
                    {
                        case (5):
                        {
                            g_uart_conf_atb.data_bits = UART_W_DATA_BITS_5;
                            break;
                        }

                        case (6):
                        {
                            g_uart_conf_atb.data_bits = UART_W_DATA_BITS_6;
                            break;
                        }

                        case (7):
                        {
                            g_uart_conf_atb.data_bits = UART_W_DATA_BITS_7;
                            break;
                        }

                        case (8):
                        {
                            g_uart_conf_atb.data_bits = UART_W_DATA_BITS_8;
                            break;
                        }

                        default:
                        {
                            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }
                    }

                    /* Convert parity option (n(none)/e(even)/o(odd)) into uart_parity_t type */
                    /* none */
                    if ((*argv[3] == 0x4E) || (*argv[3] == 0x6E))
                    {
                        g_uart_conf_atb.parity = UART_PARITY_OFF;
                    }
                    /* even */
                    else if ((*argv[3] == 0x45) || (*argv[3] == 0x65))
                    {
                        g_uart_conf_atb.parity = UART_PARITY_EVEN;
                    }
                    /* odd */
                    else if ((*argv[3] == 0x4F) || (*argv[3] == 0x6F))
                    {
                        g_uart_conf_atb.parity = UART_PARITY_ODD;
                    }
                    /* Invalid argument */
                    else
                    {
                        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    }

                    /* Convert stop bit option (1(1bit)/2(2bit)) into uart_stop_bits_t */
                    if (atoi(argv[4]) == 1)
                    {
                        g_uart_conf_atb.stop_bits = UART_STOP_BITS_1;
                    }
                    else if (atoi(argv[4]) == 2)
                    {
                        g_uart_conf_atb.stop_bits = UART_STOP_BITS_2;
                    }
                    else
                    {
                        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    }

                    /* Response "OK" if incoming parameters are acceptable */
                    bsp_safe_strcpy(ret_msg, "\r\nOK\r\n", sizeof(ret_msg));
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t const *) ret_msg, strlen(ret_msg));
                    vTaskDelay(portCONVERT_MS_2_TICKS((uint32_t) (strlen(ret_msg) * 2)));

                    R_UART_W_ConfSet(p_uart->p_ctrl, &g_uart_conf_atb);

                    if ((argc == 5) || ((argc == 6) && (atoi(argv[5]) == 1)))
                    {
 #ifdef RM_MAP_PERSISTANT_W

                        /* Store updated UART configuration into SFlash */
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_BAUDRATE",
                                                      baudrate);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_BITS",
                                                      g_uart_conf_atb.data_bits);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_PARITY",
                                                      g_uart_conf_atb.parity);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_STOPBIT",
                                                      g_uart_conf_atb.stop_bits);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_FLOWCTRL",
                                                      g_uart_conf_extend_atb.flow_control);
 #endif
                    }

                    break;
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W

                    /* Get UART configuration other than Baudrate */
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_DEVCFG,
                                                 "ATCMD_UART_BITS",
                                                 &databits);
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_DEVCFG,
                                                 "ATCMD_UART_PARITY",
                                                 &parity);
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_DEVCFG,
                                                 "ATCMD_UART_STOPBIT",
                                                 &stopbits);
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_DEVCFG,
                                                 "ATCMD_UART_FLOWCTRL",
                                                 &flow_control);

                    /* If stored value is valid */
                    if ((databits != -1) && (parity != -1) && (stopbits != -1) && (flow_control != -1))
                    {
                        g_uart_conf_atb.data_bits           = databits;
                        g_uart_conf_atb.parity              = parity;
                        g_uart_conf_atb.stop_bits           = stopbits;
                        g_uart_conf_extend_atb.flow_control = flow_control;
                    }
 #endif

                    /* Response "OK" if incoming parameters are acceptable */
                    bsp_safe_strcpy(ret_msg, "\r\nOK\r\n", sizeof(ret_msg));
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t const *) ret_msg, strlen(ret_msg));
                    vTaskDelay(portCONVERT_MS_2_TICKS((uint32_t) (strlen(ret_msg) * 2)));

                    R_UART_W_ConfSet(p_uart->p_ctrl, &g_uart_conf_atb);

                    if ((argc == 2) || ((argc == 3) && (atoi(argv[2]) == 1)))
                    {
 #ifdef RM_MAP_PERSISTANT_W

                        /* Store updated UART configuration into SFlash */
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_BAUDRATE",
                                                      baudrate);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_BITS",
                                                      g_uart_conf_atb.data_bits);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_PARITY",
                                                      g_uart_conf_atb.parity);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_STOPBIT",
                                                      g_uart_conf_atb.stop_bits);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_DEVCFG,
                                                      "ATCMD_UART_FLOWCTRL",
                                                      g_uart_conf_extend_atb.flow_control);
 #endif
                    }
                }

                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                break;
            }
        }
    }
    else
    {
        // ERROR case
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    return err;
#else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
#endif
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATB)
{
    const char * p_usage = "<baudrate>[[,<databits>][,<parity>][,<stopbits>][,<flow_ctrl>][,<persistence>]]";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATB)
{
    const char * p_description = "Setting UART parameters";

    return p_description;
}

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(RESTART)
{
    extern bool reset(void);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char     result_str[16]    = {0x00, };
    uint32_t timeout           = 1000;

    bsp_safe_strcpy(result_str, "\r\nOK\r\n", sizeof(result_str));
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

 #if 0                                 //! defined(RM_ATCMD_W_CORE_BASIC_NOT_IMPLEMETED)
  #if defined(__SUPPORT_ATCMD_TLS__)
    atcmd_transport_ssl_reboot();
  #endif                               // (__SUPPORT_ATCMD_TLS__)
 #endif

    vTaskDelay(portCONVERT_MS_2_TICKS(timeout));
    reset();

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(RESTART)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(RESTART)
{
    const char * p_description = "System Restart";

    return p_description;
}
#endif                                 /* CFG_WIFI */

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(CHIPNAME)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[32]        = {0, };

    sprintf(result_str, "\r\n%s:%s\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), CHIPSET_NAME);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(CHIPNAME)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(CHIPNAME)
{
    const char * p_description = "Chipset Name (RA6W1/RA6W2)";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(VER)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[96]        = {0};
    char fw_ver_str[64]        = {0};

    sprintf(fw_ver_str, "%s", FSP_VERSION_STRING);

#if 0
#if defined(__SUPPORT_OTA__)
 #if (SUPPORT_FSP_RM_OTA_W == 1)
    uint32_t addr = 0x00;
    rm_ota_w_image_header_data_t infoImage = {0, };

    if (p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_CURRENT_ADDR, RM_OTA_W_TYPE_RTOS, (uint32_t *) &addr) == 0)
    {
        if (p_ota_instance->p_api->getImageInfo(p_ota_instance->p_ctrl, RM_OTA_W_TYPE_RTOS, addr, &infoImage) == 0)
        {
            if ((strcmp("RRQ61000-1.0.0", (char *) infoImage.name)))
            {
                memset(fw_ver_str, 0, sizeof(fw_ver_str));
                sprintf(fw_ver_str, "%s", infoImage.name);
            }
        }
    }
 #endif                                /* SUPPORT_FSP_RM_OTA_W */
#endif
#endif  /* 0 */
    sprintf(result_str, AT_CMD_ENTER_NEW_LINE "%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), fw_ver_str);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(VER)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(VER)
{
    const char * p_description = "FW Version info";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(SDKVER)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[128]       = {0, };

    sprintf(result_str,
            "\r\n%s:%d.%d.%d.%d.%d",
            rm_atcmd_w_core_common_strupr(argv[0] + 2),
            SDK_VER_PRODUCT_LINE,
            SDK_VER_MODE,
            SDK_VER_TARGET,
            SDK_VER_BRANCH,
            SDK_VER_R);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(SDKVER)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(SDKVER)
{
    const char * p_description = "SDK Version info";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(TIME)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  ret;
    char time_str[64]   = {0x00, };
    char result_str[92] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+TIME=? */
        struct tm   ts;
        struct tm * p_ts = &ts;

        R_RTC_W_CalendarTimeGet(R_RTC_W_GetCtrl(), p_ts);

        R_RTC_W_Time2Str(R_RTC_W_GetCtrl(), p_ts, time_str, sizeof(time_str), "%Y-%m-%d,%H:%M:%S");

        sprintf(result_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), time_str);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }
    else if (argc == 3)
    {
        /* AT+TIME=<date>,<time> */
        ret = rm_atcmd_w_set_time(argv[1], argv[2], 0);

        if (ret != 0)
        {
            switch (ret)
            {
                case -1:
                {
                    err = FSP_ERR_AT_CMD_ERR_BASIC_ARG_NULL_PTR;
                    break;
                }

                case -2:
                {
                    err = FSP_ERR_AT_CMD_ERR_BASIC_ARG_DATE;
                    break;
                }

                case -3:
                {
                    err = FSP_ERR_AT_CMD_ERR_BASIC_ARG_TIME;
                    break;
                }

                default:
                {
                    err = FSP_ERR_AT_CMD_ERR_BASIC_ARG_TIME_ETC;
                    break;
                }
            }
        }
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(TIME)
{
    const char * p_usage = "<date>,<time>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(TIME)
{
    const char * p_description = "Set current time";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(RLT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[128] = {0, };

    __time64_t uptime = __uptime();

    sprintf(result_str,
            "\r\n%s:%lu,%02lu:%02lu.%02lu\r\n",
            rm_atcmd_w_core_common_strupr(argv[0] + 2),
            (unsigned long) uptime / (24 * 3600),
            (unsigned long) uptime % (24 * 3600) / 3600,
            (unsigned long) uptime % (3600) / 60,
            (unsigned long) uptime % 60);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(RLT)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(RLT)
{
    const char * p_description = "System Runtime Inquiry";

    return p_description;
}

#if CFG_WIFI
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(TZONE)
{
    extern long get_time_zone(void);
    extern void ra6w1_SetTzoff(long offset);

    fsp_err_atcmd_err_code err           = FSP_ERR_AT_CMD_ERR_CMD_OK;
    const int              max_time_zone = 43200;
    const int              min_time_zone = -43200;
    char resp_str[128] = {0x00, };
    int  time_zone     = 0;
    long remainder     = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+TZONE=? */
        time_zone = get_time_zone();

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), time_zone);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+TZONE=<sec> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &time_zone, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        }
        else
        {
            if ((time_zone < min_time_zone) || (time_zone > max_time_zone))
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                goto end;
            }

            remainder = (time_zone / 60) * 60;

            if (time_zone != 0)
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_SYSCFG,
                                              NVR_KEY_TIMEZONE,
                                              time_zone);
 #endif
                R_RTC_W_CalendarTimeZoneSet(R_RTC_W_GetCtrl(), &remainder);
            }
            else
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_TIMEZONE);
 #endif
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(TZONE)
{
    const char * p_usage = "<sec>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(TZONE)
{
    const char * p_description = "Set Timezone (GMT; -43200 ~ 43200)";

    return p_description;
}
#endif                                 /* CFG_WIFI */

#if CFG_WIFI
static void config_ap_atcmd (int device_mode)
{
    char          ssid_str[wificonfigMAX_SSID_LEN + 1] = {0, };
    const char  * psk_str              = "12345678";
    char          dhcp_lease_start[16] = {0, };
    char          dhcp_lease_end[16]   = {0, };
    unsigned long temp_ip;
    char        * country_code = NULL;
    ip_addr_t     tmp_addr;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_COUNTRY_CODE,
                                    &country_code);

    /* Clear NVRAM */
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG);
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG);
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG);
    RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE);

    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_SYS_MODE,
                                  device_mode);

    gen_ssid(CHIPSET_NAME, WLAN1_IFACE, 0, ssid_str, sizeof(ssid_str));
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_SSID_1,
                                     ssid_str);

  #if defined(__USE_WPA2_WPA3_SOFTAP_AS_DEFAULT__)

    /* save security to nvram */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_SECURITY_1,
                                  eWiFiSecurityWPA2_WPA3_ext);
  #else

    /* save security to nvram */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_SECURITY_1,
                                  eWiFiSecurityWPA2_ext);
  #endif                               /* __USE_WPA2_WPA3_SOFTAP_AS_DEFAULT__ */

    /* save pmf to nvram */
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_1,
                                  PMF_DEFAULT);

    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_ENCKEY_1,
                                     psk_str);

  #if defined(__SUPPORT_SETBAND_5GHZ__)
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_BAND,
                                  WPA_SETBAND_2G);
  #endif                               /* __SUPPORT_SETBAND_5GHZ__ */

    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);

    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_COUNTRY_CODE,
                                     country_code && strlen(country_code) ?
                                     country_code : DEFAULT_AP_COUNTRY);

    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL, 1);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_IPADDR_1,
                                     DEFAULT_IPADDR_WLAN1);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_NETMASK_1,
                                     DEFAULT_SUBNET_WLAN1);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_1, 2);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_GATEWAY_1,
                                     DEFAULT_GATEWAY_WLAN1);

  #if defined(__SUPPORT_IPV4__)
    ipaddr_aton(DEFAULT_IPADDR_WLAN1, &tmp_addr);
    temp_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    longtoip((temp_ip + 1), dhcp_lease_start);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_SYSCFG,
                                     DHCP_SERVER_START_IP,
                                     dhcp_lease_start);
    longtoip((temp_ip + 10), dhcp_lease_end);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_SYSCFG,
                                     DHCP_SERVER_END_IP,
                                     dhcp_lease_end);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_LEASE_TIME, 1800);
   #if (LWIP_DHCPS && LWIP_IPV4)
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, pdTRUE);
   #endif                              /* LWIP_DHCPS && LWIP_IPV4 */
  #endif                               /* __SUPPORT_IPV4__ */
 #endif                                /* RM_MAP_PERSISTANT_W */
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(DEFAP)
{
    extern bool reset(void);

    const fsp_err_atcmd_err_code err        = FSP_ERR_AT_CMD_ERR_CMD_OK;
    const char                 * result_str = "\r\nOK\r\n";
    const uint32_t               timeout    = 1000;

    config_ap_atcmd(WIFI_DEVICE_MODE_EXT_AP);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    vTaskDelay(portCONVERT_MS_2_TICKS(timeout));
    reset();

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(DEFAP)
{
    const char * p_usage = "<boot>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(DEFAP)
{
    const char * p_description = "Factory setting(AP mode) --- "
                                 "SSID:<CHIPNAME>_XXXXXX, Auth:WPA2/CCMP, IP:10.0.0.1, Enable DHCP";

    return p_description;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(DEFCCRNT)
{
    extern bool reset(void);

    const fsp_err_atcmd_err_code err        = FSP_ERR_AT_CMD_ERR_CMD_OK;
    const char                 * result_str = "\r\nOK\r\n";
    const uint32_t               timeout    = 1000;

    config_ap_atcmd(WIFI_DEVICE_MODE_EXT_AP_STATION);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    vTaskDelay(portCONVERT_MS_2_TICKS(timeout));
    reset();

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(DEFCCRNT)
{
    const char * p_usage = "<boot>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(DEFCCRNT)
{
    const char * p_description = "Factory setting(Concurrent mode) --- "
                                 "SSID:<CHIPNAME>_XXXXXX, Auth:WPA2/CCMP, IP:10.0.0.1, Enable DHCP";

    return p_description;
}
#endif                                 /* CFG_WIFI */

#if (ATCMD_BLE_BRG == 1)
RM_ATCMD_W_CORE_BASIC_ATCMD_CB(BLEBRG)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    char result_str[50] = {0x00, };

    if (argc > 2)
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    if (rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        sprintf(result_str,
                "\r\n%s:(%s|%s|%s|%s)\r\n",
                rm_atcmd_w_core_common_strupr(argv[0] + 2),
                RM_ATCMD_W_CORE_BASICE_ATBLE_ALL,
                RM_ATCMD_W_CORE_BASICE_ATBLE_RESET,
                RM_ATCMD_W_CORE_BASICE_ATBLE_BRG,
                RM_ATCMD_W_CORE_BASICE_ATBLE_PINS);
    }
    else if (argc == 2)
    {
        if (0 == strcmp(argv[1], RM_ATCMD_W_CORE_BASICE_ATBLE_ALL))
        {
            p_ctrl->run_mode = AT_MODE_BLEBRG_ALL;
            sprintf(result_str,
                    "\r\n%s:%s\r\n",
                    rm_atcmd_w_core_common_strupr(argv[0] + 2),
                    RM_ATCMD_W_CORE_BASICE_ATBLE_ALL);
        }
        else if (0 == strcmp(argv[1], RM_ATCMD_W_CORE_BASICE_ATBLE_RESET))
        {
            p_ctrl->run_mode = AT_MODE_BLEBRG_RESET;
            sprintf(result_str,
                    "\r\n%s:%s\r\n",
                    rm_atcmd_w_core_common_strupr(argv[0] + 2),
                    RM_ATCMD_W_CORE_BASICE_ATBLE_RESET);
        }
        else if (0 == strcmp(argv[1], RM_ATCMD_W_CORE_BASICE_ATBLE_BRG))
        {
            p_ctrl->run_mode = AT_MODE_BLEBRG_BRG;
            sprintf(result_str,
                    "\r\n%s:%s\r\n",
                    rm_atcmd_w_core_common_strupr(argv[0] + 2),
                    RM_ATCMD_W_CORE_BASICE_ATBLE_BRG);
        }
        else if (0 == strcmp(argv[1], RM_ATCMD_W_CORE_BASICE_ATBLE_PINS))
        {
            p_ctrl->run_mode = AT_MODE_BLEBRG_PINS;
            sprintf(result_str,
                    "\r\n%s:%s\r\n",
                    rm_atcmd_w_core_common_strupr(argv[0] + 2),
                    RM_ATCMD_W_CORE_BASICE_ATBLE_PINS);
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
    }
    else
    {
        p_ctrl->run_mode = AT_MODE_BLEBRG_ALL;
        sprintf(result_str,
                "\r\n%s:%s\r\n",
                rm_atcmd_w_core_common_strupr(argv[0] + 2),
                RM_ATCMD_W_CORE_BASICE_ATBLE_ALL);
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(BLEBRG)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(BLEBRG)
{
    const char * p_description = "start BLE Bridge";

    return p_description;
}
#endif

#if (ATCMD_SECURE_CHANNEL == 1) & CFG_WIFI
 #include "common.h"
 #include "mbedtls/aes.h"
 #include "r_cc312_common.h"

RM_ATCMD_W_CORE_BASIC_ATCMD_CB(ATS)
{
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char    resp_str[IV_HEX_LEN_AT + 1]   = {0x00, };
    uint8_t iv[AES_IV_SIZE_AT]            = {0x00, };
    int     ret;
    uint8_t key[32];
    int     is_query = 0;

    if ((argc > 1) && (argv[1] != NULL))
    {
        is_query = rm_atcmd_w_core_common_is_query_arg(argc, argv[1]);
    }

    if ((argc == 1) || is_query)
    {
        /* ATS=? */
        sprintf(resp_str, "\r\nSECUREC CHANNEL:%d", p_ctrl->secure_channel);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        size_t len = strlen(argv[1]);

        if (len == strlen("off"))
        {
            memcpy(resp_str, rm_atcmd_w_core_common_strupr(argv[1]), len);
            resp_str[len] = '\0';

            if (strcmp(resp_str, "OFF") == 0) // ATS=OFF
            {
                if (p_ctrl->secure_channel)
                {
                    /* Clear IV */
                    memcpy(p_ctrl->rx_iv, iv, AES_IV_SIZE_AT);
                    memcpy(p_ctrl->tx_iv, iv, AES_IV_SIZE_AT);

                    /* Reset secure channel */
                    mbedtls_aes_free(p_ctrl->ctx_rx);
                    mbedtls_aes_free(p_ctrl->ctx_tx);
                    p_ctrl->secure_channel = 0;
                }

                sprintf(resp_str, "\r\nSECUREC CHANNEL:%d", p_ctrl->secure_channel);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                goto ATS_END;
            }
        }

        if ((strlen(argv[1]) != IV_HEX_LEN_AT) || !is_valid_iv_hex_arg(argv[1]))
        {
            sprintf(resp_str, "\r\nIV should be 16byte.");
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto ATS_END;
        }

        /* ATS = <32 HEX valid IV> */
        if (is_all_zero(p_ctrl->key, ATCMD_W_SECURE_KEY_MAX))
        {
            sprintf(resp_str, "\r\nThere is no key asset.");
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto ATS_END;
        }

        memcpy(key, p_ctrl->key, ATCMD_W_SECURE_KEY_MAX);

        for (int i = 0; i < AES_IV_SIZE_AT; i++)
        {
            iv[i] = (uint8_t) ((rm_atcmd_w_core_common_htoi_char(argv[1][i * 2]) << 4) |
                               rm_atcmd_w_core_common_htoi_char(argv[1][i * 2 + 1]));
        }

        memcpy(p_ctrl->rx_iv, iv, AES_IV_SIZE_AT);
        memcpy(p_ctrl->tx_iv, iv, AES_IV_SIZE_AT);

        /* Create AES handler */
        mbedtls_aes_init(p_ctrl->ctx_rx);
        ret = mbedtls_aes_setkey_dec(p_ctrl->ctx_rx, key, 128); /* RX decode */
        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            mbedtls_aes_free(p_ctrl->ctx_rx);
            goto ATS_END;
        }

        mbedtls_aes_init(p_ctrl->ctx_tx);
        ret = mbedtls_aes_setkey_enc(p_ctrl->ctx_tx, key, 128); /* TX encode */
        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            mbedtls_aes_free(p_ctrl->ctx_rx);
            mbedtls_aes_free(p_ctrl->ctx_tx);
            goto ATS_END;
        }

        p_ctrl->secure_channel = 1;

        /* Disable echo in secure channel */
        p_ctrl->uart_echo_on = 0;
        p_ctrl->echo_on      = 0;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

ATS_END:

    return err;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_FORMAT_CB(ATS)
{
    const char * p_usage = "<IV (32byte of ASCII) or off>";

    return p_usage;
}

RM_ATCMD_W_CORE_BASIC_ATCMD_BRIEF_CB(ATS)
{
    const char * p_description = "Secure Channel On/Off and Reset";

    return p_description;
}
#endif
