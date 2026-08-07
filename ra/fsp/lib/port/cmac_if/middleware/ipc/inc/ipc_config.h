/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup MID_SYS_IPC
 * \{
 * \addtogroup IPC
 * \{
 */

/**
 **************************************************************************************
 **
 *
 * @file ipc_config.h
 *
 * @brief This file contains the configuration fields for the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_CONFIG_H_
#define IPC_CONFIG_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/** \addtogroup IPC_CONFIG Configuration
 *  \brief IPC configuration ranges, locations
 *  @{
 */

/**
 * Number of event handler for SYS2CMAC that can be utilized by the application
 */
#define IPC_SYS2CMAC_HANDLERS_MAX     16

/**
 * Number of event handler for CMAC2SYS that can be utilized by the application
 */
#define IPC_CMAC2SYS_HANDLERS_MAX     16

#if BSP_FEATURE_HAS_DSP

/**
 * Number of event handler for SYS2DSP that can be utilized by the application
 */
 #define IPC_SYS2DSP_HANDLERS_MAX     16

/**
 * Number of event handler for CMAC2DSP that can be utilized by the application
 */
 #define IPC_CMAC2DSP_HANDLERS_MAX    16

/**
 * Number of event handler for DSP2SYS that can be utilized by the application
 */
 #define IPC_DSP2SYS_HANDLERS_MAX     16

/**
 * Number of event handler for DSP2CMAC that can be utilized by the application
 */
 #define IPC_DSP2CMAC_HANDLERS_MAX    16

#endif                                 /* BSP_FEATURE_HAS_DSP */

/**
 * The number of event codes for each IPC channel
 * \note This derived from the ::IPC_EVENT_CODE_MASK number of bits (2^4)
 */
#define IPC_EVENT_CODES_MAX           16

/**
 * Check the parameters for the handlers
 */
#if (IPC_SYS2CMAC_HANDLERS_MAX > IPC_EVENT_CODES_MAX) || (IPC_CMAC2SYS_HANDLERS_MAX > IPC_EVENT_CODES_MAX) || \
    (IPC_SYS2DSP_HANDLERS_MAX > IPC_EVENT_CODES_MAX) || (IPC_CMAC2DSP_HANDLERS_MAX > IPC_EVENT_CODES_MAX) ||  \
    (IPC_DSP2SYS_HANDLERS_MAX > IPC_EVENT_CODES_MAX) || (IPC_DSP2CMAC_HANDLERS_MAX > IPC_EVENT_CODES_MAX)
 #error "The IPC handlers (per IPC channel) cannot exceed the supported number of event codes"
#endif

/** @}*/

#endif                                 /* IPC_CONFIG_H_ */

/**
 * \}
 * \}
 */
