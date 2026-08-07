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
 * @file ipc_helper.h
 *
 * @brief This file contains the declaration of the helper functions.
 *
 **************************************************************************************
 **
 */

#ifndef IPC_HELPER_H_
#define IPC_HELPER_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include "ipc_types.h"

/// \endcond

/****************************************************************************
 *                            Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS_HELPER Helpers
 *  \brief IPC helper functions
 *  @{
 */

/**
 * \brief Helper function to calculate the memory size from the payload. The payload should hold the memory size (in bytes) located in bits 27:18
 *
 * \param [in] payload The event payload
 *
 * \returns The memory size
 */
uint16_t ipc_calculate_memory_size(ipc_event_payload payload);

/**
 * \brief Helper function that can be used to combine a memory address and the given size to a single \link ipc_event_payload \endlink
 *
 * \param [in] address Pointer to the shared memory address
 * \param [in] size The length (in bytes) of the provided memory section. The maximum size that is supported is 1KB
 *
 * \return The resulted \link ipc_event_payload \endlink
 */
ipc_event_payload ipc_pack_payload(void * address, uint16_t size);

/** @}*/

#endif                                 /* IPC_HELPER_H_ */

/**
 * \}
 * \}
 */
