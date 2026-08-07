/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 ****************************************************************************************
 *
 * @file rm_atcmd_w_core_blebrg.c
 *
 * @brief rm_atcmd_w ble bridge application
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

/*******************************************************************************************************************//**
 * @addtogroup BLE_LOADER_IOPORT
 * @{
 ***********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#if (ATCMD_BLE_BRG == 1)
#include <stdarg.h>
#include "bsp_api.h"
#include "bsp_common.h"
#include "r_gpio_w.h"

#include "r_uart_w.h"
#include "r_uart_w_cfg.h"
#include "SEGGER_RTT.h"

#include "rm_atcmd_w_core.h"
#include "rm_atcmd_w_core_blebrg.h"

/* This code is needed for using FreeRTOS */
#if (BSP_CFG_RTOS == 2 || BSP_CFG_RTOS_USED == 1)
 #include "FreeRTOS.h"
 #include "task.h"
#else
 #error "This moduless is supposed to run under FreeRTOS"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_BLE_PC_RX_PIN          (BSP_IO_PORT_00_PIN_04)
#define RM_ATCMD_W_BLE_PC_TX_PIN          (BSP_IO_PORT_00_PIN_05)

#define RM_ATCMD_W_BLE_BT_RX_PIN          (BSP_IO_PORT_01_PIN_10) // PB0_0 Barium(Tx)->tin(Rx)
#define RM_ATCMD_W_BLE_BLE_BT_TX_PIN      (BSP_IO_PORT_01_PIN_11) // PB0_1 Barium(Rx)<-tin(Tx)
#define RM_ATCMD_W_BLE_BLE_BT_CTSN_PIN    (BSP_IO_PORT_01_PIN_12) // PB0_3 Barium(RTS)->tin(CTS)
#define RM_ATCMD_W_BLE_BLE_BT_RTSN_PIN    (BSP_IO_PORT_01_PIN_13) // PB0_4 Barium(CTS)<-tin(RTS)

#define RM_ATCMD_W_BLE_UART_PC_RX         GPIO_W_PERIPHERAL_UART_RX
#define RM_ATCMD_W_BLE_UART_PC_TX         GPIO_W_PERIPHERAL_UART_TX
#define RM_ATCMD_W_BLE_UART_PC_CTSN       GPIO_W_PERIPHERAL_UART_CTSN
#define RM_ATCMD_W_BLE_UART_PC_RTSN       GPIO_W_PERIPHERAL_UART_RTSN

#define RM_ATCMD_W_BLE_UART_BT_RX         GPIO_W_PERIPHERAL_UART2_RX
#define RM_ATCMD_W_BLE_UART_BT_TX         GPIO_W_PERIPHERAL_UART2_TX
#define RM_ATCMD_W_BLE_UART_BT_CTSN       GPIO_W_PERIPHERAL_UART2_CTSN
#define RM_ATCMD_W_BLE_UART_BT_RTSN       GPIO_W_PERIPHERAL_UART2_RTSN

#define RM_ATCMD_W_BLE_RESET_PULSE_MS     (200)
#define WAIT_TIMEOUT                      1000000
#define UART_W_CH_NUM                     (2)
#define RX_BUFF_SIZE                      1024
#define BLE_BT_CH                         2
#define BLE_PC_CH                         1

#define STX                               0x02
#define SOH                               0x01
#define ACK                               0x06
#define NAK                               0x15

#define BLE_BT_IND                        0
#define BLE_PC_IND                        1

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct st_rm_atcmd_w_blebrg_uart_cfg
{
    uart_cfg_t               cfg;
    uart_w_extended_cfg_t    ext_cfg;
    uart_w_instance_ctrl_t * pctrl;
    uart_w_instance_ctrl_t   base;
    uint8_t  input_buffer[RX_BUFF_SIZE];
    uint32_t input_buffer_rx;
    uint32_t input_buffer_read;
    uint32_t rx_data_event;
    uint32_t rx_complete;
    uint32_t tx_complete;
    uint32_t tx_data_empty;
    uint32_t error;
    uint32_t cb_count;
    uint32_t last_event;
    char     last_data;
    char     uart_no;
} gr_at_rm_blebrg_uart_cfg_t;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

extern gpio_w_instance_ctrl_t g_ioport_ctrl;

static gpio_w_instance_ctrl_t gr_rm_atcmd_w_blebrg_ioport_ctrl;

/* Pin definitions */
const ioport_pin_cfg_t g_pin_cfg_data[] =
{
    {
        .pin     = RM_ATCMD_W_BLE_PC_RX_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_PC_RX),
    },
    {
        .pin     = RM_ATCMD_W_BLE_PC_TX_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_PC_TX),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BT_RX_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_BT_RX),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_TX_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_BT_TX),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_CTSN_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_BT_CTSN),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_RTSN_PIN,
        .pin_cfg = (uint32_t) (RM_ATCMD_W_BLE_UART_BT_RTSN),
    },
    {
        .pin     = BSP_IO_PORT_01_PIN_14,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_OUTPUT),
    },
};

const ioport_cfg_t gr_rm_atcmd_w_ble_brg_ioport_pin_cfg =
{
    .number_of_pins = (sizeof(g_pin_cfg_data)) / (sizeof(ioport_pin_cfg_t)),
    .p_pin_cfg_data = &g_pin_cfg_data[0],
};

/* Floting Pin definitions */
const ioport_pin_cfg_t g_pin_floting_cfg_data[] =
{
    {
        .pin     = RM_ATCMD_W_BLE_BT_RX_PIN,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_INPUT),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_TX_PIN,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_INPUT),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_CTSN_PIN,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_INPUT),
    },
    {
        .pin     = RM_ATCMD_W_BLE_BLE_BT_RTSN_PIN,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_INPUT),
    },
    {
        .pin     = BSP_IO_PORT_01_PIN_14,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_INPUT),
    },
};

const ioport_cfg_t gr_rm_atcmd_w_ble_brg_ioport_pin_floting_cfg =
{
    .number_of_pins = (sizeof(g_pin_floting_cfg_data)) / (sizeof(ioport_pin_cfg_t)),
    .p_pin_cfg_data = &g_pin_floting_cfg_data[0],
};

extern void uart_w_isr(void);

#if DEFINE_VECTOR_TABLE

/* Add ISR indexed by IRQ number for each interrupt used. */
BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_IRQ_VECTOR_MAX_ENTRIES] BSP_PLACE_IN_SECTION(
    BSP_SECTION_APPLICATION_VECTORS) =
{
    [UART_IRQn]  = uart_w_isr,
    [UART2_IRQn] = uart_w_isr,
    [UART3_IRQn] = uart_w_isr,
};
#endif

bool gr_at_rm_blebrg_uart_run = true;

gr_at_rm_blebrg_uart_cfg_t gr_at_rm_blebrg_uart_data[UART_W_CH_NUM] = {0};

uart_w_baud_setting_t gr_at_rm_blebrg_uart_baud_setting =
{
    .int_baud = 1,
    .fra_baud = 0,
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Initializes pin configuration function to configure pins.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_open (void)
{
    REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPI_GPIO_MODE, 1);

    R_GPIO_W_Open(&gr_rm_atcmd_w_blebrg_ioport_ctrl, &gr_rm_atcmd_w_ble_brg_ioport_pin_cfg);
}

/*******************************************************************************************************************//**
 * @brief Initializes pin configuration function to configure pins.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_floting_open (void)
{
    REG_SETF(CRG_TOP, CLK_AMBA_REG, OQSPI_GPIO_MODE, 1);

    R_GPIO_W_Open(&gr_rm_atcmd_w_blebrg_ioport_ctrl, &gr_rm_atcmd_w_ble_brg_ioport_pin_floting_cfg);
}

/*******************************************************************************************************************//**
 * @brief close pin configuration function.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_close (void)
{
    R_GPIO_W_Close(&gr_rm_atcmd_w_blebrg_ioport_ctrl);;
}

/*******************************************************************************************************************//**
 * @brief write level value to selected IO pin.
 * @param[in] pin: selected pin to be write
 * @param[in] level: velel to be write (BSP_IO_LEVEL_LOW or BSP_IO_LEVEL_HIGH)
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_pin_write (bsp_io_port_pin_t pin, bsp_io_level_t level)
{
    R_GPIO_W_PinWrite(&gr_rm_atcmd_w_blebrg_ioport_ctrl, pin, level);
}

/*******************************************************************************************************************//**
 * @brief configure selected pin to be output direction
 * @param[in] pin: selected pin to be configure
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_pin_out_cfg (bsp_io_port_pin_t pin)
{
    R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, pin, GPIO_W_CFG_PORT_DIRECTION_OUTPUT);
}

/*******************************************************************************************************************//**
 * @brief configure selected pin to be uart RX
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_pin_rxtx_cfg (bool uart_enable)
{
    if (uart_enable)
    {
        R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_BT_RX_PIN, RM_ATCMD_W_BLE_UART_BT_RX);
        R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_PC_RX_PIN, RM_ATCMD_W_BLE_UART_PC_RX);
    }
    else
    {
        R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_BT_RX_PIN, RM_ATCMD_W_BLE_UART_BT_RX);
        R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_PC_RX_PIN, RM_ATCMD_W_BLE_UART_PC_RX);
    }
}

/*******************************************************************************************************************//**
 * @brief configure selected pin to be uart CTSN
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_pin_ctsn_cfg (void)
{
    R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_BLE_BT_CTSN_PIN, RM_ATCMD_W_BLE_UART_BT_CTSN);
}

/*******************************************************************************************************************//**
 * @brief configure selected pin to be uart RSTN
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_pin_rtsn_cfg (void)
{
    R_GPIO_W_PinCfg(&gr_rm_atcmd_w_blebrg_ioport_ctrl, RM_ATCMD_W_BLE_BLE_BT_RTSN_PIN, RM_ATCMD_W_BLE_UART_BT_RTSN);
}

/*******************************************************************************************************************//**
 * @brief configure BLE core to HW reset
 **********************************************************************************************************************/
static void r_at_rm_blebrg_ioport_ble_hw_reset (bool uart_enable)
{
    /* 1-st stage: bring POR to active mode. */
    r_at_rm_blebrg_ioport_pin_write(BSP_IO_PORT_01_PIN_14, BSP_IO_LEVEL_HIGH);
    vTaskDelay(pdMS_TO_TICKS(RM_ATCMD_W_BLE_RESET_PULSE_MS / 2));

    /* 2-nd stage: bring bootloader reset pin to active mode (high). */
    r_at_rm_blebrg_ioport_pin_out_cfg(BSP_IO_PORT_01_PIN_10);
    r_at_rm_blebrg_ioport_pin_write(BSP_IO_PORT_01_PIN_10, BSP_IO_LEVEL_LOW);
    vTaskDelay(pdMS_TO_TICKS(RM_ATCMD_W_BLE_RESET_PULSE_MS / 2));

    /* 3-rd stage: set POR inactive. */
    r_at_rm_blebrg_ioport_pin_write(BSP_IO_PORT_01_PIN_14, BSP_IO_LEVEL_LOW);

    /* 4-th stage: restore original GPIO state. */
    r_at_rm_blebrg_ioport_pin_rxtx_cfg(uart_enable);
}

/*******************************************************************************************************************//**
 * @brief UART callback function that handle UART events:
 *   UART_EVENT_RX_COMPLETE, UART_EVENT_RX_CHAR, add rx char to input buffer
 *   UART_EVENT_TX_COMPLETE, UART_EVENT_TX_DATA_EMPTY -
 *  and update global evet flags with new event status.
 * @param[in] p_args: pointer to callback arguments
 **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_callback (uart_callback_args_t * p_args)
{
    gr_at_rm_blebrg_uart_cfg_t * data_p;
    uint8_t channel = (p_args->channel == BLE_PC_CH) ? BLE_PC_IND : BLE_BT_IND;

    data_p             = &gr_at_rm_blebrg_uart_data[channel];
    data_p->last_event = p_args->event;
    data_p->cb_count++;

    switch (data_p->last_event)
    {
        case UART_EVENT_RX_COMPLETE:
        {
            data_p->rx_complete++;
            break;
        }

        case UART_EVENT_RX_CHAR:
        {
            uint32_t index = (data_p->input_buffer_rx + 1) % RX_BUFF_SIZE;

            data_p->last_data = (uint8_t) p_args->data;
            if (index != data_p->input_buffer_read)
            {
                // Buffer not full
                // in case of buffer full - data will be lost and not overwrite
                data_p->input_buffer[data_p->input_buffer_rx] = (uint8_t) p_args->data;
                data_p->input_buffer_rx = index;
            }

            data_p->rx_data_event++;
            break;
        }

        case UART_EVENT_TX_COMPLETE:
        {
            data_p->tx_complete++;
            break;
        }

        case UART_EVENT_TX_DATA_EMPTY:
        {
            data_p->tx_data_empty++;
            break;
        }

        default:
        {
            data_p->error++;
        }
    }
}

/*******************************************************************************************************************//**
 * @brief clear global rx event flags.
 * @param[in] ch_num: channel numbert to use.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_clear_rx_status (uint8_t ch_num)
{
    gr_at_rm_blebrg_uart_cfg_t * data_p = &gr_at_rm_blebrg_uart_data[ch_num];

    data_p->rx_complete   = 0;
    data_p->rx_data_event = 0;
}

/*******************************************************************************************************************//**
 * @brief clear global tx event flag.
 * @param[in] ch_num: channel numbert to use.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_clear_tx_status (uint8_t ch_num)
{
    gr_at_rm_blebrg_uart_cfg_t * data_p = &gr_at_rm_blebrg_uart_data[ch_num];

    data_p->tx_complete   = 0;
    data_p->tx_data_empty = 0;
}

/*******************************************************************************************************************//***
 * @brief Configur and open UART handler as follow:
 *   UART_W_DATA_BITS_8
 *   UART_PARITY_OFF
 *   UART_STOP_BITS_1
 *   UART_W_AUTO_FLOW_CONTROL_ENABLED
 * celar event flags.
 * @param[in] ch_num: channel numbert to use.
 **********************************************************************************************************************/
static fsp_err_t r_at_rm_blebrg_uart_open (uint8_t ch_num)
{
    gr_at_rm_blebrg_uart_cfg_t * data_p = &gr_at_rm_blebrg_uart_data[ch_num];

    if (ch_num >= UART_W_CH_NUM)
    {
        FSP_ASSERT(0);
    }

    memset(data_p, 0, sizeof(gr_at_rm_blebrg_uart_cfg_t));

    data_p->uart_no              = ch_num;
    data_p->cfg.data_bits        = UART_W_DATA_BITS_8;
    data_p->cfg.parity           = UART_PARITY_OFF;
    data_p->cfg.stop_bits        = UART_STOP_BITS_1;
    data_p->ext_cfg.flow_control = UART_W_AUTO_FLOW_CONTROL_ENABLED;
    data_p->cfg.p_extend         = (void *) &data_p->ext_cfg;
    data_p->cfg.p_callback       = r_at_rm_blebrg_uart_callback;

    data_p->cfg.p_context            = data_p->pctrl;
    data_p->cfg.rxi_ipl              = (uint8_t) 5; // NOLINT(readability-magic-numbers)
    data_p->ext_cfg.loop_back_enable = UART_W_LOOP_BACK_DISABLE;
    data_p->ext_cfg.fifo_enable      = UART_W_FIFO_ENABLE;
    data_p->cfg.p_transfer_rx        = NULL;
    data_p->cfg.p_transfer_tx        = NULL;

    if (FSP_SUCCESS != R_UART_W_BaudCalculate(115200, &gr_at_rm_blebrg_uart_baud_setting))
    {
        FSP_ASSERT(0);
    }

    data_p->ext_cfg.p_baud_setting = &gr_at_rm_blebrg_uart_baud_setting;

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    if (ch_num == BLE_BT_IND)
    {
        CRG_PER->CLK_COM_REG_b.UART3_ENABLE = 1;
        data_p->cfg.rxi_irq                 = (IRQn_Type) (UART3_IRQn);
        data_p->cfg.channel                 = (uint8_t) 2;
    }
    else if (ch_num == BLE_PC_IND)
    {
        CRG_PER->CLK_COM_REG_b.UART2_ENABLE = 1;
        data_p->cfg.rxi_irq                 = (IRQn_Type) (UART2_IRQn);
        data_p->cfg.channel                 = (uint8_t) 1;
    }

    CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE = 1;

    FSP_CRITICAL_SECTION_EXIT;

    r_at_rm_blebrg_uart_clear_rx_status(ch_num);
    r_at_rm_blebrg_uart_clear_tx_status(ch_num);

    data_p->pctrl = &data_p->base;
    if (FSP_SUCCESS != g_uart_on_uart_w.open(data_p->pctrl, &data_p->cfg))
    {
        FSP_ASSERT(0);
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief tx data stream to the UART and wait till tx is done.
 * @param[in] ch_num: channel numbert to use.
 * @param[in] data: pointer to data stream to be tx.
 * @param[in] len: length pf data stream to be tx.
 * @param[in] ticks: maxmum number of ticks wait till tx will done.
 * @retval FSP_SUCCESS                  write done.
 * @retval FSP_ERR_BUFFER_EMPTY         write was not complete.
 **********************************************************************************************************************/
static fsp_err_t r_at_rm_blebrg_uart_send_sync (uint8_t ch_num, char * data, uint16_t len, uint32_t ticks)
{
    fsp_err_t ret_value                 = FSP_SUCCESS;
    gr_at_rm_blebrg_uart_cfg_t * data_p = &gr_at_rm_blebrg_uart_data[ch_num];
    uint32_t tx_complete_local          = data_p->tx_complete;

    g_uart_on_uart_w.write(data_p->pctrl, (uint8_t *) data, len);

    while ((tx_complete_local == data_p->tx_complete) && (--ticks))
    {
        // Waiting interrupt and do nothing.
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MICROSECONDS);
    }

    if (0 == ticks)
    {
        SEGGER_RTT_printf(0, "send_sync Not send\n");
        ret_value = FSP_ERR_WRITE_FAILED;
    }

    return ret_value;
}

/*******************************************************************************************************************//**
 * @brief tx one char and wait till WAIT_TIMEOUT or tx is done.
 * @param[in] ch_num: channel numbert to use.
 * @param[in] data: data char to be tx.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_write_char (uint8_t ch_num, char data)
{
    uint32_t timeout = WAIT_TIMEOUT;

    r_at_rm_blebrg_uart_send_sync(ch_num, &data, 1, timeout);
}

/*******************************************************************************************************************//**
 * @brief close the open UART handler.
 * @param[in] ch_num: channel numbert to use.
 **********************************************************************************************************************/
static fsp_err_t r_at_rm_blebrg_uart_close (uint8_t ch_num)
{
    gr_at_rm_blebrg_uart_cfg_t * data_p = &gr_at_rm_blebrg_uart_data[ch_num];

    if (FSP_SUCCESS != g_uart_on_uart_w.close(data_p->pctrl))
    {
        FSP_ASSERT(0);
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief close the open UART handler.
 * @param[in] first: first channel numbert to use.
 * @param[in] second: second channel numbert to use.
 * **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_transfer (uint8_t first, uint8_t second)
{
    volatile uint32_t            first_rx_count  = 0;
    volatile uint32_t            second_rx_count = 0;
    gr_at_rm_blebrg_uart_cfg_t * first_p         = &gr_at_rm_blebrg_uart_data[first];
    gr_at_rm_blebrg_uart_cfg_t * second_p        = &gr_at_rm_blebrg_uart_data[second];
    char data;

    r_at_rm_blebrg_uart_clear_rx_status(first);
    r_at_rm_blebrg_uart_clear_rx_status(second);

    r_at_rm_blebrg_uart_clear_rx_status(first);
    r_at_rm_blebrg_uart_clear_tx_status(second);

    while (gr_at_rm_blebrg_uart_run)
    {
        while (first_p->input_buffer_read != first_p->input_buffer_rx)
        {
            first_rx_count++;
            data = first_p->input_buffer[first_p->input_buffer_read];
            first_p->input_buffer_read = (first_p->input_buffer_read + 1) % RX_BUFF_SIZE;
            r_at_rm_blebrg_uart_write_char(second, data);
        }

        while (second_p->input_buffer_read != second_p->input_buffer_rx)
        {
            second_rx_count++;
            data = second_p->input_buffer[second_p->input_buffer_read];
            second_p->input_buffer_read = (second_p->input_buffer_read + 1) % RX_BUFF_SIZE;
            r_at_rm_blebrg_uart_write_char(first, data);
        }
    }
}

/*******************************************************************************************************************//**
 * @brief stop the UART handler loop
 * **********************************************************************************************************************/
static void r_at_rm_blebrg_uart_stop (void)
{
    gr_at_rm_blebrg_uart_run = false;
}

/*******************************************************************************************************************//****
 * @brief open the UART GW
 **********************************************************************************************************************/
static void r_at_rm_blebrg_open (void)
{
    r_at_rm_blebrg_ioport_open();

    r_at_rm_blebrg_ioport_pin_ctsn_cfg();
    r_at_rm_blebrg_ioport_pin_rtsn_cfg();

    r_at_rm_blebrg_uart_open(BLE_PC_IND);
    r_at_rm_blebrg_uart_open(BLE_BT_IND);
}

/*******************************************************************************************************************//****
 * @brief close the ports the UARTs.
 **********************************************************************************************************************/
static void r_at_rm_blebrg_close (void)
{
    r_at_rm_blebrg_uart_stop();
    r_at_rm_blebrg_uart_close(BLE_BT_IND);
    r_at_rm_blebrg_uart_close(BLE_PC_IND);
    r_at_rm_blebrg_ioport_close();
}

/*******************************************************************************************************************//****
 * @brief start the main function for handel the uart tarnafer.
 **********************************************************************************************************************/
void r_at_rm_blebrg_start (atcmd_w_core_running_mode_t running_mode)
{
    switch (running_mode)
    {
        case AT_MODE_BLEBRG_PINS:
        {
            r_at_rm_blebrg_ioport_floting_open();
            break;
        }

        case AT_MODE_BLEBRG_BRG:
        {
            r_at_rm_blebrg_open();
            r_at_rm_blebrg_uart_transfer(BLE_PC_IND, BLE_BT_IND);
            break;
        }

        case AT_MODE_BLEBRG_RESET:
        {
            r_at_rm_blebrg_ioport_open();
            r_at_rm_blebrg_ioport_ble_hw_reset(false);
            break;
        }

        case AT_MODE_BLEBRG_ALL:
        {
            r_at_rm_blebrg_open();
            r_at_rm_blebrg_ioport_ble_hw_reset(true);
            r_at_rm_blebrg_uart_transfer(BLE_PC_IND, BLE_BT_IND);
            break;
        }

        default:
        {
            break;
        }
    }
}

/*******************************************************************************************************************//****
 * @brief end the main function for handel the uart tarnafer.
 **********************************************************************************************************************/
void r_at_rm_blebrg_end (void)
{
    r_at_rm_blebrg_close();
}

#endif

/*******************************************************************************************************************//**
 * @} (end addtogroup BLE_LOADER_IOPORT)
 **********************************************************************************************************************/

/* EOF */
