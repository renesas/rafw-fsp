/**
 ****************************************************************************************
 *
 * @file rm_cli_w_debug_utils.c
 *
 * @brief Debug utilities
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "osal.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w.h"
#include "rm_cli_w_utils.h"

#ifdef CHECK_MAX_PARAMETER
/*
 * If counts maximum number of parameters of the command by checking how many spaces are included
 * in /p param_str.
 */
static uint8_t get_max_num_p(const char *param_str)
{
    int i;
    uint8_t param_num = 0;

    /* If there is no parameters then return 0 immediately */
    if (strcmp(param_str, "") == 0) {
        return param_num;
    }

    for (i = 0; i < strlen(param_str); i++) {
        if (param_str[i] == ' ') {
            param_num += 1;
        }
    }

    return param_num + 1;   // need to add 1 because there is always one empty space less than
    // number of parameters
}
#endif	// CHECK_MAX_PARAMETER

static void print_debug_handler_help(const char *category, const debug_handler_t *debug_handler)
{
    (void) category;
    print("Usage: %s %s", debug_handler->command, debug_handler->help);
}

bool debug_handle_message(int argc, const char *argv[], const debug_handler_t *handlers)
{
    const debug_handler_t *handler;

    if (strcmp(argv[0], "ps") == 0) {
        show_task_list();

        return true;
    }

    if (!(strcmp(argv[0], "help"))) {

        if (current_cmd_group->name && argc == 1) {
            print("\n%s commands:", current_cmd_group->name);
        }

        for (handler = handlers; handler && handler->command; handler++) {

            if (strcasecmp(handler->command, "set_mc") == 0 ||
                strcasecmp(handler->command, "tcpd")   == 0 ||
                strcasecmp(handler->command, "udpd")   == 0) {
                continue;
            }

            if (argc == 2 && !(strcmp(argv[1], handler->command))) {
                printf("\n");
                print_debug_handler_help(argv[0], handler);
                return true;
            } else if (argc == 1) {
                if (handler->command != NULL && handler->command[0] != '\0' && handler->callback != NULL) {
                    print("\t %-15s : %s", handler->command, handler->help != NULL ? handler->help:"");
                } else {
                    printf("\n");
                }
            }
        }
        return true;
    }

    for (handler = handlers; handler && handler->command; handler++) {
        if (strcmp(handler->command, argv[0]) != 0) {
            continue;
        }

        printf("\n");

        /*
         * Maximum number of the command parameters including name of the command
         */
#ifdef CHECK_MAX_PARAMETER
        uint8_t cmd_max_num_p = get_max_num_p(handler->help) + 2;

        if ((argc > cmd_max_num_p) || (!handler->callback(argc, argv)))
#else
        if (!handler->callback(argc, argv))
#endif	// CHECK_MAX_PARAMETER
        {
        	if (strcmp(argv[0], "time")) { // add exception, because there is help description in time command
        		print_debug_handler_help(argv[0], handler);
        	}
        }
        return true;
    }

    return false;
}

#if (dg_configUSE_TRACE_FOR_DEBUG == 1)

#define MAX_FUNC_TRACE	30
#define FUNC_NAME_SIZE	24
#define START_FUNCTION_TRACE_FLAG	0xDEADBEEF

typedef struct {	// size: 40
	unsigned char	name[FUNC_NAME_SIZE];
	unsigned int	tick;
	unsigned int	param1;
	unsigned int	param2;
	unsigned int	param3;
} function_info_t;

typedef struct {	// size: 1216
    unsigned int    start_function_trace;
    unsigned int	function_index;
    unsigned int	function_trace_count;
    unsigned int	dummy;
	function_info_t trace_debug_table[MAX_FUNC_TRACE];
} trace_info_t;

__attribute__((section("trace_debug_info"))) volatile trace_info_t trace_debug;
trace_info_t *trace_info_ptr = &trace_debug;
void function_trace(unsigned char *name, int param1, int param2, int param3)
{
	function_info_t	*trace_debug_table_ptr;

	trace_info_ptr->start_function_trace = START_FUNCTION_TRACE_FLAG;
	trace_info_ptr->function_trace_count++;

    if (trace_info_ptr->function_index == MAX_FUNC_TRACE) {
		trace_info_ptr->function_index = 0;
    }

	trace_debug_table_ptr = &trace_info_ptr->trace_debug_table[trace_info_ptr->function_index];

	strncpy(trace_debug_table_ptr->name, name, FUNC_NAME_SIZE-1);
	trace_debug_table_ptr->name[FUNC_NAME_SIZE-1] = 0;

	trace_debug_table_ptr->tick = xTaskGetTickCount();
	trace_debug_table_ptr->param1 = param1;
	trace_debug_table_ptr->param2 = param2;
	trace_debug_table_ptr->param3 = param3;

	trace_info_ptr->function_index++;
}

void clear_function_trace()
{
    memset(trace_info_ptr, 0x00, sizeof(trace_info_t));
}

void print_function_trace()
{
	int	i;
	int	index;
	int	count;
	function_info_t	*trace_debug_table_ptr;

	if (trace_info_ptr->start_function_trace != START_FUNCTION_TRACE_FLAG) {
		return;
	}

    printf(" <<<Function trace>>> \n");
    //printf(" start_function_trace:0x%X \n", trace_info_ptr->start_function_trace);
    printf(" function_index:%d \n", trace_info_ptr->function_index);	// next
    printf(" function_trace_count:%d \n", trace_info_ptr->function_trace_count);

	if ( trace_info_ptr->function_trace_count < MAX_FUNC_TRACE) {
		count = trace_info_ptr->function_trace_count;
	}
	else {
		count = MAX_FUNC_TRACE;
	}

	if (trace_info_ptr->function_index == 0) {
		index = MAX_FUNC_TRACE - 1;
	}
	else {
		index = trace_info_ptr->function_index - 1;
	}

	for(i=0; i<count; i++) {
		trace_debug_table_ptr = &trace_info_ptr->trace_debug_table[index];
    	printf(" %3d) %3d : %s(tick:%d, p1:0x%X, p2:0x%X, p3:0x%X) \n",
										i, index, 
										&trace_debug_table_ptr->name, 
										trace_debug_table_ptr->tick, 
										trace_debug_table_ptr->param1, 
										trace_debug_table_ptr->param2, 
										trace_debug_table_ptr->param3);
		index--;
		if (index < 0) {
			index = MAX_FUNC_TRACE - 1;
		}
	}
	vTaskDelay(portCONVERT_MS_2_TICKS(100));
}
#else	// dg_configUSE_TRACE_FOR_DEBUG
void function_trace(unsigned char *name, int param1, int param2, int param3)
{
    (void) name;
    (void) param1;
    (void) param2;
    (void) param3;
}
void clear_function_trace()
{
}
void print_function_trace()
{
}
#endif  // dg_configUSE_TRACE_FOR_DEBUG

/* EOF */