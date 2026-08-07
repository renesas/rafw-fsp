/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file ipc_functions.c
 *
 * @brief Implementation of the low-level functions for the IPC component. V0_4_0
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
#include <string.h>
#include "sdk_defs.h"
#include "ipc_config.h"
#include "ipc_interrupts.h"
#include "ipc_eventq.h"
#include "ipc.h"
#include "shared_memory_ipc.h"
#if defined(CMAC_CPU)
 #include "renesas_cmac.h"
#endif

/****************************************************************************
 *                            Global variables/const
 ****************************************************************************/

/**
 * Shared memory start address for the F1 processor. For the M33/CMAC CPUs, the symbol is imported from the linker script
 * \todo Import the address from the linker script
 */
#if !defined(CORTEX_M33) && !defined(CMAC_CPU)
const uint32_t __shared_space_start__ = 0x61100000;
#endif

/****************************************************************************
 *                            Local variables/const
 ****************************************************************************/

/****************************************************************************
 *                          Local Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */
#if defined(CORTEX_M33)
void ipc_cmac2sys_isr_cb(void);

 #if BSP_FEATURE_HAS_DSP
void ipc_dsp2sys_isr_cb(void);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)
void ipc_sys2cmac_isr_cb(void);

 #if BSP_FEATURE_HAS_DSP
void ipc_dsp2cmac_isr_cb(void);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#else
void ipc_sys2dsp_isr_cb(void);
void ipc_cmac2dsp_isr_cb(void);

#endif

/** @}*/

/****************************************************************************
 *                                Implementation
 ****************************************************************************/
#if defined(CORTEX_M33)

/** \addtogroup IPC_SYS_TO_CMAC
 *  @{
 */
int16_t ipc_sys2cmac_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled & shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.sys2cmac.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.sys2cmac, &shared_ram_ipc_ptr->ipc.sys2cmac.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_sys2cmac_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_SYS_TO_DSP
 *  @{
 */
int16_t ipc_sys2dsp_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled & shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.sys2dsp.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.sys2dsp, &shared_ram_ipc_ptr->ipc.sys2dsp.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_sys2dsp_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the CMAC2SYS ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 */
void ipc_cmac2sys_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.cmac2sys.irq_callback(shared_ram_ipc_ptr->ipc.cmac2sys.event);
    }
}

/** @}*/

int16_t ipc_cmac2sys_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.cmac2sys.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.cmac2sys.status.value = 0;
    shared_ram_ipc_ptr->ipc.cmac2sys.irq_callback = callback;
    NVIC_ClearPendingIRQ(CMAC2SYS_IRQn);
    ipc_cmac2sys_interrupt_clear();
    NVIC_EnableIRQ(CMAC2SYS_IRQn);
    shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_cmac2sys_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.cmac2sys.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_cmac2sys_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.cmac2sys, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_cmac2sys_enable ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_cmac2sys_disable ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_cmac2sys_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.cmac2sys.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_SYS
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the DSP2SYS ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 */
void ipc_dsp2sys_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.dsp2sys.irq_callback(shared_ram_ipc_ptr->ipc.dsp2sys.event);
    }
}

/** @}*/

int16_t ipc_dsp2sys_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.dsp2sys.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.dsp2sys.status.value = 0;
    shared_ram_ipc_ptr->ipc.dsp2sys.irq_callback = callback;
    NVIC_ClearPendingIRQ(DSP_IRQn);
    ipc_dsp2sys_interrupt_clear();
    NVIC_EnableIRQ(DSP_IRQn);
    shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_dsp2sys_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.dsp2sys.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_dsp2sys_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.dsp2sys, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_dsp2sys_enable ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_dsp2sys_disable ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_dsp2sys_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.dsp2sys.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */
void ipc_cmac2sys_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.cmac2sys.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.cmac2sys.event        = 0;
    shared_ram_ipc_ptr->ipc.cmac2sys.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.cmac2sys.status.value = 0;
}

/** @}*/

/** \addtogroup IPC_SYS_TO_CMAC
 *  @{
 */
void ipc_sys2cmac_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.sys2cmac.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.sys2cmac.event        = 0;
    shared_ram_ipc_ptr->ipc.sys2cmac.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.sys2cmac.status.value = 0;
}

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_SYS_TO_DSP
 *  @{
 */
void ipc_sys2dsp_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.sys2dsp.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.sys2dsp.event        = 0;
    shared_ram_ipc_ptr->ipc.sys2dsp.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.sys2dsp.status.value = 0;
}

/** @}*/

/** \addtogroup IPC_DSP_TO_SYS
 *  @{
 */
void ipc_dsp2sys_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.dsp2sys.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.dsp2sys.event        = 0;
    shared_ram_ipc_ptr->ipc.dsp2sys.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.dsp2sys.status.value = 0;
}

/** @}*/

/** \addtogroup IPC_CMAC_TO_DSP
 *  @{
 */
void ipc_cmac2dsp_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.cmac2dsp.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.cmac2dsp.event        = 0;
    shared_ram_ipc_ptr->ipc.cmac2dsp.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.cmac2dsp.status.value = 0;
}

/** @}*/

 #endif                                /* BSP_FEATURE_HAS_DSP */

/** \addtogroup IPC_SYS_TO_CMAC
 *  @{
 */
ipc_status_t ipc_sys2cmac_status ()
{
    return shared_ram_ipc_ptr->ipc.sys2cmac.status;
}

/** @}*/

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */
ipc_status_t ipc_cmac2sys_status ()
{
    return shared_ram_ipc_ptr->ipc.cmac2sys.status;
}

/** @}*/

#elif defined(CMAC_CPU)

/** \addtogroup IPC_SYS_TO_CMAC
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the SYS2CMAC ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 *
 */
void ipc_sys2cmac_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.sys2cmac.irq_callback(shared_ram_ipc_ptr->ipc.sys2cmac.event);
    }
}

/** @}*/

int16_t ipc_sys2cmac_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.sys2cmac.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.sys2cmac.status.value = 0;
    shared_ram_ipc_ptr->ipc.sys2cmac.irq_callback = callback;
    NVIC_ClearPendingIRQ(SYS2CMAC_IRQn);
    ipc_sys2cmac_interrupt_clear();
    NVIC_EnableIRQ(SYS2CMAC_IRQn);
    shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_sys2cmac_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.sys2cmac.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_sys2cmac_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.sys2cmac, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_sys2cmac_enable ()
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_sys2cmac_disable ()
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_sys2cmac_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.sys2cmac.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.sys2cmac.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_CMAC
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the DSP2CMAC ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 */
void ipc_dsp2cmac_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.dsp2cmac.irq_callback(shared_ram_ipc_ptr->ipc.dsp2cmac.event);
    }
}

/** @}*/

int16_t ipc_dsp2cmac_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.dsp2cmac.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.dsp2cmac.status.value = 0;
    shared_ram_ipc_ptr->ipc.dsp2cmac.irq_callback = callback;
    NVIC_ClearPendingIRQ(SYSPER_IRQn);
    ipc_dsp2cmac_interrupt_clear();
    NVIC_EnableIRQ(SYSPER_IRQn);
    *(uint32_t *) CM_SYSPER_IRQ_ENABLE_REG              |= SYSPER_IRQ_DSP2CMAC;
    shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_dsp2cmac_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.dsp2cmac.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_dsp2cmac_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.dsp2cmac, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_dsp2cmac_enable ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_dsp2cmac_disable ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_dsp2cmac_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.dsp2cmac.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

void ipc_dsp2cmac_clear ()
{
    memset(&shared_ram_ipc_ptr->ipc.dsp2cmac.eventq, 0, sizeof(ipc_eventq_t));
    shared_ram_ipc_ptr->ipc.dsp2cmac.event        = 0;
    shared_ram_ipc_ptr->ipc.dsp2cmac.irq_callback = NULL;
    shared_ram_ipc_ptr->ipc.dsp2cmac.status.value = 0;
}

ipc_isr_cb_t ipc_dsp2cmac_callback_get ()
{
    return ipc_dsp2cmac_isr_cb;
}

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */
int16_t ipc_cmac2sys_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.enabled & shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.cmac2sys.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.cmac2sys.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.cmac2sys, &shared_ram_ipc_ptr->ipc.cmac2sys.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_cmac2sys_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_CMAC_TO_DSP
 *  @{
 */
int16_t ipc_cmac2dsp_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled & shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.cmac2dsp.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.cmac2dsp, &shared_ram_ipc_ptr->ipc.cmac2dsp.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_cmac2dsp_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

/** \addtogroup IPC_SYS_TO_CMAC
 *  @{
 */
ipc_status_t ipc_sys2cmac_status ()
{
    return shared_ram_ipc_ptr->ipc.sys2cmac.status;
}

/** @}*/

/** \addtogroup IPC_CMAC_TO_SYS
 *  @{
 */
ipc_status_t ipc_cmac2sys_status ()
{
    return shared_ram_ipc_ptr->ipc.cmac2sys.status;
}

#else

/** \addtogroup IPC_SYS_TO_DSP
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the SYS2DSP ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 */
void ipc_sys2dsp_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.sys2dsp.irq_callback(shared_ram_ipc_ptr->ipc.sys2dsp.event);
    }

    ipc_sys2dsp_interrupt_clear();
}

/** @}*/

int16_t ipc_sys2dsp_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.sys2dsp.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.sys2dsp.status.value = 0;
    shared_ram_ipc_ptr->ipc.sys2dsp.irq_callback = callback;
    ipc_sys2dsp_interrupt_clear();
    shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_sys2dsp_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.sys2dsp.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_sys2dsp_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.sys2dsp, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_sys2dsp_enable ()
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_sys2dsp_disable ()
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_sys2dsp_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.sys2dsp.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.sys2dsp.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

ipc_isr_cb_t ipc_sys2dsp_callback_get ()
{
    return ipc_sys2dsp_isr_cb;
}

/** @}*/

/** \addtogroup IPC_CMAC_TO_DSP
 *  @{
 */

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */

/**
 * \brief This is the main callback function by the CMAC2DSP ISR.\n
 * The function gets the event and writes it in the event queue. Then calls the user-provided callback function
 */
void ipc_cmac2dsp_isr_cb ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.irq_callback != NULL)
    {
        shared_ram_ipc_ptr->ipc.cmac2dsp.irq_callback(shared_ram_ipc_ptr->ipc.cmac2dsp.event);
    }

    ipc_cmac2dsp_interrupt_clear();
}

/** @}*/

int16_t ipc_cmac2dsp_init (ipc_event_cb_t callback)
{
    int16_t status = ipc_eventq_init(&shared_ram_ipc_ptr->ipc.cmac2dsp.eventq, IPC_EVENTQ_SIZE);
    if (status != IPC_SUCCESS)
    {
        return status;
    }

    shared_ram_ipc_ptr->ipc.cmac2dsp.status.value = 0;
    shared_ram_ipc_ptr->ipc.cmac2dsp.irq_callback = callback;
    ipc_cmac2dsp_interrupt_clear();
    shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled = 1;
    shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready   = 1;

    return IPC_SUCCESS;
}

int16_t ipc_cmac2dsp_callback (ipc_event_cb_t callback)
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        shared_ram_ipc_ptr->ipc.cmac2dsp.irq_callback = callback;

        return IPC_SUCCESS;
    }

    return IPC_ERROR_RECEIVER_NOT_READY;
}

int16_t ipc_cmac2dsp_next_pending (ipc_event_type * event)
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        if (ipc_eventq_read(&shared_ram_ipc_ptr->ipc.cmac2dsp, event) == 0)
        {
            return IPC_ERROR_EVENTQ_EMPTY;
        }

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_cmac2dsp_enable ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        if (!shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled = 1;
        }

        return IPC_ENABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int16_t ipc_cmac2dsp_disable ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled)
        {
            shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.enabled = 0;
        }

        return IPC_DISABLED;
    }

    return IPC_ERROR_UNINITIALIZED;
}

int ipc_cmac2dsp_count_pending ()
{
    if (shared_ram_ipc_ptr->ipc.cmac2dsp.status.bits.ready)
    {
        return ipc_eventq_items_count(&shared_ram_ipc_ptr->ipc.cmac2dsp.eventq);
    }

    return IPC_ERROR_UNINITIALIZED;
}

ipc_isr_cb_t ipc_cmac2dsp_callback_get ()
{
    return ipc_cmac2dsp_isr_cb;
}

/** @}*/

/** \addtogroup IPC_DSP_TO_SYS
 *  @{
 */
int16_t ipc_dsp2sys_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.enabled & shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.dsp2sys.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.dsp2sys.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.dsp2sys, &shared_ram_ipc_ptr->ipc.dsp2sys.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_dsp2sys_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

/** \addtogroup IPC_DSP_TO_CMAC
 *  @{
 */
int16_t ipc_dsp2cmac_write (ipc_event_type event)
{
    if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.enabled & shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.ready)
    {
        if (shared_ram_ipc_ptr->ipc.dsp2cmac.status.bits.eventq_full)
        {
            return IPC_ERROR_EVENTQ_FULL;
        }

        shared_ram_ipc_ptr->ipc.dsp2cmac.event = event;
        if (ipc_eventq_write(&shared_ram_ipc_ptr->ipc.dsp2cmac, &shared_ram_ipc_ptr->ipc.dsp2cmac.event) == 0)
        {
            return IPC_ERROR_UNKNOWN;
        }

        ipc_dsp2cmac_interrupt_set();

        return IPC_SUCCESS;
    }

    return IPC_ERROR_UNINITIALIZED;
}

/** @}*/

#endif

#if defined(CMAC_CPU) || defined(CORTEX_M33)

/** @}*/
#endif

#if defined(CMAC_CPU) || !defined(CORTEX_M33)
 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_CMAC
 *  @{
 */
ipc_status_t ipc_dsp2cmac_status ()
{
    return shared_ram_ipc_ptr->ipc.dsp2cmac.status;
}

/** @}*/

/** \addtogroup IPC_CMAC_TO_DSP
 *  @{
 */
ipc_status_t ipc_cmac2dsp_status ()
{
    return shared_ram_ipc_ptr->ipc.cmac2dsp.status;
}

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

#if !defined(CMAC_CPU) || defined(CORTEX_M33)
 #if BSP_FEATURE_HAS_DSP

/** \addtogroup IPC_DSP_TO_SYS
 *  @{
 */
ipc_status_t ipc_dsp2sys_status ()
{
    return shared_ram_ipc_ptr->ipc.dsp2sys.status;
}

/** @}*/

/** \addtogroup IPC_SYS_TO_DSP
 *  @{
 */
ipc_status_t ipc_sys2dsp_status ()
{
    return shared_ram_ipc_ptr->ipc.sys2dsp.status;
}

/** @}*/
 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

/**
 \}
 \}
 */
