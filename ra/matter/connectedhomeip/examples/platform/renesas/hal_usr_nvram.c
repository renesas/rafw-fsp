/**
****************************************************************************************
*
* @file hal_usr_nvram.c
*
* @brief user NVRAM hardware adaptation layer
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
#include <stdlib.h>
#include "timers.h"

void (* usr_nv_gc_proc_cb)(void) = NULL;

unsigned long long hal_utl_get_cur_time(void)
{
    return (R_BSP_SystemRtcCountGet() * 33);
}

unsigned long long hal_utl_check_time(int reset, const char *msg)
{
    static unsigned long long time_old = 0, check_time = 0;
    if (reset == 1) {
#if defined (USRNV_DEBUG_T)
        if (msg != NULL)
            PRINTF("%s\n", msg);
#endif
        time_old = hal_utl_get_cur_time();
        return time_old;
    } else {
        check_time = (hal_utl_get_cur_time() - time_old);
#if defined (USRNV_DEBUG_T)
        if (reset == 2)
            PRINTF("\n %s time : %lld.%03lld ms\n", (msg)?msg:"", check_time / 1000, check_time % 1000);
#endif
        time_old = 0;
    }
    return check_time;
}

static void hal_utl_gc_timer_cb(TimerHandle_t xTimer)
{
    RA6W1_UNUSED_ARG(xTimer);

    if (usr_nv_gc_proc_cb != NULL)
        usr_nv_gc_proc_cb();
}

uint32_t *hal_os_create_timer(char *name, uint16_t time_ms, void *callback)
{
    TimerHandle_t gc_timer = NULL;
    gc_timer = xTimerCreate(name, pdMS_TO_TICKS(time_ms), pdTRUE, (void *)0, hal_utl_gc_timer_cb);

    if (gc_timer == NULL) {
        PRINTF(" [%s] timer create error \n", __func__);
        return NULL;
    }

    if (callback != NULL)
        usr_nv_gc_proc_cb = callback;

    if ( xTimerStart(gc_timer, 0) != pdPASS ) {
        /* The timer could not be set into the Active
        state. */
        PRINTF(" [%s] The timer could not be set into the Active state.  0x%x\n", __func__, (unsigned int)gc_timer);
    }

    return (uint32_t *)gc_timer;
}

uint32_t hal_user_nv_sflash_read(uint32_t sflash_addr, void *rd_buf, uint32_t rd_size)
{
    memcpy(rd_buf, (void *)(sflash_addr | 0x2A000000U /* OSPI_B_AUTOMODE_BASE_ADD */), rd_size);
    return TRUE;
}

uint32_t hal_user_nv_sflash_write(uint32_t sflash_addr, uint8_t *wr_buf, uint32_t wr_size)
{
    spi_flash_instance_t const * p_flash_instance = gp_matter_app_instance->p_cfg->p_flash_instance;

    if (p_flash_instance->p_api->open(p_flash_instance->p_ctrl, p_flash_instance->p_cfg) == FSP_SUCCESS) {
        if (p_flash_instance->p_api->write(p_flash_instance->p_ctrl, wr_buf, (uint8_t *)sflash_addr, wr_size) == FSP_SUCCESS)
            return TRUE;
        else
            return FALSE;
    } else
        return FALSE;
}

uint32_t hal_user_nv_sflash_erase(uint32_t sflash_addr, uint32_t er_size)
{
    spi_flash_instance_t const * p_flash_instance = gp_matter_app_instance->p_cfg->p_flash_instance;

    if (p_flash_instance->p_api->open(p_flash_instance->p_ctrl, p_flash_instance->p_cfg) == FSP_SUCCESS) {
        if (p_flash_instance->p_api->erase(p_flash_instance->p_ctrl, (uint8_t *)sflash_addr, er_size) == FSP_SUCCESS)
            return TRUE;
        else
            return FALSE;
    } else
        return FALSE;
}
#endif // __SUPPORT_USR_NVRAM__
