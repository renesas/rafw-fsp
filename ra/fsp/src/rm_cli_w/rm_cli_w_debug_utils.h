/**
 ****************************************************************************************
 *
 * @file rm_cli_w_debug_utils.h
 *
 * @brief Debug utilities
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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


#ifndef RM_CLI_W_DEBUG_UTILS_H_
#define RM_CLI_W_DEBUG_UTILS_H_

#include <stdbool.h>
#include <stdlib.h>


#define _debug_printf(type, fmt, args ...) printf(PRE_##type fmt POST_##type, ##args)

#define PRE_PARAMETER     "\t\t"
#define POST_PARAMETER    "\r\n"

#define print_parameter(fmt, args ...) _debug_printf(PARAMETER, fmt, ## args)

#define PRE_CATEGORY      "\r\n\t"
#define POST_CATEGORY     "\r\n"

#define print_category(fmt, args ...) _debug_printf(CATEGORY, fmt, ## args)

#define PRE_COMMAND       "\r\n\r\n"
#define POST_COMMAND      "\r\n"

#define print_command(fmt, args ...) _debug_printf(COMMAND, fmt, ## args)

#define PRE_STATUS        "\r\n"
#define POST_STATUS       "\r\n\r\n"

#define print_status(fmt, args ...) _debug_printf(STATUS, fmt, ## args)

#define PRE_EVENT         "\r\n"
#define POST_EVENT        "\r\n"

#define print_event(fmt, args ...) _debug_printf(EVENT, fmt, ## args)

#define PRE_NO_FORMAT     ""
#define POST_NO_FORMAT    "\r\n"

#define print(fmt, args ...) _debug_printf(NO_FORMAT, fmt, ## args)

/**
 * Debug handler callback. Returns true if called successfully, otherwise false - help message
 * will be printed out.
 */
typedef bool (* debug_callback_t) (int argc, const char **argv);

/**
 * Debug handler struct
 */
typedef struct {
    char *command;
    char *help;
    debug_callback_t callback;
} debug_handler_t;

/**
 * Helper comparing second argv with debug handler commands and calling callback
 */
bool debug_handle_message(int argc, const char *argv[], const debug_handler_t *handlers);

void clear_function_trace(void);
void print_function_trace(void);
void function_trace(unsigned char *name, int param1, int param2, int param3);

#endif /* RM_CLI_W_DEBUG_UTILS_H_ */
