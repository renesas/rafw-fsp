/**
 ****************************************************************************************
 *
 * @file rm_cli_w_easysetup.h
 *
 * @brief defines for easy-setup
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#ifndef __RM_CLI_W_EASYSETUP_H__
#define __RM_CLI_W_EASYSETUP_H__
#if CFG_WIFI
#include "rm_wifi_api.h"

#if defined __SUPPORT_WPA3_PERSONAL__
#if !defined __SUPPORT_WPA3_PERSONAL_CORE__
    #error "This SDK does not support WPA3-Personal."
#endif /* __SUPPORT_WPA3_PERSONAL_CORE__ */
#endif /* __SUPPORT_WPA3_PERSONAL__ */

#if defined __SUPPORT_WPA_ENTERPRISE__
#if !defined __SUPPORT_WPA_ENTERPRISE_CORE__
    #error "This SDK does not support WPA-Enterprise."
#else
    #if !defined __SUPPORT_WPA3_ENTERPRISE_CORE__
        #error "This SDK does not support WPA3-Enterprise."
    #else
        #if defined __SUPPORT_WPA3_ENTERPRISE_192B__
            #if !defined __SUPPORT_WPA3_ENTERPRISE_192B_CORE__
                #error "This SDK does not support WPA3-Enterprise 192Bits."
            #endif /* __SUPPORT_WPA3_ENTERPRISE_192B_CORE__ */
        #endif /* __SUPPORT_WPA3_ENTERPRISE_192B__ */
    #endif /* __SUPPORT_WPA3_ENTERPRISE_CORE__ */
#endif /* __SUPPORT_WPA_ENTERPRISE_CORE__ */
#endif /* __SUPPORT_WPA_ENTERPRISE__ */

#define RET_QUIT        -99
#define RET_DEFAULT     -88
#define RET_NODIGIT     -100
#define RET_OVERFLOW    -101
#define RET_MANUAL      -200
#define RUN_DELAY       100
#define MAX_CHANNEL_LIST 20
#define SETUP_IP_STR_LEN 16
/**
 * IMPROTANT: when updating setup_params, sntp_params_t or dhcp_params_t structs, increase the EASYSETUP_VERSION so that
 * load_params will ignore old existing NVRAM data made for an incompatible version of the setup_params struct
 */
#define EASYSETUP_VERSION 4

enum {
    CA_CERT0 = 0,    // For MQTTs CLI
    CLIENT_CERT0,
    CLIENT_KEY0,
    DH_PARAM0,
    CA_CERT1 = 0,    // For HTTPs CLI
    CLIENT_CERT1,
    CLIENT_KEY1,
    DH_PARAM1,
    CA_CERT2,        // For Enterprise(802.1x)
    CLIENT_CERT2,
    CLIENT_KEY2,
    DH_PARAM2,
    CA_CERT3,        // For OTA
    CLIENT_CERT3,
    CLIENT_KEY3,
    DH_PARAM3,
    CA_CERT4,        // For HTTPs SVR
    CLIENT_CERT4,
    CLIENT_KEY4,
    DH_PARAM4,
    CA_CERT6,        // For AWS
    INITIAL_CERT6,
    INITIAL_KEY6,
    UNIQUE_CERT6,
    UNIQUE_KEY6,
    CD7,             // For MATTER
    DAC_CERT7,
    PAI_CERT7,
    DAC_KEY7,
    DAC_PUB7,
    CA_CERT8,        // For Miscellaneous Application 1
    CLIENT_CERT8,
    CLIENT_KEY8,
    DH_PARAM8,
    EXCH_PARAM8,
    CA_CERT9,        // For Miscellaneous Application 2
    CLIENT_CERT9,
    CLIENT_KEY9,
    DH_PARAM9,
    EXCH_PARAM9,
    CA_CERT10,        // For Miscellaneous Application 3
    CLIENT_CERT10,
    CLIENT_KEY10,
    DH_PARAM10,
    EXCH_PARAM10,
    CA_CERT11,        // For Miscellaneous Application 4
    CLIENT_CERT11,
    CLIENT_KEY11,
    DH_PARAM11,
    EXCH_PARAM11,
    CA_CERT12,        // For Miscellaneous Application 5
    CLIENT_CERT12,
    CLIENT_KEY12,
    DH_PARAM12,
    EXCH_PARAM12,
    CA_CERT13,        // For Miscellaneous Application 6
    CLIENT_CERT13,
    CLIENT_KEY13,
    DH_PARAM13,
    EXCH_PARAM13,
    CA_CERT14,        // For Miscellaneous Application 7
    CLIENT_CERT14,
    CLIENT_KEY14,
    DH_PARAM14,
    EXCH_PARAM14,
    CA_CERT15,        // For Miscellaneous Application 8
    CLIENT_CERT15,
    CLIENT_KEY15,
    DH_PARAM15,
    EXCH_PARAM15,
    TLS_CERT_01 = 100,
    TLS_CERT_02,
    TLS_CERT_03,
    TLS_CERT_04,
    TLS_CERT_05,
    TLS_CERT_06,
    TLS_CERT_07,
    TLS_CERT_08,
    TLS_CERT_09,
    TLS_CERT_10,
    TLS_CERT_11,
    TLS_CERT_12,
    TLS_CERT_13,
    TLS_CERT_14,
    TLS_CERT_15,
    TLS_CERT_16,
    CERT_ALL,
    CERT_END,
};
enum {
    ACT_NONE = 0,
    ACT_WRITE,
    ACT_READ,
    ACT_DELETE,
    ACT_STATUS
};

struct sntp_params {
    unsigned char   sntp_client;
    int     sntp_client_period_time; // = NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL;
    char    sntp_gmt_timezone[8];
    int     sntp_timezone_int;
    char    sntp_svr_addr[256];
    char    sntp_svr_addr1[256];
    char    sntp_svr_addr2[256];
    int     select_ssid;
    int     channel_selected;
    int     sntp_svr_index;
};

struct dhcp_params {
    unsigned char use_dhcps;
    char dhcp_lease_start[16];
    char dhcp_lease_end[16];
    int dhcp_lease_time;
    int dhcp_lease_count;
};

struct setup_params {
    char country_code[4];
    unsigned char sysmode;
    unsigned char band;
    unsigned char channel;
    unsigned char wifi_mode;
#if CFG_PMGR
    unsigned char enable_dpm;
    int dpm_keepalive_time;
    int dpm_user_wakeup_time;
    int dpm_TIM_wakeup_count;
#endif //CFG_PMGR
    char ssid[2][wificonfigMAX_SSID_LEN + 1];
    bool hidden_ssid;
    WIFIPmf_t pmf[2];
    WIFISecurityExt_t security[2];
    char password[2][wificonfigMAX_PASSPHRASE_LEN + 1];
    char wep_key[wificonfigMAX_WEPKEY_LEN + 1];
    uint8_t wep_key_idx;
    unsigned char wep_key_type;
    unsigned char wep_bit;
    unsigned char eap_auth_mode;
    unsigned char eap_phase2;
    char eap_id[wificonfigMAX_ENT_IDENTITY_LEN + 1];
    char eap_pw[wificonfigMAX_ENT_PASSWORD_LEN + 1];
    unsigned char p2p_listen_chan;
    unsigned char p2p_go_intent;
    char p2p_ssid_postfix[23];
    int netmode[2];
    char ipaddress[2][SETUP_IP_STR_LEN];
    char subnetmask[2][SETUP_IP_STR_LEN];
    char gateway[2][SETUP_IP_STR_LEN];
    char dns[2][SETUP_IP_STR_LEN];
#if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || defined __SUPPORT_MESH__
    char sae_groups[2][wificonfigMAX_SAE_GROUPS_LEN];
#endif // __SUPPORT_WPA3_SAE__ || __SUPPORT_MESH__
    int ap_max_inactivity;
    int ap_wmm_ps_enabled;
    int ap_wmm_enabled;
    uint8_t channel_number;             /* Number of channels for scan (STA mode only) */
    uint32_t channel_list[wificonfigMAX_CHANNEL_LIST]; /* List of channels : MAX - 20  */
    int ap_enc_mode;
};

/*---------------------------------------------------------------------------
 *      External Functions
 *---------------------------------------------------------------------------*/
extern int  ra6w1_cli_reply(char *cmdline, char *delimit, char *cli_reply);
extern int  ra6w1_regdb_get_ch_range_by_country_n_band(char* country, 
                                                       int band, int* min_ch, int* max_ch, 
                                                       unsigned int* ch_bitmap_5g, unsigned int exclude_flags);
extern int  ra6w1_regdb_get_5g_ch(int idx);
extern void ra6w1_regdb_gen_5g_ch_range_string(char* str_buf, unsigned int ch_bitmap, char delimiter);
extern int  chk_channel_by_country(char *country, int set_channel, int mode, int *rec_channel, int dbg_info);

extern int  get_run_mode(void);
extern void set_run_mode(int mode);

extern void sntp_stop(void);
extern bool reset(void);
extern int ra6w1_network_main_get_wlaninit_mode(void);
extern int ra6w1_network_main_init_wlan(void);
extern int ra6w1_network_main_is_wlaninit(void);
extern int ra6w1_network_main_set_wlaninit_mode(int flag);

void create_easy_setup_task(bool load_from_nvram);
void easy_setup_suspend(void);
#endif /* CFG_WIFI */
#endif /* __RM_CLI_W_EASYSETUP_H__ */

/* EOF */
