/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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
 *      Source implementation of an input / output stream for Renesas Device targets.
 */

#include <lib/shell/Engine.h>
#include <lib/shell/streamer.h>

#if __SUPPORT_MATTER_FSP_MODULE__
#else
#include "common_uart.h"
#endif
#include <stdio.h>
#include <string.h>
#include "rnDeviceWrapAPIs.h"

#if defined (ENABLE_CHIP_SHELL) // for matter_shell
extern QueueHandle_t shell_queue;
extern int shell_enable;
#endif //ENABLE_CHIP_SHELL // for matter_shell

namespace chip {
namespace Shell {
namespace {

int streamer_rnDevice_init(streamer_t * streamer)
{
    (void) streamer;

#if defined (ENABLE_CHIP_SHELL) // for matter_shell
    if (shell_queue == NULL) {
        // queue not initialized
        return -1;
    }
#endif //ENABLE_CHIP_SHELL // for matter_shell

    return 0;
}

ssize_t streamer_rnDevice_read(streamer_t * streamer, char * buffer, size_t length)
{
#if defined (ENABLE_CHIP_SHELL) // for matter_shell
    BaseType_t lineReceived = xQueueReceive(shell_queue, buffer, portMAX_DELAY);

    if (lineReceived == pdTRUE) {
        return length;
    }
#endif //ENABLE_CHIP_SHELL // for matter_shell

    return 0;
}


ssize_t streamer_rnDevice_write(streamer_t * streamer, const char * buffer, size_t length)
{
    (void) streamer;
#if defined (ENABLE_CHIP_SHELL) // for matter_shell
    shell_print("%s", buffer);
#endif //ENABLE_CHIP_SHELL // for matter_shell
    return length;
}

static streamer_t streamer_rnDevice = {
    .init_cb  = streamer_rnDevice_init,
    .read_cb  = streamer_rnDevice_read,
    .write_cb = streamer_rnDevice_write,
};
} // namespace

streamer_t * streamer_get(void)
{
    return &streamer_rnDevice;
}

} // namespace Shell
} // namespace chip
