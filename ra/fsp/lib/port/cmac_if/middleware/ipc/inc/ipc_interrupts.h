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
 * @file ipc_interrupts.h
 *
 * @brief This file contains the interrupt handling for the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_INTERRUPTS_H_
#define IPC_INTERRUPTS_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include "ipc_types.h"

/// \endcond

/****************************************************************************
 *                            Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS_IRQ Interrupts
 *  \brief IPC interrupt routines
 *  @{
 */

/// @cond (DA1487X_lbl)

/**
 * \brief Sets CMAC2SYS interrupt request
 */
void ipc_cmac2sys_interrupt_set(void);

/**
 * \brief Sets SYS2CMAC interrupt request
 */
void ipc_sys2cmac_interrupt_set(void);

/**
 * \brief Clears CMAC2SYS interrupt request
 */
void ipc_cmac2sys_interrupt_clear(void);

/**
 * \brief Clears SYS2CMAC interrupt request
 */
void ipc_sys2cmac_interrupt_clear(void);

#if BSP_FEATURE_HAS_DSP

/**
 * \brief Sets DSP2CMAC interrupt request
 */
void ipc_dsp2cmac_interrupt_set(void);

/// @endcond

/**
 * \brief Sets SYS2DSP interrupt request
 */
void ipc_sys2dsp_interrupt_set(void);

/// @cond (DA1487X_lbl)

/**
 * \brief Sets CMAC2DSP interrupt request
 */
void ipc_cmac2dsp_interrupt_set(void);

/**
 * \brief Sets DSP2SYS interrupt request
 */
void ipc_dsp2sys_interrupt_set(void);

/// @endcond

/**
 * \brief Clears DSP2SYS interrupt request
 */
void ipc_dsp2sys_interrupt_clear(void);

/// @cond (DA1487X_lbl)

/**
 * \brief Clears DSP2CMAC interrupt request
 */
void ipc_dsp2cmac_interrupt_clear(void);

/// @endcond

/**
 * \brief Clears SYS2DSP interrupt request
 */
void ipc_sys2dsp_interrupt_clear(void);

/// @cond (DA1487X_lbl)

/**
 * \brief Clears CMAC2DSP interrupt request
 */
void ipc_cmac2dsp_interrupt_clear(void);

#endif                                 /* BSP_FEATURE_HAS_DSP */

/// @endcond

/** @}*/

#endif                                 /* IPC_INTERRUPTS_H_ */

/**
 * \}
 * \}
 */
