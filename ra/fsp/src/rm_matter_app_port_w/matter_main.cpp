/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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


#include "matter_main.h"
#include <app/util/endpoint-config-api.h> //matterupwork

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer;
using namespace ::chip::Credentials;

#define UNUSED_PARAMETER(a) (a = a)

volatile int apperror_cnt;
static chip::DeviceLayer::DeviceInfoProviderImpl gExampleDeviceInfoProvider;

//matterworkup[[
constexpr EndpointId kNetworkCommissioningEndpointSecondary = 0xFFFE;
//]]matterworkup

// ================================================================================
// Main Code
// ================================================================================

#ifdef __cplusplus
extern "C" {
#endif

void matter_app_main_start(void *arg)
{
    CHIP_ERROR err;
#if defined ( __SUPPORT_USR_NVRAM__ )
{
    #include "api_usr_nvram.h"
    api_usr_nvram_init();
}
#endif // __SUPPORT_USR_NVRAM__

    init_rnPlatform();

    RENES_LOG("============================================1...");

    if ((err = rnMatterConfig::InitMatter(BLE_DEV_NAME)) != CHIP_NO_ERROR) {
//matterwork[[::exceptions
        if ((err.AsInteger() >= CHIP_ERROR_TLV_UNDERRUN.AsInteger() && err.AsInteger() <= CHIP_ERROR_TLV_CONTAINER_OPEN.AsInteger())
            || err == CHIP_ERROR_UNEXPECTED_TLV_ELEMENT) {
            RENES_LOG("Matter KVS being removed...");
            {
                chip::DeviceLayer::Internal::RnConfig::FactoryResetConfig();

                PersistedStorage::KeyValueStoreMgrImpl().ErasePartition();

                vTaskDelay(5); //for displaying debug message
                reboot_func(_SYS_REBOOT_);
            }
        }
//]]matterwork
        appError(err);
    }

    RENES_LOG("============================================2...");

    gExampleDeviceInfoProvider.SetStorageDelegate(&chip::Server::GetInstance().GetPersistentStorage());
    chip::DeviceLayer::SetDeviceInfoProvider(&gExampleDeviceInfoProvider);

    RENES_LOG("============================================3...");


    chip::DeviceLayer::PlatformMgr().LockChipStack();
    // Initialize device attestation config
#ifdef RENES_ATTESTATION_CREDENTIALS
    SetDeviceAttestationCredentialsProvider(Renes::GetRenesDacProvider());
#else
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
#endif

    RENES_LOG("============================================4...");


//matterworkup[[
    // We only have network commissioning on endpoint 0.
    emberAfEndpointEnableDisable(kNetworkCommissioningEndpointSecondary, false);
//]]matterworkup

    RENES_LOG("============================================5...");

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    RENES_LOG("Starting App Task");
    if ((err = AppTask::GetAppTask().StartAppTask()) != CHIP_NO_ERROR) {
        appError(err);
    }
    RENES_LOG("============================================6...");

    vTaskDelete(NULL);
}

int rn_btn_short_on_clicked(void)
{
#ifdef RN_FEATURE_SIMPLE_BUTTON_PRESENT //matterbutton[[::
    AppTask::GetAppTask().ButtonEventHandler();
#endif //]]matterbutton
	return 0; //must be '0' because SDK's low driver proceed reboot in case of '1'
}

#ifdef __cplusplus
}
#endif
//]]matterwork

