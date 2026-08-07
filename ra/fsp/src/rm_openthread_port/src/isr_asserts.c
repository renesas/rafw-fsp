/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "platform-ra.h"
#include "openthread-system.h"
#include "openthread/logging.h"

typedef struct isrAssertContext_s
{
    bool pending;
    otPlatFatalErrorSource source;
    const char           * file;
    int          line;
    const char * message;
} isrAssertContext_t;

static isrAssertContext_t isrAssertContext =
{
    .pending = false,
    .source  = OT_PLAT_FATAL_ERROR_SOURCE_UNKNOWN,
    .file    = NULL,
    .line    = 0,
    .message = NULL
};

static const char * const assertSourceNames[] =
{
    "Unknown",
    "CMAC",
};

void otPlatISRAssertSignal (otPlatFatalErrorSource source, const char * message, const char * file, int line)
{
    assert(xPortIsInsideInterrupt());

    /* Prevent assert info overwrite from another ISR. */
    if (!isrAssertContext.pending)
    {
        isrAssertContext.pending = true;
        isrAssertContext.source  = source;
        isrAssertContext.message = message;
        isrAssertContext.file    = file;
        isrAssertContext.line    = line;
    }

    otSysEventSignalPending();
}

void otPlatISRAssertsProcess (void)
{
    if (isrAssertContext.pending)
    {
        if ((uint32_t) isrAssertContext.source >= OT_PLAT_FATAL_ERROR_SOURCE_LAST)
        {
            isrAssertContext.source = OT_PLAT_FATAL_ERROR_SOURCE_UNKNOWN;
        }

        isrAssertContext.pending = false;
        otLogCritPlat("Fatal ISR assert source: %s, %s:%d, %s", assertSourceNames[(uint32_t) isrAssertContext.source],
                      (isrAssertContext.file != NULL) ? isrAssertContext.file : "unknown", isrAssertContext.line,
                      (isrAssertContext.message != NULL) ? isrAssertContext.message : "no details");
        OT_ASSERT(false);
    }
}
