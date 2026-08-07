/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <stdio.h>
#include <strings.h>
#include "bsp_api.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#include "r_pm_if.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define UNDEFINE_NOTIFIER_ID 255
#define UNDEFINE_NOTIFIER_ORDER 0
#define BIT(x) (1 << (x))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/


/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
pmgr_instance_ctrl_t * g_p_ctrl = NULL;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/
const pmgr_api_t g_pmgr_w_api =
{
    .open                    = RM_PMGR_W_Open,
    .close                   = RM_PMGR_W_Close,
    .notifier_register       = RM_PMGR_W_notifier_register,
    .enter_idle              = RM_PMGR_W_enter_idle,
    .add_sleep_constraint    = RM_PMGR_W_add_sleep_constraint,
    .remove_sleep_constraint = RM_PMGR_W_remove_sleep_constraint,
};

typedef struct pmgr_w_info_sleep_mode
{
    pmgr_lld_power_mode_t lld_power_mode;        /// which power mode
    pmgr_wake_source_t not_allowed_wake_sources; /// which wake sources are not supported for this sleep mode
    pmgr_constraints_t off_components;           /// which constraints are not supported for this sleep mode
    uint64_t min_sleep_time_us;                  /// mode may be chosen for sleep times greater then this time
    uint32_t sleep_exit_latency_us;                 /// Time which takes to exist from mode back to active
} g_pmgr_w_info_sleep_mode_t;

/* The wake source/constatins which set for spesic mode are not allowed for this mode */
g_pmgr_w_info_sleep_mode_t info_power_modes_table[] =
{
    {
        .lld_power_mode = PMGR_LLD_POWER_MODE_SLEEP2,
        .not_allowed_wake_sources = PMGR_WAKE_SOURCE_GPT |
                                    PMGR_WAKE_SOURCE_WIFI,
        .off_components = PMGR_CONSTRAINT_POWER_RAM |
                          PMGR_CONSTRAINT_POWER_MAC_HW |
                          PMGR_CONSTRAINT_POWER_RETENTION |
                          PMGR_CONSTRAINT_SLEEP_PROHIBITED,
        .min_sleep_time_us = 3000,
        .sleep_exit_latency_us = 0,
    },
    {
        .lld_power_mode = PMGR_LLD_POWER_MODE_SLEEP3,
        .not_allowed_wake_sources = PMGR_WAKE_SOURCE_GPT |
                                    PMGR_WAKE_SOURCE_WIFI,
        .off_components = PMGR_CONSTRAINT_POWER_RAM |
                          PMGR_CONSTRAINT_POWER_MAC_HW |
                          PMGR_CONSTRAINT_SLEEP_PROHIBITED,
        .min_sleep_time_us = 3000,
        .sleep_exit_latency_us = 0,
    },
    {
        .lld_power_mode = PMGR_LLD_POWER_MODE_DPM,
        .not_allowed_wake_sources = PMGR_WAKE_SOURCE_GPT,
        .off_components = PMGR_CONSTRAINT_POWER_RAM |
                          PMGR_CONSTRAINT_SLEEP_PROHIBITED,
        .min_sleep_time_us = 0,
        .sleep_exit_latency_us = 0,
    },
    {
        .lld_power_mode = PMGR_LLD_POWER_MODE_SLEEP4,
        .not_allowed_wake_sources = PMGR_WAKE_SOURCE_GPT,
        .off_components = PMGR_CONSTRAINT_POWER_MAC_HW |
                          PMGR_CONSTRAINT_SLEEP_PROHIBITED,
        .min_sleep_time_us = 3000,
        .sleep_exit_latency_us = 4300,
    },
    {
        /* The sleep mode is always valid */
        .lld_power_mode = PMGR_LLD_POWER_MODE_CPU_WFI,
        .not_allowed_wake_sources = PMGR_WAKE_SOURCE_NONE,
        .off_components = PMGR_CONSTRAINT_NONE,
        .min_sleep_time_us = 0,
        .sleep_exit_latency_us = 0,
    }
};

#if CFG_WIFI
/*dpm sleep list to manage by dpm sleep daemon*/
pmgr_w_info_dpm_sleep_t    pmgr_w_info_dpm_sleep_list[MAX_DPM_INFO_ARRAY] = { 0, };
#endif /*CFG_WIFI*/

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/
/* Update all the args of registered application about constarins, wake source, event, and the selected power mode */
static void RM_PMGR_W_update_arg_items(pmgr_ctrl_t * const p_ctrl, pmgr_lld_power_mode_t power_mode,
                                       uint8_t entry_idx, pmgr_event_t event)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    notifier_array_entry_t *notifier_array = p_instance_ctrl->notifier_array;
    pmgr_callback_args_t * p_current_callback_memory = notifier_array[entry_idx].p_callback_memory;
    pmgr_instance_info_t *p_current_instance_info = (pmgr_instance_info_t *)p_current_callback_memory->p_instance_info;
    uint64_t tickless_idle_rtc = 0;
    uint8_t bit_i;

    if ((power_mode == PMGR_LLD_POWER_MODE_SLEEP4) &&
        (event == PMGR_EVENT_EXITING_SLEEP))
    {
        /* Get tickless idle duration */
        tickless_idle_rtc = R_PM_TicklessRtcDeltaGet();
    }

    /* Fill instance specific info (when supplied in callback memory) */
    if (p_current_instance_info != NULL)
    {
        p_current_instance_info->power_mode = power_mode;
        p_current_instance_info->wake_source = p_instance_ctrl->wake_source_bitmap;
        p_current_instance_info->tickless_time_us = RTC_TICKS_TO_US(tickless_idle_rtc);
    }

    /* Fill PMGR common info */
    p_current_callback_memory->constraints = 0;

    /* Convert the constraints array to bitmask */
    for (bit_i = 0; bit_i < PMGR_CONSTRAINT_MAX; bit_i++)
    {
        if (p_instance_ctrl->constraint_counter[bit_i])
        {
            p_current_callback_memory->constraints |= (1 << bit_i);
        }
    }

    /* Update the arg of registered application about event */
    p_current_callback_memory->event = event;
}

/* Update all the callback functions about the parameters that the decision was chosen with */
static void RM_PMGR_W_update_all_cb_args(pmgr_ctrl_t * const p_ctrl, pmgr_lld_power_mode_t power_mode,
                                         pmgr_event_t event)
{
    uint8_t i = 0;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    notifier_array_entry_t *notifier_array = p_instance_ctrl->notifier_array;

    /* Execut callback functions by the highest order */
    for (i = 0; i < PMGR_MAX_NOTIFIER_ARRAY; i++)
    {
        if (notifier_array[i].p_callback && notifier_array[i].p_callback_memory)
        {
            RM_PMGR_W_update_arg_items(p_ctrl, power_mode, i, event);
        }
    }
}

/* Call all registered function by the order */
static void RM_PMGR_W_call_registered_cb_func(pmgr_ctrl_t * const p_ctrl, bool reversed)
{
    uint8_t i = 0;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    notifier_array_entry_t *notifier_array = p_instance_ctrl->notifier_array;

    if (reversed)
    {
        /* Execute callback functions by the highest order */
        for (i = PMGR_MAX_NOTIFIER_ARRAY; i != 0; i--)
        {
            if (notifier_array[i - 1].p_callback)
            {
                notifier_array[i - 1].p_callback((pmgr_callback_args_t *)
                    notifier_array[i - 1].p_callback_memory);
            }
        }
    }
    else
    {
        /* Execute callback functions by the highest order */
        for (i = 0; i < PMGR_MAX_NOTIFIER_ARRAY; i++)
        {
            if (notifier_array[i].p_callback)
            {
                notifier_array[i].p_callback((pmgr_callback_args_t *)
                    notifier_array[i].p_callback_memory);
            }
        }
    }
}

static void RM_PMGR_W_sort_notifier_array(pmgr_ctrl_t * const p_ctrl)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    notifier_array_entry_t *notifier_array = p_instance_ctrl->notifier_array;
    uint8_t i, j;

    /* Sort notifier_array by the order using bubble sort algorithm,
     * higher order will locate first, undefine entries will be at the last places
     */
    for(i = 0; i < PMGR_MAX_NOTIFIER_ARRAY; i++)
    {
        for (j = 0; j < PMGR_MAX_NOTIFIER_ARRAY - i - 1; j++)
        {
            /* Swap if the element found is smaller than the next element */
            if (notifier_array[j].notify_extend.order < notifier_array[j + 1].notify_extend.order)
            {
                notifier_array_entry_t temp = notifier_array[j];

                notifier_array[j].notify_extend = notifier_array[j + 1].notify_extend;
                notifier_array[j].p_callback = notifier_array[j + 1].p_callback;
                notifier_array[j].p_callback_memory = notifier_array[j + 1].p_callback_memory;

                notifier_array[j + 1].notify_extend = temp.notify_extend;
                notifier_array[j + 1].p_callback = temp.p_callback;
                notifier_array[j + 1].p_callback_memory = temp.p_callback_memory;
            }
        }
    }
}

/* Find the available entry index at the array by the entry that does not have cb func */
static uint8_t RM_PMGR_W_find_available_entry(notifier_array_entry_t *notifier_array, size_t size)
{
    uint8_t available_entry_index;

    for(available_entry_index = 0; available_entry_index < size; available_entry_index++)
    {
        if (!notifier_array[available_entry_index].p_callback)
        {
            return available_entry_index;
        }
    }

    /* notifier_array is full, there is no available entry index */
    return UNDEFINE_NOTIFIER_ID;
}

/* Find the smallest unique notifier id at the array (range: 0 - size-1) which is unused */
static uint8_t RM_PMGR_W_find_available_notifier_id(notifier_array_entry_t *notifier_array, size_t size)
{
    uint8_t i, notifier_id = 0;

    while(notifier_id < size)
    {
        for(i = 0; i < size; i++)
        {
            if (notifier_array[i].notify_extend.notifier_id == notifier_id)
            {
                /* This unique notifier id already in use */
                notifier_id++;
                continue;
            }
        }

        return notifier_id;
    }

    /* notifier_array is full, there is no available unique notifier id at range: 0 - size-1 */
    return UNDEFINE_NOTIFIER_ID;
}

static bool RM_PMGR_W_is_valid_constraints(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraints)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    uint8_t bit_i;

    for (bit_i = 0; bit_i < PMGR_CONSTRAINT_MAX; bit_i++)
    {
        /* Check if the bit index is signed on at bitmap constraints and the constraint is set at the instance */
        if (constraints & (1 << bit_i) && p_instance_ctrl->constraint_counter[bit_i])
        {
            return false;
        }
    }

    /* Constraint is not set/not relevat for this sleep mode */
    return true;
}

static pmgr_lld_power_mode_t RM_PMGR_W_trigger_mechanism(pmgr_ctrl_t * const p_ctrl, uint64_t idle_time_us, uint32_t *sleep_exit_latency_us)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    int max_modes = sizeof(info_power_modes_table) / sizeof(info_power_modes_table[0]);

    /* Power mode is forced */
    if (p_instance_ctrl->lld_power_mode != PMGR_LLD_POWER_MODE_AUTO)
    {
        return p_instance_ctrl->lld_power_mode;
    }

    for (int i = 0; i < max_modes; i++)
    {
        g_pmgr_w_info_sleep_mode_t * mode_entry = &info_power_modes_table[i];

        bool wake_source_valid = !(mode_entry->not_allowed_wake_sources & p_instance_ctrl->wake_source_bitmap);
        bool constraint_valid = RM_PMGR_W_is_valid_constraints(p_ctrl, mode_entry->off_components);
        bool sleep_time_valid = idle_time_us >= mode_entry->min_sleep_time_us + mode_entry->sleep_exit_latency_us;

        if (wake_source_valid && constraint_valid && sleep_time_valid)
        {
            *sleep_exit_latency_us = mode_entry->sleep_exit_latency_us;
            return mode_entry->lld_power_mode;
        }
    }

    /* Always valid sleep mode */
    return PMGR_LLD_POWER_MODE_CPU_WFI;
}

/***********************************************************************************************************************
* Function Name: R_PMGR_GetWakeupMode
* Description  : Determines the wakeup source and returns the corresponding sleep mode level.
*                This function checks the wakeup source using R_BSP_WakeupSourceGet() and classifies the wakeup
*                into one of the following categories:
*                - Reset, POR (Sleep1), or Watchdog reset
*                - Sleep2 wakeup (GPIO, Wakeup Counter, Watchdog, Sensor)
*                - Sleep3 or DPM (Deep Power Management) wakeup
* Arguments    : None
* Return Value : uint16_t
*                  - 1 : Wakeup due to Reset, POR(Sleep1), or Watchdog
*                  - 2 : Wakeup from Sleep2 (non-retention wakeup sources)
*                  - 3 : Wakeup from Sleep3 (retention reset without DPM event)
*                  - 4 : Wakeup from DPM (retention reset with DPM event)
***********************************************************************************************************************/
static uint16_t R_PMGR_GetWakeupMode(void)
{
    bsp_wakeup_source_mask_t wakeup_type = R_BSP_WakeupSourceGet();
    int wakeup_type_sleep2 = (BSP_WAKEUP_SOURCE_GPIO | BSP_WAKEUP_SOURCE_WAKEUP_COUNTER |
                              BSP_WAKEUP_SOURCE_WATCHDOG | BSP_WAKEUP_SOURCE_SENSOR);

    /* Check for Reset, POR (Sleep1), or Watchdog reset */
    if ((wakeup_type == BSP_WAKEUP_RESET)
        || (wakeup_type &  BSP_WAKEUP_SOURCE_POR)
        || (wakeup_type & BSP_WAKEUP_SOURCE_WATCHDOG)
        || (wakeup_type == BSP_WAKEUP_RESET_WITH_RETENTION)) {
        return 1;
    }
    /* Check for Sleep2 wakeup (non-retention sources) */
    else if (((wakeup_type & BSP_WAKEUP_RESET_WITH_RETENTION) == 0) &&
             ((wakeup_type & wakeup_type_sleep2) != 0)) {
        return 2;
    }
    /* Check for Sleep3 or DPM wakeup (retention reset) */
    else if (wakeup_type & BSP_WAKEUP_RESET_WITH_RETENTION) {
        /* Wakeup from Sleep3 (no DPM event detected) */
        return 3;
    }

    /* Default fallback */
    return 1;
}

#if CFG_WIFI
fsp_err_t RM_PMGR_W_dpm_notify(int dpm_event)
{
    FSP_ASSERT(g_p_ctrl != NULL);

    RM_PMGR_W_update_all_cb_args(g_p_ctrl, PMGR_LLD_POWER_MODE_DPM, dpm_event);
    RM_PMGR_W_call_registered_cb_func(g_p_ctrl, false);

    return FSP_SUCCESS;
}
#endif /* CFG_WIFI */

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup PMGR_W
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Perform any necessary initialization and add constraint for RAM
 *
 * @retval     FSP_SUCCESS                   PMGR instance opened
 * @retval     FSP_ERR_OUT_OF_MEMORY         Alloc mutex failed
 * @retval     FSP_ERR_ALREADY_OPEN          PMGR instance is already open
 **********************************************************************************************************************/
fsp_err_t RM_PMGR_W_Open (pmgr_ctrl_t * const p_api_ctrl, pmgr_cfg_t const * const p_cfg)
{
    FSP_ASSERT(p_api_ctrl != NULL);
    FSP_ASSERT(p_cfg != NULL);
    fsp_err_t err;

    if (g_p_ctrl)
    {
        return FSP_ERR_ALREADY_OPEN;
    }

    pmgr_instance_ctrl_t * p_ctrl = (pmgr_instance_ctrl_t *) p_api_ctrl;
    notifier_array_entry_t *notifier_array = p_ctrl->notifier_array;
    uint8_t i;
    uint8_t bit_i;

    /* Save the configuration */
    p_ctrl->p_cfg = p_cfg;
    p_ctrl->lld_power_mode = PMGR_LLD_POWER_MODE_AUTO;

    /* Initialize wake source bitmap */
    p_ctrl->wake_source_bitmap = 0;

    /* Initialize notifier array */
    for (i = 0; i < PMGR_MAX_NOTIFIER_ARRAY; i++)
    {
        notifier_array[i].notify_extend.order = UNDEFINE_NOTIFIER_ORDER;
        notifier_array[i].notify_extend.notifier_id = UNDEFINE_NOTIFIER_ID;
        notifier_array[i].p_callback = NULL;
        notifier_array[i].p_callback_memory = NULL;
    }

    for (i = 0; i < PMGR_DBG_RAM_CONSTRAINT_MAX; i++)
    {
        p_ctrl->ram_counter_cause[i] = 0;
    }

    for (bit_i = 0; bit_i < PMGR_CONSTRAINT_MAX; bit_i++)
    {
        p_ctrl->constraint_counter[bit_i] = 0;
    }

    p_ctrl->debug_runtime_flag = PMGR_DEBUG_RAM_DISABLED;

    g_p_ctrl = p_api_ctrl;

    if (!RM_PMGR_W_get_ctrl())
    {
        err = FSP_ERR_ASSERTION;
        return err;
    }

    err = RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);

    FSP_ERROR_RETURN(err == FSP_SUCCESS, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Close the PMGR Instance
 *
 * @retval     FSP_SUCCESS        PMGR driver closed
 * @retval     FSP_ERR_ASSERTION  Null Pointer
 * @retval     FSP_ERR_NOT_OPEN   PMGR instance is not open yet
 **********************************************************************************************************************/
fsp_err_t RM_PMGR_W_Close (pmgr_ctrl_t * const p_api_ctrl)
{
    pmgr_instance_ctrl_t * p_ctrl = (pmgr_instance_ctrl_t *) p_api_ctrl;

    FSP_ASSERT(p_api_ctrl != NULL);

    if (g_p_ctrl)
    {
        p_ctrl->p_cfg = NULL;
    }
    else
    {
        return FSP_ERR_NOT_OPEN;
    }

    g_p_ctrl = NULL;

    return FSP_SUCCESS;
}

/******************************************************************************************************************//**
 * Register notifier_id to PMGR Instance and sort notifier array by highest order first
 *
 * @retval     FSP_SUCCESS               Registration done successfully
 * @retval     FSP_ERR_UNSUPPORTED       Registration failed
 * @retval     FSP_ERR_INVALID_MODE      Registration failed because there is no available entry
 * @retval     FSP_ERR_OUT_OF_MEMORY     Registration failed because there is no available memory
 *********************************************************************************************************************/
fsp_err_t RM_PMGR_W_notifier_register(pmgr_ctrl_t * const p_ctrl, void (* p_callback)(pmgr_callback_args_t *),
                                      pmgr_callback_args_t * const p_callback_memory, void const * p_extend)
{
    uint8_t entry_inx = UNDEFINE_NOTIFIER_ID;
    uint8_t unique_notifier_id = UNDEFINE_NOTIFIER_ID;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    pmgr_w_notifier_extend_t * p_pmgr_w_extend = (pmgr_w_notifier_extend_t *) p_extend;

    FSP_ASSERT(p_ctrl != NULL);

    entry_inx = RM_PMGR_W_find_available_entry(p_instance_ctrl->notifier_array, PMGR_MAX_NOTIFIER_ARRAY);
    if (entry_inx == UNDEFINE_NOTIFIER_ID)
    {
        return FSP_ERR_INVALID_MODE;
    }

    unique_notifier_id = RM_PMGR_W_find_available_notifier_id(p_instance_ctrl->notifier_array, PMGR_MAX_NOTIFIER_ARRAY);
    if (unique_notifier_id == UNDEFINE_NOTIFIER_ID)
    {
        return FSP_ERR_INVALID_MODE;
    }

    if (!p_callback_memory)
    {
        return FSP_ERR_INVALID_POINTER;
    }

    p_pmgr_w_extend->notifier_id = unique_notifier_id;
    p_instance_ctrl->notifier_array[entry_inx].notify_extend.notifier_id = unique_notifier_id;
    p_instance_ctrl->notifier_array[entry_inx].notify_extend.order = p_pmgr_w_extend->order;
    p_instance_ctrl->notifier_array[entry_inx].p_callback = p_callback;
    p_instance_ctrl->notifier_array[entry_inx].p_callback_memory = p_callback_memory;

    RM_PMGR_W_sort_notifier_array(p_ctrl);

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_timer_notifier_register(pmgr_ctrl_t * const p_ctrl, timer_callback_t p_callback,
                                            void * p_param)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    int notifier_id;

    FSP_ASSERT(p_ctrl != NULL);

    for (notifier_id = 0; notifier_id < MAX_TIMER_NOTIFIER_ARRAY; notifier_id++)
    {
        if (!p_instance_ctrl->timer_notifier_array[notifier_id].p_callback)
        {
            break;
        }
    }

    if (notifier_id == MAX_TIMER_NOTIFIER_ARRAY)
    {
        return FSP_ERR_OVERFLOW;
    }

    p_instance_ctrl->timer_notifier_array[notifier_id].p_callback = p_callback;
    p_instance_ctrl->timer_notifier_array[notifier_id].p_param = p_param;

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_timer_notifier_unregister(pmgr_ctrl_t * const p_ctrl, timer_callback_t p_callback)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    int notifier_id;

    FSP_ASSERT(p_ctrl != NULL);

    for (notifier_id = 0; notifier_id < MAX_TIMER_NOTIFIER_ARRAY; notifier_id++)
    {
        if (p_instance_ctrl->timer_notifier_array[notifier_id].p_callback == p_callback)
        {
            p_instance_ctrl->timer_notifier_array[notifier_id].p_callback = NULL;
        }
    }

    return FSP_SUCCESS;
}

static uint64_t RM_PMGR_W_timer_notify(pmgr_ctrl_t * const p_ctrl, uint64_t idle_time_us)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;
    pmgr_timer_notifier_array_entry_t * notifier_entry = NULL;
    uint64_t min_idle_time_us = idle_time_us;

    FSP_ASSERT(p_ctrl != NULL);

    /* Idle time is forced (return input time) */
    if (p_instance_ctrl->lld_power_mode <= PMGR_LLD_POWER_MODE_DPM)
    {
        return idle_time_us;
    }

    for (int i = 0; i < MAX_TIMER_NOTIFIER_ARRAY; i++)
    {
        notifier_entry = &p_instance_ctrl->timer_notifier_array[i];

        if (notifier_entry->p_callback)
        {
            idle_time_us = notifier_entry->p_callback(notifier_entry->p_param);

            if (idle_time_us < min_idle_time_us)
            {
                min_idle_time_us = idle_time_us;
            }
        }
    }

    return min_idle_time_us;
}

/* This function can be called only within disabled IRQ */
static eSleepModeStatus RM_PMGR_W_confirm_enter_idle(void)
{
    eSleepModeStatus status = eStandardSleep;

    /* Checks if there is a Task that moved to Ready state */
    status = eTaskConfirmSleepModeStatus();

    /* An interrupt is pending */
    if ((NVIC->ISER[0] & NVIC->ISPR[0]) ||
        (NVIC->ISER[1] & NVIC->ISPR[1]) ||
        (NVIC->ISER[2] & NVIC->ISPR[2]))
    {
        status = eAbortSleep;
    }

    return status;
}

/******************************************************************************************************************//**
 * Informs PM of this sleep duration and trigger idle mode, PMGR mechanism will decide which sleep mode to trigger.
 * Can be called by the OS (reach to idle task by RTOS_idle_task when FreeRTOS has no other task to run) or by
 * customized application to inform there is a power save opportunity.
 *
 * @retval     FSP_SUCCESS          Trigger idle mode successfully or was ignored
  ********************************************************************************************************************/
fsp_err_t RM_PMGR_W_enter_idle(pmgr_ctrl_t * const p_ctrl, uint64_t idle_time_us)
{
    pmgr_lld_power_mode_t selected_power_mode;
    uint32_t sleep_exit_latency_us = 0;

    FSP_ASSERT(p_ctrl != NULL);

    /* Read SysTick CTRL before disabling interrupts - this clears COUNTFLAG.
     * Another read will later detect whether SysTick event occured between
     * disabling of intrrupts and stopping SysTick timer (Critical for sleep4).
     * TODO: Disabling SysTick actually needs to be done before disabling IRQ. */
    (void)SysTick->CTRL;

    __disable_irq();

    idle_time_us = RM_PMGR_W_timer_notify(p_ctrl, idle_time_us);
    selected_power_mode = RM_PMGR_W_trigger_mechanism(p_ctrl, idle_time_us, &sleep_exit_latency_us);

    /* Sleep4 and above can only be executed from idle task context */
    if ((xTaskGetCurrentTaskHandle() != xTaskGetIdleTaskHandle()) &&
        (selected_power_mode >= PMGR_LLD_POWER_MODE_SLEEP4))
    {
        __enable_irq();
        return FSP_ERR_UNSUPPORTED;
    }

    /* Check if a context switch or an interrupt is pending. If so, abort sleep.
     * Otherwise, PMGR is committed to sleep. Further interrupts will be ignored
     * unless they are a wakeup event of the selected sleep mode. */
    if (RM_PMGR_W_confirm_enter_idle() == eAbortSleep)
    {
        __enable_irq();
        return FSP_SUCCESS;
    }

#if CFG_WIFI
    if (selected_power_mode == PMGR_LLD_POWER_MODE_DPM)
    {
        RM_PMGR_W_dpm_set_sleep();
    }
    else
#endif /*CFG_WIFI*/
    {
        /* Sleep notification */
        RM_PMGR_W_update_all_cb_args(p_ctrl, selected_power_mode, PMGR_EVENT_ENTERING_SLEEP);
        RM_PMGR_W_call_registered_cb_func(p_ctrl, true);

        R_PM_LowPowerModeEnter(selected_power_mode, US_TO_RTC_TICKS(idle_time_us - sleep_exit_latency_us));

        /* Wakeup notification */
        RM_PMGR_W_update_all_cb_args(p_ctrl, selected_power_mode, PMGR_EVENT_EXITING_SLEEP);
        RM_PMGR_W_call_registered_cb_func(p_ctrl, false);
    }

    __enable_irq();

    return FSP_SUCCESS;
}

/******************************************************************************************************************//**
 * Add single sleep constraint, prevent going to specific sleep mode
 *
 * @retval     FSP_SUCCESS          Add sleep constraint done successfully
 * @retval     FSP_ERR_UNSUPPORTED  Add sleep constraint is not supported
 * @retval     FSP_ERR_IN_USE       Add sleep constraint failed due to the semaphore is not available
  ********************************************************************************************************************/
fsp_err_t RM_PMGR_W_add_sleep_constraint(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint)
{
    int bit_idx;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);

    /* Keep only valid bits */
    constraint &= PMGR_CONSTRAINT_MASK;

    while (constraint)
    {
        bit_idx = ffs(constraint) - 1;
        constraint &= ~BIT(bit_idx);

        /* Updating counter is done with disabled interrupts.
         * This call is safe from both Task and ISR contexts.
         * These macros support nesting, so calling function
         * may also enable/disable interrupts. */
        GLOBAL_INT_DISABLE();
        p_instance_ctrl->constraint_counter[bit_idx]++;
        GLOBAL_INT_RESTORE();
    }

    return FSP_SUCCESS;
}

/******************************************************************************************************************//**
 * Remove single sleep constraint, prevent going to specific sleep mode
 *
 * @retval     FSP_SUCCESS          Remove sleep constraint done successfully
 * @retval     FSP_ERR_UNSUPPORTED  Remove sleep constraint is not supported
 * @retval     FSP_ERR_IN_USE       Remove sleep constraint failed due to the semaphore is not available
  ********************************************************************************************************************/
fsp_err_t RM_PMGR_W_remove_sleep_constraint(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint)
{
    int bit_idx;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);

    /* Keep only valid bits */
    constraint &= PMGR_CONSTRAINT_MASK;

    while (constraint)
    {
        bit_idx = ffs(constraint) - 1;
        constraint &= ~BIT(bit_idx);

        /* Updating counter is done with disabled interrupts.
         * This call is safe from both Task and ISR contexts.
         * These macros support nesting, so calling function
         * may also enable/disable interrupts. */
        GLOBAL_INT_DISABLE();
        if (p_instance_ctrl->constraint_counter[bit_idx])
        {
            p_instance_ctrl->constraint_counter[bit_idx]--;
        }
        GLOBAL_INT_RESTORE();
    }

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_get_sleep_constraint_counter(pmgr_ctrl_t * const p_ctrl, pmgr_constraints_t constraint, uint8_t * counter)
{
    int bit_idx;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    FSP_ASSERT(counter != NULL);

    /* Keep only valid bits */
    constraint &= PMGR_CONSTRAINT_MASK;

    /* Default return value */
    *counter = 0;

    if (!constraint)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* We can count only one constraint at a time */
    if (constraint & (constraint - 1))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    /* Get the constraint idx */
    bit_idx = ffs(constraint) - 1;

    GLOBAL_INT_DISABLE();
    *counter = p_instance_ctrl->constraint_counter[bit_idx];
    GLOBAL_INT_RESTORE();

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_notifier_unregister(pmgr_ctrl_t * const p_ctrl, uint32_t notifier_id)
{
    int notifier_array_inx;
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);

    /* Unregister notifier_id from the correct order */
    for (notifier_array_inx = 0; notifier_array_inx < PMGR_MAX_NOTIFIER_ARRAY; notifier_array_inx++)
    {
        if (p_instance_ctrl->notifier_array[notifier_array_inx].notify_extend.notifier_id == notifier_id)
        {
            memset((void *)&p_instance_ctrl->notifier_array[notifier_array_inx], 0, sizeof(notifier_array_entry_t));
            p_instance_ctrl->notifier_array[notifier_array_inx].notify_extend.order = UNDEFINE_NOTIFIER_ORDER;
            p_instance_ctrl->notifier_array[notifier_array_inx].notify_extend.notifier_id = UNDEFINE_NOTIFIER_ID;
            p_instance_ctrl->notifier_array[notifier_array_inx].p_callback = NULL;
            p_instance_ctrl->notifier_array[notifier_array_inx].p_callback_memory = NULL;
            RM_PMGR_W_sort_notifier_array(p_ctrl);

            return FSP_SUCCESS;
        }
    }

    return FSP_ERR_INVALID_ARGUMENT;
}

fsp_err_t RM_PMGR_W_set_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t wake_source)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    GLOBAL_INT_DISABLE();
    p_instance_ctrl->wake_source_bitmap |= wake_source;
    GLOBAL_INT_RESTORE();
        
    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_clr_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t wake_source)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);
    GLOBAL_INT_DISABLE();
    p_instance_ctrl->wake_source_bitmap &= ~(wake_source);
    GLOBAL_INT_RESTORE();

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_get_wake_source(pmgr_ctrl_t * const p_ctrl, pmgr_wake_source_t* wake_source)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);

    GLOBAL_INT_DISABLE();
    if (0 != (RTC->GPIO_WAKEUP1_REG_b.GPIO_WAKEUP_EN_SEL & 0x7FF))
    {
        p_instance_ctrl->wake_source_bitmap |= PMGR_WAKE_SOURCE_GPIO;
    }
    if (0 != (RTC->XADC12_TIMER_SET_REG_b.SENSOR_DETECT_ACT & 1))
    {
        p_instance_ctrl->wake_source_bitmap |= PMGR_WAKE_SOURCE_ADC;
    }
    *wake_source = p_instance_ctrl->wake_source_bitmap;
    GLOBAL_INT_RESTORE();

    return FSP_SUCCESS;
}

fsp_err_t RM_PMGR_W_force(pmgr_ctrl_t * const p_ctrl, pmgr_lld_power_mode_t mode, uint64_t idle_time_us)
{
    pmgr_instance_ctrl_t * p_instance_ctrl = (pmgr_instance_ctrl_t *)p_ctrl;

    FSP_ASSERT(p_ctrl != NULL);

    p_instance_ctrl->lld_power_mode = mode;

    if ((idle_time_us != 0) && (mode <= PMGR_LLD_POWER_MODE_DPM))
    {
        RM_PMGR_W_enter_idle(RM_PMGR_W_get_ctrl(), idle_time_us);
    }

    return FSP_SUCCESS;
}

pmgr_ctrl_t * RM_PMGR_W_get_ctrl(void)
{
    return g_p_ctrl;
}

/*******************************************************************************************************************//**
 * @brief Checks if the system has woken up from Sleep3 mode.
 *
 * This function determines whether the current boot/wakeup state originated from Sleep3 mode.
 * Sleep3 is a deep sleep mode where most peripherals are powered down to minimize power consumption.
 *
 * @retval pdTRUE    System has woken up from Sleep3 mode
 * @retval pdFALSE   System has not woken up from Sleep3 mode (normal boot or wakeup from another sleep mode)
 *
 * @note This function should be called early in the boot process to correctly identify the wakeup source.
 **********************************************************************************************************************/
bool RM_PMGR_W_IsSleep3Wakeup(void)
{
    uint16_t Wakeupmode = R_PMGR_GetWakeupMode();

    if (Wakeupmode == 3) // Sleep3 or DPM (which is type of sleep3)
    {
        return pdTRUE;
    }
    return pdFALSE;
}

#if CFG_WIFI

/* Sync with pmgr_dbg_ram_constraint_counter_cause_t in number of items and the order */
static const char* pre_def_job_names[] = {
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_GENERIC_FORCE,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_PMGR_CLI,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_DPM_AT,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_TCP_CLIENT,
    "ifconfig",       // PMGR_DBG_RAM_CONSTRAINT_NET_IFCONFIG,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_APP_POLL_STATE,
    "sntp_client",    // PMGR_DBG_RAM_CONSTRAINT_SNTP,
    "websocket_c",    // PMGR_DBG_RAM_CONSTRAINT_APP_WEBSOCKET_CLIENT,
    "DPM_SUPP",       // PMGR_DBG_RAM_CONSTRAINT_DPM_SUPPLICANT,
    "dpm_timer_test", // PMGR_DBG_RAM_CONSTRAINT_DPM_REG_NAME_TIMER,
    "UDP_IPV6_DPM",   // PMGR_DBG_RAM_CONSTRAINT_UDP_IPV6_DPM,
    "TCPC_IPV6_DPM",  // PMGR_DBG_RAM_CONSTRAINT_TCPC_OVER_IPV6_DPM,
    "DPM_TIMER",      // PMGR_DBG_RAM_CONSTRAINT_DPM_TIMER_PROC,
    "HttpClient",     // PMGR_DBG_RAM_CONSTRAINT_HTTPC,
    "DPM_IPV6",       // PMGR_DBG_RAM_CONSTRAINT_DPM_LWIP_IPV6,
    "DPM_KEY",        // PMGR_DBG_RAM_CONSTRAINT_DPM_KEY,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_WA_CAUSE,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_RWNX_DRV_TASK,
    NULL,             // PMGR_DBG_RAM_CONSTRAINT_RWNX_MAC_TASK
    "mqtt_sub"        // PMGR_DBG_RAM_CONSTRAINT_MQTT_SUB
};

fsp_err_t RM_PMGR_W_dpm_job_name_set(char * dpm_name, int dpm_port)
{
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return FSP_SUCCESS;
    }
    
    pmgr_w_info_dpm_sleep_t * pmgr_w_dpm_list = NULL;
    int i = 0, ret = FSP_SUCCESS, dup_name = false;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
                dup_name = true;
                break;
        }
        
        i++;
    }

    if (dup_name == true) {
        if (pmgr_w_dpm_list->port_number == (unsigned int) dpm_port) {
            ret = FSP_ERR_ABORTED;
            goto JOB_NAME_SET_FIN;
        } else {
            pmgr_w_dpm_list->port_number = dpm_port;
            ret = FSP_SUCCESS;
            goto JOB_NAME_SET_FIN;            
        }
    }
    
    /* Find an empty slot */
    i = 0;
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strlen(pmgr_w_dpm_list->dpm_name) == 0) {
            break;
        }

        i++;
    }
    if (i >= MAX_DPM_INFO_ARRAY) {
        RM_PMGR_W_dpm_info_lock_give();
        return FSP_ERR_ABORTED;
    }

    /* Set job name information */
    dpm_strcpy(pmgr_w_dpm_list->dpm_name, dpm_name);
    pmgr_w_dpm_list->port_number = dpm_port;

    /* Set idx into pre-defined job_names list */
    for (i = 0; i < PMGR_DBG_RAM_CONSTRAINT_MAX; i++) {
        if (pre_def_job_names[i] && dpm_strcmp(pmgr_w_dpm_list->dpm_name, pre_def_job_names[i]) == 0) {
            pmgr_w_dpm_list->idx_ram_constr_dbg_tbl = i;
            break;
        }
    }
    
    if (i == PMGR_DBG_RAM_CONSTRAINT_MAX) {
        pmgr_w_dpm_list->idx_ram_constr_dbg_tbl = -1; // job_name not belong to system-pre-defined names
    }

    ret = FSP_SUCCESS;
    
JOB_NAME_SET_FIN:

    RM_PMGR_W_dpm_info_lock_give();

    return ret;
}

fsp_err_t RM_PMGR_W_dpm_job_name_clear(char * dpm_name)
{
    if (RM_PMGR_W_dpm_is_enabled() == pdFALSE) {
        return FSP_SUCCESS;
    }

    pmgr_w_info_dpm_sleep_t * pmgr_w_dpm_list = NULL;
    pmgr_instance_ctrl_t * p_instance_ctrl = RM_PMGR_W_get_ctrl();
    int i = 0;

    RM_PMGR_W_dpm_info_lock_take(portMAX_DELAY);
    while (i < MAX_DPM_INFO_ARRAY) {
        pmgr_w_dpm_list = (pmgr_w_info_dpm_sleep_t *)&pmgr_w_info_dpm_sleep_list[i];

        if (dpm_strcmp(pmgr_w_dpm_list->dpm_name, dpm_name) == 0) {
            break;
        }
        
        i++;
    }
    if (i >= MAX_DPM_INFO_ARRAY)
    {
        RM_PMGR_W_dpm_info_lock_give();
        return FSP_SUCCESS;
    }

    /* clear regist information */
    if(pmgr_w_dpm_list->is_managed == pdTRUE && pmgr_w_dpm_list->sleep_count == 1)
    {
        RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        if (p_instance_ctrl && p_instance_ctrl->debug_runtime_flag == PMGR_DEBUG_RAM_ENABLED) {
            if (pmgr_w_dpm_list->idx_ram_constr_dbg_tbl != -1) {
                p_instance_ctrl->ram_counter_cause[pmgr_w_dpm_list->idx_ram_constr_dbg_tbl]--;
            }
        }
    }
    memset((void *)pmgr_w_dpm_list->dpm_name, 0, MAX_DPM_NAME_LEN);
    pmgr_w_dpm_list->init_done=0;
    pmgr_w_dpm_list->port_number=0;
    pmgr_w_dpm_list->rcv_ready=0;
    pmgr_w_dpm_list->sleep_count=0;
    pmgr_w_dpm_list->is_managed=pdFALSE;
    pmgr_w_dpm_list->idx_ram_constr_dbg_tbl = -1;

    RM_PMGR_W_dpm_info_lock_give();

    return FSP_SUCCESS;
}

#endif /* CFG_WIFI */
#endif /* CFG_PMGR */
/*******************************************************************************************************************//**
 * @} (end addtogroup PMGR_W)
 **********************************************************************************************************************/
