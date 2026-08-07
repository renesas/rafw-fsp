/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_UTILS_H
#define RM_ATCMD_W_CORE_UTILS_H

#include <time.h>

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* atoi error policy */
/* default leading "0" / "+" / "-0" are not allowed */
#define POL_1    1

/* leading "+" / "-0" are not allowed */
#define POL_2    2

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
int rm_atcmd_w_atoi_custom(char * str);
int rm_atcmd_w_get_int_val_from_str(char * param, int * int_val, int policy);
int rm_atcmd_w_get_int_val_from_date_time_str(char * param, int * int_val);
int rm_atcmd_w_is_date_time_valid(struct tm * t);
int rm_atcmd_w_set_time(char * date_format, char * time_format, int daylight);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */

#endif                                 /* RM_ATCMD_W_CORE_UTILS_H */
