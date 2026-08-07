/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "rm_ota_w_api.h"
#include "rm_ota_w_util_api.h"
#include "r_ospi_w.h"
#include "FreeRTOS.h"
#include "task.h"

#ifndef FLASH_SECTOR_SIZE
 #define FLASH_SECTOR_SIZE    (OSPI_W_FLASH_SECTOR_SIZE)
#endif

extern const ota_instance_t * p_ota_instance;

#define R_OTA_FLASH_CTRL      (((const spi_flash_instance_t *) p_ota_instance->p_cfg->p_spi_flash)->p_ctrl)
#define R_OTA_FLASH_CFG       (((const spi_flash_instance_t *) p_ota_instance->p_cfg->p_spi_flash)->p_cfg)
#define R_OTA_FLASH_API       (((const spi_flash_instance_t *) p_ota_instance->p_cfg->p_spi_flash)->p_api)

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

bool rm_ota_w_util_api_sflash_read (uint32_t sflash_addr, void * rd_buf, uint32_t len)
{
    /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
    memcpy((void *) rd_buf, (void *) (sflash_addr | OSPI_W_DEVICE_START_ADDRESS_DATA), len);

    return pdTRUE;
}

bool rm_ota_w_util_api_sflash_write (uint32_t sflash_addr, char * wr_buf, uint32_t len)
{
    uint32_t addr_offset = 0;
    uint32_t buff_offset = 0;
    uint32_t tot_len     = 0;
    uint32_t write_len   = 0;

    char   * stash_buf = NULL;
    uint32_t stash_len = 0;

    addr_offset = sflash_addr;
    buff_offset = (uint32_t) wr_buf;
    tot_len     = len;

    if (((ospi_w_instance_ctrl_t *) R_OTA_FLASH_CTRL)->open != 0x4F535049) // ASCII characters "OSPI", Refer to r_ospi_w.c
    {
        R_OTA_FLASH_API->open(R_OTA_FLASH_CTRL, R_OTA_FLASH_CFG);
    }

    while (tot_len > 0)
    {
        if (tot_len > FLASH_SECTOR_SIZE)
        {
            write_len = FLASH_SECTOR_SIZE;
        }
        else
        {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE)
        {
            stash_len = FLASH_SECTOR_SIZE - write_len;
            stash_buf = (char *) pvPortMalloc(stash_len + 1);

            if (stash_buf == NULL)
            {
                printf("[%s:%d] Failed to allocate buffer(%ld bytes)\n", __func__, __LINE__, stash_len + 1);
                goto finish;
            }

            memset(stash_buf, 0x00, stash_len + 1);

            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void *) stash_buf,
                   (void *) ((addr_offset + write_len) | OSPI_W_DEVICE_START_ADDRESS_DATA),
                   stash_len);
        }

        /* Erase flash before writing */
        if (R_OTA_FLASH_API->erase(R_OTA_FLASH_CTRL, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA),
                                   FLASH_SECTOR_SIZE) != FSP_SUCCESS)
        {
            printf("[%s:%d] Flash erase failed(addr=0x%lx, size=%d)\n",
                   __func__,
                   __LINE__,
                   addr_offset,
                   FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Flash write */
        if (R_OTA_FLASH_API->write(R_OTA_FLASH_CTRL, (uint8_t *) buff_offset,
                                   (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA),
                                   (uint32_t) write_len) != FSP_SUCCESS)
        {
            printf("[%s:%d] Flash write failed(addr=0x%lx, size=%ld)\n", __func__, __LINE__, addr_offset, write_len);
            goto finish;
        }

        addr_offset += write_len;
        buff_offset += write_len;

        /* Stash pop */
        if (stash_len > 0)
        {
            if (R_OTA_FLASH_API->write(R_OTA_FLASH_CTRL, (uint8_t *) stash_buf,
                                       (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA),
                                       (uint32_t) stash_len) != FSP_SUCCESS)
            {
                printf("[%s:%d] Flash write failed(addr=0x%lx, size=%ld)\n", __func__, __LINE__, addr_offset,
                       stash_len);
                goto finish;
            }

            stash_len = 0;
        }

        tot_len -= write_len;
        vTaskDelay(1);                 // This is to allow task switching
    }

finish:

    if (stash_buf)
    {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0)
    {
        printf("[%s:%d] Failed size = %ld)\n", __func__, __LINE__, tot_len);

        return pdFALSE;
    }

    return pdTRUE;
}

bool rm_ota_w_util_api_sflash_erase (uint32_t sflash_addr, uint32_t len)
{
    unsigned int    addr_offset = 0;
    unsigned int    tot_len     = 0;
    unsigned int    write_len   = 0;
    unsigned char * stash_buf   = NULL;
    unsigned int    stash_len   = 0;

    addr_offset = sflash_addr;
    tot_len     = (unsigned int) len;

    if (((ospi_w_instance_ctrl_t *) R_OTA_FLASH_CTRL)->open != 0x4F535049) // ASCII characters "OSPI", Refer to r_ospi_w.c
    {
        R_OTA_FLASH_API->open(R_OTA_FLASH_CTRL, R_OTA_FLASH_CFG);
    }

    while (tot_len > 0)
    {
        if (tot_len > FLASH_SECTOR_SIZE)
        {
            write_len = FLASH_SECTOR_SIZE;
        }
        else
        {
            write_len = tot_len;
        }

        /* Since erasing is always 4KB, stash the data erased on the last write */
        if (write_len < FLASH_SECTOR_SIZE)
        {
            stash_len = FLASH_SECTOR_SIZE - write_len;

            stash_buf = (unsigned char *) pvPortMalloc(stash_len + 1);
            if (stash_buf == NULL)
            {
                printf("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, stash_len + 1);
                goto finish;
            }

            memset(stash_buf, 0x00, stash_len + 1);

            /* RRQ61X runs on the XIP it can read the flash contents through the memcpy() function */
            memcpy((void *) stash_buf,
                   (void *) ((addr_offset + write_len) | OSPI_W_DEVICE_START_ADDRESS_DATA),
                   stash_len);
        }

        /* Erase flash before writing */
        if (R_OTA_FLASH_API->erase(R_OTA_FLASH_CTRL, (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA),
                                   FLASH_SECTOR_SIZE) != FSP_SUCCESS)
        {
            printf("[%s:%d] Flash erase failed(addr=0x%x, size=%d)\n",
                   __func__,
                   __LINE__,
                   addr_offset,
                   FLASH_SECTOR_SIZE);
            goto finish;
        }

        /* Stash pop */
        addr_offset += write_len;

        if (stash_len > 0)
        {
            if (R_OTA_FLASH_API->write(R_OTA_FLASH_CTRL, (uint8_t *) stash_buf,
                                       (uint8_t *) (addr_offset | OSPI_W_DEVICE_START_ADDRESS_DATA),
                                       (uint32_t) stash_len) != FSP_SUCCESS)
            {
                printf("[%s:%d] Flash write failed(addr=0x%x, size=%d)\n", __func__, __LINE__, addr_offset, stash_len);
                goto finish;
            }

            stash_len = 0;
        }

        tot_len -= write_len;
        vTaskDelay(1);                 // This is to allow task switching
    }

finish:

    if (stash_buf)
    {
        vPortFree(stash_buf);
        stash_buf = NULL;
    }

    if (tot_len != 0)
    {
        printf("[%s:%d] Failed size = %d)\n", __func__, __LINE__, tot_len);

        return pdFALSE;
    }

    return pdTRUE;
}

bool rm_ota_w_util_api_sflash_copy (uint32_t dest_addr, uint32_t src_addr, uint32_t len)
{
    uint32_t offset   = 0;
    uint32_t loop_cnt = 0;
    uint32_t copy_len = 0;
    uint32_t tmp_len  = 0;
    char   * buf      = NULL;

    if ((dest_addr % FLASH_SECTOR_SIZE) || (src_addr % FLASH_SECTOR_SIZE))
    {
        printf("[%s] Flash address offset must be 4Kbyte\n", __func__);

        return pdFALSE;
    }

    copy_len = len;
    loop_cnt = len / FLASH_SECTOR_SIZE;

    if (loop_cnt > 0)
    {
        tmp_len = FLASH_SECTOR_SIZE;
    }
    else
    {
        tmp_len = len;
    }

    if (len % FLASH_SECTOR_SIZE)
    {
        loop_cnt = loop_cnt + 1;
    }

    buf = (char *) pvPortMalloc(tmp_len + 1);

    if (buf == NULL)
    {
        printf("[%s] Fail to alloc memory(%ldbytes)\n", __func__, tmp_len + 1);

        return pdFALSE;
    }

    while (loop_cnt--)
    {
        rm_ota_w_util_api_sflash_read(src_addr, buf, tmp_len);

        if (rm_ota_w_util_api_sflash_erase(dest_addr + offset, tmp_len) != pdTRUE)
        {
            printf("[%s] Erase failed\n", __func__);
            goto finish;
        }

        if (rm_ota_w_util_api_sflash_write((dest_addr + offset), buf, tmp_len) != pdTRUE)
        {
            printf("[%s] Write failed\n", __func__);
            goto finish;
        }

        offset += tmp_len;
        tmp_len = copy_len - tmp_len;
    }

finish:
    if (buf != NULL)
    {
        vPortFree(buf);
    }

    if (offset != len)
    {
        return pdFALSE;
    }

    return pdTRUE;
}
