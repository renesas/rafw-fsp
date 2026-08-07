
/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_PMGR_W_DPM_SOCK_INTERNAL_H
#define RM_PMGR_W_DPM_SOCK_INTERNAL_H

#if CFG_WIFI
/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "FreeRTOS.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

#define DPM_SOCK_MAX_TCP_SESS	6

#define DPM_SOCK_MAX_TCP_WAIT_CNT       1400

#define DPM_SOCK_MAX_UDPH (DPM_SOCK_MAX_TCP_SESS)

/***********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
typedef struct _dpm_tcp_sess_info {
    unsigned int used;
    unsigned int wait_cnt;
    struct tcp_pcb pcb;
    uint8_t sess_idx;
} dpm_tcp_sess_info;

typedef struct st_ptim_udph_conf {
    uint8_t     in_use;
    uint8_t     ip_type;
    int32_t     period;
    uint16_t    src_port;
    uint16_t    dst_port;
    uint32_t    peer_ip;        //IPV4
    uint32_t    peer_ip6[4];    //IPV6
} ptim_udph_conf_t;

typedef struct st_ptim_udph_conf_data
{
    ptim_udph_conf_t ptim_udph_config[DPM_SOCK_MAX_UDPH];
    int active_udph_count;
} ptim_udph_conf_data_t;

typedef struct st_ptim_tcpka_timeout_tbl
{
    int local_port[DPM_SOCK_MAX_TCP_SESS];
    int count;
} ptim_tcpka_timeout_tbl_t;

void RM_PMGR_W_socket_dpm_init(void);

void RM_PMGR_W_socket_dpm_all_tcp_sess_info_clear(void);

void *RM_PMGR_W_socket_dpm_lwip_sock_names_get(void);

struct tcp_pcb *RM_PMGR_W_socket_dpm_tcp_pcb_create(char *name);

struct tcp_pcb *RM_PMGR_W_socket_dpm_tcp_pcb_get(char *name);

int RM_PMGR_W_socket_dpm_tcp_pcb_delete(char *name);

int RM_PMGR_W_socket_dpm_ongoing_transaction_is_finished(void);

int RM_PMGR_W_socket_dpm_tcp_sess_is_any_connected_one_existing(void);

int RM_PMGR_W_socket_dpm_tcp_sess_is_connected(char *name);

void RM_PMGR_W_socket_dpm_tcp_sess_ptim_config_for_connected_one(void);

void RM_PMGR_W_socket_dpm_tcp_sess_rtm_content_print(void);

void RM_PMGR_W_socket_dpm_tcp_sess_pcb_print(int index);

int RM_PMGR_W_socket_dpm_ptim_udph_config(void);

void RM_PMGR_W_socket_dpm_tcpka_update_pcb(void);

dpm_tcp_sess_info *RM_PMGR_W_socket_dpm_get_sess_info_by_name(char *name);

void RM_PMGR_W_socket_dpm_set_tcp_ka_time_by_name(char *name);

int ptim_udph_conf_free_slot_get(void);

int ptim_udph_conf_add(uint32_t dst_ip, uint32_t *dst_ip6, uint16_t src_port, uint16_t dst_port, int period);

void ptim_udph_conf_status_print(void);

void ptim_udph_sock_status_print(void);

uint64_t dpm_get_rtclk(void);

#ifdef __cplusplus
}
#endif

#endif /*CFG_WIFI*/

#endif /*RM_PMGR_W_DPM_SOCK_INTERNAL_H*/