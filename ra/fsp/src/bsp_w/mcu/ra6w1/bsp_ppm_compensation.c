/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 ****************************************************************************************
 *
 * @file bsp_ppm_compensation.c
 *
 * @brief RA6W1 XTAL PPM compensation implementation.
 *
 * Device-specific implementation of the PPM compensation API declared in
 * bsp_ppm_compensation.h. Reads the calibrated 7-bit temperature code
 * (TEMP_VALUE_AFTER_PROCESS_CAL, [6:0] of register 0x60D0006C), looks up
 * a signed XTAL40M_CCTRL delta in bsp_ppm_trim_table[], and applies it to
 * the XTAL40M_CCTRL field clamped to the valid 7-bit range [0, 127].
 *
 * Compilation and execution of the compensation logic is gated by
 * BSP_CFG_PPM_COMP_ENABLE (emitted from the e2studio BSP property
 * "General|Enable XTAL PPM compensation"). When disabled (default), all
 * three entry points become runtime no-ops with no register writes.
 *
 * The first call after reset captures the current XTAL40M_CCTRL value as
 * the baseline; subsequent calls compute (baseline + delta) so the trim
 * table represents absolute deltas relative to a single anchor point
 * rather than drifting on each call.
 *
 ****************************************************************************************
 */

#include "bsp_ppm_compensation.h"

static bool    s_baseline_captured = false;
static uint8_t s_cctrl_baseline    = 0U;

bool bsp_ppm_compensation_apply_for_tx (bsp_ppm_compensation_result_t * const p_result)
{
    if (NULL != p_result)
    {
        p_result->applied   = false;
        p_result->temp_code = 0U;
        p_result->delta     = 0;
        p_result->cctrl_old = 0U;
        p_result->cctrl_new = 0U;
        p_result->temp_mdeg = 0;
    }

    if (BSP_CFG_PPM_COMP_ENABLE == 0)
    {
        return false;
    }

    uint8_t temp_code = (uint8_t) ((*(volatile uint32_t *) BSP_PPM_TEMP_SENSOR_CAL_ADDR) & 0x7FU);

    const uint32_t table_size = bsp_ppm_trim_table_size;

    int8_t delta;
    if ((table_size == 0U) || (temp_code <= bsp_ppm_trim_table[0].temp_code))
    {
        delta = (table_size == 0U) ? 0 : bsp_ppm_trim_table[0].cctrl_delta;
    }
    else if (temp_code >= bsp_ppm_trim_table[table_size - 1U].temp_code)
    {
        delta = bsp_ppm_trim_table[table_size - 1U].cctrl_delta;
    }
    else
    {
        delta = bsp_ppm_trim_table[0].cctrl_delta;
        for (uint32_t i = 0U; i < (table_size - 1U); i++)
        {
            uint8_t lo = bsp_ppm_trim_table[i].temp_code;
            uint8_t hi = bsp_ppm_trim_table[i + 1U].temp_code;
            if ((temp_code >= lo) && (temp_code <= hi))
            {
                int32_t d_lo = (int32_t) bsp_ppm_trim_table[i].cctrl_delta;
                int32_t d_hi = (int32_t) bsp_ppm_trim_table[i + 1U].cctrl_delta;
                int32_t span = (int32_t) (hi - lo);

                if (span == 0)
                {
                    delta = (int8_t) d_lo;
                }
                else
                {
                    int32_t num = (d_lo * (int32_t) (hi - temp_code)) +
                                  (d_hi * (int32_t) (temp_code - lo));
                    int32_t rounded = (num >= 0) ? ((num + (span / 2)) / span) :
                                      -(((-num) + (span / 2)) / span);
                    delta = (int8_t) rounded;
                }
                break;
            }
        }
    }

    int32_t cctrl_old = (int32_t) CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_CCTRL;
    if (!s_baseline_captured)
    {
        s_cctrl_baseline    = (uint8_t) cctrl_old;
        s_baseline_captured = true;
    }

    int32_t cctrl_new = (int32_t) s_cctrl_baseline + (int32_t) delta;
    if (cctrl_new < 0)
    {
        cctrl_new = 0;
    }
    else if (cctrl_new > 127)
    {
        cctrl_new = 127;
    }

    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_CCTRL = ((uint32_t) cctrl_new) & 0x7FU;

    if (NULL != p_result)
    {
        p_result->applied   = true;
        p_result->temp_code = temp_code;
        p_result->delta     = delta;
        p_result->cctrl_old = (uint8_t) cctrl_old;
        p_result->cctrl_new = (uint8_t) cctrl_new;
        p_result->temp_mdeg = ((int32_t) temp_code * -1667) + 153000;
    }

    return true;
}

void bsp_ppm_compensation_recomp_start (void)
{
    /* Reserved for platform-specific lifecycle handling. */
}

void bsp_ppm_compensation_recomp_stop (void)
{
    /* Reserved for platform-specific lifecycle handling. */
}