/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifdef __SUPPORT_MATTER_IOT__
#ifndef RM_ATCMD_W_CORE_MATTER_PARSE_H
 #define RM_ATCMD_W_CORE_MATTER_PARSE_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
 #include "sdk_defs.h"
 #include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #define PREFIX_MATTER_CERT    "AT+MCONFIG=CERT"

// atoi error policy
// default leading "0" / "+" / "-0" are not allowed
 #define POL_1                 1

// leading "+" / "-0" are not allowed
 #define POL_2                 2
typedef struct
{
    UINT offset;
    UINT content_length;
    UINT received_length;
    UINT size;
    UINT crc;
    UINT imgcrc;
} ota_mcu_fw_stream_info_t;
 #define OTA_MCU_FW_STREAM_HEADER_SIZE    sizeof(ota_mcu_fw_stream_info_t)

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
UINT app_writeDataToMCU(UINT offset, UINT tot_len, UINT r_len, UINT * srcMemAddr, UINT size);

/***********************************************************************************************************************
 * AT Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_MATTER_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_MATTER_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_MATTER_open(atcmd_w_ctrl_t * const p_at_ctrl);

uint32_t RM_ATCMD_W_CORE_MATTER_close(atcmd_w_ctrl_t * const p_at_ctrl);

void RM_MATTER_PRINTF_ATCMD(char * p_str);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_MATTER_PARSE_H */
#endif                                 /* __SUPPORT_MATTER_IOT__ */
