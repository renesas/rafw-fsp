/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_BLEBRG_H
#define RM_ATCMD_W_CORE_BLEBRG_H

#include "rm_atcmd_w_cfg.h"
#if (ATCMD_BLE_BRG == 1)

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

void r_at_rm_blebrg_start(atcmd_w_core_running_mode_t running_mode);
void r_at_rm_blebrg_end(void);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* ATCMD_BLE_BRG == 1 */

#endif /* RM_ATCMD_W_CORE_BLEBRG_H */

