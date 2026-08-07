/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_TRANSPORT_UART_W_H
#define RM_ATCMD_TRANSPORT_UART_W_H

#include "bsp_api.h"
#include "r_uart_api.h"
#include "r_uart_w.h"
#include "r_ioport_api.h"
#include "rm_atcmd_transport_w_api.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "rm_atcmd_w_core.h"


/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ATCMD_MODULE_UART2_EN   1
#define ATCMD_UART_WRITE_CHAR   1

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** User configuration structure, used in open function */
typedef struct st_atcmd_transport_uart_w_extended_cfg
{
    /*  Number of UART interfaces to use */
    const uint32_t num_uarts;
    /*  SCI UART instances */
    const uart_instance_t * uart_instances[ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_UART_PORTS];
    /*  Reset pin used for module */
    const bsp_io_port_pin_t reset_pin;
} atcmd_transport_uart_w_extended_cfg_t;

/** AT_TRANSPORT private control block. DO NOT MODIFY. */
typedef struct st_atcmd_transport_uart_w_instance_ctrl
{
    /*  Pointer to initial configurations. */
    atcmd_transport_w_cfg_t const * p_cfg;
    /*  number of UARTS currently used for communication with module */
    uint32_t num_uarts;
    /*  Current UART instance index for AT commands */
    uint32_t curr_cmd_port;
    /*  Flag to indicate if transport instance has been initialized */
    uint32_t open;
    /*  Command port receive buffer used by byte queue */
    uint8_t cmd_rx_queue_buf[ATCMD_TRANSPORT_W_CFG_CMD_RX_BUF_SIZE];
    /*  Socket stream buffer handle */
    StreamBufferHandle_t socket_byteq_hdl;
    /*  Structure to hold stream buffer info */
    StaticStreamBuffer_t socket_byteq_struct;
    /*  Transmit binary semaphore handle */
    SemaphoreHandle_t tx_sem;
    /*  Transmit binary semaphore data */
    StaticSemaphore_t tx_sem_data;
    /*  Transmit mutex handle */
    SemaphoreHandle_t tx_mutex;
    /*  Transmit mutex data */
    StaticSemaphore_t tx_mutex_data;
    /*  Receive binary semaphore handle */
    StaticSemaphore_t rx_sem_data;
    /*  Receive mutex handle */
    SemaphoreHandle_t rx_mutex;
    /*  Receive mutex data */
    StaticSemaphore_t rx_mutex_data;
    /*  UART instance object */
    uart_instance_t * uart_instance_objects[ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_UART_PORTS];
    /*  UART transmission end binary semaphore */
    SemaphoreHandle_t uart_sem[ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_UART_PORTS];
} atcmd_transport_uart_w_instance_ctrl_t;

extern const char * g_atcmd_transport_uart_w_cmd_baud;
extern atcmd_transport_w_api_t const g_atcmd_transport_on_uart;


/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_Open(atcmd_transport_w_ctrl_t            * p_ctrl,
                                         atcmd_transport_w_cfg_t const * const p_cfg);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_Close(atcmd_transport_w_ctrl_t * const p_ctrl);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_AtCmdSendThreadSafe(atcmd_transport_w_ctrl_t * const p_ctrl,
                                                        atcmd_transport_w_data_t       * p_at_cmd);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_AtCmdSend(atcmd_transport_w_ctrl_t * const p_ctrl,
                                              atcmd_transport_w_data_t * p_at_cmd);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_GiveMutex(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_TakeMutex(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);
fsp_err_t RM_ATCMD_TRANSPORT_UART_W_StatusGet(atcmd_transport_w_ctrl_t * const p_ctrl,
                                              atcmd_transport_w_status_t * p_status);
size_t    RM_ATCMD_TRANSPORT_UART_W_BufferRecv(atcmd_transport_w_ctrl_t * const p_ctrl,
                                               char                           * p_data,
                                               uint32_t                         length,
                                               uint32_t                         rx_timeout);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_TRANSPORT_UART_W_H */
