/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "r_gpio_w.h"
#include "rm_atcmd_transport_uart_w.h"
#include "r_uart_w.h"
#include "rm_atcmd_w_core.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "common_data.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

extern void uart_w_isr(void);

/* Definitions of Open flag "DATU" */
#define ATCMD_TRANSPORT_UART_W_OPEN     (0x44415455U)
#define ATCMD_TRANSPORT_UART_W_CLOSE    (0x00U)

#define WATERMARK_REACHED(x)            ((x) > (ATCMD_TRANSPORT_W_CFG_CMD_RX_BUF_SIZE * 4 / 5))

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
atcmd_transport_w_api_t const g_atcmd_transport_on_uart =
{
    .open  = RM_ATCMD_TRANSPORT_UART_W_Open,
    .close = RM_ATCMD_TRANSPORT_UART_W_Close,
    .atCommandSendThreadSafe = RM_ATCMD_TRANSPORT_UART_W_AtCmdSendThreadSafe,
    .atCommandSend           = RM_ATCMD_TRANSPORT_UART_W_AtCmdSend,
    .giveMutex               = RM_ATCMD_TRANSPORT_UART_W_GiveMutex,
    .takeMutex               = RM_ATCMD_TRANSPORT_UART_W_TakeMutex,
    .bufferRecv              = RM_ATCMD_TRANSPORT_UART_W_BufferRecv,
    .statusGet               = RM_ATCMD_TRANSPORT_UART_W_StatusGet,
};

/***********************************************************************************************************************
 * Static Globals
 **********************************************************************************************************************/

fsp_err_t rm_atcmd_transport_uart_w_write(atcmd_transport_w_ctrl_t * p_ctrl, const uint8_t * buff, size_t buff_size);

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

void rm_atcmd_transport_uart_w_cb (uart_callback_args_t * p_args)
{
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl =
        (atcmd_transport_uart_w_instance_ctrl_t *) p_args->p_context;
    uart_instance_t                        * p_uart          = p_instance_ctrl->uart_instance_objects[0];
    BaseType_t                               wake_up_task    = pdFALSE;

    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            xStreamBufferSendFromISR(p_instance_ctrl->socket_byteq_hdl,
                                     (const void *) &p_args->data,
                                     1,
                                     &wake_up_task);

            if (WATERMARK_REACHED(xStreamBufferBytesAvailable(p_instance_ctrl->socket_byteq_hdl)))
            {
                p_uart->p_api->receiveSuspend(p_uart->p_ctrl);
            }

            portYIELD_FROM_ISR(wake_up_task);

            break;
        }

        case UART_EVENT_TX_COMPLETE:
        {
            if (xPortIsInsideInterrupt())
            {
                if (pdTRUE == xSemaphoreGiveFromISR(p_instance_ctrl->tx_sem,  &wake_up_task))
                {
                    portYIELD_FROM_ISR(wake_up_task);
                }
            }
            else
            {
                xSemaphoreGive(p_instance_ctrl->tx_sem);
            }

            break;
        }

        default:
        {
            /* ERROR CASE */
            break;
        }
    }
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_Open (atcmd_transport_w_ctrl_t            * p_ctrl,
                                          atcmd_transport_w_cfg_t const * const p_cfg)
{
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t         err    = FSP_SUCCESS;
    uart_instance_t * p_uart = NULL;
    atcmd_transport_uart_w_extended_cfg_t * p_transport_extended_cfg;

#if (ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_UART_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    /* Update control structure from configuration values */
    p_instance_ctrl->p_cfg     = p_cfg;
    p_transport_extended_cfg   = (atcmd_transport_uart_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    p_instance_ctrl->curr_cmd_port = 0;

    p_instance_ctrl->tx_sem           = xSemaphoreCreateBinaryStatic(&p_instance_ctrl->tx_sem_data);
    p_instance_ctrl->tx_mutex         = xSemaphoreCreateMutexStatic(&p_instance_ctrl->tx_mutex_data);
    p_instance_ctrl->rx_mutex         = xSemaphoreCreateMutexStatic(&p_instance_ctrl->rx_mutex_data);
    p_instance_ctrl->socket_byteq_hdl = xStreamBufferCreateStatic(sizeof(p_instance_ctrl->cmd_rx_queue_buf),
                                                                  1,
                                                                  p_instance_ctrl->cmd_rx_queue_buf,
                                                                  &p_instance_ctrl->socket_byteq_struct);

    for (uint32_t i = 0; i < p_transport_extended_cfg->num_uarts; i++)
    {
        p_instance_ctrl->uart_instance_objects[i] = (uart_instance_t *) p_transport_extended_cfg->uart_instances[i];
    }

    p_uart = p_instance_ctrl->uart_instance_objects[0];
    err    = p_uart->p_api->open(p_uart->p_ctrl, p_uart->p_cfg);
    FSP_ASSERT(err == FSP_SUCCESS);

    /* Open UART port */
    p_instance_ctrl->open = ATCMD_TRANSPORT_UART_W_OPEN;

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_Close (atcmd_transport_w_ctrl_t * const p_ctrl)
{
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;
    uart_instance_t * p_uart = p_instance_ctrl->uart_instance_objects[0];
    fsp_err_t         err    = FSP_SUCCESS;

    if (p_instance_ctrl->open != ATCMD_TRANSPORT_UART_W_OPEN)
    {
        return FSP_ERR_NOT_OPEN;
    }

    /* Clean up output buffer. */
    if (pdFAIL == xStreamBufferReset(p_instance_ctrl->socket_byteq_hdl))
    {
        /* Sending task is waiting for host to pick up data. */
        return FSP_ERR_IN_USE;
    }

    err = p_uart->p_api->close(p_uart->p_ctrl);

    p_instance_ctrl->open = ATCMD_TRANSPORT_UART_W_CLOSE;

    vStreamBufferDelete(p_instance_ctrl->socket_byteq_hdl);
    vSemaphoreDelete(p_instance_ctrl->rx_mutex);
    vSemaphoreDelete(p_instance_ctrl->tx_mutex);
    vSemaphoreDelete(p_instance_ctrl->tx_sem);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_AtCmdSendThreadSafe (atcmd_transport_w_ctrl_t * const p_ctrl,
                                                         atcmd_transport_w_data_t       * p_at_cmd)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_at_cmd);

    fsp_err_t err = FSP_SUCCESS;

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_AtCmdSend (atcmd_transport_w_ctrl_t * const p_ctrl,
                                               atcmd_transport_w_data_t       * p_at_cmd)
{
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    err = rm_atcmd_transport_uart_w_write(p_instance_ctrl,
                                          (const uint8_t *) p_at_cmd->p_at_cmd_string,
                                          p_at_cmd->at_cmd_string_length);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_GiveMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(mutex_flag);

    fsp_err_t err = FSP_SUCCESS;

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_TakeMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(mutex_flag);

    fsp_err_t err = FSP_SUCCESS;

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_UART_W_StatusGet (atcmd_transport_w_ctrl_t * const p_ctrl,
                                               atcmd_transport_w_status_t     * p_status)
{
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ASSERT(p_status != NULL);
#endif

    p_status->open = (ATCMD_TRANSPORT_UART_W_OPEN == p_instance_ctrl->open);

    return FSP_SUCCESS;
}

size_t RM_ATCMD_TRANSPORT_UART_W_BufferRecv (atcmd_transport_w_ctrl_t * const p_ctrl,
                                             char                           * p_data,
                                             uint32_t                         length,
                                             uint32_t                         rx_timeout)
{
    size_t                                   recv_len        = 0;
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;
    uart_instance_t                        * p_uart          = NULL;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ERROR_RETURN(p_instance_ctrl != NULL, 0);
    FSP_ERROR_RETURN(p_instance_ctrl->uart_instance_objects[0] != NULL, 0);
    FSP_ERROR_RETURN(p_data != NULL, 0);
    FSP_ERROR_RETURN(length != 0, 0);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_UART_W_OPEN == p_instance_ctrl->open, 0);
#endif

    p_uart = p_instance_ctrl->uart_instance_objects[0];

    /* Lock UART until the reception is complete */
    xSemaphoreTake(p_instance_ctrl->rx_mutex, portMAX_DELAY);

    recv_len = xStreamBufferReceive(p_instance_ctrl->socket_byteq_hdl, p_data, length, rx_timeout);

    if (pdTRUE == xStreamBufferIsEmpty(p_instance_ctrl->socket_byteq_hdl))
    {
        p_uart->p_api->receiveResume(p_uart->p_ctrl);
    }

    /* UART is now available for other read requests */
    xSemaphoreGive(p_instance_ctrl->rx_mutex);

    return recv_len;
}

fsp_err_t rm_atcmd_transport_uart_w_write (atcmd_transport_w_ctrl_t * p_ctrl, const uint8_t * buff, size_t buff_size)
{
    fsp_err_t                                ret             = FSP_SUCCESS;
    atcmd_transport_uart_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_uart_w_instance_ctrl_t *) p_ctrl;
    uart_instance_t                        * p_uart          = p_instance_ctrl->uart_instance_objects[0];

    /* Lock UART until the transmission is complete */
    xSemaphoreTake(p_instance_ctrl->tx_mutex, portMAX_DELAY);

    ret = p_uart->p_api->write(p_uart->p_ctrl, buff, buff_size);

    if (ret != FSP_SUCCESS)
    {
        goto rm_atcmd_transport_uart_w_write_exit;
    }

    /* Wait until we finish transmitting data */
    ret = xSemaphoreTake(p_instance_ctrl->tx_sem, pdMS_TO_TICKS(5000));

rm_atcmd_transport_uart_w_write_exit:

    /* UART is now available for other transmissions */
    xSemaphoreGive(p_instance_ctrl->tx_mutex);

    return ret;
}
