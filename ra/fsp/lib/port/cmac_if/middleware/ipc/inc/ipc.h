/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup MID_SYS_IPC
 * \{
 * \addtogroup IPC IPC interface
 * \brief IPC data/API
 * \{
 */

/**
 **************************************************************************************
 **
 *
 * @file ipc.h
 *
 * @brief This file provides high-level APIs to the Inter-Processor Communication Interface (IPC).
 *
 **************************************************************************************
 **
 */

#ifndef IPC_H_
#define IPC_H_

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <stdint.h>
#include "ipc_config.h"
#include "ipc_types.h"
#include "ipc_helper.h"
#include "ipc_functions.h"

/// \endcond

/*! \page ipc-page IPC channels - introduction
 * IPC is a software component which utilizes the shared memory and the mailbox interrupts to provide a mechanism for inter-processor communication. The following table describes the different IPC channels that are available:
 * <table>
 * <caption id="ipc-channels-sections">IPC channels</caption>
 * <tr><th>IPC channel     <th>Sender processor                    <th>Receiver processor
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_SYS2CMAC      <td>ARM Cortex-M33          <td>ARM Cortex-M0+
 * \endif
 * <tr><td>::IPC_CHANNEL_SYS2DSP       <td>ARM Cortex-M33          <td>Fusion-1
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_CMAC2SYS      <td>ARM Cortex-M0+          <td>ARM Cortex-M33
 * <tr><td>::IPC_CHANNEL_CMAC2DSP      <td>ARM Cortex-M0+          <td>Fusion-1
 * \endif
 * <tr><td>::IPC_CHANNEL_DSP2SYS       <td>Fusion-1                <td>ARM Cortex-M33
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_DSP2CMAC      <td>Fusion-1                <td>ARM Cortex-M0+
 * \endif
 * </table>
 * \par Initialization
 * Each IPC channel should be initialized by the receiving processor using the low-level functions or the high-level APIs. In case of the low-level functions, the respective initialization function for the IPC channel shall be used. For example:
 * \if DA1487X_lbl
 * ```
 * ipc_cmac2sys_init(&my_callback_function);
 * ```
 * \else
 * ```
 * ipc_dsp2sys_init(&my_callback_function);
 * ```
 * \endif
 * In case of the high-level API, the initialization IPC channel is given as parameter:
 * \if DA1487X_lbl
 * ```
 * ipc_initialize(IPC_CHANNEL_CMAC2SYS, &my_callback_function);
 * ```
 * \else
 * ```
 * ipc_initialize(IPC_CHANNEL_DSP2SYS, &my_callback_function);
 * ```
 * \endif
 * \par Interrupt handling
 * \if DA1487X_lbl
 * For the ::IPC_CHANNEL_CMAC2SYS, ::IPC_CHANNEL_DSP2SYS and ::IPC_CHANNEL_SYS2CMAC IPC channels, the respective interrupt service routine (ISR) is included in the provided software. This not the case for the ::IPC_CHANNEL_DSP2CMAC which must be defined in the application since the IRQ line ( \b SYSPER_Handler) is shared with other peripherals. Regarding the DSP core, the interrupt vectors are user configurable and the application should use the \b ARM_M33_IRQ IRQ line for ::IPC_CHANNEL_SYS2DSP and the \b CMAC_IRQ for ::IPC_CHANNEL_CMAC2DSP. The following table summarizes the IRQ lines for each IPC channel:
 * \else
 * For the ::IPC_CHANNEL_DSP2SYS IPC channels, the respective interrupt service routine (ISR) is included in the provided software. Regarding the DSP core, the interrupt vectors are user configurable and the application should use the \b ARM_M33_IRQ IRQ line for ::IPC_CHANNEL_SYS2DSP. The following table summarizes the IRQ lines for each IPC channel:
 * \endif
 * <table>
 * <caption id="ipc-irqs-sections">IRQ lines for the IPC channels</caption>
 * <tr><th>IPC channel                 <th>IRQ line at receiver
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_SYS2CMAC      <td>SYS2CMAC_IRQn
 * \endif
 * <tr><td>::IPC_CHANNEL_SYS2DSP       <td>ARM_M33_IRQ
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_CMAC2SYS      <td>CMAC2SYS_IRQ
 * <tr><td>::IPC_CHANNEL_CMAC2DSP      <td>CMAC_IRQ
 * \endif
 * <tr><td>::IPC_CHANNEL_DSP2SYS       <td>DSP_IRQ
 * \if DA1487X_lbl
 * <tr><td>::IPC_CHANNEL_DSP2CMAC      <td>SYSPER_IRQn
 * \endif
 * </table>
 *
 * \par Build-time configuration
 * IPC requires the configuration of the event buffers size and the maximum number of handlers that can be registered using the high-level API.\n
 * Moreover, the linker script must include specific memory section describing the shared memory layout.\n
 * The IPC component uses the information of the linker symbols to place the IPC event channels and the IPC data.\n\n
 * An example of the linker script is given below:
 * ```
 * MEMORY
 * {
 *        . . .
 *        SHARED(rwx)     : ORIGIN = 0x60100000,    LENGTH = 256K
 *        . . .
 * }
 * . . .
 * .shared (NOLOAD) :
 * {
 *        __shared_start__ = .;
 *
 *        . = ALIGN(4);
 *        __shared_space_start__ = .;
 *        KEEP(*(ipc_shared_eventq_zi))
 *        KEEP(*(ipc_shared_mbox_zi))
 *        __shared_space_end__ = .;
 *
 *        . = ALIGN(4);
 *        __shared_end__ = .;
 * } > SHARED
 * ```
 * \note The origin of the shared memory may differ per CPU. For example the origin for SYSCPU could be at 0x60100000 while for the DSP processor at 0x61100000
 *
 * \par IPC usage
 *
 * **Using the low-level APIs**
 * 1. Initialize the IPC channel in the receiving processor (example)
 * \if DA1487X_lbl
 * ```
 * ipc_cmac2sys_init(&my_callback_function);
 * ```
 * \else
 * ```
 * ipc_dsp2sys_init(&my_callback_function);
 * ```
 * \endif
 * \if DA1487X_lbl
 * Where the \b my_callback_function is the function that will receive the event from CMAC
 * \else
 * Where the \b my_callback_function is the function that will receive the event from DSP
 * \endif
 * 2. Send an event from (example)
 * \attention Please check the \link ipc_event_codes_common \endlink enumeration for the reserved event codes values. It is strongly suggested not to use the reserved event codes.
 *
 * \if DA1487X_lbl
 * ```
 * ipc_event_code application_event_code = 0x0A;
 * ipc_event_payload event_payload = 0x01;
 * ipc_event_type event = IPC_CREATE_EVENT(application_event_code, event_payload);
 * int16_t status = ipc_cmac2sys_write(event);
 * if (status == IPC_SUCCESS) {
 *        printf("CMAC2SYS event sent successfully\n");
 * } else if (status == IPC_ERROR_EVENTQ_FULL) {
 *        printf("Event queue is full for CMAC2SYS\n");
 * } else {
 *        printf("IPC channel (CMAC2SYS) is not initialized\n");
 * }
 * ```
 * \else
 * ```
 * ipc_event_code application_event_code = 0x0A;
 * ipc_event_payload event_payload = 0x01;
 * ipc_event_type event = IPC_CREATE_EVENT(application_event_code, event_payload);
 * int16_t status = ipc_dsp2sys_write(event);
 * if (status == IPC_SUCCESS) {
 *        printf("DSP2SYS event sent successfully\n");
 * } else if (status == IPC_ERROR_EVENTQ_FULL) {
 *        printf("Event queue is full for DSP2SYS\n");
 * } else {
 *        printf("IPC channel (DSP2SYS) is not initialized\n");
 * }
 * ```
 * \endif
 * 3. Use the event on the receiving processor
 *
 * **Using the high-level APIs**
 * 1. Initialize the IPC channel in the receiving processor (example)
 * \if DA1487X_lbl
 * ```
 * ipc_initialize(IPC_CHANNEL_CMAC2SYS, &my_callback_function);
 * ```
 * \else
 * ```
 * ipc_initialize(IPC_CHANNEL_DSP2SYS, &my_callback_function);
 * ```
 * \endif
 * \if DA1487X_lbl
 * Where the \b my_callback_function is the function that will receive the event code and the payload from CMAC
 * \else
 * Where the \b my_callback_function is the function that will receive the event code and the payload from DSP
 * \endif
 * 2. [Optional] Register additional handler per event code
 * \attention Please check the \link ipc_event_codes_common \endlink enumeration for the reserved event codes values. It is strongly suggested not to use the reserved event codes.
 *
 * \if DA1487X_lbl
 * ```
 * ipc_event_code application_event_code = 0x0A;
 * ipc_register_handler(IPC_CHANNEL_CMAC2SYS, application_event_code, &my_callback_function_event_code_A);
 * ```
 * \else
 * ```
 * ipc_event_code application_event_code = 0x0A;
 * ipc_register_handler(IPC_CHANNEL_DSP2SYS, application_event_code, &my_callback_function_event_code_A);
 * ```
 * \endif
 * Where the \b my_callback_function_event_code_A is the function that will receive the event sent with event code 0x0A
 * 3. Send an event (example)
 * \if DA1487X_lbl
 * ```
 * uint8_t *some_buffer_in_shared_memory_ptr;
 * // The event code to be used
 * ipc_event_code application_event_code = 0x0A;
 * // In this case, the payload is a memory address (calculated as an offset) of a buffer located in shared memory
 * ipc_event_payload payload = GET_SHARED_RAM_OFFSET((uint32_t)some_buffer_in_shared_memory_ptr);
 * int16_t status = ipc_write_data(IPC_CHANNEL_CMAC2SYS, application_event_code, payload);
 * if (status == IPC_SUCCESS) {
 *        printf("CMAC2SYS event sent successfully\n");
 * } else if (status == IPC_ERROR_EVENTQ_FULL) {
 *        printf("Event queue is full for CMAC2SYS\n");
 * } else if (status == IPC_ERROR_UNINITIALIZED) {
 *        printf("IPC channel (CMAC2SYS) is not initialized\n");
 * } else {
 *        printf("The provided IPC parameter is incorrect\n");
 * }
 * ```
 * \else
 * ```
 * uint8_t *some_buffer_in_shared_memory_ptr;
 * // The event code to be used
 * ipc_event_code application_event_code = 0xA;
 * // In this case, the payload is a memory address (calculated as an offset) of a buffer located in shared memory
 * ipc_event_payload payload = GET_SHARED_RAM_OFFSET((uint32_t)some_buffer_in_shared_memory_ptr);
 * int16_t status = ipc_write_data(IPC_CHANNEL_DSP2SYS, application_event_code, payload);
 * if (status == IPC_SUCCESS) {
 *        printf("DSP2SYS event sent successfully\n");
 * } else if (status == IPC_ERROR_EVENTQ_FULL) {
 *        printf("Event queue is full for DSP2SYS\n");
 * } else if (status == IPC_ERROR_UNINITIALIZED) {
 *        printf("IPC channel (DSP2SYS) is not initialized\n");
 * } else {
 *        printf("The provided IPC parameter is incorrect\n");
 * }
 * ```
 * \endif
 * 4. Use the event on the receiving processor: On the receiving processor, the handler registered for event code 0x0A will be called with parameters the event code and the payload
 *
 * \par RTOS Support
 * Specific IPC channels can be configured to route the received IPC event to RTOS tasks. The supported channels are: \n
 * ::IPC_CHANNEL_DSP2SYS \n
 * \if DA1487X_lbl
 * ::IPC_CHANNEL_CMAC2SYS \n
 * ::IPC_CHANNEL_DSP2CMAC \n
 * ::IPC_CHANNEL_SYS2CMAC \n
 * \endif
 * In this case, the application should initialize the IPC SW component for RTOS support and register the task(s) to be notified:
 * ```
 * int16_t os_ipc_initialize(ipc_channel_t ipc_channel);
 * int16_t os_ipc_register_task(ipc_channel_t ipc_channel, int index, OS_TASK task_handler);
 * ```
 * The related header with API, osal_ipc.h, can be found in the platform/dlg-osal folder.
 * \note \link os_ipc_initialize \endlink registers the OS handler at position 0 of the IPC high-level handlers. This means that IPC events with \b event \b code \b 0 will be routed to OS handling functionality.\n
 * In case the application wants to use also the IPC high-level functions outside of OS context, the registered callback functions should use event code greater than \b 0.
 */

/** \addtogroup IPC_PUBLIC_FUNCTIONS_HL High level
 *  \brief IPC high level public functions
 *  @{
 */

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/**
 * The default event code where the event handler is installed during initialization
 */
#define DEFAULT_EVENT_CODE    0

/****************************************************************************
 *                            Function prototypes
 ****************************************************************************/

/**
 * \brief Initializes the given IPC channel
 *
 * \param [in] ipc_channel The IPC channel to initialize. See \link ipc_channel_t \endlink
 * \param [in] event_handler The event handler to be called when an event is received. The event handler is installed for \link DEFAULT_EVENT_CODE \endlink event code.
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization
 *
 *  \note This functions should be called by the receiver processor
 */
int16_t ipc_initialize(ipc_channel_t ipc_channel, ipc_event_handler_t event_handler);

/**
 * \brief Writes an event to the IPC channel providing the event code and the payload.\n
 * The event code identifies the event handler that will be used to handle the event while the payload is an application data (with length of 28 bits) received by the registered callback function.
 *
 * \param [in] ipc_channel The IPC channel to write the event. See \link ipc_channel_t \endlink
 * \param [in] event_code The event code which identifies the handler in the receiving processor
 * \param [in] event_payload The data payload (max 18 bits)
 *
 * \returns ::IPC_SUCCESS is case the event is written successfully. See also\n
 * ::IPC_ERROR_EVENTQ_FULL\n
 * ::IPC_ERROR_UNINITIALIZED\n
 * ::IPC_ERROR_PARAMETER\n
 *
 *  \note This functions should be called by the sender processor. \p event_code and \p event_payload are combined in a single \link ipc_event_type \endlink
 */
int16_t ipc_write_data(ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_payload event_payload);

/**
 * \brief Registers a handler to the IPC channel.\n
 *
 * \param [in] ipc_channel The IPC channel for which the handler will be registered. See \link ipc_channel_t \endlink
 * \param [in] event_code The event code to register the handler
 * \param [in] event_handler The event handler
 *
 * \returns Returns the event code on successful registration. Otherwise it returns ::IPC_ERROR_PARAMETER
 *
 *  \note This functions should be called by the receiving processor
 */
int16_t ipc_register_handler(ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_handler_t event_handler);

/** @}*/

#endif                                 /* IPC_H_ */

/**
 * \}
 * \}
 */
