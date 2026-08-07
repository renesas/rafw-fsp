/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_PMGR_W_RTM_INTERNAL_H
#define RM_PMGR_W_RTM_INTERNAL_H

#if CFG_WIFI
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "common_def.h"

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/


/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
#define USER_RTM_POOL_NAME	"user_rtm_pool"
#define REG_NAME_DPM_MAX_LEN    20

typedef struct _dpm_user_rtm {
	char		name[REG_NAME_DPM_MAX_LEN];
	unsigned char	*start_user_addr;
	unsigned int	size;

	struct _dpm_user_rtm *next_user_addr;
} dpm_user_rtm;

typedef struct _dpm_user_rtm_pool {
	void		*free_ptr;
	dpm_user_rtm	*first_user_addr;
} dpm_user_rtm_pool;

typedef enum e_rtm_static_key
{
    RTM_STATIC_KEY_SNTP_TIMEOUT,
    RTM_STATIC_KEY_SNTP_PERIOD,
    RTM_STATIC_KEY_SNTP_USE,
    
    RTM_STATIC_KEY_TIMEZONE,
    RTM_STATIC_KEY_SYS_TIME_OFFSET,
    
    RTM_STATIC_KEY_DPM_KEEPALIVE,
    RTM_STATIC_KEY_DPM_WAKEUP_FLAG,
    RTM_STATIC_KEY_DPM_DBG_LVL,
    
    RTM_STATIC_KEY_SUPPL_NET_INFO_PTR,
    RTM_STATIC_KEY_SUPPL_KEY_INFO_PTR,
    RTM_STATIC_KEY_SUPPL_IP_INFO_PTR,
    RTM_STATIC_KEY_SUPPL_CONN_INFO_PTR,

    RTM_STATIC_KEY_ARP_INFO_PTR,
    RTM_STATIC_KEY_IPV6_INFO_PTR,
    RTM_STATIC_KEY_DNS_CACHE_PTR,
    
    RTM_STATIC_KEY_RTC_TIMER_PTR,
    RTM_STATIC_KEY_RTC_OLD_TIME,

    RTM_STATIC_KEY_COUNTRY_CODE,

    RTM_STATIC_KEY_HTTP_SVR_ENABLE_FLAG,
    
    RTM_STATIC_KEY_SNTP_NUM
} rtm_static_key_t;

#undef RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
#undef RA6WX_DPM_RTM_MEM_SANITY_CHECK

typedef unsigned short ra6w1_dpm_rtm_mem_size_t;

/***********************************************************************************************************************
 * Functions
 *********************************************************************************************************************/

#if RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK
void RM_PMGR_W_rtm_heap_init_raw_with_overflow_check(void *p, size_t size);
void RM_PMGR_W_rtm_heap_overflow_check(void *p, size_t size, const char *descr1, const char *descr2);
#endif /* RA6WX_DPM_RTM_MEM_OVERFLOW_CHECK */

void RM_PMGR_W_rtm_dpm_area_init(unsigned int wakeup_src);

bool RM_PMGR_W_rtm_exist(void);

void  RM_PMGR_W_rtm_heap_init(void *mem_ptr, size_t mem_len);

void  RM_PMGR_W_rtm_heap_recover(void *mem_ptr, size_t mem_len, void *lfree_ptr);

void *RM_PMGR_W_rtm_heap_free_ptr_get(void);

void *RM_PMGR_W_rtm_heap_trim(void *rmem, ra6w1_dpm_rtm_mem_size_t new_size);

void *RM_PMGR_W_rtm_heap_malloc(ra6w1_dpm_rtm_mem_size_t size_in);

void *RM_PMGR_W_rtm_heap_calloc(ra6w1_dpm_rtm_mem_size_t count, ra6w1_dpm_rtm_mem_size_t size);

void  RM_PMGR_W_rtm_heap_free(void *rmem);

void  RM_PMGR_W_rtm_heap_status_print(void);

dpm_user_rtm *dpm_user_rtm_search(char *name);

void dpm_user_rtm_add(dpm_user_rtm *data);

dpm_user_rtm *dpm_user_rtm_remove(char *name);

void dpm_user_rtm_print(void);

#endif /*CFG_WIFI*/

#endif /*RM_PMGR_W_RTM_INTERNAL_H*/
