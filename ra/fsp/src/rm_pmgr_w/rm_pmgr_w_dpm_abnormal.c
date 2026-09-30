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
#include "rm_wifi.h"

#define __CHK_NETWORK_TRAFFIC__

#include "common_def.h"
#include "iface_defs.h"

#include <stdio.h>
#include "sys_app_defs.h"
#include "nvedit.h"
#include "common/defs.h"

#include "timers.h"
#include "r_pm_if.h"

#include "limits.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#include "rm_pmgr_w_instance.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define PRINTF(...) printf(__VA_ARGS__)

#undef	DBG_PRINT_INFO
#define DBG_PRINT_ERROR
#define DBG_PRINT_TEMP

#define	PERIOD_STS_CHK_TIME			1000	// 1sec reschedule_ticks

#define	DPM_STS_CHK_NAME			"dpm_sts_chk"

#define	MAX_NET_INIT_TIME			60
#define	MAX_INIT_WIFI_CONN_TIME		30
#define	MAX_DHCP_RENEW_TIME			30
#define	MAX_ARP_WAIT_TIME			5
#define	MAX_POWER_DOWN_TIME			30
#define	MAX_USER_APP_WAIT_TIME		30

#define	DPM_ABNORM_ACT_1	1	// WIFI Connect fail
#define	DPM_ABNORM_ACT_2	2	// DHCP Renew fail
#define	DPM_ABNORM_ACT_3	3	// ARP response fail
#define	DPM_ABNORM_ACT_4	4	// DPM Power down fail (30 Sec)
#define	DPM_ABNORM_ACT_5	5	// No data for 5 seconds and DPM is ready
#define	DPM_ABNORM_ACT_6	6	// DPM application fail


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/



/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/* Global extern functions */
extern UCHAR *get_dpm_unsleep_task_names(void);
extern int get_netmode(int iface);
extern int RM_PMGR_W_dpm_mode_get(void);
extern int check_net_init(int iface);
extern void show_dpm_unsleep_task_names(void);
extern int is_dpm_sleep_ready(void);
extern int RM_PMGR_B_dpm_wakeup_src_get(void);
extern int RM_PMGR_B_dpm_timer_delete_by_tid(int timer_id);
extern bool RM_PMGR_B_rtm_exist(void);

extern void    (*dpmDaemonSleepFunction)();
#if defined (__SUPPORT_ATCMD__)
extern void PRINTF_ATCMD(const char *fmt,...);
#endif

#ifdef BCWU_SLEEP_DOWN_MODE
extern void fc80211_fc9k_sec_pwr_down(unsigned long long usec , bool retention);
#endif	//BCWU_SLEEP_DOWN_MODE


extern unsigned long ra6wx_get_wakeup_source();
extern UINT get_current_arp_req_status(void);
extern u8 wpa_supp_wps_in_use(void);
extern unsigned int ra6w1_network_main_check_ip_addr(unsigned char iface);

/* Global external variables */

void force_dpm_abnormal_sleep_by_wifi_conn_fail(void);
void force_dpm_abnormal_sleep1(void);
dpm_monitor_info_t *get_dpm_monitor_info_ptr(void);
void dpm_monitor_stop(void);
void dpm_monitor_restart(void);
void dpm_monitor_cleanup(void);
void dpm_show_dpm_monitor_info(void);
void dpm_show_dpm_monitor_count(void);
void dpm_save_dpm_tim_status(int tim_status);
void dpm_save_dpm_error_code(int act_type);
int RM_PMGR_W_dpm_abnormal_wifi_conn_retry_cnt_get(void);
int dpm_decr_wifi_conn_retry_cnt(void);
int set_dpm_abnormal_wait_time(int time, int mode);
UCHAR get_last_abnormal_act(void);
UCHAR get_last_abnormal_cnt(void);
void save_dpm_sleep_type(UCHAR type);
void start_dpm_sts_chk_timer(int dpm_wu_type);
void dpm_abnormal_chk_hold(void);
void dpm_abnormal_chk_resume(void);
int size_of_dpm_monitor_info_t();

/* Global Local variables */
const unsigned long long dpm_monitor_retry_interval[DPM_MON_RETRY_CNT] = {
	30,
	60,
	60,
	60,
	60 * 30,
	60 * 30,
	60 * 30,
	60 * 60,
	60 * 60,
	0xDEADBEEF //to stop wakeup, go to infinite sleep
};

unsigned long long *dpm_abnorm_user_wakeup_interval = (unsigned long long *)NULL;

TimerHandle_t   dpm_sts_chk_tm = NULL;
dpm_monitor_info_t *dpm_monitor_info_ptr = NULL;


#if defined ( __CHK_NETWORK_TRAFFIC__ )
extern unsigned int dpm_get_net_traffic_rx_cnt(void);
extern unsigned int dpm_get_net_traffic_tx_cnt(void);
static ULONG old_dpm_tx_pck_cnt, old_dpm_rx_pck_cnt;
static int idle_time = 0;
#endif	// __CHK_NETWORK_TRAFFIC__

/* Local static variables */
static int dpm_connection_fail_cnt	= 0;
static int dpm_dhcp_no_response_cnt	= 0;
static int dpm_arp_no_response_cnt	= 0;
static int dpm_power_down_fail_cnt	= 0;
static int dpm_unknown_fail_cnt		= 0;
static int dpm_monitor_stopped		= 0;
static int dpm_abnorm_chk_hold_flag = 0;
static int dpm_abnorm_sleep1_flag 	= 0;
static int dpm_wifi_conn_retry_cnt	= 0;

void force_dpm_abnormal_sleep_by_wifi_conn_fail(void)
{
	dpm_connection_fail_cnt = MAX_INIT_WIFI_CONN_TIME-1;
}

void force_dpm_abnormal_sleep1(void)
{
	dpm_abnorm_sleep1_flag = 1;
}

dpm_monitor_info_t *get_dpm_monitor_info_ptr(void)
{
	if (RM_PMGR_W_rtm_exist() == pdFALSE) {
		/* Unsupport RTM */
		return NULL;
	}

	return (void *)RTM_DPM_MONITOR_PTR;
}

void dpm_monitor_stop(void)
{
	dpm_monitor_stopped = 1;
	PRINTF("[DPM_Mon] STOP\n");
}

void dpm_monitor_restart(void)
{
	dpm_monitor_stopped = 0;
	PRINTF("[DPM_Mon] Restart\n");
}

void dpm_monitor_cleanup(void)
{
	dpm_power_down_fail_cnt = 0;
	dpm_connection_fail_cnt = 0;
	dpm_dhcp_no_response_cnt = 0;
	dpm_arp_no_response_cnt = 0;
	dpm_unknown_fail_cnt = 0;
#if defined ( __CHK_NETWORK_TRAFFIC__ )
	idle_time = 0;
#endif	// __CHK_NETWORK_TRAFFIC__
}

void dpm_show_dpm_monitor_info(void)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	if (dpm_monitor_info_ptr->wifi_conn_wait_time > 0) {
		PRINTF("DPM_Mon will enter SLEEP Mode-2 after %d secs\n",
				dpm_monitor_info_ptr->wifi_conn_wait_time);
	} else {
		PRINTF("DPM_Mon will enter SLEEP Mode-2 after\n"
					"5 secs due to ARP no response\n"
					"60 seds due to DHCP no response\n"
					"30 secs due to any other reasons.\n");
	}
}

void dpm_show_dpm_monitor_count(void)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();
	int total_cnt = 0;

	if (RM_PMGR_W_rtm_exist () == pdFALSE) {
		return;
	}

	PRINTF("dpm_monitor_info_ptr = %p\n", dpm_monitor_info_ptr);

	PRINTF("\n");
	PRINTF("=========================================================\n");
	PRINTF("Wakeup Type\t TIM Status\t Counts from last power on \n");
	PRINTF("=========================================================\n");

	total_cnt = dpm_monitor_info_ptr->tim_count[6] + dpm_monitor_info_ptr->tim_count[7] +
			dpm_monitor_info_ptr->tim_count[9] + dpm_monitor_info_ptr->tim_count[11];

	PRINTF("0\t\tDPM_FROM_KEEP_ALIVE\t%ld\n", dpm_monitor_info_ptr->tim_count[6]);
	PRINTF("\t\tDPM_NO\t\t\t%ld\n", dpm_monitor_info_ptr->tim_count[7]);
	PRINTF("\t\tDPM_AP_RESET\t\t%ld\n", dpm_monitor_info_ptr->tim_count[9]);
	PRINTF("(total:%d)\tDPM_DETECTED_STA\t%ld\n", total_cnt, dpm_monitor_info_ptr->tim_count[11]);
	PRINTF("---------------------------------------------------------\n");

	PRINTF("1\t\tDPM_FROM_FAST\t\t%ld\n", dpm_monitor_info_ptr->tim_count[4]);
	PRINTF("(total:%ld)\t\t\t\t\n", dpm_monitor_info_ptr->tim_count[4]);
	PRINTF("---------------------------------------------------------\n");

	total_cnt = dpm_monitor_info_ptr->tim_count[12] + dpm_monitor_info_ptr->tim_count[13];

	PRINTF("2\t\tDPM_USER0\t\t%ld\n", dpm_monitor_info_ptr->tim_count[12]);
	PRINTF("(total:%d)\tDPM_USER1\t\t%ld\n", total_cnt, dpm_monitor_info_ptr->tim_count[13]);
	PRINTF("---------------------------------------------------------\n");

	total_cnt = dpm_monitor_info_ptr->tim_count[3] + dpm_monitor_info_ptr->tim_count[5] + dpm_monitor_info_ptr->tim_count[10];

	PRINTF("3\t\tDPM_NO_BCN\t\t%ld\n", dpm_monitor_info_ptr->tim_count[3]);
	PRINTF("\t\tDPM_KEEP_ALIVE_NO_ACK\t%ld\n", dpm_monitor_info_ptr->tim_count[5]);
	PRINTF("(total:%d)\tDPM_DEAUTH\t\t%ld\n", total_cnt, dpm_monitor_info_ptr->tim_count[10]);
	PRINTF("---------------------------------------------------------\n");

	total_cnt = dpm_monitor_info_ptr->tim_count[0] + dpm_monitor_info_ptr->tim_count[8];

	PRINTF("4\t\tDPM_UC\t\t\t%ld\n", dpm_monitor_info_ptr->tim_count[0]);
	PRINTF("(total:%d)\tDPM_UC_MORE\t\t%ld\n", total_cnt, dpm_monitor_info_ptr->tim_count[8]);
	PRINTF("---------------------------------------------------------\n");

	total_cnt = dpm_monitor_info_ptr->tim_count[1] + dpm_monitor_info_ptr->tim_count[2];

	PRINTF("5\t\tDPM_BC_MC\t\t%ld\n", dpm_monitor_info_ptr->tim_count[1]);
	PRINTF("(total:%d)\tDPM_BCN_CHANGED\t\t%ld\n", total_cnt, dpm_monitor_info_ptr->tim_count[2]);
	PRINTF("=========================================================\n");

	PRINTF("Error Code\t\t\t\t\t\t\n");
	PRINTF("=========================================================\n");
	PRINTF("101(Assoc/Auth Failed)\t\t\t%ld\n", dpm_monitor_info_ptr->error_count[1]);
	PRINTF("---------------------------------------------------------\n");
	PRINTF("102(NO Response from DHCP Server)\t%ld\n", dpm_monitor_info_ptr->error_count[2]);
	PRINTF("---------------------------------------------------------\n");
	PRINTF("103(No ARP Response)\t\t\t%ld\n", dpm_monitor_info_ptr->error_count[3]);
	PRINTF("---------------------------------------------------------\n");
	PRINTF("104(DPM Power Down Failed)\t\t%ld\n", dpm_monitor_info_ptr->error_count[4]);
	PRINTF("---------------------------------------------------------\n");
	PRINTF("105(Unknown Error)\t\t\t%ld\n", dpm_monitor_info_ptr->error_count[5]);
	PRINTF("---------------------------------------------------------\n");
	PRINTF("106(Application Failed to set DPM)\t%ld\n", dpm_monitor_info_ptr->error_count[6]);
	PRINTF("=========================================================\n");
	PRINTF("Last Act\t\t\t\t%d\n", dpm_monitor_info_ptr->last_abnormal_type);
	PRINTF("Last Count\t\t\t\t%d\n", dpm_monitor_info_ptr->last_abnormal_count);

}

void dpm_save_dpm_tim_status(int tim_status)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_UPLOAD)) {
        dpm_monitor_info_ptr->tim_count[0]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_BCN_CHANGED)) {
        dpm_monitor_info_ptr->tim_count[2]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_NOBCN)) {
        dpm_monitor_info_ptr->tim_count[3]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_FROM_FAST)) {
        dpm_monitor_info_ptr->tim_count[4]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_NOACK)) {
        dpm_monitor_info_ptr->tim_count[5]++;
    }
    if (CHK_PTIM_STATUS_UNDEF(tim_status)) {
        dpm_monitor_info_ptr->tim_count[7]++;
    }
    if (CHK_PTIM_STATUS(tim_status, DPM_DEAUTH)) {
        dpm_monitor_info_ptr->tim_count[10]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_FB0)) {
        dpm_monitor_info_ptr->tim_count[12]++;
    }
    if (CHK_PTIM_STATUS(tim_status, RTOS4DPM_ST_FB0)) {
        dpm_monitor_info_ptr->tim_count[13]++;
    }
}

/* error code
 *		101 : Connection Fail
 *		102 : DHCP Fail
 *		103 : ARP Fail
 *		104 : Power Down Fail
 *		105 : Unknown Fail
 */
void dpm_save_dpm_error_code(int act_type)
{
	int count = dpm_monitor_info_ptr->error_count[act_type];

	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	dpm_monitor_info_ptr->error_count[act_type] = count + 1;
}

int RM_PMGR_W_dpm_abnormal_wifi_conn_retry_cnt_get(void)
{
	if (dpm_monitor_info_ptr != NULL) {
		return dpm_monitor_info_ptr->wifi_conn_retry_cnt;
	} else {
		return 0;
	}
}

int dpm_decr_wifi_conn_retry_cnt(void)
{
	return --dpm_wifi_conn_retry_cnt;
}

int set_dpm_abnormal_wait_time(int time, int mode)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	if (mode == DPM_ABNORM_DPM_WIFI_RETRY_CNT) {
		if (time < 0 || time > 6) {
			PRINTF("\nError : out of range [0 .. 6 times]\n\n");
			return -1;
		}
	} else {
		/* Range 10 secs ~ 60 secs */
		if (time < 10 || time > 60) {
			PRINTF("\nError : Wrong setting time [10 .. 60 secs]\n\n");
			return -1;
		}
	}

	/* Permit NVRAM write when first POR boot ...
	 * Else case,, it means on DPM operation and block NVRAM Write operation */
	if (RM_PMGR_W_dpm_is_enabled() && !RM_PMGR_W_dpm_is_wakeup()) {
		switch (mode)
		{
			case DPM_ABNORM_WIFI_CONN_WAIT	:
#ifdef RM_MAP_PERSISTANT_W
				RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                            ENV_GROUP_WIFICFG,
                                                            WIFI_CONN_WAIT_NAME,
                                                            time);
#endif
				break;

			case DPM_ABNORM_DHCP_RSP_WAIT	:
#ifdef RM_MAP_PERSISTANT_W
				RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                            ENV_GROUP_WIFICFG,
                                                            DHCP_RSP_WAIT_NAME, time);
#endif
				break;

			case DPM_ABNORM_ARP_RSP_WAIT	:
#ifdef RM_MAP_PERSISTANT_W
				RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                            ENV_GROUP_WIFICFG,
                                                            ARP_RSP_WAIT_NAME, time);
#endif
				break;

			case DPM_ABNORM_DPM_FAIL_WAIT	:
#ifdef RM_MAP_PERSISTANT_W
				RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                            ENV_GROUP_WIFICFG,
                                                            UNKNOWN_DPM_FAIL_WAIT_NAME,
                                                            time);
#endif
				break;

			case DPM_ABNORM_DPM_WIFI_RETRY_CNT	:
#ifdef RM_MAP_PERSISTANT_W
				RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                            ENV_GROUP_WIFICFG,
                                                            WIFI_CONN_RETRY_CNT_NAME,
                                                            time);
#endif
				break;
		}
	} else if (!RM_PMGR_W_dpm_is_enabled() && mode == DPM_ABNORM_DPM_WIFI_RETRY_CNT) {
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            WIFI_CONN_RETRY_CNT_NAME,
                                            time);
#endif
	}

	switch (mode)
	{
		case DPM_ABNORM_WIFI_CONN_WAIT	:
			dpm_monitor_info_ptr->wifi_conn_wait_time = time;
			PRINTF("- Wi-Fi connection waiting time = %d sec\n", time);
			break;

		case DPM_ABNORM_DHCP_RSP_WAIT	:
			dpm_monitor_info_ptr->dhcp_rsp_wait_time = time;
			PRINTF("- DHCP response waiting time = %d sec\n", time);
			break;

		case DPM_ABNORM_ARP_RSP_WAIT	:
			dpm_monitor_info_ptr->arp_rsp_wait_time = time;
			PRINTF("- ARP response waiting time = %d sec\n", time);
			break;

		case DPM_ABNORM_DPM_FAIL_WAIT	:
			dpm_monitor_info_ptr->unknown_dpm_fail_wait_time = time;
			PRINTF("- DPM fail waiting time = %d sec\n", time);
			break;

        case DPM_ABNORM_DPM_WIFI_RETRY_CNT  :
            dpm_wifi_conn_retry_cnt = time;

            if (dpm_monitor_info_ptr != NULL) {
                dpm_monitor_info_ptr->wifi_conn_retry_cnt = time;
            }

            if (dpm_wifi_conn_retry_cnt == 0) {
                PRINTF("- DPM wifi conn retry count = default action \n");
			} else {
                PRINTF("- DPM wifi conn retry count = %d time(s)\n", dpm_wifi_conn_retry_cnt);
            }
            break;
        default :
            break;
    }

    return 0;
}

UCHAR get_last_abnormal_act(void)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	return dpm_monitor_info_ptr->last_abnormal_type;
}

UCHAR get_last_abnormal_cnt(void)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	return dpm_monitor_info_ptr->last_abnormal_count;
}

static void save_abnormal_act(UCHAR act, UCHAR cnt)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	dpm_monitor_info_ptr->last_abnormal_type = act;
	dpm_monitor_info_ptr->last_abnormal_count = cnt;

#ifdef DBG_PRINT_INFO
	PRINTF(CYN_COL "[%s] act = %d cnt = %d\n" CLR_COL, __func__, act, cnt);
#endif
}

void save_dpm_sleep_type(UCHAR type)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	dpm_monitor_info_ptr->last_sleep_type = type;
}

UCHAR RM_PMGR_W_dpm_wakeup_is_abnormal(void)
{
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();
#ifdef FOR_DEBUG
	printf("[%s], last sleep type = %d\n",__func__, dpm_monitor_info_ptr->last_sleep_type);
#endif
	return dpm_monitor_info_ptr->last_sleep_type;
}

static fsp_err_t dpm_notify_abnormal_status(int act_type) {
	switch(act_type) {
		case DPM_ABNORM_ACT_1:	// WIFI Connect fail
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_WIFI_DISCONN);
		case DPM_ABNORM_ACT_2:	// DHCP Renew fail
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_DHCP_RENEW_FAIL);
		case DPM_ABNORM_ACT_3:	// ARP response fail
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_ARP_RESP_FAIL);
		case DPM_ABNORM_ACT_4:	// DPM Power down fail (30 Sec)
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_DPM_POWER_DOWN_FAIL);
		case DPM_ABNORM_ACT_5:	// No data for 5 seconds and DPM is ready
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_NO_DATA);
		case DPM_ABNORM_ACT_6:	// DPM application fail
			return RM_PMGR_W_dpm_notify(PMGR_W_DPM_EVENT_ABNORMAL_DPM_APP_FAIL);
		default:
			break;
	}

	return FSP_ERR_INVALID_STATE;
}

/*
 * In this function, most of the print/PRINTF/lowPrintf does not run normally
 * because the RA6W1 enter to Sleep Mode 2 before finishing print operation.
 *
 * If wants to shown print log message on console,
 * fc80211_fc9k_sec_pwr_down() should be blocked.
 * When a print statement is entered, the output is reduced due to processing delay.
 * vTaskDelay() does not work because it is a function called by the timer.
 * static void dpm_abnormal_next_action(int act_type)
 */
static void dpm_abnormal_next_action(int act_type)
{
	UCHAR last_cnt = 0;
	UCHAR current_cnt = 0;

	unsigned long long uTimeValue = 0;

	/*
	 * To printout New-Line character
	 * Don't printout a long message.
	 * The power will be down in short time
	 * ( It is not suffician to pritnout for long message. )
	 */

	last_cnt = get_last_abnormal_cnt(); /* Total count of consecutive Abrnomal actions */

	if (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_GPIO_WITH_RETENTION)
	    && RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_ABNORMAL) > MAX_INIT_WIFI_CONN_TIME
		) {
		/* Fix Abnormal event count by External Wakeup button event */
		current_cnt = last_cnt;
	} else {
		current_cnt = last_cnt + 1;
	}

	if (current_cnt > DPM_MON_RETRY_CNT) {
		current_cnt--;
	}

	save_abnormal_act(act_type, current_cnt);

	dpm_save_dpm_error_code(act_type);

	if (act_type == DPM_ABNORM_ACT_6) { //User application error
#ifdef DBG_PRINT_INFO
		PRINTF(YEL_COL "!!! Unable to enter DPM Sleep Mode !!! Please check following threads.\n" CLR_COL);
#endif
		show_dpm_unsleep_task_names();
		//set_dpm_all_flags();	/* for futher study */
	} else {

/* All DPM Module is set, so power-down will be started.
 * During Aging , no wakeup issue happed */

		/* Need to hold the DPM daemon operation to handle abnormal sequences */
		RM_PMGR_W_dpm_hold();

		save_dpm_sleep_type(1);
#ifdef DBG_PRINT_INFO
		PRINTF("Abnormal DPM operation.\n");
#endif /* DBG_PRINT_INFO */

		if (RM_PMGR_W_dpm_wakeup_src_get() == (BSP_WAKEUP_GPIO_WITH_RETENTION)
		    && RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_ABNORMAL) > MAX_INIT_WIFI_CONN_TIME
			) { /* Correct wakeup count when Wakeup-Button */
			uTimeValue = (RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_ABNORMAL) - MAX_INIT_WIFI_CONN_TIME) * 1000000ULL;
		} else {
#ifndef __BLE_COMBO_REF__
			if (dpm_abnorm_user_wakeup_interval == NULL) {
				/* Use default wakeup interval */
				uTimeValue = dpm_monitor_retry_interval[current_cnt-1] * 1000000ULL;

				/* Abnormal Setting Time should be checked one more */
				if (uTimeValue < 1000000ULL || uTimeValue > 60 * 60 *1000000ULL) {
					// overflow ...
					if (uTimeValue / 1000000ULL != 0xDEADBEEF) { // skip for sleep1 condition
						PRINTF("Abnormal Time value (cnt %d) is wrong, so set as last abnormal time (60 min)\n", current_cnt);
						uTimeValue = (unsigned long long)(60 * 60 * 1000000ULL);
					}
				}
			} else {
				uTimeValue = dpm_abnorm_user_wakeup_interval[current_cnt-1] * 1000000ULL;

				/* rtc uses 32khz clock and 36 bits wide register, 
						so a value until 0x1FFFFF sec (== 24 days) is allowed */ 
				if (uTimeValue < 1000000ULL || uTimeValue > 0x1FFFFF * 1000000ULL) {
					// overflow ...
					if (uTimeValue / 1000000ULL != 0xDEADBEEF) { // skip for sleep1 condition
						PRINTF("Abnormal Time value (cnt %d) is wrong, so set as (60 min) \n", current_cnt);
						uTimeValue = (unsigned long long)(60ULL * 60ULL * 1000000ULL);
					}
				}
			}
#endif /* ! __BLE_COMBO_REF__ */
		}

#ifdef DBG_PRINT_INFO
		printf(" [%s] uTimeValue:%d \n", __func__,  uTimeValue/1000000);
#endif

		if (   current_cnt == 1
			&& RM_WIFI_dpm_supp_is_connected() == pdFAIL
		    && RM_PMGR_W_dpm_wakeup_src_get() != (BSP_WAKEUP_SOURCE_GPIO | BSP_WAKEUP_RESET_WITH_RETENTION)) {

			if (RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(TID_DHCP_CLIENT) > 0) {
				RM_PMGR_W_dpm_timer_delete_by_tid(TID_DHCP_CLIENT); // Delete the DHCP Client RTC Timer
			}
		}

#ifdef __DPM_MNG_SAVE_RTM__
		if (dpmDaemonSleepFunction) {
#ifdef DBG_PRINT_INFO
			PRINTF(" Call register function !!!\n", __func__);
#endif
			dpmDaemonSleepFunction();
		}
#endif

		/* level 1 : Delete only RTM data
		 * level 2 : Delete User RTC Timer and RTM data
		 */
		RM_PMGR_W_dpm_timer_delete_all(2);

#ifdef __BLE_COMBO_REF__
		extern void combo_make_sleep_rdy(void);
		combo_make_sleep_rdy();
#endif

		//call DPM notifier to PMGR to report abnormal event
		if(dpm_notify_abnormal_status(act_type) != FSP_SUCCESS) {
			printf("Error RM_PMGR_W_dpm_notify()\n");
		}

#if !SUPPORT_DPM_ABNORMAL_NOTIFICATION_ONLY
		/* Power down */
		if (dpm_abnorm_sleep1_flag == true)
		{
			// OLD DPM fc80211_ra6wx_pri_pwr_down(true);
		} else
		{
			if (uTimeValue / 1000000ULL == 0xDEADBEEF)
			{ // sleep2 power down for infinite sleep
#ifdef DBG_PRINT_ERROR
				printf(YELLOW_COLOR ">>> Goto abnormal sleep infinitely (type:%d cnt:%d timer:Infinite) \n" CLEAR_COLOR, act_type, current_cnt);
				vTaskDelay(portCONVERT_MS_2_TICKS(30));
#endif	// DBG_PRINT_ERROR
				R_PM_LowPowerModeEnter(PMGR_LLD_POWER_MODE_SLEEP2, 0);	//go to sleep2 for infinite sleep
			} 
			else
			{
				int ret;
				uint32_t id = DPM_TIMER_0;
				dpm_timer_param_t tparam; 	//<<< sleep3 pwr down

				R_DPM_TIMER_Kill(id);
				tparam.callback_func = (void*)NULL;
				tparam.callback_param = (void*)NULL;
				tparam.booting_offset = (void*)NULL;

#ifdef DBG_PRINT_ERROR
				printf(YELLOW_COLOR ">>> Goto abnormal sleep (type:%d cnt:%d timer:%lu sec) \n" CLEAR_COLOR, act_type, current_cnt, (unsigned long)(uTimeValue/1000000));
				vTaskDelay(portCONVERT_MS_2_TICKS(30));		// TEMPORARY for print
#endif	// DBG_PRINT_ERROR
				ret = R_DPM_TIMER_SleepSet(id, uTimeValue, tparam, true);
#ifdef DBG_PRINT_ERROR
				printf(" [%s] Err goto abnormal sleep: %d \n", __func__,  ret);
#endif	// DBG_PRINT_ERROR
			}
		}
#endif /*!SUPPORT_DPM_ABNORMAL_NOTIFICATION_ONLY*/
	}
}

/* dpm_wu_type 0 : normal power on reboot
 *	1 : RTC wakeup
 *	2 : user wakeup
 *	3 : no beacon / no ack / deauth
 *	4 : uc wakeup
 *	5 : beacon changed / bc_mc
 *	other: unknown
 */
static void dpm_sts_chk_tm_fn(TimerHandle_t xTimer)
{
    FSP_PARAMETER_NOT_USED(xTimer);

	int	act_type;
#ifdef __CHK_NETWORK_TRAFFIC__
	int dpm_traffic_exist = false;
#endif
	int wait_time;
#ifdef __PRE_NOTIFY_ABNORMAL__
	int notify_time = 0;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */
#ifdef DBG_PRINT_INFO
	int	dpm_wu_type;
#endif

	if (RM_PMGR_W_dpm_mode_get() == 0)
		return;

	if (dpm_abnorm_chk_hold_flag) {
		return;
	}

	if (RM_PMGR_W_dpm_sleep_is_hold() || dpm_monitor_stopped) {
		dpm_monitor_cleanup();
		return;
	}

#ifdef DBG_PRINT_INFO
	dpm_wu_type = RM_WIFI_dpm_ptim_event_get();

	PRINTF("[%s] Called Timer Function : dpm_wu_type=0x%x\n", __func__, dpm_wu_type);
#endif
	/* Check the Wi-Fi connection state ... */
	if (RM_WIFI_dpm_supp_is_connected() == 0) {
		goto chk_wifi_conn_fail;
	} else {
		char	*block_dpm_mod_name;

		/* In case of Wi-Fi connection okay,,, */
		block_dpm_mod_name = (char *)get_dpm_unsleep_task_names();

		/* If the Disconnect Loss module and DPM KEY module is failed, go abnormal power down*/
		if (   (block_dpm_mod_name != NULL)
		    && (   strcmp("DPM_KEY", block_dpm_mod_name) == 0
				|| strcmp("Disconnect_loss", block_dpm_mod_name) == 0)) {
			goto chk_wifi_conn_fail;
		}
	}

	/* Check DHCPC renew operation */
	if ((get_netmode(WLAN0_IFACE) == DHCPCLIENT) && (ra6w1_network_main_check_ip_addr(WLAN0_IFACE) == pdFALSE)) {
#ifdef DBG_PRINT_INFO
		PRINTF("--- [%s] wu_type=0x%x \n", __func__, dpm_wu_type);
#endif
		goto chk_dhcpc_state;
	}

	/* Check if ARP Request is sent and no response received yet. */
	if (get_current_arp_req_status()) {
#ifdef DBG_PRINT_INFO
		PRINTF("--- [%s] wu_type=0x%x, arp request sent. \n", __func__, dpm_wu_type);
#endif
		goto chk_arp_state;
	}

	/* Check DPM Power down fail state */
	if (RM_PMGR_W_dpm_sleep_is_started() == DONE_DPM_SLEEP) {
#ifdef DBG_PRINT_INFO
		PRINTF("--- [%s] Power Down Failed \n", __func__);
#endif
		goto chk_dpm_power_down_fail_state;
	}

	/* Unknown power down fail */
#if defined ( __CHK_NETWORK_TRAFFIC__ )
	if ( (old_dpm_tx_pck_cnt != dpm_get_net_traffic_tx_cnt())
		|| (old_dpm_rx_pck_cnt != dpm_get_net_traffic_rx_cnt())) {
		dpm_traffic_exist = true;
	}

	/* Save current DPM Data packet Traffic */
	old_dpm_tx_pck_cnt = dpm_get_net_traffic_tx_cnt();
	old_dpm_rx_pck_cnt = dpm_get_net_traffic_rx_cnt();

	//no traffic for idle_time
	if (dpm_traffic_exist == true) {
		idle_time = 0;
	} else {
		idle_time++;
#ifdef DBG_PRINT_INFO
		//PRINTF("idle_time: %d\n", idle_time);
#endif
	}

#ifndef TEST_DPM_ABNORM_ERROR_105
	if (   idle_time > 30
		&& !is_dpm_sleep_ready()) { //No data for 30 seconds and some application has not set DPM bit.
		goto user_dpm_fail_state;
	} else if (idle_time > 5 && is_dpm_sleep_ready()) { //No data for 5 seconds and DPM is ready
		goto chk_unknown_dpm_fail_state;
	}
#else
	if (idle_time > 5) { //No data for 5 seconds and DPM is ready
		goto chk_unknown_dpm_fail_state;
	}
#endif
#endif	// __CHK_NETWORK_TRAFFIC__

	goto no_action;

chk_wifi_conn_fail :
	if (wpa_supp_wps_in_use()) {
		goto no_action;
	}

	if (dpm_monitor_info_ptr->wifi_conn_wait_time > 0)
		wait_time = dpm_monitor_info_ptr->wifi_conn_wait_time;
	else
		wait_time = MAX_INIT_WIFI_CONN_TIME;

#ifdef __PRE_NOTIFY_ABNORMAL__
	notify_time = (wait_time * 3) / 4;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

	if (dpm_connection_fail_cnt++ < wait_time) {
		if (dpm_connection_fail_cnt == wait_time) {
#if defined (__SUPPORT_ATCMD__)
#ifdef __SUPPORT_DPM_ABNORM_MSG__
			extern void atcmd_wf_jap_print_with_cause(int cause);
			if (RM_WIFI_dpm_supp_state_get() >= WPA_SCANNING) {
				atcmd_wf_jap_print_with_cause(0);
			}
			PRINTF_ATCMD("\r\n+DPM_ABNORM_SLEEP\r\n");
#endif /* __SUPPORT_DPM_ABNORM_MSG__ */
#endif
			if (dpm_abnorm_sleep1_flag == pdTRUE) {
				PRINTF(RED_COL ">> Abnormal DPM(%d) Sleep1 after 1 second. \r\n" CLR_COL, DPM_ABNORM_ACT_1);
			} else {
				PRINTF(CYN_COL ">> Abnormal DPM(%d) operation after 1 second.\r\n" CLR_COL, DPM_ABNORM_ACT_1);
			}
		}

#ifdef __PRE_NOTIFY_ABNORMAL__
		if (dpm_connection_fail_cnt == notify_time) {
			PRINTF(CYN_COL ">> Notify Abnormal Status(condition:%d, remain:%d) \r\n" CLR_COL, DPM_ABNORM_ACT_1, (wait_time-notify_time));
			notifyAbnormalBeforeDeepSleep(DPM_ABNORM_ACT_1, wait_time-notify_time);
		}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

		/* just return because it's periodic timer function. */
		return;
	} else {
		/* Need to check "ssid->auth_failures" what to do something. */
		// Check ... the operation ... wpas_auth_failed() ...

		act_type = DPM_ABNORM_ACT_1;	// WIFI Connect fail
		goto next_action_for_dpm_abnormal;
	}

chk_dhcpc_state :
	/* dhcp client renew */
#if 0	// Need to implement additionally if needs
	if (dpm_monitor_info_ptr->dhcp_rsp_wait_time > 0)
		wait_time = dpm_monitor_info_ptr->dhcp_rsp_wait_time;
	else
#endif	// 0
		wait_time = MAX_DHCP_RENEW_TIME;

#ifdef __PRE_NOTIFY_ABNORMAL__
	notify_time = (wait_time * 3) / 4;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

	if (dpm_dhcp_no_response_cnt++ < wait_time) {
		/* just return because it's periodic timer function. */
		if (dpm_dhcp_no_response_cnt == wait_time) {
			PRINTF(CYN_COL ">> Abnormal DPM(%d) operation after 1 second.\r\n" CLR_COL, DPM_ABNORM_ACT_2);
		}

#ifdef __PRE_NOTIFY_ABNORMAL__
		if (dpm_dhcp_no_response_cnt == notify_time) {
			PRINTF(CYN_COL ">> Notify Abnormal Status(condition:%d, remain:%d) \r\n" CLR_COL, DPM_ABNORM_ACT_2, (wait_time-notify_time));
			notifyAbnormalBeforeDeepSleep(DPM_ABNORM_ACT_2, wait_time-notify_time);
		}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

		return;
	} else {
		act_type = DPM_ABNORM_ACT_2;	// DHCP Renew fail
		goto next_action_for_dpm_abnormal;
	}


chk_arp_state:
#if 0	// Need to implement additionally if needs
	if (dpm_monitor_info_ptr->arp_rsp_wait_time > 0)
		wait_time = dpm_monitor_info_ptr->arp_rsp_wait_time;
	else
#endif	// 0
		wait_time = MAX_ARP_WAIT_TIME;

#ifdef __PRE_NOTIFY_ABNORMAL__
	notify_time = (wait_time * 3) / 4;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

	if (dpm_arp_no_response_cnt++ < wait_time) {
		/* just return because it's periodic timer function. */
		if (dpm_arp_no_response_cnt == wait_time) {
#if defined (__SUPPORT_ATCMD__)
#ifdef __SUPPORT_DPM_ABNORM_MSG__
			extern void atcmd_wf_jap_print_with_cause(int cause);
			if (RM_WIFI_dpm_supp_state_get() >= WPA_SCANNING) {
				atcmd_wf_jap_print_with_cause(3);
			}
			PRINTF_ATCMD("\r\n+DPM_ABNORM_SLEEP\r\n");
#endif
#endif
			PRINTF(CYN_COL ">> Abnormal DPM(%d) operation after 1 second.\r\n" CLR_COL, DPM_ABNORM_ACT_3);
		}

#ifdef __PRE_NOTIFY_ABNORMAL__
		if (dpm_arp_no_response_cnt == notify_time) {
			PRINTF(CYN_COL ">> Notify Abnormal Status(condition:%d, remain:%d) \r\n" CLR_COL, DPM_ABNORM_ACT_3, (wait_time-notify_time));
			notifyAbnormalBeforeDeepSleep(DPM_ABNORM_ACT_3, wait_time-notify_time);
		}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

		return;
	} else {
		act_type = DPM_ABNORM_ACT_3;		// ARP response fail
		goto next_action_for_dpm_abnormal;
	}

chk_dpm_power_down_fail_state :
	// Need to implement additionally
	// if needs to change waiting time for DPM Power-down sequence
	wait_time = MAX_POWER_DOWN_TIME;

#ifdef __PRE_NOTIFY_ABNORMAL__
	notify_time = (wait_time * 3) / 4;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

	if (dpm_power_down_fail_cnt++ < wait_time) {
		/* just return because it's periodic timer function. */
		if (dpm_power_down_fail_cnt == wait_time) {
			PRINTF(CYN_COL ">> Abnormal DPM(%d) operation after 1 second.\r\n" CLR_COL, DPM_ABNORM_ACT_4);
		}

#ifdef __PRE_NOTIFY_ABNORMAL__
		if (dpm_power_down_fail_cnt == notify_time) {
			PRINTF(CYN_COL ">> Notify Abnormal Status(condition:%d, remain:%d) \r\n" CLR_COL, DPM_ABNORM_ACT_4, (wait_time-notify_time));
			notifyAbnormalBeforeDeepSleep(DPM_ABNORM_ACT_4, wait_time-notify_time);
		}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

		return;
	} else {
		act_type = DPM_ABNORM_ACT_4;	// DPM Power down fail (30 Sec)
		goto next_action_for_dpm_abnormal;
	}


user_dpm_fail_state :
	act_type = DPM_ABNORM_ACT_6;	// DPM application fail
	goto next_action_for_dpm_abnormal;


chk_unknown_dpm_fail_state :
#if 0	// Need to implement additionally if needs
	if (dpm_monitor_info_ptr->unknown_dpm_fail_wait_time > 0)
		wait_time = dpm_monitor_info_ptr->unknown_dpm_fail_wait_time;
	else
#endif	// 0
		wait_time = MAX_POWER_DOWN_TIME;

#ifdef __PRE_NOTIFY_ABNORMAL__
	notify_time = (wait_time * 3) / 4;
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

	if (dpm_unknown_fail_cnt++ < wait_time) {
		/* just return because it's periodic timer function. */
		if (dpm_unknown_fail_cnt == wait_time) {
			PRINTF(CYN_COL ">> Abnormal DPM(%d) operation after 1 second.\r\n" CLR_COL, DPM_ABNORM_ACT_5);
		}

#ifdef __PRE_NOTIFY_ABNORMAL__
		if (dpm_unknown_fail_cnt == notify_time) {
			PRINTF(CYN_COL ">> Notify Abnormal Status(condition:%d, remain:%d) \r\n" CLR_COL, DPM_ABNORM_ACT_5, (wait_time-notify_time));
			notifyAbnormalBeforeDeepSleep(DPM_ABNORM_ACT_5, wait_time-notify_time);
		}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

		return;
	} else {
		act_type = DPM_ABNORM_ACT_5;	// No data for 5 seconds and DPM is ready
		goto next_action_for_dpm_abnormal;
	}

next_action_for_dpm_abnormal :
	dpm_monitor_cleanup();
	dpm_abnormal_next_action(act_type);

no_action :
	return;
}

#ifdef __PRE_NOTIFY_ABNORMAL__
void setDeepSleepWaitTime(int time)
{
	PRINTF(CYN_COL ">> Set delay_time to Abnormal %d Sec \r\n" CLR_COL, time);

	dpm_monitor_info_ptr->wifi_conn_wait_time = time;

	dpm_power_down_fail_cnt = 0;
	dpm_connection_fail_cnt = 0;
	dpm_dhcp_no_response_cnt = 0;
	dpm_arp_no_response_cnt = 0;
	dpm_unknown_fail_cnt = 0;
	idle_time = 0;
}
#endif  /* __PRE_NOTIFY_ABNORMAL__ */

void start_dpm_sts_chk_timer(int dpm_wu_type)
{
	int		wifi_conn_wait_time = 0;
	int		dhcp_rsp_wait_time = 0;
	int		arp_rsp_wait_time = 0;
	int		unknown_dpm_fail_wait_time = 0;
	int 	wifi_conn_retry_cnt = 0;
#ifndef __DISABLE_DPM_ABNORM__
    int     dpm_abnormal_stop_flag = 0;
#endif	/*__DISABLE_DPM_ABNORM__*/

	/* RTM_DPM_MONITOR_PTR */
	dpm_monitor_info_ptr = get_dpm_monitor_info_ptr();

	/* Check the network running state
	 * Wait during defined checking time : 10 sec */
#define	WLAN0_IFACE	0
	while (check_net_init(WLAN0_IFACE) != pdPASS) {
		vTaskDelay(portCONVERT_MS_2_TICKS(100));		// 100 msec
	}

	if (RM_PMGR_W_dpm_wakeup_is_abnormal() == 0) {
#ifdef DBG_PRINT_INFO
		PRINTF(CYN_COL "Reset Last abnormal count.\n" CLR_COL);
#endif
		save_abnormal_act(0, 0);
	}

#ifdef __RA6WX_DPM_MON_CLIENT__
	start_dpm_monitor_client(dpm_wu_type);
#endif	// __RA6WX_DPM_MON_CLIENT__

#ifdef DBG_PRINT_INFO
	PRINTF(CYN_COL " [%s] dpm_wu_type=0x%x \n" CLR_COL, __func__, dpm_wu_type);
#endif

	dpm_save_dpm_tim_status(dpm_wu_type);

	/* Permit NVRAM write when first POR boot ...
	 * Else case,, it means on DPM operation and block NVRAM Write operation */
	if (RM_PMGR_W_dpm_is_enabled() && !RM_PMGR_W_dpm_is_wakeup()) {
		/* restore saved wifi_conn_wait_time from NVRAM */
#ifdef RM_MAP_PERSISTANT_W
		RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFICFG,
                                           WIFI_CONN_WAIT_NAME,
                                           &wifi_conn_wait_time);
#endif
		if (wifi_conn_wait_time > 0) {
			dpm_monitor_info_ptr->wifi_conn_wait_time = wifi_conn_wait_time;
		}

		/* restore dpm abnormal wifi conn retry count from NVRAM */
#ifdef RM_MAP_PERSISTANT_W
		RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFICFG,
                                           WIFI_CONN_RETRY_CNT_NAME,
                                           &wifi_conn_retry_cnt);
#endif
		if (wifi_conn_retry_cnt > 0) {
			dpm_wifi_conn_retry_cnt = dpm_monitor_info_ptr->wifi_conn_retry_cnt = wifi_conn_retry_cnt;
		} else {
			dpm_wifi_conn_retry_cnt = dpm_monitor_info_ptr->wifi_conn_retry_cnt = 0;
		}

		/* restore saved dhcp_rsp_wait_time from NVRAM */
#ifdef RM_MAP_PERSISTANT_W
		RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFICFG,
                                           DHCP_RSP_WAIT_NAME,
                                           &dhcp_rsp_wait_time);
#endif
		if (dhcp_rsp_wait_time > 0) {
			dpm_monitor_info_ptr->dhcp_rsp_wait_time = dhcp_rsp_wait_time;
		}

		/* restore saved arp_rsp_wait_time from NVRAM */
#ifdef RM_MAP_PERSISTANT_W
		RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFICFG,
                                           ARP_RSP_WAIT_NAME,
                                           &arp_rsp_wait_time);
#endif
		if (arp_rsp_wait_time > 0) {
			dpm_monitor_info_ptr->arp_rsp_wait_time = arp_rsp_wait_time;
		}

		/* restore saved unknown_dpm_fail_wait_time from NVRAM */
#ifdef RM_MAP_PERSISTANT_W
		RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFICFG,
                                           UNKNOWN_DPM_FAIL_WAIT_NAME,
                                           &unknown_dpm_fail_wait_time);
#endif
		if (unknown_dpm_fail_wait_time > 0) {
			dpm_monitor_info_ptr->unknown_dpm_fail_wait_time = unknown_dpm_fail_wait_time;
		}
	} else if (RM_PMGR_W_dpm_is_enabled() && RM_PMGR_W_dpm_is_wakeup()) {
		dpm_wifi_conn_retry_cnt = dpm_monitor_info_ptr->wifi_conn_retry_cnt;
	}

    /* Create timer to check the dpm running abnormal status. */
	dpm_sts_chk_tm = xTimerCreate(DPM_STS_CHK_NAME,
								portCONVERT_MS_2_TICKS(PERIOD_STS_CHK_TIME),
								pdTRUE,
								(void*) 0,
								(TimerCallbackFunction_t) dpm_sts_chk_tm_fn);

	xTimerStart(dpm_sts_chk_tm, portCONVERT_MS_2_TICKS(PERIOD_STS_CHK_TIME));

    /* Check if exist DPM-abnormal stop flag in NVRAM */
#ifndef __DISABLE_DPM_ABNORM__
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                   ENV_GROUP_WIFICFG,
                                   NVR_KEY_DPM_ABNORM_STOP,
                                   &dpm_abnormal_stop_flag) == 0) {
#endif
        if (dpm_abnormal_stop_flag == 1) {
            xTimerStop(dpm_sts_chk_tm, portCONVERT_MS_2_TICKS(10));
        }
    }
#endif /* __DISABLE_DPM_ABNORM__ */
}

void dpm_abnormal_chk_hold(void)
{
	dpm_abnorm_chk_hold_flag = 1;
#ifdef DBG_PRINT_INFO
	PRINTF(CYN_COL "--- [%s] Holding DPM Abnormal check \n" CLR_COL, __func__);
#endif
}

void dpm_abnormal_chk_resume(void)
{
	dpm_abnorm_chk_hold_flag = 0;
#ifdef DBG_PRINT_INFO
	PRINTF(CYN_COL "--- [%s] Resume DPM Abnormal check \n" CLR_COL, __func__);
#endif
}

char RM_PMGR_W_dpm_abnormal_checker_run(char flag)
{
#ifdef __DISABLE_DPM_ABNORM__
    return pdFAIL;
#endif

    /* Timer does not created yet. */
    if (dpm_sts_chk_tm == NULL) {
        return pdFAIL; 
    }

    if (flag == pdTRUE) {
        if (xTimerIsTimerActive(dpm_sts_chk_tm) == pdFALSE) {
            /* Start DPM abnormal check timer */
            xTimerStart(dpm_sts_chk_tm, portCONVERT_MS_2_TICKS(PERIOD_STS_CHK_TIME));
        }
    } else {
        /* Stop DPM abnormal check timer */
        xTimerStop(dpm_sts_chk_tm, portCONVERT_MS_2_TICKS(10));
    }

    return pdTRUE;
}

int	size_of_dpm_monitor_info_t()
{
	return(sizeof(dpm_monitor_info_t));
}
#endif /* CFG_WIFI */
#endif /* CFG_PMGR */
