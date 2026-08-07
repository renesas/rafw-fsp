/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR

#if CFG_WIFI
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_pmgr_w_instance.h"
 #include "fsp_common_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static bool    user_rtm_pool_init_done    = pdFALSE;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
extern unsigned char dpm_dbg_cmd_flag;
extern dpm_user_rtm_pool *user_rtm_pool;

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/



/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
extern int  get_boot_mode(void);

static fsp_err_t RM_PMGR_W_user_rtm_data_clear(void)
{
    memset((void *)RTM_USER_DATA_PTR, 0x00, USER_DATA_ALLOC_SZ);

    return FSP_SUCCESS;
}


static fsp_err_t RM_PMGR_W_user_rtm_hdr_clear(void)
{
    memset((void *)user_rtm_pool, 0x00, sizeof(dpm_user_rtm_pool));

    return FSP_SUCCESS;
}

unsigned int RM_PMGR_W_user_rtm_pool_create(void)
{
    if (!user_rtm_pool_init_done) {
        if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE && !get_boot_mode()) {

            //to clear user rtm pool
            RM_PMGR_W_user_rtm_hdr_clear();

            //to clear user rtm data
			RM_PMGR_W_user_rtm_data_clear();

            RM_PMGR_W_rtm_heap_init(RTM_USER_DATA_PTR, USER_DATA_ALLOC_SZ);

            user_rtm_pool->free_ptr = RM_PMGR_W_rtm_heap_free_ptr_get();

            user_rtm_pool->first_user_addr = NULL;
        } else {
            RM_PMGR_W_rtm_heap_recover(RTM_USER_DATA_PTR, USER_DATA_ALLOC_SZ,
                    user_rtm_pool->free_ptr);
        }

        // Set flag to mark User RTM Initialize done ...
        user_rtm_pool_init_done = pdTRUE;
    }

    return pdTRUE;
}

fsp_err_t RM_PMGR_W_user_rtm_pool_delete(void)
{
    //to clear user rtm pool
    RM_PMGR_W_user_rtm_hdr_clear();

    //to clear user rtm data
    RM_PMGR_W_user_rtm_data_clear();

    return FSP_SUCCESS;
}

unsigned int RM_PMGR_W_user_rtm_pool_alloc(char *name,
                                   void **memory_ptr,
                                   unsigned long memory_size,
                                   unsigned long wait_option)
{
    RA6W1_UNUSED_ARG(wait_option);

    dpm_user_rtm *user_memory = NULL;

    // to check parameters
    if (   (name == NULL)
        || (strlen(name) >= REG_NAME_DPM_MAX_LEN)
        || (memory_size == 0)) {
        return ER_INVALID_PARAMETERS;
    }

    // to check duplicated name
    if (dpm_user_rtm_search(name) != NULL) {
        if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= 1 /* MSG_ERROR */) {
            printf("[%s] Already registered user rtm(%s)\n", __func__, name);
        }

        return ER_DUPLICATED_ENTRY;
    }

    // to allocate memory
    user_memory = (dpm_user_rtm *)RM_PMGR_W_rtm_heap_calloc((memory_size + sizeof(dpm_user_rtm)), sizeof(unsigned char));
    if (user_memory == NULL) {
       if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= 1 /* MSG_ERROR */) {
            printf("[%s] Failed to allocate memory(size:%ld)\n", __func__, memory_size);
        }

        return ER_NO_MEMORY;
    }

    // update lfree. It has to be called after allocate & free
    user_rtm_pool->free_ptr = RM_PMGR_W_rtm_heap_free_ptr_get();

    // to set user rtm
    bsp_safe_strcpy(user_memory->name, name, sizeof(user_memory->name));
    user_memory->size = memory_size;
    user_memory->start_user_addr = (unsigned char *)(user_memory) + sizeof(dpm_user_rtm);
    user_memory->next_user_addr = NULL;

    // to add user rtm into chain
    dpm_user_rtm_add(user_memory);

    // to set user memory point
    *memory_ptr = user_memory->start_user_addr;

    if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= 5 /* MSG_EXCESSIVE */) {
        printf("[%s] Allocated memory information\n", __func__);
        printf("\tname:%s\n"
            "\tsize:%d\n"
            "\tstart address:%p\n"
            "\tuser address:%p\n",
            user_memory->name,
            user_memory->size,
            user_memory,
            user_memory->start_user_addr);
    }

    return ER_SUCCESS;
}

unsigned int RM_PMGR_W_user_rtm_free(char *name)
{
    dpm_user_rtm *cur_mem = NULL;

    if ((name == NULL) || (strlen(name) >= REG_NAME_DPM_MAX_LEN)) {
        return  ER_INVALID_PARAMETERS;
    }

    cur_mem = dpm_user_rtm_remove(name);
    if (cur_mem != NULL) {
        RM_PMGR_W_rtm_heap_free(cur_mem);

        //update lfree. It has to be called after allocate & free
        user_rtm_pool->free_ptr = RM_PMGR_W_rtm_heap_free_ptr_get();

        return 0;
    }

    if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= 1 /* MSG_ERROR */) {
        printf("[%s] Not found user data(%s)\n", __func__, name);
    }

    return ER_NOT_FOUND;
}

int RM_PMGR_W_user_rtm_is_init_done(void)
{
    return user_rtm_pool_init_done;
}

unsigned int RM_PMGR_W_user_rtm_get(char *name, unsigned char **data)
{
    dpm_user_rtm *cur = NULL;

    if ((name == NULL) || (strlen(name) >= REG_NAME_DPM_MAX_LEN)) {
        return  0;
    }

    cur = dpm_user_rtm_search(name);
    if (cur != NULL) {
        *data = cur->start_user_addr;
        return cur->size;
    }

    return 0;
}

fsp_err_t RM_PMGR_W_rtm_static_set(int key, long value_l, unsigned long long value_ull)
{
    switch (key) {
        case RTM_STATIC_KEY_SNTP_TIMEOUT:
            RTM_FLAG_PTR->dpm_sntp_timeout = value_l;
            break;

        case RTM_STATIC_KEY_SNTP_PERIOD:
            RTM_FLAG_PTR->dpm_sntp_period = value_l;
            break;
        
        case RTM_STATIC_KEY_DPM_KEEPALIVE:
            if (!RM_PMGR_W_rtm_exist()) {
                /* Unsupport RTM */
                return FSP_ERR_UNSUPPORTED;
            }
            
            RTM_FLAG_PTR->dpm_keepalive_time_msec = (int)value_l;
            break;

        case RTM_STATIC_KEY_DPM_WAKEUP_FLAG:
            if (!RM_PMGR_W_rtm_exist()) {
                /* Unsupport RTM */
                return FSP_ERR_UNSUPPORTED;
            }
            
            RTM_FLAG_PTR->dpm_wakeup = (int)value_l;
            break;

        case RTM_STATIC_KEY_TIMEZONE:
            RTM_FLAG_PTR->time_params.__timezone = value_l;
            break;

        case RTM_STATIC_KEY_SYS_TIME_OFFSET:
            RTM_FLAG_PTR->time_params.systime_offset = value_ull;
            break;

        case RTM_STATIC_KEY_SNTP_USE:
            RTM_FLAG_PTR->dpm_sntp_use = (unsigned char)value_ull;
            break;
            
        case RTM_STATIC_KEY_HTTP_SVR_ENABLE_FLAG:
            RTM_FLAG_PTR->dpm_http_svr_enable = (unsigned char)value_ull;
            break;
            
        case RTM_STATIC_KEY_RTC_OLD_TIME:
            RTM_FLAG_PTR->time_params.rtc_oldtime = value_ull;
            break;
     
        default:
            return FSP_ERR_NOT_FOUND;
            
    }

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_rtm_static_get(int key, long * p_value_l, unsigned long long * p_value_ull, void ** pp_ptr)
{
    if (p_value_l == NULL && p_value_ull == NULL && pp_ptr == NULL) {
        return FSP_ERR_INVALID_POINTER;
    }

    switch (key) {
        case RTM_STATIC_KEY_SNTP_TIMEOUT:
            *p_value_ull = RTM_FLAG_PTR->dpm_sntp_timeout;
            break;

        case RTM_STATIC_KEY_SNTP_PERIOD:
            *p_value_ull = RTM_FLAG_PTR->dpm_sntp_period;
            break;

        case RTM_STATIC_KEY_SNTP_USE:
            *p_value_ull = (UINT)RTM_FLAG_PTR->dpm_sntp_use;
            break;

        case RTM_STATIC_KEY_TIMEZONE:
            *p_value_l = RTM_FLAG_PTR->time_params.__timezone;
            break;

        case RTM_STATIC_KEY_RTC_OLD_TIME:
            *p_value_ull = (unsigned long long)(RTM_FLAG_PTR->time_params.rtc_oldtime);
            break;

        case RTM_STATIC_KEY_SYS_TIME_OFFSET:
            *p_value_ull = (unsigned long long)RTM_FLAG_PTR->time_params.systime_offset;
            break;

        case RTM_STATIC_KEY_SUPPL_NET_INFO_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_SUPP_NET_INFO_PTR;
            break;

        case RTM_STATIC_KEY_SUPPL_KEY_INFO_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_SUPP_KEY_INFO_PTR;
            break;

        case RTM_STATIC_KEY_SUPPL_IP_INFO_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_SUPP_IP_INFO_PTR;
            break;

        case RTM_STATIC_KEY_SUPPL_CONN_INFO_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_SUPP_CONN_INFO_PTR;
            break;

        case RTM_STATIC_KEY_RTC_TIMER_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_RTC_TIMER_PTR;
            break;

        case RTM_STATIC_KEY_ARP_INFO_PTR:
#if defined (__SUPPORT_IPV4__)
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_ARP_PTR;
#else
            *pp_ptr = NULL;
#endif /* __SUPPORT_IPV4__ */
            break;

        case RTM_STATIC_KEY_IPV6_INFO_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_SUPP_IPV6_INFO_PTR;
            break;

        case RTM_STATIC_KEY_DNS_CACHE_PTR:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = (void *)RTM_DNS_PTR;
            break;

        case RTM_STATIC_KEY_COUNTRY_CODE:
            if (RM_PMGR_W_rtm_exist() == pdFAIL) {
                /* Unsupport RTM */
                *pp_ptr = NULL;
            }

            *pp_ptr = RTM_SUPP_NET_INFO_PTR->country;
            break;

        case RTM_STATIC_KEY_HTTP_SVR_ENABLE_FLAG:
            *p_value_ull = (UINT)RTM_FLAG_PTR->dpm_http_svr_enable;
            break;

        case RTM_STATIC_KEY_DPM_KEEPALIVE:
            *p_value_ull = (unsigned int)RTM_FLAG_PTR->dpm_keepalive_time_msec;
            break;

        default:
            return FSP_ERR_NOT_FOUND;
    }

    return FSP_SUCCESS;
}

#endif /* CFG_WIFI */

#endif /* CFG_PMGR */
