/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_DA14XXX_PARSE_H
#define RM_ATCMD_W_CORE_DA14XXX_PARSE_H

#include "rm_atcmd_w_cfg.h"
#if (ATCMD_DA14XXX_CODELESS == 1)

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_DA14XXX_NOTIFY_SOURCE_HOST    (1<<0)
#define RM_ATCMD_W_CORE_DA14XXX_NOTIFY_SOURCE_DA14XXX (1<<1)

#define RM_ATCMD_W_CORE_DA14XXX_RX_DATA_LEN_MAX       (256)

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

fsp_err_t RM_ATCMD_W_CORE_DA14xxx_Register(atcmd_w_core_module_list_t * p_list);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_Unregister(atcmd_w_core_module_list_t * p_list);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_Open(atcmd_w_ctrl_t * const p_at_ctrl);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_Close(atcmd_w_ctrl_t * const p_at_ctrl);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_ProcEvents(atcmd_w_ctrl_t * const restrict p_at_ctrl);
fsp_err_atcmd_err_code RM_ATCMD_W_CORE_DA14xxx_ProcRemoteCmd(atcmd_w_ctrl_t * const p_at_ctrl,
                                                        const char* cmd,
                                                        uint32_t size);
bool RM_ATCMD_W_CORE_DA14xxx_IsBinaryMode(void * const p_ctrl);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_BinaryWrite(void * const p_ctrl, const uint8_t * const data, uint32_t size);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_BinaryRead(void * const p_ctrl);
fsp_err_t RM_ATCMD_W_CORE_DA14xxx_ForceExitBinary(void * const p_ctrl);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* ATCMD_DA14XXX_CODELESS == 1 */

#endif /* RM_ATCMD_W_CORE_DA14XXX_PARSE_H */

