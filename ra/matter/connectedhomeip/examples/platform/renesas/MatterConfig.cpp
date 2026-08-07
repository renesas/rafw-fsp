 /*
  *
  *    Copyright (c) 2023 Renesas Electronics Corporation and/or its  affiliates
  *    All rights reserved.
  *
  *    Licensed under the Apache License, Version 2.0 (the "License");
  *    you may not use this file except in compliance with the License.
  *    You may obtain a copy of the License at
  *
  *     http://www.apache.org/licenses/LICENSE-2.0
 
  *
  *    Unless required by applicable law or agreed to in writing, software
  *    distributed under the License is distributed on an "AS IS" BASIS,
  *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  *    See the License for the specific language governing permissions and
  *    limitations under the License.
  */
 /**
  ****************************************************************************************
  *
  * @file MatterConfig.cpp
  *
  * @brief initialize and start Matter code
  *
 ****************************************************************************************
 */

#include "AppConfig.h"
#include "app/clusters/ota-requestor/DefaultOTARequestor.h"

//#define _DBG_EXCEPT_
#include <MatterConfig.h>

#include <FreeRTOS.h>

#include <mbedtls/platform.h>

#ifdef RN_WIFI
#include "wifi_rn_events.h"
#endif /* RN_WIFI */

#ifdef ENABLE_CHIP_SHELL
#include "matter_shell.h"
#endif

//matterwork[[::exceptions::overriding for commissioning evt
using namespace ::chip;
using namespace ::chip::DeviceLayer;

#include <app/server/AppDelegate.h>
class CommissionAppDelegate: public AppDelegate
{
public:
    void OnCommissioningSessionStarted() {
#ifdef _DBG_EXCEPT_
        RENES_LOG("[%s:%s:%d] Renesas specific CM callback called", __FILENAME__, __func__, __LINE__);
#endif
        mIsSessionStarted = true;
    }
    void OnCommissioningSessionStopped() {
#ifdef _DBG_EXCEPT_
        RENES_LOG("[%s:%s:%d] Renesas specific CM callback called", __FILENAME__, __func__, __LINE__);
#endif
    }

    void OnCommissioningWindowOpened() {
        mIsWindowOpened = true;
#ifdef _DBG_EXCEPT_
        RENES_LOG("[%s:%s:%d] Renesas specific CM callback called", __FILENAME__, __func__, __LINE__);
#endif
    }
    void OnCommissioningWindowClosed() {
        uint8_t curFabCount = 0;

        curFabCount = Server::GetInstance().GetFabricTable().FabricCount();

#ifdef _DBG_EXCEPT_
        RENES_LOG("[%s:%s:%d] FabricCount()=%d", __FILENAME__, __func__, __LINE__, curFabCount);
        RENES_LOG("[%s:%s:%d] Renesas specific CM callback called: mIsWindowOpened=%d", __FILENAME__, __func__, __LINE__, mIsWindowOpened);
#endif
        if (mIsSessionStarted == false && mIsWindowOpened) {
            if (curFabCount == 0) {
#ifdef _DBG_EXCEPT_
                RENES_LOG("[%s:%s:%d] SetExceptEvent(kEvtExcCommissionWindowExpired) called", __FILENAME__, __func__, __LINE__);
#endif
                PersistedStorage::KeyValueStoreMgrImpl().SetExceptionEvent(PersistedStorage::ExceptionEventTypes::kEvtExcCommissionWindowExpired);
            }
            else {
                uint8_t idx;
                for (idx = 1; idx <= curFabCount; idx++) {
                    const FabricInfo * fabric = Server::GetInstance().GetFabricTable().FindFabricWithIndex(idx);
                    if (fabric == nullptr) {
#ifdef _DBG_EXCEPT_
                        RENES_LOG("[%s:%s:%d] index(=%d) fabric invalid (nullptr)", __FILENAME__, __func__, __LINE__, idx);
#endif
                        if (curFabCount == 1 && idx == 1) {
#ifdef _DBG_EXCEPT_
                            RENES_LOG("[%s:%s:%d] SetExceptEvent(kEvtExcCommissionWindowExpired) called", __FILENAME__, __func__, __LINE__);
#endif
                            PersistedStorage::KeyValueStoreMgrImpl().SetExceptionEvent(PersistedStorage::ExceptionEventTypes::kEvtExcCommissionWindowExpired);
                        }
                    }
                }
            }
        }
        if(mIsSessionStarted && mIsWindowOpened == false) {
            //RENES_LOG("[%s:%s:%d] mIsSessionStarted=%d being false", __FILENAME__, __func__, __LINE__, mIsSessionStarted);
            mIsSessionStarted = false;
        }
        mIsWindowOpened = false;
    }
private:
    bool mIsWindowOpened;
    bool mIsSessionStarted;
};

CommissionAppDelegate cmChildAppDelegate, *pcmChildAppDelegate = nullptr;
//]]matterwork

using namespace ::chip::Inet;

#include <crypto/CHIPCryptoPAL.h>

#include "rnDeviceDataProvider.h"
#include "rnDeviceWrapAPIs.h"
#include <app/InteractionModelEngine.h>

#if (CHIP_CONFIG_USE_ICD_SUBSCRIPTION_CALLBACKS == 1)
ICDCallbackManager rnMatterConfig::mICDCallbackManagerHandler;
#endif // CHIP_CONFIG_USE_ICD_SUBSCRIPTION_CALLBACKS

#if CHIP_ENABLE_OPENTHREAD
#include <inet/EndPointStateOpenThread.h>
#include <openthread/cli.h>
#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/heap.h>
#include <openthread/icmp6.h>
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/platform/openthread-system.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>

// ================================================================================
// Matter Networking Callbacks
// ================================================================================
void LockOpenThreadTask(void)
{
    chip::DeviceLayer::ThreadStackMgr().LockThreadStack();
}

void UnlockOpenThreadTask(void)
{
    chip::DeviceLayer::ThreadStackMgr().UnlockThreadStack();
}

// ================================================================================
// rnMatterConfig Methods
// ================================================================================

CHIP_ERROR rnMatterConfig::InitOpenThread(void)
{
    ReturnErrorOnFailure(ThreadStackMgr().InitThreadStack());

#if CHIP_DEVICE_CONFIG_THREAD_FTD
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router));
#else // CHIP_DEVICE_CONFIG_THREAD_FTD
#if CHIP_CONFIG_ENABLE_ICD_SERVER
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice));
#else  // CHIP_CONFIG_ENABLE_ICD_SERVER
    ReturnErrorOnFailure(ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice));
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER
#endif // CHIP_DEVICE_CONFIG_THREAD_FTD

    return ThreadStackMgrImpl().StartThreadTask();
}
#endif // CHIP_ENABLE_OPENTHREAD

#if RN_MATTER_OTA_ENABLED
void rnMatterConfig::InitOTARequestorHandler(System::Layer * systemLayer, void * appState)
{
    OTAConfig::Init();
}
#endif

void rnMatterConfig::ConnectivityEventCallback(const ChipDeviceEvent * event, intptr_t arg)
{
    // Initialize OTA only when Thread or WiFi connectivity is established
    if (((event->Type == DeviceEventType::kThreadConnectivityChange) &&
         (event->ThreadConnectivityChange.Result == kConnectivity_Established)) ||
        ((event->Type == DeviceEventType::kInternetConnectivityChange) &&
         (event->InternetConnectivityChange.IPv6 == kConnectivity_Established)))
    {
#if RN_MATTER_OTA_ENABLED
        chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds32(OTAConfig::kInitOTARequestorDelaySec),
                                                    InitOTARequestorHandler, nullptr);
#endif
    }
}

CHIP_ERROR rnMatterConfig::InitMatter(const char * appName)
{
    CHIP_ERROR err;

    RENES_LOG("==================================================");
    RENES_LOG("%s starting", appName);
    RENES_LOG("==================================================");

    //==============================================
    // Init Matter Stack
    //==============================================
    RENES_LOG("Init CHIP Stack");
    // Init Chip memory management before the stack
    ReturnErrorOnFailure(chip::Platform::MemoryInit());
    ReturnErrorOnFailure(PlatformMgr().InitChipStack());

    SetDeviceInstanceInfoProvider(&Rn::rnDeviceDataProvider::GetDeviceDataProvider());
    SetCommissionableDataProvider(&Rn::rnDeviceDataProvider::GetDeviceDataProvider());

    chip::DeviceLayer::ConnectivityMgr().SetBLEDeviceName(appName);

#if CHIP_ENABLE_OPENTHREAD
    ReturnErrorOnFailure(InitOpenThread());
#endif

    // Stop Matter event handling while setting up resources
    chip::DeviceLayer::PlatformMgr().LockChipStack();

    // Create initParams with SDK example defaults here
    static chip::CommonCaseDeviceServerInitParams initParams;

#if CHIP_CRYPTO_PLATFORM
    // When building with DA16XXX crypto, use the opaque key store
    // instead of the default (insecure) one.
    gOperationalKeystore.Init();
    initParams.operationalKeystore = &gOperationalKeystore;
#endif

    // Initialize the remaining (not overridden) providers to the SDK example defaults
    (void) initParams.InitializeStaticResourcesBeforeServerInit();

#if CHIP_ENABLE_OPENTHREAD
    // Set up OpenThread configuration when OpenThread is included
    chip::Inet::EndPointStateOpenThread::OpenThreadEndpointInitParam nativeParams;
    nativeParams.lockCb                = LockOpenThreadTask;
    nativeParams.unlockCb              = UnlockOpenThreadTask;
    nativeParams.openThreadInstancePtr = chip::DeviceLayer::ThreadStackMgrImpl().OTInstance();
    initParams.endpointNativeParams    = static_cast<void *>(&nativeParams);
#endif

//matterwork[[::exceptions::overriding for commissioning evt
    pcmChildAppDelegate = &cmChildAppDelegate;
    if (initParams.appDelegate == nullptr) {
        initParams.appDelegate = pcmChildAppDelegate;
    }
//]]matterwork
    // Init Matter Server and Start Event Loop
    err = chip::Server::GetInstance().Init(initParams);

#if (CHIP_CONFIG_USE_ICD_SUBSCRIPTION_CALLBACKS == 1)
    // Register ICD subscription callback to match subscription max intervals to its idle time interval
    chip::app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&mICDCallbackManagerHandler);
#endif // CHIP_CONFIG_USE_ICD_SUBSCRIPTION_CALLBACKS

    chip::DeviceLayer::PlatformMgr().UnlockChipStack();

    ReturnErrorOnFailure(err);

    // OTA Requestor initialization will be triggered by the connectivity events
    PlatformMgr().AddEventHandler(ConnectivityEventCallback, reinterpret_cast<intptr_t>(nullptr));

    RENES_LOG("Starting Platform Manager Event Loop");
    ReturnErrorOnFailure(PlatformMgr().StartEventLoopTask());

#ifdef RN_WIFI
    InitWiFi();
#endif

#ifdef ENABLE_CHIP_SHELL
    chip::startShellTask();
#endif

    return CHIP_NO_ERROR;
}

#ifdef RN_WIFI
void rnMatterConfig::InitWiFi(void)
{
}
#endif // RN_WIFI

