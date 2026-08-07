/**
****************************************************************************************
*
* @file drv_usr_nvram.c
*
* @brief Driver of user NVRAM
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

#include "drv_usr_nvram.h"

#if defined(__SUPPORT_USR_NVRAM__)
#include <stdlib.h>
#include <memory.h>

#undef USRNV_DEBUG_L
#undef USRNV_DEBUG_H
#define USRNV_BOOT_GC

#define ITEM_CHK_FAIL 0
#define ITEM_CHK_SAME 2
#define ITEM_CHK_OK 1
#define RES_NOK 1
#define RES_OK 0

#define GET_ITEM_ADDR(b, o) (USR_NV_BANK_START + ((b - 1) * USR_NV_BANK_SIZE) + o)
#define USR_NV_ITEM_HDR_SIZE (sizeof(struct usr_nvitem_struct))
#define BANK_ITEM_START_PTR(p) ((uint8_t *)p + USR_NV_BANK_HDR_SIZE)
#define ITEM_NAME_PTR(t) ((uint8_t *)t + USR_NV_ITEM_HDR_SIZE)
#define ITEM_DATA_PTR(t) ((uint8_t *)t + USR_NV_ITEM_HDR_SIZE + (t)->name_length)
#define ITEM_NEXT_PTR(t) ((uint8_t *)t + (t)->tot_length)
#define USR_NV_ITEM_MAX_SIZE (USR_NV_BANK_SIZE - USR_NV_ITEM_HDR_SIZE - USR_NV_ITEM_NAME_MAX_SIZE)

static SemaphoreHandle_t sem_usrnv;
struct usr_nv_manage_struct g_nv_manager;
struct usr_nvitem_index_struct g_index_manager[INDEX_MAX_NUM];

uint16_t index_item_max;
uint8_t usr_bank_buffer[USR_NV_BANK_SIZE];

static u8 gc_manager_timer_activate;
uint8_t utl_nvitem_bit_check(struct usr_nvitem_struct *org, uint8_t *update, uint16_t len)
{
    uint32_t *org_val, *new_val;
    uint16_t offset = 0;

    if ((org == NULL) || (update == NULL)){
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] ptr error org : 0x%lx new : 0x%lx\n", __func__, __LINE__, (uint32_t)org, (uint32_t)update);
#endif
        return ITEM_CHK_FAIL;
    }

    if (org->data_length != len) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] data_length org : %d new : %d\n", __func__, __LINE__, org->data_length, len);
#endif
        return ITEM_CHK_FAIL;
    }
    org_val = (uint32_t *)ITEM_DATA_PTR(org);
    new_val = (uint32_t *)update;
    if (memcmp((const void *)org_val, (const void *)new_val, (uint32_t)len) == 0) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[SAME] Item Same\n");
#endif
        return ITEM_CHK_SAME;
    }

    while (1) {
        if (len >= 4) {
            if (((*org_val ^ *new_val) == 0) || (((*org_val ^ *new_val) & *org_val) == (*org_val ^ *new_val))) {
                offset++;
                new_val++;
                org_val++;
            } else {
#if defined(USRNV_DEBUG_L)
                PRINTF("[%s:%d] Fail offset : %d (0x%lx, 0x%lx)  0x%lx, 0x%lx\n", __func__, __LINE__
                , offset, (uint32_t)*org_val, (uint32_t)*new_val, (uint32_t)(*org_val ^ *new_val), (uint32_t)((*org_val ^ *new_val) & *org_val));
#endif
                return ITEM_CHK_FAIL;
            }
        }
        if (offset >= (len / 4)) {
            if (len % 4) {
                uint8_t *org_part = ITEM_DATA_PTR(org) + (offset * 4);
                uint8_t *new_part = update + (offset * 4);

                offset = len % 4;
                while (offset--) {
                    if (((*org_part ^ *new_part) == 0) || (((*org_part ^ *new_part) & *org_part) == (*org_part ^ *new_part))) {
                        new_part++;
                        org_part++;
                    } else {
#if defined(USRNV_DEBUG_L)
                        PRINTF("[%s:%d] Part Fail offset : %d (0x%x, 0x%x)  0x%x, 0x%x\n", __func__, __LINE__
                        , offset, *org_part, *new_part, (*org_part ^ *new_part), ((*org_part ^ *new_part) & *org_part));
#endif
                        return ITEM_CHK_FAIL;
                    }
                }
            }
            break;
        }
    }
#if defined(USRNV_DEBUG_L)
    PRINTF("[%s:%d] new : %s Updatable\n", __func__, __LINE__, update);
#endif
    org_val = (uint32_t *)ITEM_DATA_PTR(org);
    memcpy((void *)org_val, update, len);

    return ITEM_CHK_OK;
}

void *utl_get_temp_buf(uint32_t size)
{
    static void *gtmp_item = NULL;

    if (gtmp_item == NULL) {
        gtmp_item = pvPortMalloc(size);
    } else {
        vPortFree(gtmp_item);
        gtmp_item = pvPortMalloc(size);
    }

    if (gtmp_item != NULL)
        memset(gtmp_item, 0x00, size);

    return gtmp_item;
}

void *utl_get_index_buf(uint32_t size)
{
    static void *gtmp_item = NULL;

    if (gtmp_item == NULL) {
        gtmp_item = pvPortMalloc(size);
    } else {
        vPortFree(gtmp_item);
        gtmp_item = pvPortMalloc(size);
    }

    if (gtmp_item != NULL)
        memset(gtmp_item, 0x00, size);

    return gtmp_item;
}

uint8_t utl_chk_bank_clr(uint16_t bank)
{
    uint32_t *bank_chk_buf, *val;
    uint32_t bank_addr;
    uint16_t i = 0;
    int32_t res;

    val = bank_chk_buf = pvPortMalloc(USR_NV_BANK_SIZE);
    if (bank_chk_buf) {
        bank_addr = GET_ITEM_ADDR(bank, 0);
        res = hal_user_nv_sflash_read(bank_addr, bank_chk_buf, USR_NV_BANK_SIZE);

        if (res) {
            while (i < (USR_NV_BANK_SIZE / 4)) {
                if (*val != 0xFFFFFFFF) {
                    vPortFree(bank_chk_buf);
                    return ITEM_CHK_FAIL;
                }
                val++;
                i++;
            }
        } else {
            PRINTF("[%s:%d] nv read fail\n", __func__, __LINE__);
            vPortFree(bank_chk_buf);
            return ITEM_CHK_FAIL;
        }
    } else
        return ITEM_CHK_FAIL;

    vPortFree(bank_chk_buf);
#if defined(USRNV_DEBUG_L)
    PRINTF("[%s:%d] count : %d end : 0x%lx start : 0x%lx\n", __func__, __LINE__, i, (uint32_t)val, (uint32_t)bank_chk_buf);
#endif
    return ITEM_CHK_OK;
}

uint8_t utl_is_string(uint8_t *buf, uint32_t len)
{
    uint32_t cnt  = 0;
    while (cnt < len) {
        if ((buf[cnt] < 0x20) || (buf[cnt] > 0x7E))
            return 0;
        cnt++;
    }
    return 1;
}

void utl_show_bank_status(uint8_t all)
{
    struct nvitem_bank_struct *bank_hdr;
    struct usr_nvitem_struct *f_item;
    void *bank_ptr, *name_ptr, *data_ptr;

    uint32_t bank_addr, offset, total_used = 0, total_gc = 0;
    uint16_t bank = 1;
    char *prn_buf;
    int32_t res;

#define LEN_PRN_LINE 64
#define LEN_NAME_FIELD 30
    bank_ptr = pvPortMalloc(USR_NV_BANK_SIZE);
    prn_buf = pvPortMalloc(LEN_PRN_LINE + 1);
    PRINTF("=====================    NV STATUS    =====================\n\n");
    if (all)
        PRINTF("- BankBit : 0x%08lx,%08lx Max Bank : %02d\n"
        , g_nv_manager.bank_info.bank_used.h, g_nv_manager.bank_info.bank_used.l, g_nv_manager.bank_info.max_bank);
    while (bank <= g_nv_manager.bank_info.max_bank) {
        if (bank < 32) {
            if (!(g_nv_manager.bank_info.bank_used.l & (1 << bank))) {
                bank++;
                continue;
            }
        } else {
            if (!(g_nv_manager.bank_info.bank_used.h & (1 << (bank - 32)))) {
                bank++;
                continue;
            }
        }

        if ((all) && (all <= USR_NV_BANK_MAX_CNT)) {
            if (all != bank) {
                bank++;
                continue;
            }
        }
        bank_addr = GET_ITEM_ADDR(bank, 0);
        memset(bank_ptr, 0x00, USR_NV_BANK_SIZE);
        res = hal_user_nv_sflash_read(bank_addr, bank_ptr, USR_NV_BANK_SIZE);

        if (res == 0) {
            PRINTF("[ERR_USRNV] Flash read Error\n");
        }
        //bank info
        bank_hdr = (struct nvitem_bank_struct *)bank_ptr;
        if (bank_hdr->bank_en == VAL_BANK_ENABLE) {
            total_used += g_nv_manager.bank_info.used[bank];
            total_gc += g_nv_manager.bank_info.gc[bank];
            if (all)
                PRINTF("\n- Bank_%03d [Addr:0x%lx]  [ Erased : %05d, Used : %d, GC : %d]\n"
                    , bank_hdr->bank_idx, bank_addr
                    , bank_hdr->erase_cnt
                    , g_nv_manager.bank_info.used[bank]
                    , g_nv_manager.bank_info.gc[bank]);
            //item_info
            f_item = (struct usr_nvitem_struct *)((void *)bank_hdr + USR_NV_BANK_HDR_SIZE);
            offset = 0;
            while (1) {
                if (f_item->item_magic != USR_NV_ITEM_MAGIC) {
#if defined (USRNV_DEBUG_L)
                    PRINTF("[%s:%d] magic fail(0x%x) bank(%d), item_offset(%ld) used (%d)\n"
                    , __func__, __LINE__, f_item->item_magic, bank, offset, g_nv_manager.bank_info.used[bank]);
#endif
                    break;
                }

                if (f_item->latest == VAL_ITEM_LATEST) {
                    name_ptr = ITEM_NAME_PTR(f_item);
                    data_ptr = ITEM_DATA_PTR(f_item);
                    memset(prn_buf, 0x00, LEN_PRN_LINE + 1);
                    memcpy(prn_buf, name_ptr, (f_item->name_length > 16) ? 16 : f_item->name_length);
                    snprintf(&prn_buf[f_item->name_length], 8, " (%03d) ", f_item->data_length);
                    memset(&prn_buf[strlen(prn_buf)], 0x2E, LEN_NAME_FIELD - strlen(prn_buf));
                    prn_buf[strlen(prn_buf)] = 0x20;
                    if (utl_is_string(data_ptr, f_item->data_length)) {
                        memcpy(&prn_buf[strlen(prn_buf)]
                            , data_ptr
                            , (f_item->data_length > (LEN_PRN_LINE - strlen(prn_buf))) ? (LEN_PRN_LINE - strlen(prn_buf)) : f_item->data_length);
                    } else {
                        uint8_t prn_offset = strlen(prn_buf);
                        char *prn_ptr = &prn_buf[prn_offset];
                        uint16_t data_offset = 0;
                        while (1) {
                            snprintf(prn_ptr, 4, "x%02x", *((uint8_t *)data_ptr++));
                            prn_offset += 3;
                            prn_ptr = &prn_buf[prn_offset];
                            if (prn_offset >= LEN_PRN_LINE)
                                break;
                            data_offset++;
                            if (data_offset >= f_item->data_length)
                                break;
                        }
                    }
                    PRINTF("%s\n", prn_buf);
                }
                offset += f_item->tot_length;
                if (offset > (USR_NV_BANK_SIZE)) {
#if defined (USRNV_DEBUG_L)
                    PRINTF("[%s:%d] bank (%d, %ld) check done\n"
                    , __func__, __LINE__, bank, offset);
#endif
                    break;
                }
                f_item = (struct usr_nvitem_struct *)ITEM_NEXT_PTR(f_item);
            }
        }
        bank++;
    }
    if (all)
        PRINTF("\n- Bank_Total  [ Used : %ld (%ld), GC : %ld] \n", total_used, total_used - total_gc, total_gc);
    PRINTF("===========================================================\n");
    vPortFree(prn_buf);
    vPortFree(bank_ptr);
}

uint8_t utl_bank_reset(uint16_t bank)
{
    uint32_t bank_addr;

    if (bank == 0) {
        PRINTF("[%s, %d] need to implement cache init\n", __func__, __LINE__);
    } else if (bank <= g_nv_manager.bank_info.max_bank) {
        if (g_nv_manager.bank_info.used[bank] == 0)
        {
            return RES_OK;
        }
        bank_addr = GET_ITEM_ADDR(bank, 0);
        if (!utl_chk_bank_clr(bank))
            hal_user_nv_sflash_erase(bank_addr, USR_NV_BANK_SIZE);
        mgr_nvram_manager_update(1, VAL_BANK_DISABLE, bank, 0, 0);
#if defined (USRNV_DEBUG_L)
        if (g_nv_manager.index_en == 1)
            PRINTF("[%s, %d] need to implement index reconstruct or reboot\n", __func__, __LINE__);
#endif
    } else {
        uint16_t l_bank = 1;
        while (l_bank <= g_nv_manager.bank_info.max_bank) {
            bank_addr = GET_ITEM_ADDR(l_bank, 0);
            if (!utl_chk_bank_clr(l_bank)) {
                hal_user_nv_sflash_erase(bank_addr, USR_NV_BANK_SIZE);
            }
            mgr_nvram_manager_update(1, VAL_BANK_DISABLE, l_bank, 0, 0);
            l_bank++;
        }
#if defined (USRNV_DEBUG_L)
        if (g_nv_manager.index_en == 1)
            PRINTF("[%s, %d] need to implement index reconstruct or reboot\n", __func__, __LINE__);
#endif
    }
    return RES_OK;
}

uint32_t utl_get_nv_total_used(uint16_t bank)
{
    if ((bank > 0) && (bank <= USR_NV_BANK_MAX_CNT)) {
        return g_nv_manager.bank_info.used[bank];
    } else
        return g_nv_manager.bank_info.total_used;
}

int8_t fls_nvitem_bank_init(uint16_t bank)
{
    struct nvitem_bank_struct pbank;
    uint32_t bank_addr;
    int32_t res;

    pbank.bank_en = VAL_BANK_ENABLE;
    pbank.bank_idx = bank;
    pbank.erase_cnt = 1;
    
    bank_addr = GET_ITEM_ADDR(bank, 0);

    if (!utl_chk_bank_clr(bank)) {
        hal_user_nv_sflash_erase(bank_addr, USR_NV_BANK_SIZE);
    }

    res = hal_user_nv_sflash_write(bank_addr, (uint8_t *)&pbank, sizeof(pbank));
    mgr_nvram_manager_update(1, VAL_BANK_DISABLE, bank, 0, 0);
    mgr_nvram_manager_update(0, VAL_BANK_ENABLE, bank, sizeof(pbank), 0);

    if (res)
        return RES_OK;
    else
        return RES_NOK;
}

int8_t fls_nvitem_set(void *item, uint8_t update)
{
    struct usr_nvitem_struct *l_item = (struct usr_nvitem_struct *)item;
    uint32_t bank_addr;
    int32_t res;

    bank_addr = GET_ITEM_ADDR(l_item->bank_idx, l_item->offset);
    if (l_item->latest == VAL_ITEM_GC)
        res = hal_user_nv_sflash_write(bank_addr, (uint8_t *)l_item, USR_NV_ITEM_HDR_SIZE);
    else
        res = hal_user_nv_sflash_write(bank_addr, (uint8_t *)l_item, l_item->tot_length);
#if defined(USRNV_DEBUG_L)
    PRINTF("[%s:%d] addr=0x%lx, offset =0x%x\n", __func__, __LINE__, (uint32_t)bank_addr, l_item->offset);
#endif

    if (res) {
        if (!update) {
            if (l_item->latest == VAL_ITEM_LATEST)
                mgr_nvram_manager_update(0, VAL_BANK_ENABLE, l_item->bank_idx, l_item->tot_length, 0);
            else
                mgr_nvram_manager_update(0, VAL_BANK_ENABLE, l_item->bank_idx, 0, l_item->tot_length);
        }
        return RES_OK;
    } else
        return RES_NOK;
}

int8_t fls_nvitem_get(void *item)
{
    struct usr_nvitem_struct *l_item = (struct usr_nvitem_struct *)item;
    uint32_t bank_addr;
    int32_t res;

    bank_addr = GET_ITEM_ADDR(l_item->bank_idx, l_item->offset);
    res = hal_user_nv_sflash_read(bank_addr, l_item, l_item->tot_length);

    if (res)
        return RES_OK;
    else
        return RES_NOK;
}

int8_t mgr_nvram_cache(const char *name, int *_val)
{
    PRINTF("[%s, %d] TBD need to implement\n", __func__, __LINE__);
    return ITEM_CHK_FAIL;
}

void mgr_nvram_manager_update(uint8_t bank_reset, uint8_t bank_en, uint16_t bank, uint16_t used, uint16_t gc)
{
#if defined(USRNV_DEBUG_L)
    PRINTF("[%s, %d] reset : %d, enable : 0x%x, bank : %02d, used : %d, gc : %d\n"
        , __func__, __LINE__, bank_reset, bank_en, bank, used, gc);
#endif
    if (bank_reset) {
        g_nv_manager.bank_info.total_used -= (g_nv_manager.bank_info.used[bank]);
        g_nv_manager.bank_info.total_gc -=g_nv_manager.bank_info.gc[bank];
        g_nv_manager.bank_info.used[bank] = 0;
        g_nv_manager.bank_info.gc[bank] = 0;
        g_nv_manager.bank_info.bank_status[bank] = VAL_BANK_DISABLE;
        bank_unmask_64(&g_nv_manager.bank_info.bank_used, bank);
    } else {
        g_nv_manager.bank_info.used[bank] += used;
        g_nv_manager.bank_info.gc[bank] += gc;
        g_nv_manager.bank_info.bank_status[bank] = bank_en;
        if (bank_en == VAL_BANK_ENABLE)
            bank_mask_64(&g_nv_manager.bank_info.bank_used, bank);
        else
            bank_unmask_64(&g_nv_manager.bank_info.bank_used, bank);
        g_nv_manager.bank_info.total_used += used;
        g_nv_manager.bank_info.total_gc += gc;
    }
#if defined(USRNV_DEBUG_H)
        PRINTF("[%s:%d] bank(%02d), used(%d), gc(%d), tot_used(%ld), tot_gc(%ld)\n", __func__, __LINE__
            , bank, used, gc, g_nv_manager.bank_info.total_used, g_nv_manager.bank_info.total_gc);
#endif
    return;
}

void mgr_nvram_gc_manager_callback(void)
{
    uint16_t bank, most_gc_bank = 1;
    uint32_t most_gc_num = 0;
    uint32_t tot_nv = USR_NV_BANK_SIZE * USR_NV_BANK_MAX_CNT;
    uint64_t start_time;

    if (gc_manager_timer_activate == 0) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] Not Activated\n", __func__, __LINE__);
#endif
        return;
    }
    if (g_nv_manager.bank_info.total_used < ((tot_nv / 100) * GC_USED_PERCENT)) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] doesn't need gc total_used %ld\n", __func__, __LINE__, g_nv_manager.bank_info.total_used);
#endif
        return;
    }

    if (g_nv_manager.bank_info.total_gc < ((tot_nv / 100) * GC_GC_PERCENT)) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] doesn't need gc total_gc %ld\n", __func__, __LINE__, g_nv_manager.bank_info.total_gc);
#endif
        return;
    }
    start_time = hal_utl_get_cur_time();

    drv_usr_nv_lock();
continue_gc:
    for (bank = 1; bank <= g_nv_manager.bank_info.max_bank; bank++) {
        if (bank_check_mask_64(&g_nv_manager.bank_info.bank_used, bank)) {
            if (g_nv_manager.bank_info.gc[bank] > most_gc_num) {
                most_gc_num = g_nv_manager.bank_info.gc[bank];
                most_gc_bank = bank;
            }
        }
    }
    prc_bank_gc(0, most_gc_bank);

#if defined(USRNV_DEBUG_T)
    PRINTF("[%s:%d] GCTime %lld.%lld ms\n", __func__, __LINE__, (hal_utl_get_cur_time() - start_time) / 1000, (hal_utl_get_cur_time() - start_time) % 1000);
#endif // USRNV_DEBUG_T
    if (g_nv_manager.bank_info.total_gc < ((tot_nv / 100) * GC_URGENT_PERCENT)) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] doesn't need gc %ld\n", __func__, __LINE__, g_nv_manager.bank_info.total_used);
#endif
        drv_usr_nv_unlock();
        return;
    }

    if ((hal_utl_get_cur_time() - start_time) > GC_ACT_TIME) {
        drv_usr_nv_unlock();
        return;
    }

    goto continue_gc;
}

void mgr_nvram_gc_manager_activate(uint8_t act)
{
    gc_manager_timer_activate = act;
}

uint8_t prc_index_init(void)
{
    if (g_nv_manager.index_en == 0)
        return RES_NOK;

    index_item_max = 0;
    memset((void *) g_index_manager, 0x00, sizeof(g_index_manager));
    return RES_OK;
}

void prc_index_deinit(void)
{
    return;
}

int16_t prc_index_find(const char *name, struct usr_nvitem_struct **item)
{
    struct usr_nvitem_index_struct *i_ptr = g_index_manager;
    uint32_t bank_addr;
    uint16_t idx_count = 0, item_en_cnt = 0;

    if (g_nv_manager.index_en == 0) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[%s:%d] indexer is not enabled\n", __func__, __LINE__);
#endif
        return -1;
    }

    if (index_item_max == 0) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[%s:%d] index_item_max %d\n", __func__, __LINE__, index_item_max);
#endif
        return -1;
    }

    while (1) {
        if (i_ptr[idx_count].en && (strcmp((char *)i_ptr[idx_count].item_name, name) == 0)) {
            *item = (struct usr_nvitem_struct *)utl_get_index_buf(i_ptr[idx_count].tot_len);
            bank_addr = GET_ITEM_ADDR(i_ptr[idx_count].bank, i_ptr[idx_count].offset);
            hal_user_nv_sflash_read(bank_addr, (void *)*item, i_ptr[idx_count].tot_len);
#if defined(USRNV_DEBUG_H)
            PRINTF("[IDX:FOUND] idx (%d) name (%s), idx_tot_len(%d), idx_bank(%d), idx_offset(0x%04x), itm_name_len(%d), itm_tot_length(%d), itm_latest(0x%x)\n"
                , idx_count, name, i_ptr[idx_count].tot_len, i_ptr[idx_count].bank, i_ptr[idx_count].offset, (*item)->name_length, (*item)->tot_length, (*item)->latest);
#endif
            return idx_count;
        }

        if (i_ptr[idx_count].en)
            item_en_cnt++;
        idx_count++;
        if (item_en_cnt >= index_item_max)
            break;
    }
    return -1;
}

int16_t prc_index_find_blank(void)
{
    struct usr_nvitem_index_struct *i_ptr = g_index_manager;
    uint16_t idx_count = 0;

    if (g_nv_manager.index_en == 0) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[%s:%d] indexer is not enabled\n", __func__, __LINE__);
#endif
        return -1;
    }

    if (index_item_max >= INDEX_MAX_NUM) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[%s:%d] index_item_max %d\n", __func__, __LINE__, index_item_max);
#endif
        return -1;
    }

    while (1) {
        if (i_ptr[idx_count].en == 0) {
            return idx_count;
        }
        idx_count++;
        if (idx_count >= INDEX_MAX_NUM)
            break;
    }
    return -1;
}

void prc_index_add(struct usr_nvitem_struct *item)
{
    struct usr_nvitem_index_struct *i_ptr = g_index_manager;
    struct usr_nvitem_struct *tmp;
    uint8_t name[USR_NV_ITEM_NAME_MAX_SIZE + 1];
    int16_t idx, blank;

    if (g_nv_manager.index_en == 0) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] indexer is not enabled\n", __func__, __LINE__);
#endif
        return;
    }

    if (index_item_max >= INDEX_MAX_NUM) {
        PRINTF("[%s:%d] index overflow\n", __func__, __LINE__);
        return;
    }

    memset(name, 0x00, USR_NV_ITEM_NAME_MAX_SIZE + 1);
    memcpy(name, (void *)ITEM_NAME_PTR(item), item->name_length);

    if ((idx = prc_index_find((const char *)name, &tmp)) >= 0) {
        i_ptr[idx].bank = item->bank_idx;
        i_ptr[idx].offset = item->offset;
        i_ptr[idx].tot_len = item->tot_length;
        memcpy(i_ptr[idx].item_name, ITEM_NAME_PTR(item), item->name_length);
        i_ptr[idx].item_name[item->name_length] = 0;
#if defined(USRNV_DEBUG_H)
        PRINTF("[INDEX:UPDATE] idx(%d) idx_max(%d) name[%s], bank(%d), offset(0x%x), tot_len(%d)\n"
            , idx, index_item_max, i_ptr[idx].item_name, i_ptr[idx].bank, i_ptr[idx].offset, i_ptr[idx].tot_len);
#endif
    } else {
        if ((blank = prc_index_find_blank()) >= 0) {
            i_ptr[blank].en = 1;
            i_ptr[blank].bank = item->bank_idx;
            i_ptr[blank].offset = item->offset;
            i_ptr[blank].tot_len = item->tot_length;
            memcpy(i_ptr[blank].item_name, ITEM_NAME_PTR(item), item->name_length);
            i_ptr[blank].item_name[item->name_length] = 0;
#if defined(USRNV_DEBUG_H)
            PRINTF("[INDEX:ADD] idx(%d) idx_max(%d) name[%s], bank(%d), offset(0x%x), tot_len(%d)\n"
                , blank, index_item_max, i_ptr[blank].item_name, i_ptr[blank].bank, i_ptr[blank].offset, i_ptr[blank].tot_len);
#endif
            index_item_max++;
        } else {
            PRINTF("[INDEX:ADD FAIL]\n");
        }
    }

    return;
}

uint8_t prc_index_remove(struct usr_nvitem_struct *item)
{
    struct usr_nvitem_index_struct *i_ptr = g_index_manager;
    uint8_t name[USR_NV_ITEM_NAME_MAX_SIZE + 1];
    int16_t idx;
    struct usr_nvitem_struct *tmp;

    if (index_item_max <= 0) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] indexer is empty\n", __func__, __LINE__);
#endif
        return ITEM_CHK_FAIL;
    }

    memset(name, 0x00, USR_NV_ITEM_NAME_MAX_SIZE + 1);
    memcpy(name, ITEM_NAME_PTR(item), item->name_length);
    if ((idx = prc_index_find((const char *)name, &tmp)) >= 0) {
        i_ptr[idx].en = 0;
        index_item_max--;
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] indexer is removed idx_max(%d), size_idx(%d), idx(%d), name : %s, struct size : (%d)\n"
            , __func__, __LINE__, index_item_max, sizeof (g_index_manager), idx, name, sizeof(struct usr_nvitem_index_struct));
#endif
        return ITEM_CHK_OK;
    } else {
        return ITEM_CHK_FAIL;
    }
}

uint16_t prc_bank_sort(uint8_t *data, uint32_t len, uint16_t *garbage)
{
    struct usr_nvitem_struct *item;
    uint16_t offset = sizeof(struct nvitem_bank_struct);

    *garbage = 0;
    while (offset <= len) {
        item = (struct usr_nvitem_struct *)(data + offset);

        if (item->item_magic != USR_NV_ITEM_MAGIC)
            break;

        if (item->latest != VAL_ITEM_LATEST)
            *garbage += item->tot_length;
        else
            prc_index_add(item);

        offset += item->tot_length;
    }

    return (offset);
}

int32_t prc_bank_gc(uint8_t init, uint16_t bank)
{
    struct usr_nvitem_struct *src;
    struct nvitem_bank_struct *bank_hdr = NULL;
    uint8_t *dst;
    uint16_t gc_hit = 0;
    uint32_t bank_addr;
    uint16_t offset = USR_NV_BANK_HDR_SIZE;
    uint16_t l_used = USR_NV_BANK_HDR_SIZE;
    uint16_t cp_len = 0;

    if ((g_nv_manager.bank_info.gc[bank] == 0) && (init == 0)) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] Coudln't gc bank, %d\n", __func__, __LINE__, bank);
#endif
        return ITEM_CHK_FAIL;
    }

    bank_hdr = (struct nvitem_bank_struct *)pvPortMalloc(USR_NV_BANK_SIZE);
    if (bank_hdr == NULL) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] bank_hdr NULL\n", __func__, __LINE__);
#endif
        return ITEM_CHK_FAIL;
    }

    src = (struct usr_nvitem_struct *)BANK_ITEM_START_PTR(bank_hdr);
    dst = BANK_ITEM_START_PTR(bank_hdr);

    bank_addr = GET_ITEM_ADDR(bank, 0);
    hal_user_nv_sflash_read(bank_addr, bank_hdr, USR_NV_BANK_SIZE /* or USR_NV_BANK_HDR_SIZE */);
    offset += src->tot_length;

    if (bank_hdr->bank_en != VAL_BANK_ENABLE) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s:%d] Disabled bank(%d)\n", __func__, __LINE__, bank);
#endif
        vPortFree(bank_hdr);
        return ITEM_CHK_FAIL;
    }

    while (1) {
        if ((src->item_magic != USR_NV_ITEM_MAGIC) || (src->tot_length == 0) || (src->name_length == 0) || (src->data_length == 0)) {
#if defined(USRNV_DEBUG_L)
            PRINTF("[%s:%d] src_data 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n"
                , __func__, __LINE__, *((uint8_t *)src + 0), *((uint8_t *)src + 1),  *((uint8_t *)src + 2),  *((uint8_t *)src + 3)
                , *((uint8_t *)src + 4), *((uint8_t *)src + 5),  *((uint8_t *)src + 6),  *((uint8_t *)src + 7)
                , *((uint8_t *)src + 8), *((uint8_t *)src + 9),  *((uint8_t *)src + 10) , *((uint8_t *)src + 11));
#endif
            break;
        }

        if (src->latest == VAL_ITEM_LATEST) {
            cp_len = src->tot_length;
            src->offset = l_used;

            prc_index_add(src);
            memcpy(dst, (void *)src, cp_len); //the source can be polluted, should run this code at last.

            l_used += cp_len;
            dst += cp_len;
        } else {
            cp_len = src->tot_length;
            gc_hit += src->tot_length;
#if defined(USRNV_DEBUG_H)
            PRINTF("[GCed] Bank : %02d, name: 00%c%c %d\n", bank, (ITEM_NAME_PTR(src))[2], (ITEM_NAME_PTR(src))[3], src->tot_length);
#endif
        }

        src = (struct usr_nvitem_struct *)((uint8_t *)(src) + cp_len);
        offset += src->tot_length;
        if (offset > (USR_NV_IDX_BUF_SIZE))
            break;
    }

    if (gc_hit > 0) {
        if (bank_hdr->erase_cnt < 0xFFFF)
            bank_hdr->erase_cnt++;
        hal_user_nv_sflash_erase(bank_addr, USR_NV_BANK_SIZE);
        hal_user_nv_sflash_write(bank_addr, (uint8_t *)bank_hdr, l_used);
    }
#if defined(USRNV_DEBUG_H)
    PRINTF("[GCed] Bank : %02d, used : %d, gc : %d\n", bank, l_used, gc_hit);
#endif

    mgr_nvram_manager_update(1, VAL_BANK_DISABLE, bank, 0, 0);
    mgr_nvram_manager_update(0, VAL_BANK_ENABLE, bank, l_used, 0);
    vPortFree(bank_hdr);
    return ITEM_CHK_OK;
}

uint8_t prc_finding_item_inpool(uint8_t *pool, uint32_t len, const char *name, struct usr_nvitem_struct **item)
{
    struct usr_nvitem_struct *f_item = (struct usr_nvitem_struct *)pool;
    uint8_t *f_name = ITEM_NAME_PTR(f_item);
    uint32_t offset =0;
#if defined(USRNV_DEBUG_L)
    uint8_t tmp[USR_NV_ITEM_NAME_MAX_SIZE + 1];
#endif

    while (1) {
#if defined(USRNV_DEBUG_L)
        memset(tmp, 0x00, USR_NV_ITEM_NAME_MAX_SIZE + 1);
        memcpy(tmp, f_name, (USR_NV_ITEM_NAME_MAX_SIZE < f_item->name_length) ? USR_NV_ITEM_NAME_MAX_SIZE : f_item->name_length);

        PRINTF("[%s:%d] chk : %s, name : %s, latest : 0x%x, offset : 0x%x, tot_len : %d (0x%lx,%lx)\n"
            , __func__, __LINE__, name, tmp, f_item->latest, f_item->offset, f_item->tot_length
            , g_nv_manager.bank_info.bank_used.h, g_nv_manager.bank_info.bank_used.l);
#endif
        if (f_item->item_magic != USR_NV_ITEM_MAGIC) {
#if defined(USRNV_DEBUG_L)
            PRINTF("[%s:%d] f_item_data 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n"
                , __func__, __LINE__, *((uint8_t *)f_item + 0), *((uint8_t *)f_item + 1),  *((uint8_t *)f_item + 2),  *((uint8_t *)f_item + 3)
                , *((uint8_t *)f_item + 4), *((uint8_t *)f_item + 5),  *((uint8_t *)f_item + 6),  *((uint8_t *)f_item + 7)
                , *((uint8_t *)f_item + 8), *((uint8_t *)f_item + 9),  *((uint8_t *)f_item + 10) , *((uint8_t *)f_item + 11));
#endif
            return ITEM_CHK_FAIL;
        }

        if ((f_item->latest == VAL_ITEM_LATEST)
            && (strlen(name) == f_item->name_length)
            && (strncmp(name, (const char *)f_name, f_item->name_length) == 0)) {
            *item = f_item;
            return ITEM_CHK_OK;
        }
        offset += f_item->tot_length;
        f_item = (struct usr_nvitem_struct *)ITEM_NEXT_PTR(f_item);
        if (offset >= (len - USR_NV_ITEM_HDR_SIZE))
            return ITEM_CHK_FAIL;

        f_name = ITEM_NAME_PTR(f_item);
    }
}

int8_t prc_nvitem_add(const char *name, uint8_t *val, uint32_t size, uint16_t prebank)
{
    struct usr_nvitem_struct *new_item;
    void *bank_item_ptr = NULL, *name_ptr, *data_ptr;
    uint16_t bank;
    int32_t max_gc_bank = 0, max_gc = 0;
    int32_t item_malloc_size;

    item_malloc_size = USR_NV_ITEM_HDR_SIZE + strlen(name) + size;
    if (item_malloc_size > USR_NV_ITEM_MAX_SIZE) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s] Item size err %ld, %d, %d\n", __func__, item_malloc_size, USR_NV_ITEM_HDR_SIZE, strlen(name));
#endif
        return RES_NOK;
    }

    bank_item_ptr = pvPortMalloc(item_malloc_size + 4);
    memset(bank_item_ptr, 0x00, item_malloc_size + 4);
    if (bank_item_ptr == NULL) {
#if defined(USRNV_DEBUG_L)
        PRINTF("[%s] malloc error\n", __func__);
#endif
        return RES_NOK;
    }

    new_item = (struct usr_nvitem_struct *)bank_item_ptr;
    new_item->item_magic = USR_NV_ITEM_MAGIC;
    new_item->type = 0xFF;
    new_item->name_length = strlen(name);
    new_item->data_length = size;
    new_item->latest = VAL_ITEM_LATEST;
    new_item->tot_length = USR_NV_ITEM_HDR_SIZE + new_item->name_length + new_item->data_length;
    new_item->tot_length = ((new_item->tot_length + 3) >> 2) << 2;
    name_ptr = ITEM_NAME_PTR(bank_item_ptr);
    data_ptr = ITEM_DATA_PTR((struct usr_nvitem_struct *)bank_item_ptr);

    memcpy(name_ptr, name, new_item->name_length);
    memcpy(data_ptr, val, new_item->data_length);

    for (bank = 1; bank <= g_nv_manager.bank_info.max_bank; bank++) {
        if (g_nv_manager.bank_info.bank_status[bank] != VAL_BANK_INABILITY) {
            if ((USR_NV_BANK_SIZE - g_nv_manager.bank_info.used[bank]) >= new_item->tot_length) {
                if (g_nv_manager.bank_info.bank_status[bank] != VAL_BANK_ENABLE) {
                    fls_nvitem_bank_init(bank);
                }
                new_item->bank_idx = bank;
                new_item->offset = g_nv_manager.bank_info.used[bank];
                fls_nvitem_set(new_item, 0);
#if defined(USRNV_DEBUG_H)
                PRINTF("[ADD] name : %s, size : %ld, bank : %02d, offset : 0x%x\n"
                                , name, size, new_item->bank_idx, new_item->offset);
#endif
                prc_index_add(new_item);
                vPortFree(bank_item_ptr);
                return RES_OK;
            }
            if ((g_nv_manager.bank_info.gc[bank] > max_gc) && (bank != prebank)) {
                max_gc = g_nv_manager.bank_info.gc[bank];
                max_gc_bank = bank;
            }
        }
    }

    if ((max_gc + (USR_NV_BANK_SIZE - g_nv_manager.bank_info.used[max_gc_bank])) >= new_item->tot_length) {
        bank = max_gc_bank;
        prc_bank_gc(0, bank);
        new_item->bank_idx = bank;
        new_item->offset = g_nv_manager.bank_info.used[bank];
        fls_nvitem_set(new_item, 0);
        prc_index_add(new_item);
        vPortFree(bank_item_ptr);
#if defined(USRNV_DEBUG_H)
        PRINTF("[GC and ADD] name : %s, size : %ld, bank : %02d, offset : 0x%x\n"
                        , name, size, new_item->bank_idx, new_item->offset);
#endif
        return RES_OK;
    }
    vPortFree(bank_item_ptr);
#if defined(USRNV_DEBUG_H)
    PRINTF("[ADD FAIL] name : %s, size : %ld, bank : %02d, offset : 0x%x\n"
                    , name, size, new_item->bank_idx, new_item->tot_length);
#endif
    return RES_NOK;
    //TBD check inability bank
}

uint8_t prc_nvitem_find(const char *name, struct usr_nvitem_struct **item)
{
#if defined(USRNV_DEBUG_L)
    struct nvitem_bank_struct *bank_hdr;
#endif
    void *bank_ptr = usr_bank_buffer;
    uint16_t bank;
    uint32_t bank_addr;

    if (prc_index_find(name, item) >= 0)
        return ITEM_CHK_OK;
    else {
        if (g_nv_manager.index_en == 1)
            return ITEM_CHK_FAIL;
    }
    for (bank = 1; bank <= g_nv_manager.bank_info.max_bank; bank++) {
        if (bank_check_mask_64(&g_nv_manager.bank_info.bank_used, bank)) {
            bank_addr = GET_ITEM_ADDR(bank, 0);
            hal_user_nv_sflash_read(bank_addr, bank_ptr, USR_NV_BANK_SIZE);
#if defined(USRNV_DEBUG_L)
            bank_hdr = (struct nvitem_bank_struct *)bank_ptr;
            PRINTF("[%s:%d] bank_hdr : 0x%lx, bank_en : 0x%x, bank_index : 0x%x, bank_erase : 0x%x\n"
                , __func__, __LINE__, (uint32_t)bank_hdr, bank_hdr->bank_en, bank_hdr->bank_idx, bank_hdr->erase_cnt);
#endif
            if (prc_finding_item_inpool((uint8_t *)(bank_ptr + USR_NV_BANK_HDR_SIZE)
                    , USR_NV_BANK_SIZE - USR_NV_BANK_HDR_SIZE, name, item)) {
#if defined(USRNV_DEBUG_H)
                    PRINTF("[FOUND] bank : %02d, offset : 0x%x\n", bank, (*item)->offset);
#endif
                return ITEM_CHK_OK;
            }
        } else {
#if defined(USRNV_DEBUG_L)
            PRINTF("[%s:%d] unused bank %d, used : 0x%lx,%lx\n"
                , __func__, __LINE__, bank, g_nv_manager.bank_info.bank_used.h, g_nv_manager.bank_info.bank_used.l);
#endif
        }
    }
     return ITEM_CHK_FAIL;
}

int8_t drv_nvitem_add(const char *name, uint8_t *val, uint16_t size)
{
    struct usr_nvitem_struct *item;
    struct usr_nvitem_struct *tmp_item;
    uint8_t update;
    int32_t res;

    if (strlen(name) > USR_NV_ITEM_NAME_MAX_SIZE) {
#if defined(USRNV_DEBUG_H)
        PRINTF("[ERR_USRNV] Max name length is %d\n", USR_NV_ITEM_NAME_MAX_SIZE);
#endif
        return RES_NOK;
    }

    if (prc_nvitem_find(name, &item)) {
        tmp_item = pvPortMalloc(item->tot_length);
        if (tmp_item == NULL) {
#if defined(USRNV_DEBUG_L)
            PRINTF("[%s:%d] Malloc Failed]\n", __func__, __LINE__);
#endif
            return RES_NOK;
        }
        memcpy(tmp_item, item, item->tot_length);
        update = utl_nvitem_bit_check(tmp_item, val, size);
        if (update) {
            if (update == ITEM_CHK_SAME) {
                res = RES_OK;
#if defined(USRNV_DEBUG_H)
                PRINTF("[%s:%d] Add Item same\n", __func__, __LINE__);
#endif
            } else {
                res = fls_nvitem_set(tmp_item, 1);
                if (res == RES_OK) {
#if defined(USRNV_DEBUG_H)
                PRINTF("[UPDATE] %s, bank : %02d, offset : 0x%x, res(0x%lx)\n"
                    , name, tmp_item->bank_idx, tmp_item->offset, res);
#endif
                } else {
                    PRINTF("[%s:%d] Item Update Failed\n", __func__, __LINE__);
                }
            }
        } else { 
            res = prc_nvitem_add(name, val, size, tmp_item->bank_idx);
            if (res == RES_OK) {
                tmp_item->latest = VAL_ITEM_GC;
                res = fls_nvitem_set(tmp_item, 0);
                if (res == RES_NOK) {
                    PRINTF("[%s:%d] [Add GC Set Failed]\n", __func__, __LINE__);
                }
            } else
                PRINTF("[%s:%d] Item Add Failed\n", __func__, __LINE__);
        }
        vPortFree(tmp_item);
    } else {
        res = prc_nvitem_add(name, val, size, 0);
        if (res == RES_NOK) {
            PRINTF("[%s:%d] Item Add Failed\n", __func__, __LINE__);
        }
    }
    return res;
}

int8_t drv_nvitem_read(const char *name, uint8_t **val, uint16_t *size)
{
    struct usr_nvitem_struct *item;

    if (g_nv_manager.cache_en) {
        PRINTF("[%s, %d] TBD need to implement\n", __func__, __LINE__);
    }

    if (prc_nvitem_find(name, &item)) {
        *size = item->data_length;
        *val = utl_get_temp_buf(item->data_length + 1);
        if (*val == NULL) {
            PRINTF("[READ] Malloc Err\n");
            return RES_NOK;
        }
        memcpy(*val, ITEM_DATA_PTR(item), item->data_length);
        (*val)[item->data_length] = 0;
#if defined(USRNV_DEBUG_H)
        PRINTF("[READ] name : %s, size : %d, bank : %02d, offset : 0x%x, data_len ; %d\n"
                        , name, *size, item->bank_idx, item->offset, item->data_length);
#endif
        return RES_OK;
    } else {
        *val = NULL;
        *size = 0;
        return RES_NOK;
    }
}

int8_t drv_nvitem_del(const char *name)
{
    struct usr_nvitem_struct *item;
    int32_t res;

    if (prc_nvitem_find(name, &item)) {
        item->latest = VAL_ITEM_GC;
        res = fls_nvitem_set(item, 0);
        prc_index_remove(item);
#if defined(USRNV_DEBUG_H)
        PRINTF("[REMOVE] name : %s, bank : %02d, offset : 0x%x\n"
                        , name, item->bank_idx, item->offset);
#endif
        if (res)
            return RES_NOK;
        else
            return RES_OK;
    } else {
        return RES_NOK;
    }
    return RES_OK;
}

void drv_usr_nv_cfg(void)
{
    PRINTF("[%s:%d] need to implement read nvram to config usr nvram\n", __func__, __LINE__);
}

void drv_usr_nv_init(void)
{
    sem_usrnv = xSemaphoreCreateMutex();
    if (sem_usrnv == NULL) {
        PRINTF(" Create error: usr nvram semaphore\n");
    }
    g_nv_manager.bank_info.bank_start = SF_USER_NVRAM_START;
    // the max bank should be lower than 64
    g_nv_manager.bank_info.max_bank = SF_USER_NVRAM_SIZE /USR_NV_BANK_SIZE;
#if defined (__SUPPORT_USR_NVRAM_CACHE__)
    g_nv_manager.cache_en = 1;
#else
    g_nv_manager.cache_en = 0;
#endif // __SUPPORT_USR_NVRAM_CACHE__
#if defined (__SUPPORT_USR_NVRAM_INDEX__)
    g_nv_manager.index_en = 1;
#else
    g_nv_manager.index_en = 0;
#endif // __SUPPORT_USR_NVRAM_INDEX__

    g_nv_manager.gc_manager_duration = 1000;
    g_nv_manager.gc_manager_en = 1;

    if (g_nv_manager.gc_manager_en)
        gc_manager_timer_activate = 1;
    else
        gc_manager_timer_activate = 0;
#if defined (__SUPPORT_USR_NVRAM_SUPPORT_NVCFG__)
#endif

    prc_index_init();
    if (g_nv_manager.gc_manager_en) {
        hal_os_create_timer("usrnv_gc", g_nv_manager.gc_manager_duration, mgr_nvram_gc_manager_callback);
    }
}

void drv_usr_nv_sort(void)
{
#if !defined (USRNV_BOOT_GC)
    struct nvitem_bank_struct *bank_hdr;
    void *bank_ptr = usr_bank_buffer;
    uint32_t bank_addr;
#endif
    uint16_t bank;

    drv_usr_nv_lock();
#if defined(USRNV_DEBUG_T)
    hal_utl_check_time(1, NULL);
#endif // USRNV_DEBUG_T
    g_nv_manager.bank_info.bank_used.l = 0;
    g_nv_manager.bank_info.bank_used.h = 0;
    for (bank = 1; bank <= g_nv_manager.bank_info.max_bank; bank++) {
#if defined (USRNV_BOOT_GC)
        prc_bank_gc(1, bank);
#else
        bank_addr = GET_ITEM_ADDR(bank, 0);
        hal_user_nv_sflash_read(bank_addr, bank_ptr, USR_NV_BANK_SIZE /* or USR_NV_BANK_HDR_SIZE */);
        bank_hdr = (struct nvitem_bank_struct *)bank_ptr;
        g_nv_manager.bank_info.bank_status[bank] = bank_hdr->bank_en;
        if (bank_hdr->bank_en == VAL_BANK_ENABLE) {
            bank_mask_64(&g_nv_manager.bank_info.bank_used, bank);
            g_nv_manager.bank_info.used[bank] = 
                prc_bank_sort(bank_ptr, USR_NV_BANK_SIZE, &g_nv_manager.bank_info.gc[bank]);
            g_nv_manager.bank_info.total_used += g_nv_manager.bank_info.used[bank];
            g_nv_manager.bank_info.total_gc += g_nv_manager.bank_info.gc[bank];
        }
#endif
    }
#if defined(USRNV_DEBUG_T)
    hal_utl_check_time(2, "drv_usr_nv_sort");
#endif // USRNV_DEBUG_T
    drv_usr_nv_unlock();

    return;
}

void drv_usr_nv_lock(void)
{
    if ( sem_usrnv != NULL ) {
        xSemaphoreTake(sem_usrnv, portMAX_DELAY);
    }
    return;
}

void drv_usr_nv_unlock(void)
{
    if ( sem_usrnv != NULL ) {
        xSemaphoreGive(sem_usrnv);
    }
    return;
}
#endif // __SUPPORT_USR_NVRAM__
