/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/* Copyright (C) 2026 Modified by Renesas Electronics Corporation and/or its affiliates */

#include "platform-ra.h"
#include "openthread/logging.h"
#include "openthread-system.h"
#include "openthread/platform/alarm-micro.h"
#include "openthread/platform/radio.h"
#include "openthread/platform/entropy.h"
#include "utils/code_utils.h"
#include "utils/mac_frame.h"
#include "logging_helpers.h"
#include "rm_ftdf_api.h"

#if !defined(MBEDTLS_CONFIG_FILE)
 #include "mbedtls/config.h"
#else
 #include MBEDTLS_CONFIG_FILE
#endif

#define SHR_DURATION_US                                    (160) /* Duration of SHR in us */
#define CCA_TIME_US                                        (128)

#define DEFAULT_CCA_MODE                                   (0x03) /** FTDF_CCA_ED_OR_CS*/

#define RM_OT_CSMA_BACKOFFS_MAX_MASK    (0x07U)   /* 3-bit field width mask for csma_backoffs ops field */
#define RM_OT_USEC_PER_SEC              (1000000ULL) /* Microseconds per second, for timestamp formatting */

/* TODO Perhaps a different definition should be chosen */
#ifdef MBEDTLS_AES_ALT
 #define USING_AES128_HW_ACCELERATOR                       (1)
#endif

/* TODO Perhaps a different definition should be chosen */
#ifdef USING_AES128_HW_ACCELERATOR

/* Calculation of frame encryption time using HW crypto-accelerator  */
static uint32_t hw_encryption_usec(otRadioFrame * aFrame);

 #define FRAME_ENCRYPTION_TIME(frame)    hw_encryption_usec(frame)
#else

/* Calculation of frame encryption time using software encryption */
static uint32_t sw_encryption_usec(otRadioFrame * aFrame);

 #define FRAME_ENCRYPTION_TIME(frame)    sw_encryption_usec(frame)
#endif

static uint64_t radio_get_now (void);

typedef enum ftdf_event_bits_e
{
    OT_FTDF_EVENT_GO_SLEEP,            // Requested to enter Sleep state.

    OT_FTDF_EVENT_TX_DONE,             // Transmitted frame and received ACK (if requested).
    OT_FTDF_EVENT_TX_FAIL,             // Failed to transmit frame (received invalid or no ACK).
    OT_FTDF_EVENT_TX_FAIL_CH_BUSY,     // Failed to transmit frame (channel busy).
    OT_FTDF_EVENT_TX_FAIL_NO_ACK,      // Failed to transmit frame (received invalid or no ACK).

    OT_FTDF_EVENT_RX_Q_OVFL,           // Received queue overflowed.
    OT_FTDF_EVENT_RX_FAIL,             // Failed to receive a valid frame.

    OT_FTDF_EVENT_SCAN_START,          // Requested to start Energy Detection procedure.
    OT_FTDF_EVENT_SCAN_DONE,           // Energy Detection finished.

    OT_FTDF_EVENT_MAX
} ftdf_event_bits_t;

typedef enum EDState_e
{
    OT_FTDF_ED_STATE_IDLE = 0,
    OT_FTDF_ED_STATE_FULL_SCAN,
    OT_FTDF_ED_STATE_SHORT_SCAN
} EDState_t;

typedef struct EDScanResult_e
{
    fsp_err_t RetCode;
    int8_t    RSSI;
} EDScanResult_t;

/*
 * Enumeration for storing Acknowledgments State
 */
typedef enum otAckState_e
{
    ACK_DISABLED,
    ACK_ACTION_DISABLE,
    ACK_ACTION_ENABLE,
    ACK_ENABLED
} otAckState_t;

typedef struct localRadioContext_s
{
    int8_t MaxTxPowerTable[OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MAX - OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MIN + 1];

    bool             isSrcMatchPBEnabled : 1;
    bool             isEnabled           : 1;
    volatile bool    isTxInProgress      : 1;
    bool             isGoingToSleep      : 1;
    bool             isTxFrameCopyValid  : 1;
    volatile uint8_t enableAcks          : 2;
    int8_t           ccaEnergyThreshold;
    uint8_t          eui64[OT_EXT_ADDRESS_SIZE];

    uint32_t RadioMacFrameCounter;

    struct
    {
        uint16_t      TimeMs;
        uint8_t       Channel;
        EDState_t     State;
        QueueHandle_t Queue;
    } EnergyDetection;

    otRadioContext OtRadioContext;
} localRadioContext_t;

static localRadioContext_t localRadioContext;

/*
 * Event Management
 */

static volatile uint32_t ulOTRadioEvtNV;

#define OT_RADIO_EVT_RAISE(bit)     (ulOTRadioEvtNV |= (1 << bit))
#define OT_RADIO_EVT_ACTIVE(bit)    ((ulOTRadioEvtNV & (1 << bit)) != 0)
#define OT_RADIO_EVT_CLEAR(bit)     (ulOTRadioEvtNV &= (uint32_t) ~(1 << bit))
#define OT_RADIO_EVT_IS_EMPTY()     (ulOTRadioEvtNV == 0)

/*
 * Buffer definitions for reception and transmission of packets
 */

typedef struct xOtRcvdMsg_s
{
    uint8_t               len;
    uint8_t               buf_idx;
    uint8_t             * psdu;
    ftdf_rx_frame_param_t params;
} xOtRcvdMsg_t;

#define OT_RX_BUFS_NUM              (8)
#define OT_RadioRxMsgBuffer_size    (OT_RX_BUFS_NUM * sizeof(xOtRcvdMsg_t) + sizeof(configMESSAGE_BUFFER_LENGTH_TYPE))

static MessageBufferHandle_t xOtRcvdMsgBuffHandle;

static __ALIGNED(4) uint8_t sTransmitPsdu[OT_RADIO_FRAME_MAX_SIZE];
static __ALIGNED(4) uint8_t sTransmitPsduSaved[OT_RADIO_FRAME_MAX_SIZE];
static __ALIGNED(4) uint8_t sReceivePsdu[OT_RX_BUFS_NUM][OT_RADIO_FRAME_MAX_SIZE];
static __ALIGNED(4) uint8_t sAckPsdu[OT_RADIO_FRAME_MAX_SIZE];

static otRadioFrame sTransmitFrame, sAckFrame, sTransmitFrameSaved;
static otError      sReceiveError;

static volatile uint8_t  ucOtRxBuffInUse;
static volatile uint32_t ulOtRxBuffUseMask;

/*
 * OpenThread data primitives
 */

#define OT_RADIO_FRAME_FCS_SZ           (2)

typedef enum FrameDirection_e
{
    DIR_TX = 0,
    DIR_RX = 1
} FrameDirection_t;

#define OT_FTDF_FRAMES_LEN_SYNC(dir, len)    (dir ? (len + OT_RADIO_FRAME_FCS_SZ) : (len - OT_RADIO_FRAME_FCS_SZ))

#define OT_FTDF_SHORT_ED_SCAN_TIME_MS         (50)
#define OT_FTDF_ED_SCAN_DEFAULT_TIMEOUT_MS    (50)

__STATIC_INLINE bool otRadioEvtCheck_n_ClearIfSet (ftdf_event_bits_t evt)
{
    if (OT_RADIO_EVT_ACTIVE(evt))
    {
        OT_RADIO_EVT_CLEAR(evt);

        return true;
    }

    return false;
}

__STATIC_INLINE otError take_a_buf (uint8_t * buf_index)
{
    uint8_t idx = 0;

    for (idx = 0; idx < OT_RX_BUFS_NUM; idx++)
    {
        if ((ulOtRxBuffUseMask & (1 << idx)) == 0)
        {
            ulOtRxBuffUseMask |= (1 << idx);
            ucOtRxBuffInUse++;
            *buf_index = idx;

            return OT_ERROR_NONE;
        }
    }

    return OT_ERROR_NO_BUFS;
}

__STATIC_INLINE void release_buf (uint8_t buf_id)
{
    OT_ASSERT(buf_id < OT_RX_BUFS_NUM);
    if ((ulOtRxBuffUseMask & (1 << buf_id)) != 0)
    {
        ulOtRxBuffUseMask ^= 1 << buf_id;
        ucOtRxBuffInUse--;
    }
}

static uint64_t get_ot_frame_timestamp (ftdf_time_t radio_timestamp)
{
    /* The current time is always greater than the time at which a frame was either sent or received.
     * Therefore, the lower 32 bits of the current time should always be greater than the lower
     * 32 bits of the radio timestamp, unless an overflow occurs.
     */
    uint64_t radio_now;
    gp_openthread_port_ftdf_instance->p_api->timeUsNow64BitGet(gp_openthread_port_ftdf_instance->p_ctrl, &radio_now);
    uint64_t diff = radio_now - radio_timestamp;

    uint64_t ot_frame_timestamp;

    if (radio_now > radio_timestamp)
    {
        ot_frame_timestamp = radio_get_now() - diff;
    }
    else
    {
        /* radio time wraps at 2^31*/
        ot_frame_timestamp = radio_get_now() - diff + (UINT32_MAX / 2);
    }

    return ot_frame_timestamp;
}

static void tx_complete_cb (const ftdf_tx_conf_t * tx_conf)
{
    ftdf_event_bits_t ntfy_evt = 0;

    /* Clear the flag for the presence of the actuated transmission,
     * which is used to prevent Tx and RxAt operations from competing.
     * The stack does not control this.
     * There are a few reasons to do it here:
     *   1. Obviously, the transmission has completed and does not
     *      need to interrupt the RxAt operation right now.
     *   2. In `otPlatRadioProcess()`, this must be done four times:
     *      on TX_DONE, TX_FAIL_CH_BUSY, TX_FAIL_NO_ACK, and TX_FAIL events.
     *   3. The operation is very lightweight, so executing it in the
     *      context of an interrupt is not a problem.
     */
    localRadioContext.isTxInProgress = false;
    sAckFrame.mLength                = 0;

    if (RM_FTDF_TX_STATUS_OK == tx_conf->status)
    {
        ntfy_evt = OT_FTDF_EVENT_TX_DONE;

        if ((tx_conf->p_ack_psdu != NULL) && (tx_conf->ack_psdu_length != 0))
        {
            /* Note: the following OT_SPECIAL_ASSERT will not prevent from execution new CMAC ISR */
            OT_SPECIAL_ASSERT(tx_conf->ack_psdu_length <= OT_RADIO_FRAME_MAX_SIZE,
                              OT_PLAT_FATAL_ERROR_SOURCE_CMAC,
                              "TX callback: ACK PSDU length exceeds OT_RADIO_FRAME_MAX_SIZE");
            memcpy(sAckFrame.mPsdu, tx_conf->p_ack_psdu, tx_conf->ack_psdu_length);

            /* Although it does not verify the FCS, the OpenThread stack expects
             * a MAC ACK with an FCS. However, it does verify the size!
             */
            sAckFrame.mPsdu[tx_conf->ack_psdu_length]     = 0;
            sAckFrame.mPsdu[tx_conf->ack_psdu_length + 1] = 0;
            sAckFrame.mLength = tx_conf->ack_psdu_length + sizeof(uint8_t) * 2;
        }

        sAckFrame.mInfo.mRxInfo.mRssi           = tx_conf->ack_rssi;
        sAckFrame.mInfo.mRxInfo.mLqi            = tx_conf->ack_lqi;
        sAckFrame.mInfo.mRxInfo.mTimestamp      = get_ot_frame_timestamp(tx_conf->ack_rx_timestamp);
        sTransmitFrame.mInfo.mTxInfo.mTimestamp = get_ot_frame_timestamp(tx_conf->tx_timestamp);
    }
    else if (RM_FTDF_TX_STATUS_CHANNEL_BUSY == tx_conf->status)
    {
        // Handle TX failure case
        ntfy_evt = OT_FTDF_EVENT_TX_FAIL_CH_BUSY;
    }
    else if (RM_FTDF_TX_STATUS_NO_ACK == tx_conf->status)
    {
        // Handle TX failure case
        ntfy_evt = OT_FTDF_EVENT_TX_FAIL_NO_ACK;
    }
    else
    {
        ntfy_evt = OT_FTDF_EVENT_TX_FAIL;
    }

    if (ntfy_evt != 0)
    {
        OT_RADIO_EVT_RAISE(ntfy_evt);
        otSysEventSignalPending();
    }
}

static void rx_complete_cb (uint8_t * buf, uint8_t len, ftdf_rx_frame_param_t * params, ftdf_rx_status_t status)
{
    BaseType_t   xHigherPriorityTaskWoken = pdFALSE;
    xOtRcvdMsg_t msg     = {0};
    uint8_t      buf_idx = 0;

    if ((status == RM_FTDF_RX_STATUS_OK) &&
        (len > 0) &&
        (len <= OT_RADIO_FRAME_MAX_SIZE) &&
        (buf != NULL) &&
        (params != NULL))
    {
        if (xMessageBufferIsFull(xOtRcvdMsgBuffHandle) || (OT_ERROR_NONE != take_a_buf(&buf_idx)))
        {
            OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_RX_Q_OVFL);
        }
        else
        {
            /*
             * If there are no free buffers, command the disabling of Automatic Acknowledgments
             */
            if ((ACK_ENABLED == localRadioContext.enableAcks) && (ucOtRxBuffInUse >= OT_RX_BUFS_NUM))
            {
                localRadioContext.enableAcks = ACK_ACTION_DISABLE;
            }

            memcpy(&msg.params, params, sizeof(ftdf_rx_frame_param_t));
            msg.buf_idx = buf_idx;
            msg.len     = (uint8_t)OT_FTDF_FRAMES_LEN_SYNC(DIR_RX, len);
            msg.psdu    = sReceivePsdu[buf_idx];
            memcpy(msg.psdu, buf, len);
            xMessageBufferSendFromISR(xOtRcvdMsgBuffHandle, &msg, sizeof(xOtRcvdMsg_t), &xHigherPriorityTaskWoken);

            /* If the radio has increased the MAC Frame Counter, remember its value locally */
            if ((msg.params.ack_frame_ctr > localRadioContext.OtRadioContext.mMacFrameCounter) ||
                (msg.params.ack_frame_ctr > localRadioContext.RadioMacFrameCounter))
            {
                localRadioContext.RadioMacFrameCounter = msg.params.ack_frame_ctr;
            }
        }
    }
    else
    {
        sReceiveError = OT_ERROR_FCS;  /* TODO for RM_FTDF: Add some params or API to provide more details about RX state */
        OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_RX_FAIL);
    }

    otSysEventSignalPending();
}

static void scan_complete_cb (fsp_err_t status, int8_t max_rssi, uint8_t ed_value)
{
    OT_UNUSED_VARIABLE(ed_value);

    BaseType_t     xHigherPriorityTaskWoken = pdFALSE;
    EDScanResult_t result = {.RetCode = status, .RSSI = max_rssi};
    (void) xQueueOverwriteFromISR(localRadioContext.EnergyDetection.Queue, &result, &xHigherPriorityTaskWoken);
    if (localRadioContext.EnergyDetection.State == OT_FTDF_ED_STATE_FULL_SCAN)
    {
        OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_SCAN_DONE);
        otSysEventSignalPending();
    }

    localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_IDLE;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * Initialize the RX/TX buffers.
 *
 * Zeros out the receive and transmit buffers and sets up the data structures
 * of the receive queue.
 */
static void rfCoreInitBufs (void)
{
    memset(sTransmitPsdu, 0x00, sizeof(sTransmitPsdu));
    sTransmitFrame.mPsdu        = sTransmitPsdu;
    sTransmitFrame.mLength      = 0;
    sTransmitFrameSaved.mPsdu   = sTransmitPsduSaved;
    sTransmitFrameSaved.mLength = 0;
    sAckFrame.mPsdu             = sAckPsdu;
    sAckFrame.mLength           = 0;
}

void otPlatRadioGetIeeeEui64 (otInstance * aInstance, uint8_t * aIeeeEui64)
{
    OT_UNUSED_VARIABLE(aInstance);

    /** Recalling IeeeEui64 from local context as a workaround */
    /** gp_openthread_port_ftdf_instance->p_api->ieeeEui64Get(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aIeeeEui64);*/
    memcpy(aIeeeEui64, &localRadioContext.eui64, OT_EXT_ADDRESS_SIZE );

    otLogDebgPlat("otPlatRadioGetIeeeEui64 " TRACE_64_FMT, TRACE_64_R(aIeeeEui64));
}

void otPlatRadioSetPanId (otInstance * aInstance, otPanId aPanid)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->panIdSet(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aPanid);
    otLogDebgPlat("otPlatRadioSetPanId 0x%04x", aPanid);
}

static void reverse_ext_address (otExtAddress * aReversed, const otExtAddress * aOrigin)
{
    for (size_t i = 0; i < sizeof(*aReversed); i++)
    {
        aReversed->m8[i] = aOrigin->m8[sizeof(*aOrigin) - 1 - i];
    }
}

void otPlatRadioSetExtendedAddress (otInstance * aInstance, const otExtAddress * aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->longAddressSet(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aExtAddress->m8);

    reverse_ext_address(&localRadioContext.OtRadioContext.mExtAddress, aExtAddress);

    otLogDebgPlat("otPlatRadioSetExtendedAddress " TRACE_64_FMT, TRACE_64_R(aExtAddress->m8));
}

void otPlatRadioSetShortAddress (otInstance * aInstance, otShortAddress aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->shortAddressSet(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aShortAddress);

    localRadioContext.OtRadioContext.mShortAddress = aShortAddress;

    otLogDebgPlat("otPlatRadioSetShortAddress 0x%04x", aShortAddress);
}

void otPlatRadioSetPromiscuous (otInstance * aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->promiscuousModeSet(gp_openthread_port_ftdf_instance->p_ctrl, aEnable);
}

void otPlatRadioInit (void)
{
    xOtRcvdMsgBuffHandle = xMessageBufferCreate(OT_RadioRxMsgBuffer_size);
    OT_ASSERT(xOtRcvdMsgBuffHandle);

    localRadioContext.EnergyDetection.Queue = xQueueCreate(1, sizeof(EDScanResult_t));
    OT_ASSERT(localRadioContext.EnergyDetection.Queue);

    localRadioContext.isEnabled             = false;
    localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_IDLE;

    rfCoreInitBufs();

    ftdf_callbacks_t radio_callbacks = {0};

    radio_callbacks.tx_complete      = tx_complete_cb;
    radio_callbacks.rx_complete      = rx_complete_cb;
    radio_callbacks.ed_scan_complete = scan_complete_cb;

    /** Set EUI64 from the only unique variable available on the platform 
     *  otPlatEntropyGet returns a seed for random, which is injected into the binary each time the binary is compiled and, thus, unique per binary and untouchable by reboot.
     */
    otPlatEntropyGet(localRadioContext.eui64, OT_EXT_ADDRESS_SIZE);

    fsp_err_t err = gp_openthread_port_ftdf_instance->p_api->open(gp_openthread_port_ftdf_instance->p_ctrl, gp_openthread_port_ftdf_instance->p_cfg);
    OT_ASSERT(FSP_SUCCESS == err);

    err = gp_openthread_port_ftdf_instance->p_api->callbackSet(gp_openthread_port_ftdf_instance->p_ctrl, &radio_callbacks);

    OT_ASSERT(FSP_SUCCESS == err);

    localRadioContext.enableAcks = ACK_ENABLED;

    err = gp_openthread_port_ftdf_instance->p_api->autoEnhAckEnable(gp_openthread_port_ftdf_instance->p_ctrl);

    OT_ASSERT(FSP_SUCCESS == err);

    localRadioContext.ccaEnergyThreshold = OPENTHREAD_PORT_CFG_FTDF_DEFAULT_CCA_THRESHOLD;

    err = gp_openthread_port_ftdf_instance->p_api->ccaConfig(gp_openthread_port_ftdf_instance->p_ctrl, DEFAULT_CCA_MODE, CCA_TIME_US, localRadioContext.ccaEnergyThreshold);
    OT_ASSERT(FSP_SUCCESS == err);
    
    localRadioContext.OtRadioContext.mAlternateShortAddress = OT_RADIO_INVALID_SHORT_ADDR;

    otLogDebgPlat("otPlatRadioInit");
}

bool otPlatRadioIsEnabled (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return localRadioContext.isEnabled;
}

otError otPlatRadioEnable (otInstance * aInstance)
{
    otError error = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (!localRadioContext.isEnabled)
    {
        localRadioContext.isEnabled = true;
    }
    else
    {
        error = OT_ERROR_INVALID_STATE;
    }

    return error;
}

otError otPlatRadioDisable (otInstance * aInstance)
{
    otError error = OT_ERROR_NONE;

    otEXPECT(otPlatRadioIsEnabled(aInstance));
    otEXPECT_ACTION(otPlatRadioGetState(aInstance) == OT_RADIO_STATE_SLEEP ||
                    !localRadioContext.isGoingToSleep,
                    error = OT_ERROR_INVALID_STATE);

    localRadioContext.isEnabled = false;

exit:

    return error;
}

otError otPlatRadioSleep (otInstance * aInstance)
{
    fsp_err_t sts   = FSP_SUCCESS;
    otError   error = OT_ERROR_FAILED;

    OT_UNUSED_VARIABLE(aInstance);

    otLogDebgPlat("Radio going to Sleep");

    sts = gp_openthread_port_ftdf_instance->p_api->idleSet(gp_openthread_port_ftdf_instance->p_ctrl);

    if (FSP_SUCCESS == sts)
    {
        error = OT_ERROR_NONE;
        localRadioContext.isGoingToSleep = false;
    }
    else if (FSP_ERR_IN_USE == sts)
    {
        OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_GO_SLEEP);
        error = OT_ERROR_INVALID_STATE;
        localRadioContext.isGoingToSleep = true;
        otSysEventSignalPending();
    }
    else
    {
        otLogCritPlat("Failed to sleep radio");
        OT_ASSERT(false);
    }

    return error;
}

otError otPlatRadioReceive (otInstance * aInstance, uint8_t aChannel)
{
    fsp_err_t sts   = FSP_SUCCESS;
    otError   error = OT_ERROR_FAILED;

    OT_UNUSED_VARIABLE(aInstance);

    sts = gp_openthread_port_ftdf_instance->p_api->channelSet(gp_openthread_port_ftdf_instance->p_ctrl, 0, aChannel);

    if (FSP_SUCCESS != sts)
    {
        otLogCritPlat("Failed to set channel %u, err: %u", aChannel, sts);
    }

    sts = gp_openthread_port_ftdf_instance->p_api->rxOnSet(gp_openthread_port_ftdf_instance->p_ctrl);

    if (FSP_SUCCESS == sts)
    {
        error = OT_ERROR_NONE;
        otLogDebgPlat("Success to enable Rx, ch: %u", aChannel);
    }
    else if (FSP_ERR_IN_USE != sts)
    {
        otLogCritPlat("Failed to enable Rx %u, ch %u", sts, aChannel);
        OT_ASSERT(false);

        // error = OT_ERROR_FAILED;
    }
    else
    {
        otLogWarnPlat("Failed to enable Rx - BUSY, ch %u", aChannel);
    }

    return error;
}

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
 #ifdef USING_AES128_HW_ACCELERATOR
static uint32_t hw_encryption_usec (otRadioFrame * aFrame)
{
    /* TODO */
    OT_UNUSED_VARIABLE(aFrame);
    return 50;                         /* Random number. If using a crypto-accelerator, a proper calculation will be needed. */
}

 #else
static uint32_t sw_encryption_usec (otRadioFrame * aFrame)
{
    if (otMacFrameIsSecurityEnabled(aFrame) && !aFrame->mInfo.mTxInfo.mIsSecurityProcessed)
    {

        /* The software encryption time is obtained empirically
         * and calculated using the following formula:
         * encr_time = frame_length * 5.3 + 305
         */
        return ((uint32_t) aFrame->mLength * 53) / 10 + 305;
    }
    else
    {

        /* Average frame processing time witout encryption by `otMacFrameProcessTxSfd()` */
        return 10;
    }
}

 #endif                                /* USING_AES128_HW_ACCELERATOR */

static void save_restore_frame_for_retransmit (otRadioFrame * aFrame)
{
    if (aFrame->mInfo.mTxInfo.mIsARetx)
    {
        uint8_t sequence1 = 0;
        uint8_t sequence2 = 1;         /* The different values `sequence1` and `sequence2` are specifically set */

        if (localRadioContext.isTxFrameCopyValid &&
            (otMacFrameGetSequence(aFrame, &sequence1) == OT_ERROR_NONE) &&
            (otMacFrameGetSequence(&sTransmitFrameSaved, &sequence2) == OT_ERROR_NONE) &&
            (sequence1 == sequence2))
        {
            /* Restore the frame to its original state */
            memcpy(&sTransmitFrame, &sTransmitFrameSaved, sizeof(sTransmitFrame));
            memcpy(sTransmitPsdu, sTransmitPsduSaved, sizeof(sTransmitPsdu));

            /* Restore mIsARetx flag */
            aFrame->mInfo.mTxInfo.mIsARetx = true;
        }
    }
    else
    {
        /* The frame is not a retransmission. Just save it. */
        memcpy(&sTransmitFrameSaved, &sTransmitFrame, sizeof(sTransmitFrame));
        memcpy(sTransmitPsduSaved, sTransmitPsdu, sizeof(sTransmitPsdu));
        localRadioContext.isTxFrameCopyValid = true;
    }
}

#endif                                 /* OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2 */

otError otPlatRadioTransmit (otInstance * aInstance, otRadioFrame * aFrame)
{
    fsp_err_t fsp_err;
    otError   err = OT_ERROR_NONE;
    uint32_t  mac_frame_counter;
    bool      is_local_mfc_updated  = false;           /* For tracing purposes only */
    uint64_t  tx_process_start_time = radio_get_now(); /* It is also used for tracing purposes */
    uint64_t  estimated_tx_time;

    OT_ASSERT(aFrame == &sTransmitFrame);
    OT_ASSERT(aFrame->mPsdu == sTransmitPsdu);

    /* If the radio has increased the MAC Frame Counter, adjust it. */
    if (localRadioContext.RadioMacFrameCounter >= localRadioContext.OtRadioContext.mMacFrameCounter)
    {
        localRadioContext.OtRadioContext.mMacFrameCounter = localRadioContext.RadioMacFrameCounter + 1;
        is_local_mfc_updated = true;
    }

    /* Remember the MAC Frame Counter, as security procedures may change it */
    mac_frame_counter = localRadioContext.OtRadioContext.mMacFrameCounter;

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    if (localRadioContext.OtRadioContext.mCslPeriod > 0)
    {
        /**
         * When the OT stack sends a frame, it passes a memory region where the frame is stored.
         * Since we are performing encryption at the radio layer, we are modifying the frame in-place,
         * replacing the original frame with the encrypted frame, with CSL IE added.
         * The stack does not count with this behaviour, as it expects this alteration to be made in the radio core.
         * If the transmission fails, the stack resends the same pointer to this memory region, to be transmitted again.
         * To allow this method to work, before encrypting we save the frame and then restore the original frame in
         * case of a retransmission.
         * This allows the SSED device to stay synchronized with the Parent.
         * This mechanism is only needed while we perform encryption outside the radio core.
         */
        save_restore_frame_for_retransmit(aFrame);

        /* Use the TxAt mechanism with a delay time clearly greater than
         * that of all components that could delay frame transmission.
         */
        aFrame->mInfo.mTxInfo.mTxDelayBaseTime = (uint32_t)tx_process_start_time;
        aFrame->mInfo.mTxInfo.mTxDelay         = SSED_TX_AT_US;
        estimated_tx_time = tx_process_start_time + aFrame->mInfo.mTxInfo.mTxDelay;
    }
    else
    {
        localRadioContext.isTxFrameCopyValid = false;
        estimated_tx_time = tx_process_start_time;

        /* Add obvious delays to the current radio time, including the SHR duration,
         * the time for one CCA attempt, the time for frame transfer to the CMAC,
         * Tx rump up, and the time for frame encryption.
         */
        estimated_tx_time += SHR_DURATION_US + CCA_TIME_US * 2;
        estimated_tx_time += CMAC_MAILBOX_WRITE_TIME_US + CMAC_TX_RAMP_UP_US;
        estimated_tx_time += FRAME_ENCRYPTION_TIME(aFrame);
    }
    err = otMacFrameProcessTxSfd(aFrame, estimated_tx_time, &localRadioContext.OtRadioContext);
#endif                                 // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

    if (err == OT_ERROR_NONE)
    {
        ftdf_tx_frame_param_t tx_param =
        {
            .channel              = aFrame->mChannel,
            .repeats              = 0, /* Number of transmit repeats for GreenPower frame  */
            .tx_repeat_period     = 0,
            .rx_channel_after_tx  = aFrame->mInfo.mTxInfo.mRxChannelAfterTxDone,
            .ops.csma_backoffs    = (uint8_t)(aFrame->mInfo.mTxInfo.mMaxCsmaBackoffs & RM_OT_CSMA_BACKOFFS_MAX_MASK),
            .ops.is_hdr_upd       = aFrame->mInfo.mTxInfo.mIsHeaderUpdated,
            .ops.is_a_retx        = aFrame->mInfo.mTxInfo.mIsARetx,
            .ops.csma_ca_enabled  = aFrame->mInfo.mTxInfo.mCsmaCaEnabled,
            .ops.csl_present      = aFrame->mInfo.mTxInfo.mCslPresent,
            .ops.is_sec_processed = aFrame->mInfo.mTxInfo.mIsSecurityProcessed
        };

        if ((aFrame->mInfo.mTxInfo.mTxDelay != 0) || (aFrame->mInfo.mTxInfo.mTxDelayBaseTime != 0))
        {
            tx_param.ops.tx_at       = true;
            tx_param.tx_at_timestamp = aFrame->mInfo.mTxInfo.mTxDelayBaseTime + aFrame->mInfo.mTxInfo.mTxDelay;
        }

        /* If the MAC Frame Counter has changed by security procedures, update it on the radio. */
        if (localRadioContext.OtRadioContext.mMacFrameCounter != mac_frame_counter)
        {
            gp_openthread_port_ftdf_instance->p_api->macFrameCounterSet(gp_openthread_port_ftdf_instance->p_ctrl,
                                                   localRadioContext.OtRadioContext.mMacFrameCounter);
        }

        fsp_err =
            gp_openthread_port_ftdf_instance->p_api->frameSend(gp_openthread_port_ftdf_instance->p_ctrl, aFrame->mPsdu,
                                          (uint8_t)OT_FTDF_FRAMES_LEN_SYNC(DIR_TX, aFrame->mLength), &tx_param);

        /* To avoid causing additional delay, perform these traces specifically after calling RM_FTDF_FrameSend() */
        if (otLoggingGetLevel() >= OT_LOG_LEVEL_DEBG)
        {
            uint64_t radio_time_sec  = tx_process_start_time / RM_OT_USEC_PER_SEC;
            uint64_t radio_time_usec = tx_process_start_time % RM_OT_USEC_PER_SEC;

            otLogDebgPlat(
                "TX: Len %u FCF 0x%04x Seq %u hdr_upd %u sec_en %u sec_proc %u tx_at %lu ch %u radio time at start %llu.%06llu",
                aFrame->mLength,
                aFrame->mPsdu[0] | ((uint16_t) aFrame->mPsdu[1] << 8),
                aFrame->mPsdu[2],
                tx_param.ops.is_hdr_upd,
                otMacFrameIsSecurityEnabled(aFrame),
                tx_param.ops.is_sec_processed,
                tx_param.tx_at_timestamp,
                aFrame->mChannel,
                radio_time_sec,
                radio_time_usec);
        }

        if (is_local_mfc_updated)
        {
            otLogDebgPlat("Adjust local MAC Frame Counter to %lu", mac_frame_counter);
        }

        if (localRadioContext.OtRadioContext.mMacFrameCounter != mac_frame_counter)
        {
            otLogDebgPlat("Adjust radio MAC Frame Counter to %lu", localRadioContext.OtRadioContext.mMacFrameCounter);
        }

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

        /* The `otMacFrameProcessTxSfd()` function assigns the value to `aFrame->mInfo.mTxInfo.mTimestamp` */
#else

        /* Since we can't determine the transmission start time before it's complete, we're doing it roughly.
         * This won't affect anything in the OT stack anyway.
         */
        aFrame->mInfo.mTxInfo.mTimestamp = estimated_tx_time;
#endif

        /* Inform the OT stack */
        otPlatRadioTxStarted(aInstance, aFrame);

        if (FSP_SUCCESS != fsp_err)
        {
            /* The OT stack assertion failure will result if this function returns an error.
             * Take a look at the code in `core\mac\sub_mac.cpp`.
             * The error status is sent via `otPlatRadioTxDone()`.
             */
            localRadioContext.isTxInProgress = false;
            OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_TX_FAIL_CH_BUSY);
            otSysEventSignalPending();
        }
        else
        {
            localRadioContext.isTxInProgress = true;
        }
    }
    else
    {
        otLogWarnPlat("TX Sec ERR %u: Len %u FCF 0x%04x Seq %u hdr_upd %u sec_en %u sec_proc %u tx_dly %lu ch %u",
                      err,
                      aFrame->mLength,
                      aFrame->mPsdu[0] | ((uint16_t) aFrame->mPsdu[1] << 8),
                      aFrame->mPsdu[2],
                      aFrame->mInfo.mTxInfo.mIsHeaderUpdated,
                      otMacFrameIsSecurityEnabled(aFrame),
                      aFrame->mInfo.mTxInfo.mIsSecurityProcessed,
                      aFrame->mInfo.mTxInfo.mTxDelay,
                      aFrame->mChannel);

        localRadioContext.isTxInProgress = false;
        OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_TX_FAIL);
        otSysEventSignalPending();
    }

    return OT_ERROR_NONE;
}

otRadioFrame * otPlatRadioGetTransmitBuffer (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return &sTransmitFrame;
}

int8_t otPlatRadioGetRssi (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    int8_t rssi_i8 = OT_RADIO_RSSI_INVALID;

    bool isRxOn;
    gp_openthread_port_ftdf_instance->p_api->rxStateGet(gp_openthread_port_ftdf_instance->p_ctrl, &isRxOn);

    if (localRadioContext.isEnabled &&
        (localRadioContext.EnergyDetection.State == OT_FTDF_ED_STATE_IDLE) &&
        !localRadioContext.isTxInProgress &&
        isRxOn)
    {
        localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_SHORT_SCAN;
        xQueueReset(localRadioContext.EnergyDetection.Queue);
        if (FSP_SUCCESS == gp_openthread_port_ftdf_instance->p_api->edScanStart(gp_openthread_port_ftdf_instance->p_ctrl, OT_FTDF_SHORT_ED_SCAN_TIME_MS))
        {
            EDScanResult_t result;
            if (pdPASS ==
                xQueueReceive(localRadioContext.EnergyDetection.Queue, &result,
                              pdMS_TO_TICKS(OT_FTDF_SHORT_ED_SCAN_TIME_MS + OT_FTDF_ED_SCAN_DEFAULT_TIMEOUT_MS)))
            {
                if (result.RetCode == FSP_SUCCESS)
                {
                    rssi_i8 = result.RSSI;
                }
            }
        }

        localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_IDLE;
        gp_openthread_port_ftdf_instance->p_api->rxOnSet(gp_openthread_port_ftdf_instance->p_ctrl);
    }

    return rssi_i8;
}

otRadioCaps otPlatRadioGetCaps (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    /* NOTE: Some features are not implemented currently (We will uncomment some lines when it will possible) */
    return (otRadioCaps) (
        OT_RADIO_CAPS_ACK_TIMEOUT |
        OT_RADIO_CAPS_ENERGY_SCAN |
        /* OT_RADIO_CAPS_TRANSMIT_RETRIES         | Not implemented in current FTDF driver */
        OT_RADIO_CAPS_CSMA_BACKOFF     |
        OT_RADIO_CAPS_SLEEP_TO_TX      |
        OT_RADIO_CAPS_TRANSMIT_SEC     |
        OT_RADIO_CAPS_TRANSMIT_TIMING  |
        OT_RADIO_CAPS_RECEIVE_TIMING   |

        /* OT_RADIO_CAPS_RX_ON_WHEN_IDLE          | */
        /* OT_RADIO_CAPS_TRANSMIT_FRAME_POWER     | */
        OT_RADIO_CAPS_NONE
        );
}

bool otPlatRadioGetPromiscuous (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    bool promiscuousMode;
    gp_openthread_port_ftdf_instance->p_api->promiscuousModeGet(gp_openthread_port_ftdf_instance->p_ctrl, &promiscuousMode);

    return promiscuousMode;
}

void otPlatRadioEnableSrcMatch (otInstance * aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);

    localRadioContext.isSrcMatchPBEnabled = aEnable;

    otLogDebgPlat("otPlatRadioEnableSrcMatch %u", aEnable);
}

otError otPlatRadioAddSrcMatchShortEntry (otInstance * aInstance, uint16_t aShortAddress)
{
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS !=
        gp_openthread_port_ftdf_instance->p_api->sourceMatchShortAddressAddWithPendingBitSet(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT,
                                                                        aShortAddress,
                                                                        localRadioContext.isSrcMatchPBEnabled))
    {
        err = OT_ERROR_NO_ADDRESS;
    }

    otLogDebgPlat("otPlatRadioAddSrcMatchShortEntry 0x%04x err %u", aShortAddress, err);

    return err;
}

otError otPlatRadioAddSrcMatchExtEntry (otInstance * aInstance, const otExtAddress * aExtAddress)
{
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS !=
        gp_openthread_port_ftdf_instance->p_api->sourceMatchIeeeAddressAdd(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aExtAddress->m8))
    {
        err = OT_ERROR_NO_ADDRESS;
    }

    otLogDebgPlat("otPlatRadioAddSrcMatchExtEntry "TRACE_64_FMT " err %u", TRACE_64_R(aExtAddress->m8), err);

    return err;
}

otError otPlatRadioClearSrcMatchShortEntry (otInstance * aInstance, uint16_t aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS !=
        gp_openthread_port_ftdf_instance->p_api->sourceMatchShortAddressDelete(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aShortAddress))
    {
        return OT_ERROR_NO_ADDRESS;
    }

    return OT_ERROR_NONE;
}

otError otPlatRadioClearSrcMatchExtEntry (otInstance * aInstance, const otExtAddress * aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS !=
        gp_openthread_port_ftdf_instance->p_api->sourceMatchIeeeAddressDelete(gp_openthread_port_ftdf_instance->p_ctrl, RM_FTDF_STACK_ID_OT, aExtAddress->m8))
    {
        return OT_ERROR_NO_ADDRESS;
    }

    return OT_ERROR_NONE;
}

void otPlatRadioClearSrcMatchShortEntries (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    /*
     * TODO: Current FTDF implementation does not provide necessary APIs.
     * Probably the feature should be implemented locally
     */
}

void otPlatRadioClearSrcMatchExtEntries (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    /*
     * TODO: Current FTDF implementation does not provide necessary APIs.
     * Probably the feature should be implemented locally
     */
}

otError otPlatRadioEnergyScan (otInstance * aInstance, uint8_t aScanChannel,
                               uint16_t aScanDuration /* in milliseconds */)
{
    OT_UNUSED_VARIABLE(aInstance);

    OT_ASSERT(aScanChannel >= OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MIN && aScanChannel <= OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MAX);
    OT_ASSERT(aScanDuration > 0);

    localRadioContext.EnergyDetection.TimeMs  = aScanDuration;
    localRadioContext.EnergyDetection.Channel = aScanChannel;

    if (localRadioContext.EnergyDetection.State != OT_FTDF_ED_STATE_IDLE)
    {
        otLogWarnPlat("Energy scan already in progress");

        return OT_ERROR_BUSY;
    }

    OT_RADIO_EVT_RAISE(OT_FTDF_EVENT_SCAN_START);
    otSysEventSignalPending();

    return OT_ERROR_NONE;
}

otError otPlatRadioGetTransmitPower (otInstance * aInstance, int8_t * aPower)
{
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (aPower == NULL)
    {
        err = OT_ERROR_INVALID_ARGS;
    }

    if (err == OT_ERROR_NONE)
    {
        if (FSP_SUCCESS != gp_openthread_port_ftdf_instance->p_api->txPowerGet(gp_openthread_port_ftdf_instance->p_ctrl, aPower))
        {
            err = OT_ERROR_FAILED;
        }
    }

    otLogDebgPlat("otPlatRadioGetTransmitPower %d err %u", *aPower, err);

    return err;
}

otError otPlatRadioSetTransmitPower (otInstance * aInstance, int8_t aPower)
{
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS != gp_openthread_port_ftdf_instance->p_api->txPowerSet(gp_openthread_port_ftdf_instance->p_ctrl, aPower))
    {
        err = OT_ERROR_FAILED;
    }

    otLogDebgPlat("otPlatRadioSetTransmitPower %d err %u", aPower, err);

    return err;
}

otError otPlatRadioGetCcaEnergyDetectThreshold (otInstance * aInstance, int8_t * aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);

    *aThreshold = localRadioContext.ccaEnergyThreshold;

    return OT_ERROR_NONE;
}

otError otPlatRadioSetCcaEnergyDetectThreshold (otInstance * aInstance, int8_t aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    otError err = OT_ERROR_NONE;

    if (FSP_SUCCESS != gp_openthread_port_ftdf_instance->p_api->ccaConfig(gp_openthread_port_ftdf_instance->p_ctrl, DEFAULT_CCA_MODE, CCA_TIME_US, aThreshold))
    {
        err = OT_ERROR_FAILED;
    }
    else
    {
        localRadioContext.ccaEnergyThreshold = aThreshold;
    }

    return err;
}

otError otPlatRadioGetFemLnaGain (otInstance * aInstance, int8_t * aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aGain);

    return OT_ERROR_NOT_IMPLEMENTED;
}

otError otPlatRadioSetFemLnaGain (otInstance * aInstance, int8_t aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aGain);

    return OT_ERROR_NOT_IMPLEMENTED;
}

int8_t otPlatRadioGetReceiveSensitivity (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    int8_t receiver_sensitivity;

    fsp_err_t err = gp_openthread_port_ftdf_instance->p_api->receiverSensitivityGet(gp_openthread_port_ftdf_instance->p_ctrl, &receiver_sensitivity);
    
    OT_UNUSED_VARIABLE(err);

    return receiver_sensitivity;
}

otRadioState otPlatRadioGetState (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    otRadioState state = OT_RADIO_STATE_TRANSMIT;

    bool isIdle;
    bool isRxOn;
    gp_openthread_port_ftdf_instance->p_api->idleStateGet(gp_openthread_port_ftdf_instance->p_ctrl, &isIdle);
    gp_openthread_port_ftdf_instance->p_api->rxStateGet(gp_openthread_port_ftdf_instance->p_ctrl, &isRxOn);

    if (!otPlatRadioIsEnabled(aInstance))
    {
        state = OT_RADIO_STATE_DISABLED;
        otLogDebgPlat("Radio state DISABLED");
    }
    else if (true == isIdle)
    {
        state = OT_RADIO_STATE_SLEEP;
        otLogDebgPlat("Radio state SLEEP");
    }
    else if (true == isRxOn)
    {
        state = OT_RADIO_STATE_RECEIVE;
        otLogDebgPlat("Radio state RECEIVE");
    }
    else
    {
        otLogDebgPlat("Radio state TRANSMIT");
    }

    return state;
}

uint64_t radio_get_now (void)
{
    static uint8_t  overflow_shift     = 0;
    static uint32_t timer_overflows    = 0;
    static uint32_t prev_radio_time_us = 0;
    static uint32_t now_radio_time_us  = 0;

    UBaseType_t uxSavedInterruptStatus = 0;

    if (xPortIsInsideInterrupt())
    {
        uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    }
    else
    {
        taskENTER_CRITICAL();
    }

    gp_openthread_port_ftdf_instance->p_api->timeUsNow32BitGet(gp_openthread_port_ftdf_instance->p_ctrl, &now_radio_time_us);

    /* Timer has overflowed */
    if (prev_radio_time_us > now_radio_time_us)
    {
        /* Determine the radio time resolution.
         * We handle both 31- and 32-bit resolutions.
         * Currently, the radio time overflows when the value exceeds 2^31.
         */
        if (overflow_shift == 0)
        {
            if (prev_radio_time_us <= (UINT32_MAX / 2))
            {
                overflow_shift = 31;
            }
            else
            {
                overflow_shift = 32;
            }
        }

        timer_overflows++;
    }

    prev_radio_time_us = now_radio_time_us;

    if (xPortIsInsideInterrupt())
    {
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
    }
    else
    {
        taskEXIT_CRITICAL();
    }

    return (((uint64_t) timer_overflows) << overflow_shift) + now_radio_time_us;
}

uint64_t otPlatRadioGetNow (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return radio_get_now();
}

otError otPlatRadioEnableCsl (otInstance         * aInstance,
                              uint32_t             aCslPeriod,
                              otShortAddress       aShortAddr,
                              const otExtAddress * aExtAddr)
{
    otError      err = OT_ERROR_NONE;
    otExtAddress reversed_addr;
    OT_UNUSED_VARIABLE(aInstance);

    OT_ASSERT(aCslPeriod < UINT16_MAX);

    reverse_ext_address(&reversed_addr, aExtAddr);

    if (FSP_SUCCESS != gp_openthread_port_ftdf_instance->p_api->cslEnable(gp_openthread_port_ftdf_instance->p_ctrl, aCslPeriod, aShortAddr, reversed_addr.m8))
    {
        localRadioContext.OtRadioContext.mCslPeriod = 0;
        err = OT_ERROR_FAILED;
    }
    else
    {
        localRadioContext.OtRadioContext.mCslPeriod = (uint16_t) aCslPeriod;
    }

    otLogDebgPlat("otPlatRadioEnableCsl CslPeriod %lu ShortAddr 0x%04x ExtAddress " TRACE_64_FMT " err %u",
                  aCslPeriod,
                  aShortAddr,
                  TRACE_64(aExtAddr->m8),
                  err);

    return err;
}

otError otPlatRadioResetCsl (otInstance * aInstance)
{
    uint8_t ieee[OT_EXT_ADDRESS_SIZE] = {0};
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (FSP_SUCCESS != gp_openthread_port_ftdf_instance->p_api->cslEnable(gp_openthread_port_ftdf_instance->p_ctrl, 0, 0, ieee))
    {
        err = OT_ERROR_FAILED;
    }

    localRadioContext.OtRadioContext.mCslPeriod = 0;

    otLogDebgPlat("otPlatRadioResetCsl err %u", err);

    return err;
}

void otPlatRadioUpdateCslSampleTime (otInstance * aInstance, uint32_t aCslSampleTime)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->cslSampleTimeUpdate(gp_openthread_port_ftdf_instance->p_ctrl, aCslSampleTime);

    localRadioContext.OtRadioContext.mCslSampleTime = aCslSampleTime;
}

uint8_t otPlatRadioGetCslAccuracy (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return RADIO_CLOCK_ACCURACY;
}

uint8_t otPlatRadioGetCslUncertainty (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return PLATFORM_UNCERTAINTY;
}

void otPlatRadioSetMacKey (otInstance             * aInstance,
                           uint8_t                  aKeyIdMode,
                           uint8_t                  aKeyId,
                           const otMacKeyMaterial * aPrevKey,
                           const otMacKeyMaterial * aCurrKey,
                           const otMacKeyMaterial * aNextKey,
                           otRadioKeyType           aKeyType)
{
    OT_UNUSED_VARIABLE(aInstance);

    OT_ASSERT(aKeyType == OT_KEY_TYPE_LITERAL_KEY);

    if ((aPrevKey == NULL) || (aCurrKey == NULL) || (aNextKey == NULL))
    {
        return;
    }

    gp_openthread_port_ftdf_instance->p_api->macKeySet(gp_openthread_port_ftdf_instance->p_ctrl,
                                  aKeyIdMode,
                                  aKeyId,
                                  aPrevKey->mKeyMaterial.mKey.m8,
                                  aCurrKey->mKeyMaterial.mKey.m8,
                                  aNextKey->mKeyMaterial.mKey.m8);

    localRadioContext.OtRadioContext.mKeyId               = aKeyId;
    localRadioContext.OtRadioContext.mKeyType             = aKeyType;
    localRadioContext.OtRadioContext.mPrevMacFrameCounter = localRadioContext.OtRadioContext.mMacFrameCounter;
    localRadioContext.OtRadioContext.mMacFrameCounter     = 0;
    localRadioContext.RadioMacFrameCounter                = 0;

    memcpy(&localRadioContext.OtRadioContext.mPrevKey, aPrevKey, sizeof(otMacKeyMaterial));
    memcpy(&localRadioContext.OtRadioContext.mCurrKey, aCurrKey, sizeof(otMacKeyMaterial));
    memcpy(&localRadioContext.OtRadioContext.mNextKey, aNextKey, sizeof(otMacKeyMaterial));

    otLogDebgPlat("SetMacKey KeyIdMode %u KeyId %u KeyType %u", aKeyIdMode >> 3, aKeyId, aKeyType);
    otLogDebgPlat("     pre " TRACE_128_FMT, TRACE_128(aPrevKey->mKeyMaterial.mKey.m8));
    otLogDebgPlat("     cur " TRACE_128_FMT, TRACE_128(aCurrKey->mKeyMaterial.mKey.m8));
    otLogDebgPlat("     nxt " TRACE_128_FMT, TRACE_128(aNextKey->mKeyMaterial.mKey.m8));
}

void otPlatRadioSetMacFrameCounter (otInstance * aInstance, uint32_t aMacFrameCounter)
{
    OT_UNUSED_VARIABLE(aInstance);

    gp_openthread_port_ftdf_instance->p_api->macFrameCounterSet(gp_openthread_port_ftdf_instance->p_ctrl, aMacFrameCounter);

    localRadioContext.OtRadioContext.mMacFrameCounter = aMacFrameCounter;

    otLogDebgPlat("otPlatRadioSetMacFrameCounter %lu", aMacFrameCounter);
}

void otPlatRadioSetMacFrameCounterIfLarger (otInstance * aInstance, uint32_t aMacFrameCounter)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (aMacFrameCounter > localRadioContext.OtRadioContext.mMacFrameCounter)
    {
        gp_openthread_port_ftdf_instance->p_api->macFrameCounterSet(gp_openthread_port_ftdf_instance->p_ctrl, aMacFrameCounter);
        localRadioContext.OtRadioContext.mMacFrameCounter = aMacFrameCounter;
    }

    otLogDebgPlat("otPlatRadioSetMacFrameCounterIfLarger %lu", aMacFrameCounter);
}

otError otPlatRadioSetChannelMaxTransmitPower (otInstance * aInstance, uint8_t aChannel, int8_t aMaxPower)
{
    OT_UNUSED_VARIABLE(aInstance);

    if ((aChannel < OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MIN) || (aChannel > OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MAX))
    {
        return OT_ERROR_INVALID_ARGS;
    }

    localRadioContext.MaxTxPowerTable[aChannel - OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MIN] = aMaxPower;

    /* TODO: Probably is needed to set transmit power for current ch */

    return OT_ERROR_NONE;
}

otError otPlatRadioConfigureEnhAckProbing (otInstance         * aInstance,
                                           otLinkMetrics        aLinkMetrics,
                                           const otShortAddress aShortAddress,
                                           const otExtAddress * aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aLinkMetrics);
    OT_UNUSED_VARIABLE(aShortAddress);
    OT_UNUSED_VARIABLE(aExtAddress);

    /* TODO: Implement feature in CMAC and implement API */
    
    return OT_ERROR_NOT_IMPLEMENTED;
}

otError otPlatRadioSetRegion (otInstance * aInstance, uint16_t aRegionCode)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aRegionCode);

    return OT_ERROR_NOT_IMPLEMENTED;
}

otError otPlatRadioGetRegion (otInstance * aInstance, uint16_t * aRegionCode)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aRegionCode);

    return OT_ERROR_NOT_IMPLEMENTED;
}

void otPlatRadioProcessHighPriority (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (ACK_ACTION_DISABLE == localRadioContext.enableAcks)
    {
        gp_openthread_port_ftdf_instance->p_api->autoEnhAckDisable(gp_openthread_port_ftdf_instance->p_ctrl);
        localRadioContext.enableAcks = ACK_DISABLED;
    }
}

void otPlatRadioProcess (otInstance * aInstance)
{
    size_t       MsgRcvdBytes   = 0;
    xOtRcvdMsg_t xRcvdMsg       = {0};
    otRadioFrame sReceivedFrame = {0};

    /* TODO Split into two or three functions */

    while (xMessageBufferIsEmpty(xOtRcvdMsgBuffHandle) == pdFALSE)
    {
        MsgRcvdBytes = xMessageBufferReceive(xOtRcvdMsgBuffHandle, &xRcvdMsg, sizeof(xOtRcvdMsg_t), 0);

        if (MsgRcvdBytes == sizeof(xOtRcvdMsg_t))
        {
            xOtRcvdMsg_t * msg = &xRcvdMsg;
            ftdf_time_t       rx_timestamp; /* For tracing purposes only */
            OT_UNUSED_VARIABLE(rx_timestamp);

            OT_ASSERT(msg->psdu == sReceivePsdu[msg->buf_idx]);

            sReceivedFrame.mInfo.mRxInfo.mLqi  = msg->params.lqi;
            sReceivedFrame.mInfo.mRxInfo.mRssi = msg->params.rssi;
            sReceivedFrame.mInfo.mRxInfo.mAckedWithFramePending = msg->params.pending_bit;
            sReceivedFrame.mInfo.mRxInfo.mAckedWithSecEnhAck    = msg->params.secure_enh_ack;
            sReceivedFrame.mInfo.mRxInfo.mTimestamp             = get_ot_frame_timestamp(msg->params.rx_timestamp);
            sReceivedFrame.mInfo.mRxInfo.mAckKeyId              = msg->params.ack_key_id;
            sReceivedFrame.mInfo.mRxInfo.mAckFrameCounter       = msg->params.ack_frame_ctr;
            sReceivedFrame.mChannel = msg->params.channel;
            sReceivedFrame.mLength  = msg->len;
            sReceivedFrame.mPsdu    = sReceivePsdu[msg->buf_idx];

            otPlatRadioReceiveDone(aInstance, &sReceivedFrame, OT_ERROR_NONE);

            rx_timestamp = msg->params.rx_timestamp;

            release_buf(msg->buf_idx);

            /*
             * Upon release of a buffer, Automatic Acknowledgments can be reactivated.
             */

            if ((ACK_DISABLED == localRadioContext.enableAcks) && (ucOtRxBuffInUse < OT_RX_BUFS_NUM))
            {
                gp_openthread_port_ftdf_instance->p_api->interruptsDisable(gp_openthread_port_ftdf_instance->p_ctrl);
                localRadioContext.enableAcks = ACK_ENABLED;
                gp_openthread_port_ftdf_instance->p_api->autoEnhAckEnable(gp_openthread_port_ftdf_instance->p_ctrl);
                gp_openthread_port_ftdf_instance->p_api->interruptsEnable(gp_openthread_port_ftdf_instance->p_ctrl);
            }

            otLogDebgPlat(
                "RX_DONE: Len %u FCF 0x%04x Seq %u AckedWithSecEnhAck %u MFC %lu AckKeyId %u buf in use %u rx_timestamp %llu",
                sReceivedFrame.mLength,
                sReceivedFrame.mPsdu[0] | ((uint16_t) sReceivedFrame.mPsdu[1] << 8),
                sReceivedFrame.mPsdu[2],
                sReceivedFrame.mInfo.mRxInfo.mAckedWithSecEnhAck,
                sReceivedFrame.mInfo.mRxInfo.mAckFrameCounter,
                sReceivedFrame.mInfo.mRxInfo.mAckKeyId,
                /* The value has already been decreased by 1 in release_buf() above */
                ucOtRxBuffInUse + 1,
                rx_timestamp);
        }
        else
        {
            xMessageBufferReset(xOtRcvdMsgBuffHandle);
            otLogCritPlat("xMessageBufferReset!");
        }
    }

    if (OT_RADIO_EVT_IS_EMPTY())
    {
        return;
    }

    otLogDebgPlat("Notified value: 0x%lx", ulOTRadioEvtNV);

    if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_TX_DONE))
    {
        otLogDebgPlat("TX_DONE: Len %u FCF 0x%04x Seq %u ack_len %u",
                      sTransmitFrame.mLength,
                      sTransmitFrame.mPsdu[0] | ((uint16_t) sTransmitFrame.mPsdu[1] << 8),
                      sTransmitFrame.mPsdu[2],
                      sAckFrame.mLength);

        if (localRadioContext.OtRadioContext.mCslPeriod > 0)
        {
            /**
             * Some devices imnplement retransmission of CSL Phase without recalculating CSL IE.
             * There is a mechanism that forces an "extra" DataRequest to force the recalculation of CSL IE, for those cases.
             * Renesas platform always recalculates CSL IE when retransmitting, so it does not need this mechanism.
             */
            sTransmitFrame.mInfo.mTxInfo.mIsARetx = false;
        }

        otPlatRadioTxDone(aInstance, &sTransmitFrame, (sAckFrame.mLength > 0) ? &sAckFrame : NULL, OT_ERROR_NONE);
        sAckFrame.mLength = 0;
    }
    else if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_TX_FAIL_CH_BUSY))
    {
        otLogNotePlat("TX_FAIL_CH_BUSY");
        otPlatRadioTxDone(aInstance, &sTransmitFrame, NULL, OT_ERROR_CHANNEL_ACCESS_FAILURE);
    }
    else if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_TX_FAIL_NO_ACK))
    {
        otLogNotePlat("TX_FAIL_NO_ACK");
        otPlatRadioTxDone(aInstance, &sTransmitFrame, NULL, OT_ERROR_NO_ACK);
    }
    else if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_TX_FAIL))
    {
        otLogWarnPlat("TX_FAIL");
        otPlatRadioTxDone(aInstance, &sTransmitFrame, NULL, OT_ERROR_ABORT);
    }

    if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_RX_Q_OVFL))
    {
        otLogDebgPlat("OT_FTDF_EVENT_RX_Q_OVFL");
    }
    else if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_RX_FAIL))
    {
        otLogDebgPlat("OT_FTDF_EVENT_RX_FAIL");
        otPlatRadioReceiveDone(aInstance, NULL, sReceiveError);
    }

    if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_SCAN_START))
    {
        otLogDebgPlat("OT_FTDF_EVENT_SCAN_START Ch: %u Duration: %u ms",
                      localRadioContext.EnergyDetection.Channel,
                      localRadioContext.EnergyDetection.TimeMs);
        xQueueReset(localRadioContext.EnergyDetection.Queue);
        fsp_err_t err =
            gp_openthread_port_ftdf_instance->p_api->channelSet(gp_openthread_port_ftdf_instance->p_ctrl, 0, localRadioContext.EnergyDetection.Channel);
        if (err == FSP_SUCCESS)
        {
            err = gp_openthread_port_ftdf_instance->p_api->edScanStart(gp_openthread_port_ftdf_instance->p_ctrl, localRadioContext.EnergyDetection.TimeMs);
        }

        if (err == FSP_SUCCESS)
        {
            localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_FULL_SCAN;
        }
        else
        {
            localRadioContext.EnergyDetection.State = OT_FTDF_ED_STATE_IDLE;
            otLogCritPlat("Unable to perform energy scan Err: %u", err);
            otPlatRadioEnergyScanDone(aInstance, OT_RADIO_RSSI_INVALID);
        }
    }
    else if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_SCAN_DONE))
    {
        EDScanResult_t result;
        int8_t         rssi_i8 = OT_RADIO_RSSI_INVALID;
        otLogDebgPlat("OT_FTDF_EVENT_SCAN_DONE");
        if (pdPASS == xQueueReceive(localRadioContext.EnergyDetection.Queue, &result, 0))
        {
            rssi_i8 = result.RetCode == FSP_SUCCESS ? result.RSSI : OT_RADIO_RSSI_INVALID;
        }

        otPlatRadioEnergyScanDone(aInstance, rssi_i8);
    }

    if (otRadioEvtCheck_n_ClearIfSet(OT_FTDF_EVENT_GO_SLEEP))
    {
        (void) otPlatRadioSleep(aInstance);
    }
}

#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
otError otPlatRadioReceiveAt (otInstance * aInstance, uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    otError err = OT_ERROR_NONE;
    OT_UNUSED_VARIABLE(aInstance);

    if (localRadioContext.isTxInProgress)
    {
        otLogWarnPlat("otPlatRadioReceiveAt is prohibited because Tx is in progress");
        err = OT_ERROR_FAILED;
    }
    else
    {
        if (FSP_SUCCESS !=
            gp_openthread_port_ftdf_instance->p_api->receiveAt(gp_openthread_port_ftdf_instance->p_ctrl, aChannel, aStart - SHR_DURATION_US, aDuration))
        {
            err = OT_ERROR_FAILED;
        }

        if (otLoggingGetLevel() >= OT_LOG_LEVEL_DEBG)
        {
            uint32_t radio_tm_32;
            gp_openthread_port_ftdf_instance->p_api->timeUsNow32BitGet(gp_openthread_port_ftdf_instance->p_ctrl, &radio_tm_32); /* For tracing purposes only */

            otLogDebgPlat(
                "RX_AT ch %u starting %ld us from now, aStart %lu, local tm %lu duration %lu nextCslSampleTime %lu err %u",
                aChannel,
                aStart % (1U << 31) - radio_tm_32,
                /* radio_tm_32 overflows at 2^31. Adapting aStart to 2^31 scope */
                aStart,
                otPlatAlarmMicroGetNow(),
                aDuration,
                localRadioContext.OtRadioContext.mCslSampleTime,
                err);
        }
    }

    return err;
}

#endif                                 // OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2

/**
 * Get the bus speed in bits/second between the host and the radio chip.
 *
 * @param[in]   aInstance    A pointer to an OpenThread instance.
 *
 * @returns The bus speed in bits/second between the host and the radio chip.
 *          Return 0 when the MAC and above layer and Radio layer resides on the same chip.
 */
uint32_t otPlatRadioGetBusSpeed (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

#if OPENTHREAD_RADIO != 1
    /* It is a monolithic build */
    return 0;
#else

    /* It's an RCP. */
    uint32_t sel_rcp_uart_baudrate = OPENTHREAD_PORT_CFG_SPINEL_UART_BAUDRATE;
    uint8_t sel_rcp_uart_data_bits = gp_openthread_port_uart_instance->p_cfg->data_bits + 5;
    uint8_t sel_rcp_uart_parity_bit = gp_openthread_port_uart_instance->p_cfg->parity ? 1 : 0;
    uint8_t sel_rcp_uart_stop_bits = gp_openthread_port_uart_instance->p_cfg->stop_bits + 1;

    return sel_rcp_uart_baudrate / (1 + sel_rcp_uart_data_bits + sel_rcp_uart_parity_bit + sel_rcp_uart_stop_bits) * sel_rcp_uart_data_bits;
#endif
}

/**
 * Get the bus latency in microseconds between the host and the radio chip.
 *
 * @param[in]   aInstance    A pointer to an OpenThread instance.
 *
 * @returns The bus latency in microseconds between the host and the radio chip.
 *          Return 0 when the MAC and above layer and Radio layer resides on the same chip.
 */
uint32_t otPlatRadioGetBusLatency (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    /* TODO Find out */
    return 0;
}
