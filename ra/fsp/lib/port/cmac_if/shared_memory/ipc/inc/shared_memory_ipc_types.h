/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup SHARED_MEM
 \{
 * \addtogroup SHARED_MEM_IPC
 * \brief IPC common data structures
 \{
 */

/**
 **************************************************************************************
 **
 *
 * @file shared_memory_ipc_types.h
 *
 * @brief This file defines the layout of the SHARED_RAM_IPC memory section
 *
 **************************************************************************************
 **
 */
#ifndef SHARED_MEMORY_IPC_TYPES_H
#define SHARED_MEMORY_IPC_TYPES_H

/****************************************************************************
 *          Include files
 ****************************************************************************/

/// \cond
#if BSP_FEATURE_CODE_READY

/*
 * Include files
 */
 #include "sys_op_mode.h"
#endif

/*
 * IPC include files
 */
#include "ipc_config.h"
#include "ipc_types.h"

/*
 * MBox include files
 */
#include "mbox_ipc.h"

#if BSP_FEATURE_CODE_READY

/*
 * Crypto include files
 */
 #include "crypto_protocol.h"
 #include "crypto_queue.h"

/*
 * RNG include files
 */
 #include "shared_memory_rng_types.h"
#endif

#if BSP_CFG_POWER_MGR_USED

/*
 * Power manager
 */
 #include "power_mgr_types.h"

/*
 * TWS Audio
 */
 #include "shared_memory_tws_audio_types.h"

#endif

/// \endcond

#if defined(PAS_APD_INCLUDED) && (APD_EXPORT == 1)
 #include "shared_memory_pas_types.h"

/**
 * The default size of the APD debug queue
 */
 #ifndef APD_DEBUG_QUEUE_SIZE
  #define APD_DEBUG_QUEUE_SIZE    (2048)
 #endif
#endif

/****************************************************************************
 *          Structures/Enumerations
 ****************************************************************************/

#if defined(PAS_APD_INCLUDED) && (APD_EXPORT == 1)

/**
 * \struct audio_debug_queue_t
 * \brief The data layout of the audio debug queue buffer
 */
typedef DAF_STRUCT {
    PAS_packet_queue_h owner;                       // owner queue lock
    DAF_FIELD(uint16_t, length);                    // The length of the FIFO portion of the queue
    DAF_FIELD(uint16_t, packet_count);              // balance counter between write and pull full packet
    DAF_FIELD(uint16_t, full_flag);                 // A flag indicating a full condition when head = tail
    DAF_FIELD(uint16_t, write_pos);                 // Next write position index in the debug queue FIFO
    DAF_FIELD(uint16_t, read_pos);                  // Next read position index in the debug queue FIFO
    DAF_FIELD(uint8_t, data[APD_DEBUG_QUEUE_SIZE]); // The debug queue data
} audio_debug_queue_t;
#endif

/**
 * \struct shared_ram_ipc_t
 * \brief The structure type definition to hold the variables residing in the
 * SHARED_RAM_IPC memory section
 */
typedef DAF_STRUCT SHARED_RAM_IPC_T
{
    /**
     * The IPC component for the available IPC channels
     */
    ipc_shared_t ipc;

    /**
     * Shared memory data interface used for the Mailbox
     */
    struct mbox_channel_core mboxes[MBOX_NO];

#if BSP_FEATURE_CODE_READY

    /**
     * Shared memory data interface used for the Crypto
     */
    crypto_queue_t crypto_mboxes[CRYPTO_MBOX_NO];

    /**
     * Shared memory data interface used for the CPU operation mode
     */
    DAF_ENUM32(sys_op_mode_t, syscpu_op_mode);
    DAF_ENUM32(sys_op_mode_t, cmaccpu_op_mode);

    /**
     * Shared memory data interface used for the RNG
     */
    rng_t rng;
 #if defined(PAS_APD_INCLUDED) && (APD_EXPORT == 1)

    /**
     * Shared memory data interface used for APD (audio packet debug)
     */
    audio_debug_queue_t apd_mbox;
 #endif

 #if BSP_CFG_POWER_MGR_USED

    /**
     * Shared memory for the current state of each CPU
     */
    pm_cpu_sleep_mode_t pm_cpu_state;
  #if BSP_FEATURE_HAS_DSP

    /**
     * Shared memory data interface used for the TWS audio
     */
    tws_audio_shared_memory_t tws_audio;
  #endif
 #endif
#endif
} shared_ram_ipc_t;

#endif                                 /* SHARED_MEMORY_IPC_TYPES_H */

/**
 * \}
 * \}
 */
