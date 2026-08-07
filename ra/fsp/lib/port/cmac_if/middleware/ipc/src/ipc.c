/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file ipc.c
 *
 * @brief Implementation of the high-level functions for the IPC component.
 *
 **************************************************************************************
 **
 */

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include "ipc.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/****************************************************************************
 *                            Local variables/const
 ****************************************************************************/
#if defined(CORTEX_M33)

/**
 * Vector of event handlers for the CMAC2SYS IPC channel
 */
ipc_event_handler_t cmac2sys_handlers[IPC_CMAC2SYS_HANDLERS_MAX] = {NULL};

 #if BSP_FEATURE_HAS_DSP

/**
 * Vector of event handlers for the DSP2SYS IPC channel
 */
ipc_event_handler_t dsp2sys_handlers[IPC_DSP2SYS_HANDLERS_MAX] = {NULL};
 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)

/**
 * Vector of event handlers for the SYS2CMAC IPC channel
 */
ipc_event_handler_t sys2cmac_handlers[IPC_SYS2CMAC_HANDLERS_MAX] = {NULL};

 #if BSP_FEATURE_HAS_DSP

/**
 * Vector of event handlers for the DSP2CMAC IPC channel
 */
ipc_event_handler_t dsp2cmac_handlers[IPC_DSP2CMAC_HANDLERS_MAX] = {NULL};
 #endif                                /* BSP_FEATURE_HAS_DSP */

#else

/**
 * Vector of event handlers for the SYS2DSP IPC channel
 */
ipc_event_handler_t sys2dsp_handlers[IPC_SYS2DSP_HANDLERS_MAX] = {NULL};

/**
 * Vector of event handlers for the CMAC2DSP IPC channel
 */
ipc_event_handler_t cmac2dsp_handlers[IPC_CMAC2DSP_HANDLERS_MAX] = {NULL};
#endif

/****************************************************************************
 *                          Local Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */
#if defined(CORTEX_M33)
static void ipc_cmac2sys_main_handler(ipc_event_type event);

 #if BSP_FEATURE_HAS_DSP
static void ipc_dsp2sys_main_handler(ipc_event_type event);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)
static void ipc_sys2cmac_main_handler(ipc_event_type event);

 #if BSP_FEATURE_HAS_DSP
static void ipc_dsp2cmac_main_handler(ipc_event_type event);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#else
static void ipc_sys2dsp_main_handler(ipc_event_type event);
static void ipc_cmac2dsp_main_handler(ipc_event_type event);

#endif

/** @}*/

/****************************************************************************
 *                                Implementation
 ****************************************************************************/

/** \addtogroup IPC_PUBLIC_FUNCTIONS
 *  @{
 */

#if defined(CORTEX_M33)
int16_t ipc_initialize (ipc_channel_t ipc_channel, ipc_event_handler_t event_handler)
{
    int16_t status = IPC_ERROR_PARAMETER;

    if (ipc_channel == IPC_CHANNEL_CMAC2SYS)
    {
        for (int index = 0; index < IPC_CMAC2SYS_HANDLERS_MAX; index++)
        {
            cmac2sys_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            cmac2sys_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_cmac2sys_init(&ipc_cmac2sys_main_handler);
    }

 #if BSP_FEATURE_HAS_DSP
    else if (ipc_channel == IPC_CHANNEL_DSP2SYS)
    {
        for (int index = 0; index < IPC_DSP2SYS_HANDLERS_MAX; index++)
        {
            dsp2sys_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            dsp2sys_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_dsp2sys_init(&ipc_dsp2sys_main_handler);
    }
 #endif                                /* BSP_FEATURE_HAS_DSP */

    return status;
}

#elif defined(CMAC_CPU)
int16_t ipc_initialize (ipc_channel_t ipc_channel, ipc_event_handler_t event_handler)
{
    int16_t status = IPC_ERROR_PARAMETER;
    if (ipc_channel == IPC_CHANNEL_SYS2CMAC)
    {
        for (int index = 0; index < IPC_SYS2CMAC_HANDLERS_MAX; index++)
        {
            sys2cmac_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            sys2cmac_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_sys2cmac_init(&ipc_sys2cmac_main_handler);
    }

 #if BSP_FEATURE_HAS_DSP
    else if (ipc_channel == IPC_CHANNEL_DSP2CMAC)
    {
        for (int index = 0; index < IPC_DSP2CMAC_HANDLERS_MAX; index++)
        {
            dsp2cmac_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            dsp2cmac_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_dsp2cmac_init(&ipc_dsp2cmac_main_handler);
    }
 #endif                                /* BSP_FEATURE_HAS_DSP */

    return status;
}

#else
int16_t ipc_initialize (ipc_channel_t ipc_channel, ipc_event_handler_t event_handler)
{
    int16_t status = IPC_ERROR_PARAMETER;
    if (ipc_channel == IPC_CHANNEL_SYS2DSP)
    {
        for (int index = 0; index < IPC_SYS2CMAC_HANDLERS_MAX; index++)
        {
            sys2dsp_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            sys2dsp_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_sys2dsp_init(&ipc_sys2dsp_main_handler);
    }
    else if (ipc_channel == IPC_CHANNEL_CMAC2DSP)
    {
        for (int index = 0; index < IPC_DSP2CMAC_HANDLERS_MAX; index++)
        {
            cmac2dsp_handlers[index] = NULL;
        }

        if (event_handler != NULL)
        {
            cmac2dsp_handlers[DEFAULT_EVENT_CODE] = event_handler;
        }

        status = ipc_cmac2dsp_init(&ipc_cmac2dsp_main_handler);
    }

    return status;
}

#endif

#if defined(CORTEX_M33)
int16_t ipc_register_handler (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_handler_t event_handler)
{
    if (ipc_channel == IPC_CHANNEL_CMAC2SYS)
    {
        if (event_code < IPC_CMAC2SYS_HANDLERS_MAX)
        {
            cmac2sys_handlers[event_code] = event_handler;

            return event_code;
        }
    }

 #if BSP_FEATURE_HAS_DSP
    else if (ipc_channel == IPC_CHANNEL_DSP2SYS)
    {
        if (event_code < IPC_DSP2SYS_HANDLERS_MAX)
        {
            dsp2sys_handlers[event_code] = event_handler;

            return event_code;
        }
    }
 #endif                                /* BSP_FEATURE_HAS_DSP */

    return IPC_ERROR_PARAMETER;
}

#elif defined(CMAC_CPU)
int16_t ipc_register_handler (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_handler_t event_handler)
{
    if (ipc_channel == IPC_CHANNEL_SYS2CMAC)
    {
        if (event_code < IPC_SYS2CMAC_HANDLERS_MAX)
        {
            sys2cmac_handlers[event_code] = event_handler;

            return event_code;
        }
    }

 #if BSP_FEATURE_HAS_DSP
    else if (ipc_channel == IPC_CHANNEL_DSP2CMAC)
    {
        if (event_code < IPC_DSP2CMAC_HANDLERS_MAX)
        {
            dsp2cmac_handlers[event_code] = event_handler;

            return event_code;
        }
    }
 #endif                                /* BSP_FEATURE_HAS_DSP */

    return IPC_ERROR_PARAMETER;
}

#else
int16_t ipc_register_handler (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_handler_t event_handler)
{
    if (ipc_channel == IPC_CHANNEL_SYS2DSP)
    {
        if (event_code < IPC_SYS2DSP_HANDLERS_MAX)
        {
            sys2dsp_handlers[event_code] = event_handler;

            return event_code;
        }
    }
    else if (ipc_channel == IPC_CHANNEL_CMAC2DSP)
    {
        if (event_code < IPC_CMAC2DSP_HANDLERS_MAX)
        {
            cmac2dsp_handlers[event_code] = event_handler;

            return event_code;
        }
    }

    return IPC_ERROR_PARAMETER;
}

#endif

#if defined(CORTEX_M33)
int16_t ipc_write_data (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_payload event_payload)
{
    ipc_event_type event = IPC_CREATE_EVENT(event_code, event_payload);
 #if BSP_FEATURE_HAS_DSP
    if (ipc_channel == IPC_CHANNEL_SYS2DSP)
    {
        return ipc_sys2dsp_write(event);
    }
    else
 #endif                                /* BSP_FEATURE_HAS_DSP */
    if (ipc_channel == IPC_CHANNEL_SYS2CMAC)
    {
        return ipc_sys2cmac_write(event);
    }

    return IPC_ERROR_PARAMETER;
}

#elif defined(CMAC_CPU)
int16_t ipc_write_data (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_payload event_payload)
{
    ipc_event_type event = IPC_CREATE_EVENT(event_code, event_payload);
    if (ipc_channel == IPC_CHANNEL_CMAC2SYS)
    {
        return ipc_cmac2sys_write(event);
    }

 #if BSP_FEATURE_HAS_DSP
    else if (ipc_channel == IPC_CHANNEL_CMAC2DSP)
    {
        return ipc_cmac2dsp_write(event);
    }
 #endif                                /* BSP_FEATURE_HAS_DSP */

    return IPC_ERROR_PARAMETER;
}

#else
int16_t ipc_write_data (ipc_channel_t ipc_channel, ipc_event_code event_code, ipc_event_payload event_payload)
{
    ipc_event_type event = IPC_CREATE_EVENT(event_code, event_payload);
    if (ipc_channel == IPC_CHANNEL_DSP2SYS)
    {
        return ipc_dsp2sys_write(event);
    }
    else if (ipc_channel == IPC_CHANNEL_DSP2CMAC)
    {
        return ipc_dsp2cmac_write(event);
    }

    return IPC_ERROR_PARAMETER;
}

#endif

/** @}*/

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */
#if defined(CORTEX_M33)

/**
 * \brief Main handler for the CMAC2SYS IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_cmac2sys_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_cmac2sys_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_CMAC2SYS_HANDLERS_MAX) && (cmac2sys_handlers[event_code] != NULL))
            {
                cmac2sys_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

 #if BSP_FEATURE_HAS_DSP

/**
 * \brief Main handler for the DSP2SYS IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_dsp2sys_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_dsp2sys_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_DSP2SYS_HANDLERS_MAX) && (dsp2sys_handlers[event_code] != NULL))
            {
                dsp2sys_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)

/**
 * \brief Main handler for the SYS2CMAC IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_sys2cmac_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_sys2cmac_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_SYS2CMAC_HANDLERS_MAX) && (sys2cmac_handlers[event_code] != NULL))
            {
                sys2cmac_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

 #if BSP_FEATURE_HAS_DSP

/**
 * \brief Main handler for the DSP2CMAC IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_dsp2cmac_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_dsp2cmac_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_DSP2CMAC_HANDLERS_MAX) && (dsp2cmac_handlers[event_code] != NULL))
            {
                dsp2cmac_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#else

/**
 * \brief Main handler for the SYS2DSP IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_sys2dsp_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_sys2dsp_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_SYS2DSP_HANDLERS_MAX) && (sys2dsp_handlers[event_code] != NULL))
            {
                sys2dsp_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

/**
 * \brief Main handler for the CMAC2DSP IPC channel.\n
 * This function is registered when using the high-level APIs. It is used to call the different handlers registered by the application
 *
 * \param [in] received_event The event that has been received
 *
 */
static void ipc_cmac2dsp_main_handler (ipc_event_type received_event)
{
    int16_t           status        = IPC_SUCCESS;
    ipc_event_code    event_code    = 0;
    ipc_event_payload event_payload = 0;
    do
    {
        status = ipc_cmac2dsp_next_pending(&received_event);
        if (status == IPC_SUCCESS)
        {
            event_code    = (ipc_event_code) IPC_EVENT_CODE_EXTRACT(received_event);
            event_payload = (ipc_event_payload) IPC_EVENT_PAYLOAD_EXTRACT(received_event);
            if ((event_code < IPC_CMAC2DSP_HANDLERS_MAX) && (cmac2dsp_handlers[event_code] != NULL))
            {
                cmac2dsp_handlers[event_code](event_code, event_payload);
            }
        }
    } while (status == IPC_SUCCESS);
}

#endif

/** @}*/
