/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/* Copyright (C) 2026 Modified by Renesas Electronics Corporation and/or its affiliates */

#include "platform-ra.h"
#include "openthread/tasklet.h"
#include "openthread/platform/alarm-milli.h"
#include "openthread/platform/radio.h"
#include "openthread/platform/time.h"
#include "openthread/logging.h"
#include "r_timer_api.h"
#include "openthread-system.h"

#define RM_OT_USEC_PER_MSEC    (1000U)  /* Microseconds per millisecond, for timer conversions */

static uint32_t timer_overflows = 0;

static void timer_callback (timer_callback_args_t * callback_args)
{
    switch (callback_args->event)
    {
        case TIMER_EVENT_CYCLE_END:
        {
            if (isOtPlatMicroAlarmEnabled() == true)
            {
                otPlatMicroAlarmTrigger();
                otPlatMicroAlarmDisable();
                otSysEventSignalPending();
            }

            break;
        }

        case TIMER_EVENT_COMPARE_A:
        {
            if (isOtPlatMilliAlarmEnabled() == true)
            {
                otPlatMilliAlarmTrigger();
                otPlatMilliAlarmDisable();
                otSysEventSignalPending();
            }

            break;
        }

        case TIMER_EVENT_OVERFLOW:
        {
            timer_overflows = timer_overflows + 1;
            break;
        }

        default:
        {
            break;
        }
    }
}

static void otSysTimerInit (void)
{
    gp_openthread_port_timer_instance->p_api->open(gp_openthread_port_timer_instance->p_ctrl, 
        gp_openthread_port_timer_instance->p_cfg);
    gp_openthread_port_timer_instance->p_api->enable(gp_openthread_port_timer_instance->p_ctrl);
    gp_openthread_port_timer_instance->p_api->callbackSet(gp_openthread_port_timer_instance->p_ctrl, 
        timer_callback, NULL, NULL);
    gp_openthread_port_timer_instance->p_api->start(gp_openthread_port_timer_instance->p_ctrl);
}

uint64_t otPlatTimeGet (void)
{
    timer_status_t timer0_status;

    fsp_err_t err = gp_openthread_port_timer_instance->p_api->statusGet(gp_openthread_port_timer_instance->p_ctrl, 
                        &timer0_status);

    if(FSP_SUCCESS != err)
    {
        return 0;
    }

    return ((uint64_t) timer_overflows << 32) + timer0_status.counter;
}

void otSysTimerSetMilliAlarm (uint32_t alarm_timestamp)
{
    uint32_t diff = alarm_timestamp - (uint32_t)(otPlatTimeGet() / RM_OT_USEC_PER_MSEC);

    fsp_err_t err = gp_openthread_port_timer_instance->p_api->compareMatchSet(gp_openthread_port_timer_instance->p_ctrl,
                        (uint32_t)otPlatTimeGet() + diff * RM_OT_USEC_PER_MSEC, OPENTHREAD_PORT_CFG_PREC_TIMER_COMPARE_MATCH_CHANNEL);
        
    OT_ASSERT(FSP_SUCCESS == err);
}

void otSysTimerSetMicroAlarm (uint32_t alarm_us_timestamp)
{
    fsp_err_t err = gp_openthread_port_timer_instance->p_api->periodSet(gp_openthread_port_timer_instance->p_ctrl, 
                        alarm_us_timestamp);

    OT_ASSERT(FSP_SUCCESS == err);
}

void otSysInit (int aArgCount, char * aArgVector[])
{
    OT_UNUSED_VARIABLE(aArgCount);
    OT_UNUSED_VARIABLE(aArgVector);

    otSysTimerInit();
    otLogInfoPlat("App started");

    otPlatRadioInit();
    otPlatAlarmInit(0);

    otPlatRandomInit();
}

bool otSysPseudoResetWasRequested (void)
{
    return false;
}

void otSysDeinit (void)
{
}

void otSysProcessDrivers (otInstance * aInstance)
{
    otPlatISRAssertsProcess();
    otPlatRadioProcessHighPriority(aInstance);
    otPlatAlarmProcess(aInstance);
    otPlatRadioProcess(aInstance);
    otPlatUartProcess();
}
