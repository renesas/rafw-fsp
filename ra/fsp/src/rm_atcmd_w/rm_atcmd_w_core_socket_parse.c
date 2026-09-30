/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_parse.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"

#include "rm_atcmd_w_core_socket_tcp_server.h"
#include "rm_atcmd_w_core_socket_tcp_client.h"
#include "rm_atcmd_w_core_socket_udp_session.h"
#include "rm_atcmd_w_core_socket_tls_client.h"
#include "rm_atcmd_w_core_socket_cert_mng.h"

#include "FreeRTOS.h"
#include "rm_wifi.h"

#if CFG_PMGR
 #include "rm_pmgr_w_instance.h"
#endif                                 /* CFG_PMGR */
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_wifi_helper.h"
#include "rm_wifi.h"
#include "rm_atcmd_w_core_socket_internal.h"
#include "net_network_main.h"
#include "net_sntp_client.h"

#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
#endif

#include <strings.h>

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(atcmd)    "AT+" # atcmd

#define RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, size_t inlen)

#define RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_SOCKET_ ## atcmd ## _brief_cb

#define ATCMD_DPM_TMP                             "ATCMD_DPM_TEMP"
#define ATCMD_DPM_NETWORK_TMP                     "ATCMD_DPM_NET_TEMP"

#define RM_ATCMD_W_CORE_SOCKET_MAX_TX_SIZE        (1024 * 4)
#define RM_ATCMD_W_CORE_SOCKET_MAX_WAIT_TIME      (30 * 1000) /* 30sec */
#define RM_ATCMD_W_CORE_SOCKET_INIT_STACK_SIZE    (512)

#define RM_ATCMD_W_CORE_SOCKET_DEBUG(fmt, ...)                // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define RM_ATCMD_W_CORE_SOCKET_ERROR(fmt, ...)                // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _atcmd_sess_type
{
    ATCMD_SESS_NONE        = -1,
    ATCMD_SESS_TCP_SERVER  = 0,
    ATCMD_SESS_TCP_CLIENT  = 1,
    ATCMD_SESS_UDP_SESSION = 2,
} atcmd_sess_type;

typedef enum _atcmd_tls_role
{
    ATCMD_TLS_NONE   = -1,
    ATCMD_TLS_SERVER = 0,
    ATCMD_TLS_CLIENT = 1,
} atcmd_tls_role;

typedef enum _atcmd_sess_nvr_type
{
    ATCMD_SESS_NVR_CID              = 0,
    ATCMD_SESS_NVR_LOCAL_PORT       = 1,
    ATCMD_SESS_NVR_PEER_PORT        = 2,
    ATCMD_SESS_NVR_PEER_IP_ADDRESS  = 3,
    ATCMD_SESS_NVR_MAX_ALLOWED_PEER = 4,
    ATCMD_SESS_NVR_IP_TYPE          = 5,
} atcmd_sess_nvr_type;

typedef struct _atcmd_sess_info
{
    atcmd_sess_type type;
    union
    {
        atcmd_tcps_sess_info tcps;
        atcmd_tcpc_sess_info tcpc;
        atcmd_udps_sess_info udps;
    } sess;
} atcmd_sess_info;

typedef struct _atcmd_sess_context
{
    int             cid;
    atcmd_sess_type type;
    union
    {
        void               * ptr;
        atcmd_tcps_context * tcps;
        atcmd_tcpc_context * tcpc;
        atcmd_udps_context * udps;
    } ctx;
    union
    {
        atcmd_tcps_config tcps;
        atcmd_tcpc_config tcpc;
        atcmd_udps_config udps;
    } conf;
    struct _atcmd_sess_context * next;
} atcmd_sess_context;

typedef struct _atcmd_tls_context
{
    int            cid;
    atcmd_tls_role role;

    union
    {
        atcmd_tlsc_context * tlsc_ctx;
    } ctx;

    union
    {
        atcmd_tlsc_config tlsc_conf;
    } conf;

    struct _atcmd_tls_context * next;
} atcmd_tls_context;

/// NVRAM string value structure
typedef struct _atcmd_conf_str
{
    /// Parameter name (RRQ61X_ATCMD_CONF_STR)
    int id;

    /// NVRAM save name
    char nvram_name[24];

    /// Maximum length of the string value
    int max_length;
} atcmd_conf_str;

/// NVRAM integer value structure
typedef struct _atcmd_conf_int
{
    /// Parameter name (RRQ61X_ATCMD_CONF_INT)
    int id;

    /// NVRAM save name
    char nvram_name[24];

    /// Minimum value
    int min_value;

    /// Maximum value
    int max_value;

    /// Default value
    int def_value;
} atcmd_conf_int;

/// User Configurations (for string value)
typedef enum
{
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_0 = 1,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_1,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_2,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_3,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_4,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_5,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_6,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_7,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_8,
    RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_9,

    RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_0,
    RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_1,
    RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_0,
    RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_1,
    RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_0,
    RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_1,
    RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_0,
    RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_1,

    RRQ61X_CONF_STR_ATCMD_FINAL_MAX
} RRQ61X_ATCMD_CONF_STR;

/// User Configurations (for integer value)
typedef enum
{
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_0 = 1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_2,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_3,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_4,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_5,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_6,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_7,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_8,
    RRQ61X_CONF_INT_ATCMD_NW_TR_CID_9,

    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_0,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_2,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_3,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_4,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_5,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_6,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_7,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_8,
    RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_9,

    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_0,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_2,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_3,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_4,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_5,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_6,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_7,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_8,
    RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_9,

    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_0,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_2,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_3,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_4,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_5,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_6,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_7,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_8,
    RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_9,

    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_0,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_1,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_2,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_3,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_4,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_5,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_6,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_7,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_8,
    RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_9,

    RRQ61X_CONF_INT_ATCMD_TLS_CID_0,
    RRQ61X_CONF_INT_ATCMD_TLS_CID_1,
    RRQ61X_CONF_INT_ATCMD_TLS_ROLE_0,
    RRQ61X_CONF_INT_ATCMD_TLS_ROLE_1,
    RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_0,
    RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_1,
    RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_0,
    RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_1,
    RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_0,
    RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_1,
    RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_0,
    RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_1,
    RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_0,
    RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_1,
    RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_0,
    RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_1,

    RRQ61X_CONF_INT_ATCMD_FINAL_MAX
} RRQ61X_ATCMD_CONF_INT;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTS);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTS);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTS);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTC);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTC);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTC);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRUSE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRUSE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRUSE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRUR);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRUR);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRUR);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRPRT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRPRT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRPRT);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRPALL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRPALL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRPALL);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTRM);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTRM);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTRM);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTALL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTALL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTALL);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSAVE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSAVE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSAVE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TCPDATAMODE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TCPDATAMODE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TCPDATAMODE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLINIT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLINIT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLINIT);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCFG);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCFG);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCFG);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCO);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCO);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCO);

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(TRSSLWR);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLWR);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLWR);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCL);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCL);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLPRT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLPRT);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLPRT);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCERTLIST);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTLIST);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTLIST);

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(TRSSLCERTSTORE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTSTORE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTSTORE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCERTDELETE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTDELETE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTDELETE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLSAVE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLSAVE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLSAVE);

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLDELETE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLDELETE);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLDELETE);

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(M);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(M);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(M);

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(H);
RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(H);
RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(H);

static int set_atcmd_param_str(int name, char * value);

#if CFG_PMGR
static int get_atcmd_param_str(int name, char * value, size_t value_size);

#endif                                 /* CFG_PMGR */
static int set_atcmd_param_int(int name, int value);
static int get_atcmd_param_int(int name, int * value);

static int                  atcmd_transport_create_atcmd_sess_ctx_mutex();
static int                  atcmd_transport_take_atcmd_sess_ctx_mutex(unsigned int timeout);
static int                  atcmd_transport_give_atcmd_sess_ctx_mutex();
static int                  atcmd_transport_set_max_session(const unsigned int cnt);
static unsigned int         atcmd_transport_get_max_session();
static unsigned int         atcmd_transport_get_max_cid();
static atcmd_sess_context * atcmd_transport_alloc_context(const atcmd_sess_type type);
static atcmd_sess_context * atcmd_transport_create_context(const atcmd_sess_type type);
static atcmd_sess_context * atcmd_transport_find_context(int cid);
static void                 atcmd_transport_free_context(atcmd_sess_context ** ctx);
static int                  atcmd_transport_delete_context(int cid);

static int  atcmd_network_get_nvr_id(int cid, atcmd_sess_nvr_type type);
static void atcmd_network_display_sess_info(int cid, atcmd_sess_info * sess_info);
static int  atcmd_network_save_tcps_context(atcmd_sess_context * p_ctx, int is_rtm);
static int  atcmd_network_save_tcpc_context(atcmd_sess_context * ctx, int is_rtm);
static int  atcmd_network_save_udps_context(atcmd_sess_context * p_ctx, int is_rtm);
static int  atcmd_network_save_context_nvram(void);

#if CFG_PMGR
static int atcmd_network_recover_context_nvram(atcmd_w_ctrl_t * const p_at_ctrl,
                                               atcmd_sess_info      * p_sess_info,
                                               const int              sess_info_cnt);

#endif                                 /* CFG_PMGR */
static int               atcmd_network_get_sess_info_cnt();
static atcmd_sess_info * atcmd_network_get_sess_info(const int cid);
static int               atcmd_network_clear_sess_info(int cid);
static int               atcmd_network_delete_sess_info(void);

#if CFG_PMGR
static int atcmd_network_recover_session(atcmd_w_ctrl_t * const p_at_ctrl,
                                         atcmd_sess_info      * p_sess_info,
                                         const int              sess_info_cnt);

#endif                                 /* CFG_PMGR */

static int atcmd_network_check_network_ready(const char * module, int iface, int ip_type, unsigned int timeout);

/* TCP server */
static int atcmd_network_connect_tcps(atcmd_w_ctrl_t * const p_at_ctrl,
                                      atcmd_sess_context   * p_ctx,
                                      int                    port,
                                      int                    max_peer_cnt,
                                      int                    ip_type);
static int atcmd_network_disconnect_tcps(const int cid);
static int atcmd_network_disconnect_tcps_cli(const int cid, const char * ip_addr, const int port);
static int atcmd_network_display_tcps(atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix);

/* TCP client */
static int atcmd_network_connect_tcpc(atcmd_w_ctrl_t * const p_at_ctrl,
                                      atcmd_sess_context   * p_ctx,
                                      char                 * p_svr_ip,
                                      int                    svr_port,
                                      int                    port);
static int atcmd_network_disconnect_tcpc(const int cid);
static int atcmd_network_display_tcpc(atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix);

/* UDP session */
static int atcmd_network_connect_udps(atcmd_w_ctrl_t * const p_at_ctrl,
                                      atcmd_sess_context   * p_ctx,
                                      char                 * p_peer_ip,
                                      int                    peer_port,
                                      int                    port,
                                      int                    ip_type);
static int atcmd_network_disconnect_udps(const int cid);
static int atcmd_network_display_udps(atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix);

static int atcmd_network_display(atcmd_w_ctrl_t * const p_at_ctrl, const int cid);
static int atcmd_network_terminate_session(const int cid);

static int atcmd_transport_ssl_create_tls_client(int * cid);
static int atcmd_transport_ssl_delete_tls_client(int cid);
static int atcmd_transport_ssl_run_tls_client(atcmd_tls_context * ctx);
static int atcmd_transport_ssl_recover_tls_session(atcmd_w_ctrl_t * const p_at_ctrl);

#if CFG_PMGR
static void atcmd_transport_ssl_recover_context_nvram(void);

#endif                                 /* CFG_PMGR */
static int atcmd_transport_ssl_save_context_nvram(atcmd_tls_context * ctx);
static int atcmd_transport_ssl_clear_all_context_nvram(void);
static int atcmd_transport_ssl_save_tlsc_nvram(atcmd_tls_context * ctx, int profile_idx);

#if CFG_PMGR
static int  atcmd_transport_ssl_recover_tlsc_nvram(atcmd_tls_context * ctx, int profile_idx);
static void atcmd_transport_ssl_recover_context_dpm(void);
static void atcmd_transport_ssl_update_context_dpm(void);

#endif                                 /* CFG_PMGR */
static atcmd_tls_context * atcmd_transport_ssl_create_context(void);
static atcmd_tls_context * atcmd_transport_ssl_find_context(int cid);
static int                 atcmd_transport_ssl_delete_context(int cid);
static int                 atcmd_transport_ssl_do_init(int role, int * cid);
static int                 atcmd_transport_ssl_do_cfg(char * cid_str, char * conf_id_str, char * conf_val_str);
static int                 atcmd_transport_ssl_do_co(atcmd_w_ctrl_t * const p_at_ctrl,
                                                     char                 * cid_str,
                                                     char                 * ip_str,
                                                     char                 * port_str);
static int atcmd_transport_ssl_do_cl(char * cid_str, int * cid);
static int atcmd_transport_ssl_do_certlist(char * type_str, char * out, int outlen);
static int                    atcmd_transport_ssl_do_certdelete(char * type_str, char * name_str);
static int                    atcmd_transport_ssl_do_save(void);
static int                    atcmd_transport_ssl_do_delete(void);
static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_read_trsslwr_cmd(atcmd_w_ctrl_t * const p_at_ctrl,
                                                                      char                 * p_atcmd,
                                                                      size_t                 atcmd_len,
                                                                      char                ** pp_data,
                                                                      size_t               * p_data_len);
static int atcmd_transport_ssl_send_tls_client(int      cid,
                                               char   * dst_ip,
                                               char   * dst_port,
                                               char   * data,
                                               size_t * datalen);
static int atcmd_transport_ssl_do_wr(char   * cid_str,
                                     char   * dst_ip_str,
                                     char   * dst_port_str,
                                     size_t * data_len,
                                     char   * p_data);
static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_display_tls_session(int cid, char * p_out, size_t outlen);
static int                    count_int_len(int val);

static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_read_esc_cmd(atcmd_w_ctrl_t * const p_at_ctrl,
                                                                  char                 * p_atcmd,
                                                                  size_t                 atcmd_len,
                                                                  char                ** pp_data,
                                                                  size_t               * p_data_len);
static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_transfer_msg(atcmd_w_ctrl_t * const p_at_ctrl,
                                                                  char                 * p_in,
                                                                  size_t                 inlen);

static unsigned int rm_atcmd_w_core_socket_is_using_heap(void);
static void         rm_atcmd_w_core_socket_set_heap_alloc(unsigned int val);

#if (ATCMD_SECURE_CHANNEL == 1)
static int atcmd_transport_ssl_do_certstore(char * type_str,
                                            char * seq_str,
                                            char * format_str,
                                            char * name_str,
                                            char * p_data,
                                            size_t data_len);
uint32_t RM_ATCMD_W_CORE_SOCKET_TRSSLWR_fixed_cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[]);
uint32_t RM_ATCMD_W_CORE_SOCKET_TRSSLCERTSTORE_fixed_cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc,
                                                            char * argv[]);

#endif

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_socket_module[] =
{
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRTS),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRTS),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRTS),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRTS)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRTC),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRTC),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRTC),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRTC)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRUSE),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRUSE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRUSE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRUSE)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRUR),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRUR),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRUR),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRUR)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRPRT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRPRT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRPRT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRPRT)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRPALL),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRPALL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRPALL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRPALL)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRTRM),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRTRM),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRTRM),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRTRM)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRTALL),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRTALL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRTALL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRTALL)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSAVE),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSAVE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSAVE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSAVE)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TCPDATAMODE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TCPDATAMODE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TCPDATAMODE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TCPDATAMODE)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLINIT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLINIT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLINIT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLINIT)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCFG),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCFG),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCFG),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCFG)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCO),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCO),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCO),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCO)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCL),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCL)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLPRT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLPRT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLPRT),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLPRT)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCERTLIST),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCERTLIST),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCERTLIST),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCERTLIST)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCERTDELETE),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCERTDELETE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCERTDELETE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCERTDELETE)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLSAVE),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLSAVE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLSAVE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLSAVE)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLDELETE),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLDELETE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLDELETE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLDELETE)
    },
#if (ATCMD_SECURE_CHANNEL == 1)
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLWR),
        ATCMD_W_TYPE_SECURE_UNFIXED,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_TRSSLWR_fixed_cmd_cb,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLWR),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLWR)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCERTSTORE),
        ATCMD_W_TYPE_SECURE_UNFIXED,
        1,
        0,
        RM_ATCMD_W_CORE_SOCKET_TRSSLCERTSTORE_fixed_cmd_cb,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCERTSTORE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCERTSTORE)
    },
#endif
    {
        NULL,
        ATCMD_W_TYPE_MAX,
        0,
        0,
        NULL,
        NULL,
        NULL
    },
};

const atcmd_w_core_unfixed_module_t at_core_socket_unfixed_module[] =
{
    {                                  /* This command can not ues secure channel. */
        {AT_CMD_ESC_KEY_CHAR, 'M', 0x00, },
        2,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(M),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(M),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(M),
    },
    {                                  /* This command can not ues secure channel. */
        {AT_CMD_ESC_KEY_CHAR, 'H', 0x00, },
        2,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(H),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(H),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(H),
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLWR),
        10,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLWR),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLWR),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLWR)
    },
    {
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLCERTSTORE),
        17,
        RM_ATCMD_W_CORE_SOCKET_ATCMD_CB_P(TRSSLCERTSTORE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB_P(TRSSLCERTSTORE),
        RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB_P(TRSSLCERTSTORE)
    },
    {
        "",
        0,
        NULL,
        NULL,
        NULL
    },
};

/* UNCRUSTIFY-OFF */
static const atcmd_conf_str atcmd_config_str_with_nvram_name[] =
{
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_0, ATCMD_NVR_NW_TR_PEER_IPADDR_0,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_1, ATCMD_NVR_NW_TR_PEER_IPADDR_1,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_2, ATCMD_NVR_NW_TR_PEER_IPADDR_2,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_3, ATCMD_NVR_NW_TR_PEER_IPADDR_3,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_4, ATCMD_NVR_NW_TR_PEER_IPADDR_4,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_5, ATCMD_NVR_NW_TR_PEER_IPADDR_5,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_6, ATCMD_NVR_NW_TR_PEER_IPADDR_6,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_7, ATCMD_NVR_NW_TR_PEER_IPADDR_7,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_8, ATCMD_NVR_NW_TR_PEER_IPADDR_8,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_9, ATCMD_NVR_NW_TR_PEER_IPADDR_9,  ATCMD_NVR_NW_TR_PEER_IPADDR_LEN  },
    { RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_0, ATCMD_TLSC_NVR_CA_CERT_NAME_0,  ATCMD_TLSC_NVR_CA_CERT_NAME_LEN  },
    { RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_1, ATCMD_TLSC_NVR_CA_CERT_NAME_1,  ATCMD_TLSC_NVR_CA_CERT_NAME_LEN  },
    { RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_0,    ATCMD_TLSC_NVR_CERT_NAME_0,     ATCMD_TLSC_NVR_CERT_NAME_LEN     },
    { RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_1,    ATCMD_TLSC_NVR_CERT_NAME_1,     ATCMD_TLSC_NVR_CERT_NAME_LEN     },
    { RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_0,    ATCMD_TLSC_NVR_HOST_NAME_0,     ATCMD_TLSC_NVR_HOST_NAME_LEN     },
    { RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_1,    ATCMD_TLSC_NVR_HOST_NAME_1,     ATCMD_TLSC_NVR_HOST_NAME_LEN     },
    { RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_0,  ATCMD_TLSC_NVR_PEER_IPADDR_0,   ATCMD_TLSC_NVR_PEER_IPADDR_LEN   },
    { RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_1,  ATCMD_TLSC_NVR_PEER_IPADDR_1,   ATCMD_TLSC_NVR_PEER_IPADDR_LEN   },
    { 0, "", 0 }
};
/* UNCRUSTIFY-ON */

/* UNCRUSTIFY-OFF */
static const atcmd_conf_int atcmd_config_int_with_nvram_name[] =
{
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_0, ATCMD_NVR_NW_TR_CID_0, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_1, ATCMD_NVR_NW_TR_CID_1, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_2, ATCMD_NVR_NW_TR_CID_2, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_3, ATCMD_NVR_NW_TR_CID_3, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_4, ATCMD_NVR_NW_TR_CID_4, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_5, ATCMD_NVR_NW_TR_CID_5, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_6, ATCMD_NVR_NW_TR_CID_6, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_7, ATCMD_NVR_NW_TR_CID_7, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_8, ATCMD_NVR_NW_TR_CID_8, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_CID_9, ATCMD_NVR_NW_TR_CID_9, ATCMD_SESS_NONE, ATCMD_SESS_UDP_SESSION, ATCMD_SESS_NONE         },

    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_0,       ATCMD_NVR_NW_TR_LOCAL_PORT_0,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_1,       ATCMD_NVR_NW_TR_LOCAL_PORT_1,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_2,       ATCMD_NVR_NW_TR_LOCAL_PORT_2,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_3,       ATCMD_NVR_NW_TR_LOCAL_PORT_3,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_4,       ATCMD_NVR_NW_TR_LOCAL_PORT_4,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_5,       ATCMD_NVR_NW_TR_LOCAL_PORT_5,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_6,       ATCMD_NVR_NW_TR_LOCAL_PORT_6,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_7,       ATCMD_NVR_NW_TR_LOCAL_PORT_7,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_8,       ATCMD_NVR_NW_TR_LOCAL_PORT_8,  0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_9,       ATCMD_NVR_NW_TR_LOCAL_PORT_9,  0,  65535, 0 },

    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_0,        ATCMD_NVR_NW_TR_PEER_PORT_0,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_1,        ATCMD_NVR_NW_TR_PEER_PORT_1,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_2,        ATCMD_NVR_NW_TR_PEER_PORT_2,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_3,        ATCMD_NVR_NW_TR_PEER_PORT_3,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_4,        ATCMD_NVR_NW_TR_PEER_PORT_4,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_5,        ATCMD_NVR_NW_TR_PEER_PORT_5,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_6,        ATCMD_NVR_NW_TR_PEER_PORT_6,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_7,        ATCMD_NVR_NW_TR_PEER_PORT_7,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_8,        ATCMD_NVR_NW_TR_PEER_PORT_8,   0,  65535, 0 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_9,        ATCMD_NVR_NW_TR_PEER_PORT_9,   0,  65535, 0 },

    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_0, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_0, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_1, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_1, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_2, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_2, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_3, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_3, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_4, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_4, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_5, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_5, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_6, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_6, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_7, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_7, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_8, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_8, 0, 0, 1 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_9, ATCMD_NVR_NW_TR_MAX_ALLOWED_PEER_9, 0, 0, 1 },

    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_0, ATCMD_NVR_NW_TR_IP_TYPE_0, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_1, ATCMD_NVR_NW_TR_IP_TYPE_1, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_2, ATCMD_NVR_NW_TR_IP_TYPE_2, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_3, ATCMD_NVR_NW_TR_IP_TYPE_3, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_4, ATCMD_NVR_NW_TR_IP_TYPE_4, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_5, ATCMD_NVR_NW_TR_IP_TYPE_5, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_6, ATCMD_NVR_NW_TR_IP_TYPE_6, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_7, ATCMD_NVR_NW_TR_IP_TYPE_7, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_8, ATCMD_NVR_NW_TR_IP_TYPE_8, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },
    { RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_9, ATCMD_NVR_NW_TR_IP_TYPE_9, IPADDR_TYPE_V4, IPADDR_TYPE_V6, IPADDR_TYPE_V4 },

    { RRQ61X_CONF_INT_ATCMD_TLS_CID_0,      ATCMD_TLS_NVR_CID_0,              -1,       ATCMD_TLS_MAX_ALLOW_CNT,          -1     },
    { RRQ61X_CONF_INT_ATCMD_TLS_CID_1,      ATCMD_TLS_NVR_CID_1,              -1,       ATCMD_TLS_MAX_ALLOW_CNT,          -1     },
    { RRQ61X_CONF_INT_ATCMD_TLS_ROLE_0,     ATCMD_TLS_NVR_ROLE_0,             -1,       1,                                -1     },
    { RRQ61X_CONF_INT_ATCMD_TLS_ROLE_1,     ATCMD_TLS_NVR_ROLE_1,             -1,       1,                                -1     },
    { RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_0,  ATCMD_TLS_NVR_PROFILE_0,          -1,       ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE, -1     },
    { RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_1,  ATCMD_TLS_NVR_PROFILE_1,          -1,       ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE, -1     },
    { RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_0,    ATCMD_TLSC_NVR_INCOMING_LEN_0,      ATCMD_TLSC_MIN_INCOMING_LEN, ATCMD_TLSC_MAX_INCOMING_LEN, ATCMD_TLSC_DEF_INCOMING_LEN },
    { RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_1,    ATCMD_TLSC_NVR_INCOMING_LEN_1,      ATCMD_TLSC_MIN_INCOMING_LEN, ATCMD_TLSC_MAX_INCOMING_LEN, ATCMD_TLSC_DEF_INCOMING_LEN },
    { RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_0,    ATCMD_TLSC_NVR_OUTGOING_LEN_0,      ATCMD_TLSC_MIN_OUTGOING_LEN, ATCMD_TLSC_MAX_OUTGOING_LEN, ATCMD_TLSC_DEF_OUTGOING_LEN },
    { RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_1,    ATCMD_TLSC_NVR_OUTGOING_LEN_1,      ATCMD_TLSC_MIN_OUTGOING_LEN, ATCMD_TLSC_MAX_OUTGOING_LEN, ATCMD_TLSC_DEF_OUTGOING_LEN },
    { RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_0,  ATCMD_TLSC_NVR_AUTH_MODE_0,    pdFALSE,  pdTRUE,  pdFALSE                         },
    { RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_1,  ATCMD_TLSC_NVR_AUTH_MODE_1,    pdFALSE,  pdTRUE,  pdFALSE                         },
    { RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_0, ATCMD_TLSC_NVR_LOCAL_PORT_0,   0,        65535,   0                               },
    { RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_1, ATCMD_TLSC_NVR_LOCAL_PORT_1,   0,        65535,   0                               },
    { RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_0,  ATCMD_TLSC_NVR_PEER_PORT_0,    0,        65535,   0                               },
    { RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_1,  ATCMD_TLSC_NVR_PEER_PORT_1,    0,        65535,   0                               },
    { 0, "", 0, 0, 0 }
};
/* UNCRUSTIFY-ON */

static const atcmd_conf_str * atcmd_conf_str_table = atcmd_config_str_with_nvram_name;
static const atcmd_conf_int * atcmd_conf_int_table = atcmd_config_int_with_nvram_name;

#if CFG_PMGR
unsigned int        atcmd_sess_max_session              = ATCMD_NW_TR_MAX_SESSION_CNT_DPM;
unsigned int        atcmd_sess_max_cid                  = (ATCMD_NW_TR_MAX_SESSION_CNT_DPM + 1); // + Reserved CID(0,1,2)
static unsigned int g_rm_atcmd_w_core_socket_using_heap = pdTRUE;
#else
unsigned int        atcmd_sess_max_session              = ATCMD_NW_TR_MAX_SESSION_CNT;
unsigned int        atcmd_sess_max_cid                  = (ATCMD_NW_TR_MAX_SESSION_CNT + 1);     // + Reserved CID(0,1,2)
static unsigned int g_rm_atcmd_w_core_socket_using_heap = pdFALSE;
#endif /* CFG_PMGR */
atcmd_sess_info    * atcmd_sess_info_header = NULL;
atcmd_sess_context * atcmd_sess_ctx_header  = NULL;

const TickType_t  atcmd_sess_ctx_mutex_timeout = portMAX_DELAY;
SemaphoreHandle_t atcmd_sess_ctx_mutex         = NULL;

atcmd_tls_context * atcmd_tls_ctx_header     = NULL;
atcmd_tls_context * atcmd_tls_ctx_rtm_header = NULL;

#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
static int g_atcmd_w_core_tcp_recv_data_mode = 0;
#endif                                 // __SUPPORT_TCP_RECVDATA_HEX_MODE__

static TaskHandle_t rm_atcmd_w_core_sock_init_task_handler = NULL;

static uint8_t gs_esc_cmd_buffer[RM_ATCMD_W_CORE_SOCKET_MAX_TX_SIZE + 512] = {0};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_SOCKET_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    if (p_list->unfixed_module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    if (rm_atcmd_w_core_register_module_node(p_list, at_core_socket_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (rm_atcmd_w_core_register_unfixed_module_node(p_list,
                                                     at_core_socket_unfixed_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_SOCKET_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_socket_module);
    rm_atcmd_w_core_unfixed_deregister(p_list, at_core_socket_unfixed_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_SOCKET_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err       = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_sess_info      * sess_info = NULL;
#if CFG_PMGR
    int sess_info_cnt  = 0;
    int is_reg_tmp_dpm = pdFALSE;

    const int rtm_init_timeout = 10;
    const int max_rtm_init_cnt = 10;
    int       rtm_init_cnt     = 0;

    if (RM_PMGR_W_dpm_is_enabled() && (RM_PMGR_W_dpm_is_wakeup() == pdFALSE))
    {
        while ((rtm_init_cnt < max_rtm_init_cnt) && (RM_PMGR_W_user_rtm_is_init_done() == pdFALSE))
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(rtm_init_timeout));
            rtm_init_cnt++;
        }

        if (rtm_init_cnt >= max_rtm_init_cnt)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("User RTM is not init\n");

            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_CONFIG;
        }
    }
#endif                                 /* CFG_PMGR */

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        rm_atcmd_w_core_socket_set_heap_alloc(pdFALSE);
    }
    else
#else
    {
        rm_atcmd_w_core_socket_set_heap_alloc(pdTRUE);
    }
#endif

    /* Set max session cnt */
#if CFG_PMGR
    {
        atcmd_transport_set_max_session(ATCMD_NW_TR_MAX_SESSION_CNT_DPM);
    }
    sess_info_cnt = atcmd_network_get_sess_info_cnt();
#else
    {
        atcmd_transport_set_max_session(ATCMD_NW_TR_MAX_SESSION_CNT);
    }
#endif                                 /* CFG_PMGR */

    sess_info = atcmd_network_get_sess_info(0);

    if (!sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get user session info\n");
    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_wakeup() == DPM_WAKEUP)
    {
        atcmd_transport_ssl_recover_context_dpm();
    }
    else if (RM_PMGR_W_dpm_is_enabled() == DPM_ENABLED)
    {
        if (sess_info)
        {
            atcmd_network_recover_context_nvram(p_at_ctrl, sess_info, sess_info_cnt);
        }

        atcmd_transport_ssl_recover_context_nvram();
    }

    /* Is required to regiter temp dpm to wait */
    if ((RM_PMGR_W_dpm_is_enabled() == DPM_ENABLED) && (RM_PMGR_W_dpm_is_wakeup() != DPM_WAKEUP))
    {
        if (sess_info)
        {
            for (int cid = 0; cid < sess_info_cnt; cid++)
            {
                if (sess_info[cid].type != ATCMD_SESS_NONE)
                {
                    is_reg_tmp_dpm = pdTRUE;
                    break;
                }
            }
        }

        if (atcmd_tls_ctx_header)
        {
            is_reg_tmp_dpm = pdTRUE;
        }

        if (is_reg_tmp_dpm)
        {
            RM_ATCMD_W_CORE_SOCKET_DEBUG("Register DPM(%s)\n", ATCMD_DPM_TMP);
            RM_PMGR_W_dpm_job_name_set(ATCMD_DPM_TMP, 0);
            RM_PMGR_W_dpm_sleep_ready_clear(ATCMD_DPM_TMP);
        }
    }

    /* Recover tcp server, client and udp session in DPM */
    if (RM_PMGR_W_dpm_is_enabled() && sess_info)
    {
        atcmd_network_recover_session(p_at_ctrl, sess_info, sess_info_cnt);
    }
    else
#endif                                 /* CFG_PMGR */
    {
        /* Not required sess_info. */
        atcmd_network_delete_sess_info();
    }

    // Recover tls session
    atcmd_transport_ssl_recover_tls_session(p_at_ctrl);

#if CFG_PMGR
    if (is_reg_tmp_dpm)
    {
        while (1)
        {
            if (chk_network_ready(0) == 1)
            {
                break;
            }

            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }

        atcmd_network_display(p_at_ctrl, 0xFF);
        RM_PMGR_W_dpm_job_name_clear(ATCMD_DPM_TMP);
    }
#endif                                 /* CFG_PMGR */

    return err;
}

static void RM_ATCMD_W_CORE_SOCKET_init_task (void * pvParameters)
{
    atcmd_w_ctrl_t * const         p_at_ctrl = (atcmd_w_ctrl_t * const) pvParameters;
    atcmd_w_core_instance_ctrl_t * p_ctrl    = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    atcmd_w_core_module_list_t   * p_list    = &p_ctrl->list;

    RM_ATCMD_W_CORE_SOCKET_open(p_at_ctrl);
    RM_ATCMD_W_CORE_SOCKET_register(p_list);

    vTaskDelete(NULL);
}

void RM_ATCMD_W_CORE_SOCKET_init_start (atcmd_w_ctrl_t * const p_at_ctrl)
{
    BaseType_t ret;

    ret = xTaskCreate(RM_ATCMD_W_CORE_SOCKET_init_task,
                      "at_sock_init",
                      RM_ATCMD_W_CORE_SOCKET_INIT_STACK_SIZE,
                      (void *) p_at_ctrl,
                      (OS_TASK_PRIORITY_USER),
                      &rm_atcmd_w_core_sock_init_task_handler);

    if (ret != pdPASS)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Task Create ERROR!\n");
    }
}

uint32_t RM_ATCMD_W_CORE_SOCKET_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

static int set_atcmd_param_str (int name, char * value)
{
    const atcmd_conf_str * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_conf_str_table; cmd_ptr->id; cmd_ptr++)
    {
        if (name == cmd_ptr->id)
        {
            if (strlen(cmd_ptr->nvram_name) == 0)
            {
                break;
            }

            if (value == NULL)
            {
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, cmd_ptr->nvram_name);
#endif

                return CC_STATUS_SUCCESS;
            }

            if ((cmd_ptr->max_length > 0) && (strlen(value) > (unsigned int) cmd_ptr->max_length))
            {
                return CC_FAILURE_STRING_LENGTH;
            }

#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_APPCFG,
                                             cmd_ptr->nvram_name,
                                             value);
#endif
            result = CC_STATUS_SUCCESS;
            break;
        }
    }

    return result;
}

#if CFG_PMGR
static int get_atcmd_param_str (int name, char * value, size_t value_size)
{
    const atcmd_conf_str * cmd_ptr      = NULL;
    char                 * nvram_string = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_conf_str_table; cmd_ptr->id; cmd_ptr++)
    {
        if (name == cmd_ptr->id)
        {
            if (strlen(cmd_ptr->nvram_name) == 0)
            {
                break;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_APPCFG,
                                            cmd_ptr->nvram_name,
                                            &nvram_string);
 #endif

            if (nvram_string == NULL)
            {
                result = CC_FAILURE_NO_VALUE;
            }
            else
            {
                bsp_safe_strcpy(value, nvram_string, value_size);
                result = CC_STATUS_SUCCESS;
            }

            break;
        }
    }

    return result;
}

#endif                                 /* CFG_PMGR */

static int set_atcmd_param_int (int name, int value)
{
    const atcmd_conf_int * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_conf_int_table; cmd_ptr->id; cmd_ptr++)
    {
        if (name == cmd_ptr->id)
        {
            if (strlen(cmd_ptr->nvram_name) == 0)
            {
                break;
            }

            if ((cmd_ptr->min_value || cmd_ptr->max_value) &&
                ((value < cmd_ptr->min_value) || (value > cmd_ptr->max_value)))
            {
                return CC_FAILURE_RANGE_OUT;
            }

            if (value == cmd_ptr->def_value)
            {
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, cmd_ptr->nvram_name);
#endif
            }
            else
            {
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_APPCFG,
                                              cmd_ptr->nvram_name,
                                              value);
#endif
            }

            result = CC_STATUS_SUCCESS;
            break;
        }
    }

    return result;
}

static int get_atcmd_param_int (int name, int * value)
{
    const atcmd_conf_int * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;
    int nvram_int;

    for (cmd_ptr = atcmd_conf_int_table; cmd_ptr->id; cmd_ptr++)
    {
        if (name == cmd_ptr->id)
        {
            if (strlen(cmd_ptr->nvram_name) == 0)
            {
                break;
            }

#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_APPCFG,
                                         cmd_ptr->nvram_name,
                                         &nvram_int);
#endif

            if (nvram_int == -1)
            {
                *value = cmd_ptr->def_value;
            }
            else
            {
                *value = nvram_int;
            }

            result = CC_STATUS_SUCCESS;
            break;
        }
    }

    return result;
}

static int atcmd_transport_create_atcmd_sess_ctx_mutex ()
{
    if (!atcmd_sess_ctx_mutex)
    {
        atcmd_sess_ctx_mutex = xSemaphoreCreateMutex();

        if (atcmd_sess_ctx_mutex == NULL)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Faild to create atcmd_sess_ctx_mutex\n");

            return -1;
        }

        RM_ATCMD_W_CORE_SOCKET_DEBUG("Created atcmd_sess_ctx_mutex\n");
    }

    return 0;
}

static int atcmd_transport_take_atcmd_sess_ctx_mutex (unsigned int timeout)
{
    int ret = 0;

    if (atcmd_sess_ctx_mutex)
    {
        ret = xSemaphoreTake(atcmd_sess_ctx_mutex, timeout);

        if (ret != pdTRUE)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to take atcmd_sess_ctx_mutex\n");

            return -1;
        }
    }

    return 0;
}

static int atcmd_transport_give_atcmd_sess_ctx_mutex ()
{
    int ret = 0;

    if (atcmd_sess_ctx_mutex)
    {
        ret = xSemaphoreGive(atcmd_sess_ctx_mutex);

        if (ret != pdTRUE)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to give atcmd_sess_ctx_mutex(%d)\n", ret);

            return -1;
        }
    }

    return 0;
}

static int atcmd_transport_set_max_session (const unsigned int cnt)
{
#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled() && (cnt > ATCMD_NW_TR_MAX_SESSION_CNT_DPM))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter(%d)\n", cnt);

        return -1;
    }
    else if (!RM_PMGR_W_dpm_is_enabled() && (cnt > ATCMD_NW_TR_MAX_SESSION_CNT))
#else
    if (cnt > ATCMD_NW_TR_MAX_SESSION_CNT)
#endif                                 /* CFG_PMGR */
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter(%d)\n", cnt);

        return -1;
    }

    atcmd_sess_max_session = cnt;
    atcmd_sess_max_cid     = (atcmd_sess_max_session + 1);

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Set max session(%d), max CID(%d)\n", atcmd_sess_max_session, atcmd_sess_max_cid);

    return 0;
}

static unsigned int atcmd_transport_get_max_session (void)
{
    return atcmd_sess_max_session;
}

static unsigned int atcmd_transport_get_max_cid (void)
{
    return atcmd_sess_max_cid;
}

int atcmd_transport_get_available_session (void)
{
    atcmd_sess_context * ctx      = NULL;
    atcmd_sess_context * ctx_tmp  = NULL;
    atcmd_tcps_context * tcps_ctx = NULL;
    atcmd_tcpc_context * tcpc_ctx = NULL;
    atcmd_udps_context * udps_ctx = NULL;
    int session_num               = 0;

    for (ctx = atcmd_sess_ctx_header; ctx != NULL; ctx = ctx->next)
    {
        switch (ctx->type)
        {
            case ATCMD_SESS_TCP_SERVER:
            {
                ctx_tmp     = atcmd_transport_find_context(ctx->cid);
                tcps_ctx    = ctx_tmp->ctx.tcps;
                session_num = session_num + (tcps_ctx->cli_cnt);
                session_num = session_num + 1;
                break;
            }

            case ATCMD_SESS_TCP_CLIENT:
            {
                ctx_tmp  = atcmd_transport_find_context(ctx->cid);
                tcpc_ctx = ctx_tmp->ctx.tcpc;

                if (tcpc_ctx->state == ATCMD_TCPC_STATE_CONNECTED)
                {
                    session_num = session_num + 1;
                }

                break;
            }

            case ATCMD_SESS_UDP_SESSION:
            {
                ctx_tmp  = atcmd_transport_find_context(ctx->cid);
                udps_ctx = ctx_tmp->ctx.udps;

                if (udps_ctx->state == ATCMD_UDPS_STATE_ACTIVE)
                {
                    session_num = session_num + 1;
                }

                break;
            }

            default:
            {
                break;
            }
        }
    }

    return ATCMD_NW_TR_MAX_NVR_CNT - session_num;
}

static atcmd_sess_context * atcmd_transport_alloc_context (const atcmd_sess_type type)
{
    atcmd_sess_context * new_ctx  = NULL;
    size_t               ctx_size = 0;

    if (type == ATCMD_SESS_TCP_SERVER)
    {
        ctx_size = sizeof(atcmd_tcps_context);
    }
    else if (type == ATCMD_SESS_TCP_CLIENT)
    {
        ctx_size = sizeof(atcmd_tcpc_context);
    }
    else if (type == ATCMD_SESS_UDP_SESSION)
    {
        ctx_size = sizeof(atcmd_udps_context);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid type(%d)\n", type);

        return NULL;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("size info(atcmd_sess_context:%d, type:%d, ctx_size:%d)\n",
                                 sizeof(atcmd_sess_context),
                                 type,
                                 ctx_size);

    new_ctx = pvPortMalloc(sizeof(atcmd_sess_context));

    if (!new_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create atcmd_sess_context(%d)\n", sizeof(atcmd_sess_context));

        return NULL;
    }

    memset(new_ctx, 0x00, sizeof(atcmd_sess_context));

    if (ctx_size)
    {
        new_ctx->ctx.ptr = pvPortMalloc(ctx_size);

        if (!new_ctx->ctx.ptr)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate context(%d:%d)\n", type, ctx_size);
            goto err;
        }

        memset(new_ctx->ctx.ptr, 0x00, ctx_size);
    }

    return new_ctx;
err:
    atcmd_transport_free_context(&new_ctx);

    return NULL;
}

static void atcmd_transport_free_context (atcmd_sess_context ** ctx)
{
    atcmd_sess_context * ctx_ptr = *ctx;
    *ctx = NULL;

    if (ctx_ptr)
    {
        if (ctx_ptr->ctx.ptr)
        {
            vPortFree(ctx_ptr->ctx.ptr);
            ctx_ptr->ctx.ptr = NULL;
        }

        vPortFree(ctx_ptr);
        ctx_ptr = NULL;
    }
}

static atcmd_sess_context * atcmd_transport_create_context (const atcmd_sess_type type)
{
    const int            init_cid  = -1;
    const int            start_cid = 3; // ID_US + 1
    unsigned int         ctx_cnt   = 0;
    atcmd_sess_context * new_ctx   = NULL;
    atcmd_sess_context * cur_ctx   = NULL;
    atcmd_sess_context * prev_ctx  = NULL;

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("Before CID:\n");

    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t%d(%d), ", cur_ctx->cid, cur_ctx->type);
    }
    RM_ATCMD_W_CORE_SOCKET_DEBUG("sizeof(atcmd_sess_context):%d\n", sizeof(atcmd_sess_context));
#endif                                 // (DEBUG_ATCMD)

    atcmd_transport_create_atcmd_sess_ctx_mutex();

    atcmd_transport_take_atcmd_sess_ctx_mutex(atcmd_sess_ctx_mutex_timeout);

    /* Check number of context */
    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        ctx_cnt++;

        if (ctx_cnt >= atcmd_transport_get_max_session())
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Over max number of context(Max:%d,%d)\n",
                                         atcmd_transport_get_max_session(),
                                         ctx_cnt);
            goto err;
        }
    }

    /* Allocate atcmd_sess_context */
    new_ctx = atcmd_transport_alloc_context(type);

    if (!new_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create atcmd_sess_context\n");

        return NULL;
    }

    /* Init cid */
    new_ctx->cid = init_cid;

    /* Assign cid - reserved cid for compatibility */
    switch (type)
    {
        case ATCMD_SESS_TCP_SERVER:
        {
            if (!atcmd_transport_find_context(ID_TS))
            {
                new_ctx->cid = ID_TS;
            }

            break;
        }

        case ATCMD_SESS_TCP_CLIENT:
        {
            if (!atcmd_transport_find_context(ID_TC))
            {
                new_ctx->cid = ID_TC;
            }

            break;
        }

        case ATCMD_SESS_UDP_SESSION:
        {
            if (!atcmd_transport_find_context(ID_US))
            {
                new_ctx->cid = ID_US;
            }

            break;
        }

        case ATCMD_SESS_NONE:
        default:
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid type(%d)\n", type);
            goto err;
    }

    /* Assign type */
    new_ctx->type = type;

    /* Add linked-list */
    if (atcmd_sess_ctx_header == NULL)
    {
        atcmd_sess_ctx_header = new_ctx;
    }
    else if (new_ctx->cid == ID_TS)
    {
        new_ctx->next         = atcmd_sess_ctx_header;
        atcmd_sess_ctx_header = new_ctx;
    }
    else if (new_ctx->cid == ID_TC)
    {
        prev_ctx = atcmd_sess_ctx_header;
        cur_ctx  = atcmd_sess_ctx_header->next;

        if (prev_ctx->cid == ID_TS)
        {
            prev_ctx->next = new_ctx;
            new_ctx->next  = cur_ctx;
        }
        else
        {
            atcmd_sess_ctx_header = new_ctx;
            new_ctx->next         = prev_ctx;
        }
    }
    else if (new_ctx->cid == ID_US)
    {
        prev_ctx = atcmd_sess_ctx_header;
        cur_ctx  = atcmd_sess_ctx_header->next;

        if ((prev_ctx->cid == ID_TS) && (cur_ctx != NULL) && (cur_ctx->cid == ID_TC))
        {
            new_ctx->next = cur_ctx->next;
            cur_ctx->next = new_ctx;
        }
        else if ((prev_ctx->cid == ID_TS) || (prev_ctx->cid == ID_TC))
        {
            new_ctx->next  = prev_ctx->next;
            prev_ctx->next = new_ctx;
        }
        else
        {
            atcmd_sess_ctx_header = new_ctx;
            new_ctx->next         = prev_ctx;
        }
    }
    else
    {
        prev_ctx = atcmd_sess_ctx_header;
        cur_ctx  = atcmd_sess_ctx_header->next;

        while ((cur_ctx != NULL) && (cur_ctx->cid == ID_TS || cur_ctx->cid == ID_TC || cur_ctx->cid == ID_US))
        {
            prev_ctx = cur_ctx;
            cur_ctx  = cur_ctx->next;
        }

        if ((cur_ctx == NULL) || (cur_ctx->cid != start_cid))
        {
            new_ctx->cid   = start_cid;
            new_ctx->next  = prev_ctx->next;
            prev_ctx->next = new_ctx;
        }
        else
        {
            while (cur_ctx)
            {
                if ((cur_ctx->cid != start_cid) && (prev_ctx->cid + 1 != cur_ctx->cid))
                {
                    new_ctx->cid   = prev_ctx->cid + 1;
                    new_ctx->next  = prev_ctx->next;
                    prev_ctx->next = new_ctx;
                    break;
                }

                prev_ctx = cur_ctx;
                cur_ctx  = cur_ctx->next;
            }

            if (new_ctx->cid == init_cid)
            {
                new_ctx->cid   = prev_ctx->cid + 1;
                new_ctx->next  = prev_ctx->next;
                prev_ctx->next = new_ctx;
            }
        }
    }

    atcmd_transport_give_atcmd_sess_ctx_mutex();

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("After CID:\n");

    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t%d(%d), ", cur_ctx->cid, cur_ctx->type);
    }
#endif                                 // (DEBUG_ATCMD)

    return new_ctx;
err:
    atcmd_transport_free_context(&new_ctx);
    atcmd_transport_give_atcmd_sess_ctx_mutex();

    return NULL;
}

static atcmd_sess_context * atcmd_transport_find_context (int cid)
{
    atcmd_sess_context * cur_ctx = NULL;

    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        if (cur_ctx->cid == cid)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Found cid(%d) - type(%d)\n", cid, cur_ctx->type);

            return cur_ctx;
        }
    }

    return NULL;
}

static int atcmd_transport_delete_context (int cid)
{
    int ret = 0;
    atcmd_sess_context * cur_ctx  = NULL;
    atcmd_sess_context * prev_ctx = NULL;

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("Before CID:\n");

    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t%d(%d), ", cur_ctx->cid, cur_ctx->type);
    }
#endif                                 // (DEBUG_ATCMD)

    atcmd_transport_take_atcmd_sess_ctx_mutex(atcmd_sess_ctx_mutex_timeout);

    if (atcmd_sess_ctx_header == NULL)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("There is no sess context\n");
        ret = -1;
    }
    else if (atcmd_sess_ctx_header->cid == cid)
    {
        cur_ctx               = atcmd_sess_ctx_header;
        atcmd_sess_ctx_header = atcmd_sess_ctx_header->next;

        // Release memory
        atcmd_transport_free_context(&cur_ctx);
    }
    else
    {
        prev_ctx = atcmd_sess_ctx_header;
        cur_ctx  = atcmd_sess_ctx_header->next;

        while (cur_ctx)
        {
            if (cur_ctx->cid == cid)
            {
                prev_ctx->next = cur_ctx->next;

                // Release memory
                atcmd_transport_free_context(&cur_ctx);
                break;
            }

            prev_ctx = cur_ctx;
            cur_ctx  = cur_ctx->next;
        }
    }

    if (atcmd_sess_ctx_header == NULL)
    {
        atcmd_network_delete_sess_info();
    }

    atcmd_transport_give_atcmd_sess_ctx_mutex();

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("After CID:\n");

    for (cur_ctx = atcmd_sess_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t%d(%d), ", cur_ctx->cid, cur_ctx->type);
    }
#endif                                 // (DEBUG_ATCMD)

    return ret;
}

static int atcmd_network_get_nvr_id (int cid, atcmd_sess_nvr_type type)
{
    const RRQ61X_ATCMD_CONF_INT nvr_cid[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_0,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_1,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_2,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_3,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_4,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_5,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_6,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_7,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_8,
        RRQ61X_CONF_INT_ATCMD_NW_TR_CID_9
    };
    const RRQ61X_ATCMD_CONF_INT nvr_lport[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_0,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_1,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_2,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_3,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_4,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_5,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_6,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_7,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_8,
        RRQ61X_CONF_INT_ATCMD_NW_TR_LOCAL_PORT_9
    };
    const RRQ61X_ATCMD_CONF_INT nvr_pport[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_0,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_1,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_2,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_3,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_4,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_5,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_6,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_7,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_8,
        RRQ61X_CONF_INT_ATCMD_NW_TR_PEER_PORT_9
    };
    const RRQ61X_ATCMD_CONF_INT nvr_max_allowed_peer[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_0,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_1,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_2,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_3,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_4,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_5,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_6,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_7,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_8,
        RRQ61X_CONF_INT_ATCMD_NW_TR_MAX_ALLOWED_PEER_9
    };
    const RRQ61X_ATCMD_CONF_STR nvr_pipaddr[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_0,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_1,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_2,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_3,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_4,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_5,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_6,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_7,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_8,
        RRQ61X_CONF_STR_ATCMD_NW_TR_PEER_IPADDR_9
    };
    const RRQ61X_ATCMD_CONF_INT nvr_ip_type[ATCMD_NW_TR_MAX_NVR_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_0,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_1,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_2,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_3,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_4,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_5,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_6,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_7,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_8,
        RRQ61X_CONF_INT_ATCMD_NW_TR_IP_TYPE_9
    };

    if ((cid > (int) atcmd_transport_get_max_cid()) || (cid > ATCMD_NW_TR_MAX_NVR_CNT))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid cid(%d)\n", cid);

        return -1;
    }

    if (type == ATCMD_SESS_NVR_CID)
    {
        return (int) nvr_cid[cid];
    }
    else if (type == ATCMD_SESS_NVR_LOCAL_PORT)
    {
        return (int) nvr_lport[cid];
    }
    else if (type == ATCMD_SESS_NVR_PEER_PORT)
    {
        return (int) nvr_pport[cid];
    }
    else if (type == ATCMD_SESS_NVR_PEER_IP_ADDRESS)
    {
        return (int) nvr_pipaddr[cid];
    }
    else if (type == ATCMD_SESS_NVR_MAX_ALLOWED_PEER)
    {
        return (int) nvr_max_allowed_peer[cid];
    }
    else if (type == ATCMD_SESS_NVR_IP_TYPE)
    {
        return (int) nvr_ip_type[cid];
    }

    return -1;
}

static void atcmd_network_display_sess_info (int cid, atcmd_sess_info * sess_info)
{
#if defined(DEBUG_ATCMD)
    if (sess_info)
    {
        if (sess_info->type == ATCMD_SESS_TCP_SERVER)
        {
            RM_ATCMD_W_CORE_SOCKET_DEBUG("Session info\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n",
                                         "CID",
                                         cid,
                                         "Type",
                                         sess_info->type,
                                         "Local port",
                                         sess_info->sess.tcps.local_port,
                                         "Max allowed client",
                                         sess_info->sess.tcps.max_allow_client,
                                         "IP type",
                                         sess_info->ip_type);
        }
        else if (sess_info->type == ATCMD_SESS_TCP_CLIENT)
        {
            RM_ATCMD_W_CORE_SOCKET_DEBUG("Session info\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %s:%d\n",
                                         "CID",
                                         cid,
                                         "Type",
                                         sess_info->type,
                                         "Local port",
                                         sess_info->sess.tcpc.local_port,
                                         "Peer IP Address",
                                         sess_info->sess.tcpc.peer_ipaddr,
                                         sess_info->sess.tcpc.peer_port);
        }
        else if (sess_info->type == ATCMD_SESS_UDP_SESSION)
        {
            RM_ATCMD_W_CORE_SOCKET_DEBUG("Session info\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %d\n"
                                         "* %-20s : %s\n"
                                         "* %-20s : %d\n",
                                         "CID",
                                         cid,
                                         "Type",
                                         sess_info->type,
                                         "Local port",
                                         sess_info->sess.udps.local_port,
                                         "Peer IP Address",
                                         sess_info->sess.udps.peer_ipaddr,
                                         "Peer Port",
                                         sess_info->sess.udps.peer_port,
                                         "IP type",
                                         sess_info->ip_type);
        }
    }

#else
    RA6W1_UNUSED_ARG(cid);
    RA6W1_UNUSED_ARG(sess_info);
#endif                                 // DEBUG_ATCMD
}

// is_rtm: 0 is to save data to RTM. 1 is to save data to NVRAM.
static int atcmd_network_save_tcps_context (atcmd_sess_context * p_ctx, int is_rtm)
{
    int               ret             = 0;
    int               nvr_id          = -1;
    atcmd_sess_info   src_sess_info   = {0x00, };
    atcmd_sess_info * p_dst_sess_info = NULL;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    if ((p_ctx == NULL) || (p_ctx->type != ATCMD_SESS_TCP_SERVER))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    src_sess_info.type = p_ctx->type;
    memcpy(&src_sess_info.sess.tcps, p_ctx->conf.tcps.sess_info, sizeof(atcmd_tcps_sess_info));
    atcmd_network_display_sess_info(p_ctx->cid, &src_sess_info);

    if (is_rtm)
    {
        /* Save session info to rtm */
        p_dst_sess_info = atcmd_network_get_sess_info(p_ctx->cid);
        if (!p_dst_sess_info)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get memory to save session info\n");

            return -1;
        }

        memcpy(p_dst_sess_info, &src_sess_info, sizeof(atcmd_sess_info));
        ret = 0;
    }
    else
    {
        /* Save session info to nvram */
        /* Write local port */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_LOCAL_PORT);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.tcps.local_port);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local port of tcp server(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", p_ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write max allowed peer */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_MAX_ALLOWED_PEER);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.tcps.max_allow_client);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set max allowed client of tcp server(%d:%d)\n", p_ctx->cid,
                                             ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of max allowed peer(%d/%d)\n", p_ctx->cid, nvr_id);
            ret = -1;
            goto end_save_nvr;
        }

        /* Wirte IP type */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_IP_TYPE);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.tcps.ip_type);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set ip type of tcp server(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of ip type(%d/%d)\n", p_ctx->cid, nvr_id);
            ret = -1;
            goto end_save_nvr;
        }

end_save_nvr:

        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_CID);
        if (nvr_id != -1)
        {
            if (ret != -1)
            {
                ret = set_atcmd_param_int(nvr_id, (int) p_ctx->type);
                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to write cid(%d:%d)\n", p_ctx->cid, ret);
                    ret = -1;
                }
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of cid(%d)\n", p_ctx->cid);
            ret = -1;
        }
    }

    return ret;
}

static int atcmd_network_save_tcpc_context (atcmd_sess_context * ctx, int is_rtm)
{
    int               ret           = 0;
    int               nvr_id        = -1;
    atcmd_sess_info   src_sess_info = {0x00, };
    atcmd_sess_info * dst_sess_info = NULL;

    if ((ctx == NULL) || (ctx->type != ATCMD_SESS_TCP_CLIENT))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    src_sess_info.type = ctx->type;
    memcpy(&src_sess_info.sess.tcpc, ctx->conf.tcpc.sess_info, sizeof(atcmd_tcpc_sess_info));
    atcmd_network_display_sess_info(ctx->cid, &src_sess_info);

    if (is_rtm)
    {
        /* Save session info to rtm */
        dst_sess_info = atcmd_network_get_sess_info(ctx->cid);

        if (!dst_sess_info)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get memory to save session info\n");

            return -1;
        }

        memcpy(dst_sess_info, &src_sess_info, sizeof(atcmd_sess_info));
        ret = 0;
    }
    else
    {
        /* Save session info to nvram */
        /* Write local port */
        nvr_id = atcmd_network_get_nvr_id(ctx->cid, ATCMD_SESS_NVR_LOCAL_PORT);

        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.tcpc.local_port);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local port of tcp client(%d:%d)\n", ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write peer port */
        nvr_id = atcmd_network_get_nvr_id(ctx->cid, ATCMD_SESS_NVR_PEER_PORT);

        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.tcpc.peer_port);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set peer port of tcp client(%d:%d)\n", ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write peer IP address */
        nvr_id = atcmd_network_get_nvr_id(ctx->cid, ATCMD_SESS_NVR_PEER_IP_ADDRESS);

        if (nvr_id != -1)
        {
            ret = set_atcmd_param_str(nvr_id, src_sess_info.sess.tcpc.peer_ipaddr);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set peer ip address of tcp client(%d:%d)\n", ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of peer ip address(%d)\n", ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

end_save_nvr:
        nvr_id = atcmd_network_get_nvr_id(ctx->cid, ATCMD_SESS_NVR_CID);

        if (nvr_id != -1)
        {
            if (ret != -1)
            {
                ret = set_atcmd_param_int(nvr_id, (int) ctx->type);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to write cid(%d:%d)\n", ctx->cid, ret);
                    ret = -1;
                }
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of cid(%d)\n", ctx->cid);
            ret = -1;
        }
    }

    return ret;
}

/* is_rtm: 0 is to save data to NVRAM. 1 is to save data to RTM. */
static int atcmd_network_save_udps_context (atcmd_sess_context * p_ctx, int is_rtm)
{
    int               ret             = 0;
    int               nvr_id          = -1;
    atcmd_sess_info   src_sess_info   = {0x00, };
    atcmd_sess_info * p_dst_sess_info = NULL;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    if ((p_ctx == NULL) || (p_ctx->type != ATCMD_SESS_UDP_SESSION))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    src_sess_info.type = p_ctx->type;
    memcpy(&src_sess_info.sess.udps, p_ctx->conf.udps.sess_info, sizeof(atcmd_udps_sess_info));
    atcmd_network_display_sess_info(p_ctx->cid, &src_sess_info);

    if (is_rtm)
    {
        /* Save session info to rtm */
        p_dst_sess_info = atcmd_network_get_sess_info(p_ctx->cid);
        if (!p_dst_sess_info)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get memory to save session info\n");

            return -1;
        }

        memcpy(p_dst_sess_info, &src_sess_info, sizeof(atcmd_sess_info));
        ret = 0;
    }
    else
    {
        /* Save session info to nvram */
        /* Write local port */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_LOCAL_PORT);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.udps.local_port);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local port of udp session(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", p_ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write peer addr */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_PEER_IP_ADDRESS);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_str(nvr_id, src_sess_info.sess.udps.peer_ipaddr);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set peer addr v4 of udp session(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of peer addr v4 (%d)\n", p_ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write peer port */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_PEER_PORT);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.udps.peer_port);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set peer port of udp session(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of peer port (%d)\n", p_ctx->cid);
            ret = -1;
            goto end_save_nvr;
        }

        /* Write IP type */
        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_IP_TYPE);
        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, src_sess_info.sess.udps.ip_type);
            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set ip type of udp session(%d:%d)\n", p_ctx->cid, ret);
                ret = -1;
                goto end_save_nvr;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of ip type(%d:%d)\n", p_ctx->cid, nvr_id);
            ret = -1;
            goto end_save_nvr;
        }

end_save_nvr:

        nvr_id = atcmd_network_get_nvr_id(p_ctx->cid, ATCMD_SESS_NVR_CID);
        if (nvr_id != -1)
        {
            if (ret != -1)
            {
                ret = set_atcmd_param_int(nvr_id, (int) p_ctx->type);
                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to write cid(%d:%d)\n", p_ctx->cid, ret);
                    ret = -1;
                }
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of cid(%d)\n", p_ctx->cid);
            ret = -1;
        }
    }

    return ret;
}

static int atcmd_network_save_context_nvram (void)
{
    int ret = 0;
    atcmd_sess_context * ctx   = NULL;
    int                cid     = 0;
    int                nvr_id  = 0;
    const unsigned int max_cid = atcmd_transport_get_max_cid();

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    if (!atcmd_sess_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("there is no session\n");

        return -1;
    }

    /* Clear previous configuration */
    for (cid = 0; cid <= (const int) max_cid; cid++)
    {
        nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_CID);

        if (nvr_id != -1)
        {
            ret = set_atcmd_param_int(nvr_id, ATCMD_SESS_NONE);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to delete cid(%d:%d)\n", cid, ret);
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of cid(%d)\n", cid);
        }
    }

    for (ctx = atcmd_sess_ctx_header; ctx != NULL; ctx = ctx->next)
    {
        if (ctx->type == ATCMD_SESS_TCP_SERVER)
        {
            ret = atcmd_network_save_tcps_context(ctx, 0);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to save tcp server's context to nvram(%d)\n", ret);
            }
        }
        else if (ctx->type == ATCMD_SESS_TCP_CLIENT)
        {
            ret = atcmd_network_save_tcpc_context(ctx, 0);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to save tcp client's context to nvram(%d)\n", ret);
            }
        }
        else if (ctx->type == ATCMD_SESS_UDP_SESSION)
        {
            ret = atcmd_network_save_udps_context(ctx, 0);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to save udp session's context to nvram(%d)\n", ret);
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid type(%d)\n", ctx->type);
            ret = -2;
        }
    }

    return 0;
}

#if CFG_PMGR
static int atcmd_network_recover_context_nvram (atcmd_w_ctrl_t * const p_at_ctrl,
                                                atcmd_sess_info      * p_sess_info,
                                                const int              sess_info_cnt)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    int  status         = 0;
    int  ret            = 0;
    int  cid            = 0;
    int  nvr_id         = 0;
    int  type           = 0;
    int  lport          = 0;
    int  pport          = 0;
    int  max_allow_peer = 0;
    int  ip_type        = 0;
    char pipaddr[ATCMD_NVR_NW_TR_PEER_IPADDR_LEN] = {0x00, };

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(sess_info_cnt:%d)\n", sess_info_cnt);

    if (atcmd_sess_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("there already is session\n");

        return 0;
    }

    if (!p_sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    /* Read CID & Type */
    for (cid = 0; cid < sess_info_cnt; cid++)
    {
        memset(&(p_sess_info[cid]), 0x00, sizeof(atcmd_sess_info));

        nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_CID);
        if (nvr_id != -1)
        {
            status = get_atcmd_param_int(nvr_id, &type);
            if ((status == CC_STATUS_SUCCESS) &&
                ((type == ATCMD_SESS_TCP_SERVER) ||
                 (type == ATCMD_SESS_TCP_CLIENT) ||
                 (type == ATCMD_SESS_UDP_SESSION)))
            {
                p_sess_info[cid].type = (atcmd_sess_type) type;
            }
            else
            {
                p_sess_info[cid].type = ATCMD_SESS_NONE;
            }
        }
        else
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of cid(%d)\n", cid);
        }
    }

    /* Set session_info & linked-list */
    for (cid = 0; cid < sess_info_cnt; cid++)
    {
        ret            = 0;
        lport          = 0;
        pport          = 0;
        max_allow_peer = 0;
        memset(pipaddr, 0x00, sizeof(pipaddr));

        if (p_sess_info[cid].type == ATCMD_SESS_TCP_SERVER)
        {
            /* Read local port */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_LOCAL_PORT);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &lport);
                if (status || (lport == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR(
                        "Failed to read local port of tcp server(cid:%d,status(%d),lport(%d)\n",
                        cid,
                        status,
                        lport);
                    ret = -1;
                    goto end_init_tcps;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read local port of tcp server(%d:%d)\n", cid, status);
                ret = -1;
                goto end_init_tcps;
            }

            /* Read max allowed peer */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_MAX_ALLOWED_PEER);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &max_allow_peer);
                if (status || (max_allow_peer == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read max allowed peer of tcp server"
                                                 "(cid:%d,status(%d),max_allow_peer(%d)\n",
                                                 cid,
                                                 status,
                                                 max_allow_peer);
                    ret = -1;
                    goto end_init_tcps;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of max allowed peer(%d)\n", cid);
                ret = -1;
                goto end_init_tcps;
            }

            /* Read IP type */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_IP_TYPE);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &ip_type);
                if (status || ((ip_type != IPADDR_TYPE_V4) && (ip_type != IPADDR_TYPE_V6)))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read ip type of tcp server"
                                                 "(cid:%d,status(%d),ip_type(%d)\n",
                                                 cid,
                                                 status,
                                                 ip_type);
                    ret = -1;
                    goto end_init_tcps;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of ip type(%d)\n", cid);
                ret = -1;
                goto end_init_tcps;
            }

end_init_tcps:

            if (!ret)
            {
                /* Construct session_info */
                p_sess_info[cid].sess.tcps.local_port       = lport;
                p_sess_info[cid].sess.tcps.max_allow_client = max_allow_peer;
                p_sess_info[cid].sess.tcps.ip_type          = ip_type;
                atcmd_network_display_sess_info(cid, &p_sess_info[cid]);
            }
            else
            {
                /* Unset session_info */
                p_sess_info[cid].type = ATCMD_SESS_NONE;
            }
        }
        else if (p_sess_info[cid].type == ATCMD_SESS_TCP_CLIENT)
        {
            /* Read local port */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_LOCAL_PORT);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &lport);
                if (status || (lport == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read local port of tcp cliet(cid:%d,status(%d),lport(%d)\n",
                                                 cid,
                                                 status,
                                                 lport);
                    ret = -1;
                    goto end_init_tcpc;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", cid);
                ret = -1;
                goto end_init_tcpc;
            }

            /* Read peer port */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_PEER_PORT);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &pport);
                if (status || (pport == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read peer port of tcp client"
                                                 "(cid:%d,status(%d),pport(%d)\n",
                                                 cid,
                                                 status,
                                                 pport);
                    ret = -1;
                    goto end_init_tcpc;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", cid);
                ret = -1;
                goto end_init_tcpc;
            }

            /* Read peer IP address */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_PEER_IP_ADDRESS);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_str(nvr_id, pipaddr, ATCMD_NVR_NW_TR_PEER_IPADDR_LEN);
                if (status || (strlen(pipaddr) == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read peer ip address of tcp client"
                                                 "(cid:%d,status(%d),pipaddr(%s)\n",
                                                 cid,
                                                 status,
                                                 pipaddr);
                    ret = -1;
                    goto end_init_tcpc;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of peer ip address(%d)\n", cid);
                ret = -1;
                goto end_init_tcpc;
            }

end_init_tcpc:

            if (!ret)
            {
                /* Construct session_info */
                p_sess_info[cid].sess.tcpc.local_port = lport;
                p_sess_info[cid].sess.tcpc.peer_port  = pport;
                bsp_safe_strcpy(p_sess_info[cid].sess.tcpc.peer_ipaddr, pipaddr, ATCMD_NVR_NW_TR_PEER_IPADDR_LEN);
                atcmd_network_display_sess_info(cid, &p_sess_info[cid]);
            }
            else
            {
                /* Unset session_info */
                p_sess_info[cid].type = ATCMD_SESS_NONE;
            }
        }
        else if (p_sess_info[cid].type == ATCMD_SESS_UDP_SESSION)
        {
            /* Read local port */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_LOCAL_PORT);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &lport);
                if (status || (lport == 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR(
                        "Failed to read local port of udp session(cid:%d,status(%d),lport(%d)\n",
                        cid,
                        status,
                        lport);
                    ret = -1;
                    goto end_init_udps;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of udp session(%d)\n", cid);
                ret = -1;
                goto end_init_udps;
            }

            /* Read peer port */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_PEER_PORT);
            if (nvr_id != -1)
            {
                get_atcmd_param_int(nvr_id, &pport);
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of local port(%d)\n", cid);
                ret = -1;
                goto end_init_tcpc;
            }

            /* Read peer IP address */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_PEER_IP_ADDRESS);
            if (nvr_id != -1)
            {
                get_atcmd_param_str(nvr_id, pipaddr, ATCMD_NVR_NW_TR_PEER_IPADDR_LEN);
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of peer ip address(%d)\n", cid);
                ret = -1;
                goto end_init_tcpc;
            }

            /* Read IP type */
            nvr_id = atcmd_network_get_nvr_id(cid, ATCMD_SESS_NVR_IP_TYPE);
            if (nvr_id != -1)
            {
                status = get_atcmd_param_int(nvr_id, &ip_type);
                if (status || ((ip_type != IPADDR_TYPE_V4) && (ip_type != IPADDR_TYPE_V6)))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read ip type of udp session"
                                                 "(cid:%d,status(%d),ip_type(%d)\n",
                                                 cid,
                                                 status,
                                                 ip_type);
                    ret = -1;
                    goto end_init_udps;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get nvram id of ip type(%d)\n", cid);
                ret = -1;
                goto end_init_udps;
            }

end_init_udps:

            if (!ret)
            {
                /* Construct session_info */
                p_sess_info[cid].sess.udps.local_port = lport;
                p_sess_info[cid].sess.udps.peer_port  = pport;
                bsp_safe_strcpy(p_sess_info[cid].sess.udps.peer_ipaddr, pipaddr, ATCMD_NVR_NW_TR_PEER_IPADDR_LEN);
                p_sess_info[cid].sess.udps.ip_type = ip_type;
                atcmd_network_display_sess_info(cid, &p_sess_info[cid]);
            }
            else
            {
                /* Unset session_info */
                p_sess_info[cid].type = ATCMD_SESS_NONE;
            }
        }
    }

    return 0;
}

#endif                                 /* CFG_PMGR */

static int atcmd_network_get_sess_info_cnt ()
{
    return atcmd_transport_get_max_session() + 2;
}

static void atcmd_network_init_sess_info (atcmd_sess_info * sess_info)
{
    if (sess_info)
    {
        memset(sess_info, 0x00, sizeof(atcmd_sess_info));
        sess_info->type = ATCMD_SESS_NONE;
    }
}

static unsigned int rm_atcmd_w_core_socket_is_using_heap (void)
{
    return g_rm_atcmd_w_core_socket_using_heap;
}

static void rm_atcmd_w_core_socket_set_heap_alloc (unsigned int val)
{
#if CFG_PMGR
    if (val == pdFALSE)
    {
        g_rm_atcmd_w_core_socket_using_heap = val;
    }
    else
#endif
    {
        g_rm_atcmd_w_core_socket_using_heap = pdTRUE;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("g_rm_atcmd_w_core_socket_using_heap: %d\n", g_rm_atcmd_w_core_socket_using_heap);
}

static atcmd_sess_info * atcmd_network_get_sess_info (const int cid)
{
#if CFG_PMGR
    unsigned int ret = 0;
#endif                                 /* CFG_PMGR */
    unsigned int      idx           = 0;
    atcmd_sess_info * ptr           = NULL;
    unsigned int      sess_info_cnt = 0;
    size_t            len           = 0;

    sess_info_cnt = atcmd_network_get_sess_info_cnt();
    len           = (sizeof(atcmd_sess_info) * sess_info_cnt);

    if (atcmd_sess_info_header)
    {
        return &(atcmd_sess_info_header[cid]);
    }

    RM_ATCMD_W_CORE_SOCKET_ERROR("Load sess_info(atcmd_sess_info:%d, len:%d, cnt:%d)\n",
                                 sizeof(atcmd_sess_info),
                                 len,
                                 sess_info_cnt);

    if (!rm_atcmd_w_core_socket_is_using_heap())
    {
#if CFG_PMGR
        ret = RM_PMGR_W_user_rtm_get(ATCMD_RTM_NW_TR_NAME, (unsigned char **) &ptr);

        if (ret == 0)
        {
            ret = RM_PMGR_W_user_rtm_pool_alloc(ATCMD_RTM_NW_TR_NAME, (void **) &ptr, len, 0);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate memory to save user session(%d)\n", len);

                return NULL;
            }

            for (idx = 0; idx < sess_info_cnt; idx++)
            {
                atcmd_network_init_sess_info(&ptr[idx]);
            }
        }
        else if (ret != len)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid size(%d,%d)\n", ret, len);

            return NULL;
        }
#endif                                 /* CFG_PMGR */
    }
    else
    {
        ptr = (atcmd_sess_info *) pvPortMalloc(len);

        if (!ptr)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate memory to save user session(%d)\n", len);

            return NULL;
        }

        for (idx = 0; idx < sess_info_cnt; idx++)
        {
            atcmd_network_init_sess_info(&ptr[idx]);
        }
    }

    atcmd_sess_info_header = ptr;

    return &(atcmd_sess_info_header[cid]);
}

static int atcmd_network_clear_sess_info (int cid)
{
    atcmd_sess_info * sess_info = atcmd_network_get_sess_info(cid);

    if (sess_info)
    {
        atcmd_network_init_sess_info(sess_info);

        return 0;
    }

    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to clear sess_info(%d)\n", cid);

    return -1;
}

static int atcmd_network_delete_sess_info (void)
{
    if (atcmd_sess_info_header)
    {
        if (!rm_atcmd_w_core_socket_is_using_heap())
        {
#if CFG_PMGR
            RM_PMGR_W_user_rtm_free(ATCMD_RTM_NW_TR_NAME);
#endif                                 /* CFG_PMGR */
        }
        else
        {
            vPortFree(atcmd_sess_info_header);
        }

        atcmd_sess_info_header = NULL;
    }

    return 0;
}

#if CFG_PMGR
static int atcmd_network_recover_session (atcmd_w_ctrl_t * const p_at_ctrl,
                                          atcmd_sess_info      * p_sess_info,
                                          const int              sess_info_cnt)
{
    int                  ret           = 0;
    int                  cid           = 0;
    int                * p_failed_cids = NULL;
    atcmd_sess_context * p_prev_ctx    = NULL;
    atcmd_sess_context * p_new_ctx     = NULL;
    int                  timeout       = RM_ATCMD_W_CORE_SOCKET_MAX_WAIT_TIME;
    int                  ip_type       = IPADDR_TYPE_V4;

    if (!p_sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    if (atcmd_sess_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("there is already session\n");

        return 0;
    }

    p_failed_cids = pvPortMalloc(sizeof(int) * sess_info_cnt);
    if (!p_failed_cids)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate memory for notification(%d)\n", (sizeof(int) * sess_info_cnt));
    }
    else
    {
        memset(p_failed_cids, 0x00, (sizeof(int) * sess_info_cnt));
    }

    /* Create session */
    for (cid = 0; cid < sess_info_cnt; cid++)
    {
        atcmd_network_display_sess_info(cid, &p_sess_info[cid]);

        if ((p_sess_info[cid].type == ATCMD_SESS_TCP_SERVER) ||
            (p_sess_info[cid].type == ATCMD_SESS_TCP_CLIENT) ||
            (p_sess_info[cid].type == ATCMD_SESS_UDP_SESSION))
        {
            /* Create context */
            p_new_ctx = atcmd_transport_alloc_context(p_sess_info[cid].type);
            if (!p_new_ctx)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate atcmd_sess_context\n");
                continue;
            }

            /* CID & Type */
            p_new_ctx->cid  = cid;
            p_new_ctx->type = p_sess_info[cid].type;

            /* Check network connection */
            if (p_sess_info[cid].type == ATCMD_SESS_TCP_SERVER)
            {
                ip_type = p_sess_info[cid].sess.tcps.ip_type;
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_TCP_CLIENT)
            {
                ip_type = rm_wifi_select_ipaddr_type_from_str(p_sess_info[cid].sess.tcpc.peer_ipaddr, NULL);
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_UDP_SESSION)
            {
                ip_type = p_sess_info[cid].sess.udps.ip_type;
            }

            if (!atcmd_network_check_network_ready("RM_ATCMD_W_CORE_SOCKET", WLAN0_IFACE, ip_type, timeout))
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to wait network init(%d) - cid:%d\n", timeout, cid);
                continue;
            }

            /* Create session */
            if (p_sess_info[cid].type == ATCMD_SESS_TCP_SERVER)
            {
                ret = atcmd_network_connect_tcps(p_at_ctrl,
                                                 p_new_ctx,
                                                 p_sess_info[cid].sess.tcps.local_port,
                                                 p_sess_info[cid].sess.tcps.max_allow_client,
                                                 p_sess_info[cid].sess.tcps.ip_type);
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_TCP_CLIENT)
            {
                ret = atcmd_network_connect_tcpc(p_at_ctrl,
                                                 p_new_ctx,
                                                 p_sess_info[cid].sess.tcpc.peer_ipaddr,
                                                 p_sess_info[cid].sess.tcpc.peer_port,
                                                 p_sess_info[cid].sess.tcpc.local_port);
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_UDP_SESSION)
            {
                ret = atcmd_network_connect_udps(p_at_ctrl,
                                                 p_new_ctx,
                                                 p_sess_info[cid].sess.udps.peer_ipaddr,
                                                 p_sess_info[cid].sess.udps.peer_port,
                                                 p_sess_info[cid].sess.udps.local_port,
                                                 p_sess_info[cid].sess.udps.ip_type);
            }

            /* Construct context to linked-list */
            if (ret == 0)
            {
                if (p_prev_ctx)
                {
                    p_prev_ctx->next = p_new_ctx;
                    p_prev_ctx       = p_new_ctx;
                }
                else
                {
                    atcmd_sess_ctx_header = p_prev_ctx = p_new_ctx;
                }
            }
            else
            {
                if (p_failed_cids)
                {
                    p_failed_cids[cid] = 1;
                }

                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to recover session(cid:%d,ret:%d)\n", cid, ret);
                atcmd_transport_free_context(&p_new_ctx);
            }
        }
    }

    /* Notification */
    if (p_failed_cids)
    {
        for (cid = 0; cid < sess_info_cnt; cid++)
        {
            if (p_sess_info[cid].type == ATCMD_SESS_TCP_SERVER)
            {
                if ((p_failed_cids[cid] == 0) && !RM_PMGR_W_dpm_is_wakeup())
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("[AT-CMD] %d TCP Server OPEN (Port: %d)\r\n",
                                                 cid,
                                                 p_sess_info[cid].sess.tcps.local_port);
                }
                else if (p_failed_cids[cid] == 1)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start %d TCP Server with saved information ...\n", cid);

                    // PRINTF_ATCMD("\r\n" ATCMD_TCPS_DISCONN_RX_PREFIX ":%d,0,0\r\n", cid);
                }
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_TCP_CLIENT)
            {
                if ((p_failed_cids[cid] == 0) && !RM_PMGR_W_dpm_is_wakeup())
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("[AT-CMD] %d, TCP Client CONNECTED (IP: %s, Port: %d)\n",
                                                 cid,
                                                 p_sess_info[cid].sess.tcpc.peer_ipaddr,
                                                 p_sess_info[cid].sess.tcpc.peer_port);
                }
                else if (p_failed_cids[cid] == 1)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start %d TCP Client with saved information ...\n", cid);

                    /*
                     * PRINTF_ATCMD("\r\n" ATCMD_TCPC_DISCONN_RX_PREFIX ":%d,%s,%d\r\n", cid,
                     *           p_sess_info[cid].sess.tcpc.peer_ipaddr,
                     *           p_sess_info[cid].sess.tcpc.peer_port);
                     */
                }
            }
            else if (p_sess_info[cid].type == ATCMD_SESS_UDP_SESSION)
            {
                if ((p_failed_cids[cid] == 0) && !RM_PMGR_W_dpm_is_wakeup())
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR(
                        "[AT-CMD] %d UDP session OPEN (Port: %d, Remote IP: %s, Remote Port: %ld)\n",
                        cid,
                        p_sess_info[cid].sess.udps.local_port,
                        p_sess_info[cid].sess.udps.peer_ipaddr,
                        p_sess_info[cid].sess.udps.peer_port);
                }
                else if (p_failed_cids[cid] == 1)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start %d UDP Client with saved information ...\n", cid);

                    /*
                     * PRINTF_ATCMD("\r\n" ATCMD_UDP_SESS_FAIL_PREFIX ":%d,%d\r\n",
                     *           cid, p_sess_info[cid].sess.udps.local_port);
                     */
                }
            }
        }

        vPortFree(p_failed_cids);
        p_failed_cids = NULL;
    }

 #if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_ERROR("After CID:\n");

    for (p_new_ctx = atcmd_sess_ctx_header; p_new_ctx != NULL; p_new_ctx = p_new_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("\t%d(%d), ", p_new_ctx->cid, p_new_ctx->type);
    }
 #endif                                // (DEBUG_ATCMD)

    return 0;
}

#endif                                 /* CFG_PMGR */

static int atcmd_network_check_network_ready (const char * module, int iface, int ip_type, unsigned int timeout)
{
    FSP_PARAMETER_NOT_USED(module);

    int ret = pdTRUE;
#if CFG_PMGR
    const unsigned int wait_option = portCONVERT_MS_2_TICKS(100);
    unsigned int       start_time, cur_time, elapsed_time, time_remaining;
    time_remaining = portCONVERT_MS_2_TICKS(timeout);

    if (RM_PMGR_W_dpm_is_enabled() && !RM_PMGR_W_dpm_is_wakeup())
    {
        RM_PMGR_W_dpm_job_name_set(ATCMD_DPM_NETWORK_TMP, 0);
        RM_PMGR_W_dpm_sleep_ready_clear(ATCMD_DPM_NETWORK_TMP);

        while (time_remaining > 0)
        {
            start_time = xTaskGetTickCount();

            if (!chk_network_ready(iface))
            {
                vTaskDelay(wait_option);
            }

 #if defined(__SUPPORT_IPV6__)
            else if ((ip_type == IPADDR_TYPE_V6) && check_net_ipv6_status(iface))
            {
                vTaskDelay(wait_option);
            }
 #endif
            else
            {
                break;
            }

            cur_time = xTaskGetTickCount();

            if (cur_time >= start_time)
            {
                elapsed_time = cur_time - start_time;
            }
            else
            {
                elapsed_time = (((unsigned int) 0xFFFFFFFF) - start_time) + cur_time;
            }

            if (time_remaining > elapsed_time)
            {
                time_remaining -= elapsed_time;
            }
            else
            {
                time_remaining = 0;
            }
        }

        if (!chk_network_ready(iface))
        {
            ret = pdFALSE;
        }

 #if defined(__SUPPORT_IPV6__)
        else if ((ip_type == IPADDR_TYPE_V6) && check_net_ipv6_status(iface))
        {
            ret = pdFALSE;
        }
 #endif

        RM_PMGR_W_dpm_job_name_clear(ATCMD_DPM_NETWORK_TMP);
    }
    else if (RM_PMGR_W_dpm_is_enabled() && !RM_PMGR_W_dpm_is_wakeup())
    {
        /* Pass thru ... */
    }
    else
#endif                                 /* CFG_PMGR */
    {
        if (!chk_network_ready(iface))
        {
            ret = pdFALSE;
        }

#if defined(__SUPPORT_IPV6__)
        else if ((ip_type == IPADDR_TYPE_V6) && check_net_ipv6_status(iface))
        {
            ret = pdFALSE;
        }
#endif
    }

    if (!ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("[%s] Timeout to connect an Wi-Fi AP\n", module);
    }

    return ret;
}

static int atcmd_network_connect_tcps (atcmd_w_ctrl_t * const p_at_ctrl,
                                       atcmd_sess_context   * p_ctx,
                                       int                    port,
                                       int                    max_peer_cnt,
                                       int                    ip_type)
{
    int ret = 0;
    atcmd_tcps_context * p_tcps_ctx  = NULL;
    atcmd_tcps_config  * p_tcps_conf = NULL;
    atcmd_sess_info    * p_sess_info = NULL;

    if (!p_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    p_tcps_ctx  = p_ctx->ctx.tcps;
    p_tcps_conf = &p_ctx->conf.tcps;

    ret = atcmd_tcps_init_context(p_tcps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tcps context(%d)\n", ret);
        goto err;
    }

    p_sess_info = atcmd_network_get_sess_info(p_ctx->cid);
    if (!p_sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get session info(%d)\n", p_ctx->cid);
        goto err;
    }

    ret = atcmd_tcps_set_at_ctrl(p_tcps_ctx, p_at_ctrl);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set at_ctrl(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcps_init_config(p_ctx->cid, p_tcps_conf, &(p_sess_info->sess.tcps));
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tcps config(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcps_set_local_addr(p_tcps_conf, ip_type, NULL, port);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local port(%d)\n", ret);
        goto err;
    }

    if (max_peer_cnt)
    {
        ret = atcmd_tcps_set_max_allowed_client(p_tcps_conf, max_peer_cnt);
        if (ret)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set max peer cnt(%d)\n", ret);
            goto err;
        }
    }

    ret = atcmd_tcps_set_config(p_tcps_ctx, p_tcps_conf);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set config(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcps_start(p_tcps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start tcp server task(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcps_wait_for_ready(p_tcps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to active tcps(%d,%d)\n", ret, p_tcps_ctx->state);
        goto err;
    }

    p_sess_info->type = p_ctx->type;

    return 0;

err:
    atcmd_network_disconnect_tcps(p_ctx->cid);

    return -1;
}

static int atcmd_network_disconnect_tcps (const int cid)
{
    atcmd_sess_context * ctx       = NULL;
    atcmd_tcps_context * tcps_ctx  = NULL;
    atcmd_tcps_config  * tcps_conf = NULL;
    int ret = 0;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(cid:%d)\n", cid);

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return 0;
    }

    tcps_ctx  = ctx->ctx.tcps;
    tcps_conf = &ctx->conf.tcps;

    ret = atcmd_tcps_stop(tcps_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to stop tcp server task(%d)\n", ret);
    }

    ret = atcmd_tcps_deinit_context(tcps_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit tcps context(%d)\n", ret);
    }

    ret = atcmd_tcps_deinit_config(tcps_conf);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit tcps config(%d)\n", ret);
    }

    atcmd_network_clear_sess_info(cid);

    return 0;
}

static int atcmd_network_disconnect_tcps_cli (const int cid, const char * ip_addr, const int port)
{
    atcmd_sess_context * ctx      = NULL;
    atcmd_tcps_context * tcps_ctx = NULL;
    int ret = 0;

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return -1;
    }

    tcps_ctx = ctx->ctx.tcps;

    ret = atcmd_tcps_stop_cli(tcps_ctx, ip_addr, port);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect TCP client(%d,%s:%d,ret:%d)\n", cid, ip_addr, port, ret);
    }

    return ret;
}

static int atcmd_network_display_tcps (atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix)
{
    atcmd_sess_context     * ctx              = NULL;
    atcmd_tcps_context     * tcps_ctx         = NULL;
    atcmd_tcps_config      * tcps_conf        = NULL;
    atcmd_tcps_cli_context * tcps_cli_ctx     = NULL;
    const size_t             max_sentence_len = 64;
    char * out     = NULL;
    size_t outlen  = 0;
    int    out_idx = 0;
    int    ret     = 0;

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return -1;
    }

    tcps_ctx  = ctx->ctx.tcps;
    tcps_conf = &ctx->conf.tcps;

    RM_ATCMD_W_CORE_SOCKET_ERROR("cid(%d), state(%d)\n", cid, tcps_ctx->state);

    if (prefix)
    {
        outlen += strlen(prefix);
    }

    outlen += (max_sentence_len * (tcps_ctx->cli_cnt > 0 ? tcps_ctx->cli_cnt : 1));

    out = pvPortMalloc(outlen);

    if (!out)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate memory to display tcp server info(%d)\n", outlen);

        return -1;
    }

    memset(out, 0x00, outlen);

    if (tcps_ctx->state == ATCMD_TCPS_STATE_ACCEPT)
    {
        if (prefix)
        {
            bsp_safe_strcpy(out, prefix, outlen);
            out_idx = strlen(out);
        }

        if (tcps_conf->ip_type == IPADDR_TYPE_V4)
        {
#if defined(__SUPPORT_IPV4__)
            if (tcps_ctx->cli_cnt > 0)
            {
                for (tcps_cli_ctx = tcps_ctx->cli_ctx; tcps_cli_ctx != NULL; tcps_cli_ctx = tcps_cli_ctx->next)
                {
                    if (tcps_cli_ctx->state == ATCMD_TCPS_CLI_STATE_CONNECTED)
                    {
                        sprintf((out + out_idx),
                                "%d,%s,%ld.%ld.%ld.%ld,%d,%d\r\n",
                                cid,
                                "TCP",
                                (ntohl(tcps_cli_ctx->addr.sin_addr.s_addr) >> 24) & 0xFF,
                                (ntohl(tcps_cli_ctx->addr.sin_addr.s_addr) >> 16) & 0xFF,
                                (ntohl(tcps_cli_ctx->addr.sin_addr.s_addr) >> 8) & 0xFF,
                                (ntohl(tcps_cli_ctx->addr.sin_addr.s_addr)) & 0xFF,
                                (ntohs(tcps_cli_ctx->addr.sin_port)),
                                (ntohs(tcps_conf->local_addr.sin_port)));
                        out_idx = strlen(out);
                    }
                }
            }
            else
            {
                sprintf((out + out_idx), "%d,%s,%s,%d,%d\r\n", cid, "TCP", "0.0.0.0", 0,
                        ntohs(tcps_conf->local_addr.sin_port));
                out_idx = strlen(out);
            }
#endif                                 // __SUPPORT_IPV4__
        }
        else if (tcps_conf->ip_type == IPADDR_TYPE_V6)
        {
#if defined(__SUPPORT_IPV6__)
            if (tcps_ctx->cli_cnt > 0)
            {
                for (tcps_cli_ctx = tcps_ctx->cli_ctx; tcps_cli_ctx != NULL; tcps_cli_ctx = tcps_cli_ctx->next)
                {
                    if (tcps_cli_ctx->state == ATCMD_TCPS_CLI_STATE_CONNECTED)
                    {
                        sprintf((out + out_idx), "%d,%s,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%d,%d\r\n", cid, "TCP",
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                                ((PP_HTONL(tcps_cli_ctx->addr6.sin6_addr.un.u32_addr[3])) & 0xffff),
                                (ntohs(tcps_cli_ctx->addr6.sin6_port)), (ntohs(tcps_conf->local_addr6.sin6_port)));
                        out_idx = strlen(out);
                    }
                }
            }
            else
            {
                sprintf((out + out_idx), "%d,%s,%s,%d,%d\r\n", cid, "TCP", "0:0:0:0:0:0:0:0", 0,
                        ntohs(tcps_conf->local_addr6.sin6_port));
                out_idx = strlen(out);
            }
#endif                                 // __SUPPORT_IPV6__
        }
    }
    else
    {
        if (tcps_conf->ip_type == IPADDR_TYPE_V4)
        {
#if defined(__SUPPORT_IPV4__)
            sprintf((out + out_idx), "%d,%s,%s,%d,%d\r\n", cid, "TCP", "0.0.0.0", 0, 0);
            out_idx = strlen(out);
#endif                                 // __SUPPORT_IPV4__
        }
        else if (tcps_conf->ip_type == IPADDR_TYPE_V6)
        {
#if defined(__SUPPORT_IPV6__)
            sprintf((out + out_idx), "%d,%s,%s,%d,%d\r\n", cid, "TCP", "0:0:0:0:0:0:0:0", 0, 0);
            out_idx = strlen(out);
#endif                                 // __SUPPORT_IPV6__
        }
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("%s(%d)\n", out, strlen(out));

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) out, strlen(out));

    if (out)
    {
        vPortFree(out);
        out = NULL;
    }

    return ret;
}

static int atcmd_network_connect_tcpc (atcmd_w_ctrl_t * const p_at_ctrl,
                                       atcmd_sess_context   * p_ctx,
                                       char                 * p_svr_ip,
                                       int                    svr_port,
                                       int                    port)
{
    int ret                          = 0;
    int wait_option                  = (1000 * 5); // 5sec.
    atcmd_tcpc_context * p_tcpc_ctx  = NULL;
    atcmd_tcpc_config  * p_tcpc_conf = NULL;
    atcmd_sess_info    * p_sess_info = NULL;

    int ip_type = IPADDR_TYPE_V4;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    if (!p_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    p_tcpc_ctx  = p_ctx->ctx.tcpc;
    p_tcpc_conf = &p_ctx->conf.tcpc;

    /* Get IP Type */
    ip_type = rm_wifi_select_ipaddr_type_from_str(p_svr_ip, NULL);

    p_tcpc_conf->ip_type = ip_type;

    /* Check network connection */
    if (!atcmd_network_check_network_ready("TCP Client", WLAN0_IFACE, ip_type, wait_option))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to wait network init(%d)\n", wait_option);

        return -1;
    }

    ret = atcmd_tcpc_init_context(p_tcpc_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tcpc context(%d)\n", ret);
        goto err;
    }

    p_sess_info = atcmd_network_get_sess_info(p_ctx->cid);
    if (!p_sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get session info(%d)\n", p_ctx->cid);
        goto err;
    }

    ret = atcmd_tcpc_set_at_ctrl(p_tcpc_ctx, p_at_ctrl);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set at_ctrl(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_init_config(p_ctx->cid, p_tcpc_conf, &(p_sess_info->sess.tcpc));
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tcpc config(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_set_local_addr(p_tcpc_conf, NULL, port);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local port(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_set_svr_addr(p_tcpc_conf, p_svr_ip, svr_port);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set tcp server address(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_set_config(p_tcpc_ctx, p_tcpc_conf);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set config(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_start(p_tcpc_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start tcp client task(%d)\n", ret);
        goto err;
    }

    ret = atcmd_tcpc_wait_for_ready(p_tcpc_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to active tcp client(%d,%d)\n", ret, p_tcpc_ctx->state);
        goto err;
    }

    p_sess_info->type = p_ctx->type;

    return 0;

err:

    atcmd_network_disconnect_tcpc(p_ctx->cid);

    return ret;
}

static int atcmd_network_disconnect_tcpc (const int cid)
{
    atcmd_sess_context * ctx       = NULL;
    atcmd_tcpc_context * tcpc_ctx  = NULL;
    atcmd_tcpc_config  * tcpc_conf = NULL;
    int ret = 0;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(cid:%d)\n", cid);

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return 0;
    }

    tcpc_ctx  = ctx->ctx.tcpc;
    tcpc_conf = &ctx->conf.tcpc;
    ret       = atcmd_tcpc_stop(tcpc_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to stop tcp client task(%d)\n", ret);
    }

    ret = atcmd_tcpc_deinit_context(tcpc_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit tcpc context(%d)\n", ret);
    }

    ret = atcmd_tcpc_deinit_config(tcpc_conf);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit tcpc config(%d)\n", ret);
    }

    atcmd_network_clear_sess_info(cid);

    return ret;
}

static int atcmd_network_display_tcpc (atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix)
{
    atcmd_sess_context * ctx         = NULL;
    atcmd_tcpc_context * tcpc_ctx    = NULL;
    atcmd_tcpc_config  * tcpc_conf   = NULL;
    char                 result[256] = {0x00, };

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return -1;
    }

    tcpc_ctx  = ctx->ctx.tcpc;
    tcpc_conf = &ctx->conf.tcpc;

    if (tcpc_conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined(__SUPPORT_IPV4__)
        struct sockaddr_in local_addr = {0x00, };
        struct sockaddr_in svr_addr   = {0x00, };

        if (tcpc_ctx->state == ATCMD_TCPC_STATE_CONNECTED)
        {
            memcpy(&local_addr, &tcpc_conf->local_addr, sizeof(struct sockaddr_in));
            memcpy(&svr_addr, &tcpc_conf->svr_addr, sizeof(struct sockaddr_in));
        }

        if (prefix)
        {
            sprintf(result, "%s%d,%s,%ld.%ld.%ld.%ld,%d,%d\r\n", prefix, cid, "TCP",
                    (ntohl(svr_addr.sin_addr.s_addr) >> 24) & 0xFF, (ntohl(svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(svr_addr.sin_addr.s_addr) >> 8) & 0xFF, (ntohl(svr_addr.sin_addr.s_addr)) & 0xFF,
                    (ntohs(svr_addr.sin_port)), (ntohs(local_addr.sin_port)));
        }
        else
        {
            sprintf(result,
                    "%d,%s,%ld.%ld.%ld.%ld,%d,%d\r\n",
                    cid,
                    "TCP",
                    (ntohl(svr_addr.sin_addr.s_addr) >> 24) & 0xFF,
                    (ntohl(svr_addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(svr_addr.sin_addr.s_addr) >> 8) & 0xFF,
                    (ntohl(svr_addr.sin_addr.s_addr)) & 0xFF,
                    (ntohs(svr_addr.sin_port)),
                    (ntohs(local_addr.sin_port)));
        }
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
#endif                                 // __SUPPORT_IPV4__
    }
    else if (tcpc_conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined(__SUPPORT_IPV6__)
        struct sockaddr_in6 local_addr6 = {0x00, };
        struct sockaddr_in6 svr_addr6   = {0x00, };

        if (tcpc_ctx->state == ATCMD_TCPC_STATE_CONNECTED)
        {
            memcpy(&local_addr6, &tcpc_conf->local_addr6, sizeof(struct sockaddr_in6));
            memcpy(&svr_addr6, &tcpc_conf->svr_addr6, sizeof(struct sockaddr_in6));
        }

        if (prefix)
        {
            sprintf(result, "%s%d,%s,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%d,%d\r\n", prefix, cid, "TCP",
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff), (ntohs(svr_addr6.sin6_port)),
                    (ntohs(local_addr6.sin6_port)));
        }
        else
        {
            sprintf(result, "%d,%s,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%d,%d\r\n", cid, "TCP",
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(svr_addr6.sin6_addr.un.u32_addr[3])) & 0xffff), (ntohs(svr_addr6.sin6_port)),
                    (ntohs(local_addr6.sin6_port)));
        }
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
#endif                                 // __SUPPORT_IPV6__
    }

    return 0;
}

static int atcmd_network_connect_udps (atcmd_w_ctrl_t * const p_at_ctrl,
                                       atcmd_sess_context   * p_ctx,
                                       char                 * p_peer_ip,
                                       int                    peer_port,
                                       int                    port,
                                       int                    ip_type)
{
    int ret = 0;
    atcmd_udps_context * p_udps_ctx  = NULL;
    atcmd_udps_config  * p_udps_conf = NULL;
    atcmd_sess_info    * p_sess_info = NULL;

    if (!p_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter\n");

        return -1;
    }

    p_udps_ctx  = p_ctx->ctx.udps;
    p_udps_conf = &p_ctx->conf.udps;

    ret = atcmd_udps_init_context(p_udps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init udps context(%d)\n", ret);
        goto err;
    }

    p_sess_info = atcmd_network_get_sess_info(p_ctx->cid);
    if (!p_sess_info)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get session info(%d)\n", p_ctx->cid);
        goto err;
    }

    ret = atcmd_udps_set_at_ctrl(p_udps_ctx, p_at_ctrl);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set at_ctrl(%d)\n", ret);
        goto err;
    }

    ret = atcmd_udps_init_config(p_ctx->cid, p_udps_conf, &(p_sess_info->sess.udps));
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init udps config(%d)\n", ret);
    }

    ret = atcmd_udps_set_local_addr(p_udps_conf, ip_type, NULL, port);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set local address(%d)\n", ret);
        goto err;
    }

    if (p_peer_ip && peer_port)
    {
        ret = atcmd_udps_set_peer_addr(p_udps_conf, ip_type, p_peer_ip, peer_port);
        if (ret)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set peer address(%d)\n", ret);
            goto err;
        }
    }

    ret = atcmd_udps_set_config(p_udps_ctx, p_udps_conf);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set config(%d)\n", ret);
        goto err;
    }

    ret = atcmd_udps_start(p_udps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to start udps task(%d)\n", ret);
        goto err;
    }

    ret = atcmd_udps_wait_for_ready(p_udps_ctx);
    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to active udps(%d,%d)\n", ret, p_udps_ctx->state);
        goto err;
    }

    p_sess_info->type = p_ctx->type;

    return 0;

err:

    atcmd_network_disconnect_udps(p_ctx->cid);

    return ret;
}

static int atcmd_network_disconnect_udps (const int cid)
{
    atcmd_sess_context * ctx       = NULL;
    atcmd_udps_context * udps_ctx  = NULL;
    atcmd_udps_config  * udps_conf = NULL;
    int ret = 0;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(cid:%d)\n", cid);

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return 0;
    }

    udps_ctx  = ctx->ctx.udps;
    udps_conf = &ctx->conf.udps;

    ret = atcmd_udps_stop(udps_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to stop tcp client task(%d)\n", ret);
    }

    ret = atcmd_udps_deinit_context(udps_ctx);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit udps context(%d)\n", ret);
    }

    ret = atcmd_udps_deinit_config(udps_conf);

    if (ret)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit udps config(%d)\r\n", ret);
    }

    atcmd_network_clear_sess_info(cid);

    return ret;
}

static int atcmd_network_display_udps (atcmd_w_ctrl_t * const p_at_ctrl, const int cid, const char * prefix)
{
    atcmd_sess_context * ctx         = NULL;
    atcmd_udps_context * udps_ctx    = NULL;
    atcmd_udps_config  * udps_conf   = NULL;
    char                 result[256] = {0x00, };

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

        return -1;
    }

    udps_ctx  = ctx->ctx.udps;
    udps_conf = &ctx->conf.udps;

    if (udps_conf->ip_type == IPADDR_TYPE_V4)
    {
#if defined(__SUPPORT_IPV4__)
        struct sockaddr_in local_addr = {0x00, };
        socklen_t          addr_len   = sizeof(struct sockaddr_in);
        struct sockaddr_in peer_addr  = {0x00, };

        if (udps_ctx->state == ATCMD_UDPS_STATE_ACTIVE)
        {
            getsockname(udps_ctx->socket, (struct sockaddr *) &local_addr, &addr_len);
            memcpy(&peer_addr, &udps_conf->peer_addr, sizeof(struct sockaddr_in));
        }

        if (prefix)
        {
            sprintf(result, "%s%d,%s,%ld.%ld.%ld.%ld,%d,%d\r\n", prefix, cid, "UDP",
                    (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xFF, (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 8) & 0xFF, (ntohl(peer_addr.sin_addr.s_addr)) & 0xFF,
                    (ntohs(peer_addr.sin_port)), (ntohs(local_addr.sin_port)));
        }
        else
        {
            sprintf(result,
                    "%d,%s,%ld.%ld.%ld.%ld,%d,%d\r\n",
                    cid,
                    "UDP",
                    (ntohl(peer_addr.sin_addr.s_addr) >> 24) & 0xFF,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 16) & 0xFF,
                    (ntohl(peer_addr.sin_addr.s_addr) >> 8) & 0xFF,
                    (ntohl(peer_addr.sin_addr.s_addr)) & 0xFF,
                    (ntohs(peer_addr.sin_port)),
                    (ntohs(local_addr.sin_port)));
        }
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
#endif                                 // __SUPPORT_IPV4__
    }
    else if (udps_conf->ip_type == IPADDR_TYPE_V6)
    {
#if defined(__SUPPORT_IPV6__)
        struct sockaddr_in6 local_addr6 = {0x00, };
        socklen_t           addr_len    = sizeof(struct sockaddr_in6);
        struct sockaddr_in6 peer_addr6  = {0x00, };

        if (udps_ctx->state == ATCMD_UDPS_STATE_ACTIVE)
        {
            getsockname(udps_ctx->socket, (struct sockaddr *) &local_addr6, &addr_len);
            memcpy(&peer_addr6, &udps_conf->peer_addr6, sizeof(struct sockaddr_in6));
        }

        if (prefix)
        {
            sprintf(result, "%s%d,%s,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%d,%d\r\n", prefix, cid, "UDP",
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff), (ntohs(peer_addr6.sin6_port)),
                    (ntohs(local_addr6.sin6_port)));
        }
        else
        {
            sprintf(result, "%d,%s,%lx:%lx:%lx:%lx:%lx:%lx:%lx:%lx,%d,%d\r\n", cid, "UDP",
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[0])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[1])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[2])) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3]) >> 16) & 0xffff),
                    ((PP_HTONL(peer_addr6.sin6_addr.un.u32_addr[3])) & 0xffff), (ntohs(peer_addr6.sin6_port)),
                    (ntohs(local_addr6.sin6_port)));
        }
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
#endif                                 // __SUPPORT_IPV6__
    }

    return 0;
}

static int atcmd_network_display (atcmd_w_ctrl_t * const p_at_ctrl, const int cid)
{
    int ret = -1;
    atcmd_sess_context * ctx           = NULL;
    const char         * prefix_all    = "\r\n+TRPALL:";
    const char         * prefix_module = "\r\n+TRPRT:";
    const char         * prefix;

    if (atcmd_sess_ctx_header == NULL)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("Empty session\n");

        return ret;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(cid:%d)\n", cid);

    if (cid == 0xFF)
    {
        prefix = prefix_all;

        for (ctx = atcmd_sess_ctx_header; ctx != NULL; ctx = ctx->next)
        {
            switch (ctx->type)
            {
                case ATCMD_SESS_TCP_SERVER:
                {
                    ret = atcmd_network_display_tcps(p_at_ctrl, ctx->cid, prefix);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display tcp server(%d:%d)\n", ctx->cid, ret);
                    }

                    break;
                }

                case ATCMD_SESS_TCP_CLIENT:
                {
                    ret = atcmd_network_display_tcpc(p_at_ctrl, ctx->cid, prefix);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display tcp client(%d:%d)\n", ctx->cid, ret);
                    }

                    break;
                }

                case ATCMD_SESS_UDP_SESSION:
                {
                    ret = atcmd_network_display_udps(p_at_ctrl, ctx->cid, prefix);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display udp session(%d:%d)\n", ctx->cid, ret);
                    }

                    break;
                }

                case ATCMD_SESS_NONE:
                default:
                {
                    /* Unknown type */
                    break;
                }
            }

            /* Printed prefix */
            if (ret == 0)
            {
                prefix = NULL;
            }
        }
    }
    else
    {
        ctx = atcmd_transport_find_context(cid);

        if (!ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        switch (ctx->type)
        {
            case ATCMD_SESS_TCP_SERVER:
            {
                ret = atcmd_network_display_tcps(p_at_ctrl, cid, prefix_module);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display tcp server(%d)\n", ret);

                    return -1;
                }

                break;
            }

            case ATCMD_SESS_TCP_CLIENT:
            {
                ret = atcmd_network_display_tcpc(p_at_ctrl, cid, prefix_module);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display tcp client(%d)\n", ret);

                    return -1;
                }

                break;
            }

            case ATCMD_SESS_UDP_SESSION:
            {
                ret = atcmd_network_display_udps(p_at_ctrl, cid, prefix_module);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to display udp session(%d)\n", ret);

                    return -1;
                }

                break;
            }

            case ATCMD_SESS_NONE:
            default:
            {
                /* Unknown type */
                break;
            }
        }
    }

    return 0;
}

static int atcmd_network_terminate_session (const int cid)
{
    atcmd_sess_context * ctx = NULL;
    int ret    = 0;
    int retval = 0;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("cid(0x%x)\n", cid);

    if (cid == 0xFF)
    {
        while (atcmd_sess_ctx_header)
        {
            ctx = atcmd_sess_ctx_header;

            switch (ctx->type)
            {
                case ATCMD_SESS_TCP_SERVER:
                {
                    ret = atcmd_network_disconnect_tcps(ctx->cid);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect tcp server(%d:%d)\n", ctx->cid, ret);
                        retval = -1;
                    }

                    break;
                }

                case ATCMD_SESS_TCP_CLIENT:
                {
                    ret = atcmd_network_disconnect_tcpc(ctx->cid);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect tcp client(%d:%d)\n", ctx->cid, ret);
                        retval = -2;
                    }

                    break;
                }

                case ATCMD_SESS_UDP_SESSION:
                {
                    ret = atcmd_network_disconnect_udps(ctx->cid);

                    if (ret)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect udp session(%d:%d)\n", ctx->cid, ret);
                        retval = -3;
                    }

                    break;
                }

                case ATCMD_SESS_NONE:
                default:
                {
                    /* Unknown type */
                    break;
                }
            }

            ret = atcmd_transport_delete_context(ctx->cid);

            if (ret)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to delete context(%d:%d)\n", ctx->cid, ret);
                retval = -4;
            }

            ctx = NULL;
        }
    }
    else
    {
        ctx = atcmd_transport_find_context(cid);

        if (!ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);

            return -5;
        }

        switch (ctx->type)
        {
            case ATCMD_SESS_TCP_SERVER:
            {
                ret = atcmd_network_disconnect_tcps(cid);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect tcp server(%d:%d)\n", cid, ret);

                    return ret;
                }

                break;
            }

            case ATCMD_SESS_TCP_CLIENT:
            {
                ret = atcmd_network_disconnect_tcpc(cid);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect tcp client(%d:%d)\n", cid, ret);

                    return ret;
                }

                break;
            }

            case ATCMD_SESS_UDP_SESSION:
            {
                ret = atcmd_network_disconnect_udps(cid);

                if (ret)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to disconnect udp session(%d:%d)\n", cid, ret);

                    return ret;
                }

                break;
            }

            case ATCMD_SESS_NONE:
            default:

                /* Unknown type */
                return -6;
        }

        ret = atcmd_transport_delete_context(cid);

        if (ret)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to delete context(%d:%d)\n", cid, ret);
            retval = -7;
        }

        ctx = NULL;
    }

    return retval;
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_read_esc_cmd (atcmd_w_ctrl_t * const p_at_ctrl,
                                                                   char                 * p_atcmd,
                                                                   size_t                 atcmd_len,
                                                                   char                ** pp_data,
                                                                   size_t               * p_data_len)
{
    typedef enum
    {
        READ_CID,
        READ_LENGTH,
        READ_REMOTE_IP,
        READ_REMOTE_PORT,
        READ_MODE,
        READ_DATA
    } atcmd_esc_cmd_parameter_step;

    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err = FSP_SUCCESS;

    int       atcmd_idx           = 0;
    int       prev_atcmd_idx      = 0;
    const int max_param_atcmd_len = 256;
    char      param_atcmd[256]    = {0x00, };
    int       param_atcmd_idx     = 0;

    char result[128] = {0x00, };

    char ch = 0;
    atcmd_esc_cmd_parameter_step param_step = READ_CID;
    int cid         = 0;
    int data_len    = 0;
    int r_mode      = pdFALSE;
    int port        = 0;
    int esc_h_atcmd = pdFALSE;

    char * p_data        = NULL;
    int    data_buf_size = 0;
    int    data_idx      = 0;

    int  is_keep_tmp_data = pdFALSE;
    char tmp_data;

    /* Check <ESC>H AT-CMD */
    if ((*(p_atcmd + 1) == 'h') || (*(p_atcmd + 1) == 'H'))
    {
        esc_h_atcmd = pdTRUE;
    }

    /* Adjust index because <ESC>X AT-CMD is already input */
    atcmd_idx     += 2;
    prev_atcmd_idx = atcmd_idx - 2;

    /* Read & Construct <ESC> AT-CMD */
    while (1)
    {
        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

        if (fsp_err == FSP_SUCCESS)
        {
            if (atcmd_idx >= (int) (atcmd_len - 2))
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("overflow(%d >= %d)\n", atcmd_idx, (atcmd_len - 2));
                err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                break;
            }

read_data:

            if (param_step == READ_DATA)
            {
                if (!p_data)
                {
                    if (data_len == 0)
                    {
                        if (esc_h_atcmd || r_mode)
                        {
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        data_buf_size = RM_ATCMD_W_CORE_SOCKET_MAX_TX_SIZE + 1;
                    }
                    else
                    {
                        data_buf_size = data_len + 1;
                    }

                    p_data = (char *) gs_esc_cmd_buffer;

                    data_idx = 0;

                    if (is_keep_tmp_data)
                    {
                        is_keep_tmp_data = pdFALSE;

                        p_data[data_idx] = tmp_data;
                        data_idx++;
                    }
                }

                if (esc_h_atcmd)
                {
                    RM_ATCMD_W_CORE_SOCKET_DEBUG("<ESC>H(%d)\n", data_len);

                    /* Response OK, and then waiting to read data */

                    ATCMD_ESC_OK_STR(result);

                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));

                    /* Read data */
                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) p_data, data_len);

                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        break;
                    }

                    break;
                }
                else if (r_mode)
                {
                    RM_ATCMD_W_CORE_SOCKET_DEBUG("R mode(%d)\n", data_len);

                    p_data[data_idx++] = ch;

                    if (data_len > 1)
                    {
                        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) (p_data + 1), (data_len - 1));

                        if (fsp_err != FSP_SUCCESS)
                        {
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }
                    }

                    break;
                }
                else
                {
                    RM_ATCMD_W_CORE_SOCKET_DEBUG("T mode data_len:%d, data_idx:%d, ch=%c\n", data_len, data_idx, ch);

                    /* BackSpace */
                    if (ch == '\b')
                    {
                        if (data_idx > 0)
                        {
                            data_idx--;
                            p_data[data_idx] = '\0';
                            bsp_safe_strcpy(result, "\33[OK", sizeof(result));
                            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
                        }
                    }
                    else if (data_len)
                    {
                        p_data[data_idx] = ch;
                        data_idx++;

                        fsp_err =
                            RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) (p_data + data_idx), (data_len - data_idx));

                        if (fsp_err != FSP_SUCCESS)
                        {
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        break;
                    }
                    else
                    {
                        if ((data_idx >= (data_buf_size - 1)) || (ch == '\n') || (ch == '\r'))
                        {
                            data_len = data_idx;
                            break;
                        }

                        p_data[data_idx] = ch;
                        data_idx++;
                    }
                }
            }
            else if (param_step == READ_MODE)
            {
                if (param_atcmd_idx == 0)
                {
                    /* Check mode */
                    if ((ch == 'r') || (ch == 'R'))
                    {
                        r_mode = pdTRUE;
                    }
                    else if ((ch == 't') || (ch == 'T'))
                    {
                        r_mode = pdFALSE;
                    }
                    else
                    {
                        /* Default mode */
                        r_mode     = pdFALSE;
                        param_step = READ_DATA;
                        goto read_data;
                    }

                    param_atcmd[param_atcmd_idx] = ch;
                    param_atcmd_idx++;
                }
                else if (param_atcmd_idx == 1)
                {
                    param_step = READ_DATA;

                    /* Check comma */
                    if (ch != 0x2C)
                    {
                        is_keep_tmp_data = pdTRUE;
                        tmp_data         = param_atcmd[0];
                        r_mode           = pdFALSE;
                        goto read_data;
                    }
                }
            }
            else
            {
                if (param_atcmd_idx > max_param_atcmd_len)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("overflow(%d > %d)\n", param_atcmd_idx, max_param_atcmd_len);
                    err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                    break;
                }

                /* Allowed numeric(common) */
                if (((ch >= 0x30) && (ch <= 0x39))
#if defined(__SUPPORT_IPV4__)
                    || (ch == 0x2E)                                                  // allowed '.'(ipv4)
#endif                                                                               // __SUPPORT_IPV4__
#if defined(__SUPPORT_IPV6__)
                    || ((ch >= 0x41 && ch <= 0x46) || (ch >= 0x61 && ch <= 0x66)) || // allowed A~F or a~f(ipv6)
                    (ch == 0x3A)                                                     // allowed ':'(ipv6)
#endif                                                                               // __SUPPORT_IPV6__
                    )
                {
                    param_atcmd[param_atcmd_idx] = ch;
                    param_atcmd_idx++;
                }
                else if ((ch == '\n') || (ch == '\r'))
                {
                    if (param_step == READ_REMOTE_PORT)
                    {
                        RM_ATCMD_W_CORE_SOCKET_DEBUG("Parse remote port\n");
                        goto read_remote_port;
                    }

                    /* Rollback to previous atcmd idx */
                    atcmd_idx = prev_atcmd_idx;
                    break;
                }
                else if (ch == 0x2C)
                {
                    /* comma(,) */
                    RM_ATCMD_W_CORE_SOCKET_DEBUG("param_step(%d,%s)\n", param_step, param_atcmd);

                    /* Read CID */
                    if (param_step == READ_CID)
                    {
                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &cid, POL_1) != 0)
                        {
                            RM_ATCMD_W_CORE_SOCKET_ERROR("invalid cid(%s)\n", param_atcmd);
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        prev_atcmd_idx = prev_atcmd_idx;
                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);
                        param_step = READ_LENGTH;
                    }
                    else if (param_step == READ_LENGTH)
                    {
                        /* Read length */
                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &data_len, POL_1) != 0)
                        {
                            RM_ATCMD_W_CORE_SOCKET_ERROR("invalid data length(%s)\n", param_atcmd);
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        /* Check max data size */
                        if ((data_len > RM_ATCMD_W_CORE_SOCKET_MAX_TX_SIZE) || (data_len < 0))
                        {
                            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid data length(max:%d,%d)\n",
                                                         RM_ATCMD_W_CORE_SOCKET_MAX_TX_SIZE,
                                                         data_len);
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        prev_atcmd_idx = prev_atcmd_idx;
                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);
                        param_step = READ_REMOTE_IP;
                    }
                    else if (param_step == READ_REMOTE_IP)
                    {
                        /* Read IP address */
                        int ip_type = 0;
                        struct sockaddr_storage valid_ip = {0x00, };
                        ip_type = rm_wifi_select_ipaddr_type_from_str(param_atcmd, &valid_ip);

                        /* Common */
                        if ((strcmp(param_atcmd, "0") != 0)
#if defined(__SUPPORT_IPV4__)
                            && (ip_type != IPADDR_TYPE_V4)
#endif                                 // __SUPPORT_IPV4__
#if defined(__SUPPORT_IPV6__)
                            && (ip_type != IPADDR_TYPE_V6)
#endif                                 // __SUPPORT_IPV6__
                            )
                        {
                            RM_ATCMD_W_CORE_SOCKET_ERROR("invalid ip address(%s)\n", param_atcmd);
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        prev_atcmd_idx = prev_atcmd_idx;
                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);
                        param_step = READ_REMOTE_PORT;
                    }
                    else if (param_step == READ_REMOTE_PORT)
                    {
                        /* Read port number */
read_remote_port:

                        if ((rm_atcmd_w_core_common_stoi(param_atcmd, &port,
                                                         POL_1) != 0) || (port < 0) || (port > 0xFFFF))
                        {
                            RM_ATCMD_W_CORE_SOCKET_ERROR("invalid port(%s)\n", param_atcmd);
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            break;
                        }

                        prev_atcmd_idx = prev_atcmd_idx;
                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);

                        /* Skipped mode step in case of <ESC>H AT-CMD */
                        if (esc_h_atcmd)
                        {
                            param_step = READ_DATA;
                            goto read_data;
                        }
                        else
                        {
                            param_step = READ_MODE;
                        }
                    }

                    memset(param_atcmd, 0x00, max_param_atcmd_len);
                    param_atcmd_idx = 0;
                }
                else
                {
                    /* Rollback to previous atcmd idx */
                    atcmd_idx = prev_atcmd_idx;
                    break;
                }
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            break;
        }
    }

    p_atcmd[atcmd_idx] = '\0';
    atcmd_idx++;

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        *pp_data    = p_data;
        *p_data_len = data_len;
    }
    else
    {
        p_data = NULL;

        *pp_data    = NULL;
        *p_data_len = 0;
    }

    return err;
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_transfer_msg (atcmd_w_ctrl_t * const p_at_ctrl,
                                                                   char                 * p_in,
                                                                   size_t                 inlen)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char * p_str_cid = NULL;
    int    cid       = 0;
    char * ip_addr   = NULL;
    int    peer_port = 0;
    char * p_atcmd   = NULL;

    char * p_data   = NULL;
    size_t data_len = 0;

    atcmd_sess_context * ctx      = NULL;
    atcmd_tcps_context * tcps_ctx = NULL;
    atcmd_tcpc_context * tcpc_ctx = NULL;
    atcmd_udps_context * udps_ctx = NULL;

    char result[32] = {0x00, };

    /* Read remaining data */
    err = rm_atcmd_w_core_socket_read_esc_cmd(p_at_ctrl, p_in, inlen, &p_data, &data_len);

    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        goto end;
    }

    p_atcmd = (char *) p_in;

    /* Parse <ESC> AT-CMD */
    if (strstr(p_atcmd, ",") == NULL)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get comman\n");
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto end;
    }

    /* Read CID */
    p_str_cid = strtok(p_atcmd + 2, ",");

    if (rm_atcmd_w_core_common_stoi(p_str_cid, &cid, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get CID\n");
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto end;
    }

    ctx = atcmd_transport_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto end;
    }

    if (ctx->type == ATCMD_SESS_TCP_SERVER)
    {
        tcps_ctx = ctx->ctx.tcps;
    }
    else if (ctx->type == ATCMD_SESS_TCP_CLIENT)
    {
        tcpc_ctx = ctx->ctx.tcpc;
    }
    else if (ctx->type == ATCMD_SESS_UDP_SESSION)
    {
        udps_ctx = ctx->ctx.udps;
    }

    if ((ctx->type == ATCMD_SESS_TCP_SERVER) &&
        (tcps_ctx->state == ATCMD_TCPS_STATE_ACCEPT))
    {
        /* <ESC>M<cid>,<data length>,0.0.0.0,19999,1234567890 */
        if (tcps_ctx->cli_cnt > 0)
        {
            /* Read Data length - Skipped */
            strtok(NULL, ",");

            /* Read IP address */
            ip_addr = strtok(NULL, ",");

            if (ip_addr == NULL)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get IP address\n");
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }

            if (rm_atcmd_w_core_common_stoi(strtok(NULL, ","), &peer_port, POL_1) != 0)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get IP address\n");
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }

            err = atcmd_tcps_tx(tcps_ctx, p_data, &data_len, ip_addr, peer_port);
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
        }
    }
    else if ((ctx->type == ATCMD_SESS_TCP_CLIENT) && (tcpc_ctx->state == ATCMD_TCPC_STATE_CONNECTED))
    {
        /* <ESC>M<cid>,<data_length>,0,0,1234567890 */
        /* Read Data length - Skipped*/
        strtok(NULL, ",");

        /* Skipp peer IP address */
        ip_addr = strtok(NULL, ",");

        if (ip_addr == NULL)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get IP address\n");
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        /* Skip peer port number */
        if (rm_atcmd_w_core_common_stoi(strtok(NULL, ","), &peer_port, POL_1) != 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get port\n");
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        err = atcmd_tcpc_tx_with_peer_info(tcpc_ctx, ip_addr, peer_port, p_data, &data_len);
    }
    else if ((ctx->type == ATCMD_SESS_UDP_SESSION) && (udps_ctx->state == ATCMD_UDPS_STATE_ACTIVE))
    {
        /*  <ESC>M<cid>,<data_length>,0.0.0.0,19999,1234567890 */
        /* Read Data length - Skipped */
        strtok(NULL, ",");

        /* Read IP address */
        ip_addr = strtok(NULL, ",");

        if (ip_addr == NULL)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get IP address\n");
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(strtok(NULL, ","), &peer_port, POL_1) != 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get IP address\n");
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        err = atcmd_udps_tx(udps_ctx, p_data, &data_len, ip_addr, peer_port);
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        ATCMD_ESC_OK_STR(result);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result, strlen(result));
    }
    else
    {
        if (err == FSP_ERR_AT_CMD_ERR_DATA_TX)
        {
            char ext_err_str[8] = {0x00, };

            snprintf(ext_err_str, sizeof(ext_err_str), "%d", data_len);
#if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
            rm_atcmd_w_core_common_print_error_code_esc(p_at_ctrl, err, ext_err_str);
#else
            rm_atcmd_w_core_common_print_error_code_ext(p_at_ctrl, err, ext_err_str);
#endif
        }
        else
        {
#if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
            rm_atcmd_w_core_common_print_error_code_esc(p_at_ctrl, err, NULL);
#else
            rm_atcmd_w_core_common_print_error_code(p_at_ctrl, err);
#endif
        }
    }

    p_data = NULL;

    return err;
}

/**************************************************************************/
/* Related with TLS client                                                */
/**************************************************************************/
static int atcmd_transport_ssl_create_tls_client (int * cid)
{
    int                 status = RRQ_APP_SUCCESS;
    atcmd_tls_context * ctx    = NULL;

    ctx = atcmd_transport_ssl_create_context();

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tls context\n");

        return RRQ_APP_MALLOC_ERROR;
    }

    ctx->role = ATCMD_TLS_CLIENT;

    ctx->ctx.tlsc_ctx = pvPortMalloc(sizeof(atcmd_tlsc_context));

    if (!ctx->ctx.tlsc_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tls client context(%d)\n", sizeof(atcmd_tlsc_context));
        atcmd_transport_ssl_delete_context(ctx->cid);

        return RRQ_APP_MALLOC_ERROR;
    }

    status = atcmd_tlsc_init_context(ctx->ctx.tlsc_ctx);

    if (status != RRQ_APP_SUCCESS)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tls client context(0x%x)\n", status);
        vPortFree(ctx->ctx.tlsc_ctx);
        atcmd_transport_ssl_delete_context(ctx->cid);

        return RRQ_APP_NOT_CREATED;
    }

    *cid = ctx->cid;

    return RRQ_APP_SUCCESS;
}

static int atcmd_transport_ssl_delete_tls_client (int cid)
{
    int                  status   = RRQ_APP_SUCCESS;
    atcmd_tls_context  * ctx      = NULL;
    atcmd_tlsc_context * tlsc_ctx = NULL;

    unsigned long max_wait_time = 1000;

    ctx = atcmd_transport_ssl_find_context(cid);

    if (!ctx || (ctx->role != ATCMD_TLS_CLIENT))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found(cid:%d)\n", cid);

        return RRQ_APP_NOT_FOUND;
    }

    tlsc_ctx = ctx->ctx.tlsc_ctx;

    status = atcmd_tlsc_stop(tlsc_ctx, max_wait_time);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to stop tls client(0x%x)\n", status);

        return status;
    }

    status = atcmd_tlsc_deinit_context(tlsc_ctx);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to deinit tls client context(0x%x)n", status);
    }

    atcmd_transport_ssl_delete_context(cid);

    vPortFree(tlsc_ctx);

    return RRQ_APP_SUCCESS;
}

static int atcmd_transport_ssl_run_tls_client (atcmd_tls_context * ctx)
{
    int status = RRQ_APP_SUCCESS;

    atcmd_tlsc_context * tlsc_ctx = ctx->ctx.tlsc_ctx;

    unsigned long sleep_time    = 100;
    unsigned long wait_time     = 0;
    unsigned long max_wait_time = 0;

    max_wait_time +=
        ((ATCMD_TLSC_HOST_TIMEOUT + ATCMD_TLSC_CONN_TIMEOUT + ATCMD_TLSC_RECONN_SLEEP_TIMEOUT) *
         ATCMD_TLSC_MAX_CONN_CNT);
    max_wait_time += (((ATCMD_TLSC_HANDSHAKE_TIMEOUT * 4) + ATCMD_TLSC_DEF_TIMEOUT) * ATCMD_TLSC_MAX_CONN_CNT);

    status = atcmd_tlsc_run(tlsc_ctx, ctx->cid);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to run tls client(0x%x)\n", status);

        return status;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Waiting for tls session establishes(%ld)\n", max_wait_time);

    do
    {
        status = RRQ_APP_CANNOT_START;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("sleep_time(%ld), wait_time(%ld), max(%ld)\n", sleep_time, wait_time,
                                     max_wait_time);

        vTaskDelay(portCONVERT_MS_2_TICKS(sleep_time));

        wait_time += sleep_time;

        if (tlsc_ctx->state == ATCMD_TLSC_STATE_CONNECTED)
        {
            status = RRQ_APP_SUCCESS;
            break;
        }
        else if (tlsc_ctx->state == ATCMD_TLSC_STATE_TERMINATED)
        {
            break;
        }
    } while (wait_time < max_wait_time);

    if (status == RRQ_APP_SUCCESS)
    {
#if CFG_PMGR
        atcmd_transport_ssl_update_context_dpm();
#endif                                 /* CFG_PMGR */
    }

    return status;
}

static int atcmd_transport_ssl_recover_tls_session (atcmd_w_ctrl_t * const p_at_ctrl)
{
    int status = RRQ_APP_SUCCESS;
    int retval = RRQ_APP_SUCCESS;

    atcmd_tls_context * ctx = NULL;

    int idx         = 0;
    int del_cid_idx = 0;
    int del_cid[ATCMD_TLS_MAX_ALLOW_CNT] = {-1, };
#if CFG_PMGR
#if defined __SUPPORT_SNTP_CLIENT__
    int sntp_wait_cnt = 0;
#endif /* __SUPPORT_SNTP_CLIENT__ */
#endif

    unsigned long sleep_time    = 100;
    unsigned long wait_time     = 0;
    unsigned long max_wait_time = 0;

    max_wait_time += ((ATCMD_TLSC_CONN_TIMEOUT + ATCMD_TLSC_RECONN_SLEEP_TIMEOUT) * ATCMD_TLSC_MAX_CONN_CNT);
    max_wait_time += (((ATCMD_TLSC_HANDSHAKE_TIMEOUT * 4) + ATCMD_TLSC_DEF_TIMEOUT) * ATCMD_TLSC_MAX_CONN_CNT);

    if (!atcmd_tls_ctx_header)
    {
        return 0;
    }

    // wait for network connection
#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled() && !RM_PMGR_W_dpm_is_wakeup())
    {
        if (chk_network_ready(WLAN0_IFACE) != 1)
        {
            int wifi_wait_time = 30;   /* 30 seconds */

            while (1)
            {
                if (chk_network_ready(0) == 1)
                {
                    break;
                }

                vTaskDelay(portCONVERT_MS_2_TICKS(1000));

                if (--wifi_wait_time == 0)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("[TLS Client] Timeout to connect an Wi-Fi AP\n");

                    return RRQ_APP_NOT_SUCCESSFUL;
                }
            }
        }

 #ifdef __SUPPORT_SNTP_CLIENT__
        RM_ATCMD_W_CORE_SOCKET_DEBUG(">>> Checking SNTP ... \n");
        while (1)
        {
            /* waiting for SNTP Sync */
            if (get_sntp_use() == FALSE)
            {
                RM_ATCMD_W_CORE_SOCKET_DEBUG(">>> Not using SNTP \n");
                break;
            }

            if (is_sntp_sync() == TRUE)
            {
                break;
            }

            if ((++sntp_wait_cnt % 50) == 0)
            {
                /* 5 SEC */
                RM_ATCMD_W_CORE_SOCKET_ERROR(">>> Time not synched by SNTP \n");
                break;
            }

            vTaskDelay(portCONVERT_MS_2_TICKS(100));
        }
 #endif                                /* __SUPPORT_SNTP_CLIENT__ */
    }
#endif                                 /* CFG_PMGR */

    // for tls session
    for (ctx = atcmd_tls_ctx_header; ctx != NULL; ctx = ctx->next)
    {
        if (ctx->role == ATCMD_TLS_CLIENT) // tls client
        {
            ctx->ctx.tlsc_ctx = pvPortMalloc(sizeof(atcmd_tlsc_context));

            if (!ctx->ctx.tlsc_ctx)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tls client context(%d)\n", sizeof(atcmd_tlsc_context));
                goto end_of_tls_client;
            }

            status = atcmd_tlsc_init_context(ctx->ctx.tlsc_ctx);

            if (status)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to init tls client context(0x%x)\n", status);
                goto end_of_tls_client;
            }

            atcmd_tlsc_set_at_ctrl(ctx->ctx.tlsc_ctx, p_at_ctrl);

            status = atcmd_tlsc_setup_config(ctx->ctx.tlsc_ctx, &(ctx->conf.tlsc_conf));

            if (status)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to setup tls client's config(0x%x)\n", status);
                goto end_of_tls_client;
            }

            status = atcmd_tlsc_run(ctx->ctx.tlsc_ctx, ctx->cid);

            if (status)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to run tls client(0x%x)\n", status);
                goto end_of_tls_client;
            }

end_of_tls_client:

            if (status)
            {
                if (ctx->ctx.tlsc_ctx)
                {
                    vPortFree(ctx->ctx.tlsc_ctx);
                    ctx->ctx.tlsc_ctx = NULL;
                }

                retval                += status;
                del_cid[del_cid_idx++] = ctx->cid;
            }
        }
    }

    for (idx = 0; idx < del_cid_idx; idx++)
    {
        atcmd_transport_ssl_delete_context(del_cid[idx]);
    }

    // wait for connection
    for (ctx = atcmd_tls_ctx_header; ctx != NULL; ctx = ctx->next)
    {
        if (ctx->role == ATCMD_TLS_CLIENT)
        {
            do
            {
                status = RRQ_APP_CANNOT_START;

                RM_ATCMD_W_CORE_SOCKET_DEBUG("cid(%d), sleep_time(%ld), wait_time(%ld), max(%ld)\n",
                                             ctx->cid,
                                             sleep_time,
                                             wait_time,
                                             max_wait_time);

                vTaskDelay(portCONVERT_MS_2_TICKS(sleep_time));

                wait_time += sleep_time;

                if (ctx->ctx.tlsc_ctx->state == ATCMD_TLSC_STATE_CONNECTED)
                {
                    status = RRQ_APP_SUCCESS;
                    break;
                }
                else if (ctx->ctx.tlsc_ctx->state == ATCMD_TLSC_STATE_TERMINATED)
                {
                    break;
                }
            } while (wait_time < max_wait_time);
        }
    }

#if CFG_PMGR
    atcmd_transport_ssl_update_context_dpm();
#endif                                 /* CFG_PMGR */

    return retval;
}

/**************************************************************************/
/* Related with management of TLS Client/Server                           */
/**************************************************************************/
#if CFG_PMGR
static void atcmd_transport_ssl_recover_context_nvram (void)
{
    int status = RRQ_APP_SUCCESS;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    RRQ61X_ATCMD_CONF_INT tls_nvr_cid_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_CID_0,
        RRQ61X_CONF_INT_ATCMD_TLS_CID_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_role_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_profile_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_1
    };

    atcmd_tls_context * prev_ctx = NULL;
    atcmd_tls_context * new_ctx  = NULL;

    int tls_nvr_idx = -1;

    int tls_nvr_cid_value[ATCMD_TLS_MAX_ALLOW_CNT]     = {-1, };
    int tls_nvr_role_value[ATCMD_TLS_MAX_ALLOW_CNT]    = {-1, };
    int tls_nvr_profile_value[ATCMD_TLS_MAX_ALLOW_CNT] = {-1, };

    int cid     = -1;
    int role    = -1;
    int profile = -1;

    /* Already existed */
    if (atcmd_tls_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("Already existed atcmd_tls_ctx_header\n");

        return;
    }

    for (tls_nvr_idx = 0; tls_nvr_idx < ATCMD_TLS_MAX_ALLOW_CNT; tls_nvr_idx++)
    {
        cid     = -1;
        role    = ATCMD_TLS_NONE;
        profile = -1;

        tls_nvr_cid_value[tls_nvr_idx]     = cid;
        tls_nvr_role_value[tls_nvr_idx]    = role;
        tls_nvr_profile_value[tls_nvr_idx] = profile;

        status = get_atcmd_param_int(tls_nvr_cid_name[tls_nvr_idx], &cid);

        if (status || (cid < 0) || (cid > ATCMD_TLS_MAX_ALLOW_CNT))
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invaild CID(%d,%d,%d)\n", status, tls_nvr_cid_name[tls_nvr_idx], cid);
            continue;
        }

        status = get_atcmd_param_int(tls_nvr_role_name[tls_nvr_idx], &role);

        if (status || ((role != ATCMD_TLS_SERVER) && (role != ATCMD_TLS_CLIENT)))
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid role(%d,%d,%d)\n", status, tls_nvr_role_name[tls_nvr_idx], role);
            continue;
        }

        status = get_atcmd_param_int(tls_nvr_profile_name[tls_nvr_idx], &profile);

        if (status || (profile < 0) || (profile >= ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE))
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get profile(%d,%d,%d)\n",
                                         status,
                                         tls_nvr_profile_name[tls_nvr_idx],
                                         profile);
            continue;
        }

        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. CID: %d, Role: %d, Profile: %d\n", tls_nvr_idx, cid, role, profile);

        tls_nvr_cid_value[tls_nvr_idx]     = cid;
        tls_nvr_role_value[tls_nvr_idx]    = role;
        tls_nvr_profile_value[tls_nvr_idx] = profile;
    }

    // sort
    for (int i = 0; i < ATCMD_TLS_MAX_ALLOW_CNT - 1; i++)
    {
        for (int j = 0; j < ATCMD_TLS_MAX_ALLOW_CNT - i - 1; j++)
        {
            if (tls_nvr_cid_value[j] > tls_nvr_cid_value[j + 1])
            {
                RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. Swap cid(%d,%d)\n", j, tls_nvr_cid_value[j],
                                             tls_nvr_cid_value[j + 1]);

                // swap
                int tmp = tls_nvr_cid_value[j];
                tls_nvr_cid_value[j]     = tls_nvr_cid_value[j + 1];
                tls_nvr_cid_value[j + 1] = tmp;

                tmp = tls_nvr_role_value[j];
                tls_nvr_role_value[j]     = tls_nvr_role_value[j + 1];
                tls_nvr_role_value[j + 1] = tmp;

                tmp = tls_nvr_profile_value[j];
                tls_nvr_profile_value[j]     = tls_nvr_profile_value[j + 1];
                tls_nvr_profile_value[j + 1] = tmp;
            }
        }
    }

    for (tls_nvr_idx = 0; tls_nvr_idx < ATCMD_TLS_MAX_ALLOW_CNT; tls_nvr_idx++)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("tls_nvr_cid_value[%d]=%d, tls_nvr_role_value[%d]=%d, "
                                     "tls_nvr_profile_value[%d]=%d\n",
                                     tls_nvr_idx,
                                     tls_nvr_cid_value[tls_nvr_idx],
                                     tls_nvr_idx,
                                     tls_nvr_role_value[tls_nvr_idx],
                                     tls_nvr_idx,
                                     tls_nvr_profile_value[tls_nvr_idx]);

        if (tls_nvr_cid_value[tls_nvr_idx] >= 0)
        {
            new_ctx = pvPortMalloc(sizeof(atcmd_tls_context));

            if (!new_ctx)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create atcmd_tls_context(%d)\n", sizeof(atcmd_tls_context));
                continue;
            }

            memset(new_ctx, 0x00, sizeof(atcmd_tls_context));

            // copy
            new_ctx->cid  = tls_nvr_cid_value[tls_nvr_idx];
            new_ctx->role = (atcmd_tls_role) tls_nvr_role_value[tls_nvr_idx];

            if (new_ctx->role == ATCMD_TLS_CLIENT)
            {
                status = atcmd_transport_ssl_recover_tlsc_nvram(new_ctx, tls_nvr_profile_value[tls_nvr_idx]);

                if (status)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to recover tls client(0x%x)\n", status);
                    vPortFree(new_ctx);
                    continue;
                }
            }

            // link
            if (prev_ctx)
            {
                prev_ctx->next = new_ctx;
            }
            else
            {
                atcmd_tls_ctx_header = new_ctx;
            }

            prev_ctx = new_ctx;
        }
    }
}

#endif                                 /* CFG_PMGR */

static int atcmd_transport_ssl_save_context_nvram (atcmd_tls_context * ctx)
{
    int status = RRQ_APP_SUCCESS;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    RRQ61X_ATCMD_CONF_INT tls_nvr_cid_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_CID_0,
        RRQ61X_CONF_INT_ATCMD_TLS_CID_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_role_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_profile_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_1
    };

    int tls_nvr_idx  = -1;
    int tlsc_nvr_idx = -1;
    int tls_nvr_cid_value[ATCMD_TLS_MAX_ALLOW_CNT]             = {-1, };
    int tlsc_nvr_profile_idx[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] = {pdFALSE, };
    int cid     = -1;
    int role    = -1;
    int profile = -1;

    if (!atcmd_tls_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("Empty CID\n");

        return RRQ_APP_SUCCESS;
    }

    for (tls_nvr_idx = 0; tls_nvr_idx < ATCMD_TLS_MAX_ALLOW_CNT; tls_nvr_idx++)
    {
        cid     = -1;
        role    = ATCMD_TLS_NONE;
        profile = -1;

        tls_nvr_cid_value[tls_nvr_idx] = cid;

        status = get_atcmd_param_int(tls_nvr_cid_name[tls_nvr_idx], &cid);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get cid(%d/%d)\n", status, tls_nvr_cid_name[tls_nvr_idx]);
        }

        status = get_atcmd_param_int(tls_nvr_role_name[tls_nvr_idx], &role);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get role(%d/%d)\n", status, tls_nvr_role_name[tls_nvr_idx]);
        }

        status = get_atcmd_param_int(tls_nvr_profile_name[tls_nvr_idx], &profile);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get profile(%d/%d)\n", status, tls_nvr_profile_name[tls_nvr_idx]);
        }

        tls_nvr_cid_value[tls_nvr_idx] = cid;

        if ((cid == ctx->cid) && (role == ctx->role))
        {
            /* Found previous profile */
            if (role == ATCMD_TLS_CLIENT)
            {
                return atcmd_transport_ssl_save_tlsc_nvram(ctx, profile);
            }
        }
        else if (role == ctx->role)
        {
            /* Keep profile */
            if (role == ATCMD_TLS_CLIENT)
            {
                tlsc_nvr_profile_idx[profile] = pdTRUE;
            }
        }
    }

    /* New profile */
    for (tls_nvr_idx = 0; tls_nvr_idx < ATCMD_TLS_MAX_ALLOW_CNT; tls_nvr_idx++)
    {
        if (tls_nvr_cid_value[tls_nvr_idx] == -1)
        {
            if (ctx->role == ATCMD_TLS_CLIENT)
            {
                for (tlsc_nvr_idx = 0; tlsc_nvr_idx < ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE; tlsc_nvr_idx++)
                {
                    if (!tlsc_nvr_profile_idx[tlsc_nvr_idx])
                    {
                        RM_ATCMD_W_CORE_SOCKET_DEBUG("Found Empty profile(%d)\n", tlsc_nvr_idx);
                        break;
                    }
                }

                if (tlsc_nvr_idx < ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE)
                {
                    status = set_atcmd_param_int(tls_nvr_cid_name[tls_nvr_idx], ctx->cid);

                    if (status)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set cid(%d/%d)\n", status,
                                                     tls_nvr_cid_name[tls_nvr_idx]);
                    }

                    status = set_atcmd_param_int(tls_nvr_role_name[tls_nvr_idx], ctx->role);

                    if (status)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set role(%d/%d)\n", status,
                                                     tls_nvr_role_name[tls_nvr_idx]);
                    }

                    status = set_atcmd_param_int(tls_nvr_profile_name[tls_nvr_idx], tlsc_nvr_idx);

                    if (status)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set profile(%d/%d)\n", status,
                                                     tls_nvr_role_name[tls_nvr_idx]);
                    }

                    return atcmd_transport_ssl_save_tlsc_nvram(ctx, tlsc_nvr_idx);
                }
            }
        }
    }

    return RRQ_APP_NOT_FOUND;
}

static int atcmd_transport_ssl_clear_all_context_nvram (void)
{
    int status = RRQ_APP_SUCCESS;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    RRQ61X_ATCMD_CONF_INT tls_nvr_cid_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_CID_0,
        RRQ61X_CONF_INT_ATCMD_TLS_CID_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_role_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_ROLE_1
    };

    RRQ61X_ATCMD_CONF_INT tls_nvr_profile_name[ATCMD_TLS_MAX_ALLOW_CNT] =
    {
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_0,
        RRQ61X_CONF_INT_ATCMD_TLS_PROFILE_1
    };

    int tls_nvr_idx = 0;

    int cid     = -1;
    int role    = -1;
    int profile = -1;

    for (tls_nvr_idx = 0; tls_nvr_idx < ATCMD_TLS_MAX_ALLOW_CNT; tls_nvr_idx++)
    {
        cid     = -1;
        role    = ATCMD_TLS_NONE;
        profile = -1;

        status = get_atcmd_param_int(tls_nvr_cid_name[tls_nvr_idx], &cid);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get cid(%d/%d)\n", status, tls_nvr_cid_name[tls_nvr_idx]);
        }

        status = get_atcmd_param_int(tls_nvr_role_name[tls_nvr_idx], &role);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get role(%d/%d)\n", status, tls_nvr_role_name[tls_nvr_idx]);
        }

        status = get_atcmd_param_int(tls_nvr_profile_name[tls_nvr_idx], &profile);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get profile(%d/%d)\n", status, tls_nvr_profile_name[tls_nvr_idx]);
        }

        if ((role == ATCMD_TLS_CLIENT) && (profile >= 0))
        {
            status = atcmd_transport_ssl_save_tlsc_nvram(NULL, profile);

            if (status)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("failed to clear tls client(0x%x)\n", status);
            }
        }

        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. cid(%d), role(%d), profile(%d)\n", tls_nvr_idx, cid, role, profile);

        /* Clear cid */
        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. Clear cid\n", tls_nvr_idx);
        set_atcmd_param_int(tls_nvr_cid_name[tls_nvr_idx], -1);

        /* Clear role */
        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. Clear role\n", tls_nvr_idx);
        set_atcmd_param_int(tls_nvr_role_name[tls_nvr_idx], -1);

        /* Clear profile */
        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. Clear profile\n", tls_nvr_idx);
        set_atcmd_param_int(tls_nvr_profile_name[tls_nvr_idx], -1);
    }

    return status;
}

static int atcmd_transport_ssl_save_tlsc_nvram (atcmd_tls_context * ctx, int profile_idx)
{
    int status = RRQ_APP_SUCCESS;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start(ctx:%p, profile_idx:%d)\n", ctx, profile_idx);

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_ca_cert_name[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_cert_name[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_hostname[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_peer_ipaddr[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_auth_mode[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_incoming_len[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_outgoing_len[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_local_port[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_peer_port[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_1,
    };

    atcmd_tlsc_config * tlsc_conf = NULL;

    char * ca_cert_name = NULL;
    char * cert_name    = NULL;
    char * hostname     = NULL;
    int    incoming_len = ATCMD_TLSC_DEF_INCOMING_LEN;
    int    outgoing_len = ATCMD_TLSC_DEF_OUTGOING_LEN;
    int    auth_mode    = pdFALSE;
    int    local_port   = 0;
    char   peer_ipaddr[ATCMD_TLSC_NVR_PEER_IPADDR_LEN] = {0x00, };
    int    peer_port = 0;

    if (ctx)
    {
        tlsc_conf = &(ctx->conf.tlsc_conf);

        /* Set ca cert name */
        if (strlen(tlsc_conf->ca_cert_name))
        {
            ca_cert_name = tlsc_conf->ca_cert_name;

            RM_ATCMD_W_CORE_SOCKET_DEBUG("CA cert name: %s(%d)\n", ca_cert_name, strlen(ca_cert_name));
        }

        /* Set cert name */
        if (strlen(tlsc_conf->cert_name))
        {
            cert_name = tlsc_conf->cert_name;

            RM_ATCMD_W_CORE_SOCKET_DEBUG("cert name: %s(%d)\n", cert_name, strlen(cert_name));
        }

        /* Set hostname */
        if (strlen(tlsc_conf->hostname))
        {
            hostname = tlsc_conf->hostname;

            RM_ATCMD_W_CORE_SOCKET_DEBUG("host name: %s(%d)\n", hostname, strlen(hostname));
        }

        /* Set incoming_buflen */
        incoming_len = tlsc_conf->incoming_buflen;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("incoming_len: %d\n", incoming_len);

        /* Set outgoing_buflen */
        outgoing_len = tlsc_conf->outgoing_buflen;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("outgoing_len: %d\n", outgoing_len);

        /* Set auth mode */
        auth_mode = tlsc_conf->auth_mode;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("auth_mode: %d\n", auth_mode);

        /* Set local port */
        local_port = tlsc_conf->local_port;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("local_port: %d\n", local_port);

        /* Set peer IP address */
        strncpy(peer_ipaddr, tlsc_conf->svr_addr, sizeof(peer_ipaddr));

        RM_ATCMD_W_CORE_SOCKET_DEBUG("peer_ipaddr: %s(%d)\n", peer_ipaddr, strlen(peer_ipaddr));

        /* Set peer port */
        rm_atcmd_w_core_common_stoi(tlsc_conf->svr_port, &peer_port, POL_1);

        RM_ATCMD_W_CORE_SOCKET_DEBUG("peer_port: %d\n", peer_port);
    }

    status = set_atcmd_param_str(tlsc_nvr_ca_cert_name[profile_idx], ca_cert_name);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_ca_cert_name[profile_idx],
                                     status);
    }

    status = set_atcmd_param_str(tlsc_nvr_cert_name[profile_idx], cert_name);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_cert_name[profile_idx], status);
    }

    if (strlen(peer_ipaddr))
    {
        status = set_atcmd_param_str(tlsc_nvr_peer_ipaddr[profile_idx], peer_ipaddr);
    }
    else
    {
        status = set_atcmd_param_str(tlsc_nvr_peer_ipaddr[profile_idx], NULL);
    }

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_peer_ipaddr[profile_idx],
                                     status);
    }

    status = set_atcmd_param_str(tlsc_nvr_hostname[profile_idx], hostname);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_hostname[profile_idx], status);
    }

    status = set_atcmd_param_int(tlsc_nvr_incoming_len[profile_idx], incoming_len);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_incoming_len[profile_idx],
                                     status);
    }

    status = set_atcmd_param_int(tlsc_nvr_outgoing_len[profile_idx], outgoing_len);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_outgoing_len[profile_idx],
                                     status);
    }

    status = set_atcmd_param_int(tlsc_nvr_auth_mode[profile_idx], auth_mode);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_auth_mode[profile_idx], status);
    }

    status = set_atcmd_param_int(tlsc_nvr_local_port[profile_idx], local_port);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_local_port[profile_idx], status);
    }

    status = set_atcmd_param_int(tlsc_nvr_peer_port[profile_idx], peer_port);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set data(%d) to nvram(%d)\n", tlsc_nvr_peer_port[profile_idx], status);
    }

    return status;
}

#if CFG_PMGR
static int atcmd_transport_ssl_recover_tlsc_nvram (atcmd_tls_context * ctx, int profile_idx)
{
    int status = RRQ_APP_SUCCESS;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Start\n");

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_ca_cert_name[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_CA_CERT_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_cert_name[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_CERT_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_hostname[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_HOST_NAME_1
    };

    RRQ61X_ATCMD_CONF_STR tlsc_nvr_peer_ipaddr[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_0,
        RRQ61X_CONF_STR_ATCMD_TLSC_PEER_IPADDR_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_auth_mode[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_AUTH_MODE_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_incoming_len[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_INCOMING_LEN_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_outgoing_len[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_OUTGOING_LEN_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_local_port[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_LOCAL_PORT_1,
    };

    RRQ61X_ATCMD_CONF_INT tlsc_nvr_peer_port[ATCMD_TLSC_MAX_ALLOW_NVR_PROFILE] =
    {
        RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_0,
        RRQ61X_CONF_INT_ATCMD_TLSC_PEER_PORT_1,
    };

    atcmd_tlsc_config * tlsc_conf = NULL;

    int int_value = -1;

    tlsc_conf = &(ctx->conf.tlsc_conf);

    /* Get ca cert name */
    get_atcmd_param_str(tlsc_nvr_ca_cert_name[profile_idx], tlsc_conf->ca_cert_name, ATCMD_CM_MAX_NAME);

    /* Get cert name */
    get_atcmd_param_str(tlsc_nvr_cert_name[profile_idx], tlsc_conf->cert_name, ATCMD_CM_MAX_NAME);

    /* Get hostname */
    get_atcmd_param_str(tlsc_nvr_hostname[profile_idx], tlsc_conf->hostname, ATCMD_TLSC_MAX_HOSTNAME);

    /* Get peer IP address */
    get_atcmd_param_str(tlsc_nvr_peer_ipaddr[profile_idx], tlsc_conf->svr_addr, ATCMD_TLSC_MAX_ADDRSTRLEN);

    /* Get auth mode */
    int_value = -1;
    status    = get_atcmd_param_int(tlsc_nvr_auth_mode[profile_idx], &int_value);

    if ((int_value == pdTRUE) || (int_value == pdFALSE))
    {
        tlsc_conf->auth_mode = int_value;
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get data(%d) to nvram(%d)\n", tlsc_nvr_auth_mode[profile_idx], status);
    }

    /* Get incoming_buflen */
    int_value = -1;
    status    = get_atcmd_param_int(tlsc_nvr_incoming_len[profile_idx], &int_value);

    if (int_value >= ATCMD_TLSC_MIN_INCOMING_LEN)
    {
        tlsc_conf->incoming_buflen = int_value;
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get data(%d) to nvram(%d)\n", tlsc_nvr_incoming_len[profile_idx],
                                     status);
    }

    /* Get outgoing_buflen */
    int_value = -1;
    status    = get_atcmd_param_int(tlsc_nvr_outgoing_len[profile_idx], &int_value);

    if (int_value >= ATCMD_TLSC_MIN_OUTGOING_LEN)
    {
        tlsc_conf->outgoing_buflen = int_value;
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get data(%d) to nvram(%d)\n", tlsc_nvr_outgoing_len[profile_idx],
                                     status);
    }

    /* Get local port */
    int_value = -1;
    status    = get_atcmd_param_int(tlsc_nvr_local_port[profile_idx], &int_value);

    if (int_value >= 0)
    {
        tlsc_conf->local_port = int_value;
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("fAiled to get data(%d) to nvram(%d)\n", tlsc_nvr_local_port[profile_idx], status);
    }

    /* Get peer port */
    int_value = -1;
    status    = get_atcmd_param_int(tlsc_nvr_peer_port[profile_idx], &int_value);

    if (int_value > 0)
    {
        short int s_value = (short int) int_value;
        sprintf(tlsc_conf->svr_port, "%d", s_value);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get data(%d) to nvram(%d)\n", tlsc_nvr_peer_port[profile_idx], status);
        status = RRQ_APP_NOT_SUCCESSFUL;
        goto end;
    }

end:

    return status;
}

static void atcmd_transport_ssl_recover_context_dpm (void)
{
    unsigned int len = 0;
    int          idx = 0;

    atcmd_tls_context * prev_ctx = NULL;
    atcmd_tls_context * new_ctx  = NULL;
    atcmd_tls_context * src_ctx  = NULL;

    /* Only supported in DPM */
    if (!RM_PMGR_W_dpm_is_enabled())
    {
        return;
    }

    /* Already existed */
    if (atcmd_tls_ctx_header)
    {
        return;
    }

    if (!atcmd_tls_ctx_rtm_header)
    {
        if (RM_PMGR_W_dpm_is_enabled() == pdFALSE)
        {
            return;
        }

        len = RM_PMGR_W_user_rtm_get(ATCMD_TLS_DPM_CONTEXT_NAME, (unsigned char **) &atcmd_tls_ctx_rtm_header);

        if (len == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not found tls context\n");

            return;
        }
    }

    src_ctx = atcmd_tls_ctx_rtm_header;

    while ((idx < ATCMD_TLS_MAX_ALLOW_CNT) && (src_ctx->role != ATCMD_TLS_NONE))
    {
        /* Create context */
        new_ctx = pvPortMalloc(sizeof(atcmd_tls_context));

        if (!new_ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create atcmd_tls_context(%d)\n", sizeof(atcmd_tls_context));

            return;
        }

        memset(new_ctx, 0x00, sizeof(atcmd_tls_context));

        /* Copy CID & Role*/
        new_ctx->cid  = src_ctx->cid;
        new_ctx->role = src_ctx->role;

        RM_ATCMD_W_CORE_SOCKET_DEBUG("#%d. cid(%d), role(%d)\n", idx, new_ctx->cid, new_ctx->role);

        if (new_ctx->role == ATCMD_TLS_CLIENT)
        {
            new_ctx->ctx.tlsc_ctx = NULL;

            /* Copy configuration */
            memcpy(&new_ctx->conf.tlsc_conf, &src_ctx->conf.tlsc_conf, sizeof(atcmd_tlsc_config));
        }

        /* Consturct linked-list */
        if (prev_ctx)
        {
            prev_ctx->next = new_ctx;
        }
        else
        {
            atcmd_tls_ctx_header = new_ctx;
        }

        prev_ctx = new_ctx;
        src_ctx++;
        idx++;
    }

    ;

 #if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("Recovery CID:\n");

    for (new_ctx = atcmd_tls_ctx_header; new_ctx != NULL; new_ctx = new_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t* CID: %d\n", new_ctx->cid);
    }
 #endif                                // DEBUG_ATCMD
}

static void atcmd_transport_ssl_update_context_dpm (void)
{
    const unsigned long wait_option = 100;

    int          status = RRQ_APP_SUCCESS;
    unsigned int len    = 0;

    atcmd_tls_context * dst_ctx = NULL;
    atcmd_tls_context * src_ctx = NULL;

    if (!RM_PMGR_W_dpm_is_enabled())
    {
        return;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("sizeof(atcmd_tls_context):%d * %d = %d\n",
                                 sizeof(atcmd_tls_context),
                                 ATCMD_TLS_MAX_ALLOW_CNT,
                                 sizeof(atcmd_tls_context) * ATCMD_TLS_MAX_ALLOW_CNT);

    if (!atcmd_tls_ctx_header)
    {
        status = RM_PMGR_W_user_rtm_free(ATCMD_TLS_DPM_CONTEXT_NAME);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to free rtm memory for tls context(0x%x)\n", status);

            return;
        }

        RM_ATCMD_W_CORE_SOCKET_DEBUG("Clear CID(DPM)\n");

        atcmd_tls_ctx_rtm_header = NULL;

        return;
    }

    if (!atcmd_tls_ctx_rtm_header)
    {
        len = RM_PMGR_W_user_rtm_get(ATCMD_TLS_DPM_CONTEXT_NAME, (unsigned char **) &atcmd_tls_ctx_rtm_header);

        if (len == 0)
        {
            status = RM_PMGR_W_user_rtm_pool_alloc(ATCMD_TLS_DPM_CONTEXT_NAME,
                                                   (void **) &atcmd_tls_ctx_rtm_header,
                                                   sizeof(atcmd_tls_context) * ATCMD_TLS_MAX_ALLOW_CNT,
                                                   wait_option);

            if (status)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to allocate rtm memory for tls context(0x%x)\n", status);

                return;
            }
        }
    }

    /* Init */
    memset(atcmd_tls_ctx_rtm_header, 0x00, sizeof(atcmd_tls_context) * ATCMD_TLS_MAX_ALLOW_CNT);

    for (int cnt = 0; cnt < ATCMD_TLS_MAX_ALLOW_CNT; cnt++)
    {
        atcmd_tls_ctx_rtm_header[cnt].cid  = -1;
        atcmd_tls_ctx_rtm_header[cnt].role = ATCMD_TLS_NONE;
    }

    /* Update */
    dst_ctx = atcmd_tls_ctx_rtm_header;

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Update CID(DPM):\n");

    for (src_ctx = atcmd_tls_ctx_header; src_ctx != NULL; src_ctx = src_ctx->next)
    {
        memcpy(dst_ctx, src_ctx, sizeof(atcmd_tls_context));
        RM_ATCMD_W_CORE_SOCKET_ERROR("\t* CID: %d\n", dst_ctx->cid);
        dst_ctx++;
    }
}

#endif                                 /* CFG_PMGR */

static atcmd_tls_context * atcmd_transport_ssl_create_context (void)
{
    atcmd_tls_context * new_ctx  = NULL;
    atcmd_tls_context * cur_ctx  = NULL;
    atcmd_tls_context * prev_ctx = NULL;

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("Before CID:\n");

    for (cur_ctx = atcmd_tls_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("\t* CID: %d\n", cur_ctx->cid);
    }
#endif                                 // DEBUG_ATCMD

    new_ctx = pvPortMalloc(sizeof(atcmd_tls_context));

    if (!new_ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create atcmd_tls_context(%d)\n", sizeof(atcmd_tls_context));

        return NULL;
    }

    memset(new_ctx, 0x00, sizeof(atcmd_tls_context));

    if (!atcmd_tls_ctx_header)
    {
        new_ctx->cid         = 0;
        atcmd_tls_ctx_header = new_ctx;
    }
    else if (atcmd_tls_ctx_header->cid >= 1)
    {
        new_ctx->cid         = 0;
        new_ctx->next        = atcmd_tls_ctx_header;
        atcmd_tls_ctx_header = new_ctx;
    }
    else
    {
        prev_ctx = atcmd_tls_ctx_header;
        cur_ctx  = atcmd_tls_ctx_header->next;

        while (cur_ctx)
        {
            if (prev_ctx->cid + 1 < cur_ctx->cid)
            {
                new_ctx->cid   = prev_ctx->cid + 1;
                prev_ctx->next = new_ctx;
                new_ctx->next  = cur_ctx;
                break;
            }

            prev_ctx = cur_ctx;
            cur_ctx  = cur_ctx->next;
        }

        if (!cur_ctx)
        {
            if (prev_ctx->cid + 1 >= ATCMD_TLS_MAX_ALLOW_CNT)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Over max count(%d)\n", ATCMD_TLS_MAX_ALLOW_CNT);

                vPortFree(new_ctx);

                return NULL;
            }

            new_ctx->cid   = prev_ctx->cid + 1;
            prev_ctx->next = new_ctx;
        }
    }

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("After CID:\n");

    for (cur_ctx = atcmd_tls_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("\t* CID: %d\n", cur_ctx->cid);
    }
#endif                                 // DEBUG_ATCMD

    return new_ctx;
}

static atcmd_tls_context * atcmd_transport_ssl_find_context (int cid)
{
    atcmd_tls_context * cur_ctx = NULL;

    for (cur_ctx = atcmd_tls_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        if (cur_ctx->cid == cid)
        {
            RM_ATCMD_W_CORE_SOCKET_DEBUG("Found cid(%d)\n", cid);

            return cur_ctx;
        }
    }

    return NULL;
}

static int atcmd_transport_ssl_delete_context (int cid)
{
    atcmd_tls_context * cur_ctx  = NULL;
    atcmd_tls_context * prev_ctx = NULL;

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("Before CID:\n");

    for (cur_ctx = atcmd_tls_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("\t* CID: %d\n", cur_ctx->cid);
    }
#endif                                 // DEBUG_ATCMD

    if (atcmd_tls_ctx_header == NULL)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("There is no tls context\n");

        return -1;
    }
    else if (atcmd_tls_ctx_header->cid == cid)
    {
        cur_ctx              = atcmd_tls_ctx_header;
        atcmd_tls_ctx_header = atcmd_tls_ctx_header->next;
        vPortFree(cur_ctx);
    }
    else
    {
        prev_ctx = atcmd_tls_ctx_header;
        cur_ctx  = atcmd_tls_ctx_header->next;

        while (cur_ctx)
        {
            if (cur_ctx->cid == cid)
            {
                prev_ctx->next = cur_ctx->next;
                vPortFree(cur_ctx);
                break;
            }

            prev_ctx = cur_ctx;
            cur_ctx  = cur_ctx->next;
        }
    }

#if CFG_PMGR
    atcmd_transport_ssl_update_context_dpm();
#endif                                 /* CFG_PMGR */

#if defined(DEBUG_ATCMD)
    RM_ATCMD_W_CORE_SOCKET_DEBUG("After CID:\n");

    for (cur_ctx = atcmd_tls_ctx_header; cur_ctx != NULL; cur_ctx = cur_ctx->next)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("\t* CID: %d\n", cur_ctx->cid);
    }
#endif                                 // DEBUG_ATCMD

    return 0;
}

/**************************************************************************/
/* Related with command of TLS client/server                              */
/**************************************************************************/
static int atcmd_transport_ssl_do_init (int role, int * cid)
{
    int status = RRQ_APP_SUCCESS;

    if (role == ATCMD_TLS_SERVER)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("Not implemented yet\n");
        status = RRQ_APP_NOT_IMPLEMENTED;
        goto ssl_do_init_end;
    }
    else if (role == ATCMD_TLS_CLIENT)
    {
        if (atcmd_transport_get_available_session() == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tcp server (Max Session Number = %d)\n",
                                         atcmd_transport_get_max_session());
            status = RRQ_APP_SSL_SOCKET_CREATE_FAIL;
            goto ssl_do_init_end;
        }

        status = atcmd_transport_ssl_create_tls_client(cid);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tls client(0x%x)\n", status);
            goto ssl_do_init_end;
        }
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Role(%d), CID(%d)\n", role, *cid);

ssl_do_init_end:

    return status;
}

static int atcmd_transport_ssl_do_cfg (char * cid_str, char * conf_id_str, char * conf_val_str)
{
    int status = RRQ_APP_SUCCESS;

    int cid     = -1;
    int conf_id = 0;

    atcmd_tls_context * ctx       = NULL;
    atcmd_tlsc_config * tlsc_conf = NULL;

    int auth_mode   = 0;
    int buflen      = 0;
    int tls_version = 0;

    RA6W1_UNUSED_ARG(conf_id_str);

    /* Check cid */
    if (rm_atcmd_w_core_common_stoi(cid_str, &cid, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get CID(%s)\n", cid_str);
        status = RRQ_APP_PARAMETER_ERROR;
        goto end;
    }

    ctx = atcmd_transport_ssl_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found(cid:%d)\n", cid);
        status = RRQ_APP_NOT_FOUND;
        goto end;
    }

    if (ctx->role == ATCMD_TLS_CLIENT)
    {
        if (ctx->ctx.tlsc_ctx->state != ATCMD_TLSC_STATE_TERMINATED)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("TLS client is not terminated\n");
            status = RRQ_APP_ALREADY_ENABLED;
            goto end;
        }

        tlsc_conf = &(ctx->conf.tlsc_conf);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented(%d)\n", ctx->role);
        status = RRQ_APP_NOT_IMPLEMENTED;
        goto end;
    }

    /* Check configuration ID */
    if (rm_atcmd_w_core_common_stoi(conf_id_str, &conf_id, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get configuration ID(%s)\n", conf_id_str);
        status = RRQ_APP_NOT_SUCCESSFUL;
        goto end;
    }

    /* Check configuration value */
    switch (conf_id)
    {
        case 1:                        /* TLS version */
        {
            if (rm_atcmd_w_core_common_stoi(conf_val_str, &tls_version, POL_1) != 0)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get TLS version\n");
                status = RRQ_APP_NOT_SUCCESSFUL;
                break;
            }

            if (rm_atcmd_w_core_common_is_in_valid_range(tls_version, 0, 2) == pdFALSE)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Incorrect TLS version.\n");
                status = RRQ_APP_SSL_CFG_TLS_VER;
                break;
            }

            if (tls_version == 0)
            {
                tlsc_conf->tls_ver = ONLY_TLS12;
            }
            else if (tls_version == 1)
            {
                tlsc_conf->tls_ver = ONLY_TLS13;
            }
            else if (tls_version == 2)
            {
                tlsc_conf->tls_ver = TLS12_13;
            }

            break;
        }

        case 2:                        /* SSL CA certificate name */
        {
            if (atcmd_cm_is_exist_cert(conf_val_str, ATCMD_CM_CERT_TYPE_CA_CERT))
            {
                if (!atcmd_cm_is_exist_cert_with_seq(conf_val_str, ATCMD_CM_CERT_TYPE_CA_CERT, 0))
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Not found certificate name(%s) with seqeunce 0\n", conf_val_str);
                    status = RRQ_APP_SSL_CFG_CA_CERT_NO_ZERO_SEQ;
                }
                else
                {
                    if (ctx->role == ATCMD_TLS_CLIENT) // tls_client
                    {
                        bsp_safe_strcpy(tlsc_conf->ca_cert_name, conf_val_str, ATCMD_CM_MAX_NAME);
                    }
                    else
                    {
                        status = RRQ_APP_NOT_IMPLEMENTED;
                    }
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Not found certificate name(%s)\n", conf_val_str);
                status = RRQ_APP_SSL_CFG_CA_CERT_NO_NAME;
            }

            break;
        }

        case 3:                        /* SSL certificate name */
        {
            if (atcmd_cm_is_exist_cert_with_seq(conf_val_str, ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_CERT))
            {
                if (atcmd_cm_is_exist_cert_with_seq(conf_val_str, ATCMD_CM_CERT_TYPE_CERT, ATCMD_CM_CERT_SEQ_KEY))
                {
                    if (ctx->role == ATCMD_TLS_CLIENT) // tls_client
                    {
                        bsp_safe_strcpy(tlsc_conf->cert_name, conf_val_str, ATCMD_CM_MAX_NAME);
                    }
                    else
                    {
                        status = RRQ_APP_NOT_IMPLEMENTED;
                    }
                }
                else
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Not found private key(%s)\n", conf_val_str);
                    status = RRQ_APP_SSL_CFG_CERT_NO_KEY;
                }
            }
            else
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Not found certificate(%s)\n", conf_val_str);
                status = RRQ_APP_SSL_CFG_CERT_NO_CERT;
            }

            break;
        }

        case 4:                        /* Cipher value bitmap where the its values. */
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented to set cipher value\n");
            status = RRQ_APP_NOT_IMPLEMENTED;
            break;
        }

        case 5:                        /* Set the Tx Max fragment length */
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not supported to set Tx Max fragment length\n");
            status = RRQ_APP_NOT_SUPPORTED;
            break;
        }

        case 6:                        /* Set the SNI */
        {
            if (ctx->role == ATCMD_TLS_CLIENT)
            {
                status = atcmd_tlsc_set_hostname(tlsc_conf, conf_val_str);

                if (status)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set hostname(0x%x)\n", status);
                    status = RRQ_APP_SSL_CFG_SET_SNI;
                }
            }
            else
            {
                status = RRQ_APP_SSL_ROLE_NOT_SUPPORT;
            }

            break;
        }

        case 7:                        /* Set the Domain */
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented to set Domain\n");
            status = RRQ_APP_NOT_IMPLEMENTED;
            break;
        }

        case 8:                        /* Set the Max Fragment Length */
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented to set Max Fragment length\n");
            status = RRQ_APP_NOT_IMPLEMENTED;
            break;
        }

        case 9:                        /* To enable/disable server validation */
        {
            if (rm_atcmd_w_core_common_stoi(conf_val_str, &auth_mode, POL_1) != 0)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get Auth mode(%s)\n", conf_val_str);
                status = RRQ_APP_SSL_SVR_VAILD_TYPE;
                break;
            }

            if (rm_atcmd_w_core_common_is_in_valid_range(auth_mode, 0, 1) == pdFALSE)
            {
                status = RRQ_APP_SSL_SVR_VAILD_RANGE;
                break;
            }

            /* tls_client */
            if (ctx->role == ATCMD_TLS_CLIENT)
            {
                tlsc_conf->auth_mode = auth_mode == 0 ? pdFALSE : pdTRUE;
            }
            else
            {
                status = RRQ_APP_SSL_ROLE_NOT_SUPPORT;
            }

            break;
        }

        case 10:                       /* Set incoming buffer size */
        {
            if (rm_atcmd_w_core_common_stoi(conf_val_str, &buflen, POL_1) != 0)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get incoming buflen(%s)\n", conf_val_str);
                status = RRQ_APP_SSL_RX_BUF_LENTH;
                break;
            }

            /* tls_client */
            if (ctx->role == ATCMD_TLS_CLIENT)
            {
                status = atcmd_tlsc_set_incoming_buflen(tlsc_conf, buflen);

                if (status)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set incoming buflen(0x%x,%d)\n", status, buflen);
                    break;
                }
            }
            else
            {
                status = RRQ_APP_SSL_ROLE_NOT_SUPPORT;
            }

            break;
        }

        case 11:                       /* Set outgoing buffer size */
        {
            if (rm_atcmd_w_core_common_stoi(conf_val_str, &buflen, POL_1) != 0)
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get outgoing buflen(%s)\n", conf_val_str);
                status = RRQ_APP_SSL_TX_BUF_LENTH;
                break;
            }

            /* tls_client */
            if (ctx->role == ATCMD_TLS_CLIENT)
            {
                atcmd_tlsc_set_outgoing_buflen(tlsc_conf, buflen);

                if (status)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set outgoing buflen(0x%x,%d)\n", status, buflen);
                    break;
                }
            }
            else
            {
                status = RRQ_APP_SSL_ROLE_NOT_SUPPORT;
            }

            break;
        }

        default:
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid Configuration ID(%d)\n", conf_id);
            status = RRQ_APP_SSL_CFG_CONF_ID_NOT_SUPPORT;
            break;
        }
    }

end:

    return status;
}

static int atcmd_transport_ssl_do_co (atcmd_w_ctrl_t * const p_at_ctrl, char * cid_str, char * ip_str, char * port_str)
{
    int status = RRQ_APP_SUCCESS;
    int cid    = -1;

    atcmd_tls_context  * ctx       = NULL;
    atcmd_tlsc_context * tlsc_ctx  = NULL;
    atcmd_tlsc_config  * tlsc_conf = NULL;

    /* Check cid */
    if (rm_atcmd_w_core_common_stoi(cid_str, &cid, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("failed to get CID(%s)\n", cid_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    ctx = atcmd_transport_ssl_find_context(cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found(cid:%d)\n", cid);

        return RRQ_APP_NOT_FOUND;
    }
    else if (ctx->role != ATCMD_TLS_CLIENT)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented yet(cid:%d)\n", cid);

        return RRQ_APP_NOT_IMPLEMENTED;
    }

    tlsc_ctx  = ctx->ctx.tlsc_ctx;
    tlsc_conf = &(ctx->conf.tlsc_conf);

    /* Check tls client' state */
    if (tlsc_ctx->state != ATCMD_TLSC_STATE_TERMINATED)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("TLS Client state is not terminated(%d)\n", tlsc_ctx->state);

        return RRQ_APP_CANNOT_START;
    }

    if (rm_atcmd_w_core_common_stoi(port_str, &status, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get port(%s)\n", port_str);

        return RRQ_APP_SSL_CONN_INVALID_PORT_NUM;
    }

    strncpy(tlsc_conf->svr_addr, ip_str, sizeof(tlsc_conf->svr_addr) - 1);

    strncpy(tlsc_conf->svr_port, port_str, sizeof(tlsc_conf->svr_port) - 1);

    if (ra6w1_network_main_check_net_init(WLAN0_IFACE))
    {
        tlsc_conf->iface = WLAN0_IFACE;
    }
    else if (ra6w1_network_main_check_net_init(WLAN1_IFACE))
    {
        tlsc_conf->iface = WLAN1_IFACE;
    }

    atcmd_tlsc_set_at_ctrl(tlsc_ctx, p_at_ctrl);

    status = atcmd_tlsc_setup_config(tlsc_ctx, tlsc_conf);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to setup tls client's config(0x%x)\n", status);

        status = RRQ_APP_SSL_CONN_SETUP_CFG_ERR;

        return status;
    }

    status = atcmd_transport_ssl_run_tls_client(ctx);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to run tls client(%d)\n", -status);
        status = RRQ_APP_SSL_CONN_TLS_CLIENT_RUN_ERR;

        return status;
    }

    return status;
}

static int atcmd_transport_ssl_do_cl (char * cid_str, int * cid)
{
    int status = RRQ_APP_SUCCESS;

    atcmd_tls_context * ctx = NULL;

    *cid = -1;

    if (rm_atcmd_w_core_common_stoi(cid_str, cid, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get CID(%s)\n", cid_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    ctx = atcmd_transport_ssl_find_context(*cid);

    if (!ctx)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found(cid:%d)\n", *cid);

        return RRQ_APP_NOT_FOUND;
    }

    if (ctx->role == ATCMD_TLS_CLIENT)
    {
        status = atcmd_transport_ssl_delete_tls_client(*cid);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to close tls client(%d, 0x%x)\n", *cid, status);
        }
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not implemented yet\n");

        return RRQ_APP_NOT_IMPLEMENTED;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("CID(%d)\n", *cid);

    return RRQ_APP_SUCCESS;
}

static int atcmd_transport_ssl_do_certlist (char * type_str, char * out, int outlen)
{
    int idx  = 0;
    int type = -1;
    const atcmd_cm_cert_info_t * cert_info;

    char * cert_names = out;
    char   cert_name[ATCMD_CM_MAX_NAME + 10] = {0x00, };
    char   first_flag = 1;

    if (rm_atcmd_w_core_common_stoi(type_str, &type, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get type(%s)\n", type_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    memset(out, 0x00, outlen);

    cert_info = atcmd_cm_get_cert_info();

    if (cert_info)
    {
        for (idx = 0; idx < ATCMD_CM_MAX_CERT_NUM; idx++)
        {
            if ((strlen(cert_info[idx].name) > 0) && (cert_info[idx].type == type))
            {
                if (count_int_len((int) cert_info[idx].type) != 1)
                {
                    return RRQ_APP_INVALID_PARAMETERS;
                }

                if (type == ATCMD_CM_CERT_TYPE_CA_CERT)
                {
                    sprintf(cert_name, "%s%d,%s", first_flag ? "" : ",", cert_info[idx].type, cert_info[idx].name);
                }
                else if ((type == ATCMD_CM_CERT_TYPE_CERT) && (cert_info[idx].seq == ATCMD_CM_CERT_SEQ_CERT))
                {
                    sprintf(cert_name,
                            "%s%d,%s,%s",
                            first_flag ? "" : ",",
                            cert_info[idx].type,
                            cert_info[idx].name,
                            "CERT");
                }
                else if ((type == ATCMD_CM_CERT_TYPE_CERT) && (cert_info[idx].seq == ATCMD_CM_CERT_SEQ_KEY))
                {
                    sprintf(cert_name,
                            "%s%d,%s,%s",
                            first_flag ? "" : ",",
                            cert_info[idx].type,
                            cert_info[idx].name,
                            "KEY");
                }

                snprintf(cert_names + strlen(cert_names), (outlen - strlen(cert_names) - 1), "%s", cert_name);
                first_flag = 0;
            }
        }
    }

    return RRQ_APP_SUCCESS;
}

static int atcmd_transport_ssl_do_certdelete (char * type_str, char * name_str)
{
    int status = RRQ_APP_SUCCESS;
    int type   = -1;

    if (rm_atcmd_w_core_common_stoi(type_str, &type, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get type(%s)\n", type_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    status = atcmd_cm_clear_cert(name_str, type);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to clear certificate(%s,%d,0x%x)\n", name_str, type, status);

        return status;
    }

    return status;
}

static int atcmd_transport_ssl_do_save (void)
{
    int status = RRQ_APP_SUCCESS;

    atcmd_tls_context * ctx         = NULL;
    int                 tls_nvr_idx = 0;

    if (!atcmd_tls_ctx_header)
    {
        RM_ATCMD_W_CORE_SOCKET_DEBUG("Empty CID\n");

        status = atcmd_transport_ssl_clear_all_context_nvram();

        if (status != RRQ_APP_SUCCESS)
        {
            status = RRQ_APP_SSL_CLR_CTX_NVRAM_FAIL;
        }

        return status;
    }

    for (ctx = atcmd_tls_ctx_header; ctx != NULL; ctx = ctx->next)
    {
        if (tls_nvr_idx > ATCMD_TLS_MAX_ALLOW_CNT)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not able to save context(cid:%d, role:%d)\n", ctx->cid, ctx->role);
            break;
        }

        status = atcmd_transport_ssl_save_context_nvram(ctx);

        if (status)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to save context(0x%x)\n", status);
            continue;
        }

        tls_nvr_idx++;
    }

    if (status != RRQ_APP_SUCCESS)
    {
        status = RRQ_APP_SSL_SAVE_CTX_NVRAM_FAIL;
    }

    return status;
}

static int atcmd_transport_ssl_do_delete (void)
{
    return atcmd_transport_ssl_clear_all_context_nvram();
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_read_trsslwr_cmd (atcmd_w_ctrl_t * const p_at_ctrl,
                                                                       char                 * p_atcmd,
                                                                       size_t                 atcmd_len,
                                                                       char                ** pp_data,
                                                                       size_t               * p_data_len)
{
    typedef enum
    {
        READ_CID,
        READ_REMOTE_IP,
        READ_REMOTE_PORT,
        READ_MODE,
        READ_DATA_LENGTH,
        READ_DATA
    } atcmd_tls_cmd_parameter_step;

    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err = FSP_SUCCESS;

    const char * r_mode_str = "r";
    const char * t_mode_str = "t";

    char * p_data    = NULL;
    int    data_idx  = 0;
    int    atcmd_idx = 0;

    const int    max_param_atcmd_len = 32;
    char         param_atcmd[32]     = {0x00, };
    unsigned int param_atcmd_idx     = 0;

    char ch = '\0';
    atcmd_tls_cmd_parameter_step param_step = READ_CID;
    int data_len = 0;
    int r_mode   = pdFALSE;
    int port     = 0;

    atcmd_idx += strlen(RM_ATCMD_W_CORE_SOCKET_ATCMD_CODE(TRSSLWR));

    /* Read '=' */
    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

    if (fsp_err == FSP_SUCCESS)
    {
        if (ch != '=')
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        goto end;
    }

    *(p_atcmd + atcmd_idx) = ch;
    atcmd_idx++;

    /* Read body */
    while (1)
    {
        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

        if (fsp_err == FSP_SUCCESS)
        {
            if (atcmd_idx >= (int) (atcmd_len - 2))
            {
                RM_ATCMD_W_CORE_SOCKET_ERROR("overflow(%d >= %d)\n", atcmd_idx, (atcmd_len - 2));
                err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                break;
            }

            if (param_step == READ_DATA)
            {
                RM_ATCMD_W_CORE_SOCKET_DEBUG("Read data with %s mode(%d)\n", (r_mode ? "R" : "T"), data_len);

                if (data_len <= 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                    break;
                }

                if (!p_data)
                {
                    p_data = pvPortMalloc(data_len + 1);

                    if (!p_data)
                    {
                        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                        break;
                    }

                    memset(p_data, 0x00, (data_len + 1));
                    data_idx = 0;
                }

                if (r_mode)
                {
                    p_data[0] = ch;

                    if (data_len > 1)
                    {
                        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) (p_data + 1), (data_len - 1));

                        if (fsp_err != FSP_SUCCESS)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            goto end;
                        }
                    }

                    break;
                }
                else
                {
                    if (ch == '\b')
                    {
                        if (data_idx > 0)
                        {
                            data_idx--;
                            p_data[data_idx] = '\0';
                        }
                    }
                    else if (data_len == (data_idx + 1))
                    {
                        p_data[data_idx++] = ch;

                        if ((ch == '\n') || (ch == '\r'))
                        {
                            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) AT_CMD_ENTER_NEW_LINE, 2);
                        }

                        break;
                    }
                    else if (ch == 0x0d)
                    {
                        p_data[data_idx++] = 0x0a;
                    }
                    else if ((ch == 0x03) || (ch == 0x1a))
                    {
                        p_data[data_idx++] = '\0';
                        break;
                    }
                    else
                    {
                        p_data[data_idx++] = ch;
                    }
                }
            }
            else
            {
                if (param_atcmd_idx > max_param_atcmd_len)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("overflow(%d > %d)\n", param_atcmd_idx, sizeof(param_atcmd));
                    err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                    break;
                }

                if (ch != 0x2C)
                {
                    /* Added input */
                    param_atcmd[param_atcmd_idx++] = ch;
                }
                else
                {
                    /* Comma */
                    RM_ATCMD_W_CORE_SOCKET_DEBUG("param_step(%d)\n", param_step);

                    if (param_step == READ_CID)
                    {
                        /* CID */
                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);

                        param_step = READ_REMOTE_IP;
                    }
                    else if (param_step == READ_REMOTE_IP)
                    {
                        /* Remote IP address */
                        int tmp = 0;

                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &tmp, POL_1) == 0)
                        {
                            param_step = READ_DATA_LENGTH;
                            goto read_data_length;
                        }
                        else if (strlen(param_atcmd) == 1)
                        {
                            param_step = READ_MODE;
                            goto read_mode;
                        }

                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);

                        param_step = READ_REMOTE_PORT;
                    }
                    else if (param_step == READ_REMOTE_PORT)
                    {
                        /* Remote Port */
                        if ((rm_atcmd_w_core_common_stoi(param_atcmd, &port, POL_1) != 0) ||
                            (port < 0) || (port > 0xFFFF))
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }

                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);

                        param_step = READ_MODE;
                    }
                    else if (param_step == READ_MODE)
                    {
                        /* Mode */
read_mode:
                        param_step = READ_DATA_LENGTH;

                        if (strcasecmp(param_atcmd, r_mode_str) == 0)
                        {
                            r_mode = pdTRUE;
                        }
                        else if (strcasecmp(param_atcmd, t_mode_str) == 0)
                        {
                            r_mode = pdFALSE;
                        }
                        else
                        {
                            goto read_data_length;
                        }
                    }
                    else if (param_step == READ_DATA_LENGTH)
                    {
                        /* Data Length */
read_data_length:

                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &data_len, POL_1) != 0)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }

                        sprintf((p_atcmd + atcmd_idx), "%s,", param_atcmd);
                        atcmd_idx += (param_atcmd_idx + 1);

                        param_step = READ_DATA;
                    }

                    memset(param_atcmd, 0x00, sizeof(param_atcmd));
                    param_atcmd_idx = 0;
                }
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            break;
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        *pp_data    = p_data;
        *p_data_len = data_len;
    }
    else
    {
        vPortFree(p_data);
        p_data = NULL;

        *pp_data    = NULL;
        *p_data_len = 0;
    }

    return err;
}

static int atcmd_transport_ssl_send_tls_client (int cid, char * dst_ip, char * dst_port, char * data, size_t * datalen)
{
    int status = RRQ_APP_SUCCESS;

    atcmd_tls_context  * ctx       = NULL;
    atcmd_tlsc_context * tlsc_ctx  = NULL;
    atcmd_tlsc_config  * tlsc_conf = NULL;

    ctx = atcmd_transport_ssl_find_context(cid);

    if (!ctx || (ctx->role != ATCMD_TLS_CLIENT))
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not found(cid:%d)\n", cid);

        return RRQ_APP_NOT_FOUND;
    }

    tlsc_ctx  = ctx->ctx.tlsc_ctx;
    tlsc_conf = &(ctx->conf.tlsc_conf);

    /* Check state of tls session */
    if (tlsc_ctx->state != ATCMD_TLSC_STATE_CONNECTED)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Not connected(%d)\n", tlsc_ctx->state);

        return RRQ_APP_NOT_CONNECTED;
    }

    if (dst_ip)
    {
        /* Check ip address & port */
        if ((strcmp(tlsc_conf->svr_addr, dst_ip) != 0) ||
            (strcmp(tlsc_conf->svr_port, dst_port) != 0))
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not matched tls server info(%s:%s vs %s:%s)\n",
                                         tlsc_conf->svr_addr,
                                         tlsc_conf->svr_port,
                                         dst_ip,
                                         dst_port);

            return RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    status = atcmd_tlsc_write_data(tlsc_ctx, (unsigned char *) data, datalen);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to send data(0x%x)\n", status);
    }

    return status;
}

static int atcmd_transport_ssl_do_wr (char   * cid_str,
                                      char   * dst_ip_str,
                                      char   * dst_port_str,
                                      size_t * data_len,
                                      char   * p_data)
{
    int status = RRQ_APP_SUCCESS;

    int cid = -1;

    /* Check CID */
    if (rm_atcmd_w_core_common_stoi(cid_str, &cid, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get CID(%s)\n", cid_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    status = atcmd_transport_ssl_send_tls_client(cid, dst_ip_str, dst_port_str, p_data, data_len);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to send data(%d,%d,0x%x)\n", cid, *data_len, -status);
    }

    return status;
}

static int count_int_len (int val)
{
    int length = 0;

    if (val == 0)
    {
        return 1;
    }

    while (val != 0)
    {
        val /= 10;
        length++;
    }

    return length;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTS)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  ret               = 0;
    char result_str[32]    = {0, };
    int  ip_type           = 0;
    int  local_port        = 0;
    int  max_allowed_peers = 0;

    /* AT+TRTS=<ip_tpye>, <local_port>[,<max allowed peers>] */
    if ((argc == 3) || (argc == 4))
    {
        atcmd_sess_context * p_ctx = NULL;

        if (rm_atcmd_w_core_common_stoi(argv[1], &ip_type, POL_2) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[2], &local_port, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_TCP_SERVER_LOCAL_PORT_TYPE;
            goto end;
        }

#if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
        if (rm_atcmd_w_core_common_is_in_valid_range(ip_type, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

#elif defined(__SUPPORT_IPV4__)
        if (ip_type != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

#elif defined(__SUPPORT_IPV6__)
        if (ip_type != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }
#endif

        if (ip_type == 0)
        {
            ip_type = IPADDR_TYPE_V4;
        }
        else if (ip_type == 1)
        {
            ip_type = IPADDR_TYPE_V6;
        }

        if (argc == 4)
        {
            if (rm_atcmd_w_core_common_stoi(argv[3], &max_allowed_peers, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_TCP_SERVER_MAX_PEER_TYPE;
                goto end;
            }
        }

        if (atcmd_transport_get_available_session() == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tcp server (Max Session Number = %d)\n",
                                         atcmd_transport_get_max_session());
            err = FSP_ERR_AT_CMD_ERR_TCP_SERVER_TASK_CREATE;
            goto end;
        }

        /* Create context */
        p_ctx = atcmd_transport_create_context(ATCMD_SESS_TCP_SERVER);
        if (p_ctx == NULL)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tcp server context\n");
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto end;
        }

        ret = atcmd_network_connect_tcps(p_at_ctrl, p_ctx, local_port, max_allowed_peers, ip_type);
        if (ret == 0)
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            snprintf(result_str, sizeof(result_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2),
                     p_ctx->cid);

            /* Response CID */
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TCP_SERVER_TASK_CREATE;

            /* Delete context in failure */
            if (p_ctx)
            {
                atcmd_transport_delete_context(p_ctx->cid);
                p_ctx = NULL;
            }
        }
    }
    else
    {
        if (argc < 2)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTS)
{
    const char * p_usage = "<ip_type>, <local_port>[,<max allowed peers>]";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTS)
{
    const char * p_descrption = "Configure the local port number for TCP server";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTC)
{
    fsp_err_atcmd_err_code err          = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int                  ret            = 0;
    char                 result_str[32] = {0, };
    int                  peer_port      = 0;
    int                  local_port     = 0;
    atcmd_sess_context * p_ctx          = NULL;

    /* AT+TRTC=<server_ip>,<server_port>(,<local_port>) */
    if ((argc == 4) || (argc == 3))
    {
        if (rm_atcmd_w_core_common_stoi(argv[2], &peer_port, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_TCP_CLIENT_SVR_PORT_TYPE;
            goto end;
        }

        if ((argc == 4) && (rm_atcmd_w_core_common_stoi(argv[3], &local_port, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_TCP_CLIENT_LOCAL_PORT_TYPE;
            goto end;
        }

        if (atcmd_transport_get_available_session() == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tcp client (Max Session Number = %d)\n",
                                         atcmd_transport_get_max_session());
            err = FSP_ERR_AT_CMD_ERR_TCP_CLIENT_TASK_CREATE;
            goto end;
        }

        /* Create context */
        p_ctx = atcmd_transport_create_context(ATCMD_SESS_TCP_CLIENT);
        if (!p_ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create tcp client context\n");
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto end;
        }

        ret = atcmd_network_connect_tcpc(p_at_ctrl, p_ctx, argv[1], peer_port, (argc == 4 ? local_port : 0));
        if (ret == 0)
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            snprintf(result_str, sizeof(result_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2),
                     p_ctx->cid);

            /* Response CID */
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TCP_CLIENT_TASK_CREATE;

            /* Delete context in failure */
            if (p_ctx)
            {
                atcmd_transport_delete_context(p_ctx->cid);
                p_ctx = NULL;
            }
        }
    }
    else
    {
        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTC)
{
    const char * p_usage = "<svr_ip>,<svr_port>(,<local_port>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTC)
{
    const char * p_descrption = "Configure the information for TCP client";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRUSE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  ret            = 0;
    char result_str[32] = {0, };
    int  ip_type        = 0;
    int  port           = 0;

    /* AT+TRUSE=<ip_type>, <local_port> */
    if (argc == 3)
    {
        atcmd_sess_context * p_ctx = NULL;

        if (rm_atcmd_w_core_common_stoi(argv[1], &ip_type, POL_2) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[2], &port, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_UDP_SESS_LOCAL_PORT_TYPE;
            goto end;
        }

#if defined(__SUPPORT_IPV4__) && defined(__SUPPORT_IPV6__)
        if (rm_atcmd_w_core_common_is_in_valid_range(ip_type, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

#elif defined(__SUPPORT_IPV4__)
        if (ip_type != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

#elif defined(__SUPPORT_IPV6__)
        if (ip_type != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }
#endif

        if (ip_type == 0)
        {
            ip_type = IPADDR_TYPE_V4;
        }
        else if (ip_type == 1)
        {
            ip_type = IPADDR_TYPE_V6;
        }

        if (atcmd_transport_get_available_session() == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create udp session (Max Session Number = %d)\n",
                                         atcmd_transport_get_max_session());
            err = FSP_ERR_AT_CMD_ERR_UDP_SESS_TASK_CREATE;
            goto end;
        }

        /* Create context */
        p_ctx = atcmd_transport_create_context(ATCMD_SESS_UDP_SESSION);
        if (!p_ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create udp session context\n");
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto end;
        }

        ret = atcmd_network_connect_udps(p_at_ctrl, p_ctx, NULL, 0, port, ip_type);
        if (ret == 0)
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            snprintf(result_str, sizeof(result_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2),
                     p_ctx->cid);

            /* Response CID */
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_UDP_SESS_CONNECT;

            /* Delete context in failure */
            if (p_ctx)
            {
                atcmd_transport_delete_context(p_ctx->cid);
                p_ctx = NULL;
            }
        }
    }
    else
    {
        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRUSE)
{
    const char * p_usage = "<ip_type>,<local_port>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRUSE)
{
    const char * p_descrption = "Open UDP socket";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRUR)
{
    fsp_err_atcmd_err_code err          = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int                  ret            = 0;
    char                 result_str[32] = {0, };
    int                  ip_type        = 0;
    int                  peer_port      = 0;
    int                  local_port     = 0;
    atcmd_sess_context * p_ctx          = NULL;

    /* AT+TRUR=<remote_ip>,<remote_port>,[<local_port>] */
    if ((argc == 3) || (argc == 4))
    {
        if (rm_atcmd_w_core_common_stoi(argv[2], &peer_port, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_UDP_CID2_REMOTE_PORT_TYPE;
            goto end;
        }

        if (argc == 4)
        {
            if (rm_atcmd_w_core_common_stoi(argv[3], &local_port, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_UDP_SESS_LOCAL_PORT_TYPE;
                goto end;
            }
        }

        ip_type = rm_wifi_select_ipaddr_type_from_str(argv[1], NULL);

        /* Create context */
        if (atcmd_transport_get_available_session() == 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create udp session (Max Session Number = %d)\n",
                                         atcmd_transport_get_max_session());
            err = FSP_ERR_AT_CMD_ERR_UDP_SESS_TASK_CREATE;
            goto end;
        }

        p_ctx = atcmd_transport_create_context(ATCMD_SESS_UDP_SESSION);
        if (!p_ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to create udp sesion context\n");
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto end;
        }

        ret = atcmd_network_connect_udps(p_at_ctrl, p_ctx, argv[1], peer_port, local_port, ip_type);
        if (ret == 0)
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            snprintf(result_str, sizeof(result_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2),
                     p_ctx->cid);

            /* Response CID */
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_UDP_SESS_CONNECT;

            /* Delete context in failure */
            if (p_ctx)
            {
                atcmd_transport_delete_context(p_ctx->cid);
                p_ctx = NULL;
            }
        }
    }
    else
    {
        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRUR)
{
    const char * p_usage = "<remote_ip>,<remote_port>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRUR)
{
    const char * p_descrption = "Configure the IP_addr and port number of UDP client";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRPRT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int cid = -1;

    /* AT+TRPRT=<cid> */
    if (argc == 2)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &cid, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if (atcmd_network_display(p_at_ctrl, cid) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NO_CONNECTED_SESSION_EXIST;
        }
    }
    else
    {
        if (argc < 2)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRPRT)
{
    const char * p_usage = "<cid>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRPRT)
{
    const char * p_descrption = "Display Session Info. by CID";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRPALL)
{
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc > 1)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        if (atcmd_network_display(p_at_ctrl, 0xFF) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NO_CONNECTED_SESSION_EXIST;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRPALL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRPALL)
{
    const char * p_descrption = "Display all session";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTRM)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret  = 0;
    int port = 0;
    int cid  = -1;

    if (rm_atcmd_w_core_common_stoi(argv[1], &cid, POL_1) != 0)
    {
        err = FSP_ERR_AT_CMD_ERR_TRTRM_CID_TYPE;
        goto end;
    }

    /* AT_TRTRM=<cid> */
    if (argc == 2)
    {
        ret = atcmd_network_terminate_session(cid);

        if (ret)
        {
            switch (ret)
            {
                case -5:
                {
                    err = FSP_ERR_AT_CMD_ERR_NO_FOUND_REQ_CID_SESSION;
                    break;
                }

                case -6:
                {
                    err = FSP_ERR_AT_CMD_ERR_CONTEXT_CID_TYPE;
                    break;
                }

                case -7:
                {
                    err = FSP_ERR_AT_CMD_ERR_CONTEXT_DELETE;
                    break;
                }
            }

            goto end;
        }
    }
    else if (argc == 4)
    {
        /* AT+TRTRM=<cid>,<remote_ip>,<remote_port> */
        atcmd_sess_context * ctx = NULL;
        ctx = atcmd_transport_find_context(cid);

        if (!ctx)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Not found cid(%d)\n", cid);
            err = FSP_ERR_AT_CMD_ERR_NO_FOUND_REQ_CID_SESSION;
            goto end;
        }

        /* Allowed remote_ip and remote_port in TCP server */
        if (ctx->type != ATCMD_SESS_TCP_SERVER)
        {
            err = FSP_ERR_AT_CMD_ERR_CONTEXT_TYPE_IS_NOT_TCP_SVR;
            goto end;
        }

        ctx = NULL;

        /* Get port number */
        if (rm_atcmd_w_core_common_stoi(argv[3], &port, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_TRTRM_REMOTE_PORT_NUM_TYPE;
            goto end;
        }

        ret = atcmd_network_disconnect_tcps_cli(cid, argv[2], port);

        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_TRTRM_TCP_SVR_REMOTE_SESS_DISCON;
            goto end;
        }
    }
    else
    {
        if (argc == 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTRM)
{
    const char * p_usage = "<cid>[,<remote_ip>,<remote_port>]";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTRM)
{
    const char * p_descrption = "Close Session by CID.";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRTALL)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    if (argc > 1)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        ret = atcmd_network_terminate_session(0xFF);

        if (ret)
        {
            switch (ret)
            {
                case -1:
                {
                    err = FSP_ERR_AT_CMD_ERR_TCP_SERVER_TERMINATE;
                    break;
                }

                case -2:
                {
                    err = FSP_ERR_AT_CMD_ERR_TCP_CLIENT_TERMINATE;
                    break;
                }

                case -3:
                {
                    err = FSP_ERR_AT_CMD_ERR_UDP_SESSION_TERMINATE;
                    break;
                }

                case -4:
                {
                    err = FSP_ERR_AT_CMD_ERR_MULTI_SESSION_CID_TERMINATE;
                    break;
                }
            }
        }
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRTALL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRTALL)
{
    const char * p_descrption = "Close all session";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSAVE)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    if (argc > 1)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        if ((ret = atcmd_network_save_context_nvram()) != 0)
        {
            switch (ret)
            {
                case -1:
                {
                    err = FSP_ERR_AT_CMD_ERR_NO_SESSION_TO_SAVE_NVRAM;
                    break;
                }

                case -2:
                {
                    err = FSP_ERR_AT_CMD_ERR_CONTEXT_INVALID_SESS_TYPE;
                    break;
                }
            }
        }
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSAVE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSAVE)
{
    const char * p_descrption = "Save current status of all session";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TCPDATAMODE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

#if defined(__SUPPORT_TCP_RECVDATA_HEX_MODE__)
    int  mode         = 0;
    char resp_str[32] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+TCPDATAMODE=? */
        snprintf(resp_str,
                 sizeof(resp_str),
                 "\r\n%s:%d",
                 rm_atcmd_w_core_common_strupr(argv[0] + 2),
                 g_atcmd_w_core_tcp_recv_data_mode);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &mode, POL_1) != 0)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("invalid mode(%s)\n", argv[1]);

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if ((mode == 0) || (mode == 1))
        {
            g_atcmd_w_core_tcp_recv_data_mode = mode;

            /* Set TCP Client's data mode */
            set_tcpc_data_mode(mode);

            /* Set TCP Server's data mode */
            set_tcps_data_mode(mode);

            return FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
#endif

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TCPDATAMODE)
{
    const char * p_usage = "<mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TCPDATAMODE)
{
    const char * p_descrption = "Convert received TCP data to hex string. mode(0|1)";

    return p_descrption;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLINIT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  ret          = 0;
    int  cid          = -1;
    int  role         = ATCMD_TLS_NONE;
    char resp_str[16] = {0x00, };

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 2)
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &role, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((role != ATCMD_TLS_SERVER) && (role != ATCMD_TLS_CLIENT))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        ret = atcmd_transport_ssl_do_init(role, &cid);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                snprintf(resp_str, sizeof(resp_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), cid);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                break;
            }

            case RRQ_APP_NOT_IMPLEMENTED:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_ROLE_NOT_SUPPORT;
                break;
            }

            case RRQ_APP_MALLOC_ERROR:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            case RRQ_APP_NOT_CREATED:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }

            case RRQ_APP_SSL_SOCKET_CREATE_FAIL:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_SESS_TASK_CREATE;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLINIT)
{
    const char * p_usage = "<role>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLINIT)
{
    const char * p_description = "Initialize the SSL module";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCFG)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret     = 0;
    int tmp_int = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 4)
    {
        /* Check validation */
        /* CID */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_TYPE;
            goto end;
        }

        /* Configuration-ID */
        if (rm_atcmd_w_core_common_stoi(argv[2], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONF_ID_TYPE;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 11) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONF_ID_RANGE;
            goto end;
        }

        ret = atcmd_transport_ssl_do_cfg(argv[1], argv[2], argv[3]);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_SSL_ROLE_NOT_SUPPORT:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_ROLE_NOT_SUPPORT;
                break;
            }

            case RRQ_APP_NOT_SUPPORTED:
            case RRQ_APP_NOT_IMPLEMENTED:
            case RRQ_APP_SSL_CFG_CONF_ID_NOT_SUPPORT:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_ID_NOT_SUPPORTED;
                break;
            }

            case RRQ_APP_NOT_FOUND:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
                break;
            }

            case RRQ_APP_ALREADY_ENABLED:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_ALREADY_EXIST;
                break;
            }

            case RRQ_APP_SSL_CFG_CA_CERT_NO_NAME:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_CA_CERT_NO_NAME;
                break;
            }

            case RRQ_APP_SSL_CFG_CA_CERT_NO_ZERO_SEQ:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_CA_CERT_NO_ZERO_SEQ;
                break;
            }

            case RRQ_APP_SSL_CFG_CERT_NO_CERT:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_CERT_NO_CERT;
                break;
            }

            case RRQ_APP_SSL_CFG_CERT_NO_KEY:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_CERT_NO_KEY;
                break;
            }

            case RRQ_APP_SSL_CFG_SET_SNI:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_SNI;
                break;
            }

            case RRQ_APP_SSL_SVR_VAILD_TYPE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_SVR_VALID_TYPE;
                break;
            }

            case RRQ_APP_SSL_SVR_VAILD_RANGE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_SVR_VALID_RANGE;
                break;
            }

            case RRQ_APP_SSL_RX_BUF_LENTH:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_RX_BUF_LEN;
                break;
            }

            case RRQ_APP_SSL_TX_BUF_LENTH:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_TX_BUF_LEN;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCFG)
{
    const char * p_usage = "<cid>,<conf_id>,<conf_value>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCFG)
{
    const char * p_description = "Configure SSL connection.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCO)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 4)
    {
        ret = atcmd_transport_ssl_do_co(p_at_ctrl, argv[1], argv[2], argv[3]);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_INVALID_PARAMETERS:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_CID_TYPE;
                break;
            }

            case RRQ_APP_NOT_FOUND:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
                break;
            }

            case RRQ_APP_NOT_IMPLEMENTED:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_ROLE_NOT_SUPPORT;
                break;
            }

            case RRQ_APP_CANNOT_START:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_ALREADY_CONNECTED;
                break;
            }

            case RRQ_APP_SSL_CONN_INVALID_PORT_NUM:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_PORT_NUM_TYPE;
                break;
            }

            case RRQ_APP_SSL_CONN_UNKNOWN_HOST:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_UNKNOWN_HOSTNAME;
                break;
            }

            case RRQ_APP_SSL_CONN_SETUP_CFG_ERR:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_CFG_SETUP_FAIL;
                break;
            }

            case RRQ_APP_SSL_CONN_TLS_CLIENT_RUN_ERR:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONN_TLS_CLINET_RUN_FAIL;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCO)
{
    const char * p_usage = "<cid>,<server_ip>,<server_port>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCO)
{
    const char * p_description = "Connect to an SSL server.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(TRSSLWR)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char       * params[ATCMD_W_CORE_MAX_PARAMS] = {0x00, };
    unsigned int param_cnt = 0;

    char * p_data   = NULL;
    size_t data_len = 0;

    char * p_atcmd = NULL;

    char resp_str[32] = {0x00, };

    /* Read remaining data */
    err = rm_atcmd_w_core_socket_read_trsslwr_cmd(p_at_ctrl, (char *) p_in, inlen, &p_data, &data_len);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        goto end;
    }

    p_atcmd = (char *) p_in;

    /* First call to strtok */
    if (strchr(p_atcmd, '=') != NULL)
    {
        params[param_cnt++] = strtok(p_atcmd, AT_CMD_CLASS_BC_EXT);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to find delimit(%s)\n", AT_CMD_CLASS_BC_EXT);
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD;
        goto end;
    }

    /* Split parameters */
    while (((params[param_cnt] = strtok(NULL, AT_CMD_VAR_MRK)) != NULL))
    {
        param_cnt++;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Parameter cnt(%d)\n", param_cnt);

    if (param_cnt == 3)
    {
        /* AT+TRSSLWR=<cid>,<length>,<data> */
        ret = atcmd_transport_ssl_do_wr(params[1], NULL, NULL, &data_len, p_data);
    }
    else if (param_cnt == 4)
    {
        /* AT+TRSSLWR=<cid>,<length>,<data> */
        ret = atcmd_transport_ssl_do_wr(params[1], NULL, NULL, &data_len, p_data);
    }
    else if (param_cnt == 5)
    {
        /* AT+TRSSLWR=<cid>,<ip>,<port>,<length>,<data> */
        ret = atcmd_transport_ssl_do_wr(params[1], params[2], params[3], &data_len, p_data);
    }
    else if (param_cnt == 6)
    {
        ret = atcmd_transport_ssl_do_wr(params[1], params[2], params[3], &data_len, p_data);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter(%d)\n", param_cnt);
        ret = RRQ_APP_INVALID_PARAMETERS;
    }

    switch (ret)
    {
        case RRQ_APP_SUCCESS:
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            break;
        }

        case RRQ_APP_SSL_PARTIAL_TX:
        {
            err = FSP_ERR_AT_CMD_ERR_DATA_TX;
            break;
        }

        case RRQ_APP_INVALID_PARAMETERS:
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            break;
        }

        case RRQ_APP_NOT_FOUND:
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
            break;
        }

        case RRQ_APP_NOT_CONNECTED:
        case RRQ_APP_NOT_CREATED:
        default:
        {
            err      = FSP_ERR_AT_CMD_ERR_DATA_TX;
            data_len = 0;
            break;
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        bsp_safe_strcpy(resp_str, "\r\nOK\r\n", sizeof(resp_str));
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        if (err == FSP_ERR_AT_CMD_ERR_DATA_TX)
        {
            char ext_err_str[8] = {0x00, };

            snprintf(ext_err_str, sizeof(ext_err_str), "%d", data_len);
            rm_atcmd_w_core_common_print_error_code_ext(p_at_ctrl, err, ext_err_str);
        }
        else
        {
            rm_atcmd_w_core_common_print_error_code(p_at_ctrl, err);
        }
    }

    if (p_data)
    {
        vPortFree(p_data);
        p_data = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLWR)
{
    const char * p_usage = "<cid>,[<dest>,<port>,<data length>,<data>]";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLWR)
{
    const char * p_description = "Send the data to SSL connection.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCL)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret = 0;
    int cid = -1;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");

        return FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
    }

    if (argc == 2)
    {
        ret = atcmd_transport_ssl_do_cl(argv[1], &cid);
        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_INVALID_PARAMETERS:
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
                break;
            }

            case RRQ_APP_NOT_FOUND:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
                break;
            }

            case RRQ_APP_NOT_IMPLEMENTED:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_ROLE_NOT_SUPPORT;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_CONFIG;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCL)
{
    const char * p_usage = "<cid>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCL)
{
    const char * p_description = "Close SSL connection.";

    return p_description;
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_socket_display_tls_session (int cid, char * p_out, size_t outlen)
{
    atcmd_tls_context  * p_ctx       = NULL;
    atcmd_tlsc_context * p_tlsc_ctx  = NULL;
    atcmd_tlsc_config  * p_tlsc_conf = NULL;

    unsigned int local_port = 0;
    int          state      = ATCMD_TLSC_STATE_DISCONN;
    char         ip_str[ATCMD_TLSC_MAX_ADDRSTRLEN]   = "0";
    char         port_str[ATCMD_TLSC_MAX_PORTSTRLEN] = "0";

    p_ctx = atcmd_transport_ssl_find_context(cid);
    if (!p_ctx)
    {
        return FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
    }

    if (p_ctx->role == ATCMD_TLS_CLIENT)
    {
        p_tlsc_ctx  = p_ctx->ctx.tlsc_ctx;
        p_tlsc_conf = p_tlsc_ctx->conf;

        if (p_tlsc_ctx->state == ATCMD_TLSC_STATE_CONNECTED)
        {
            if (atcmd_tlsc_get_local_port(p_tlsc_ctx, &local_port) != RRQ_APP_SUCCESS)
            {
                return FSP_ERR_AT_CMD_ERR_UNKNOWN;
            }

            state = ATCMD_TLSC_STATE_CONN;

            bsp_safe_strcpy(ip_str, p_tlsc_conf->svr_addr, sizeof(ip_str));
            bsp_safe_strcpy(port_str, p_tlsc_conf->svr_port, sizeof(port_str));
        }

        snprintf(p_out, outlen, "%d,%d,%d,%s,%s,%d\n", cid, p_ctx->role, state, ip_str, port_str, local_port);
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_SSL_ROLE_NOT_SUPPORT;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLPRT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    atcmd_tls_context * p_ctx = NULL;

    int cid = -1;

    char * p_resp   = NULL;
    size_t resp_len = 128;

    p_resp = (char *) pvPortMalloc(resp_len);
    if (!p_resp)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_resp, 0x00, resp_len);

    /* Added prefix for response */
    snprintf(p_resp, resp_len, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        if (atcmd_tls_ctx_header)
        {
            for (p_ctx = atcmd_tls_ctx_header; p_ctx != NULL; p_ctx = p_ctx->next)
            {
                cid = p_ctx->cid;

                err =
                    rm_atcmd_w_core_socket_display_tls_session(cid, (p_resp + strlen(p_resp)),
                                                               (resp_len - strlen(p_resp)));
                if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
                {
                    break;
                }
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
        }
    }
    else if (argc == 2)
    {
        /* CID */
        if (rm_atcmd_w_core_common_stoi(argv[1], &cid, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONF_CID_TYPE;
            goto end;
        }

        err = rm_atcmd_w_core_socket_display_tls_session(cid, (p_resp + strlen(p_resp)), (resp_len - strlen(p_resp)));
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_resp, strlen(p_resp));
    }

end:

    if (p_resp)
    {
        vPortFree(p_resp);
        p_resp = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLPRT)
{
    const char * p_usage = "[<cid>]";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLPRT)
{
    const char * p_description = "Get SSL session.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCERTLIST)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int       ret              = 0;
    char    * p_buf            = NULL;
    char    * p_certlist       = NULL;
    const int prefix_reply_len = (strlen(argv[0] + 2) + 4);
    const int buflen           = ((ATCMD_CM_MAX_NAME + 10) * ATCMD_CM_MAX_CERT_NUM) + prefix_reply_len;
    int       certlist_len     = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    p_buf = (char *) pvPortMalloc(buflen);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, buflen);

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_certlist   = (p_buf + strlen(p_buf));
    certlist_len = buflen - ((p_certlist - p_buf) + 1);

    if (argc == 2)
    {
        ret = atcmd_transport_ssl_do_certlist(argv[1], p_certlist, certlist_len);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_INVALID_PARAMETERS:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
                break;
            }

            case RRQ_APP_MALLOC_ERROR:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf      = NULL;
        p_certlist = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTLIST)
{
    const char * p_usage = "<certificate type>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTLIST)
{
    const char * p_description = "List certificates of list of CA data available.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(TRSSLCERTSTORE)
{
    typedef enum
    {
        READ_CERT_TYPE,                // 0
        READ_CERT_SEQUENCE,            // 1
        READ_CERT_FORMAT,              // 2
        READ_CERT_NAME,                // 3
        READ_CERT_DATA_LENGTH,         // 4
        READ_CERT_DATA                 // 5
    } atcmd_certstore_cmd_parameter_step;

    const char * p_cert_prefix = "-----";

    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err = FSP_SUCCESS;
    int cert_err = RRQ_APP_SUCCESS;

    char         ch              = 0;
    char         param_atcmd[40] = {0x00, };
    int          param_atcmd_idx = 0;
    unsigned int is_done         = pdFALSE;
    atcmd_certstore_cmd_parameter_step param_step = READ_CERT_TYPE;

    int    type                    = 0;
    int    sequence                = 0;
    int    cert_format             = 0;
    char   name[ATCMD_CM_MAX_NAME] = {0x00, };
    int    cert_len                = 0;
    char * p_cert                  = NULL;
    int    cert_idx                = 0;

    char resp_str[32] = {0x00, };

    FSP_PARAMETER_NOT_USED(inlen);

    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(uint8_t));
    if (fsp_err == FSP_SUCCESS)
    {
        if (ch != '=')
        {
            err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
            goto end;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        goto end;
    }

    while (err == FSP_ERR_AT_CMD_ERR_CMD_OK && !is_done)
    {
        switch (param_step)
        {
            case READ_CERT_TYPE:
            case READ_CERT_SEQUENCE:
            case READ_CERT_FORMAT:
            case READ_CERT_NAME:
            case READ_CERT_DATA_LENGTH:
            {
                memset(param_atcmd, 0x00, sizeof(param_atcmd));
                param_atcmd_idx = 0;
                ch              = 0x00;

                while (ch != 0x2C)
                {
                    if (param_atcmd_idx >= (int) sizeof(param_atcmd))
                    {
                        err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                        break;
                    }

                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(uint8_t));
                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_atcmd[param_atcmd_idx++] = ch;

                    if ((param_atcmd_idx == strlen(p_cert_prefix)) &&
                        (param_step == READ_CERT_DATA_LENGTH) &&
                        (cert_format == ATCMD_CM_CERT_FORMAT_PEM) &&
                        (strncmp(param_atcmd, p_cert_prefix, strlen(p_cert_prefix)) == 0))
                    {
                        param_step = READ_CERT_DATA;
                        RM_ATCMD_W_CORE_SOCKET_DEBUG("There is no data length and input is pem format\n");
                        break;
                    }
                }

                /* Update parameters and step */
                if (param_step == READ_CERT_TYPE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &type, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
                        goto end;
                    }

                    if ((type != ATCMD_CM_CERT_TYPE_CA_CERT) &&
                        (type != ATCMD_CM_CERT_TYPE_CERT))
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_RANGE;
                        goto end;
                    }

                    param_step = READ_CERT_SEQUENCE;
                }
                else if (param_step == READ_CERT_SEQUENCE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &sequence, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_SEQ_TYPE;
                        goto end;
                    }

                    if (rm_atcmd_w_core_common_is_in_valid_range(sequence, ATCMD_CM_CERT_MIN_SEQ_CA_CERT,
                                                                 ATCMD_CM_CERT_MAX_SEQ_CA_CERT) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_SEQ_RANGE;
                        goto end;
                    }

                    param_step = READ_CERT_FORMAT;
                }
                else if (param_step == READ_CERT_FORMAT)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &cert_format, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_FORMAT_TYPE;
                        goto end;
                    }

                    if ((cert_format != ATCMD_CM_CERT_FORMAT_DER) &&
                        (cert_format != ATCMD_CM_CERT_FORMAT_PEM))
                    {
                        err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_FORMAT_RANGE;
                        goto end;
                    }

                    param_step = READ_CERT_NAME;
                }
                else if (param_step == READ_CERT_NAME)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (param_atcmd_idx >= ATCMD_CM_MAX_NAME)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    strncpy(name, param_atcmd, strlen(param_atcmd));
                    param_step = READ_CERT_DATA_LENGTH;
                }
                else if (param_step == READ_CERT_DATA_LENGTH)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &cert_len, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    if ((cert_len <= 0) || (cert_len >= ATCMD_CM_MAX_CERT_BODY))
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        break;
                    }

                    param_step = READ_CERT_DATA;
                }

                break;
            }

            case READ_CERT_DATA:
            {
                p_cert = pvPortCalloc(ATCMD_CM_MAX_CERT_BODY, sizeof(char));
                if (p_cert == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                    break;
                }

                if (cert_len > 0)
                {
                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) p_cert, cert_len);
                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

                    p_cert[cert_len] = '\0';
                    cert_len++;

                    if (cert_format == ATCMD_CM_CERT_FORMAT_PEM)
                    {
                        RM_ATCMD_W_CORE_SOCKET_DEBUG("To convert 0x0D to 0x0A in PEM format\n");
                        for (int idx = 0; idx < cert_len; idx++)
                        {
                            if (p_cert[idx] == 0x0D)
                            {
                                p_cert[idx] = 0x0A;
                            }
                        }
                    }
                }
                else
                {
                    if (cert_format == ATCMD_CM_CERT_FORMAT_DER)
                    {
                        RM_ATCMD_W_CORE_SOCKET_ERROR("DER format requires to input data length\n");
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }
                    else if (cert_len == 0)
                    {
                        memcpy(p_cert, p_cert_prefix, strlen(p_cert_prefix));
                        cert_idx += strlen(p_cert_prefix);
                    }

                    while (1)
                    {
                        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(uint8_t));
                        if (fsp_err != FSP_SUCCESS)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }

                        if ((ch == '\b') && (cert_idx > 0))
                        {
                            cert_idx--;
                            p_cert[cert_idx] = '\0';
                        }
                        else if (ch == 0x0D)
                        {
                            p_cert[cert_idx] = 0x0A;
                            cert_idx++;
                        }
                        else if ((ch == 0x03) || (ch == 0x1A))
                        {
                            p_cert[cert_idx] = '\0';
                            cert_idx++;
                            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                            break;
                        }
                        else if (cert_idx >= ATCMD_CM_MAX_CERT_BODY)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }
                        else
                        {
                            p_cert[cert_idx] = ch;
                            cert_idx++;
                        }
                    }

                    cert_len = cert_idx;
                }

                if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
                {
                    RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to read certificate(0x%x)\n", err);
                    break;
                }

                is_done = pdTRUE;
                break;
            }
        }
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        cert_err = atcmd_cm_set_cert(name,
                                     (unsigned char) type,
                                     (unsigned char) sequence,
                                     (unsigned char) cert_format,
                                     p_cert,
                                     cert_len);
        switch (cert_err)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_DUPLICATED_ENTRY:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_ALREADY_EXIST;
                break;
            }

            case RRQ_APP_OVERFLOW:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_NO_SPACE;
                break;
            }

            case RRQ_APP_NOT_CREATED:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        ATCMD_ESC_OK_STR(resp_str);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        rm_atcmd_w_core_common_print_error_code(p_at_ctrl, err);
    }

    if (p_cert)
    {
        vPortFree(p_cert);
        p_cert = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTSTORE)
{
    const char * p_usage = "<certificate type>,<seq>,<format>,<name>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTSTORE)
{
    const char * p_description = "Store a certificate/CA list data.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLCERTDELETE)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret     = 0;
    int tmp_int = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 3)
    {
        /* Check validation */
        /* Certificate-type */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
            goto end;
        }

        /* ATCMD_CM_CERT_TYPE_CA_CERT=0, ATCMD_CM_CERT_TYPE_CERT=1 */
        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_RANGE;
            goto end;
        }

        ret = atcmd_transport_ssl_do_certdelete(argv[1], argv[2]);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_NOT_FOUND:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_DEL_LIST_NOT_FOUND;
                break;
            }

            case RRQ_APP_NOT_CREATED:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLCERTDELETE)
{
    const char * p_usage = "<certificate type>,<name>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLCERTDELETE)
{
    const char * p_description = "Delete a certificate or CA list data.";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLSAVE)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 1)
    {
        ret = atcmd_transport_ssl_do_save();

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_SSL_CLR_CTX_NVRAM_FAIL:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_SAVE_CLR_ALL_NV;
                break;
            }

            case RRQ_APP_SSL_SAVE_CTX_NVRAM_FAIL:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_SAVE_FAIL_NV;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLSAVE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLSAVE)
{
    const char * p_description = "Save current status of all TLS ession";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_CB(TRSSLDELETE)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret = 0;

    if (!ra6w1_network_main_is_wlaninit())
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Wi-Fi is not initialized.\n");
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (argc == 1)
    {
        ret = atcmd_transport_ssl_do_delete();

        if (ret)
        {
            RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to progress DELETE(0x%x)\n", ret);
            err = FSP_ERR_AT_CMD_ERR_SSL_SAVE_CLR_ALL_NV;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(TRSSLDELETE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(TRSSLDELETE)
{
    const char * p_description = "Delete saved TLS session";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(M)
{
    return rm_atcmd_w_core_socket_transfer_msg(p_at_ctrl, (char *) p_in, inlen);
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(M)
{
    const char * p_usage = "<ESC>M<cid>,<length>,<remote_ip>,<remote_port>,[<mode>,]<data>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(M)
{
    const char * p_description = "Transmit data through a socket with the CID specified";

    return p_description;
}

RM_ATCMD_W_CORE_SOCKET_UNFIXED_ATCMD_CB(H)
{
    return rm_atcmd_w_core_socket_transfer_msg(p_at_ctrl, (char *) p_in, inlen);
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_FORMAT_CB(H)
{
    const char * p_usage = "<ESC>H<cid>,<length>,<remote_ip>,<remote_port>";

    return p_usage;
}

RM_ATCMD_W_CORE_SOCKET_ATCMD_BRIEF_CB(H)
{
    const char * p_description = "Transmit data (handshaked mode) through a socket with the CID specified";

    return p_description;
}

#if (ATCMD_SECURE_CHANNEL == 1)
static int atcmd_transport_ssl_do_certstore (char * type_str,
                                             char * seq_str,
                                             char * format_str,
                                             char * name_str,
                                             char * p_data,
                                             size_t data_len)
{
    int status = RRQ_APP_SUCCESS;

    int type   = -1;
    int seq    = -1;
    int format = -1;

    if (rm_atcmd_w_core_common_stoi(type_str, &type, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get type(%s)\n", type_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (rm_atcmd_w_core_common_stoi(seq_str, &seq, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get seq(%s)\n", seq_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (rm_atcmd_w_core_common_stoi(format_str, &format, POL_1) != 0)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to get format(%s)\n", format_str);

        return RRQ_APP_INVALID_PARAMETERS;
    }

    status = atcmd_cm_set_cert(name_str,
                               (unsigned char) type,
                               (unsigned char) seq,
                               (unsigned char) format,
                               p_data,
                               data_len);

    if (status)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to set certificate(name: %s(%d), type: %d, cert: (%d)\n", name_str,
                                     strlen(name_str), type, data_len);

        return status;
    }

    return RRQ_APP_SUCCESS;
}

uint32_t RM_ATCMD_W_CORE_SOCKET_TRSSLWR_fixed_cmd_cb (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char       * params[ATCMD_W_CORE_MAX_PARAMS] = {0x00, };
    unsigned int param_cnt = 0;

    char * p_data   = NULL;
    size_t data_len = 0;

    char resp_str[32] = {0x00, };

    /* First call to strtok */
    params[param_cnt++] = strtok(argv[1], AT_CMD_VAR_MRK);

    /* Split parameters */
    while (((params[param_cnt] = strtok(NULL, AT_CMD_VAR_MRK)) != NULL))
    {
        param_cnt++;
    }

    if (param_cnt < 3)
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Failed to find delimit(%s)\n", AT_CMD_CLASS_BC_EXT);
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD;
        goto end;
    }

    RM_ATCMD_W_CORE_SOCKET_DEBUG("Parameter cnt(%d)\n", param_cnt);

    if (param_cnt == 3)
    {
        /* AT+TRSSLWR=<cid>,<length>,<data> */
        data_len = atoi(params[1]);
        p_data   = params[2];

        /* Check data length */
        printf("secure trsslwr cid %s, data len %d, data %s", params[0], data_len, p_data);
        ret = atcmd_transport_ssl_do_wr(params[0], NULL, NULL, &data_len, p_data);
    }
    else if (param_cnt == 4)
    {
        data_len = atoi(params[2]);
        p_data   = params[3];
        ret      = atcmd_transport_ssl_do_wr(params[0], NULL, NULL, &data_len, p_data);
    }
    else if (param_cnt == 5)
    {
        /* AT+TRSSLWR=<cid>,<ip>,<port>,<length>,<data> */
        data_len = atoi(params[3]);
        p_data   = params[4];
        ret      = atcmd_transport_ssl_do_wr(params[0], params[1], params[2], &data_len, p_data);
    }
    else if (param_cnt == 6)
    {
        data_len = atoi(params[4]);
        p_data   = params[5];
        ret      = atcmd_transport_ssl_do_wr(params[0], params[1], params[2], &data_len, p_data);
    }
    else
    {
        RM_ATCMD_W_CORE_SOCKET_ERROR("Invalid parameter(%d)\n", param_cnt);
        ret = RRQ_APP_INVALID_PARAMETERS;
    }

    switch (ret)
    {
        case RRQ_APP_SUCCESS:
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
            break;
        }

        case RRQ_APP_SSL_PARTIAL_TX:
        {
            err = FSP_ERR_AT_CMD_ERR_DATA_TX;
            break;
        }

        case RRQ_APP_INVALID_PARAMETERS:
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            break;
        }

        case RRQ_APP_NOT_FOUND:
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CONTEXT_NOT_FOUND;
            break;
        }

        case RRQ_APP_NOT_CONNECTED:
        case RRQ_APP_NOT_CREATED:
        default:
        {
            err      = FSP_ERR_AT_CMD_ERR_DATA_TX;
            data_len = 0;
            break;
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        bsp_safe_strcpy(resp_str, "\r\nOK\r\n", sizeof(resp_str));
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        if (err == FSP_ERR_AT_CMD_ERR_DATA_TX)
        {
            char ext_err_str[8] = {0x00, };

            snprintf(ext_err_str, sizeof(ext_err_str), "%d", data_len);
            rm_atcmd_w_core_common_print_error_code_ext(p_at_ctrl, err, ext_err_str);
        }
        else
        {
            rm_atcmd_w_core_common_print_error_code(p_at_ctrl, err);
        }
    }

    return err;
}

uint32_t RM_ATCMD_W_CORE_SOCKET_TRSSLCERTSTORE_fixed_cmd_cb (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    // const int max_atcmd_len = inlen;
    char * p_at_cmd = argv[0];

    char   * p_params[ATCMD_W_CORE_MAX_PARAMS] = {0x00, };
    uint32_t param_cnt    = 0;
    char     resp_str[32] = {0x00, };

    int ret     = 0;
    int tmp_int = 0;

    char * p_data      = NULL;
    int    data_len    = 0;
    int    cert_format = ATCMD_CM_CERT_FORMAT_PEM;

    p_at_cmd = pvPortMalloc(ATCMD_CM_MAX_CERT_LEN);
    if (!p_at_cmd)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    memset(p_at_cmd, 0x00, ATCMD_CM_MAX_CERT_LEN);

    if (strlen((const char *) argv[1]) > 0)
    {
        bsp_safe_strcpy(p_at_cmd, (const char *) argv[1], ATCMD_CM_MAX_CERT_LEN);
    }

    /* Parse arguments */
    p_params[param_cnt++] = argv[0];
    p_params[param_cnt++] = strtok(p_at_cmd, AT_CMD_VAR_MRK);
    while (((p_params[param_cnt] = strtok(NULL, AT_CMD_VAR_MRK)) != NULL))
    {
        if (p_params[param_cnt][0] == '\'')
        {
            char * tmp_ptr = NULL;

            /* Restore delimit-character */
            p_params[param_cnt][strlen(p_params[param_cnt])] = ',';

            if (strncmp(p_params[param_cnt], "',", 2) == 0)
            {
                /* First argument : AT+XXX=',aaaaa','bbbb' */
                if (param_cnt == 1)
                {
                    if ((tmp_ptr = strstr(&p_params[param_cnt][1], "',")) != NULL)
                    {
                        p_params[param_cnt] = p_params[param_cnt] + 1;
                        strtok(tmp_ptr, AT_CMD_VAR_MRK);
                        *tmp_ptr       = '\0';
                        *(tmp_ptr + 1) = '\0';
                    }
                }
                else
                {
                    if (p_params[param_cnt][strlen(p_params[param_cnt]) - 1] == '\'')
                    {
                        tmp_ptr             = p_params[param_cnt] + strlen(p_params[param_cnt]) - 1;
                        p_params[param_cnt] = p_params[param_cnt] + 1;
                        strtok(tmp_ptr, "'");
                        *tmp_ptr = '\0';
                    }
                    else
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }
                }
            }
            else if ((tmp_ptr = strstr(p_params[param_cnt], "',")) != NULL)
            {
                p_params[param_cnt] = p_params[param_cnt] + 1;
                strtok(tmp_ptr, AT_CMD_VAR_MRK);
                *tmp_ptr       = '\0';
                *(tmp_ptr + 1) = '\0';
            }
            else if (p_params[param_cnt][strlen(p_params[param_cnt]) - 1] == '\'')
            {
                tmp_ptr             = p_params[param_cnt] + strlen(p_params[param_cnt]) - 1;
                p_params[param_cnt] = p_params[param_cnt] + 1;
                strtok(tmp_ptr, "'");
                *tmp_ptr = '\0';
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }
        }
        else
        {
            if (strstr(p_params[param_cnt], "'") != NULL)
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }
        }

        param_cnt++;

        if (param_cnt > (ATCMD_W_CORE_MAX_PARAMS - 1))
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }
    }

    if (param_cnt >= 6)
    {
        /* Check validation */
        /* Certificate-type */
        if (rm_atcmd_w_core_common_stoi(p_params[1], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_RANGE;
            goto end;
        }

        /* Sequence */
        if (rm_atcmd_w_core_common_stoi(p_params[2], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_SEQ_TYPE;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 5) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_SEQ_RANGE;
            goto end;
        }

        /* Format */
        if (rm_atcmd_w_core_common_stoi(p_params[3], &tmp_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_FORMAT_TYPE;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_FORMAT_RANGE;
            goto end;
        }

        cert_format = tmp_int;

        /* Check data length */
        if (rm_atcmd_w_core_common_stoi(p_params[5], &tmp_int, POL_1) != 0)
        {
            if (cert_format == ATCMD_CM_CERT_FORMAT_PEM)
            {
                data_len = (strlen(p_params[5]) + 1);
                p_data   = p_params[5];
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_FORMAT_TYPE;
                goto end;
            }
        }
        else
        {
            data_len = (tmp_int + 1);
            p_data   = p_params[6];
        }

        ret = atcmd_transport_ssl_do_certstore(p_params[1], p_params[2], p_params[3], p_params[4], p_data, data_len);

        switch (ret)
        {
            case RRQ_APP_SUCCESS:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RRQ_APP_DUPLICATED_ENTRY:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_ALREADY_EXIST;
                break;
            }

            case RRQ_APP_OVERFLOW:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_STO_NO_SPACE;
                break;
            }

            case RRQ_APP_NOT_CREATED:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
                break;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    if (p_at_cmd)
    {
        vPortFree(p_at_cmd);
        p_at_cmd = NULL;
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        bsp_safe_strcpy(resp_str, "\r\nOK\r\n", sizeof(resp_str));
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        rm_atcmd_w_core_common_print_error_code(p_at_ctrl, err);
    }

    return err;
}

#endif
#endif                                 /* CFG_WIFI */
