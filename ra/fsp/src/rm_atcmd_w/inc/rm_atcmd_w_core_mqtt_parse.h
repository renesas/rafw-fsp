/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_MQTT_PARSE_H
#define RM_ATCMD_W_CORE_MQTT_PARSE_H

#include "rm_atcmd_w_core_common.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_register(atcmd_w_core_module_list_t * p_list);
uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_deregister(atcmd_w_core_module_list_t * p_list);
uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_open(atcmd_w_ctrl_t * const p_at_ctrl);
uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_close(atcmd_w_ctrl_t * const p_at_ctrl);

void RM_ATCMD_W_CORE_NETWORK_MQTT_RESP_Handle(atcmd_w_ctrl_t * const p_at_ctrl);
void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(int state);
int  RM_ATCMD_W_CORE_NETWORK_MQTT_get_wfadp_state(void);
void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state(int state);
int  RM_ATCMD_W_CORE_NETWORK_MQTT_get_wfdap_err_state(void);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif                                 /* RM_ATCMD_W_CORE_BASIC_PARSE_H */
