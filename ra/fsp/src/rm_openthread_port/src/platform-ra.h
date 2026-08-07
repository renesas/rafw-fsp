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

/**
 * @file
 * @brief
 *   This file includes the platform-specific functions and definitions.
 */

#ifndef PLATFORM_RA_H
#define PLATFORM_RA_H

#include "openthread-core-config.h"
#include "openthread/instance.h"
#include "hal_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"
#include "common/debug.hpp"

/* Flash characteristics definition*/
#define OT_SETTINGS_START_ADDRESS    OPENTHREAD_PORT_CFG_STORAGE_FLASH_SETTINGS_START_ADDRESS
#define FLASH_PAGE_SIZE              OPENTHREAD_PORT_CFG_STORAGE_FLASH_SWAP_PAGE_SIZE
#define FLASH_BLOCKS_PER_PAGE        (FLASH_PAGE_SIZE / BSP_FEATURE_FLASH_NVMC_W_BLOCK_SIZE)
#define FLASH_WORD_SIZE              OPENTHREAD_PORT_CFG_STORAGE_FLASH_MAX_EXPECTED_WORD_SIZE
#define FLASH_PAGE_NUM               OPENTHREAD_PORT_CFG_STORAGE_FLASH_SWAP_PAGE_NUM /* must be a multiple of 2 */
#define FLASH_SWAP_PAGE_NUM          (FLASH_PAGE_NUM / 2)
#define FLASH_SWAP_SIZE              (FLASH_PAGE_SIZE * FLASH_SWAP_PAGE_NUM)

/* Flash Macros definition */
#define ALIGN_TO_FLASH(x)    (((x) / FLASH_WORD_SIZE) * FLASH_WORD_SIZE)
#define NUM_OF_WORDS(x)      ((x) / FLASH_WORD_SIZE)

/* Radio Timing Macro definition */
#define CMAC_MAILBOX_WRITE_TIME_US    (46)  /* Determined experimentally */
#define RADIO_CLOCK_ACCURACY          (54)  /* Determined experimentally, ~53.5 */
#define PLATFORM_UNCERTAINTY          (6)   /* With HW timers - as a Radio clock. In tens of microseconds. */
#define CMAC_TX_RAMP_UP_US            (700) /* Delay added by CMAC during "normal" Tx, in microseconds. Determined experimentally. */

/* The frame sending delay for the SSED role using the Tx_At mechanism. Selected experimentally.
 * The value can be reduced after enabling the hardware security accelerator
 */
#define SSED_TX_AT_US                 (4000)

/**
 * Performs radio driver processing.
 *
 */
void otPlatUartProcess(void);

void otPlatAlarmInit(uint32_t aSpeedUpFactor);

void otPlatAlarmDeinit(void);

void otPlatAlarmProcess(otInstance * aInstance);

void otPlatRandomInit(void);

void otPlatRadioInit(void);

void otPlatRadioProcess(otInstance * aInstance);

void otPlatRadioProcessHighPriority(otInstance * aInstance);

typedef enum otPlatFatalErrorSource
{
    OT_PLAT_FATAL_ERROR_SOURCE_UNKNOWN = 0,
    OT_PLAT_FATAL_ERROR_SOURCE_CMAC,
    OT_PLAT_FATAL_ERROR_SOURCE_LAST
} otPlatFatalErrorSource;

void otPlatISRAssertSignal(otPlatFatalErrorSource source, const char * message, const char * file, int line);

/**
 * OT_SPECIAL_ASSERT is a special mode to move assert processing into main thread,
 * outside ISR it behaves like regular `OT_ASSERT(false);`
 *
 * Precautions:
 * 1. On failure it returns from current function and sets a pending fatal assert.
 * 2. Other ISR and main thread code will continue execution until the next call of `otPlatISRAssertsProcess()`.
 */
#define OT_SPECIAL_ASSERT(condition, source, message)                           \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            if (xPortIsInsideInterrupt() == pdTRUE)                             \
            {                                                                   \
                otPlatISRAssertSignal((source), (message), __FILE__, __LINE__); \
                return;                                                         \
            }                                                                   \
            (void) (source);                                                    \
            (void) (message);                                                   \
            OT_ASSERT(false);                                                   \
        }                                                                       \
    } while (0)

void otPlatISRAssertsProcess(void);

bool isOtPlatMicroAlarmEnabled(void);

void otPlatMicroAlarmEnable(void);

void otPlatMicroAlarmDisable(void);

void otPlatMicroAlarmTrigger(void);

bool isOtPlatMilliAlarmEnabled(void);

void otPlatMilliAlarmEnable(void);

void otPlatMilliAlarmDisable(void);

void otPlatMilliAlarmTrigger(void);

void otSysTimerSetMicroAlarm(uint32_t alarm_us_timestamp);

void otSysTimerSetMilliAlarm(uint32_t alarm_timestamp);

#endif                                 /* PLATFORM_RA_H */
