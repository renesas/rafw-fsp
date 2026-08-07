/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_RF_TEST_PARSE_H
#define RM_ATCMD_W_CORE_RF_TEST_PARSE_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"

typedef __signed__ char s8;
typedef unsigned char u8;

typedef __signed__ short s16;
typedef unsigned short u16;

typedef __signed__ int s32;
typedef unsigned int u32;

#ifdef __GNUC__
__extension__ typedef __signed__ long long s64;
__extension__ typedef unsigned long long u64;
#else
typedef __signed__ long long s64;
typedef unsigned long long u64;
#endif

typedef uint32_t uint32;

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_RF_TEST_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_RF_TEST_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_RF_TEST_open(atcmd_w_ctrl_t * const p_at_ctrl);

uint32_t RM_ATCMD_W_CORE_RF_TEST_close(atcmd_w_ctrl_t * const p_at_ctrl);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_PROD_TEST_PARSE_H */
