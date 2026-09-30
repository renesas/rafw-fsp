/**
****************************************************************************************
*
* @file api_usr_nvram.c
*
* @brief User NVRAM API
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

#include "FreeRTOS.h"
#include "rm_wifi.h"
#include "drv_usr_nvram.h"

#if defined(__SUPPORT_USR_NVRAM__)
#include <stdlib.h>
#include <string.h>

void api_usr_nvram_init(void)
{
    drv_usr_nv_lock();
    drv_usr_nv_init();
    drv_usr_nv_sort();
    drv_usr_nv_unlock();
    return;
}

void api_usr_nvram_bank_status(uint8_t all)
{
    drv_usr_nv_lock();
    utl_show_bank_status(all);
    drv_usr_nv_unlock();
    return;
}

void api_usr_nvram_bank_reset(uint16_t bank)
{
    drv_usr_nv_lock();
    utl_bank_reset(bank);
    drv_usr_nv_unlock();
    return;
}

int32_t api_usr_nvram_read_int(const char *name, int32_t *val)
{
    uint8_t *valstr;
    uint16_t size;

    drv_usr_nv_lock();
    drv_nvitem_read(name, &valstr, &size);
    drv_usr_nv_unlock();

    if ((valstr == NULL) || (size ==0)) {
        *val = -1;
        return -1;
    }

    *val = atoi((char *)valstr);
    return 0;
}

char *api_usr_nvram_read_string(const char *name)
{
    uint8_t *valstr = NULL;
    uint16_t size;

    drv_usr_nv_lock();
    drv_nvitem_read(name, &valstr, &size);
    drv_usr_nv_unlock();

    return (char *)valstr;
}

uint8_t *api_usr_nvram_read_binary(const char *name, uint16_t *size)
{
    uint8_t *valstr = NULL;

    drv_usr_nv_lock();
    drv_nvitem_read(name, &valstr, size);
    drv_usr_nv_unlock();

    return valstr;
}

int32_t api_usr_nvram_write_int(const char *name, int32_t val)
{
    char valstr[11];

    drv_usr_nv_lock();
    memset(valstr, 0, 11);
    sprintf(valstr, "%ld", val);

    if (drv_nvitem_add(name, (uint8_t *)valstr, strlen(valstr))) {
        PRINTF("[%s] NVRAM Write: Failed(name [%s] : val [%ld])\n", __func__, name, val);
        drv_usr_nv_unlock();
        return -2;
    }
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_write_string(const char *name, const char *val)
{
    if (strlen(val) == 0)
        return -2;
    drv_usr_nv_lock();
    if (drv_nvitem_add(name, (uint8_t *)val, strlen(val))) {
        PRINTF("[%s] NVRAM Write: Failed(name [%s] : len [%d])\n", __func__, name, strlen(val));
        drv_usr_nv_unlock();
        return -2;
    }
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_write_binary(const char *name, const char *val, uint16_t size)
{
    drv_usr_nv_lock();
    if (drv_nvitem_add(name, (uint8_t *)val, size)) {
        PRINTF("[%s] NVRAM Write: Failed(name [%s] : len [%d])\n", __func__, name, size);
        drv_usr_nv_unlock();
        return -2;
    }
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_delete_item(const char *name)
{
    drv_usr_nv_lock();
    if (drv_nvitem_del(name)) {
        PRINTF("[%s] NVRAM Erase: Failed(name [%s]\n", __func__, name);
        drv_usr_nv_unlock();
        return -2;
    }
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_write_int_tmp(const char *name, int32_t val)
{
    drv_usr_nv_lock();
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_write_string_tmp(const char *name, const char *val)
{
    drv_usr_nv_lock();
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_write_binary_tmp(const char *name, const char *val, uint16_t size)
{
    drv_usr_nv_lock();
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_delete_item_tmp(const char *name)
{
    drv_usr_nv_lock();
    drv_usr_nv_unlock();
    return 0;
}

int32_t api_usr_nvram_save_tmp(void)
{
    drv_usr_nv_lock();
    drv_usr_nv_unlock();
    return 0;
}
#endif // __SUPPORT_USR_NVRAM__
