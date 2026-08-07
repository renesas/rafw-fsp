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
 *          Platform-specific configuration overrides for the Chip Device Layer
 *          on Renesas platforms using the Renesas SDK.
 */

#pragma once

// ==================== Platform Adaptations ====================

#ifdef __RRQ61400__
#else
#if CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#warning "CHIP-over-BLE is not supported."
#undef CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#define CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE 0
#endif // CHIP_DEVICE_CONFIG_ENABLE_CHIPOBLE
#endif // __RRQ61400__

//matterwork[[::for BLE
#define CHIP_DEVICE_CONFIG_RENES_BLE_ERROR_MIN 0xC00000
//]]matterwork

#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_AP 0
#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_STATION 1

#define CHIP_DNSSD_DEFAULT_MINIMAL 1

// ========== Platform-specific Configuration =========

// These are configuration options that are unique to the Renesas platform.
// These can be overridden by the application as needed.

// ========== Platform-specific Configuration Overrides =========

#ifndef CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY (configTIMER_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY (CHIP_DEVICE_CONFIG_BLE_LL_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_PRIORITY
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_PRIORITY (CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY - 1)
#endif // CHIP_DEVICE_CONFIG_BLE_STACK_TASK_PRIORITY

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE 1536
#endif // CHIP_DEVICE_CONFIG_BLE_APP_TASK_STACK_SIZE

#ifndef CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE
//orig. SDK[[::
//#define CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE (6 * 1024)
//matterwork[[::stack overflow
#define CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE (8 * 1024)
//]]matterwork
#endif // CHIP_DEVICE_CONFIG_CHIP_TASK_STACK_SIZE

#ifndef CHIP_DEVICE_CONFIG_SED_ACTIVE_INTERVAL
#define CHIP_DEVICE_CONFIG_SED_ACTIVE_INTERVAL chip::System::Clock::Milliseconds32(200)
#endif // CHIP_DEVICE_CONFIG_SED_ACTIVE_INTERVAL

#ifndef CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE
#define CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE (8 * 1024) // (2 * 1024)
#endif // CHIP_DEVICE_CONFIG_THREAD_TASK_STACK_SIZE

#define CHIP_DEVICE_CONFIG_ENABLE_WIFI_TELEMETRY 0
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_TELEMETRY 0
#define CHIP_DEVICE_CONFIG_ENABLE_THREAD_TELEMETRY_FULL 0

#ifndef CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME
#define CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME "Bluetooth event handler"
#endif // CHIP_DEVICE_CONFIG_BLE_APP_TASK_NAME

#define CHIP_DEVICE_CONFIG_MAX_EVENT_QUEUE_SIZE 50
