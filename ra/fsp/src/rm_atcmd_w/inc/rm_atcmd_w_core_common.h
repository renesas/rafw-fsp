/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_COMMON_H
#define RM_ATCMD_W_CORE_COMMON_H

#include <stdio.h>
#include <stdint.h>

#include "rm_atcmd_w_api.h"
#include "rm_atcmd_w_core_err_code.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ATCMD_W_LIST_MAX_CNT         (25)

#define AT_CMD_MRK                  "AT+"
#define AT_CMD_CLASS_BC_EXT         "="
#define AT_CMD_GET_MRK              "?"
#define AT_CMD_GET_MRK_CLASS_BC     "=?"
#define AT_CMD_GET_MRK_NWHOST       "=www."
#define AT_CMD_VAR_MRK              ","
#define AT_CMD_MAC_MRK              ":"
#define AT_CMD_PREFIX               "AT"
#define AT_CMD_RESP_TEXT_OK         "OK\n"
#define AT_CMD_RESP_TEXT_ERROR      "ERROR\n"
#define AT_CMD_CTRL_D_CHAR_CODE     '\x04'
#define AT_CMD_MIN_CLI_LINE_LEN     0
#define AT_CMD_LINE_SPACE_KEY       '\x20'
#define AT_CMD_CLEAR_SCREAN_CLS     "clear"
#define AT_CMD_CMD_LINE_CLS         "\x1B\x5B\x32\x4A\x1b[0;0H"
#define AT_CMD_NEW_LINE_CHAR        '\r'
#define AT_CMD_LINE_FEED_CHAR       '\n'
#define AT_CMD_END_OF_STR           '\0'
#define AT_CMD_ENTER_NEW_LINE       "\r\n"
#define AT_CMD_DEL_TXT_CHAR         0x7F
#define AT_CMD_BS_KEY_CHAR          0x08
#define AT_CMD_ESC_KEY_CHAR         0x1B
#define AT_CMD_INIT_DONE_STR        "DONE"
#define AT_CMD_END_OF_STR_LF        0x0A
#define AT_CMD_ESC_HELP_STR         "<ESC>"

#define POL_1   1 
#define POL_2   2
/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum e_atcmd_w_core_atcmd_type
{
    ATCMD_W_TYPE_A = 0,
    ATCMD_W_TYPE_B,
    ATCMD_W_TYPE_C,
#if (ATCMD_SECURE_CHANNEL == 1)
    ATCMD_W_TYPE_SECURE_UNFIXED,
#endif
    ATCMD_W_TYPE_MAX
} atcmd_w_core_atcmd_type_t;

/* Specific AT-CMD */
typedef struct st_atcmd_w_core_module
{
    /* Specific AT-Command */
    const char * p_at_cmd;

    /* Class of specific AT-Command */
    atcmd_w_core_atcmd_type_t at_cmd_type;

    /* Max number of argument */
    uint8_t input_var;

    /* Max timeout in millisecond */
    uint32_t timeout_ms;

    /* Callback function pointer to run the AT-CMD */
    uint32_t (* p_cmd_callback)(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[]);

    /* Callback function pointer to return synopsis of the AT-CMD */
    const char * (* p_format_callback)(void);

    /* Callback function pointer to return description of the AT-CMD */
    const char * (* p_brief_callback)(void);
} atcmd_w_core_module_t;

typedef struct st_atcmd_w_core_unfixed_module
{
    const char at_cmd[24];
    int at_cmd_len;
    uint32_t (* p_cmd_callback)(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, size_t inlen);
    const char * (* p_format_callback)(void);
    const char * (* p_brief_callback)(void);
} atcmd_w_core_unfixed_module_t;

typedef struct st_atcmd_w_core_module_node
{
    const atcmd_w_core_module_t * module;
    struct st_atcmd_w_core_module_node * next;
} atcmd_w_core_module_node_t;

typedef struct st_atcmd_w_core_unfixed_module_node
{
    const atcmd_w_core_unfixed_module_t * module;
    struct st_atcmd_w_core_unfixed_module_node * next;
} atcmd_w_core_unfixed_module_node_t;

/* AT-CMD Module List */
typedef struct st_atcmd_w_core_module_list
{
    /* Module of AT-CMD definition */

    atcmd_w_core_module_node_t * p_module_head;

    atcmd_w_core_unfixed_module_node_t * p_unfixed_module_head;

    /* Number of Module */
    uint8_t module_cnt;

    uint8_t unfixed_module_cnt;
} atcmd_w_core_module_list_t;

typedef enum {
    IOPORT_CFG_INPUT = 0,
    IOPORT_CFG_INPUT_PULLUP,
    IOPORT_CFG_INPUT_PULLDOWN,
    IOPORT_CFG_OUTPUT,
    IOPORT_CFG_OUTPUT_PUSH_PULL,
    IOPORT_CFG_OUTPUT_OPEN_DRAIN,
    IOPORT_CFG_INVALID,
} atcmd_prod_ioport_cfg_t;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
int rm_atcmd_w_core_common_is_query_arg(int argc, char * p_str);
char * rm_atcmd_w_core_common_strupr(char * p_str);
int rm_atcmd_w_core_common_atoi(char * p_str);
int rm_atcmd_w_core_common_stoi(char * param, int * int_val, int policy);
int rm_atcmd_w_core_common_is_in_valid_range(int val, int min, int max);
int rm_atcmd_w_core_common_htoi_custom(char * p_str);
uint8_t rm_atcmd_w_core_common_htoi_char(char c);
int rm_atcmd_w_core_common_is_duplicate_string_found(char ** pp_str_array, int str_count);
int rm_atcmd_w_core_common_print_error_code(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code);
int rm_atcmd_w_core_common_print_error_code_ext(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code,
                                                const char * p_ext);
int rm_atcmd_w_core_common_print_error_code_esc(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code,
                                                const char * p_ext);
/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_COMMON_H */

