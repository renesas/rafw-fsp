/**
 ****************************************************************************************
 *
 * @file rm_cli_w_ota.c
 *
 * @brief RTOS command functions
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include "bsp_api.h"
#if defined(__SUPPORT_OTA__)
 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"
 #include <stdlib.h>
 #include <stdbool.h>
 #include <string.h>
 #include <strings.h>
 #include <stdio.h>
 #if CFG_WIFI
  #include "net_network_main.h"
 #endif
 #include "rm_cli_w.h"

 #include "ota_update.h"
 #include "ota_update_common.h"
 #include "ota_update_http.h"

 #if (SUPPORT_FSP_RM_OTA_W == 1)
  #include "rm_ota_w.h"
  #include "rm_ota_w_util_api.h"
 #endif

 #define CRC_PRELOAD    0xFFFF
 #define CRC16_CCITT    0x1021

 #if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
 #endif
static uint32_t ota_update_crc16 (uint32_t crcValue, unsigned char newByte)
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

static uint16_t ota_update_calc_crc16 (uint8_t * data, uint32_t size)
{
    uint32_t crc_calc;

    crc_calc = CRC_PRELOAD;
    for (unsigned char i = 0; i < size; i++)
    {
        crc_calc = ota_update_crc16(crc_calc, data[i]);
    }

    return crc_calc & 0xFFFF;
}

UINT ota_update_sflash_product_header_crc (void)
{
    uint16_t        read_crc = 0;
    uint16_t        cal_crc;
    unsigned char   header_crc[2] = {0x00, };
    unsigned char * buf           = NULL;

    buf = OTA_MALLOC(SF_PRODUCT_HDR_SIZE);
    if (buf == NULL)
    {
        OTA_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, SF_PRODUCT_HDR_SIZE);

        return OTA_MEM_ALLOC_FAILED;
    }

    memset(buf, 0x00, SF_PRODUCT_HDR_SIZE);
    ota_update_read_flash(SF_PRODUCT_HDR, buf, SF_PRODUCT_HDR_SIZE);

    ota_update_read_flash(SF_PRODUCT_HDR + 29, header_crc, sizeof(header_crc));
    read_crc = ((int) header_crc[0] << 0) |
               ((int) header_crc[1] << 8);
    OTA_INFO("CRC: <0x%08x> Read PRODUCT_HDR CRC = 0x%08x\n", SF_PRODUCT_HDR + 29, read_crc);

    cal_crc = ota_update_calc_crc16(buf, 29);
    OTA_INFO("CRC: Calculated PRODUCT_HDR CRC = 0x%08x\n", cal_crc);

    if (buf != NULL)
    {
        OTA_FREE(buf);
        buf = NULL;
    }

    if (read_crc != cal_crc)
    {
        OTA_INFO("CRC: CRC mismatch!!\n");

        return OTA_ERROR_CRC;
    }

    return OTA_SUCCESS;
}

 #if (SUPPORT_FSP_RM_OTA_W == 1)
static void test_cmd_download_notify (ota_update_type update_type, UINT status, UINT progress)
{
  #if !defined(ENABLE_OTA_DBG)
    RA6W1_UNUSED_ARG(update_type);
    RA6W1_UNUSED_ARG(status);
    RA6W1_UNUSED_ARG(progress);
  #endif                               //! (ENABLE_OTA_DBG)

    OTA_DBG("[%s] update_type = %d, status = 0x%02x, progress = %d\n", __func__, update_type, status, progress);
}

static UINT ota_update_download_test_cmd (ota_update_type update_type, char * url)
{
    UINT                status          = OTA_SUCCESS;
    OTA_UPDATE_CONFIG * ota_update_conf = NULL;

    if ((url == NULL) || (strlen((char *) url) <= 0))
    {
        return OTA_FAILED;
    }

    ota_update_conf = OTA_MALLOC(sizeof(OTA_UPDATE_CONFIG));
    if (ota_update_conf == NULL)
    {
        OTA_ERR("[%s] Failed to alloc memory\n", __func__);

        return OTA_FAILED;
    }

    memset(ota_update_conf, 0x00, sizeof(OTA_UPDATE_CONFIG));
    ota_update_conf->update_type = update_type;
    memcpy(ota_update_conf->url, url, OTA_HTTP_URL_LEN);
    ota_update_conf->download_notify = test_cmd_download_notify;

    /* OTA FW download API */
    status = ota_update_start_download(ota_update_conf);

    if (ota_update_conf != NULL)
    {
        OTA_FREE(ota_update_conf);
    }

    return status;
}

static UINT ota_update_cli_cmd_parse (int argc, char * argv[])
{
    if ((argc == 1) || (strcasecmp("help", argv[1]) == 0))
    {
        goto __ota_cmd_help;
    }

    if ((strcasecmp("rtos", argv[1]) == 0) && (argc == 3))
    {
        return ota_update_download_test_cmd(OTA_TYPE_RTOS, argv[2]);
    }
    else if (strcasecmp("stop", argv[1]) == 0)
    {
        return ota_update_process_stop();
  #if defined(__OTA_UPDATE_MCU_FW__)
    }
    else if (strcasecmp("mcu_fw", argv[1]) == 0)
    {
        if ((argv[3] != NULL) && (argc == 4))
        {
            ota_update_set_mcu_fw_name(argv[3]);
        }
        return ota_update_download_test_cmd(OTA_TYPE_MCU_FW, argv[2]);
  #endif                               // (__OTA_UPDATE_MCU_FW__)
    }
    else if ((strcasecmp("cert_key", argv[1]) == 0) && (argc == 3))
    {
        return ota_update_download_test_cmd(OTA_TYPE_CERT_KEY, argv[2]);
    }
    else if (strcasecmp("renew", argv[1]) == 0)
    {
        return p_ota_instance->p_api->swap(p_ota_instance->p_ctrl);
    }
    else if ((strcasecmp("addr", argv[1]) == 0) && (argc == 3))
    {
        UINT sflash_addr, fw_type;
        if (strcasecmp("rtos", argv[2]) == 0)
        {
            fw_type = OTA_TYPE_RTOS;
        }
        else if (strcasecmp("mcu_fw", argv[2]) == 0)
        {
            fw_type = OTA_TYPE_MCU_FW;
        }
        else if (strcasecmp("cert_key", argv[2]) == 0)
        {
            fw_type = OTA_TYPE_CERT_KEY;
        }
        else
        {
            return OTA_FAILED;
        }

        p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_NEW_ADDR, fw_type, (uint32_t *) &sflash_addr);
        printf("Download area of %s is 0x%x.\n", argv[2], sflash_addr);

        return OTA_SUCCESS;
    }
    else if (strcasecmp("sflash_addr", argv[1]) == 0)
    {
        UINT   addr;
        char * end = NULL;
        if ((argc == 3) && (argv[2] != NULL))
        {
            addr = strtol(argv[2], &end, 16);

            return p_ota_instance->p_api->setAddr(p_ota_instance->p_ctrl, RM_OTA_W_USER_ADDR, addr);
        }
    }
    else if (strcasecmp("info", argv[1]) == 0)
    {
  #if defined(__OTA_UPDATE_MCU_FW__)
        char name[8];
        UINT size, crc, addr;
  #endif                               // (__OTA_UPDATE_MCU_FW__)
        rm_ota_w_image_header_data_t infoImage;

        if (p_ota_instance->p_api->getImageInfo(p_ota_instance->p_ctrl, RM_OTA_W_TYPE_RTOS, RM_OTA_W_STOR_RTOS_0_ADDR,
                                       &infoImage) == FSP_SUCCESS)
        {
            printf("- [0] RTOS (addr = 0x%x)\n", RM_OTA_W_STOR_RTOS_0_ADDR);
            ota_update_print_fw_header((image_header_data_t *) &infoImage);
        }

        if (p_ota_instance->p_api->getImageInfo(p_ota_instance->p_ctrl, RM_OTA_W_TYPE_RTOS, RM_OTA_W_STOR_RTOS_1_ADDR,
                                       &infoImage) == FSP_SUCCESS)
        {
            printf("- [1] RTOS (addr = 0x%x)\n", RM_OTA_W_STOR_RTOS_1_ADDR);
            ota_update_print_fw_header((image_header_data_t *) &infoImage);
        }

        printf("\n");
  #if defined(__OTA_UPDATE_MCU_FW__)

        // MCU FW info
        p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_CURRENT_ADDR, RM_OTA_W_TYPE_MCU_FW, (uint32_t *) &addr);
        memset(name, 0, 8);
        ota_update_get_mcu_fw_info(name, &size, &crc);
        printf("- MCU FW (addr = 0x%x)\n", addr);
        printf(" Name-------------%s \n", name);
        printf(" Size-------------%d \n", size);
        printf(" CRC--------------0x%x \n\n", crc);
  #endif                               // (__OTA_UPDATE_MCU_FW__)
        return OTA_SUCCESS;
    }
    else if (strcasecmp("crc", argv[1]) == 0)
    {
        UINT   addr;
        char * end = NULL;
        if (argv[2] != NULL)
        {
            addr = strtol(argv[2], &end, 16);
            if (p_ota_instance->p_api->cert(p_ota_instance->p_ctrl, RM_OTA_W_VALIDATE_TYPE_IMG_CRC, addr) == RM_OTA_W_SUCCESS)
            {
                printf("CRC: SUCCESS\n");
            }
            else
            {
                printf("CRC: FAILED\n");
            }

            return OTA_SUCCESS;
        }
    }
    else if (strcasecmp("prod_crc", argv[1]) == 0)
    {
        if (p_ota_instance->p_api->cert(p_ota_instance->p_ctrl, RM_OTA_W_VALIDATE_TYPE_PROD_CRC, 0) == RM_OTA_W_SUCCESS)
        {
            printf("CRC: SUCCESS\n");
        }
        else
        {
            printf("CRC: FAILED\n");
        }

        return OTA_SUCCESS;
    }
    else if (strcasecmp("set_boot_index", argv[1]) == 0)
    {
        if (argv[2] != NULL)
        {
            p_ota_instance->p_api->bootIdxSet(p_ota_instance->p_ctrl, atoi(argv[2]));

            return OTA_SUCCESS;
        }
    }
    else if (strcasecmp("get_boot_index", argv[1]) == 0)
    {
        uint8_t current_boot_idx = 0;
        p_ota_instance->p_api->bootIdxGet(p_ota_instance->p_ctrl, &current_boot_idx);
        printf("Current boot index = %d \n", current_boot_idx);

        return OTA_SUCCESS;
    }
    else if (strcasecmp("toggle_boot_index", argv[1]) == 0)
    {
        /* Toggle the boot index */
        if (p_ota_instance->p_api->bootIdxSet(p_ota_instance->p_ctrl, RM_OTA_W_BOOT_IDX_TOGGLE) == RM_OTA_W_SUCCESS)
        {
            return OTA_SUCCESS;
        }
    }
    else if ((strcasecmp("write_sflash", argv[1]) == 0) && (argc == 5))
    {
        UCHAR * wt_buf = NULL;
        UINT    addr, len;
        UINT    set_data;
        char  * end = NULL;

        addr     = strtol(argv[2], &end, 16);
        len      = atoi(argv[3]);
        set_data = strtol(argv[4], &end, 16);

        wt_buf = OTA_MALLOC(len + 1);
        if (wt_buf != NULL)
        {
            memset(wt_buf, 0x00, len + 1);
            memset(wt_buf, set_data, len);
        }
        else
        {
            printf("Write failed(Memory allocation failed)\n");

            return OTA_SUCCESS;
        }

        if (rm_ota_w_util_api_sflash_write(addr, (char *) wt_buf, len) != pdTRUE)
        {
            printf("Write failed\n");
        }

        if (wt_buf != NULL)
        {
            OTA_FREE(wt_buf);
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("read_sflash", argv[1]) == 0) && (argc == 4))
    {
        UCHAR * rd_buf = NULL;
        UINT    addr, len;
        char  * end = NULL;

        addr   = strtol(argv[2], &end, 16);
        len    = atoi(argv[3]);
        rd_buf = OTA_MALLOC(len);

        if (rd_buf != NULL)
        {
            memset(rd_buf, 0x00, len);
            if (rm_ota_w_util_api_sflash_read(addr, rd_buf, len) == pdTRUE)
            {
                hex_dump_cli((unsigned char *) "", rd_buf, len, OUTPUT_HEXA_ASCII);
            }
            else
            {
                printf("Read failed\n");
            }

            OTA_FREE(rd_buf);
        }
        else
        {
            printf("Read failed(Memory allocation failed)\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("copy_sflash", argv[1]) == 0) && (argc == 5))
    {
        UINT   dst_addr, src_addr, len;
        char * end = NULL;

        dst_addr = strtol(argv[2], &end, 16);
        src_addr = strtol(argv[3], &end, 16);
        len      = atoi(argv[4]);

        if (rm_ota_w_util_api_sflash_copy(dst_addr, src_addr, len) != pdTRUE)
        {
            printf("Copy failed\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("erase_sflash", argv[1]) == 0) && (argc == 4))
    {
        UINT   addr, len;
        char * end = NULL;

        addr = strtol(argv[2], &end, 16);
        len  = atoi(argv[3]);

        if (rm_ota_w_util_api_sflash_erase(addr, len) != pdTRUE)
        {
            printf("Erase failed\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("set_auth_mode", argv[1]) == 0) && (argc == 3))
    {
        if (ota_update_set_tls_auth_mode(atoi(argv[2])) != OTA_SUCCESS)
        {
            printf("Failed to set the TLS auth mode.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_auth_mode", argv[1]) == 0) && (argc == 2))
    {
        printf("%s.....%d\n", OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH, ota_update_get_tls_auth_mode());

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("set_tls_ver", argv[1]) == 0) && (argc == 3))
    {
        if (ota_update_set_tls_version(atoi(argv[2])) != OTA_SUCCESS)
        {
            printf("Failed to set the TLS version.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_tls_ver", argv[1]) == 0) && (argc == 2))
    {
        ota_update_get_tls_version();
        printf("%s.....%d\n", OTA_HTTPC_NVRAM_CONFIG_TLS_VER, ota_update_get_tls_version());

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("set_sni", argv[1]) == 0) && (argc == 3))
    {
        if (ota_update_set_sni(argv[2]) != OTA_SUCCESS)
        {
            printf("Failed to set the SNI.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_sni", argv[1]) == 0) && (argc == 2))
    {
        char * sni     = NULL;
        int    sni_len = 0;

        sni_len = ota_update_get_sni(NULL);
        sni     = OTA_MALLOC(sni_len + 1);

        if (sni != NULL)
        {
            ota_update_get_sni(sni);
            printf("%s.....%s\n", OTA_HTTPC_NVRAM_CONFIG_TLS_SNI, sni);
            OTA_FREE(sni);
            sni = NULL;
        }
        else
        {
            printf("Failed to get SNI.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("set_alpn", argv[1]) == 0) && (argc <= 5))
    {
        if (ota_update_set_alpn(argv[2], argv[3], argv[4]) != OTA_SUCCESS)
        {
            printf("Failed to set the ALPN.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn0", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = ota_update_get_alpn0(NULL);
        alpn     = OTA_MALLOC(alpn_len + 1);
        if (alpn != NULL)
        {
            ota_update_get_alpn0(alpn);
            printf("%s0.....%s\n", OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN0.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn1", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = ota_update_get_alpn1(NULL);
        alpn     = OTA_MALLOC(alpn_len + 1);

        if (alpn != NULL)
        {
            ota_update_get_alpn1(alpn);
            printf("%s1.....%s\n", OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN1.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("get_alpn2", argv[1]) == 0) && (argc == 2))
    {
        char * alpn     = NULL;
        int    alpn_len = 0;

        alpn_len = ota_update_get_alpn2(NULL);
        alpn     = OTA_MALLOC(alpn_len + 1);
        if (alpn != NULL)
        {
            ota_update_get_alpn2(alpn);
            printf("%s2.....%s\n", OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, alpn);
            OTA_FREE(alpn);
            alpn = NULL;
        }
        else
        {
            printf("Failed to get ALPN2.\n");
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("del_alpn", argv[1]) == 0) && (argc == 2))
    {
        ota_update_del_all_alpn();
        printf("Deleted all ALPNs.\n");

        return OTA_SUCCESS;
  #if defined(__OTA_UPDATE_MCU_FW__)
    }
    else if ((strcasecmp("set_name_mcu", argv[1]) == 0) && (argc == 3))
    {
        if (ota_update_set_mcu_fw_name(argv[2]) != OTA_SUCCESS)
        {
            printf("Failed to set the name of MCU FW.\n");
        }

        return OTA_SUCCESS;
    }
    else if (strcasecmp("get_name_mcu", argv[1]) == 0)
    {
        char name[8];
        if (ota_update_get_mcu_fw_name(name) != OTA_SUCCESS)
        {
            printf("Failed to get the name of MCU FW\n");
        }
        else
        {
            printf("Name = %s(len=%d)\n", name, strlen(name));
        }

        return OTA_SUCCESS;
    }
    else if ((strcasecmp("read_mcu", argv[1]) == 0) && (argc == 4))
    {
        UINT   addr, len;
        char * end = NULL;
        addr = strtol(argv[2], &end, 16);
        len  = atoi(argv[3]);
        if (ota_update_read_mcu_fw(addr, len) != OTA_SUCCESS)
        {
            printf("Failed to read.\n");
        }

        return OTA_SUCCESS;
    }
    else if (strcasecmp("trans_mcu", argv[1]) == 0)
    {
        if (ota_update_trans_mcu_fw() != OTA_SUCCESS)
        {
            printf("Failed to trans.\n");
        }

        return OTA_SUCCESS;
    }
    else if (strcasecmp("erase_mcu", argv[1]) == 0)
    {
        if (ota_update_erase_mcu_fw() != OTA_SUCCESS)
        {
            printf("Failed to erase.\n");
        }
        return OTA_SUCCESS;
  #endif                               // (__OTA_UPDATE_MCU_FW__)
    }

__ota_cmd_help:

    printf("\n");
    printf("ota_update [fw_type] [url] \t: Start to FW download. \n");
    printf("\t\t\t\t  * fw_type \n");
    printf("\t\t\t\t    rtos : Fw_type of RTOS \n");
    printf("\t\t\t\t    cert_key : update_type of cert or key.\n");
  #if defined(__OTA_UPDATE_MCU_FW__)
    printf("\t\t\t\t    mcu_fw : Fw_type of MCU FW. \n");
  #endif                               // (__OTA_UPDATE_MCU_FW__)
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

  #if defined(__OTA_UPDATE_MCU_FW__)
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
  #endif                               // (__OTA_UPDATE_MCU_FW__)
    printf("ota_update set_boot_index \t: Set current boot index info.\n");
    printf("\t\t\t\t  ex) ota_update Set_boot_index 1\n");
    printf("ota_update get_boot_index \t: Get current boot index info.\n");
    printf("\t\t\t\t  ex) ota_update get_boot_index \n");
    printf("ota_update toggle_boot_index \t: Toggle boot index.\n");
    printf("\t\t\t\t  ex) ota_update toggle_boot_index \n");
    printf("\n");

    return OTA_FAILED;
}

static UINT ota_update_cmd_parse (int argc, char * argv[])
{
    return ota_update_cli_cmd_parse(argc, argv);
}

bool cmd_ota_update (int argc, char * argv[])
{
    if (!ra6w1_network_main_is_wlaninit())
    {
        printf("Wi-Fi is not initialized.\n");

        return pdTRUE;
    }

    if (ota_update_cmd_parse(argc, argv))
    {
        PRINTF("FAIL\n");
    }
    else
    {
        PRINTF("OK\n");
    }

    return pdTRUE;
}

 #endif                                // (SUPPORT_FSP_RM_OTA_W)
#endif                                 // (__SUPPORT_OTA__)

/* EOF */
