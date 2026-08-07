/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup SHARED_MEM
 \{
 * \addtogroup SHARED_MEM_IPC
 \{
 */

/**
 **************************************************************************************
 **
 *
 * \file shared_memory_tws_audio_types.h
 *
 * \brief This file holds the data type definitions for the TWS audio protocol shared
 *        memory. It defines the variables placed in shared memory and used by the TWS
 *        audio protocol module.
 *
 **************************************************************************************
 **
 */

#ifndef SHARED_MEMORY_TWS_AUDIO_TYPES_H_
#define SHARED_MEMORY_TWS_AUDIO_TYPES_H_

#define TWS_AUDIO_BUFFER_SIZE    64                         //!< IPC TWS audio buffer size for inter-processor messages

/**
 * \brief IPC TWS audio structure handling inter-processor message buffers in shared RAM
 */
typedef DAF_STRUCT {
    DAF_FIELD(uint8_t, syscpu2cmac[TWS_AUDIO_BUFFER_SIZE]); /**< buffer for SYSCPU to CMAC TWS audio IPC messages */
    DAF_FIELD(uint8_t, cmac2syscpu[TWS_AUDIO_BUFFER_SIZE]); /**< buffer for CMAC to SYSCPU TWS audio IPC messages */
} tws_audio_shared_memory_t;

#endif /* SHARED_MEMORY_TWS_AUDIO_TYPES_H_ */

/**
 * \}
 * \}
 */
