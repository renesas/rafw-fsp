/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_NETWORK_PARSE_H
#define RM_ATCMD_W_CORE_NETWORK_PARSE_H

#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define DEFAULT_INTERVAL      1000     /* ms */
#define DEFAULT_PING_SIZE     32
#define DEFAULT_PING_WAIT     4000     /* Default time-out is 4000 (4 seconds). */
#define DEFAULT_PING_COUNT    4

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_NETWORK_register(atcmd_w_core_module_list_t * p_list);
uint32_t RM_ATCMD_W_CORE_NETWORK_deregister(atcmd_w_core_module_list_t * p_list);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_NETWORK_PARSE_H */
