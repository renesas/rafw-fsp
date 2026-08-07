/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI && defined __SUPPORT_OTA__
 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>

 #include "rm_atcmd_w_core_ota_update.h"
 #include "rm_atcmd_w_core_ota_common.h"
 #include "rm_atcmd_w_core_ota_http.h"
 #include "rm_atcmd_w_core.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "bsp_sflash_map_ra6w1.h"
 #include "rm_wifi.h"
 #include "rm_wifi_helper.h"
 #include "rm_ota_w_util_api.h"

 #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
 #pragma GCC diagnostic ignored "-Wsign-conversion"

UINT atcmd_w_ota_update_cmd_parse (int argc, char * argv[])
{
    return atcmd_w_ota_update_cli_cmd_parse(argc, argv);
}

size_t atcmd_w_ota_update_read_flash (UINT addr, VOID * buf, UINT len)
{
    if (rm_ota_w_util_api_sflash_read(addr, buf, len) != pdTRUE)
    {

        /* Error */
        return 0;
    }

    return len;
}

size_t atcmd_w_ota_update_erase_flash (UINT addr, UINT len)
{
    if (rm_ota_w_util_api_sflash_erase(addr, len) != pdTRUE)
    {

        /* Error */
        return 0;
    }

    return len;
}

size_t atcmd_w_ota_update_write_flash (UINT addr, VOID * buf, UINT len)
{
    if (rm_ota_w_util_api_sflash_write(addr, buf, len) != pdTRUE)
    {

        /* Error */
        return 0;
    }

    return len;
}

size_t atcmd_w_ota_update_copy_flash (UINT dest_addr, UINT src_addr, UINT len)
{
    if (rm_ota_w_util_api_sflash_copy(dest_addr, src_addr, len) != pdTRUE)
    {

        /* Error */
        return 0;
    }

    return len;
}

UINT atcmd_w_ota_update_start_download (atcmd_w_ctrl_t * const      p_at_ctrl,
                                        ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf)
{
    UINT status = ATCMD_W_OTA_SUCCESS;

    status = atcmd_w_ota_update_process_create(p_at_ctrl, at_ota_update_conf);

    return status;
}

UINT atcmd_w_ota_update_stop_download (void)
{
    UINT status = ATCMD_W_OTA_SUCCESS;

    status = atcmd_w_ota_update_process_stop();

    return status;
}

UINT atcmd_w_ota_update_start_renew (atcmd_w_ctrl_t * const p_at_ctrl, ATCMD_W_OTA_UPDATE_CONFIG * at_ota_update_conf)
{
    UINT status          = ATCMD_W_OTA_SUCCESS;
    UINT reboot_wait_cnt = 50;         /* 5 secs */
    char atc_buf[32]     = {0, };

    status = atcmd_w_ota_update_check_state();
    if (status == ATCMD_W_OTA_SUCCESS)
    {
        status = atcmd_w_ota_update_current_fw_renew();
    }

    memset(atc_buf, 0x00, sizeof(atc_buf));
    sprintf(atc_buf, "+NWOTARENEW:0x%02x\r\n", status);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) atc_buf, strlen(atc_buf));

    if ((at_ota_update_conf != NULL) && (at_ota_update_conf->renew_notify != NULL))
    {
        at_ota_update_conf->renew_notify(status);
    }

    if (status == ATCMD_W_OTA_SUCCESS)
    {
        while (reboot_wait_cnt-- > 0)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));

            if ((reboot_wait_cnt % 10) == 0)
            {
                printf("\r- OTA: Reboot after %d secs ...", reboot_wait_cnt / 10);
            }
        }

        printf("\n");

        reset();
    }

    return status;
}

UINT atcmd_w_ota_update_get_progress (atcmd_w_ota_update_type update_type)
{
    return atcmd_w_ota_update_get_download_progress(update_type);
}

UINT atcmd_w_ota_update_set_tls_auth_mode (int tls_auth_mode)
{
    return atcmd_w_ota_http_client_set_tls_auth_mode(tls_auth_mode);
}

UINT atcmd_w_ota_update_set_tls_version (int tls_ver)
{
    return atcmd_w_ota_http_client_set_tls_version(tls_ver);
}

UINT atcmd_w_ota_update_set_sni (char * sni)
{
    return atcmd_w_ota_http_client_set_sni(sni);
}

UINT atcmd_w_ota_update_set_alpn (char * alpn0, char * alpn1, char * alpn2)
{
    return atcmd_w_ota_http_client_set_alpn(alpn0, alpn1, alpn2);
}

UINT atcmd_w_ota_update_get_tls_auth_mode (void)
{
    return atcmd_w_ota_http_client_get_tls_auth_mode();
}

UINT atcmd_w_ota_update_get_tls_version (void)
{
    return atcmd_w_ota_http_client_get_tls_version();
}

size_t atcmd_w_ota_update_get_sni (char * sni, size_t buf_len)
{
    return atcmd_w_ota_http_client_get_sni(sni, buf_len);
}

size_t atcmd_w_ota_update_get_alpn0 (char * alpn0, size_t buf_len)
{
    return atcmd_w_ota_http_client_get_alpn0(alpn0, buf_len);
}

size_t atcmd_w_ota_update_get_alpn1 (char * alpn1, size_t buf_len)
{
    return atcmd_w_ota_http_client_get_alpn1(alpn1, buf_len);
}

size_t atcmd_w_ota_update_get_alpn2 (char * alpn2, size_t buf_len)
{
    return atcmd_w_ota_http_client_get_alpn2(alpn2, buf_len);
}

void atcmd_w_ota_update_del_all_alpn (void)
{
    return atcmd_w_ota_http_client_del_all_alpn();
}

#endif                                 /* CFG_WIFI */

/* EOF */
