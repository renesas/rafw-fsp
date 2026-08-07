/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_NETWORKING_INTERFACES
 * @defgroup AWSIOT_W_API AWSIOT_W Application (rm_awsiot_w)
 * @brief Interface for AWSIOT_W Application APIs.
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_AWSIOT_W_API_H
#define RM_AWSIOT_W_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Includes board and MCU related header files. */
#include "bsp_api.h"
#include "rm_awsiot_w_cfg.h"
/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef void awsiot_w_ctrl_t;
/** AWSIOT_W configuration parameters */
typedef struct st_rm_awsiot_w_app_cfg
{
    spi_flash_instance_t const * p_flash_instance;          ///< Pointer to flash(ospi) instance.
    void const * p_context;                                 ///< User defined context passed into callback function.
    void const * p_extend;                                  ///< Pointer to extended configuration by instance of interface.
} rm_awsiot_w_app_cfg_t;

typedef struct st_awsiot_w_api
{
    /** Initialize AWSIOT_W Application.
     *
     * @param[in, out] p_ctrl Pointer to user-provided control data for the
     * AWSIOT_W.
     * @param[in]      p_cfg  Pointer to AWSIOT_W configuration structure.
     *                            user.
     */
    fsp_err_t (* open)(awsiot_w_ctrl_t * const p_ctrl, rm_awsiot_w_app_cfg_t const * const p_cfg);

    /** Close AWSIOT_W Application.
     *
     * @param[in]   p_ctrl     Pointer to the AWSIOT_W control block.
     */
    fsp_err_t (* close)(awsiot_w_ctrl_t * const p_ctrl);


} awsiot_w_api_t;

/** This structure encompasses everything that is needed to use an instance of
 * this interface. */
typedef struct st_rm_awsiot_w_app_instance
{
    awsiot_w_ctrl_t      * p_ctrl;       ///< Pointer to the control structure for this instance
    rm_awsiot_w_app_cfg_t const * p_cfg; ///< Pointer to the configuration structure for this instance
    awsiot_w_api_t const * p_api;        ///< Pointer to the API structure for this instance
} rm_awsiot_w_app_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************/ /**
 * @} (end defgroup AWSIOT_W_API)
 **********************************************************************************************************************/
