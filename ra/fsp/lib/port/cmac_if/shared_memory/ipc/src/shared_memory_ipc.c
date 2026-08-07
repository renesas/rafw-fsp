/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file shared_memory_ipc.c
 *
 * @brief This file holds the variable definition of the SHARED_RAM_IPC memory structure
 *
 **************************************************************************************
 **
 */

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include "shared_memory_ipc.h"
#if BSP_FEATURE_CODE_READY
 #include "pds_config.h"
 #include "pds_types.h"
#endif

/// \endcond

/****************************************************************************
 *          Global variables placed in SHARED_RAM_IPC
 ****************************************************************************/

/**
 * Tha variable located in SHARED_RAM_CMAC memory section
 */
#if defined(CORTEX_M33)
shared_ram_ipc_t shared_ram_ipc;
uint8_t          shared_data[SHARED_BUFFER_SIZE];

shared_ram_ipc_t * shared_ram_ipc_ptr = &shared_ram_ipc;
uint8_t          * shared_data_ptr    = shared_data;
#endif

/**
 * Allocation of the PDS object
 */
#if BSP_FEATURE_CODE_READY
__PDS_SHARED pds_t                pds;
__PDS_STATISTICS pds_statistics_t pds_statistics;
__PDS_HEAP pds_heap_type          pds_heap_data[PDS_HEAP_SIZE];
#endif
