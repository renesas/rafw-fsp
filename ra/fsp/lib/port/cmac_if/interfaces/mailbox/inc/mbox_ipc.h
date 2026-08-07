/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup MID_SYS_IPC
 * \{
 * \addtogroup MAILBOX IPC Mailbox
 * \brief Mailbox data/API
 * \{
 */

/**
 **************************************************************************************
 **
 *
 * @file mbox_ipc.h
 *
 * @brief Mailbox handling for Generic buffer data.
 *
 **************************************************************************************
 **
 */

#ifndef MBOX_IPC_H_
 #define MBOX_IPC_H_

 #ifdef __cplusplus
extern "C"
{
 #endif

/// \cond
 #include "sdk_defs.h"
 #include <stdint.h>

/// \endcond

/** \addtogroup MAILBOX_CONFIG Config data
 *  \brief Mailbox generic config data
 *  @{
 */

/**
 * The size of MBOX buffer
 */
 #ifndef MBOX_BUFFER_SIZE
  #define MBOX_BUFFER_SIZE    (1032)
 #endif

/**
 * Core mailboxes
 */
 #ifndef MBOX_NO
  #define MBOX_NO             (2)
 #endif

DAF_STRUCT mbox_channel_core {
    DAF_FIELD(uint16_t, magic);
    DAF_FIELD(uint16_t, status);
    DAF_FIELD(uint16_t, pos_write);
    DAF_FIELD(uint16_t, pos_read);
    DAF_FIELD(uint8_t, data[MBOX_BUFFER_SIZE]);
};

/** @}*/

/** \addtogroup MAILBOX_PUBLIC_FUNCTIONS Mailbox API
 *  \brief Mailbox public API
 *  @{
 */

/**
 * \brief Initializes the IPC/Mailbox channel.
 *
 * \returns The initialization status: ::IPC_SUCCESS is case of successful initialization.
 *
 * \note This function should be called on each processor before read/write operations.
 */
int16_t mbox_init(void);

/**
 * \brief Sends a user given buffer to the receiver processor over the dedicated IPC/Mailbox channel.
 *
 * \param [in] bufptr The user buffer to be written to the mailbox.
 * \param [in] size The user buffer length.
 * \param [in] callback The user function callback to be called when written to the mailbox FIFO.
 *
 * \returns ::0 is case the buffer is written successfully, 1 when the buffer gets pending.
 *
 * \note This function should be called by the writer/sender processor.
 *
 * \attention Check always the return value of this API. In case the write operation is pending,
 *            consecutive calls of this API should not be allowed
 *
 * \attention This API is not thread-safe. It should be used in serialized way or the upper-layer
 *            software should protect the callee in case of multiple threads trying to access the MBOX
 */
int16_t mbox_write(const uint8_t * bufptr, uint16_t size, void (* callback)(uint8_t));

/**
 * \brief Reads-in to a given user buffer on the receiver processor from the dedicated IPC/Mailbox channel.
 *
 * \param [in] bufptr The user buffer to be populated from the mailbox.
 * \param [in] size The user buffer length.
 * \param [in] callback The user function callback to be called when getting data from the mailbox FIFO.
 *
 * \note This function should be called by the reader/receiver processor.
 */
void mbox_read(uint8_t * bufptr, uint16_t size, void (* callback)(uint8_t));

/**
 * \brief Resets the Mailbox component.
 *
 * \note This function can be called only from the SYSCPU.
 */
void mbox_reset(void);

/** @}*/

 #ifdef __cplusplus
}
 #endif

#endif                                 /* MBOX_IPC_H_ */

/**
 * \}
 * \}
 */
