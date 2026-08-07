/*
 *
 *    Copyright (c) 2023 Renesas Electronics Corporation and/or its  affiliates
 *    All rights reserved.
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

#include "rm_matter_wifi_cfg.h"
#if defined(__USE_MATTER_DPM_APP__)
#include "rnDeviceWrapAPIs.h"
#include <platform/CHIPDeviceConfig.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>
#include <system/SystemClock.h>

#define DPM_ENABLE

#define DPM_DELAY_SECONDS         5

#define STR_REPORT_DATA                  "ReportData"
#define STR_READ_REQ                     "ReadRequest"
#define STR_SUBSCRIBE_REQ                "SubscribeRequest"
#define STR_INVOKE_CMD_REQ               "InvokeCommandRequest"
#define STR_INVOKE_CMD_RES               "InvokeCommandResponse"
#define STR_STATUS_RES                   "StatusResponse"
#define STR_SUBSCRIBE_RES                "SubscribeResponse"
#define STR_STANDALONE_ACK               "StandaloneAck"


struct rtm_timer_info {
   uint32_t tid; ///< timer id
};


namespace chip {
namespace app {
class RnDPMManager
{
public:
    static RnDPMManager* getInstance();

    void CheckingDPMEnter(RnDPMManager* RnDpmMgr);
    bool GetDPMStatus(void);
    void HandleReadReq(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName);
    void HandleSubscribeReq(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName);
    void HandleReportData(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName);    
    void SetDPMStatus(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName);


 private:
    static const uint8_t DPM_STEP_0 = 0;
    static const uint8_t DPM_STEP_1 = 1;
    static const uint8_t DPM_STEP_2 = 2;
    static const uint8_t DPM_STEP_3 = 3;
    static const uint8_t DPM_STEP_4 = 4;

    RnDPMManager();
    static RnDPMManager* instance;
    
    bool isReportDataComplete = false;
    bool isReadReqComplete = false;
    bool isSubscribeComplete = false;

    uint16_t mExchangeID;
    uint32_t mMsgCnt;
    uint32_t mAckCnt;
    const char* mMsgTypeName;
    
    uint16_t mLocalReportDataExID;
    uint16_t mLocalReadReqExID;
    uint16_t mLocalSubscribeExID;

};

} // namespace app
} // namespace chip

#endif
