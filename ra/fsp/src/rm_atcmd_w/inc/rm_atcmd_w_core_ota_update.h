/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_OTA_UPDATE_H
#define RM_ATCMD_W_CORE_OTA_UPDATE_H

#include <stdio.h>
#include "sys_app_defs.h"
#include "rm_atcmd_w_core_common.h"
#include "ra6w1_image.h"

/// Debug feature
#define ENABLE_ATCMD_W_OTA_ERR
#define ENABLE_ATCMD_W_OTA_INFO
#undef  ENABLE_ATCMD_W_OTA_DBG

#if defined(ENABLE_ATCMD_W_OTA_INFO)
 #define ATCMD_W_OTA_INFO    printf
#else
 #define ATCMD_W_OTA_INFO(...)    do {} while (0);
#endif                                 // (ENABLE_ATCMD_W_OTA_INFO)

#if defined(ENABLE_ATCMD_W_OTA_ERR)
 #define ATCMD_W_OTA_ERR    printf
#else
 #define ATCMD_W_OTA_ERR(...)    do {} while (0);
#endif                                 // (ENABLE_ATCMD_W_OTA_ERR)

#if defined(ENABLE_ATCMD_W_OTA_DBG)
 #define ATCMD_W_OTA_DBG    printf
#else
 #define ATCMD_W_OTA_DBG(...)    do {} while (0);
#endif                                 // (ENABLE_ATCMD_W_OTA_DBG)

/*******************************************************************/
/* OTA return code */
/*******************************************************************/

/// Return success.
#define ATCMD_W_OTA_SUCCESS                0x00

/// Return failed.
#define ATCMD_W_OTA_FAILED                 0x01

/// Sflash address is wrong.
#define ATCMD_W_OTA_ERROR_SFLASH_ADDR      0x02

/// FW type is unknown.
#define ATCMD_W_OTA_ERROR_TYPE             0x03

/// Server URL is unknown.
#define ATCMD_W_OTA_ERROR_URL              0x04

/// FW size is wrong or offset address to be downloaded is wrong.
#define ATCMD_W_OTA_ERROR_SIZE             0x05

/// CRC is not correct.
#define ATCMD_W_OTA_ERROR_CRC              0x06

/// FW version is unknown.
#define ATCMD_W_OTA_VERSION_UNKNOWN        0x07

/// FW version is incompatible.
#define ATCMD_W_OTA_VERSION_INCOMPATI      0x08

/// Fw not found on the server.
#define ATCMD_W_OTA_NOT_FOUND              0x09

/// Failed to connect to server.
#define ATCMD_W_OTA_NOT_CONNECTED          0x0a

/// All new FWs have not been downloaded.
#define ATCMD_W_OTA_NOT_ALL_DOWNLOAD       0x0b

/// Failed to alloc memory.
#define ATCMD_W_OTA_MEM_ALLOC_FAILED       0x0c

/// Failed to sflash write
#define ATCMD_W_OTA_FAILED_WRITE           0x0e

/// Failed to create timer
#define ATCMD_W_OTA_FAILED_TIMER           0x0f

/// Timeout
#define ATCMD_W_OTA_FAILED_TIMEOUT         0x10

/// Not initialized
#define ATCMD_W_OTA_NOT_READY              0x11

/// BLE FW version is unknown.
#define ATCMD_W_OTA_BLE_VERSION_UNKNOWN    0xa1

/*******************************************************************/

/// SFLASH address definition
#define ATCMD_W_OTA_STOR_UNKNOWN_ADDR      0xFFFFFFFF

#define ATCMD_W_OTA_STOR_RTOS_SIZE         SF_RTOS_SIZE
#define ATCMD_W_OTA_STOR_RTOS_0_ADDR       SF_RTOS_0
#define ATCMD_W_OTA_STOR_RTOS_1_ADDR       SF_RTOS_1

#define ATCMD_W_OTA_STOR_USER_SIZE         SF_USER_AREA_SIZE
#define ATCMD_W_OTA_STOR_USER_START        SF_USER_AREA
#define ATCMD_W_OTA_STOR_USER_END          (SF_USER_AREA + SF_USER_AREA_SIZE)

#define ATCMD_W_OTA_RTOS_NAME              "RTOS"
#define ATCMD_W_OTA_MCU_FW_NAME            "MCU_FW"
#define ATCMD_W_OTA_BLE_FW_NAME            "BLE_FW"
#define ATCMD_W_OTA_BLE_COMBO_NAME         "BLE_COMBO"
#define ATCMD_W_OTA_CERT_KEY_NAME          "CERT_KEY"

#define ATCMD_W_OTA_HTTP_URL_LEN           (256)

/// Operation step of process
typedef enum
{
    /// Init value
    ATCMD_W_OTA_TYPE_INIT,

    /// RTOS
    ATCMD_W_OTA_TYPE_RTOS,

    /// BLE firmware, for RA6W1/RA6W2
    ATCMD_W_OTA_TYPE_BLE_FW,

    /// RTOS and BLE firmware, for RA6W1/RA6W2
    ATCMD_W_OTA_TYPE_BLE_COMBO,

    /// MCU firmware, not RA6W1/RA6W2
    ATCMD_W_OTA_TYPE_MCU_FW,

    /// Certificate or Key
    ATCMD_W_OTA_TYPE_CERT_KEY,
#if defined(__SUPPORT_MATTER_IOT__)

    // APP_CORE
    /// MCU firmware by Stream, not RA6W1/RA6W2
    ATCMD_W_OTA_TYPE_MCU_FW_STREAM,
#endif                                 // __SUPPORT_MATTER_IOT__
    /// Unknown value
    ATCMD_W_OTA_TYPE_UNKNOWN
} atcmd_w_ota_update_type;

/// OTA update configuration structure
typedef struct
{
    /// Update type.
    atcmd_w_ota_update_type update_type;

    /// Server address where firmware exists.
    char url[ATCMD_W_OTA_HTTP_URL_LEN];

    /// Callback function pointer to check the download status.
    void (* download_notify)(atcmd_w_ota_update_type update_type, UINT ret_status, UINT progress);

    /// Callback function pointer to check the renew state. Only for RTOS.
    void (* renew_notify)(UINT ret_status);

    /// If the value is true, if the new firmware download is successful, it will reboot with the new firmware. Only for RTOS
    UINT auto_renew;

    /// Address of sflash where other_fw is stored. Only for MCU_FW and CERT_KEY
    UINT download_sflash_addr;
} ATCMD_W_OTA_UPDATE_CONFIG;

/**
 ****************************************************************************************
 * @brief The The OTA process begins. If the firmware download is successful, the boot_idx is changed automatically and rebooted with the new firmware.
 * @param[in] p_at_ctrl AT control block.  Allocate an instance specific control block to pass into the AT API calls.
 * @param[in] at_ota_update_conf Pointer of ATCMD_W_OTA_UPDATE_CONFIG.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */

// UINT atcmd_w_ota_update_start_download(ATCMD_W_OTA_UPDATE_CONFIG *at_ota_update_conf);
UINT atcmd_w_ota_update_start_download(atcmd_w_ctrl_t * const      p_at_ctrl,
                                       ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf);

/**
 ****************************************************************************************
 * @brief The firmware download will stop.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_stop_download(void);

/**
 ****************************************************************************************
 * @brief Change boot_idx to the index of the downloaded firmware and reboot. This function is already included in atcmd_w_ota_update_start_download.
 * @param[in] p_at_ctrl AT control block.  Allocate an instance specific control block to pass into the AT API calls.
 * @param[in] at_ota_update_conf Pointer of ATCMD_W_OTA_UPDATE_CONFIG.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_start_renew(atcmd_w_ctrl_t * const p_at_ctrl, ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf);

/**
 ****************************************************************************************
 * @brief Download progress is returned as a percentage.
 * @param[in] update_type Input the firmware type. Refer to enum atcmd_w_ota_update_type.
 * @return 0~100 (100 is success).
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_progress(atcmd_w_ota_update_type update_type);

/**
 ****************************************************************************************
 * @brief SFLASH address to store downloaded data from the server.
 * @param[in] update_type  Input the firmware type. Refer to enum atcmd_w_ota_update_type.
 * @return SFLASH address (hex).
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_new_sflash_addr(atcmd_w_ota_update_type update_type);

/**
 ****************************************************************************************
 * @brief Read SFLASH as much as the input address and length.
 * @param[in] addr  SFLASH address(hex).
 * @param[out] buf  Buffer pointer to store read data.
 * @param[in] len  Length to read.
 * @return Return is 0 on failure, length on success.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_read_flash(UINT addr, VOID * buf, UINT len);

/**
 ****************************************************************************************
 * @brief Erase SFLASH as much as the input address and length.
 * @param[in] addr  SFLASH address(hex).
 * @param[in] len  Length to erase.
 * @return Return is 0 on failure, length on success.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_erase_flash(UINT addr, UINT len);

/**
 ****************************************************************************************
 * @brief Write SFLASH as much as the input address and length.
 * @param[in] addr  SFLASH address(hex).
 * @param[out] buf  Buffer pointer to retrieve data to write.
 * @param[in] len  Length to wirte.
 * @return Return is 0 on failure, length on success.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_write_flash(UINT addr, VOID * buf, UINT len);

/**
 ****************************************************************************************
 * @brief Copy as much as the length from SFLASH address src_addr to dest_addr.
 * @param[in] dest_addr  Destination SFLASH address(hex).
 * @param[in] src_addr  Source SFLASH address(hex).
 * @param[in] len  Length to copy.
 * @return Return is 0 on failure, length on success.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_copy_flash(UINT dest_addr, UINT src_addr, UINT len);

/**
 ****************************************************************************************
 * @brief Set the name(version) of MCU FW to be downloaded to sflash. If not set, it is set as the default string.
 * @param[in] name  Input the firmware name(version). Maximum 8 bytes.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_set_mcu_fw_name(char * name);

/**
 ****************************************************************************************
 * @brief Get name(version) of MCU FW downloaded to sflash.
 * @param[out] name  Pointer to get the name(version) of MCU FW.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_mcu_fw_name(char * name);

/**
 ****************************************************************************************
 * @brief Get name(version), size and CRC32 of MCU FW downloaded to sflash.
 * @param[out] name  Pointer to get the name(version) of MCU FW.
 * @param[out] size    Pointer to get the size of MCU FW.
 * @param[out] crc     Pointer to get the CRC32 value of MCU FW.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_mcu_fw_info(char * name, UINT * size, UINT * crc);

/**
 ****************************************************************************************
 * @brief Starts transmission of MCU FW stored in flash through UART2 as much as the set size.
 * @param[in] p_at_ctrl AT control block.  Allocate an instance specific control block to pass into the AT API calls.
 * @param[in] sflash_addr  Start address for reading.
 * @param[in] size     Read size.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_read_mcu_fw(atcmd_w_ctrl_t * const p_at_ctrl, UINT sflash_addr, UINT size);

/**
 ****************************************************************************************
 * @brief Starts transmission of MCU FW stored in flash through UART2.
 * @param[in] p_at_ctrl AT control block.  Allocate an instance specific control block to pass into the AT API calls.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_trans_mcu_fw(atcmd_w_ctrl_t * const p_at_ctrl);

/**
 ****************************************************************************************
 * @brief Delete MCU FW saved in sflash.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_erase_mcu_fw(void);

/**
 ****************************************************************************************
 * @brief Calculate CRC32 of MCU FW stored in sflash.
 * @param[in] sflash_addr  CRC calculation start address.
 * @param[in] size     CRC calculation size.
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_calcu_mcu_fw_crc(int sflash_addr, int size);

/**
 ****************************************************************************************
 * @brief Parsing the cli command.
 * @param[in] argc
 * @param[in] argv[]
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_cmd_parse(int argc, char * argv[]);

/**
 ****************************************************************************************
 * @brief Set mbedtls_ssl_conf_authmode for https server.
 * @param[in] tls_auth_mode
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_set_tls_auth_mode(int tls_auth_mode);

/**
 ****************************************************************************************
 * @brief Set the supported version sent from the client side and/or accepted at the server side.
 * @param[in] tls_ver
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_set_tls_version(int tls_ver);

/**
 ****************************************************************************************
 * @brief Set the SNI(Server Name Indication).
 * @param[in] sni
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_set_sni(char * sni);

/**
 ****************************************************************************************
 * @brief Set the ALPN(Application Layer Protocol Negotiation). Up to 3 ALPNs can be saved.
 * @param[in] alpn0
 * @param[in] alpn1
 * @param[in] alpn2
 * @return 0x00 (ATCMD_W_OTA_SUCCESS) on success.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_set_alpn(char * alpn0, char * alpn1, char * alpn2);

/**
 ****************************************************************************************
 * @brief Get tls auth mode.
 * @return auth mode value.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_tls_auth_mode(void);

/**
 ****************************************************************************************
 * @brief Get tls version.
 * @return tls version value.
 ****************************************************************************************
 */
UINT atcmd_w_ota_update_get_tls_version(void);

/**
 ****************************************************************************************
 * @brief Get SNI.
 * @param[out] sni
 * @param[in] buf_len
 * @return SNI string length.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_get_sni(char * sni, size_t buf_len);

/**
 ****************************************************************************************
 * @brief Get first ALPN.
 * @param[out] alpn0
 * @param[in] buf_len
 * @return First ALPN string length.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_get_alpn0(char * alpn0, size_t buf_len);

/**
 ****************************************************************************************
 * @brief Get second ALPN.
 * @param[out] alpn1
 * @param[in] buf_len
 * @return Second ALPN string length.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_get_alpn1(char * alpn1, size_t buf_len);

/**
 ****************************************************************************************
 * @brief Get third ALPN.
 * @param[out] alpn2
 * @param[in] buf_len
 * @return Third ALPN string length.
 ****************************************************************************************
 */
size_t atcmd_w_ota_update_get_alpn2(char * alpn2, size_t buf_len);

/**
 ****************************************************************************************
 * @brief Delete all ALPNs.
 * @return void
 ****************************************************************************************
 */
void atcmd_w_ota_update_del_all_alpn(void);

#endif                                 /* RM_ATCMD_W_CORE_OTA_UPDATE_H */

/* EOF */
