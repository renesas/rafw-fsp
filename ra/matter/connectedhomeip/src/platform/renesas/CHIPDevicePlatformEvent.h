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
 *          Defines platform-specific event types and data for the Chip
 *          Device Layer on Renesas platforms using the Renesas SDK.
 */

#pragma once

#include <platform/CHIPDeviceEvent.h>
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
#include "wifi_rn_events.h"
#endif

namespace chip {
namespace DeviceLayer {

namespace DeviceEventType {

/**
 * Enumerates Renesas platform-specific event types that are visible to the application.
 */
enum PublicPlatformSpecificEventTypes
{
    /* None currently defined */
};

/**
 * Enumerates Renesas platform-specific event types that are internal to the Chip Device Layer.
 */
enum InternalPlatformSpecificEventTypes
{
    kWifiSystemEvent = kRange_InternalPlatformSpecific,
};

} // namespace DeviceEventType

/**
 * Represents platform-specific event information for Renesas platforms.
 */

struct ChipDevicePlatformEvent final
{
    union
    {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION
        struct
        {
            wifi_event_base_t eventBase;
            int32_t Id;
            union
            {
                wifi_generic_message_t genericMsgEvent;
                wifi_startup_ind_t startupEvent;
                wifi_connect_ind_t connectEvent;
                wifi_disconnect_ind_t disconnectEvent;
            } data;
        } WifiSystemEvent;
#endif
    };
};
}; // namespace DeviceLayer
} // namespace chip
