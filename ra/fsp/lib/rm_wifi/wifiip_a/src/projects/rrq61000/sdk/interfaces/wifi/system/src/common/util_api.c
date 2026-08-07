/**
 ****************************************************************************************
 *
 * @file util_api.c
 *
 * @brief Utility APIs for user function
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "common_def.h"
#include "util_api.h"
#include "supp_config.h"
#ifndef RRQ61X_OSPI_W_ENABLED
#include "ad_flash.h"
#endif //!RRQ61X_OSPI_W_ENABLED
#include "lwip/err.h"
#include "common_compile_opt.h"
#include "net_sntp_client.h"

#if defined (__SUPPORT_WIFI_CONN_CB__)
#include "lwip/priv/tcp_priv.h"
#include "net_dhcp_server.h"
#include "dhcpserver.h"
#endif // __SUPPORT_WIFI_CONN_CB__

#ifdef __SUPPORT_REMOVE_MAC_NAME__
#include "app_provision.h"
#endif // __SUPPORT_REMOVE_MAC_NAME__

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */
#include "bsp_sflash_map_ra6w1.h"

#ifdef RRQ61X_OSPI_W_ENABLED
#include <string.h>
#include "r_ospi_w.h"
#endif //RRQ61X_OSPI_W_ENABLED

#ifndef MQTT_MOCK
#include "rm_wifi.h"

#include "rm_wifi_helper.h"
#endif /* MQTT_MOCK */
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#undef  UTIL_DEBUG_LOG

#ifdef RRQ61X_OSPI_W_ENABLED
 #define R_WIFI_FLASH_CTRL (((const spi_flash_instance_t *) g_wifi_cfg.p_spi_flash)->p_ctrl)
 #define R_WIFI_FLASH_CFG  (((const spi_flash_instance_t *) g_wifi_cfg.p_spi_flash)->p_cfg)
 #define R_WIFI_FLASH_API  (((const spi_flash_instance_t *) g_wifi_cfg.p_spi_flash)->p_api)
#endif

//
//// SFLASH user area api /////////////////////////////////////////////////////
//
static void util_sflash_open(void)
{
#ifdef RRQ61X_OSPI_W_ENABLED
    if (((ospi_w_instance_ctrl_t *) R_WIFI_FLASH_CTRL)->open != 0x4F535049) // ASCII characters "OSPI", Refer to r_ospi_w.c
    {
        R_WIFI_FLASH_API->open(R_WIFI_FLASH_CTRL, R_WIFI_FLASH_CFG);
    }
#else
    ad_flash_init();
#endif //RRQ61X_OSPI_W_ENABLED
}

bool util_sflash_read(int sflash_addr, void *rd_buf, int len)
{
#ifdef RRQ61X_OSPI_W_ENABLED
    /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
    memcpy((void *) rd_buf, (void *) (sflash_addr | OSPI_W_DEVICE_START_ADDRESS_DATA), len);
    return pdTRUE;

#else ///////////////////////////////////////////////////////////////////

    size_t read_size;

    if (rd_buf == NULL) {
        printf("[%s] Read buffer is NULL\n", __func__);
        return 0;
    }

    ad_flash_init();

    read_size = ad_flash_read(sflash_addr, (uint8_t*)rd_buf, len);

    if (len != read_size) {
        printf("bytes read is wropng (%zu) \n", read_size);
        return pdFALSE;
    }

    return pdTRUE;
#endif //RRQ61X_OSPI_W_ENABLED
}

bool util_sflash_write(int sflash_addr, char *wr_buf, int len)
{
    int addr_offset = 0;
    int buff_offset = 0;
    int tot_len = 0;
    int write_len = 0;

    char *stash_buf = NULL;
    int stash_len = 0;

    addr_offset = sflash_addr;
    buff_offset = (int)wr_buf;
    tot_len = len;

    util_sflash_open();

    while (tot_len > 0) {

        if (tot_len > FLASH_SECTOR_SIZE) {
            write_len = FLASH_SECTOR_SIZE;
        } else {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE) {
            stash_len = FLASH_SECTOR_SIZE - write_len;

            stash_buf = (char *)pvPortMalloc(stash_len + 1);
            if (stash_buf == NULL) {
            printf("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__,
                    stash_len + 1);
            goto finish;
            }
            memset(stash_buf, 0x00, stash_len + 1);
#ifdef RRQ61X_OSPI_W_ENABLED
            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void *) stash_buf, (void *) ((addr_offset + write_len) | OSPI_W_DEVICE_START_ADDRESS_DATA), stash_len);
#else
            ad_flash_read((addr_offset + write_len), (uint8_t*)stash_buf, stash_len);
#endif //RRQ61X_OSPI_W_ENABLED
        }

        /* Erase flash before writing */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_WIFI_FLASH_API->erase(R_WIFI_FLASH_CTRL, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA), FLASH_SECTOR_SIZE) != FSP_SUCCESS) {
#else
        if (ad_flash_erase_region((uint32_t *)addr_offset, FLASH_SECTOR_SIZE) != pdTRUE) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash erase failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Flash write */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_WIFI_FLASH_API->write(R_WIFI_FLASH_CTRL, (uint8_t *) buff_offset, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA), (uint32_t) write_len) != FSP_SUCCESS) {
#else
        if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)buff_offset, (uint32_t)write_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                write_len);
            goto finish;
        }
        addr_offset += write_len;
        buff_offset += write_len;

        /* Stash pop */
        if (stash_len > 0) {
#ifdef RRQ61X_OSPI_W_ENABLED
            if (R_WIFI_FLASH_API->write(R_WIFI_FLASH_CTRL, (uint8_t *) stash_buf, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA), (uint32_t) stash_len) != FSP_SUCCESS) {
#else
            if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)stash_buf, (uint32_t)stash_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
                printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                    addr_offset,
                    stash_len);
                goto finish;
            }
            stash_len = 0;
        }
        tot_len -= write_len;
    }

finish:

    if (stash_buf) {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0) {
        printf("[%s:%d] Failed size = %d)\n", __func__, __LINE__, tot_len);
        return pdFALSE;
    }

    return pdTRUE;
}

bool util_sflash_erase(int sflash_addr, int len)
{
    UINT addr_offset = 0;
    UINT tot_len = 0;
    UINT write_len = 0;

    UCHAR *stash_buf = NULL;
    UINT stash_len = 0;

    addr_offset = sflash_addr;
    tot_len = (UINT)len;

    util_sflash_open();

    while (tot_len > 0) {

        if (tot_len > FLASH_SECTOR_SIZE) {
            write_len = FLASH_SECTOR_SIZE;
        } else {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE) {
            stash_len = FLASH_SECTOR_SIZE - write_len;

            stash_buf = (UCHAR *)pvPortMalloc(stash_len + 1);
            if (stash_buf == NULL) {
            printf("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__,
                    stash_len + 1);
            goto finish;
            }
            memset(stash_buf, 0x00, stash_len + 1);
#ifdef RRQ61X_OSPI_W_ENABLED
            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void *) stash_buf, (void *) ((addr_offset + write_len) | OSPI_W_DEVICE_START_ADDRESS_DATA), stash_len);
#else
            ad_flash_read((addr_offset + write_len), (uint8_t*)stash_buf, stash_len);
#endif //RRQ61X_OSPI_W_ENABLED

        }

        /* Erase flash before writing */
#ifdef RRQ61X_OSPI_W_ENABLED
        if (R_WIFI_FLASH_API->erase(R_WIFI_FLASH_CTRL, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA), FLASH_SECTOR_SIZE) != FSP_SUCCESS) {
#else
        if (ad_flash_erase_region((uint32_t *)addr_offset, FLASH_SECTOR_SIZE) != pdTRUE) {
#endif //RRQ61X_OSPI_W_ENABLED
            printf("[%s:%d] Flash erase failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                addr_offset,
                FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Stash pop */
        addr_offset += write_len;
        if (stash_len > 0) {
#ifdef RRQ61X_OSPI_W_ENABLED
            if (R_WIFI_FLASH_API->write(R_WIFI_FLASH_CTRL, (uint8_t *)stash_buf, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA), (uint32_t)stash_len) != FSP_SUCCESS) {
    #else
            if (ad_flash_write((uint32_t*)addr_offset, (uint8_t *)stash_buf, (uint32_t)stash_len) == 0) {
#endif //RRQ61X_OSPI_W_ENABLED
                printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__,
                    addr_offset,
                    stash_len);
                goto finish;
            }
            stash_len = 0;
        }
        tot_len -= write_len;
    }

finish:

    if (stash_buf) {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0) {
        printf("[%s:%d] Failed size = %d)\n", __func__, __LINE__, tot_len);
        return pdFALSE;
    }

    return pdTRUE;
}

bool util_sflash_copy(int dest_addr, int src_addr, int len)
{
    int  offset = 0, loop_cnt = 0, copy_len = 0, tmp_len = 0;
    char *buf = NULL;

    if ((dest_addr % FLASH_SECTOR_SIZE) || (src_addr % FLASH_SECTOR_SIZE)) {
        printf("[%s] Flash address offset must be 4Kbyte\n", __func__);
        return 0;
    }

    copy_len = len;
    loop_cnt = len / FLASH_SECTOR_SIZE;

    if (loop_cnt > 0) {
        tmp_len = FLASH_SECTOR_SIZE;
    } else {
        tmp_len = len;
    }

    if (len % FLASH_SECTOR_SIZE) {
        loop_cnt = loop_cnt + 1;
    }

    buf = (char *)pvPortMalloc(tmp_len + 1);
    if (buf == NULL) {
        printf("[%s] Fail to alloc memory(%dbytes)\n", __func__, tmp_len + 1);
        return 0;
    }

    while (loop_cnt--) {
        util_sflash_read(src_addr, buf, tmp_len);

        if (util_sflash_erase(dest_addr + offset, tmp_len) != pdTRUE) {
            printf("[%s] Erase failed\n", __func__);
            goto finish;
        }

        if (util_sflash_write((dest_addr + offset), buf, tmp_len) != pdTRUE) {
            printf("[%s] Write failed\n", __func__);
            goto finish;
        }
        offset += tmp_len;
        tmp_len = copy_len - tmp_len;
    }

finish:

    if (buf != NULL) {
        vPortFree(buf);
    }

    if (offset != len) {
        return pdFALSE;
    }

    return pdTRUE;
}

#ifndef MQTT_MOCK
#if defined (__DA16400_PORT__) // used by eembc, scan result sample
//// For get SCAN result API //////////////////////////////////////////////////

#define SCAN_BSSID_IDX       0
#define SCAN_FREQ_IDX        1
#define SCAN_SIGNAL_IDX      2
#define SCAN_FLGA_IDX        3
#define SCAN_SSID_IDX        4

#define HIDDEN_SSID_DETECTION_CHAR    '\t'

#endif /* __DA16400_PORT__  // used by eembc, scan result sample */

///////////////////////////////////////////////////////////////////////////////

//
//// Register Notify callback function for Wi-Fi connection /////////////////////
//

/*
 * Register Customer call-back functions
 */
void wifi_conn_fail_noti_to_atcmd_host(void)
{
    #if (ATCMD_IF_SUPPORT == 1)
    #if defined (__SUPPORT_MQTT__)
    extern void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(int state);
    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(FALSE);
    #endif // __SUPPORT_MQTT__
    #endif
}

///////////////////////////////////////////////////////////////////////////////
//
//// For Factory-Reset APIs //////////////////////////////////////////////////
//
//#include "nvedit.h"
#if CFG_WIFI
int is_in_softap_acs_mode(void)
{
    int tmp_res, tmp_freq;
    int res = pdFALSE;

    if (get_run_mode() != WIFI_DEVICE_MODE_EXT_AP) {
        return pdFALSE;
    }
#ifdef RM_MAP_PERSISTANT_W
    tmp_res = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
		                         (char *)NVR_KEY_CHANNEL, &tmp_freq);
#endif

    if (tmp_res != FSP_SUCCESS) {
        // the key not existing .. softap is not fully set up yet
        return pdFALSE;
    }

    if (tmp_freq == 0) {
        res = pdTRUE;
    }

    return res;
}
#endif
// #endif    /* __SUPPORT_WIFI_CONCURRENT__ */

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#endif /* MQTT_MOCK */

/* EOF */
