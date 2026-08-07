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
#include "openthread/platform/misc.h"
#include "openthread/platform/toolchain.h"

static otPlatResetReason sPlatResetReason = OT_PLAT_RESET_REASON_POWER_ON;
bool gPlatformPseudoResetWasRequested;
static otPlatMcuPowerState gPlatMcuPowerState = OT_PLAT_MCU_POWER_STATE_ON;

OT_TOOL_WEAK void otPlatReset (otInstance * aInstance)
{
    NVIC_SystemReset();
    OT_UNUSED_VARIABLE(aInstance);
}

#if OPENTHREAD_CONFIG_PLATFORM_BOOTLOADER_MODE_ENABLE
otError otPlatResetToBootloader (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return OT_ERROR_NOT_CAPABLE;
}

#endif

otPlatResetReason otPlatGetResetReason (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return sPlatResetReason;
}

void otPlatWakeHost (void)
{
    // TODO: implement an operation to wake the host from sleep state.
}

otError otPlatSetMcuPowerState (otInstance * aInstance, otPlatMcuPowerState aState)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    switch (aState)
    {
        case OT_PLAT_MCU_POWER_STATE_ON:
        case OT_PLAT_MCU_POWER_STATE_LOW_POWER:
        {
            gPlatMcuPowerState = aState;
            break;
        }

        default:
        {
            error = OT_ERROR_FAILED;
            break;
        }
    }

    return error;
}

otPlatMcuPowerState otPlatGetMcuPowerState (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return gPlatMcuPowerState;
}

__STATIC_INLINE fsp_err_t otPlatAssertMapToFSP (uint8_t value) {
    OT_UNUSED_VARIABLE(value);

    // This macro may return an error code
    // depending on the compile-time configuration
    FSP_ASSERT(value);

    // If no return or assertion from FSP_ASSERT, do nothing
    return FSP_SUCCESS;
}

void otPlatAssertFail (const char * aFilename, int aLineNumber)
{
#if OPENTHREAD_CONFIG_LOG_PLATFORM && OPENTHREAD_CONFIG_LOG_LEVEL < OT_LOG_LEVEL_CRIT
    OT_UNUSED_VARIABLE(aFilename);
    OT_UNUSED_VARIABLE(aLineNumber);
#else
    otLogCritPlat("assert failed at %s:%d", aFilename, aLineNumber);
#endif
    otPlatAssertMapToFSP(false);
}

#if OPENTHREAD_CONFIG_PLATFORM_LOG_CRASH_DUMP_ENABLE
otError otPlatLogCrashDump (void) {
    return OT_ERROR_NONE;
}

#endif
