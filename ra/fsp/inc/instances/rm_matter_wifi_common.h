/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_MATTER_H
#define RM_MATTER_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_wifi.h"
#include "r_spi_flash_api.h"
#include "rm_matter_wifi_cfg.h"
#include "rm_matter_core_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct st_rm_matter_app_instance_ctrl
{
    rm_matter_app_cfg_t const * p_cfg; // Pointer to initial configurations.
    uint32_t     open;                 // Indicates whether the open() API has been successfully called.
    void const * p_context;            // Placeholder for user data.
} rm_matter_app_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern const rm_matter_app_instance_ctrl_t * gp_matter_app_instance;

extern rm_matter_app_instance_ctrl_t g_rm_matter_app_ctrl;
extern const rm_matter_app_cfg_t     g_rm_matter_app_cfg;
extern const matter_api_t            g_rm_matter_on_rm_matter;

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t RM_MATTER_WIFI_Open(matter_ctrl_t * const p_ctrl, rm_matter_app_cfg_t const * const p_cfg);
fsp_err_t RM_MATTER_WIFI_Close(matter_ctrl_t * const p_ctrl);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
