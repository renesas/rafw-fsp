/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_TRANSPORT_SPI_W_H
#define RM_ATCMD_TRANSPORT_SPI_W_H

#include "bsp_api.h"
#include "r_spi_api.h"
#include "r_spi_w.h"
#include "r_ioport_api.h"
#include "rm_atcmd_transport_w_api.h"
#include "rm_atcmd_transport_spi_w_cfg.h"
#include "rm_atcmd_w_core.h"

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "semphr.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* In order to achieve high performance it is crucial to prepare the entire SPI communication chain before it starts.
 *
 *      ----------------------------------------------------------------------------------------------------------------
 *      |  HOST REQ (8 bytes) | SLAVE RESP (8 bytes) | HOST DATA REQ (8 bytes) |   DATA (N-bytes aligned by 4 bytes)   |
 *      ----------------------------------------------------------------------------------------------------------------
 *      ^                     ^                      ^                         ^
 *    START_OFFSET     SLAVE_RESP_OFFSET    HOST_DATA_REQ_OFFSET           DATA_OFFSET
 * (HOST_REQ_OFFSET)
 *
 */
#define ATCMD_SPI_TRANSFER_HEADER_SIZE        (8)

#define ATCMD_SPI_BUFFER_START_OFFSET         (0)
#define ATCMD_SPI_BUFFER_HOST_REQ_OFFSET      (ATCMD_SPI_BUFFER_START_OFFSET)
#define ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET    (ATCMD_SPI_BUFFER_HOST_REQ_OFFSET + ATCMD_SPI_TRANSFER_HEADER_SIZE)
#define ATCMD_SPI_BUFFER_HOST_DATA_REQ_OFFSET (ATCMD_SPI_BUFFER_SLAVE_RESP_OFFSET + ATCMD_SPI_TRANSFER_HEADER_SIZE)
#define ATCMD_SPI_BUFFER_DATA_OFFSET          (ATCMD_SPI_BUFFER_HOST_DATA_REQ_OFFSET + ATCMD_SPI_TRANSFER_HEADER_SIZE)
#define ATCMD_SPI_INPUT_BUFFER_COUNT          (4)


#define ATCMD_SPI_TRANSFER_DATA_MAX           (ATCMD_W_RESP_LEN_MAX)
#define ATCMD_SPI_TRANSFER_BUFFER_MAX         (ATCMD_SPI_TRANSFER_DATA_MAX + ATCMD_SPI_BUFFER_DATA_OFFSET)
#define ATCMD_SPI_TRANSFER_QUEUE_SIZE         (4 * ATCMD_SPI_TRANSFER_BUFFER_MAX)

#define ATCMD_SPI_TASK_STACK_SIZE             (1024 / sizeof(StackType_t))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

typedef enum e_atcmd_transport_spi_w_op {
    ATCMD_TRANSPORT_SPI_W_OP_READ = 0,
    ATCMD_TRANSPORT_SPI_W_OP_WRITE,
    ATCMD_TRANSPORT_SPI_W_OP_CMD,
} atcmd_transport_spi_w_op_t;

typedef enum e_atcmd_transport_spi_w_stat {
    ATCMD_TRANSPORT_SPI_W_STAT_IDLE = 0,
    ATCMD_TRANSPORT_SPI_W_STAT_INITIALIZED,
    ATCMD_TRANSPORT_SPI_W_STAT_CMD_RX,
    ATCMD_TRANSPORT_SPI_W_STAT_CMD_PROCESS,
} atcmd_transport_spi_w_stat_t;

/** User configuration structure, used in open function */
typedef struct st_atcmd_transport_spi_w_extended_cfg
{
    /*  Number of SPI interfaces to use */
    const uint32_t num_spis;
    /*  SPI instances */
    const spi_instance_t * spi_instances[ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SPI_PORTS];
    /*  Reset pin used for module */
    const bsp_io_port_pin_t reset_pin;
} atcmd_transport_spi_w_extended_cfg_t;

/** AT_TRANSPORT private control block. DO NOT MODIFY. */
typedef struct st_atcmd_transport_spi_w_instance_ctrl
{
    /*  Pointer to initial configurations. */
    atcmd_transport_w_cfg_t const * p_cfg;
    /*  number of SPIs currently used for communication with module */
    uint32_t num_spis;
    /*  Flag to indicate if transport instance has been initialized */
    uint32_t open;
    /*  Command port receive buffer used by byte queue */
    uint8_t cmd_rx_buf[ATCMD_SPI_INPUT_BUFFER_COUNT][ATCMD_SPI_TRANSFER_BUFFER_MAX];
    /*  Command port transmit buffer used by byte queue */
    uint8_t cmd_tx_buf[ATCMD_SPI_TRANSFER_BUFFER_MAX];
    /*  Receive buffer head */
    volatile uint16_t cmd_rx_buf_head;
    /*  Receive buffer tail */
    volatile uint16_t cmd_rx_buf_tail;
    /*  SPI transport system state */
    volatile atcmd_transport_spi_w_stat_t spi_sys_state;
    /*  SPI transport current operation */
    volatile atcmd_transport_spi_w_op_t spi_sys_op;
    /*  Data to receive from host */
    volatile uint16_t bytes_to_receive;
    /*  Data to sent */
    volatile uint16_t bytes_to_send;
    /*  Bytes transferred in current transaction */
    volatile uint16_t cmd_spi_length;
    /*  Corrupted messages received count */
    volatile uint32_t corrupted_frames;
    /*  Corrupted messages count before */
    volatile uint32_t corrupted_before;
    /*  TX queue message buffer handle */
    MessageBufferHandle_t tx_queue_hdl;
    /*  TX queue message buffer info */
    StaticMessageBuffer_t tx_queue_data;
    /*  TX queue buffer */
    uint8_t tx_queue_buf[ATCMD_SPI_TRANSFER_QUEUE_SIZE];
    /*  Transmit mutex handle */
    SemaphoreHandle_t tx_mutex;
    /*  Transmit mutex data */
    StaticSemaphore_t tx_mutex_data;
    /*  Receive mutex handle */
    SemaphoreHandle_t rx_mutex;
    /*  Receive mutex data */
    StaticSemaphore_t rx_mutex_data;
    /*  SPI instance object */
    spi_instance_t * spi_instance_objects[ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SPI_PORTS];
    /*  SPI main dispatcher task semaphore */
    SemaphoreHandle_t spi_sem;
    /*  SPI main dispatcher task semaphore data */
    StaticSemaphore_t spi_sem_data;
    /*  SPI reception end counting semaphore */
    SemaphoreHandle_t rx_sem;
    /*  SPI reception end semaphore data */
    StaticSemaphore_t rx_sem_data;
    /*  Operation dispatch task */
    TaskHandle_t      dispatch_task;
    /*  Operation dispatch task data */
    StaticTask_t      dispatch_task_data;
    /*  Operation dispatch task stack */
    StackType_t       dispatch_task_stack[ATCMD_SPI_TASK_STACK_SIZE];
} atcmd_transport_spi_w_instance_ctrl_t;


/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
extern atcmd_transport_w_api_t const g_atcmd_transport_on_spi;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_Open(atcmd_transport_w_ctrl_t            * p_ctrl,
                                        atcmd_transport_w_cfg_t const * const p_cfg);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_Close(atcmd_transport_w_ctrl_t * const p_ctrl);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_AtCmdSendThreadSafe(atcmd_transport_w_ctrl_t * const p_ctrl,
                                                       atcmd_transport_w_data_t       * p_at_cmd);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_AtCmdSend(atcmd_transport_w_ctrl_t * const p_ctrl,
                                             atcmd_transport_w_data_t       * p_at_cmd);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_GiveMutex(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_TakeMutex(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);
fsp_err_t RM_ATCMD_TRANSPORT_SPI_W_StatusGet(atcmd_transport_w_ctrl_t   * const p_ctrl,
                                             atcmd_transport_w_status_t * p_status);
size_t    RM_ATCMD_TRANSPORT_SPI_W_BufferRecv(atcmd_transport_w_ctrl_t * const p_ctrl,
                                              char                           * p_data,
                                              uint32_t                         length,
                                              uint32_t                         rx_timeout);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_TRANSPORT_SPI_W_H */
