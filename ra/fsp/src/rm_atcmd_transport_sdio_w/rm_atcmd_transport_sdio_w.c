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

#include "rm_atcmd_w_core.h"
#if (ATCMD_TRANSPORT_SDIO_W == 1)

 #include "r_sdemmc_w.h"
 #include "r_gpio_w.h"
 #include "rm_atcmd_transport_sdio_w.h"
 #include "FreeRTOS.h"
 #include "task.h"
 #include "semphr.h"
 #include "common_data.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Definitions of Open flag "ATSD" */
 #define ATCMD_SDIO_TRANSFER_SIZE        (SDEMMC_W_MAX_BLOCK_SIZE * 16 + 512)
 #define ATCMD_SDIO_INPUT_BUFFER_COUNT   (4)

 #define ATCMD_TRANSPORT_SDIO_W_OPEN     (0x41545344U)
 #define ATCMD_TRANSPORT_SDIO_W_CLOSE    (0x00U)
 #define AT_WAIT_TRANS_COMP_COUNT        (100000)
 #define AT_WAIT_TRANS_COMP_DELAY_US     (10)

 #define ESC_RESP_OK                     (0x20)
 #define ESC_RESP_ERR                    (0xFF)
 #define ESC_BUFF_OK                     (0x00000000)
 #define ESC_BUFF_ERR                    (0xFFFFFFFF)
 #define SDIO_SUSPEND_ALL

 #define STR_LEN(x)    (sizeof(x) - 1)

 #define ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_COMPLETE    (1 << 0)
 #define ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_ERROR       (1 << 1)
 #define ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER             (ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_COMPLETE | \
                                                            ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_ERROR)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct
{
    bsp_io_port_pin_t pin[6];          /* 0:CLK, 1:CMD, 2:D0, 3:D1, 4:D2, 5:D3 */
} sdio_pins_t;

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
extern gpio_w_instance_ctrl_t g_gpio_w_ctrl;

/* IOPORT Instance */
extern const ioport_instance_t g_gpio_w;

extern atcmd_transport_sdio_w_instance_ctrl_t g_atcmd_transport_ctrl;

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
sdmmc_device_t g_sdmmc_sdio_device;

atcmd_transport_w_api_t const g_atcmd_transport_on_sdio =
{
    .open  = RM_ATCMD_TRANSPORT_SDIO_W_Open,
    .close = RM_ATCMD_TRANSPORT_SDIO_W_Close,
    .atCommandSendThreadSafe = RM_ATCMD_TRANSPORT_SDIO_W_AtCmdSendThreadSafe,
    .atCommandSend           = RM_ATCMD_TRANSPORT_SDIO_W_AtCmdSend,
    .giveMutex               = RM_ATCMD_TRANSPORT_SDIO_W_GiveMutex,
    .takeMutex               = RM_ATCMD_TRANSPORT_SDIO_W_TakeMutex,
    .bufferRecv              = RM_ATCMD_TRANSPORT_SDIO_W_BufferRecv,
    .statusGet               = RM_ATCMD_TRANSPORT_SDIO_W_StatusGet,
};

static volatile uint32_t wr_transfer_complete = 0;
static volatile uint32_t transfer_error       = 0;
static uint32_t          wr_data_status       = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;
static bsp_io_port_pin_t gs_trigger_gpio      = BSP_IO_PORT_00_PIN_00;
static uint8_t           gs_response_header[ATCMD_SDIO_TRANSFER_HEADER_SIZE] = {0};
static uint32_t          gs_prev_send_len = 0;
static volatile uint16_t gs_rx_buffer_head = 0; /*  Receive buffer head */
static volatile uint16_t gs_rx_buffer_tail = 0; /*  Receive buffer tail */
static uint8_t           gs_rx_buffer[ATCMD_SDIO_INPUT_BUFFER_COUNT][ATCMD_SDIO_TRANSFER_SIZE] = {0};
static uint16_t          gs_rx_buffer_len[ATCMD_SDIO_INPUT_BUFFER_COUNT] = { };
static uint8_t           gs_tx_buffer[ATCMD_W_RESP_LEN_MAX] = {0};

void rm_atcmd_transport_sdio_w_cb(sdmmc_callback_args_t * p_args);

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
static inline const ioport_pin_cfg_t * find_pin_cfg (const ioport_cfg_t * cfg, bsp_io_port_pin_t pin)
{
    const ioport_pin_cfg_t * tbl = cfg->p_pin_cfg_data;

    for (uint16_t i = 0; i < cfg->number_of_pins; i++)
    {
        if (tbl[i].pin == pin)
        {
            return &tbl[i];
        }
    }

    return NULL;
}

void rm_atcmd_transport_sdio_w_cb (sdmmc_callback_args_t * p_args)
{
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl   = &g_atcmd_transport_ctrl;
    sdemmc_w_transfer_result_t result;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BaseType_t xResult;

    if (p_args->event & SDMMC_EVENT_SDIO)
    {
        if (p_args->event & SDEMMC_W_PRIV_EVENT_SDIO_FN1_WR_OVR)
        {
            sdmmc_instance_t * p_sdio = p_instance_ctrl->sdio_instance_objects[0];
            uint32_t block            = ATCMD_SDIO_TRANSFER_SIZE / SDEMMC_W_MAX_BLOCK_SIZE;

            if (0 != (ATCMD_SDIO_TRANSFER_SIZE % SDEMMC_W_MAX_BLOCK_SIZE))
            {
                block = block + 1;
            }

            R_SDEMMC_W_ResultGet(p_sdio->p_ctrl, &result);
            gs_rx_buffer_len[gs_rx_buffer_head] = result.sdio_blk_size * result.sdio_blk_count;
            gs_rx_buffer_head                   = (gs_rx_buffer_head + 1) % ATCMD_SDIO_INPUT_BUFFER_COUNT;

            /* Wake up the processing task */
            xResult = xSemaphoreGiveFromISR(p_instance_ctrl->rx_sem, &xHigherPriorityTaskWoken);

            if (xResult != pdFAIL)
            {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }

            g_sdmmc_on_sdemmc_w.readIoExt(p_sdio->p_ctrl,
                                          gs_rx_buffer[gs_rx_buffer_head],
                                          1,
                                          0,
                                          &block,
                                          SDMMC_IO_MODE_TRANSFER_BLOCK,
                                          SDMMC_IO_ADDRESS_MODE_FIXED);
        }

        if (p_args->event & SDEMMC_W_PRIV_EVENT_SDIO_FN1_RD_OVR)
        {
            wr_transfer_complete = 1;

            if (ATCMD_TRANSPORT_SDIO_W_SEND_STAT_DATA == wr_data_status)
            {
                xResult = xEventGroupSetBitsFromISR(p_instance_ctrl->sdio_write_event_group,
                                                    ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_COMPLETE,
                                                    &xHigherPriorityTaskWoken);
                if (xResult != pdFAIL)
                {
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }
            }
        }

        if (p_args->event & SDEMMC_W_PRIV_EVENT_TRANSFER_ERROR)
        {
            transfer_error = 1;

            if (ATCMD_TRANSPORT_SDIO_W_SEND_STAT_DATA == wr_data_status)
            {
                xResult = xEventGroupSetBitsFromISR(p_instance_ctrl->sdio_write_event_group,
                                                    ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_ERROR,
                                                    &xHigherPriorityTaskWoken);
                if (xResult != pdFAIL)
                {
                    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
                }
            }
        }
    }
    else if (p_args->event & SDEMMC_W_PRIV_EVENT_TRANSFER_ERROR)
    {
        transfer_error = 1;
    }
}

static fsp_err_t wait_for_sdio_wr_transfer_complete (atcmd_transport_w_ctrl_t * p_ctrl, bool allow_error)
{
    (void) allow_error;
    fsp_err_t err     = FSP_SUCCESS;
    uint32_t  timeout = AT_WAIT_TRANS_COMP_COUNT;
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;

    if (ATCMD_TRANSPORT_SDIO_W_SEND_STAT_HDR == wr_data_status)
    {
        while (timeout > 0)
        {
            R_BSP_SoftwareDelay(AT_WAIT_TRANS_COMP_DELAY_US, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;

            if (wr_transfer_complete)
            {
                break;
            }
            else if (transfer_error)
            {
                wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;
                err            = FSP_ERR_ABORTED;
                break;
            }
        }
    }
    else if (ATCMD_TRANSPORT_SDIO_W_SEND_STAT_DATA == wr_data_status)
    {
        EventBits_t uxBits = xEventGroupWaitBits(p_instance_ctrl->sdio_write_event_group,
                                                 ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER,
                                                 pdTRUE,
                                                 pdFALSE,
                                                 pdMS_TO_TICKS(AT_WAIT_TRANS_COMP_COUNT * AT_WAIT_TRANS_COMP_DELAY_US /
                                                               1000));

        wr_transfer_complete = 0U;
        transfer_error       = 0U;

        wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;

        if (uxBits & ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER_ERROR)
        {
            return FSP_ERR_ABORTED;
        }

        if ((uxBits & ATCMD_TRANSPORT_SDIO_W_EVENT_TRANSFER) == 0)
        {
            return FSP_ERR_TIMEOUT;
        }
    }

    wr_transfer_complete = 0U;
    transfer_error       = 0U;

    if (timeout == 0U)
    {
        wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;

        return FSP_ERR_TIMEOUT;
    }

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_Open (atcmd_transport_w_ctrl_t            * p_ctrl,
                                          atcmd_transport_w_cfg_t const * const p_cfg)
{
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t          err    = FSP_SUCCESS;
    sdmmc_instance_t * p_sdio = NULL;
    atcmd_sdio_transport_w_extended_cfg_t * p_transport_extended_cfg = NULL;
    uint32_t                 i;
    const ioport_cfg_t     * cfg = (const ioport_cfg_t *) g_gpio_w.p_cfg;
    const ioport_pin_cfg_t * pc;
    uint32_t                 block = ATCMD_SDIO_TRANSFER_SIZE / SDEMMC_W_MAX_BLOCK_SIZE;

    if (0 != (ATCMD_SDIO_TRANSFER_SIZE % SDEMMC_W_MAX_BLOCK_SIZE))
    {
        block = block + 1;
    }

 #if (ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED == 1)
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SDIO_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
 #endif

    /* Update control structure from configuration values */
    p_instance_ctrl->p_cfg   = p_cfg;
    p_transport_extended_cfg = (atcmd_sdio_transport_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    gs_rx_buffer_head        = 0;
    gs_rx_buffer_tail        = 0;

    for (i = 0; i < p_transport_extended_cfg->num_sdios; i++)
    {
        p_instance_ctrl->sdio_instance_objects[i] = (sdmmc_instance_t *) p_transport_extended_cfg->sdio_instances[i];
    }

    gs_trigger_gpio = p_transport_extended_cfg->gpio_pin;
    pc              = find_pin_cfg(cfg, gs_trigger_gpio);

    g_gpio_w.p_api->pinCfg(&g_gpio_w_ctrl, gs_trigger_gpio, pc->pin_cfg);

    /* Open sdio port */
    p_instance_ctrl->tx_mutex = xSemaphoreCreateMutexStatic(&p_instance_ctrl->tx_mutex_data);
    p_instance_ctrl->rx_mutex = xSemaphoreCreateMutexStatic(&p_instance_ctrl->rx_mutex_data);
    p_instance_ctrl->rx_sem   = xSemaphoreCreateCountingStatic(ATCMD_SDIO_INPUT_BUFFER_COUNT,
                                                               0,
                                                               &p_instance_ctrl->rx_sem_data);

    p_instance_ctrl->sdio_write_event_group = xEventGroupCreateStatic(&p_instance_ctrl->sdio_write_event_group_data);

    /* The GPIO register configuration in rm_atcmd_transport_sdio_w_iomux_setup has been moved to r_sdemmc_w. */
    p_sdio = p_instance_ctrl->sdio_instance_objects[0];

    err = g_sdmmc_on_sdemmc_w.open(p_sdio->p_ctrl, p_sdio->p_cfg);
    if (err != FSP_SUCCESS)
    {
        goto RM_ATCMD_TRANSPORT_SDIO_W_Open_error;
    }

    err = g_sdmmc_on_sdemmc_w.mediaInit(p_sdio->p_ctrl, &g_sdmmc_sdio_device);
    if (err != FSP_SUCCESS)
    {
        goto RM_ATCMD_TRANSPORT_SDIO_W_Open_error;
    }

    err = g_sdmmc_on_sdemmc_w.readIoExt(p_sdio->p_ctrl,
                                        gs_rx_buffer[gs_rx_buffer_head],
                                        1,
                                        0,
                                        &block,
                                        SDMMC_IO_MODE_TRANSFER_BLOCK,
                                        SDMMC_IO_ADDRESS_MODE_FIXED);
    if (err != FSP_SUCCESS)
    {
        goto RM_ATCMD_TRANSPORT_SDIO_W_Open_error;
    }

    p_instance_ctrl->open = ATCMD_TRANSPORT_SDIO_W_OPEN;

    return FSP_SUCCESS;

RM_ATCMD_TRANSPORT_SDIO_W_Open_error:

    p_sdio->p_api->close(p_sdio->p_ctrl);

    vEventGroupDelete(p_instance_ctrl->sdio_write_event_group);
    xSemaphoreGive(p_instance_ctrl->rx_sem);
    vSemaphoreDelete(p_instance_ctrl->rx_sem);
    vSemaphoreDelete(p_instance_ctrl->tx_mutex);
    vSemaphoreDelete(p_instance_ctrl->rx_mutex);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_Close (atcmd_transport_w_ctrl_t * const p_ctrl)
{
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;
    sdmmc_instance_t * p_sdio = NULL;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ASSERT(p_instance_ctrl->sdio_instance_objects[0] != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SDIO_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_sdio = p_instance_ctrl->sdio_instance_objects[0];

    p_instance_ctrl->open = ATCMD_TRANSPORT_SDIO_W_CLOSE;

    g_sdmmc_on_sdemmc_w.close(p_sdio->p_ctrl);

    vEventGroupDelete(p_instance_ctrl->sdio_write_event_group);

    xSemaphoreGive(p_instance_ctrl->rx_sem);

    vSemaphoreDelete(p_instance_ctrl->rx_sem);
    vSemaphoreDelete(p_instance_ctrl->tx_mutex);
    vSemaphoreDelete(p_instance_ctrl->rx_mutex);

    return FSP_SUCCESS;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_AtCmdSendThreadSafe (atcmd_transport_w_ctrl_t * const p_ctrl,
                                                         atcmd_transport_w_data_t       * p_at_cmd)
{
    FSP_PARAMETER_NOT_USED(p_at_cmd);
    fsp_err_t err = FSP_SUCCESS;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;

    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SDIO_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
#endif

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_AtCmdSend (atcmd_transport_w_ctrl_t * const p_ctrl,
                                               atcmd_transport_w_data_t       * p_at_cmd)
{
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;
    sdmmc_instance_t * p_sdio = NULL;
    uint32_t buff_address;
    uint16_t buff_len;
    uint8_t resp_id;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ASSERT(p_instance_ctrl->sdio_instance_objects[0] != NULL);
    FSP_ASSERT(p_at_cmd != NULL);
    FSP_ASSERT(p_at_cmd->at_cmd_string_length != 0);
    FSP_ERROR_RETURN(p_at_cmd->at_cmd_string_length != 0, FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SDIO_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_sdio = p_instance_ctrl->sdio_instance_objects[0];

    xSemaphoreTake(p_instance_ctrl->tx_mutex, portMAX_DELAY);

    if(ATCMD_TRANSPORT_SDIO_W_SEND_STAT_DATA == wr_data_status)
    {
        /* blocking */
        err = wait_for_sdio_wr_transfer_complete(p_ctrl, false);
    }

    if(p_at_cmd->at_cmd_string_length > ATCMD_W_RESP_LEN_MAX)
    {
        p_at_cmd->at_cmd_string_length = ATCMD_W_RESP_LEN_MAX;
    }

    if((gs_prev_send_len > p_at_cmd->at_cmd_string_length) && (gs_prev_send_len <= ATCMD_W_RESP_LEN_MAX))
    {
        memset(&gs_tx_buffer[p_at_cmd->at_cmd_string_length], 0x00, gs_prev_send_len - p_at_cmd->at_cmd_string_length);
    }

    memcpy(gs_tx_buffer, p_at_cmd->p_at_cmd_string, p_at_cmd->at_cmd_string_length);

    /* read response */
    if (AT_CMD_ESC_KEY_CHAR == gs_tx_buffer[0])
    {
        /* ESC OK */
        if (strncmp((char *) gs_tx_buffer, ATCMD_ESC_OK, STR_LEN(ATCMD_ESC_OK)) == 0)
        {
            resp_id      = ESC_RESP_OK;
            buff_address = ESC_BUFF_OK;
            memset(gs_tx_buffer, 0x00, p_at_cmd->at_cmd_string_length);
            p_at_cmd->at_cmd_string_length = STR_LEN("\r\nOK\r\n");
            strncpy((char *) gs_tx_buffer, "\r\nOK\r\n", p_at_cmd->at_cmd_string_length);
        }
        /* ESC ERROR */
        else if (strncmp((char *) gs_tx_buffer, ATCMD_ESC_ERROR, STR_LEN(ATCMD_ESC_ERROR)) == 0)
        {
            /* \e\r\nERROR:0x%0X\r\n */
            resp_id      = ESC_RESP_ERR;
            buff_address = ESC_BUFF_ERR;

            /* delete '\e' */
            memmove(gs_tx_buffer, gs_tx_buffer + 1, p_at_cmd->at_cmd_string_length - 1);
            gs_tx_buffer[p_at_cmd->at_cmd_string_length - 1] = '\0';
        }
        /* Others */
        else
        {
            buff_address = 0;
            resp_id = ATCMD_SDIO_DEVICE_READ_RES;
        }

        buff_len = (uint16_t) p_at_cmd->at_cmd_string_length;

        gs_response_header[0] = (uint8_t) (buff_address & 0xFF);
        gs_response_header[1] = (uint8_t) ((buff_address >> 8) & 0xFF);
        gs_response_header[2] = (uint8_t) ((buff_address >> 16) & 0xFF);
        gs_response_header[3] = (uint8_t) ((buff_address >> 24) & 0xFF);
        gs_response_header[4] = (uint8_t) ((buff_len) & 0xFF);
        gs_response_header[5] = (uint8_t) ((buff_len >> 8) & 0xFF);
        gs_response_header[6] = resp_id;
        gs_response_header[7] = ATCMD_SDIO_DEVICE_PADDING;
    }
    else
    {
        /* AT */
        buff_address = 0;
        buff_len     = (uint16_t) p_at_cmd->at_cmd_string_length;

        resp_id = ATCMD_SDIO_DEVICE_READ_RES;

        gs_response_header[0] = (uint8_t) (buff_address & 0xFF);
        gs_response_header[1] = (uint8_t) ((buff_address >> 8) & 0xFF);
        gs_response_header[2] = (uint8_t) ((buff_address >> 16) & 0xFF);
        gs_response_header[3] = (uint8_t) ((buff_address >> 24) & 0xFF);
        gs_response_header[4] = (uint8_t) ((buff_len) & 0xFF);
        gs_response_header[5] = (uint8_t) ((buff_len >> 8) & 0xFF);
        gs_response_header[6] = resp_id;
        gs_response_header[7] = ATCMD_SDIO_DEVICE_PADDING;
    }

    gs_prev_send_len = p_at_cmd->at_cmd_string_length;
    wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_HDR;
    err = g_sdmmc_on_sdemmc_w.writeIoExt(p_sdio->p_ctrl, (uint8_t *)gs_response_header, 1, 0,
                                         ATCMD_SDIO_TRANSFER_HEADER_SIZE,
                                         SDMMC_IO_MODE_TRANSFER_BYTE,
                                         SDMMC_IO_ADDRESS_MODE_FIXED);

    if (FSP_SUCCESS != err)
    {
        wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;
        xSemaphoreGive(p_instance_ctrl->tx_mutex);

        return err;
    }

 #ifdef SDIO_SUSPEND_ALL
    vTaskSuspendAll();
 #endif

    /* Trigger GPIO interrupt line to high */
    g_gpio_w.p_api->pinWrite(&g_gpio_w_ctrl, gs_trigger_gpio, BSP_IO_LEVEL_HIGH);

    /* blocking */
    err = wait_for_sdio_wr_transfer_complete(p_ctrl, false);
    if (FSP_SUCCESS != err)
    {
        wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;
        g_gpio_w.p_api->pinWrite(&g_gpio_w_ctrl, gs_trigger_gpio, BSP_IO_LEVEL_LOW);
 #ifdef SDIO_SUSPEND_ALL
        if (!xTaskResumeAll())
        {
            taskYIELD();
        }
 #endif
        xSemaphoreGive(p_instance_ctrl->tx_mutex);

        return err;
    }

    /* send data */
    uint32_t count         = p_at_cmd->at_cmd_string_length;
    uint32_t transfer_mode = SDMMC_IO_MODE_TRANSFER_BYTE;

    if (SDEMMC_W_MAX_BLOCK_SIZE < p_at_cmd->at_cmd_string_length)
    {
        count = p_at_cmd->at_cmd_string_length / SDEMMC_W_MAX_BLOCK_SIZE;
        if (0 != p_at_cmd->at_cmd_string_length % SDEMMC_W_MAX_BLOCK_SIZE)
        {
            count++;
        }

        transfer_mode = SDMMC_IO_MODE_TRANSFER_BLOCK;
    }

    wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_DATA;
    err            = g_sdmmc_on_sdemmc_w.writeIoExt(p_sdio->p_ctrl,
                                                    (uint8_t *) gs_tx_buffer,
                                                    1,
                                                    0,
                                                    count,
                                                    transfer_mode,
                                                    SDMMC_IO_ADDRESS_MODE_FIXED);

    /* Reset GPIO interrupt line to low (default value) */
    g_gpio_w.p_api->pinWrite(&g_gpio_w_ctrl, gs_trigger_gpio, BSP_IO_LEVEL_LOW);

 #ifdef SDIO_SUSPEND_ALL
    if (!xTaskResumeAll())
    {
        taskYIELD();
    }
 #endif
    if (FSP_SUCCESS != err)
    {
        wr_data_status = ATCMD_TRANSPORT_SDIO_W_SEND_STAT_IDLE;
        xSemaphoreGive(p_instance_ctrl->tx_mutex);

        return err;
    }

    xSemaphoreGive(p_instance_ctrl->tx_mutex);

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_GiveMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    (void) mutex_flag;
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
#endif

    if (!p_instance_ctrl->open)
    {
        err = FSP_ERR_NOT_OPEN;
    }

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_TakeMutex (atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag)
{
    (void) mutex_flag;
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
#endif

    if (!p_instance_ctrl->open)
    {
        err = FSP_ERR_NOT_OPEN;
    }

    return err;
}

fsp_err_t RM_ATCMD_TRANSPORT_SDIO_W_StatusGet (atcmd_transport_w_ctrl_t * const p_ctrl,
                                               atcmd_transport_w_status_t     * p_status)
{
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;

 #if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ASSERT(p_instance_ctrl != NULL);
    FSP_ASSERT(p_status != NULL);
 #endif

    p_status->open = (ATCMD_TRANSPORT_SDIO_W_OPEN == p_instance_ctrl->open);

    return FSP_SUCCESS;
}

size_t RM_ATCMD_TRANSPORT_SDIO_W_BufferRecv (atcmd_transport_w_ctrl_t * const p_ctrl,
                                             char                           * p_data,
                                             uint32_t                         length,
                                             uint32_t                         rx_timeout)
{
    size_t                                   recv_len        = 0;
    atcmd_transport_sdio_w_instance_ctrl_t * p_instance_ctrl = (atcmd_transport_sdio_w_instance_ctrl_t *) p_ctrl;

#if ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
    FSP_ERROR_RETURN(p_instance_ctrl != NULL, 0);
    FSP_ERROR_RETURN(p_data != NULL, 0);
    FSP_ERROR_RETURN(length != 0, 0);
    FSP_ERROR_RETURN(ATCMD_TRANSPORT_SDIO_W_OPEN == p_instance_ctrl->open, 0);
#endif

    /* Lock SDIO until the reception is complete */
    xSemaphoreTake(p_instance_ctrl->rx_mutex, portMAX_DELAY);

    /* There is no buffered messages available. Wait for the external event */
    if (pdFALSE == xSemaphoreTake(p_instance_ctrl->rx_sem, rx_timeout))
    {
        /* RX waiting timeout. */
        xSemaphoreGive(p_instance_ctrl->rx_mutex);

        return 0;
    }

    if (ATCMD_TRANSPORT_SDIO_W_OPEN != p_instance_ctrl->open)
    {
        /* We've closed the transport from another thread - perform graceful exit. */
        return 0;
    }

    recv_len = MIN(gs_rx_buffer_len[gs_rx_buffer_tail], length);
    memcpy((void *) p_data, gs_rx_buffer[gs_rx_buffer_tail], recv_len);

    /* Update read index */
    gs_rx_buffer_tail = (gs_rx_buffer_tail + 1) % ATCMD_SDIO_INPUT_BUFFER_COUNT;

    /* SDIO is now available for other read requests */
    xSemaphoreGive(p_instance_ctrl->rx_mutex);

    return recv_len;
}

#endif                                 /* (ATCMD_TRANSPORT_SDIO_W == 1) */
