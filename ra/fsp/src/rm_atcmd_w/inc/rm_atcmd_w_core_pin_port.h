/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_PIN_PORT_H
#define RM_ATCMD_W_CORE_PIN_PORT_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define PRODMEM_SIZE 0x400
#define PRODMEM_ADDR 0x300000


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** LLD wakeup source */
#if !CFG_WIFI
typedef enum e_pmgr_wake_source
{
    PMGR_WAKE_SOURCE_NON             = (1UL << (0)),     ///< no wake source defined
    PMGR_WAKE_SOURCE_RTC             = (1UL << (1)),     ///< RTC wake source
    PMGR_WAKE_SOURCE_GPT             = (1UL << (2)),     ///< GPT wake source
    PMGR_WAKE_SOURCE_GPIO            = (1UL << (3)),     ///< GPIO wake source
    PMGR_WAKE_SOURCE_ADC             = (1UL << (4)),     ///< ADC wake source

    /** connectivity wake source. */
    PMGR_WAKE_SOURCE_WIFI            = (1UL << (5)),     ///< WIFI/MAC wake source
    PMGR_WAKE_SOURCE_BLE             = (1UL << (6)),     ///< BLE wake source
} pmgr_wake_source_t;
#endif /* !CFG_WIFI */
/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_PIN_PORT_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_PIN_PORT_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_PIN_PORT_open(atcmd_w_ctrl_t * const p_at_ctrl);

uint32_t RM_ATCMD_W_CORE_PIN_PORT_close(atcmd_w_ctrl_t * const p_at_ctrl);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_PIN_PORT_H */
