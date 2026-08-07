/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file ipc_eventq.c
 *
 * @brief Implementation of the event queue for the IPC component. V0_4_0
 *
 **************************************************************************************
 **
 */

/**
 * \addtogroup MIDDLEWARE
 \{
 * \addtogroup IPC
 \{
 */

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include <string.h>
#include <stdbool.h>
#include "sdk_defs.h"
#include "ipc_types.h"
#include "ipc_eventq.h"

/// \endcond

/****************************************************************************
 *                                Private function prototypes
 ****************************************************************************/
__STATIC_FORCEINLINE int16_t ipc_eventq_items_get(const ipc_eventq_t * eventq);

/****************************************************************************
 *                                Implementation
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS
 *  @{
 */

int16_t ipc_eventq_init (ipc_eventq_t * eventq, int size)
{
    eventq->head        = 0;
    eventq->tail        = 0;
    eventq->items_count = 0;
    eventq->size        = size;
    if (memset(eventq->buffer, 0, size * sizeof(eventq->buffer[0])) != NULL)
    {
        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNKNOWN;
}

int16_t ipc_eventq_read (ipc_t * ipc, ipc_event_type * event)
{
#if defined(CORTEX_M33)
    int status = 0;
    do
    {
        while (__LDAEX(&ipc->eventq.lock) != 0)
        {
            ;
        }

        status = __STREXW(1, &ipc->eventq.lock);
    } while (status != 0);
#else
    while (ipc->eventq.lock != 0)
    {
        ;
    }
    ipc->eventq.lock = true;
#endif

    int16_t items_read = 0;
    ipc->eventq.items_count = ipc_eventq_items_get(&ipc->eventq);
    if (ipc->eventq.items_count > 0)
    {
        *event = ipc->eventq.buffer[ipc->eventq.tail];
        ipc->eventq.tail++;
        if (ipc->eventq.tail == ipc->eventq.size)
        {
            ipc->eventq.tail = 0;
        }

        items_read++;
    }

    if (ipc->eventq.items_count < ipc->eventq.size)
    {
        ipc->status.bits.eventq_full = 0;
    }

#if defined(CORTEX_M33)
    __STL(0, &ipc->eventq.lock);
#else
    ipc->eventq.lock = false;
#endif

    return items_read;
}

int16_t ipc_eventq_write (ipc_t * ipc, const ipc_event_type * event)
{
#if defined(CORTEX_M33)
    int status = 0;
    do
    {
        while (__LDAEX(&ipc->eventq.lock) != 0)
        {
            ;
        }

        status = __STREXW(1, &ipc->eventq.lock);
    } while (status != 0);
#else
    while (ipc->eventq.lock != 0)
    {
        ;
    }
    ipc->eventq.lock = true;
#endif
    int16_t items_written = 0;
    if (!ipc->status.bits.eventq_full)
    {
        ipc->eventq.buffer[ipc->eventq.head] = *event;
        ipc->eventq.head++;
        if (ipc->eventq.head == ipc->eventq.size)
        {
            ipc->eventq.head = 0;
        }

        items_written++;
    }

    if (ipc_eventq_items_get(&ipc->eventq) >= ipc->eventq.size)
    {
        ipc->status.bits.eventq_full = 1;
    }

#if defined(CORTEX_M33)
    __STL(0, &ipc->eventq.lock);
#else
    ipc->eventq.lock = false;
#endif

    return items_written;
}

int16_t ipc_eventq_items_count (ipc_eventq_t * eventq)
{
    int16_t items = 0;
#if defined(CORTEX_M33)
    int status = 0;
    do
    {
        while (__LDAEX(&eventq->lock) != 0)
        {
            ;
        }

        status = __STREXW(1, &eventq->lock);
    } while (status != 0);
#else
    while (eventq->lock != 0)
    {
        ;
    }
    eventq->lock = true;
#endif

    items = ipc_eventq_items_get((const ipc_eventq_t *) eventq);

#if defined(CORTEX_M33)
    __STL(0, &eventq->lock);
#else
    eventq->lock = false;
#endif

    return items;
}

/**
 * \brief Gets the number of events in the queue
 *
 * \param [in] eventq Pointer to the event queue
 *
 * \returns The number of events in the queue
 */
__STATIC_FORCEINLINE int16_t ipc_eventq_items_get (const ipc_eventq_t * eventq)
{
    if (eventq->head > eventq->tail)
    {
        return eventq->head - eventq->tail;
    }
    else if (eventq->head < eventq->tail)
    {
        return (eventq->size - eventq->tail) + eventq->head;
    }

    return 0;
}

/** @}*/

/**
 \}
 \}
 */
