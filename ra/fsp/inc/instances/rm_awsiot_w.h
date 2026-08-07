/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup AWSIOT_W
 * @{
 **********************************************************************************************************************/

#ifndef RM_AWSIOT_W_H
#define RM_AWSIOT_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
 #include "rm_wifi.h"
 #include "r_spi_flash_api.h"
 #include "rm_awsiot_w_api.h"
#include "rm_awsiot_w_cfg.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** AWSIOT_W control structure. DO NOT INITIALIZE. */
typedef struct st_awsiot_w_app_instance_ctrl
{
    rm_awsiot_w_app_cfg_t const * p_cfg;    ///< Pointer to initial configurations.
    uint32_t     open;                    ///< Indicates whether the open() API has been successfully called.
    void const * p_context;               ///< Placeholder for user data.
} rm_awsiot_w_app_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern const rm_awsiot_w_app_instance_ctrl_t * gp_awsiot_w_app_instance;

extern rm_awsiot_w_app_instance_ctrl_t g_rm_awsiot_w_app_ctrl;
extern rm_awsiot_w_app_cfg_t g_rm_awsiot_w_app_cfg;
extern const awsiot_w_api_t         g_rm_awsiot_w;

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t RM_AWSIOT_W_Open(awsiot_w_ctrl_t * const p_ctrl, rm_awsiot_w_app_cfg_t const * const p_cfg);
fsp_err_t RM_AWSIOT_W_Close(awsiot_w_ctrl_t * const p_ctrl);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************//**
 * @} (end addtogroup AWSIOT_W)
 **********************************************************************************************************************/
