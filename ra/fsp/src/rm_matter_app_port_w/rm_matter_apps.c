/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_matter_wifi_common.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define MATTER_WIFI_APPS_OPEN    (0X4D545452ULL)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
#define    MATTER_APP_STACK_SIZE        ((8 * 1024) / sizeof(StackType_t))

/***********************************************************************************************************************
 * External global functions
 **********************************************************************************************************************/
extern unsigned int wait_supplicant_done(unsigned int timeout);
extern void matter_app_main_start(void *arg);

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
TaskHandle_t xHandle;

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
rm_matter_app_instance_ctrl_t  const * gp_matter_app_instance;
const matter_api_t g_rm_matter_on_rm_matter =
{
    .open        = RM_MATTER_WIFI_Open,
    .close       = RM_MATTER_WIFI_Close,
};

/*******************************************************************************************************************//**
 * @addtogroup MATTER_WIFI_APPS_LOCK_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Configure and start the MATTER. Implements @ref matter_api_t::open.
 *
 * This function should only be called once. The subsequent calls will have no effect.
 *
 * Example:
 * @snippet rm_matter_apps_example.c RM_MATTER_ON
 *
 * @retval FSP_SUCCESS              Matter successfully configured.
 * @retval FSP_ERR_ASSERTION        Null pointer, or one or more configuration options is invalid.
 * @retval FSP_ERR_ALREADY_OPEN     Module is already open.  This module can only be opened once.
 **********************************************************************************************************************/
fsp_err_t RM_MATTER_WIFI_Open(matter_ctrl_t * const p_ctrl, rm_matter_app_cfg_t const * const p_cfg)
{
    fsp_err_t    err = FSP_SUCCESS;
    BaseType_t status;
    rm_matter_app_instance_ctrl_t * p_instance_ctrl = (rm_matter_app_instance_ctrl_t *) p_ctrl;

#if MATTER_APP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ERROR_RETURN(MATTER_WIFI_APPS_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ASSERT(NULL != p_cfg->p_flash_instance);
#endif

    if (0 == wait_supplicant_done(100))
    {
        err = FSP_ERR_WIFI_FAILED;
    }

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* TODO This global pointer is tentative. */
    gp_matter_app_instance = p_instance_ctrl;
    p_instance_ctrl->p_cfg        = p_cfg;
    p_instance_ctrl->open = MATTER_WIFI_APPS_OPEN;
    status = xTaskCreate(matter_app_main_start, "matter_task",
                    MATTER_APP_STACK_SIZE, NULL,
                    OS_TASK_PRIORITY_USER, &xHandle);
    if (status == pdPASS)
        return FSP_SUCCESS;
    else 
        return FSP_ERR_NOT_OPEN;
}

/*******************************************************************************************************************//**
 * Configure and start the MATTER. Implements @ref matter_api_t::close.
 *
 * This function should only be called once. The subsequent calls will have no effect.
 *
 * Example:
 * @snippet rm_matter_apps_example.c RM_MATTER_OFF
 *
 * @retval FSP_SUCCESS              Matter successfully configured.
 * @retval FSP_ERR_ASSERTION        Null pointer, or one or more configuration options is invalid.
 * @retval FSP_ERR_ALREADY_OPEN     Module is already open.  This module can only be opened once.
 **********************************************************************************************************************/
fsp_err_t RM_MATTER_WIFI_Close(matter_ctrl_t * const p_ctrl)
{
    rm_matter_app_instance_ctrl_t * p_instance_ctrl = (rm_matter_app_instance_ctrl_t *) p_ctrl;

#if MATTER_APP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(MATTER_WIFI_APPS_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_instance_ctrl->open = 0U;
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup MATTER_WIFI_APPS_LOCK_W)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/
