/**
 ****************************************************************************************
 *
 * @file drv_usr_nvram.h
 *
 * @brief user NVRAM driver
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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
#include "hal_usr_nvram.h"

#if defined(__SUPPORT_USR_NVRAM__)
#define USR_NV_BANK_START SF_USER_NVRAM_START

#define USR_NV_IDX_MANAGE_CNT 256
#define USR_NV_IDX_BUF_SIZE 4096
#define USR_NV_CACHE_BUF_SIZE 4096

#define USR_NV_BANK_HDR_SIZE 4
#define USR_NV_BANK_SIZE 4096
#define USR_NV_BANK_MAX_CNT (SF_USER_NVRAM_SIZE / USR_NV_BANK_SIZE)

#define USR_NV_ITEM_MAGIC 0x4D49
#define USR_NV_ITEM_NAME_MAX_SIZE 16

#define bank_mask_64(p, v) \
        ((v > 31) ? \
        ((p)->h |= (1 << (v - 32))): \
        ((p)->l |= (1 << v)))

#define bank_unmask_64(p, v) \
            ((v > 31) ? \
            ((p)->h &= ~(1 << (v - 32))): \
            ((p)->l &= ~(1 << v)))

#define bank_check_mask_64(p, v) \
            ((v > 31) ? \
            ((p)->h & (1 << (v - 32))): \
            ((p)->l & (1 << v)))

enum bank_enable
{
    VAL_BANK_DISABLE = 0xFF,
    VAL_BANK_ENABLE = 0xA9,
    VAL_BANK_INABILITY = 0x01
};

enum item_latest
{
    VAL_ITEM_LATEST = 0xFE,
    VAL_ITEM_GC = 0xAA,
    VAL_ITEM_DEL = 0x00
};

enum item_type
{
    VAL_ITEM_STR = 0xFE,
    VAL_ITEM_INT = 0xFC,
    VAL_ITEM_BIN = 0xF8
};

struct usr_nvitem_index_struct
{
    uint32_t en:1;
    uint32_t tot_len:13;
    uint32_t bank:6;
    uint32_t offset:12;
    char item_name[USR_NV_ITEM_NAME_MAX_SIZE + 1];
}__packed;

struct nvitem_bank_struct
{
    uint8_t bank_en;
    uint8_t bank_idx;
    uint16_t erase_cnt;
}__packed;

struct usr_nvitem_struct
{
    uint16_t item_magic;
    uint8_t bank_idx;
    uint16_t offset;
    uint8_t type;
    uint8_t name_length;
    uint16_t data_length;
    uint8_t latest;
    uint16_t tot_length;
}__packed;

struct nv_cache_hdr_struct
{
    uint16_t used;
    uint16_t tot_hit;
};

struct nv_cache_struct
{
    struct usr_nvitem_struct item;
    uint16_t hit;
};

struct nv_bank_mask_struct
{
    uint32_t l;
    uint32_t h;
};

struct nv_bank_info_struct
{
    struct nv_bank_mask_struct bank_used;
    uint32_t bank_start;
    uint32_t total_used;
    uint32_t total_gc;
    uint16_t max_bank;
    uint8_t bank_status[USR_NV_BANK_MAX_CNT + 1];
    uint16_t gc[USR_NV_BANK_MAX_CNT + 1];
    uint16_t used[USR_NV_BANK_MAX_CNT + 1];
};

struct usr_nv_manage_struct
{
    uint8_t cache_en;
    uint8_t index_en;
    uint8_t gc_manager_en;
    uint32_t gc_manager_duration;

    struct nv_cache_hdr_struct cache_hdr;
    struct nv_cache_struct cache_item;
    struct nv_bank_info_struct bank_info;
};

void mgr_nvram_manager_update(uint8_t bank_reset, uint8_t bank_en, uint16_t bank, uint16_t used, uint16_t gc);
int8_t drv_nvitem_add(const char *name, uint8_t *val, uint16_t size);
int8_t drv_nvitem_read(const char *name, uint8_t **val, uint16_t *size);
int8_t drv_nvitem_del(const char *name);

void utl_show_bank_status(uint8_t all);
uint8_t utl_bank_reset(uint16_t bank);
uint32_t utl_get_nv_total_used(uint16_t bank);

int32_t prc_bank_gc(uint8_t init, uint16_t bank);

void drv_usr_nv_init(void);
void drv_usr_nv_sort(void);
void drv_usr_nv_lock(void);
void drv_usr_nv_unlock(void);
#endif // __SUPPORT_USR_NVRAM__
