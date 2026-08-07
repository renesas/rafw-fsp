/**
 ****************************************************************************************
 *
 * @file rm_cli_w_usr_nvram.c
 *
 * @brief NVRAM command functions
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "rm_cli_w_utils.h"
#include "rm_cli_w.h"
#include "rm_cli_w_debug_utils.h"
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "common_def.h"
#include "rm_cli_w_usr_nvram.h"
#if defined ( __SUPPORT_USR_NVRAM_CMD__)
#include "drv_usr_nvram.h"
#include "api_usr_nvram.h"
#include "hal_usr_nvram.h"

#if defined(__SUPPORT_USR_NVRAM_TEST__)
#define MAX_SPECIMEN_NUM 20
struct test_item {
    uint16_t name_val;
};

struct specimen_item {
    uint16_t name_val;
    uint16_t value_len;
    uint8_t value[512];
};

//// For create random port APIs /////////////////////////////////////////
static inline unsigned long cli_get_random_value(void)
{
    unsigned long    rand_val;

    __time64_t uptime = __uptime();

    srand(uptime);

    rand_val = (unsigned long)rand();

    return rand_val;
}

/*
 * Get random value ( 16bits )
 */
unsigned short cli_get_random_value_ushort(void)
{
    unsigned short    result;

    result = (unsigned short)(cli_get_random_value() & 0x0000FFFF);

    return result;
}

static bool cmd_read_test(int argc, const char *argv[])
{
    uint32_t addr;
    uint32_t length;
    uint8_t *data_buf = NULL;
    unsigned long long check_time;

    if (argc < 3) {
        PRINTF("Usage: read [address] [length]\n");
        return false;
    }

    addr = htoi((char *)argv[1]);
    length = atoi(argv[2]);

    if ((addr < SF_USER_NVRAM_START) || (addr > SF_USER_NVRAM_END)) {
        PRINTF("address range [0x%x-0x%x]\n", SF_USER_NVRAM_START, SF_USER_NVRAM_END);
        return false;
    }

    if ((length == 0) || (length > SF_USER_NVRAM_SIZE)) {
        PRINTF("length range [0x%x-0x%x]\n", 0, SF_USER_NVRAM_SIZE);
        return false;
    }
    data_buf = pvPortMalloc(length);
    hal_utl_check_time(1, NULL);

    if (data_buf != NULL) {
        hal_user_nv_sflash_read(addr, data_buf, length);
        check_time = hal_utl_check_time(0, NULL);
        hex_dump_cli((unsigned char *)"",data_buf, length, OUTPUT_HEXA_ASCII);
        PRINTF("\nRead ADDR : 0x%lx, Length %ld, time : %lld.%lld ms\n", addr, length, check_time / 1000, check_time % 1000);
        vPortFree(data_buf);
    } else
        PRINTF("Heap alloc ERR\n");
    return true;
}

static bool cmd_write_test(int argc, const char *argv[])
{
    uint32_t addr;
    uint32_t length;
    uint8_t *data_buf;
    uint8_t wdata;

    if (argc < 3) {
        PRINTF("Usage: write [address] [length] [data] (if data==0, the data will be random\n");
        return false;
    }
    addr = htoi((char *)argv[1]);
    length = atoi(argv[2]);
    if ((addr < SF_USER_NVRAM_START) || (addr > SF_USER_NVRAM_END)) {
        PRINTF("address range [0x%x-0x%x]\n", SF_USER_NVRAM_START, SF_USER_NVRAM_END);
        return false;
    }

    if ((length == 0) || (length > SF_USER_NVRAM_SIZE)) {
        PRINTF("length range [0x%x-0x%x]\n", 0, SF_USER_NVRAM_SIZE);
        return false;
    }
    data_buf = pvPortMalloc(length);
    if (argc == 4)
        wdata = htoi((char *)argv[3]);
    else
        wdata = (unsigned char)rand() % 256;

    memset(data_buf, wdata, length);
    hal_utl_check_time(1, NULL);
    if (data_buf != NULL) {
        hal_user_nv_sflash_write(addr, data_buf, length);
        //user_sflash_write(addr, data_buf, length);
        vPortFree(data_buf);
        hal_utl_check_time(2, NULL);
    } else
        PRINTF("Heap alloc ERR\n");
    return true;
}

static bool cmd_erase_test(int argc, const char *argv[])
{
    uint32_t addr;
    uint32_t length;

    if (argc < 3) {
        PRINTF("Usage: read [address] [length]\n");
        return false;
    }
    addr = htoi((char *)argv[1]);
    length = atoi(argv[2]);
    if ((addr < SF_USER_NVRAM_START) || (addr > SF_USER_NVRAM_END)) {
        PRINTF("address range [0x%x-0x%x]\n", SF_USER_NVRAM_START, SF_USER_NVRAM_END);
        return false;
    }

    if ((length == 0) || (length > SF_USER_NVRAM_SIZE)) {
        PRINTF("length range [0x%x-0x%x]\n", 0, SF_USER_NVRAM_SIZE);
        return false;
    }

    hal_utl_check_time(1, NULL);
    hal_user_nv_sflash_erase(addr, length);
    hal_utl_check_time(2, NULL);
    return true;
}

static bool cmd_read_item_test(int argc, const char *argv[])
{
    uint8_t *name;
    uint8_t *strval;
    uint16_t size;
    uint8_t type;
    int32_t intval;
    int32_t res;

    if (argc < 3) {
        PRINTF("Usage: nvread [name] [type][type, 0:string,1:int,2:bin]\n");
        return false;
    }

    name = (uint8_t *)argv[1];
    type = atoi(argv[2]);
    hal_utl_check_time(1, NULL);
    if (type == 0) {
        strval = (uint8_t *)api_usr_nvram_read_string((const char *)name);
        hal_utl_check_time(2, NULL);
        if (strval != NULL)
            PRINTF("Read [%s]\n", strval);
        else
            PRINTF("Read Fail\n");
    } else if (type == 1) {
        res = api_usr_nvram_read_int((const char *)name, &intval);
        hal_utl_check_time(2, NULL);
        if (res >= 0)
            PRINTF("Read [0x%lx]\n", intval);
        else
            PRINTF("Read Fail\n");
    } else if (type == 2) {
        strval = api_usr_nvram_read_binary((const char *)name, &size);
        hal_utl_check_time(2, NULL);
        if (size > 0) {
            PRINTF("\nRead ");
            for (int i = 0; i < size; i++) {
                PRINTF(" [0x%x]", strval[i]);
            }
            PRINTF("\n");
        } else
            PRINTF("Read Fail\n");
    }
    return true;
}

static bool cmd_add_item_test(int argc, const char *argv[])
{
    uint8_t type;
    int32_t res = 0;

    if (argc < 4) {
        PRINTF("Usage: nvadd [name] [value][type, 0:string,1:int,2:bin]\n");
        return false;
    }

    type = atoi(argv[3]);
    hal_utl_check_time(1, NULL);
    if (type == 0) {
        res = api_usr_nvram_write_string(argv[1], argv[2]);
        hal_utl_check_time(2, NULL);
        PRINTF("[Saved, type %d] name : %s, data : %s\n", type, argv[1], argv[2]);
    } else if (type == 1) {
        res = api_usr_nvram_write_int(argv[1], atoi(argv[2]));
        hal_utl_check_time(2, NULL);
        PRINTF("[Saved, type %d] name : %s, data : %d\n", type, argv[1], atoi(argv[2]));
    }
    else if (type == 2) {
        res = api_usr_nvram_write_binary(argv[1], argv[2], strlen(argv[2]));
        hal_utl_check_time(2, NULL);
        PRINTF("[Saved, type %d] name : %s, data : %s, size : %d\n", type, argv[1], argv[2], strlen(argv[2]));
    }

    if (res)
        PRINTF("[Saved, type %ld] Fail \n", res);
    return true;
}

static bool cmd_remove_item_test(int argc, const char *argv[])
{
    if (argc < 2) {
        PRINTF("Usage: nvremove [name]\n");
        return false;
    }

    hal_utl_check_time(1, NULL);
    api_usr_nvram_delete_item(argv[1]);
    hal_utl_check_time(2, NULL);
    return true;
}

static bool cmd_bank_gc(int argc, const char *argv[])
{
    uint8_t *buf;

    buf = pvPortMalloc(4096);
    hal_utl_check_time(1, NULL);
    if ((argc == 2) && (strncmp("all", argv[1], 3) == 0)) {
        for (int i =1; i <= USR_NV_BANK_MAX_CNT; i++) {
            if (!prc_bank_gc(0, i))
                PRINTF("Bank(%d) GC None\n", i);
        }
    } else if (argc == 2) {
        if (!prc_bank_gc(0, atoi(argv[1])))
            PRINTF("Bank GC None\n");
    } else
        PRINTF("Usage: nvgc [bank id] or [all]\n");
    hal_utl_check_time(2, NULL);
    vPortFree(buf);
    return true;
}

uint8_t chk_name_val(struct test_item *pool, uint16_t item_val, uint16_t cnt)
{
    uint16_t  i;

    for (i = 0; i < cnt; i++) {
        if (pool[i].name_val == item_val)
            return 1;
    }
    return 0;
}

int16_t chk_spec_name_val(struct specimen_item *pool, uint16_t item_val, uint16_t cnt)
{
    uint16_t  i;

    for (i = 0; i < cnt; i++) {
        if (pool[i].name_val == item_val) {
            //PRINTF("chk_spec_name_val checked %d\n", i);
            return i;
        }
    }
    return -1;
}

uint8_t usr_spec_nv_read_test(struct specimen_item *spec_pool, uint16_t spec_val)
{
    uint16_t  i;
    uint8_t *reval;
    uint8_t name[5];

    memset(name, 0x00, 5);
    for (i = 0; i < MAX_SPECIMEN_NUM; i++) {
        snprintf((char *)name, 5, "%04x", spec_pool[i].name_val);
        if (spec_pool[i].value_len == 0) {
            PRINTF("Read Stop by End of Spec\n");
            break;
        }
        reval = (uint8_t *)api_usr_nvram_read_string((const char *)name);
        if (memcmp(reval, spec_pool[i].value, spec_pool[i].value_len) != 0) {
            if (reval ==NULL) {
                PRINTF("Read Data NULL Failed  [%s][%d]\n", name, i);
            } else
                PRINTF("Read Checking [%s][%d, %d] Failed\n", name, spec_pool[i].value_len, i);
            return 1;
        } else
            PRINTF(".");
    }
    PRINTF("Read Test Success\n");
    return 0;
}

uint8_t usr_spec_nv_remove_test(struct specimen_item *spec_pool, uint16_t spec_val)
{
    uint16_t  i;
    char name[5];

    memset(name, 0x00, 5);
    for (i = 0; i < MAX_SPECIMEN_NUM; i++) {
        if (spec_pool[i].value_len == 0) {
            PRINTF("Remove Stop by End of Spec\n");
            break;
        }
        snprintf(name, 5, "%04x", spec_pool[i].name_val);
        if (api_usr_nvram_delete_item(name) < 0) {
            PRINTF("Remove Fail [%s][%d, %d] Failed\n", name, spec_pool[i].value_len, i);
        } else
            PRINTF(".");
    }
    PRINTF("Remove Done\n");

    for (i = 0; i < MAX_SPECIMEN_NUM; i++) {
        if (spec_pool[i].value_len == 0) {
            PRINTF("Remove Check Stop by End of Spec\n");
            break;
        }
        snprintf(name, 5, "%04x", spec_pool[i].name_val);
        if (api_usr_nvram_delete_item(name) == 0) {
            PRINTF("Remove Check Fail [%s][%d, %d] Failed\n", name, spec_pool[i].value_len, i);
        } else
            PRINTF(".");
    }
    PRINTF("Remove Check Test Success\n");
    return 0;
}

uint8_t make_nv_val_and_save(struct test_item *name_pool, struct specimen_item *spec_pool, uint32_t max_item, uint32_t max_size, uint16_t spec_val)
{
    uint16_t val_len;
    uint8_t name[5];
    uint8_t *val, *reval;
    char *readval;
    uint16_t  i, k = 0;
    int32_t res;
    uint16_t spc_count = 0;
    uint32_t size = 0;
    int16_t dup_spec_num;

    PRINTF("[Saving Item]......\n");
    name[4] = 0;
    for (i = 0; i < max_item; i++) {
        snprintf((char *)name, 5, "%04x", name_pool[i].name_val);
        val_len = cli_get_random_value_ushort() & 0x1FF;
        if (val_len == 0)
            val_len++;
        val = pvPortMalloc(val_len + 1);
        if (val == NULL) {
            PRINTF("[%s:%d] val malloc err\n", __func__, __LINE__);
            return 1;
        }
        reval = pvPortMalloc(val_len + 1);
        if (reval == NULL) {
            vPortFree(val);
            PRINTF("[%s:%d] reval malloc err\n", __func__, __LINE__);
            return 1;
        }
        //PRINTF("\n[W Name] %s, %d, count %d\n", name, val_len, i);
        k = 0;
        while (k < val_len) {
            val[k] = cli_get_random_value_ushort() & 0xFF;
            if (val[k] > 0x7E) val[k] -= 0x7e;
            if (val[k] < 0x20) val[k] += 0x20;
            //if (k < 16)
            //    PRINTF("[%x]", val[k]);
            k++;
        }
        //PRINTF("\n");
        val[k] = 0;

        if ((spec_val != 0) && ((i / spec_val) == spc_count)) {
            if ((dup_spec_num = chk_spec_name_val(spec_pool, name_pool[i].name_val, spc_count)) >=0) {
                spec_pool[dup_spec_num].name_val = name_pool[i].name_val;
                memcpy(spec_pool[dup_spec_num].value, val, val_len);
                spec_pool[dup_spec_num].value[val_len] = 0;
                spec_pool[dup_spec_num].value_len = val_len;
                //PRINTF("xx spec dup(%04x) add [0x%x] spc_cnt %d, spc_val %d, count : %d\n"
                //    , dup_spec_num, name_pool[i].name_val, spc_count, spec_val, i);
            } else {
                //PRINTF("spec add [0x%x] spc_cnt %d, spc_val %d, count : %d\n", name_pool[i].name_val, spc_count, spec_val, i);
                spec_pool[spc_count].name_val = name_pool[i].name_val;
                memcpy(spec_pool[spc_count].value, val, val_len);
                spec_pool[spc_count].value[val_len] = 0;
                spec_pool[spc_count].value_len = val_len;
                spc_count++;
            }
        } else if ((dup_spec_num = chk_spec_name_val(spec_pool, name_pool[i].name_val, spc_count)) >=0) {
            spec_pool[dup_spec_num].name_val = name_pool[i].name_val;
            memcpy(spec_pool[dup_spec_num].value, val, val_len);
            spec_pool[dup_spec_num].value[val_len] = 0;
            spec_pool[dup_spec_num].value_len = val_len;
            //PRINTF("x spec dup(%d) add [0x%x] spc_cnt %d, spc_val %d, count : %d, new_name : %04x, new_len : %d\n"
            //    , dup_spec_num, name_pool[i].name_val, spc_count, spec_val, i
            //    , spec_pool[dup_spec_num].name_val, spec_pool[dup_spec_num].value_len);
        }

        res = api_usr_nvram_write_string((const char *)name, (const char *)val);
        if (res) {
            PRINTF("USRNV Add Error\n");
            vPortFree(reval);
            vPortFree(val);
            return 1;
        }

        readval = api_usr_nvram_read_string((const char *)name);
        if ((readval == NULL) || (memcmp(val, readval, val_len) != 0)) {
            if (readval !=NULL) {
                int loop;
                for (loop = 0; loop < val_len; loop++) {
                    if (val[loop] != readval[loop]) {
                        PRINTF("USRNV Read Error %d, 0x%x, 0x%x\n", loop, val[loop], readval[loop]);
                        break;
                    }
                }
            } else
                PRINTF("USRNV Read NULL Error %d, %s %d\n", i, name, val_len);
            vPortFree(reval);
            vPortFree(val);
            return 1;
        }
        size += strlen((char *)readval);
        vPortFree(reval);
        vPortFree(val);
        if (size > max_size) {
            PRINTF("USRNV Saved : %ld\n", size);
            break;
        }
        if ((i % 100) == 0)
            vTaskDelay(1);
    }
    return 0;
}

uint8_t make_nv_item_name_pool(struct test_item *name_pool, uint16_t max_item, uint16_t spec_val)
{
    uint16_t name_val;
    uint16_t  i;

    for (i = 0; i < max_item; i++) {
        name_val = cli_get_random_value_ushort() & 0x00ff;
        //PRINTF("[Item name] %04x, %d\n", name_val, i);
        name_pool[i].name_val = name_val;
    }
    return 0;
}

static bool cmd_usr_nv_test(int argc, const char *argv[])
{
    uint16_t tot_item, specimen_val, test_en = 1;
    uint32_t tot_size = (USR_NV_BANK_SIZE * USR_NV_BANK_MAX_CNT);
    struct test_item *item_pool;
    struct specimen_item *spec_pool;

    if (argc < 2) {
        PRINTF("Usage: nvtest [item_count : max 255] [total size] [test enable]\n");
        return false;
    }

    tot_item = atoi(argv[1]);
    if (argc > 2) {
        tot_size = atoi(argv[2]);
    } else {
        if (tot_size > ((USR_NV_BANK_SIZE * USR_NV_BANK_MAX_CNT) - utl_get_nv_total_used(USR_NV_BANK_MAX_CNT + 1)))
            tot_size = ((USR_NV_BANK_SIZE * USR_NV_BANK_MAX_CNT) - utl_get_nv_total_used(USR_NV_BANK_MAX_CNT + 1));
    }

    if (argc > 3) {
        test_en = htoi((char *)argv[3]);
    }

    item_pool = (struct test_item *)pvPortMalloc(tot_item * sizeof(struct test_item));
    if (item_pool == NULL) {
        PRINTF("Item malloc err\n");
        return false;
    }

    spec_pool = (struct specimen_item *)pvPortMalloc(MAX_SPECIMEN_NUM * sizeof(struct specimen_item));
    if (spec_pool == NULL) {
        vPortFree(item_pool);
        PRINTF("specimen malloc err\n");
        return false;
    }
    hal_utl_check_time(1, NULL);
    memset(item_pool, 0x00, (tot_item * sizeof(struct test_item)));
    memset(spec_pool, 0x00, (MAX_SPECIMEN_NUM * sizeof(struct specimen_item)));
    // make base item
    specimen_val = tot_item / MAX_SPECIMEN_NUM;
    PRINTF("[Making Item]........................................................................\n");
    if (make_nv_item_name_pool(item_pool, tot_item, specimen_val)) {
        vPortFree(item_pool);
        vPortFree(spec_pool);
        hal_utl_check_time(2, NULL);
        PRINTF("Making name fail\n");
        return false;
    }

    PRINTF("[Saving Item, %d, %ld]........................................................................\n", tot_item, tot_size);
    if (make_nv_val_and_save(item_pool, spec_pool, tot_item, tot_size, specimen_val)) {
        vPortFree(item_pool);
        vPortFree(spec_pool);
        hal_utl_check_time(2, NULL);
        PRINTF("Making and Save NV fail\n");
        return false;
    }

    api_usr_nvram_bank_status(41);

    if (test_en) {
        PRINTF("[Read Speciment Test]........................................................................\n");
        if (usr_spec_nv_read_test(spec_pool, specimen_val)) {
            vPortFree(item_pool);
            vPortFree(spec_pool);
            hal_utl_check_time(2, NULL);
            PRINTF("NV Read Test fail\n");
            return false;
        }

        PRINTF("[Remove Test]........................................................................\n");
        if (usr_spec_nv_remove_test(spec_pool, specimen_val)) {
            vPortFree(item_pool);
            vPortFree(spec_pool);
            hal_utl_check_time(2, NULL);
            PRINTF("NV Read Test fail\n");
            return false;
        }
    }
    hal_utl_check_time(2, NULL);
    vPortFree(item_pool);
    vPortFree(spec_pool);
    return true;
}

static bool cmd_usr_nv_init(int argc, const char *argv[])
{
    if (argc < 2) {
        PRINTF("Usage: nvinit [bank, 0 ~ max bank : specified bank reset, over max : reset all]\n");
    }
    hal_utl_check_time(1, NULL);
    if ((argc == 2) && (strncmp("all", argv[1], 3) == 0)) {
        api_usr_nvram_bank_reset(USR_NV_BANK_MAX_CNT + 1);
    } else
        api_usr_nvram_bank_reset(atoi(argv[1]));
    hal_utl_check_time(2, NULL);
    PRINTF("NV Init %s %d\n", (strncmp("all", argv[1], 3) == 0)?"All":"Num", (strncmp("all", argv[1], 3) == 0)?40:atoi(argv[1]));
    return true;
}

static bool cmd_usr_index_status(int argc, const char *argv[])
{
    extern struct usr_nvitem_index_struct g_index_manager[];
    struct usr_nvitem_index_struct *i_ptr = g_index_manager;
    int i, count = 0;

    PRINTF("==== Index Items =====\n");
    for (i = 0; i < INDEX_MAX_NUM; i++) {
        if (i_ptr[i].en) {
            PRINTF("[%03d] %s(%02d:0x%04x))\n", i, i_ptr[i].item_name, i_ptr[i].bank, i_ptr[i].offset);
            count++;
        }
    }
    PRINTF("=== Index Items end (%d) ===\n", count);
    return true;
}
#endif //__SUPPORT_USR_NVRAM_TEST__

static bool cmd_bank_status(int argc, const char *argv[])
{
    hal_utl_check_time(1, NULL);
    if (argc == 1)
        api_usr_nvram_bank_status(0);
    else if ((argc == 2) && (strncmp("all", argv[1], 3) == 0))
        api_usr_nvram_bank_status(USR_NV_BANK_MAX_CNT + 1);
    else if (argc == 2)
        api_usr_nvram_bank_status(atoi(argv[1]));
    else
        PRINTF("Usage: nvstatus [all(over 40) : detail, 1~40 : each bank]\n");
    hal_utl_check_time(2, NULL);
    return true;
}

static const debug_handler_t usr_nvram_handlers[] = {
#if defined(__SUPPORT_USR_NVRAM_TEST__)
    { "read", "read [addr] [length]", cmd_read_test},
    { "write", "write [addr] [length] [data]", cmd_write_test},
    { "erase", "erase [addr] [length]", cmd_erase_test},
    { "nvadd", "nvadd [name] [value] [type]", cmd_add_item_test},
    { "nvread", "nvread [name] [type]", cmd_read_item_test},
    { "nvremove", "nvremove [name]", cmd_remove_item_test},
    { "nvgc", "nvgc [all, 1 ~ max bank: bank number]", cmd_bank_gc},
    { "nvtest", "nvtest [max item] [max size]", cmd_usr_nv_test},
    { "nvinit", "nvinit [bank, 1 ~ max bank : specified bank reset, all : reset all]", cmd_usr_nv_init},
    { "index", "index", cmd_usr_index_status},
#endif
    { "nvstatus", "nvstatus [all, 1 ~ max bank: bank number]", cmd_bank_status},
    { NULL },
};

bool usr_nv_command(int argc, const char *argv[], void *user_data)
{
    return debug_handle_message(argc, argv, usr_nvram_handlers);
}
#endif  // __SUPPORT_USR_NVRAM_CMD__
