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
#include "openthread/logging.h"
#include "openthread-system.h"
#include "openthread/platform/time.h"
#include "openthread/platform/alarm-milli.h"
#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
 #include "openthread/platform/alarm-micro.h"
#endif

static volatile bool is_ot_milli_triggered;

#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
static volatile bool is_ot_micro_triggered;
#endif

#define MS_PER_S                  (1000)
#define NS_PER_US                 (1000)
#define US_PER_MS                 (1000)
#define US_PER_S                  (1000000)

#define DEFAULT_TIMEOUT_IN_SEC    (10) // seconds

#define ALARM_TIMER_MS_2_TICKS(ms)        ((ms) / portTICK_PERIOD_MS)
#define ALARM_TIMER_TICKS_2_MS(ticks)     (((uint64_t) ticks) * portTICK_PERIOD_MS)

#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
 #define ALARM_TIMER_US_2_TICKS(us)       ((us) / US_PER_MS / portTICK_PERIOD_MS)
 #define ALARM_TIMER_TICKS_2_US(ticks)    (((uint64_t) ticks) * portTICK_PERIOD_MS * US_PER_MS)

static bool otPlatMicroAlarmEnabled = false;
#endif

static bool otPlatMilliAlarmEnabled = false;

void otPlatAlarmInit (uint32_t aSpeedUpFactor)
{
    OT_UNUSED_VARIABLE(aSpeedUpFactor);
}

void otPlatAlarmDeinit (void)
{
    otPlatAlarmMilliStop(NULL);
}

uint32_t otPlatAlarmMilliGetNow (void)
{
    return (uint32_t) (otPlatTimeGet() / 1000);
}

void otPlatAlarmMilliStartAt (otInstance * aInstance, uint32_t aT0, uint32_t aDt)
{
    OT_UNUSED_VARIABLE(aInstance);

    uint32_t now;
    uint32_t fire_time;
    now       = otPlatAlarmMilliGetNow();
    fire_time = aT0 + aDt;

    otLogDebgPlat("Starting milli alarm at %lu, duration %lu, now is: %lu", aT0, aDt, now);

    if ((aDt > 0) && (fire_time - now != 0))
    {
        otSysTimerSetMilliAlarm(aT0 + aDt);

        taskENTER_CRITICAL();
        otPlatMilliAlarmEnable();
        taskEXIT_CRITICAL();
    }
    else
    {
        taskENTER_CRITICAL();
        otPlatMilliAlarmTrigger();
        taskEXIT_CRITICAL();
        otSysEventSignalPending();
    }
}

void otPlatAlarmMilliStop (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    otPlatMilliAlarmDisable();
}

void otPlatMilliAlarmTrigger (void)
{
    is_ot_milli_triggered = true;
}

bool isOtPlatMilliAlarmEnabled (void)
{
    return otPlatMilliAlarmEnabled;
}

void otPlatMilliAlarmEnable (void)
{
    otPlatMilliAlarmEnabled = true;
}

void otPlatMilliAlarmDisable (void)
{
    otPlatMilliAlarmEnabled = false;
}

void otPlatAlarmProcess (otInstance * aInstance)
{
#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
    if (is_ot_micro_triggered)
    {
        taskENTER_CRITICAL();
        is_ot_micro_triggered = false;
        taskEXIT_CRITICAL();

        otPlatAlarmMicroFired(aInstance);
    }
#endif                                 /* OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE */

    if (is_ot_milli_triggered)
    {
        taskENTER_CRITICAL();
        is_ot_milli_triggered = false;
        taskEXIT_CRITICAL();
        otLogDebgPlat("Alarm fired at %lu", otPlatAlarmMilliGetNow());

        otPlatAlarmMilliFired(aInstance);
    }
}

#if OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE
void otPlatAlarmMicroStartAt (otInstance * aInstance, uint32_t aT0, uint32_t aDt)
{
    OT_UNUSED_VARIABLE(aInstance);

    if (aDt > 0)
    {
        otSysTimerSetMicroAlarm(aT0 + aDt);
        otLogDebgPlat("Starting micro alarm at %lu, duration %lu", aT0, aDt);
        taskENTER_CRITICAL();
        otPlatMicroAlarmEnable();
        taskEXIT_CRITICAL();
    }
    else
    {
        taskENTER_CRITICAL();
        otPlatMicroAlarmTrigger();
        otPlatMicroAlarmDisable();
        taskEXIT_CRITICAL();
        otSysEventSignalPending();
    }
}

void otPlatAlarmMicroStop (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    otPlatMicroAlarmEnabled = false;
}

void otPlatMicroAlarmTrigger (void)
{
    is_ot_micro_triggered = true;
}

bool isOtPlatMicroAlarmEnabled (void)
{
    return otPlatMicroAlarmEnabled;
}

void otPlatMicroAlarmEnable (void)
{
    otPlatMicroAlarmEnabled = true;
}

void otPlatMicroAlarmDisable (void)
{
    otPlatMicroAlarmEnabled = false;
}

uint32_t otPlatAlarmMicroGetNow (void)
{
    /* Returns platform time */
    return (uint32_t) otPlatTimeGet();
}

#endif                                 /* OPENTHREAD_CONFIG_PLATFORM_USEC_TIMER_ENABLE */
