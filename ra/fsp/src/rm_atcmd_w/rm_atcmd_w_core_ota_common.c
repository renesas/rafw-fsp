/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
 #include "FreeRTOS.h"
 #include "rm_wifi.h"

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <strings.h>
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
 #endif                                /* CFG_PMGR */
 #include "rm_atcmd_w_core_ota_update.h"
 #include "rm_atcmd_w_core_ota_common.h"
 #include "rm_atcmd_w_core_ota_http.h"
 #include "rm_atcmd_w_core.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "net_common.h"

 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
 #endif

 #if (SUPPORT_FSP_RM_OTA_W == 1)
  #include "rm_ota_w.h"
 #endif                                /* SUPPORT_FSP_RM_OTA_W */

 #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #pragma GCC diagnostic ignored "-Wsign-conversion"

extern int interface_select;

static TaskHandle_t       at_ota_proc_xHandle = NULL;
static EventGroupHandle_t at_ota_event_group;

static atcmd_w_ota_update_proc_t   _at_ota_proc = {0, };
static atcmd_w_ota_update_proc_t * at_ota_proc  = &_at_ota_proc;

static UINT CUSTOM_SFLASH_ADDR = ATCMD_W_OTA_STOR_USER_START;
static UINT at_ota_refuse_flag = 0;

static atcmd_w_ctrl_t * g_p_at_ctrl = NULL;

 #define CRC_PRELOAD          0xFFFF
 #define CRC16_CCITT          0x1021

 #define OUTPUT_ASCII_ONLY    0
 #define OUTPUT_HEXA_ONLY     1
 #define OUTPUT_HEXA_ASCII    2

 #if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
 #endif                                /* SUPPORT_FSP_RM_OTA_W */

static void hex_dump_rm_atcmd_w (unsigned char * title, const void * buf, size_t len, char output_fmt)
{
    size_t                i, llen;
    const unsigned char * pos      = buf;
    const size_t          line_len = 16;
    int    hex_index               = 0;
    char * buf_prt                 = NULL;

    buf_prt = pvPortMalloc(64);

    if (buf_prt == NULL)
    {
        printf("[%s] Failed to allocate the temporary buffer ...\n", __func__);

        return;
    }

    if (output_fmt)
    {
        printf(">>> %s \n", title);
    }

    if (buf == NULL)
    {
        printf(" - hexdump%s(len=%lu): [NULL]\n", output_fmt == OUTPUT_HEXA_ONLY ? "" : "_ascii", (unsigned long) len);

        vPortFree(buf_prt);

        return;
    }

    if (output_fmt)
    {
        printf("- (len=%lu):\n", (unsigned long) len);
    }

    while (len)
    {
        char tmp_str[4];

        llen = len > line_len ? line_len : len;

        memset(buf_prt, 0, 64);

        if (output_fmt)
        {
            sprintf(buf_prt, "[%08x] ", hex_index);

            for (i = 0; i < llen; i++)
            {
                sprintf(tmp_str, " %02x", pos[i]);
                strcat(buf_prt, tmp_str);
            }

            hex_index = hex_index + i;

            for (i = llen; i < line_len; i++)
            {
                strcat(buf_prt, "   "); /* _xx */
            }

            printf("%s  ", buf_prt);

            memset(buf_prt, 0, 64);
        }

        if ((output_fmt == OUTPUT_HEXA_ASCII) || (output_fmt == OUTPUT_ASCII_ONLY))
        {
            for (i = 0; i < llen; i++)
            {
                if (((pos[i] >= 0x20) && (pos[i] < 0x7f)) ||
                    ((output_fmt == OUTPUT_ASCII_ONLY) && ((pos[i] == 0x0d) ||
                                                           (pos[i] == 0x0a) ||
                                                           (pos[i] == 0x0c))))
                {
                    sprintf(tmp_str, "%c", pos[i]);
                    strcat(buf_prt, tmp_str);
                }
                else if (output_fmt)
                {
                    strcat(buf_prt, ".");
                }
            }
        }

        if (output_fmt)
        {
            for (i = llen; i < line_len; i++)
            {
                strcat(buf_prt, " ");
            }

            strcat(buf_prt, "\n");
        }

        printf("%s", buf_prt);

        pos += llen;
        len -= llen;
    }

    vPortFree(buf_prt);
}

uint32_t atcmd_w_ota_update_crc16 (uint32_t crcValue, unsigned char newByte)
{
    for (unsigned char i = 0; i < 8; i++)
    {
        if (((crcValue & 0x8000) >> 8) ^ (newByte & 0x80))
        {
            crcValue = (crcValue << 1) ^ CRC16_CCITT;
        }
        else
        {
            crcValue = (crcValue << 1);
        }

        newByte <<= 1;
    }

    return crcValue;
}

uint16_t atcmd_w_ota_update_calc_crc16 (uint8_t * data, uint32_t size)
{
    uint32_t crc_calc;

    crc_calc = CRC_PRELOAD;

    for (unsigned char i = 0; i < size; i++)
    {
        crc_calc = atcmd_w_ota_update_crc16(crc_calc, data[i]);
    }

    return crc_calc & 0xFFFF;
}

const uint32_t atcmd_w_ota_update_crc32_tab[] =
{
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t atcmd_w_ota_update_crc32 (const void * buf, size_t size)
{
    const uint8_t * p = buf;
    uint32_t        crc;

    crc = ~0U;

    while (size--)
    {
        crc = atcmd_w_ota_update_crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ ~0U;
}

void atcmd_w_ota_update_set_boot_index (UINT boot_idx)
{
 #if (SUPPORT_FSP_RM_OTA_W == 1)
    p_ota_instance->p_api->bootIdxSet(p_ota_instance->p_ctrl, (uint8_t) boot_idx);
 #else
    RA6W1_UNUSED_ARG(boot_idx);
 #endif
}

UINT atcmd_w_ota_update_get_boot_index (void)
{
    uint8_t current_boot_idx = 0;
 #if (SUPPORT_FSP_RM_OTA_W == 1)
    p_ota_instance->p_api->bootIdxGet(p_ota_instance->p_ctrl, &current_boot_idx);
 #endif

    return (UINT) current_boot_idx;
}

UINT atcmd_w_ota_update_toggle_boot_index (void)
{
    UINT boot_idx = 0;

    boot_idx = atcmd_w_ota_update_get_boot_index();
    atcmd_w_ota_update_set_boot_index(!boot_idx);
    ATCMD_W_OTA_INFO(">>> %s is updated and system reboots. (New boot_idx=%d) <<<\n\n"
                     ,
                     ATCMD_W_OTA_RTOS_NAME
                     ,
                     atcmd_w_ota_update_get_boot_index());

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_check_refuse_flag (void)
{
    return at_ota_refuse_flag;
}

static void atcmd_w_ota_update_print_fw_header (image_header_data_t * infoImage)
{
    ATCMD_W_OTA_INFO("\tVersion-------- %s\n", infoImage->name);
    ATCMD_W_OTA_INFO("\tData Size------ %ld\n", infoImage->size);
    ATCMD_W_OTA_INFO("\tIVT------------ 0x%x\n", (unsigned int) (infoImage->ivt_location));
    ATCMD_W_OTA_INFO("\tHCRC----------- 0x%x\n", (unsigned int) (infoImage->header_crc));
    ATCMD_W_OTA_INFO("\tDCRC----------- 0x%x\n", (unsigned int) (infoImage->crc));
}

UINT atcmd_w_ota_update_get_available_size (atcmd_w_ota_update_type update_type)
{
    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        return ATCMD_W_OTA_STOR_RTOS_SIZE;
    }
    else if ((update_type == ATCMD_W_OTA_TYPE_MCU_FW) ||
             (update_type == ATCMD_W_OTA_TYPE_CERT_KEY))
    {
        if (CUSTOM_SFLASH_ADDR != ATCMD_W_OTA_STOR_USER_START)
        {
            return ATCMD_W_OTA_STOR_USER_END - CUSTOM_SFLASH_ADDR;
        }
        else
        {
            return ATCMD_W_OTA_STOR_USER_SIZE;
        }
    }
    else
    {
        ATCMD_W_OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);

        return 0;
    }
}

UINT atcmd_w_ota_update_check_available_size (atcmd_w_ota_update_type update_type, UINT size)
{
    UINT alloc_size = 0;

    if (update_type == ATCMD_W_OTA_TYPE_INIT)
    {
        return ATCMD_W_OTA_SUCCESS;
    }

    if (update_type >= ATCMD_W_OTA_TYPE_UNKNOWN)
    {
        ATCMD_W_OTA_ERR("- OTA: Unknown FW type\n");

        return ATCMD_W_OTA_ERROR_TYPE;
    }

    alloc_size = atcmd_w_ota_update_get_available_size(update_type);

    if (alloc_size <= size)
    {
        ATCMD_W_OTA_ERR("- OTA: <%s> FW size error. (Allowable size = %d, Receiving size = %d)\n",
                        atcmd_w_ota_update_type_to_text(update_type),
                        alloc_size,
                        size);

        return ATCMD_W_OTA_ERROR_SIZE;
    }

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_get_curr_sflash_addr (atcmd_w_ota_update_type update_type)
{
    if (update_type == ATCMD_W_OTA_TYPE_INIT)
    {
        return ATCMD_W_OTA_SUCCESS;
    }

    if (update_type >= ATCMD_W_OTA_TYPE_UNKNOWN)
    {
        ATCMD_W_OTA_ERR("- OTA: Unknown FW type\n");

        return ATCMD_W_OTA_ERROR_TYPE;
    }

    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        if (atcmd_w_ota_update_get_boot_index() == 1)
        {
            return ATCMD_W_OTA_STOR_RTOS_1_ADDR;
        }
        else
        {
            return ATCMD_W_OTA_STOR_RTOS_0_ADDR;
        }
    }
    else if ((update_type == ATCMD_W_OTA_TYPE_MCU_FW) ||
             (update_type == ATCMD_W_OTA_TYPE_CERT_KEY))
    {
        return CUSTOM_SFLASH_ADDR;
    }

    ATCMD_W_OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);

    return ATCMD_W_OTA_STOR_UNKNOWN_ADDR;
}

UINT atcmd_w_ota_update_get_new_sflash_addr (atcmd_w_ota_update_type update_type)
{
    if (update_type == ATCMD_W_OTA_TYPE_INIT)
    {
        return ATCMD_W_OTA_SUCCESS;
    }

    if (update_type >= ATCMD_W_OTA_TYPE_UNKNOWN)
    {
        ATCMD_W_OTA_ERR("- OTA: Unknown FW type\n");

        return ATCMD_W_OTA_ERROR_TYPE;
    }

    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        if (atcmd_w_ota_update_get_boot_index() == 1)
        {
            return ATCMD_W_OTA_STOR_RTOS_0_ADDR;
        }
        else
        {
            return ATCMD_W_OTA_STOR_RTOS_1_ADDR;
        }
    }
    else if ((update_type == ATCMD_W_OTA_TYPE_MCU_FW) ||
             (update_type == ATCMD_W_OTA_TYPE_CERT_KEY))
    {
        return CUSTOM_SFLASH_ADDR;
    }

    ATCMD_W_OTA_ERR("- OTA: Wrong FW type (%d)\n", update_type);

    return ATCMD_W_OTA_STOR_UNKNOWN_ADDR;
}

UINT atcmd_w_ota_update_set_user_sflash_addr (UINT sflash_addr)
{
    if (((sflash_addr >= SF_TLS_CERT_BASE_ADDR) && (sflash_addr < (SF_TLS_CERT_BASE_ADDR + SF_TLS_AREA_SIZE))) ||
        ((sflash_addr >= SF_USER_AREA) && (sflash_addr < (SF_USER_AREA + SF_USER_AREA_SIZE))))
    {
        ATCMD_W_OTA_INFO("- OTA : download_sflash_addr = 0x%x \n", sflash_addr);
        CUSTOM_SFLASH_ADDR = sflash_addr;
    }
    else
    {
        ATCMD_W_OTA_ERR("- OTA : sflash address(0x%x) is incorrect \n", sflash_addr);

        return ATCMD_W_OTA_ERROR_SFLASH_ADDR;
    }

    return ATCMD_W_OTA_SUCCESS;
}

const char * atcmd_w_ota_update_type_to_text (atcmd_w_ota_update_type update_type)
{
    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        return ATCMD_W_OTA_RTOS_NAME;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        return ATCMD_W_OTA_MCU_FW_NAME;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_CERT_KEY)
    {
        return ATCMD_W_OTA_CERT_KEY_NAME;
    }

    return "UNKNOWN";
}

UINT atcmd_w_ota_update_parse_version_string (UCHAR * version, AT_FW_versionInfo_t * fw_ver)
{
    CHAR * pRev_a        = NULL;
    CHAR * pRev_b        = NULL;
    UINT   str_len       = 0;
    UINT   total_str_len = 0;
    UINT   sum_str_len   = 0;
    UINT   status        = ATCMD_W_OTA_SUCCESS;

    if ((version == NULL) || (fw_ver == NULL))
    {
        return ATCMD_W_OTA_FAILED;
    }

    memset(fw_ver, 0x00, sizeof(AT_FW_versionInfo_t));

    total_str_len = strlen((char *) version);

    /* Extract FW_Type */
    pRev_a = (CHAR *) version;
    pRev_b = (CHAR *) strstr((char *) pRev_a, ATCMD_W_OTA_VER_DELIMITER);
    if (pRev_b == NULL)
    {
        ATCMD_W_OTA_DBG("  > Delimiter (%s) not found\n", ATCMD_W_OTA_VER_DELIMITER);

        return ATCMD_W_OTA_FAILED;
    }

    str_len = pRev_b - pRev_a;
    if (str_len > UPDATE_TYPE_MAX)
    {
        ATCMD_W_OTA_DBG("  > FW_TYPE is too long (max = %d)\n", UPDATE_TYPE_MAX);
        status = ATCMD_W_OTA_FAILED;
    }

    memcpy(fw_ver->update_type, pRev_a, str_len > UPDATE_TYPE_MAX ? UPDATE_TYPE_MAX : str_len);

    sum_str_len += str_len;

    /* Extract Module name */
    pRev_b++;
    sum_str_len++;
    pRev_a = (CHAR *) strstr((char *) pRev_b, ATCMD_W_OTA_VER_DELIMITER);
    if (pRev_a == NULL)
    {
        ATCMD_W_OTA_DBG("  > Delimiter (%s) not found\n", ATCMD_W_OTA_VER_DELIMITER);

        return ATCMD_W_OTA_FAILED;
    }

    str_len      = pRev_a - pRev_b;
    sum_str_len += str_len;
    if (str_len > MODULE_MAX)
    {
        ATCMD_W_OTA_DBG("  > Module name is too long (max = %d)\n", MODULE_MAX);
        status = ATCMD_W_OTA_FAILED;
    }

    memcpy(fw_ver->module, pRev_b, str_len > MODULE_MAX ? MODULE_MAX : str_len);

    /* Extract SDK version */
    pRev_a++;
    sum_str_len++;
    pRev_b = (CHAR *) strstr((char *) pRev_a, ATCMD_W_OTA_VER_DELIMITER);
    if (pRev_b == NULL)
    {
        ATCMD_W_OTA_DBG("  > Delimiter (%s) not found\n", ATCMD_W_OTA_VER_DELIMITER);

        return ATCMD_W_OTA_FAILED;
    }

    str_len      = pRev_b - pRev_a;
    sum_str_len += str_len;
    if (str_len > SDK_MAX)
    {
        ATCMD_W_OTA_DBG("  > SDK version is too long (max = %d)\n", SDK_MAX);
        status = ATCMD_W_OTA_FAILED;
    }

    memcpy(fw_ver->sdk, pRev_a, str_len > SDK_MAX ? SDK_MAX : str_len);

    /* Extract Customer Version */
    pRev_b++;
    sum_str_len++;
    str_len = total_str_len - sum_str_len;
    if (str_len > CUSTOMER_MAX)
    {
        ATCMD_W_OTA_DBG("  > CUSTOMER is too long (max = %d)\n", CUSTOMER_MAX);
        status = ATCMD_W_OTA_FAILED;
    }

    memcpy(fw_ver->customer, pRev_b, str_len > CUSTOMER_MAX ? CUSTOMER_MAX : str_len);

    if (!strlen((char *) fw_ver->update_type) ||
        !strlen((char *) fw_ver->module) ||
        !strlen((char *) fw_ver->sdk) ||
        !strlen((char *) fw_ver->customer))
    {
        memset(fw_ver, 0x00, sizeof(AT_FW_versionInfo_t));

        return ATCMD_W_OTA_FAILED;
    }

    return status;
}

static UINT atcmd_w_ota_update_read_new_fw_version (UCHAR * data, AT_FW_versionInfo_t * fw_ver)
{
    UCHAR new_ver[IMAGE_HEADER_NAME_LEN] = {0x00, };

    if (data == NULL)
    {
        return ATCMD_W_OTA_FAILED;
    }

    memset(new_ver, 0x00, IMAGE_HEADER_NAME_LEN);
    memcpy(new_ver, &data[ATCMD_W_OTA_VER_START_OFFSET], IMAGE_HEADER_NAME_LEN);

    /* parse received version */
    if (atcmd_w_ota_update_parse_version_string(new_ver, fw_ver))
    {
        ATCMD_W_OTA_ERR("   > Failed to parse Server FW version : %s \n", new_ver);

        return ATCMD_W_OTA_VERSION_UNKNOWN;
    }
    else
    {
        ATCMD_W_OTA_INFO("   > Server FW version : %s-%s-%s-%s \n",
                         fw_ver->update_type,
                         fw_ver->module,
                         fw_ver->sdk,
                         fw_ver->customer);
    }

    return ATCMD_W_OTA_SUCCESS;
}

static UINT atcmd_w_ota_update_read_current_fw_version (UINT fw_addr, AT_FW_versionInfo_t * fw_ver)
{
    image_header_data_t infoImage = {0, };

    if (fw_ver == NULL)
    {
        return ATCMD_W_OTA_FAILED;
    }

    /* READ SFLASH */
    if (atcmd_w_ota_update_get_image_info(fw_addr, &infoImage) == 0)
    {
        return ATCMD_W_OTA_FAILED;
    }

    /* PARSE VERION */
    if (atcmd_w_ota_update_parse_version_string(infoImage.name, fw_ver))
    {
        ATCMD_W_OTA_ERR("   > Failed to parse Current FW version : %s \n", infoImage.name);
    }

    return ATCMD_W_OTA_SUCCESS;
}

static UINT atcmd_w_ota_update_compare_fw_version (atcmd_w_ota_update_type update_type,
                                                   AT_FW_versionInfo_t     cur_ver,
                                                   AT_FW_versionInfo_t     new_ver)
{
    UINT ver_check_bit = 0x00;

    ATCMD_W_OTA_INFO("- OTA Update : <%s> Compare Versions\n", atcmd_w_ota_update_type_to_text(update_type));

    /* update_type */
    if (memcmp(new_ver.update_type, cur_ver.update_type, strlen((char *) new_ver.update_type)))
    {
        ver_check_bit = BIT(ATCMD_W_OTA_HEADER_DIFF_FW_TYPE);
        ATCMD_W_OTA_INFO("   > Incompatible Image type : %s\n", new_ver.update_type);
        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(ATCMD_W_OTA_HEADER_SAME_FW_TYPE);
    }

    /* module name */
    if (memcmp(new_ver.module, cur_ver.module, strlen((char *) new_ver.module)))
    {
        ver_check_bit = BIT(ATCMD_W_OTA_HEADER_DIFF_MODULE);
        ATCMD_W_OTA_INFO("   > Incompatible Image module : %s\n", new_ver.module);
        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(ATCMD_W_OTA_HEADER_SAME_MODULE);
    }

    /* SDK version */
    if (memcmp(new_ver.sdk, cur_ver.sdk, strlen((char *) new_ver.sdk)))
    {
        ATCMD_W_OTA_INFO("   > Different SDK ver : Cur-%s, New-%s \n", cur_ver.sdk, new_ver.sdk);
        ver_check_bit |= BIT(ATCMD_W_OTA_HEADER_DIFF_SDK);
    }

    /* customer version */
    if (memcmp(new_ver.customer, cur_ver.customer, strlen((char *) new_ver.customer)))
    {
        ATCMD_W_OTA_INFO("   > Different Customer : Cur-%s, New-%s\n", cur_ver.customer, new_ver.customer);
        ver_check_bit |= BIT(ATCMD_W_OTA_HEADER_DIFF_CUST);
    }

    if (ver_check_bit & BIT(ATCMD_W_OTA_HEADER_ERROR))
    {
        ATCMD_W_OTA_INFO("   > Version comparison failed\n");
        goto chk_finish;
    }

    if (((ver_check_bit & BIT(ATCMD_W_OTA_HEADER_DIFF_SDK)) == 0) &&
        ((ver_check_bit & BIT(ATCMD_W_OTA_HEADER_DIFF_CUST)) == 0))
    {
        ATCMD_W_OTA_INFO("   > Same Version : %s-%s-%s-%s \n",
                         new_ver.update_type,
                         new_ver.module,
                         new_ver.sdk,
                         new_ver.customer);
    }

chk_finish:

    return ver_check_bit;
}

UINT atcmd_w_ota_update_check_version (atcmd_w_ota_update_type update_type, UCHAR * data, UINT data_len)
{
    AT_FW_versionInfo_t curr_ver;
    AT_FW_versionInfo_t rev_ver;
    UINT                ret_val  = 0;
    CHAR                magic[4] = ATCMD_W_OTA_FW_MAGIC_NUM;
    uint32_t            addr     = 0x00;

 #if defined(DISABLE_ATCMD_W_OTA_VER_CHK)
    ATCMD_W_OTA_ERR("- OTA: NO Version check!!\n");

    return ATCMD_W_OTA_SUCCESS;
 #endif                                // (DISABLE_ATCMD_W_OTA_VER_CHK)

    if ((data == NULL) || (data_len == 0))
    {
        ATCMD_W_OTA_ERR("- OTA: Unknown version\n");

        return ATCMD_W_OTA_VERSION_UNKNOWN;
    }

    ATCMD_W_OTA_DBG("[%s]update_type = %d, data_len = %d, data = %s\n", __func__, update_type, data_len, data);

    if (update_type == ATCMD_W_OTA_TYPE_INIT)
    {
        return ATCMD_W_OTA_SUCCESS;
    }
    else if (update_type >= ATCMD_W_OTA_TYPE_UNKNOWN)
    {
        ATCMD_W_OTA_ERR("- OTA: Unknown FW type\n");

        return ATCMD_W_OTA_ERROR_TYPE;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        if (data_len < sizeof(AT_FW_versionInfo_t))
        {
            ATCMD_W_OTA_ERR("- OTA: Data size is too small to check version \n");

            return ATCMD_W_OTA_VERSION_UNKNOWN;
        }

        /* GET CURRENT VERSION */
 #if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl,
                                       RM_OTA_W_CURRENT_ADDR,
                                       (rm_ota_w_update_type_t) update_type,
                                       &addr);
 #endif

        if (atcmd_w_ota_update_read_current_fw_version(addr, &curr_ver) != ATCMD_W_OTA_SUCCESS)
        {
            ATCMD_W_OTA_ERR("- OTA: Failed to read current version \n");
        }

        /* GET RECEIVED VERSION */
        if (atcmd_w_ota_update_read_new_fw_version(data, &rev_ver) != ATCMD_W_OTA_SUCCESS)
        {
            return ATCMD_W_OTA_VERSION_UNKNOWN;
        }

        /* CHECK MAGIC NUMBER */
        if (memcmp(&data[0], &magic[0], 4) != 0)
        {
            ATCMD_W_OTA_ERR("- OTA: Wrong magic number (0x%02x 0x%02x 0x%02x 0x%02x)\n",
                            data[0],
                            data[1],
                            data[2],
                            data[3]);

            return ATCMD_W_OTA_VERSION_UNKNOWN;
        }

        /* COMPARE CURRENT AND RECEIVED */
        ret_val = atcmd_w_ota_update_compare_fw_version(update_type, curr_ver, rev_ver);

        if ((ret_val & BIT(ATCMD_W_OTA_HEADER_DIFF_FW_TYPE)) ||
            (ret_val & BIT(ATCMD_W_OTA_HEADER_DIFF_MODULE)) ||
            (ret_val & BIT(ATCMD_W_OTA_HEADER_INCOMPATI_MAGIC)) ||
            (ret_val & BIT(ATCMD_W_OTA_HEADER_INIT)) ||
            (ret_val & BIT(ATCMD_W_OTA_HEADER_ERROR)))
        {
            return ATCMD_W_OTA_VERSION_UNKNOWN;
        }
    }

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_update_current_fw_renew (void)
{
    UCHAR * rd_buf = NULL;
    UINT    len    = 0;
    UINT    status = ATCMD_W_OTA_FAILED;

    ATCMD_W_OTA_INFO("\n- OTA Update : Renew - Start\n");

    if (atcmd_w_ota_update_check_refuse_flag())
    {
        ATCMD_W_OTA_ERR("- OTA: Try again after reboot\n\n");
        status = ATCMD_W_OTA_FAILED;
        goto _renew_fail;
    }

    if ((atcmd_w_ota_update_get_download_progress(ATCMD_W_OTA_TYPE_RTOS) == 100) ||
        (atcmd_w_ota_update_read_nvram_download_progress(ATCMD_W_OTA_TYPE_RTOS) == 100))
    {
        /* CRC - RTOS */
        if (atcmd_w_ota_update_sflash_rtos_crc(atcmd_w_ota_update_get_new_sflash_addr(ATCMD_W_OTA_TYPE_RTOS)) !=
            ATCMD_W_OTA_SUCCESS)
        {
            ATCMD_W_OTA_ERR("- OTA: <%s> CRC Error\n", ATCMD_W_OTA_RTOS_NAME);
            status = ATCMD_W_OTA_ERROR_CRC;
            goto _renew_fail;
        }

        len    = 80;                   /* byte */
        rd_buf = ATCMD_W_OTA_MALLOC(len);

        if (rd_buf != NULL)
        {
            if (atcmd_w_ota_update_read_flash(atcmd_w_ota_update_get_new_sflash_addr(ATCMD_W_OTA_TYPE_RTOS), rd_buf,
                                              len) != 0)
            {
                if (atcmd_w_ota_update_check_version(ATCMD_W_OTA_TYPE_RTOS, rd_buf, len) != ATCMD_W_OTA_SUCCESS)
                {
                    ATCMD_W_OTA_ERR("- OTA: <%s> Incompatible new version\n", ATCMD_W_OTA_RTOS_NAME);
                    status = ATCMD_W_OTA_VERSION_INCOMPATI;
                    ATCMD_W_OTA_FREE(rd_buf);
                    goto _renew_fail;
                }
            }
            else
            {
                ATCMD_W_OTA_ERR("- OTA: <%s> Failed to read new version\n", ATCMD_W_OTA_RTOS_NAME);
                status = ATCMD_W_OTA_FAILED;
                ATCMD_W_OTA_FREE(rd_buf);
                goto _renew_fail;
            }

            ATCMD_W_OTA_FREE(rd_buf);
        }
        else
        {
            ATCMD_W_OTA_ERR("[%s:%d] Failed to allocate read buffer(%d bytes)\n", __func__, __LINE__, len);
            status = ATCMD_W_OTA_MEM_ALLOC_FAILED;
            goto _renew_fail;
        }

        /* RENEW - Toggle the boot index */
        if (atcmd_w_ota_update_toggle_boot_index() == ATCMD_W_OTA_SUCCESS)
        {
            atcmd_w_ota_update_write_nvram_download_progress(ATCMD_W_OTA_TYPE_RTOS, 0);

            return ATCMD_W_OTA_SUCCESS;
        }
    }
    else
    {
        ATCMD_W_OTA_INFO("- OTA: Try to download F/W images\n");
        status = ATCMD_W_OTA_NOT_ALL_DOWNLOAD;
        goto _renew_fail;
    }

_renew_fail:

    atcmd_w_ota_update_write_nvram_download_progress(ATCMD_W_OTA_TYPE_RTOS, 0);

    /* RENEW FAIL */
    ATCMD_W_OTA_ERR("\n>>> OTA FW update failed(0x%02x) <<<\n\n", status);

    return status;
}

UINT atcmd_w_ota_update_get_image_info (UINT sectorAddr, image_header_data_t * infoImage)
{
    return atcmd_w_ota_update_read_flash(sectorAddr, (void *) infoImage, sizeof(image_header_data_t));
}

UINT atcmd_w_ota_update_sflash_rtos_crc (UINT sectorAddr)
{
    uint32_t            addr_offset = 0;
    uint32_t            tot_len     = 0;
    uint32_t            cal_len     = 0;
    uint32_t            cal_crc     = 0;
    uint32_t            crc;
    uint32_t            retry_crc = 0;
    size_t              size;
    UINT                status = ATCMD_W_OTA_SUCCESS;
    image_header_data_t infoImage;

    unsigned char * buf = NULL;

    buf = ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_SFLASH_BUF_SZ);

    if (buf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, ATCMD_W_OTA_SFLASH_BUF_SZ);

        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
    }

    if (atcmd_w_ota_update_get_image_info(sectorAddr, &infoImage))
    {
        atcmd_w_ota_update_print_fw_header(&infoImage);
    }
    else
    {
        ATCMD_W_OTA_ERR("[%s:%d] Failed to get image info\n", __func__, __LINE__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

retry:
    addr_offset = sectorAddr + infoImage.ivt_location;
    tot_len     = (int) infoImage.size;

    crc = ~0U;

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
        atcmd_w_ota_update_read_flash(addr_offset, (uint8_t *) buf, cal_len);

        uint8_t * p = buf;
        size = cal_len;

        while (size--)
        {
            crc = atcmd_w_ota_update_crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }

        addr_offset += cal_len;
        tot_len     -= cal_len;
    }

    cal_crc = crc ^ ~0U;

    if (infoImage.crc != cal_crc)
    {
        if (retry_crc++ <= 2)
        {
            ATCMD_W_OTA_INFO("\tRecalculate due to CRC error(%ld)\n", retry_crc);
            goto retry;
        }

        ATCMD_W_OTA_INFO("  CRC: CRC mismatch!!(0x%lx != 0x%lx)\n", infoImage.crc, cal_crc);
        status = ATCMD_W_OTA_ERROR_CRC;
    }
    else
    {
        ATCMD_W_OTA_INFO("\tDCRC(calc)----- 0x%lx\n", cal_crc);
    }

finish:

    if (buf != NULL)
    {
        ATCMD_W_OTA_FREE(buf);
    }

    return status;
}

UINT atcmd_w_ota_update_sflash_product_header_crc (void)
{
    uint16_t        read_crc = 0;
    uint16_t        cal_crc;
    unsigned char   header_crc[2] = {0x00, };
    unsigned char * buf           = NULL;

    buf = ATCMD_W_OTA_MALLOC(SF_PRODUCT_HDR_SIZE);

    if (buf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, SF_PRODUCT_HDR_SIZE);

        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
    }

    memset(buf, 0x00, SF_PRODUCT_HDR_SIZE);
    atcmd_w_ota_update_read_flash(SF_PRODUCT_HDR, buf, SF_PRODUCT_HDR_SIZE);

    atcmd_w_ota_update_read_flash(SF_PRODUCT_HDR + 29, header_crc, sizeof(header_crc));
    read_crc = ((int) header_crc[0] << 0) |
               ((int) header_crc[1] << 8);
    ATCMD_W_OTA_INFO("CRC: <0x%08x> Read PRODUCT_HDR CRC = 0x%08x\n", SF_PRODUCT_HDR + 29, read_crc);

    cal_crc = atcmd_w_ota_update_calc_crc16(buf, 29);
    ATCMD_W_OTA_INFO("CRC: Calculated PRODUCT_HDR CRC = 0x%08x\n", cal_crc);

    if (buf != NULL)
    {
        ATCMD_W_OTA_FREE(buf);
        buf = NULL;
    }

    if (read_crc != cal_crc)
    {
        ATCMD_W_OTA_INFO("CRC: CRC mismatch!!\n");

        return ATCMD_W_OTA_ERROR_CRC;
    }

    return ATCMD_W_OTA_SUCCESS;
}

static UINT atcmd_w_ota_update_set_proc_state (UINT state)
{
    ATCMD_W_OTA_DBG("[%s]state = %d\n", __func__, state);

    return at_ota_proc->update_state = state;
}

UINT atcmd_w_ota_update_get_proc_state (void)
{
    return at_ota_proc->update_state;
}

void atcmd_w_ota_update_print_status (atcmd_w_ota_update_type update_type, UINT status)
{
    if (status == ATCMD_W_OTA_SUCCESS)
    {
        if (atcmd_w_ota_update_get_proc_state() == ATCMD_W_OTA_STATE_STOP)
        {
            ATCMD_W_OTA_INFO("\n- OTA Update : <%s> Download - Stop\n\n", atcmd_w_ota_update_type_to_text(update_type));
        }
        else
        {
            ATCMD_W_OTA_INFO("\n- OTA Update : <%s> Download - Success\n\n",
                             atcmd_w_ota_update_type_to_text(update_type));
        }
    }
    else
    {
        ATCMD_W_OTA_ERR("- OTA Update : <%s> Download - Failed (0x%02x)\n",
                        atcmd_w_ota_update_type_to_text(update_type),
                        status);
    }
}

UINT atcmd_w_ota_update_get_download_progress (atcmd_w_ota_update_type update_type)
{
    vTaskDelay(portCONVERT_MS_2_TICKS(10));

    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        return at_ota_proc->progress_rtos;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        return at_ota_proc->progress_mcu_fw;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_CERT_KEY)
    {
        return at_ota_proc->progress_cert_key;
    }

    return 0;
}

void atcmd_w_ota_update_set_download_progress (atcmd_w_ota_update_type update_type, UINT progress)
{
    if (update_type == ATCMD_W_OTA_TYPE_RTOS)
    {
        at_ota_proc->progress_rtos = progress;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        at_ota_proc->progress_mcu_fw = progress;
    }
    else if (update_type == ATCMD_W_OTA_TYPE_CERT_KEY)
    {
        at_ota_proc->progress_cert_key = progress;
    }
}

UINT atcmd_w_ota_update_read_nvram_download_progress (atcmd_w_ota_update_type update_type)
{
    char nvr_name[32] = {0, };
    int  progress     = 0;

    sprintf(nvr_name, "%s%s", ATCMD_W_OTA_NVRAM_DW_PROGRESS, atcmd_w_ota_update_type_to_text(update_type));
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &progress);
 #endif

    if (progress <= 0)
    {
        progress = 0;
    }

    return progress;
}

void atcmd_w_ota_update_write_nvram_download_progress (atcmd_w_ota_update_type update_type, UINT progress)
{
    char nvr_name[32] = {0, };

    sprintf(nvr_name, "%s%s", ATCMD_W_OTA_NVRAM_DW_PROGRESS, atcmd_w_ota_update_type_to_text(update_type));
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, progress);
 #endif
}

 #if defined(__SUPPORT_OTA_RESUME__)
UINT32 atcmd_w_ota_update_url_fingerprint (const char * url)
{
    if ((url == NULL) || (url[0] == '\0'))
    {
        return 0;
    }

    /* Reuse the firmware CRC32 routine as a cheap, stable URL identity hash. */
    return (UINT32) atcmd_w_ota_update_crc32((const void *) url, strlen(url));
}

void atcmd_w_ota_update_write_nvram_resume (atcmd_w_ota_update_type update_type,
                                            UINT                    off,
                                            UINT                    total,
                                            UINT32                  fingerprint,
                                            UINT32                  validator)
{
    char         nvr_name[32] = {0, };
    const char * suffix       = atcmd_w_ota_update_type_to_text(update_type);

  #ifdef RM_MAP_PERSISTANT_W
    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_OFFSET, suffix);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, (int) off);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_TOTAL, suffix);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, (int) total);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_FP, suffix);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, (int) fingerprint);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_VAL, suffix);
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, (int) validator);
  #else
    (void) off;
    (void) total;
    (void) fingerprint;
    (void) validator;
    (void) suffix;
  #endif
}

UINT atcmd_w_ota_update_read_nvram_resume (atcmd_w_ota_update_type update_type,
                                           UINT                  * off,
                                           UINT                  * total,
                                           UINT32                * fingerprint,
                                           UINT32                * validator)
{
    char         nvr_name[32] = {0, };
    const char * suffix       = atcmd_w_ota_update_type_to_text(update_type);
    int          v_off        = 0;
    int          v_total      = 0;
    int          v_fp         = 0;
    int          v_val        = 0;

  #ifdef RM_MAP_PERSISTANT_W
    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_OFFSET, suffix);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &v_off);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_TOTAL, suffix);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &v_total);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_FP, suffix);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &v_fp);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_VAL, suffix);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &v_val);
  #endif

    if (v_off <= 0)
    {
        return ATCMD_W_OTA_FAILED;
    }

    if (off != NULL)
    {
        *off = (UINT) v_off;
    }

    if (total != NULL)
    {
        *total = (UINT) v_total;
    }

    if (fingerprint != NULL)
    {
        *fingerprint = (UINT32) v_fp;
    }

    if (validator != NULL)
    {
        *validator = (UINT32) v_val;
    }

    return ATCMD_W_OTA_SUCCESS;
}

void atcmd_w_ota_update_clear_nvram_resume (atcmd_w_ota_update_type update_type)
{
    char         nvr_name[32] = {0, };
    const char * suffix       = atcmd_w_ota_update_type_to_text(update_type);

  #ifdef RM_MAP_PERSISTANT_W
    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_OFFSET, suffix);
    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_TOTAL, suffix);
    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_FP, suffix);
    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);

    snprintf(nvr_name, sizeof(nvr_name), "%s%s", ATCMD_W_OTA_NVRAM_RSM_VAL, suffix);
    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
  #endif
}

 #endif                                /* __SUPPORT_OTA_RESUME__ */

static UINT atcmd_w_ota_update_evt_wait (int timeout)
{
    ULONG events;

    ATCMD_W_OTA_DBG("[%s]Event wait timeout = %d secs\n", __func__, timeout);

    while (1)
    {
        events = xEventGroupWaitBits(at_ota_event_group,
                                     ATCMD_W_OTA_EVT_RECEIVE | ATCMD_W_OTA_EVT_FINISH,
                                     pdTRUE,
                                     pdFALSE,
                                     portCONVERT_MS_2_TICKS(timeout * 1000));

        if (events == ATCMD_W_OTA_EVT_RECEIVE)
        {
            continue;
        }
        else if (events == ATCMD_W_OTA_EVT_FINISH)
        {
            ATCMD_W_OTA_DBG("[%s] ATCMD_W_OTA_EVT_FINISH = 0x%lx\n", __func__, events);
            break;
        }
        else
        {
            ATCMD_W_OTA_DBG("[%s] Error event = 0x%lx\n", __func__, events);
            if (events == ATCMD_W_OTA_EVT_TIMEOUT)
            {
                atcmd_w_ota_http_client_set_download_status(ATCMD_W_OTA_FAILED_TIMEOUT);
                break;
            }

            atcmd_w_ota_http_client_set_download_status(ATCMD_W_OTA_FAILED);
            break;
        }
    }

    if (at_ota_event_group)
    {
        vEventGroupDelete(at_ota_event_group);
        at_ota_event_group = NULL;
    }

    return atcmd_w_ota_http_client_get_download_status();
}

void atcmd_w_ota_update_evt_send (UINT event)
{
    if (at_ota_event_group != NULL)
    {
        xEventGroupSetBits(at_ota_event_group, event);
    }
}

UINT atcmd_w_ota_update_process_stop (void)
{
    UINT status = ATCMD_W_OTA_SUCCESS;
    UINT curr_state;
    UINT wait_cnt = 0;

    curr_state = atcmd_w_ota_update_get_proc_state();
    if (curr_state == ATCMD_W_OTA_STATE_PROGRESS)
    {
        atcmd_w_ota_update_set_proc_state(ATCMD_W_OTA_STATE_STOP);
        while (atcmd_w_ota_http_client_get_result() != HTTPC_RESULT_LOCAL_ABORT)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
            wait_cnt++;

            if (wait_cnt == 100)
            {
                ATCMD_W_OTA_DBG("[%s] Wait to stop ...(%d)\n", __func__, wait_cnt / 10);
                ATCMD_W_OTA_ERR("- OTA : Failed to stop previous execution\n");
                wait_cnt = 0;
                status   = ATCMD_W_OTA_FAILED;
                break;
            }
            else if ((wait_cnt % 100) == 0)
            {
                ATCMD_W_OTA_DBG("[%s] Wait to stop ...(%d)\n", __func__, wait_cnt / 10);
            }
        }
    }
    else if ((curr_state == ATCMD_W_OTA_STATE_FINISH) || (curr_state == ATCMD_W_OTA_STATE_STOP))
    {
        ATCMD_W_OTA_ERR("- OTA : In progressing... Please wait.\n");
        status = ATCMD_W_OTA_FAILED;
    }
    else
    {
        ATCMD_W_OTA_DBG("- OTA : No operation\n");
        status = ATCMD_W_OTA_FAILED;
    }

    atcmd_w_ota_http_client_set_download_status(status);
    atcmd_w_ota_update_evt_send(ATCMD_W_OTA_EVT_FINISH);

    return status;
}

UINT atcmd_w_ota_update_check_state (void)
{
    UINT curr_state;

    curr_state = atcmd_w_ota_update_get_proc_state();

    if (curr_state == ATCMD_W_OTA_STATE_PROGRESS)
    {
        ATCMD_W_OTA_ERR("- OTA : %s is downloading... Try again after finishing.\n",
                        atcmd_w_ota_update_type_to_text(at_ota_proc->update_type));

        return ATCMD_W_OTA_FAILED;
    }
    else if ((curr_state == ATCMD_W_OTA_STATE_FINISH) || (curr_state == ATCMD_W_OTA_STATE_STOP))
    {
        ATCMD_W_OTA_ERR("- OTA : In progressing(%d)... Please wait.\n", curr_state);

        return ATCMD_W_OTA_FAILED;
    }

    return ATCMD_W_OTA_SUCCESS;
}

static void atcmd_w_ota_update_process (void * arg)
{
    RA6W1_UNUSED_ARG(arg);
    const int evt_timeout = ATCMD_W_OTA_TIMEOUT;
    char      atc_buf[32] = {0, };
    uint32_t  progress;

 #if CFG_PMGR

    /* DPM mode */
    RM_PMGR_W_dpm_job_name_set(ATCMD_W_OTA_DPM_REG_NAME, 0);
 #endif                                /* CFG_PMGR */

    at_ota_event_group = xEventGroupCreate();

    if (at_ota_event_group == NULL)
    {
        ATCMD_W_OTA_ERR("- OTA : Failed to create event_flags\n");
        at_ota_proc->status = ATCMD_W_OTA_FAILED;
        goto download_finish;
    }

    ATCMD_W_OTA_DBG("[%s] update_type = %s\n", __func__, atcmd_w_ota_update_type_to_text(at_ota_proc->update_type));
    atcmd_w_ota_update_set_proc_state(ATCMD_W_OTA_STATE_PROGRESS);
    atcmd_w_ota_update_set_download_progress(at_ota_proc->update_type, 0);

    /*******************************/
    /* HTTP Client REQUEST */
    /*******************************/
    at_ota_proc->status = atcmd_w_ota_update_http_client_request(at_ota_proc);

    if (at_ota_proc->status != ATCMD_W_OTA_SUCCESS)
    {
        goto download_finish;
    }

    /* Wait Event */
    ATCMD_W_OTA_DBG("[%s] Wait for an event... \n", __func__);
    at_ota_proc->status = atcmd_w_ota_update_evt_wait(evt_timeout);

download_finish:
    memset(atc_buf, 0x00, sizeof(atc_buf));
    sprintf(atc_buf, "+NWOTADWSTART:0x%02x\r\n", at_ota_proc->status);
    RM_ATCMD_W_CORE_Write(g_p_at_ctrl, (uint8_t *) atc_buf, strlen(atc_buf));
    atcmd_w_ota_update_print_status(at_ota_proc->update_type, at_ota_proc->status);
    progress = atcmd_w_ota_update_get_download_progress(at_ota_proc->update_type);

    if (at_ota_proc->download_notify != NULL)
    {
        at_ota_proc->download_notify(at_ota_proc->update_type, at_ota_proc->status, progress);
    }

    atcmd_w_ota_update_set_proc_state(ATCMD_W_OTA_STATE_READY);
    ATCMD_W_OTA_DBG("[%s] RENEW available(status = 0x%02x, auto_renew = %d, proc_state = %d)\n",
                    __func__,
                    at_ota_proc->status,
                    at_ota_proc->auto_renew,
                    atcmd_w_ota_update_get_proc_state());

    if ((at_ota_proc->status == ATCMD_W_OTA_SUCCESS) &&
        (atcmd_w_ota_update_get_proc_state() != ATCMD_W_OTA_STATE_STOP))
    {
        if (at_ota_proc->auto_renew > 0)
        {
 #if (SUPPORT_FSP_RM_OTA_W == 1)
            p_ota_instance->p_api->swap(p_ota_instance->p_ctrl);
 #endif
        }
        else
        {
            if ((at_ota_proc->update_type == ATCMD_W_OTA_TYPE_RTOS) &&
                (atcmd_w_ota_update_get_download_progress(at_ota_proc->update_type) == 100))
            {
                atcmd_w_ota_update_write_nvram_download_progress(at_ota_proc->update_type,
                                                                 atcmd_w_ota_update_get_download_progress(at_ota_proc->
                                                                                                          update_type));
            }
        }
    }

 #if CFG_PMGR

    /* DPM mode */
    RM_PMGR_W_dpm_job_name_clear(ATCMD_W_OTA_DPM_REG_NAME);
 #endif                                /* CFG_PMGR */

    while (1)
    {
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    }
}

UINT atcmd_w_ota_update_process_create (atcmd_w_ctrl_t * const p_at_ctrl, ATCMD_W_OTA_UPDATE_CONFIG * update_conf)
{
    UINT         status   = ATCMD_W_OTA_SUCCESS;
    UINT         wait_cnt = 0;
    UINT         addr     = 0;
    BaseType_t   xReturned;
    TaskHandle_t task_handle;

    if (atcmd_w_ota_update_get_proc_state() == ATCMD_W_OTA_STATE_PROGRESS)
    {
        if (atcmd_w_ota_update_process_stop() != ATCMD_W_OTA_SUCCESS)
        {
            return ATCMD_W_OTA_FAILED;
        }

        vTaskDelay(portCONVERT_MS_2_TICKS(100));
    }

    ATCMD_W_OTA_INFO("\n- OTA Update : <%s> Download - Start\n",
                     atcmd_w_ota_update_type_to_text(update_conf->update_type));

    wait_cnt = 0;
    while (chk_network_ready((UCHAR) interface_select) != pdTRUE)
    {
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
        wait_cnt++;

        if (wait_cnt == 100)
        {
            ATCMD_W_OTA_DBG("[%s] Wait to initialize WLAN ...(%d)\n", __func__, wait_cnt / 10);
            ATCMD_W_OTA_ERR("- OTA : No network connection\n");
            wait_cnt = 0;

            return ATCMD_W_OTA_FAILED;
        }
        else if ((wait_cnt % 100) == 0)
        {
            ATCMD_W_OTA_DBG("[%s] Wait to initialize WLAN ...(%d)\n", __func__, wait_cnt / 10);
        }
    }

    if (atcmd_w_ota_update_check_refuse_flag())
    {
        ATCMD_W_OTA_ERR("- OTA: Try again after rebooting.\n");

        return ATCMD_W_OTA_FAILED;
    }

    at_ota_proc->status = ATCMD_W_OTA_SUCCESS;

    if ((update_conf->update_type == ATCMD_W_OTA_TYPE_RTOS) ||
        (update_conf->update_type == ATCMD_W_OTA_TYPE_BLE_COMBO))
    {
        at_ota_proc->progress_rtos = 0;
    }

    if (update_conf->update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        at_ota_proc->progress_mcu_fw = 0;
 #if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl,
                                       RM_OTA_W_CURRENT_ADDR,
                                       RM_OTA_W_TYPE_MCU_FW,
                                       (uint32_t *) &addr);
 #endif
        if (addr > 0)
        {
            status = atcmd_w_ota_update_set_user_sflash_addr(addr);
            if (status)
            {
                return status;
            }
        }
    }

    if (update_conf->update_type == ATCMD_W_OTA_TYPE_CERT_KEY)
    {
        at_ota_proc->progress_cert_key = 0;
    }

    at_ota_proc->update_type = update_conf->update_type;
    memcpy(at_ota_proc->url, update_conf->url, ATCMD_W_OTA_HTTP_URL_LEN);
    at_ota_proc->auto_renew           = update_conf->auto_renew;
    at_ota_proc->download_sflash_addr = update_conf->download_sflash_addr;
 #if defined(__SUPPORT_OTA_RESUME__)

    /* Consume the resume flag as a one-shot: only AT+NWOTADWRESUME sets it, and
     * a subsequent (legacy) full download must never inherit it from the shared
     * config. When the feature is disabled this block compiles out entirely, so
     * the legacy flow is unchanged. */
    at_ota_proc->is_resume = update_conf->is_resume;
    update_conf->is_resume = 0;
 #endif
    at_ota_proc->download_notify = update_conf->download_notify;
    at_ota_proc->renew_notify    = update_conf->renew_notify;
    g_p_at_ctrl = p_at_ctrl;

    if (at_ota_proc_xHandle)
    {
        task_handle         = at_ota_proc_xHandle;
        at_ota_proc_xHandle = NULL;
        vTaskDelete(task_handle);
    }

    xReturned = xTaskCreate(atcmd_w_ota_update_process,
                            ATCMD_W_OTA_TASK_NAME,
                            ATCMD_W_OTA_TASK_STACK_SZ,
                            NULL,
                            OS_TASK_PRIORITY_USER,
                            &at_ota_proc_xHandle);

    if (xReturned != pdPASS)
    {
        ATCMD_W_OTA_ERR(RED_COLOR " [%s] Failed task create %s \r\n" CLEAR_COLOR, __func__, ATCMD_W_OTA_TASK_NAME);

        return ATCMD_W_OTA_FAILED;
    }

    return ATCMD_W_OTA_SUCCESS;
}

static void test_cmd_download_notify (atcmd_w_ota_update_type update_type, UINT status, UINT progress)
{
 #if !defined(ENABLE_ATCMD_W_OTA_DBG)
    RA6W1_UNUSED_ARG(update_type);
    RA6W1_UNUSED_ARG(status);
    RA6W1_UNUSED_ARG(progress);
 #endif                                //! (ENABLE_ATCMD_W_OTA_DBG)

    ATCMD_W_OTA_DBG("[%s] update_type = %d, status = 0x%02x, progress = %d\n", __func__, update_type, status, progress);
}

static void test_cmd_renew_notify (UINT status)
{
 #if !defined(ENABLE_ATCMD_W_OTA_DBG)
    RA6W1_UNUSED_ARG(status);
 #endif                                //! (ENABLE_ATCMD_W_OTA_DBG)

    ATCMD_W_OTA_DBG("[%s] status = 0x%02x\n", __func__, status);
}

static UINT atcmd_w_ota_update_download_test_cmd (atcmd_w_ota_update_type update_type, char * url)
{
    UINT status = ATCMD_W_OTA_SUCCESS;
    ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf = NULL;

    if ((url == NULL) || (strlen((char *) url) <= 0))
    {
        return ATCMD_W_OTA_FAILED;
    }

    at_ota_update_conf = ATCMD_W_OTA_MALLOC(sizeof(ATCMD_W_OTA_UPDATE_CONFIG));

    if (at_ota_update_conf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Failed to alloc memory\n", __func__);

        return ATCMD_W_OTA_FAILED;
    }

    memset(at_ota_update_conf, 0x00, sizeof(ATCMD_W_OTA_UPDATE_CONFIG));
    at_ota_update_conf->update_type = update_type;
    memcpy(at_ota_update_conf->url, url, ATCMD_W_OTA_HTTP_URL_LEN);
    at_ota_update_conf->download_notify = test_cmd_download_notify;

    /* OTA FW download API */
    status = atcmd_w_ota_update_start_download(g_p_at_ctrl, at_ota_update_conf);

    if (at_ota_update_conf != NULL)
    {
        ATCMD_W_OTA_FREE(at_ota_update_conf);
    }

    return status;
}

static UINT atcmd_w_ota_update_renew_test_cmd (void)
{
    UINT status = ATCMD_W_OTA_SUCCESS;
    ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf = NULL;

    at_ota_update_conf = ATCMD_W_OTA_MALLOC(sizeof(ATCMD_W_OTA_UPDATE_CONFIG));

    if (at_ota_update_conf == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Failed to alloc memory\n", __func__);

        return ATCMD_W_OTA_FAILED;
    }

    memset(at_ota_update_conf, 0x00, sizeof(ATCMD_W_OTA_UPDATE_CONFIG));

    at_ota_update_conf->renew_notify = test_cmd_renew_notify;

    status = atcmd_w_ota_update_start_renew(g_p_at_ctrl, at_ota_update_conf);

    if (at_ota_update_conf != NULL)
    {
        ATCMD_W_OTA_FREE(at_ota_update_conf);
    }

    return status;
}

UINT atcmd_w_ota_update_cli_cmd_parse (int argc, char * argv[])
{
    if ((argc == 1) || (strcasecmp("help", argv[1]) == 0))
    {
        goto __at_ota_cmd_help;
    }

    if ((strcasecmp("rtos", argv[1]) == 0) && (argc == 3))
    {
        return atcmd_w_ota_update_download_test_cmd(ATCMD_W_OTA_TYPE_RTOS, argv[2]);
    }
    else if (strcasecmp("stop", argv[1]) == 0)
    {
        return atcmd_w_ota_update_process_stop();
    }
    else if (strcasecmp("mcu_fw", argv[1]) == 0)
    {
        if ((argv[3] != NULL) && (argc == 4))
        {
            atcmd_w_ota_update_set_mcu_fw_name(argv[3]);
        }

        return atcmd_w_ota_update_download_test_cmd(ATCMD_W_OTA_TYPE_MCU_FW, argv[2]);
    }
    else if ((strcasecmp("cert_key", argv[1]) == 0) && (argc == 3))
    {
        return atcmd_w_ota_update_download_test_cmd(ATCMD_W_OTA_TYPE_CERT_KEY, argv[2]);
    }
    else if (strcasecmp("renew", argv[1]) == 0)
    {
        return atcmd_w_ota_update_renew_test_cmd();
    }
    else if ((strcasecmp("addr", argv[1]) == 0) && (argc == 3))
    {
        UINT sflash_addr, fw_type;

        if (strcasecmp("rtos", argv[2]) == 0)
        {
            fw_type = ATCMD_W_OTA_TYPE_RTOS;
        }
        else if (strcasecmp("mcu_fw", argv[2]) == 0)
        {
            fw_type = ATCMD_W_OTA_TYPE_MCU_FW;
        }
        else if (strcasecmp("cert_key", argv[2]) == 0)
        {
            fw_type = ATCMD_W_OTA_TYPE_CERT_KEY;
        }
        else
        {
            return ATCMD_W_OTA_FAILED;
        }

        sflash_addr = atcmd_w_ota_update_get_new_sflash_addr(fw_type);
        printf("Download area of %s is 0x%x.\n", argv[2], sflash_addr);

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("sflash_addr", argv[1]) == 0)
    {
        UINT   addr;
        char * end = NULL;

        if ((argc == 3) && (argv[2] != NULL))
        {
            addr = strtol(argv[2], &end, 16);

            return atcmd_w_ota_update_set_user_sflash_addr(addr);
        }
    }
    else if (strcasecmp("info", argv[1]) == 0)
    {
        char                name[8];
        UINT                size;
        UINT                crc;
        UINT                addr = 0x00;
        image_header_data_t infoImage;

        printf(" * OS(%d %%)\n", atcmd_w_ota_update_get_progress(ATCMD_W_OTA_TYPE_RTOS));

        if (atcmd_w_ota_update_get_image_info(ATCMD_W_OTA_STOR_RTOS_0_ADDR, &infoImage))
        {
            printf("- [0] RTOS (addr = 0x%x)\n", ATCMD_W_OTA_STOR_RTOS_0_ADDR);
            atcmd_w_ota_update_print_fw_header(&infoImage);
        }

        if (atcmd_w_ota_update_get_image_info(ATCMD_W_OTA_STOR_RTOS_1_ADDR, &infoImage))
        {
            printf("- [1] RTOS (addr = 0x%x)\n", ATCMD_W_OTA_STOR_RTOS_1_ADDR);
            atcmd_w_ota_update_print_fw_header(&infoImage);
        }

        printf("\n");

        // MCU FW info
 #if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl,
                                       RM_OTA_W_CURRENT_ADDR,
                                       RM_OTA_W_TYPE_MCU_FW,
                                       (uint32_t *) &addr);
 #endif
        memset(name, 0, 8);
        atcmd_w_ota_update_get_mcu_fw_info(name, &size, &crc);
        printf("- MCU FW (addr = 0x%x)\n", addr);
        printf(" Name-------------%s \n", name);
        printf(" Size-------------%d \n", size);
        printf(" CRC--------------0x%x \n\n", crc);

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("crc", argv[1]) == 0)
    {
        UINT   addr;
        char * end = NULL;

        if (argv[2] != NULL)
        {
            addr = strtol(argv[2], &end, 16);

            if (atcmd_w_ota_update_sflash_rtos_crc(addr) == ATCMD_W_OTA_SUCCESS)
            {
                printf("CRC: SUCCESS\n");
            }
            else
            {
                printf("CRC: FAILED\n");
            }

            return ATCMD_W_OTA_SUCCESS;
        }
    }
    else if (strcasecmp("prod_crc", argv[1]) == 0)
    {
        if (atcmd_w_ota_update_sflash_product_header_crc() == ATCMD_W_OTA_SUCCESS)
        {
            printf("CRC: SUCCESS\n");
        }
        else
        {
            printf("CRC: FAILED\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("set_boot_index", argv[1]) == 0)
    {
        if (argv[2] != NULL)
        {
            atcmd_w_ota_update_set_boot_index(atoi(argv[2]));

            return ATCMD_W_OTA_SUCCESS;
        }
    }
    else if (strcasecmp("get_boot_index", argv[1]) == 0)
    {
        printf("Current boot index = %d \n", atcmd_w_ota_update_get_boot_index());

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("toggle_boot_index", argv[1]) == 0)
    {
        /* Toggle the boot index */
        if (atcmd_w_ota_update_toggle_boot_index() == ATCMD_W_OTA_SUCCESS)
        {
            return ATCMD_W_OTA_SUCCESS;
        }
    }
    else if ((strcasecmp("write_sflash", argv[1]) == 0) && (argc == 5))
    {
        UCHAR * wt_buf = NULL;
        UINT    addr, len, ret_len;
        UINT    set_data;
        char  * end = NULL;

        addr     = strtol(argv[2], &end, 16);
        len      = atoi(argv[3]);
        set_data = strtol(argv[4], &end, 16);

        wt_buf = ATCMD_W_OTA_MALLOC(len + 1);

        if (wt_buf != NULL)
        {
            memset(wt_buf, 0x00, len + 1);
            memset(wt_buf, set_data, len);
        }
        else
        {
            printf("Write failed(Memory allocation failed)\n");

            return ATCMD_W_OTA_SUCCESS;
        }

        ret_len = atcmd_w_ota_update_write_flash(addr, wt_buf, len);

        if (ret_len != len)
        {
            printf("Write failed\n");
        }

        if (wt_buf != NULL)
        {
            ATCMD_W_OTA_FREE(wt_buf);
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("read_sflash", argv[1]) == 0) && (argc == 4))
    {
        UCHAR * rd_buf = NULL;
        UINT    addr, len;
        char  * end = NULL;

        addr   = strtol(argv[2], &end, 16);
        len    = atoi(argv[3]);
        rd_buf = ATCMD_W_OTA_MALLOC(len);

        if (rd_buf != NULL)
        {
            memset(rd_buf, 0x00, len);

            if (atcmd_w_ota_update_read_flash(addr, rd_buf, len) != 0)
            {
                hex_dump_rm_atcmd_w((unsigned char *) "", rd_buf, len, OUTPUT_HEXA_ASCII);
            }
            else
            {
                printf("Read failed\n");
            }

            ATCMD_W_OTA_FREE(rd_buf);
        }
        else
        {
            printf("Read failed(Memory allocation failed)\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("copy_sflash", argv[1]) == 0) && (argc == 5))
    {
        UINT   dst_addr, src_addr, len;
        char * end = NULL;

        dst_addr = strtol(argv[2], &end, 16);
        src_addr = strtol(argv[3], &end, 16);
        len      = atoi(argv[4]);

        if (atcmd_w_ota_update_copy_flash(dst_addr, src_addr, len) == 0)
        {
            printf("Copy failed\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("erase_sflash", argv[1]) == 0) && (argc == 4))
    {
        UINT   addr, len;
        char * end = NULL;

        addr = strtol(argv[2], &end, 16);
        len  = atoi(argv[3]);

        if (atcmd_w_ota_update_erase_flash(addr, len) == 0)
        {
            printf("Erase failed\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("set_auth_mode", argv[1]) == 0) && (argc == 3))
    {
        if (atcmd_w_ota_update_set_tls_auth_mode(atoi(argv[2])) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to set the TLS auth mode.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_auth_mode", argv[1]) == 0) && (argc == 2))
    {
        printf("%s.....%d\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH, atcmd_w_ota_update_get_tls_auth_mode());

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("set_tls_ver", argv[1]) == 0) && (argc == 3))
    {
        if (atcmd_w_ota_update_set_tls_version(atoi(argv[2])) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to set the TLS version.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_tls_ver", argv[1]) == 0) && (argc == 2))
    {
        atcmd_w_ota_update_get_tls_version();
        printf("%s.....%d\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_VER, atcmd_w_ota_update_get_tls_version());

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("set_sni", argv[1]) == 0) && (argc == 3))
    {
        if (atcmd_w_ota_update_set_sni(argv[2]) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to set the SNI.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_sni", argv[1]) == 0) && (argc == 2))
    {
        char * sni     = NULL;
        int    sni_len = 0;

        sni_len = atcmd_w_ota_update_get_sni(NULL, 0);
        sni     = ATCMD_W_OTA_MALLOC(sni_len + 1);

        if (sni != NULL)
        {
            atcmd_w_ota_update_get_sni(sni, sni_len + 1);
            printf("%s.....%s\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI, sni);
            ATCMD_W_OTA_FREE(sni);
            sni = NULL;
        }
        else
        {
            printf("Failed to get SNI.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("set_alpn", argv[1]) == 0) && (argc <= 5))
    {
        if (atcmd_w_ota_update_set_alpn(argv[2], argv[3], argv[4]) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to set the ALPN.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn0", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = atcmd_w_ota_update_get_alpn0(NULL, 0);
        alpn     = ATCMD_W_OTA_MALLOC(alpn_len + 1);

        if (alpn != NULL)
        {
            atcmd_w_ota_update_get_alpn0(alpn, alpn_len + 1);
            printf("%s0.....%s\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            ATCMD_W_OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN0.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn1", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = atcmd_w_ota_update_get_alpn1(NULL, 0);
        alpn     = ATCMD_W_OTA_MALLOC(alpn_len + 1);

        if (alpn != NULL)
        {
            atcmd_w_ota_update_get_alpn1(alpn, alpn_len + 1);
            printf("%s1.....%s\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            ATCMD_W_OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN1.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn2", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = atcmd_w_ota_update_get_alpn2(NULL, 0);
        alpn     = ATCMD_W_OTA_MALLOC(alpn_len + 1);

        if (alpn != NULL)
        {
            atcmd_w_ota_update_get_alpn2(alpn, alpn_len + 1);
            printf("%s2.....%s\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            ATCMD_W_OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN2.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("del_alpn", argv[1]) == 0) && (argc == 2))
    {
        atcmd_w_ota_update_del_all_alpn();
        printf("Deleted all ALPNs.\n");

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("set_name_mcu", argv[1]) == 0) && (argc == 3))
    {
        if (atcmd_w_ota_update_set_mcu_fw_name(argv[2]) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to set the name of MCU FW.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("get_name_mcu", argv[1]) == 0)
    {
        char name[8];

        if (atcmd_w_ota_update_get_mcu_fw_name(name) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to get the name of MCU FW\n");
        }
        else
        {
            printf("Name = %s(len=%d)\n", name, strlen(name));
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if ((strcasecmp("read_mcu", argv[1]) == 0) && (argc == 4))
    {
        UINT   addr, len;
        char * end = NULL;
        addr = strtol(argv[2], &end, 16);
        len  = atoi(argv[3]);

        if (atcmd_w_ota_update_read_mcu_fw(g_p_at_ctrl, addr, len) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to read.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("trans_mcu", argv[1]) == 0)
    {
        if (atcmd_w_ota_update_trans_mcu_fw(g_p_at_ctrl) != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to trans.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (strcasecmp("erase_mcu", argv[1]) == 0)
    {
        if (atcmd_w_ota_update_erase_mcu_fw() != ATCMD_W_OTA_SUCCESS)
        {
            printf("Failed to erase.\n");
        }

        return ATCMD_W_OTA_SUCCESS;
    }

__at_ota_cmd_help:

    printf("\n");
    printf("ota_update [fw_type] [url] \t: Start to FW download. \n");
    printf("\t\t\t\t  * fw_type \n");
    printf("\t\t\t\t    rtos : Fw_type of RTOS \n");
    printf("\t\t\t\t    cert_key : update_type of cert or key.\n");
    printf("\t\t\t\t    mcu_fw : Fw_type of MCU FW. \n");
    printf("\t\t\t\t  * url  : Server URL where FW exists \n");
    printf("\t\t\t\t  ex) ota_update rtos http://192.168.0.1/rtos.img \n");
    printf("\n");

    printf("ota_update stop \t\t: Stop to FW download. \n");
    printf("\t\t\t\t  ex) ota_update stop \n");
    printf("\n");

    printf("ota_update renew \t\t: Change current FW to new FW. \n");
    printf("\t\t\t\t  ex) ota_update renew \n");
    printf("\n");

    printf("ota_update info \t\t: Show FW information. \n");
    printf("\t\t\t\t  ex) ota_update info \n");
    printf("\n");

    printf("ota_update crc [addr] \t\t: Check CRC of FW. \n");
    printf("\t\t\t\t  ex) ota_update crc 0x2000 \n");
    printf("\n");

    printf("ota_update prod_crc [addr] \t\t: Check CRC of product header. \n");
    printf("\t\t\t\t  ex) ota_update prod_crc \n");
    printf("\n");

    printf("ota_update write_sflash [addr] [size] [set_data]\n");
    printf("\t\t\t\t  : Write sflash data.\n");
    printf("\t\t\t\t  ex) ota_update write_sflash 0x500000 64 0x0A\n");
    printf("\n");

    printf("ota_update read_sflash [addr] [size]\n");
    printf("\t\t\t\t  : Read sflash data.\n");
    printf("\t\t\t\t  ex) ota_update read_sflash 0x500000 128\n");
    printf("\n");

    printf("ota_update copy_sflash [dst_addr] [src_addr] [size]\n");
    printf("\t\t\t\t  : Copy from sflash data src_add to dst_add.\n");
    printf("\t\t\t\t  ex) ota_update copy_sflash 0x500000 0x501000 4096\n");
    printf("\n");

    printf("ota_update erase_sflash [addr] [size]\n");
    printf("\t\t\t\t  : Erase sflash data.\n");
    printf("\t\t\t\t  ex) ota_update erase_sflash 0x500000 4096\n");
    printf("\n");

    printf("ota_update set_auth_mode [auth_mode]\n");
    printf("\t\t\t\t  : Set the certificate verification mode\n");
    printf("\t\t\t\t    0(NONE), 1(OPTIONAL), 2(REQUIRED)\n");
    printf("\t\t\t\t  ex) ota_update set_auth_mode 1\n");
    printf("\n");

    printf("ota_update get_auth_mode\n");
    printf("\t\t\t\t  : Get the certificate verification mode\n");
    printf("\t\t\t\t  ex) ota_update get_auth_mode\n");
    printf("\n");

    printf("ota_update set_tls_ver [tls_ver]\n");
    printf("\t\t\t\t  : Set the supported version sent from the client side and/or accepted at the server side.\n");
    printf("\t\t\t\t    0(1.2 only), 1(1.3 only), 2(1.2 and 1.3)\n");
    printf("\t\t\t\t  ex) ota_update set_tls_ver 1\n");
    printf("\n");

    printf("ota_update get_tls_ver\n");
    printf("\t\t\t\t  : Get the supported version of tls\n");
    printf("\t\t\t\t  ex) ota_update get_tls_ver\n");
    printf("\n");

    printf("ota_update set_sni [sni]\n");
    printf("\t\t\t\t  : Set the SNI(Server Name Indication)\n");
    printf("\t\t\t\t  ex) ota_update set_sni example.iot.server\n");
    printf("\n");

    printf("ota_update get_sni\n");
    printf("\t\t\t\t  : Get the SNI(Server Name Indication)\n");
    printf("\t\t\t\t  ex) ota_update get_sni\n");
    printf("\n");

    printf("ota_update set_alpn [alpn0] [alpn1] [alpn2]\n");
    printf("\t\t\t\t  : Set the ALPN(Application Layer Protocol Negotiation). Up to 3 ALPNs can be saved\n");
    printf("\t\t\t\t  ex) ota_update set_alpn alpn0 alpn1 alpn2\n");
    printf("\n");

    printf("ota_update get_alpn0\n");
    printf("\t\t\t\t  : Get the ALPN(Application Layer Protocol Negotiation)\n");
    printf("\t\t\t\t  ex) ota_update set_alpn1\n");
    printf("\n");

    printf("ota_update get_alpn1\n");
    printf("\t\t\t\t  : Get the ALPN(Application Layer Protocol Negotiation)\n");
    printf("\t\t\t\t  ex) ota_update set_alpn2\n");
    printf("\n");

    printf("ota_update get_alpn2\n");
    printf("\t\t\t\t  : Get the ALPN(Application Layer Protocol Negotiation)\n");
    printf("\t\t\t\t  ex) ota_update set_alpn3\n");
    printf("\n");

    printf("ota_update del_alpn\n");
    printf("\t\t\t\t  : Deleted all ALPNs\n");
    printf("\t\t\t\t  ex) ota_update del_alpn\n");
    printf("\n");

    printf("ota_update set_name_mcu \t\t: Set the name(version) of MCU FW to be downloaded to sflash. \n");
    printf("\t\t\t\t  ex) ota_update set_name_mcu MCU_FW \n");
    printf("\n");

    printf("ota_update get_name_mcu \t\t: Get name(version) of MCU FW downloaded to sflash. \n");
    printf("\t\t\t\t  ex) ota_update get_name_mcu \n");
    printf("\n");

    printf("ota_update read_mcu [addr] [size]\n");
    printf("\t\t\t\t  : Read the firmware as much as the size from the read_addr and transmit it.\n");
    printf("\t\t\t\t  ex) ota_update read_mcu 0x3ad000 128\n");
    printf("\n");

    printf("ota_update trans_mcu \t\t: Transmit a firmware to MCU through UART. \n");
    printf("\t\t\t\t  ex) ota_update trans_mcu \n");
    printf("\n");

    printf("ota_update erase_mcu \t\t: Delete MCU firmware saved in Flash. \n");
    printf("\t\t\t\t  ex) ota_update erase_mcu \n");
    printf("\n");

    printf("ota_update set_boot_index \t: Set current boot index info.\n");
    printf("\t\t\t\t  ex) ota_update Set_boot_index 1\n");
    printf("ota_update get_boot_index \t: Get current boot index info.\n");
    printf("\t\t\t\t  ex) ota_update get_boot_index \n");
    printf("ota_update toggle_boot_index \t: Toggle boot index.\n");
    printf("\t\t\t\t  ex) ota_update toggle_boot_index \n");
    printf("\n");

    return ATCMD_W_OTA_FAILED;
}

/* Write the downloaded data directly to sflash. */
UINT atcmd_w_ota_update_buffer_write_flash (atcmd_w_ota_update_sflash_t * sflash_ctx, UCHAR * data, UINT length)
{
    UINT    status          = ATCMD_W_OTA_SUCCESS;
    UINT    buff_offset     = 0;
    UINT    copyToBufLen    = 0;
    UINT    writeToFlashLen = 0;
    UINT    input_len       = 0;
    UCHAR * input_data      = NULL;

    if (sflash_ctx == NULL)
    {
        ATCMD_W_OTA_ERR("[%s:%d] sflash_ctx error\n", __func__, __LINE__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

    if (length == 0)
    {
        ATCMD_W_OTA_ERR("[%s:%d] Data length error\n", __func__, __LINE__);
        status = ATCMD_W_OTA_FAILED;
        goto finish;
    }

    if (sflash_ctx->buffer == NULL)
    {
        sflash_ctx->buffer = (UCHAR *) ATCMD_W_OTA_MALLOC(ATCMD_W_OTA_SFLASH_BUF_SZ);

        if (sflash_ctx->buffer == NULL)
        {
            ATCMD_W_OTA_ERR("[%s:%d] Failed to allocate receive buffer(%d bytes)\n",
                            __func__,
                            __LINE__,
                            ATCMD_W_OTA_SFLASH_BUF_SZ);
            status = ATCMD_W_OTA_MEM_ALLOC_FAILED;
            goto finish;
        }

        memset(sflash_ctx->buffer, 0x00, ATCMD_W_OTA_SFLASH_BUF_SZ);
    }

    input_data = data;
    input_len  = length;

    sflash_ctx->length += input_len;

    if (sflash_ctx->offset > 0)
    {
        writeToFlashLen = sflash_ctx->offset;
    }

    writeToFlashLen += input_len;

    while (writeToFlashLen > 0)
    {
        copyToBufLen = ATCMD_W_OTA_SFLASH_BUF_SZ - sflash_ctx->offset;

        if (copyToBufLen > (input_len - buff_offset))
        {
            copyToBufLen = (input_len - buff_offset);
        }

        if (copyToBufLen > 0)
        {
            memcpy(&sflash_ctx->buffer[sflash_ctx->offset], (input_data + buff_offset), copyToBufLen);
            buff_offset        += copyToBufLen;
            sflash_ctx->offset += copyToBufLen;
        }

        if ((sflash_ctx->offset == ATCMD_W_OTA_SFLASH_BUF_SZ) ||
            (sflash_ctx->offset == sflash_ctx->total_length) ||
            ((sflash_ctx->length == sflash_ctx->total_length) && (sflash_ctx->offset > 0)))
        {
            /* SFLASH write */
            if (atcmd_w_ota_update_write_flash(sflash_ctx->sflash_addr, &sflash_ctx->buffer[0],
                                               sflash_ctx->offset) != sflash_ctx->offset)
            {
                ATCMD_W_OTA_ERR("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n",
                                __func__,
                                __LINE__,
                                sflash_ctx->sflash_addr,
                                sflash_ctx->offset);
                status = ATCMD_W_OTA_FAILED;
                goto finish;
            }

            sflash_ctx->sflash_addr += sflash_ctx->offset;

            memset(sflash_ctx->buffer, 0x00, ATCMD_W_OTA_SFLASH_BUF_SZ);
            sflash_ctx->offset = 0;

            if (writeToFlashLen >= ATCMD_W_OTA_SFLASH_BUF_SZ)
            {
                writeToFlashLen = writeToFlashLen - ATCMD_W_OTA_SFLASH_BUF_SZ;
            }
        }
        else
        {
            break;
        }
    }

finish:

    if ((status != ATCMD_W_OTA_SUCCESS) ||
        (sflash_ctx->length == sflash_ctx->total_length))
    {
        sflash_ctx->sflash_addr  = 0x00;
        sflash_ctx->total_length = 0;
        sflash_ctx->length       = 0;
        sflash_ctx->offset       = 0;

        if (sflash_ctx->buffer != NULL)
        {
            ATCMD_W_OTA_FREE(sflash_ctx->buffer);
            sflash_ctx->buffer = NULL;
        }
    }

    return status;
}

#endif                                 /* CFG_WIFI */

/* EOF */
