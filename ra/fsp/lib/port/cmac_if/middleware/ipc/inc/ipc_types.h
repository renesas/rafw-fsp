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
 * @file ipc_types.h
 *
 * @brief This file contains the different data types, structures and enumerations used by the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_TYPES_H_
#define IPC_TYPES_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "sdk_defs.h"
#include "ipc_config.h"

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/** \addtogroup IPC_DATA Error codes
 *  \brief IPC error codes, helper macros
 *  @{
 */

/**
 * IPC operation successful
 */
#define IPC_SUCCESS                       0

/**
 * The referenced IPC channel is enabled
 */
#define IPC_ENABLED                       -1

/**
 * The referenced IPC channel is disabled
 */
#define IPC_DISABLED                      -2

/**
 * Unkown error occured during the IPC operation
 */
#define IPC_ERROR_UNKNOWN                 -3

/**
 * The sender is not ready
 */
#define IPC_ERROR_SENDER_NOT_READY        -4

/**
 * The receiver is not ready
 */
#define IPC_ERROR_RECEIVER_NOT_READY      -5

/**
 * The IPC channel is not initialized
 */
#define IPC_ERROR_UNINITIALIZED           -6

/**
 * The event queue, for the referenced IPC channel, is full
 */
#define IPC_ERROR_EVENTQ_FULL             -7

/**
 * The event queue, for the referenced IPC channel, is empty
 */
#define IPC_ERROR_EVENTQ_EMPTY            -8

/**
 * The provided parameter is wrong
 */
#define IPC_ERROR_PARAMETER               -9

/**
 * The following bits (mask/position) describe the event partitioning into event code and payload. \n
 * The partitioning is used by the IPC wrapper to provide a way to the application to execute specific handler per event. The payload is application-dependent.
 * <table>
 * <caption id="ipc-event-bits">IPC event</caption>
 * <tr><th>Bit position   <th>Bits (length)       <th>Description
 * <tr><td>31:28          <td>4                   <td>Identifies the handler to handle the event
 * <tr><td>27:0           <td>28                  <td>The event payload
 * </table>
 *
 * The bit position of the event code
 */
#define IPC_EVENT_CODE_BIT_POSITION       (28U)

/**
 * The bit mask for the event code
 */
#define IPC_EVENT_CODE_MASK               0x0F

/**
 * The bit position of the payload
 */
#define IPC_EVENT_PAYLOAD_BIT_POSITION    (0U)

/**
 * The bit mask for the payload
 */
#define IPC_EVENT_PAYLOAD_MASK            0x0FFFFFFF

/**
 *  The bit mask used get the memory offset from the payload
 */
#define SHARED_MEMORY_OFFSET_MASK         0x3FFFF

/**
 * Macro used to combine the \link ipc_event_code \endlink and \link ipc_event_payload \endlink into a single \link ipc_event_type \endlink
 */
#define IPC_CREATE_EVENT(event_code, event_payload)                        \
    (((event_code & IPC_EVENT_CODE_MASK) << IPC_EVENT_CODE_BIT_POSITION) | \
     (event_payload & IPC_EVENT_PAYLOAD_MASK))

/**
 * Macro used to extract the \link ipc_event_code \endlink from \link ipc_event_type \endlink
 */
#define IPC_EVENT_CODE_EXTRACT(event_type) \
    ((event_type >> IPC_EVENT_CODE_BIT_POSITION) & IPC_EVENT_CODE_MASK)

/**
 * Macro used to extract the \link ipc_event_payload \endlink from \link ipc_event_type \endlink
 */
#define IPC_EVENT_PAYLOAD_EXTRACT(event_type)    (event_type & IPC_EVENT_PAYLOAD_MASK)

/**
 * The size of the event queue for the IPC channels
 */
#define IPC_EVENTQ_SIZE    32

/** @}*/

/****************************************************************************
 *                     Enumerations/Type definitions/Structs
 ****************************************************************************/

/** \addtogroup IPC_DATA_TYPES Data types
 *  \brief IPC wide-data types
 *  @{
 */

/**
 * \brief Enumeration which describes the IPC channels
 */
typedef enum ipc_channel_enum
{
    IPC_CHANNEL_SYS2CMAC = 0,          /*!< SYS2CMAC IPC channel  */
    IPC_CHANNEL_CMAC2SYS,              /*!< CMAC2SYS IPC channel  */
#if BSP_FEATURE_HAS_DSP
    IPC_CHANNEL_SYS2DSP,               /*!< SYS2DSP IPC channel   */
    IPC_CHANNEL_CMAC2DSP,              /*!< CMAC2DSP IPC channel  */
    IPC_CHANNEL_DSP2SYS,               /*!< DSP2SYS IPC channel   */
    IPC_CHANNEL_DSP2CMAC,              /*!< DSP2CMAC IPC channel  */
#endif /* BSP_FEATURE_HAS_DSP */
    N_IPC_CHANNEL
} ipc_channel_t;

/**
 * \brief Enumeration with the possible error codes in the IPC status error field
 */
typedef enum ipc_status_error_enum
{
    IPC_STATUS_NO_ERROR = 0,           /*!< No error condition */
    IPC_STATUS_ERROR_CPU_ASSERT,       /*!< CPU blocked on software error */
    IPC_STATUS_ERROR_MAX = 0xF         /*!< A 4-bit field */
} ipc_status_error_t;

/**
 * \brief Event data type. This data-type will be used for the event queue as the element, sent from one processor to the other.
 */
typedef uint32_t ipc_event_type;

/**
 * \brief The data type used to identify the event handler to handle an IPC message
 */
typedef uint8_t ipc_event_code;

/**
 * \brief The data type for the event payload
 * \note \link ipc_event_payload \endlink and \link ipc_event_code \endlink are combined in a \link ipc_event_type \endlink. The usable bits of \link ipc_event_code \endlink are specified to 4 bits
 */
typedef uint32_t ipc_event_payload;

/**
 * \brief Type of IPC interrupt callback
 */
typedef void (* ipc_isr_cb_t)(void);

/**
 * \brief Type of IPC event callback. This is used as a callback function from the ISR
 */
typedef void (* ipc_event_cb_t)(ipc_event_type);

/**
 * \brief Type of IPC callback. This is used as an upper-layer callback function that could be called from the event callback to include additional parameters
 */
typedef void (* ipc_event_handler_t)(ipc_event_code, ipc_event_payload);

/** \addtogroup IPC_STATUS Status types
 *  \brief IPC status types
 *  @{
 */

/**
 * \brief This union describes the status of an IPC channel
 */
typedef DAF_UNION {
    /**
     * \brief Status bits
     */
    DAF_STRUCT ipc_status_bits {
        uint32_t ready       : 1;      /*!< Ready bit. Indicates that the IPC channel is initialized and ready to receive events       */
        uint32_t enabled     : 1;      /*!< Enable bit. Indicates that the IPC channel is enabled              */
        uint32_t eventq_full : 1;      /*!< The event queue is full                  */
        uint32_t error       : 4;      /*!< Error information shared between the two processors           */
    } bits;                            /*!< Structure for the status bits             */

    /**
     * \brief Status raw value
     */
    DAF_FIELD(uint32_t, value);
} ipc_status_t;

/** @}*/

/**
 * \brief Describes the event queue
 */
typedef DAF_STRUCT ipc_eventq_struct {
    DAF_FIELD(ipc_event_type, buffer[IPC_EVENTQ_SIZE]); /*!< Event queue buffer                          */
    DAF_FIELD(int16_t, head);                           /*!< The head in the queue                       */
    DAF_FIELD(int16_t, tail);                           /*!< The tail in the queue                       */
    DAF_FIELD(int16_t, size);                           /*!< The size (elements) of the queue            */
    DAF_FIELD(int16_t, items_count);                    /*!< The current number of elements in the queue */
    DAF_FIELD(volatile uint32_t, lock);                 /*!< Lock variable for accessing the message queue */
} ipc_eventq_t;

/**
 * \brief This structure holds the data for a single IPC channel
 */
typedef DAF_STRUCT {
    DAF_FIELD(volatile ipc_status_t, status); /*!< The status of the IPC channel               */
    ipc_eventq_t eventq;                      /*!< The event queue of the IPC channel          */
    DAF_FIELD(ipc_event_type, event);         /*!< The event sent/received                     */
    DAF_FIELD(ipc_event_cb_t, irq_callback);  /*!< The callback function for the IPC channel   */
} ipc_t;

/**
 * \brief The structure describes the layout of the IPC channels
 */
typedef DAF_STRUCT ipc_shared {
    ipc_t sys2cmac;                    /*!< The SYS2CMAC IPC channel                   */
    ipc_t cmac2sys;                    /*!< The CMAC2SYS IPC channel                   */
#if BSP_FEATURE_HAS_DSP
    ipc_t sys2dsp;                     /*!< The SYS2DSP IPC channel                    */
    ipc_t cmac2dsp;                    /*!< The CMAC2DSP IPC channel                   */
    ipc_t dsp2sys;                     /*!< The DSP2SYS IPC channel                    */
    ipc_t dsp2cmac;                    /*!< The DSP2CMAC IPC channel                   */
#endif /* BSP_FEATURE_HAS_DSP */
} ipc_shared_t;

/** @}*/

#endif                                 /* IPC_TYPES_H_ */

/**
 * \}
 * \}
 */
