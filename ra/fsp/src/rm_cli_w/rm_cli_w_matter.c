/**
 ****************************************************************************************
 *
 * @file rm_cli_w_matter.c
 *
 * @brief matter command functions
 *
 * Copyright (c) 2025 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */
#if defined (__MATTER_CLI_DEBUG__)
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w_matter.h"

extern bool send_alarm_event(int argc, const char **argv);
extern bool send_door_state_event(int argc, const char **argv);

static const debug_handler_t matter_handlers[] = {
    { "trigger_alarm",             "[Alarm Code]",          (debug_callback_t)send_alarm_event                },
    { "trigger_door_state",        "[Door state]",          (debug_callback_t)send_door_state_event           },
    { NULL },
};

bool matter_command(int argc, const char *argv[], void *user_data)
{
    FSP_PARAMETER_NOT_USED(user_data);

    return debug_handle_message(argc, argv, matter_handlers);
}
#endif /* __MATTER_CLI_DEBUG__ */
