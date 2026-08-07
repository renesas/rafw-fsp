/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file ipc_helper.c
 *
 * @brief This file contains some helper functions, as utilities, that can be used by the application. V0_4_0
 *
 **************************************************************************************
 **
 */

/**
 * \addtogroup MIDDLEWARE
 \{
 * \addtogroup IPC
 \{
 */

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "ipc_config.h"
#include "ipc_types.h"
#include "ipc_helper.h"

#include <sdk_defs.h>
#include "hw_sys.h"

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/**
 * Definition for the size (LSB) bits within the payload \link ipc_event_payload \endlink \n
 * When the payload is used to combine the memory address and the size:
 * - Bits 17:0 contain the address offset
 * - Bits 27:18 contain the size
 * - Bits 31:28  contain the event code \link ipc_event_code \endlink
 */
#define IPC_PAYLOAD_SIZE_BIT_INDEX    18

/**
 * Bit mask for the size bits (10 bits)
 */
#define IPC_PAYLOAD_SIZE_BIT_MASK     0x3FF

/****************************************************************************
 *                                Implementation
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS
 *  @{
 */

uint16_t ipc_calculate_memory_size (ipc_event_payload payload)
{
    return (uint16_t) ((payload >> IPC_PAYLOAD_SIZE_BIT_INDEX) & IPC_PAYLOAD_SIZE_BIT_MASK);
}

ipc_event_payload ipc_pack_payload (void * address, uint16_t size)
{
    return (ipc_event_payload) (GET_SHARED_RAM_OFFSET((uint32_t) address) |
                                ((size & IPC_PAYLOAD_SIZE_BIT_MASK) << IPC_PAYLOAD_SIZE_BIT_INDEX));
}

/** @}*/

/**
 \}
 \}
 */
