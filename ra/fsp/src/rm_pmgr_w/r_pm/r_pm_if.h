/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef R_PM_IF_H
#define R_PM_IF_H

#include "rm_pmgr_w_instance.h"

/** Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define DPM_SCHEDULER_DEBUG
#define DPM_POWER_DOWN          1
#define DPM_POWER_ON            0

#define DPM_IDX_USING           0
#define DPM_IDX_NEXT            1
#define DPM_IDX_POWER_STATUS    2
#define DPM_IDX_RESERVED0       3

#define MAX_DPM_TIMER           16

/* Time to RTC Ticks conversion macros */
#define US_TO_RTC_TICKS(us)      ((((uint64_t) (us)) << 9LL) / 15625LL)
#define MS_TO_RTC_TICKS(ms)      ((((uint64_t) (ms)) << 15LL) / 1000LL)
#define SEC_TO_RTC_TICKS(sec)    (((uint64_t) (sec)) << 15LL)
#define RTC_TICKS_TO_US(clk)     ((((uint64_t) (clk)) * 15625LL) >> 9LL)
#define RTC_TICKS_TO_MS(clk)     ((((uint64_t) (clk)) * 1000) >> 15LL)
#define RTC_TICKS_TO_SEC(clk)    (((uint64_t) (clk)) >> 15LL)

/* For Backwards compatibility (avoid using these) */
#define SLEEPUSTOTICK(us)        US_TO_RTC_TICKS(us)
#define SLEEPMSTOTICK(ms)        MS_TO_RTC_TICKS(ms)
#define SLEEPSECTOTICK(sec)      SEC_TO_RTC_TICKS(sec)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum st_dpm_timer_id
{
    DPM_TIMER_0 = 0,
    DPM_TIMER_1,
    DPM_TIMER_2,
    DPM_TIMER_3,
    DPM_TIMER_4,
    DPM_TIMER_5,
    DPM_TIMER_6,
    DPM_TIMER_7,
    DPM_TIMER_8,
    DPM_TIMER_9,
    DPM_TIMER_10,
    DPM_TIMER_11,
    DPM_TIMER_12,
    DPM_TIMER_13,
    DPM_TIMER_14,
    DPM_TIMER_15,
    DPM_TIMER_ERR
} dpm_timer_id_t;

typedef struct __attribute__((packed)) st_dpm_timer_callback_type
{
    void   (* func)(void *);
    void * param;
}
dpm_timer_callback_type_t;

typedef struct __attribute__((packed)) st_dpm_timer_param
{
    void * callback_param;
    void * callback_func;
    void * booting_offset;
}
dpm_timer_param_t;

/***********************************************************************************************************************
 * Exported global functions (to be accessed by other files)
 **********************************************************************************************************************/
void r_pm_low_power_mode3_enter(uint32_t sleep_id, uint64_t periodic);
void r_pm_low_power_mode2_enter(uint32_t sleep_id, uint64_t rtc_ticks);

/*******************************************************************************************************************//**
 * Get wakeup source.
 *
 * @return Wakeup Source
 **********************************************************************************************************************/
bsp_wakeup_source_mask_t R_PM_WakeupSourceGet(void);

/*******************************************************************************************************************//**
 *  Do power down ready.
 *
 * @note This function must be called 35us before R_BSP_SleepEnter()
 * @param[in] clear wakeup source to clear
 **********************************************************************************************************************/
void R_PM_WakeupSourceClear(bool clear);

/*******************************************************************************************************************//**
 * @brief Enters the specified low power mode.
 *
 * This function prepares the system to enter the specified low power mode.
 * It sets up the necessary configurations and enters the sleep mode.
 *
 * @param power_mode The low power mode to enter.
 * @param rtc_ticks  RTC ticks to sleep
 *                  - value range(36bit) :0x04 ~ 0xfffffffff
 *                  - usign with US_TO_RTC_TICKS, MS_TO_RTC_TICKS, SEC_TO_RTC_TICKS
 *                  - maximum is about 24days, minimum is about 100usec
 **********************************************************************************************************************/
fsp_err_t R_PM_LowPowerModeEnter(pmgr_lld_power_mode_t power_mode, uint64_t rtc_ticks);

/*******************************************************************************************************************//**
 * @brief Returns the delta in RTC of tickless time
 *
 * The output is vaid only after wakeup from sleep4, and is relvant only for the last tickless sleep
 *
 * @return  Delta in RTC of tickless time
 **********************************************************************************************************************/
uint64_t R_PM_TicklessRtcDeltaGet(void);

/*******************************************************************************************************************//**
 * @brief Execute the clock gating mode - CPU HALT.
 **********************************************************************************************************************/
void R_PM_Execute_Wfi(void);

/*******************************************************************************************************************//**
 * \brief check allocated DPM timer ID
 *
 * This function:
 * - return true if it is use already, false else case.
 **********************************************************************************************************************/
bool R_DPM_TIMER_IdPendingCheck(dpm_timer_id_t id);

/*******************************************************************************************************************//**
 * \brief Get empty DPM timer ID
 *
 * This function:
 * - return empty DPM timer ID
 **********************************************************************************************************************/
dpm_timer_id_t R_DPM_TIMER_EmptyIdGet(void);

/*******************************************************************************************************************//**
 * \brief Set DPM timer
 *
 * This function register the Timer. the timer interrupt shall occur after wake-up
 *
 * \param [in] id       DPM Timer ID
 * \param [in] time     Time for interrupt(or wake-up) occur
 * \param [in] param    callback function and it's parameter.
 * \param [in] b_enterSleep set entersleep
 *
 * \sa ad_dpm_set_wakeup_timer()
 *
 * \return id value if success, if not AD_DPM_TIMER_ERR
 **********************************************************************************************************************/
dpm_timer_id_t R_DPM_TIMER_SleepSet(dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param, uint32_t b_enterSleep);

/*******************************************************************************************************************//**
 * \brief Destroy DPM timer
 *
 *  This function destroy DPM timer.
 *
 * \param [in] id       DPM Timer ID
 *
 * \return id if success, if not AD_DPM_TIMER_ERR
 **********************************************************************************************************************/
dpm_timer_id_t R_DPM_TIMER_Kill(dpm_timer_id_t id);

/*******************************************************************************************************************//**
 * \brief DPM timer Wakeup handler
 *
 * This function:
 * - Should be called in system initialization.
 * - Initializes retention memory for DPM and make timer interrupt occur.
 * - Manage the DPM timers order.
 * - it should be called in the system init step
 *
 * \return wake-up source if success
 **********************************************************************************************************************/
bsp_wakeup_source_mask_t R_DPM_TIMER_WakeupHandler(void);

/*******************************************************************************************************************//**
 * \brief Print DPM timer info
 *
 * \return none
 **********************************************************************************************************************/
void R_DPM_TIMER_PrintList(void);

int rtc_timer_info(int tid);

dpm_timer_id_t r_dpm_wakeup_timer_id_get(void);

void r_dpm_wakeup_timer_id_set(dpm_timer_id_t timer_id);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER
#endif
