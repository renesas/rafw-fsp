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
 * @file ipc_functions.h
 *
 * @brief This file contains the low-level APIs of the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_FUNCTIONS_H_
#define IPC_FUNCTIONS_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "ipc_config.h"
#include "ipc_eventq.h"
#include "ipc_types.h"

/// \endcond

/****************************************************************************
 *                            Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS_LL Low level
 *  \brief IPC low level interface API
 *  @{
 */

#if defined(CORTEX_M33)

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_CMAC_TO_SYS IPC CMAC to SYS
 *  \brief IPC CMAC-SYS context API
 *  @{
 */

/**
 * \brief Initializes the CMAC2SYS IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2sys_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of CMAC2SYS IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2sys_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the CMAC2SYS IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2sys_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the CMAC2SYS IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2sys_enable(void);

/**
 * \brief Disables the CMAC2SYS IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2sys_disable(void);

/**
 * \brief Gets the number of events in the event queue for the CMAC2SYS IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_cmac2sys_count_pending(void);

/**
 * \brief Clear the memory alocated to the CMAC2SYS IPC channel
 */
void ipc_cmac2sys_clear(void);

/** @}*/

/// @endcond

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_SYS IPC DSP to SYS
 *  \brief IPC DSP-SYS context API
 *  @{
 */

/**
 * \brief Initializes the DSP2SYS IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2sys_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of DSP2SYS IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2sys_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the DSP2SYS IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2sys_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the DSP2SYS IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2sys_enable(void);

/**
 * \brief Disables the DSP2SYS IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2sys_disable(void);

/**
 * \brief Gets the number of events in the event queue for the DSP2SYS IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_dsp2sys_count_pending(void);

/**
 * \brief Clear the memory alocated to the DSP2SYS IPC channel
 */
void ipc_dsp2sys_clear(void);

/** @}*/

 #endif                                /* BSP_FEATURE_HAS_DSP */

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_SYS_TO_CMAC IPC SYS to CMAC
 *  \brief IPC SYS-CMAC context API
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the SYS2CMAC IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_sys2cmac_write(ipc_event_type event);

/**
 * \brief Clear the memory allocated to the SYS2CMAC IPC channel
 */
void ipc_sys2cmac_clear(void);

/** @}*/

/// @endcond

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_SYS_TO_DSP IPC SYS to DSP
 *  \brief IPC SYS-DSP context API
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the SYS2DSP IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_sys2dsp_write(ipc_event_type event);

/**
 * \brief Clear the memory alocated to the SYS2DSP IPC channel
 */
void ipc_sys2dsp_clear(void);

/** @}*/

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_CMAC_TO_DSP IPC CMAC to DSP
 *  \brief IPC CMAC-DSP context API
 *  @{
 */

/**
 * \brief Clear the memory allocated to the CMAC2DSP IPC channel
 *
 */
void ipc_cmac2dsp_clear(void);

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

/// @endcond

#elif defined(CMAC_CPU)

/** \addtogroup IPC_SYS_TO_CMAC IPC SYS to CMAC
 *  @{
 */

/**
 * \brief Initializes the SYS2CMAC IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2cmac_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of SYS2CMAC IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2cmac_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the SYS2CMAC IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2cmac_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the SYS2CMAC IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2cmac_enable(void);

/**
 * \brief Disables the SYS2CMAC IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2cmac_disable(void);

/**
 * \brief Gets the number of events in the event queue for SYS2CMAC IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_sys2cmac_count_pending(void);

/** @}*/

/** \addtogroup IPC_CMAC_TO_SYS IPC CMAC to SYS
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the CMAC2SYS IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_cmac2sys_write(ipc_event_type event);

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_CMAC_TO_DSP IPC CMAC to DSP
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the CMAC2DSP IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_cmac2dsp_write(ipc_event_type event);

/** @}*/

/** \addtogroup IPC_DSP_TO_CMAC IPC DSP to CMAC
 *  \brief CMAC context API
 *  @{
 */

/**
 * \brief Initializes the DSP2CMAC IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2cmac_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of DSP2CMAC IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2cmac_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the DSP2CMAC IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2cmac_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the DSP2CMAC IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2cmac_enable(void);

/**
 * \brief Disables the DSP2CMAC IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_dsp2cmac_disable(void);

/**
 * \brief Gets the number of events in the event queue for the DSP2CMAC IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_dsp2cmac_count_pending(void);

/**
 * \brief Clear the memory allocated to the  DSP2CMAC IPC channel
 */
void ipc_dsp2cmac_clear(void);

/**
 * \brief Gets the callback function of the DSP2CMAC IPC channel
 *
 * \returns The callback function (see \link ipc_isr_cb_t \endlink)
 *
 *  \note This functions should be called by the receiver processor. The interface to the callback function is provided since the DSP2CMAC IRQ is shared with other interrupt sources. Therefore, the configured interrupt should use this API to call the callback function of DSP2CMAC IPC channel
 */
ipc_isr_cb_t ipc_dsp2cmac_callback_get(void);

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

#else

/** \addtogroup IPC_SYS_TO_DSP IPC SYS to DSP
 *  @{
 */

/**
 * \brief Initializes the SYS2DSP IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2dsp_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of SYS2DSP IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2dsp_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the SYS2DSP IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2dsp_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the SYS2DSP IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2dsp_enable(void);

/**
 * \brief Disables the SYS2DSP IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_sys2dsp_disable(void);

/**
 * \brief Gets the number of events in the event queue for the SYS2DSP IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_sys2dsp_count_pending(void);

/**
 * \brief Gets the callback function of the SYS2DSP IPC channel
 *
 * \returns The ISR callback function (see \link ipc_isr_cb_t \endlink)
 *
 *  \note This functions should be called by the receiver processor. The interface to the callback function is provided since the interrupt vertor in the DSP processor is application configurable. Therefore, the configured interrupt should use this API to call the callback function of SYS2DSP IPC channel
 */
ipc_isr_cb_t ipc_sys2dsp_callback_get(void);

/** @}*/

/** \addtogroup IPC_CMAC_TO_DSP IPC CMAC to DSP
 *  @{
 */

/**
 * \brief Initializes the CMAC2DSP IPC channel
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2dsp_init(ipc_event_cb_t callback);

/**
 * \brief Gets the pending event from the event queue of CMAC2DSP IPC channel
 *
 * \param [in] event Pointer to the event where the read value will be placed
 *
 * \returns The read status: ::IPC_SUCCESS is case of successful read. See also\n
 * ::IPC_ERROR_EVENTQ_EMPTY
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2dsp_next_pending(ipc_event_type * event);

/**
 * \brief Sets the callback function for the CMAC2DSP IPC channel. The callback function replaces the one provided when the IPC channel was initialized.
 *
 * \param [in] callback The callback function to be called when the interrupt event is triggeres
 *
 * \returns ::IPC_SUCCESS is case of successful callback replacement. See also\n
 * ::IPC_ERROR_RECEIVER_NOT_READY
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2dsp_callback(ipc_event_cb_t callback);

/**
 * \brief Enabled the CMAC2DSP IPC channel
 *
 * \returns ::IPC_ENABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2dsp_enable(void);

/**
 * \brief Disables the CMAC2DSP IPC channel
 *
 * \returns ::IPC_DISABLED is case the IPC channel is enabled ::IPC_ERROR_UNINITIALIZED otherwise
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_cmac2dsp_disable(void);

/**
 * \brief Gets the number of events in the event queue for the CMAC2DSP IPC channel
 *
 * \returns The number of events or ::IPC_ERROR_UNINITIALIZED if the IPC channel is not initialized
 *
 *  \note This functions should be called by the receiver processor
 */
int ipc_cmac2dsp_count_pending(void);

/**
 * \brief Gets the callback function of the CAMC2DSP IPC channel
 *
 * \returns The callback function (see \link ipc_isr_cb_t \endlink)
 *
 *  \note This functions should be called by the receiver processor. The interface to the callback function is provided since the interrupt vertor in the DSP processor is application configurable. Therefore, the configured interrupt should use this API to call the callback function of CMAC2DSP IPC channel
 */
ipc_isr_cb_t ipc_cmac2dsp_callback_get(void);

/** @}*/

/** \addtogroup IPC_DSP_TO_SYS IPC DSP to SYS
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the DSP2SYS IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_dsp2sys_write(ipc_event_type event);

/** @}*/

/** \addtogroup IPC_DSP_TO_SYS IPC DSP to SYS
 *  @{
 */

/**
 * \brief Sends an event to the receiver processor for the DSP2CMAC IPC channel. This functions writes the provided event to the event queue and triggres the related IRQ
 *
 * \param [in] event The event to be written in the event queue
 *
 * \returns ::IPC_SUCCESS is case the event is sent successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL
 * ::IPC_ERROR_UNINITIALIZED
 *
 *  \note This functions should be called by the sender processor
 */
int16_t ipc_dsp2cmac_write(ipc_event_type event);

/** @}*/
#endif

#if defined(CMAC_CPU) || defined(CORTEX_M33)

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_SYS_TO_CMAC IPC SYS to CMAC
 *  @{
 */

/**
 * \brief Gets the status of the SYS2CMAC IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_sys2cmac_status(void);

/** @}*/

/// @endcond

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */

/**
 * \brief Gets the status of the CMAC2SYS IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_cmac2sys_status(void);

/** @}*/

/// @endcond

#endif

#if defined(CMAC_CPU) || !defined(CORTEX_M33)

 #if BSP_FEATURE_HAS_DSP

/// @cond (DA1487X_lbl)

/** \addtogroup IPC_DSP_TO_CMAC IPC DSP to CMAC
 *  \brief DSP-CMAC context API
 *  @{
 */

/**
 * \brief Gets the status of the DSP2CMAC IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_dsp2cmac_status(void);

/** @}*/

/** \addtogroup IPC_CMAC_TO_DSP IPC CMAC to DSP
 *  @{
 */

/**
 * \brief Gets the status of the CMAC2DSP IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_cmac2dsp_status(void);

/** @}*/

/// @endcond

 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

#if !defined(CMAC_CPU) || defined(CORTEX_M33)

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_SYS IPC DSP to SYS
 *  @{
 */

/**
 * \brief Gets the status of the DSP2SYS IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_dsp2sys_status(void);

/** @}*/

/** \addtogroup IPC_SYS_TO_DSP IPC SYS to DSP
 *  @{
 */

/**
 * \brief Gets the status of the SYS2DSP IPC channel
 *
 * \returns The IPC channel status \link ipc_status_t \endlink
 */
ipc_status_t ipc_sys2dsp_status(void);

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

/** @}*/

#endif                                 /* IPC_FUNCTIONS_H_ */

/**
 * \}
 * \}
 */
