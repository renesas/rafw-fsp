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
#include "sdk_defs.h"
#include "r_spi_w.h"
#include "r_gpio_w.h"
#include "rm_atcmd_transport_spi_w.h"
#include "rm_atcmd_w_core.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "common_data.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Definitions of Open flag "DATU" */
#define ATCMD_TRANSPORT_SPI_W_OPEN      (0x455154AAU)
#define ATCMD_TRANSPORT_SPI_W_CLOSE     (0x00U)

#define ATCMD_SPI_TASK_PRIO             (configTIMER_TASK_PRIORITY-1)

#define ATCMD_SPI_BYTES_PER_TRANSACTION (4)
#define ATCMD_SPI_DATA_ALIGNMENT        (4)

#define HOST_WRITE_REQ                  (0x80)
#define HOST_READ_REQ                   (0xC0)
#define DEVICE_WRITE_RES                (0x81)

#define DEVICE_READ_RES                 (0x83)

#define DEVICE_ESC_RES                  (0x20)

#define DEVICE_RESPONSE_PAD             (0x00)

#define FSP_AT_GENCMD_ADDR              (0x50080254)    /// General Command address to Write Command
#define FSP_AT_RESP_ADDR                (0x50080258)    /// AT Command address to Read Response
#define FSP_AT_ATCMD_ADDR               (0x50080260)    /// Response Command address to Send AT Command

#define ATCMD_REQUEST_BUFFER_ADDR_3     (0)
#define ATCMD_REQUEST_BUFFER_ADDR_2     (1)
#define ATCMD_REQUEST_BUFFER_ADDR_1     (2)
#define ATCMD_REQUEST_BUFFER_ADDR_0     (3)
#define ATCMD_REQUEST_BUFFER_LEN_LSB    (7)
#define ATCMD_REQUEST_BUFFER_LEN_MSB    (6)

#define ATCMD_GENCMD_BUFFER_LEN_LSB     (ATCMD_SPI_BUFFER_HOST_DATA_REQ_OFFSET + 7)
#define ATCMD_GENCMD_BUFFER_LEN_MSB     (ATCMD_SPI_BUFFER_HOST_DATA_REQ_OFFSET + 6)

#define ATCMD_RESPONSE_BUFFER_ADDR_3    (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET)
#define ATCMD_RESPONSE_BUFFER_ADDR_2    (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 1)
#define ATCMD_RESPONSE_BUFFER_ADDR_1    (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 2)
#define ATCMD_RESPONSE_BUFFER_ADDR_0    (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 3)
#define ATCMD_RESPONSE_BUFFER_LEN_LSB   (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 4)
#define ATCMD_RESPONSE_BUFFER_LEN_MSB   (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 5)
#define ATCMD_RESPONSE_BUFFER_RES       (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 6)
#define ATCMD_RESPONSE_BUFFER_PAD       (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + 7)

#define STR_LEN(x)                      (sizeof(x) - 1)
#define MODULO(a, N)                    (((a) % (N) + (N)) % (N))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
static void     rm_atcmd_transport_spi_w_gpio_interrupt_set(atcmd_transport_w_ctrl_t * p_ctrl);
static void     rm_atcmd_transport_spi_w_gpio_interrupt_reset(atcmd_transport_w_ctrl_t * p_ctrl);
static uint16_t rm_atcmd_transport_spi_w_pad_comp(uint16_t length);
static void     rm_atcmd_transport_spi_w_transaction_init(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static void     rm_atcmd_transport_spi_w_transaction_process(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static void     rm_atcmd_transport_spi_w_fsm_reset(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static bool     rm_atcmd_transport_spi_w_fsm_initialized(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static void     rm_atcmd_transport_spi_w_fsm_cmd_rx(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static bool     rm_atcmd_transport_spi_w_fsm_cmd_process(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
static bool     rm_atcmd_transport_spi_w_fsm(atcmd_transport_spi_w_instance_ctrl_t * p_ctrl);
void            rm_atcmd_transport_spi_w_cb(spi_callback_args_t * p_args);
static void     rm_atcmd_transport_spi_w_dispatch_thread_func(void * p_param);

atcmd_transport_w_api_t const g_atcmd_transport_on_spi =
{
    .open                    = RM_ATCMD_TRANSPORT_SPI_W_Open,
    .close                   = RM_ATCMD_TRANSPORT_SPI_W_Close,
    .atCommandSendThreadSafe = RM_ATCMD_TRANSPORT_SPI_W_AtCmdSendThreadSafe,
    .atCommandSend           = RM_ATCMD_TRANSPORT_SPI_W_AtCmdSend,
    .giveMutex               = RM_ATCMD_TRANSPORT_SPI_W_GiveMutex,
    .takeMutex               = RM_ATCMD_TRANSPORT_SPI_W_TakeMutex,
    .bufferRecv              = RM_ATCMD_TRANSPORT_SPI_W_BufferRecv,
    .statusGet               = RM_ATCMD_TRANSPORT_SPI_W_StatusGet,
};

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

static void rm_atcmd_transport_spi_w_gpio_interrupt_set (atcmd_transport_w_ctrl_t * p_ctrl)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;
    atcmd_transport_spi_w_extended_cfg_t * p_transport_extended_cfg;
    p_transport_extended_cfg = (atcmd_transport_spi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    /* Trigger GPIO interrupt line to high to signal spi-master side */
    R_GPIO_W_PinWrite(&g_gpio_w_ctrl, p_transport_extended_cfg->reset_pin, BSP_IO_LEVEL_HIGH);
}

static void rm_atcmd_transport_spi_w_gpio_interrupt_reset (atcmd_transport_w_ctrl_t * p_ctrl)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;
    atcmd_transport_spi_w_extended_cfg_t * p_transport_extended_cfg;
    p_transport_extended_cfg = (atcmd_transport_spi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    /* Reset GPIO interrupt line to low (default value) */
    R_GPIO_W_PinWrite(&g_gpio_w_ctrl, p_transport_extended_cfg->reset_pin, BSP_IO_LEVEL_LOW);
}

static uint16_t rm_atcmd_transport_spi_w_pad_comp (uint16_t length)
{
    /* Buffer Length field contains the should be a multiple of 4.
       The padding field contains number of padded bytes in the Data field due to 4-byte aligned.
       For example, if the length of the actual data is 11 bytes, the Buffer Length will be 12. */
    if (0 != length)
    {
        return ATCMD_SPI_DATA_ALIGNMENT * ((length + ATCMD_SPI_DATA_ALIGNMENT - 1) / ATCMD_SPI_DATA_ALIGNMENT);
    }
    else
    {
        return ATCMD_SPI_DATA_ALIGNMENT;
    }
}

static void rm_atcmd_transport_spi_w_transaction_init (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    spi_instance_t * p_spi      = p_ctrl->spi_instance_objects[0];

    /* Expect the header from the host. */
    p_spi->p_api->writeRead(p_spi->p_ctrl,
                            p_ctrl->cmd_tx_buf,
                            p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head],
                            p_ctrl->cmd_spi_length / ATCMD_SPI_BYTES_PER_TRANSACTION,
                            SPI_BIT_WIDTH_32_BITS);

    p_ctrl->spi_sys_state = ATCMD_TRANSPORT_SPI_W_STAT_INITIALIZED;

    /* We are ready to accept data from host */
    rm_atcmd_transport_spi_w_gpio_interrupt_reset(p_ctrl);
}

static void rm_atcmd_transport_spi_w_transaction_process (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    uint32_t         op_offset  = ATCMD_SPI_BUFFER_START_OFFSET;
    spi_instance_t * p_spi      = p_ctrl->spi_instance_objects[0];

    if ((0 != p_ctrl->bytes_to_send) || (ATCMD_TRANSPORT_SPI_W_OP_READ != p_ctrl->spi_sys_op))
    {
        uint32_t data_addr       = 0;
        uint16_t data_len_adj    = 0;
        uint8_t  resp_op         = DEVICE_ESC_RES;
        uint16_t transaction_len = ATCMD_SPI_TRANSFER_HEADER_SIZE;

        p_ctrl->spi_sys_state = ATCMD_TRANSPORT_SPI_W_STAT_CMD_PROCESS;

        /* Handle GEN_CMD first as it was initiated by the Host. */
        if (ATCMD_TRANSPORT_SPI_W_OP_WRITE == p_ctrl->spi_sys_op)
        {
            data_len_adj        = p_ctrl->bytes_to_receive;
            data_addr           = (uint32_t) (p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head] +
                                              ATCMD_SPI_BUFFER_DATA_OFFSET);
            resp_op             = DEVICE_WRITE_RES;
            transaction_len     = ATCMD_SPI_BUFFER_DATA_OFFSET + data_len_adj;
        }
        else if (ATCMD_TRANSPORT_SPI_W_OP_READ == p_ctrl->spi_sys_op)
        {
            if (AT_CMD_ESC_KEY_CHAR != *(p_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET))
            {
                data_addr       = (uint32_t) (p_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET);
                resp_op         = DEVICE_READ_RES;
                transaction_len = ATCMD_SPI_BUFFER_DATA_OFFSET + p_ctrl->bytes_to_send;
                data_len_adj    = p_ctrl->bytes_to_send;
            }
            else
            {
                char * esc_res  = (char *) (p_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET);

                if (0 == strncmp(esc_res, ATCMD_ESC_OK, STR_LEN(ATCMD_ESC_OK)))
                {
                    resp_op         = DEVICE_ESC_RES;
                    transaction_len = ATCMD_SPI_BUFFER_HOST_DATA_REQ_OFFSET;
                }
                else
                {
                    resp_op         = strtol(esc_res + STR_LEN(ATCMD_ESC_ERROR), NULL, 16);
                    transaction_len = ATCMD_SPI_BUFFER_DATA_OFFSET + p_ctrl->bytes_to_send;
                    data_len_adj    = p_ctrl->bytes_to_send;

                    /* delete '\e' */
                    memmove(esc_res, esc_res + 1, p_ctrl->bytes_to_send - 1);
                    esc_res[p_ctrl->bytes_to_send - 1] = '\0';
                }

                data_addr = (uint32_t) 0xffffffff; /* ESC response address. */
            }
        }
        else
        {
            /* Shouldn't reach this line. */
            assert(false);
        }

        p_ctrl->cmd_spi_length = transaction_len;
        op_offset              = ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET;

        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_ADDR_3]  = (data_addr >> 24) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_ADDR_2]  = (data_addr >> 16) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_ADDR_1]  = (data_addr >> 8) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_ADDR_0]  = (data_addr) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_LEN_LSB] = (data_len_adj) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_LEN_MSB] = (data_len_adj >> 8) & 0xFF;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_RES]     = resp_op;
        p_ctrl->cmd_tx_buf[ATCMD_RESPONSE_BUFFER_PAD]     = DEVICE_RESPONSE_PAD;
    }
    else
    {
        /* Read host data. */
        p_ctrl->cmd_spi_length = ATCMD_SPI_TRANSFER_HEADER_SIZE;
    }

    p_spi->p_api->writeRead(p_spi->p_ctrl,
                            p_ctrl->cmd_tx_buf + op_offset,
                            p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head] + op_offset,
                            (p_ctrl->cmd_spi_length - op_offset) / ATCMD_SPI_BYTES_PER_TRANSACTION,
                            SPI_BIT_WIDTH_32_BITS);

    /* We are ready to accept data from host */
    rm_atcmd_transport_spi_w_gpio_interrupt_reset(p_ctrl);
}

static void rm_atcmd_transport_spi_w_fsm_reset (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    p_ctrl->bytes_to_receive = 0;
    p_ctrl->bytes_to_send    = 0;
    p_ctrl->spi_sys_state    = ATCMD_TRANSPORT_SPI_W_STAT_IDLE;
    p_ctrl->spi_sys_op       = ATCMD_TRANSPORT_SPI_W_OP_READ;
    p_ctrl->cmd_spi_length   = ATCMD_SPI_TRANSFER_HEADER_SIZE;

    rm_atcmd_transport_spi_w_transaction_init(p_ctrl);
}

static bool rm_atcmd_transport_spi_w_fsm_initialized (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    uint8_t * cmd_rx_buf   = p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head]; /* Current RX buffer */
    /* Extract the command address */
    uint32_t  address_type = ((cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_3] << 24) |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_2] << 16) |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_1] << 8)  |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_0]));
    switch (address_type)
    {
        case FSP_AT_GENCMD_ADDR:
        case FSP_AT_ATCMD_ADDR:
        {
            uint16_t payload_len   = ((cmd_rx_buf[ATCMD_REQUEST_BUFFER_LEN_MSB] << 8) |
                                      (cmd_rx_buf[ATCMD_REQUEST_BUFFER_LEN_LSB]));
            spi_instance_t * p_spi = p_ctrl->spi_instance_objects[0];

            p_ctrl->cmd_spi_length = MIN(payload_len, ATCMD_SPI_TRANSFER_DATA_MAX);

            p_ctrl->spi_sys_op     = ((FSP_AT_ATCMD_ADDR == address_type) ?
                                      (ATCMD_TRANSPORT_SPI_W_OP_CMD) :
                                      (ATCMD_TRANSPORT_SPI_W_OP_WRITE));

            p_ctrl->spi_sys_state  = ATCMD_TRANSPORT_SPI_W_STAT_CMD_RX;

            /* Read remaining data. */
            p_spi->p_api->writeRead(p_spi->p_ctrl,
                                    p_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET,
                                    cmd_rx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET,
                                    p_ctrl->cmd_spi_length / ATCMD_SPI_BYTES_PER_TRANSACTION,
                                    SPI_BIT_WIDTH_32_BITS);

            break;
        }

        case FSP_AT_RESP_ADDR:
        {
            rm_atcmd_transport_spi_w_transaction_process(p_ctrl);

            break;
        }

        default:
        {
            /* Corrupted frame received. */

            p_ctrl->corrupted_frames++;

            rm_atcmd_transport_spi_w_fsm_reset(p_ctrl);

            return true;
        }
    }

    return false;
}

static void rm_atcmd_transport_spi_w_fsm_cmd_rx (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    if (ATCMD_TRANSPORT_SPI_W_OP_WRITE == p_ctrl->spi_sys_op)
    {
        p_ctrl->bytes_to_receive = MIN(((p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head][0]) |
                                        (p_ctrl->cmd_rx_buf[p_ctrl->cmd_rx_buf_head][1] << 8)),
                                       ATCMD_SPI_TRANSFER_DATA_MAX);

        /* We are preparing GEN_CMD operation. */
        rm_atcmd_transport_spi_w_gpio_interrupt_set(p_ctrl);
    }
    else
    {
        BaseType_t unblock_rx_thread = pdFALSE;

        /* Update the RX buffer write index */
        p_ctrl->cmd_rx_buf_head = (p_ctrl->cmd_rx_buf_head + 1) % ATCMD_SPI_INPUT_BUFFER_COUNT;

        /* Wake up the processing task */
        xSemaphoreGiveFromISR(p_ctrl->rx_sem, &unblock_rx_thread);
        portYIELD_FROM_ISR(unblock_rx_thread);
    }

    /* For simple AT CMD we are done here. GEN_CMD should be handled separately. */
    p_ctrl->spi_sys_op       = ((ATCMD_TRANSPORT_SPI_W_OP_CMD == p_ctrl->spi_sys_op) ?
                                (ATCMD_TRANSPORT_SPI_W_OP_READ) :
                                p_ctrl->spi_sys_op);
    p_ctrl->cmd_spi_length   = ATCMD_SPI_TRANSFER_HEADER_SIZE;
    p_ctrl->spi_sys_state    = ATCMD_TRANSPORT_SPI_W_STAT_IDLE;

    rm_atcmd_transport_spi_w_transaction_init(p_ctrl);
}

static bool rm_atcmd_transport_spi_w_fsm_cmd_process (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    if (ATCMD_TRANSPORT_SPI_W_OP_WRITE == p_ctrl->spi_sys_op)
    {
        BaseType_t unblock_rx_thread = pdFALSE;

        /* Update the RX buffer write index */
        p_ctrl->cmd_rx_buf_head = (p_ctrl->cmd_rx_buf_head + 1) % ATCMD_SPI_INPUT_BUFFER_COUNT;

        /* Wake up the processing task */
        xSemaphoreGiveFromISR(p_ctrl->rx_sem, &unblock_rx_thread);
        portYIELD_FROM_ISR(unblock_rx_thread);

        /* Process remaining read operation if scheduled. */
        if (0 != p_ctrl->bytes_to_send)
        {
            rm_atcmd_transport_spi_w_gpio_interrupt_set(p_ctrl);

            p_ctrl->bytes_to_receive = 0;
            p_ctrl->spi_sys_state    = ATCMD_TRANSPORT_SPI_W_STAT_IDLE;
            p_ctrl->spi_sys_op       = ATCMD_TRANSPORT_SPI_W_OP_READ;

            rm_atcmd_transport_spi_w_transaction_init(p_ctrl);
        }
    }

    rm_atcmd_transport_spi_w_fsm_reset(p_ctrl);

    return !xMessageBufferIsEmpty(p_ctrl->tx_queue_hdl);
}

static bool rm_atcmd_transport_spi_w_fsm (atcmd_transport_spi_w_instance_ctrl_t * p_ctrl)
{
    bool unblock_task = false;

    switch(p_ctrl->spi_sys_state)
    {
        case ATCMD_TRANSPORT_SPI_W_STAT_INITIALIZED:
        {
            unblock_task = rm_atcmd_transport_spi_w_fsm_initialized(p_ctrl);
        }
        break;

        case ATCMD_TRANSPORT_SPI_W_STAT_CMD_RX:
        {
            rm_atcmd_transport_spi_w_fsm_cmd_rx(p_ctrl);
        }
        break;

        case ATCMD_TRANSPORT_SPI_W_STAT_CMD_PROCESS:
        {
            unblock_task = rm_atcmd_transport_spi_w_fsm_cmd_process(p_ctrl);
        }
        break;

        default:
        {
            /* Shouldn't reach this line. */
            assert(false);
        }
        break;
    }

    return unblock_task;
}

void rm_atcmd_transport_spi_w_cb (spi_callback_args_t * p_args)
{
    bool unblock_task                              = true;
    atcmd_transport_spi_w_instance_ctrl_t * p_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_args->p_context;

    switch (p_args->event)
    {
        case SPI_EVENT_TRANSFER_COMPLETE:
        {
            unblock_task = rm_atcmd_transport_spi_w_fsm(p_ctrl);
            break;
        }

        case SPI_EVENT_TRANSFER_ABORTED:
        case SPI_EVENT_ERR_MODE_FAULT:
        case SPI_EVENT_ERR_READ_OVERFLOW:
        case SPI_EVENT_ERR_PARITY:
        case SPI_EVENT_ERR_OVERRUN:
        case SPI_EVENT_ERR_FRAMING:
        case SPI_EVENT_ERR_MODE_UNDERRUN:
        default:

        {
            rm_atcmd_transport_spi_w_fsm_reset(p_ctrl);
            break;
        }
    }

    if (unblock_task)
    {
        BaseType_t need_to_yield = pdFALSE;

        /* Wake up the processing task */
        xSemaphoreGiveFromISR(p_ctrl->spi_sem, &need_to_yield);
        portYIELD_FROM_ISR(need_to_yield);
    }
}

static void rm_atcmd_transport_spi_w_dispatch_thread_func (void * p_param)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = p_param;

    /* Start a write/read transfer */
    while (1)
    {
        FSP_CRITICAL_SECTION_DEFINE;
        bool force_output = false;
        bool data_avail   = !xMessageBufferIsEmpty(p_instance_ctrl->tx_queue_hdl);

        while (0 != (p_instance_ctrl->corrupted_frames - p_instance_ctrl->corrupted_before))
        {
            force_output                      = true;
            p_instance_ctrl->corrupted_before = p_instance_ctrl->corrupted_frames;

            /* Reschedule to give FSM time to clear erroneous input before trying to output data from our side. */
            vTaskDelay(1);
        }

        if ((false != data_avail) && (0 == p_instance_ctrl->bytes_to_send))
        {
            uint16_t data_len     = 0;
            uint16_t data_len_adj = 0;

            data_len = xMessageBufferReceive(p_instance_ctrl->tx_queue_hdl,
                                             p_instance_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET,
                                             ATCMD_SPI_TRANSFER_DATA_MAX,
                                             0);
            data_len = MIN(ATCMD_SPI_TRANSFER_DATA_MAX, data_len);
            if (0 != data_len)
            {
                data_len_adj               = rm_atcmd_transport_spi_w_pad_comp(data_len);

                /* Only padded bytes should be set to 0 */
                memset(p_instance_ctrl->cmd_tx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET + data_len,
                       0x0,
                       data_len_adj - data_len);
            }

            FSP_CRITICAL_SECTION_ENTER;
            /* Try to acquire the time slot for output */
            rm_atcmd_transport_spi_w_gpio_interrupt_set(p_instance_ctrl);
            p_instance_ctrl->bytes_to_send = data_len_adj;
            FSP_CRITICAL_SECTION_EXIT;
        }

        FSP_CRITICAL_SECTION_ENTER;
        if (ATCMD_TRANSPORT_SPI_W_STAT_IDLE == p_instance_ctrl->spi_sys_state || force_output)
        {
            rm_atcmd_transport_spi_w_transaction_init(p_instance_ctrl);
        }

        if (ATCMD_TRANSPORT_SPI_W_STAT_INITIALIZED == p_instance_ctrl->spi_sys_state)
        {
            /* We are ready to accept data from host */
            rm_atcmd_transport_spi_w_gpio_interrupt_reset(p_instance_ctrl);
        }
        FSP_CRITICAL_SECTION_EXIT;

        /* Wait for SPI_EVENT_TRANSFER_COMPLETE callback event. */
        xSemaphoreTake(p_instance_ctrl->spi_sem, portMAX_DELAY);

        if (ATCMD_TRANSPORT_SPI_W_OPEN != p_instance_ctrl->open)
        {
            /* We've closed the transport from another thread - perform graceful exit. */
            return;
        }
    }
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_Open (atcmd_transport_w_ctrl_t            * p_ctrl,
                                         atcmd_transport_w_cfg_t const * const p_cfg)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;
    atcmd_transport_spi_w_extended_cfg_t * p_transport_extended_cfg;

    #if (ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    #endif

    /* Update control structure from configuration values */
    p_instance_ctrl->p_cfg            = p_cfg;
    p_instance_ctrl->bytes_to_receive = 0;
    p_instance_ctrl->spi_sys_state    = ATCMD_TRANSPORT_SPI_W_STAT_IDLE;
    p_instance_ctrl->spi_sys_op       = ATCMD_TRANSPORT_SPI_W_OP_READ;
    p_instance_ctrl->cmd_spi_length   = ATCMD_SPI_TRANSFER_HEADER_SIZE;
    p_instance_ctrl->cmd_rx_buf_head  = 0;
    p_instance_ctrl->cmd_rx_buf_tail  = 0;
    p_instance_ctrl->corrupted_frames = 0;
    p_instance_ctrl->corrupted_before = 0;

    p_transport_extended_cfg = (atcmd_transport_spi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    p_instance_ctrl->tx_queue_hdl = xMessageBufferCreateStatic(sizeof(p_instance_ctrl->tx_queue_buf),
                                                               p_instance_ctrl->tx_queue_buf,
                                                               &p_instance_ctrl->tx_queue_data);

    p_instance_ctrl->tx_mutex     = xSemaphoreCreateMutexStatic(&p_instance_ctrl->tx_mutex_data);
    p_instance_ctrl->rx_mutex     = xSemaphoreCreateMutexStatic(&p_instance_ctrl->rx_mutex_data);


    for (uint32_t i = 0; i < p_transport_extended_cfg->num_spis; i++)
    {
        p_instance_ctrl->spi_instance_objects[i] = (spi_instance_t *) p_transport_extended_cfg->spi_instances[i];
    }

    p_instance_ctrl->spi_sem = xSemaphoreCreateBinaryStatic(&p_instance_ctrl->spi_sem_data);
    p_instance_ctrl->rx_sem  = xSemaphoreCreateCountingStatic(ATCMD_SPI_INPUT_BUFFER_COUNT,
                                                              0,
                                                              &p_instance_ctrl->rx_sem_data);

    spi_instance_t * p_spi   = p_instance_ctrl->spi_instance_objects[0];

    err = p_spi->p_api->open(p_spi->p_ctrl, p_spi->p_cfg);

    if (FSP_SUCCESS != err)
    {
        xSemaphoreGive(p_instance_ctrl->rx_sem);
        vSemaphoreDelete(p_instance_ctrl->rx_sem);

        xSemaphoreGive(p_instance_ctrl->spi_sem);
        vSemaphoreDelete(p_instance_ctrl->spi_sem);

        vSemaphoreDelete(p_instance_ctrl->tx_mutex);
        vSemaphoreDelete(p_instance_ctrl->rx_mutex);

        vMessageBufferDelete(p_instance_ctrl->tx_queue_hdl);

        return err;
    }

    p_instance_ctrl->dispatch_task = xTaskCreateStatic(rm_atcmd_transport_spi_w_dispatch_thread_func,
                                                       "ATCMD_SPI_DISPATCH",
                                                       ATCMD_SPI_TASK_STACK_SIZE,
                                                       p_instance_ctrl,
                                                       ATCMD_SPI_TASK_PRIO,
                                                       p_instance_ctrl->dispatch_task_stack,
                                                       &p_instance_ctrl->dispatch_task_data);

    p_instance_ctrl->open = ATCMD_TRANSPORT_SPI_W_OPEN;

    return FSP_SUCCESS;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_Close (atcmd_transport_w_ctrl_t * const p_ctrl)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Clean up output buffer. */
    if (pdFAIL == xMessageBufferReset(p_instance_ctrl->tx_queue_hdl))
    {
        /* Sending task is waiting for host to pick up data. */
        return FSP_ERR_IN_USE;
    }

    p_instance_ctrl->open = ATCMD_TRANSPORT_SPI_W_CLOSE;

    spi_instance_t * p_spi = p_instance_ctrl->spi_instance_objects[0];

    err = p_spi->p_api->close(p_spi->p_ctrl);

    vTaskDelete(p_instance_ctrl->dispatch_task);

    xSemaphoreGive(p_instance_ctrl->rx_sem);
    vSemaphoreDelete(p_instance_ctrl->rx_sem);

    xSemaphoreGive(p_instance_ctrl->spi_sem);
    vSemaphoreDelete(p_instance_ctrl->spi_sem);

    vSemaphoreDelete(p_instance_ctrl->tx_mutex);
    vSemaphoreDelete(p_instance_ctrl->rx_mutex);

    vMessageBufferDelete(p_instance_ctrl->tx_queue_hdl);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_AtCmdSendThreadSafe (atcmd_transport_w_ctrl_t * const p_ctrl,
                                                        atcmd_transport_w_data_t       * p_at_cmd)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_AtCmdSend (atcmd_transport_w_ctrl_t * const p_ctrl,
                                              atcmd_transport_w_data_t       * p_at_cmd)
{
    fsp_err_t                               err             = FSP_SUCCESS;
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* It can be only one writer at the time. */
    xSemaphoreTake(p_instance_ctrl->tx_mutex, portMAX_DELAY);

    if (ATCMD_SPI_TRANSFER_DATA_MAX >= p_at_cmd->at_cmd_string_length)
    {
        xMessageBufferSend(p_instance_ctrl->tx_queue_hdl,
                           p_at_cmd->p_at_cmd_string,
                           p_at_cmd->at_cmd_string_length,
                           portMAX_DELAY);

        /* Wake up the task. */
        xSemaphoreGive(p_instance_ctrl->spi_sem);
    }
    else
    {
        err = FSP_ERR_INVALID_SIZE;
    }

    xSemaphoreGive(p_instance_ctrl->tx_mutex);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_GiveMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    FSP_PARAMETER_NOT_USED(mutex_flag);

    return FSP_SUCCESS;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_TakeMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    FSP_PARAMETER_NOT_USED(mutex_flag);

    return FSP_SUCCESS;
}

fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_StatusGet (atcmd_transport_w_ctrl_t * const p_ctrl,
                                              atcmd_transport_w_status_t     * p_status)
{
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ASSERT(p_status != NULL);
#endif

    p_status->open = (ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open);

    return FSP_SUCCESS;
}

size_t RM_ATCMD_TRANSPORT_SPI_W_BufferRecv (atcmd_transport_w_ctrl_t * const p_ctrl,
                                            char                           * p_data,
                                            uint32_t                         length,
                                            uint32_t                         rx_timeout)
{
    size_t                                  recv_len        = 0;
    atcmd_transport_spi_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_spi_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ERROR_RETURN(p_instance_ctrl != NULL, 0);
    FSP_ERROR_RETURN(p_data != NULL, 0);
    FSP_ERROR_RETURN(length != 0, 0);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SPI_W_OPEN == p_instance_ctrl->open, 0);
#endif

    /* Lock SPI until the reception is complete */
    xSemaphoreTake(p_instance_ctrl->rx_mutex, portMAX_DELAY);

    /* There is no buffered messages available. Wait for the external event */
    if (pdFALSE == xSemaphoreTake(p_instance_ctrl->rx_sem, rx_timeout))
    {
        /* RX waiting timeout. */
        xSemaphoreGive(p_instance_ctrl->rx_mutex);

        return 0;
    }

    if (ATCMD_TRANSPORT_SPI_W_OPEN != p_instance_ctrl->open)
    {
        /* We've closed the transport from another thread - perform graceful exit. */
        return 0;
    }

    uint8_t * cmd_rx_buf   = p_instance_ctrl->cmd_rx_buf[p_instance_ctrl->cmd_rx_buf_tail]; /* Current RX buffer */
    /* Extract the command address */
    uint32_t  address_type = ((cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_3] << 24) |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_2] << 16) |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_1] << 8)  |
                              (cmd_rx_buf[ATCMD_REQUEST_BUFFER_ADDR_0]));

    if (FSP_AT_ATCMD_ADDR == address_type)
    {
        recv_len = ((cmd_rx_buf[ATCMD_REQUEST_BUFFER_LEN_MSB] << 8)  |
                    (cmd_rx_buf[ATCMD_REQUEST_BUFFER_LEN_LSB]));
    }
    else /* FSP_AT_GENCMD_ADDR */
    {
        recv_len = ((cmd_rx_buf[ATCMD_GENCMD_BUFFER_LEN_MSB] << 8)  |
                    (cmd_rx_buf[ATCMD_GENCMD_BUFFER_LEN_LSB]));
    }

    recv_len = MIN(recv_len, ATCMD_SPI_TRANSFER_DATA_MAX);
    recv_len = MIN(recv_len, length);
    memcpy(p_data, cmd_rx_buf + ATCMD_SPI_BUFFER_DATA_OFFSET, recv_len);

    /* Update read index */
    p_instance_ctrl->cmd_rx_buf_tail = (p_instance_ctrl->cmd_rx_buf_tail + 1) % ATCMD_SPI_INPUT_BUFFER_COUNT;

    /* SPI is now available for other read requests */
    xSemaphoreGive(p_instance_ctrl->rx_mutex);

    return recv_len;
}
