/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR
 #if CFG_WIFI

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
  #include "rm_atcmd_w_core_websocket_parse.h"
  #include "rm_atcmd_w_core_err_code.h"
  #include "rm_atcmd_w_core.h"
  #include "rm_atcmd_w_core_dpm_parse.h"

  #include "FreeRTOS.h"
  #include "rm_wifi.h"

  #include "rm_pmgr_w_instance.h"
  #include "rm_wifi_helper.h"
  #include "rm_vee_flash_w_rrq_nvram.h"

  #ifdef RM_MAP_PERSISTANT_W
   #include "rm_map_persistant_w.h"
   #include dg_configADNVPARAM_PROJ_FILE
  #endif

/* Workaround for ADC wakeup settings until AT+ADC is available */
  #include "r_adc_w.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

  #define RM_ATCMD_W_CORE_DPM_ATCMD_CODE(atcmd)    "AT+" # atcmd

  #define RM_ATCMD_W_CORE_DPM_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_DPM_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
  #define RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_DPM_ ## atcmd ## _format_cb(void)
  #define RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_DPM_ ## atcmd ## _brief_cb(void)

  #define RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_DPM_ ## atcmd ## _cmd_cb
  #define RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_DPM_ ## atcmd ## _format_cb
  #define RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_DPM_ ## atcmd ## _brief_cb

  #define RM_ATCMD_W_CORE_DPM_DEBUG(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
  #define RM_ATCMD_W_CORE_DPM_ERROR(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
   #define RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME               "at_ext_wu_mon"
   #define RM_ATCMD_W_CORE_DPM_EXT_WU_MON_SIZE               (256)
   #define RM_ATCMD_W_CORE_DPM_EXT_WU_DELAY_TO_RCV_AT_CMD    (20) // 200 msec
  #endif

  #define EVT_MCUWUDONE_RCV_DONE                             (1UL << (0))
  #define EVT_SLEEP_BLOCK_RCV_DONE                           (1UL << (1))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
static int          rm_atcmd_w_core_dpm_ext_wu_sleep_flag  = pdTRUE;
static TaskHandle_t rm_atcmd_w_core_dpm_ext_wu_mon_handler = NULL;
  #endif

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMKA);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMKA);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMKA);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRIPCOND);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRIPCOND);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRIPCOND);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMTIMWU);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMTIMWU);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMTIMWU);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMUSERWU);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMUSERWU);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMUSERWU);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRCONSTRAINT);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRCONSTRAINT);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRCONSTRAINT);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRMCUWUDONE);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRMCUWUDONE);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRMCUWUDONE);

  #if !defined(__DISABLE_DPM_ABNORM__)
RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMABNOPS);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMABNOPS);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMABNOPS);
  #endif                               /* !(__DISABLE_DPM_ABNORM__) */

  #if ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT
RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMABNWFCCNT);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMABNWFCCNT);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMABNWFCCNT);
  #endif                               /* ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT */

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRWAKESRC);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRWAKESRC);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRWAKESRC);

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRFORCE);
RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRFORCE);
RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRFORCE);

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
static char * RM_ATCMD_W_CORE_DPM_get_dpm_wakeup_status(void);
static void   RM_ATCMD_W_CORE_DPM_ext_wu_mon(void * pvParameters);
static void   RM_ATCMD_W_CORE_DPM_create_ext_wu_mon(void);

  #endif                               // (__SUPPORT_DPM_EXT_WU_MON__)

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_dpm_module[] =
{
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRDPMKA),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRDPMKA),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRDPMKA),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRDPMKA)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRIPCOND),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRIPCOND),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRIPCOND),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRIPCOND)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRDPMTIMWU),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRDPMTIMWU),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRDPMTIMWU),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRDPMTIMWU)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRDPMUSERWU),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRDPMUSERWU),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRDPMUSERWU),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRDPMUSERWU)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRCONSTRAINT),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRCONSTRAINT),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRCONSTRAINT),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRCONSTRAINT)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRMCUWUDONE),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRMCUWUDONE),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRMCUWUDONE),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRMCUWUDONE)
    },
  #if !defined(__DISABLE_DPM_ABNORM__)
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRDPMABNOPS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRDPMABNOPS),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRDPMABNOPS),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRDPMABNOPS)
    },
  #endif                               /* !(__DISABLE_DPM_ABNORM__) */
  #if ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRDPMABNWFCCNT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRDPMABNWFCCNT),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRDPMABNWFCCNT),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRDPMABNWFCCNT)
    },
  #endif                               /* ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT */
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRWAKESRC),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRWAKESRC),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRWAKESRC),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRWAKESRC)
    },
    {
        RM_ATCMD_W_CORE_DPM_ATCMD_CODE(PMGRFORCE),
        ATCMD_W_TYPE_A,
        9,
        0,
        RM_ATCMD_W_CORE_DPM_ATCMD_CB_P(PMGRFORCE),
        RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB_P(PMGRFORCE),
        RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB_P(PMGRFORCE)
    },
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

pmgr_instance_info_t g_rm_atcmd_w_core_dpm_cb_info_arg =
{
    .power_mode  = PMGR_LLD_POWER_MODE_AUTO,
    .wake_source = PMGR_WAKE_SOURCE_NONE,
};

pmgr_callback_args_t g_rm_atcmd_w_core_dpm_cb_args =
{
    .constraints     = PMGR_CONSTRAINT_NONE,
    .event           = PMGR_EVENT_NOT_SET,
    .p_instance_info = &g_rm_atcmd_w_core_dpm_cb_info_arg,
    .p_context       = NULL,
};

pmgr_w_notifier_extend_t g_rm_atcmd_w_core_dpm_cb_noti_ext =
{
    .order = PMGR_W_NOTIFIER_ORDER_SYS_HIGH,
};

static void RM_ATCMD_W_CORE_DPM_CB (pmgr_callback_args_t * args)
{
    pmgr_instance_info_t * pmgr_info  = (pmgr_instance_info_t *) args->p_instance_info;
    pmgr_lld_power_mode_t  power_mode = pmgr_info->power_mode;
    pmgr_event_t           event      = args->event;
    atcmd_w_ctrl_t       * p_at_ctrl  = (atcmd_w_ctrl_t *) args->p_context;
    char p_resp[12] = {0};

    if ((power_mode != PMGR_LLD_POWER_MODE_DPM) || (event != PMGR_EVENT_ENTERING_SLEEP))
    {
        return;
    }

    sprintf(p_resp, "\r\n+PMGR:%u\r\n", (uint8_t) event);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_resp, strlen(p_resp));
}

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_DPM_register (atcmd_w_core_module_list_t * p_list)
{
  #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
  #endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_dpm_module);
}

uint32_t RM_ATCMD_W_CORE_DPM_deregister (atcmd_w_core_module_list_t * p_list)
{
  #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
  #endif

    rm_atcmd_w_core_deregister(p_list, at_core_dpm_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_DPM_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
    RM_ATCMD_W_CORE_DPM_create_ext_wu_mon();
  #endif

    g_rm_atcmd_w_core_dpm_cb_args.p_context = p_at_ctrl;

    RM_PMGR_W_notifier_register(RM_PMGR_W_get_ctrl(),
                                RM_ATCMD_W_CORE_DPM_CB,
                                &g_rm_atcmd_w_core_dpm_cb_args,
                                &g_rm_atcmd_w_core_dpm_cb_noti_ext);

    return err;
}

uint32_t RM_ATCMD_W_CORE_DPM_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
    if (rm_atcmd_w_core_dpm_ext_wu_mon_handler)
    {
        vTaskDelete(rm_atcmd_w_core_dpm_ext_wu_mon_handler);
        rm_atcmd_w_core_dpm_ext_wu_mon_handler = NULL;
    }
  #endif

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMKA)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  ret          = 0;
    int  period       = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME,
                                     &period);
  #endif
        if (period == -1)
        {
            period = DFLT_DPM_KEEPALIVE_TIME;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), period);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRDPMKA=<period> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &period, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((period < MIN_DPM_KEEPALIVE_TIME) || (period > MAX_DPM_KEEPALIVE_TIME))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME,
                                      period);
  #endif
        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMKA)
{
    const char * p_usage = "<period>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMKA)
{
    const char * p_description = "Set DPM Kepp-Alive Period (msec : 0 .. 600000)";

    return p_description;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRIPCOND)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  ret          = 0;
    int  ip_condition = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFICFG,
                                     NVR_KEY_DPM_IP_CONDITION,
                                     &ip_condition);
  #endif
        if (ip_condition == -1)
        {
            ip_condition = PMGR_CONDITION_IPV4_MANDATORY;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ip_condition);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRIPCOND=<ip condition> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &ip_condition, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((ip_condition < PMGR_CONDITION_IPV4_MANDATORY) || (ip_condition > PMGR_CONDITION_IPV46_MANDATORY))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFICFG,
                                      NVR_KEY_DPM_IP_CONDITION,
                                      ip_condition);
  #endif
        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRIPCOND)
{
    const char * p_usage = "<ip_condition>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRIPCOND)
{
    const char * p_description = "Set IP Condition to enter PMGR_LLD_DPM (1: IPv4, 2:IPv6, 3:Both)";

    return p_description;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMTIMWU)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  ret          = 0;
    int  count        = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT,
                                     &count);
  #endif

        if (count == -1)
        {
            count = DFLT_DPM_TIM_WAKEUP_COUNT;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), count);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRDPMTIMWU=<count> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &count, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((count < MIN_DPM_TIM_WAKEUP_COUNT) || (count > MAX_DPM_TIM_WAKEUP_COUNT))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        ret = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT,
                                            count);
  #endif

        if (ret != FSP_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMTIMWU)
{
    const char * p_usage = "<count>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMTIMWU)
{
    const char * p_description = "Set DPM TIM Wakeup Time (DTIM count : 1 .. 6000)";

    return p_description;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMUSERWU)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  ret          = 0;
    int  time         = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_USER_WAKEUP_TIME,
                                     &time);
  #endif
        if (time == -1)
        {
            time = DFLT_DPM_USER_WAKEUP_TIME;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), time);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRDPMTIMWU=<time> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &time, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if (((time < MIN_DPM_USER_WAKEUP_TIME) || (time > MAX_DPM_USER_WAKEUP_TIME)) && (time != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if (!RM_PMGR_W_dpm_is_enabled())
        {
            err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_MODE_DISABLED;
            goto end;
        }

        ret = RM_PMGR_W_dpm_user_wakeup_timer_set(time);

        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_UNKNOWN;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMUSERWU)
{
    const char * p_usage = "<time>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMUSERWU)
{
    const char * p_description = "Set DPM User Wakeup Time (msec : 0 .. 86400000)";

    return p_description;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRCONSTRAINT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
    {
        rm_atcmd_w_core_dpm_ext_wu_sleep_flag = pdFALSE;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_MODE_DISABLED;
    }

  #else
    pmgr_ctrl_t * pmgr_ctrl = NULL;
    uint8_t       ram_cnt, ret_cnt, mac_hw_cnt, prohibited_cnt;

    char resp_str[32]            = {0x00, };
    char resp_str_constr_cnt[16] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+PMGRCONSTRAINT=? */

        pmgr_ctrl = RM_PMGR_W_get_ctrl();
        if (pmgr_ctrl == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl, PMGR_CONSTRAINT_POWER_RAM, &ram_cnt);
        RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl, PMGR_CONSTRAINT_POWER_RETENTION, &ret_cnt);
        RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl, PMGR_CONSTRAINT_POWER_MAC_HW, &mac_hw_cnt);
        RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl, PMGR_CONSTRAINT_SLEEP_PROHIBITED, &prohibited_cnt);

        sprintf(resp_str_constr_cnt, "%d,%d,%d,%d", ram_cnt, ret_cnt, mac_hw_cnt, prohibited_cnt);

        sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), resp_str_constr_cnt);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 3)
    {
        /* AT+PMGRCONSTRAINT=<action>,<constraint> */
        int                    action, constraint;
        pmgr_constraints_t     constraint_enum = PMGR_CONSTRAINT_NONE;
        pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *) pmgr_ctrl;
        pmgr_ctrl = RM_PMGR_W_get_ctrl();
        if (pmgr_ctrl == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        if ((rm_atcmd_w_core_common_stoi(argv[1], &action, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[2], &constraint, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if ((rm_atcmd_w_core_common_is_in_valid_range(action, 1, 2) == pdFALSE) ||
            (rm_atcmd_w_core_common_is_in_valid_range(constraint, 1, 4) == pdFALSE))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        switch (constraint)
        {
            case 1:
            {
                constraint_enum = PMGR_CONSTRAINT_POWER_RAM;
                break;
            }

            case 2:
            {
                constraint_enum = PMGR_CONSTRAINT_POWER_RETENTION;
                break;
            }

            case 3:
            {
                constraint_enum = PMGR_CONSTRAINT_POWER_MAC_HW;
                break;
            }

            case 4:
            {
                constraint_enum = PMGR_CONSTRAINT_SLEEP_PROHIBITED;
                break;
            }
        }

        if (action == 1 /* ADD */)
        {
   #if ATCMD_IF_SUPPORT
            TaskHandle_t p_init_msg_sender = ((atcmd_w_core_instance_ctrl_t *) p_at_ctrl)->at_init_msg_sender_thread;
   #endif                              /* ATCMD_IF_SUPPORT */
            /* 1 (RAM), 2 (RET), 3 (MAC_HW), 4 (PROHIBITED) */
            if ((constraint == 1) && p_instance_ctrl && (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED))
            {
                p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_DPM_AT]++;
            }

            RM_PMGR_W_add_sleep_constraint(pmgr_ctrl, constraint_enum);

   #if ATCMD_IF_SUPPORT
            if (p_init_msg_sender)
            {
                if (((constraint == 1) || (constraint == 4)) && RM_PMGR_W_dpm_is_wakeup())
                {
                    xTaskNotify(p_init_msg_sender, EVT_SLEEP_BLOCK_RCV_DONE, eSetBits);
                }
            }
   #endif                              /* ATCMD_IF_SUPPORT */
        }
        else /* REMOVE */
        {
            RM_PMGR_W_remove_sleep_constraint(pmgr_ctrl, constraint_enum);
            if ((constraint == 1) && p_instance_ctrl && (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED))
            {
                p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_DPM_AT]--;
            }
        }
    }
    else
    {
        if (argc == 2)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }
end:
  #endif

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRCONSTRAINT)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRCONSTRAINT)
{
    const char * p_description = "Add or Remove PMGR constraint";

    return p_description;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRMCUWUDONE)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    p_ctrl->mcu_wu_done = pdTRUE;

  #if ATCMD_IF_SUPPORT
    if (p_ctrl->at_init_msg_sender_thread)
    {
        if (RM_PMGR_W_dpm_is_wakeup())
        {
            xTaskNotify(p_ctrl->at_init_msg_sender_thread, EVT_MCUWUDONE_RCV_DONE, eSetBits);
        }
    }
  #endif                               /* ATCMD_IF_SUPPORT */

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRMCUWUDONE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRMCUWUDONE)
{
    const char * p_description = "Notify that MCU is ready.";

    return p_description;
}

  #if !defined(__DISABLE_DPM_ABNORM__)
RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMABNOPS)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  dpm_abnormal_stop_flag = 0;
    char resp_str[32]           = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
   #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFICFG,
                                     NVR_KEY_DPM_ABNORM_STOP,
                                     &dpm_abnormal_stop_flag);
   #endif

        if (dpm_abnormal_stop_flag == -1)
        {
            /* NVR_KEY_DPM_ABNORM_STOP not present in NVRAM */
            dpm_abnormal_stop_flag = CC_VAL_ENABLE;
        }
        else
        {
            (dpm_abnormal_stop_flag) ? (dpm_abnormal_stop_flag = CC_VAL_DISABLE) : (dpm_abnormal_stop_flag =
                                                                                        CC_VAL_ENABLE);
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), dpm_abnormal_stop_flag);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRDPMABNOPS=<flag> */
        if ((rm_atcmd_w_core_common_stoi(argv[1], &dpm_abnormal_stop_flag, POL_1) != 0) ||
            (rm_atcmd_w_core_common_is_in_valid_range(dpm_abnormal_stop_flag, 0, 1) == pdFALSE))
        {
            err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_ABN_ARG;
            goto end;
        }

        if (get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
            goto end;
        }

        if ((dpm_abnormal_stop_flag != CC_VAL_ENABLE) && (dpm_abnormal_stop_flag != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_ABN_ARG;
            goto end;
        }

        if (dpm_abnormal_stop_flag == CC_VAL_ENABLE)
        {
            if (RM_PMGR_W_dpm_abnormal_checker_run(pdTRUE) == pdFAIL)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_ABN_ARG;
                goto end;
            }
            else
            {
                /* Delete flag from NVRAM */
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_DPM_ABNORM_STOP);
   #endif
            }
        }
        else
        {
            if (RM_PMGR_W_dpm_abnormal_checker_run(pdFALSE) == pdFAIL)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_ABN_ARG;
                goto end;
            }
            else
            {
                /* Save flag into NVRAM */
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFICFG,
                                              NVR_KEY_DPM_ABNORM_STOP,
                                              1);
   #endif
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMABNOPS)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMABNOPS)
{
    const char * p_description = "DPM Abnormal operation On/Off (0:Off, 1:On)";

    return p_description;
}
  #endif                               /* !(__DISABLE_DPM_ABNORM__) */

  #if ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT
RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRDPMABNWFCCNT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    extern int set_dpm_abnormal_wait_time(int count, int mode);

    int  count        = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
   #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                         NVR_KEY_DPM_AB_WF_CONN_RETRY, &count) != FSP_SUCCESS)
   #endif
        {
            count = 0;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), count);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+PMGRDPMABNWFCCNT=<count> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &count, POL_1) == 0)
        {
            if (rm_atcmd_w_core_common_is_in_valid_range(count, 0, 6) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            }
            else
            {
                set_dpm_abnormal_wait_time(count, DPM_ABNORM_DPM_WIFI_RETRY_CNT);
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        }
    }
    else if (argc < 2)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRDPMABNWFCCNT)
{
    const char * p_usage = "<count>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRDPMABNWFCCNT)
{
    const char * p_description = "Set Wi-Fi Connection retry count when the system is in DPM abnormal state)";

    return p_description;
}
  #endif                               /* ATCMD_W_PMGR_DPM_ABN_WF_CONN_RETRY_CNT */

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRWAKESRC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    pmgr_ctrl_t      * pmgr_ctrl          = NULL;
    pmgr_wake_source_t wake_source_bitmap = 0;
    int                i = 0;

    /* 1 (RTC), 2 (GPT), 3 (GPIO), 4 (ADC), 5 (WIFI), 6 (BLE) */
    int  wake_src_table[RM_ATCMD_W_CORE_DPM_WAKE_SRC_NUM] = {0, };
    char resp_str[128]         = {0x00, };
    char resp_str_wake_src[12] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+PMGRWAKESRC=? */
        pmgr_ctrl = RM_PMGR_W_get_ctrl();
        if (pmgr_ctrl == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        RM_PMGR_W_get_wake_source(pmgr_ctrl, &wake_source_bitmap);

        for (i = 0; i < RM_ATCMD_W_CORE_DPM_WAKE_SRC_NUM + 1; i++)
        {
            if (wake_source_bitmap & (1UL << (i)))
            {
                wake_src_table[i] = 1;
            }
        }

        for (i = 0; i < RM_ATCMD_W_CORE_DPM_WAKE_SRC_NUM; i++)
        {
            resp_str_wake_src[i * 2] = wake_src_table[i] + '0'; // Convert int to char
            if (i < 5)
            {
                resp_str_wake_src[i * 2 + 1] = ',';             // Add comma
            }
        }

        sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), resp_str_wake_src);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 3)
    {
        /* AT+PMGRWAKESRC=<action>,<wake_source> */
        int action, wake_source;
        pmgr_ctrl = RM_PMGR_W_get_ctrl();
        if (pmgr_ctrl == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        if ((rm_atcmd_w_core_common_stoi(argv[1], &action, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[2], &wake_source, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if ((rm_atcmd_w_core_common_is_in_valid_range(action, 1, 2) == pdFALSE) ||
            (rm_atcmd_w_core_common_is_in_valid_range(wake_source, 1, 6) == pdFALSE))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        wake_source--;

        if (action == 1 /* SET */)
        {
            RM_PMGR_W_set_wake_source(pmgr_ctrl, (1UL << wake_source));
        }
        else                           /* CLR */
        {
            RM_PMGR_W_clr_wake_source(pmgr_ctrl, (1UL << wake_source));
        }
    }
    else
    {
        if (argc == 2)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRWAKESRC)
{
    const char * p_usage = "<action>,<wake_source>";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRWAKESRC)
{
    const char * p_description =
        "Set / Clear action (1:Set, 2:Clear) PMGR wake source (0:RTC, 1:GPT, 2:GPIO, 3:ADC, 4:WIFI, 5:BLE)";

    return p_description;
}

extern bool twt_setup_req_valid(struct twt_setup_req * req);

static fsp_err_atcmd_err_code handle_twt (int argc, char * argv[])
{
    struct twt_teardown_req teardown_params = {0};
    struct twt_setup_req    setup_params;
    int twt_action;
    int wake_int_mantissa, wake_int_exp, min_twt_wake_dur;
    int flow_type, trigger, neg_type;
    int auto_setup;

    if (argc < 4)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (rm_atcmd_w_core_common_stoi(argv[2], &twt_action, POL_1) != 0)
    {
        printf("ERROR: Failed to parse twt_action from argv[2] = %s\n", argv[2]);

        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    if (twt_action == 0)               /* TWT Teardown */
    {
        if (argc < 4)                  /* For teardown, we expect exactly 4 tokens */
        {
            printf("ERROR: Not enough arguments for teardown, expected at least 4\n");

            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        if (rm_atcmd_w_core_common_stoi(argv[3], &neg_type, POL_1) != 0)
        {
            printf("ERROR: Failed to parse neg_type from argv[3] = %s\n", argv[3]);

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        printf("Performing TWT Teardown with neg_type = %d\n", neg_type);

        teardown_params.neg_type = (uint8_t) neg_type;
        teardown_params.all_twt  = 1;
        WIFI_TwtTeardown(&teardown_params);
    }
    else if (twt_action == 1)          /* TWT Setup */
    {
        /* For setup, we expect 10 tokens total: command + 9 arguments */
        if (argc < 10)
        {
            printf("ERROR: Not enough arguments for setup, expected at least 9\n");

            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        setup_params.vif_idx    = 0;
        setup_params.setup_type = 1;

        if ((rm_atcmd_w_core_common_stoi(argv[3], &wake_int_mantissa, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[4], &wake_int_exp, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[5], &min_twt_wake_dur, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[6], &flow_type, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[7], &trigger, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[8], &neg_type, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[9], &auto_setup, POL_1) != 0))
        {
            printf("ERROR: Failed to parse one or more arguments\n");

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        setup_params.conf.wake_int_mantissa = (uint16_t) wake_int_mantissa;
        setup_params.conf.wake_int_exp      = (uint8_t) wake_int_exp;
        setup_params.conf.min_twt_wake_dur  = (uint8_t) min_twt_wake_dur;
        setup_params.conf.wake_dur_unit     = 1; /* Fixed value */
        setup_params.conf.flow_type         = (uint8_t) flow_type;
        setup_params.conf.trigger           = (uint8_t) trigger;
        setup_params.conf.neg_type          = (uint8_t) neg_type;

        setup_params.auto_setup = (uint8_t) auto_setup;

        if (!twt_setup_req_valid(&setup_params))
        {
            printf("ERROR: Invalid set-up request parameters\n");

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
        else
        {
            WIFI_TwtSetup(&setup_params);
        }
    }
    else
    {
        printf("ERROR: Invalid twt_action value: %d\n", twt_action);

        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_DPM_ATCMD_CB(PMGRFORCE)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    pmgr_ctrl_t          * pmgr_ctrl = NULL;
    uint8_t                const_cnt = 0, i;
    pmgr_wake_source_t     wake_src = 0;
    int sleep_mode;

    if (argc == 3)
    {
        /* AT+PMGRFORCE=<sleep_mode>,<duration> */
        int duration = 0;
        pmgr_lld_power_mode_t pwr_mode;

        pmgr_ctrl = RM_PMGR_W_get_ctrl();
        pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *) pmgr_ctrl;
        if (pmgr_ctrl == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        if ((rm_atcmd_w_core_common_stoi(argv[1], &sleep_mode, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[2], &duration, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(sleep_mode, 2, 5) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (sleep_mode == 2)
        {
            pwr_mode = PMGR_LLD_POWER_MODE_SLEEP2;
        }
        else if (sleep_mode == 3)
        {
            pwr_mode = PMGR_LLD_POWER_MODE_SLEEP3;
        }
        else if (sleep_mode == 4)
        {
            pwr_mode = PMGR_LLD_POWER_MODE_DPM;
        }
        else
        {
            pwr_mode = PMGR_LLD_POWER_MODE_SLEEP4;
        }

        if (pwr_mode == PMGR_LLD_POWER_MODE_DPM)
        {
            if (get_run_mode() != 0 /* WIFI_DEVICE_MODE_EXT_STATION */)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_UNSUPPORTED_SYS_MODE;
                goto end;
            }

            if (chk_network_ready(0 /* WLAN0_IFACE */) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_NO_WIFI_CONN;
                goto end;
            }

            RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RETENTION, &const_cnt);
            if (const_cnt == 0)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_CONSTR_RETENTION_NOT_SET;
                goto end;
            }

            RM_PMGR_W_get_wake_source(RM_PMGR_W_get_ctrl(), &wake_src);
            if ((wake_src & PMGR_WAKE_SOURCE_WIFI) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_WAKE_SRC_WIFI_NOT_SET;
                goto end;
            }

            if (!RM_PMGR_W_dpm_is_enabled())
            {
                err = FSP_ERR_AT_CMD_ERR_PMGR_DPM_MODE_DISABLED;
                goto end;
            }

            RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM, &const_cnt);

            RM_PMGR_W_dpm_timer_create("at_force_dpm", "f_tmr", NULL, SEC2MS(duration), 0);

            for (i = 0; i < const_cnt; i++)
            {
                RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                if (p_instance_ctrl && (p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED))
                {
                    p_instance_ctrl->ram_counter_cause[PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE]--;
                }
            }
        }
        else
        {
            RM_PMGR_W_force(RM_PMGR_W_get_ctrl(), pwr_mode, SEC2US(duration));
        }
    }
    else if (argc > 3)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &sleep_mode, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (sleep_mode == 5)
        {
            err = handle_twt(argc, argv);
            goto end;
        }

        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto end;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_DPM_ATCMD_FORMAT_CB(PMGRFORCE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_DPM_ATCMD_BRIEF_CB(PMGRFORCE)
{
    const char * p_description = "Force sleep mode for the specified duration (in seconds)";

    return p_description;
}

  #if defined(__SUPPORT_DPM_EXT_WU_MON__)
   #if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
    #if defined(__SUPPORT_DPM_EXT_WU_MON__)
static char * RM_ATCMD_W_CORE_DPM_get_dpm_wakeup_status (void)
{
    unsigned long wakeup_src;

     #if defined(__SUPPORT_NOTIFY_RTC_WAKEUP__)
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
    {
        return NULL;
    }

    if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE)
    {
        return "POR";
    }
     #endif                            // __SUPPORT_NOTIFY_RTC_WAKEUP__

    /* DPM Wakeup */
    wakeup_src = RM_WIFI_dpm_ptim_event_get();

    if (CHK_PTIM_STATUS(wakeup_src, RTOS4DPM_ST_DEAUTH))
    {
        return "DEAUTH";
    }
    else if (CHK_PTIM_STATUS(wakeup_src, RTOS4DPM_ST_UPLOAD))
    {
        return "UC";
    }
    else if (CHK_PTIM_STATUS(wakeup_src, RTOS4DPM_ST_NOBCN))
    {
        return "NOBCN";
    }
    else if (CHK_PTIM_STATUS(wakeup_src, RTOS4DPM_ST_NOACK))
    {
        return "NOACK";
    }
    else if (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_GPIO_WAKEUP_COUNTER_WITH_RETENTION))
    {
        return "EXT\\RTC";
    }
    else if (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_GPIO_WITH_RETENTION))
    {
        return "EXT";
    }

     #if defined(__SUPPORT_DPM_ABNORM_MSG__)
    else if (CHK_PTIM_STATUS_UNDEF(wakeup_src))
    {
        return "UNDEFINED";
    }
     #endif                            // __SUPPORT_DPM_ABNORM_MSG__
     #ifdef    __DPM_WAKEUP_NOTICE_ADDITIONAL__
    else if (CHK_PTIM_STATUS(wakeup_src, DPM_FROM_FAST))
    {
        return "RTC";
    }
    else
    {
        return "ETC";
    }
     #else
    else
    {
        return NULL;
    }
     #endif                            /* __DPM_WAKEUP_NOTICE_ADDITIONAL__ */
}

    #endif                             /* __SUPPORT_DPM_EXT_WU_MON__ */
   #else /* For fixed compiler error when RAM build configuration */
static char * RM_ATCMD_W_CORE_DPM_get_dpm_wakeup_status (void)
{
    return "POR";
}

   #endif                              // (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)

static void RM_ATCMD_W_CORE_DPM_ext_wu_mon (void * pvParameters)
{
    RA6W1_UNUSED_ARG(pvParameters);

    char   dpm_wu_str[8] = {0x00, };
    char * tmp_str       = NULL;

    /* Check DPM Wakeup type */
    tmp_str = RM_ATCMD_W_CORE_DPM_get_dpm_wakeup_status();

    if (tmp_str == NULL)
    {
        /* The dpm is registered after this task is created.
         * RM_PMGR_W_dpm_job_name_clear() function may be called before registeration.
         */
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
        goto end;
    }

    bsp_safe_strcpy(dpm_wu_str, tmp_str, sizeof(dpm_wu_str));

   #if defined(__SUPPORT_NOTIFY_RTC_WAKEUP__)
    vTaskDelay(portCONVERT_MS_2_TICKS(RM_ATCMD_W_CORE_DPM_EXT_WU_DELAY_TO_RCV_AT_CMD * 10));
   #else
    if (strcmp(dpm_wu_str, "EXT") == 0) // External Wakeup
    {
        /* Wait until to receive AT-CMD */
        vTaskDelay(portCONVERT_MS_2_TICKS(RM_ATCMD_W_CORE_DPM_EXT_WU_DELAY_TO_RCV_AT_CMD * 10));
    }

    #if defined(__SUPPORT_UC_WU_MON__)
    else if (strcmp(dpm_wu_str, "UC") == 0) // Unicast Rx Wakeup
    {
        /* Wait until to receive AT-CMD */
        vTaskDelay(portCONVERT_MS_2_TICKS(RM_ATCMD_W_CORE_DPM_EXT_WU_DELAY_TO_RCV_AT_CMD * 10));
    }
    #endif                             // __SUPPORT_UC_WU_MON__
    else
    {
        goto end;
    }
   #endif                              // __SUPPORT_NOTIFY_RTC_WAKEUP__

    /* Check sleep state */
    while (pdTRUE)
    {
        if (rm_atcmd_w_core_dpm_ext_wu_sleep_flag == pdFALSE)
        {
            RM_PMGR_W_dpm_sleep_ready_clear(RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME);
        }
        else
        {
            RM_PMGR_W_dpm_sleep_ready_set(RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME);
        }

        vTaskDelay(portCONVERT_MS_2_TICKS(10));
    }

end:

    RM_PMGR_W_dpm_job_name_clear(RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME);

    rm_atcmd_w_core_dpm_ext_wu_mon_handler = NULL;

    vTaskDelete(NULL);
}

static void RM_ATCMD_W_CORE_DPM_create_ext_wu_mon (void)
{
    BaseType_t ret;

    ret = xTaskCreate(RM_ATCMD_W_CORE_DPM_ext_wu_mon,
                      RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME,
                      RM_ATCMD_W_CORE_DPM_EXT_WU_MON_SIZE,
                      (void *) NULL,
                      (OS_TASK_PRIORITY_USER),
                      &rm_atcmd_w_core_dpm_ext_wu_mon_handler);

    if (ret != pdPASS)
    {
        RM_ATCMD_W_CORE_DPM_ERROR("ERROR(%d)\n", ret);
    }

    if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) && RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_dpm_job_name_set(RM_ATCMD_W_CORE_DPM_EXT_WU_MON_NAME, 0);
    }
}

  #endif                               // (__SUPPORT_DPM_EXT_WU_MON__)
 #endif                                /* CFG_WIFI */
#endif                                 /* CFG_PMGR */
