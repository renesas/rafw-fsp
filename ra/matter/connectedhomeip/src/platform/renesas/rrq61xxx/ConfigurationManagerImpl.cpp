/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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
 *          Provides the implementation of the Device Layer ConfigurationManager object
 *          for Renesas platforms using the Renesas SDK.
 */
/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/internal/GenericConfigurationManagerImpl.ipp>

#include <platform/ConfigurationManager.h>
#include <platform/DiagnosticDataProvider.h>

//matterwork[[::for Renesas
#include "rnDeviceWrapAPIs.h"
#include "platform/renesas/RnConfig.h"
//]]matterwork


#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#include "wifi_rn_events.h"
#endif

namespace chip {
namespace DeviceLayer {

using namespace ::chip::DeviceLayer::Internal;

ConfigurationManagerImpl & ConfigurationManagerImpl::GetDefaultInstance()
{
    static ConfigurationManagerImpl sInstance;
    return sInstance;
}

CHIP_ERROR ConfigurationManagerImpl::Init()
{
    CHIP_ERROR err;

    // Initialize the generic implementation base class.
    err = Internal::GenericConfigurationManagerImpl<RnConfig>::Init();
    SuccessOrExit(err);
    // TODO: Initialize the global GroupKeyStore object here (#1626)

    IncreaseBootCount();
//orig SDK[[::
/*
    // It is possible to configure the possible reset sources with RMU_ResetControl
    // In this case, we keep Reset control at default setting
    rebootCause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();
*/
//matterwork[[::
//]]matterwork

    err = CHIP_NO_ERROR;

exit:
    return err;
}

bool ConfigurationManagerImpl::CanFactoryReset()
{
    // TODO: query the application to determine if factory reset is allowed.
    return true;
}

void ConfigurationManagerImpl::InitiateFactoryReset()
{
    PlatformMgr().ScheduleWork(DoFactoryReset);
}

CHIP_ERROR ConfigurationManagerImpl::GetRebootCount(uint32_t & rebootCount)
{
    return RnConfig::ReadConfigValue(RnConfig::kConfigKey_BootCount, rebootCount);
}

CHIP_ERROR ConfigurationManagerImpl::IncreaseBootCount(void)
{
    uint32_t bootCount = 0;

    if (RnConfig::ConfigValueExists(RnConfig::kConfigKey_BootCount))
    {
        GetRebootCount(bootCount);
    }

    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_BootCount, bootCount + 1);
}

CHIP_ERROR ConfigurationManagerImpl::GetBootReason(uint32_t & bootReason)
{
    // rebootCause is obtained at bootup.
    BootReasonType matterBootCause;

    matterBootCause = BootReasonType::kUnspecified;
    bootReason = to_underlying(matterBootCause);
    return CHIP_NO_ERROR;
}

CHIP_ERROR ConfigurationManagerImpl::GetTotalOperationalHours(uint32_t & totalOperationalHours)
{
    if (!RnConfig::ConfigValueExists(RnConfig::kConfigKey_TotalOperationalHours))
    {
        totalOperationalHours = 0;
        return CHIP_NO_ERROR;
    }

    return RnConfig::ReadConfigValue(RnConfig::kConfigKey_TotalOperationalHours, totalOperationalHours);
}

CHIP_ERROR ConfigurationManagerImpl::StoreTotalOperationalHours(uint32_t totalOperationalHours)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_TotalOperationalHours, totalOperationalHours);
}

CHIP_ERROR ConfigurationManagerImpl::ReadPersistedStorageValue(::chip::Platform::PersistedStorage::Key persistedStorageKey,
                                                               uint32_t & value)
{
    // This method reads CHIP Persisted Counter type NVRAM objects.
    // (where persistedStorageKey represents an index to the counter).
    CHIP_ERROR err;

    err = RnConfig::ReadConfigValueCounter(persistedStorageKey, value);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }
    SuccessOrExit(err);

exit:
    return err;
}

CHIP_ERROR ConfigurationManagerImpl::WritePersistedStorageValue(::chip::Platform::PersistedStorage::Key persistedStorageKey,
                                                                uint32_t value)
{
    // This method reads CHIP Persisted Counter type NVRAM objects.
    // (where persistedStorageKey represents an index to the counter).
    CHIP_ERROR err;

    err = RnConfig::WriteConfigValueCounter(persistedStorageKey, value);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }
    SuccessOrExit(err);

exit:
    return err;
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, bool & val)
{
    return RnConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint32_t & val)
{
    return RnConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint64_t & val)
{
    return RnConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    return RnConfig::ReadConfigValueStr(key, buf, bufSize, outLen);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    return RnConfig::ReadConfigValueBin(key, buf, bufSize, outLen);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, bool val)
{
    return RnConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint32_t val)
{
    return RnConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint64_t val)
{
    return RnConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str)
{
    return RnConfig::WriteConfigValueStr(key, str);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    return RnConfig::WriteConfigValueStr(key, str, strLen);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    return RnConfig::WriteConfigValueBin(key, data, dataLen);
}

void ConfigurationManagerImpl::RunConfigUnitTest(void)
{
    RnConfig::RunConfigUnitTest();
}

void ConfigurationManagerImpl::DoFactoryReset(intptr_t arg)
{
    CHIP_ERROR err;

    ChipLogProgress(DeviceLayer, "Performing factory reset");

    err = RnConfig::FactoryResetConfig();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "FactoryResetConfig() failed: %s", chip::ErrorStr(err));
    }

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD

    ChipLogProgress(DeviceLayer, "Clearing Thread provision");
    ThreadStackMgr().ErasePersistentInfo();

#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

    PersistedStorage::KeyValueStoreMgrImpl().ErasePartition();

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
    ChipLogProgress(DeviceLayer, "Clearing WiFi provision");
    wifi_clear_wifi_provision();
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION

    // Restart the system.
    ChipLogProgress(DeviceLayer, "System restarting");
//matterwork[[::for displaying debug message
	vTaskDelay(5);
//]]matterwork

    // When called from an RPC, the following reset occurs before the RPC can respond,
    // which breaks tests (because it looks like the RPC hasn't successfully completed).
    // One way to fix this is to reduce the priority of this task, to allow logging to
    // complete before the reset occurs.
    vTaskPrioritySet(NULL, 0);
#if __MATTER_FSP_TEMP_NO_API__
#else
    reboot_func(_SYS_REBOOT_);
#endif
}

#ifdef RN_WIFI
CHIP_ERROR ConfigurationManagerImpl::GetPrimaryWiFiMACAddress(uint8_t * buf)
{
    wifi_mac_address_t macaddr;
    wifi_get_wifi_mac_addr(RN_WIFI_STA_INTERFACE, &macaddr);
    memcpy(buf, &macaddr.octet[0], sizeof(macaddr.octet));

    return CHIP_NO_ERROR;
}
#endif

ConfigurationManager & ConfigurationMgrImpl()
{
    return ConfigurationManagerImpl::GetDefaultInstance();
}

} // namespace DeviceLayer
} // namespace chip
