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
#include "r_pm_if.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
extern TimerHandle_t        dpm_trigger_timer;
extern bool    dpm_daemon_start_flag;
extern bsp_wakeup_source_t source;
extern int    dpm_pdown_state;
extern pmgr_w_info_dpm_sleep_t    pmgr_w_info_dpm_sleep_list[];
extern SemaphoreHandle_t    dpm_timer_mutex;

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/



/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
extern void do_printf(int do_print, const char *fmt, ...);

extern int do_autoarp_check(void);

#if CFG_PMGR

int RM_PMGR_W_dpm_is_wakeup(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL || get_run_mode() != WIFI_DEVICE_MODE_EXT_STATION) {
        /* Unsupport RTM */
        return pdFALSE;
    }

    if (   (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
        && RTM_FLAG_PTR->dpm_mode
        && RTM_FLAG_PTR->dpm_wakeup) {
        return pdTRUE;
    }

    return pdFALSE;
}

int RM_PMGR_W_dpm_wakeup_src_get(void)
{
	return (int)source;
}

int RM_PMGR_W_dpm_wakeup_type_get(int do_print)
{
    //extern void run_keepalive_callback();
    unsigned long retwakeup = DPM_UNKNOWN_WAKEUP;
    unsigned long dpm_status = RM_WIFI_dpm_ptim_event_get();
#if 0	// TO_DO
    uint8_t inst_nbr = dpm_get_mm_set_beacon_int_req(dpmp)->inst_nbr;
    struct vif_info_tag *vif_entry_boot = &vif_info_tab[inst_nbr];
#endif

#if 0	// TO_DO
    /* PS Mode off */
    if (ps_env.fci_dpsm_use_pspoll && CHK_ST_STATUS(DPM_ST_UC_MORE)) {
        nxmac_pwr_mgt_setf(1);
    }
	else {
        ps_set_mode(PS_MODE_OFF, TASK_API);
    }
#else
	//set_ps_mode_off_for_dpm();		// TO_DO
#endif
	
    //DPM_DEBUG_PRINT(3, "DPM [%s] tim status %d \n", __FUNCTION__, dpm_get_env_st_status(dpmp));
    //PRINTF(">>> TIM STATUS: 0x%08x\n", dpm_get_st_status(dpmp));
    do_printf(do_print, ">>> TIM STATUS: 0x%08lx\n", dpm_status);

   /* TIM ERR wakeup is #1 */

    /* Deauth case wakeup is #2 */
    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_DEAUTH)) {
        do_printf(do_print, ">>> TIM : Deauth\n");
        /** In case of BCN Changed, there is just two case
         * 	SSID & DS Parameter set IE , We have to reconnect
         */
        retwakeup = DPM_DEAUTH_WAKEUP;
        goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    if ((CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_NOBCN)) ||  (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_NOUC))) {
        do_printf(do_print, ">>> TIM : No BCN\n");
        retwakeup = DPM_NOACK_WAKEUP;
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_NOACK)) {	// DPM_KEEP_ALIVE_NO_ACK
	/* In case of NO-ACK Wakeup, one more PS checking with Null Active frame  */
        do_printf(do_print, ">>> TIM : NO_ACK\n");
        retwakeup = DPM_NOACK_WAKEUP;
    }
 
    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_TCP_KA_TIMEOUT)) {
        do_printf(do_print, ">>> TIM : TCP_KA_TIMEOUT\n");
        retwakeup = DPM_TCP_KA_TIMEOUT_WAKEUP;
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_INTERNAL_PTIM_ISSUE)) {
        do_printf(do_print, ">>> TIM : INTERNAL PTIM ISSUE\n");
        retwakeup = DPM_INTERNAL_PTIM_ISSUE_WAKEUP;
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_UPLOAD)) {
        do_printf(do_print, ">>> TIM : UC\n");

        retwakeup = DPM_PACKET_WAKEUP;
        goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_BUFP_DONE)) { /* BUFP DONE */
		// UC signal count
	    do_printf(do_print, ">>> TIM : DDPS-BUFP\n");

		retwakeup = DPM_DDPS_BUFP_WAKEUP	;
		goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    /* Deauth case wakeup is #2 */
    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_BCN_CHANGED)) {
        do_printf(do_print, ">>> TIM : BCN Chg\n");
        /** In case of BCN Changed, there is just two case
         * 	SSID & DS Parameter set IE , We have to reconnect
         */
        retwakeup = DPM_DEAUTH_WAKEUP;
        goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    /* RTC Wakeup is #3 */
    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_FROM_FAST)) {	// DPM FROM FAST
        do_printf(do_print, ">>> TIM : FAST\n");
        retwakeup = DPM_RTCTIME_WAKEUP;
        goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    /* RTC Wakeup is #3 */
    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_FROM_FULL)) {	// DPM FROM FAST
        do_printf(do_print, ">>> TIM : FULL\n");
        retwakeup = DPM_RTCTIME_WAKEUP;
        goto TIM_STATUS_END;	//Direct return w/o other checking
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_WEAK_SIGNAL_0)) {
        do_printf(do_print, ">>> TIM : WEAK_SIG_0\n");
        //timp_display_interface_data(TIMP_WEAK_SIG_0);
        retwakeup = DPM_RTCTIME_WAKEUP;
        goto TIM_STATUS_END;		//Direct return w/o other checking
    }

    if (CHK_PTIM_STATUS(dpm_status, DPM_ST_WEAK_SIGNAL_1)) {
        do_printf(do_print, ">>> TIM : WEAK_SIG_1\n");
        //timp_display_interface_data(TIMP_WEAK_SIG_1);
        retwakeup = DPM_RTCTIME_WAKEUP;
        goto TIM_STATUS_END;		//Direct return w/o other checking
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_TIMP_UPDATE_0)) {
        do_printf(do_print, ">>> TIM : DPM_TIMP_UPDATE_0\n");
        //timp_display_interface_data(TIMP_UPDATED_0);
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_TIMP_UPDATE_1)) {
        do_printf(do_print, ">>> TIM : DPM_TIMP_UPDATE_1\n");
        //timp_display_interface_data(TIMP_UPDATED_1);
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_FB0)) {	// USER0
        do_printf(do_print, ">>> TIM : DPM_USER_0\n");
        /* don't need it */
        retwakeup = DPM_USER_WAKEUP;
    }

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_FB1)) {	// USER1
        do_printf(do_print, ">>> TIM : DPM_USER_1\n");
        //run_keepalive_callback();		// TO_DO
        retwakeup = DPM_USER_WAKEUP;
    }

TIM_STATUS_END:

#if 0
    /** BCN Checking Interval for DPM **/
#ifdef CUSTOMER_VENSTAR_SS
    if(retwakeup == DPM_RTCTIME_WAKEUP)
        ps_env.dpm_bcn_chk_interval = 10;	//MM_RX_BCN_CNT/2;	//10 as 1sec
    else
        ps_env.dpm_bcn_chk_interval = 20;	//MM_RX_BCN_CNT;
#else
    //ps_env.dpm_bcn_chk_interval = 20;	//MM_RX_BCN_CNT;		// TO_DO
#endif
#else
	//bcn_check_interval_for_dpm();		// TO_DO
#endif

#ifdef __DPM_SLEEPED_TIME_PRINT__
    dpm_sleeped_time_calc(1);
#endif /* __DPM_SLEEPED_TIME_PRINT__ */

    return (int) retwakeup;
}
#else
bool RM_PMGR_W_IsSleep3Wakeup(void)
{
    return false;
}
#endif /* CFG_PMGR */

int RM_PMGR_W_dpm_wakeup_done(char *dpm_name)
{
    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;
    int     i = 0;

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return DPM_SET_OK;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];
        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            pmgr_w_dpm_list->init_done = 1;
            break;
        }

        i++;
        if (i > MAX_DPM_INFO_ARRAY) {
            RM_PMGR_W_dpm_info_lock_give();
            return DPM_SET_ERR;
        }
    }
    RM_PMGR_W_dpm_info_lock_give();

    return DPM_SET_OK;

}

#if CFG_PMGR
int RM_PMGR_W_dpm_rcv_ready_get(char *dpm_name)
{
    pmgr_w_info_dpm_sleep_t    *pmgr_w_dpm_list;
    int    i;

    i = 0;
    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];
        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            RM_PMGR_W_dpm_info_lock_give();
            return (pmgr_w_dpm_list->rcv_ready);
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return 0;
}
#endif /* CFG_PMGR */

int RM_PMGR_W_dpm_rcv_ready_set(char *dpm_name)
{

    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;
    int    i = 0;

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return DPM_SET_OK;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];
        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            if (pmgr_w_dpm_list->port_number == 0)
                pmgr_w_dpm_list->rcv_ready = 0;
            else
                pmgr_w_dpm_list->rcv_ready = 1;

            break;
        }

        i++;
        if (i > MAX_DPM_INFO_ARRAY) {
            RM_PMGR_W_dpm_info_lock_give();
            return DPM_SET_ERR;
        }
    }
    RM_PMGR_W_dpm_info_lock_give();

    return DPM_SET_OK;
}

#if CFG_PMGR
/*
 * API for registering to use DPM_SLEEP
 */
int RM_PMGR_W_dpm_job_name_is_set(char *dpm_name)
{
    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;

    int    i;

    i = 0;
    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            RM_PMGR_W_dpm_info_lock_give();
            return DPM_REG_DUP_NAME;
        }

        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return DPM_NOT_REGISTERED;
}

char *RM_PMGR_W_dpm_port_is_set(unsigned int port_number)
{

    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;

    int i = 0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];
        if (pmgr_w_dpm_list->port_number == port_number) {
            RM_PMGR_W_dpm_info_lock_give();
            return pmgr_w_dpm_list->dpm_name;
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return (char *)NULL;
}
#endif /* CFG_PMGR */

int RM_PMGR_W_dpm_timer_create(char *task_name,
                char *timer_name,
                void (* callback_func)(char *name),
                unsigned int time,
                unsigned int reschedule_time)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int regi_tid = DPM_TIMER_ERR;
    int exist_tid = DPM_TIMER_ERR;
    int name_len = 0;
    dpm_timer_param_t    tparam;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE)
        return -1;        // DPM_MODE_NOT_ENABLED

    name_len = strlen(task_name);
    if ((REG_NAME_DPM_MAX_LEN < name_len) || (name_len < 0)) {
        PRINTF("[REG] Check the task name_len(%d) (max len=%d)\n", name_len, REG_NAME_DPM_MAX_LEN);
        return -1;
    }

    name_len = strlen(timer_name);
    if ((DPM_TIMER_NAME_MAX_LEN < name_len) || (name_len < 0)) {
        PRINTF("[REG] Check the timer name_len(%d) (max len=%d)\n", name_len, DPM_TIMER_NAME_MAX_LEN);
        return -1;
    }

    if (time > TIMER_MAX_MSEC) {
        /* size of intiger */
        PRINTF("[RTC] Timer is out of range. (max: 2097151000 msecs)\n");
        return -2;        // DPM_TIMER_SEC_OVERFLOW
    }

    exist_tid = dpm_timer_find_timer_id(task_name, timer_name);
    if ((exist_tid >= DPM_TIMER_0) && (exist_tid < DPM_TIMER_ERR)) {
        PRINTF("[DPM_TM] Exists: task_name=%s, timer_name=%s, exist_tid=%d\n",
                        task_name,
                        timer_name,
                        exist_tid);
        return -3;        // DPM_TIMER_ALREADY_EXIST
    } else if (exist_tid == -1) {
        PRINTF("[DPM_TM] Timer name error. (%s/%s)\n", task_name, timer_name);
        return -4;        // DPM_TIMER_NAME_ERROR
    }

    if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE)
        PRINTF("[DPM_TM] mutex_get error !!!\n");

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
    if (dpm_timer == NULL) {
        PRINTF("[DPM_TM] Unsupport RTM.\n");
        regi_tid = -6;        // DPM_UNSUPPORTED_RTM
        goto finish;
    }

    if (exist_tid == DPM_TIMER_ERR) {
        exist_tid = R_DPM_TIMER_EmptyIdGet();
    }

    /* Register to RTC */
    dpm_timer[exist_tid - 2].tid = exist_tid;

    tparam.callback_func = (void*)rtc_timeout_cb;
    tparam.callback_param = (void*)&(dpm_timer[exist_tid - 2].tid);
    tparam.booting_offset = (void *)NULL;

    regi_tid = R_DPM_TIMER_SleepSet(exist_tid, (((uint64_t) time) * 1000), tparam, false);
    if ((regi_tid == 0) || (regi_tid >= DPM_TIMER_ERR)) {
        PRINTF("[DPM_TM] Timer registration failed. (err_tid=%d)\n", regi_tid);
        regi_tid = -7;        // DPM_TIMER_REGISTER_FAIL
        goto finish;
    }
#ifdef FOR_DEBUG
    PRINTF(" [%s] set timer (%s/%s, msecs:%u, reg_tid:%d)\n", __func__, task_name, timer_name, time, regi_tid);
#endif

    /* Initialize */
    dpm_timer[regi_tid - 2].timeout_callback = NULL;
    dpm_timer[regi_tid - 2].msec = 0;
    memset((void *)dpm_timer[regi_tid - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
    memset((void *)dpm_timer[regi_tid - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);

    /* Register thread name */
    strncpy(dpm_timer[regi_tid - 2].task_name, task_name, sizeof(dpm_timer[regi_tid - 2].task_name));

    /* Register timer name */
    strncpy(dpm_timer[regi_tid - 2].timer_name, timer_name, sizeof(dpm_timer[regi_tid - 2].timer_name));

    /* Register callback function */
    if (callback_func != NULL) {
        dpm_timer[regi_tid - 2].timeout_callback = callback_func;
    }

    /* Register reschedule time */
    if (reschedule_time > 0)
        dpm_timer[regi_tid - 2].msec = reschedule_time;

finish:
    if (xSemaphoreGive(dpm_timer_mutex) != pdTRUE)
        PRINTF("[DPM_TM] mutex_put error !!!\n");

    return regi_tid;
}

int RM_PMGR_W_dpm_timer_delete_by_tid(int timer_id)
{
    unsigned int status;
    int ret;
    dpm_timer_info_t *dpm_timer = NULL;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE || timer_id == 2)
        return -1;
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
    if (dpm_timer == NULL) {
        return -1;
    }
    dpm_timer[timer_id - 2].timeout_callback = NULL;
    dpm_timer[timer_id - 2].msec = 0;
    memset((void *)dpm_timer[timer_id - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
    memset((void *)dpm_timer[timer_id - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);

    /* Get mutex */
    if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE) {
        PRINTF("[CANCEL] mutex_get error\n");
    }

    ret = R_DPM_TIMER_Kill(timer_id);

    status = xSemaphoreGive(dpm_timer_mutex);
    if (status != pdTRUE) {
        PRINTF("[CANCEL] mutex_put error(0x%02x)\n", status);
    }

#ifdef DPM_TIMER_DEBUG
    PRINTF("[%s] tid=%d ret=%d\n", __func__, timer_id, ret);
#endif /* DPM_TIMER_DEBUG */

    return ret;
}

int RM_PMGR_W_dpm_timer_change(char *task_name, char *timer_name, unsigned int msecs)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int exist_tid = DPM_TIMER_ERR;
    int regi_tid = DPM_TIMER_ERR;
    dpm_timer_param_t tparam;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE)
        return -1;

    if (msecs > TIMER_MAX_MSEC) {
        /* size of intiger */
        PRINTF("[RTC] Timer is out of range. (max: %lu msecs)\n", (unsigned long)TIMER_MAX_MSEC);
        return -1;
    }

    exist_tid = dpm_timer_find_timer_id(task_name, timer_name);
    if ((exist_tid >= DPM_TIMER_0) && (exist_tid < DPM_TIMER_ERR)) {
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
        dpm_timer[exist_tid - 2].msec = msecs;

        if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE)
            PRINTF("[DPM_TM] mutex_get error !!!\n");

        /* Re-register to RTC */
        dpm_timer[exist_tid - 2].tid = exist_tid;

        tparam.callback_func = (void*)rtc_timeout_cb;
        tparam.callback_param = (void*)&(dpm_timer[exist_tid - 2].tid);
        tparam.booting_offset = (void *)NULL;

        regi_tid = R_DPM_TIMER_SleepSet(exist_tid, (((uint64_t)msecs)*1000), tparam, false);
#ifdef FOR_DEBUG
        printf(" [%s] set timer (%s/%s, msecs:%u, reg_tid:%d)\n", __func__, task_name, timer_name, msecs, regi_tid);
#endif

        if (xSemaphoreGive(dpm_timer_mutex) != pdTRUE)
            PRINTF("[DPM_TM] mutex_put error !!!\n");

        return regi_tid;
    }

    PRINTF("[DPM_TM] Failed to change Timer.(%s/%s)\n", task_name, timer_name);

    return -1;
}

int RM_PMGR_W_dpm_timer_remaining_msec_get(char *task_name, char *timer_name)
{
    int timer_id = DPM_TIMER_ERR;

    timer_id = dpm_timer_find_timer_id(task_name, timer_name);
    if ((timer_id >= DPM_TIMER_0) && (timer_id < DPM_TIMER_ERR)) {
        return RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(timer_id);
    }

#if (TCP_CLIENT_APP_START != 1)
    PRINTF("[DPM_TM] Failed to read remaining time. (%s/%s)\n", task_name, timer_name);
#endif

    return -1;
}

#endif /* CFG_WIFI */

#endif /* CFG_PMGR */
