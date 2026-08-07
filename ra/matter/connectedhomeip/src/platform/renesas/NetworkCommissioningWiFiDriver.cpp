/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <lib/support/CodeUtils.h>
#include <lib/support/SafeInt.h>
#include <platform/CHIPDeviceLayer.h>
// matterwork[[::
#ifdef __cplusplus
extern "C" {
#endif
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#include dg_configADNVPARAM_PROJ_FILE
#endif
#ifdef __cplusplus
}
#endif
#include "rnDeviceWrapAPIs.h"
#include <platform/renesas/NetworkCommissioningWiFiDriver.h>
#include <platform/renesas/RnConfig.h>
//]]matterwork

#include <limits>

using namespace ::chip;
using namespace ::chip::DeviceLayer::Internal;

namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

namespace {
NetworkCommissioning::WiFiScanResponse * sScanResult;
RnScanResponseIterator<NetworkCommissioning::WiFiScanResponse> mScanResponseIter(sScanResult);
} // namespace

CHIP_ERROR
RnWiFiDriver::Init(NetworkStatusChangeCallback * networkStatusChangeCallback)
{
    CHIP_ERROR err;
    size_t ssidLen        = 0;
    size_t credentialsLen = 0;
    mpScanCallback        = nullptr;
    mpConnectCallback     = nullptr;

#ifdef CHIP_ONNETWORK_PAIRING
    char * result_ptr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, &result_ptr);
#endif
    if (result_ptr)
    {
        ssidLen = strlen(result_ptr);
        if (ssidLen >= sizeof(mSavedNetwork.ssid))
        {
            ssidLen = sizeof(mSavedNetwork.ssid) - 1;
        }
        memset(mSavedNetwork.ssid, 0, ssidLen);
        memcpy(mSavedNetwork.ssid, result_ptr, ssidLen);
        mSavedNetwork.ssid[ssidLen] = '\0';

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_SSID_0, mSavedNetwork.ssid);
#endif
         err = CHIP_NO_ERROR;
    }
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_NO_ERROR);

    result_ptr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENCKEY_0, &result_ptr);
#endif
    if (result_ptr)
    {
        credentialsLen = strlen(result_ptr);
        if (credentialsLen >= sizeof(mSavedNetwork.credentials))
        {
            credentialsLen = sizeof(mSavedNetwork.credentials) - 1;
        }
        memset(mSavedNetwork.credentials, 0, credentialsLen);
        memcpy(mSavedNetwork.credentials, result_ptr, credentialsLen);
        mSavedNetwork.credentials[credentialsLen] = '\0';

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_ENCKEY_0, mSavedNetwork.credentials);
#endif
         err = CHIP_NO_ERROR;
    }

    mSavedNetwork.credentialsLen = credentialsLen;
    mSavedNetwork.ssidLen        = ssidLen;
    //]]matterwork
    mStagingNetwork = mSavedNetwork;
    err             = CHIP_NO_ERROR;
    result_ptr = NULL;
#else
    // If reading fails, wifi is not provisioned, no need to go further.
    char * result_ptr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    fsp_err_t status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                                       WIFI_PROFILE_SSID_0, &result_ptr);
#endif
    if (result_ptr)
    {
        ssidLen = strlen(result_ptr);
        if (ssidLen >= sizeof(mSavedNetwork.ssid))
        {
            ssidLen = sizeof(mSavedNetwork.ssid) - 1;
        }
        memset(mSavedNetwork.ssid, 0, ssidLen);
        memcpy(mSavedNetwork.ssid, result_ptr, ssidLen);
        mSavedNetwork.ssid[ssidLen] = '\0';
    }
    VerifyOrReturnError(fsp_to_chip_error(status) == CHIP_NO_ERROR, CHIP_NO_ERROR);
    result_ptr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_ENCKEY_0, &result_ptr);
#endif
    if (result_ptr)
    {
        credentialsLen = strlen(result_ptr);
        if (credentialsLen >= sizeof(mSavedNetwork.credentials))
        {
            credentialsLen = sizeof(mSavedNetwork.credentials) - 1;
        }
        memset(mSavedNetwork.credentials, 0, credentialsLen);
        memcpy(mSavedNetwork.credentials, result_ptr, credentialsLen);
        mSavedNetwork.credentials[credentialsLen] = '\0';
    }
    VerifyOrReturnError(fsp_to_chip_error(status) == CHIP_NO_ERROR, CHIP_NO_ERROR);

    mSavedNetwork.credentialsLen = credentialsLen;
    mSavedNetwork.ssidLen        = ssidLen;
    mStagingNetwork              = mSavedNetwork;
#endif
    result_ptr = NULL;
    ConnectWiFiNetwork(mSavedNetwork.ssid, ssidLen, mSavedNetwork.credentials, credentialsLen);
    return err;
}

CHIP_ERROR RnWiFiDriver::CommitConfiguration()
{
    uint8_t securityType = eWiFiSecurityWPA2;

#ifdef RM_MAP_PERSISTANT_W
    uint16_t data_length;
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_SSID_0, mStagingNetwork.ssid);
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENCKEY_0, mStagingNetwork.credentials);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_SECURITY_0, securityType);
#endif

    mSavedNetwork = mStagingNetwork;
    return CHIP_NO_ERROR;
}

CHIP_ERROR RnWiFiDriver::RevertConfiguration()
{
    mStagingNetwork = mSavedNetwork;
    return CHIP_NO_ERROR;
}

bool RnWiFiDriver::NetworkMatch(const WiFiNetwork & network, ByteSpan networkId)
{
    return networkId.size() == network.ssidLen && memcmp(networkId.data(), network.ssid, network.ssidLen) == 0;
}

Status RnWiFiDriver::AddOrUpdateNetwork(ByteSpan ssid, ByteSpan credentials, MutableCharSpan & outDebugText,
                                        uint8_t & outNetworkIndex)
{
    outDebugText.reduce_size(0);
    outNetworkIndex = 0;
    VerifyOrReturnError(mStagingNetwork.ssidLen == 0 || NetworkMatch(mStagingNetwork, ssid), Status::kBoundsExceeded);
    VerifyOrReturnError(credentials.size() <= sizeof(mStagingNetwork.credentials), Status::kOutOfRange);
    VerifyOrReturnError(ssid.size() <= sizeof(mStagingNetwork.ssid), Status::kOutOfRange);

    memset(mStagingNetwork.credentials, 0, sizeof(mStagingNetwork.credentials));
    memcpy(mStagingNetwork.credentials, credentials.data(), credentials.size());
    mStagingNetwork.credentialsLen = static_cast<decltype(mStagingNetwork.credentialsLen)>(credentials.size());

    memset(mStagingNetwork.ssid, 0, sizeof(mStagingNetwork.ssid));
    memcpy(mStagingNetwork.ssid, ssid.data(), ssid.size());
    mStagingNetwork.ssidLen = static_cast<decltype(mStagingNetwork.ssidLen)>(ssid.size());

    return Status::kSuccess;
}

Status RnWiFiDriver::RemoveNetwork(ByteSpan networkId, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex)
{
    outDebugText.reduce_size(0);
    outNetworkIndex = 0;
    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);

    // Use empty ssid for representing invalid network
    mStagingNetwork.ssidLen = 0;
    return Status::kSuccess;
}

Status RnWiFiDriver::ReorderNetwork(ByteSpan networkId, uint8_t index, MutableCharSpan & outDebugText)
{
    outDebugText.reduce_size(0);
    // Only one network is supported for now
    VerifyOrReturnError(index == 0, Status::kOutOfRange);
    VerifyOrReturnError(NetworkMatch(mStagingNetwork, networkId), Status::kNetworkIDNotFound);
    return Status::kSuccess;
}

CHIP_ERROR RnWiFiDriver::ConnectWiFiNetwork(const char * ssid, uint8_t ssidLen, const char * key, uint8_t keyLen)
{
    if (ConnectivityMgr().IsWiFiStationProvisioned())
    {
        ChipLogProgress(DeviceLayer, "Disconecting for current wifi");
        int32_t status = wifi_sta_discon();
        if (status != 0)
        {
            return CHIP_ERROR_INTERNAL;
        }
    }
    ReturnErrorOnFailure(ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Disabled));
    // Set the wifi configuration
    wifi_provision_t wifiConfig = {};
    memcpy(wifiConfig.ssid, ssid, ssidLen);
    memcpy(wifiConfig.passkey, key, keyLen);
    if (keyLen == 0)
        wifiConfig.security = WIFI_SEC_NONE;
    else
        wifiConfig.security = WIFI_SEC_WPA2;

    ChipLogProgress(NetworkProvisioning, "Setting up connection for WiFi SSID: %.*s", static_cast<int>(ssidLen), ssid);
    // Configure the WiFi interface.
    wifi_set_wifi_provision(&wifiConfig);
    ReturnErrorOnFailure(ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Disabled));
    ReturnErrorOnFailure(ConnectivityMgr().SetWiFiStationMode(ConnectivityManager::kWiFiStationMode_Enabled));
    return CHIP_NO_ERROR;
}

void RnWiFiDriver::OnConnectWiFiNetwork()
{
    if (mpConnectCallback)
    {
        CommitConfiguration();
        mpConnectCallback->OnResult(Status::kSuccess, CharSpan(), 0);
        mpConnectCallback = nullptr;
    }
}

void RnWiFiDriver::ConnectNetwork(ByteSpan networkId, ConnectCallback * callback)
{
    CHIP_ERROR err          = CHIP_NO_ERROR;
    Status networkingStatus = Status::kUnknownError;

    VerifyOrExit(NetworkMatch(mStagingNetwork, networkId), networkingStatus = Status::kNetworkIDNotFound);
    VerifyOrExit(mpConnectCallback == nullptr, networkingStatus = Status::kUnknownError);

    err = ConnectWiFiNetwork(mStagingNetwork.ssid, mStagingNetwork.ssidLen, mStagingNetwork.credentials,
                             mStagingNetwork.credentialsLen);
    if (err == CHIP_NO_ERROR)
    {
        mpConnectCallback = callback;
        networkingStatus  = Status::kSuccess;
    }

exit:
    if (networkingStatus != Status::kSuccess)
    {
        ChipLogError(NetworkProvisioning, "Failed to connect to WiFi network:%s", chip::ErrorStr(err));
        mpConnectCallback = nullptr;
        callback->OnResult(networkingStatus, CharSpan(), 0);
    }
}

chip::BitFlags<WiFiSecurity> RnWiFiDriver::ConvertSecuritytype(uint8_t security)
{
    chip::BitFlags<WiFiSecurity> securityType;
    if (security == WIFI_SEC_NONE)
    {
        securityType = WiFiSecurity::kUnencrypted;
    }
    else if (security & WIFI_SEC_WEP)
    {
        securityType = WiFiSecurity::kWep;
    }
    else if (security & WIFI_SEC_WPA)
    {
        securityType = WiFiSecurity::kWpaPersonal;
    }
    else if (security & WIFI_SEC_WPA2)
    {
        securityType = WiFiSecurity::kWpa2Personal;
    }
    else if (security == WIFI_SEC_WPA3)
    {
        securityType = WiFiSecurity::kWpa3Personal;
    }
    else
    {
        // wifi_sec_t support more type
        securityType = WiFiSecurity::kUnencrypted;
    }

    return securityType;
}

bool RnWiFiDriver::StartScanWiFiNetworks(ByteSpan ssid)
{
    bool scanStarted = false;
    ChipLogProgress(DeviceLayer, "Start Scan WiFi Networks");
    // orig. SDK[[::

    if (!ssid.empty()) // ssid is given, only scan this network
    {
        char cSsid[DeviceLayer::Internal::kMaxWiFiSSIDLength] = {};
        memcpy(cSsid, ssid.data(), ssid.size());
        scanStarted = wifi_start_scan(cSsid, OnScanWiFiNetworkDone);
    }
    else // scan all networks
    {
        scanStarted = wifi_start_scan(nullptr, OnScanWiFiNetworkDone);
    }

    // matterwork[[::
    ChipLogProgress(DeviceLayer, "Start Scan WiFi Networks: bypassed");
    //]]matterwork
    return scanStarted;
}

void RnWiFiDriver::OnScanWiFiNetworkDone(wifi_scan_result_t * aScanResult)
{
    ChipLogProgress(DeviceLayer, "OnScanWiFiNetworkDone");
    if (!aScanResult)
    {
        if (GetInstance().mpScanCallback != nullptr)
        {
            DeviceLayer::SystemLayer().ScheduleLambda([]() {
                GetInstance().mpScanCallback->OnFinished(NetworkCommissioning::Status::kSuccess, CharSpan(), &mScanResponseIter);
                GetInstance().mpScanCallback = nullptr;
            });
        }
    }
    else
    {
        NetworkCommissioning::WiFiScanResponse scanResponse = {};

        scanResponse.security.Set(GetInstance().ConvertSecuritytype(aScanResult->security));
        scanResponse.channel = aScanResult->chan;
        scanResponse.rssi    = aScanResult->rssi;
        scanResponse.ssidLen = strnlen(aScanResult->ssid, DeviceLayer::Internal::kMaxWiFiSSIDLength);
        memcpy(scanResponse.ssid, aScanResult->ssid, scanResponse.ssidLen);
        memcpy(scanResponse.bssid, aScanResult->bssid, sizeof(scanResponse.bssid));

        mScanResponseIter.Add(&scanResponse);
    }
}

void RnWiFiDriver::ScanNetworks(ByteSpan ssid, WiFiDriver::ScanCallback * callback)
{
    if (callback != nullptr)
    {
        mpScanCallback = callback;
        if (!StartScanWiFiNetworks(ssid))
        {
            ChipLogError(DeviceLayer, "ScanWiFiNetworks failed to start");
            mpScanCallback = nullptr;
            callback->OnFinished(Status::kUnknownError, CharSpan(), nullptr);
        }
    }
}

CHIP_ERROR GetConnectedNetwork(Network & network)
{
    wifi_provision_t wifiConfig;

    if (!wifi_is_sta_connected() || !wifi_get_wifi_provision(&wifiConfig))
    {
        return CHIP_ERROR_INCORRECT_STATE;
    }

    uint8_t length = strnlen(wifiConfig.ssid, DeviceLayer::Internal::kMaxWiFiSSIDLength);
    if (length > sizeof(network.networkID))
    {
        ChipLogError(DeviceLayer, "SSID too long");
        return CHIP_ERROR_INTERNAL;
    }

    memcpy(network.networkID, wifiConfig.ssid, length);
    network.networkIDLen = length;

    return CHIP_NO_ERROR;
}

size_t RnWiFiDriver::WiFiNetworkIterator::Count()
{
    return mDriver->mStagingNetwork.ssidLen == 0 ? 0 : 1;
}

bool RnWiFiDriver::WiFiNetworkIterator::Next(Network & item)
{
    if (mExhausted || mDriver->mStagingNetwork.ssidLen == 0)
    {
        return false;
    }
    memcpy(item.networkID, mDriver->mStagingNetwork.ssid, mDriver->mStagingNetwork.ssidLen);
    item.networkIDLen = mDriver->mStagingNetwork.ssidLen;
    item.connected    = false;
    mExhausted        = true;

    Network connectedNetwork;
    CHIP_ERROR err = GetConnectedNetwork(connectedNetwork);
    if (err == CHIP_NO_ERROR)
    {
        if (connectedNetwork.networkIDLen == item.networkIDLen &&
            memcmp(connectedNetwork.networkID, item.networkID, item.networkIDLen) == 0)
        {
            item.connected = true;
        }
    }
    return true;
}

} // namespace NetworkCommissioning
} // namespace DeviceLayer
} // namespace chip
