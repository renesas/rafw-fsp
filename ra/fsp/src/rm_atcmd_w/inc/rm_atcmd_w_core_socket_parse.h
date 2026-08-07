/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_ATCMD_W_CORE_SOCKET_PARSE_H
#define RM_ATCMD_W_CORE_SOCKET_PARSE_H

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_common.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/// NVRAM name of TCP server local port
#define ATC_NVRAM_TCPS_PORT                 "ATC_TS_PORT"

/// NVRAM name of TCP client remote IP address
#define ATC_NVRAM_TCPC_IP                   "ATC_TC_IP"

/// NVRAM name of TCP client remote port
#define ATC_NVRAM_TCPC_RPORT                "ATC_TC_RPORT"

/// NVRAM name of TCP client local port
#define ATC_NVRAM_TCPC_LPORT                "ATC_TC_LPORT"

/// NVRAM name of UDP session local port
#define ATC_NVRAM_UDPS_PORT                 "ATC_US_PORT"

#define ATCMD_NW_TR_MAX_NVR_CNT             16
#define ATCMD_NW_TR_MAX_SESSION_CNT         ATCMD_NW_TR_MAX_NVR_CNT    // 16. Must be less than ATCMD_NW_TR_MAX_NVR_CNT
#if CFG_PMGR
#define ATCMD_NW_TR_MAX_SESSION_CNT_DPM     DPM_SOCK_MAX_TCP_SESS      // 16. Must be less than ATCMD_NW_TR_MAX_NVR_CNT.
#endif /* CFG_PMGR */

#define ATCMD_RTM_NW_TR_NAME                "ATC_NW_TR_NAME"

// Name of NVRAM to support multi session
#define ATCMD_NVR_NW_TR_CID_0               "0:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_1               "1:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_2               "2:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_3               "3:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_4               "4:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_5               "5:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_6               "6:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_7               "7:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_8               "8:ATC_NW_TR_CID"
#define ATCMD_NVR_NW_TR_CID_9               "9:ATC_NW_TR_CID"

#define ATCMD_NVR_NW_TR_LOCAL_PORT_0        "0:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_1        "1:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_2        "2:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_3        "3:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_4        "4:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_5        "5:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_6        "6:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_7        "7:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_8        "8:ATC_NW_TR_LPORT"
#define ATCMD_NVR_NW_TR_LOCAL_PORT_9        "9:ATC_NW_TR_LPORT"

#define ATCMD_NVR_NW_TR_PEER_PORT_0         "0:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_1         "1:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_2         "2:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_3         "3:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_4         "4:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_5         "5:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_6         "6:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_7         "7:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_8         "8:ATC_NW_TR_PPORT"
#define ATCMD_NVR_NW_TR_PEER_PORT_9         "9:ATC_NW_TR_PPORT"

#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_0  "0:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_1  "1:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_2  "2:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_3  "3:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_4  "4:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_5  "5:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_6  "6:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_7  "7:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_8  "8:ATC_NW_TR_MAX_PEER"
#define ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_9  "9:ATC_NW_TR_MAX_PEER"

#define ATCMD_NVR_NW_TR_PEER_IPADDR_0       "0:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_1       "1:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_2       "2:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_3       "3:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_4       "4:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_5       "5:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_6       "6:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_7       "7:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_8       "8:ATC_NW_TR_PIPADDR"
#define ATCMD_NVR_NW_TR_PEER_IPADDR_9       "9:ATC_NW_TR_PIPADDR"

#define ATCMD_NVR_NW_TR_IP_TYPE_0           "0:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_1           "1:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_2           "2:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_3           "3:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_4           "4:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_5           "5:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_6           "6:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_7           "7:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_8           "8:ATC_NW_TR_IP_TYPE"
#define ATCMD_NVR_NW_TR_IP_TYPE_9           "9:ATC_NW_TR_IP_TYPE"

#define ATCMD_NVR_NW_TR_PEER_IPADDR_LEN     46

/// TCP Server CID
#define ID_TS                               0
/// TCP Client CID
#define ID_TC                               1
/// UDP Session CID
#define ID_US                               2

#define ATCMD_TLS_MAX_ALLOW_CNT             2

// For NVRAM
#define ATCMD_TLS_NVR_CID_0                  "0:ATC_NW_TLS_CID"
#define ATCMD_TLS_NVR_CID_1                  "1:ATC_NW_TLS_CID"
#define ATCMD_TLS_NVR_ROLE_0                 "0:ATC_NW_TLS_ROLE"
#define ATCMD_TLS_NVR_ROLE_1                 "1:ATC_NW_TLS_ROLE"
#define ATCMD_TLS_NVR_PROFILE_0              "0:ATC_NW_TLS_PROFILE"
#define ATCMD_TLS_NVR_PROFILE_1              "1:ATC_NW_TLS_PROFILE"

#define ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE     2

#define ATCMD_TLSC_NVR_CA_CERT_NAME_0        "0:ATC_NW_TLSC_CA_NAME"
#define ATCMD_TLSC_NVR_CA_CERT_NAME_1        "1:ATC_NW_TLSC_CA_NAME"
#define ATCMD_TLSC_NVR_CA_CERT_NAME_LEN      32                      // ATCMD_CM_MAX_NAME

#define ATCMD_TLSC_NVR_CERT_NAME_0           "0:ATC_NW_TLSC_CERT_NAME"
#define ATCMD_TLSC_NVR_CERT_NAME_1           "1:ATC_NW_TLSC_CERT_NAME"
#define ATCMD_TLSC_NVR_CERT_NAME_LEN         32                      // ATCMD_CM_MAX_NAME

#define ATCMD_TLSC_NVR_HOST_NAME_0           "0:ATC_NW_TLSC_HOST_NAME"
#define ATCMD_TLSC_NVR_HOST_NAME_1           "1:ATC_NW_TLSC_HOST_NAME"
#define ATCMD_TLSC_NVR_HOST_NAME_LEN         64                      // ATCMD_TLSC_MAX_HOSTNAME

#define ATCMD_TLSC_NVR_AUTH_MODE_0           "0:ATC_NW_TLSC_AUTH_MODE"
#define ATCMD_TLSC_NVR_AUTH_MODE_1           "1:ATC_NW_TLSC_AUTH_MODE"

#define ATCMD_TLSC_NVR_INCOMING_LEN_0        "0:ATC_NW_TLSC_INCOMING"
#define ATCMD_TLSC_NVR_INCOMING_LEN_1        "1:ATC_NW_TLSC_INCOMING"

#define ATCMD_TLSC_NVR_OUTGOING_LEN_0        "0:ATC_NW_TLSC_OUTGOING"
#define ATCMD_TLSC_NVR_OUTGOING_LEN_1        "1:ATC_NW_TLSC_OUTGOING"

#define ATCMD_TLSC_NVR_LOCAL_PORT_0          "0:ATC_NW_TLSC_LPORT"
#define ATCMD_TLSC_NVR_LOCAL_PORT_1          "1:ATC_NW_TLSC_LPORT"

#define ATCMD_TLSC_NVR_PEER_PORT_0           "0:ATC_NW_TLSC_PPORT"
#define ATCMD_TLSC_NVR_PEER_PORT_1           "1:ATC_NW_TLSC_PPORT"

#define ATCMD_TLSC_NVR_PEER_IPADDR_0         "0:ATC_NW_TLSC_PIPADDR"
#define ATCMD_TLSC_NVR_PEER_IPADDR_1         "1:ATC_NW_TLSC_PIPADDR"
#define ATCMD_TLSC_NVR_PEER_IPADDR_LEN       64

#define ATCMD_TLS_DPM_CONTEXT_NAME           "atcmd_tls_ctx"

#define ATCMD_TLS_CERTSTORE_CMD              "AT+TRSSLCERTSTORE"
#define ATCMD_TLS_WR_CMD                     "AT+TRSSLWR"

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_SOCKET_register(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_SOCKET_deregister(atcmd_w_core_module_list_t * p_list);

uint32_t RM_ATCMD_W_CORE_SOCKET_open(atcmd_w_ctrl_t * const p_at_ctrl);

uint32_t RM_ATCMD_W_CORE_SOCKET_close(atcmd_w_ctrl_t * const p_at_ctrl);

void     RM_ATCMD_W_CORE_SOCKET_init_start(atcmd_w_ctrl_t * const p_at_ctrl);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif /* RM_ATCMD_W_CORE_SOCKET_PARSE_H */

