/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup SHARED_MEM_IPC
 \{
 */

/**
 **************************************************************************************
 **
 *
 * @file shared_memory_ipc.h
 *
 * @brief This file holds the variable declarations of the SHARED_RAM_IPC memory structure
 *
 **************************************************************************************
 **
 */
#ifndef SHARED_MEMORY_IPC_H
#define SHARED_MEMORY_IPC_H

/****************************************************************************
 *          Include files
 ****************************************************************************/

/// \cond
#include "sdk_defs.h"
#include "shared_memory_ipc_types.h"
#if BSP_FEATURE_CODE_READY
 #include "pds_types.h"
#endif

/// \endcond

#define SHARED_BUFFER_SIZE    1024

/**
 * \brief Pointer to the shared ram data used for IPC.
 */
extern shared_ram_ipc_t * shared_ram_ipc_ptr;
extern uint8_t          * shared_data_ptr;

/**
 * Allocation of the PDS object
 */
#if BSP_FEATURE_CODE_READY
extern pds_t            pds;
extern pds_statistics_t pds_statistics;
extern uint32_t         pds_heap_data[];
#endif

#endif                                 /* SHARED_MEMORY_IPC_H */

/**
 * \}
 */
