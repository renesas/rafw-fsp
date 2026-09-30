/*
 *
 *    Copyright (c) 2021-2022 Project CHIP Authors
 *    All rights reserved.
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
 *          Platform-specific key value storage implementation for Renesas
 */

#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/CHIPMemString.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/KeyValueStoreManager.h>
//matterwork[[::
#include <app/server/Server.h>
#include "platform/renesas/RnConfig.h"
#include "rnDeviceWrapAPIs.h"
#if __SUPPORT_MATTER_FSP_MODULE__
#else
#include "nvedit.h"
#endif
extern void matter_init_nvram(void);
//#define _DBG_KVS_
#define _PROC_EXCEPT_
//]]matterwork
#include <stdio.h>
#include <string.h>

using namespace ::chip;
using namespace ::chip::Crypto;
using namespace ::chip::DeviceLayer::Internal;

#define CONVERT_KEYMAP_INDEX_TO_NVRAMKEY(index) (RnConfig::kConfigKey_KvsFirstKeySlot + index)
#define CONVERT_NVRAMKEY_TO_KEYMAP_INDEX(nvramKey) (nvramKey - RnConfig::kConfigKey_KvsFirstKeySlot)

namespace chip {
namespace DeviceLayer {
namespace PersistedStorage {

KeyValueStoreManagerImpl KeyValueStoreManagerImpl::sInstance;
uint16_t mKvsKeyMap[KeyValueStoreManagerImpl::kMaxEntries] = { 0 };

CHIP_ERROR KeyValueStoreManagerImpl::Init(void)
{
    CHIP_ERROR err;
    err = RnConfig::Init();
    SuccessOrExit(err);

    size_t outLen;

    memset(mKvsKeyMap, 0, sizeof(mKvsKeyMap));
    err = RnConfig::ReadConfigValueBin(RnConfig::kConfigKey_KvsStringKeyMap, reinterpret_cast<uint8_t *>(mKvsKeyMap),
                                           sizeof(mKvsKeyMap), outLen);

    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) // Initial boot
    {
        err = CHIP_NO_ERROR;
    }

exit:
    return err;
}

bool KeyValueStoreManagerImpl::IsValidKvsNvramKey(uint32_t nvramKey) const
{
    return ((RnConfig::kConfigKey_KvsFirstKeySlot <= nvramKey) && (nvramKey <= RnConfig::kConfigKey_KvsLastKeySlot));
}

uint16_t KeyValueStoreManagerImpl::hashKvsKeyString(const char * key) const
{
    uint8_t hash256[Crypto::kSHA256_Hash_Length] = { 0 };
    Crypto::Hash_SHA256(reinterpret_cast<const uint8_t *>(key), strlen(key), hash256);

    uint16_t hash16, i = 0;

    while (!hash16 && (i < (Crypto::kSHA256_Hash_Length - 1)))
    {
        hash16 = (hash256[i] | (hash256[i + 1] << 8));
        i++;
    }
    return hash16;
}

CHIP_ERROR KeyValueStoreManagerImpl::MapKvsKeyToNvram(const char * key, uint16_t hash, uint32_t & nvramKey, bool isSlotNeeded) const
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    char * strPrefix          = nullptr;
    uint8_t firstEmptyKeySlot = kMaxEntries;
    for (uint8_t keyIndex = 0; keyIndex < kMaxEntries; keyIndex++)
    {
        if (mKvsKeyMap[keyIndex] == hash)
        {
            uint32_t tempNvramkey = CONVERT_KEYMAP_INDEX_TO_NVRAMKEY(keyIndex);
            VerifyOrDie(IsValidKvsNvramKey(tempNvramkey) == true);

            size_t readCount;
            size_t length = strlen(key);
            if (strPrefix == nullptr)
            {
                // Use a calloc to initialize all bits to 0. alloc +1 for a null char
                strPrefix = static_cast<char *>(Platform::MemoryCalloc(1, length + 1));
                VerifyOrDie(strPrefix != nullptr);
            }

            // Collision prevention
            // Read the data from NVRAM it should be prefixed by the kvsString
            // else we will look for another matching hash in the map
            RnConfig::ReadConfigValueBin(tempNvramkey, reinterpret_cast<uint8_t *>(strPrefix), length, readCount, 0);
            if (strcmp(key, strPrefix) == 0)
            {
                // String matches we have confirmed the hash pointed us the right key data
                nvramKey = tempNvramkey;
                Platform::MemoryFree(strPrefix);
                return CHIP_NO_ERROR;
            }
        }

        if (isSlotNeeded && (firstEmptyKeySlot == kMaxEntries) && (mKvsKeyMap[keyIndex] == 0))
        {
            firstEmptyKeySlot = keyIndex;
        }
    }

    Platform::MemoryFree(strPrefix);

    if (isSlotNeeded)
    {
        if (firstEmptyKeySlot != kMaxEntries)
        {
            nvramKey = CONVERT_KEYMAP_INDEX_TO_NVRAMKEY(firstEmptyKeySlot);
            VerifyOrDie(IsValidKvsNvramKey(nvramKey) == true);
            err = CHIP_NO_ERROR;
        }
        else
        {
            err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
        }
    }
    else
    {
        err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }
    return err;
}

void KeyValueStoreManagerImpl::ForceKeyMapSave()
{
    OnScheduledKeyMapSave(nullptr, nullptr);
}

//matterwork[[::exceptions::
void KeyValueStoreManagerImpl::SetExceptionEvent(ExceptionEventTypes except_evt)
{
	uint8_t curFabCount = 0;
	
	curFabCount = Server::GetInstance().GetFabricTable().FabricCount();
#ifdef _PROC_EXCEPT_
#ifdef _DBG_KVS_
	RENES_LOG("[%s:%s:%d] FabricCount()=%d", __FILENAME__, __func__, __LINE__, curFabCount);
#endif
	
	if (except_evt == kEvtExcFailSafeTimerExpired || except_evt == kEvtExcCommissionWindowExpired) {
		if (curFabCount <= 0) {
CHK_MAP:
			uint16_t keyIndex = 0;
			uint16_t countTrash = 0;
			uint16_t startIndex = 9;
#if CHIP_CONFIG_PERSIST_SUBSCRIPTIONS
			startIndex += 1;
#endif
			/*
			1:"g/lkgt",
			2:"g/gcc",
			3:"g/gdc",
			4:"g/im/ec",
			5:"g/ts/tz", //"g/a/%x/%u/%u" (v1.1)
			6:"g/a/1/8/0"
			7:"g/a/1/300/7"
			8:"g/a/1/300/4001"
			*/
			//checking trash data
			for (keyIndex = startIndex; keyIndex < KeyValueStoreManagerImpl::kMaxEntries; keyIndex++) {
				if (mKvsKeyMap[keyIndex] != 0 && mKvsKeyMap[keyIndex] != 0xffff) {
					CHIP_ERROR err = RnConfig::ClearConfigValueTemp(RnConfig::kMinConfigKey_MatterKvs+keyIndex+1);
					if (err == CHIP_NO_ERROR) {
					}
					countTrash++;
				}
			}
			if (countTrash) {
				RENES_LOG("[%s:%s:%d] Matter KVS being arranged (countTrash=%d) by event(=%s)...and reboot for commisssioning...", __FILENAME__, __func__, __LINE__,
					countTrash, except_evt == kEvtExcFailSafeTimerExpired ? "kEvtExcFailSafeTimerExpired" : "kEvtExcCommissionWindowExpired");
				ErasePartition();
#if defined(__RRQ61400__)
				matter_init_nvram();
#endif
			}
			else {
				RENES_LOG("event(=%s) received...and reboot for commisssioning...", 
					except_evt == kEvtExcFailSafeTimerExpired ? "kFailSafeTimerExpired" : "kEvtExcCommissionWindowExpired");
			}
			vTaskDelay(5); //for displaying debug message
			reboot_func(_SYS_REBOOT_);
		}
		else {
			uint8_t idx;
			for (idx = 1; idx <= curFabCount; idx++) {
				const FabricInfo * fabric = Server::GetInstance().GetFabricTable().FindFabricWithIndex(idx);
				if (fabric == nullptr) {
#ifdef _DBG_KVS_
					RENES_LOG("[%s:%s:%d] index(=%d) fabric invalid (nullptr)", __FILENAME__, __func__, __LINE__, idx);
#endif
					if (curFabCount == 1 && idx == 1) {
						goto CHK_MAP;
					}
				}
				else {
#ifdef _DBG_KVS_
					RENES_LOG("[%s:%s:%d] index(=%d) fabric valid: label=%s", __FILENAME__, __func__, __LINE__, idx, fabric->GetFabricLabel().data());
					RENES_LOG("[%s:%s:%d] GetFabricIndex()=%d, GetFabricId()=0x%llx", __FILENAME__, __func__, __LINE__, static_cast<unsigned>(fabric->GetFabricIndex()), static_cast<unsigned>(fabric->GetFabricId()));
					RENES_LOG("[%s:%s:%d] GetVendorId()=0x%04x (%s), GetNodeId()=0x%llx", __FILENAME__, __func__, __LINE__, 
						fabric->GetVendorId(), (fabric->GetVendorId()==0x1349) ? "Apple" : ((fabric->GetVendorId()==0x6006) ? "Google" : "Others"),
						fabric->GetNodeId());
#endif
				}
			}
		}
	}
#else
    (void) curFabCount;
	RENES_LOG("exception event(=%s)...received...", 
		except_evt == kEvtExcFailSafeTimerExpired ? "kEvtExcFailSafeTimerExpired" : "kEvtExcCommissionWindowExpired");
#endif
}
//]]matterwork

void KeyValueStoreManagerImpl::OnScheduledKeyMapSave(System::Layer * systemLayer, void * appState)
{
#ifdef _DBG_KVS_
	RENES_LOG("[%s:%s:%d] FabricCount()=%d", __FILENAME__, __func__, __LINE__, Server::GetInstance().GetFabricTable().FabricCount());
#endif
    RnConfig::WriteConfigValueBin(RnConfig::kConfigKey_KvsStringKeyMap, reinterpret_cast<const uint8_t *>(mKvsKeyMap),
                                      sizeof(mKvsKeyMap));
}

void KeyValueStoreManagerImpl::OnScheduledReboot(System::Layer * systemLayer, void * appState)
{
#ifdef _DBG_KVS_
	RENES_LOG("[%s:%s:%d] FabricCount()=%d ... reboot...", __FILENAME__, __func__, __LINE__, Server::GetInstance().GetFabricTable().FabricCount());
#endif
	vTaskDelay(5); //for displaying debug message
#if __MATTER_FSP_TEMP_NO_API__
#else
    reboot_func(_SYS_REBOOT_);
#endif
}

void KeyValueStoreManagerImpl::ScheduleKeyMapSave(void)
{
	//matterwork[[::exceptions
	uint8_t curFabCount = 0;
	bool needToReboot = false;
	//uint32_t cmMode = static_cast<uint32_t>(Server::GetInstance().GetCommissioningWindowManager().GetCommissioningMode());
	
	curFabCount = Server::GetInstance().GetFabricTable().FabricCount();
#ifdef _DBG_KVS_
	RENES_LOG("[%s:%s:%d] FabricCount()=%d, max_key_entry=%d", __FILENAME__, __func__, __LINE__, curFabCount, KeyValueStoreManagerImpl::kMaxEntries);
	//RENES_LOG("[%s:%s:%d] GetCommissioningMode()=%d", __FILENAME__, __func__, __LINE__, cmMode);
#endif

#ifdef _PROC_EXCEPT_
	if (curFabCount <= 0) {
CHK_MAP:
        uint16_t keyIndex = 0;
        uint16_t countTrash = 0;
		uint16_t startIndex = 9;
#if CHIP_CONFIG_PERSIST_SUBSCRIPTIONS
		startIndex += 1;
#endif
		/*
		1:"g/lkgt", 
		2:"g/gcc",
		3:"g/gdc",
		4:"g/im/ec",
		5:"g/a/%x/%u/%u"
		*/
		//checking trash data
		for (keyIndex = startIndex; keyIndex < KeyValueStoreManagerImpl::kMaxEntries; keyIndex++) {
			if (mKvsKeyMap[keyIndex] != 0 && mKvsKeyMap[keyIndex] != 0xffff) {
				countTrash++;
			}
		}
		if (countTrash) {
			RENES_LOG("[%s:%s:%d] Matter KVS being arranged (countTrash=%d) by writing and deleting...and reboot for commisssioning...", __FILENAME__, __func__, __LINE__, countTrash);
			{
				ErasePartition();
#if defined(__RRQ61400__)
				matter_init_nvram();
#endif
			}
			needToReboot = true;
		}
	}
	else {
		uint8_t idx;
		for (idx = 1; idx <= curFabCount; idx++) {
			const FabricInfo * fabric = Server::GetInstance().GetFabricTable().FindFabricWithIndex(idx);
			if (fabric == nullptr) {
#ifdef _DBG_KVS_
				RENES_LOG("[%s:%s:%d] index(=%d) fabric invalid (nullptr)", __FILENAME__, __func__, __LINE__, idx);
#endif
				if (curFabCount == 1 && idx == 1) {
					// not used for Google
					//goto CHK_MAP;
				}
			}
			else {
#ifdef _DBG_KVS_
				RENES_LOG("[%s:%s:%d] index(=%d) fabric valid: label=%s", __FILENAME__, __func__, __LINE__, idx, fabric->GetFabricLabel().data());
				RENES_LOG("[%s:%s:%d] GetFabricIndex()=%d, GetFabricId()=0x%llx", __FILENAME__, __func__, __LINE__, static_cast<unsigned>(fabric->GetFabricIndex()), static_cast<unsigned>(fabric->GetFabricId()));
				RENES_LOG("[%s:%s:%d] GetVendorId()=0x%04x (%s), GetNodeId()=0x%llx", __FILENAME__, __func__, __LINE__, 
					fabric->GetVendorId(), (fabric->GetVendorId()==0x1349) ? "Apple" : ((fabric->GetVendorId()==0x6006) ? "Google" : "Others"),
					fabric->GetNodeId());
#endif
			}
		}
	}
	if (needToReboot) {
		SystemLayer().StartTimer(
			std::chrono::duration_cast<System::Clock::Timeout>(System::Clock::Seconds32(RENESAS_KVS_SAVE_DELAY_SECONDS)),
			KeyValueStoreManagerImpl::OnScheduledReboot, NULL);
		return;
	}
	//]]matterwork
    /*
        During commissioning, the key map will be modified multiples times subsequently.
        Commit the key map in NVRAM once it as stabilized.
    */
#else
    (void)curFabCount;
#endif
    SystemLayer().StartTimer(
        std::chrono::duration_cast<System::Clock::Timeout>(System::Clock::Seconds32(RENESAS_KVS_SAVE_DELAY_SECONDS)),
        KeyValueStoreManagerImpl::OnScheduledKeyMapSave, NULL);
}

CHIP_ERROR KeyValueStoreManagerImpl::_Get(const char * key, void * value, size_t value_size, size_t * read_bytes_size,
                                          size_t offset_bytes) const
{
    VerifyOrReturnError(key != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    uint32_t nvramKey;
    uint16_t hash  = hashKvsKeyString(key);
    CHIP_ERROR err = MapKvsKeyToNvram(key, hash, nvramKey);
    VerifyOrReturnError(err == CHIP_NO_ERROR, err);

    size_t outLen;
    // The user doesn't need the KeyString prefix, Read data after it
    size_t KeyStringLen = strlen(key);
    err                 = RnConfig::ReadConfigValueBin(nvramKey, reinterpret_cast<uint8_t *>(value), value_size, outLen,
                                           (offset_bytes + KeyStringLen));
    if (read_bytes_size)
    {
        *read_bytes_size = outLen;
    }

    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Put(const char * key, const void * value, size_t value_size)
{
    VerifyOrReturnError(key != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    uint32_t nvramKey;
    uint16_t hash  = hashKvsKeyString(key);
    CHIP_ERROR err = MapKvsKeyToNvram(key, hash, nvramKey, /* isSlotNeeded */ true);
    VerifyOrReturnError(err == CHIP_NO_ERROR, err);

    // add the string Key as prefix to the stored data as a collision prevention mechanism.
    size_t keyStringLen    = strlen(key);
    uint8_t * prefixedData = static_cast<uint8_t *>(Platform::MemoryAlloc(keyStringLen + value_size));
    VerifyOrDie(prefixedData != nullptr);
    memcpy(prefixedData, key, keyStringLen);
    memcpy(prefixedData + keyStringLen, value, value_size);

#if !defined(USE_LVL_VALUE_RAM)
	// Clear first for successful data update. If there is no key, it is not deleted.
	//RnConfig::ClearConfigValue(nvramKey);
	//
#endif		
    err = RnConfig::WriteConfigValueBin(nvramKey, prefixedData, keyStringLen + value_size);
    if (err == CHIP_NO_ERROR)
    {
        uint32_t keyIndex    = CONVERT_NVRAMKEY_TO_KEYMAP_INDEX(nvramKey);
        mKvsKeyMap[keyIndex] = hash;
#if CHIP_CONFIG_PERSIST_SUBSCRIPTIONS
		//"g/sum"
		if (strcmp(key, DefaultStorageKeyAllocator::SubscriptionResumptionMaxCount().KeyName()) == 0) {
			ForceKeyMapSave();
		}
		else 
#endif
        ScheduleKeyMapSave();
    }
    Platform::MemoryFree(prefixedData);
    return err;
}

CHIP_ERROR KeyValueStoreManagerImpl::_Delete(const char * key)
{
    VerifyOrReturnError(key != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    uint32_t nvramKey;
    uint16_t hash  = hashKvsKeyString(key);
    CHIP_ERROR err = MapKvsKeyToNvram(key, hash, nvramKey);
    VerifyOrReturnError(err == CHIP_NO_ERROR, err);

    err = RnConfig::ClearConfigValue(nvramKey);
    if (err == CHIP_NO_ERROR)
    {
        uint32_t keyIndex = CONVERT_NVRAMKEY_TO_KEYMAP_INDEX(nvramKey);
        mKvsKeyMap[keyIndex] = 0;
#if CHIP_CONFIG_PERSIST_SUBSCRIPTIONS
		//"g/sum"
		if (strcmp(key, DefaultStorageKeyAllocator::SubscriptionResumptionMaxCount().KeyName()) == 0) {
			ForceKeyMapSave();
		}
		else 
#endif
        ScheduleKeyMapSave();
    }

    return err;
}

void KeyValueStoreManagerImpl::ErasePartition(void)
{
    // Iterate over all the Matter Kvs NVRAM records and delete each one...
    for (uint32_t nvramKey = RnConfig::kMinConfigKey_MatterKvs; nvramKey < RnConfig::kMaxConfigKey_MatterKvs; nvramKey++)
    {
        RnConfig::ClearConfigValueTemp(nvramKey);
    }
	RnConfig::SaveConfigValueTemp();

    memset(mKvsKeyMap, 0, sizeof(mKvsKeyMap));
}

} // namespace PersistedStorage
} // namespace DeviceLayer
} // namespace chip
