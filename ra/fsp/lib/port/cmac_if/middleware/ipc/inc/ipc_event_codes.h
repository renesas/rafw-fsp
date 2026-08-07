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
 * @file ipc_event_codes.h
 *
 * @brief This file contains the interface (APIs) and datatypes of the event codes
 *        for the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_EVENT_CODES_H_
#define IPC_EVENT_CODES_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "ipc_types.h"

/// \endcond

/** \addtogroup IPC_DATA_TYPES Data types
 *  @{
 */

/****************************************************************************
 *                               Enumerations
 ****************************************************************************/

/**
 * \enum ipc_event_codes_common
 * \brief Enumeration which holds the common event codes to all the system cores
 */
enum ipc_event_codes_common
{
    IPC_EVENT_CODE_DEFAULT = 0,        /*!< Default event code. Please DO NOT utilize this event code */
    IPC_EVENT_CODE_POWER_MANAGER,      /*!< Power manager event code */
    IPC_EVENT_CODE_SYS_INIT,           /*!< System initialization event code */
    IPC_EVENT_CODE_NOTIFY_PLATFORM,    /*!< Notification event code*/
    IPC_EVENT_CODE_PDS,                /*!< The event code used for the PDS notifications */
    IPC_EVENT_CODE_COMMON_RESERVED_MAX /*!< Maximum number of the reserved event codes */
};

#if defined(CORTEX_M33) || defined(CMAC_CPU)

/**
 * \enum ipc_event_codes_syscpu_cmac
 * \brief Enumeration of the event codes common for SYSCPU and CMAC
 */
enum ipc_event_codes_syscpu_cmac
{
    IPC_EVENT_CODE_PROTOCOL_DATA = IPC_EVENT_CODE_COMMON_RESERVED_MAX, /*!< Crypto protocol data event code */
    IPC_EVENT_CODE_MBOX_NORMAL,                                        /*!< Mailbox normal event code */
    IPC_EVENT_CODE_MBOX_READY,                                         /*!< Mailbox ready event code */
    IPC_EVENT_CODE_HCI_SHIM,                                           /*!< HCI shim event code */
    IPC_EVENT_CODE_TWS_AUDIO,                                          /*!< TWS Audio event code */
    IPC_EVENT_CODE_SYSCPU_CMAC_RESERVED_MAX                            /*!< Maximum number of the reserved event codes */
};

#endif /* CORTEX_M33 || CMAC_CPU */

#if BSP_FEATURE_HAS_DSP

/**
 * \enum ipc_event_codes_dsp_sys
 * \brief Enumeration which holds the event codes used by DSP to sent IPC to SysCpu
 */
enum ipc_event_codes_dsp_sys
{
    IPC_EVENT_CODE_DSP_EXCEPTION = IPC_EVENT_CODE_COMMON_RESERVED_MAX, /*!< DSP Hardfault/exception */
    IPC_EVENT_CODE_DSP_ASSERT,                                         /*!< DSP Assert */
    IPC_EVENT_CODE_DSP_RESERVED_MAX                                    /*!< Maximum number of the reserved event codes */
};

#endif /* BSP_FEATURE_HAS_DSP */

/** @}*/

#endif                                 /* IPC_EVENT_CODES_H_ */

/**
 * \}
 * \}
 */
