/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *    Copyright (c) 2022 Renesas Electronics.
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
#include <AppTask.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/software-diagnostics-server/software-diagnostics-server.h>
#include <app/server/OnboardingCodesUtil.h>
#include <app/util/attribute-storage.h>
#include <app/util/attribute-table.h>
#include <lib/shell/Engine.h>
#include <lwip/netif.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <stdlib.h>

#include "MatterConfig.h"
#include "iface_defs.h"
#include "rnDeviceDataProvider.h"
#include "rnDeviceWrapAPIs.h"
#include "wifi_rn_events.h"

#include "RnDeviceAttestationCreds.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#include dg_configADNVPARAM_PROJ_FILE
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_wifi_api.h"
#include "rm_wifi.h"
#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#include "ble_app_main.h"
#endif
#ifdef __cplusplus
}
#endif

using chip::Shell::Engine;

using namespace ::chip;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Internal;
using namespace ::chip::Credentials;
using namespace chip::app;
using namespace chip::app::Clusters;

static EmberAfDeviceType DeviceTypeList[] = { { 0x0016, 1 }, { 0, 1 } };

#if defined(__RRQ61400__)
struct ble_msg_context evt_ble_matter;
struct ble_advertise ble_advertise_matter;
#endif
extern unsigned char matter_wifi_conn_status;

#if defined(__SUPPORT_USR_NVRAM__)
extern "C" int api_usr_nvram_read_int(const char * name, int32_t * _val);
extern "C" char * api_usr_nvram_read_string(const char * name);
extern "C" uint8_t * api_usr_nvram_read_binary(const char * name, uint16_t * size);

extern "C" int api_usr_nvram_write_int(const char * name, int32_t val);
extern "C" int api_usr_nvram_write_string(const char * name, const char * val);
extern "C" int api_usr_nvram_write_binary(const char * name, const char * val, uint16_t size);
extern "C" int32_t api_usr_nvram_delete_item(const char * name);
extern "C" void api_usr_nvram_bank_reset(uint16_t bank);
#endif

matterDeviceStatus latestStatus = _Status_IDLE;
char tSerialNum[32]             = {
    0,
};
static constexpr size_t kMaxThreadNameLength = 32;
#if defined(__cplusplus)
 #define FSP_CPP_HEADER    extern "C" {
 #define FSP_CPP_FOOTER    }
#else
 #define FSP_CPP_HEADER
 #define FSP_CPP_FOOTER
#endif
FSP_CPP_HEADER
int R_BSP_WarmStartt(int event);
FSP_CPP_FOOTER

CHIP_ERROR fsp_to_chip_error(fsp_err_t error)
{
    switch(error)
    {
        case FSP_ERR_INVALID_ARGUMENT:
            return CHIP_ERROR_INVALID_ARGUMENT;
        case FSP_ERR_NOT_FOUND:
            return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
        case FSP_ERR_WRITE_FAILED:
            return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
        case FSP_ERR_NOT_OPEN:
            return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
        case FSP_ERR_ERASE_FAILED:
            return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
        case FSP_SUCCESS:
            return CHIP_NO_ERROR;
        default:
            return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }
}

int R_BSP_WarmStartt(int event)
{
    return event + 1;
}

void appErrorInt(int err)
{
    char msg[64];
    RENES_LOG("!!!!!!!!!!!! App Critical Error: %d (=0x%x) !!!!!!!!!!!", err, err);
    snprintf(msg, sizeof msg, "App Error:%d", err);
    set_software_fault(msg);
    while (true)
        vTaskDelay(10);
}

void appError(CHIP_ERROR error)
{
    appErrorInt(static_cast<int>(error.AsInteger()));
}

void ble_init(void)
{
#ifdef __RRQ61400__
    app_start_ble_main();
#endif
}

void ble_deinit(void)
{
#ifdef __RRQ61400__
    (void) chip::DeviceLayer::ConnectivityMgr().SetBLEAdvertisingEnabled(false);
    app_stop_ble_main();
#endif
}

void ble_advertising_start(void)
{
#ifdef __RRQ61400__
    ble_adv_start();
#endif
}

void ble_advertising_stop(void)
{
#ifdef __RRQ61400__
    ble_adv_stop();
#endif
}

void ble_disconnect(void)
{
#ifdef __RRQ61400__
    ble_close_connection();
#endif
}

void ble_set_advertise_interval(unsigned short min, unsigned short max)
{
#if defined(__RRQ61400__)
    ble_advertise_matter.interval_min = min;
    ble_advertise_matter.interval_max = max;
#endif
    return;
}

int ble_set_advertise_data(char * adv_data, char data_len)
{
    if (data_len > BLE_ADV_DATA_MAX_SIZE)
        return RN_STATUS_FAIL;
#if defined(__RRQ61400__)
    ble_advertise_matter.advertise_data_len = data_len;
    memcpy(ble_advertise_matter.advertise_data, adv_data, data_len);
#endif
    return RN_STATUS_OK;
}

int ble_set_scan_response_data(char * res_data, char data_len)
{
    if (data_len > BLE_SCAN_RESP_DATA_MAX_SIZE)
        return RN_STATUS_FAIL;
#if defined(__RRQ61400__)
    ble_advertise_matter.scan_response_data_len = data_len;
    memcpy(ble_advertise_matter.scan_response_data, res_data, data_len);
#endif
    return RN_STATUS_OK;
}

int wifi_provision(char * ssid, char * passkey, unsigned int sec)
{
    CHIP_ERROR err = CHIP_ERROR_INVALID_ARGUMENT;
    int security = sec;
    int profileComplete = 1;

#ifdef RM_MAP_PERSISTANT_W
    fsp_err_t status = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, ssid);
    err = fsp_to_chip_error(status);
#endif

    if (err != CHIP_NO_ERROR)
        return err.AsInteger();

#ifdef RM_MAP_PERSISTANT_W
    status = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_SECURITY_0, security);
    err = fsp_to_chip_error(status);
#endif
    if (err != CHIP_NO_ERROR)
        return err.AsInteger();

    if (passkey != NULL)
    {
#ifdef RM_MAP_PERSISTANT_W
        status = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
		                                           WIFI_PROFILE_ENCKEY_0, passkey);
        err = fsp_to_chip_error(status);
#endif
    }
    if (err != CHIP_NO_ERROR)
        return err.AsInteger();

#ifdef RM_MAP_PERSISTANT_W
    status = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_COMPLETE, profileComplete);
    err = fsp_to_chip_error(status);
#endif
    printf("wifi_provision: cred: %s %d %s\n", ssid, sec, passkey);
    return err.AsInteger();
}

void wifi_callback(WIFIEvent_t * xEvent)
{
    if (xEvent->xEventType == eWiFiEventConnected)
        wifi_noti_connected();
    else if (xEvent->xEventType == eWiFiEventDisconnected)
        wifi_noti_disconnected();
    return;
}

int wifi_connect(void)
{
    WIFIReturnCode_t wifi_err;
    /* Setup Access Point connection parameters */
    WIFINetworkParams_t net_params = { 0 };
    CHIP_ERROR err;
    wifi_sec_t sec;
    int tmp = 0;
    int security = 0;
    char *result_ptr = NULL;

#ifdef RM_MAP_PERSISTANT_W
    fsp_err_t status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                       ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, &result_ptr);
    err = fsp_to_chip_error(status);
#endif

    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        return RN_STATUS_FAIL;
    }
    if (result_ptr)
    {
        net_params.ucSSIDLength = strlen(result_ptr);
        if (net_params.ucSSIDLength >= wificonfigMAX_SSID_LEN)
        {
            net_params.ucSSIDLength = wificonfigMAX_SSID_LEN - 1;
        }
        memset(net_params.ucSSID, 0, sizeof(net_params.ucSSID));
        memcpy(net_params.ucSSID, result_ptr, net_params.ucSSIDLength);
        net_params.ucSSID[net_params.ucSSIDLength] = '\0';
    }
    result_ptr = NULL;
#ifdef RM_MAP_PERSISTANT_W
    status = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_SECURITY_0, &security);
    err = fsp_to_chip_error(status);
#endif
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        return RN_STATUS_FAIL;
    }
    sec = static_cast<wifi_sec_t>(security);

    net_params.ucChannel = 0;
    if ((sec == WIFI_SEC_WPA) || (sec == WIFI_SEC_WPA2))
    { // WPA/WPA2
#ifdef RM_MAP_PERSISTANT_W
        status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0, &result_ptr);
        err = fsp_to_chip_error(status);
#endif
        if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
        {
            return RN_STATUS_FAIL;
        }
        if (result_ptr)
        {
            net_params.xPassword.xWPA.ucLength = strlen(result_ptr);
            if (net_params.xPassword.xWPA.ucLength >= wificonfigMAX_PASSPHRASE_LEN)
            {
                net_params.xPassword.xWPA.ucLength = wificonfigMAX_PASSPHRASE_LEN - 1;
            }
            memset(net_params.xPassword.xWPA.cPassphrase, 0, sizeof(net_params.xPassword.xWPA.cPassphrase));
            memcpy(net_params.xPassword.xWPA.cPassphrase, result_ptr, net_params.xPassword.xWPA.ucLength);
            net_params.xPassword.xWPA.cPassphrase[net_params.xPassword.xWPA.ucLength] = '\0';
        }
        result_ptr = NULL;

        if (sec == WIFI_SEC_WPA)
        {
            net_params.xSecurity = eWiFiSecurityWPA;
        }
        else
        {
            net_params.xSecurity = eWiFiSecurityWPA2;
        }
    }
    else if (sec == WIFI_SEC_NONE)
    { // OPEN
        net_params.xSecurity = eWiFiSecurityOpen;
        memset(net_params.xPassword.xWPA.cPassphrase, 0x00, wificonfigMAX_PASSPHRASE_LEN);
    }

    WIFI_RegisterEvent(eWiFiEventConnected,
                       wifi_callback); // when this function implemented, should be remove
                                       // regarding code of matter_wifi_conn_status in util_api.c
    WIFI_RegisterEvent(eWiFiEventDisconnected,
                       wifi_callback); // when this function implemented, should be remove
                                       // regarding code of matter_wifi_conn_status in util_api.c
    /* Connect to the Access Point */
    wifi_err = WIFI_ConnectAP(&net_params);
    if (wifi_err != eWiFiSuccess)
        return RN_STATUS_FAIL;
    while (1)
    {
        if (WIFI_IsConnected(&net_params) == eWiFiSuccess)
        {
            return RN_STATUS_OK;
        }
        tmp++;
        vTaskDelay(portCONVERT_MS_2_TICKS(1000));
        if (tmp >= 10)
        {
            return RN_STATUS_FAIL;
        }
    }

    vTaskDelay(5);
    return RN_STATUS_OK;
}

int wifi_disconnect(void)
{
    if (WIFI_Disconnect() == eWiFiSuccess)
        return RN_STATUS_OK;
    else
        return RN_STATUS_FAIL;
}

int is_wifi_connect(void)
{
    CHIP_ERROR err;
    /* Setup Access Point connection parameters */
    WIFINetworkParams_t net_params = { 0 };
    wifi_sec_t sec;
    int security = 0;
    char * result_ptr = NULL;

#ifdef RM_MAP_PERSISTANT_W
    fsp_err_t status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                       ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, &result_ptr);
    err = fsp_to_chip_error(status);
#endif
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        return RN_STATUS_FAIL;
    }
    if (result_ptr)
    {
        net_params.ucSSIDLength = strlen(result_ptr);
        if (net_params.ucSSIDLength >= wificonfigMAX_SSID_LEN)
        {
            net_params.ucSSIDLength = wificonfigMAX_SSID_LEN - 1;
        }
        memset(net_params.ucSSID, 0, sizeof(net_params.ucSSID));
        memcpy(net_params.ucSSID, result_ptr, net_params.ucSSIDLength);
        net_params.ucSSID[net_params.ucSSIDLength] = '\0';
    }
    result_ptr = NULL;

#ifdef RM_MAP_PERSISTANT_W
    status = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_SECURITY_0, &security);
    err = fsp_to_chip_error(status);
#endif
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        return RN_STATUS_FAIL;
    }
    sec = static_cast<wifi_sec_t>(security);

    net_params.ucChannel = 0;
    if ((sec == WIFI_SEC_WPA) || (sec == WIFI_SEC_WPA2))
    { // WPA/WPA2
#ifdef RM_MAP_PERSISTANT_W
        status = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                               WIFI_PROFILE_ENCKEY_0, &result_ptr);
        err = fsp_to_chip_error(status);
#endif
        if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
        {
            return RN_STATUS_FAIL;
        }
        if (result_ptr)
        {
            net_params.xPassword.xWPA.ucLength = strlen(result_ptr);
            if (net_params.xPassword.xWPA.ucLength >= wificonfigMAX_PASSPHRASE_LEN)
            {
                net_params.xPassword.xWPA.ucLength = wificonfigMAX_PASSPHRASE_LEN - 1;
            }
            memset(net_params.xPassword.xWPA.cPassphrase, 0, sizeof(net_params.xPassword.xWPA.cPassphrase));
            memcpy(net_params.xPassword.xWPA.cPassphrase, result_ptr, net_params.xPassword.xWPA.ucLength);
            net_params.xPassword.xWPA.cPassphrase[net_params.xPassword.xWPA.ucLength] = '\0';
        }
        result_ptr = NULL;
        if (sec == WIFI_SEC_WPA)
            net_params.xSecurity = eWiFiSecurityWPA;
        else
            net_params.xSecurity = eWiFiSecurityWPA2;
    }
    else if (sec == WIFI_SEC_NONE)
    { // OPEN
        net_params.xSecurity = eWiFiSecurityOpen;
        memset(net_params.xPassword.xWPA.cPassphrase, 0x00, wificonfigMAX_PASSPHRASE_LEN);
    }
    if (WIFI_IsConnected(&net_params) == eWiFiSuccess)
        return RN_STATUS_OK;
    else
        return RN_STATUS_FAIL;
}

int wifi_noti_started(void)
{
    PlatformMgrImpl().HandleWifiSystemEvent(WIFI_EVENT, RN_WIFI_STARTUP_IND_ID, NULL);
    return RN_STATUS_OK;
}

int wifi_noti_connected(void)
{
    PlatformMgrImpl().HandleWifiSystemEvent(WIFI_EVENT, RN_WIFI_CONNECT_IND_ID, NULL);
    return RN_STATUS_OK;
}

int wifi_noti_disconnected(void)
{
    R_BSP_WarmStartt(3);
    PlatformMgrImpl().HandleWifiSystemEvent(WIFI_EVENT, RN_WIFI_DISCONNECT_IND_ID, NULL);
    return RN_STATUS_OK;
}

int wifi_noti_ipv4(void)
{
    PlatformMgrImpl().HandleWifiSystemEvent(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL);
    return RN_STATUS_OK;
}

int wifi_noti_ipv6(void)
{
    PlatformMgrImpl().HandleWifiSystemEvent(IP_EVENT, IP_EVENT_GOT_IP6, NULL);
    return RN_STATUS_OK;
}

int check_wifi_provisioning_info(void)
{
    WIFIConnectionInfo_t pxConnectionInfo;
    WIFIReturnCode_t res;
    res = WIFI_GetConnectionInfo(&pxConnectionInfo);

    if (res != eWiFiSuccess)
        return RN_STATUS_FAIL;

    if ((pxConnectionInfo.ucSSIDLength == 0) || (strlen((const char *)&pxConnectionInfo.ucSSID[0]) == 0))
        return RN_STATUS_FAIL;
    return RN_STATUS_OK;
}

char * getSerialNumber(void)
{
    uint8_t pucMac[wificonfigMAX_BSSID_LEN] = { 0 };

    if (WIFI_GetMAC(pucMac) == eWiFiSuccess)
    {
        sprintf(tSerialNum, "%s_%02X%02X%02X", DEVICE_SERIAL_SUFFIX, pucMac[3], pucMac[4], pucMac[5]);
    }
    RENES_LOG("getSerialNumber %02X%02X%02X%02X%02X%02X\r\n", pucMac[0], pucMac[1], pucMac[2], pucMac[3], pucMac[4], pucMac[5]);
    return (char *) tSerialNum;
}

void shell_command(int argc, char * argv[])
{
    Engine::Root().ExecCommand(argc, argv);
}

void PRINTF_ATCMD_WRAP(const char *fmt, ...)
{
#if defined(ENABLE_CHIP_APP_EXT)
    char *at_buf = NULL;
    va_list ap;

    at_buf = (char *)pvPortMalloc(AT_RESMSG_LEN);
    if (at_buf == NULL) {
        RENES_LOG("- [%s] Failed to allocate the PRINTF_ATCMD_WRAP buffer\r\n", __func__);
        return;
    }
    va_start(ap, fmt);
    vsnprintf(at_buf, AT_RESMSG_LEN, fmt, ap);
    va_end(ap);
    RM_MATTER_PRINTF_ATCMD(at_buf);
    vPortFree(at_buf);
#endif // ENABLE_CHIP_APP_EXT
}

void app_print_ext(const char * fmt, ...)
{
#if defined(ENABLE_CHIP_APP_EXT)
    char *buf = NULL;
    va_list ap;

    buf = (char *)pvPortMalloc(AT_RESMSG_LEN);
    if (buf == NULL) {
        RENES_LOG("- [%s] Failed to allocate the app_print_ext buffer\r\n", __func__);
        return;
    }

    va_start(ap, fmt);
    vsnprintf(buf, AT_RESMSG_LEN, fmt, ap);
    va_end(ap);

    PRINTF_ATCMD_WRAP("\r\n%s\r\n", buf);

    va_end(ap);
    vPortFree(buf);
#endif // ENABLE_CHIP_APP_EXT
}

void app_ext_status_set(matterDeviceStatus _val)
{
#if defined(ENABLE_CHIP_APP_EXT)
    latestStatus = _val;

    PRINTF_ATCMD_WRAP("\r\n+MSTATUS=%d\r\n", _val);
#endif // ENABLE_CHIP_APP_EXT
}

int app_ext_status_get(void)
{
#if defined(ENABLE_CHIP_APP_EXT)
    return (int) latestStatus;
#else
    return 0;
#endif // ENABLE_CHIP_APP_EXT
}

int matter_config_check(void)
{
#if defined(ENABLE_CHIP_APP_EXT)
    uint16_t discriminator;
    DeviceLayer::GetCommissionableDataProvider()->GetSetupDiscriminator(discriminator);
    if (discriminator == 0)
        return RN_STATUS_FAIL;
    else
        return RN_STATUS_OK;
#endif // ENABLE_CHIP_APP_EXT
    return 0;
}

void set_matter_deviceType(uint16_t edpoint, uint32_t id, uint8_t ver)
{
    DeviceTypeList[edpoint].deviceId      = id;
    DeviceTypeList[edpoint].deviceVersion = ver;
}

uint8_t * get_matter_deviceType(uint16_t edpoint)
{
    return (uint8_t *) &DeviceTypeList[edpoint];
}

int set_matter_config(char * item, char * value, char * value1, char * value2, char * value3, char * value4)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    if (strncasecmp(item, "DISC", 4) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetSetupDiscriminator((unsigned short) atoi(value));
    }
    else if (strncasecmp(item, "PINCODE", 7) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetSetupPasscode(atoi(value));
    }
    else if (strncasecmp(item, "VID", 3) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetVendorId(atoi(value));
    }
    else if (strncasecmp(item, "PID", 3) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetProductId((unsigned short) atoi(value));
    }
    else if (strncasecmp(item, "HWVER", 5) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetHardwareVersion((unsigned short) atoi(value));
    }
    else if (strncasecmp(item, "SPKPCNT", 7) == 0)
    {
        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetSpake2pIterationCount(atoi(value));
    }
    else if (strncasecmp(item, "SPKPSALT", 8) == 0)
    {
        size_t saltLen = strlen(value);
        err            = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetSpake2pSalt(value, saltLen);
    }
    else if (strncasecmp(item, "SPKPVF", 6) == 0)
    {
        size_t verifierLen = strlen(value);
        err                = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetSpake2pVerifier(value, verifierLen);
    }
    else if (strncasecmp(item, "DEVTYPE", 7) == 0)
    {
        chip::Span<const EmberAfDeviceType> devicetypelst;
        chip::EndpointId edpoint = 1;
        uint32_t devicetypeid    = atoi(value);
        uint8_t deviceversion    = (uint8_t) atoi(value1);

        err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetDeviceTypeIdEp1(devicetypeid);
        if (err == CHIP_NO_ERROR)
            err = Rn::rnDeviceDataProvider::GetDeviceDataProvider().SetDeviceTypeVersionEp1(deviceversion);

        set_matter_deviceType((uint16_t) edpoint, devicetypeid, deviceversion);
        devicetypelst = chip::Span<const EmberAfDeviceType>((EmberAfDeviceType *) get_matter_deviceType((uint16_t) edpoint), 1);
        emberAfSetDeviceTypeList(edpoint, devicetypelst);
    }
    else
        return RN_STATUS_FAIL;

    return err.AsInteger();
}

int get_matter_onboardingcodes(char * rendezvous)
{
    chip::RendezvousInformationFlags aRendezvousFlags;
    char qrCodeBuffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
    chip::MutableCharSpan QRCode(qrCodeBuffer);
    char qrCodeUrlBuffer[128];

    if (strcasecmp(rendezvous, "none") == 0)
        aRendezvousFlags = chip::RendezvousInformationFlag::kNone;
    else if (strcasecmp(rendezvous, "softap") == 0)
        aRendezvousFlags = chip::RendezvousInformationFlag::kSoftAP;
    else if (strcasecmp(rendezvous, "ble") == 0)
        aRendezvousFlags = chip::RendezvousInformationFlag::kBLE;
    else if (strcasecmp(rendezvous, "onnetwork") == 0)
        aRendezvousFlags = chip::RendezvousInformationFlag::kOnNetwork;
    else
        return RN_STATUS_FAIL;

    if (GetQRCode(QRCode, aRendezvousFlags) != CHIP_NO_ERROR)
        return RN_STATUS_FAIL;
    if (GetQRCodeUrl(qrCodeUrlBuffer, sizeof(qrCodeUrlBuffer), QRCode) != CHIP_NO_ERROR)
        return RN_STATUS_FAIL;

    PRINTF_ATCMD_WRAP("\r\n+MSTATUS=BRDINFO,%s\r\n", qrCodeUrlBuffer);
    return RN_STATUS_OK;
}

int set_matter_certification_declaration(unsigned char * data, unsigned int size)
{
    CHIP_ERROR err;
    err = Renes::SetCertificationDeclaration(data, size);
    return err.AsInteger();
}

int set_matter_device_attestation_cert(unsigned char * data, unsigned int size)
{
    CHIP_ERROR err;
    err = Renes::SetDeviceAttestationCert(data, size);
    return err.AsInteger();
}

int set_matter_product_attestation_intermediate_cert(unsigned char * data, unsigned int size)
{
    CHIP_ERROR err;
    err = Renes::SetProductAttestationIntermediateCert(data, size);
    return err.AsInteger();
}

int set_matter_device_attestation_privkey(unsigned char * data, unsigned int size)
{
    CHIP_ERROR err;
    err = Renes::SetDeviceAttestationPrivKey(data, size);
    return err.AsInteger();
}

int set_matter_device_attestation_pubkey(unsigned char * data, unsigned int size)
{
    CHIP_ERROR err;
    err = Renes::SetDeviceAttestationPubKey(data, size);
    return err.AsInteger();
}

int set_matter_ble_adv_control(int control)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    bool adv_enabled;

    adv_enabled = ConnectivityMgr().IsBLEAdvertisingEnabled();
    if (control)
    {
        if (adv_enabled)
        {
            RENES_LOG("BLE advertising already enabled\r\n");
        }
        else
        {
            RENES_LOG("Starting BLE advertising\r\n");
            // err = ConnectivityMgr().SetBLEAdvertisingEnabled(control);

            PlatformMgrImpl().HandleWifiSystemEvent(IP_EVENT, IP_EVENT_GOT_IP6, NULL);
        }
    }
    else
    {
        if (adv_enabled)
        {
            RENES_LOG("Stopping BLE advertising\r\n");
            // err = ConnectivityMgr().SetBLEAdvertisingEnabled(control);
            PlatformMgrImpl().HandleWifiSystemEvent(IP_EVENT, IP_EVENT_STA_LOST_IP, NULL);
        }
        else
        {
            RENES_LOG("BLE advertising already stopped\r\n");
        }
    }

    return err.AsInteger();
}

int set_matter_attribute_control(uint16_t endpoint, uint32_t cluster, uint32_t attributeID, uint8_t * data, uint8_t dataType)
{
    return static_cast<int>(emberAfWriteAttribute(endpoint, cluster, attributeID, data, dataType));
}

int get_matter_attribute_control(uint16_t endpoint, uint32_t cluster, uint32_t attributeID, uint8_t * data, uint16_t dataLen)
{
    return static_cast<int>(emberAfReadAttribute(endpoint, cluster, attributeID, data, dataLen));
}

void set_software_fault(const char * faultstring)
{
    TaskStatus_t xTaskDetails;
    TaskHandle_t task_handle     = xTaskGetCurrentTaskHandle();
    System::Clock::Timestamp now = System::SystemClock().GetMonotonicTimestamp();
    SoftwareDiagnostics::Events::SoftwareFault::Type softwareFault;
    char threadName[kMaxThreadNameLength + 1];

    EnabledEndpointsWithServerCluster enabledEndpoints(SoftwareDiagnostics::Id);
    VerifyOrReturn(enabledEndpoints.begin() != enabledEndpoints.end());

    vTaskGetInfo(task_handle, &xTaskDetails, pdFALSE, eInvalid);
    Platform::CopyString(threadName, xTaskDetails.pcTaskName);
    softwareFault.name.SetValue(CharSpan::fromCharString(threadName));
    softwareFault.id = xTaskDetails.xTaskNumber;

    if (faultstring == NULL)
    {
        char timeChar[80];
        sprintf(timeChar, "%05lu%05lu%05lu%05lu ms", (unsigned long) (now.count() / 1000000000000000),
                (unsigned long) (now.count() / 10000000000 % 100000), (unsigned long) (now.count() / 100000 % 100000),
                (unsigned long) (now.count() % 100000));
        softwareFault.faultRecording.SetValue(ByteSpan(Uint8::from_const_char(timeChar), strlen(timeChar)));
    }
    else
    {
        softwareFault.faultRecording.SetValue(ByteSpan(Uint8::from_const_char(faultstring), strlen(faultstring)));
    }
    SoftwareDiagnosticsServer::Instance().OnSoftwareFaultDetect(softwareFault);
}

void set_platform_shutdown(void)
{
    PlatformMgrImpl().Shutdown();
}

void set_led_init(void)
{
    // hw_gpio_set_pin_function(HW_GPIO_PORT_1, HW_GPIO_PIN_14,
    // HW_GPIO_MODE_OUTPUT, HW_GPIO_FUNC_GPIO);
}

void set_led_control(int port, int num, int on)
{
    if (on)
    {
        // hw_gpio_set_active((HW_GPIO_PORT)port, (HW_GPIO_PIN)num);
    }
    else
    {
        // hw_gpio_set_inactive((HW_GPIO_PORT)port, (HW_GPIO_PIN)num);
    }
}

int matter_read_nvram_int(const char * name, int * _val)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    unsigned short _size;
    uint8_t * val = NULL;

    val = api_usr_nvram_read_binary(name, &_size);
    if (_size > 0)
    {
        *_val = *(int *) val;
        res   = 0;
    }
    else
    {
        // RENES_LOG("[%s] Read Err (%d, %d)!!!\n", __func__, val, _size);
    }
#endif

    return res;
}

char * matter_read_nvram_string(const char * name)
{
    char * str = NULL;

#if defined(__SUPPORT_USR_NVRAM__)
    str = api_usr_nvram_read_string(name);
#endif

    return str;
}

int matter_read_nvram_binary(const char * name, void * val, unsigned int size)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    unsigned short _size;
    uint8_t * bin = NULL;

    bin = api_usr_nvram_read_binary(name, &_size);
    memcpy(val, (void *) bin, (size < _size) ? size : _size);
    res = _size;
#endif

    return res;
}

int matter_write_nvram_int(const char * name, int val)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    res = api_usr_nvram_write_binary(name, (const char *) &val, 4);
#endif

    return res;
}

int matter_write_nvram_string(const char * name, const char * val)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    res = api_usr_nvram_write_string(name, val);
#endif

    return res;
}

int matter_write_nvram_binary(const char * name, const char * val, unsigned int size)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    res = api_usr_nvram_write_binary(name, val, size);
#endif

    return res;
}

int matter_delete_nvram_env(const char * name)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    res = api_usr_nvram_delete_item(name);
#endif

    return res;
}

int matter_delete_tmp_nvram_env(const char * name)
{
    int res = -1;

#if defined(__SUPPORT_USR_NVRAM__)
    res = api_usr_nvram_delete_item(name);
#endif

    return res;
}

int matter_save_tmp_nvram(void)
{
    return 0;
}

void matter_init_nvram(void)
{
#if defined(__SUPPORT_USR_NVRAM__)
    api_usr_nvram_bank_reset(0);
    api_usr_nvram_bank_reset(1);
    api_usr_nvram_bank_reset(2);
#endif
}

