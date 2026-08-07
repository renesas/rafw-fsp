/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_PMGR_W_DPM_INTERNAL_H
#define RM_PMGR_W_DPM_INTERNAL_H

#include "bsp_api.h"

#if CFG_PMGR
#if CFG_WIFI
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include <stdbool.h>
#include "romac4rtos.h"
#include "rm_pmgr_w_rtm_internal.h"
#include "rm_wifi_dpm.h"
#include "sleep_mgmt_regs.h"

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/
/*Wakeup definition compitable for legacy*/
#undef	DPM_TIMER_DEBUG
#define USE_SET_DPM_INIT_DONE

/* Defines */
#define DPM_ENABLED            1
#define DPM_DISABLED           0

#define DPM_WAKEUP             1
#define NORMAL_BOOT            0

#define DPM_MUTEX_WAIT                    100

/* NVRAM KEY */
#define	REG_NAME_DPM_KEY			"DPM_KEY"
#define	REG_NAME_DISCON_LOSS		"Disconnect_loss"
#define	REG_NAME_DPM_MAC			"DPM_MAC"
#define	REG_NAME_DPM_DRV			"DPM_DRV"
#define	REG_NAME_DPM_SUPPLICANT		"DPM_SUPP"
#define	REG_NAME_DPM_LWIP_IPV6		"DPM_IPV6"

#define WAIT_DPM_SLEEP				0
#define RUN_DPM_SLEEP				1
#define DONE_DPM_SLEEP				2

#define	DPM_OK						0
#define	DPM_REG_OK					0
#define	DPM_SET_OK					0

#define	DPM_REG_ERR					-1
#define	DPM_SET_ERR					-2
#define	DPM_SET_MUTEX_ERR			-3
#define	DPM_SET_ERR_BLOCK			-9

#define DPM_NOT_DPM_MODE			7777
#define DPM_NOT_REGISTERED			8888
#define DPM_REG_DUP_NAME			9999

//__CHK_DPM_ABNORM_STS__ - Start
#define DPM_UNDEFINED				0x000000
#define DPM_UC						0x000001 /* UC */
#define DPM_BC_MC					0x000002 /* BC/MC */
#define DPM_BCN_CHANGED				0x000004
#define DPM_NO_BCN					0x000008
#define DPM_FROM_FAST				0x000010
#define DPM_KEEP_ALIVE_NO_ACK		0x000020
#define DPM_FROM_KEEP_ALIVE			0x000040
#define DPM_NO						0x000080
#define DPM_UC_MORE					0x000200 /* UC more */
#define DPM_AP_RESET				0x000400
#define DPM_DEAUTH					0x000800
#define DPM_DETECTED_STA			0x001000
#define DPM_FROM_FULL				0x002000
#define DPM_USER0					0x080000
#define DPM_USER1					0x100000

#define FUNC_ERROR					100
#define FUNC_STATUS					200

#define STATUS_WAKEUP				(FUNC_STATUS + 1)
#define STATUS_POR					(FUNC_STATUS + 2)
//__CHK_DPM_ABNORM_STS__ - End

#define REG_NAME_DPM_MAX_LEN		20
#define DPM_TIMER_NAME_MAX_LEN		8

#define	EVT_MPM_DPM_SLEEP_RDY	(1UL << (2))
#define EVT_MPM_DPM_ALL         (EVT_MPM_DPM_SLEEP_RDY)

#undef	WORKING_FOR_DPM_TCP	// For DPM TCP Session


#define DPM_DAEMON_TRIGGER_TASK_NAME    "dpm_trigger"
#define DPM_TRIGGER_POLLING_TIME        30    // 30 msec
#define SLEEP_DAEMON_WAKEUP_TIME        portCONVERT_MS_2_TICKS(10)

#define DPM_SLEEP_DAEMON_NAME            "dpm_lld_task"
#define DPM_SLEEPD_PRIORITY                OS_TASK_PRIORITY_USER
#define DPM_SLEEPD_STACK_SZ                256

#define DPM_DAEMON_TRIGGER                ( 1 << 0 )
#define DPM_SLEEP_SET                    ( 1 << 1 )
#define DPM_STOP_FLAG                    ( 1 << 2 )

#define MAX_CHECK_CNT_PS_CMD_FAIL        10    // 1000 msec
#define TIMER_MAX_MSEC                  (2097151*1000) // 0x1FFFFF * 1000, ~24 days

#define DPM_TIMER_TASK_NAME                "dpm_tm_task"
#define DPM_TIMER_TASK_PRIORITY            OS_TASK_PRIORITY_USER
#define DPM_TIMER_STK_SZ                1024
#define RTC_ABNORMAL_MAX_CNT            3

#define PMGR_CONDITION_IPV4_MANDATORY   (1UL << (0))  /// wait until IPv4 address Ready
#define PMGR_CONDITION_IPV6_MANDATORY   (1UL << (1))  /// wait until IPv6 address Ready
#define PMGR_CONDITION_IPV46_MANDATORY  (PMGR_CONDITION_IPV4_MANDATORY | PMGR_CONDITION_IPV6_MANDATORY)
                                        /// wait until IPv4/IPv6 address Ready
#define PRINTF(...)    printf(__VA_ARGS__)

#define dg_configUSE_IDLE_TASK_TRIGGER             ( 0 ) // Activating sleep_dameon by IDLE task
#if (dg_configUSE_IDLE_TASK_TRIGGER == 1)
#define dg_configUSE_TIMER_TRIGGER                 ( 0 )
#else
#define dg_configUSE_TIMER_TRIGGER                 ( 1 ) // Activating sleep_dameon by timer
#endif

/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
/// DPM Wakeup type
enum DPM_WAKEUP_TYPE {
    DPM_UNKNOWN_WAKEUP              = 0,
    DPM_RTCTIME_WAKEUP              = 1,
    DPM_PACKET_WAKEUP               = 2,
    DPM_USER_WAKEUP                 = 3,
    DPM_NOACK_WAKEUP                = 4,
    DPM_DEAUTH_WAKEUP               = 5,
    DPM_TIM_ERR_WAKEUP              = 6,
    DPM_DDPS_BUFP_WAKEUP            = 7,
    DPM_TCP_KA_TIMEOUT_WAKEUP       = 8,
    DPM_INTERNAL_PTIM_ISSUE_WAKEUP  = 9
};

enum DPM_TID {
	TID_USER_WAKEUP = 2,
	TID_DHCP_CLIENT,
	TID_ABNORMAL
};

typedef struct _dpm_mdns_info {
	char hostname[64];
	size_t len;
} dpm_mdns_info_t;

#if defined ( WORKING_FOR_DPM_TCP )
/* RTC Timer ID for TCP Session Keep-Alive */
typedef struct {
        char    tcp_sess_name[DPM_MAX_TCP_SESS][REG_NAME_DPM_MAX_LEN];
        unsigned char	rtc_tid[DPM_MAX_TCP_SESS];
} dpm_tcp_ka_info_t;
#endif /* WORKING_FOR_DPM_TCP */

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
extern dpm_supp_conn_info_t *dpm_supp_conn_info;
extern dpm_supp_conn_ext_info_t *dpm_supp_conn_ext_info;
extern dpm_supp_key_info_t *dpm_supp_key_info;

extern int show_info_periodically;
extern int show_info_term_by_sec;

/***********************************************************************************************************************
 * Global external function
 *********************************************************************************************************************/
extern void RM_PMGR_W_dpm_lld_task_start(int dpm_wu_type);
extern void RM_PMGR_W_dpm_lld_task_init(void);
extern fsp_err_t RM_PMGR_W_dpm_notify(int dpm_event);

/***********************************************************************************************************************
 * Inilne function
 *********************************************************************************************************************/
static inline char *dpm_strcpy(char *s1, char *s2)
{
	char *s = s1;

	while ((*s++ = *s2++) != 0)
		;

	return (s1);
}

static inline size_t dpm_strlen(char *str)
{
	char *s;

	for (s = str; *s; ++s);

	return (size_t)(s - str);
}

static inline int dpm_strcmp(const char *s1, const char *s2)
{
	for ( ; *s1 == *s2; s1++, s2++) {
		if (*s1 == '\0')
			return 0;
	}

	return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

static inline int dpm_strncmp(const char *s1, const char *s2, size_t n)
{
	for ( ; n > 0; s1++, s2++, --n) {
		if (*s1 != *s2)
			return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
		else if (*s1 == '\0')
			return 0;
	}

	return 0;
}

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/*
 ****************************************************************************************
 * @brief Set the DPM module to goto DPM sleep.
 * @param[in] mod_name Name of DPM module.
 *
 * @return 0(DPM_SET_OK) if successful.
 ****************************************************************************************
 */
int	RM_PMGR_W_dpm_sleep_ready_set(char *dpm_name);

/*
 ****************************************************************************************
 * @brief Name of DPM module to clear DPM sleep
 * @param[in] mod_name Name of DPM module.
 *
 * @return 0(DPM_SET_OK) if successful.
 ****************************************************************************************
 */
int	RM_PMGR_W_dpm_sleep_ready_clear(char *dpm_name);

/*
 ****************************************************************************************
 * @brief Set the DPM module to ready data transmission using registered port number.
 * @param[in] port Port number of DPM module.
 *
 * @return 0(DPM_SET_OK) if successful.
 ****************************************************************************************
 */
int RM_PMGR_W_dpm_rcv_ready_set_by_port(unsigned int port);

/*
 ****************************************************************************************
 * @brief Get whether DPM mode is on or not.1 if DPM mode is on.
 *
 * @return 1 if DPM mode is on.
 ****************************************************************************************
 */
int	RM_PMGR_W_dpm_mode_get(void);

/*
 ****************************************************************************************
 * @brief Check whether the DPM module is able to goto DPM sleep or not.
 * @param[in] mod_name Name of DPM module.
 *
 * @return 1 if the DPM module is able to goto DPM sleep.
 ****************************************************************************************
 */
int	RM_PMGR_W_dpm_sleep_is_set(char *mod_name);

/*
 ****************************************************************************************
 * @brief Check whether the DPM module is ready to get RTC timer callback function or not.
 *        It will be internally waiting for 1 sec if it's not ready.
 * @param[in] mod_name Nmae of DPM module.
 *
 * @return 1, if the DPM module is ready to get RTC timer callback function.
 ****************************************************************************************
 */
bool RM_PMGR_W_dpm_wakeup_is_done(char *mod_name);

/*
 ****************************************************************************************
 * @brief Enable ARP in DPM mode..
 * @param[in] period Period.
 * @param[in] mode Enable to ARP.
 ****************************************************************************************
 */
void dpm_arp_en(int period, int mode);

/*
 ****************************************************************************************
 * @brief Delete all registered timers.
 * @param[in] if 1, only the last remaining timeout of each timer is occurred and all are deleted.
 *            if 2, all timers are deleted immediately.
 *
 * @return 1(pdTRUE) if successful.
 ****************************************************************************************
 */
int	RM_PMGR_W_dpm_timer_delete_all(unsigned int level);

/*
 ****************************************************************************************
 * @brief In DPM sleep mode, the timer count is also activated,
 *        and when the timeout event occurs,
 *        the RA6W1 wakes up and calls the registered callback function.
 * @param[in] msec Timeout setting in mili-seconds.
 * @param[in] name NULL or a character string of 20 bytes or less.
 *                 If the user program that calls the RTC timer is registered
 *                 and operated in DPM Manager, the same string
 *                 as the name string registered in DPM Manager must be entered.
 *                 If it is a user program that is not related to DPM Manager, enter Null.
 * @param[in] timer_id If 0, an available idle timer index value is automatically assigned. (recommend)
 * @param[in] peri If 0, timeout occurs once and then ends. (Non Periodic)
 *                 If 1, it is automatically re-registered after timeout occurs. (Periodic)
 * @param[in] callback_func Function pointer of the callback function
 *                          to be executed when timeout occurs.
 *
 * @return Timer IDs 5 to 15 are returned if successful.  -1 if failure.
 ****************************************************************************************
 */
int RM_PMGR_W_dpm_timer_create_by_tid(unsigned int msec, char *name, int timer_id, int peri, void (* callback_func)(char *timer_name));

/*
 ****************************************************************************************
 * @brief Delete the registered timer.
 * @param[in] timer_id RTC timer ID to delete.
 *
 * @return Timer IDs 5 to 15 if successful.
 ****************************************************************************************
 */
int RM_PMGR_W_dpm_timer_delete_by_tid(int timer_id);

/*
 ****************************************************************************************
 * @brief Print registered timer information to the terminal.
 ****************************************************************************************
 */
void rtc_timer_list_info(void);

/*
 ****************************************************************************************
 * @brief Get the remaining time of a registered timer.
 * @param[in] tid RTC timer ID to inquire the remaining time.
 *
 * @return Remaining timeout msec if successful.
 ****************************************************************************************
 */

int RM_PMGR_W_dpm_timer_remaining_msec_get_by_tid(int tid);

/*
 * Get state of what RRQ61x goes to DPM sleep.
 *
 * @retval      WAIT_DPM_SLEEP				0       Wait DPM sleep
 * @retval      RUN_DPM_SLEEP				1       Going to DPM sleep
 * @retval      DONE_DPM_SLEEP				2       DPM sleep is done.
 */
int	RM_PMGR_W_dpm_sleep_is_started(void);

/*
 * Turn on/off abnormal check timer
 *
 * @param[in]   flag            true: start abnormal check timer, false: stop abnormal check timer
 * @retval      true            Success
 * @retval      false           Abnormal check timer does not created yet.
 */
char RM_PMGR_W_dpm_abnormal_checker_run(char flag);

/*
 * it should be called in the system init step 
 * @brief DPM timer Wakeup Handler
 *
 * This function:
 * - Should be called in system initialization.
 * - Initializes retention memory for DPM and make timer interrupt occur.
 * - Manage the DPM timers order.
 *
 * @retval     FSP_SUCCESS          Remove sleep constraint done successfully
 * @retval     FSP_ERR_UNSUPPORTED  Remove sleep constraint is not supported
 * @retval     FSP_ERR_IN_USE       Remove sleep constraint failed due to the semaphore is not available
*/
fsp_err_t RM_PMGR_W_DpmTimerWakeupHandler();

void RM_PMGR_W_dpm_enable(void);

void RM_PMGR_W_dpm_disable(void);

int	RM_PMGR_W_dpm_is_enabled(void);

void RM_PMGR_W_dpm_dbg_info_get(void);

int	RM_PMGR_W_dpm_sleep_is_hold(void);

void RM_PMGR_W_dpm_hold(void);

void RM_PMGR_W_dpm_sleep_resume(void);

bool RM_PMGR_W_dpm_sleep_is_lld_task_running(void);

void dpm_save_country_code(const char *country);

void RM_PMGR_W_dpm_environ_init(void);

void reset_dpm_info(void);

int is_wifi_profile_set_from_nvram(void);

int RM_PMGR_W_dpm_mode_get_from_nvram(void);

unsigned char *get_ra6w1_dpm_arp_ptr(void);

void set_dpm_bcn_wait_timeout(unsigned int msec);

void set_dpm_nobcn_check_step(int step);

fsp_err_t RM_PMGR_W_dpm_set_sleep();

int	dpm_daemon_regist_sleep_cb(void (*regConfigFunction)());

unsigned int RM_PMGR_W_dpm_timer_task_create(void);

unsigned int get_dpm_user_wu_time(void);

void RM_PMGR_W_dpm_ptim_abnormal_wakeup_set(void);

void RM_PMGR_W_dpm_wakeup_src_save(int wakeupsource);

int chk_ext_int_exist(void);

void unsupport_func_in_dpm(void);

int dpm_timer_find_timer_id(char *task_name, char *timer_name);

void rtc_timeout_cb(unsigned int *param);

void enable_dpm_wakeup(void);

void disable_dpm_wakeup(void);

void RM_PMGR_W_dpm_user_timer_list_print(void);

int RM_PMGR_W_dpm_info_lock_take(unsigned int timeout);

int RM_PMGR_W_dpm_info_lock_give(void);

int RM_PMGR_W_dpm_is_wakeup(void);

fsp_err_t RM_PMGR_W_dpm_user_wakeup_timer_set (int user_wu_time);

void RM_PMGR_W_dpm_user_wakeup_timer_init (void);

#endif /*CFG_WIFI*/

#endif /* CFG_PMGR */

#endif /*RM_PMGR_W_DPM_INTERNAL_H*/
