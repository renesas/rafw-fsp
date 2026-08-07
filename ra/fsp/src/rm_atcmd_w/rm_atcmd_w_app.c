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
#include <stdarg.h>
#include "rm_atcmd_w_api.h"
#include "rm_atcmd_w_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "common_data.h"
#if (TEST_WIFI || TEST_ATCMD_W_APP)
 #include "rm_atcmd_w_core_tests_data.h"
#endif
#if ATCMD_IF_SUPPORT
 #if CFG_WIFI
  #include "romac4rtos.h"
  #include "common_def.h"
  #include "rm_wifi_helper.h"
  #if CFG_PMGR
   #include "rm_pmgr_w_instance.h"
  #endif                               /* CFG_PMGR */
 #endif                                /* CFG_WIFI */
#endif                                 /* ATCMD_IF_SUPPORT */
#if (ATCMD_BLE_BRG == 1)
 #include "rm_atcmd_w_core_blebrg.h"
#endif
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
 #include dg_configADNVPARAM_PROJ_FILE
#endif

#define ATCMD_W_APP_TASK_STACK_SIZE    (1024 * 5 / 4)
 #define ATCMD_W_APP_TASK_PRIORITY     (1)
#define ATCMD_W_TASK_DELAY             (200)

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
atcmd_w_core_instance_t g_at_core_instance = {0x0, };
TaskHandle_t            atcmd_task_handler = NULL;
#if ATCMD_IF_SUPPORT
static void * gp_startup_atcmd_ctrl            = NULL;
static int    atcmd_softap_init_done_sent_flag = pdFALSE;
#endif

#if (ATCMD_TRANSPORT_UART_W == 1)
 #ifdef RM_MAP_PERSISTANT_W
uart_cfg_t            g_uart_conf_app         = {0};
uart_w_extended_cfg_t g_uart_conf_extend_app  = {0};
uart_w_baud_setting_t g_uart_baud_setting_app = {0};
 #endif
#endif

/***********************************************************************************************************************
 * Private function
 **********************************************************************************************************************/

#if ATCMD_IF_SUPPORT
static unsigned int (* gp_startup_atcmd_event_cb)(void * const p_ctrl, unsigned char * p_in, unsigned int inlen) = NULL;
 #if CFG_PMGR
static const char * atcmd_wakeup_state_txt(void);
 #endif // CFG_PMGR
#endif

fsp_err_t atcmd_w_core_init (atcmd_w_core_instance_t * p_at_core_instance)
{
    fsp_err_t err = FSP_SUCCESS;

    p_at_core_instance->at_conf.conf = g_at0_cfg;

    memset((atcmd_w_tx_queue_t *) &p_at_core_instance->tx_queue, 0x00, sizeof(p_at_core_instance->tx_queue));

    err = g_at_core.open(&p_at_core_instance->at_ctrl, &p_at_core_instance->at_conf.conf);
    FSP_ASSERT(err == FSP_SUCCESS);

    return err;
}

void rm_atcmd_w_app_task (void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    /* Init AT_CORE */

    while (g_at_core_instance.at_ctrl.run_mode == AT_MODE_RUN)
    {
        vTaskDelay(ATCMD_W_TASK_DELAY);
    }

#if (ATCMD_BLE_BRG == 1)
    if ((g_at_core_instance.at_ctrl.run_mode >= AT_MODE_BLEBRG_ALL) &&
        (g_at_core_instance.at_ctrl.run_mode <= AT_MODE_BLEBRG_PINS))
    {
 #if (ATCMD_IF_SUPPORT == 1)
        while (g_at_core_instance.at_ctrl.parser_task_handle)
        {
            vTaskDelay(ATCMD_W_TASK_DELAY);
        }
 #endif

        r_at_rm_blebrg_start(g_at_core_instance.at_ctrl.run_mode);
    }
#endif

    atcmd_task_handler = NULL;

    vTaskDelete(NULL);
}

#if ATCMD_IF_SUPPORT
uint32_t atcmd_set_startup_atcmd_event_callback (void * const        p_ctrl,
                                                 unsigned int (    * p_callback)(
                                                     void * const    p_ctrl,
                                                     unsigned char * p_in,
                                                     unsigned int    inlen))
{
    gp_startup_atcmd_ctrl = p_ctrl;

    gp_startup_atcmd_event_cb = p_callback;

    return FSP_SUCCESS;
}

 #if CFG_WIFI
void atcmd_print_initdone_softap_mode (void)
{
    char resp_str[64] = {0x00, };

    if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) &&
        atcmd_get_init_done_msg_to_mcu_on_softap())
    {
        sprintf(resp_str, "\r\n+INIT:DONE,%d\r\n", get_run_mode());

        if (gp_startup_atcmd_ctrl && gp_startup_atcmd_event_cb)
        {
            gp_startup_atcmd_event_cb(gp_startup_atcmd_ctrl, (unsigned char *) resp_str, strlen(resp_str));
        }

        atcmd_set_init_done_msg_to_mcu_on_softap(pdTRUE);
    }
}

uint32_t atcmd_set_initdone_resp (char * p_out, size_t outlen, int is_startup)
{
#if CFG_PMGR
#if defined(__SUPPORT_DPM_ABNORM_MSG__)
    int abn_type = get_last_abnormal_act();
  #endif
    const char * p_wakeup_state_txt = atcmd_wakeup_state_txt();

    if (!RM_PMGR_W_dpm_is_wakeup())
    {
  #if defined(__SUPPORT_DPM_ABNORM_MSG__)
        if (abn_type != 0)
        {
            snprintf(p_out, outlen, "\r\n+INIT:WAKEUP,ABNORM_%d\r\n", abn_type);
        }
        else
  #endif                               // __SUPPORT_DPM_ABNORM_MSG__
        {
#endif // CFG_PMGR
            if (is_startup)
            {
                if (!rm_wifi_is_in_softap_acs_mode())
                {
                    snprintf(p_out, outlen, "\r\n+INIT:DONE,%d\r\n", get_run_mode());

                    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP)
                    {
                        atcmd_set_init_done_msg_to_mcu_on_softap(pdTRUE);
                    }
                }
            }
            else
            {
                snprintf(p_out, outlen, "\r\n+INIT:DONE,%d\r\n", get_run_mode());
            }
#if CFG_PMGR
        }
    }
    else
    {
  #if defined(__DPM_WAKEUP_NOTICE_ADDITIONAL__)
        snprintf(p_out, outlen, "\r\n+INIT:WAKEUP,%s\r\n", p_wakeup_state_txt);
  #else
        if (p_wakeup_state_txt != NULL
   #if defined(__SUPPORT_NOTIFY_RTC_WAKEUP__)
            && strcmp(p_wakeup_state_txt, "POR") != 0
   #endif                              // __SUPPORT_NOTIFY_RTC_WAKEUP__
            )
        {
   #if defined(__SUPPORT_DPM_ABNORM_MSG__)
            if ((strcmp(p_wakeup_state_txt, "UNDEFINED") == 0) && (abn_type != 0))
            {
                snprintf(p_out, outlen, "\r\n+INIT:WAKEUP,ABNORM_%d\r\n", abn_type);
            }
            else
   #endif                              // (__SUPPORT_DPM_ABNORM_MSG__)
            {
                snprintf(p_out, outlen, "\r\n+INIT:WAKEUP,%s\r\n", p_wakeup_state_txt);
            }
        }
  #endif                               // (__DPM_WAKEUP_NOTICE_ADDITIONAL__)
    }
#endif // CFG_PMGR

    return 0;
}

#if CFG_PMGR
  #if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static const char * atcmd_wakeup_state_txt(void)
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
   #endif                              // __SUPPORT_NOTIFY_RTC_WAKEUP__

    // DPM Wakeup ...
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
   #endif                              // __SUPPORT_DPM_ABNORM_MSG__
   #if defined(__DPM_WAKEUP_NOTICE_ADDITIONAL__)
    else if ((CHK_PTIM_STATUS(wakeup_src, RTOS4DPM_ST_FROM_FAST) || CHK_PTIM_STATUS_UNDEF(wakeup_src)) &&
             (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_COUNTER_WITH_RETENTION)))
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
        if (CHK_PTIM_STATUS_UNDEF(wakeup_src) &&
            (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_COUNTER_WITH_RETENTION)))
        {
            return "RTC";
        }
        else
        {
            return NULL;
        }
    }
   #endif                              /* __DPM_WAKEUP_NOTICE_ADDITIONAL__ */
}

  #else /* For fixed compiler error when RAM build configuration */
static const char * atcmd_wakeup_state_txt (void)
{
    return "POR";
}
#endif // (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
#endif // CFG_PMGR


void atcmd_set_init_done_msg_to_mcu_on_softap (unsigned int flag)
{
    atcmd_softap_init_done_sent_flag = flag;
}

int atcmd_get_init_done_msg_to_mcu_on_softap (void)
{
    return atcmd_softap_init_done_sent_flag;
}

 #endif                                /* CFG_WIFI */
#endif

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

void rm_atcmd_w_app_init ()
{
    /* Init AT_CORE */
    fsp_err_t err = FSP_SUCCESS;

    g_at_core_instance.at_ctrl.run_mode = AT_MODE_RUN;

    err = atcmd_w_core_init(&g_at_core_instance);

    if (err == FSP_SUCCESS)
    {
        /* Init AT_TRANSPORT */
#if (ATCMD_TRANSPORT_UART_W == 1)

        /* Init UART AT_TRANSPORT */
        g_atcmd_transport_on_uart.open(g_atcmd_transport.p_ctrl, &g_atcmd_transport_cfg);

 #ifdef RM_MAP_PERSISTANT_W
        int baudrate     = 0;
        int databits     = 0;
        int parity       = 0;
        int stopbits     = 0;
        int flow_control = 0;

        /* If NVRM has UART configuration, use the configuration for AT command UART .*/
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "ATCMD_UART_BAUDRATE",
                                     &baudrate);
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "ATCMD_UART_BITS", &databits);
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "ATCMD_UART_PARITY", &parity);
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG, "ATCMD_UART_STOPBIT", &stopbits);
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_DEVCFG,
                                     "ATCMD_UART_FLOWCTRL",
                                     &flow_control);

        /* All stored value exist */
        if ((baudrate != -1) && (databits != -1) && (parity != -1) && (stopbits != -1) && (flow_control != -1))
        {
            memcpy((void *) &g_uart_conf_app, (void *) g_atcmd_uart.p_cfg, sizeof(uart_cfg_t));
            memcpy((void *) &g_uart_conf_extend_app, (void *) (g_atcmd_uart.p_cfg->p_extend),
                   sizeof(uart_w_extended_cfg_t));
            g_uart_conf_extend_app.p_baud_setting = &g_uart_baud_setting_app;
            g_uart_conf_app.p_extend              = &g_uart_conf_extend_app;

            err = R_UART_W_BaudCalculate((uint32_t) baudrate, &g_uart_baud_setting_app);
            if (err == FSP_SUCCESS)
            {
                g_uart_conf_app.data_bits           = databits;
                g_uart_conf_app.parity              = parity;
                g_uart_conf_app.stop_bits           = stopbits;
                g_uart_conf_extend_app.flow_control = flow_control;

                R_UART_W_ConfSet(g_atcmd_uart.p_ctrl, &g_uart_conf_app);
            }
        }
 #endif

 #if ATCMD_IF_SUPPORT
        rm_atcmd_w_core_init_rsp_evt_set(ATCMD_W_CORE_EVT_TRANSPORT_INIT_DONE);
 #endif                                /* ATCMD_IF_SUPPORT */
#elif (ATCMD_TRANSPORT_SPI_W == 1)

        /* Init SPI AT_TRANSPORT */
        g_atcmd_transport_on_spi.open(g_atcmd_transport.p_ctrl, &g_atcmd_transport_cfg);
 #if ATCMD_IF_SUPPORT
        rm_atcmd_w_core_init_rsp_evt_set(ATCMD_W_CORE_EVT_TRANSPORT_INIT_DONE);
 #endif                                /* ATCMD_IF_SUPPORT */
#elif (ATCMD_TRANSPORT_SDIO_W == 1)

        /* Init SDIO AT_TRANSPORT */
        g_atcmd_transport_on_sdio.open(g_atcmd_transport.p_ctrl, &g_atcmd_transport_cfg);
 #if ATCMD_IF_SUPPORT
        rm_atcmd_w_core_init_rsp_evt_set(ATCMD_W_CORE_EVT_TRANSPORT_INIT_DONE);
 #endif                                /* ATCMD_IF_SUPPORT */
#endif
    }
}

void atcmd_w_start ()
{
#if ATCMD_IF_SUPPORT
    rm_atcmd_w_core_init_rsp_evt_create();
#endif                                 /* ATCMD_IF_SUPPORT */

    rm_atcmd_w_app_init();

#if (ATCMD_TRANSPORT_UART_W != 1)
    xTaskCreate(rm_atcmd_w_app_task,
                "ATCMD_W_APP_TASK",
                (ATCMD_W_APP_TASK_STACK_SIZE),
                (void *) NULL,
                ATCMD_W_APP_TASK_PRIORITY,
                &atcmd_task_handler);
#endif
}

#if ATCMD_IF_SUPPORT
uint32_t atcmd_print_initdone_resp (void)
{
    uint32_t res = 0;
 #if CFG_WIFI
    char resp_str[64] = {0x00, };

    res = atcmd_set_initdone_resp(resp_str, sizeof(resp_str), 0);
    if ((res == 0) && (strlen(resp_str) > 0))
    {
        if (rm_atcmd_w_core_init_rsp_evt_wait(ATCMD_W_CORE_EVT_WIFI_OPEN_DONE) == FSP_SUCCESS)
        {
  #if CFG_PMGR
            uint32_t notified_val = 0;
            int      pmgr_is_mcu_wu_done_condition         = pdFALSE;
            int      pmgr_is_add_constraint_wait_condition = pdFALSE;
  #endif                               /* CFG_PMGR */

            if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP)
            {
                /* After wpas_ap_configured_cb() it will send "+INIT:DONE,1" */
                atcmd_set_init_done_msg_to_mcu_on_softap(pdTRUE);
            }
            else
            {
  #if CFG_PMGR
                if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) && RM_PMGR_W_dpm_is_wakeup())
                {
                    pmgr_is_mcu_wu_done_condition = pdTRUE;
                }
  #endif                               /* CFG_PMGR */
                gp_startup_atcmd_event_cb(gp_startup_atcmd_ctrl, (unsigned char *) resp_str, strlen(resp_str));
            }

            rm_atcmd_w_core_init_rsp_evt_set(ATCMD_W_CORE_EVT_INIT_RSP_SENT_DONE);

  #if CFG_PMGR
            if (pmgr_is_mcu_wu_done_condition)
            {
                g_at_core_instance.at_ctrl.at_init_msg_sender_thread = xTaskGetCurrentTaskHandle();

                RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);

                xTaskNotifyWait(0x0, EVT_MCUWUDONE_RCV_DONE, &notified_val,
                                portCONVERT_MS_2_TICKS(PMGR_MCUWU_MAX_WAIT));

                if (notified_val & EVT_MCUWUDONE_RCV_DONE)
                {
                    pmgr_is_add_constraint_wait_condition = pdTRUE;
                }
                else
                {
                    pmgr_is_add_constraint_wait_condition = pdFALSE;
                    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
                }
            }

            if (pmgr_is_add_constraint_wait_condition)
            {
                notified_val = 0;

                xTaskNotifyWait(0x0, EVT_SLEEP_BLOCK_RCV_DONE, &notified_val,
                                portCONVERT_MS_2_TICKS(PMGR_SLEEP_BLOCK_MAX_WAIT));

                if (notified_val & EVT_SLEEP_BLOCK_RCV_DONE)
                {
                    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
                    pmgr_is_add_constraint_wait_condition = pdFALSE;
                }
                else
                {
                    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
                    pmgr_is_add_constraint_wait_condition = pdFALSE;
                }
            }
  #endif                               /* CFG_PMGR */
        }
        else
        {
            printf("Wrong state in atcmd init_done event, check the system. \n");
        }
    }

 #else                                 // !CFG_WIFI
    rm_atcmd_w_core_init_rsp_evt_set(ATCMD_W_CORE_EVT_INIT_RSP_SENT_DONE);
 #endif

    return res;
}

#endif

void set_user_app_atcmd_open_callback (ATCmdOpenCallback_t callback)
{
    atcmd_open_user_app_callback = callback;
}

void set_user_app_atcmd_close_callback (ATCmdCloseCallback_t callback)
{
    atcmd_close_user_app_callback = callback;
}

void rm_atcmd_w_core_user_command_register (atcmd_w_core_module_t * user_atcmd)
{
    atcmd_w_core_module_t ** p_user_atcmd = rm_atcmd_get_user_atcmd_ptr();
    *p_user_atcmd = user_atcmd;
}

void rm_atcmd_w_core_user_command_deregister (atcmd_w_core_module_t * user_atcmd)
{
    atcmd_w_core_module_t ** p_user_atcmd = rm_atcmd_get_user_atcmd_ptr();
    *p_user_atcmd = NULL;
}

void rm_atcmd_w_core_user_command_unfixed_register (atcmd_w_core_unfixed_module_t * user_atcmd)
{
    atcmd_w_core_unfixed_module_t ** p_user_atcmd = rm_atcmd_get_user_unfixed_atcmd_ptr();
    *p_user_atcmd = user_atcmd;
}

void rm_atcmd_w_core_user_command_unfixed_deregister (atcmd_w_core_unfixed_module_t * user_atcmd)
{
    atcmd_w_core_unfixed_module_t ** p_user_atcmd = rm_atcmd_get_user_unfixed_atcmd_ptr();
    *p_user_atcmd = NULL;
}

uint8_t rm_atcmd_w_core_mcuwudone_is_received (atcmd_w_core_instance_t * p_at_core_instance)
{
    return p_at_core_instance->at_ctrl.mcu_wu_done;
}

#if ATCMD_IF_SUPPORT
uint8_t rm_atcmd_w_core_sleep_block_is_received (atcmd_w_core_instance_t * p_at_core_instance)
{
    return p_at_core_instance->at_ctrl.sleep_block_recv_at_dpm_wakeup;
}

uint8_t rm_atcmd_w_core_sleep_block_received_set (atcmd_w_core_instance_t * p_at_core_instance, uint8_t flag)
{
    return p_at_core_instance->at_ctrl.sleep_block_recv_at_dpm_wakeup = flag;
}

#endif                                 /* ATCMD_IF_SUPPORT */
