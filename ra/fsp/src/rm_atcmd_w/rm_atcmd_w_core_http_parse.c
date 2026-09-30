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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include "FreeRTOS.h"

#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "rm_atcmd_w_core_http_parse.h"
#include "rm_atcmd_w_core_http.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_map_persistant_w.h"
#include "rm_wifi_helper.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(atcmd)    "AT+NW" # atcmd

#define RM_ATCMD_W_CORE_HTTP_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_HTTP_ ## atcmd ## _brief_cb

#define RM_ATCMD_W_CORE_HTTP_DEBUG(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define RM_ATCMD_W_CORE_HTTP_ERROR(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTC);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTC);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTC);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSTLSVER);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSTLSVER);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSTLSVER);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSNI);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSNI);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSNI);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSNIDEL);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSNIDEL);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSNIDEL);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCALPN);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCALPN);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCALPN);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCALPNDEL);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCALPNDEL);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCALPNDEL);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCTLSAUTH);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCTLSAUTH);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCTLSAUTH);

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTS);
RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTS);
RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTS);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_HTTP_module[] =
{
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTC),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTC),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTC),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTC)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCSTLSVER),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCSTLSVER),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCSTLSVER),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCSTLSVER)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCSNI),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCSNI),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCSNI),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCSNI)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCSNIDEL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCSNIDEL),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCSNIDEL),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCSNIDEL)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCALPN),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCALPN),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCALPN),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCALPN)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCALPNDEL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCALPNDEL),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCALPNDEL),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCALPNDEL)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTCTLSAUTH),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTCTLSAUTH),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTCTLSAUTH),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTCTLSAUTH)
    },
    {
        RM_ATCMD_W_CORE_HTTP_ATCMD_CODE(HTS),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_HTTP_ATCMD_CB_P(HTS),
        RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB_P(HTS),
        RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB_P(HTS)
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
uint32_t RM_ATCMD_W_CORE_HTTP_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_HTTP_module);
}

uint32_t RM_ATCMD_W_CORE_HTTP_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_HTTP_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_HTTP_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

uint32_t RM_ATCMD_W_CORE_HTTP_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

/* HTTP-CLIENT */
RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc < 2)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 5)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        if (rm_atcmd_w_run_user_http_client(p_at_ctrl, argc, argv))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTC_TASK_CREATE_FAIL;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTC)
{
    const char * p_usage = "<uri>,<option>(,<msg>)";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTC)
{
    const char * p_descrption = "HTTP Client Operation (get|post|put)";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSTLSVER)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char resp_str[32]          = {0x00, };

    int tls_ver = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        http_client_get_tls_version(&tls_ver);

        snprintf(resp_str, sizeof(resp_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), tls_ver);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &tls_ver, POL_1) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if (http_client_set_tls_version(tls_ver))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSTLSVER)
{
    const char * p_usage = "<tls_version>";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSTLSVER)
{
    const char * p_descrption = "Set TLS version";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSNI)
{
    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char                 * p_sni          = NULL;
    char resp_str[HTTPC_MAX_SNI_LEN + 12] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        HTTPC_NVRAM_CONFIG_TLS_SNI,
                                        &p_sni);

        if (p_sni)
        {
            sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), p_sni);

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        }
    }
    else if (argc == 2)
    {
        if (strlen(argv[1]) > HTTPC_MAX_SNI_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTC_SNI_LEN;
        }
        else if (http_client_set_sni(argc, argv))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSNI)
{
    const char * p_usage = "<sni>";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSNI)
{
    const char * p_descrption = "Configure TLS SNI";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCSNIDEL)
{
    char * p_nv_sni = NULL;

    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc < 1)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 1)
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_APPCFG,
                                    HTTPC_NVRAM_CONFIG_TLS_SNI,
                                    &p_nv_sni);

    if (p_nv_sni)
    {
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_SNI);
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCSNIDEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCSNIDEL)
{
    const char * p_descrption = "Delete SNI";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCALPN)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int    idx      = 0;
    int    alpn_num = 0;
    char * p_alpn   = NULL;
    char * p_resp   = NULL;
    char   resp_str[(HTTPC_MAX_ALPN_CNT * HTTPC_MAX_ALPN_LEN) + 32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &alpn_num);

        if (alpn_num >= 1)
        {
            p_resp = resp_str + sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), alpn_num);

            for (idx = 0; idx < alpn_num; idx++)
            {
                char nvrName[25] = {0, };

                sprintf(nvrName, "%s%d", HTTPC_NVRAM_CONFIG_TLS_ALPN, idx);
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvrName, &p_alpn);

                if (p_alpn)
                {
                    p_resp += sprintf(p_resp, ",\"%s\"", p_alpn);
                }
            }

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        }
    }
    else if (argc > (HTTPC_MAX_ALPN_CNT + 2))
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        if ((atoi(argv[1]) > HTTPC_MAX_ALPN_CNT) || (atoi(argv[1]) <= 0))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTC_ALPN_CNT_RANGE;
        }
        else if (http_client_set_alpn(argc, argv))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCALPN)
{
    const char * p_usage = "<alpn_count>,<alpn_1>,<alpn_2>,<alpn_3>";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCALPN)
{
    const char * p_descrption = "Configure TLS ALPN protocol name";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCALPNDEL)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc < 1)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 1)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        if (http_client_set_alpn(argc, argv))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCALPNDEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCALPNDEL)
{
    const char * p_descrption = "Delete ALPN";

    return p_descrption;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTCTLSAUTH)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char resp_str[32]          = {0x00, };

    int auth = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        http_client_get_tls_auth_mode(&auth);

        snprintf(resp_str, sizeof(resp_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), auth);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &auth, POL_1) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if (http_client_set_tls_auth_mode(auth))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTCTLSAUTH)
{
    const char * p_usage = "<tls_auth_mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTCTLSAUTH)
{
    const char * p_descrption = "Set TLS auth mode";

    return p_descrption;
}

/* HTTP-SERVER */
RM_ATCMD_W_CORE_HTTP_ATCMD_CB(HTS)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char * _cmd[2] = {0x00,};
    char resp_str[32] = {0x00,};
    int run_mode = 0;

    if (argc > 2)
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        if (http_server_get_status(&run_mode) != FSP_SUCCESS)
        {
            run_mode = 0;
        }

        snprintf(resp_str, sizeof(resp_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), run_mode);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &run_mode, POL_1) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(run_mode, 0, 1) == pdFALSE)
        {
            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        _cmd[0] = "http-server";
        _cmd[1] = (run_mode == 1) ? "start" : "stop";

        if (rm_atcmd_w_run_user_http_server(p_at_ctrl, 2, _cmd) != FSP_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTS_TASK_CREATE_FAIL;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_FORMAT_CB(HTS)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_HTTP_ATCMD_BRIEF_CB(HTS)
{
    const char * p_descrption = "Enable/Disable HTTP Server";

    return p_descrption;
}
#endif                                 /* CFG_WIFI */
