/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __SLEEP_MGMT_REGS_H__
#define __SLEEP_MGMT_REGS_H__

#if CFG_WIFI
#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "lwip/dns.h"
#include "net_arp.h"
#include "dpmrtm.h"
#include "lwipopts.h"
#include "supp_def.h"
#include "bsp_rtm.h"

#define DPM_ETH_ALEN            6
#define DPM_MAX_SSID_LEN        32
#define DPM_MAX_WEP_KEY_LEN     16
#define DPM_PSK_LEN             32
#define DPM_NEIGHBOR_NUM        5
#define DPM_MAX_SAE_GROUPS      6
#define DPM_SAE_PASSWORD_LEN    (128+1)

#define REG_NAME_DPM_MAX_LEN    20
#define DPM_TIMER_NAME_MAX_LEN  8
#define DPM_TCP_KA_MAX          6

typedef struct _dpm_manage_table {
    char    mod_name[REG_NAME_DPM_MAX_LEN];
    unsigned long    bit_index;
    unsigned int    port_number;
    unsigned int    rcv_ready;
    unsigned int    init_done;
} dpm_list_table;

#if 0 // OLD DPM
typedef struct _umac_dpm_info_
{
    volatile unsigned char dpm_bssid[6];
    volatile unsigned short dpm_freq;
    volatile unsigned char  dpm_qos;
    volatile unsigned char  dpm_11b_flag;
    volatile unsigned short dpm_cap;
    volatile unsigned char  dpm_ht_supported;
    volatile unsigned char  dpm_ch_width;
    volatile unsigned char      dpm_key_seq_num[6];
    volatile unsigned short dpm_seq_num[0x10];  //I3ED11_QOS_CTL_TID_MASK       0x000f

    volatile unsigned long long    dpm_rx_pn[0x0a];    // ccmp rx pn number
    //volatile unsigned char    dpm_dummy2;
    volatile unsigned short dpm_tim_wakeup_dur;

    volatile unsigned char  dpm_ddps_flag;  //DDPS Enable flag
    volatile unsigned char  dpm_dummy;
} dpm_umac_info_t;
#endif

typedef struct _dpm_flag_in_rtm {
    time_in_rtm_t    time_params;
    
    unsigned char    dpm_mode;
    unsigned char    dpm_wakeup;
    unsigned char    dpm_sleepd_stop;

    int     dpm_rtc_timeout_flag;
    int     dpm_rtc_timeout_tid;
    int     dpm_keepalive_time_msec;    /* DPM Keepalive periodic time */
    int     dpm_dtim_period;    		/* DPM dtim period */
    int     dpm_supp_state;             /* Supplicant connection state */
    unsigned char    dpm_sntp_use;      /* for sntp use */
    unsigned long    dpm_sntp_period;   /* for sntp period */
    unsigned long    dpm_sntp_timeout;  /* for sntp timeout */

    unsigned char     dpm_dbg_level;

    unsigned char    dpm_http_svr_enable;  /* for http-server use */

    char            reserved[10];
} dpm_flag_in_rtm_t;                    // Total : 64 Bytes

/* Data structure in Retention memory for DPM operation */
typedef  struct {
    /* for Network Instances */
    char    net_mode;

    /* for ra6wx Supplicant */
    char    wifi_mode;
    char    country[4];
    char    reserve_1[2];
} dpm_supp_net_info_t;                // Total 8 Bytes

typedef  struct {
    /* for DHCP Client and Network Instance */
    long             dpm_dhcp_xid;
    unsigned long    dpm_ip_addr;
    unsigned long    dpm_netmask;
    unsigned long    dpm_gateway;
    unsigned long    dpm_dns_addr[2];    // 2 * 4
    unsigned long    dpm_lease;
    unsigned long    dpm_renewal;
    unsigned long    dpm_timeout; // dpm_rebind

    /* for dpm dhcp renew */
    unsigned long    dpm_dhcp_server_ip;
} dpm_supp_ip_info_t;                // Total : 40 Bytes

typedef  struct {
    /* for IPV6 Network Instance */
    ip6_addr_t       dpm_next_hop_address[DPM_NEIGHBOR_NUM]; // 20 * 5
    unsigned char    dpm_ipv6_addr[16];
	unsigned char    dpm_gateway[16];
    unsigned char    dpm_dns_addr[16];       // 148 Bytes

    unsigned char    dpm_neighbor_macaddr[DPM_NEIGHBOR_NUM][DPM_ETH_ALEN]; // 5(Neighbor Num) * 6(Mac Addr)
    unsigned short   dpm_router_lifetime;    // 32 Bytes

    unsigned char    dpm_router_idx;
    unsigned char    reserved_1[3];          // 4 Bytes

    unsigned long    dpm_ipv6_valid_life;
    unsigned long    dpm_ipv6_pref_life;
    unsigned long    dpm_reachable_time;    // 12 Bytes
} dpm_supp_ipv6_info_t;                  // Total : 196 Bytes

typedef     struct {
    int        mode;
    int        disabled;

    int        id;
    int        ssid_len;
    int        scan_ssid;
    int        psk_set;
    int        auth_alg;
    unsigned char    bssid[DPM_ETH_ALEN];    // 6
    unsigned char    reserved[2]; // Padding 2bytes
    unsigned char    ssid[DPM_MAX_SSID_LEN]; // 32
    unsigned char    psk[DPM_PSK_LEN];       // 32

#ifdef DEF_SAVE_DPM_WIFI_MODE
    int     wifi_mode;
    int     dpm_opt;

#ifdef __SUPPORT_IEEE80211W__
    unsigned char    pmf;
    unsigned char    setband_mask;  // setband
    unsigned char    reserved_2[2]; // Padding 2bytes
#else
    unsigned char    setband_mask;  // setband
    unsigned char    reserved_2[3]; // 3bytes
#endif // __SUPPORT_IEEE80211W__
#else // DEF_SAVE_DPM_WIFI_MODE
#ifdef __SUPPORT_IEEE80211W__
    unsigned char    pmf;
    unsigned char    setband_mask;   // setband
    unsigned char    reserved_2[10]; // Padding 1bytes + 9bytes
#else
    unsigned char    reserved_2[12]; // 12bytes
#endif // __SUPPORT_IEEE80211W__
#endif
} dpm_supp_conn_info_t;

typedef     struct {
    int ieee80211w;                             // 4 bytes
    int sae_groups[DPM_MAX_SAE_GROUPS];         // 24 bytes
    char sae_password[DPM_SAE_PASSWORD_LEN];    // 129 bytes
    char passphrase[wificonfigMAX_PASSPHRASE_LEN + 1];

    unsigned char identity[wificonfigMAX_ENT_IDENTITY_LEN];
    unsigned int identity_len;
    unsigned char password[wificonfigMAX_ENT_PASSWORD_LEN];
    unsigned int password_len;
} dpm_supp_conn_ext_info_t;

#define WPA_KCK_MAX_LEN    32
#define WPA_KEK_MAX_LEN    64
#define WPA_TK_MAX_LEN    32
typedef     struct{
    int        wpa_alg;
    int        key_idx;
    int        set_tx;
    unsigned char    seq[6];
    unsigned char    reserved[2];    // Padding 2bytes
    int              seq_len;
    unsigned char    ptk_kck[WPA_KCK_MAX_LEN];    /* EAPOL-Key Key Confirmation Key (KCK) */
    unsigned char    ptk_kek[WPA_KEK_MAX_LEN];    /* EAPOL-Key Key Encryption Key (KEK) */
    unsigned char    ptk_tk[WPA_TK_MAX_LEN];        /* Temporal Key (TK) */

    unsigned char    ptk_kck_len;
    unsigned char    ptk_kek_len;
    unsigned char    ptk_tk_len;
    unsigned char    reserved_3[1];    // Padding 1bytes
} cipher_ptk_t;                    // Total : 156 Bytes

typedef     struct{
    int        wpa_alg;
    int        key_idx;
    int        set_tx;
    unsigned char    seq[6];
    unsigned char    reserved[2];    // Padding 2bytes
    int              seq_len;
    unsigned char    gtk[32];
    unsigned char    gtk_len;
    unsigned char    reserved_2[3];    // Padding 3bytes
} cipher_gtk_t;                    // Total : 60 Bytes

typedef     struct {
    int        proto;
    int        key_mgmt;
    int        pairwise_cipher;
    int        group_cipher;
    int        key_flag;

    unsigned char    pmk_len;
    unsigned char    reserved;    // Padding 1bytes
    unsigned char    wep_key_len;
    unsigned char    wep_tx_keyidx;
    unsigned char    wep_key[DPM_MAX_WEP_KEY_LEN];    // 16 bytes

    cipher_ptk_t ptk;                        // 156 bytes
    cipher_gtk_t gtk;                        // 60 bytes
    unsigned char    reserved_2[4]; // Padding 4bytes
} dpm_supp_key_info_t;            // Total : 256 Bytes

typedef void (*timeout_cb)(char *timer_name);
typedef struct {
    timeout_cb     timeout_callback;
    unsigned int   msec;
    unsigned int   tid;
    char        task_name[REG_NAME_DPM_MAX_LEN];    // 20 Bytes
    char        timer_name[DPM_TIMER_NAME_MAX_LEN];    //  8 Bytes
} dpm_timer_info_t;                    // Total : 40 Bytes

typedef struct {
    dpm_timer_info_t timer_2;
    dpm_timer_info_t timer_3;
    dpm_timer_info_t timer_4;
    dpm_timer_info_t timer_5;
    dpm_timer_info_t timer_6;
    dpm_timer_info_t timer_7;
    dpm_timer_info_t timer_8;
    dpm_timer_info_t timer_9;
    dpm_timer_info_t timer_10;
    dpm_timer_info_t timer_11;
    dpm_timer_info_t timer_12;
    dpm_timer_info_t timer_13;
    dpm_timer_info_t timer_14;
    dpm_timer_info_t timer_15;
} dpm_timer_list_t;                    // Total : 504 Bytes

// RA6WX_MON_CLIENT - Start
#define NUM_TIM_STATUS 14
#define NUM_ERROR_CODE 10
typedef struct {
    unsigned long    tim_count[NUM_TIM_STATUS];
    unsigned long    error_count[NUM_ERROR_CODE];

    unsigned char    last_abnormal_type;
    unsigned char    last_abnormal_count;
    unsigned char    last_sleep_type; //0: by DPM sleep daemon, 1: by DPM Monitor
    unsigned char    reserved_10;

    char    wifi_conn_wait_time;
    char    dhcp_rsp_wait_time;
    char    arp_rsp_wait_time;
    char    unknown_dpm_fail_wait_time;

    unsigned int    wifi_conn_retry_cnt;
    unsigned int    reserved_21;
    unsigned int    reserved_22;

    unsigned int    fault_PC;
    unsigned char   fault_CNT;
    unsigned char   autoRebootStopFlag;
    unsigned char   reserved_26;
    unsigned char   reserved_27;
} dpm_monitor_info_t;
// RA6WX_MON_CLIENT - End

typedef struct {
    uint64_t last_ka_time[DPM_TCP_KA_MAX];
} dpm_kats;


/******************************************************************************
 *
 *  RTM Base Address
 *
 ******************************************************************************/
#define RA6WX_RTM_MAC_BASE              dg_configMAC_RTM_ADDR
#define RA6WX_RTM_MAC_SIZE              dg_configMAC_RTM_SIZE

#define RA6WX_RTM_APP_SUPP_BASE         dg_configAPPSUPP_RTM_ADDR
#define RA6WX_RTM_APP_SUPP_SIZE         dg_configAPPSUPP_RTM_SIZE

#define RA6WX_RTM_USER_SIZE             dg_configUSER_RTM_SIZE
#define RA6WX_RTM_PTIM_SIZE             dg_configPTIMG_SIZE

#define RTM_TCP_BASE                    dg_configTCP_RTM_ADDR
#define TCP_ALLOC_SZ                    dg_configTCP_RTM_SIZE

#define RTM_TCP_KA_TIME_BASE            dg_configTCPKAT_RTM_ADDR
#define RTM_TCP_KA_TIME_SIZE            dg_configTCPKAT_RTM_SIZE

#define RTM_SUPP_CONN_EXT_INFO_BASE     (dg_configAPPSUPP_EXT_RTM_ADDR)
#define RTM_SUPP_CONN_EXT_INFO_SIZE     (dg_configAPPSUPP_EXT_RTM_SIZE)

/* RETMEM_USER_BASE */
#define RTM_USER_POOL_BASE              dg_configUSERHDR_RTM_ADDR
#define RTM_USER_DATA_BASE              dg_configUSER_RTM_ADDR

/*
 * RRQ61x SDK Retention Memory Map (from show_rtm_map())
 */

/* MAC ALLOC SZ(MAC Connection info(0x80) */
#define MAC_ALLOC_SZ                    RA6WX_RTM_MAC_SIZE

#define FLAG_ALLOC_SZ                   (sizeof(dpm_flag_in_rtm_t))
#define NET_INFO_ALLOC_SZ               (sizeof(dpm_supp_net_info_t))
#define NET_IP_ALLOC_SZ                 (sizeof(dpm_supp_ip_info_t))
#define NET_IPV6_ALLOC_SZ               (sizeof(dpm_supp_ipv6_info_t))
#define CONN_INFO_ALLOC_SZ              (sizeof(dpm_supp_conn_info_t))
#define KEY_INFO_ALLOC_SZ               (sizeof(dpm_supp_key_info_t))
#if LWIP_IPV4
#ifdef RM_LWIP_W_CLEANED
#define ARP_ALLOC_SZ                    (sizeof(struct rm_etharp_entry) * ARP_TABLE_SIZE)
#else
#define ARP_ALLOC_SZ                    (sizeof(struct etharp_entry) * ARP_TABLE_SIZE)
#endif /* RM_LWIP_W_CLEANED */
#endif /* LWIP_IPV4 */
#define DNS_ALLOC_SZ                    (sizeof(struct dpm_dns_cache_entry))
#define RTC_TIMER_ALLOC_SZ              (sizeof(dpm_timer_list_t))
#define MONITOR_ALLOC_SZ                (sizeof(dpm_monitor_info_t))

#define USER_DATA_ALLOC_SZ              RA6WX_RTM_USER_SIZE

#if LWIP_IPV4
#define SUPP_ALLOC_SZ                   (                        \
                                        FLAG_ALLOC_SZ +          \
                                        NET_INFO_ALLOC_SZ +      \
                                        NET_IP_ALLOC_SZ +        \
                                        NET_IPV6_ALLOC_SZ +      \
                                        CONN_INFO_ALLOC_SZ +     \
                                        KEY_INFO_ALLOC_SZ +      \
                                        ARP_ALLOC_SZ +           \
                                        DNS_ALLOC_SZ +           \
                                        RTC_TIMER_ALLOC_SZ +     \
                                        MONITOR_ALLOC_SZ         \
                                        )
#else
#define SUPP_ALLOC_SZ                   (                        \
                                        FLAG_ALLOC_SZ +          \
                                        NET_INFO_ALLOC_SZ +      \
                                        NET_IP_ALLOC_SZ +        \
                                        CONN_INFO_ALLOC_SZ +     \
                                        KEY_INFO_ALLOC_SZ +      \
                                        DNS_ALLOC_SZ +           \
                                        RTC_TIMER_ALLOC_SZ +     \
                                        MONITOR_ALLOC_SZ         \
                                        )
#endif

#define APP_ALLOC_SZ                    (MAC_ALLOC_SZ + SUPP_ALLOC_SZ)

/*****************************************************************************/
#define RETMEM_APP_BASE                 RA6WX_RTM_MAC_BASE

/*****************************************************************************/
#define RETMEM_APP_MAC_OFFSET          (RETMEM_APP_BASE)
#define RETMEM_APP_SUPP_OFFSET         (RA6WX_RTM_APP_SUPP_BASE)

/*****************************************************************************/
#define RETM_MAC_BASE                  (RETMEM_APP_MAC_OFFSET)

/*****************************************************************************/
#define RTM_FLAG_BASE                  RETMEM_APP_SUPP_OFFSET
#define RTM_SUPP_NET_INFO_BASE         (RTM_FLAG_BASE             + FLAG_ALLOC_SZ)
#define RTM_SUPP_IP_INFO_BASE          (RTM_SUPP_NET_INFO_BASE  + NET_INFO_ALLOC_SZ)
#define RTM_SUPP_IPV6_INFO_BASE        (RTM_SUPP_IP_INFO_BASE  + NET_IP_ALLOC_SZ)
#define RTM_SUPP_CONN_INFO_BASE        (RTM_SUPP_IPV6_INFO_BASE     + NET_IPV6_ALLOC_SZ)
#define RTM_SUPP_KEY_INFO_BASE         (RTM_SUPP_CONN_INFO_BASE + CONN_INFO_ALLOC_SZ)
#define RTM_ARP_BASE                   ((RTM_SUPP_KEY_INFO_BASE + KEY_INFO_ALLOC_SZ))
#if LWIP_IPV4
#define RTM_DNS_BASE                   (RTM_ARP_BASE        + ARP_ALLOC_SZ)
#else
#define RTM_DNS_BASE                   (RTM_ARP_BASE)
#endif
#define RTM_RTC_TIMER_BASE             (RTM_DNS_BASE        + DNS_ALLOC_SZ)
#define RTM_DPM_MONITOR_BASE           (RTM_RTC_TIMER_BASE    + RTC_TIMER_ALLOC_SZ)

//****************************************************************************
#define TCP_SESS_INFO                  (RTM_TCP_BASE)

//****************************************************************************
#define RTM_FLAG_PTR                   ((dpm_flag_in_rtm_t *)   RTM_FLAG_BASE)
#define RTM_SUPP_NET_INFO_PTR          ((dpm_supp_net_info_t *) RTM_SUPP_NET_INFO_BASE)
#define RTM_SUPP_IP_INFO_PTR           ((dpm_supp_ip_info_t *)  RTM_SUPP_IP_INFO_BASE)
#define RTM_SUPP_IPV6_INFO_PTR         ((dpm_supp_ip_info_t *)  RTM_SUPP_IPV6_INFO_BASE)
#define RTM_SUPP_CONN_INFO_PTR         ((dpm_supp_conn_info_t *)RTM_SUPP_CONN_INFO_BASE)
#define RTM_SUPP_CONN_EXT_INFO_PTR     ((dpm_supp_conn_ext_info_t *)RTM_SUPP_CONN_EXT_INFO_BASE)
#define RTM_SUPP_KEY_INFO_PTR          ((dpm_supp_key_info_t *) RTM_SUPP_KEY_INFO_BASE)

#define RTM_ARP_PTR                    ((unsigned char *)RTM_ARP_BASE)
#define RTM_DNS_PTR                    ((unsigned char *)RTM_DNS_BASE)

#define RTM_RTC_TIMER_PTR              ((dpm_timer_list_t *) RTM_RTC_TIMER_BASE)
#define RTM_DPM_MONITOR_PTR            ((dpm_monitor_info_t *)  RTM_DPM_MONITOR_BASE)

#define RTM_DPM_TCP_KA_TIME_PTR        ((dpm_kats *)  RTM_TCP_KA_TIME_BASE)

#define RTM_USER_POOL_PTR              ((unsigned char *)RTM_USER_POOL_BASE)
#define RTM_USER_DATA_PTR              ((unsigned char *)RTM_USER_DATA_BASE)

//****************************************************************************
#endif /*CFG_WIFI*/
#endif /* __SLEEP_MGMT_REGS_H__ */
/* EOF */
