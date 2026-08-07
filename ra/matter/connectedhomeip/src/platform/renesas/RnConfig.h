/*
 *
 *    Copyright (c) 2020-2022 Project CHIP Authors
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
 *          Utilities for accessing persisted device configuration on
 *          platforms based on the Renesas SDK.
 */

#pragma once

#include <functional>
//#include <platform/internal/CHIPDeviceLayerInternal.h>   //if activated, make compile error

#ifndef KVS_MAX_ENTRIES
#define KVS_MAX_ENTRIES  255 // Available key slot count for Kvs Key mapping.
#endif

// Delay before Key/Value is actually saved in NVRAM
#define RENESAS_KVS_SAVE_DELAY_SECONDS 5

static_assert((KVS_MAX_ENTRIES <= 255), "Implementation supports up to 255 Kvs entries");
static_assert((KVS_MAX_ENTRIES >= 30), "Mininimal Kvs entries requirement is not met");

namespace chip {
namespace DeviceLayer {
namespace Internal {

/**
 *
 * This implementation uses the Renesas NVRAM sflash data storage library
 * as the underlying storage layer.
 *
 * NOTE: This class is designed to be mixed-in to the concrete subclass of the
 * GenericConfigurationManagerImpl<> template.  When used this way, the class
 * naturally provides implementations for the delegated members referenced by
 * the template class (e.g. the ReadConfigValue() method).
 */

// Renesas NVRAM objects use a 20-bit number,
// NVRAM Key 19:16 Stack region
// NVRAM Key 15:0 Available NVRAM keys 0x0000 -> 0xFFFF.
// Matter stack reserved region ranges from 0x03AB7200 to 0x03AB7FFF
// e.g. key = 0x03AB7201
// '03AB' = Matter NVRAM region
// '72' = the sub region group base offset (Factory, Config, Counter or KVS)
// '01' = the id offset inside the group.
constexpr uint32_t kUserNvramKeyDomainLoLimit = 0x000000U; // User Domain NVRAM Key Range lower limit
constexpr uint32_t kUserNvramKeyDomainHiLimit = 0x00FFFFU; // User Domain NVRAM Key Range Maximum limit
constexpr uint32_t kMatterNvramKeyDomain      = 0x03AB0000U;
constexpr uint32_t kMatterNvramKeyLoLimit     = 0x03AB7200U;
constexpr uint32_t kMatterNvramKeyHiLimit     = 0x03AB7FFFU;
constexpr inline uint32_t RnConfigKey(uint8_t keyBaseOffset, uint8_t id)
{
    return kMatterNvramKeyDomain | static_cast<uint32_t>(keyBaseOffset) << 8 | id;
}

class RnConfig
{
public:
    // Definitions for Renesas NVRAM SFLASH driver:-

    using Key = uint32_t;

    // NVRAM key base offsets used by the CHIP Device Layer.
    // ** Key base can range from 0x72 to 0x7F **
    // Persistent config values set at manufacturing time. Retained during factory reset.
    static constexpr uint8_t kMatterFactory_KeyBase = 0x72;
    // Persistent config values set at runtime. Cleared during factory reset.
    static constexpr uint8_t kMatterConfig_KeyBase = 0x73;
    // Persistent counter values set at runtime. Retained during factory reset.
    static constexpr uint8_t kMatterCounter_KeyBase = 0x74;
    // Persistent config values set at runtime. Cleared during factory reset.
    static constexpr uint8_t kMatterKvs_KeyBase = 0x75;

    // Key definitions for well-known configuration values.
    // Factory config keys
    static constexpr Key kConfigKey_SerialNum             = RnConfigKey(kMatterFactory_KeyBase, 0x00);
    static constexpr Key kConfigKey_MfrDeviceId           = RnConfigKey(kMatterFactory_KeyBase, 0x01);
    static constexpr Key kConfigKey_MfrDeviceCert         = RnConfigKey(kMatterFactory_KeyBase, 0x02);
    static constexpr Key kConfigKey_MfrDevicePrivateKey   = RnConfigKey(kMatterFactory_KeyBase, 0x03);
    static constexpr Key kConfigKey_ManufacturingDate     = RnConfigKey(kMatterFactory_KeyBase, 0x04);
    static constexpr Key kConfigKey_SetupPayloadBitSet    = RnConfigKey(kMatterFactory_KeyBase, 0x05);
    static constexpr Key kConfigKey_MfrDeviceICACerts     = RnConfigKey(kMatterFactory_KeyBase, 0x06);
    static constexpr Key kConfigKey_SetupDiscriminator    = RnConfigKey(kMatterFactory_KeyBase, 0x07);
    static constexpr Key kConfigKey_Spake2pIterationCount = RnConfigKey(kMatterFactory_KeyBase, 0x08);
    static constexpr Key kConfigKey_Spake2pSalt           = RnConfigKey(kMatterFactory_KeyBase, 0x09);
    static constexpr Key kConfigKey_Spake2pVerifier       = RnConfigKey(kMatterFactory_KeyBase, 0x0A);
    static constexpr Key kConfigKey_ProductId             = RnConfigKey(kMatterFactory_KeyBase, 0x0B);
    static constexpr Key kConfigKey_VendorId              = RnConfigKey(kMatterFactory_KeyBase, 0x0C);
    static constexpr Key kConfigKey_VendorName            = RnConfigKey(kMatterFactory_KeyBase, 0x0D);
    static constexpr Key kConfigKey_ProductName           = RnConfigKey(kMatterFactory_KeyBase, 0x0E);
    static constexpr Key kConfigKey_HardwareVersionString = RnConfigKey(kMatterFactory_KeyBase, 0x0F);
    static constexpr Key KConfigKey_ProductLabel          = RnConfigKey(kMatterFactory_KeyBase, 0x10);
    static constexpr Key kConfigKey_ProductURL            = RnConfigKey(kMatterFactory_KeyBase, 0x11);
    static constexpr Key kConfigKey_PartNumber            = RnConfigKey(kMatterFactory_KeyBase, 0x12);
    static constexpr Key kConfigKey_UniqueId              = RnConfigKey(kMatterFactory_KeyBase, 0x1F);
//matterwork[[::
    static constexpr Key kConfigKey_SetupPinCode          = RnConfigKey(kMatterFactory_KeyBase, 0x20);
    static constexpr Key kConfigKey_DeviceTypeEp1Id       = RnConfigKey(kMatterFactory_KeyBase, 0x21);
    static constexpr Key kConfigKey_DeviceTypeEp1Ver      = RnConfigKey(kMatterFactory_KeyBase, 0x22);
//]]matterwork
    // Matter Config Keys
    static constexpr Key kConfigKey_ServiceConfig      = RnConfigKey(kMatterConfig_KeyBase, 0x01);
    static constexpr Key kConfigKey_PairedAccountId    = RnConfigKey(kMatterConfig_KeyBase, 0x02);
    static constexpr Key kConfigKey_ServiceId          = RnConfigKey(kMatterConfig_KeyBase, 0x03);
    static constexpr Key kConfigKey_LastUsedEpochKeyId = RnConfigKey(kMatterConfig_KeyBase, 0x05);
    static constexpr Key kConfigKey_FailSafeArmed      = RnConfigKey(kMatterConfig_KeyBase, 0x06);
    static constexpr Key kConfigKey_GroupKey           = RnConfigKey(kMatterConfig_KeyBase, 0x07);
    static constexpr Key kConfigKey_HardwareVersion    = RnConfigKey(kMatterConfig_KeyBase, 0x08);
    static constexpr Key kConfigKey_RegulatoryLocation = RnConfigKey(kMatterConfig_KeyBase, 0x09);
    static constexpr Key kConfigKey_CountryCode        = RnConfigKey(kMatterConfig_KeyBase, 0x0A);
    static constexpr Key kConfigKey_WiFiSSID           = RnConfigKey(kMatterConfig_KeyBase, 0x0C);
    static constexpr Key kConfigKey_WiFiPSK            = RnConfigKey(kMatterConfig_KeyBase, 0x0D);
    static constexpr Key kConfigKey_WiFiSEC            = RnConfigKey(kMatterConfig_KeyBase, 0x0E);
    static constexpr Key kConfigKey_GroupKeyBase       = RnConfigKey(kMatterConfig_KeyBase, 0x0F);
    static constexpr Key kConfigKey_LockUser           = RnConfigKey(kMatterConfig_KeyBase, 0x10);
    static constexpr Key kConfigKey_Credential         = RnConfigKey(kMatterConfig_KeyBase, 0x11);
    static constexpr Key kConfigKey_LockUserName       = RnConfigKey(kMatterConfig_KeyBase, 0x12);
    static constexpr Key kConfigKey_CredentialData     = RnConfigKey(kMatterConfig_KeyBase, 0x13);
    static constexpr Key kConfigKey_UserCredentials    = RnConfigKey(kMatterConfig_KeyBase, 0x14);
    static constexpr Key kConfigKey_WeekDaySchedules   = RnConfigKey(kMatterConfig_KeyBase, 0x15);
    static constexpr Key kConfigKey_YearDaySchedules   = RnConfigKey(kMatterConfig_KeyBase, 0x16);
    static constexpr Key kConfigKey_HolidaySchedules   = RnConfigKey(kMatterConfig_KeyBase, 0x17);
    static constexpr Key kConfigKey_OpKeyMap           = RnConfigKey(kMatterConfig_KeyBase, 0x20);
	
    static constexpr Key kConfigKey_Creds_KeyId        = RnConfigKey(kMatterConfig_KeyBase, 0x21);
    static constexpr Key kConfigKey_Creds_Base_Addr    = RnConfigKey(kMatterConfig_KeyBase, 0x22);
    static constexpr Key kConfigKey_Creds_DAC_Offset   = RnConfigKey(kMatterConfig_KeyBase, 0x23);
    static constexpr Key kConfigKey_Creds_DAC_Size     = RnConfigKey(kMatterConfig_KeyBase, 0x24);
    static constexpr Key kConfigKey_Creds_PAI_Offset   = RnConfigKey(kMatterConfig_KeyBase, 0x25);
    static constexpr Key kConfigKey_Creds_PAI_Size     = RnConfigKey(kMatterConfig_KeyBase, 0x26);
    static constexpr Key kConfigKey_Creds_CD_Offset    = RnConfigKey(kMatterConfig_KeyBase, 0x27);
    static constexpr Key kConfigKey_Creds_CD_Size      = RnConfigKey(kMatterConfig_KeyBase, 0x28);
	
    static constexpr Key kConfigKey_Creds_DAC_Priv_Size  = RnConfigKey(kMatterConfig_KeyBase, 0x29);
    static constexpr Key kConfigKey_Creds_DAC_Pub_Size   = RnConfigKey(kMatterConfig_KeyBase, 0x2A);



    static constexpr Key kConfigKey_GroupKeyMax =
        RnConfigKey(kMatterConfig_KeyBase, 0x1E); // Allows 16 Group Keys to be created.

    // Matter Counter Keys
    static constexpr Key kConfigKey_BootCount             = RnConfigKey(kMatterCounter_KeyBase, 0x00);
    static constexpr Key kConfigKey_TotalOperationalHours = RnConfigKey(kMatterCounter_KeyBase, 0x01);
    static constexpr Key kConfigKey_LifeTimeCounter       = RnConfigKey(kMatterCounter_KeyBase, 0x02);
    static constexpr Key kConfigKey_MigrationCounter      = RnConfigKey(kMatterCounter_KeyBase, 0x03);

    // Matter KVS storage Keys
    static constexpr Key kConfigKey_KvsStringKeyMap = RnConfigKey(kMatterKvs_KeyBase, 0x00);
    static constexpr Key kConfigKey_KvsFirstKeySlot = RnConfigKey(kMatterKvs_KeyBase, 0x01);
#if 1 //org user_nvram
    static constexpr Key kConfigKey_KvsLastKeySlot  = RnConfigKey(kMatterKvs_KeyBase, KVS_MAX_ENTRIES);
#else //[rrq61000 matter work]    
    static constexpr Key kConfigKey_KvsLastKeySlot  = RnConfigKey(kMatterKvs_KeyBase, (NVDS_CHIPCFG_TOTAL_TAG - NVDS_TAG_KVS_7500 +1));
#endif
    // Set key id limits for each group.
    static constexpr Key kMinConfigKey_MatterFactory = RnConfigKey(kMatterFactory_KeyBase, 0x00);
    static constexpr Key kMaxConfigKey_MatterFactory = RnConfigKey(kMatterFactory_KeyBase, 0x1F);
    static constexpr Key kMinConfigKey_MatterConfig  = RnConfigKey(kMatterConfig_KeyBase, 0x00);
    static constexpr Key kMaxConfigKey_MatterConfig  = RnConfigKey(kMatterConfig_KeyBase, 0x20);

    // Allows 32 Counters to be created.
    static constexpr Key kMinConfigKey_MatterCounter = RnConfigKey(kMatterCounter_KeyBase, 0x00);
    static constexpr Key kMaxConfigKey_MatterCounter = RnConfigKey(kMatterCounter_KeyBase, 0x1F);

    static constexpr Key kMinConfigKey_MatterKvs = kConfigKey_KvsStringKeyMap;
    static constexpr Key kMaxConfigKey_MatterKvs = kConfigKey_KvsLastKeySlot;

    static CHIP_ERROR Init(void);
    static void DeInit(void);

    static CHIP_ERROR ReadConfigValue(Key key, bool & val);
    static CHIP_ERROR ReadConfigValue(Key key, uint32_t & val);
    static CHIP_ERROR ReadConfigValue(Key key, uint64_t & val);
    static CHIP_ERROR ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen);
    static CHIP_ERROR ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen);
    static CHIP_ERROR ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen, size_t offset);
    static CHIP_ERROR ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen, size_t keyLen, size_t offset);
    static CHIP_ERROR ReadConfigValueCounter(uint8_t counterIdx, uint32_t & val);
    static CHIP_ERROR WriteConfigValue(Key key, bool val);
    static CHIP_ERROR WriteConfigValue(Key key, uint32_t val);
    static CHIP_ERROR WriteConfigValue(Key key, uint64_t val);
    static CHIP_ERROR WriteConfigValueStr(Key key, const char * str);
    static CHIP_ERROR WriteConfigValueStr(Key key, const char * str, size_t strLen);
    static CHIP_ERROR WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen);
    static CHIP_ERROR WriteConfigValueCounter(uint8_t counterIdx, uint32_t val);
    static CHIP_ERROR ClearConfigValue(Key key);
    static CHIP_ERROR ClearConfigValueTemp(Key key);
    static CHIP_ERROR SaveConfigValueTemp(void);
    static bool ConfigValueExists(Key key);
    static bool ConfigValueExists(Key key, size_t & dataLen);
    static CHIP_ERROR FactoryResetConfig(void);
    static bool ValidConfigKey(Key key);

    static void RunConfigUnitTest(void);

};

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
