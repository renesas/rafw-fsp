/*
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    Copyright (c) 2019 Nest Labs, Inc.
 *    Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Provides an implementation of the BLEManager singleton object
 *          for the Renesas DA16600 platforms.
 */

/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>
#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE

#include <platform/internal/BLEManager.h>

#include "FreeRTOS.h"
//matterwork[[::
#include "rn_status.h"
//]]matterwork
#include "timers.h"
#include <ble/CHIPBleServiceData.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CommissionableDataProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>

#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
#include <setup_payload/AdditionalDataPayloadGenerator.h>
#endif
#include "rnDeviceWrapAPIs.h"

using namespace ::chip;
using namespace ::chip::Ble;

namespace chip {
namespace DeviceLayer {
namespace Internal {

namespace {

#define CHIP_ADV_DATA_TYPE_FLAGS 0x01
#define CHIP_ADV_DATA_TYPE_UUID 0x03
#define CHIP_ADV_DATA_TYPE_NAME 0x09
#define CHIP_ADV_DATA_TYPE_SERVICE_DATA 0x16

#define CHIP_ADV_DATA_FLAGS 0x06

#define CHIP_ADV_SHORT_UUID_LEN 2

#define MAX_RESPONSE_DATA_LEN 31
#define MAX_ADV_DATA_LEN 31

TimerHandle_t sbleAdvTimeoutTimer;

const uint8_t ShortUUID_CHIPoBLEService[]  = { 0xF6, 0xFF };
const ChipBleUUID ChipUUID_CHIPoBLEChar_RX = { { 0x18, 0xEE, 0x2E, 0xF5, 0x26, 0x3D, 0x45, 0x59, 0x95, 0x9F, 0x4F, 0x9C, 0x42, 0x9F,
                                                 0x9D, 0x11 } };
const ChipBleUUID ChipUUID_CHIPoBLEChar_TX = { { 0x18, 0xEE, 0x2E, 0xF5, 0x26, 0x3D, 0x45, 0x59, 0x95, 0x9F, 0x4F, 0x9C, 0x42, 0x9F,
                                                 0x9D, 0x12 } };

} // namespace

BLEManagerImpl BLEManagerImpl::sInstance;

CHIP_ERROR BLEManagerImpl::_Init()
{
    CHIP_ERROR err;

    // Initialize the CHIP BleLayer.
    err = BleLayer::Init(this, this, &DeviceLayer::SystemLayer());
    SuccessOrExit(err);

    memset(mBleConnections, 0, sizeof(mBleConnections));
    memset(mIndConfId, kUnusedIndex, sizeof(mIndConfId));
    mServiceMode = ConnectivityManager::kCHIPoBLEServiceMode_Enabled;

    sbleAdvTimeoutTimer = xTimerCreate("BleAdvTimer",
                                       1,
                                       false,
                                       (void *) this,
                                       BleAdvTimeoutHandler
    );

    mFlags.ClearAll().Set(Flags::kAdvertisingEnabled, CHIP_DEVICE_CONFIG_CHIPOBLE_ENABLE_ADVERTISING_AUTOSTART);
    mFlags.Set(Flags::kFastAdvertisingEnabled, true);
    ble_init();
    PlatformMgr().ScheduleWork(DriveBLEState, 0);

exit:
    return err;
}

uint16_t BLEManagerImpl::_NumConnections(void)
{
    uint16_t numCons = 0;
    for (uint16_t i = 0; i < kMaxConnections; i++)
    {
        if (mBleConnections[i].allocated)
        {
            numCons++;
        }
    }

    return numCons;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingEnabled(bool val)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(mServiceMode != ConnectivityManager::kCHIPoBLEServiceMode_NotSupported, err = CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    if (mFlags.Has(Flags::kAdvertisingEnabled) != val)
    {
        mFlags.Set(Flags::kAdvertisingEnabled, val);
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }

exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::_SetAdvertisingMode(BLEAdvertisingMode mode)
{
    switch (mode)
    {
    case BLEAdvertisingMode::kFastAdvertising:
        mFlags.Set(Flags::kFastAdvertisingEnabled, true);
        break;
    case BLEAdvertisingMode::kSlowAdvertising:
        mFlags.Set(Flags::kFastAdvertisingEnabled, false);
        break;
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    mFlags.Set(Flags::kRestartAdvertising);
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_GetDeviceName(char * buf, size_t bufSize)
{
    if (strlen(mDeviceName) >= bufSize)
    {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    strcpy(buf, mDeviceName);
    return CHIP_NO_ERROR;
}

CHIP_ERROR BLEManagerImpl::_SetDeviceName(const char * deviceName)
{
    if (mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_NotSupported)
    {
        return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    }
    if (deviceName != NULL && deviceName[0] != 0)
    {
        if (strlen(deviceName) >= kMaxDeviceNameLength)
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
        strcpy(mDeviceName, deviceName);
        mFlags.Set(Flags::kDeviceNameSet);
        mFlags.Set(Flags::kRestartAdvertising);
    }
    else
    {
        mDeviceName[0] = 0;
    }
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
    return CHIP_NO_ERROR;
}

void BLEManagerImpl::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    switch (event->Type)
    {
    case DeviceEventType::kCHIPoBLESubscribe: {
        ChipDeviceEvent connEstEvent;
        HandleSubscribeReceived(event->CHIPoBLESubscribe.ConId, &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
        connEstEvent.Type = DeviceEventType::kCHIPoBLEConnectionEstablished;
        PlatformMgr().PostEventOrDie(&connEstEvent);
    }
    break;

    case DeviceEventType::kCHIPoBLEUnsubscribe: {
        HandleUnsubscribeReceived(event->CHIPoBLEUnsubscribe.ConId, &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
    }
    break;

    case DeviceEventType::kCHIPoBLEWriteReceived: {
        HandleWriteReceived(event->CHIPoBLEWriteReceived.ConId, &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_RX,
                            PacketBufferHandle::Adopt(event->CHIPoBLEWriteReceived.Data));
    }
    break;

    case DeviceEventType::kCHIPoBLEConnectionError: {
        HandleConnectionError(event->CHIPoBLEConnectionError.ConId, event->CHIPoBLEConnectionError.Reason);
    }
    break;

    case DeviceEventType::kCHIPoBLEIndicateConfirm: {
        HandleIndicationConfirmation(event->CHIPoBLEIndicateConfirm.ConId, &CHIP_BLE_SVC_ID, &ChipUUID_CHIPoBLEChar_TX);
    }
    break;

    default:
        break;
    }
}

CHIP_ERROR BLEManagerImpl::SubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId)
{
    ChipLogProgress(DeviceLayer, "BLEManagerImpl::SubscribeCharacteristic() not supported");
    return CHIP_ERROR_NO_CONNECTION_HANDLER;
}

CHIP_ERROR BLEManagerImpl::UnsubscribeCharacteristic(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId)
{
    ChipLogProgress(DeviceLayer, "BLEManagerImpl::UnsubscribeCharacteristic() not supported");
    return CHIP_ERROR_NO_CONNECTION_HANDLER;
}

CHIP_ERROR BLEManagerImpl::CloseConnection(BLE_CONNECTION_OBJECT conId)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    rn_status_t ret;

    ChipLogProgress(DeviceLayer, "Closing BLE GATT connection (con %u)", conId);

    ble_disconnect();

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "rn_bt_connection_close() failed: %s", ErrorStr(err));
    }

    return err;
}

uint16_t BLEManagerImpl::GetMTU(BLE_CONNECTION_OBJECT conId) const
{
    CHIPoBLEConState * conState = const_cast<BLEManagerImpl *>(this)->GetConnectionState(conId);
    return (conState != NULL) ? conState->mtu : 0;
}

CHIP_ERROR BLEManagerImpl::SendIndication(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                                    PacketBufferHandle data)
{
    CHIP_ERROR err              = CHIP_NO_ERROR;
    CHIPoBLEConState * conState = GetConnectionState(conId);
    rn_status_t ret;
    uint8_t timerHandle = GetTimerHandle(conId, true);

    VerifyOrExit(((conState != NULL) && (conState->subscribed != 0)), err = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(timerHandle != kMaxConnections, err = CHIP_ERROR_NO_MEMORY);
    ret = ble_gattc_indicate_send(data->Start(), data->DataLength());
    err = MapBLEError(ret);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "BLEManagerImpl::SendIndication() failed: %s", ErrorStr(err));
    }

    return err;
}

CHIP_ERROR BLEManagerImpl::SendWriteRequest(BLE_CONNECTION_OBJECT conId, const ChipBleUUID * svcId, const ChipBleUUID * charId,
                                      PacketBufferHandle pBuf)
{
    ChipLogProgress(DeviceLayer, "BLEManagerImpl::SendWriteRequest() not supported");
    return CHIP_ERROR_NO_CONNECTION_HANDLER;
}

void BLEManagerImpl::NotifyChipConnectionClosed(BLE_CONNECTION_OBJECT conId)
{
    // Nothing to do
}

CHIP_ERROR BLEManagerImpl::MapBLEError(int bleErr)
{
    switch (bleErr)
    {
    case RN_STATUS_OK:
        return CHIP_NO_ERROR;
    case RN_STATUS_BT_ATT_INVALID_ATT_LENGTH:
        return CHIP_ERROR_INVALID_STRING_LENGTH;
    case RN_STATUS_INVALID_PARAMETER:
        return CHIP_ERROR_INVALID_ARGUMENT;
    case RN_STATUS_INVALID_STATE:
        return CHIP_ERROR_INCORRECT_STATE;
    case RN_STATUS_NOT_SUPPORTED:
        return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    default:
        return CHIP_ERROR(ChipError::Range::kPlatform, bleErr + CHIP_DEVICE_CONFIG_RENES_BLE_ERROR_MIN);
    }
}

void BLEManagerImpl::DriveBLEState(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    VerifyOrExit(mFlags.Has(Flags::kRnBLEStackInitialized), /* */);

    if (mServiceMode == ConnectivityManager::kCHIPoBLEServiceMode_Enabled && mFlags.Has(Flags::kAdvertisingEnabled) &&
        NumConnections() < kMaxConnections)
    {
        // Start/re-start advertising if not already started, or if there is a pending change
        // to the advertising configuration.
        if (!mFlags.Has(Flags::kAdvertising) || mFlags.Has(Flags::kRestartAdvertising))
        {
            err = StartAdvertising();
            SuccessOrExit(err);
        }
    }
    else if (mFlags.Has(Flags::kAdvertising))
    {
        err = StopAdvertising();
        SuccessOrExit(err);
    }

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Disabling CHIPoBLE service due to error: %s", ErrorStr(err));
        mServiceMode = ConnectivityManager::kCHIPoBLEServiceMode_Disabled;
    }
}

CHIP_ERROR BLEManagerImpl::ConfigureAdvertisingData(void)
{
    rn_status_t ret;
    ChipBLEDeviceIdentificationInfo mDeviceIdInfo;
    CHIP_ERROR err;
    uint8_t responseData[MAX_RESPONSE_DATA_LEN];
    uint8_t advData[MAX_ADV_DATA_LEN];
    uint32_t index              = 0;
    uint32_t mDeviceNameLength  = 0;
    uint8_t mDeviceIdInfoLength = 0;

    VerifyOrExit((kMaxDeviceNameLength + 1) < UINT8_MAX, err = CHIP_ERROR_INVALID_ARGUMENT);

    memset(responseData, 0, MAX_RESPONSE_DATA_LEN);
    memset(advData, 0, MAX_ADV_DATA_LEN);

    err = ConfigurationMgr().GetBLEDeviceIdentificationInfo(mDeviceIdInfo);
    SuccessOrExit(err);

    if (!mFlags.Has(Flags::kDeviceNameSet))
    {
        uint16_t discriminator;
        SuccessOrExit(err = GetCommissionableDataProvider()->GetSetupDiscriminator(discriminator));

        snprintf(mDeviceName, sizeof(mDeviceName), "%s%04u", CHIP_DEVICE_CONFIG_BLE_DEVICE_NAME_PREFIX, discriminator);

        mDeviceName[kMaxDeviceNameLength] = 0;
        mDeviceNameLength                 = strlen(mDeviceName);

        VerifyOrExit(mDeviceNameLength < kMaxDeviceNameLength, err = CHIP_ERROR_INVALID_ARGUMENT);
    }

    mDeviceNameLength = strlen(mDeviceName); // Device Name length + length field
    VerifyOrExit(mDeviceNameLength < kMaxDeviceNameLength, err = CHIP_ERROR_INVALID_ARGUMENT);

    mDeviceIdInfoLength = sizeof(mDeviceIdInfo); // Servicedatalen + length+ UUID (Short)

    index            = 0;
    advData[index++] = static_cast<uint8_t>(mDeviceIdInfoLength + CHIP_ADV_SHORT_UUID_LEN + 1);
    advData[index++] = CHIP_ADV_DATA_TYPE_SERVICE_DATA;
    advData[index++] = ShortUUID_CHIPoBLEService[0];
    advData[index++] = ShortUUID_CHIPoBLEService[1];
    memcpy(&advData[index], (void *) &mDeviceIdInfo, mDeviceIdInfoLength);
    index += mDeviceIdInfoLength;

#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
    ReturnErrorOnFailure(EncodeAdditionalDataTlv());
#endif

    connection_handle = 1;
    ret = ble_set_advertise_data((char *)advData, (char)index);

    if (ret != RN_STATUS_OK)
    {
        err = MapBLEError(ret);
        ChipLogError(DeviceLayer, "ble_set_advertise_data() - Advertising Data failed: %s", ErrorStr(err));
        ExitNow();
    }

    index = 0;
    responseData[index++] = CHIP_ADV_SHORT_UUID_LEN + 1;
    responseData[index++] = CHIP_ADV_DATA_TYPE_UUID;
    responseData[index++] = ShortUUID_CHIPoBLEService[0];
    responseData[index++] = ShortUUID_CHIPoBLEService[1];
    responseData[index++] = static_cast<uint8_t>(mDeviceNameLength + 1);
    responseData[index++] = CHIP_ADV_DATA_TYPE_NAME;
    memcpy(&responseData[index], mDeviceName, mDeviceNameLength);
    index += mDeviceNameLength;

    ret = ble_set_scan_response_data((char *)responseData, (char)index);

    if (ret != RN_STATUS_OK)
    {
        err = MapBLEError(ret);
        ChipLogError(DeviceLayer, "ble_set_scan_response_data() - Scan Response failed: %s", ErrorStr(err));
        ExitNow();
    }

    err = MapBLEError(ret);

exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::StartAdvertising(void)
{
    CHIP_ERROR err;

    uint32_t interval_min;
    uint32_t interval_max;

    ChipLogDetail(DeviceLayer, "BLE advertise Starting");

    if (mFlags.Has(Flags::kAdvertising))
    {
        ble_advertising_stop();
    }

    err = ConfigureAdvertisingData();
    SuccessOrExit(err);

    mFlags.Clear(Flags::kRestartAdvertising);

    if (mFlags.Has(Flags::kFastAdvertisingEnabled))
    {
        interval_min = CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MIN;
        interval_max = CHIP_DEVICE_CONFIG_BLE_FAST_ADVERTISING_INTERVAL_MAX;
    }
    else
    {
        interval_min = CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MIN;
        interval_max = CHIP_DEVICE_CONFIG_BLE_SLOW_ADVERTISING_INTERVAL_MAX;
    }

    ble_set_advertise_interval(interval_min, interval_max);
    ble_advertising_start();

    if (mFlags.Has(Flags::kFastAdvertisingEnabled))
    {
        StartBleAdvTimeoutTimer(CHIP_DEVICE_CONFIG_BLE_ADVERTISING_INTERVAL_CHANGE_TIME);
    }
    mFlags.Set(Flags::kAdvertising);

exit:
    return err;
}

CHIP_ERROR BLEManagerImpl::StopAdvertising(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    rn_status_t ret;
    ChipLogDetail(DeviceLayer, "BLE StopAdvertising");

    if (mFlags.Has(Flags::kAdvertising))
    {
        mFlags.Clear(Flags::kAdvertising).Clear(Flags::kRestartAdvertising);
        mFlags.Set(Flags::kFastAdvertisingEnabled, true);

        ble_advertising_stop();
        connection_handle = 0xff;
        CancelBleAdvTimeoutTimer();
    }

exit:
    return err;
}

void BLEManagerImpl::UpdateMtu(struct ble_msg_context * evt)
{
    CHIPoBLEConState * bleConnState = GetConnectionState(evt->conn_handle);
    if (bleConnState != NULL)
    {
        // bleConnState->MTU is a 10-bit field inside a uint16_t.  We're
        // assigning to it from a uint16_t, and compilers warn about
        // possibly not fitting.  There's no way to suppress that warning
        // via explicit cast; we have to disable the warning around the
        // assignment.
        //
        // TODO: https://github.com/project-chip/connectedhomeip/issues/2569
        // tracks making this safe with a check or explaining why no check
        // is needed.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        bleConnState->mtu = evt->gatt.mtu;
#pragma GCC diagnostic pop
        ;
    }
}

void BLEManagerImpl::HandleBootEvent(void)
{
    mFlags.Set(Flags::kRnBLEStackInitialized);
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
}

void BLEManagerImpl::HandleConnectEvent(struct ble_msg_context * evt)
{
    uint8_t connHandle                       = evt->conn_handle;

    AddConnection(connHandle);
    PlatformMgr().ScheduleWork(DriveBLEState, 0);
}

void BLEManagerImpl::HandleConnectionCloseEvent(struct ble_msg_context * evt)
{
    uint8_t connHandle                       = evt->conn_handle;

    ChipLogProgress(DeviceLayer, "Disconnect Event for handle : %d", connHandle);

    if ((evt->disconn_reason != 0) && (RemoveConnection(connHandle)))
    {
        ChipDeviceEvent event;
        event.Type                          = DeviceEventType::kCHIPoBLEConnectionError;
        event.CHIPoBLEConnectionError.ConId = connHandle;

        switch (evt->disconn_reason | 0x1000)
        {
        case RN_STATUS_BT_CTRL_REMOTE_USER_TERMINATED:
        case RN_STATUS_BT_CTRL_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_LOW_RESOURCES:
        case RN_STATUS_BT_CTRL_REMOTE_POWERING_OFF:
            event.CHIPoBLEConnectionError.Reason = BLE_ERROR_REMOTE_DEVICE_DISCONNECTED;
            break;

        case RN_STATUS_BT_CTRL_CONNECTION_TERMINATED_BY_LOCAL_HOST:
            event.CHIPoBLEConnectionError.Reason = BLE_ERROR_APP_CLOSED_CONNECTION;
            break;

        default:
            event.CHIPoBLEConnectionError.Reason = BLE_ERROR_CHIPOBLE_PROTOCOL_ABORT;
            break;
        }

        ChipLogProgress(DeviceLayer, "BLE GATT connection closed (con %u, reason %u)", connHandle, evt->disconn_reason);

        PlatformMgr().PostEventOrDie(&event);

        // Arrange to re-enable connectable advertising in case it was disabled due to the
        // maximum connection limit being reached.
        mFlags.Set(Flags::kRestartAdvertising);
        mFlags.Set(Flags::kFastAdvertisingEnabled);
        PlatformMgr().ScheduleWork(DriveBLEState, 0);
    }
}

void BLEManagerImpl::HandleWriteEvent(struct ble_msg_context * evt)
{
    HandleRXCharWrite(evt);
}

void BLEManagerImpl::HandleTXCharCCCDWrite(struct ble_msg_context * evt)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    CHIPoBLEConState * bleConnState;
    //bool isIndicationEnabled = false; // for matter_600
    bool isIndicationEnabled = true; // for matter_600
    ChipDeviceEvent event;
    bleConnState = GetConnectionState(evt->conn_handle);
    VerifyOrExit(bleConnState != NULL, err = CHIP_ERROR_NO_MEMORY);

    // Determine if the client is enabling or disabling notification/indication.
    isIndicationEnabled = (evt->gatt.data[0] != 0);

    if (isIndicationEnabled)
    {
        // If indications are not already enabled for the connection...
        if (!bleConnState->subscribed)
        {
            bleConnState->subscribed = 1;
            // Post an event to the CHIP queue to process either a CHIPoBLE Subscribe or Unsubscribe based on
            // whether the client is enabling or disabling indications.
            {
                event.Type                    = DeviceEventType::kCHIPoBLESubscribe;
                event.CHIPoBLESubscribe.ConId = evt->conn_handle;
                err                           = PlatformMgr().PostEvent(&event);
            }
        }
    }
    else
    {
        bleConnState->subscribed      = 0;
        event.Type                    = DeviceEventType::kCHIPoBLEUnsubscribe;
        event.CHIPoBLESubscribe.ConId = evt->conn_handle;
        err                           = PlatformMgr().PostEvent(&event);
    }

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "HandleTXCharCCCDWrite() failed: %s", ErrorStr(err));
    }
}

void BLEManagerImpl::HandleRXCharWrite(struct ble_msg_context * evt)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    System::PacketBufferHandle buf;
    uint16_t writeLen = evt->gatt.len;
    uint8_t * data    = evt->gatt.data;

    buf = System::PacketBufferHandle::NewWithData(data, writeLen, 0, 0);
    VerifyOrExit(!buf.IsNull(), err = CHIP_ERROR_NO_MEMORY);

    ChipLogDetail(DeviceLayer, "Write request/command received for CHIPoBLE RX characteristic (con %u, len %u)",
                  evt->conn_handle, buf->DataLength());

    // Post an event to the CHIP queue to deliver the data into the CHIP stack.
    {
        ChipDeviceEvent event;
        event.Type                        = DeviceEventType::kCHIPoBLEWriteReceived;
        event.CHIPoBLEWriteReceived.ConId = evt->conn_handle;
        event.CHIPoBLEWriteReceived.Data  = std::move(buf).UnsafeRelease();
        err                               = PlatformMgr().PostEvent(&event);
    }

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "HandleRXCharWrite() failed: %s", ErrorStr(err));
    }

}

void BLEManagerImpl::HandleTxConfirmationEvent(BLE_CONNECTION_OBJECT conId)
{
    ChipDeviceEvent event;
    uint8_t timerHandle = sInstance.GetTimerHandle(conId, false);

    // stop indication confirmation timer
    if (timerHandle < kMaxConnections)
    {
        ChipLogProgress(DeviceLayer, " stop soft timer");
    }

    event.Type                          = DeviceEventType::kCHIPoBLEIndicateConfirm;
    event.CHIPoBLEIndicateConfirm.ConId = conId;
    PlatformMgr().PostEventOrDie(&event);
}

bool BLEManagerImpl::RemoveConnection(uint8_t connectionHandle)
{
    CHIPoBLEConState * bleConnState = GetConnectionState(connectionHandle, true);
    bool status                     = false;

    if (bleConnState != NULL)
    {
        memset(bleConnState, 0, sizeof(CHIPoBLEConState));
        status = true;
    }

    return status;
}

void BLEManagerImpl::AddConnection(uint8_t connectionHandle)
{
    CHIPoBLEConState * bleConnState = GetConnectionState(connectionHandle, true);

    if (bleConnState != NULL)
    {
        memset(bleConnState, 0, sizeof(CHIPoBLEConState));
        bleConnState->allocated        = 1;
        bleConnState->connectionHandle = connectionHandle;
    }
}

BLEManagerImpl::CHIPoBLEConState * BLEManagerImpl::GetConnectionState(uint8_t connectionHandle, bool allocate)
{
    uint8_t freeIndex = kMaxConnections;

    for (uint8_t i = 0; i < kMaxConnections; i++)
    {
        if (mBleConnections[i].allocated == 1)
        {
            if (mBleConnections[i].connectionHandle == connectionHandle)
            {
                return &mBleConnections[i];
            }
        }

        else if (i < freeIndex)
        {
            freeIndex = i;
        }
    }

    if (allocate)
    {
        if (freeIndex < kMaxConnections)
        {
            return &mBleConnections[freeIndex];
        }

        ChipLogError(DeviceLayer, "Failed to allocate CHIPoBLEConState");
    }

    return NULL;
}

#if CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING
CHIP_ERROR BLEManagerImpl::EncodeAdditionalDataTlv()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    BitFlags<AdditionalDataFields> additionalDataFields;
    AdditionalDataPayloadGeneratorParams additionalDataPayloadParams;

#if CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID)
    uint8_t rotatingDeviceIdUniqueId[ConfigurationManager::kRotatingDeviceIDUniqueIDLength] = {};
    MutableByteSpan rotatingDeviceIdUniqueIdSpan(rotatingDeviceIdUniqueId);

    err = DeviceLayer::GetDeviceInstanceInfoProvider()->GetRotatingDeviceIdUniqueId(rotatingDeviceIdUniqueIdSpan);
    SuccessOrExit(err);
    err = ConfigurationMgr().GetLifetimeCounter(additionalDataPayloadParams.rotatingDeviceIdLifetimeCounter);
    SuccessOrExit(err);
    additionalDataPayloadParams.rotatingDeviceIdUniqueId = rotatingDeviceIdUniqueIdSpan;
    additionalDataFields.Set(AdditionalDataFields::RotatingDeviceId);
#endif /* CHIP_ENABLE_ROTATING_DEVICE_ID && defined(CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID) */

    err = AdditionalDataPayloadGenerator().generateAdditionalDataPayload(additionalDataPayloadParams, c3AdditionalDataBufferHandle,
                                                                         additionalDataFields);

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to generate TLV encoded Additional Data (%s)", __func__);
    }

    return err;
}

void BLEManagerImpl::HandleC3ReadRequest(struct ble_msg_context * evt)
{
}
#endif // CHIP_ENABLE_ADDITIONAL_DATA_ADVERTISING

uint8_t BLEManagerImpl::GetTimerHandle(uint8_t connectionHandle, bool allocate)
{
    uint8_t freeIndex = kMaxConnections;

    for (uint8_t i = 0; i < kMaxConnections; i++)
    {
        if (mIndConfId[i] == connectionHandle)
        {
            return i;
        }
        else if (allocate)
        {
            if (i < freeIndex)
            {
                freeIndex = i;
            }
        }
    }

    if (freeIndex < kMaxConnections)
    {
        mIndConfId[freeIndex] = connectionHandle;
    }
    else
    {
        ChipLogError(DeviceLayer, "Failed to Save Conn Handle for indication");
    }

    return freeIndex;
}

void BLEManagerImpl::BleAdvTimeoutHandler(TimerHandle_t xTimer)
{
    if (BLEMgrImpl().mFlags.Has(Flags::kFastAdvertisingEnabled))
    {
        ChipLogDetail(DeviceLayer, "bleAdv Timeout : Start slow advertisement");
        BLEMgr().SetAdvertisingMode(BLEAdvertisingMode::kSlowAdvertising);
    }
}

void BLEManagerImpl::CancelBleAdvTimeoutTimer(void)
{
    if (xTimerStop(sbleAdvTimeoutTimer, 0) == pdFAIL)
    {
        ChipLogError(DeviceLayer, "Failed to stop BledAdv timeout timer");
    }
}

void BLEManagerImpl::StartBleAdvTimeoutTimer(uint32_t aTimeoutInMs)
{
    if (xTimerIsTimerActive(sbleAdvTimeoutTimer))
    {
        CancelBleAdvTimeoutTimer();
    }

    // timer is not active, change its period to required value (== restart).
    // FreeRTOS- Block for a maximum of 100 ticks if the change period command
    // cannot immediately be sent to the timer command queue.
    if (xTimerChangePeriod(sbleAdvTimeoutTimer, aTimeoutInMs / portTICK_PERIOD_MS, 100) != pdPASS)
    {
        ChipLogError(DeviceLayer, "Failed to start BledAdv timeout timer");
    }
}

void BLEManagerImpl::DriveBLEState(intptr_t arg)
{
    sInstance.DriveBLEState();
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

extern "C" void ble_comm_event(struct ble_msg_context * evt)
{
    // As this is running in a separate thread, we need to block CHIP from operating,
    // until the events are handled.
    chip::DeviceLayer::PlatformMgr().LockChipStack();

    // handle bluetooth events
    switch (evt->bType)
    {
    case BT_EVT_DEVICE_READY: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleBootEvent();
    }
    break;

    case BT_EVT_DEVICE_CONNECT: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleConnectEvent(evt);
    }
    break;

    case BT_EVT_DEVICE_WRITE_RX: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleWriteEvent(evt);
    }
    break;

    case BT_EVT_DEVICE_WRITE_CCCD: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleTXCharCCCDWrite(evt);
    }
    break;
    
    case BT_EVT_DEVICE_WRITE_CONFIRM: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleTxConfirmationEvent(evt->conn_handle);
    }
    break;

    case BT_EVT_DEVICE_UPDATE_MTU: {
        chip::DeviceLayer::Internal::BLEMgrImpl().UpdateMtu(evt);
    }
	break;

    case BT_EVT_DEVICE_DISCONNECTED: {
        chip::DeviceLayer::Internal::BLEMgrImpl().HandleConnectionCloseEvent(evt);
    }
    break;

    default:
        break;
    }

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
