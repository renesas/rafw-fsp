/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "r_pm_if.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define MINIMUM_TIME_SLICE    (*time_slice)
#define BOOT_TIM              (*boot_condition)
#define gCurIdx               (*gs_current_id)
#define gTimeGap              (*time_gap)
#define isr_block             (*isr_doing)

#ifdef  CLK2US
 #undef CLK2US
#endif
#define CLK2US(clk)    ((((uint64_t) (clk)) * 15625ULL) >> 9ULL)

#ifdef  US2CLK
 #undef US2CLK
#endif
#define US2CLK(us)     ((((uint64_t) (us)) << 9LL) / 15625LL)

#ifdef  CLK2MS
 #undef CLK2MS
#endif
#define CLK2MS(clk)    ((CLK2US(clk)) / 1000ULL)

#define SYSTEM_INITIAL_TIME         3   // Time before call scheduler() 3 == 32.768us*[3] ~= 100us
#define MINIMUM_TIME_SLICE_VALUE    200 // Next callback function will be called with in this time value default 200us

#define R_RTM_DPM_TIMER_BASE        (dg_configSCHEDULER_RTM_ADDR)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct __attribute__((packed)) _dpm_timer_
{
    union
    {
        uint32_t space;
        uint8_t  content[4];
    } type;
    dpm_timer_callback_type_t callback;
    void   * booting_offset;
    uint64_t time;
}
dpm_timer_t;

typedef struct __attribute__((packed)) _dpm_timer_map_
{
    dpm_timer_t timer[MAX_DPM_TIMER];
    uint32_t    time_slice;
    uint32_t    boot_condition;
    uint32_t    curIdx;
    uint32_t    systime_offset;
    uint32_t    systime_offset_msec;
    uint64_t    timer_offset;
    uint32_t    isr_flag;
}
dpm_timer_map_t;

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static void r_dpm_rtc_mirror_isr(void * param);

/* Retention memory value start ********************************************/
dpm_timer_map_t    * g_test_timer   = (dpm_timer_map_t *) (R_RTM_DPM_TIMER_BASE);
static dpm_timer_t * gtimer         = (dpm_timer_t *) (R_RTM_DPM_TIMER_BASE + (offsetof(dpm_timer_map_t, timer)));
static uint32_t    * time_slice     = (uint32_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, time_slice));
static uint32_t    * boot_condition = (uint32_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, boot_condition));
static uint32_t    * gs_current_id  = (uint32_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, curIdx));
static uint64_t    * time_gap       = (uint64_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, timer_offset));
static uint32_t    * isr_doing      = (uint32_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, isr_flag));
static uint64_t      isr_gap;

static dpm_timer_callback_type_t   rtc_mirror_callback;
static dpm_timer_callback_type_t * rtc_exp_callback;

static dpm_timer_id_t wakeup_timer_id = DPM_TIMER_ERR;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/
static uint64_t       r_dpm_get_remain_time(void);
static dpm_timer_id_t r_dpm_check_id(dpm_timer_id_t id);
static dpm_timer_id_t r_dpm_process_callback(dpm_timer_id_t id);
static void           r_dpm_timer_active(dpm_timer_id_t id);
static void           r_dpm_timer_callback(void);
static void           r_dpm_rtc_mirror_isr(void * param);
static dpm_timer_id_t r_dpm_time_sorting(dpm_timer_id_t current_id, dpm_timer_id_t new_one);
static dpm_timer_id_t r_dpm_timer_alloc(dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param);
static int            r_dpm_timerStart(uint64_t count, dpm_timer_callback_type_t * cb);
static int            r_dpm_timerStop(uint64_t * remain);
static int            r_rtc_timer_is_running(void);
static uint32_t       r_rtc_get_sleep_id(void);

dpm_timer_id_t             R_DPM_TIMER_Set(dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param);
BSP_PLACE_CODE_IN_RAM void RTC_IF_EXP_Handler(void);

dpm_timer_id_t R_DPM_TIMER_EmptyIdGet (void)
{
    dpm_timer_id_t i;
    for (i = DPM_TIMER_5; i < DPM_TIMER_ERR; i++)
    {
        /* if it found empty id */
        if (r_dpm_check_id(i) == i)
        {
            break;
        }
    }

    return i;
}

bool R_DPM_TIMER_IdPendingCheck (dpm_timer_id_t id)
{
    if (r_dpm_check_id(id) == DPM_TIMER_ERR)
    {
        return true;
    }
    else
    {
        return false;
    }
}

dpm_timer_id_t R_DPM_TIMER_Set (dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param)
{
    if (isr_block)
    {
        return DPM_TIMER_ERR;
    }

    if (r_dpm_timer_alloc(id, time, param) == DPM_TIMER_ERR)
    {
#if !dg_configPTIM_APP
        printf(/*RED_COLOR */ "[%s] Error alloc timer id:%d time:%d \n" /* CLEAR_COLOR*/, __func__, id, (int) time);
#endif

        return DPM_TIMER_ERR;
    }

    if (gCurIdx != DPM_TIMER_ERR)
    {
        uint64_t remain;
        if (r_dpm_timerStop(&remain))
        {
            gtimer[gCurIdx].time = CLK2US(remain);
        }

        gCurIdx = r_dpm_time_sorting(gCurIdx, id);
    }
    else
    {
        gCurIdx = id;
    }

    r_dpm_timer_active(gCurIdx);

    return id;
}

dpm_timer_id_t R_DPM_TIMER_SleepSet (dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param, uint32_t b_enterSleep)
{
    dpm_timer_id_t next = DPM_TIMER_ERR, before;
    uint64_t       temp_gap;
    uint32_t       idx = 0;

    if (b_enterSleep == false)
    {
        return R_DPM_TIMER_Set(id, time, param);
    }

    if (isr_block)
    {
        return DPM_TIMER_ERR;
    }

    if (r_dpm_timer_alloc(id, time, param) == DPM_TIMER_ERR)
    {
#if !dg_configPTIM_APP
        printf(/*RED_COLOR */ "[%s] Error alloc timer id:%d time:%d \n" /* CLEAR_COLOR*/, __func__, id, (int) time);
#endif

        return DPM_TIMER_ERR;
    }

    if (gCurIdx != DPM_TIMER_ERR)
    {
        if (r_rtc_timer_is_running())
        {
            uint64_t remain;
            if (r_dpm_timerStop(&remain))
            {
                gtimer[gCurIdx].time = CLK2US(remain);
            }
        }

        if (gTimeGap != 0)
        {
            temp_gap = CLK2US(r_dpm_get_remain_time() + 2);
            before   = gCurIdx;
            next     = gCurIdx;
            while ((next != DPM_TIMER_ERR) && (temp_gap > 0))
            {
                if (gtimer[next].time > temp_gap)
                {
                    gtimer[next].time -= temp_gap;
                    break;
                }
                else
                {
                    temp_gap         -= gtimer[next].time;
                    gtimer[next].time = 0;
                    next              = (dpm_timer_id_t) gtimer[next].type.content[DPM_IDX_NEXT];
                    if (temp_gap < MINIMUM_TIME_SLICE)
                    {
                        break;
                    }
                }

                idx++;
            }

            gCurIdx = before;
            idx     = 0;
        }

        gCurIdx = r_dpm_time_sorting(gCurIdx, id);
    }
    else
    {
        gCurIdx = id;
    }

    if (id < 2)
    {
        BOOT_TIM = 1;
    }

    /*Need update */
    ((uint32_t *) dg_configBOOTER_RTM_ADDR)[1] = (uint32_t) gtimer[gCurIdx].booting_offset;

    if (gtimer[gCurIdx].time < 1000)
    {
        gtimer[gCurIdx].time = 1000;
    }

    r_pm_low_power_mode3_enter(gCurIdx, (US2CLK(gtimer[gCurIdx].time)));

    return id;
}

dpm_timer_id_t R_DPM_TIMER_Kill (dpm_timer_id_t id)
{
    uint8_t  next;
    uint32_t idx = 0;

    if (isr_block)
    {
        return DPM_TIMER_ERR;
    }

    /* if this timer unused */
    if (r_dpm_check_id(id) == id)
    {
        return DPM_TIMER_ERR;
    }

    gtimer[id].type.content[DPM_IDX_USING] = 0;

    if (gCurIdx == id)
    {
        uint64_t remain = 0;
        r_dpm_timerStop(&remain);

        next = gtimer[id].type.content[DPM_IDX_NEXT];
        if (next == DPM_TIMER_ERR)
        {
            gCurIdx = DPM_TIMER_ERR;

            return id;
        }

        gtimer[next].time += CLK2US(remain);
        gCurIdx            = (dpm_timer_id_t) next;
        r_dpm_timer_active(gCurIdx);
    }
    else
    {
        next = (uint8_t) gCurIdx;
        while (gtimer[next].type.content[DPM_IDX_NEXT] != id)
        {
            next = gtimer[next].type.content[DPM_IDX_NEXT];
            idx++;
        }

        gtimer[next].type.content[DPM_IDX_NEXT] = gtimer[id].type.content[DPM_IDX_NEXT];

        if (gtimer[id].type.content[DPM_IDX_NEXT] != DPM_TIMER_ERR)
        {
            gtimer[gtimer[id].type.content[DPM_IDX_NEXT]].time += gtimer[id].time;
        }
    }

    return id;
}

BSP_PLACE_CODE_IN_RAM bsp_wakeup_source_mask_t R_DPM_TIMER_WakeupHandler (void)
{
    uint32_t wakeup_source;
    uint32_t wakeup_id, idx = 0, next = DPM_TIMER_ERR;

    /* wake up source read */
    wakeup_source = R_BSP_WakeupSourceGet();
    wakeup_id     = r_rtc_get_sleep_id();

    gCurIdx = (dpm_timer_id_t) wakeup_id;

    gTimeGap = 0;

    switch (wakeup_source)
    {
        case BSP_WAKEUP_COUNTER_WITH_RETENTION:
        case BSP_WAKEUP_GPIO_WAKEUP_COUNTER_WITH_RETENTION:
        case BSP_WAKEUP_SENSOR_WAKEUP_COUNTER_WITH_RETENTION:
        {
            if (wakeup_id < 2)
            {
                if (BOOT_TIM)
                {
                    gtimer[wakeup_id].type.content[DPM_IDX_USING] = 0;
                    BOOT_TIM = 0;
                    gCurIdx  = (dpm_timer_id_t) gtimer[wakeup_id].type.content[DPM_IDX_NEXT];

                    gTimeGap = R_BSP_SystemRtcCountGet();

                    return wakeup_source;
                }
                else
                {
                    gTimeGap = r_dpm_get_remain_time();
                    gtimer[wakeup_id].type.content[DPM_IDX_USING] = 1;
                }
            }
            else
            {
                gCurIdx  = (dpm_timer_id_t) wakeup_id;
                gTimeGap = r_dpm_get_remain_time();
            }

            /* callback current timer id */
            next     = r_dpm_process_callback((dpm_timer_id_t) wakeup_id);
            gTimeGap = 0;

            /* wake-up source clearing */
            R_BSP_WakeupSourceClear(true);

            /* enable next timer */
            gTimeGap = R_BSP_SystemRtcCountGet();

            if ((next != DPM_TIMER_ERR) && (gtimer[next].type.content[DPM_IDX_USING]))
            {
                gCurIdx = (dpm_timer_id_t) next;
                r_dpm_timer_active(gCurIdx);
            }
            else
            {
                gCurIdx = DPM_TIMER_ERR;
            }

            gTimeGap = 0;
            break;
        }

        case BSP_WAKEUP_GPIO_WITH_RETENTION:
        case BSP_WAKEUP_SENSOR_WITH_RETENTION:
        case BSP_WAKEUP_SENSOR_GPIO_WITH_RETENTION:
        {
            if (wakeup_id < 2)
            {
                gtimer[wakeup_id].type.content[DPM_IDX_USING] = 0;
                gCurIdx               = (dpm_timer_id_t) gtimer[wakeup_id].type.content[DPM_IDX_NEXT];
                gtimer[gCurIdx].time += CLK2US(r_dpm_get_remain_time());
            }
            else
            {
                gCurIdx              = (dpm_timer_id_t) wakeup_id;
                gtimer[gCurIdx].time = CLK2US(r_dpm_get_remain_time());
            }

            /* wake-up source clearing */
            R_BSP_WakeupSourceClear(true);

            /* enable next timer */
            if ((gCurIdx != DPM_TIMER_ERR) && (gtimer[gCurIdx].type.content[DPM_IDX_USING]))
            {
                r_dpm_timer_active(gCurIdx);
            }
            else
            {
                gCurIdx = DPM_TIMER_ERR;
            }

            break;
        }

        case BSP_WAKEUP_SOURCE_POR:
        case BSP_WAKEUP_RESET:
        case BSP_WAKEUP_RESET_WITH_RETENTION:
        case BSP_WAKEUP_SOURCE_WAKEUP_COUNTER:
        case BSP_WAKEUP_SOURCE_WATCHDOG:
        case BSP_WAKEUP_WATCHDOG_GPIO:
        case BSP_WAKEUP_SOURCE_SENSOR:
        case BSP_WAKEUP_SOURCE_PULSE:
        case BSP_WAKEUP_SOURCE_GPIO:
        case BSP_WAKEUP_SENSOR_GPIO:
        case BSP_WAKEUP_SENSOR_WAKEUP_COUNTER:
        case BSP_WAKEUP_SENSOR_GPIO_COUNTER:
        case BSP_WAKEUP_SENSOR_WATCHDOG:
        case BSP_WAKEUP_SENSOR_GPIO_WATCHDOG:
        case BSP_WAKEUP_GPIO_WAKEUP_COUNTER:
        {
            R_BSP_WakeupSourceClear(true);
            MINIMUM_TIME_SLICE = MINIMUM_TIME_SLICE_VALUE;
            BOOT_TIM           = 0;
            memset((void *) &gtimer[0], 0x00, sizeof(dpm_timer_t) * MAX_DPM_TIMER);
            for (idx = 0; idx < DPM_TIMER_ERR; idx++)
            {
                gtimer[idx].type.content[DPM_IDX_NEXT] = DPM_TIMER_ERR;
            }

            gCurIdx   = DPM_TIMER_ERR;
            isr_block = 0;
            break;
        }

        default:
        {
            gCurIdx = DPM_TIMER_ERR;
            break;
        }
    }

    return wakeup_source;
}

static dpm_timer_id_t r_dpm_timer_alloc (dpm_timer_id_t id, uint64_t time, dpm_timer_param_t param)
{
    if (r_dpm_check_id(id) == DPM_TIMER_ERR)
    {
        return DPM_TIMER_ERR;
    }

    gtimer[id].type.content[DPM_IDX_USING] = 1;

    gtimer[id].type.content[DPM_IDX_NEXT]         = DPM_TIMER_ERR;
    gtimer[id].type.content[DPM_IDX_POWER_STATUS] = DPM_POWER_ON;
    gtimer[id].callback.func  = (void (*)(void *))(param.callback_func);
    gtimer[id].callback.param = param.callback_param;
    gtimer[id].time           = time;

    gtimer[id].booting_offset = param.booting_offset;

    return id;
}

// returns time left until the programmed wakeup counter in RTC clocks.
// returns 0 if programmed time already passed.
static BSP_PLACE_CODE_IN_RAM uint64_t r_dpm_get_remain_time (void)
{
    volatile uint64_t   wakeup;
    volatile uint32_t * ptemp = (uint32_t *) &wakeup;

    ptemp[0] = RTC->WAKEUP_CNT_0_REG;
    ptemp[1] = RTC->WAKEUP_CNT_1_REG;

    uint64_t now = R_BSP_SystemRtcCountGet();

    return R_BSP_SystemRtcDiff(wakeup, now);
}

static dpm_timer_id_t r_dpm_check_id (dpm_timer_id_t id)
{
    if (id >= DPM_TIMER_ERR)
    {
        return DPM_TIMER_ERR;
    }

    if (gtimer[id].type.content[DPM_IDX_USING])
    {
        return DPM_TIMER_ERR;
    }
    else
    {
        return id;
    }
}

static BSP_PLACE_CODE_IN_RAM dpm_timer_id_t r_dpm_process_callback (dpm_timer_id_t id)
{
    dpm_timer_id_t    next = DPM_TIMER_ERR, before;
    volatile uint64_t temp_gap;
    if (gtimer[id].type.content[DPM_IDX_USING])
    {
        if (gtimer[id].callback.func != NULL)
        {
            wakeup_timer_id = id;
            gtimer[id].callback.func(gtimer[id].callback.param);
        }

        gtimer[id].type.content[DPM_IDX_USING] = 0;
        gtimer[id].time = 0;

        next = (dpm_timer_id_t) gtimer[gCurIdx].type.content[DPM_IDX_NEXT];

        if (gTimeGap)
        {
            temp_gap = CLK2US(r_dpm_get_remain_time() + SYSTEM_INITIAL_TIME);
            before   = next;
            while ((next != DPM_TIMER_ERR) && (temp_gap > 0))
            {
                if (gtimer[next].time > temp_gap)
                {
                    gtimer[next].time -= temp_gap;
                    break;
                }
                else
                {
                    temp_gap         -= gtimer[next].time;
                    gtimer[next].time = 0;
                    next              = (dpm_timer_id_t) gtimer[next].type.content[DPM_IDX_NEXT];
                    if (temp_gap < MINIMUM_TIME_SLICE)
                    {
                        break;
                    }
                }
            }

            next = before;
        }

        while ((next != DPM_TIMER_ERR) && (gtimer[next].time <= MINIMUM_TIME_SLICE))
        {
            if (gtimer[next].callback.func != NULL)
            {
                gtimer[next].callback.func(gtimer[next].callback.param);
            }

            gtimer[next].type.content[DPM_IDX_USING] = 0;

            next = (dpm_timer_id_t) gtimer[next].type.content[DPM_IDX_NEXT];
        }
    }

    return next;
}

dpm_timer_id_t r_dpm_wakeup_timer_id_get (void)
{
    return wakeup_timer_id;
}

void r_dpm_wakeup_timer_id_set (dpm_timer_id_t timer_id)
{
    wakeup_timer_id = timer_id;
}

static BSP_PLACE_CODE_IN_RAM void r_dpm_timer_active (dpm_timer_id_t id)
{
    uint64_t temp;
    r_dpm_timerStop(&temp);

    rtc_mirror_callback.func  = &r_dpm_rtc_mirror_isr;
    rtc_mirror_callback.param = (void *) id;
    r_dpm_timerStart(US2CLK(gtimer[id].time), &rtc_mirror_callback);
}

static void r_dpm_timer_callback (void)
{
    gCurIdx = r_dpm_process_callback(gCurIdx);

    if (gCurIdx != DPM_TIMER_ERR)
    {
        // uint64_t diff = CLK2US(R_BSP_SystemRtcDiff(isr_gap, R_BSP_SystemRtcCountGet()));
        // gtimer[gCurIdx].time = gtimer[gCurIdx].time > diff ? gtimer[gCurIdx].time - diff : 0;
        gtimer[gCurIdx].time -= CLK2US(R_BSP_SystemRtcDiff(isr_gap, R_BSP_SystemRtcCountGet()));

        // TODO: check gtimer[gCurIdx].time is not 'negative', and above minimal time slice?
        r_dpm_timer_active(gCurIdx);
    }
}

static void r_dpm_rtc_mirror_isr (void * param)
{
    FSP_PARAMETER_NOT_USED(param);

    isr_block = 1;
    isr_gap   = R_BSP_SystemRtcCountGet();
    r_dpm_timer_callback();
    isr_block = 0;
}

static dpm_timer_id_t r_dpm_time_sorting (dpm_timer_id_t current_id, dpm_timer_id_t new_one)
{
    uint32_t       idx = 1;
    dpm_timer_id_t ret = current_id, before = current_id;

    while (gtimer[current_id].type.content[DPM_IDX_USING] == 1)
    {
        if (gtimer[current_id].time > gtimer[new_one].time)
        {
            gtimer[current_id].time -= gtimer[new_one].time;
            if (current_id == before)
            {
                gtimer[new_one].type.content[DPM_IDX_NEXT] = current_id;

                return new_one;
            }
            else
            {
                gtimer[before].type.content[DPM_IDX_NEXT]  = new_one;
                gtimer[new_one].type.content[DPM_IDX_NEXT] = current_id;

                return ret;
            }
        }
        else if (gtimer[current_id].time < gtimer[new_one].time)
        {
            gtimer[new_one].time -= gtimer[current_id].time;
            if (gtimer[current_id].type.content[DPM_IDX_NEXT] != DPM_TIMER_ERR)
            {
                before     = current_id;
                current_id = (dpm_timer_id_t) gtimer[current_id].type.content[DPM_IDX_NEXT];
            }
            else
            {
                gtimer[current_id].type.content[DPM_IDX_NEXT] = new_one;

                return ret;
            }
        }
        else
        {
            gtimer[new_one].type.content[DPM_IDX_NEXT]    = gtimer[current_id].type.content[DPM_IDX_NEXT];
            gtimer[current_id].type.content[DPM_IDX_NEXT] = new_one;
            gtimer[new_one].time = 0;

            return ret;
        }

        idx++;
    }

    if (gtimer[ret].time > gtimer[new_one].time)
    {
        return new_one;
    }
    else
    {
        return ret;
    }
}

void R_DPM_TIMER_PrintList (void)
{
    dpm_timer_id_t next = DPM_TIMER_ERR;
    if (gCurIdx != DPM_TIMER_ERR)
    {
        printf("current ID: %d time(ms): %lu\n", (int) gCurIdx, (uint32_t) (gtimer[gCurIdx].time / 1000));
        next = gtimer[gCurIdx].type.content[DPM_IDX_NEXT];
        while (next != DPM_TIMER_ERR)
        {
            printf("next ID: %d time(ms): %lu\n", next, (uint32_t) (gtimer[next].time / 1000));
            next = gtimer[next].type.content[DPM_IDX_NEXT];
        }
    }
    else
    {
        printf("timer non\n");
    }
}

static uint32_t clear_exp_timer (uint32_t clear)
{
    uint32_t dumpreg;

    if (clear == true)
    {
        RTC->RTC_EXP_CLR_IRQ_REG_b.RTC_EXP_CLR_IRQ = clear & 0x1;
        rtc_exp_callback = NULL;       // unregistry
    }

    dumpreg = RTC->RTC_EXP_OP_STS_REG;

    return dumpreg;
}

static int enable_exp_timer_interrupt (uint32_t enable)
{
    if (enable)
    {
        R_BSP_IrqCfgEnable(RTC_IF_EXP_IRQn, 3, NULL);
    }
    else
    {
        NVIC_DisableIRQ(RTC_IF_EXP_IRQn);
    }

    return 0;
}

static int set_exp_timer (uint64_t exptime, void * cb)
{
    if ((rtc_exp_callback != NULL) || (RTC->RTC_EXP_OP_STS_REG_b.RTC_EXP_OP_STATUS))
    {
        return false;                  // running now !!
    }

    rtc_exp_callback = cb;

    RTC->RTC_EXP_IRQ_EN_REG_b.RTC_EXP_IRQ_EN = 0x07;

    RTC->RTC_EXP_TH0_REG_b.RTC_EXP_TH0 = (uint32_t) exptime;
    RTC->RTC_EXP_TH1_REG_b.RTC_EXP_TH1 = (uint32_t) ((exptime >> 32) & 0xF);

    RTC->RTC_EXP_CLR_IRQ_REG_b.RTC_EXP_START = (uint32_t) (exptime & 0x1);
    RTC->RTC_EXP_CLR_IRQ_REG                 = RTC_RTC_EXP_CLR_IRQ_REG_RTC_EXP_START_Msk |
                                               RTC_RTC_EXP_CLR_IRQ_REG_RTC_EXP_CLR_IRQ_Msk;

    return true;
}

BSP_PLACE_CODE_IN_RAM void RTC_IF_EXP_Handler (void)
{
    if ((rtc_exp_callback != NULL) && (rtc_exp_callback->func != NULL))
    {
        rtc_exp_callback->func(rtc_exp_callback->param);
    }
}

BSP_PLACE_CODE_IN_RAM void RTC_Handler(void);

BSP_PLACE_CODE_IN_RAM void RTC_Handler (void)
{
    // TODO rtc handler
}

static int r_dpm_timerStart (uint64_t count, dpm_timer_callback_type_t * cb)
{
    clear_exp_timer(true);
    enable_exp_timer_interrupt(true);

    return set_exp_timer(count + R_BSP_SystemRtcCountGet(), cb);
}

static int r_dpm_timerStop (uint64_t * remain)
{
    uint64_t temp = 0;

    enable_exp_timer_interrupt(false);
    clear_exp_timer(true);
    temp  = RTC->RTC_EXP_TH0_REG_b.RTC_EXP_TH0;
    temp |= ((uint64_t) (RTC->RTC_EXP_TH1_REG_b.RTC_EXP_TH1) << 32);
    uint64_t rtc  = R_BSP_SystemRtcCountGet();
    uint64_t diff = R_BSP_SystemRtcDiff(rtc, temp);
    *remain = diff;

    return (int) (diff != 0);
}

/* return true if it is running */
static int r_rtc_timer_is_running (void)
{
    return RTC->RTC_EXP_OP_STS_REG_b.RTC_EXP_OP_STATUS;
}

static BSP_PLACE_CODE_IN_RAM uint32_t r_rtc_get_sleep_id (void)
{
    return RTC->RTM_CONTROL_REG_b.RTM_CTRL_PWR_DN_INFO;
}

int rtc_timer_info (int tid)
{
    dpm_timer_id_t first_tid   = DPM_TIMER_ERR;
    dpm_timer_id_t next_tid    = DPM_TIMER_ERR;
    unsigned long  first_msec  = 0;
    unsigned long  before_msec = 0;
    unsigned long  next_msec   = 0;
    int            i           = 0;

    dpm_timer_t     * p_gtimer      = (dpm_timer_t *) (R_RTM_DPM_TIMER_BASE + (offsetof(dpm_timer_map_t, timer)));
    static uint32_t * sp_current_id = (uint32_t *) (R_RTM_DPM_TIMER_BASE + offsetof(dpm_timer_map_t, curIdx));
#define curidx    *sp_current_id

    if (curidx < DPM_TIMER_ERR)
    {
        first_tid  = (dpm_timer_id_t) curidx;
        first_msec = (unsigned int) (p_gtimer[first_tid].time / 1000);

        if (first_tid == tid)
        {
            return (int) first_msec;
        }

        i        = 0;
        next_tid = first_tid;

        before_msec = first_msec;
        while (i < DPM_TIMER_ERR)
        {
            next_tid  = (dpm_timer_id_t) p_gtimer[next_tid].type.content[DPM_IDX_NEXT];
            next_msec = (unsigned int) (p_gtimer[next_tid].time / 1000) + before_msec;
            if (next_tid == tid)
            {
                return (int) next_msec;
            }

            if (next_tid >= DPM_TIMER_ERR)
            {
                break;
            }

            i++;
            before_msec = next_msec;
        }
    }

    return 0;
}
