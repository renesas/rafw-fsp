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

/**********************************************************
 * Includes
 *********************************************************/

#include "AppConfig.h"
#include "AppEvent.h"
#include "AppTask.h"

#include "rnDeviceDataProvider.h"
#include "rnDeviceWrapAPIs.h"
#include <app/icd/server/ICDNotifier.h>
#include <app/server/OnboardingCodesUtil.h>
#include <app/server/Server.h>
#include <app/server/Dnssd.h>
#include <app/util/attribute-storage.h>
#include <assert.h>
#include <lib/support/CodeUtils.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#include <app/util/MatterCallbacks.h>
#if defined(__USE_MATTER_DPM_APP__)
#include "RnDPMManager.h"
#endif

#if CHIP_ENABLE_OPENTHREAD
#include <platform/OpenThread/OpenThreadUtils.h>
#include <platform/ThreadStackManager.h>
//matterwork[[::
#include <platform/renesas/ThreadStackManagerImpl.h>
//]]matterwork
#endif // CHIP_ENABLE_OPENTHREAD

#ifdef RN_WIFI
#include "wifi_rn_events.h"
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <platform/renesas/NetworkCommissioningWiFiDriver.h>
#endif // RN_WIFI

/* Renesas_Matter_OTA related includes */
#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
#include <app/clusters/ota-requestor/BDXDownloader.h>
#include <app/clusters/ota-requestor/DefaultOTARequestor.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorDriver.h>
#include <app/clusters/ota-requestor/DefaultOTARequestorStorage.h>
#include <platform/renesas/rrq61xxx/OTAImageProcessorImpl.h>
#endif

/**********************************************************
 * Defines and Constants
 *********************************************************/

#define FACTORY_RESET_TRIGGER_TIMEOUT 3000
#define FACTORY_RESET_CANCEL_WINDOW_TIMEOUT 3000
#ifndef APP_TASK_STACK_SIZE
#define APP_TASK_STACK_SIZE (4096)
#endif
#define APP_TASK_PRIORITY 2
#define APP_EVENT_QUEUE_SIZE 10
#define EXAMPLE_VENDOR_ID 0xcafe

using namespace chip;
using namespace chip::app;
using namespace ::chip::DeviceLayer;

namespace {

/**********************************************************
 * Variable declarations
 *********************************************************/

TimerHandle_t sFunctionTimer; // FreeRTOS app sw timer.
TimerHandle_t sLightTimer;

TaskHandle_t sAppTaskHandle;
QueueHandle_t sAppEventQueue;

#ifdef RN_WIFI
app::Clusters::NetworkCommissioning::Instance
    sWiFiNetworkCommissioningInstance(0 /* Endpoint Id */, &(NetworkCommissioning::RnWiFiDriver::GetInstance()));
#endif /* RN_WIFI */

bool sIsProvisioned = false;

#if !(defined(CHIP_CONFIG_ENABLE_ICD_SERVER) && CHIP_CONFIG_ENABLE_ICD_SERVER)
bool sIsEnabled          = false;
bool sIsAttached         = false;
bool sHaveBLEConnections = false;
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER


#if ( configSUPPORT_STATIC_ALLOCATION == 1 ) //matterwork[[::
uint8_t sAppEventQueueBuffer[APP_EVENT_QUEUE_SIZE * sizeof(AppEvent)];
StaticQueue_t sAppEventQueueStruct;
#endif //]]

StackType_t appStack[APP_TASK_STACK_SIZE / sizeof(StackType_t)];
#if ( configSUPPORT_STATIC_ALLOCATION == 1 ) //matterwork[[::
StaticTask_t appTaskStruct;
#endif //]]

BaseApplication::Function_t mFunction;
bool mFunctionTimerActive;

#ifdef EMBER_AF_PLUGIN_IDENTIFY_SERVER
Clusters::Identify::EffectIdentifierEnum sIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;

Identify gIdentify = {
    chip::EndpointId{ 1 },
    BaseApplication::OnIdentifyStart,
    BaseApplication::OnIdentifyStop,
    Clusters::Identify::IdentifyTypeEnum::kVisibleIndicator,
    BaseApplication::OnTriggerIdentifyEffect,
};

#endif // EMBER_AF_PLUGIN_IDENTIFY_SERVER

/* Renesas_Matter_OTA related includes */
#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
DefaultOTARequestor gRequestorCore;
DefaultOTARequestorStorage gRequestorStorage;
DefaultOTARequestorDriver gRequestorUser;
BDXDownloader gDownloader;
OTAImageProcessorImpl gImageProcessor;

constexpr uint16_t requestedOtaBlockSize = 1024;
uint32_t gPeriodicQueryTimeoutSec = 24 * 60 * 60;		// for test 2 * 60  default value defined (24 * 60 *60)  24H
uint32_t gWatchdogTimeoutSec      = 6 * 60 * 60;			// for test    default value defined (6 * 60 * 60) 6H
#endif

} // namespace

/**********************************************************
 * AppTask Definitions
 *********************************************************/

CHIP_ERROR BaseApplication::StartAppTask(TaskFunction_t taskFunction)
{
//matterwork[[::
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    sAppEventQueue = xQueueCreateStatic(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent), sAppEventQueueBuffer, &sAppEventQueueStruct);
#else
    sAppEventQueue = xQueueCreate(APP_EVENT_QUEUE_SIZE, sizeof(AppEvent));
#endif
//]]matterwork
    if (sAppEventQueue == NULL)
    {
        RENES_LOG("Failed to allocate app event queue");
        appError(APP_ERROR_EVENT_QUEUE_FAILED);
    }

    // Start App task.
#if 0 //orig SDK[[::static fn not used
    sAppTaskHandle =
        xTaskCreateStatic(taskFunction, APP_TASK_NAME, ArraySize(appStack), &sAppEventQueue, 1, appStack, &appTaskStruct);
    if (sAppTaskHandle == nullptr)
    {
        appError(APP_ERROR_CREATE_TASK_FAILED);
    }
#else //matterwork[[::
#if ( configSUPPORT_STATIC_ALLOCATION == 1 )
    sAppTaskHandle =
        xTaskCreateStatic(taskFunction, APP_TASK_NAME, ArraySize(appStack), &sAppEventQueue, 1, appStack, &appTaskStruct);
    if (sAppTaskHandle == nullptr)
    {
        RENES_LOG("Failed to create app task");
        appError(APP_ERROR_CREATE_TASK_FAILED);
    }
#else
    BaseType_t xReturn = pdFAIL;
    xReturn =
        xTaskCreate(taskFunction, APP_TASK_NAME, ArraySize(appStack), &sAppEventQueue, 1, &sAppTaskHandle);
    if (xReturn != pdPASS) {
        appError(APP_ERROR_CREATE_TASK_FAILED);
    }
#endif
#endif //]]
    return CHIP_NO_ERROR;
}


#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR  //  Renesas_Matter_OTA
void BaseApplication::InitOTARequestor(intptr_t arg)
{
    // Initialize and interconnect the Requestor and Image Processor objects -- START
    SetRequestorInstance(&gRequestorCore);

    // Periodic query timeout must be set prior to the driver being initialized 
    gRequestorUser.SetPeriodicQueryTimeout(gPeriodicQueryTimeoutSec);

    // Watchdog timeout can be set any time before a query image is sent	 
    gRequestorUser.SetWatchdogTimeout(gWatchdogTimeoutSec);    	

    gRequestorStorage.Init(chip::Server::GetInstance().GetPersistentStorage());
    gRequestorCore.Init(chip::Server::GetInstance(), gRequestorStorage, gRequestorUser, gDownloader);
    gRequestorUser.SetMaxDownloadBlockSize(requestedOtaBlockSize);	
    gRequestorUser.Init(&gRequestorCore, &gImageProcessor);
    gImageProcessor.SetOTADownloader(&gDownloader);

    // Connect the gDownloader and Image Processor objects
    gDownloader.SetImageProcessorDelegate(&gImageProcessor);
    // Initialize and interconnect the Requestor and Image Processor objects -- END
    
}
#endif

CHIP_ERROR BaseApplication::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    /* OTA related includes */
#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
    chip::DeviceLayer::PlatformMgr().ScheduleWork(InitOTARequestor, reinterpret_cast<intptr_t>(nullptr));
#endif

#ifdef RN_WIFI
    /*
     * Wait for the WiFi to be initialized
     */
    RENES_LOG("APP: Wait WiFi Init");
    while (!wifi_hw_ready())
    {
        vTaskDelay(10);
    }
    RENES_LOG("APP: Done WiFi Init");
    /* We will init server when we get IP */

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    sWiFiNetworkCommissioningInstance.Init();	
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
#endif

    // Create FreeRTOS sw timer for Function Selection.
    sFunctionTimer = xTimerCreate("FnTmr",                  // Just a text name, not used by the RTOS kernel
                                  1,         // == default timer period
                                  false,                    // no timer reload (==one-shot)
                                  (void *) this,            // init timer id = app task obj context
                                  FunctionTimerEventHandler // timer callback handler
    );
    if (sFunctionTimer == NULL)
    {
        RENES_LOG("funct timer create failed");
        appError(APP_ERROR_CREATE_TIMER_FAILED);
    }

    // Create FreeRTOS sw timer for LED Management.
    sLightTimer = xTimerCreate("LightTmr",            // Text Name
                               10,     // Default timer period
                               true,                  // reload timer
                               (void *) this,         // Timer Id
                               LightTimerEventHandler // Timer callback handler
    );
    if (sLightTimer == NULL)
    {
        RENES_LOG("Light Timer create failed");
        appError(APP_ERROR_CREATE_TIMER_FAILED);
    }

    RENES_LOG("Current Software Version String: %s", CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING);
    RENES_LOG("Current Software Version: %d", CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION);

    ConfigurationMgr().LogDeviceConfig();

#if defined(ENABLE_CHIP_APP_EXT)
    chip::Span<const EmberAfDeviceType> devicetypelst;
    chip::EndpointId edpoint = 1;
    static EmberAfDeviceType DeviceTypeList[] = {{0x0016,1},{0,1}};
    uint32_t devicetypeid = 0x0100;
    uint8_t deviceversion = 1;

    if (Rn::rnDeviceDataProvider::GetDeviceDataProvider().GetDeviceTypeIdEp1(devicetypeid) != CHIP_NO_ERROR)
        RENES_LOG("Getting DeviceType ID for EP1 failed!");

    if (Rn::rnDeviceDataProvider::GetDeviceDataProvider().GetDeviceTypeVersionEp1(deviceversion) != CHIP_NO_ERROR)
        RENES_LOG("Getting DeviceType Version for EP1 failed!");

    set_matter_deviceType((uint16_t)edpoint, devicetypeid, deviceversion);
    
    devicetypelst = chip::Span<const EmberAfDeviceType>((EmberAfDeviceType *)get_matter_deviceType((uint16_t)edpoint), 1);
    emberAfSetDeviceTypeList(edpoint, devicetypelst);
    
    RENES_LOG("Base App EP1 %ld, %d ", devicetypeid, deviceversion);

    if (matter_config_check() != 0)
    {
        app_ext_status_set(_Status_need_configuration);
        while (1)
        {
            RENES_LOG("APP: Waiting commissioning configuration......");
            vTaskDelay(portCONVERT_MS_2_TICKS(1000));
            if(matter_config_check() == 0)
                break;
        }
    }
#endif //ENABLE_CHIP_APP_EXT
    // Create buffer for QR code that can fit max size and null terminator.
    char qrCodeBuffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
    chip::MutableCharSpan QRCode(qrCodeBuffer);

    if (Rn::rnDeviceDataProvider::GetDeviceDataProvider().GetSetupPayload(QRCode) == CHIP_NO_ERROR)
    {
#if defined(__RRQ61400__)
        PrintOnboardingCodes(chip::RendezvousInformationFlag::kBLE);
#else
        PrintOnboardingCodes(chip::RendezvousInformationFlag::kOnNetwork);
#endif
    }
    else
    {
        RENES_LOG("Getting QR code failed!");
    }

    PlatformMgr().AddEventHandler(OnPlatformEvent, 0);
    sIsProvisioned = ConnectivityMgr().IsThreadProvisioned();
#if defined(RRQ61400)
#if CFG_PMGR //some env makes fault when wifi down, so disable dpm temporary.
    RM_PMGR_W_dpm_disable();
#endif
#endif
    //[rrq61000 matter work]
    if (Server::GetInstance().GetFabricTable().FabricCount() != 0)
        app_ext_status_set(_Status_STA_start);
    else
        app_ext_status_set(_Status_commissioning_mode_start);

    return err;
}

void BaseApplication::FunctionTimerEventHandler(TimerHandle_t xTimer)
{
    AppEvent event;
    event.Type               = AppEvent::kEventType_Timer;
    event.TimerEvent.Context = (void *) xTimer;
    event.Handler            = FunctionEventHandler;
    PostEvent(&event);
}

void BaseApplication::FunctionEventHandler(AppEvent * aEvent)
{
    if (aEvent->Type != AppEvent::kEventType_Timer)
    {
        return;
    }

    // If we reached here, the button was held past FACTORY_RESET_TRIGGER_TIMEOUT,
    // initiate factory reset
    if (mFunctionTimerActive && mFunction == kFunction_StartBleAdv)
    {
        RENES_LOG("Factory Reset Triggered. Release button within %ums to cancel.", FACTORY_RESET_CANCEL_WINDOW_TIMEOUT);

        // Start timer for FACTORY_RESET_CANCEL_WINDOW_TIMEOUT to allow user to
        // cancel, if required.
        StartFunctionTimer(FACTORY_RESET_CANCEL_WINDOW_TIMEOUT);

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
        StartStatusLEDTimer();
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER

        mFunction = kFunction_FactoryReset;

    }
    else if (mFunctionTimerActive && mFunction == kFunction_FactoryReset)
    {
        // Actually trigger Factory Reset
        mFunction = kFunction_NoneSelected;

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
        StopStatusLEDTimer();
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER

        ScheduleFactoryReset();
    }
}

void BaseApplication::LightEventHandler()
{
    // Collect connectivity and configuration state from the CHIP stack. Because
    // the CHIP event loop is being run in a separate task, the stack must be
    // locked while these values are queried.  However we use a non-blocking
    // lock request (TryLockCHIPStack()) to avoid blocking other UI activities
    // when the CHIP task is busy (e.g. with a long crypto operation).
#if !(defined(CHIP_CONFIG_ENABLE_ICD_SERVER) && CHIP_CONFIG_ENABLE_ICD_SERVER)
    if (PlatformMgr().TryLockChipStack())
    {
#ifdef RN_WIFI
        sIsProvisioned = ConnectivityMgr().IsWiFiStationProvisioned();
        sIsEnabled     = ConnectivityMgr().IsWiFiStationEnabled();
        sIsAttached    = ConnectivityMgr().IsWiFiStationConnected();
#endif /* RN_WIFI */
#if CHIP_ENABLE_OPENTHREAD
        sIsEnabled  = ConnectivityMgr().IsThreadEnabled();
        sIsAttached = ConnectivityMgr().IsThreadAttached();
#endif /* CHIP_ENABLE_OPENTHREAD */
        sHaveBLEConnections = (ConnectivityMgr().NumBLEConnections() != 0);
        PlatformMgr().UnlockChipStack();
    }
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER

    // Update the status LED if factory reset has not been initiated.
    //
    // If system has "full connectivity", keep the LED On constantly.
    //
    // If thread and service provisioned, but not attached to the thread network
    // yet OR no connectivity to the service OR subscriptions are not fully
    // established THEN blink the LED Off for a short period of time.
    //
    // If the system has ble connection(s) uptill the stage above, THEN blink
    // the LEDs at an even rate of 100ms.
    //
    // Otherwise, blink the LED ON for a very short time.
    if (mFunction != kFunction_FactoryReset)
    {

    }

}

#ifdef RN_FEATURE_SIMPLE_BUTTON_PRESENT //matterbutton[[::
void BaseApplication::ButtonHandler(AppEvent * aEvent)
{
    // To trigger software update: press the APP_FUNCTION_BUTTON button briefly (<
    // FACTORY_RESET_TRIGGER_TIMEOUT) To initiate factory reset: press the
    // APP_FUNCTION_BUTTON for FACTORY_RESET_TRIGGER_TIMEOUT +
    // FACTORY_RESET_CANCEL_WINDOW_TIMEOUT All LEDs start blinking after
    // FACTORY_RESET_TRIGGER_TIMEOUT to signal factory reset has been initiated.
    // To cancel factory reset: release the APP_FUNCTION_BUTTON once all LEDs
    // start blinking within the FACTORY_RESET_CANCEL_WINDOW_TIMEOUT
    if (aEvent->ButtonEvent.Action == static_cast<uint8_t>(RnPlatform::ButtonAction::ButtonPressed))
    {
        if (!mFunctionTimerActive && mFunction == kFunction_NoneSelected)
        {
            StartFunctionTimer(FACTORY_RESET_TRIGGER_TIMEOUT);
            mFunction = kFunction_StartBleAdv;
        }
    }
    else
    {
        // If the button was released before factory reset got initiated, open the commissioning window and start BLE advertissement
        // in fast mode
        if (mFunctionTimerActive && mFunction == kFunction_StartBleAdv)
        {
            CancelFunctionTimer();
            mFunction = kFunction_NoneSelected;

            if (!ConnectivityMgr().IsWiFiStationProvisioned())
            {
                // Open Basic CommissioningWindow. Will start BLE advertisements
                chip::DeviceLayer::PlatformMgr().LockChipStack();
                CHIP_ERROR err = chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow();
                chip::DeviceLayer::PlatformMgr().UnlockChipStack();
                if (err != CHIP_NO_ERROR)
                {
                    RENES_LOG("Failed to open the Basic Commissioning Window");
                }
            }
            else
            {
                RENES_LOG("Network is already provisioned, Ble advertissement not enabled");
#if CHIP_CONFIG_ENABLE_ICD_SERVER
                // Temporarily claim network activity, until we implement a "user trigger" reason for ICD wakeups.
                PlatformMgr().LockChipStack();
                ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
                PlatformMgr().UnlockChipStack();
#endif // CHIP_CONFIG_ENABLE_ICD_SERVER
            }
        }
        else if (mFunctionTimerActive && mFunction == kFunction_FactoryReset)
        {
            CancelFunctionTimer();

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
            StopStatusLEDTimer();
#endif

            // Change the function to none selected since factory reset has been
            // canceled.
            mFunction = kFunction_NoneSelected;
            RENES_LOG("Factory Reset has been Canceled");
        }
    }
}

#endif //]]matterbutton

void BaseApplication::CancelFunctionTimer()
{
    if (xTimerStop(sFunctionTimer, 0) == pdFAIL)
    {
        RENES_LOG("app timer stop() failed");
        appError(APP_ERROR_STOP_TIMER_FAILED);
    }

    mFunctionTimerActive = false;
}

void BaseApplication::StartFunctionTimer(uint32_t aTimeoutInMs)
{
    if (xTimerIsTimerActive(sFunctionTimer))
    {
        RENES_LOG("app timer already started!");
        CancelFunctionTimer();
    }

    // timer is not active, change its period to required value (== restart).
    // FreeRTOS- Block for a maximum of 100 ticks if the change period command
    // cannot immediately be sent to the timer command queue.
    if (xTimerChangePeriod(sFunctionTimer, aTimeoutInMs / portTICK_PERIOD_MS, 100) != pdPASS)
    {
        RENES_LOG("app timer start() failed");
        appError(APP_ERROR_START_TIMER_FAILED);
    }

    mFunctionTimerActive = true;
}

void BaseApplication::StartStatusLEDTimer()
{
    if (pdPASS != xTimerStart(sLightTimer, 0))
    {
        RENES_LOG("Light Time start failed");
        appError(APP_ERROR_START_TIMER_FAILED);
    }
}

void BaseApplication::StopStatusLEDTimer()
{
    if (xTimerStop(sLightTimer, 100) != pdPASS)
    {
        RENES_LOG("Light Time start failed");
        appError(APP_ERROR_START_TIMER_FAILED);
    }
}

#ifdef EMBER_AF_PLUGIN_IDENTIFY_SERVER
void BaseApplication::OnIdentifyStart(Identify * identify)
{
    ChipLogProgress(Zcl, "onIdentifyStart");

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
    StartStatusLEDTimer();
#endif
}

void BaseApplication::OnIdentifyStop(Identify * identify)
{
    ChipLogProgress(Zcl, "onIdentifyStop");

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
    StopStatusLEDTimer();
#endif
}

void BaseApplication::OnTriggerIdentifyEffectCompleted(chip::System::Layer * systemLayer, void * appState)
{
    ChipLogProgress(Zcl, "Trigger Identify Complete");
    sIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
    StopStatusLEDTimer();
#endif
}

void BaseApplication::OnTriggerIdentifyEffect(Identify * identify)
{
    sIdentifyEffect = identify->mCurrentEffectIdentifier;

    if (identify->mEffectVariant != Clusters::Identify::EffectVariantEnum::kDefault)
    {
        ChipLogDetail(AppServer, "Identify Effect Variant unsupported. Using default");
    }

#if CHIP_CONFIG_ENABLE_ICD_SERVER == 1
    StartStatusLEDTimer();
#endif

    switch (sIdentifyEffect)
    {
    case Clusters::Identify::EffectIdentifierEnum::kBlink:
    case Clusters::Identify::EffectIdentifierEnum::kOkay:
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(5), OnTriggerIdentifyEffectCompleted,
                                                           identify);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kBreathe:
    case Clusters::Identify::EffectIdentifierEnum::kChannelChange:
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(10), OnTriggerIdentifyEffectCompleted,
                                                           identify);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kFinishEffect:
        (void) chip::DeviceLayer::SystemLayer().CancelTimer(OnTriggerIdentifyEffectCompleted, identify);
        (void) chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Seconds16(1), OnTriggerIdentifyEffectCompleted,
                                                           identify);
        break;
    case Clusters::Identify::EffectIdentifierEnum::kStopEffect:
        (void) chip::DeviceLayer::SystemLayer().CancelTimer(OnTriggerIdentifyEffectCompleted, identify);
        break;
    default:
        sIdentifyEffect = Clusters::Identify::EffectIdentifierEnum::kStopEffect;
        ChipLogProgress(Zcl, "No identifier effect");
    }
}
#endif // EMBER_AF_PLUGIN_IDENTIFY_SERVER


void BaseApplication::LightTimerEventHandler(TimerHandle_t xTimer)
{
    LightEventHandler();
}

void BaseApplication::PostEvent(const AppEvent * aEvent)
{
    if (sAppEventQueue != NULL)
    {
        BaseType_t status;
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPrioTaskWoken = pdFALSE;
            status                         = xQueueSendFromISR(sAppEventQueue, aEvent, &higherPrioTaskWoken);

#ifdef portYIELD_FROM_ISR
            portYIELD_FROM_ISR(higherPrioTaskWoken);
#elif portEND_SWITCHING_ISR // portYIELD_FROM_ISR or portEND_SWITCHING_ISR
            portEND_SWITCHING_ISR(higherPrioTaskWoken);
#else                       // portYIELD_FROM_ISR or portEND_SWITCHING_ISR
#error "Must have portYIELD_FROM_ISR or portEND_SWITCHING_ISR"
#endif // portYIELD_FROM_ISR or portEND_SWITCHING_ISR
        }
        else
        {
            status = xQueueSend(sAppEventQueue, aEvent, 1);
        }

        if (!status)
        {
            RENES_LOG("Failed to post event to app task event queue");
        }
    }
    else
    {
        RENES_LOG("Event Queue is NULL should never happen");
    }
}

void BaseApplication::DispatchEvent(AppEvent * aEvent)
{
    if (aEvent->Handler)
    {
        aEvent->Handler(aEvent);
    }
    else
    {
        RENES_LOG("Event received with no handler. Dropping event.");
    }
}

void BaseApplication::ScheduleFactoryReset()
{
    PlatformMgr().ScheduleWork([](intptr_t) {
        PlatformMgr().HandleServerShuttingDown();
        ConfigurationMgr().InitiateFactoryReset();
    });
}

/*		move src/util/mattercallback.cpp
CHIP_ERROR MatterPreCommandReceivedCallback(const chip::app::ConcreteCommandPath & commandPath,
                                                                  const chip::Access::SubjectDescriptor & subjectDescriptor)
{
    return CHIP_NO_ERROR;
}

void MatterPostCommandReceivedCallback(const chip::app::ConcreteCommandPath & commandPath,
                                                             const chip::Access::SubjectDescriptor & subjectDescriptor)
{
    ClusterId clusterId = commandPath.mClusterId;
    CommandId commandId = commandPath.mCommandId;
    app_print_ext("+MCMD=%d,%d,%d", (uint32_t)clusterId, (uint32_t)commandId, 0);
}
*/

#if NotUsedForNow
void MatterPreAttributeWriteCallback(const chip::app::ConcreteAttributePath & attributePath)
{
}
void MatterPostAttributeWriteCallback(const chip::app::ConcreteAttributePath & attributePath)
{
}
void MatterPreAttributeReadCallback(const chip::app::ConcreteAttributePath & attributePath)
{
}
void MatterPostAttributeReadCallback(const chip::app::ConcreteAttributePath & attributePath)
{
}

chip::Protocols::InteractionModel::Status 
MatterPreAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                 uint8_t * value)
{
}
#endif

void BaseApplication::OnPlatformEvent(const ChipDeviceEvent * event, intptr_t)
{
    switch (event->Type)
    {
    case DeviceEventType::kServiceProvisioningChange:
        app_print_ext(MATTER_PRV_CHANGED);
        sIsProvisioned = event->ServiceProvisioningChange.IsServiceProvisioned;
        break;

    case DeviceEventType::kInternetConnectivityChange:
        if (event->InternetConnectivityChange.IPv4 == kConnectivity_Established)
        {
            app_print_ext(MATTER_WIFI_IP4EST);
            chip::app::DnssdServer::Instance().StartServer();
        }
        else if (event->InternetConnectivityChange.IPv4 == kConnectivity_Lost)
        {
            app_print_ext(MATTER_WIFI_IP4LST);
            ChipLogProgress(DeviceLayer, "Lost IPv4 connectivity...");
        }
        if (event->InternetConnectivityChange.IPv6 == kConnectivity_Established)
        {
            app_print_ext(MATTER_WIFI_IP6EST);    //[rrq61000 matter work]
            chip::app::DnssdServer::Instance().StartServer();
#if defined(__USE_MATTER_DPM_APP__)
            // After any boot (DPM wake, cold boot, power-loss recovery) where the device
            // is already commissioned, skip the 240-second ICD idle period and enter
            // ActiveMode immediately.  This sends a subscription keepalive report to the
            // controller right after WiFi reconnects, preventing the controller from
            // marking the device offline because the subscription MaxInterval (240s) is
            // exceeded during the DPM sleep + reboot + idle-mode wait period (~510s total).
            // OnPlatformEvent is called from within the CHIP event loop — the stack lock
            // is already held, so LockChipStack() must NOT be called here.
            if (Server::GetInstance().GetFabricTable().FabricCount() > 0)
            {
                RENES_LOG("[%s:%d][dpmwork] WiFi ready, commissioned — forcing immediate ICD ActiveMode for subscription refresh", __func__, __LINE__);
                ICDNotifier::GetInstance().NotifyNetworkActivityNotification();
            }
#endif
        }
        else if (event->InternetConnectivityChange.IPv6 == kConnectivity_Lost)
        {
            app_print_ext(MATTER_WIFI_IP6LST);
            ChipLogProgress(DeviceLayer, "Lost IPv6 connectivity...");
        }

        break;

    case DeviceEventType::kCHIPoBLEConnectionEstablished:
        app_print_ext(MATTER_BLE_EST);
        ChipLogProgress(DeviceLayer, "CHIPoBLE connection established");
        break;

    case DeviceEventType::kCHIPoBLEConnectionClosed:
        app_print_ext(MATTER_BLE_DST);
        ChipLogProgress(DeviceLayer, "CHIPoBLE disconnected");
        break;

    case DeviceEventType::kCommissioningComplete:
        app_ext_status_set(_Status_commissioning_mode_done);
        app_ext_status_set(_Status_STA_start);
        app_print_ext(MATTER_COMM_COMPLETE); 
        ChipLogProgress(DeviceLayer, "Commissioning complete");
        ble_deinit();
        break;

//matterwork[[::exceptions::checking KVS Map
    case DeviceEventType::kFailSafeTimerExpired:
        RENES_LOG("[%s:%s:%d] kFailSafeTimerExpired received ... checking map state", __FILENAME__, __func__, __LINE__);
        PersistedStorage::KeyValueStoreMgrImpl().SetExceptionEvent(PersistedStorage::ExceptionEventTypes::kEvtExcFailSafeTimerExpired);
        app_ext_status_set(_Status_network_Fail);
        break;
//]]matterwork

    default:
        RENES_LOG("[%s:%s:%d] event=%d", __FILENAME__, __func__, __LINE__, event->Type);
        break;
    }
}

