/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file mbox_ipc.c
 *
 * @brief Mailbox handling for generic data. V_1_0
 *
 **************************************************************************************
 **
 */

#include <sdk_defs.h>
#include "hw_sys.h"
#include <string.h>

#include "ipc.h"
#include "ipc_types.h"
#include "ipc_functions.h"
#include "ipc_helper.h"
#include "ipc_interrupts.h"
#include "ipc_event_codes.h"
#include "mbox_ipc.h"
#include "shared_memory_ipc.h"
#if defined(CMAC_CPU)
 #include "cmac_util.h"
#endif

#if defined(CORTEX_M33)
 #define BSP_CRITICAL_SECTION_DEFINE    FSP_CRITICAL_SECTION_DEFINE
 #define BSP_CRITICAL_SECTION_ENTER     FSP_CRITICAL_SECTION_ENTER
 #define BSP_CRITICAL_SECTION_EXIT      FSP_CRITICAL_SECTION_EXIT
#endif

#if defined(CORTEX_M33) || defined(CMAC_CPU)

 #define MBOX_STATUS_OK                 (0)

 #define MBOX_MAGIC_TAG                 (0xA55A)
 #define MBOX_WRITER_PEND               (0x20)

 #define MBOX_PRINTF                    (0)

 #ifndef CONTAINER_OF
  #define CONTAINER_OF(ptr, type, field)    ((type *) (((char *) (ptr)) - offsetof(type, field)))
 #endif

/**
 * Disable the IRQ protection of the \link mbox_write \endlink API\n
 * This is the default value. The protection can be enabled by defining
 * MBOX_WRITE_PROTECT to 1 at application-level.
 */
 #ifndef MBOX_WRITE_PROTECT
  #define MBOX_WRITE_PROTECT        (0)
 #endif

/**
 * Disable the IRQ protection of the \link mbox_read \endlink API for CMAC.\n
 * This is the default value. The protection can be enabled by defining
 * MBOX_READ_PROTECT_CMAC to 1 at application-level.
 *
 * Reading from mailbox on CMAC doesn't require critical section as it is
 * always done in SYS2CMAC ISR - there is no conflict with writing to mailbox.
 * On the other hand, SYSCPU defers reading from mailbox to FreeRTOS context,
 * thus mbox_write can be invoked while mbox_read is in progress.
 */
 #ifndef MBOX_READ_PROTECT_CMAC
  #define MBOX_READ_PROTECT_CMAC    (0)
 #endif

 #if defined(CMAC_CPU)

/**
 * Special interrupt protection for CMAC around the call of ipc_cmac2sys_write() API call
 */

/**
 * Those interrupts should not be masked in CMAC. Includes IRQ0 + HW_EV which should have minimal downtime.
 * Note that those interrupts can safely "not be disabled" because they don't utilize mailbox API.
 */
  #define MBOX_IPC_PROT_NON_MASK_IRQs    ((1 << FIELD_IRQn) | (1 << CALLBACK_IRQn) | (1 << FRAME_IRQn)            \
                                          | (1 << LL_TIMER2PRMTV_IRQn) | (1 << SW_LLC_1_IRQn) | (1 << HW_EV_IRQn) \
                                          | (1 << LL_TIMER2LLC_IRQn) | (1 << SW_LLC_2_IRQn) | (1 << SW_LLC_3_IRQn))

/* Disable all interrupts except those in MBOX_IPC_PROT_NON_MASK_IRQs */
  #define MBOX_IPC_ACCESS_PROTECTION_ENABLE()   \
    do {                                        \
        unsigned int __l_iser = NVIC->ISER[0U]; \
                                                \
        NVIC->ICER[0U] = __l_iser & ~MBOX_IPC_PROT_NON_MASK_IRQs;

/* Re-enable the interrupts */
  #define MBOX_IPC_ACCESS_PROTECTION_DISABLE() \
    NVIC->ISER[0U] = __l_iser;                 \
}                                              \
    while (0)

 #endif                                /* CMAC_CPU */

typedef ipc_event_payload mbox_event_payload;

 #if (MBOX_PRINTF)
  #include <stdio.h>
 #endif

struct mbox_channel
{
    /// size
    uint32_t size;

    /// magic
    uint16_t * magic;

    /// status
    uint16_t * status;

    /// where to write
    uint16_t * pos_write;

    /// where to read
    uint16_t * pos_read;

    /// where to read kept for sanity check
    uint16_t pos_read_sanity;

    /// base pointer
    uint8_t * base;

    /// buffer pointer
    union
    {
        const uint8_t * wr;
        uint8_t       * rd;
    } bufptr;

    /// call back function pointer
    void (* callback)(uint8_t);
};

// mailbox environment structure
struct mbox_env
{
    /// tx channel
    struct mbox_channel tx;

    /// rx channel
    struct mbox_channel rx;
};

// mailbox local static cpu control environment object
static struct mbox_env mailbox_env;

static void _mbox_write_pend_set (struct mbox_channel * tx, uint8_t val)
{
    if (val != 0)
    {
        *tx->status |= MBOX_WRITER_PEND;
    }
    else
    {
        *tx->status &= ~MBOX_WRITER_PEND;
    }
}

static int16_t _mbox_write (struct mbox_channel * tx, const uint8_t * bufptr, uint16_t size, void (* callback)(uint8_t))
{
    uint16_t        avail = 0, avail_1 = 0, avail_2 = 0;
    uint16_t        pending;
    static uint32_t _write;

    if (*tx->pos_write >= *tx->pos_read)
    {
        avail_1 = MBOX_BUFFER_SIZE - *tx->pos_write;
        avail_2 = *tx->pos_read;
        avail   = avail_1 + avail_2;
    }
    else
    {
        avail_1 = *tx->pos_read - *tx->pos_write - 1;
        avail   = avail_1;
    }

    // write it all or save it for later
    if (avail > size)
    {
        if (size < avail_1)
        {
            avail_1 = size;
        }

 #if defined(CMAC_CPU)
        memcpy_op(&tx->base[*tx->pos_write], bufptr, avail_1);
 #else
        memcpy(&tx->base[*tx->pos_write], bufptr, avail_1);
 #endif

        // Advance write position by "avail_1" number of bytes
        uint16_t next_pos_write;
        next_pos_write = (*tx->pos_write) + avail_1;
        BSP_CHECK_FATAL(next_pos_write <= MBOX_BUFFER_SIZE);
        if (next_pos_write == MBOX_BUFFER_SIZE)
        {
            next_pos_write = 0;
        }

        *tx->pos_write = next_pos_write;

        if (size > avail_1)
        {
 #if defined(CMAC_CPU)
            memcpy_op(&tx->base[*tx->pos_write], bufptr + avail_1, size - avail_1);
 #else
            memcpy(&tx->base[*tx->pos_write], bufptr + avail_1, size - avail_1);
 #endif
            *tx->pos_write += (size - avail_1);
            BSP_CHECK_FATAL(*tx->pos_write <= MBOX_BUFFER_SIZE);
        }

        mailbox_env.tx.size      = 0;
        mailbox_env.tx.bufptr.wr = NULL;
        mailbox_env.tx.callback  = NULL;
        _write++;
        _mbox_write_pend_set(tx, 0);
        pending = 0;
    }
    else
    {
        tx->size      = size;
        tx->bufptr.wr = bufptr;
        tx->callback  = callback;
        _mbox_write_pend_set(tx, 1);
        pending = 1;
    }

    // Create the IPC event and write it to the IPC event queue
    ipc_event_type event =
        IPC_CREATE_EVENT(IPC_EVENT_CODE_MBOX_NORMAL, ipc_pack_payload((void *) mailbox_env.tx.base, *tx->pos_write));
 #if defined(CMAC_CPU)
    MBOX_IPC_ACCESS_PROTECTION_ENABLE();
    ipc_cmac2sys_write(event);
    MBOX_IPC_ACCESS_PROTECTION_DISABLE();
 #else
    ipc_sys2cmac_write(event);
 #endif
    if ((pending == 0) && (callback != NULL))
    {
        // Call handler
        callback(MBOX_STATUS_OK);
    }

    return pending;
}

__STATIC_INLINE uint8_t _mbox_data_rdy_getf (void)
{
    return *mailbox_env.rx.pos_read != *mailbox_env.rx.pos_write;
}

__STATIC_INLINE uint8_t _mbox_rxdata_getf (void)
{
    uint8_t r = mailbox_env.rx.base[*mailbox_env.rx.pos_read];

    // Advance read position by 1
    uint16_t next_pos_read = *mailbox_env.rx.pos_read + 1;
    BSP_CHECK_DEBUG(next_pos_read <= MBOX_BUFFER_SIZE);

    if (next_pos_read == MBOX_BUFFER_SIZE)
    {
        next_pos_read = 0;
    }

    *mailbox_env.rx.pos_read = next_pos_read;

    return r;
}

static void _mbox_data_normal_handler (ipc_event_code evt_code, mbox_event_payload event_payload)
{
    uint8_t * data_ptr = (uint8_t *) GET_SHARED_RAM_PTR(event_payload);
    void      (* callback)(uint8_t) = NULL;

    if (data_ptr == NULL)
    {
        return;
    }

    struct mbox_channel_core * ch_data = CONTAINER_OF(data_ptr, struct mbox_channel_core, data[0]);

    BSP_CHECK_FATAL(evt_code == IPC_EVENT_CODE_MBOX_NORMAL);
    BSP_CHECK_FATAL(ch_data->magic == MBOX_MAGIC_TAG);
    BSP_CHECK_FATAL(ch_data->data == data_ptr);

    if (mailbox_env.rx.size == 0)
    {
        // Retrieve callback pointer
        callback = mailbox_env.rx.callback;
        if (callback != NULL)
        {
            // Clear callback pointer
            mailbox_env.rx.callback = NULL;

            // Call handler
            callback(MBOX_STATUS_OK);
        }

        return;
    }

    while (_mbox_data_rdy_getf())
    {
        // Read the received in the FIFO
        *mailbox_env.rx.bufptr.rd      = _mbox_rxdata_getf();
        mailbox_env.rx.pos_read_sanity = *mailbox_env.rx.pos_read;

        // Update RX parameters
        mailbox_env.rx.size--;
        mailbox_env.rx.bufptr.rd++;

        // Check if all expected data have been received
        if (mailbox_env.rx.size == 0)
        {
            // Retrieve callback pointer
            callback = mailbox_env.rx.callback;

            if (callback != NULL)
            {
                // Clear callback pointer
                mailbox_env.rx.callback = NULL;

                // Call handler
                callback(MBOX_STATUS_OK);
            }
            else
            {
                BSP_CHECK_FATAL(0);
            }

            // Exit loop
            break;
        }
    }

    if (*mailbox_env.rx.status & MBOX_WRITER_PEND)
    {
        ipc_event_type event =
            IPC_CREATE_EVENT(IPC_EVENT_CODE_MBOX_READY,
                             ipc_pack_payload((void *) mailbox_env.rx.base, *mailbox_env.rx.pos_read));
 #if defined(CMAC_CPU)
        MBOX_IPC_ACCESS_PROTECTION_ENABLE();
        ipc_cmac2sys_write(event);
        MBOX_IPC_ACCESS_PROTECTION_DISABLE();
 #else
        ipc_sys2cmac_write(event);
 #endif
    }
}

static void _mbox_data_ready_handler (ipc_event_code evt_code, mbox_event_payload event_payload)
{
    uint8_t * data_ptr = (uint8_t *) GET_SHARED_RAM_PTR(event_payload);

    if (data_ptr == NULL)
    {
        return;
    }

    struct mbox_channel_core * ch_data = CONTAINER_OF(data_ptr, struct mbox_channel_core, data[0]);

    BSP_CHECK_FATAL(evt_code == IPC_EVENT_CODE_MBOX_READY);
    BSP_CHECK_FATAL(ch_data->magic == MBOX_MAGIC_TAG);
    BSP_CHECK_FATAL(ch_data->data == data_ptr);

    if (!(ch_data->status & MBOX_WRITER_PEND))
    {
        return;
    }

    BSP_CHECK_FATAL(ch_data->status & MBOX_WRITER_PEND);

    if (mailbox_env.tx.bufptr.wr)
    {
        _mbox_write(&mailbox_env.tx, mailbox_env.tx.bufptr.wr, mailbox_env.tx.size, mailbox_env.tx.callback);
    }
}

static void _mbox_setup_txrx (void)
{
 #if defined(CMAC_CPU)
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_read  = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_write = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].status    = 0;

    mailbox_env.tx.magic     = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].magic);
    mailbox_env.tx.pos_read  = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_read);
    mailbox_env.tx.pos_write = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_write);
    mailbox_env.tx.status    = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].status);
    mailbox_env.tx.base      = (uint8_t *) &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].data);

    mailbox_env.rx.magic     = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].magic);
    mailbox_env.rx.pos_read  = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_read);
    mailbox_env.rx.pos_write = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_write);
    mailbox_env.rx.status    = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].status);
    mailbox_env.rx.base      = (uint8_t *) &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].data);

    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].magic = MBOX_MAGIC_TAG;
 #else                                 // CORTEX_M33
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_read  = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_write = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].status    = 0;

    mailbox_env.tx.magic     = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].magic);
    mailbox_env.tx.pos_read  = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_read);
    mailbox_env.tx.pos_write = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_write);
    mailbox_env.tx.status    = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].status);
    mailbox_env.tx.base      = (uint8_t *) &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].data);

    mailbox_env.rx.magic     = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].magic);
    mailbox_env.rx.pos_read  = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_read);
    mailbox_env.rx.pos_write = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].pos_write);
    mailbox_env.rx.status    = &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].status);
    mailbox_env.rx.base      = (uint8_t *) &(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].data);

    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS].magic = MBOX_MAGIC_TAG;
 #endif

    volatile uint16_t * magic_ptr = mailbox_env.tx.magic;
    while (*magic_ptr != MBOX_MAGIC_TAG)
    {
        // Wait for the other side to initialize Tx path
    }
}

int16_t mbox_init (void)
{
    int16_t      status;
    ipc_status_t ipc_channel_status;

    _mbox_setup_txrx();

 #if defined(CMAC_CPU)
  #define IPC_CHANNEL    (IPC_CHANNEL_SYS2CMAC)
    ipc_channel_status = ipc_sys2cmac_status();
 #else
  #define IPC_CHANNEL    (IPC_CHANNEL_CMAC2SYS)
    ipc_channel_status = ipc_cmac2sys_status();
 #endif
    if (!ipc_channel_status.bits.ready)
    {
        status = ipc_initialize(IPC_CHANNEL, NULL);
        if (status)
        {
            return status;
        }
    }

    status = ipc_register_handler(IPC_CHANNEL, IPC_EVENT_CODE_MBOX_NORMAL, _mbox_data_normal_handler);
    if (status != IPC_EVENT_CODE_MBOX_NORMAL)
    {
        return status;
    }

    status = ipc_register_handler(IPC_CHANNEL, IPC_EVENT_CODE_MBOX_READY, _mbox_data_ready_handler);
    if (status != IPC_EVENT_CODE_MBOX_READY)
    {
        return status;
    }

    return IPC_SUCCESS;
}

int16_t mbox_write (const uint8_t * bufptr, uint16_t size, void (* callback)(uint8_t))
{
    uint16_t status;
 #if (MBOX_WRITE_PROTECT == 1)
    BSP_CRITICAL_SECTION_DEFINE;
    BSP_CRITICAL_SECTION_ENTER;
 #endif
    status = _mbox_write(&mailbox_env.tx, bufptr, size, callback);
 #if (MBOX_WRITE_PROTECT == 1)
    BSP_CRITICAL_SECTION_EXIT;
 #endif

    return status;
}

void mbox_read (uint8_t * bufptr, uint16_t size, void (* callback)(uint8_t))
{
 #if (!defined(CMAC_CPU) || MBOX_READ_PROTECT_CMAC == 1)
    BSP_CRITICAL_SECTION_DEFINE;
    BSP_CRITICAL_SECTION_ENTER;
 #endif

    mailbox_env.rx.size      = size;
    mailbox_env.rx.bufptr.rd = bufptr;
    mailbox_env.rx.callback  = callback;

    if (_mbox_data_rdy_getf())
    {
        _mbox_data_normal_handler(IPC_EVENT_CODE_MBOX_NORMAL,
                                  ipc_pack_payload((void *) mailbox_env.rx.base, *mailbox_env.rx.pos_read));
    }

 #if (!defined(CMAC_CPU) || MBOX_READ_PROTECT_CMAC == 1)
    BSP_CRITICAL_SECTION_EXIT;
 #endif
}

 #if defined(CORTEX_M33)
void mbox_reset ()
{
    // Reset CMAC2SYS
    ipc_cmac2sys_interrupt_clear();
    memset(&shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS],
           0,
           sizeof(shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_CMAC2SYS]));
    ipc_cmac2sys_clear();

    // Reset SYS2CMAC
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].magic     = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_write = 0;
    shared_ram_ipc_ptr->mboxes[IPC_CHANNEL_SYS2CMAC].pos_read  = 0;
    ipc_sys2cmac_clear();
}

 #endif
#endif                                 /* defined(CORTEX_M33) || defined(CMAC_CPU) */
