/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "bsp_pd.h"
#include "bsp_io.h"
#include "r_pm_if.h"
#include "r_gpio_w.h"
#include "common_data.h"
#include "../../../bsp_w/mcu/ra6w1/bsp_cache.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/* Retention memory for Using Booter */
typedef struct BOOTER_RETENTION_INFO
{
    uint32_t tin_wakeup_source;
    uint32_t tin_tim_app_address;

    /*more*/
} RETENTION_MEM_type;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
extern void goto_deepsleep(void);

/***********************************************************************************************************************
 * Private value
 **********************************************************************************************************************/
RETENTION_MEM_type * boot_info = (RETENTION_MEM_type *) dg_configBOOTER_RTM_ADDR;
static uint64_t      g_rtc_tickless_delta;

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/
static fsp_err_t r_pm_ready_for_sleep ()
{
    /* Clear wakeup Source */
    RTC->WAKEUP_SRC_CLR_SIG_REG = 0xfe;

    /* Ready for Sleep */
    RTC->ENABLE_CTRL_REG   &= ~(RTC_ENABLE_CTRL_REG_SLEEP3_EN_Msk | RTC_ENABLE_CTRL_REG_WATCHDOG_OUT_INT_EN_Msk);
    RTC->DCDC_CNTL_OFF_REG &= ~(RTC_DCDC_CNTL_OFF_REG_DCDC_PWR_OFF_Msk);

    return FSP_SUCCESS;
}

static fsp_err_t r_pm_enter_for_sleep (uint32_t sleep_id, uint64_t rtc_ticks)
{
    volatile uint64_t result_time;

    /* Clear RESET_STAT_REG */
    CRG_TOP->RESET_STAT_REG = 0x0;

    if ((rtc_ticks != 0))
    {
        result_time = R_BSP_SystemRtcCountGet() + rtc_ticks;

        /* Clear wakeup source */
        RTC->WAKEUP_SRC_CLR_SIG_REG                 = 0x00;
        RTC->RTM_CONTROL_REG_b.RTM_CTRL_PWR_DN_INFO = sleep_id & 0xF;

        /* Set wakeup count */
        RTC->WAKEUP_CNT_1_REG_b.WAKEUP_CNT_1 = (uint32_t) ((result_time >> 32) & 0xF);
        RTC->WAKEUP_CNT_0_REG_b.WAKEUP_CNT_0 = (uint32_t) (result_time);

        /* Enter sleep mode */
        RTC->ENABLE_CTRL_REG_b.SLEEP3_EN = 1;
    }
    else
    {
        RTC->RTM_CONTROL_REG_b.RTM_CTRL_PWR_DN_INFO = sleep_id & 0xF;
        RTC->DCDC_CNTL_OFF_REG_b.DCDC_PWR_OFF       = 1;
    }

    /* Waiting entering sleep */
    while (1)
    {
        ;
    }

    return FSP_SUCCESS;
}

static fsp_err_t r_pm_enter_for_sleep4 (uint32_t sleep_id, uint64_t rtc_ticks)
{
    volatile uint64_t result_time;

    /* Clear RESET_STAT_REG */
    CRG_TOP->RESET_STAT_REG = 0x0;

    result_time = R_BSP_SystemRtcCountGet() + rtc_ticks;

    /* Clear wakeup source */
    RTC->WAKEUP_SRC_CLR_SIG_REG                 = 0x00;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_PWR_DN_INFO = sleep_id & 0xF;

    /* To prevent sleep mode4 wake-up failure */
    RTC->ULDO_CONT_REG_b.DCDC_ST_CTRL_4_0 = 0x1f;
    RTC->LDO_ENABLE_REG_b.CFG_LPDIG_LDO_V = 0x0D; // 0.85
    RTC->ENABLE_CTRL_REG_b.DIG_LDO_DELAY  = 3;

    /* end of work around */

    /* Set wakeup count */
    RTC->WAKEUP_CNT_1_REG_b.WAKEUP_CNT_1 = (uint32_t) ((result_time >> 32) & 0xF);
    RTC->WAKEUP_CNT_0_REG_b.WAKEUP_CNT_0 = (uint32_t) (result_time);

    /* Enter sleep mode */
    if (sleep_id == 5)
    {
        RTC->ENABLE_CTRL_REG_b.SLEEP5_EN = 1;

        /* Waiting entering sleep */
        while (!RTC->ENABLE_CTRL_REG_b.SLEEP5_EN)
        {
            ;
        }
    }
    else
    {
        RTC->ENABLE_CTRL_REG_b.SLEEP4_EN = 1;

        /* Waiting entering sleep */
        while (!RTC->ENABLE_CTRL_REG_b.SLEEP4_EN)
        {
            ;
        }
    }

    return FSP_SUCCESS;
}

void r_pm_low_power_mode2_enter (uint32_t sleep_id, uint64_t rtc_ticks)
{
     R_BSP_RetainedIoExecute();

    R_BSP_WakeupSourcePinClear();

    r_pm_ready_for_sleep();

    /* Clear retention memory clear*/
    RTC->RTM_CONTROL_REG_b.RTM_INFO = 0;

    /* Off retetion memory power*/
    RTC->LDO_ENABLE_REG_b.LDO_EN_PDB_ULDO = 0;

    r_pm_enter_for_sleep(sleep_id, rtc_ticks);
}

void r_pm_low_power_mode3_enter (uint32_t sleep_id, uint64_t rtc_ticks)
{
     R_BSP_RetainedIoExecute();

    R_BSP_WakeupSourcePinClear();

    /* Set retention memory flag*/
    RTC->RTM_CONTROL_REG_b.RTM_INFO = 1;

    /* Close retention memory for power saving*/
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_PDB_ISO = 0;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_RET = RETENTION_MEM_BANK_ALL;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_SLR = RETENTION_MEM_NONE;

    r_pm_enter_for_sleep(sleep_id, rtc_ticks);
}

__RETAINED_CODE static void r_pm_enter_deepsleep_handler (void)
{
    uint32_t is_pll;
#if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
    uint32_t cache_enabled = REG_GETF(CACHE, CACHE_CTRL2_REG, CACHE_LEN);

    OQSPIF->OQSPIF_CTRLMODE_REG_b.OSPIC_AUTO_MD = 0;
#endif
    is_pll = CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL;

    goto_deepsleep();

    if (is_pll)
    {
        pll_on();
    }

#if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
    hw_cache_disable();
    do
    {
        // switch off and on automode in order to trigger re-sending the read command
        OQSPIF->OQSPIF_CTRLMODE_REG_b.OSPIC_AUTO_MD = 0;
        OQSPIF->OQSPIF_CTRLMODE_REG_b.OSPIC_AUTO_MD = 1;

        if ((*(volatile uint32_t *) 0x0a000000 & 0x0000ffff) == 0x00007050)
        {
            break;                     // if "Pp" was found, we assume the flash is ready to be used in automode.
        }
    } while (1);

    /* Cache may be disabled when coming back from sleep so let's restore it */
    hw_cache_clear_flushed();

    REG_SETF(CACHE, CACHE_CTRL2_REG, CACHE_LEN, cache_enabled);
    hw_cache_enable();
    while (hw_cache_is_flushed() == 0)
    {
        ;
    }
#endif
}

#define RTC_RATE_HZ    (1U << 15)      // 32768

static uint32_t r_pm_restore_rtos_ticks (uint64_t rtc_delta, uint32_t systick_val_pre)
{
    uint32_t rtos_ticks = 0;
    uint32_t val = 0, load = 0, ctrl = 0;
    uint32_t reload = 0;
    uint64_t systick_delta;
    uint32_t remainder = 0;

    /* Get values of SysTick->VAL, LOAD and CTRL before the sleep */
    val  = prvSysTick_stored_val_get();
    load = prvSysTick_stored_load_get();
    ctrl = prvSysTick_stored_ctrl_get();

    /* Convert RTC to sysTick */
    systick_delta = (rtc_delta * configCPU_CLOCK_HZ) / RTC_RATE_HZ;

    /* Need to subtract the time it took to sample RTC to stop sysTick.
     * systick_val_pre is sample of sysTick->VAL right before RTC sample start,
     * val holds sysTick->VAL right after sysTick was disabled. This time is
     * not constant. */
    if (systick_val_pre > val)
    {
        /* There was no sysTick wrap (Note that systick is counting down) */
        systick_delta -= (systick_val_pre - val);
    }
    else
    {
        /* There was sysTick wrap (Note that systick is counting down) */
        systick_delta -= (systick_val_pre + (load - val));
    }

    /* Calculate how many RTOS ticks passed during deepsleep */
    rtos_ticks = (uint32_t) (systick_delta / load);
    remainder  = (uint32_t) (systick_delta % load);

    /* Calculate first reload value to correctly complete RTOS tick:
     * Val holds SysTick->VAL right when SysTick was disabled (before sleep).
     * systick_remainder holds RTC sleep time remainder from a full RTOS tick.
     * With both parameters can determine whether another RTOS tick happened,
     * and what SysTick reload value is needed to get to the first RTOS tick
     * after the wakeup. */
    if (val > remainder)
    {
        /* There is no limit on how small reload can be. */
        reload = val - remainder;
    }
    else
    {
        rtos_ticks += 1;
        reload      = load - (remainder - val);
    }

    /* Compenstae for RTOS tick that occurred before the sleep, at the critical
     * section between disabling IRQ to disabling of SysTick timer */
    if (ctrl & SysTick_CTRL_COUNTFLAG_Msk)
    {
        rtos_ticks += 1;
    }

    /* Step the RTOS tick counter */
    vTaskStepTick((TickType_t) rtos_ticks);

    /* Load value to be used for first tick only */
    return reload;
}

static void r_pm_low_power_mode4_enter (uint32_t sleep_id, uint64_t rtc_ticks)
{
    uint64_t rtc_tickless_start;
    uint32_t load = 0, systick_val_pre;

    /* Sample sysTick value right before sampling RTC */
    systick_val_pre = SysTick->VAL;

    /* Sample RTC before stopping the RTOS tick timer */
    rtc_tickless_start = R_BSP_SystemRtcCountGet();

    /* Stop the timer that is generating the tick interrupt */
    prvStopTickInterruptTimer();

    R_BSP_WakeupSourcePinClear();

    r_pm_ready_for_sleep();

    CRG_TOP->PMU_CTRL_REG_b.PHY_SLEEP = 1;
    CRG_TOP->PMU_CTRL_REG_b.MAC_SLEEP = 1;

    /* Close retention memory for power saving*/
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_PDB_ISO = 0;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_RET = RETENTION_MEM_BANK_ALL;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_SLR = RETENTION_MEM_NONE;

    r_pm_enter_for_sleep4(sleep_id, rtc_ticks);

    /* Enter sleep here */
    r_pm_enter_deepsleep_handler();

    /* Open retention memory */
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_RET = 0;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_RET_SLR = 0;
    RTC->RTM_CONTROL_REG_b.RTM_CTRL_PDB_ISO = 1;

    CRG_TOP->CLK_AMBA_REG_b.TIMER_CLK_ENABLE = 1;

    RTC->CNT_TESTI_REG_b.EN_RTC_GPO_SW_VBAT = 0;
    CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE = 1;

    SystemWakeupSourceUpdate();

    /* Correct ticks to account for the time spent in deep sleep */
    g_rtc_tickless_delta = R_BSP_SystemRtcDiff(rtc_tickless_start, R_BSP_SystemRtcCountGet());
    load                 = r_pm_restore_rtos_ticks(g_rtc_tickless_delta, systick_val_pre);

    /* Restart the timer that is generating the tick interrupt */
    prvStartTickInterruptTimer(load);
}

static void r_pm_execute_wfi (void)
{
#if (dg_configDISABLE_BACKGROUND_FLASH_OPS == 0)

    /* If an interrupt is already pending, do not sleep. */
    if ((NVIC->ISER[0] & NVIC->ISPR[0]) ||
        (NVIC->ISER[1] & NVIC->ISPR[1]) ||
        (NVIC->ISER[2] & NVIC->ISPR[2]))
    {
        DBG_SET_LOW(PWR_MGR_USE_TIMING_DEBUG, PWRDBG_SLEEP_ENTER);
        DBG_SET_HIGH(PWR_MGR_USE_TIMING_DEBUG, PWRDBG_SLEEP_EXIT);

        return;
    }

    if (qspi_process_operations() == false)
    {
        /*
         * Wait for an interrupt
         *
         * Any interrupt will cause an exit from WFI(). This is not a problem since even
         * if an interrupt other that the tick interrupt occurs before the next tick comes,
         * the only thing that should be done is to resume the scheduler. Since no tick has
         * occurred, the OS time will be the same.
         */
        DBG_SET_LOW(PWR_MGR_USE_TIMING_DEBUG, PWRDBG_SLEEP_ENTER);
        __DSB();
        __ISB();
        __WFI();
        DBG_SET_HIGH(PWR_MGR_USE_TIMING_DEBUG, PWRDBG_SLEEP_EXIT);
    }

    qspi_check_and_suspend_operations();
#else
    __DSB();
    __ISB();
    __WFI();
#endif
}

/***********************************************************************************************************************
 * Public Functions
 **********************************************************************************************************************/
uint64_t R_PM_TicklessRtcDeltaGet (void)
{
    return g_rtc_tickless_delta;
}

fsp_err_t R_PM_LowPowerModeEnter (pmgr_lld_power_mode_t power_mode, uint64_t rtc_ticks)
{
    fsp_err_t ret = FSP_SUCCESS;

    switch (power_mode)
    {
        case PMGR_LLD_POWER_MODE_SLEEP2:
        {
            /* Enter power mode 2 without retention memory */
            r_pm_low_power_mode2_enter(3, rtc_ticks);
            break;
        }

        case PMGR_LLD_POWER_MODE_SLEEP3:
        {
            /* Force wakeup FULL_BOOT*/
            boot_info->tin_tim_app_address = 0;

            /* Enter power mode 3 with retention memory */
            r_pm_low_power_mode3_enter(3, rtc_ticks);
            break;
        }

        case PMGR_LLD_POWER_MODE_SLEEP4:
        {
            /* Enter power mode 4 with system RAM powered */
            r_pm_low_power_mode4_enter(4, rtc_ticks);
            break;
        }

        case PMGR_LLD_POWER_MODE_CPU_WFI:
        {
            /* Suspend CPU executions (wait for interrupt) */
            r_pm_execute_wfi();
            break;
        }

        default:
        {
            ret = FSP_ERR_INVALID_ARGUMENT;
            break;
        }
    }

    return ret;
}
