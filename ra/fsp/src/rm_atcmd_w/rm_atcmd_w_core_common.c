/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"
#include "rm_atcmd_w_core.h"
#include "ctype.h"
#if CFG_WIFI
#include "rm_wifi_helper.h"
#endif /* CFG_WIFI */

int rm_atcmd_w_core_common_is_query_arg(int argc, char * p_str)
{
    return (argc == 2 && strcmp(p_str, AT_CMD_GET_MRK) == 0);
}

char * rm_atcmd_w_core_common_strupr(char * p_str)
{
    char * ptr = NULL;

    for (ptr = p_str; *ptr ; ptr++)
    {
        *ptr = (char) toupper(*ptr);
    }

    return p_str;
}

int rm_atcmd_w_core_common_atoi(char * p_str)
{
    int res = 0;
    int minus_sign = 0;

    if (p_str == NULL)
    {
        return 0;
    }

    for (int i = 0 ; p_str[i] != '\0' ; ++i)
    {
        if (i == 0)
        {
            if (p_str[i] >= '0' && p_str[i] <= '9')
            {
                res = res * 10 + p_str[i] - '0';
            }
            else if (p_str[i] == '-')
            {
                minus_sign = 1;
            }
            else if (p_str[i] == '+')
            {
                minus_sign = 0;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            if (p_str[i] >= '0' && p_str[i] <= '9')
            {
                res = res * 10 + p_str[i] - '0';
            }
            else
            {
                return 0;
            }
        }
    }

    return (minus_sign ? (res * (-1)) : (res));
}

int rm_atcmd_w_core_common_stoi(char * param, int * int_val, int policy)
{
    int result = -1, param_len, int_val_old;

    if (param == NULL || int_val == NULL)
    {
        return -1;
    }

    param_len = (int) strlen(param);
    int_val_old = *int_val;

    if (param_len == 1)
    {
        if (param[0] == '0')
        {
            // "0" <- non error 0 return
            *int_val = 0;
            result = 0; /* SUCCESS */
        }
        else
        {
            // check if valid single digit 1 ~ 9
            *int_val = rm_atcmd_w_core_common_atoi(param);

            if (*int_val > 0 && *int_val < 10)
            {
                // valid value: 1~9
                result = 0; /* SUCCESS */
            }
            else
            {
                // error: e.g. == 0
                *int_val = int_val_old;
                result = -1;
            }
        }
    }
    else if (param_len == 0)
    {
        *int_val = int_val_old;
        result = -1;
    }
    else
    {
        if (policy == POL_1)
        {
            // leading "0" / "+" / "-0" are not allowed
            if (param[0] == '0' || param[0] == '+')
            {
                return -1;
            }

            if (param[0] == '-' && param[1] == '0')
            {
                return -1;
            }
        }
        else if (policy == POL_2)
        {
            // leading "+" / "-0" are not allowed
            if (param[0] == '+')
            {
                return -1;
            }

            if (param[0] == '-' && param[1] == '0')
            {
                return -1;
            }
        }

        *int_val = rm_atcmd_w_core_common_atoi(param);

        if (*int_val > -1 && *int_val < 10)
        {
            /* Considered error */
            *int_val = int_val_old;
            result = -1;
        }
        else
        {
            result = 0; /* SUCCESS */
        }
    }

    return result;
}

int rm_atcmd_w_core_common_is_in_valid_range(int val, int min, int max)
{
    if (val >= min && val <= max)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int rm_atcmd_w_core_common_htoi_custom(char * p_str)
{
    int idx = 0;
    char ch = 0;
    int dec = 0;

    if (p_str == NULL)
    {
        return 0;
    }

    for (idx = 0; (ch = p_str[idx]) != '\0'; idx++)
    {
        dec *= 16;

        if (idx == 0 && ch == '0')
        {
            ch = p_str[++idx];

            if (ch != 'x' && ch != 'X')
            {
                --idx;
            }
        }
        else if (ch >= '0' && ch <= '9')
        {
            dec += ch - '0';
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            dec += 10 + (ch - 'a');
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            dec += 10 + (ch - 'A');
        }
        else
        {
            return dec;
        }
    }

    return dec;
}

uint8_t rm_atcmd_w_core_common_htoi_char(char c)
{
    uint8_t rslt;

    if ((c >= '0') && (c <= '9'))
    {
        rslt = (c-'0');
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        rslt = (c-'a' + (uint8_t) 10);
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        rslt = (c-'A' + (uint8_t) 10);
    }
    else
    {
        rslt = (uint8_t) 0;
    }

    return rslt;
}

int rm_atcmd_w_core_common_is_duplicate_string_found(char ** pp_str_array, int str_count)
{
    int duplicate_found = 0;
    int i;

    for (i = str_count ; i > 0 ; i--)
    {
        unsigned int j;

        if (pp_str_array[i - 1] == NULL)
        {
            continue;
        }

        for (j = (unsigned int) (i - 1) ; j > 0 ; j--)
        {
            if (strcmp(pp_str_array[i - 1], pp_str_array[j - 1]) == 0)
            {
                duplicate_found = 1;
                break;
            }
        }

        if (duplicate_found)
        {
            break;
        }
    }

    return duplicate_found;
}

int rm_atcmd_w_core_common_print_error_code(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code)
{
    return rm_atcmd_w_core_common_print_error_code_ext(p_at_ctrl, code, NULL);
}

int rm_atcmd_w_core_common_print_error_code_ext(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code,
                                                const char * p_ext)
{
    fsp_err_t err;
    char resp_str[64] = {0x00,};

    if (p_ext)
    {
        snprintf(resp_str, sizeof(resp_str), "\r\nERROR:0x%X (%d),%s\r\n", code, code, p_ext);
    }
    else
    {
        snprintf(resp_str, sizeof(resp_str), "\r\nERROR:0x%X (%d)\r\n", code, code);
    }

    err = RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)resp_str, strlen(resp_str));
    if (err != FSP_SUCCESS)
    {
        return -1;
    }

    return 0;
}

#if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
int rm_atcmd_w_core_common_print_error_code_esc(atcmd_w_ctrl_t * const p_at_ctrl, const fsp_err_atcmd_err_code code,
                                                const char * p_ext)
{
    fsp_err_t err;
    char resp_str[64] = {0x00,};

    if (p_ext)
    {
        snprintf(resp_str, sizeof(resp_str), "\e\r\nERROR:0x%X (%d),%s\r\n", code, code, p_ext);
    }
    else
    {
        snprintf(resp_str, sizeof(resp_str), "\e\r\nERROR:0x%X (%d)\r\n", code, code);
    }

    err = RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *)resp_str, strlen(resp_str));
    if (err != FSP_SUCCESS)
    {
        return -1;
    }

    return 0;
}
#endif
