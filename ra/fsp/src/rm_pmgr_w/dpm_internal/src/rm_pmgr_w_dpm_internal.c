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
#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include <stdarg.h>
#include <strings.h>

#include "defs.h"
#include "lwip/err.h"
#include "rm_wifi_helper.h"
#include "r_pm_if.h"
#include "rm_pmgr_w_instance.h"
#include "rm_pmgr_w_dpm_internal.h"
#include "rm_pmgr_w_dpm_socket_internal.h"
#include "sleep_mgmt_regs.h"

#include "ra6w1_dpm_system.h"
#include "common_def.h"
#include "nvedit.h"

#include "common_def.h"
#include "rm_lwip_w_helper.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#include dg_configADNVPARAM_PROJ_FILE
#endif

#include "rwnx_hw.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define PS_TRANSITION_FAIL_MAX  100

#undef DPM_SLP_TRIGGER

#define TID_U_USER_WAKEUP   2
#define	USER_WAKEUP_T_NAME	"USR_TMR"

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
/* Local static variables */
static EventGroupHandle_t    ra6w1_dpm_ev_group = NULL;
TaskHandle_t dpm_timer_proc_handler = NULL;

/* Local initial variables */

#if (dg_configUSE_IDLE_TASK_TRIGGER == 1 ) || (dg_configUSE_TIMER_TRIGGER == 1)
static bool    dpm_mac_err_flag        = pdFALSE;
#endif /* (dg_configUSE_IDLE_TASK_TRIGGER == 1 ) || (dg_configUSE_TIMER_TRIGGER == 1) */
static bool    dpm_mac_err_max_try_reached_flag = pdFALSE;
static bool    dpm_rtc_to_chk_flag        = pdFALSE;

static bool    dpm_sleep_ready_flag    = pdFALSE;

extern pmgr_w_info_dpm_sleep_t    pmgr_w_info_dpm_sleep_list[];

#if defined ( WORKING_FOR_DPM_TCP )
static dpm_tcp_ka_info_t *dpm_tcp_ka_info            = NULL;
#endif /* WORKING_FOR_DPM_TCP */

static char dpm_timer_abnormal_flag[DPM_TIMER_ERR] = { 0, };

#ifndef __DISABLE_DPM_ABNORM__
static bool is_set_abnormal_timer = pdFALSE;
#endif /*__DISABLE_DPM_ABNORM__*/

/* Local static functions */
#if defined ( WORKING_FOR_DPM_TCP )
static void clr_all_dpm_tcp_ka_info(void);
#endif /* WORKING_FOR_DPM_TCP */

void RM_WIFI_set_ptim_wake_dur_according_nvram(void);
UINT is_dpm_supplicant_done(void);
static bool RM_PMGR_W_dpm_user_timer_is_soon_timeout(int soon_time);

static SemaphoreHandle_t dpm_sleep_info_lock = NULL;

static int dpm_lld_task_is_ready = pdFALSE;

#ifdef DPM_SLP_TRIGGER
#define DPM_LLD_SLP_TRIGGER_COUNTER_MAX 5
static int dpm_lld_slp_trigger_counter = 0; 
#endif /* DPM_SLP_TRIGGER */

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
/* External variables */

extern unsigned char dpm_dbg_cmd_flag;
unsigned char dpm_slp_time_reduce_flag;
extern TimerHandle_t dpm_sts_chk_tm;
extern TaskHandle_t poll_state_chk_thd;

/* External global functions */
int    dpm_wpa_supp_state        = WPA_DISCONNECTED;
extern int  get_run_mode(void);
extern int  get_boot_mode(void);
extern bool get_wifi_driver_can_rcv(void);
extern void set_wifi_driver_not_rcv(void);
extern void unset_wifi_driver_not_rcv(void);
extern void start_dpm_sts_chk_timer(int dpm_wu_type);

extern void fc80211_connection_loss(void);
extern void set_dpm_wakeup_condition(unsigned int type);
extern int  fc80211_get_empty_rid(void);
extern unsigned char get_last_abnormal_cnt(void);

extern int do_autoarp_check(void);

/* Global variables */
dpm_supp_key_info_t        *dpm_supp_key_info = NULL;
dpm_supp_conn_info_t    *dpm_supp_conn_info = NULL;
dpm_supp_conn_ext_info_t    *dpm_supp_conn_ext_info = NULL;

/* Local static functions */
#if (dg_configUSE_IDLE_TASK_TRIGGER == 1)
void dpm_trigger_fn();
#elif (dg_configUSE_TIMER_TRIGGER == 1)
static void dpm_trigger_fn(unsigned long arg);
#endif    // dg_configUSE_IDLE_TASK_TRIGGER

enum dpm_enum_dbg_level {
    MSG_ERROR = 1, MSG_WARNING, MSG_INFO, MSG_DEBUG, MSG_EXCESSIVE, MSG_MSGDUMP
};

bsp_wakeup_source_t source = BSP_WAKEUP_SOURCE_POR;

#if (dg_configUSE_TIMER_TRIGGER == 1)
TimerHandle_t        dpm_trigger_timer = NULL;
#endif    // dg_configUSE_TIMER_TRIGGER
TaskHandle_t            dpm_sleep_daemon_handler = NULL;

bool    dpm_daemon_start_flag    = pdFALSE;
static bool    dpm_daemon_hold_flag    = pdFALSE;
int    dpm_pdown_state            = WAIT_DPM_SLEEP;

SemaphoreHandle_t    dpm_timer_mutex = NULL;
/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/



/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
void set_dpm_supp_ptr(void);
char dpm_lmac_get_tim_wakeup_abnormal(void);
bool  get_dpm_pwrdown_fail_max_trial_reached(void);
void  set_dpm_pwrdown_fail_max_trial_reached(bool value);
void stop_dpm_mode(void);
int is_dpm_sleep_ready(void);
void show_dpm_unsleep_task_names(void);
unsigned char *get_dpm_unsleep_task_names(void);
void set_dpm_all_flags(void);
int chk_dpm_sleep_daemon(void);
void clear_abnormal_DPM_param(void);
int goto_dpm_sleep(void);
int dpm_mac_ps_mode_set(bool ps_mode);
void dpm_mac_drv_recv_mode_set(int recv_on);
bool chk_dpm_init_done(char * dpm_name);
void dpm_accept_tim_arp_resp(unsigned long gw_ip, unsigned char * gw_mac);
void dpm_print_regi_tcp_port(void);
void get_dpm_tcp_port_filter(void);
void set_dpm_tim_tcp_chkport_enable(void);
void dpm_reset_env_udph_enable(void);
void ddps_wakeup_chg_noti_cb_reg(void (* user_1sec_cb)(void) , void (* user_3sec_cb)(void));
void dpm_set_arp_resp(int bc_timeout);
unsigned char dpm_get_ap_params_dtim_period(void);
USHORT get_dpm_tim_data_seqnum(void);
unsigned int get_rtc_timeout_tid(void);
unsigned int clr_rtc_timeout_tid(int tid);
void disable_all_dpm_timer(void);
void dpm_set_otp_xtal40_offset(unsigned long otp_xtal40_offset);
bool dpm_rxl_mpdu_to_umac(void);
void do_printf(int do_print, const char * fmt, ...) ;


/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
static void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260) {
        loop_count = (unsigned char)(260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0) {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }
    printf("%s", prt_str);
}

void RM_PMGR_W_dpm_enable(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFALSE) {
        /* Unsupport RTM */

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENABLE_DPM, 1);
#endif
        return;
    }

    if (RTM_FLAG_PTR->dpm_mode) {
        return;
    }

    /* Stop functions while dpm mode running */
    unsupport_func_in_dpm();

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_ENABLE_DPM, 1);
#endif

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    RTM_FLAG_PTR->dpm_mode = 1;
}

void RM_PMGR_W_dpm_disable(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFALSE) {
        /* Unsupport RTM */

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENABLE_DPM, 0);
#endif
        return;
    }

    if (RTM_FLAG_PTR->dpm_mode == 0) {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENABLE_DPM, 0);
#endif
        return;
    }

#if (dg_configUSE_TIMER_TRIGGER == 1)
    if (dpm_trigger_timer != NULL) {
        xTimerStop(dpm_trigger_timer, 0);        // Stop DPM trigger timer
        xTimerDelete(dpm_trigger_timer, 0);        // Delete DPM trigger timer
        dpm_trigger_timer = NULL;
    }
#endif    // dg_configUSE_TIMER_TRIGGER
    
    /* Delete DPM daemon task */
    if (dpm_sleep_daemon_handler != NULL) {
        vTaskDelete(dpm_sleep_daemon_handler);
        dpm_sleep_daemon_handler = NULL;
    }

    if (poll_state_chk_thd) {
        vTaskDelete(poll_state_chk_thd);
        poll_state_chk_thd = NULL;
    }

    vTaskDelay(portCONVERT_MS_2_TICKS(20));

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                ENV_GROUP_WIFIPROFILE,
                                WIFI_PROFILE_ENABLE_DPM, 0);
#endif
    vTaskDelay(portCONVERT_MS_2_TICKS(10));

    dpm_daemon_start_flag = pdFALSE;
    dpm_daemon_hold_flag = pdFALSE;

    RTM_FLAG_PTR->dpm_mode = 0;
}

#if CFG_PMGR
int RM_PMGR_W_dpm_is_enabled(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return pdFALSE ;
    }

    if (RTM_FLAG_PTR->dpm_mode) {
        return pdTRUE;
    }

    return pdFALSE;
}
#endif /* CFG_PMGR */

int RM_PMGR_W_dpm_sleep_is_hold(void)
{
    return dpm_daemon_hold_flag;
}

void RM_PMGR_W_dpm_sleep_resume(void)
{
    dpm_daemon_hold_flag = 0;
}

bool RM_PMGR_W_dpm_sleep_is_lld_task_running(void)
{
    return (dpm_sleep_daemon_handler != NULL);
}

static int get_dpm_tim_wakeup_dur(void)
{
    if (!RM_PMGR_W_rtm_exist()) {
        return -1;
    }

    return RTM_FLAG_PTR->dpm_dtim_period;
}

char dpm_wakeup_is_abnormal = 0;
void RM_PMGR_W_dpm_ptim_abnormal_wakeup_set(void)
{
    dpm_wakeup_is_abnormal = 1;
    return;
}

char dpm_lmac_get_tim_wakeup_abnormal(void)
{
    return dpm_wakeup_is_abnormal;
}

/// Common APIs ////////////////////////////////////////

bool  get_dpm_pwrdown_fail_max_trial_reached(void)
{
    return dpm_mac_err_max_try_reached_flag;
}

void  set_dpm_pwrdown_fail_max_trial_reached(bool value)
{
    dpm_mac_err_max_try_reached_flag = value;
}

void unsupport_func_in_dpm(void)
{
    /* dpm is not support - Simple roaming */
#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                "STA_roam", 0);
#endif
}

void stop_dpm_mode(void)
{
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return;

    if (RTM_FLAG_PTR->dpm_mode == 0)
        return;

    dpm_daemon_start_flag = pdFALSE;            // Stop DPM Sleep Daemon
    dpm_daemon_hold_flag = pdFALSE;                // Stop DPM Sleep Daemon

#if (dg_configUSE_TIMER_TRIGGER == 1)
    xTimerStop(dpm_trigger_timer, 0);        // Stop DPM trigger timer
    xTimerDelete(dpm_trigger_timer, 0);        // Delete DPM trigger timer
#endif    // dg_configUSE_TIMER_TRIGGER

    /* Delete DPM daemon task */
    if (dpm_sleep_daemon_handler != NULL) {
        vTaskDelete(dpm_sleep_daemon_handler);
        dpm_sleep_daemon_handler = NULL;
    }

    RTM_FLAG_PTR->dpm_mode = 0;

    vTaskDelay(portCONVERT_MS_2_TICKS(20));
}

int RM_PMGR_W_dpm_mode_get(void)
{
    int dpm_mode = 0;

    if (RM_PMGR_W_rtm_exist() == pdFALSE) {
        /* Unsupport RTM */
        return 0;
    }

    if (RTM_FLAG_PTR->dpm_mode != 1) {
#ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                       ENV_GROUP_WIFIPROFILE,
				                       WIFI_PROFILE_ENABLE_DPM, &dpm_mode) != FSP_SUCCESS)
#endif
#if (TEST_APP_START != 0)
            RTM_FLAG_PTR->dpm_mode = 1; // by default, if 'dpm_mode' nvram item does not exist, enable dpm_mode
#else
            RTM_FLAG_PTR->dpm_mode = 0; // for cicd wifi test disable dpm
#endif
        else
            RTM_FLAG_PTR->dpm_mode = dpm_mode;
    }

    return RTM_FLAG_PTR->dpm_mode;
}

void RM_PMGR_W_dpm_dbg_info_get(void)
{
    pmgr_w_info_dpm_sleep_t * pmgr_w_dpm_list;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
    int i = 0;
    bool is_managed_job_sleep_ready = pdTRUE;
    int user_wu_time = 0;

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                               WIFI_PROFILE_DPM_USER_WAKEUP_TIME, &user_wu_time);
#endif

    printf("\n");
    printf("[%s]\n", RM_PMGR_W_dpm_sleep_is_hold() ? "HOLD" : "RESUME");
    printf("===============================================\n");
    printf("  DPM_SLEEP state - mode(%d), wakeup(%d), debug(%d)\n",
                RTM_FLAG_PTR->dpm_mode,
                RTM_FLAG_PTR->dpm_wakeup,
                RTM_FLAG_PTR->dpm_dbg_level);
    printf("===============================================\n");

    printf(" - User wakeup time      = %d msec\n", (user_wu_time == -1) ? DFLT_DPM_USER_WAKEUP_TIME : user_wu_time);
    printf(" - KeepAlive time        = %d ms\n", RTM_FLAG_PTR->dpm_keepalive_time_msec);
    printf(" - DPM TIM Wakeup Count  = %d dtim\n", RTM_FLAG_PTR->dpm_dtim_period);
    printf("===============================================\n");
    printf("Job\t\tSleep Ready Status\n");
    printf("===============================================\n");

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);

    while (i < MAX_DPM_INFO_ARRAY)
    {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strlen(pmgr_w_dpm_list->dpm_name) != 0)
        {
            if (pmgr_w_dpm_list->is_managed == pdTRUE)
            {
                printf("%s\t\t%s\n", pmgr_w_dpm_list->dpm_name, pmgr_w_dpm_list->sleep_count == 1 ? "No" : "Yes");

                if(pmgr_w_dpm_list->sleep_count == 1)
                {
                    is_managed_job_sleep_ready = pdFALSE;
                }
            }
            else
            {
                printf("%s\t\t%s\n", pmgr_w_dpm_list->dpm_name, "Unknown");
            }
        }

        i++;
    }

    RM_PMGR_W_dpm_info_lock_give();

    printf("\n");
    printf("===============================================\n\n");

    if (is_managed_job_sleep_ready)
    {
        printf("WiFi Tasks: ready to sleep\n");
    }
    else
    {
        printf("WiFi Tasks: not ready to sleep\n");
    }

    if (p_instance_ctrl != NULL)
    {
        uint8_t ram_constarints = 0;

        RM_PMGR_W_get_sleep_constraint_counter(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM, &ram_constarints);

        if (ram_constarints == 0)
        {
            printf("PMGR RAM Constraints: ready to sleep\n");
        }
        else
        {
            printf("PMGR RAM Constraints: not ready to sleep (PMGR_CONSTRAINT_POWER_RAM:%d)\n", ram_constarints);
        }
    }

    if (dpm_sleep_ready_flag)
    {
        printf("PMGR DPM sleep event: signaled \n");
    }
    else
    {
        printf("PMGR DPM sleep event: not signaled \n");
    }

    printf("DPM Sleep started: %d\n", RM_PMGR_W_dpm_sleep_is_started());

    printf("\n");
    printf("===============================================\n\n");
}


int is_dpm_sleep_ready(void)
{
    if (dpm_sleep_ready_flag) {
        return 1;
    } else {
        return 0;
    }
}

void show_dpm_unsleep_task_names(void) {
        pmgr_w_info_dpm_sleep_t    *pmgr_w_dpm_list;
    int i=0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strlen(pmgr_w_dpm_list->dpm_name) != 0) {
            if(pmgr_w_dpm_list->is_managed==pdTRUE && pmgr_w_dpm_list->sleep_count==1) {
                printf("[%s] Unsleep module name=<%s>\n", __func__, pmgr_w_dpm_list->dpm_name);
            }
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();
    return;
}

unsigned char *get_dpm_unsleep_task_names(void) {
    pmgr_w_info_dpm_sleep_t    *pmgr_w_dpm_list;
    int i=0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strlen(pmgr_w_dpm_list->dpm_name) != 0) {
            if(pmgr_w_dpm_list->is_managed==pdTRUE && pmgr_w_dpm_list->sleep_count==1) {
                RM_PMGR_W_dpm_info_lock_give();
                return ((unsigned char *)pmgr_w_dpm_list->dpm_name);
            }
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return (unsigned char *) NULL;
}

void set_dpm_all_flags(void) /*UNUSED*/
{
    dpm_sleep_ready_flag = pdTRUE;
}

#if CFG_PMGR
int RM_PMGR_W_dpm_sleep_is_set(char *mod_name) {
    pmgr_w_info_dpm_sleep_t    *pmgr_w_dpm_list;
    int i=0;
    FSP_PARAMETER_NOT_USED(mod_name);

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strlen(pmgr_w_dpm_list->dpm_name) != 0) {
            if(pmgr_w_dpm_list->sleep_count==0) {
                RM_PMGR_W_dpm_info_lock_give();
                return 1;
            } else {
                RM_PMGR_W_dpm_info_lock_give();
                return 0;
            }
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return 0;
}
#endif /* CFG_PMGR */

int RM_PMGR_W_dpm_rcv_ready_set_by_port(unsigned int port)
{
    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;
    int    i = 0;

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return DPM_SET_OK;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    for (i = 0 ; i < MAX_DPM_INFO_ARRAY ; i++) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];
        if (pmgr_w_dpm_list->port_number == port) {
            pmgr_w_dpm_list->rcv_ready = 1;
            RM_PMGR_W_dpm_info_lock_give();
            return DPM_SET_OK;
        }
    }
    RM_PMGR_W_dpm_info_lock_give();
    
    return DPM_SET_ERR;
}

#if CFG_PMGR
int RM_PMGR_W_dpm_sleep_ready_set(char *dpm_name) {
    
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return DPM_SET_OK;
    }

    int result = DPM_SET_ERR;

    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    pmgr_w_info_dpm_sleep_t * pmgr_w_dpm_list = NULL;
    int i = 0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {

            pmgr_w_dpm_list->is_managed = pdTRUE;

            if (pmgr_w_dpm_list->sleep_count == 1) {
                if(RM_PMGR_W_remove_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM) == FSP_SUCCESS) {

                    if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED) {
                        if (pmgr_w_dpm_list->idx_ram_constr_dbg_tbl != -1) {
                            p_instance_ctrl->ram_counter_cause[pmgr_w_dpm_list->idx_ram_constr_dbg_tbl]--;
                        }
                    }

                    pmgr_w_dpm_list->sleep_count = 0;
                    result = DPM_SET_OK;
                }
            } else {
                result = DPM_SET_OK;
            }

            break;
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    if (i == MAX_DPM_INFO_ARRAY) {
        result = DPM_NOT_REGISTERED;
    }

    return result;
}

int RM_PMGR_W_dpm_sleep_ready_clear(char *dpm_name) {

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return DPM_SET_OK;
    }

    int result = DPM_SET_ERR;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();

    pmgr_w_info_dpm_sleep_t * pmgr_w_dpm_list = NULL;
    int i = 0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            pmgr_w_dpm_list->is_managed = pdTRUE;

            if (pmgr_w_dpm_list->sleep_count == 0) {
                if(RM_PMGR_W_add_sleep_constraint(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM) == FSP_SUCCESS) {
                    
                    if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED) {
                        if (pmgr_w_dpm_list->idx_ram_constr_dbg_tbl != -1) {
                            p_instance_ctrl->ram_counter_cause[pmgr_w_dpm_list->idx_ram_constr_dbg_tbl]++;
                        }
                    }

                    pmgr_w_dpm_list->sleep_count = 1;
                    result = DPM_SET_OK;
                }
            } else {
                result = DPM_SET_OK;
            }

            break;
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    if (i >= MAX_DPM_INFO_ARRAY) {
        result = DPM_NOT_REGISTERED;
    }
    
    return result;
}
#endif /* CFG_PMGR */

int chk_dpm_sleep_daemon(void)
{
    return RTM_FLAG_PTR->dpm_sleepd_stop;
}

/* Abnormal DPM Parameter clearing */
void clear_abnormal_DPM_param(void)
{
    /* If last sleep type is abnormal , do clearing */
    //if (RTM_DPM_MONITOR_PTR->last_sleep_type) {
        //save_dpm_sleep_type(0);
        RTM_DPM_MONITOR_PTR->last_sleep_type = 0;
        RTM_DPM_MONITOR_PTR->last_abnormal_type = 0;
        RTM_DPM_MONITOR_PTR->last_abnormal_count = 0;
    //}
    return;
}

int goto_dpm_sleep(void)
{
    uint32_t addr = dg_configPTIMG_HDR_ADDR;
    uint32_t id = DPM_TIMER_0;
    int64_t  sleep_time;
    dpm_timer_param_t tparam;
    int            ret;

    uint32_t wsrc = *((uint32_t *) dg_configBOOTER_RTM_ADDR);
    bool    dpminit = false;

    clear_abnormal_DPM_param();

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_wakeup()) {
        // As ARP Table is ready, do Auto ARP schduling with period, mode 
        RM_WIFI_dpm_ptim_auto_arp_enable(1);
    }
#endif /* CFG_PMGR */

    RM_PMGR_W_socket_dpm_tcp_sess_ptim_config_for_connected_one();

    RM_PMGR_W_socket_dpm_ptim_udph_config();

    if ((wsrc & BSP_WAKEUP_SOURCE_POR) ||                  // POR
        (wsrc & BSP_WAKEUP_SOURCE_WATCHDOG) ||             // WDOG
        ((wsrc & BSP_WAKEUP_RESET_WITH_RETENTION) == 0) || // w/o RTM
        ((wsrc & 0x7f) == 0) ||                            // RESET or REBOOT
        (!RM_PMGR_W_dpm_is_wakeup()))                      // Not DPM
        dpminit = true;

    GLOBAL_INT_DISABLE();
#ifdef FOR_DEBUG
    PRINTF(" [%s] DPM Tim dtim_period: %d ka: %d \n", __func__, get_dpm_tim_wakeup_dur(), RTM_FLAG_PTR->dpm_keepalive_time_msec);
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
#endif
    romac4rtos_req_period(get_dpm_tim_wakeup_dur(), (RTM_FLAG_PTR->dpm_keepalive_time_msec) / 100);
    romac4rtos_ready(dpminit, 0);
    romac4rtos_ready_sleep(dpminit);
    romac4rtos_finalize();

#if defined(VBATT_TX_POWER_LIMIT)
    extern uint8_t	tx_power_limit;

    printf("pTIM pwr : %d\n", tx_power_limit);
    romac4rtos_set_pwr(tx_power_limit);
    romac4rtos_set_usrpwr(tx_power_limit);
#endif

    rwnx_hw_power_down(RWNX_HW_POWERDOWN_MODE_ALL);

    R_DPM_TIMER_Kill(id);

    tparam.callback_func = (void*)NULL;
    tparam.callback_param = (void*)NULL;
    tparam.booting_offset = (void*)addr;

    sleep_time = romac4rtos_calc_sleep_time();

    ret = R_DPM_TIMER_SleepSet(id, sleep_time, tparam, true);
    GLOBAL_INT_RESTORE();

    return ret;
}

#define MAC_PS_MODE_ON  1
#define MAC_PS_MODE_OFF 0
extern bool dpm_mac_send_ps_mode(bool pwrmgt_flg);
int dpm_mac_ps_mode_set(bool ps_mode)
{
    if (!dpm_mac_send_ps_mode(ps_mode))
    {
        return DPM_SET_ERR;
    }

    romac4rtos_set_pwrmgt(ps_mode);

    return DPM_SET_OK;
}

#define MAC_DRV_RECV_ON  1
#define MAC_DRV_RECV_OFF 0
void dpm_mac_drv_recv_mode_set(int recv_on)
{
    if (recv_on == 1) {
        if (get_wifi_driver_can_rcv() == pdTRUE) {
            unset_wifi_driver_not_rcv();
        }  
    } else {
        set_wifi_driver_not_rcv();
    }    
}
#ifdef DPM_SLP_TRIGGER
static void dpm_lld_sleep_trigger_cb(TimerHandle_t xTimer)
{   
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
    uint8_t ram_counter, rtm_counter;

    RM_PMGR_W_get_sleep_constraint_counter(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RETENTION, &rtm_counter);
    RM_PMGR_W_get_sleep_constraint_counter(p_instance_ctrl, PMGR_CONSTRAINT_POWER_RAM, &ram_counter);

    if (dpm_daemon_hold_flag == pdFALSE && rtm_counter > 0  && ram_counter == 0  && dpm_pdown_state == WAIT_DPM_SLEEP) {
        dpm_lld_slp_trigger_counter++;

        if (dpm_lld_slp_trigger_counter > DPM_LLD_SLP_TRIGGER_COUNTER_MAX) {
            if (dpm_sleep_daemon_handler && dpm_lld_task_is_ready) {
                dpm_sleep_ready_flag = pdTRUE;
                xTaskNotify(dpm_sleep_daemon_handler, EVT_MPM_DPM_SLEEP_RDY, eSetBits);
                dpm_lld_slp_trigger_counter = 0;
            }
        }
    } else {
        dpm_lld_slp_trigger_counter = 0;
    }
}
#endif /* DPM_SLP_TRIGGER */

static void dpm_lld_task(void *arg)
{
    uint32_t ret;
    uint32_t notified_val = 0;
    int pdown_result;
    int ps_transition_fail_cnt = 0;
    bool wifi_ps_mode_config = false;
    // The arg is RTOS4DPM_ST, not DPM_WAKEUP_TYPE
    int dpm_wu_type = (int)arg;
#ifdef __DISABLE_DPM_ABNORM__
    int init_count = 0;
#endif /* __DISABLE_DPM_ABNORM__ */

#ifdef DPM_SLP_TRIGGER
    TimerHandle_t   slp_trig_hdl = NULL;
#endif /* DPM_SLP_TRIGGER */

#ifdef FOR_DEBUG
    PRINTF(CYN_COL "Start dpm sleep daemon !!!\n" CLR_COL);
#endif /* FOR_DEBUG */

    /* move to dpm_full_wakeup_wlaninit to here */

    /* dpm_wu_type 0 : normal power on reboot
     *           3 : no beacon / no ack / deauth
     *           4 : uc wakeup
     *           5 : beacon changed / bc_mc
     */
    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
    {
        /* In case of DPM Wakeup & tim status is UC, BC, BC Changed,
         * we should start the RX packet of MAC of TIM */
        if (CHK_PTIM_STATUS(dpm_wu_type, RTOS4DPM_ST_UPLOAD) || CHK_PTIM_STATUS(dpm_wu_type, RTOS4DPM_ST_DEAUTH))
        {
            /* For Others rx packet processing,
             * move to wlaninit(), only need packet processing delay */

            if (dpm_slp_time_reduce_flag == pdFALSE)
            {
                // For UC wakeup, need enough loading time of network stack.
                vTaskDelay(portCONVERT_MS_2_TICKS(100));
            }
        }
        else if (dpm_wu_type == DPM_UNKNOWN_WAKEUP)
        {
            /* In case of Abnormal wakeup with UNNKOWN_TYPE,
             * some ISSI EVB should to have time delay to start DPM daemon. */
            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }
    }
    else
    {
        /* Clear TCP network information,,, first POR state */
#if defined ( WORKING_FOR_DPM_TCP )
        clr_all_dpm_tcp_ka_info();
#endif    // WORKING_FOR_DPM_TCP

        RM_PMGR_W_socket_dpm_all_tcp_sess_info_clear();
    }

    /* for user RTM area ... */
    RM_PMGR_W_user_rtm_pool_create();

    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
    {
        while (!is_dpm_supplicant_done())
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(10));
        }
    }

#ifdef DPM_SLP_TRIGGER
    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE)
    {
        slp_trig_hdl = xTimerCreate("slp_trig_tmr",
                                    portCONVERT_MS_2_TICKS(1000),
                                    pdTRUE,
                                    (void*) 0,
                                    (TimerCallbackFunction_t) dpm_lld_sleep_trigger_cb);

        xTimerStart(slp_trig_hdl, portCONVERT_MS_2_TICKS(1000));
     }
#endif /* DPM_SLP_TRIGGER */

    while(1)
    {
        /* Create timer to check DPM abnormal status. */
#ifndef __DISABLE_DPM_ABNORM__
        // In case of disable DPM-abnormal timer..
        int wifi_profile = 0;
        if (is_set_abnormal_timer==pdFALSE && 
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFICFG,
                                         NVR_KEY_PROFILE_0, &wifi_profile) == FSP_SUCCESS)
        {
#endif
            start_dpm_sts_chk_timer(dpm_wu_type);
            is_set_abnormal_timer = pdTRUE;
        }
#else
        while (check_net_init(0) != pdPASS)
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(100));        // 100 msec
            init_count++;
            if (init_count >= 3)
            {
                PRINTF("[%s] Failed to init network.(retry: %d) \n", __func__, init_count);
                break;
            }
        }
#endif /* __DISABLE_DPM_ABNORM__ */

        /* Enable Data Rx */
        dpm_mac_drv_recv_mode_set(MAC_DRV_RECV_ON);

        dpm_lld_task_is_ready = pdTRUE;

        // wait to be signaled
        ret = xTaskNotifyWait(0x0, EVT_MPM_DPM_ALL, &notified_val, portMAX_DELAY);
        configASSERT(ret == pdPASS);

        if (notified_val & EVT_MPM_DPM_SLEEP_RDY)
        {
            /* Mark sleep process has started - blocks other tasks from doing further work */
            dpm_pdown_state = RUN_DPM_SLEEP;

            if (RM_PMGR_W_dpm_user_timer_is_soon_timeout(200) == pdTRUE) //msec
            {
                dpm_pdown_state = WAIT_DPM_SLEEP;
                dpm_sleep_ready_flag = pdFALSE;
                /* Continue to process the timeout callback */
                continue;
            }

            /*
                Wait until tcp sess (if any) are finished with ongoing transaction 
                e.g. checking packet pool and transmission state <-- v2.4's code can be 
                replaced with this code
            */
            if (!RM_PMGR_W_socket_dpm_ongoing_transaction_is_finished())
            {
                dpm_pdown_state = WAIT_DPM_SLEEP;
                vTaskDelay(portCONVERT_MS_2_TICKS(20));
                dpm_sleep_ready_flag = pdFALSE;
                continue;
            }

            extern void dpm_wait_for_twt_wake_interval(void);
            dpm_wait_for_twt_wake_interval();
            
            WIFI_GetPsMode(&wifi_ps_mode_config);
            if (!wifi_ps_mode_config) {
                if (dpm_mac_ps_mode_set(MAC_PS_MODE_ON) != DPM_SET_OK)
                {
                    ps_transition_fail_cnt++;
                    if (ps_transition_fail_cnt >= PS_TRANSITION_FAIL_MAX)
                    {
                        PRINTF("Fail to send PS transition frame exceeded limit (%d). force DPM_sleep...\r\n", PS_TRANSITION_FAIL_MAX);
                        goto FORCE_DPM_SLEEP;
                        
                    }
                    dpm_pdown_state = WAIT_DPM_SLEEP;
                    vTaskDelay(portCONVERT_MS_2_TICKS(100));
                    dpm_sleep_ready_flag = pdFALSE;
                    continue;
                }
                ps_transition_fail_cnt = 0;
            }

FORCE_DPM_SLEEP:

            /* Start Power Down for DPM sleep */
            printf(YEL_COL ">>> Start DPM Power-Down !!! \n" CLR_COL);

            dpm_pdown_state = DONE_DPM_SLEEP;

            // call notifier callback for DPM Sleep event
            RM_PMGR_W_dpm_notify(PMGR_EVENT_ENTERING_SLEEP);

            vTaskDelay(portCONVERT_MS_2_TICKS(10));

#ifdef __DPM_MNG_SAVE_RTM__
            if (dpmDaemonSleepFunction)
            {
#ifdef ENABLE_DPM_DBG_LOG
                PRINTF("[%s] Call register function !!!\n", __func__);
#endif // ENABLE_DPM_DBG_LOG
                dpmDaemonSleepFunction();
            }
#endif /* __DPM_MNG_SAVE_RTM__ */

            extern void dpm_set_mac_idle_before_sleep(void);
            extern void mm_active(void);

            if (!wifi_ps_mode_config)

                dpm_set_mac_idle_before_sleep();

            /* Set Data-Rx blocking */
            dpm_mac_drv_recv_mode_set(MAC_DRV_RECV_OFF);

#ifdef __BLE_COMBO_REF__
            extern void combo_make_sleep_rdy(void);
            combo_make_sleep_rdy();
#endif // __BLE_COMBO_REF__

            pdown_result = goto_dpm_sleep();

            /* Save DPM sleep type in RTM */
#if 0 // move to Power down api as clear_abnormal_DPM_param() API    
            RTM_DPM_MONITOR_PTR->last_sleep_type = 0;
            RTM_DPM_MONITOR_PTR->last_abnormal_type = 0;
            RTM_DPM_MONITOR_PTR->last_abnormal_count = 0;
#endif
            if (pdown_result ==  DPM_TIMER_ERR)
            {
                dpm_mac_drv_recv_mode_set(MAC_DRV_RECV_ON);
                if (!wifi_ps_mode_config) {
                    mm_active();
                    dpm_mac_ps_mode_set(MAC_PS_MODE_OFF);
                }
                dpm_pdown_state = WAIT_DPM_SLEEP;
            }
            else
            {
                // cannot reach here if not DPM_TIMER_ERR
            }
        }
    }

    vTaskDelete(NULL);
}

void RM_PMGR_W_dpm_lld_task_init(void)
{
    // Don't start if DPM mode is disabled.
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return;

    // Create mendatory resources
    if (dpm_daemon_start_flag == pdFALSE) {
        // Create event group for DPM operation
        ra6w1_dpm_ev_group = xEventGroupCreate();
        if (ra6w1_dpm_ev_group == NULL) {
            PRINTF("[%s] Failed to create event group !!!\n", __func__);
            return;
        }

        // Mark create status
        dpm_daemon_start_flag = pdTRUE;
    }
}

#if (dg_configUSE_TIMER_TRIGGER == 1)
static void dpm_trigger_fn_wrapper(TimerHandle_t xTimer)
{
    dpm_trigger_fn((unsigned long) xTimer);
}
#endif

void RM_PMGR_W_dpm_lld_task_start(int dpm_wu_type)
{
    int        result;

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return;
    }

    /* Disable DPM Sleep daemon */
    RTM_FLAG_PTR->dpm_sleepd_stop = 0;

#if (dg_configUSE_TIMER_TRIGGER == 1)
    /* Create timer to trigger the dpm_sleep_daemon periodically. */
    dpm_trigger_timer = xTimerCreate(DPM_DAEMON_TRIGGER_TASK_NAME,
                                portCONVERT_MS_2_TICKS(DPM_TRIGGER_POLLING_TIME),
                                pdTRUE,
                                (void *)NULL,
                                dpm_trigger_fn_wrapper);
    if (dpm_trigger_timer == NULL) {
        if (dpm_dbg_cmd_flag == pdTRUE && RTM_FLAG_PTR->dpm_dbg_level >= MSG_ERROR) {
            PRINTF("[%s] Failed to create DPM trigger timer !!!\n", __func__);
        }
#ifdef FOR_DEBUG
    } else {
        PRINTF(YELLOW_COLOR " [%s] Create DPM trigger timer !!!\n" CLEAR_COLOR, __func__);
#endif
    }

    /* For Fast DPM Trigger Starting */    
    xTimerStart(dpm_trigger_timer, portCONVERT_MS_2_TICKS(10));    // Starting After 10m sec
#ifdef FOR_DEBUG
    PRINTF(YELLOW_COLOR " [%s] Start DPM trigger timer !!!\n" CLEAR_COLOR, __func__);
#endif
#endif    // dg_configUSE_TIMER_TRIGGER

#if (dg_configUSE_SLEEP_MGMT_FUNCTION == 1)
    // Create DPM daemon task
    result = xTaskCreate(dpm_lld_task,
                            DPM_SLEEP_DAEMON_NAME,
                            DPM_SLEEPD_STACK_SZ,
                            (void *)dpm_wu_type,
                            DPM_SLEEPD_PRIORITY,
                            (TaskHandle_t *)&dpm_sleep_daemon_handler);
    if (result != pdPASS) {
        PRINTF("[%s] Failed to create DPM daemon task !!!\n", __func__);
    }
#endif    // (dg_configUSE_SLEEP_MGMT_FUNCTION == 1)
}

bool chk_dpm_init_done(char * dpm_name)
{
    pmgr_w_info_dpm_sleep_t *pmgr_w_dpm_list;
    int     i;

    i = 0;
    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            RM_PMGR_W_dpm_info_lock_give();
            return (pmgr_w_dpm_list->init_done);
        }
        i++;
    }
    RM_PMGR_W_dpm_info_lock_give();

    return pdFALSE;
}



bool RM_PMGR_W_dpm_wakeup_is_done(char *dpm_name)
{
    int     retryCount = 0;

    if (dpm_name == NULL)
        return 1;

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        return 1;

    if (strlen(dpm_name) == 0)
        return 1;

confirm:
    if (chk_dpm_init_done(dpm_name)) {
        return 1;
    } else {
        if ( ++retryCount < 50 ) {
            vTaskDelay(portCONVERT_MS_2_TICKS(20));
            goto confirm;
        } 
        else {
        }
    }

    return ERR_OK;
}

void RM_PMGR_W_dpm_environ_init(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return;
    }

    set_dpm_supp_ptr();    // for Supplicant

    RM_PMGR_W_socket_dpm_init(); // for TCP session

    RM_PMGR_W_user_rtm_pool_create();    //for user RTM region

    /* RTC Timer Mutex */
    if (dpm_timer_mutex == NULL) {
        dpm_timer_mutex = xSemaphoreCreateMutex();
    }
    
    if (dpm_timer_mutex == NULL) {
        PRINTF("\n[DPM_TM] dpm_timer_mutex create error !!!\n");
    }

    if (RM_PMGR_W_dpm_is_wakeup() == pdFALSE)
    {
        int ka_period;
        // Clear RTM - Normal booting
#if CFG_PMGR
        RM_WIFI_dpm_conn_info_clear();
#endif /* CFG_PMGR */

        // Save keepalive to RTM
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                   ENV_GROUP_WIFIPROFILE,
                                   WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME, &ka_period);
#endif
        if (ka_period == -1) {
            ka_period = DFLT_DPM_KEEPALIVE_TIME;
        }
        
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_KEEPALIVE, ka_period, 0);

        RM_WIFI_dpm_ptim_wakeup_count_set_from_nvram();

#if defined ( WORKING_FOR_DPM_TCP )
        /* clear tcp network info */
        clr_all_dpm_tcp_ka_info();
#endif    // WORKING_FOR_DPM_TCP

        RM_PMGR_W_socket_dpm_all_tcp_sess_info_clear();

        // RTC timeout flag clear
        RTM_FLAG_PTR->dpm_rtc_timeout_flag = 0;
        RTM_FLAG_PTR->dpm_rtc_timeout_tid = 0;

        if (dpm_dbg_cmd_flag == pdTRUE) {
            /* Set default debug level */
            RTM_FLAG_PTR->dpm_dbg_level = MSG_ERROR;
        }
    }
}


void set_dpm_supp_ptr(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return;
    }

    /* Supplicant Basic WiFi infomation */
    dpm_supp_conn_info = RTM_SUPP_CONN_INFO_PTR;

    /* Supplicant External WiFi information */
    dpm_supp_conn_ext_info = RTM_SUPP_CONN_EXT_INFO_PTR;

    /* Supplicant Key infomation */
    dpm_supp_key_info = RTM_SUPP_KEY_INFO_PTR;
}

void RM_PMGR_W_dpm_hold(void)
{
    dpm_daemon_hold_flag = 1;

    /* Clear Data-Rx blocking */
    unset_wifi_driver_not_rcv();
}

void dpm_save_country_code(const char *country)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return;
    }

    memset((void *)RTM_SUPP_NET_INFO_PTR->country, 0, 4);
    memcpy((void *)RTM_SUPP_NET_INFO_PTR->country, country, 3);
}

void reset_dpm_info(void)
{
    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return;
    }

    RM_PMGR_W_dpm_hold();
    RM_PMGR_W_dpm_disable();
#if CFG_PMGR
    RM_WIFI_dpm_conn_info_clear();
#endif /* CFG_PMGR */

    // DNS
    // mdns
    // Http
}

void set_dpm_wakeup_condition(unsigned int type)
{
    FSP_PARAMETER_NOT_USED(type);
}

void dpm_accept_tim_arp_resp(unsigned long gw_ip, unsigned char * gw_mac)
{

#ifdef FOR_DEBUG
    {
    ULONG     tmp_ip;
    char buf[20];
    tmp_ip = ( (gw_ip>>24)&0xff)
            | ((gw_ip<<8)&0xff0000)
            | ((gw_ip>>8)&0xff00)
            | ((gw_ip<<24)&0xff000000);

    longtoip(tmp_ip, buf);
    printf(YELLOW_COLOR " [%s] etharp_add_static_entry: %s(0x%x) - %02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F":%02"X16_F"\n" CLEAR_COLOR,
              __func__, buf, gw_ip,
              (u16_t)gw_mac[0], (u16_t)gw_mac[1], (u16_t)gw_mac[2],
              (u16_t)gw_mac[3], (u16_t)gw_mac[4], (u16_t)gw_mac[5]);
    }
#endif

    /* ARP Response :: period 1, mode 2 */
    //dpm_set_arp(tmp_ip , (unsigned short *)gw_mac);

    /* For ARP Processing in TIM, set the GW MAC address */
    //dpm_set_env_arp_target_ip(GET_DPMP(), tmp_ip);
    //dpm_set_env_arp_target_mac(GET_DPMP(), (unsigned char *) gw_mac);
    romac4rtos_set_arp(gw_mac, gw_ip);

    /* For ARP request processing in TIM, set the GW MAC address */
    //dpm_set_env_arpreq_target_ip(GET_DPMP(), tmp_ip);
    //dpm_set_env_arpreq_target_mac(GET_DPMP(), (unsigned char *) gw_mac);
    romac4rtos_set_arpreq(gw_mac, gw_ip);

    return;
}

void dpm_print_regi_tcp_port(void)
{
    unsigned char index = 0;

    for(index = 0; index < DPM_MAX_TCP_FILTER ; index++) {
        PRINTF("tcp_port[%d] = %d\n", index, romac4rtos_get_tcport(index));
    }
}

void get_dpm_tcp_port_filter(void)
{
    unsigned char index = 0;
    unsigned short    reg_port = 0;
    
    for(index = 0; index < DPM_MAX_TCP_FILTER ; index++) {
        reg_port = romac4rtos_get_tcport(index);
        if(reg_port == 0) break;

        PRINTF(" [DPM] %s %dth Reg TCP Port(%d)\n", __func__, index, reg_port);
    }
    return;
}

void set_dpm_tim_tcp_chkport_enable()
{
    //struct dpm_param *dpmp = GET_DPMP();

    // Run ChkPort when the application calls dpm_setup
    //dpm_set_env_tcpka_chkport_en(dpmp);
    romac4rtos_set_tcpchken(1);
}

void dpm_reset_env_udph_enable(void)
{
    //dpm_reset_env_udph_en(GET_DPMP());
    romac4rtos_set_udphen(0);
}

/* DDPS related Setting */
#define DDPS_TIM_WAKEUP_DEFAULT    30

/* TIM Wakeup change as 3sec callback function */
void (*ddps_tim_wakeup_chg_3sec_cb)(void) = NULL;

/* TIM Wakeup change as 1sec callback function */
void (*ddps_tim_wakeup_chg_1sec_cb)(void) = NULL;

void ddps_wakeup_chg_noti_cb_reg(void (* user_1sec_cb)(void) , void (* user_3sec_cb)(void))
{
    ddps_tim_wakeup_chg_1sec_cb = user_1sec_cb;
    ddps_tim_wakeup_chg_3sec_cb = user_3sec_cb;
}

/** auto arp enable primitive
 */
static unsigned char dpm_cmd_autoarp_en;
unsigned char dpm_cmd_autoarp_period = 1;
void dpm_arp_en(int period, int mode)
{
    dpm_cmd_autoarp_en = mode;
    dpm_cmd_autoarp_period = period;
    //PRINTF("Auto ARP enabled(%d)\n", auto_arp_period);
}

/** ARP Resp timeout set Primitive
 */
void dpm_set_arp_resp(int bc_timeout /* us */)
{
#if 0    // TEMP_FOR_COMPILE
    struct dpm_param *dpmp = GET_DPMP();

    dpm_set_env_arpresp_timeout_tu(dpmp , bc_timeout);
#else
    FSP_PARAMETER_NOT_USED(bc_timeout);
#endif    // TEMP_FOR_COMPILE
}


#if 0    // TEMP_FOR_COMPILE
/** AP Sync Enable/Disable **/
static unsigned char main_ap_sync_cntrl = 1;        //AP Sync enable as default
void dpm_set_tim_ap_sync_cntrl(char    ap_sync_flag)
{
    /* TIM AP Sync Enable */
    if (ap_sync_flag) {
        dpm_set_aptrk_en(GET_DPMP(),     APTRK_MODE_COARSE_LOCK |
                       APTRK_MODE_FINE_LOCK |
                       APTRK_MODE_UNDER_TRACKING |
                       APTRK_MODE_OVER_TRACKING |
                       APTRK_MODE_SLOPE_MEASURE);
        main_ap_sync_cntrl = 0;
    } else  {    /* TIM AP Sync Diable */
        dpm_set_aptrk_en(GET_DPMP(), APTRK_MODE_DISABLE);
        main_ap_sync_cntrl = 1;
    }
}

unsigned char    dpm_get_tim_ap_sync_cntrl(void)
{
#if 0
    // AP Sync Reset value is 0, we can not use AP Sync enable flag
    return dpm_get_aptrk_en(GET_DPMP());
#else
    // If AP Sync is disabled as Command, return 0
    if (!main_ap_sync_cntrl)    return 0;
    else                        return 1;
#endif
}
#endif    // 0    // TEMP_FOR_COMPILE

USHORT get_dpm_tim_data_seqnum(void)
{
#if 0
    UINT32    conf_addr = DPM_TIM_DATA_SEQ_NUM;
    UINT16    tmp_seqnum , seqnum;

    tmp_seqnum = *(volatile USHORT *)(conf_addr);

    seqnum = (tmp_seqnum & 0xffff) << 4;
    //PRINTF("[UMAC Seq Num] ret_seqnum 0x%04x seqnum 0x%04x\n", ret_seqnum , seqnum);
    seqnum += 0x10;        // Main use it after plus
    return seqnum;
#else
    return 0;
#endif
}

int RM_PMGR_W_dpm_mode_get_from_nvram(void)
{
    int dpm_mode = 0;

    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        /* Unsupport RTM */
        return 0;
    }

#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                   ENV_GROUP_WIFIPROFILE,
                                   WIFI_PROFILE_ENABLE_DPM, &dpm_mode) != FSP_SUCCESS)
#endif
        RTM_FLAG_PTR->dpm_mode = 1; // by default, if 'dpm_mode' nvram item does not exist, enable dpm_mode
    else
        RTM_FLAG_PTR->dpm_mode = dpm_mode;

    return RTM_FLAG_PTR->dpm_mode;
}

/// DPM TIMER ////////////////////////////////////////////////////
//
#if (dg_configUSE_IDLE_TASK_TRIGGER == 1 ) || (dg_configUSE_TIMER_TRIGGER == 1)
#if (dg_configUSE_IDLE_TASK_TRIGGER == 1)
void dpm_trigger_fn()
#elif (dg_configUSE_TIMER_TRIGGER == 1)
void dpm_trigger_fn(unsigned long arg)
#else
#warning Not supported trigger function
#endif
{
    EventBits_t    dpm_ev_bits;
    FSP_PARAMETER_NOT_USED(arg);

    // Trigger when DPM daemon is running ...
    if (dpm_daemon_start_flag == pdTRUE) {
        /* Just return if abnormal MAC case occurs... */
        if (dpm_mac_err_flag == pdTRUE) {
            dpm_pdown_state = WAIT_DPM_SLEEP;
            dpm_mac_err_flag = pdFALSE;
            return;
        }

        /** If DPM Sleep Command is running, do not trigger **/
        if (RM_PMGR_W_dpm_sleep_is_started() != WAIT_DPM_SLEEP) {
            return;
        }

        if (ra6w1_dpm_ev_group != NULL) {
            dpm_ev_bits = xEventGroupSetBits(ra6w1_dpm_ev_group, DPM_DAEMON_TRIGGER);
            if ((dpm_ev_bits & DPM_DAEMON_TRIGGER) != DPM_DAEMON_TRIGGER) {
                PRINTF("[%s] event flag set error !!!\n", __func__);
            }
        }
    }
}
#endif /* (dg_configUSE_IDLE_TASK_TRIGGER == 1 ) || (dg_configUSE_TIMER_TRIGGER == 1) */

void rtc_timeout_cb(unsigned int *param)
{
    unsigned int timeout_id = *param;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (timeout_id >= DPM_TIMER_ERR) {
        PRINTF("[DPM_TM] rtc_timeout_cb error!!! (timeout_id=%d)\n", timeout_id);
        return;
    }

    RTM_FLAG_PTR->dpm_rtc_timeout_tid |= (1 << timeout_id);
    if (dpm_timer_proc_handler) {
        vTaskNotifyGiveFromISR(dpm_timer_proc_handler, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

unsigned int get_rtc_timeout_tid(void)
{
    return RTM_FLAG_PTR->dpm_rtc_timeout_tid;
}

unsigned int clr_rtc_timeout_tid(int tid)
{
    RTM_FLAG_PTR->dpm_rtc_timeout_tid &= ~(1 << tid);

    return pdTRUE;
}

int RM_PMGR_W_dpm_timer_create_by_tid(unsigned int msecs, char *name, int timer_id, int peri, void (* callback_func)(char *timer_name))
{
    dpm_timer_info_t *dpm_timer = NULL;
    int available_tid = DPM_TIMER_ERR;
    int registered_tid = DPM_TIMER_ERR;
    int task_name_len = 0;
    dpm_timer_param_t tparam;


    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE || timer_id == 1)
        return -1;

#ifdef DPM_TIMER_DEBUG
    PRINTF("[%s] try tid=%d msecs=%u\n\n", __func__, timer_id, msecs);
#endif // DPM_TIMER_DEBUG

    task_name_len = strlen(name);
    if ((REG_NAME_DPM_MAX_LEN < task_name_len) || (task_name_len < 0)) {
        PRINTF("[REG] Check the timer name_len(%d) (max len=%d)\n", task_name_len, REG_NAME_DPM_MAX_LEN);
        return -1;
    }

    if (msecs > TIMER_MAX_MSEC) {
        /* size of intiger */
        PRINTF("[REG] Time value overflow. (Max:%lu msecs)\n", (unsigned long)TIMER_MAX_MSEC);
        return -1;
    }

    /* Get mutex */
    if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE) {
        PRINTF("[REG] mutex_get error\n");
    }

    if (timer_id >= 2) {
        available_tid = timer_id;
    } else {
        available_tid = R_DPM_TIMER_EmptyIdGet();
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);

    /* Initialize */
    dpm_timer[available_tid - 2].timeout_callback = NULL;
    dpm_timer[available_tid - 2].msec = 0;
    memset((void *)dpm_timer[available_tid - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
    memset((void *)dpm_timer[available_tid - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);

    /* Register task name */
    strncpy(dpm_timer[available_tid - 2].task_name, name, task_name_len);
    strncpy(dpm_timer[available_tid - 2].timer_name, name, task_name_len);

    /* Register callback function */
    dpm_timer[available_tid - 2].timeout_callback = callback_func;

    /* Register reschedule time */
    if (peri > 0) {
        dpm_timer[available_tid - 2].msec = msecs;
    }

    /* Register to RTC */
    dpm_timer[available_tid - 2].tid = available_tid;

    tparam.callback_func = (void*)rtc_timeout_cb;
    tparam.callback_param = (void*)&(dpm_timer[available_tid - 2].tid);
    tparam.booting_offset = (void *)NULL;

    registered_tid = R_DPM_TIMER_SleepSet(available_tid, (((uint64_t)msecs)*1000), tparam, false);
#ifdef FOR_DEBUG
    printf("[%s] set timer(%s, msecs:%u, avail_tid:%d, reg_tid:%d)\n",
                    __func__, name, msecs, available_tid, registered_tid);
#endif

    if (xSemaphoreGive(dpm_timer_mutex) != pdTRUE) {
        PRINTF("[REG] mutex_put error !!!\n");
    }

    if ((registered_tid <= DPM_TIMER_0) || (registered_tid >= DPM_TIMER_ERR)) {
        PRINTF("[REG] Registration failed (name=<%s>, id=%d).\n", name, registered_tid);
        return -1;
    }

#ifdef DPM_TIMER_DEBUG
    PRINTF("[%s] Register tid=%d, name=%s(len=%d), msec=%d, fuc=0x%p\n\n",__func__,
        registered_tid,
        dpm_timer[available_tid - 2].task_name,
        strlen(dpm_timer[available_tid - 2].task_name),
        dpm_timer[available_tid - 2].msec,
        dpm_timer[available_tid - 2].timeout_callback);
#endif /* DPM_TIMER_DEBUG */
    return registered_tid;
}

int dpm_timer_find_timer_id(char *task_name, char *timer_name)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int task_name_len = 0;
    int timer_name_len = 0;
    int timer_idx = DPM_TIMER_2;

    if (RM_PMGR_W_rtm_exist() == pdFAIL) {
        return -1;
    }

    if (   (task_name == NULL)
        || (timer_name == NULL)
        || (strlen(task_name) <= 0)
        || (strlen(timer_name) <= 0)) {
        PRINTF("[FIND] Check timer_name !!!\n");
        return -1;
    }

    task_name_len = strlen(task_name);
    timer_name_len = strlen(timer_name);
    if (   (REG_NAME_DPM_MAX_LEN < task_name_len)
        || (task_name_len <= 0)
        || (DPM_TIMER_NAME_MAX_LEN < timer_name_len)
        || (timer_name_len <= 0)) {
        PRINTF("[FIND] Check timer_name (max len=%d)\n", DPM_TIMER_NAME_MAX_LEN);
        return -1;
    }

    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);

    while (timer_idx < DPM_TIMER_ERR) {
        if (   (strncmp(dpm_timer[timer_idx - 2].task_name, task_name, sizeof(dpm_timer[timer_idx - 2].task_name)) == 0)
            && (strncmp(dpm_timer[timer_idx - 2].timer_name, timer_name, sizeof(dpm_timer[timer_idx - 2].timer_name)) == 0)) {

            return timer_idx;
        }
        timer_idx++;
    }

    // Not found ... It's okay to create RTC timer with this name.
    return DPM_TIMER_ERR;
}

bool RM_PMGR_W_dpm_user_timer_is_soon_timeout(int soon_time)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int timer_id = DPM_TIMER_2;
    int get_time = 0;
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);

    while (timer_id < DPM_TIMER_ERR)
    {
        if (strlen(dpm_timer[timer_id - 2].task_name) > 0)
        {
            get_time = RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(timer_id);
            if (get_time > 0 && get_time <= soon_time)
            {
                return pdTRUE;
            }
        }
        timer_id++;
    }

    return pdFALSE;
}

void RM_PMGR_W_dpm_user_timer_list_print(void)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int timer_id = DPM_TIMER_5;
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);

    PRINTF("DPM Timer\n");
    PRINTF("---------------------------------------------------------\n");

    while (timer_id < DPM_TIMER_ERR) {
        if (strlen(dpm_timer[timer_id - 2].task_name) > 0) {

            PRINTF("TID : %d  \tName : %s / %s  \tRemain : %d\n",
                    timer_id,
                    dpm_timer[timer_id - 2].task_name,
                    dpm_timer[timer_id - 2].timer_name,
                    RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(timer_id));
        }

        timer_id++;
    }
    PRINTF("---------------------------------------------------------\n");

}

#define TIMER_DELETE_ERROR    0x11
int RM_PMGR_W_dpm_timer_delete_all(unsigned int level)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int tid = DPM_TIMER_5;
    int delete_tid = DPM_TIMER_ERR;
    int    ret = pdTRUE;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE)
        return pdFAIL;

    if (dpm_timer_mutex == NULL) {
        dpm_timer_mutex = xSemaphoreCreateMutex();

        if (dpm_timer_mutex == NULL) {
                PRINTF("\n[DPM_TM] dpm_timer_mutex create error !!!\n");
                return pdFAIL;
        }
    }

    if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE) {
        PRINTF("[DPM_TM] mutex_get error !!!\n");
    }
    
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
    if (dpm_timer == NULL)
        return pdFAIL;

#ifdef FOR_DEBUG
    PRINTF("[DPM_TM] Delete tid : %d ~ %d (lvl.%d). \r\n", DPM_TIMER_5, DPM_TIMER_ERR-1 , level);
#endif

    for (tid = DPM_TIMER_5; tid < DPM_TIMER_ERR; tid ++) {
        /* level == 1 : Delete only RTM data */
        /* level == 2 : Delete RTC and RTM */
        if (level > 1) {
            /* Delete RTC */
            delete_tid = R_DPM_TIMER_Kill(tid);
            if (delete_tid != tid) {
#ifdef FOR_DEBUG
                PRINTF("[%s] Timer deletion failed. (%d/%d)\n", __func__, delete_tid, tid);
#endif
                ret = TIMER_DELETE_ERROR;
                continue;
            }
        }

        /* Delete RTM saved data */
        dpm_timer[tid - 2].timeout_callback = NULL;
        dpm_timer[tid - 2].msec = 0;
        memset((void *)dpm_timer[tid - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
        memset((void *)dpm_timer[tid - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);
    }

    if (xSemaphoreGive(dpm_timer_mutex) != pdTRUE) {
        PRINTF("[DPM_TM] mutex_put error !!!\n");
    }

    return ret;
}

int RM_PMGR_W_dpm_timer_delete(char *task_name, char *timer_name)
{
    dpm_timer_info_t *dpm_timer = NULL;
    int exist_tid = DPM_TIMER_ERR;
    int delete_tid = DPM_TIMER_ERR;
    UINT status;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE)
        return -1;

    exist_tid = dpm_timer_find_timer_id(task_name, timer_name);
    if ((exist_tid >= DPM_TIMER_0) && (exist_tid < DPM_TIMER_ERR)) {
        RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
        if (dpm_timer == NULL)
            goto fail;

        status = xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY);
        if (status != pdTRUE) {
            PRINTF("[DPM_TM] mutex_get error(0x%02x)\n", status);
            goto fail;
        }

        /* Delete RTC */
        delete_tid = R_DPM_TIMER_Kill(exist_tid);

        status = xSemaphoreGive(dpm_timer_mutex);
        if (status != pdTRUE) {
            PRINTF("[DPM_TM] mutex_put error(0x%02x)\n", status);
        }

        if (delete_tid != exist_tid) {
#ifdef FOR_DEBUG
            PRINTF("[%s] Timer deletion failed. (%d/%d)\n", __func__, delete_tid, exist_tid);
#endif
            return -1;
        }

        /* Delete RTM saved data */
        dpm_timer[exist_tid - 2].timeout_callback = NULL;
        dpm_timer[exist_tid - 2].msec = 0;
        memset((void *) dpm_timer[exist_tid - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
        memset((void *) dpm_timer[exist_tid - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);

        return delete_tid; /* Success */
    } else if (exist_tid == DPM_TIMER_ERR) {
#ifdef FOR_DEBUG
        PRINTF("[DPM_TM] Timer not found. (%s/%s)\n", task_name, timer_name);
#endif /* FOR_DEBUG */
    }
fail:
#ifdef FOR_DEBUG
    PRINTF("[%s] Timer deletion failed. (%s/%s)\n", __func__, task_name, timer_name);
#endif /* FOR_DEBUG */
    return -1;
}

static int dpm_timer_process(unsigned int timeout_id)
{
    int registered_tm_id = DPM_TIMER_ERR;
    dpm_timer_info_t *dpm_timer = NULL;
    dpm_timer_param_t tparam;
    int ret;

    ret = RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**)&dpm_timer);
    if (ret != FSP_SUCCESS) {
        return pdFAIL;
    }

    if (dpm_timer == NULL) {
        PRINTF("[%s] Null pointer Error!!\r\n", __func__);
        return pdFAIL;
    }

    /* Do not sleep while this function working */
    dpm_rtc_to_chk_flag = pdTRUE;

    if (((int)timeout_id < DPM_TIMER_0) || ((int)timeout_id >= DPM_TIMER_ERR)) {
        /* No RTC timeout event. */
        dpm_rtc_to_chk_flag = pdFALSE;
        return pdPASS;
    }

#ifdef DPM_TIMER_DEBUG
    PRINTF("\n[%s] Timeout (id=%d, name=<%s>, remain_time_msec=%d cb:%p) \r\n\n",
                                    __func__,
                                    timeout_id,
                                    dpm_timer[timeout_id - 2].task_name,
                                    dpm_timer[timeout_id - 2].msec,
                                    dpm_timer[timeout_id - 2].timeout_callback);
#endif /* DPM_TIMER_DEBUG */

    /* Reset DPM timer tid only at first timer_process attempt */
    if (dpm_timer_abnormal_flag[timeout_id] == 0) {
        dpm_timer[timeout_id - 2].tid = DPM_TIMER_ERR;
    }

    /* Reschedule periodic timer only if it was not already rescheduled */
    if ((dpm_timer[timeout_id - 2].msec > 0) &&
        (dpm_timer[timeout_id - 2].tid == DPM_TIMER_ERR)) {
        if (xSemaphoreTake(dpm_timer_mutex, (TickType_t)portMAX_DELAY) != pdTRUE)
            PRINTF("[DPM_TM] mutex_get error !!!\n");

        /* Re-register to RTC */
        dpm_timer[timeout_id - 2].tid = timeout_id;

        tparam.callback_func = (void*)rtc_timeout_cb;
        tparam.callback_param = (void*)&(dpm_timer[timeout_id - 2].tid);
        tparam.booting_offset = (void *)NULL;

        registered_tm_id = R_DPM_TIMER_SleepSet(timeout_id, (uint64_t)(((uint64_t)(dpm_timer[timeout_id - 2].msec))*1000), tparam, false);
#ifdef FOR_DEBUG
        printf("[%s] set timer(%s, msecs:%u, reg_tid:%d)\n",
                                __func__,
                                dpm_timer[timeout_id - 2].task_name,
                                dpm_timer[timeout_id - 2].msec,
                                registered_tm_id);
#endif

        if (xSemaphoreGive(dpm_timer_mutex) != pdTRUE)
            PRINTF("[DPM_TM] mutex_put error !!!\n");

        if (registered_tm_id == DPM_TIMER_ERR) {
            PRINTF("[%s] TID=%d, Re-register failed\n", __func__, timeout_id);
        } else if (registered_tm_id != (int)timeout_id) {
            PRINTF("[%s] Re-register TID has been changed (%d->%d)\n", __func__, timeout_id, registered_tm_id);
        } else {
#ifdef DPM_TIMER_DEBUG
            PRINTF("[%s] Re-register success (%d, %s, sec=%d)\r\n",
                                __func__,
                                registered_tm_id,
                                dpm_timer[timeout_id - 2].task_name,
                                dpm_timer[timeout_id - 2].msec);
#endif /* DPM_TIMER_DEBUG */
        }
    }

    if (dpm_timer[timeout_id - 2].timeout_callback != NULL) {
        if (strlen(dpm_timer[timeout_id - 2].task_name) <= 0) {

            /* Call user callback */
            dpm_timer[timeout_id - 2].timeout_callback(dpm_timer[timeout_id - 2].timer_name);

#ifndef USE_SET_DPM_INIT_DONE
        } else if (dpm_timer_cb_ready(dpm_timer[timeout_id - 2].task_name) == 0) {
            dpm_timer[timeout_id - 2].timeout_callback(dpm_timer[timeout_id - 2].timer_name);
#endif /* USE_SET_DPM_INIT_DONE */
        } else if (RM_PMGR_W_dpm_wakeup_is_done(dpm_timer[timeout_id - 2].task_name) != 0) {
            /* Call user callback */
            dpm_timer[timeout_id - 2].timeout_callback(dpm_timer[timeout_id - 2].timer_name);
            if (r_dpm_wakeup_timer_id_get() == timeout_id && RM_PMGR_W_dpm_is_wakeup()) {
                RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
                r_dpm_wakeup_timer_id_set(DPM_TIMER_ERR);
            }
        } else {
            /* Wait "rtc_abnormal_cnt" times until user app is created. */
            dpm_timer_abnormal_flag[timeout_id] = dpm_timer_abnormal_flag[timeout_id] + 1;

            if (dpm_timer_abnormal_flag[timeout_id] > RTC_ABNORMAL_MAX_CNT) {
                PRINTF("[%s] '%s' is not ready. Callback can't be called. (%s/%d) \r\n",
                                                    __func__,
                                                    dpm_timer[timeout_id - 2].task_name,
                                                    dpm_timer[timeout_id - 2].timer_name,
                                                    timeout_id);

                dpm_timer_abnormal_flag[timeout_id] = 0;
            } else {
                /* Restore a TID that has not yet been called RM_PMGR_W_dpm_wakeup_done function. */
                RTM_FLAG_PTR->dpm_rtc_timeout_tid |= (1 << timeout_id);
                xTaskNotifyGive(dpm_timer_proc_handler);
#ifdef DPM_TIMER_DEBUG
                PRINTF("[%s] Callback abnormal TID=%d, Cnt=%d \r\n",
                                        __func__,
                                        timeout_id,
                                        dpm_timer_abnormal_flag[timeout_id]);
#endif /* DPM_TIMER_DEBUG */
            }
        }
    }

    if ((dpm_timer[timeout_id - 2].msec <= 0)
        && (dpm_timer[timeout_id - 2].tid == DPM_TIMER_ERR)
        && (dpm_timer_abnormal_flag[timeout_id] == 0)) {
#ifdef DPM_TIMER_DEBUG
        PRINTF("[%s] Clear RTM (name=%s, tid=%d)\n",
                                __func__,
                                dpm_timer[timeout_id - 2].task_name,
                                timeout_id);
#endif /* DPM_TIMER_DEBUG */

        /* Delete RTM saved data */
        dpm_timer[timeout_id - 2].timeout_callback = NULL;
        dpm_timer[timeout_id - 2].msec = 0;
        memset((void *)dpm_timer[timeout_id - 2].task_name, 0x00, REG_NAME_DPM_MAX_LEN);
        memset((void *)dpm_timer[timeout_id - 2].timer_name, 0x00, DPM_TIMER_NAME_MAX_LEN);
    }

    /* function working done */
    dpm_rtc_to_chk_flag = pdFALSE;

    return pdPASS;
}

static void dpm_timer_task(void * arg)
{
    int ret, tid_mask = 0, tid = -1;

    RA6W1_UNUSED_ARG(arg);

    while (pdTRUE) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        tid_mask = get_rtc_timeout_tid();
        while (tid_mask) {
            tid = ffs(tid_mask) - 1;
            /* Clear tid from bitmaps */
            tid_mask &= ~BIT(tid);
            clr_rtc_timeout_tid(tid);
            ret = dpm_timer_process(tid);

            if (ret != pdPASS)
                goto exit;
        }
    }

exit:
    printf("[%s] Terminated!! (%d)\r\n", __func__, ret);
    vSemaphoreDelete(dpm_timer_mutex);
    vTaskDelete(NULL);
}

unsigned int RM_PMGR_W_dpm_timer_task_create(void)
{
    if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION) &&
        (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
    {
        BaseType_t xReturned;

        /* dpm_timer_task */
        xReturned = xTaskCreate(dpm_timer_task,
                                DPM_TIMER_TASK_NAME,
                                DPM_TIMER_STK_SZ,
                                (void *)NULL,
                                DPM_TIMER_TASK_PRIORITY,
                                &dpm_timer_proc_handler);
        if (xReturned != pdPASS)
        {
            PRINTF("\n[DPM_TM] Failed to create dpm_timer_proc task (0x%lx)!!!\n", xReturned);
            return pdFAIL;
        }
    }

    /* Enable RTC interrupts */
    RM_PMGR_W_DpmTimerWakeupHandler();

    if (r_dpm_wakeup_timer_id_get() != DPM_TIMER_ERR && RM_PMGR_W_dpm_is_wakeup()) {
       RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
    }

    return pdTRUE;
}

#if 0 /*conflict with r_pm*/
int rtc_timer_info(int tid)
{
    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE)
        return -1;

    AD_DPM_TIMER_ID first_tid = AD_DPM_TIMER_ERR;
    AD_DPM_TIMER_ID next_tid = AD_DPM_TIMER_ERR;
    unsigned long first_msec = 0;
    unsigned long before_msec = 0;
    unsigned long next_msec = 0;
    int i = 0;

    ad_dpm_timer_t *gtimer = (ad_dpm_timer_t *)(AD_DPM_TIMER_BASE + (offsetof(ad_dpm_timer_map_t, timer)));
    static uint32_t *current_id = (uint32_t *)(AD_DPM_TIMER_BASE + offsetof(ad_dpm_timer_map_t,curIdx));
    #define curidx    *current_id

    if (curidx < AD_DPM_TIMER_ERR) {
        first_tid = (AD_DPM_TIMER_ID)curidx;
        first_msec = (unsigned int)(gtimer[first_tid].time / 1000);

        if (first_tid == tid) {
            return first_msec;
        }

        i = 0;
        next_tid = first_tid;

        before_msec = first_msec;
        while (i < AD_DPM_TIMER_ERR) {
            next_tid = (AD_DPM_TIMER_ID)gtimer[next_tid].type.content[DPM_IDX_NEXT];
            next_msec = (unsigned int)(gtimer[next_tid].time / 1000) + before_msec;
            if (next_tid == tid) {
                return next_msec;
            }

            if (next_tid >= AD_DPM_TIMER_ERR) {
                break;
            }

            i++;
            before_msec = next_msec;
        }
    }

    return 0;
}
#endif /*0*/

int RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(int tid)
{
	return rtc_timer_info(tid);
}

static unsigned int RM_PMGR_W_dpm_timer_reschedule_msec_get (int tid, char * job_name)
{
    dpm_timer_info_t * dpm_timer = NULL;
    unsigned int resched_msec = 0;
    
    RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_RTC_TIMER_PTR, NULL, NULL, (void**) &dpm_timer);

    if (strlen(dpm_timer[tid - 2].task_name) > 0)
    {
        if (strcmp(dpm_timer[tid - 2].task_name, job_name) == 0)
        {
           resched_msec = dpm_timer[tid - 2].msec;
        }
    }

    return resched_msec;
}

void rtc_timer_list_info(void)
{
    dpm_timer_id_t tid = DPM_TIMER_0;
    unsigned long     msec = 0;

    if (RM_PMGR_W_dpm_is_enabled() != pdTRUE) {
        PRINTF("Timer does not exist.\n");
        return;
    }

    /* Print timeout list */
    PRINTF("\nDPM Timer List (Non-real-time information)\n");
    print_separate_bar('-', 45, 1);
    PRINTF("ID       Expire Time  Descrition\n");
    print_separate_bar('=', 45, 1);
    while ((int)tid < DPM_TIMER_ERR) {
        msec = RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid((int)tid);

        PRINTF("%02d       %11lu  ", (int)tid, msec);
        
        switch ((int)tid) {
            case 2:
                PRINTF("<User Wake Up>");
                break;

            case 3:
                PRINTF("<DHCP Client>");
                break;

            case 4:
#ifndef __DISABLE_DPM_ABNORM__
                PRINTF("<Abnormal>");
#endif
                break;

            default:
                break;
        }

        PRINTF("\n");
        tid ++;
    }

    print_separate_bar('-', 45, 2);
}

void disable_all_dpm_timer(void)
{
#if (dg_configUSE_TIMER_TRIGGER == 1)
    if (dpm_trigger_timer != NULL)
        xTimerStop(dpm_trigger_timer, 0);
#endif    // dg_configUSE_TIMER_TRIGGER

    if (dpm_sts_chk_tm != NULL)
        xTimerStop(dpm_sts_chk_tm, 0);
}


///////////////////////////////////////////////////////////////////////////////
/////  For TCP Session on DPM  ////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if defined ( WORKING_FOR_DPM_TCP )
/*
 *****************************************************************************
 * Function : dpm_clr_tcp_ka_info
 *
 * - arguments
 *       sess_idx : Unique TCP session name for DPM
 *       rtc_timer_id : User RTC timer id for timeout on DPM
 *
 * - return : None
 *
 * - Discription :
 *       Clear TCP session information on RTM
 *
 *****************************************************************************
 */
void set_dpm_tcp_ka_info(char *tcp_sess_name, unsigned int rtc_timer_id)
{
}

int get_dpm_tcp_ka_info(char *tcp_sess_name)
{
    return 0;
}

void dpm_clr_tcp_ka_info(char *tcp_sess_name, unsigned int rtc_timer_id)
{
}

int dpm_chk_tcp_socket_transmit_state(void)
{
    return pdTRUE;
}

int dpm_chk_tcp_socket_receive_queue_state(void)
{
    return pdTRUE;
}
#endif    // WORKING_FOR_DPM_TCP

///////////////////////////////////////////////////////////////////////////////
///  End of APIs - DPM TCP Session handling  //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/// For APIs ////////////////////////////////////////////////
bool dpm_rxl_mpdu_to_umac(void)
{
    unsigned long dpm_status = RM_WIFI_dpm_ptim_event_get();
    bool is_mgmt_deauth_exist = false;

    if (CHK_PTIM_STATUS(dpm_status, RTOS4DPM_ST_DEAUTH)) {
        is_mgmt_deauth_exist = true;
	}

    return is_mgmt_deauth_exist;
}   

#if 0    // TO_DO
bool dpm_ops_tim_rx_handler()
{
    ke_evt_set(KE_EVT_RXREADY);
    return dpm_rxl_mpdu_to_umac();
}
#endif

/* Default Wakeup source is POR */
void RM_PMGR_W_dpm_wakeup_src_save( int wakeupsource )
{
	source = (bsp_wakeup_source_t)wakeupsource;
}

int chk_ext_int_exist(void)
{
	if (RM_PMGR_W_dpm_wakeup_src_get() & BSP_WAKEUP_SOURCE_GPIO)
		return pdTRUE;	// Exist

	return pdFALSE;	// No Exist
}

void do_printf(int do_print, const char * fmt, ...) 
{
    if (!do_print) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

fsp_err_t RM_PMGR_W_dpm_set_sleep() {

    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return FSP_ERR_UNSUPPORTED;
    }

    if (dpm_daemon_hold_flag) {
        return FSP_SUCCESS;
    }

    if (dpm_pdown_state > WAIT_DPM_SLEEP) {
        return FSP_SUCCESS;
    }

    /* TVR-2260 : If there is no wifi profile, dpm sleep does not occur */
    if ((dpm_supp_conn_info->ssid_len <= 0) || (dpm_supp_conn_info->disabled == 1)) {
        return FSP_SUCCESS;
    }

    if (dpm_sleep_daemon_handler && dpm_lld_task_is_ready) {
        dpm_sleep_ready_flag = pdTRUE;
        xTaskNotify(dpm_sleep_daemon_handler, EVT_MPM_DPM_SLEEP_RDY, eSetBits);
    }

    return FSP_SUCCESS;
}

#if CFG_PMGR
int RM_PMGR_W_dpm_sleep_is_started(void)
{
    return dpm_pdown_state;
}
#endif /* CFG_PMGR */

BSP_PLACE_CODE_IN_RAM fsp_err_t RM_PMGR_W_DpmTimerWakeupHandler()
{
    R_DPM_TIMER_WakeupHandler();
    return FSP_SUCCESS;
}

static int RM_PMGR_W_dpm_info_lock_create(void)
{
    if (!dpm_sleep_info_lock) {
        dpm_sleep_info_lock = xSemaphoreCreateMutex();

        if (dpm_sleep_info_lock == NULL) {
            printf("[%s] Faild to create a mutex !\n", __func__);
            return pdFAIL;
        }
    }

    return pdPASS;
}

int RM_PMGR_W_dpm_info_lock_take(unsigned int timeout)
{
    int ret = 0;

    if (!dpm_sleep_info_lock) {
        if (!RM_PMGR_W_dpm_info_lock_create()) {
            return pdFAIL;
        }
    }

    ret = xSemaphoreTake(dpm_sleep_info_lock, timeout);

    if (ret != pdTRUE) {
        printf("[%s] Failed to take mutex(%d) \n", __func__, ret);
        return pdFAIL;
    }

    return pdPASS;
}

int RM_PMGR_W_dpm_info_lock_give(void)
{
    int ret = 0;

    if (!dpm_sleep_info_lock) {
        if (!RM_PMGR_W_dpm_info_lock_create()) {
            return pdFAIL;
        }
    }    

    ret = xSemaphoreGive(dpm_sleep_info_lock);

    if (ret != pdTRUE) {
        printf("[%s] Failed to give mutex(%d)\n", __func__, ret);
        return pdFAIL;
    }

    return pdPASS;
}

static void RM_PMGR_W_dpm_user_wakeup_timer_callback(char * timer_name)
{
    if (!RM_PMGR_W_dpm_is_wakeup() || RM_PMGR_W_dpm_sleep_is_hold())
    {
        return;
    }

    RM_PMGR_W_dpm_sleep_ready_clear(USER_WAKEUP_T_NAME);

    printf(CYAN_COLOR "\n\t < USER WAKE UP : %s > \n\n" CLEAR_COLOR, timer_name);

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    RM_PMGR_W_dpm_sleep_ready_set(USER_WAKEUP_T_NAME);
}

fsp_err_t RM_PMGR_W_dpm_user_wakeup_timer_set (int user_wu_time)
{
    fsp_err_t ret = FSP_SUCCESS;

    if (!RM_PMGR_W_dpm_is_enabled())
    { 
        return FSP_ERR_INVALID_MODE;
    }

    if (user_wu_time < 0)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (user_wu_time == 0)
    {

        RM_PMGR_W_dpm_timer_delete(USER_WAKEUP_T_NAME, USER_WAKEUP_T_NAME);
        
#ifdef RM_MAP_PERSISTANT_W
        ret = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, 
                                WIFI_PROFILE_DPM_USER_WAKEUP_TIME);
#endif /* RM_MAP_PERSISTANT_W */
        RM_PMGR_W_dpm_job_name_clear(USER_WAKEUP_T_NAME);

        return ret;
    }

    /* Delete timer (TID_U_USER_WAKEUP) first if existing */
    RM_PMGR_W_dpm_timer_delete(USER_WAKEUP_T_NAME, USER_WAKEUP_T_NAME);

#ifdef RM_MAP_PERSISTANT_W
    ret = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_DPM_USER_WAKEUP_TIME, user_wu_time); 
#endif /* RM_MAP_PERSISTANT_W */

    RM_PMGR_W_dpm_timer_create_by_tid(user_wu_time, USER_WAKEUP_T_NAME, TID_U_USER_WAKEUP, 1,
                                                RM_PMGR_W_dpm_user_wakeup_timer_callback);
#ifdef FOR_DEBUG
    printf(YELLOW_COLOR " [%s] Register timer id: %d, msec: %d \n" CLEAR_COLOR,
                   __func__, TID_U_USER_WAKEUP, user_wu_time);
#endif
    RM_PMGR_W_dpm_job_name_set(USER_WAKEUP_T_NAME, 0);
    RM_PMGR_W_dpm_wakeup_done(USER_WAKEUP_T_NAME);
    RM_PMGR_W_dpm_sleep_ready_set(USER_WAKEUP_T_NAME);

    return ret;
}

void RM_PMGR_W_dpm_user_wakeup_timer_init (void)
{
    int user_wu_time = 0, reschedule_time = 0;

    if (!RM_PMGR_W_dpm_is_enabled())
    {
        return;
    }

    if (!RM_PMGR_W_dpm_is_wakeup())
    {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_DPM_USER_WAKEUP_TIME, &user_wu_time);
        if (user_wu_time == -1)
        {
            user_wu_time = DFLT_DPM_USER_WAKEUP_TIME;
        }
#else
        user_wu_time = DFLT_DPM_USER_WAKEUP_TIME;
#endif /* RM_MAP_PERSISTANT_W */

        if (user_wu_time > 0)
        {
            RM_PMGR_W_dpm_timer_create_by_tid(user_wu_time, USER_WAKEUP_T_NAME, TID_U_USER_WAKEUP, 1,
                                                            RM_PMGR_W_dpm_user_wakeup_timer_callback);
#ifdef FOR_DEBUG
            printf(YELLOW_COLOR " [%s] Register timer id: %d, msec: %d \n" CLEAR_COLOR, 
                    __func__, TID_U_USER_WAKEUP, user_wu_time);
#endif /* FOR_DEBUG */
        }
    }
    else
    {
        reschedule_time = RM_PMGR_W_dpm_timer_reschedule_msec_get(TID_U_USER_WAKEUP, USER_WAKEUP_T_NAME);
    }

    if (user_wu_time > 0 || reschedule_time > 0)
    {
        RM_PMGR_W_dpm_job_name_set(USER_WAKEUP_T_NAME, 0);
        RM_PMGR_W_dpm_wakeup_done(USER_WAKEUP_T_NAME);
#ifdef FOR_DEBUG
        printf(YELLOW_COLOR " [%s] Init done: %s \n" CLEAR_COLOR, __func__, USER_WAKEUP_T_NAME);
#endif /* FOR_DEBUG */
        RM_PMGR_W_dpm_sleep_ready_set(USER_WAKEUP_T_NAME);
    }

    return;
}

#endif /* CFG_WIFI */

#endif /* CFG_PMGR */
