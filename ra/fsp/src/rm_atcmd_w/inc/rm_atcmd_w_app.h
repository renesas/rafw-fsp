/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_APP_H
#define RM_ATCMD_W_APP_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdint.h>
#include <stddef.h>
#include "bsp_api.h"
#include "rm_atcmd_transport_uart_w.h"
#include "r_uart_w.h"
#if (ATCMD_TRANSPORT_SDIO_W == 1)
 #include "rm_atcmd_transport_sdio_w.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define MAX_BUFFER_LEN                256
#if ATCMD_IF_SUPPORT
 #if CFG_PMGR
  #define EVT_MCUWUDONE_RCV_DONE      (1UL << (0))
  #define EVT_SLEEP_BLOCK_RCV_DONE    (1UL << (1))
 #endif                                /* CFG_PMGR */
#endif                                 /* ATCMD_IF_SUPPORT */

extern atcmd_w_core_instance_t g_at_core_instance;

void      atcmd_w_start(void);
fsp_err_t atcmd_w_core_init(atcmd_w_core_instance_t * p_at_core_instance);
void rm_atcmd_w_app_task(void * pvParameters);

void set_user_app_atcmd_open_callback(ATCmdOpenCallback_t callback);
void set_user_app_atcmd_close_callback(ATCmdCloseCallback_t callback);
void rm_atcmd_w_core_user_command_unfixed_register(
    atcmd_w_core_unfixed_module_t * user_atcmd);
void rm_atcmd_w_core_user_command_unfixed_deregister(
    atcmd_w_core_unfixed_module_t * user_atcmd);
void                             rm_atcmd_w_core_user_command_register(atcmd_w_core_module_t * user_atcmd);
void                             rm_atcmd_w_core_user_command_deregister(atcmd_w_core_module_t * user_atcmd);
atcmd_w_core_module_t         ** rm_atcmd_get_user_atcmd_ptr(void);
atcmd_w_core_unfixed_module_t ** rm_atcmd_get_user_unfixed_atcmd_ptr(void);

#if ATCMD_IF_SUPPORT
uint32_t atcmd_print_initdone_resp(void);
uint32_t atcmd_set_startup_atcmd_event_callback(void * const        p_ctrl,
                                                unsigned int (    * p_callback)(
                                                    void * const    p_ctrl,
                                                    unsigned char * p_in,
                                                    unsigned int    inlen));
void     atcmd_print_initdone_softap_mode(void);
uint32_t atcmd_set_initdone_resp(char * p_out, size_t outlen, int is_startup);

void atcmd_set_init_done_msg_to_mcu_on_softap(unsigned int flag);
int  atcmd_get_init_done_msg_to_mcu_on_softap(void);

uint8_t rm_atcmd_w_core_mcuwudone_is_received(atcmd_w_core_instance_t * p_at_core_instance);
uint8_t rm_atcmd_w_core_sleep_block_is_received(atcmd_w_core_instance_t * p_at_core_instance);
uint8_t rm_atcmd_w_core_sleep_block_received_set(atcmd_w_core_instance_t * p_at_core_instance, uint8_t flag);

#endif                                 /* ATCMD_IF_SUPPORT */
#endif                                 /* RM_AR_APP_H */
