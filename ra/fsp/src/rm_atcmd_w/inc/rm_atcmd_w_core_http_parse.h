/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_HTTP_PARSE_H
#define RM_ATCMD_W_CORE_HTTP_PARSE_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_HTTP_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_HTTP_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_HTTP_open(atcmd_w_ctrl_t * const p_at_ctrl);

uint32_t RM_ATCMD_W_CORE_HTTP_close(atcmd_w_ctrl_t * const p_at_ctrl);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_HTTP_PARSE_H */
