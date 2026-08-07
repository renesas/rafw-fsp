/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_MATTER_INTERFACES
 * @defgroup MATTER_API Matter Interface
 * @brief Interface for Matter functions.
 *
 * @section MATTER_API_Summary Summary
 * The interface provides Matter functionality including resetting the
 * device or generating an interrupt.
 *
 *
 * @{
 **********************************************************************************************************************/

// THIS FILE SHOULD BE MOVED TO ra/fsp/inc/api, THIS IS JUST A TEMPORARY SOLUTION
#ifndef RM_MATTER_WIFI_ALLCLUSTERS_API_H
#define RM_MATTER_WIFI_ALLCLUSTERS_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_spi_flash_api.h"

/* Register definitions, common services and error codes. */

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Matter return codes */
typedef enum
{
    eMatterSuccess      = 0,           ///< Success
    eMatterFailure      = 1,           ///< Failure
    eMatterTimeout      = 2,           ///< Timeout
    eMatterNotSupported = 3,           ///< Not supported
} MATTERReturnCode_t;

/**
 * @brief Turns on Matter.
 *
 * This function turns on the Matter module, initializes the drivers and must be called
 * before calling any other Matter API
 *
 * @note Wi-Fi module must be initialized before calling this function.
 *
 * @return @ref eMatterFailure if Matter module was successfully turned on, failure code otherwise.
 */
MATTERReturnCode_t MATTER_On(void);

/**
 * @brief Turns off Matter.
 *
 * This function turns off the Matter module.
 *
 * @return @ref eMatterFailure if Matter module was successfully turned on, failure code otherwise.
 */
MATTERReturnCode_t MATTER_Off(void);

/** Matter control block. Allocate an instance specific control block to pass into the Matter API calls. */
typedef void matter_ctrl_t;

/** User configuration structure, used in open function. */
typedef struct st_rm_matter_app_cfg
{
    spi_flash_instance_t const * p_flash_instance; ///< Pointer to flash(ospi) instance.
    void const                 * p_context;        ///< User defined context passed into callback function.
    void const                 * p_extend;         ///< Pointer to extended configuration by instance of interface.
} rm_matter_app_cfg_t;

/** Matter functions implemented at the HAL layer will follow this API. */
typedef struct st_matter_api
{
    /*******************************************************************************************************************//**
     * Initialize the Matter service.
     *
     * @param[in]  p_ctrl               Pointer to Matter instance control structure.
     * @param[in]  p_cfg                Pointer to Matter configuration structure.
     *
     * @retval FSP_SUCCESS              Matter successfully configured.
     * @retval FSP_ERR_ASSERTION        Null pointer, or one or more configuration options is invalid.
     * @retval FSP_ERR_ALREADY_OPEN     Module is already open.  This module can only be opened once.
     **********************************************************************************************************************/
    fsp_err_t (* open)(matter_ctrl_t * const p_ctrl, rm_matter_app_cfg_t const * const p_cfg);

    /*******************************************************************************************************************//**
     * Closes the Matter service.
     *
     * @param[in]  p_ctrl               Pointer to Matter instance control structure.
     *
     * @retval FSP_SUCCESS              Matter successfully configured.
     * @retval FSP_ERR_ASSERTION        Null pointer, or one or more configuration options is invalid.
     * @retval FSP_ERR_ALREADY_OPEN     Module is already open.  This module can only be opened once.
     **********************************************************************************************************************/
    fsp_err_t (* close)(matter_ctrl_t * const p_ctrl);
} matter_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_rm_matter_app_instance
{
    matter_ctrl_t             * p_ctrl; ///< Pointer to the control structure for this instance
    rm_matter_app_cfg_t const * p_cfg;  ///< Pointer to the configuration structure for this instance
    matter_api_t const        * p_api;  ///< Pointer to the API structure for this instance
} rm_matter_app_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************//**
 * @} (end defgroup MATTER_API)
 **********************************************************************************************************************/
