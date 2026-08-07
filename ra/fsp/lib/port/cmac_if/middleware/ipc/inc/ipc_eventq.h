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
 * @file ipc_eventq.h
 *
 * @brief This file contains the interface to the event queue of the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_EVENTQ_H_
#define IPC_EVENTQ_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "ipc_types.h"

/// \endcond

/****************************************************************************
 *                            Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS_EVENT_Q Events API
 *  \brief IPC public event functions
 *  @{
 */

/**
 * \brief Initializes the provided event queue
 *
 * \param [in] eventq Pointer to the \link ipc_eventq_t \endlink event queue
 * \param [in] size The size of the event queue (number of elements)
 *
 * \returns ::IPC_SUCCESS on successfull initialization
 */
int16_t ipc_eventq_init(ipc_eventq_t * eventq, int size);

/**
 * \brief Reads an event from the event queue
 *
 * \param [in] ipc Pointer to the \link ipc_t \endlink which holds the information about the event queue
 * \param [out] event Pointer where read the event will be written
 *
 * \returns 1 if the event has been read successfully
 */
int16_t ipc_eventq_read(ipc_t * ipc, ipc_event_type * event);

/**
 * \brief Writes an event to the event queue
 *
 * \param [in] ipc Pointer to the \link ipc_t \endlink which holds the information about the event queue
 * \param [in] event Pointer to the event which will be added in the queue
 *
 * \returns 1 if the event has been added successfully
 */
int16_t ipc_eventq_write(ipc_t * ipc, const ipc_event_type * event);

/**
 * \brief Gets the number of elements in the event queue
 *
 * \param [in] eventq Pointer to the \link ipc_eventq_t \endlink event queue
 *
 * \returns The number of elements in the event queue
 */
int16_t ipc_eventq_items_count(ipc_eventq_t * eventq);

/** @}*/

#endif                                 /* IPC_EVENTQ_H_ */

/**
 * \}
 * \}
 */
