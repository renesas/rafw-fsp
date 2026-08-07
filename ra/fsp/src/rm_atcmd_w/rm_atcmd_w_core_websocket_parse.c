/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_websocket_parse.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"

#include "rm_atcmd_w_core_websocket_client.h"
#include "net_network_main.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CODE(atcmd)    "AT+NW" # atcmd

#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_WEBSOCKET_ ## atcmd ## _brief_cb

#define RM_ATCMD_W_CORE_WEBSOCKET_DEBUG(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define RM_ATCMD_W_CORE_WEBSOCKET_ERROR(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CB(WSC);
RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_FORMAT_CB(WSC);
RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_BRIEF_CB(WSC);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_websocket_module[] =
{
    {
        RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CODE(WSC),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CB_P(WSC),
        RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_FORMAT_CB_P(WSC),
        RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_BRIEF_CB_P(WSC)
    },
    {
        NULL,
        ATCMD_W_TYPE_MAX,
        0,
        0,
        NULL,
        NULL,
        NULL
    },
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_WEBSOCKET_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_websocket_module);
}

uint32_t RM_ATCMD_W_CORE_WEBSOCKET_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_websocket_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

#if CFG_PMGR
uint32_t RM_ATCMD_W_CORE_WEBSOCKET_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

#if defined(__SUPPORT_WEBSOCKET_CLIENT_FOR_ATCMD__)
    if (ra6w1_network_main_get_wlaninit_mode())
    {
        websocket_auto_start_begin();
    }
#endif

    return err;
}

uint32_t RM_ATCMD_W_CORE_WEBSOCKET_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}
#endif /* CFG_PMGR */

RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_CB(WSC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

#if defined(__SUPPORT_WEBSOCKET_CLIENT_FOR_ATCMD__)
    int retval = WS_OK;

    /* AT+NWWSC=<operation>,<uri>(<msg>) */
    if (strcmp(argv[1], "connect") == 0)
    {
        err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;

        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (sizeof(argv[2]) < 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_WSC_URL_STR_LEN;
            goto end;
        }

        retval = websocket_client_connect(p_at_ctrl, argv[2]);

        switch (retval)
        {
            case WS_ERR_CONN_ALREADY_EXIST:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_TASK_ALREADY_EXIST;
                break;
            }

            case WS_ERR_NO_MEM:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            case WS_ERR_INVALID_ARG:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_INVALID_URL;
                break;
            }

            case WS_ERR_CB_FUNC_NOT_EXIST:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_CB_FUNC_DOES_NO_EXIST;
                break;
            }

            case WS_ERR_NOT_SUPPORTED:
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
                break;
            }

            case WS_ERR_INVALID_STATE:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_INVALID_STATE;
                break;
            }

            case WS_FAIL:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_TASK_CREATE_FAIL;
                break;
            }
        }
    }
    else if (strcmp(argv[1], "disconnect") == 0)
    {
        if (argc > 2)
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }

        retval = websocket_client_disconnect();

        switch (retval)
        {
            case WS_ERR_INVALID_STATE:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_INVALID_STATE;
                break;
            }

            case WS_FAIL:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_WSC_CLOSE_FAIL;
                break;
            }
        }
    }
    else if (strcmp(argv[1], "send") == 0)
    {
        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (sizeof(argv[2]) < 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_WSC_URL_STR_LEN;
            goto end;
        }

        if (websocket_client_send_msg(argv[2]) != WS_FAIL)
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NW_WSC_SESS_NOT_CONNECTED;
        }
    }
    else if (strcmp(argv[1], "header") == 0)
    {
        if (argc < 4)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (argc > 5)
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }

        retval = websocket_client_add_header(argv[2], argv[3]);
    }
    else
    {
        if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NW_WSC_UNKNOW_CMD;
        }
    }

end:
#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
#endif

    return err;
}

RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_FORMAT_CB(WSC)
{
    const char * p_usage = "<operation>,<uri>[<msg>]";

    return p_usage;
}

RM_ATCMD_W_CORE_WEBSOCKET_ATCMD_BRIEF_CB(WSC)
{
    const char * p_descrption = "Websocket Client Operation(connect|disconnect|send)";

    return p_descrption;
}
#endif                                 /* CFG_WIFI */
