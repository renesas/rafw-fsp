/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
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
 *          Platform-specific configuration overrides for the CHIP BLE
 *          Layer on DA16600 platforms using the Renesas SDK.
 *
 */

#pragma once

// ==================== Platform Adaptations ====================

#ifdef __RRQ61400__
#else
#if CONFIG_NETWORK_LAYER_BLE
#warning "BLE is not supported."
#undef CONFIG_NETWORK_LAYER_BLE
#define CONFIG_NETWORK_LAYER_BLE 0
#endif // CONFIG_NETWORK_LAYER_BLE
#endif // __RRQ61400__

#define BLE_CONNECTION_OBJECT uint8_t
#define BLE_CONNECTION_UNINITIALIZED ((uint8_t) -1)
#define BLE_MAX_RECEIVE_WINDOW_SIZE 5

#define BLE_CONFIG_ERROR_MIN 6000000
#define BLE_CONFIG_ERROR_MAX 6000999

// ========== Platform-specific Configuration Overrides =========

/* none so far */
