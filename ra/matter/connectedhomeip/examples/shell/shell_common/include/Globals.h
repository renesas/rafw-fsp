/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#pragma once

#include <credentials/FabricTable.h>
#include <lib/core/CHIPCore.h>
#include <lib/support/TestPersistentStorageDelegate.h>
#include <messaging/ExchangeMgr.h>
#include <protocols/secure_channel/MessageCounterManager.h>
#include <transport/SessionHolder.h>
#include <transport/SessionManager.h>
#if INET_CONFIG_ENABLE_TCP_ENDPOINT
#include <transport/raw/TCP.h>
#endif // INET_CONFIG_ENABLE_TCP_ENDPOINT
#include <transport/raw/UDP.h>
#include <unordered_map>

#if defined(__USE_MATTER_DPM_APP__)
extern std::unordered_map<chip::NodeId, chip::CATValues> gCATsTable;
const uint8_t MAX_CATS_INDEX = 4;
#endif
