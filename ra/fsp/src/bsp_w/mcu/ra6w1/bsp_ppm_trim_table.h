/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 ****************************************************************************************
 *
 * @file bsp_ppm_trim_table.h
 *
 * @brief PPM compensation lookup table for the RA6W1.
 *
 * The table maps a calibrated 7-bit temperature code (read from the
 * WIFI_RFHPI_TEMP_SENSOR_CAL register's TEMP_VALUE_AFTER_PROCESS_CAL field at
 * address 0x60D0006C, bits [6:0]) to a signed delta that is added to the
 * current XTAL40M_CCTRL field (at 0x400C0204, bits [22:16]) immediately
 * before an ATE TX command is started. The result is clamped to the valid
 * unsigned 7-bit range [0, 127].
 *
 * The table is consulted using nearest-neighbour lookup, with out-of-range
 * temperature codes clamped to the nearest endpoint.
 *
 * --- Customer re-characterisation ---
 * The trim table is defined in bsp_ppm_trim_table.c with BSP_WEAK_REFERENCE
 * defaults. To use a custom table for a non-reference XTAL:
 *
 *   1. Copy ra/fsp/src/bsp_w/mcu/ra6w1/bsp_ppm_trim_table.c into your
 *      e2studio project's src/ folder (project-owned; not regenerated
 *      by FSP "Generate Project Content").
 *   2. Remove the BSP_WEAK_REFERENCE qualifiers on bsp_ppm_trim_table
 *      and bsp_ppm_trim_table_size in your copy.
 *   3. Edit the entries; keep them sorted ascending by temp_code and
 *      keep bsp_ppm_trim_table_size consistent with the array length.
 *   4. Rebuild. The linker prefers your strong definitions over the
 *      pack-shipped weak defaults.
 *
 * Do not edit the pack copy in place; it will be overwritten on the
 * next FSP regeneration. A minimal template stanza is provided at the
 * bottom of this header (wrapped in #if 0) for quick reference.
 *
 * --- Offline derivation of cctrl_delta ---
 * The CCTRL slope is -0.7 ppm per LSB. For each temperature, measure the
 * residual frequency error (ppm_meas) at the current trim and pick a
 * ppm_target. Then:
 *
 *     cctrl_delta = round( (ppm_target - ppm_meas) / -0.7 )
 *
 * Example: ppm_meas = +4.2, ppm_target = 0  =>  cctrl_delta = +6.
 * A positive ppm error (clock running fast) yields a positive delta;
 * a negative ppm error (clock running slow) yields a negative delta.
 *
 * --- Row format ---
 *   .temp_code   : raw value of TEMP_VALUE_AFTER_PROCESS_CAL [6:0]
 *                  (related to temperature by: T_C = temp_code * -1.667 + 153)
 *   .cctrl_delta : signed adjustment, in CCTRL LSBs, to add to the current
 *                  XTAL40M_CCTRL value before TX starts.
 *
 * Rows MUST be sorted by temp_code in ascending order.
 *
 ****************************************************************************************
 */
#ifndef BSP_PPM_TRIM_TABLE_H
#define BSP_PPM_TRIM_TABLE_H

#include <stdint.h>

/* ------------------------------------------------------------------------ */

typedef struct
{
    uint8_t temp_code;                 /* TEMP_VALUE_AFTER_PROCESS_CAL [6:0] */
    int8_t  cctrl_delta;               /* signed adjustment to XTAL40M_CCTRL */
} bsp_ppm_trim_entry_t;

/* Trim-table symbols. Defined in bsp_ppm_trim_table.c with weak linkage
 * so a project-supplied (strong) definition takes precedence. The table
 * MUST be sorted ascending by temp_code; bsp_ppm_trim_table_size MUST
 * equal the array element count. */
extern const bsp_ppm_trim_entry_t bsp_ppm_trim_table[];
extern const uint32_t             bsp_ppm_trim_table_size;

#if 0

/* --- Minimal customer template -----------------------------------------
 * Drop this content into a file named bsp_ppm_trim_table.c in your
 * project's src/ folder, remove the surrounding #if 0/#endif, and edit
 * the entries. Strong definitions override the BSP weak defaults.
 */
 #include "bsp_ppm_trim_table.h"

const bsp_ppm_trim_entry_t bsp_ppm_trim_table[] =
{
    /* temp_code, cctrl_delta */
    { 21,  +39 },
    /* ... add entries sorted ascending by temp_code ... */
    { 105, +6  },
};

const uint32_t bsp_ppm_trim_table_size = FSP_ARRAY_LENGTH(bsp_ppm_trim_table);

#endif                                 /* customer template */

#endif                                 /* BSP_PPM_TRIM_TABLE_H */
