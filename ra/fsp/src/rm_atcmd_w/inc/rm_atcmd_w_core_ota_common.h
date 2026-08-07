/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_OTA_COMMON_H
#define RM_ATCMD_W_CORE_OTA_COMMON_H

#if 1                                  // defined (__SUPPORT_OTA__)
 #include <stdio.h>
 #include "sys_app_defs.h"

 #include "ra6w1_image.h"
 #include "rm_atcmd_w_core_ota_update.h"

 #if !defined(BIT)
  #define BIT(x)    (1 << (x))
 #endif                                // (BIT)

// For dynamic memory allocation ...
 #define ATCMD_W_OTA_MALLOC    pvPortMalloc
 #define ATCMD_W_OTA_FREE      vPortFree

/// Disable version checking for debugging
 #undef DISABLE_ATCMD_W_OTA_VER_CHK

/// OTA update thread name
 #define ATCMD_W_OTA_TASK_NAME            "ATCMD_W_OTA_update"
 #define ATCMD_W_OTA_DPM_REG_NAME         ATCMD_W_OTA_TASK_NAME

 #define ATCMD_W_OTA_TASK_STACK_SZ        (1024 * 5) / 4        // WORD
 #define ATCMD_W_OTA_SFLASH_BUF_SZ        SF_PARTITION_TBL_SIZE // 4KB

 #define ATCMD_W_OTA_TIMEOUT              HTTPC_DEF_TIMEOUT * 2 // sec
 #define ATCMD_W_OTA_EVT_TIMEOUT          0x00
 #define ATCMD_W_OTA_EVT_RECEIVE          0x01
 #define ATCMD_W_OTA_EVT_FINISH           0x10

/// NVRAM name of Download progress
 #define ATCMD_W_OTA_NVRAM_DW_PROGRESS    "OTA_PROG_"

/// Process is not ready.
 #define ATCMD_W_OTA_STATE_NOT_READY      0

/// Process is ready.
 #define ATCMD_W_OTA_STATE_READY          1

/// Process is ongoing.
 #define ATCMD_W_OTA_STATE_PROGRESS       2

/// Process completed.
 #define ATCMD_W_OTA_STATE_FINISH         3

/// Process stopped.
 #define ATCMD_W_OTA_STATE_STOP           4

 #define ATCMD_W_OTA_VER_DELIMITER        "-"
 #define ATCMD_W_OTA_VER_START_OFFSET     8 // address 0x02008 ~ 0x02063
 #define ATCMD_W_OTA_FW_MAGIC_NUM         {0x44, 0x41, 0x31, 0x36} // IMAGE_HEADER_MAGIC_CODE 0x36314144

/// Firmware version structure max length in bytes.
 #define UPDATE_TYPE_MAX                  12
 #define MODULE_MAX                       12
 #define SDK_MAX                          14
 #define CUSTOMER_MAX                     20

typedef struct
{
    // Update type
    CHAR update_type[UPDATE_TYPE_MAX + 1];

    // Module name
    CHAR module[MODULE_MAX + 1];

    // SDK version
    CHAR sdk[SDK_MAX + 1];

    // Customer version
    CHAR customer[CUSTOMER_MAX + 1];
} AT_FW_versionInfo_t;

// Status for version check.
enum at_ota_version_flags
{
    // Init value.
    ATCMD_W_OTA_HEADER_INIT = 0,

    // The type of FW to be compared is the same.
    ATCMD_W_OTA_HEADER_SAME_FW_TYPE,

    // The Module name of FW to be compared is the same.
    ATCMD_W_OTA_HEADER_SAME_MODULE,

    // The SDK version of FW to be compared is the different.
    ATCMD_W_OTA_HEADER_DIFF_SDK,

    // The customer version of FW to be compared is the different.
    ATCMD_W_OTA_HEADER_DIFF_CUST,

    // The type of FW to be compared is the different.
    ATCMD_W_OTA_HEADER_DIFF_FW_TYPE,

    // The Module of FW to be compared is the different.
    ATCMD_W_OTA_HEADER_DIFF_MODULE,

    // The magic number of the FW is the same.
    ATCMD_W_OTA_HEADER_SAME_MAGIC,

    // The magic number of the FW is not compatible.
    ATCMD_W_OTA_HEADER_INCOMPATI_MAGIC,

    // The FW is not involved in version checking. (user FW or cert)
    ATCMD_W_OTA_HEADER_VER_DONT_CARE,

    // Error.
    ATCMD_W_OTA_HEADER_ERROR
};

/// Struct for flash writing
typedef struct
{
    UINT    sflash_addr;
    UINT    total_length;
    UINT    length;
    UINT    offset;
    UCHAR * buffer;
} atcmd_w_ota_update_sflash_t;

/// Structure to download firmware
typedef struct
{
    atcmd_w_ota_update_type     update_type;
    atcmd_w_ota_update_sflash_t write;
    UINT download_status;
    UINT version_check;
    UINT content_length;
    UINT received_length;
    UINT httpc_result;
} atcmd_w_ota_update_download_t;

/// Settings structure used for ota update requests.
typedef struct
{
    /// FW type being downloaded
    atcmd_w_ota_update_type update_type;

    /// OTA Server address where RTOS firmware exists.
    char url[ATCMD_W_OTA_HTTP_URL_LEN];

    /// If the value is true, if the new firmware download is successful, it will reboot with the new firmware. Only for RTOS
    UINT auto_renew;

    /// Flash address where the downloaded FW is written.
    UINT download_sflash_addr;

    /// Callback function pointer to check the download status.
    void (* download_notify)(atcmd_w_ota_update_type update_type, UINT ret_status, UINT progress);

    /// Callback function pointer to check the renew state. Only for RTOS.
    void (* renew_notify)(UINT ret_status);

    /// Status of the download process (not_ready, ready, progress, finish, stop).
    UINT update_state;

    /// Download status. (success or failed)
    UINT status;

    /// RTOS download progress.
    UINT progress_rtos;

    /// MCU FW download progress.
    UINT progress_mcu_fw;

    /// CERT_KEY download progress.
    UINT progress_cert_key;
} atcmd_w_ota_update_proc_t;

UINT         atcmd_w_ota_update_check_version(atcmd_w_ota_update_type update_type, UCHAR * data, UINT data_len);
UINT         atcmd_w_ota_update_parse_version_string(UCHAR * version, AT_FW_versionInfo_t * fw_ver);
const char * atcmd_w_ota_update_type_to_text(atcmd_w_ota_update_type update_type);

/**
 ****************************************************************************************
 * @brief      Hexa dump the data on console for cli uses
 * @param[in]  data      Dump address
 * @param[in]  length    Dump length
 * @param[in]  endian    Endian type
 * @return     None
 ****************************************************************************************
 */
 #define OUTPUT_ASCII_ONLY    0
 #define OUTPUT_HEXA_ONLY     1
 #define OUTPUT_HEXA_ASCII    2
UINT   atcmd_w_ota_update_cli_cmd_parse(int argc, char * argv[]);
UINT32 atcmd_w_ota_update_sflash_rtos_crc(UINT sectorAddr);
UINT   atcmd_w_ota_update_get_available_size(atcmd_w_ota_update_type update_type);
UINT   atcmd_w_ota_update_check_available_size(atcmd_w_ota_update_type update_type, UINT size);
UINT   atcmd_w_ota_update_get_curr_sflash_addr(atcmd_w_ota_update_type update_type);
UINT   atcmd_w_ota_update_set_user_sflash_addr(UINT sflash_addr);
UINT   atcmd_w_ota_update_check_all_download(void);
UINT   atcmd_w_ota_update_current_fw_renew(void);

UINT     atcmd_w_ota_update_check_refuse_flag(void);
UINT     atcmd_w_ota_update_process_create(atcmd_w_ctrl_t * const p_at_ctrl, ATCMD_W_OTA_UPDATE_CONFIG * update_conf);
UINT     atcmd_w_ota_update_check_state(void);
UINT     atcmd_w_ota_update_process_stop(void);
void     atcmd_w_ota_update_set_download_progress(atcmd_w_ota_update_type update_type, UINT progress);
UINT     atcmd_w_ota_update_get_download_progress(atcmd_w_ota_update_type update_type);
void     atcmd_w_ota_update_write_nvram_download_progress(atcmd_w_ota_update_type update_type, UINT progress);
UINT     atcmd_w_ota_update_read_nvram_download_progress(atcmd_w_ota_update_type update_type);
void     atcmd_w_ota_update_evt_send(UINT event);
UINT     atcmd_w_ota_update_get_proc_state(void);
void     atcmd_w_ota_update_print_status(atcmd_w_ota_update_type update_type, UINT status);
UINT     atcmd_w_ota_update_buffer_write_flash(atcmd_w_ota_update_sflash_t * sflash_ctx, UCHAR * data, UINT length);
UINT     atcmd_w_ota_update_get_image_info(UINT sectorAddr, image_header_data_t * infoImage);
uint32_t atcmd_w_ota_update_crc32(const void * buf, size_t size);
void     atcmd_w_ota_update_set_boot_index(UINT boot_idx);
UINT     atcmd_w_ota_update_get_boot_index(void);
UINT     atcmd_w_ota_update_toggle_boot_index(void);
uint32_t atcmd_w_ota_update_crc16(uint32_t crcValue, unsigned char newByte);
uint16_t atcmd_w_ota_update_calc_crc16(uint8_t * data, uint32_t size);
UINT     atcmd_w_ota_update_sflash_product_header_crc(void);

#endif                                 /* (__SUPPORT_OTA__) */
#endif                                 /* RM_ATCMD_W_CORE_OTA_COMMON_H */

/* EOF */
