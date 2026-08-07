/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "r_rtc_w.h"
#include "rm_atcmd_w_core_utils.h"

#define CHECK_RANGE(min, max, val, ret_val) if( val < min || val > max ){ return( ret_val ); }

int rm_atcmd_w_atoi_custom (char* str)
{
    int res = 0, minus_sign = 0;

    if (str == NULL)
    {
        return 0;
    }

    for (int i = 0; str[i] != '\0'; ++i)
    {
        if (i == 0)
        {
            if (str[i] >= '0' && str[i] <= '9')
            {
                res = res * 10 + str[i] - '0';
            }
            else if (str[i] == '-')
            {
                minus_sign = 1;
            }
            else if (str[i] == '+')
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
            if (str[i] >= '0' && str[i] <= '9')
            {
                res = res * 10 + str[i] - '0';
            }
            else
            {
                return 0;
            }
        }
    }

    return (minus_sign?(res*(-1)):(res));
}

int rm_atcmd_w_get_int_val_from_str(char* param, int* int_val, int policy)
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
            /* "0" <- non error 0 return */
            *int_val = 0;
            result = 0; // SUCCESS
        }
        else
        {
            /* check if valid single digit 1 ~ 9 */
            *int_val = rm_atcmd_w_atoi_custom
		(param);

            if (*int_val > 0 && *int_val < 10)
            {
                /* valid value: 1~9 */
                result = 0; // SUCCESS
            }
            else
            {
                /* error: e.g. == 0 */
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
        /* param_len > 1 */

        if (policy == POL_1)
        {
            /* leading "0" / "+" / "-0" are not allowed */
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
            /* leading "+" / "-0" are not allowed */
            if (param[0] == '+')
            {
                return -1;
            }

            if (param[0] == '-' && param[1] == '0')
            {
                return -1;
            }
        }

        *int_val = rm_atcmd_w_atoi_custom
	(param);

        if (*int_val > -1 && *int_val < 10)
        {
            /*  considered error */
            *int_val = int_val_old;
            result = -1;
        }
        else
        {
            result = 0; // SUCCESS
        }
    }

    return result;
}

int rm_atcmd_w_get_int_val_from_date_time_str(char* param, int* int_val)
{
    int param_len;
    char *vaild_digit = "0123456789";

    if (param == NULL || int_val == NULL)
    {
        return -1;
    }

    param_len = (int) strlen(param);

    if (param_len == 2)
    {
        if (strchr(vaild_digit, param[0]) == NULL || strchr(vaild_digit, param[1]) == NULL)
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    *int_val = rm_atcmd_w_atoi_custom(param);

    return 0;
}

int rm_atcmd_w_is_date_time_valid(struct tm *t)
{
    /*
        int tm_sec;
        int tm_min;
        int tm_hour;

        int tm_mday;
        int tm_mon;
        int tm_year;
    */

    int month_len;
    int month;

    month = t->tm_mon + 1;
    CHECK_RANGE( 70, 8099, t->tm_year, -2); // 1970 ~ 9999
    CHECK_RANGE( 0, 23,    t->tm_hour, -3);
    CHECK_RANGE( 0, 59,    t->tm_min,  -3);
    CHECK_RANGE( 0, 59,    t->tm_sec,  -3);

    switch (month)
    {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            month_len = 31;
            break;

        case 4: case 6: case 9: case 11:
            month_len = 30;
            break;

        case 2:
            if ( ( !( t->tm_year % 4 ) && t->tm_year % 100 ) || !( t->tm_year % 400 ) ) {
                month_len = 29;
            } else {
                month_len = 28;
            }
            break;

        default:
            return -2;
    }
    CHECK_RANGE( 1, month_len, t->tm_mday, -2);

    return pdTRUE;
}

int rm_atcmd_w_set_time(char *date_format, char *time_format, int daylight)
{
	struct tm correction;
	char *pos = NULL;
	int tmp_int1 = 0;

	if (!date_format || !time_format)
    {
		return -1;
	}

    memset(&correction, 0x00, sizeof(struct tm));

	/* Year */
	pos = strtok(date_format, "-");
	if (pos && (rm_atcmd_w_get_int_val_from_str(pos, &tmp_int1, 3) == 0) )
    {
		correction.tm_year = tmp_int1 - 1900;
	}
    else
    {
		return -2;
	}

	/* Month */
	pos = strtok(NULL, "-");
	if (pos && (rm_atcmd_w_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) )
    {
		correction.tm_mon  = tmp_int1 - 1;
	}
    else
    {
		return -2;
	}

	/* Day */
	pos = strtok(NULL, "\0");
	if (pos && (rm_atcmd_w_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) )
    {
		correction.tm_mday = tmp_int1;
	}
    else
    {
		return -3;
	}

	/* Hour */
	pos = strtok(time_format, ":");
	if (pos && (rm_atcmd_w_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) )
    {
		correction.tm_hour = tmp_int1;
	}
    else
    {
		return -3;
	}

	/* Min */
	pos = strtok(NULL, ":");
	if (pos && (rm_atcmd_w_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) )
    {
		correction.tm_min  = tmp_int1;
	}
    else
    {
		return -3;
	}

	/* Sec */
	pos = strtok(NULL, "\0");
	if (pos && (rm_atcmd_w_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) )
    {
		correction.tm_sec  = tmp_int1;
	}
    else
    {
		return -3;
	}

	if ((tmp_int1 = rm_atcmd_w_is_date_time_valid(&correction)) != pdTRUE)
    {
        return tmp_int1;
    }

	/* Season flag, such as daylight saving time */
	if (daylight)
    {
		correction.tm_isdst = 1;
	}
    else
    {
		correction.tm_isdst = -1;
	}

	__time64_t  corrtime, now;

	ra6w1_mktime64(&correction, &corrtime);
	ra6w1_time64(&corrtime, &now);	// set time UTC
	
	return 0;
}
