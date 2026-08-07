/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_WIFI_PARSE_H
#define RM_ATCMD_W_CORE_WIFI_PARSE_H

#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_WIFI_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_WIFI_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_WIFI_open(atcmd_w_ctrl_t * const p_at_ctrl);
uint32_t RM_ATCMD_W_CORE_WIFI_close(atcmd_w_ctrl_t * const p_at_ctrl);
uint32_t RM_ATCMD_W_CORE_WIFI_WFJAP_conn_resp(int check_dhcpc);
uint32_t RM_ATCMD_W_CORE_WIFI_WFDAP_disconn_resp(void);
uint32_t RM_ATCMD_W_CORE_WIFI_WFCST_resp(const char * p_in, size_t inlen);
uint32_t RM_ATCMD_W_CORE_WIFI_WFDST_resp(const char * p_in, size_t inlen);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_WIFI_PARSE_H */
