
/**
 ****************************************************************************************
 *
 * @file rm_cli_w.c
 *
 * @brief CLI service for the Renesas RA6W1/RA6W2 platform
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

#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>
#include "rm_cli_w.h"
#include "rm_cli_w_utils.h"
#include "common_def.h"
#include "r_rtc_w.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif
#include "SEGGER_RTT.h"

#define LINEBUF_SIZE    192
#define APPBUF_SIZE     (1024)
#define ARGC_MAX        32
#define QUEUE_LEN        1

/* DOS_KEY */
#define MAX_HIS_NUM             3
static char hist_buf[MAX_HIS_NUM][LINEBUF_SIZE + 1];
static int hist_cnt = -1;
#ifdef CONFIG_RETARGET
static char pre_key1 = 0;
#endif
static char pre_key2 = 0;
static char pre_key3 = 0;
static char cur_hist_idx = -1;

typedef struct {
    TaskHandle_t task;
    uint32_t notif_mask;
    QueueHandle_t queue;

    const cli_command_t *commands;
    cli_handler_t def_handler;
} client_t;

/* cli task handle */
TaskHandle_t cli_handle_task;

/* Registered client data */
static client_t reg_client;

/* Current command line buffer */
static struct {
    char *buf;
    size_t len;
} line;

#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ )
/* Buffer for app */
static struct {
    char *buf;
    size_t len;
} app_buff;

static atomic_int app_console_access = 0;
#endif  // __SUPPORT_APP_CONSOLE_INPUT__

const cli_command_t *current_cmd_group = NULL;

void print_prompt(void)
{
    printf("\n["PROMPT"%s%s] # ",
        current_cmd_group?"/":"",
        current_cmd_group?current_cmd_group->name:"");
}

void print_prompt_wake(void)
{
    printf("\n[W."PROMPT"%s%s] # ",
        current_cmd_group?"/":"",
        current_cmd_group?current_cmd_group->name:"");
}

void hex_dump_cli(unsigned char *title, const void *buf, size_t len, char output_fmt)
{
    size_t i, llen;
    const unsigned char *pos = buf;
    const size_t line_len = 16;
    int hex_index = 0;
    char *buf_prt = NULL;

    buf_prt = pvPortMalloc(64);
    if (buf_prt == NULL) {
        printf("[%s] Failed to allocate the temporary buffer ...\n", __func__);
        return;
    }

    if (output_fmt) {
        printf(">>> %s \n", title);
    }

    if (buf == NULL) {
        printf(" - hexdump%s(len=%lu): [NULL]\n",
            output_fmt == OUTPUT_HEXA_ONLY ? "":"_ascii", (unsigned long) len);

        vPortFree(buf_prt);

        return;
    }

    if (output_fmt) {
        printf("- (len=%lu):\n", (unsigned long) len);
    }

    while (len) {
        char tmp_str[4];

        llen = len > line_len ? line_len : len;

        memset(buf_prt, 0, 64);

        if (output_fmt) {
            sprintf(buf_prt, "[%08x] ", hex_index);

            for (i = 0; i < llen; i++) {
                sprintf(tmp_str, " %02x", pos[i]);
                strcat(buf_prt, tmp_str);
            }

            hex_index = hex_index + i;

            for (i = llen; i < line_len; i++) {
                strcat(buf_prt, "   ");  /* _xx */
            }

            printf("%s  ", buf_prt);

            memset(buf_prt, 0, 64);
        }

        if (output_fmt == OUTPUT_HEXA_ASCII || output_fmt == OUTPUT_ASCII_ONLY) {
            for (i = 0; i < llen; i++) {
                if (   (pos[i] >= 0x20 && pos[i] < 0x7f)
                    || (output_fmt == OUTPUT_ASCII_ONLY && (pos[i]  == 0x0d
                    || pos[i]  == 0x0a
                    || pos[i]  == 0x0c))) {

                    sprintf(tmp_str, "%c", pos[i]);
                    strcat(buf_prt, tmp_str);
                } else if (output_fmt) {
                    strcat(buf_prt, ".");
                }
            }
        }

        if (output_fmt) {
            for (i = llen; i < line_len; i++) {
                strcat(buf_prt, " ");
            }

            strcat(buf_prt, "\n");
        }

        printf("%s", buf_prt);

        pos += llen;
        len -= llen;
    }

    vPortFree(buf_prt);
}

static void notify_client(void)
{
    line.buf[line.len] = '\0';

    if (xQueueSendToBack(reg_client.queue, &line.buf, 0) != pdTRUE) {
        vPortFree(line.buf);
    } else {
        xTaskNotify(reg_client.task, reg_client.notif_mask, eSetBits);
    }

    line.buf = pvPortMalloc(LINEBUF_SIZE + 1);
    memset(line.buf, 0, LINEBUF_SIZE + 1);
    line.len = 0;
}
#ifdef CONFIG_RETARGET
static bool doskey_check(void)
{
    if (pre_key3 == 0x1B && pre_key2 == 0x5B) { /* 'esc[' */
        if (pre_key1 == 0x41 || pre_key1 == 0x42) { /* Check A or B */
            if (pre_key1 == 0x41) { /* UP KEY : esc[A */
                //printf("\x1b[B"); /* DOWN key : esc[B */
            }

            if (hist_cnt >= 0) {
                int hist_idx = 0;

                /* UP Key */
                if (pre_key1 == 0x41) {
                    if (hist_cnt > cur_hist_idx) {
                        cur_hist_idx++;
                    } else {
                        cur_hist_idx = 0;
                    }
                }
                /* Down Key */
                else if (pre_key1 == 0x42) {
                    if (0 < cur_hist_idx) {
                        cur_hist_idx--;
                    } else {
                        cur_hist_idx = (char)hist_cnt;
                    }
                }

                hist_idx = (MAX_HIS_NUM + hist_cnt - cur_hist_idx) % MAX_HIS_NUM;

                bsp_safe_strcpy(line.buf, hist_buf[hist_idx], LINEBUF_SIZE + 1);

                /* Delete right before character */
                while (line.len > 0) {
                    line.len--;
                    putchar('\b'); /* backspace */
                }
                /* Delete next line of current cursor position */
                printf("\x1b[0K"); /* esc[0K */

                printf("%s", line.buf);
                line.len = strlen(line.buf);
            }
        }
        return true;
    } else {
        return false;
    }
}
#endif

#ifdef __SUPPORT_APP_CONSOLE_INPUT__
#define RET_QUIT        -99

/**
 * Gets characters from the input using getchar() until '\r' or '\n' are encountered,
 * but no more than 'get_len' characters, and writes to provided get_data buffer.
 * get_data size should be at least (get_len + 1) to acocmodate terminating \0 character.
 * retruns the number of characters written to get_data excluding the terminating \0 character,
 * or RET_QUIT if Ctrl-C was encountered.
 */
int getStr(char *get_data, int get_len)
{
    int i = 0;
    unsigned char ch = 0;
    unsigned char previous_ch = 0;

    memset(get_data, 0, get_len);
    TickType_t old = xTaskGetTickCount();

    do {
        ch = (char)getchar();

        // LF('\n') discarding among CR+LF received from cli_task_func.
        if (previous_ch == 0x00 && ch == 0x0A && ((xTaskGetTickCount() - old) < portCONVERT_MS_2_TICKS(50))) {
            continue;
        } else if (previous_ch == 0x0D && ch == 0x0A) { /* 10 Ctrl-J Line Feed  '\n' */
            continue;
        } else if (ch == 0x0D) { /* 13 Ctrl-M Carriage Return '\r' */
            previous_ch = ch;
            ch = 0x0A;
        } else {
            previous_ch = ch;
        }
       
        if (ch == 0x03) {   /* 03 Ctrl-C */
            /* Move the cursor left & Erase from the cursor to the end of the line */
            printf("\33[D\33[0K\n");
            return RET_QUIT;
        } else if (   ch <= 0x1F // Echo off when Control character input
                   && ch != 0x08               // 08 Ctrl-H Backspace
                   && ch != 0x0A               // 10 Ctrl-J Line Feed  '\n'
                   && ch != 0x0D               // 13 Ctrl-M Carriage Return '\r'
                   && ch != 0x1B               // 27 Ctrl-[ ESC
                  ) {
            continue;
        }

        putchar(ch); // local echo

        if (ch == '\b') {   /* backspace */
            if (i > 0) {
                get_data[--i] = '\0';
                printf("\33[0K");
            } else {
                printf("\33[C"); /* Move the cursor right */
            }

            continue;
        }

        get_data[i++] = ch;

    } while (i < get_len && get_data[i - 1] != '\n' && get_data[i - 1] != '\r');

    if (get_len <= i) {
        printf("\n");
    }
    
    if ((get_data[i - 1] == '\n') || (get_data[i - 1] == '\r')) {
        get_data[i - 1] = '\0';
        return i - 1;
    } else {
        get_data[i] = '\0';
        return i;
    }
}
#endif /* __SUPPORT_APP_CONSOLE_INPUT__ */

static void doskey_cmd_add(char *ptr, int len)
{
    int idx;
    /* Append to History if different from previous command. */
    if (strncmp(hist_buf[hist_cnt], ptr, len) != 0) {
        for (idx = 0; idx < (MAX_HIS_NUM-1); idx++) {
            bsp_safe_strcpy(hist_buf[idx], hist_buf[idx + 1], LINEBUF_SIZE + 1);
        }
        bsp_safe_strcpy(hist_buf[MAX_HIS_NUM - 1], ptr, LINEBUF_SIZE + 1);

        if (hist_cnt + 1 >= MAX_HIS_NUM - 1) {
            hist_cnt = MAX_HIS_NUM - 1;
        } else {
            hist_cnt = hist_cnt + 1;
        }
    }
}

#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ )
int getchar_nowait(void)
{
    if (app_buff.len > 0)
    {
        char ch = app_buff.buf[0];
        for (size_t i = 1; i < app_buff.len; i++)
        {
            app_buff.buf[i - 1] = app_buff.buf[i];
        }
        app_buff.len--;
        return (int)ch;
    }
    return -1;
}

void app_request_console_input_access(void)
{
    atomic_store(&app_console_access, 1);
}

void app_release_console_input_access(void)
{
    atomic_store(&app_console_access, 0);
}

#endif /* __SUPPORT_APP_CONSOLE_INPUT__ */

#if defined (__SUPPORT_PRODTEST_CONSOLE__)
prod_test_clicmd_t prod_test_cmd_func_ptr;
void prod_test_cli_register(void *cb)
{
    prod_test_cmd_func_ptr = (prod_test_clicmd_t)cb;
}
#endif

extern void cli_task(void *params);

void cli_open(void)
{
    TaskHandle_t cli_task_h = NULL;
    BaseType_t status;
    cli_init();
    status = xTaskCreate(cli_task, "CMD",
                         1000 * sizeof(StackType_t), NULL,
                         OS_TASK_PRIORITY_NORMAL, &cli_task_h);
    ASSERT_ERROR_UNINIT(status == pdPASS);
    ASSERT_ERROR_UNINIT(cli_task_h);
}

#if CFG_PMGR
#define CLI_PMGR_CONSTRAIN_TIMEOUT_TICKS pdMS_TO_TICKS(3000)
static TimerHandle_t pmgr_constrain_timer = NULL;
static bool cli_pmgr_constrained = false;

static void cli_pmgr_cb(TimerHandle_t xTimer)
{
    RA6W1_UNUSED_ARG(xTimer);

    if (cli_pmgr_constrained)
    {
        RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
        cli_pmgr_constrained = false;
    }
}

static void cli_pmgr_constrain(void)
{
    if (!pmgr_constrain_timer)
    {
        pmgr_constrain_timer = xTimerCreate("cli_pmgr", CLI_PMGR_CONSTRAIN_TIMEOUT_TICKS,
                                            pdFALSE, NULL, cli_pmgr_cb);
    }

    if (cli_pmgr_constrained)
    {
        xTimerChangePeriod(pmgr_constrain_timer, CLI_PMGR_CONSTRAIN_TIMEOUT_TICKS, 0);
    }
    else
    {
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
        xTimerStart(pmgr_constrain_timer, 0);
        cli_pmgr_constrained = true;
    }
}
#endif

static void cli_task_func(void *param)
{
    (void) param;
    uint32_t rtn;
    char ch = 0;
    line.buf = pvPortMalloc(LINEBUF_SIZE + 1);
    line.len = 0;
    uint32_t notify_val = 0;

#if defined  ( __SUPPORT_APP_CONSOLE_INPUT__ )
    app_buff.buf = pvPortMalloc(APPBUF_SIZE + 1);
    app_buff.len = 0;
#endif

    // Wait for client task to be ready to process cli input
    while (1) {
        rtn = xTaskNotifyWait(0x0, 0x0, &notify_val, portMAX_DELAY);
        configASSERT(rtn == pdPASS);
#ifdef __SUPPORT_EASY_SETUP__
        // Wait until Task easy setup sends release
        while (!(notify_val & EASY_SETUP_TASK_EVENT_FINISH))
        {
            xTaskNotifyWait(0x0, EASY_SETUP_TASK_EVENT_FINISH, &notify_val, portMAX_DELAY);
        }
#endif /* __SUPPORT_EASY_SETUP__ */

        for (;;) {
            ch = (char)getchar();
#if defined  ( __SUPPORT_APP_CONSOLE_INPUT__ )
            if(app_console_access)
            {
                if (app_buff.len < APPBUF_SIZE)
                {
                    *(app_buff.buf + app_buff.len) = ch;
                    app_buff.len++;
                }
                continue;
            }
#endif
            if(ch == -1) // detected wkaup signal
            {
                print_prompt_wake();
                line.buf[0] = '\0';
                line.len = 0;
                continue;
            }
            if (ch >= 0x00 && ch <= 0x1F // Echo off when Control character input
                && ch != 0x03 /* 03 Ctrl-C */
                && ch != 0x08 /* 08 Ctrl-H Backspace */
                && ch != 0x0A /* 10 Ctrl-J Line Feed */
                && ch != 0x0D /* 13 Ctrl-M Carriage Return */
                && ch != 0x1B /* 27 Ctrl-[ ESC */
            ) {
                continue;
            }

#if CFG_PMGR
            /* Constrain PMGR sleep for better CLI user experience */
            cli_pmgr_constrain();
#endif

#ifdef CONFIG_RETARGET
            pre_key3 = pre_key2;
            pre_key2 = pre_key1;
            pre_key1 = ch;

            if (line.len == 0 && ch == '\b') {
                continue;
            } else if (line.len > 0 && ch == '\b') { // backspace process
                putchar(ch); // local echo
                printf("\x1b[K"); // esc[0K Clear line from cursor right
                line.len--;
                continue;
            } else if (doskey_check()) {
                continue;
            } else {
                if ((ch != '\r' && ch != '\n')) { // NOTE : Normal Char.
                    if (line.len < LINEBUF_SIZE) {
                        if ((ch >= 0x00 && ch <= 0x1F)  // Echo off when Control character input
                            || (pre_key2 == 0x1b && ch == '[')) { // for DosKey 'esc['
                            continue;
                        }

                        putchar(ch); // local echo
                        *(line.buf + line.len) = ch;
                        line.len++;
                    }
                    continue;
                } else if (line.len == 0 && (ch == '\r' || ch == '\n')) { // NOTE : If only Enter key is pressed.
                    // Discard the 2nd \r or \n entered.
                    if ((pre_key2 == '\r' && ch == '\n') || (pre_key2 == '\n' && ch == '\r')) {
                        pre_key1 = 0;
                        pre_key2 = 0;
                        pre_key3 = 0;
                        continue;
                    }

                    line.buf[0] = '\0';
                    print_prompt();
                }
            }
#endif
            if (line.len > 0 && (ch == '\r' || ch == '\n')) { // NOTE :  If the command and enter key are entered.
                // DOS_KEY Do not initialize pre_key1.(For discard the 2nd \r or \n entered.)
                pre_key2 = 0;
                pre_key3 = 0;
                cur_hist_idx = -1;

                doskey_cmd_add(line.buf, line.len); // NOTE : Save previous command

                notify_client();
                line.buf[0] = '\0';
                line.len = 0;

#if defined __SUPPORT_APP_CONSOLE_INPUT__
                vTaskDelay(portCONVERT_MS_2_TICKS(100)); // For printing command groups.
                if(!app_console_access)
#endif  // __SUPPORT_APP_CONSOLE_INPUT__
                print_prompt();
                continue;
            }
        }
    }
}

void cli_init(void)
{
    if (cli_handle_task) {
        configASSERT(0);
        return;
    }

#if (dg_configSYSTEMVIEW == 0)
    xTaskCreate(cli_task_func, "cli", (((200 * sizeof(StackType_t)) - 1) / sizeof(StackType_t) + 1), NULL, CLI_TASK_PRIORITY, &cli_handle_task);
#else
    xTaskCreate(cli_task_func, "cli", (((200 * sizeof(StackType_t) + dg_configSYSTEMVIEW_STACK_OVERHEAD) - 1) / sizeof(StackType_t) + 1), NULL, CLI_TASK_PRIORITY, &cli_handle_task);
#endif /* (dg_configSYSTEMVIEW == 1) */
}

cli_t cli_register(uint32_t notif_mask, const cli_command_t commands[], cli_handler_t def_handler)
{
    // TODO: add support for more than one task registered for CLI
    if (reg_client.task) {
        configASSERT(0);
        return NULL;
    }

    reg_client.task = xTaskGetCurrentTaskHandle();
    reg_client.notif_mask = notif_mask;
    reg_client.commands = commands;
    reg_client.def_handler = def_handler;
    reg_client.queue = xQueueCreate(QUEUE_LEN, sizeof(char *));

    // Notify cli task it can start processing cli input
    configASSERT(cli_handle_task);
    xTaskNotify(cli_handle_task, CLI_TASK_EVENT, eSetBits);

    return (cli_t)&reg_client;
}

static int make_argv(char *s, int argvsz, char *argv[])
{
    int argc = 0;

    /* split into argv */
    while (argc < argvsz - 1) {
        /* skip any white space */
        while ((*s == ' ') || (*s == '\t') || (*s == '\r') || (*s == '\n')) {
            ++s;
        }

        if (*s == '\0') {     /* end of s, no more args */
            break;
        }

        /* find end of string */
        if (*s == '"') {
            /* string parameter */
            ++s;
            argv[argc++] = s;    /* begin of argument string */

            while (*s && (*s != '"') && (*s != '\r') && (*s != '\n')) {
                ++s;
            }

            if (*s == '\0') {       /* end of s, no more args */
                break;
            }

            if (*s == '"') {
                *s = '\0';
            }

            *s++ = '\0';    /* terminate current arg */

        } else if (*s == '\'') {
            /* string parameter */
            argv[argc++] = s;    /* begin of argument string    */
            ++s;

            while (*s && (*s != '\'') && (*s != '\r') && (*s != '\n')) {
                ++s;
            }

            if (*s == '\0' || *s == '\'') {   /* end of s, no more args    */
                break;
            }
        } else {
            /* non string parameter */
            argv[argc++] = s;    /* begin of argument string */

            while (*s && (*s != ' ') && (*s != '\t') && (*s != '\r') && (*s != '\n')) {
                ++s;
            }

            if (*s == '\0') {      /* end of s, no more args */
                break;
            }

            *s++ = '\0';    /* terminate current arg */
        }
    }

    argv[argc] = NULL;

    return argc;
}

void cli_handle_notified(cli_t cli)
{
    client_t *client = (client_t *)cli;
    char *line_buf;
    char *ptr;
    int argc;
    const char *argv[ARGC_MAX];
    const cli_command_t *cmd;
    bool cmd_valid = false;

    if (!client) {
        configASSERT(0);
        return;
    }

    if (xQueueReceive(client->queue, &line_buf, 0) != pdTRUE) {
        return;
    }

    ptr = line_buf;
    
    argc = make_argv(ptr, ARGC_MAX, (char **)argv);

    if (!strcmp(argv[0], "up") || !strcmp(argv[0], "..")) {
        current_cmd_group = NULL;
        goto done;
    }

    if (current_cmd_group == NULL && !strcmp(argv[0], "help")) {
        if (client->def_handler) {
            client->def_handler(argc, argv, NULL);
            goto done;
        }
    }

    /* check command group */
    if (current_cmd_group) {
        if (cmd_valid |= current_cmd_group->handler(argc, argv, current_cmd_group->user_data)) {
            goto done;
        }
    }

#if defined (__SUPPORT_PRODTEST_CONSOLE__)
    /* check production command */
    if (prod_test_cmd_func_ptr) {
        if (cmd_valid |= prod_test_cmd_func_ptr(argc, argv, NULL)) {
            goto done;
        }
    }
#endif

    /* check root command */
    if (cmd_valid |= root_command_handlers(argc, argv, NULL)) {
        goto done;
    }

    for (cmd = client->commands; cmd && cmd->name; cmd++) {
        if (!strcmp(cmd->name, argv[0])) {
            current_cmd_group = cmd;
            if (argc > 1) {
                if (cmd_valid |= cmd->handler(--argc, &argv[1], cmd->user_data)) {
                    goto done;
                }
            } else {
                goto done;
            }
        }
    }

    if (!cmd_valid) {
        printf("\nUnknown command: %s\n", argv[0]);
    }

done:
    vPortFree(line_buf);

    if (uxQueueMessagesWaiting(reg_client.queue)) {
        xTaskNotify(reg_client.task, reg_client.notif_mask, eSetBits);
    }
}
