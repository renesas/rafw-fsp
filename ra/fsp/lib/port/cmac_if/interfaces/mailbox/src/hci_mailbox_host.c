/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdint.h>
#include <stdbool.h>

#include "sdk_defs.h"

#include "ipc.h"
#include "ipc_event_codes.h"
#include "mbox_ipc.h"

#if dg_configSECURITY_TOOLBOX
 #include "crypto_server.h"
#endif

#if (BSP_CFG_RTOS != 0)                // Any RTOS used
 #if (BSP_CFG_RTOS == 1)               // ThreadX used
// Do nothing, ThreadX not tested yet
 #elif (BSP_CFG_RTOS == 2)             // FreeRTOS used
  #include "FreeRTOS.h"
  #include "semphr.h"
 #else                                 // Zephyr used
  #include <zephyr/kernel.h>
 #endif
#endif /* BSP_CFG_RTOS != 0 */

#include "hci_mailbox.h"
#include "hci_mailbox_internal.h"

/*
 * Configuration definitions
 */

/**
 *  @brief Workaround to reply to the HCI_RESET command without power-cycle or relaying it on CMAC
 */
#if !defined(HCI_MBOX_CHEAT_ON_HCI_RESET)
 #define HCI_MBOX_CHEAT_ON_HCI_RESET             (0)
#endif

/**
 *  @brief Enable mutex protection on hci_mbox_host_write function.
 */
#if (BSP_CFG_RTOS != 0)                // Any RTOS used
 #define HCI_MBOX_HOST_WRITE_HAS_MUTEX           (1)
#endif

/*
 *  Critical section handling macros
 */
#define HCI_MBOX_HOST_CRITICAL_SECTION_DEFINE    FSP_CRITICAL_SECTION_DEFINE
#define HCI_MBOX_HOST_CRITICAL_SECTION_ENTER     FSP_CRITICAL_SECTION_ENTER
#define HCI_MBOX_HOST_CRITICAL_SECTION_EXIT      FSP_CRITICAL_SECTION_EXIT

/*
 *  Type Definitions
 */

/**
 * @brief HCI Shim Host to Controller Transmit states
 */
typedef enum
{
    HCI_MBOX_H2C_TX_IDLE,
    HCI_MBOX_H2C_TX_HDR,
    HCI_MBOX_H2C_TX_PAYLOAD
} hci_mbox_h2c_tx_state_t;

/**
 * @brief HCI Shim Host to Controller Receive states
 */
typedef enum
{
    HCI_MBOX_H2C_RX_IDLE,
    HCI_MBOX_H2C_RX_HDR,
    HCI_MBOX_H2C_RX_PAYLOAD
} hci_mbox_h2c_rx_state_t;

/**
 * @brief HCI Shim Host Status/State
 */
static struct hci_mbox_host_status_t
{
    // configuration
    hci_mbox_state_t                 hci_state;
    mbox_protocol_flag_t             enabled_protocols;
    uint8_t                          cmac_ready_received;
    uint8_t                        * conf_rx_buf;
    hci_mbox_host_get_ctrl_conf_cb_t conf_received_callback;
    hci_mbox_host_state_updated_cb_t trigger_host_state_update;

    // error handling
    uint8_t request_reset_ack;
#if (HCI_MBOX_CHEAT_ON_HCI_RESET == 1)
    uint8_t cheat_on_reset;
#endif

    // tx/rx
    hci_mbox_h2c_tx_state_t    h2c_tx_state;
    hci_mbox_h2c_rx_state_t    h2c_rx_state;
    uint16_t                   h2c_tx_len;
    uint16_t                   h2c_rx_len;
    uint8_t                    h2c_tx_hdr[MBOX_PKT_HEADER_SZ];
    uint8_t                    h2c_rx_hdr[MBOX_PKT_HEADER_SZ];
    uint8_t                  * h2c_tx_buffer;
    uint8_t                  * h2c_rx_buffer[MBOX_PROTO_MAX];
    hci_mbox_host_tx_done_cb_t h2c_tx_done_cb;
    hci_mbox_host_rx_done_cb_t h2c_rx_done_cb[MBOX_PROTO_MAX];

    // OS Progress
    hci_mbox_host_progress_cb_t host_tx_progress[MBOX_PROTO_MAX];
    hci_mbox_host_progress_cb_t host_rx_progress[MBOX_PROTO_MAX];
} hci_mbox_status = {0};

/*
 *  Static data
 */

#if (HCI_MBOX_HOST_WRITE_HAS_MUTEX)
 #if (BSP_CFG_RTOS == 1)               // ThreadX used
// Do nothing, ThreadX not tested yet
 #elif (BSP_CFG_RTOS == 2)             // FreeRTOS used
static SemaphoreHandle_t hci_mbox_host_write_mutex = NULL;
 #else                                 // Zephyr used
static struct k_mutex hci_mbox_host_write_mutex = NULL;
 #endif
#endif  /* HCI_MBOX_HOST_WRITE_HAS_MUTEX */

/*
 *  Internal functions
 */

#if (HCI_MBOX_HOST_WRITE_HAS_MUTEX)

/*
 * Mutex protection handling for hci_mbox_host_write()
 */
static inline void hci_mbox_host_write_mutex_init (void)
{
 #if (BSP_CFG_RTOS == 1)               // ThreadX used
    // Do nothing, ThreadX not tested yet
 #elif (BSP_CFG_RTOS == 2)             // FreeRTOS used
    if (!hci_mbox_host_write_mutex)
    {
        hci_mbox_host_write_mutex = xSemaphoreCreateMutex();
        BSP_CHECK_FATAL(hci_mbox_host_write_mutex != NULL);
    }

 #else                                 // Zephyr used
    if (!hci_mbox_host_write_mutex)
    {
        if (k_mutex_init(&hci_mbox_host_write_mutex) != 0)
        {
            BSP_CHECK_FATAL(0);
        }
    }
 #endif
}

static inline void hci_mbox_host_write_mutex_lock (void)
{
 #if (BSP_CFG_RTOS == 1)               // ThreadX used
    // Do nothing, ThreadX not tested yet
 #elif (BSP_CFG_RTOS == 2)             // FreeRTOS used
    xSemaphoreTake(hci_mbox_host_write_mutex, portMAX_DELAY);
 #else                                 // Zephyr used
    k_mutex_lock(&hci_mbox_host_write_mutex, K_FOREVER);
 #endif
}

static inline void hci_mbox_host_write_mutex_unlock (void)
{
 #if (BSP_CFG_RTOS == 1)               // ThreadX used
    // Do nothing, ThreadX not tested yet
 #elif (BSP_CFG_RTOS == 2)             // FreeRTOS used
    xSemaphoreGive(hci_mbox_host_write_mutex);
 #else                                 // Zephyr used
    k_mutex_unlock(&hci_mbox_host_write_mutex);
 #endif
}

#endif                                 /* HCI_MBOX_HOST_WRITE_HAS_MUTEX */

static void hci_mbox_set_state (hci_mbox_state_t new_state)
{
    switch (new_state)
    {
        case HCI_MBOX_STATE_UNINITIALIZED:
        {
            break;
        }

        case HCI_MBOX_STATE_WAIT_CMAC_READY:
        {
            hci_mbox_status.cmac_ready_received = 0;
            break;
        }

        case HCI_MBOX_STATE_NORMAL:
        {
            // ready to receive data from host
            if (hci_mbox_status.trigger_host_state_update)
            {
                hci_mbox_status.trigger_host_state_update(HCI_MBOX_STATE_NORMAL);
            }

#if (HCI_MBOX_CHEAT_ON_HCI_RESET == 1)
            if (hci_mbox_status.cheat_on_reset)
            {
                // Send event to request HCI reset Complete
                ipc_event_payload pld = HCI_MBOX_EVENT_CREATE_PAYLOAD(HCI_MBOX_EVENT_H2C_TRIGGER_HCI_RESET_CMPLT, 0);
                ipc_write_data(IPC_CHANNEL_SYS2CMAC, IPC_EVENT_CODE_HCI_SHIM, pld);

                hci_mbox_status.cheat_on_reset = 0;
            }
#endif
            break;
        }

        case HCI_MBOX_STATE_RESET_CMAC:
        {
#if (HCI_MBOX_EXECUTES_CMAC_RESET == 1)

            // reset rx
            hci_mbox_status.h2c_rx_state                  = HCI_MBOX_H2C_RX_IDLE;
            hci_mbox_status.h2c_rx_len                    = 0;
            hci_mbox_status.h2c_rx_buffer[MBOX_PROTO_BT]  = NULL;
            hci_mbox_status.h2c_rx_done_cb[MBOX_PROTO_BT] = NULL;

            hci_mbox_status.request_reset_ack = 1;
#endif

            // reset. stop receiving data from host
            if (hci_mbox_status.trigger_host_state_update)
            {
                hci_mbox_status.trigger_host_state_update(HCI_MBOX_STATE_RESET_CMAC);
            }

            break;
        }
    }

    hci_mbox_status.hci_state = new_state;
}

/*
 *  Receive from Controller complete
 */
static void hci_mbox_rx_continue (uint8_t status)
{
    (void) status;

    switch (hci_mbox_status.h2c_rx_state)
    {
        case HCI_MBOX_H2C_RX_IDLE:
        {
            // Error?
            break;
        }

        case HCI_MBOX_H2C_RX_HDR:
        {
            hci_mbox_status.h2c_rx_state = HCI_MBOX_H2C_RX_PAYLOAD;
            hci_mbox_status.h2c_rx_len   =
                (uint16_t) (hci_mbox_status.h2c_rx_hdr[0] + (hci_mbox_status.h2c_rx_hdr[1] << 8));
#if (MBOX_PKT_HEADER_SZ > 2)
            mbox_protocol_t proto = hci_mbox_status.h2c_rx_hdr[2];
#else
            mbox_protocol_t proto = MBOX_PROTO_BT;
#endif
            if (hci_mbox_status.host_rx_progress[proto])
            {
                mbox_read(hci_mbox_status.h2c_rx_buffer[proto],
                          hci_mbox_status.h2c_rx_len,
                          hci_mbox_status.host_rx_progress[proto]);
            }
            else
            {
                mbox_read(hci_mbox_status.h2c_rx_buffer[proto], hci_mbox_status.h2c_rx_len, hci_mbox_rx_continue);
            }

            break;
        }

        case HCI_MBOX_H2C_RX_PAYLOAD:
        {
#if (MBOX_PKT_HEADER_SZ > 2)
            mbox_protocol_t proto = hci_mbox_status.h2c_rx_hdr[2];
#else
            mbox_protocol_t proto = MBOX_PROTO_BT;
#endif
            hci_mbox_host_rx_done_cb_t callback = hci_mbox_status.h2c_rx_done_cb[proto];
            uint8_t * buf = hci_mbox_status.h2c_rx_buffer[proto];
            uint16_t  len = hci_mbox_status.h2c_rx_len;

            // Reset SM
            hci_mbox_status.h2c_rx_buffer[proto]  = NULL;
            hci_mbox_status.h2c_rx_len            = 0;
            hci_mbox_status.h2c_rx_done_cb[proto] = NULL;
            hci_mbox_status.h2c_rx_state          = HCI_MBOX_H2C_RX_IDLE;

            // And call RX done
            callback((const uint8_t *) buf, len);

            // TODO: Re-triggering may be need here on a per-protocol base

            break;
        }
    }
}

/*
 *  Transmit to Controller complete
 */
static void hci_mbox_tx_continue (uint8_t status)
{
    (void) status;

    switch (hci_mbox_status.h2c_tx_state)
    {
        case HCI_MBOX_H2C_TX_IDLE:
        {
            // Error?
            break;
        }

        case HCI_MBOX_H2C_TX_HDR:
        {
            hci_mbox_status.h2c_tx_state = HCI_MBOX_H2C_TX_PAYLOAD;
#if (MBOX_PKT_HEADER_SZ > 2)
            mbox_protocol_t proto = hci_mbox_status.h2c_tx_hdr[2];
#else
            mbox_protocol_t proto = MBOX_PROTO_BT;
#endif
            if (hci_mbox_status.host_tx_progress[proto])
            {
                mbox_write(hci_mbox_status.h2c_tx_buffer, hci_mbox_status.h2c_tx_len,
                           hci_mbox_status.host_tx_progress[proto]);
            }
            else
            {
                mbox_write(hci_mbox_status.h2c_tx_buffer, hci_mbox_status.h2c_tx_len, hci_mbox_tx_continue);
            }

            break;
        }

        case HCI_MBOX_H2C_TX_PAYLOAD:
        {
            // call Tx done reset SM
            hci_mbox_status.h2c_tx_buffer = NULL;
            hci_mbox_status.h2c_tx_len    = 0;
            hci_mbox_status.h2c_tx_state  = HCI_MBOX_H2C_TX_IDLE;

#if (HCI_MBOX_HOST_WRITE_HAS_MUTEX)
            hci_mbox_host_write_mutex_unlock();
#endif
            if (hci_mbox_status.h2c_tx_done_cb)
            {
                hci_mbox_status.h2c_tx_done_cb(0);
            }

            break;
        }
    }
}

/**
 * @brief Mbox RX Complete Callback for receiving configuration from controller
 *
 * @param status The status code of Mbox Rx Complete. Not Used.
 */
static void hci_config_rx_complete (uint8_t status)
{
    // Call the HCI Shim callback and reset
    hci_mbox_status.conf_received_callback(status);
    hci_mbox_status.conf_received_callback = NULL;
    hci_mbox_status.conf_rx_buf            = NULL;
}

/**
 * @brief Event handler for HCI Shim Events
 *
 * @param event_code The received event code. This should be the registered event code
 * @param event_payload The received payload
 */
static void ipc_hci_mbox_host_event_handler (ipc_event_code event_code, ipc_event_payload event_payload)
{
    ASSERT_ERROR(event_code == IPC_EVENT_CODE_HCI_SHIM);

    uint8_t  shim_hci_event_code = HCI_MBOX_EVENT_GET_CODE(event_payload);
    uint16_t shim_hci_event_data = HCI_MBOX_EVENT_GET_DATA(event_payload);

    switch (shim_hci_event_code)
    {
        case HCI_MBOX_EVENT_C2H_CMAC_READY:
        {
            hci_mbox_status.cmac_ready_received = 1;
            hci_mbox_set_state(HCI_MBOX_STATE_NORMAL);
#if (HCI_MBOX_EXECUTES_CMAC_RESET == 1)

            // trigger ready after reset
            if (hci_mbox_status.request_reset_ack)
            {
                // Send event to request HCI reset Complete
                ipc_event_payload pld = HCI_MBOX_EVENT_CREATE_PAYLOAD(HCI_MBOX_EVENT_H2C_TRIGGER_HCI_RESET_CMPLT, 0);
                ipc_write_data(IPC_CHANNEL_SYS2CMAC, IPC_EVENT_CODE_HCI_SHIM, pld);

                hci_mbox_status.request_reset_ack = 0;
            }
#endif
            break;
        }

        case HCI_MBOX_EVENT_C2H_CTRL_CONFIG:
        {
            uint8_t sz = (uint8_t) shim_hci_event_data;
            mbox_read(hci_mbox_status.conf_rx_buf, sz, hci_config_rx_complete);
            break;
        }
    }
}

/*
 *  Public API
 */

/*
 *  Pre-Initialize Host side. Blocks at mbox_init
 */
hci_mbox_err_t hci_mbox_host_preinit (void)
{
    int16_t status = 0;

    // Wait for config and CMAC Ready
    if (hci_mbox_status.hci_state == HCI_MBOX_STATE_UNINITIALIZED)
    {
        hci_mbox_set_state(HCI_MBOX_STATE_WAIT_CMAC_READY);

        // Initialize CMAC2SYS IPC channel to be ready for HCI Shim events
        if (!ipc_cmac2sys_status().bits.ready)
        {
            status = ipc_initialize(IPC_CHANNEL_CMAC2SYS, NULL);
            if (status != IPC_SUCCESS)
            {
                return HCI_MBOX_HOST_ERR_IPC_INIT;
            }
        }

        // Register IPC event handler fot Controller -> Host events
        status = ipc_register_handler(IPC_CHANNEL_CMAC2SYS, IPC_EVENT_CODE_HCI_SHIM, ipc_hci_mbox_host_event_handler);
        ASSERT_ERROR(status == IPC_EVENT_CODE_HCI_SHIM);

        // Initialize Mbox HCI layer. This blocks until mbox_init() is also ran on CMAC
        mbox_init();
    }
    else
    {
        return HCI_MBOX_HOST_ALREADY_INIT;
    }

    return HCI_MBOX_NO_ERROR;
}

/*
 *  Initialize Host side. Blocks at mbox_init
 */
hci_mbox_err_t hci_mbox_host_init (mbox_protocol_t protocol, const hci_mbox_host_conf_t * host_conf)
{
    hci_mbox_err_t err = HCI_MBOX_NO_ERROR;

    // Keep enabled
    hci_mbox_status.enabled_protocols |= (1 << protocol);

    // Keep configuration, if available
    if (host_conf)
    {
        hci_mbox_status.trigger_host_state_update  = host_conf->host_state_update_trigger;
        hci_mbox_status.host_tx_progress[protocol] = host_conf->host_progress_tx;
        hci_mbox_status.host_rx_progress[protocol] = host_conf->host_progress_rx;
    }

    // Initialize mbox/ipc
    // Blocks until controller side mbox shared memory is initialized
    err = hci_mbox_host_preinit();

#if (HCI_MBOX_HOST_WRITE_HAS_MUTEX)
    hci_mbox_host_write_mutex_init();
#endif

    return err;
}

/*
 *  Receive packet from Controller side
 */
hci_mbox_err_t hci_mbox_host_read (mbox_protocol_t proto, uint8_t * hci_pkt, hci_mbox_host_rx_done_cb_t rx_done)
{
    // Cannot execute read for a protocol, if there is already one running for this.
    BSP_CHECK_FATAL(hci_mbox_status.h2c_rx_buffer[proto] == NULL);

    HCI_MBOX_HOST_CRITICAL_SECTION_DEFINE;
    HCI_MBOX_HOST_CRITICAL_SECTION_ENTER;

    // keep rx args for this protocol
    hci_mbox_status.h2c_rx_buffer[proto]  = hci_pkt;
    hci_mbox_status.h2c_rx_done_cb[proto] = rx_done;

    // reset tx if there is no ongoing transaction
    if (hci_mbox_status.h2c_rx_state == HCI_MBOX_H2C_RX_IDLE)
    {
        hci_mbox_status.h2c_rx_len    = 0;
        hci_mbox_status.h2c_rx_hdr[0] = 0;
        hci_mbox_status.h2c_rx_hdr[1] = 0;
#if (MBOX_PKT_HEADER_SZ > 2)
        hci_mbox_status.h2c_rx_hdr[2] = 0;
#endif
        hci_mbox_status.h2c_rx_state = HCI_MBOX_H2C_RX_HDR;

        mbox_read((uint8_t *) &hci_mbox_status.h2c_rx_hdr, MBOX_PKT_HEADER_SZ, hci_mbox_rx_continue);
    }

    HCI_MBOX_HOST_CRITICAL_SECTION_EXIT;

    return HCI_MBOX_NO_ERROR;
}

/*
 *  Transmit packet to Controller side
 */
hci_mbox_err_t hci_mbox_host_write (mbox_protocol_t            proto,
                                    const uint8_t            * pkt,
                                    uint16_t                   pkt_len,
                                    hci_mbox_host_tx_done_cb_t tx_done)
{
    int16_t status = 0;

#if (HCI_MBOX_HOST_WRITE_HAS_MUTEX)
    hci_mbox_host_write_mutex_lock();
#endif

    hci_mbox_status.h2c_tx_len    = pkt_len;
    hci_mbox_status.h2c_tx_hdr[0] = (uint8_t) (pkt_len & 0xFF);
    hci_mbox_status.h2c_tx_hdr[1] = (uint8_t) ((pkt_len >> 8) & 0xFF);
#if (MBOX_PKT_HEADER_SZ > 2)
    hci_mbox_status.h2c_tx_hdr[2] = (uint8_t) proto;
#endif
    hci_mbox_status.h2c_tx_buffer  = (uint8_t *) pkt;
    hci_mbox_status.h2c_tx_done_cb = tx_done;
    hci_mbox_status.h2c_tx_state   = HCI_MBOX_H2C_TX_HDR;

    status = mbox_write((uint8_t *) &hci_mbox_status.h2c_tx_hdr, MBOX_PKT_HEADER_SZ, hci_mbox_tx_continue);

    if (status)
    {
        return HCI_MBOX_HOST_ERR_TX_PENDING;
    }

    return HCI_MBOX_NO_ERROR;
}

/*
 *  HCI Reset Received
 */
void hci_mbox_host_received_hci_reset (void)
{
    // switch state
    hci_mbox_set_state(HCI_MBOX_STATE_RESET_CMAC);

#if (HCI_MBOX_EXECUTES_CMAC_RESET == 1)

    /*
     * DEVELOPER's NOTE: the code between the #if-#endif block might need a review
     * and/or re-use code from the system initialization process, e.g. call a common function!
     * At debugging, I verified that the code blocks at mbox_init of the Host.
     *//*
     * Reset the shared resources, reboot CMAC core and re-initialize
     */                                // Disable CMAC Core and wait a bit

 #if !defined(HCI_SHIM_DONT_ALLOW_CMAC_SLEEP)
    bsp_pd_enable(BSP_PD_RAD);
 #endif

    while (CRG_TOP->SYS_STAT_REG_b.RAD_IS_UP == 0)
    {
        // TODO hw_pd_wait_power_down_rad() is missing ;
        ;
    }

    /* STOP CMAC sleep timer */
    CMAC_TIMER_SLP->CM_SLP_CTRL_REG_b.TCLK_FROM_PCLK = 1;
    CMAC_TIMER_SLP->CM_SLP_CTRL_REG_b.SLP_TIMER_SW   = 0;

    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);

    /* Execute CMAC reset*/
    CRG_TOP->CLK_RADIO_REG_b.CMAC_SYNCH_RESET = 1;

    // Wait until CMAC is not active
    while (bsp_cmac_is_active())
    {
        ;
    }

    // Now Enable the CMAC Core again by releasing CMAC Synchronous Reset
    CRG_TOP->CLK_RADIO_REG_b.CMAC_SYNCH_RESET = 0;

    /* Wait until CMAC is active. */
    while (!bsp_cmac_is_active())
    {
        ;
    }

    mbox_reset();

    // NOTE: Maybe stop other M33<->M0 services (crypto, rng) ?
    // Maybe re-init M33<->M0 services?
    hci_mbox_set_state(HCI_MBOX_STATE_UNINITIALIZED);
    hci_mbox_host_init(hci_mbox_status.enabled_protocols, NULL);

 #if TEST_HCI_SHIM
    {
        while (!hci_mbox_status.cmac_ready_received)
        {
            ;
        }
    }
 #endif

 #if dg_configSECURITY_TOOLBOX

    // NOTE: crypto_protocol_init_server(), like mbox_init(), waits forever for the CMAC side to initialize.
    // Call them in the same order as in system initialization to avoid "locking"
    crypto_server_restart();
 #endif

 #if !defined(HCI_SHIM_DONT_ALLOW_CMAC_SLEEP)
    bsp_pd_disable(BSP_PD_RAD);
 #endif

    R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);
#else                                  /* HCI_MBOX_EXECUTES_CMAC_RESET == 0 */
 #if (HCI_MBOX_CHEAT_ON_HCI_RESET == 1)

    // NOTE: reset work around
    hci_mbox_status.cheat_on_reset = 1;
    hci_mbox_set_state(HCI_MBOX_STATE_NORMAL);
 #else

    // Since the Shim doesn't execute reset, we shortcut the procedure
    // just wait CMAC Ready
    hci_mbox_set_state(HCI_MBOX_STATE_WAIT_CMAC_READY);
 #endif
#endif                                 /* HCI_MBOX_EXECUTES_CMAC_RESET */
}

void hci_mbox_host_get_ctrl_conf (uint8_t * conf_buffer, hci_mbox_host_get_ctrl_conf_cb_t get_conf_complete)
{
    // Keep the pointers from the HCI Shim layer
    hci_mbox_status.conf_rx_buf            = conf_buffer;
    hci_mbox_status.conf_received_callback = get_conf_complete;

    ipc_event_payload pld = HCI_MBOX_EVENT_CREATE_PAYLOAD(HCI_MBOX_EVENT_H2C_REQUEST_CTRL_CONFIG, 0);
    ipc_write_data(IPC_CHANNEL_SYS2CMAC, IPC_EVENT_CODE_HCI_SHIM, pld);
}

void hci_mbox_host_trigger_hw_error (mbox_bt_hci_hw_error_code_t code)
{
    // trigger controller to send an HW_ERROR event
    ipc_event_payload pld = HCI_MBOX_EVENT_CREATE_PAYLOAD(HCI_MBOX_EVENT_H2C_TRIGGER_HCI_HW_ERR, (uint8_t) code);
    ipc_write_data(IPC_CHANNEL_SYS2CMAC, IPC_EVENT_CODE_HCI_SHIM, pld);
}

void hci_mbox_host_progress_tx_rx (uint8_t direction, uint8_t status)
{
    switch (direction)
    {
        case MBOX_HOST_PROGRESS_TX:
        {
            hci_mbox_tx_continue(status);
            break;
        }

        case MBOX_HOST_PROGRESS_RX:
        {
            hci_mbox_rx_continue(status);
            break;
        }
    }
}
