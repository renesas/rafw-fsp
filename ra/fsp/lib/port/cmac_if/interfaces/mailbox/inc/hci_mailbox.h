/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file hci_mailbox.h
 *
 * @brief HCI Mailbox API header file
 *
 */

#if !defined(HCI_MAILBOX_H)
#define HCI_MAILBOX_H

#include <stdint.h>

#if defined(HCI_MAILBOX_CUSTOM_CONFIG)
 #include "hci_mbox_config.h"
#endif

/*
 *  Defines
 */

/** @brief Distinguish between Host/Controller core */
#if !defined(HCI_MAILBOX_HOST) && !defined(HCI_MAILBOX_CONTROLLER)
 #if defined(CORTEX_M33) || defined(SYS_CPU)
  #define HCI_MAILBOX_HOST          (1)
 #elif defined(CORTEX_M0PLUS) || defined(CMAC_CPU)
  #define HCI_MAILBOX_CONTROLLER    (1)
 #else
  #error "HCI_MAILBOX core not recognized?"
 #endif
#endif                                 /* HCI_MAILBOX_HOST/HCI_MAILBOX_CONTROLLER */

/** @brief Direction definitions used on host with hci_mbox_host_progress_tx_rx() */
#define MBOX_HOST_PROGRESS_TX            (1)
#define MBOX_HOST_PROGRESS_RX            (2)

/** @brief Definitions of Bluetooth (MBOX_PROTO_BT) specific Sub-Protocols */
#define MBOX_SUB_PROTO_BLE               (0) /*!< Mbox Sub-Protocol Bluetoth Low Energy */
#define MBOX_SUB_PROTO_BREDR             (1) /*!< Mbox Sub-Protocol Bluetoth Classic */

/**
 *  @brief Execute full reset sequence of CMAC core on SysCPU side
 */
#if !defined(HCI_MBOX_EXECUTES_CMAC_RESET)
 #define HCI_MBOX_EXECUTES_CMAC_RESET    (1)
#endif

/*
 *  Type Definitions
 */

/**
 * @brief Mailbox Protocols supported.
 */
typedef enum
{
    MBOX_PROTO_BT   = 0,               /*!< Mbox Protocol Bluetooth. Used by DMC controller */
    MBOX_PROTO_FTDF = 1,               /*!< Mbox Protocol IEEE 802.15.4 */
    MBOX_PROTO_MAX                     /*!< Maximum number of protocols */
} mbox_protocol_t;

/**
 * @brief HCI Mailbox Protocol flag definitions.
 * Used for hci_mbox_xxx_init() functions to mark used protocols.
 */
typedef uint8_t mbox_protocol_flag_t;

#define MBOX_PROTO_FLAG_BT      (1 << 0) /*!< Mbox Protocol Bluetooth. Used by DMC controller */
#define MBOX_PROTO_FLAG_FTDF    (1 << 1) /*!< Mbox Protocol IEEE 802.15.4 */

/**
 * @brief Bluetooth HCI Messages supported.
 */
typedef enum
{
    MBOX_BT_HCI_PKT_INVALID  = 0,      /*!< Invalid type (0x00) */
    MBOX_BT_HCI_PKT_TYPE_CMD = 1,      /*!< Command packet (receive only). */
    MBOX_BT_HCI_PKT_TYPE_ACL = 2,      /*!< ACL data packet (send or receive). */
    MBOX_BT_HCI_PKT_TYPE_SYN = 3,      /*!< Synchronous data packet (send or receive). */
    MBOX_BT_HCI_PKT_TYPE_EVT = 4,      /*!< Event packet (send only). */
    MBOX_BT_HCI_PKT_TYPE_ISO = 5,      /*!< ISO data packet (send or receive). */
    MBOX_BT_HCI_PKT_TYPE_MAX           /*!< Last entry, anything >= is invalid */
} mbox_bt_hci_pkt_t;

/**
 * @brief HCI HW Error codes raised by HCI Mailbox towards the host.
 */
typedef enum
{
    MBOX_BT_HCI_HW_ERR_INVALID_DATA     = 0xA0, /*!< Invalid data received. Usually in type or header bytes */
    MBOX_BT_HCI_HW_ERR_INVALID_DATA_LEN = 0xA1, /*!< Invalid data length. The payload length exceeds the expected limits */
    MBOX_BT_HCI_HW_ERR_OUT_OF_MEMORY    = 0xA2, /*!< Out of memory. Controller memory allocation error */
    MBOX_BT_HCI_HW_ERR_RX_TIMEOUT       = 0xA3  /*!< Receive timeout. Missing bytes in header/payload */
} mbox_bt_hci_hw_error_code_t;

/**
 * @brief  HCI Mailbox API return type
 */
typedef enum
{
    HCI_MBOX_NO_ERROR,                 /*!< Success. */
    HCI_MBOX_HOST_ALREADY_INIT,        /*!< Host already initialized. */
    HCI_MBOX_HOST_ERR_IPC_INIT,        /*!< Host IPC initialization error. */
    HCI_MBOX_HOST_ERR_TX_PENDING,      /*!< Mbox Host Transmit is ongoing. */
    HCI_MBOX_HOST_ERR_RX_PENDING,      /*!< Mbox Host Receive is ongoing. */
} hci_mbox_err_t;

/**
 * @brief HCI Configuration options from Protocol Controller
 */
typedef struct __attribute__((packed, aligned(1)))
{
    uint16_t max_acl_le_len;           /*!< Maximum ACL data length for LE. */
    uint16_t max_acl_bredr_len;        /*!< Maximum ACL data length for BR-EDR. */
    uint16_t max_iso_sdu_len;          /*!< Maximum ISO data length. */
    uint8_t  max_syn_len;              /*!< Maximum SCO data length. */
    uint8_t  bredr_disabled;           /*!< Whether the controller is LE only. */
    uint16_t bredr_min_conn_handle;    /*!< Minimum value of a connection handle to be valid BR/EDR. */
    uint16_t bredr_max_conn_handle;    /*!< Maximum value of a connection handle to be valid BR/EDR. */
}
mbox_bt_hci_cfg_t;

/**
 * @brief Configuration options of Mailbox
 */
typedef struct
{
    mbox_protocol_flag_t protocols;     /*!< Protocols enabled in Mailbox */
    mbox_bt_hci_cfg_t    bt_hci_config; /*!< Bluetooth HCI configuration options of the controller */
} mbox_config_t;

/*
 *  API Function Definitions
 */

#if defined(HCI_MAILBOX_HOST)

/**
 * @brief HCI Mailbox main state machine states
 */
typedef enum
{
    HCI_MBOX_STATE_UNINITIALIZED,
    HCI_MBOX_STATE_WAIT_CMAC_READY,
    HCI_MBOX_STATE_NORMAL,
    HCI_MBOX_STATE_RESET_CMAC
} hci_mbox_state_t;

/**
 * @brief Type of HCI Mailbox Host callback function when Receive is complete
 *
 * @param hci_pkt HCI Message buffer (type uint8_*)
 * @param pkt_len HCI Message length (type uint16_t)
 */
typedef void (* hci_mbox_host_rx_done_cb_t)(const uint8_t * hci_pkt, uint16_t pkt_len);

/**
 * @brief Type of HCI Mailbox Host callback function when Transmit is complete
 *
 * @param status Completion Status (type uint8_t)
 */
typedef void (* hci_mbox_host_tx_done_cb_t)(uint8_t status);

/**
 * @brief Type of HCI Mailbox Host callback function for getting the configuration of the controller
 *
 * @param status Completion Status (type uint8_t)
 */
typedef void (* hci_mbox_host_get_ctrl_conf_cb_t)(uint8_t status);

/**
 * @brief The Host RX/TX Progress callback function definition
 *
 * @param status Completion Status (type uint8_t)
 */
typedef void (* hci_mbox_host_progress_cb_t)(uint8_t status);

/**
 * @brief HCI Mailbox Host function callback definition
 *
 * Used to trigger Shim of a state change in Mbox Host
 *
 * @param new_state The updated state of host (type hci_mbox_state_t)
 */
typedef void (* hci_mbox_host_state_updated_cb_t)(hci_mbox_state_t new_state);

/**
 * @brief HCI Mailbox Host OS/Shim interaction functions
 */
typedef struct
{
    hci_mbox_host_state_updated_cb_t host_state_update_trigger;
    hci_mbox_host_progress_cb_t      host_progress_tx;
    hci_mbox_host_progress_cb_t      host_progress_rx;
} hci_mbox_host_conf_t;

/**
 * @brief HCI Mailbox Host Early Initialization
 * This function blocks until the initialization of the Controller is done.
 *
 */
hci_mbox_err_t hci_mbox_host_preinit(void);

/**
 * @brief HCI Mailbox Host Initialization
 * This function may block until the initialization of the Controller is done.
 *
 * @param protocols the protocols to be initialized on host side
 * @param host_conf HCI Mailbox Host Configuration
 *
 * @return hci_mbox_err_t HCI_MBOX_NO_ERROR on success, else the related error
 */
hci_mbox_err_t hci_mbox_host_init(mbox_protocol_t protocol, const hci_mbox_host_conf_t * host_conf);

/**
 * @brief Initiate an HCI packet Receive from the Controller
 *
 * @param protocol The protocol of the packet to be received
 * @param hci_pkt The pointer to the packet to be received. This needs to be pre-allocated
 * @param rx_complete_cb Callback function for when the receive is complete
 *
 * @return hci_mbox_err_t HCI_MBOX_NO_ERROR on success
 */
hci_mbox_err_t hci_mbox_host_read(mbox_protocol_t protocol, uint8_t * hci_pkt,
                                  hci_mbox_host_rx_done_cb_t rx_complete_cb);

/**
 * @brief Initiate an HCI packet transmission from Host to the Controller.
 *
 * @param protocol The protocol of the packet to be transmitted
 * @param hci_pkt The pointer to the raw HCI packet buffer
 * @param pkt_len The length of the packet
 * @param tx_complete_cb Callback function for when he transmission is complete
 * @return hci_mbox_err_t HCI_MBOX_NO_ERROR on success or HCI_MBOX_HOST_ERR_TX_PENDING if the interface is busy
 */
hci_mbox_err_t hci_mbox_host_write(mbox_protocol_t            protocol,
                                   const uint8_t            * hci_pkt,
                                   uint16_t                   pkt_len,
                                   hci_mbox_host_tx_done_cb_t tx_complete_cb);

/**
 * @brief Get HCI configuration of the controller
 *
 * NOTE: Used by HCI Shim
 *
 * @param conf_buffer The pointer to the raw configuration buffer
 * @param get_conf_complete Callback function for when the configuration is received is complete
 */
void hci_mbox_host_get_ctrl_conf(uint8_t * conf_buffer, hci_mbox_host_get_ctrl_conf_cb_t get_conf_complete);

/**
 * @brief Request a HW Error to be raised from the Controller. This initiates the error recovery procedure.
 *
 * NOTE: Used by HCI Shim
 *
 * @param code The error code to send
 */
void hci_mbox_host_trigger_hw_error(mbox_bt_hci_hw_error_code_t code);

/**
 * @brief HCI Mailbox Host function called when HCI_RESET is received
 *
 * NOTE: Used by HCI Shim
 */
void hci_mbox_host_received_hci_reset(void);

/**
 * @brief HCI Mailbox Host function to pregress transmit/receive
 *
 * NOTE: Used by HCI Shim when hci_mbox_host_progress_cb_t callbacks are set
 *
 * @param direction The direction (TX/RX) to progress
 * @param status The status argument passed by upper layers
 */
void hci_mbox_host_progress_tx_rx(uint8_t direction, uint8_t status);

#else                                  /* HCI_MAILBOX_CONTROLLER */

/**
 * @brief Type of HCI Mailbox Controller callback function when Receive is complete
 *
 * @param protocol HCI Message protocol (type mbox_protocol_t)
 * @param msg_len HCI Message length (type uint16_t)
 * @param buf HCI Message buffer (type uint8_*)
 */
typedef void (* hci_mbox_ctrl_rx_complete_cb_t)(mbox_protocol_t, uint16_t, uint8_t *);

/**
 * @brief Type of HCI Mailbox Controller callback function when Transmit is complete
 *
 * @param status Completion Status (type uint8_t)
 */
typedef void (* hci_mbox_ctrl_tx_complete_cb_t)(uint8_t);

/**
 * @brief Initialize HCI Mailbox Interface on controller side.
 *
 * @param mbox_conf Controller configuration parameters (for Mailbox)
 */
void hci_mbox_ctrl_init(const mbox_config_t mbox_conf);

/**
 * @brief Initiate an HCI packet receive from Host side.
 *        The internal HCI Mailbox mechanism will deliver the next valid HCI packet to
 *        the rx_complete_cb function.
 *
 * @param protocol The protocol of the packet to be received
 * @param rx_complete_cb Callback function when the Receive is complete
 */
void hci_mbox_ctrl_read(mbox_protocol_t protocol, hci_mbox_ctrl_rx_complete_cb_t rx_complete_cb);

/**
 * @brief Initiate an HCI packet tranmission to Host side
 *
 * @param protocol The protocol of the packet to be transmitted
 * @param len length of HCI packet to send
 * @param buf pointer to the raw HCI packet to send
 * @param tx_complete_cb Callback function when the Transmit is complete
 */
void hci_mbox_ctrl_write(mbox_protocol_t                protocol,
                         uint16_t                       len,
                         uint8_t                      * buf,
                         hci_mbox_ctrl_tx_complete_cb_t tx_complete_cb);

#endif                                 /* HCI_MAILBOX_HOST/HCI_MAILBOX_CONTROLLER */

#endif                                 /* HCI_MAILBOX_H */
