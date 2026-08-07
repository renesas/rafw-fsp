/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 ****************************************************************************************
 *
 * @file rm_ble_recovery.c
 *
 * @brief Functions to BLE main recovery
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

/*******************************************************************************************************************//**
 * @addtogroup BLE_RECOVERY
 * @{
 ***********************************************************************************************************************/


/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include <stdarg.h>
#include "sdk_defs.h"

#if defined(RM_BLE_RECOVERY_ENABLE)

#if defined(START_BLE_GTL)
 #include "r_ble_gtl.h"
 #include "rm_ble_abs.h"
#endif

#include "rm_ble_recovery.h"

#if (BSP_CFG_RTOS == 2) // FREERTOS
 #include <FreeRTOS.h>
 #include "task.h"
 #include "timers.h"
 #include "portmacro.h"
#endif

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
 #include "rm_watchdog_service_w.h"
#endif

#if defined(RM_BLE_LOG_MSG)
 #include "SEGGER_RTT.h"
#endif

/***********************************************************************************************************************
 * Function Prototypes
 ***********************************************************************************************************************/

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
static void rm_ble_wd_callback(wdt_callback_args_t * p_args);
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define RM_BLE_RECOVERY_TASK_NAME         "BLE recovery task"
#define RM_BLE_RECOVERY_STACK_SIZE        (256)

#if (BSP_CFG_RTOS == 2) // FREERTOS
 #define RM_BLE_RECOVERY_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)
#endif

#define RM_BLE_RECOVERY_TASK_DELAY        (5 * pdMS_TO_TICKS(1000))
#define RM_BLE_RECOVERY_MAX_CHECK_RETRY   (3)
#define RM_BLE_RECOVERY_CHECK_DELAY       (1 * pdMS_TO_TICKS(1000))

#if RM_BLE_LOG_MSG
#define    RM_BLE_LOG_PRINT(x)            SEGGER_RTT_printf(0, x)
#else
#define    RM_BLE_LOG_PRINT(x)
#endif

 /***********************************************************************************************************************
 * Local Typedef definitions
 **********************************************************************************************************************/

/* WDT Instance. */
#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
uint8_t            g_rm_ble_sys_wdog_id;
wdog_w_instance_ctrl_t    g_rm_ble_wdt_ctrl;
const wdt_cfg_t        g_rm_ble_wdt_cfg =
{
    .timeout          = WATCHDOG_SERVICE_W_TIMER_RESET_VALUE * 10,
    .reset_control    = WDT_RESET_CONTROL_NMI,
    .p_callback       = rm_ble_wd_callback,
    .p_context        = NULL,
    .p_extend         = NULL,
};
const wdt_instance_t  g_wdt_ble =
{
    .p_ctrl           = &g_rm_ble_wdt_ctrl,
    .p_cfg            = &g_rm_ble_wdt_cfg,
    .p_api            = &g_wdt_on_wdog_w,
};


/* Watchdog Service Instance. */
watchdog_service_w_instance_ctrl_t   g_rm_ble_watchdog_service_ctrl;
const watchdog_service_cfg_t         g_rm_ble_watchdog_service_cfg =
{
    .p_wdt            = &g_wdt_ble,
    .p_context        = NULL,
    .p_extend         = NULL,
};
#endif

typedef struct st_rm_ble_recovery_info_type
{
    uint32_t    count;
    bool        opened;

#if (BSP_CFG_RTOS == 2) // FREERTOS
    TaskHandle_t     task_id;
#endif

#if RM_BLE_RECOVERY_CB_ON_FAIL
    rm_ble_recovery_fail_callback_t  p_fail_callback;
#endif

} rm_ble_recovery_info_t;

/***********************************************************************************************************************
 * Static Private Variables
 **********************************************************************************************************************/
static rm_ble_recovery_info_t grm_ble_recovery_info = { 0 };

#if (configSUPPORT_STATIC_ALLOCATION == 1)
static StackType_t     grm_ble_recovery_stack[ RM_BLE_RECOVERY_STACK_SIZE ];
static StaticTask_t    grm_ble_recover_task_TCB_buffer;
#endif

/***********************************************************************************************************************
 * Local function prototypes
 **********************************************************************************************************************/
static void rm_ble_recovery_task_function( void *pvParameters);

/***********************************************************************************************************************
 * Public Functions Implementation
 **********************************************************************************************************************/

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
/*******************************************************************************************************************//**
 * @brief  register the task for WD.
**********************************************************************************************************************/
static void rm_ble_wd_register(void)
{
    g_rm_ble_sys_wdog_id = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;

    if (g_rm_ble_watchdog_service_ctrl.open == 0)
    {
        return;
    }

    RM_WATCHDOG_SERVICE_W_Register(&g_rm_ble_watchdog_service_ctrl, &g_rm_ble_watchdog_service_cfg, &g_rm_ble_sys_wdog_id);
}

/*******************************************************************************************************************//**
 * @brief  unregister the task for WD.
**********************************************************************************************************************/
static void rm_ble_wd_unregister(void)
{
    if (g_rm_ble_watchdog_service_ctrl.open == 0)
    {
        return;
    }

    RM_WATCHDOG_SERVICE_W_Unregister(&g_rm_ble_watchdog_service_ctrl, g_rm_ble_sys_wdog_id);
}

/*******************************************************************************************************************//**
 * @brief  suspend the task from WD.
**********************************************************************************************************************/
static void  rm_ble_wd_suspend(void)
{
    if (g_rm_ble_watchdog_service_ctrl.open == 0)
    {
        return;
    }

    RM_WATCHDOG_SERVICE_W_Suspend(&g_rm_ble_watchdog_service_ctrl, g_rm_ble_sys_wdog_id);
}

/*******************************************************************************************************************//**
 * @brief  resume_ and notify the task to WD.
**********************************************************************************************************************/
static void rm_ble_wd_resume_notify(void)
{
    if (g_rm_ble_watchdog_service_ctrl.open == 0)
    {
        return;
    }

    RM_WATCHDOG_SERVICE_W_ResumeAndNotify(&g_rm_ble_watchdog_service_ctrl, &g_rm_ble_watchdog_service_cfg, g_rm_ble_sys_wdog_id);
}
#endif

/*******************************************************************************************************************//**
 * @brief  print log and SW Reset .
**********************************************************************************************************************/
static void rm_ble_recovery_swreset(void)
{
    printf("n\n\nBLE NOT Responding\n\nRebooting...\n\n");

#if (BSP_CFG_RTOS == 2) // FREERTOS
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
#endif

    SWRESET;
}

/*******************************************************************************************************************//**
 * @brief  call the GTL  to get the ble response.
 * @retval BLE_ERR_RSP_TIMEOUT    - on case of failed to get response from BLE
**********************************************************************************************************************/
static fsp_err_t rm_ble_recovery_check_ble(void)
{
    fsp_err_t status = BLE_ERR_RSP_TIMEOUT;
    uint8_t retry_count = RM_BLE_RECOVERY_MAX_CHECK_RETRY;

    while (--retry_count)
    {
        if (R_BLE_GTL_GAP_GetVerInfo() == BLE_SUCCESS)
        {
            status = BLE_SUCCESS;
            break;
        }
        RM_BLE_LOG_PRINT("Check BLE Fail\n");
        vTaskDelay(RM_BLE_RECOVERY_CHECK_DELAY);
    }

    return status;
}

/*******************************************************************************************************************//**
 * @brief init the recovery internal information
**********************************************************************************************************************/
static void rm_ble_recovery_info_init(rm_ble_recovery_info_t * info_p)
{
}

/*******************************************************************************************************************//**
 * @brief start the recovery task.
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed to start the task
**********************************************************************************************************************/
static fsp_err_t rm_ble_recovery_task_start(rm_ble_recovery_info_t  *info_p)
{
    fsp_err_t status = FSP_SUCCESS;

#if (BSP_CFG_RTOS == 2) // FREERTOS
#if (configSUPPORT_STATIC_ALLOCATION == 1)
    info_p->task_id = xTaskCreateStatic(rm_ble_recovery_task_function,     /** Entry function of INT_EVT_TSK task  **/
                                        RM_BLE_RECOVERY_TASK_NAME,         /** Task Name                           **/
                                        RM_BLE_RECOVERY_STACK_SIZE,        /** Stack size in words                 **/
                                        info_p,                            /** Task's parameter                    **/
                                        RM_BLE_RECOVERY_TASK_PRIORITY,     /** Task's priority                     **/
                                        &grm_ble_recovery_stack[0],        /** Task STACK                          **/
                                        &grm_ble_recover_task_TCB_buffer); /** Task TCB                            **/
#else
    info_p->task_id = xTaskCreate(rm_ble_recovery_task_function,           /** Entry function of INT_EVT_TSK task  **/
                                  RM_BLE_RECOVERY_TASK_NAME,               /** Task Name                           **/
                                  RM_BLE_RECOVERY_STACK_SIZE,              /** Stack size in words                 **/
                                  info_p,                                  /** Task's parameter                    **/
                                  RM_BLE_RECOVERY_TASK_PRIORITY,           /** Task's priority                     **/
                                  NULL);                                   /** The task handle is not required     **/

#endif
    if (info_p->task_id == NULL)
    {
        status = FSP_ERR_BLE_INIT_FAILED;
    }
#endif

    return status;
}

/*******************************************************************************************************************//**
 * @brief the finction that use as the recovery task.
**********************************************************************************************************************/
static void rm_ble_recovery_task_function( void *pvParameters)
{
    rm_ble_recovery_info_t  *info_p = (rm_ble_recovery_info_t * )pvParameters;
    fsp_err_t status = BLE_SUCCESS;

    RM_BLE_LOG_PRINT("start Recovery Task\n");

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
    rm_ble_wd_register();
#endif

#if (BSP_CFG_RTOS == 2) // FREERTOS
    while (true)
    {
        info_p->count++;

        RM_BLE_LOG_PRINT("check id\n");
        status = rm_ble_recovery_check_ble();
        if (status != BLE_SUCCESS)
        {
            RM_BLE_LOG_PRINT("BLE NOT Responding\n");

#if RM_BLE_RECOVERY_CB_ON_FAIL
            if (info_p->p_fail_callback)
            {
                info_p->p_fail_callback();
            }
            else
            {
                rm_ble_recovery_swreset();
            }

#else
            rm_ble_recovery_swreset();
#endif

        }

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
        rm_ble_wd_suspend();
#endif

        RM_BLE_LOG_PRINT("Task Delay\n");
        vTaskDelay(RM_BLE_RECOVERY_TASK_DELAY);

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
        rm_ble_wd_resume_notify();
#endif

    }
#endif // (BSP_CFG_RTOS == 2)

}

/*******************************************************************************************************************//****
 * @brief Init the BLE recovery module
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed
**********************************************************************************************************************/
static fsp_err_t rm_ble_recovery_init(rm_ble_recovery_info_t  *info_p)
{
    fsp_err_t status = FSP_SUCCESS;

    rm_ble_recovery_info_init(info_p);

    RM_BLE_LOG_PRINT("Creating Recovery Task\n");
    status = rm_ble_recovery_task_start(info_p);
    if (status != FSP_SUCCESS)
    {
        RM_BLE_LOG_PRINT("Failed to create Recovery Task\n");
    }

    return status;
}

#if RM_BLE_RECOVERY_CB_ON_FAIL
/*******************************************************************************************************************//****
 * @brief function to set fail callback
***********************************************************************************************************************/
void RM_BLE_RECOVERY_set_fail_callback(void *callback)
{
    rm_ble_recovery_info_t  *info_p = &grm_ble_recovery_info;

    info_p->p_fail_callback = (rm_ble_recovery_fail_callback_t)callback;
}
#endif

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
/*******************************************************************************************************************//****
 * @brief Watchdog call back function
***********************************************************************************************************************/
static void rm_ble_wd_callback(wdt_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}
#endif

/*******************************************************************************************************************//****
 * @brief function to set fail callback
***********************************************************************************************************************/
uint32_t RM_BLE_RECOVERY_running(void)
{
    rm_ble_recovery_info_t  *info_p = &grm_ble_recovery_info;

    return info_p->count;
}

/*******************************************************************************************************************//****
 * @brief function to open and start the recovery
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed
***********************************************************************************************************************/
fsp_err_t RM_BLE_RECOVERY_Open(void)
{
    fsp_err_t status;
    rm_ble_recovery_info_t *info_p = &grm_ble_recovery_info;

    if (info_p->opened)
    {
        return FSP_ERR_ALREADY_OPEN;
    }

    status = rm_ble_recovery_init(info_p);
    if (status == FSP_SUCCESS)
    {
        info_p->opened = true;
    }

    return status;
}

/*******************************************************************************************************************//****
 * @brief function to stop and close the recovery
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed
**********************************************************************************************************************/
fsp_err_t RM_BLE_RECOVERY_Close(void)
{
    rm_ble_recovery_info_t  *info_p = &grm_ble_recovery_info;

#if RM_BLE_RECOVERY_CFG_WATCHDOG_SERVICE_ENABLE
    rm_ble_wd_unregister();
#endif

#if (BSP_CFG_RTOS == 2) // FREERTOS
    if (info_p->task_id)
    {
        vTaskDelete(info_p->task_id);
    }
#endif

    memset(info_p, 0, sizeof(rm_ble_recovery_info_t));
    info_p->opened = false;

    return FSP_SUCCESS;
}

#endif  // defined(RM_BLE_RECOVERY_ENABLE)

/*******************************************************************************************************************//**
 * @} (end addtogroup BLE_LOADER)
 **********************************************************************************************************************/

/* EOF */