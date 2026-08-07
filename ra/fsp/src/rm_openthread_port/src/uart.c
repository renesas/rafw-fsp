/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/* Copyright (C) 2026 Modified by Renesas Electronics Corporation and/or its affiliates */

#include "platform-ra.h"
#include "openthread-system.h"
#include "utils/code_utils.h"
#include "utils/uart.h"
#include "openthread/logging.h"
#include "r_uart_api.h"

#define PLAT_RA_RECV_CIRC_BUFF_SIZE    (256)

static uint8_t           sReceiveBuffer[PLAT_RA_RECV_CIRC_BUFF_SIZE];
static uint32_t          sReceiveHeadIdx = 0;
static volatile uint32_t sReceiveTailIdx = 0;

enum
{
    UART_IDLE,
    UART_TRANSMITING,
    UART_TRANSMIT_FINISHED,
    UART_ERROR
};

static volatile uint32_t uartTransmitState;
void otPlatReceiveByteEvtHandler(uint8_t char_byte);

static void uart_evt_callback (uart_callback_args_t * p_args)
{
    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            otPlatReceiveByteEvtHandler((uint8_t)p_args->data);
            break;
        }

        case UART_EVENT_RX_COMPLETE:
        {
            break;
        }

        case UART_EVENT_TX_COMPLETE:
        {
            break;
        }

        case UART_EVENT_TX_DATA_EMPTY:
        {
            if (uartTransmitState == UART_TRANSMITING)
            {
                uartTransmitState = UART_TRANSMIT_FINISHED;
            }

            otSysEventSignalPending();
            break;
        }

        case UART_EVENT_RX_TIMEOUT:
        {
            break;
        }

        default:
        {
            uartTransmitState = UART_ERROR;
            otSysEventSignalPending();
        }
    }
}

otError otPlatUartEnable (void)
{
    otError error = OT_ERROR_NONE;
    uartTransmitState = UART_IDLE;

	fsp_err_t err = gp_openthread_port_uart_instance->p_api->open(gp_openthread_port_uart_instance->p_ctrl,
						gp_openthread_port_uart_instance->p_cfg);
	OT_ASSERT(FSP_SUCCESS == err);

	err = gp_openthread_port_uart_instance->p_api->callbackSet(gp_openthread_port_uart_instance->p_ctrl,
			uart_evt_callback, NULL, NULL);
	OT_ASSERT(FSP_SUCCESS == err);

    return error;
}

otError otPlatUartDisable (void)
{
    otError error = OT_ERROR_NONE;

	fsp_err_t err = gp_openthread_port_uart_instance->p_api->close(gp_openthread_port_uart_instance->p_ctrl);
	OT_ASSERT(FSP_SUCCESS == err);

    return error;
}

otError otPlatUartSend (const uint8_t * aBuf, uint16_t aBufLength)
{
    otError error = OT_ERROR_NONE;

    uartTransmitState = UART_TRANSMITING;

	fsp_err_t err = gp_openthread_port_uart_instance->p_api->write(gp_openthread_port_uart_instance->p_ctrl,
						(uint8_t *) aBuf, aBufLength);
	if (err != FSP_SUCCESS)
	{
		otLogCritPlat("Error while writing %d bytes in uart %d", aBufLength, err);
		error = OT_ERROR_FAILED;
	}

    while (UART_TRANSMITING == uartTransmitState)
    {
    }

    return error;
}

void otPlatReceiveByteEvtHandler (uint8_t char_byte)
{
    sReceiveBuffer[sReceiveTailIdx] = char_byte;
    sReceiveTailIdx++;

    if (sReceiveTailIdx >= PLAT_RA_RECV_CIRC_BUFF_SIZE)
    {
        sReceiveTailIdx = 0;
    }

    otSysEventSignalPending();
}

/**
 * @brief process the receive side of the buffers
 */
static void processReceive (void)
{
    uint32_t tailIdx = sReceiveTailIdx; // Freeze tailIdx in an atomic operation as it might change via ISR.

    while (sReceiveHeadIdx != tailIdx)
    {
        if (sReceiveHeadIdx < tailIdx)
        {
            otPlatUartReceived(&(sReceiveBuffer[sReceiveHeadIdx]), (uint16_t)(tailIdx - sReceiveHeadIdx));
            sReceiveHeadIdx = tailIdx;
        }
        else
        {
            otPlatUartReceived(&(sReceiveBuffer[sReceiveHeadIdx]), (uint16_t)(PLAT_RA_RECV_CIRC_BUFF_SIZE - sReceiveHeadIdx));
            otPlatUartReceived(&(sReceiveBuffer[0]), (uint16_t)tailIdx);
            sReceiveHeadIdx = tailIdx;
        }
    }
}

/**
 * @brief process the transmit side of the buffers
 */
static void processTransmit (void)
{
    if (UART_TRANSMIT_FINISHED == uartTransmitState)
    {
        uartTransmitState = UART_IDLE;
        otPlatUartSendDone();
    }
    else if (UART_ERROR == uartTransmitState)
    {
        otLogWarnPlat("Error during UART transmission");
        uartTransmitState = UART_IDLE;
        otPlatUartSendDone();
    }
}

otError otPlatUartFlush (void)
{
    otError error = OT_ERROR_NONE;

    while (UART_TRANSMITING == uartTransmitState)
    {
    }

    if (UART_ERROR == uartTransmitState)
    {
        error = OT_ERROR_FAILED;
    }

    processTransmit();

    return error;
}

void otPlatUartProcess (void)
{
    processReceive();
    processTransmit();
}

#if OPENTHREAD_CONFIG_ENABLE_DEBUG_UART && (OPENTHREAD_CONFIG_LOG_OUTPUT == OPENTHREAD_CONFIG_LOG_OUTPUT_DEBUG_UART)

otError otPlatDebugUart_logfile (const char * filename)
{
    OT_UNUSED_VARIABLE(filename);

    return OT_ERROR_NONE;
}

void otPlatDebugUart_putchar_raw (int c)
{
    OT_UNUSED_VARIABLE(c);
}

int otPlatDebugUart_kbhit (void)
{
    /* not supported */
    return 0;
}

int otPlatDebugUart_getc (void)
{
    /* not supported */
    return -1;
}

#endif
