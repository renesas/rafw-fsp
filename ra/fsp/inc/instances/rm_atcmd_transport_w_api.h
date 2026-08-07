/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * @ingroup RENESAS_CONNECTIVITY_INTERFACES
 * @defgroup AT_TRANSPORT_API AT Interface
 * @brief Interface for AT TRANSPORT communications.
 *
 * @section AT_INTERFACE_SUMMARY Summary
 * The AT interface provides common APIs for AT HAL drivers. The AT interface supports the following features:
 * - Full-duplex AT communication
 * - Interrupt driven transmit/receive processing
 * - Callback function with returned event code
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_ATCMD_TRANSPORT_W_API_H_
#define RM_ATCMD_TRANSPORT_W_API_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stddef.h>
#include <stdbool.h>
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

 #ifndef ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED
  #define ATCMD_TRANSPORT_W_CFG_PARAM_CHECKING_ENABLED    ((BSP_CFG_PARAM_CHECKING_ENABLE))
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_CMD_RX_BUF_SIZE
  #define ATCMD_TRANSPORT_W_CFG_CMD_RX_BUF_SIZE           (4096)
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_SEM_MAX_TIMEOUT
  #define ATCMD_TRANSPORT_W_CFG_SEM_MAX_TIMEOUT           (10000)
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_UART_PORTS
  #define ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_UART_PORTS     (2)
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_MAX_RETRIES_UART_COMMS
  #define ATCMD_TRANSPORT_W_CFG_MAX_RETRIES_UART_COMMS    (10)
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SPI_PORTS
  #define ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SPI_PORTS      (1)
 #endif

 #ifndef ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SDIO_PORTS
  #define ATCMD_TRANSPORT_W_CFG_MAX_NUMBER_SDIO_PORTS      (1)
 #endif

/* Error Response Codes */
#define ATCMD_TRANSPORT_W_ERR_UNKNOWN_CMD          (-1)
#define ATCMD_TRANSPORT_W_ERR_INSUF_PARAMS         (-2)
#define ATCMD_TRANSPORT_W_ERR_TOO_MANY_PARAMS      (-3)
#define ATCMD_TRANSPORT_W_ERR_INVALID_PARAM        (-4)
#define ATCMD_TRANSPORT_W_ERR_UNSUPPORTED_FUN      (-5)
#define ATCMD_TRANSPORT_W_ERR_NOT_CONNECTED_AP     (-6)
#define ATCMD_TRANSPORT_W_ERR_NO_RESULT            (-7)
#define ATCMD_TRANSPORT_W_ERR_RESP_BUF_OVERFLOW    (-8)
#define ATCMD_TRANSPORT_W_ERR_FUNC_NOT_CONFIG      (-9)
#define ATCMD_TRANSPORT_W_ERR_CMD_TIMEOUT          (-10)
#define ATCMD_TRANSPORT_W_ERR_NVRAM_WR_FAIL        (-11)
#define ATCMD_TRANSPORT_W_ERR_RETEN_MEM_WR_FAIL    (-12)
#define ATCMD_TRANSPORT_W_ERR_UNKNOWN              (-99)

#define TX_QUEUE_SIZE (128)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct tx_req {
    uint8_t *buff;
    size_t buff_size;
} tx_req_t;

typedef struct tx_queue {
    tx_req_t queue[TX_QUEUE_SIZE];
    size_t q_head;
    size_t q_tail;
} tx_queue_t;

/** Event in the callback function */
typedef enum e_atcmd_transport_w_event
{
    ATCMD_TRANSPORT_W_RX_BYTE_EVENT,
} atcmd_transport_w_event_t;

/** Transport type */
typedef enum e_atcmd_transport_w_type
{
    ATCMD_TRANSPORT_W_TYPE_STREAMING, /* Per-byte data transfer */
    ATCMD_TRANSPORT_W_TYPE_COMMAND,   /* Per-command data transfer */
} atcmd_transport_w_type_t;

/** Middleware configuration block */
typedef struct st_atcmd_transport_w_cfg
{
    /* Type of transport */
    atcmd_transport_w_type_t type;

    /* Pointer to extended configuration by instance of interface */
    void const             * p_extend;
} atcmd_transport_w_cfg_t;

/** Data structure */
typedef struct st_atcmd_transport_w_data
{
    uint8_t    * p_at_cmd_string;      ///< Pointer to ATCMD string.
    uint32_t     at_cmd_string_length; ///< ATCMD string length.
    uint8_t    * p_response_buffer;    ///< Pointer to ATCMD response buffer.
    uint32_t     response_buffer_size; ///< ATCMD response buffer string length.
    uint32_t     timeout_ms;           ///< ATCMD timeout in ms.
    const char * p_expect_code;        ///< Expected string in the ATCMD response.
    uint32_t     comm_ch_id;           ///< Communication channel ID.
} atcmd_transport_w_data_t;

/** Status indicators */
typedef struct st_atcmd_transport_w_status
{
    /* True if driver is open */
    bool open;
} atcmd_transport_w_status_t;

/** At transport control block.
 *  Allocate an instance specific control block
 *  to pass into the Communications API calls.
 */
typedef void atcmd_transport_w_ctrl_t;

/** AT Command APIs */
typedef struct st_atcmd_transport_w_api
{
    /** Open at cmd instance.
     *
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_cfg        Pointer to configuration structure.
     */
    fsp_err_t (* open)(atcmd_transport_w_ctrl_t * p_ctrl,
                       atcmd_transport_w_cfg_t const * const p_cfg);

    /** Close at cmd instance.
     * @param[in]  p_ctrl       Pointer to control structure.
     */
    fsp_err_t (* close)(atcmd_transport_w_ctrl_t * const p_ctrl);

    /** at cmd send thread safe.
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_at_cmd      Pointer to AT command data structure.
     */
    fsp_err_t (* atCommandSendThreadSafe)(atcmd_transport_w_ctrl_t * const p_ctrl, atcmd_transport_w_data_t * p_at_cmd);

    /** at cmd send.
     * @param[in]  p_ctrl       Pointer to control structure.
     * @param[in]  p_at_cmd      Pointer to AT command data structure.
     */
    fsp_err_t (* atCommandSend)(atcmd_transport_w_ctrl_t * const p_ctrl, atcmd_transport_w_data_t * p_at_cmd);

    /** Give the mutex.
     *  @param[in]  p_ctrl       Pointer to Transport layer instance control structure.
     *  @param[in] mutex_flag    TX/RX Flags for the mutex.
     */
    fsp_err_t (* giveMutex)(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);

    /** Take the mutex .
     *  @param[in]  p_ctrl       Pointer to Transport layer instance control structure.
     *  @param[in] mutex_flag    TX/RX Flags for the mutex.
     */
    fsp_err_t (* takeMutex)(atcmd_transport_w_ctrl_t * const p_ctrl, uint32_t mutex_flag);

    /** Gets the status of the configured transport.
     *
     * @param[in]   p_ctrl             Pointer to the to Transport layer instance control structure.
     * @param[out]  p_status           Pointer to store current status.
     */
    fsp_err_t (* statusGet)(atcmd_transport_w_ctrl_t * const p_ctrl, atcmd_transport_w_status_t * p_status);

    /** Receive data from stream buffer.
     * @param[in]  p_ctrl               Pointer to Transport layer instance control structure.
     * @param[in]  p_data               Pointer to data.
     * @param[in]  length               Data length.
     * @param[in]  rx_timeout           Timeout for receiving data on the buffer.
     * @param[in]  trigger_level        Trigger level for stream buffer.
     */
    size_t (* bufferRecv)(atcmd_transport_w_ctrl_t * const p_ctrl, char * p_data, uint32_t length,
                          uint32_t rx_timeout);

} atcmd_transport_w_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_atcmd_transport_instance
{
    atcmd_transport_w_ctrl_t      * p_ctrl;
    atcmd_transport_w_cfg_t       * p_cfg;
    atcmd_transport_w_api_t const * p_api;
} atcmd_transport_w_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_FOOTER

#endif  /* RM_ATCMD_TRANSPORT_W_API_H_ */

/*******************************************************************************************************************//**
 * @} (end addtogroup RM_ATCMD_TRANSPORT_W_API_H_)
 **********************************************************************************************************************/

