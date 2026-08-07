/**
****************************************************************************************
*
* @file hal_usr_nvram.h
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
#include "rm_matter_wifi_common.h"
#include "bsp_sflash_map_ra6w1.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "common_def.h"

#undef USRNV_DEBUG_T

#define SF_USER_NVRAM_SIZE          0x28000  // 160 KB, 4KB * 40
#define SF_USER_NVRAM_START     ( SF_USER_AREA + SF_USER_AREA_SIZE - SF_USER_NVRAM_SIZE)
#define SF_USER_NVRAM_END       ( SF_USER_NVRAM_START + SF_USER_NVRAM_SIZE - 1 )

#define GC_USED_PERCENT 80
#define GC_URGENT_PERCENT 50
#define GC_GC_PERCENT 30
#define GC_ACT_TIME 50
#define INDEX_MAX_NUM 512

#if defined(__SUPPORT_USR_NVRAM__)
unsigned long long hal_utl_get_cur_time(void);
unsigned long long hal_utl_check_time(int reset, const char *msg);

uint32_t hal_user_nv_sflash_read(uint32_t sflash_addr, VOID *rd_buf, uint32_t rd_size);
uint32_t hal_user_nv_sflash_write(uint32_t sflash_addr, UCHAR *wr_buf, uint32_t wr_size);
uint32_t hal_user_nv_sflash_erase(uint32_t sflash_addr, uint32_t er_size);
uint32_t *hal_os_create_timer(char *name, uint16_t time_ms, void *callback);
#endif // __SUPPORT_USR_NVRAM__
