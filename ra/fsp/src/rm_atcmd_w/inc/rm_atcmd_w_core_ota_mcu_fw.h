/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_OTA_MCU_FW_H
#define RM_ATCMD_W_CORE_OTA_MCU_FW_H

#include "FreeRTOS.h"
#include "rm_wifi.h"

#include <stdio.h>
#include "sys_app_defs.h"

#define PREFIX_OTA_BY_MCU              "tx_size="

/// MCU Firmware version structure.
#define ATCMD_W_OTA_MCU_FW_NAME_LEN    (8)
typedef struct
{
    char name[ATCMD_W_OTA_MCU_FW_NAME_LEN];
    UINT size;
    UINT crc;
} atcmd_w_ota_mcu_fw_info_t;
#define ATCMD_W_OTA_MCU_FW_HEADER_SIZE    sizeof(atcmd_w_ota_mcu_fw_info_t)

UINT atcmd_w_ota_update_by_mcu_init(UINT fw_type, UINT len);
UINT atcmd_w_ota_update_by_mcu_get_total_len(void);
UINT atcmd_w_ota_update_by_mcu_download(UCHAR * rev_data, UINT rev_data_len);
UINT atcmd_w_ota_update_gen_mcu_header(UINT size);

#endif                                 /* RM_ATCMD_W_CORE_OTA_MCU_FW_H */

/* EOF */
