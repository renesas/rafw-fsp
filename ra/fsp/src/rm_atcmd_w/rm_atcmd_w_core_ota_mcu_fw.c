/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"

 #include <stdarg.h>
 #include "strings.h"

 #include "rm_atcmd_w_core.h"
 #include "rm_atcmd_w_core_ota_update.h"
 #include "rm_atcmd_w_core_ota_common.h"
 #include "rm_atcmd_w_core_ota_mcu_fw.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "rm_wifi.h"

 #if (SUPPORT_FSP_RM_OTA_W == 1)
  #include "rm_ota_w.h"
 #endif                                /* SUPPORT_FSP_RM_OTA_W */

 #undef DEBUG_ATCMD_W_OTA_MCU_DUMP

 #if defined(__SUPPORT_UART2__)
extern HANDLE uart2;
extern HANDLE uart3;
 #else                                 // SPI || SPIO
extern int host_response(unsigned int buf_addr, unsigned int len, unsigned int resp, unsigned int padding_bytes);

 #endif

 #if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
 #endif                                 /* SUPPORT_FSP_RM_OTA_W */

 #define ATCMD_W_OTA_MCU_BUF_SIZE        (2048)

 #define ATCMD_W_OTA_WDOG_MAX_LATENCY    1 // T_task(Flash write+Crypto verify=1.5sec) / T_wdog(4sec)

static UCHAR g_mcu_fw_name[ATCMD_W_OTA_MCU_FW_NAME_LEN] = ATCMD_W_OTA_MCU_FW_NAME;
static atcmd_w_ota_update_download_t by_mcu;

UINT atcmd_w_ota_update_calcu_mcu_fw_crc (int sflash_addr, int size)
{
    UINT addr_offset = 0;
    UINT tot_len     = 0;
    UINT cal_len     = 0;
    UINT cal_crc     = 0;

    unsigned char * buf = NULL;

    buf = ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_SFLASH_BUF_SZ);
    if (buf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Failed to allocate buffer(%d bytes)\n", __func__, ATCMD_W_OTA_SFLASH_BUF_SZ);

        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
    }

    addr_offset = (UINT) sflash_addr;
    tot_len     = (UINT) size;

    while (tot_len > 0)
    {
        if (tot_len > ATCMD_W_OTA_SFLASH_BUF_SZ)
        {
            cal_len = ATCMD_W_OTA_SFLASH_BUF_SZ;
        }
        else
        {
            cal_len = tot_len;
        }

        memset(buf, 0x00, ATCMD_W_OTA_SFLASH_BUF_SZ);
        if (atcmd_w_ota_update_read_flash(addr_offset, buf, cal_len) == 0)
        {
            ATCMD_W_OTA_ERR("[%s] Failed to crc calculation crc\n", __func__);
            cal_crc = 0;
            break;
        }

        cal_crc = atcmd_w_ota_update_crc32(buf, cal_len);

        addr_offset += cal_len;
        tot_len     -= cal_len;
    }

    ATCMD_W_OTA_INFO("CRC: Calculated CRC = 0x%08x\n", cal_crc);

    if (buf != NULL)
    {
        ATCMD_W_OTA_FREE(buf);
        buf = NULL;
    }

    return cal_crc;
}

UINT atcmd_w_ota_update_gen_mcu_header (UINT size)
{
    UINT status             = ATCMD_W_OTA_SUCCESS;
    UINT mcu_fw_sflash_addr = 0;
    atcmd_w_ota_mcu_fw_info_t at_ota_mcu_fw_info;
    UCHAR * temp_buf = NULL;

    mcu_fw_sflash_addr      = atcmd_w_ota_update_get_curr_sflash_addr(ATCMD_W_OTA_TYPE_MCU_FW);
    at_ota_mcu_fw_info.size = size;
    at_ota_mcu_fw_info.crc  =
        atcmd_w_ota_update_calcu_mcu_fw_crc((int) (mcu_fw_sflash_addr + ATCMD_W_OTA_MCU_FW_HEADER_SIZE),
                                            (int) (at_ota_mcu_fw_info.size));

    memset(&at_ota_mcu_fw_info.name[0], 0x00, sizeof(at_ota_mcu_fw_info.name));
    atcmd_w_ota_update_get_mcu_fw_name(&at_ota_mcu_fw_info.name[0]);
    if (strlen(at_ota_mcu_fw_info.name) == 0)
    {
        bsp_safe_strcpy(&at_ota_mcu_fw_info.name[0], ATCMD_W_OTA_MCU_FW_NAME, ATCMD_W_OTA_MCU_FW_NAME_LEN);
    }

    temp_buf = ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_SFLASH_BUF_SZ);
    if (temp_buf != NULL)
    {
        memset(temp_buf, 0x00, ATCMD_W_OTA_SFLASH_BUF_SZ);
        atcmd_w_ota_update_read_flash(mcu_fw_sflash_addr, temp_buf, ATCMD_W_OTA_SFLASH_BUF_SZ);

        /* Inserting MCU information in front of MCU FW */
        memcpy(temp_buf, &at_ota_mcu_fw_info, ATCMD_W_OTA_MCU_FW_HEADER_SIZE);
        ATCMD_W_OTA_INFO("Insert MCU FW header info (addr=0x%x)\n", mcu_fw_sflash_addr);
        ATCMD_W_OTA_INFO(" Version-------- %s \n", at_ota_mcu_fw_info.name);
        ATCMD_W_OTA_INFO(" Data Size------ %d \n", at_ota_mcu_fw_info.size);
        ATCMD_W_OTA_INFO(" Data CRC------- 0x%x \n", at_ota_mcu_fw_info.crc);

        if (atcmd_w_ota_update_write_flash(mcu_fw_sflash_addr, temp_buf, (UINT) ATCMD_W_OTA_SFLASH_BUF_SZ) == 0)
        {
            ATCMD_W_OTA_ERR("[%s] Failed to write MCU information\n", __func__);
            status = ATCMD_W_OTA_FAILED;
        }
    }
    else
    {
        status = ATCMD_W_OTA_FAILED;
    }

    if (temp_buf != NULL)
    {
        ATCMD_W_OTA_FREE(temp_buf);
        temp_buf = NULL;
    }

    return status;
}

UINT atcmd_w_ota_update_set_mcu_fw_name (char * name)
{
    if (name == NULL)
    {
        return ATCMD_W_OTA_FAILED;
    }

    if ((strlen(name) > 0) && (strlen(name) < ATCMD_W_OTA_MCU_FW_NAME_LEN))
    {
        memset(&g_mcu_fw_name[0], 0x00, ATCMD_W_OTA_MCU_FW_NAME_LEN);
        memcpy(&g_mcu_fw_name[0], name, ATCMD_W_OTA_MCU_FW_NAME_LEN);
    }
    else
    {
        return ATCMD_W_OTA_FAILED;
    }

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_get_mcu_fw_name (char * name)
{
    if (name == NULL)
    {
        return ATCMD_W_OTA_FAILED;
    }

    memset(name, 0x00, ATCMD_W_OTA_MCU_FW_NAME_LEN);
    memcpy(name, &g_mcu_fw_name[0], ATCMD_W_OTA_MCU_FW_NAME_LEN);

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_get_mcu_fw_info (char * name, UINT * size, UINT * crc)
{
    UINT addr, name_len;
    atcmd_w_ota_mcu_fw_info_t at_ota_mcu_fw_info = {0x00, };

    addr = atcmd_w_ota_update_get_curr_sflash_addr(ATCMD_W_OTA_TYPE_MCU_FW);
    memset(at_ota_mcu_fw_info.name, 0x00, ATCMD_W_OTA_MCU_FW_NAME_LEN);
    if (atcmd_w_ota_update_read_flash(addr, &at_ota_mcu_fw_info, sizeof(atcmd_w_ota_mcu_fw_info_t)) != 0)
    {
        /* Check Size */
        if ((at_ota_mcu_fw_info.size <= 0) || (at_ota_mcu_fw_info.size >= 0xffffffff))
        {
            at_ota_mcu_fw_info.size = 0;
        }

        /* Check CRC */
        if ((at_ota_mcu_fw_info.crc <= 0) || (at_ota_mcu_fw_info.crc >= 0xffffffff))
        {
            at_ota_mcu_fw_info.crc = 0;
        }

        /* Check Name */
        name_len = strlen(at_ota_mcu_fw_info.name);
        if ((name_len <= 0) || (at_ota_mcu_fw_info.size <= 0))
        {
            bsp_safe_strcpy(at_ota_mcu_fw_info.name, "NULL", ATCMD_W_OTA_MCU_FW_NAME_LEN);
        }

        /* Copy Size */
        if (size != NULL)
        {
            memcpy(size, &at_ota_mcu_fw_info.size, sizeof(at_ota_mcu_fw_info.size));
        }

        /* Copy CRC */
        if (crc != NULL)
        {
            memcpy(crc, &at_ota_mcu_fw_info.crc, sizeof(at_ota_mcu_fw_info.crc));
        }

        /* Copy Name */
        if (name != NULL)
        {
            if (name_len > ATCMD_W_OTA_MCU_FW_NAME_LEN)
            {
                name_len = ATCMD_W_OTA_MCU_FW_NAME_LEN;
            }

            memcpy(name, at_ota_mcu_fw_info.name, name_len);
        }

        /*
         *      ATCMD_W_OTA_INFO("\n- MCU FW (addr = 0x%x)\n", addr);
         *      ATCMD_W_OTA_INFO(" Name-------------%s \n", at_ota_mcu_fw_info.name);
         *      ATCMD_W_OTA_INFO(" Size-------------%d \n", at_ota_mcu_fw_info.size);
         *      ATCMD_W_OTA_INFO(" CRC--------------0x%x \n\n", at_ota_mcu_fw_info.crc);
         */
        return ATCMD_W_OTA_SUCCESS;
    }

    return ATCMD_W_OTA_FAILED;
}

UINT atcmd_w_ota_update_erase_mcu_fw (void)
{
    UINT sflash_addr = 0, fw_size = 0;
    UINT status = ATCMD_W_OTA_SUCCESS;

    ATCMD_W_OTA_INFO("\n---------FW erase start--------- \n");
    if (atcmd_w_ota_update_get_mcu_fw_info(NULL, &fw_size, NULL) == ATCMD_W_OTA_SUCCESS)
    {
        fw_size    += ATCMD_W_OTA_MCU_FW_HEADER_SIZE;
        sflash_addr = atcmd_w_ota_update_get_new_sflash_addr(ATCMD_W_OTA_TYPE_MCU_FW);

        if (atcmd_w_ota_update_erase_flash(sflash_addr, fw_size) <= 0)
        {
            ATCMD_W_OTA_ERR("[%s] Failed to erase (addr=0x%x, size=%d).\n", __func__, sflash_addr, fw_size);
            status = ATCMD_W_OTA_FAILED;
        }
    }

    ATCMD_W_OTA_INFO("\n---------FW erase finish (0x%02x)--------- \n", status);

    return status;
}

UINT atcmd_w_ota_update_read_mcu_fw (atcmd_w_ctrl_t * const p_at_ctrl, UINT sflash_addr, UINT size)
{
    char * fw_buf = NULL;
    UINT   i, send_cnt = 0;
    UINT   fw_size = 0, blk_size = 0, last_blk_size = 0;
    UINT   fw_addr = 0;
    UINT   status  = ATCMD_W_OTA_SUCCESS;

    ATCMD_W_OTA_INFO("\n---------FW read start--------- \n");

    if ((sflash_addr == 0) ||
        (sflash_addr < ATCMD_W_OTA_STOR_USER_START) ||
        (sflash_addr > ATCMD_W_OTA_STOR_USER_END) ||
        (size == 0))
    {
        ATCMD_W_OTA_ERR("[%s] Please check the input parameter.\n", __func__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

    ATCMD_W_OTA_INFO("\nRead sflash (addr = 0x%x, size = %d) \n", sflash_addr, size);

    fw_addr = sflash_addr;
    fw_size = size;

    fw_buf = (char *) ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_MCU_BUF_SIZE);
    if (fw_buf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Failed to alloc memory (size=%d).\n", __func__, ATCMD_W_OTA_MCU_BUF_SIZE);
        status = ATCMD_W_OTA_MEM_ALLOC_FAILED;
        goto finish;
    }

    memset(fw_buf, 0x00, ATCMD_W_OTA_MCU_BUF_SIZE);

    send_cnt = fw_size / ATCMD_W_OTA_MCU_BUF_SIZE;
    if (fw_size >= ATCMD_W_OTA_MCU_BUF_SIZE)
    {
        blk_size      = ATCMD_W_OTA_MCU_BUF_SIZE;
        last_blk_size = fw_size % ATCMD_W_OTA_MCU_BUF_SIZE;
    }
    else
    {
        last_blk_size = fw_size;
    }

    /* Send FW Binary */
    ATCMD_W_OTA_INFO("Send binary (size=%d) \n", fw_size);
    for (i = 0; i < send_cnt; i++)
    {
        atcmd_w_ota_update_read_flash(fw_addr, (VOID *) fw_buf, blk_size);
        fw_addr += ATCMD_W_OTA_MCU_BUF_SIZE;
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) fw_buf, blk_size);
    }

    if (last_blk_size > 0)
    {
        atcmd_w_ota_update_read_flash(fw_addr, (VOID *) fw_buf, last_blk_size);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) fw_buf, last_blk_size);
    }

    if (fw_buf != NULL)
    {
        ATCMD_W_OTA_FREE(fw_buf);
        fw_buf = NULL;
    }

finish:
    ATCMD_W_OTA_INFO("\n---------FW read finish (0x%02x)--------- \n", status);

    return status;
}

UINT atcmd_w_ota_update_trans_mcu_fw (atcmd_w_ctrl_t * const p_at_ctrl)
{
    UINT status = ATCMD_W_OTA_SUCCESS;
 #if (SUPPORT_FSP_RM_OTA_W == 1)
    UINT   i, send_cnt = 0;
    UINT   fw_size = 0, blk_size = 0, last_blk_size = 0;
    UINT   fw_addr = 0, cal_crc = 0;
    char * fw_buf = NULL;
    char   fw_name[ATCMD_W_OTA_MCU_FW_NAME_LEN + 1] = {0x00, };
    char   atc_buf[32] = {0, };

    ATCMD_W_OTA_INFO("\n---------FW transfer start--------- \n");

    /* Check exists - MCU FW */
    if (atcmd_w_ota_update_get_mcu_fw_info(&fw_name[0], &fw_size, &cal_crc) == ATCMD_W_OTA_SUCCESS)
    {
        if ((strlen(fw_name) <= 0) ||
            (fw_size == 0) || (cal_crc == 0))
        {
            ATCMD_W_OTA_ERR("[%s] MCU FW does not exist.\n", __func__);
            status = ATCMD_W_OTA_FAILED;
            goto finish;
        }
    }
    else
    {
        ATCMD_W_OTA_ERR("[%s] Failed to get FW info.\n", __func__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

    p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_NEW_ADDR, (rm_ota_w_update_type_t) ATCMD_W_OTA_TYPE_MCU_FW,
                          (uint32_t *) &fw_addr);

    if (fw_addr != ATCMD_W_OTA_STOR_UNKNOWN_ADDR)
    {
        /* Header information offset */
        fw_addr += ATCMD_W_OTA_MCU_FW_HEADER_SIZE;
    }
    else
    {
        ATCMD_W_OTA_ERR("[%s] Failed to get addr\n", __func__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

    ATCMD_W_OTA_INFO("\n Transfer %s(len=%d)\n ReadAddr = 0x%x\n FW_SIZE = %d\n FW_CRC = 0x%x\n\n", fw_name,
                     strlen(fw_name), fw_addr, fw_size, cal_crc);

    /* Send FW Name & Size*/
    memset(atc_buf, 0x00, sizeof(atc_buf));
    sprintf(atc_buf, "%s,%d,0x%x\r\n", &fw_name[0], fw_size, cal_crc);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) atc_buf, strlen(atc_buf));

    fw_buf = (char *) ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_MCU_BUF_SIZE);
    if (fw_buf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Failed to alloc memory (size=%d).\n", __func__, ATCMD_W_OTA_MCU_BUF_SIZE);
        status = ATCMD_W_OTA_MEM_ALLOC_FAILED;
        goto finish;
    }

    send_cnt = fw_size / ATCMD_W_OTA_MCU_BUF_SIZE;
    if (fw_size >= ATCMD_W_OTA_MCU_BUF_SIZE)
    {
        blk_size      = ATCMD_W_OTA_MCU_BUF_SIZE;
        last_blk_size = fw_size % ATCMD_W_OTA_MCU_BUF_SIZE;
    }
    else
    {
        last_blk_size = fw_size;
    }

    /* Send FW Binary */
    ATCMD_W_OTA_INFO("Send binary (size=%d, crc=0x%x) \n", fw_size, cal_crc);
    for (i = 0; i < send_cnt; i++)
    {
        memset(fw_buf, 0x00, ATCMD_W_OTA_MCU_BUF_SIZE);
        atcmd_w_ota_update_read_flash(fw_addr, (VOID *) fw_buf, blk_size);
        fw_addr += ATCMD_W_OTA_MCU_BUF_SIZE;
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) fw_buf, blk_size);
    }

    if (last_blk_size > 0)
    {
        memset(fw_buf, 0x00, ATCMD_W_OTA_MCU_BUF_SIZE);
        atcmd_w_ota_update_read_flash(fw_addr, (VOID *) fw_buf, last_blk_size);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) fw_buf, last_blk_size);
    }

    if (fw_buf != NULL)
    {
        ATCMD_W_OTA_FREE(fw_buf);
        fw_buf = NULL;
    }
finish:
 #endif
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    ATCMD_W_OTA_INFO("\n---------FW transfer finish (0x%02x)--------- \n", status);

    return status;
}

UINT atcmd_w_ota_update_by_mcu_init (UINT fw_type, UINT len)
{
    if (atcmd_w_ota_update_check_available_size(fw_type, len) != ATCMD_W_OTA_SUCCESS)
    {
        return ATCMD_W_OTA_ERROR_SIZE;
    }

    by_mcu.write.sflash_addr = atcmd_w_ota_update_get_new_sflash_addr(fw_type);
    if ((by_mcu.write.sflash_addr != ATCMD_W_OTA_STOR_RTOS_0_ADDR) &&
        (by_mcu.write.sflash_addr != ATCMD_W_OTA_STOR_RTOS_1_ADDR))
    {
        return ATCMD_W_OTA_ERROR_SFLASH_ADDR;
    }

    by_mcu.update_type        = fw_type;
    by_mcu.received_length    = 0;
    by_mcu.write.total_length = len;
    by_mcu.write.offset       = 0;
    by_mcu.download_status    = ATCMD_W_OTA_SUCCESS;
    by_mcu.version_check      = ATCMD_W_OTA_NOT_FOUND;
    by_mcu.content_length     = len;
    by_mcu.received_length    = 0;

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_by_mcu_get_total_len (void)
{
    return by_mcu.content_length;
}

UINT atcmd_w_ota_update_by_mcu_download (UCHAR * rev_data, UINT rev_data_len)
{
    UINT status   = ATCMD_W_OTA_SUCCESS;
    UINT progress = 0;
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    uint8_t ota_wdog_id      = WATCHDOG_SERVICE_W_NOT_REGISTERED_ID;
    uint8_t ota_wdog_latency = ATCMD_W_OTA_WDOG_MAX_LATENCY;

    g_wifi_cfg.p_watchdog_service->p_api->registerTask(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                       g_wifi_cfg.p_watchdog_service->p_cfg,
                                                       &ota_wdog_id);
    if (ota_wdog_id == WATCHDOG_SERVICE_W_NOT_REGISTERED_ID)
    {
        ATCMD_W_OTA_ERR("- OTA : Failed to register watchdog service.\n");
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }
 #endif

    if (by_mcu.update_type != ATCMD_W_OTA_TYPE_RTOS)
    {
        status = ATCMD_W_OTA_ERROR_TYPE;
        goto finish;
    }

    if (rev_data == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] rev_data is null\n", __func__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }
    else
    {
        if (by_mcu.version_check == ATCMD_W_OTA_NOT_FOUND)
        {
            if (atcmd_w_ota_update_check_version(by_mcu.update_type, rev_data, rev_data_len) == ATCMD_W_OTA_SUCCESS)
            {
                by_mcu.version_check = ATCMD_W_OTA_SUCCESS;
            }
            else
            {
                /* Version mismatch */
                by_mcu.version_check = ATCMD_W_OTA_VERSION_INCOMPATI;
                status               = ATCMD_W_OTA_VERSION_INCOMPATI;
                goto finish;
            }

            if (by_mcu.version_check == ATCMD_W_OTA_SUCCESS)
            {
                if (atcmd_w_ota_update_check_available_size(by_mcu.update_type,
                                                            by_mcu.content_length) != ATCMD_W_OTA_SUCCESS)
                {
                    by_mcu.version_check = ATCMD_W_OTA_VERSION_INCOMPATI;
                    status               = ATCMD_W_OTA_VERSION_INCOMPATI;
                    goto finish;
                }

                if (by_mcu.write.sflash_addr != atcmd_w_ota_update_get_new_sflash_addr(ATCMD_W_OTA_TYPE_RTOS))
                {
                    status = ATCMD_W_OTA_ERROR_SFLASH_ADDR;
                    goto finish;
                }

                atcmd_w_ota_update_set_download_progress(by_mcu.update_type, 1); // Just for printf
            }
        }

        by_mcu.received_length += rev_data_len;
 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->setLatency(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                         ota_wdog_id,
                                                         ota_wdog_latency);
 #endif
        status = atcmd_w_ota_update_buffer_write_flash(&by_mcu.write, rev_data, rev_data_len);

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
        g_wifi_cfg.p_watchdog_service->p_api->notify(g_wifi_cfg.p_watchdog_service->p_ctrl,
                                                     g_wifi_cfg.p_watchdog_service->p_cfg,
                                                     ota_wdog_id);
 #endif
        if (status != ATCMD_W_OTA_SUCCESS)
        {
            ATCMD_W_OTA_ERR("[%s] Failed to write data to sflash(0x%02x)\n", __func__, status);
            goto finish;
        }

        if ((by_mcu.received_length > 0) && (by_mcu.content_length > 0))
        {
            progress = (by_mcu.received_length * 100) / by_mcu.content_length;
            if (((progress == 0) || ((progress % 10) == 0)) &&
                (progress != atcmd_w_ota_update_get_download_progress(by_mcu.update_type)))
            {
                ATCMD_W_OTA_INFO("\r   >> By MCU Downloading... %d %% (%d/%d Bytes)%s",
                                 progress,
                                 by_mcu.received_length,
                                 by_mcu.content_length,
                                 progress == 100 ? "\n" : " ");
            }
        }
        else
        {
            progress = 0;
        }
    }

finish:
    if (by_mcu.version_check == ATCMD_W_OTA_SUCCESS)
    {
        atcmd_w_ota_update_set_download_progress(by_mcu.update_type, progress);
    }

    if ((status == ATCMD_W_OTA_SUCCESS) && (progress == 100))
    {
        atcmd_w_ota_update_write_nvram_download_progress(by_mcu.update_type,
                                                         atcmd_w_ota_update_get_download_progress(by_mcu.update_type));
    }

    if ((status != ATCMD_W_OTA_SUCCESS) || (progress == 100))
    {
        atcmd_w_ota_update_print_status(by_mcu.update_type, status);
        by_mcu.update_type        = ATCMD_W_OTA_TYPE_INIT;
        by_mcu.received_length    = 0;
        by_mcu.write.sflash_addr  = 0;
        by_mcu.write.total_length = 0;
        by_mcu.write.offset       = 0;
        by_mcu.download_status    = ATCMD_W_OTA_SUCCESS;
        by_mcu.version_check      = ATCMD_W_OTA_NOT_FOUND;
        by_mcu.content_length     = 0;
        by_mcu.received_length    = 0;
    }

 #if WIFI_CFG_WATCHDOG_SERVICE_ENABLE
    g_wifi_cfg.p_watchdog_service->p_api->unregisterTask(g_wifi_cfg.p_watchdog_service->p_ctrl, ota_wdog_id);
 #endif

    return status;
}

#endif                                 /* CFG_WIFI */

/* EOF */
