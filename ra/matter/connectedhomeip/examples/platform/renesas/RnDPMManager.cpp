/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *    Copyright (c) 2022 Renesas Electronics.
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
#include "rm_matter_wifi_cfg.h"
#if defined(__USE_MATTER_DPM_APP__)
#include "RnDPMManager.h"
#include <lib/core/CHIPConfig.h>
#include <app/server/Server.h>
#include <app/InteractionModelEngine.h>
#include <app/icd/server/ICDNotifier.h>
#include <transport/SessionManager.h>
#include "rm_pmgr_api.h"
#include "rm_pmgr_w_instance.h"

namespace chip {
namespace app {

RnDPMManager* RnDPMManager::instance = nullptr;

RnDPMManager* RnDPMManager::getInstance()
{
    if(!instance){        
        instance = new RnDPMManager();
    }
    return instance;
}

RnDPMManager::RnDPMManager() {}

void RnDPMManager:: CheckingDPMEnter(RnDPMManager* RnDpmMgr)
{
#if defined(__USE_MATTER_DPM_APP__)
    // Block DPM entry until the device is commissioned.
    // gMatRtmPtr may carry stale exchange IDs / completion flags from the previous
    // DPM wake cycle (e.g. mLocalReportDataExID retained from before reset), which
    // can make GetDPMStatus() return true even on a fresh boot before any fabric
    // exists.  Entering DPM at that point would close the commissioning window.
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
    {
        RENES_LOG("[%s:%d] Not commissioned — DPM entry suppressed", __func__, __LINE__);
        return;
    }
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RENES_LOG("[%s:%d] RN DPM MANAGER", __func__, __LINE__);
        if (gMatRtmPtr)
        {
            if (RM_PMGR_W_dpm_job_name_is_set(const_cast<char*>(MATTER_JOB_NAME)))
            {
                RM_WIFI_dpm_udp_port_filter_set(CHIP_PORT);
                vTaskDelay(5);
                gMatRtmPtr->uc_cmd_done = 0;
                if(RM_PMGR_W_dpm_sleep_ready_set(const_cast<char*>(MATTER_JOB_NAME)) != DPM_SET_OK)
                {
                    RENES_LOG("[%s:%d] RM_PMGR_W_dpm_sleep_ready_set: Failure", __func__, __LINE__);
                }
                // remove constraint (RAM)
                if(RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED) != FSP_SUCCESS)
                {
                    RENES_LOG("[%s:%d] RM_PMGR_W_remove_sleep_constraint: Failure", __func__, __LINE__);
                }
            }
        }
        else
        {
            RENES_LOG("[%s:%d] Matter RTM not allocated", __func__, __LINE__);
        }
    }
#else
        RENES_LOG("[%s:%d] dpm enter !! but DPM_ENABLE is not define  !!! ", __func__, __LINE__);
#endif
}

bool RnDPMManager::GetDPMStatus(void)
{
    RENES_LOG("[%s:%d]mExchangeID: %d, mLocalReadReqExID: %d, mLocalSubscribeExID: %d, mLocalReportDataExID: %d",__func__, __LINE__,
                                                                        mExchangeID, mLocalReadReqExID, mLocalSubscribeExID, mLocalReportDataExID);
    RENES_LOG("[%s:%d]isReadReqComplete: %d, isSubscribeComplete: %d, isReportDataComplete: %d",__func__, __LINE__,
                                                                        isReadReqComplete, isSubscribeComplete, isReportDataComplete);

    return (isReadReqComplete || isSubscribeComplete || isReportDataComplete);
}

void RnDPMManager::HandleReadReq(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName)
{
    static uint32_t localExchangeID=0;
    static uint8_t msgStatus=0;
    static uint32_t localMsgCnt=0;

    size_t dirtyCount = chip::app::InteractionModelEngine::GetInstance()->GetNumDirtyHandlers();
    RENES_LOG("[%s:%d]msgStatus: %d, dirtyCount: %u",__func__, __LINE__, msgStatus, dirtyCount);
    RENES_LOG("[%s:%d]exchangeID:%d, localExchangeID:%ld, msgCnt:%ld, ackCnt:%ld, localMsgCnt:%ld, msgTypeName: %s ",__func__, __LINE__, 
                                                                         exchangeID, localExchangeID, msgCnt, ackCnt, localMsgCnt, msgTypeName);

    // Recover from a stuck tracker (e.g. previous exchange's session went defunct before StandaloneAck).
    if (msgStatus != DPM_STEP_0 && localExchangeID != exchangeID && !strcmp(msgTypeName, STR_READ_REQ)) {
        msgStatus = DPM_STEP_0;
        isReadReqComplete = false;
    }

    if(msgStatus == DPM_STEP_0)
    {
        if(!strcmp(msgTypeName, STR_READ_REQ)) {
            localExchangeID = exchangeID;
            mLocalReadReqExID = exchangeID;
            localMsgCnt = msgCnt;
            msgStatus = DPM_STEP_1;
        } else {
            msgStatus = DPM_STEP_0;
        }
        isReadReqComplete = false;
    }
    else if(msgStatus == DPM_STEP_1)
    {    
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_REPORT_DATA) &&(localMsgCnt == ackCnt)) {
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_2;
            } else {
                msgStatus = DPM_STEP_0;
            }
        }
        isReadReqComplete = false;
    }
    else if(msgStatus == DPM_STEP_2)
    {    
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_STANDALONE_ACK) && (localMsgCnt == ackCnt)) {
                msgStatus = DPM_STEP_0;
                RENES_LOG("[%s:%d]isReadReqComplete: true ",__func__, __LINE__);
                isReadReqComplete = true;
            }
        }
    }    
    else
    {
        msgStatus = DPM_STEP_0;
        isReadReqComplete = false;
    }

    if (dirtyCount) {
        isReadReqComplete = false;
    } 
 
}

void RnDPMManager::HandleSubscribeReq(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName)
{
    static uint32_t localExchangeID=0;
    static uint8_t msgStatus=0;    
    static uint32_t localMsgCnt=0;

    size_t dirtyCount = chip::app::InteractionModelEngine::GetInstance()->GetNumDirtyHandlers();

    // Recover from a stuck tracker (e.g. previous exchange's session went defunct before StandaloneAck).
    if (msgStatus != DPM_STEP_0 && localExchangeID != exchangeID && !strcmp(msgTypeName, STR_SUBSCRIBE_REQ)) {
        msgStatus = DPM_STEP_0;
        isSubscribeComplete = false;
    }

    if(msgStatus == DPM_STEP_0)
    {
        if(!strcmp(msgTypeName, STR_SUBSCRIBE_REQ)) {
            
            localExchangeID = exchangeID;
            mLocalSubscribeExID = exchangeID;
            localMsgCnt = msgCnt;
            msgStatus = DPM_STEP_1;
        } else {
            msgStatus = DPM_STEP_0;
        }
        isSubscribeComplete = false;
    }
    else if(msgStatus == DPM_STEP_1)
    {
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_REPORT_DATA) &&(localMsgCnt == ackCnt)) {
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_2;
            } 
            else 
            {
                msgStatus = DPM_STEP_0;
            }
        }
        isSubscribeComplete = false;
    }
    else if(msgStatus == DPM_STEP_2)
    {    
        if(localExchangeID == exchangeID) {        
            if(!strcmp(msgTypeName, STR_STATUS_RES) &&(localMsgCnt == ackCnt)) {
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_3;
            } else if(!strcmp(msgTypeName, STR_STANDALONE_ACK) && ((localMsgCnt-1) == ackCnt)){
                 msgStatus = DPM_STEP_2;  //Retransmission of STR_REPORT_DATA 
                
            } else {
                msgStatus = DPM_STEP_0;
            }
        }
        isSubscribeComplete = false;
    }
    else if(msgStatus == DPM_STEP_3) //
    {
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_SUBSCRIBE_RES) &&(localMsgCnt == ackCnt)) {
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_4;
            }
            else if(!strcmp(msgTypeName, STR_REPORT_DATA) &&(localMsgCnt == ackCnt))
            {
               localMsgCnt = msgCnt;
               msgStatus = DPM_STEP_2;
            }
        }
        isSubscribeComplete = false;    
    }    
    else if(msgStatus == DPM_STEP_4) //
    {    
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_STANDALONE_ACK) && (localMsgCnt == ackCnt)) {
                msgStatus = DPM_STEP_0;
                isSubscribeComplete = true;
                RENES_LOG("[%s:%d]isSubscribeComplete: true",__func__, __LINE__);
            }
            else 
            {
               msgStatus = DPM_STEP_0;
               isSubscribeComplete = false;
            }
        }
    }
    else
    {
        msgStatus = DPM_STEP_0;
        isSubscribeComplete = false;
    }

    if (dirtyCount) {
        isSubscribeComplete = false;
    }
}

void RnDPMManager::HandleReportData(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName)
{
    static uint32_t localExchangeID=0;
    static uint8_t msgStatus=0;    
    static uint32_t localMsgCnt=0;

    size_t dirtyCount = chip::app::InteractionModelEngine::GetInstance()->GetNumDirtyHandlers();

    // Recover from a stuck tracker (e.g. previous exchange's session went defunct before StandaloneAck).
    if (msgStatus != DPM_STEP_0 && localExchangeID != exchangeID && !strcmp(msgTypeName, STR_REPORT_DATA)) {
        msgStatus = DPM_STEP_0;
        isReportDataComplete = false;
    }

    if(msgStatus == DPM_STEP_0)
    {
        if(!strcmp(msgTypeName, STR_REPORT_DATA)) {
            
            localExchangeID = exchangeID;
            mLocalReportDataExID = exchangeID;
            localMsgCnt = msgCnt;
            msgStatus = DPM_STEP_1;
        } else {                
            msgStatus = DPM_STEP_0;
        }             
        isReportDataComplete = false;
    }
    else if(msgStatus == DPM_STEP_1)
    {    
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_STATUS_RES) &&(localMsgCnt == ackCnt)) {
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_2;
            } else if(!strcmp(msgTypeName, STR_STANDALONE_ACK) && ((localMsgCnt-1) == ackCnt)){
                 msgStatus = DPM_STEP_1;  //Retransmission of STR_REPORT_DATA of DPM_STEP_2
            } else {                
                msgStatus = DPM_STEP_0;
            }               
        }
        isReportDataComplete = false;
    }
    else if(msgStatus == DPM_STEP_2)
    {    
        if(localExchangeID == exchangeID) {
            if(!strcmp(msgTypeName, STR_STANDALONE_ACK) && (localMsgCnt == ackCnt)) {
                msgStatus = DPM_STEP_0;     
            
                RENES_LOG("[%s:%d]isReportDataComplete: true ",__func__, __LINE__);
                isReportDataComplete = true;     
                
            }
            else if(!strcmp(msgTypeName, STR_REPORT_DATA) &&(localMsgCnt == ackCnt))
            {
               //reportdata and count check !! -> status 1
               localMsgCnt = msgCnt;
               msgStatus = DPM_STEP_1;
               isReportDataComplete = false;
            }
        }
        else {
            if(!strcmp(msgTypeName, STR_REPORT_DATA)) {
                localExchangeID = exchangeID;
                mLocalReportDataExID = exchangeID;
                localMsgCnt = msgCnt;
                msgStatus = DPM_STEP_1;
            }
        }
    }
    else
    {
        msgStatus = DPM_STEP_0;     
        isReportDataComplete = false;
    }

    if (dirtyCount) {
        isReportDataComplete = false;
    } 

}

void RnDPMManager::SetDPMStatus(uint16_t exchangeID, const uint32_t msgCnt, const uint32_t ackCnt, const char* msgTypeName)
{
    mExchangeID = exchangeID;
    mMsgCnt = msgCnt;
    mAckCnt = ackCnt;
    mMsgTypeName = msgTypeName;

    HandleReadReq(exchangeID, msgCnt, ackCnt, msgTypeName);
    HandleSubscribeReq(exchangeID, msgCnt, ackCnt, msgTypeName);
    HandleReportData(exchangeID, msgCnt, ackCnt, msgTypeName);
}
  
} // namespace app
} // namespace chip

#endif
