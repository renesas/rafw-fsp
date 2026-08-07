/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_H
#define RM_ATCMD_W_CORE_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "rm_atcmd_w_api.h"
#if CFG_WIFI
 #include "rm_wifi_api.h"
#endif                                 /* CFG_WIFI */
#include "rm_atcmd_transport_w_api.h"
#include "rm_atcmd_w_cfg.h"
#include "rm_atcmd_w_core_common.h"
#if (ATCMD_SECURE_CHANNEL == 1)
 #include "common.h"
 #include "mbedtls/aes.h"
#endif
#if ATCMD_IF_SUPPORT
 #include "FreeRTOS.h"
 #include "task.h"
#endif                                 /* ATCMD_IF_SUPPORT */

/*******************************************************************************************************************//**
 * @addtogroup ATCMD_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER
#if ATCMD_TRANSPORT_UART_W
 #define ATCMD_W_CORE_ECHO_EN    1
#else
 #define ATCMD_W_CORE_ECHO_EN    0
#endif
#define ATCMD_W_REQ_SIZE         53
#define ATCMD_W_LENGTH           60
#if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
 #define ATCMD_W_RESP_LEN_MAX    (4096)
 #define ATCMD_ESC_BASE          (0x20)
 #define ATCMD_ESC_OK            "\e\r\nOK\r\n"
 #define ATCMD_ESC_ERROR         "\e\r\nERROR:"
 #define ATCMD_ESC_FMT           ATCMD_ESC_ERROR "0x%08x\r\n"
 #define ATCMD_ESC_OK_STR(s)            (strcpy((s), ATCMD_ESC_OK))
 #define ATCMD_ESC_ERROR_STR(s, err)    (sprintf((s), ATCMD_ESC_FMT, err))
#else
 #define ATCMD_ESC_OK_STR(s)            (strcpy((s), "\r\nOK\r\n"))
 #define ATCMD_ESC_ERROR_STR(s, err)    (sprintf((s), "\r\nERROR:%d\r\n", (err)))
 #define ATCMD_W_SECURE_KEY_MAX                  (16)
#endif
#define ATCMD_W_MAX_DELAY                        1000
#define ATCMD_W_CORE_OPEN_KEY                    0x5A5A5A5AU
#define ATCMD_W_CORE_CLOSE_KEY                   0x00000000U
#define ATCMD_W_CORE_CALLBACK_KEY                0x7F7F7F7FU
#define ATCMD_W_CORE_RX_SINGLE_CHAR              1
#define ATCMD_W_CORE_INIT_EVENT                  0
#define ATCMD_W_CORE_MAX_PARAMS                  20
#if (ATCMD_TRANSPORT_SPI_W == 1)
 #define ATCMD_W_TX_QUEUE_SIZE                   (4096)
#else
 #define ATCMD_W_TX_QUEUE_SIZE                   (255)
#endif

#if ATCMD_IF_SUPPORT
 #define ATCMD_W_CORE_EVT_WIFI_OPEN_DONE         (0x1)
 #define ATCMD_W_CORE_EVT_INIT_RSP_SENT_DONE     (0x2)
 #define ATCMD_W_CORE_EVT_TRANSPORT_INIT_DONE    (0x4)

 #if (ATCMD_TRANSPORT_SDIO_W == 0)
  #define ATCMD_W_MAIN_PARSER_PRIO               (10)
 #else
  #define ATCMD_W_MAIN_PARSER_PRIO               (19)
 #endif

 #define RM_ATCMD_W_CORE_PARSER_TASK_SIZE        ((1024 * 8) / 4)
#endif                                 /* ATCMD_IF_SUPPORT */

#define RM_ATCMD_W_CORE_DPM_WAKE_SRC_NUM         (6)
#define RM_ATCMD_W_CORE_DPM_WAKE_SRC_GPIO        (3)
#define RM_ATCMD_W_CORE_DPM_PORT0_GPIO_OFFSET    (7)
#define RM_ATCMD_W_CORE_DPM_PORT1_GPIO_OFFSET    (3)

/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
typedef enum e_atcmd_w_core_running_mode
{
    AT_MODE_STOP = 0,
    AT_MODE_RUN,
#if (ATCMD_BLE_BRG == 1)
    AT_MODE_BLEBRG_ALL,
    AT_MODE_BLEBRG_RESET,
    AT_MODE_BLEBRG_BRG,
    AT_MODE_BLEBRG_PINS,
#endif
    AT_MODE_END,
} atcmd_w_core_running_mode_t;

typedef struct st_atcmd_w_uart_input_buff
{
    char     at_cmd_req_str[ATCMD_W_RESP_LEN_MAX];
    uint32_t at_cmd_req_idx;
#if (ATCMD_W_CORE_ECHO_EN)
    char     at_cmd_resp_str[ATCMD_W_RESP_LEN_MAX];
    uint32_t at_cmd_resp_idx;
#endif
#if (ATCMD_SECURE_CHANNEL == 1)
    uint16_t sec_expected;             ///< ciphertext length from <len>
    uint16_t sec_start_idx;            ///< index in at_cmd_req_str where ciphertext starts
    uint8_t  sec_active;               ///< 0=normal line mode, 1=collecting binary
#endif
} atcmd_w_uart_input_buff_t;

/* AT module control block. DO NOT INITIALIZE. Initialization occurs when @ref atcmd_w_api_t::open is called. */
typedef struct st_atcmd_w_core_instance_ctrl
{
    /* Transport instance of the communication channel. */
    atcmd_transport_w_instance_t const * p_transport_instance;

    /* FIFO depth of the UART channel. */
    uint8_t fifo_depth;

    /* Used to determine if the channel is configured. */
    uint32_t open;

    /* Source buffer pointer used to fill hardware FIFO from transmit ISR. */
    uint8_t const * p_tx_src;

    /* Size of source buffer pointer used to fill hardware FIFO from transmit ISR. */
    uint32_t tx_src_bytes;

    /* Destination buffer used for receiving data. */
    uint8_t rx_dest[ATCMD_W_RESP_LEN_MAX];

    /* Number of data bytes received. */
    uint32_t rx_dest_bytes;

    /* Pointer to current octet read. */
    uint32_t rx_dest_idx;

    /* Pointer to the configuration block. */
    atcmd_w_cfg_t const * p_cfg;

    /* List of AT-CMD modules. */
    atcmd_w_core_module_list_t list;

    /* Echo. */
    uint8_t echo_on;

    /* UART Echo. */
    uint8_t uart_echo_on;

    uint8_t q_result;

    uint8_t mcu_wu_done;

#if ATCMD_IF_SUPPORT
    TaskHandle_t at_init_msg_sender_thread;
    uint8_t      sleep_block_recv_at_dpm_wakeup;
#endif                                 /* ATCMD_IF_SUPPORT */

    uint8_t init_rsp_not_sent;

    atcmd_w_core_running_mode_t run_mode;

#if (ATCMD_SECURE_CHANNEL == 1)
    mbedtls_aes_context   ctx_rx_body;    ///< mbedTLS AES rx body
    mbedtls_aes_context   ctx_tx_body;    ///< mbedTLS AES tx body
    mbedtls_aes_context * ctx_rx;         ///< mbedTLS AES handle
    mbedtls_aes_context * ctx_tx;         ///< mbedTLS AES handle
    uint8_t               rx_iv[16];      ///< IV for RX
    uint8_t               tx_iv[16];      ///< IV for TX
    uint8_t               secure_channel; ///< Secure Channel
    uint8_t               key[16];
#endif
#if (ATCMD_IF_SUPPORT == 1)
    atcmd_w_uart_input_buff_t rx_data;
    TaskHandle_t              parser_task_handle;
#endif
} atcmd_w_core_instance_ctrl_t;

typedef struct st_atcmd_w_tx_req
{
    uint8_t * buff;
    size_t    buff_size;
} atcmd_w_tx_req_t;

typedef struct st_atcmd_w_tx_queue
{
    atcmd_w_tx_req_t queue[ATCMD_W_TX_QUEUE_SIZE];
    size_t           q_head;
    size_t           q_tail;
} atcmd_w_tx_queue_t;

/** AT configuration struct. */
typedef struct st_atcmd_w_conf
{
    atcmd_w_cfg_t conf;                ///< Instruction configuration for the AT module
} atcmd_w_conf_t;

/* AT core instance. */
typedef struct st_atcmd_w_core_instance
{
    atcmd_w_core_instance_ctrl_t at_ctrl;
    atcmd_w_conf_t               at_conf;
    atcmd_w_tx_queue_t           tx_queue;
    uint8_t * curr_tx_buff;
} atcmd_w_core_instance_t;

typedef uint32_t (* ATCmdOpenCallback_t)(atcmd_w_ctrl_t * const p_atcmd_w_ctrl);
typedef uint32_t (* ATCmdCloseCallback_t)(atcmd_w_ctrl_t * const p_atcmd_w_ctrl);

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const atcmd_w_api_t  g_at_core;
extern ATCmdOpenCallback_t  atcmd_open_user_app_callback;
extern ATCmdCloseCallback_t atcmd_close_user_app_callback;

/** @endcond */

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
#if ATCMD_IF_SUPPORT
void      rm_atcmd_w_core_init_rsp_evt_create(void);
void      rm_atcmd_w_core_init_rsp_evt_set(uint16_t event);
fsp_err_t rm_atcmd_w_core_init_rsp_evt_wait(uint16_t event);

#endif                                 /* ATCMD_IF_SUPPORT */

#if (ATCMD_SECURE_CHANNEL == 1)
int rm_atcmd_secchan_pad(char * data, int length);
int rm_atcmd_secchan_unpad(char * data, int length);
int is_hex_char(char c);

#endif

fsp_err_t RM_ATCMD_W_CORE_Open(atcmd_w_ctrl_t * const p_at_ctrl, atcmd_w_cfg_t const * const p_cfg);
fsp_err_t RM_ATCMD_W_CORE_Read(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * const p_dest, uint32_t const bytes);
fsp_err_t RM_ATCMD_W_CORE_DataRead(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * const p_dest, uint32_t const bytes);
fsp_err_t RM_ATCMD_W_CORE_Write(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t const * const p_src, uint32_t const bytes);
fsp_err_t RM_ATCMD_W_CORE_InfoGet(atcmd_w_ctrl_t * const p_at_ctrl, atcmd_w_info_t * const p_info);
fsp_err_t RM_ATCMD_W_CORE_Close(atcmd_w_ctrl_t * const p_at_ctrl);
fsp_err_t RM_ATCMD_W_CORE_SecureChannelKeySet(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * key);

uint32_t rm_atcmd_w_core_register_module_node(atcmd_w_core_module_list_t  * p_list,
                                              const atcmd_w_core_module_t * module);
uint32_t rm_atcmd_w_core_register_unfixed_module_node(atcmd_w_core_module_list_t          * p_list,
                                                      const atcmd_w_core_unfixed_module_t * module);
void rm_atcmd_w_core_deregister(atcmd_w_core_module_list_t * p_list, const atcmd_w_core_module_t * module);
void rm_atcmd_w_core_unfixed_deregister(atcmd_w_core_module_list_t          * p_list,
                                        const atcmd_w_core_unfixed_module_t * module);

/*******************************************************************************************************************//**
 * @} (end addtogroup ATCMD_W)
 **********************************************************************************************************************/

#endif                                 /* RM_ATCMD_W_CORE_H */
