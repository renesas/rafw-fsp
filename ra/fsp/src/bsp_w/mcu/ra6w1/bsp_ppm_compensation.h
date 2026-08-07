/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef BSP_PPM_COMPENSATION_H
#define BSP_PPM_COMPENSATION_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp_api.h"
#include "bsp_ppm_trim_table.h"

typedef struct st_bsp_ppm_compensation_result
{
    uint8_t temp_code;
    int8_t  delta;
    uint8_t cctrl_old;
    uint8_t cctrl_new;
    int32_t temp_mdeg;
    bool    applied;
} bsp_ppm_compensation_result_t;

/* Address of WIFI_RFHPI_TEMP_SENSOR_CAL; bits [6:0] hold TEMP_VALUE_AFTER_PROCESS_CAL. */
#define BSP_PPM_TEMP_SENSOR_CAL_ADDR    (0x60D0006Cu)

bool bsp_ppm_compensation_apply_for_tx (bsp_ppm_compensation_result_t * const p_result);

void bsp_ppm_compensation_recomp_start (void);

void bsp_ppm_compensation_recomp_stop (void);

#endif                                 /* BSP_PPM_COMPENSATION_H */
