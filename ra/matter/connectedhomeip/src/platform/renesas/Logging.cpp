/* See Project CHIP LICENSE file for licensing information. */
/*	 Copyright (c) 2023 Modified by Renesas Electronics Corporation
*/
#include <platform/logging/LogV.h>

#include <lib/core/CHIPConfig.h>
#include <platform/CHIPDeviceConfig.h>

#include <lib/support/SafeString.h>
#include <lib/support/logging/CHIPLogging.h>
#include <lib/support/CHIPMem.h>
#include <system/SystemClock.h>

#include "AppConfig.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <stdio.h>
#include <string.h>
#include <task.h>

//matterwork[[::
#include "rnDeviceWrapAPIs.h"
extern "C" int ra6w1_vsnprintf(char *buf, size_t n, int linefeed, const char *fmt, va_list args);
//]]matterwork

namespace chip {
namespace Logging {
namespace Platform {

/**
 * CHIP log output functions.
 */
#define LOG_BUF_SIZE 1024
void LogV(const char * module, uint8_t category, const char * msg, va_list args)
{
    char *szBuf;

    if (!IsCategoryEnabled(category))
    {
        return;
    }

    {
        szBuf = static_cast<char *>(chip::Platform::MemoryCalloc(LOG_BUF_SIZE, sizeof(char)));
#if __SUPPORT_MATTER_FSP_MODULE__
       vsnprintf(szBuf, LOG_BUF_SIZE, (const char*)msg, args);
#else
        ra6w1_vsnprintf(szBuf, LOG_BUF_SIZE, 0, (const char*)msg, args);
#endif
        if (strlen(szBuf) >= LOG_BUF_SIZE) {
            szBuf[LOG_BUF_SIZE - 1] = '\0';
        }
        CHIP_LOG("%s: %s", module, szBuf);
        chip::Platform::MemoryFree(szBuf);
    }
}
} // namespace Platform
} // namespace Logging
} // namespace chip

