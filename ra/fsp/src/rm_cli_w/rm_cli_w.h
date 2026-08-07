/**
 *
 * \brief Command Line Interface
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file rm_cli_w.h
 *
 * @brief Declarations for CLI service
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

#ifndef RM_CLI_W_H_
#define RM_CLI_W_H_

#include <osal.h>
#include <stdint.h>
#ifdef RM_STDIO_W
#include "rm_stdio_w_cfg.h"   
#endif
#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief CLI task priority
 */
#ifndef CLI_TASK_PRIORITY
#define CLI_TASK_PRIORITY       (OS_TASK_PRIORITY_USER)
#endif

#define CLI_TASK_EVENT                   BIT0
#define EASY_SETUP_TASK_EVENT_FINISH     BIT1

/**
 * CLI instance
 */
typedef void *cli_t;

/**
 * Command handler
 *
 * \p argv[0] is always a command name
 *
 * \param [in] argc       number of arguments
 * \param [in] argv       array of arguments
 * \param [in] user_data  user data passed from command
 */
typedef bool (* cli_handler_t) (int argc, const char *argv[], void *user_data);

/**
 * Command definition
 */
typedef struct {
    const char *name;       /**< command name (i.e. \p argv[0]) */
    cli_handler_t handler;  /**< command handler */
    void *user_data;        /**< user data passed to command handler */
} cli_command_t;

#if !dg_configUSE_CLI_STUBS

/**
 * Start CLI module
*/
void cli_open(void);

/**
 * Initialize CLI
 *
 * This function initializes CLI internal structures and shall be called before CLI is used and
 * after \p console_init().
 *
 */
void cli_init(void);

/**
 * Register command handlers for current task
 *
 * This functions registers command handlers to be evaluated for matching CLI input.
 *
 * Once full line of text is entered, CLI task notifies registered task using \p notif_mask and task
 * shall then call cli_handle_notified() to process entry.
 *
 * \warning
 * CLI stores only pointer to \p cmd_handler and application should guarantee that this pointer
 * is valid for entire adapter lifetime.
 *
 * \note
 * Only one task can register handlers in current implementation.
 *
 * \param [in] notif_mask       bit mask for task notification
 * \param [in] cmd_handler      predefined commands handlers
 * \param [in] def_handler      default command handler
 *
 * \return CLI instance
 *
 * \sa cli_handle_notified
 *
 */
cli_t cli_register(uint32_t notif_mask, const cli_command_t cmd_handler[],
                   cli_handler_t def_handler);

/**
 * Handle notification from CLI
 *
 * This function shall be called when application task is notified from CLI task to process pending
 * entry.
 *
 * Entry received from adapter will be split into tokens internally and matched against handlers
 * provided on registration.
 *
 * \param [in] cli              CLI instance
 *
 * \sa cli_register
 *
 */
void cli_handle_notified(cli_t cli);


#else /* !dg_configUSE_CLI_STUBS */

__STATIC_INLINE void cli_init(void)
{

}

__STATIC_INLINE cli_t cli_register(uint32_t notif_mask, const cli_command_t cmd_handler[],
                                   cli_handler_t def_handler)
{
    return NULL;
}

__STATIC_INLINE void cli_handle_notified(cli_t cli)
{

}

#endif /* !dg_configUSE_CLI_STUBS */

extern TaskHandle_t cli_handle_task;
extern const cli_command_t *current_cmd_group;
void print_prompt(void);

/**
 ****************************************************************************************
 * @brief      Hexa dump the data on console for cli uses
 * @param[in]  data      Dump address
 * @param[in]  length    Dump length
 * @param[in]  endian    Endian type
 * @return     None
 ****************************************************************************************
 */
#define OUTPUT_ASCII_ONLY    0
#define OUTPUT_HEXA_ONLY     1
#define OUTPUT_HEXA_ASCII    2
void hex_dump_cli(unsigned char *title, const void *buf, size_t len, char output_fmt);

#if defined (__SUPPORT_PRODTEST_CONSOLE__)
typedef bool (*prod_test_clicmd_t)(int argc, const char *argv[], void *user_data);
void prod_test_cli_register(void *cb);
#define prod_test_help_str "prod-help"
#endif

void print_prompt_wake(void);
void app_request_console_input_access(void);
void app_release_console_input_access(void);
int getStr(char *get_data, int get_len);
#ifdef __cplusplus
}
#endif

#endif /* CLI_H_ */

/**
 * \}
 */
