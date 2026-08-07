/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_BASIC_PARSE_H
#define RM_ATCMD_W_CORE_BASIC_PARSE_H

#include "rm_atcmd_w_core_common.h"

#if (ATCMD_SECURE_CHANNEL == 1)
#define AES_IV_SIZE_AT 16
#define IV_HEX_LEN_AT  (AES_IV_SIZE_AT * 2)
#endif

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

uint32_t RM_ATCMD_W_CORE_BASIC_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_BASIC_deregister(atcmd_w_core_module_list_t * p_list);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_BASIC_PARSE_H */
