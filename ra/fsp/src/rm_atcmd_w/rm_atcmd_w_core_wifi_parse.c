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
 #include "rm_atcmd_w_core_wifi_parse.h"
 #include "rm_atcmd_w_core_err_code.h"
 #include "rm_atcmd_w_core.h"
 #include "rm_atcmd_w_app.h"

 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"
 #include "net_common.h"
 #include "util_api.h"
 #include "supp_config.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "rm_wifi.h"
 #include "rm_wifi_helper.h"
 #include "net_network_main.h"
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
 #endif                                /* CFG_PMGR */
 #include "net_ip_handler.h"
 #include "lwip/dhcp.h"

 #if defined(__SUPPORT_MQTT__)
  #include "rm_atcmd_w_core_mqtt_parse.h"
 #endif
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
  #include dg_configADNVPARAM_PROJ_FILE
 #endif
 #include <strings.h>

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #define RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(atcmd)    "AT+" # atcmd

 #define RM_ATCMD_W_CORE_WIFI_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
 #define RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _format_cb(void)
 #define RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _brief_cb(void)

 #define RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _cmd_cb
 #define RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _format_cb
 #define RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_WIFI_ ## atcmd ## _brief_cb

 #define DEFAULT_BSS_MAX_COUNT_AT_SPI_SDIO    (60)
 #define RM_ATCMD_W_CORE_WIFI_PMK_HEX_STR_LEN    (64)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/// AT-CMD Wi-Fi Authentication Mode values
typedef enum
{
    /// Wi-Fi Open Security
    CC_VAL_AUTH_OPEN,

    /// Wi-Fi Security: WEP
    CC_VAL_AUTH_WEP,

    /// Wi-Fi Security: WPA
    CC_VAL_AUTH_WPA,

    /// Wi-Fi Security: WPA2 (RSN)
    CC_VAL_AUTH_WPA2,

    /// Wi-Fi Security: WPA & WPA2
    CC_VAL_AUTH_WPA_AUTO,

    /// Wi-Fi Security: WPA3 OWE
    CC_VAL_AUTH_OWE,

    /// Wi-Fi Security: WPA3 SAE
    CC_VAL_AUTH_SAE,

    /// Wi-Fi Security: WPA2 (RSN) & WPA3 SAE
    CC_VAL_AUTH_RSN_SAE,

    /// Wi-Fi Security: WPA-Enterprise
    CC_VAL_AUTH_WPA_EAP = 8,

    /// Wi-Fi Security: WPA2-Enterprise
    CC_VAL_AUTH_WPA2_EAP = 9,

    /// Wi-Fi Security: WPA & WPA2-Enterprise
    CC_VAL_AUTH_WPA_WPA2_EAP = 10,

    /// Wi-Fi Secureity: WPA3-Enterprise */
    CC_VAL_AUTH_WPA3_EAP = 11,

    /// Wi-Fi Secureity: WPA3-Enterprise-192 Bits */
    CC_VAL_AUTH_WPA3_EAP_192B = 12,

    /// Wi-Fi Secureity: WPA2 & WPA3-Enterprise */
    CC_VAL_AUTH_WPA2_WPA3_EAP = 13,

    /// Wi-Fi Security: UNKNOWN
    CC_VAL_UNKNOWN = 99,
} cc_val_auth;

/// AT-CMD Wi-Fi WPA Encryption values
typedef enum
{
    /// WPA Encryption: TKIP
    CC_VAL_ENC_TKIP = 0,

    /// WPA Encryption: CCMP
    CC_VAL_ENC_CCMP = 1,

    /// WPA Encryption: TKIP + CCMP
    CC_VAL_ENC_AUTO = 2,

    /// WPA3 Encryption-192 Bits: GCMP-256
    CC_VAL_ENC_WPA3_EAP_192B = 3,
} cc_val_enc;

/// AT-CMD Wi-Fi Mode values
typedef enum
{
    /// Wi-Fi IEEE802.11 Mode: 11b/g/n
    CC_VAL_WFMODE_BGN,

    /// Wi-Fi IEEE802.11 Mode: 11g/n
    CC_VAL_WFMODE_GN,

    /// Wi-Fi IEEE802.11 Mode: 11b/g
    CC_VAL_WFMODE_BG,

    /// Wi-Fi IEEE802.11 Mode: 11n only
    CC_VAL_WFMODE_N,

    /// Wi-Fi IEEE802.11 Mode: 11g only
    CC_VAL_WFMODE_G,

    /// Wi-Fi IEEE802.11 Mode: 11b only
    CC_VAL_WFMODE_B,

    /// Wi-Fi IEEE802.11 Mode: 11a/n
    CC_VAL_WFMODE_AN,

    /// Wi-Fi IEEE802.11 Mode: 11a only
    CC_VAL_WFMODE_A,

    /// Wi-Fi IEEE802.11 Mode: 11n 5GHz only
    CC_VAL_WFMODE_N_5G
} cc_val_wfmode;

/// AT-CMD Wi-Fi Enterprise EAP Phase#1 values
typedef enum
{
    /// WPA-Enterprise Phase1: Default ( PEAP / TTLS / FAST )
    CC_VAL_EAP_DEFAULT,

    /// WPA-Enterprise Phase1: PEAPv0
    CC_VAL_EAP_PEAP0,

    /// WPA-Enterprise Phase1: PEAPv1
    CC_VAL_EAP_PEAP1,

    /// WPA-Enterprise Phase1: EAP-FAST
    CC_VAL_EAP_FAST,

    /// WPA-Enterprise Phase1: EAP-TTLS
    CC_VAL_EAP_TTLS,

    /// WPA-Enterprise Phase1: EAP-TLS
    CC_VAL_EAP_TLS,
} cc_val_eap1;

/// AT-CMD Wi-Fi Enterprise EAP Phase#2 values
typedef enum
{
    /// WPA-Enterprise Phase2: MSCHAPV2 GTC
    CC_VAL_EAP_PHASE2_MIX,

    /// WPA-Enterprise Phase2: EAP-MSCHAPv2
    CC_VAL_EAP_MSCHAPV2,

    /// WPA-Enterprise Phase2: EAP-GTC
    CC_VAL_EAP_GTC,

    /// WPA-Enterprise Phase2: EAP-TLS
    CC_VAL_EAP_PHASE2_TLS,
} cc_val_eap2;

typedef enum
{
    RM_ATCMD_W_CORE_WIFI_ASYNC_CLOSE             = -1,
    RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_STACONN    = 0,
    RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_STADISCONN = 1,
    RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_APCONN     = 2,
    RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_APDISCONN  = 3,
    RM_ATCMD_W_CORE_WIFI_ASYNC_INIT_DONE         = 10,
    RM_ATCMD_W_CORE_WIFI_ASYNC_CONN_FAILURE      = 11,
    RM_ATCMD_W_CORE_WIFI_ASYNC_PRINT             = 12,
} rm_atcmd_w_core_wifi_async_idx_t;

typedef struct _rm_atcmd_w_core_wifi_async_msg_t
{
    void          * p_ctrl;
    int             index;
    unsigned char * p_in;
    unsigned int    inlen;
} rm_atcmd_w_core_wifi_async_msg_t;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMODE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMAC);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMAC);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMAC);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSPF);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSPF);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSPF);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFOTP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFOTP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFOTP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSTAT);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSTAT);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSTAT);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSTA);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSTA);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSTA);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPBC);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPBC);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPBC);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPIN);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPIN);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPIN);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCWPS);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCWPS);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCWPS);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCC);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCC);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCC);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSCAN);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSCAN);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSCAN);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFRSSI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFRSSI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFRSSI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPA);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPA);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPA);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPA3);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPA3);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPA3);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPMK);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPMK);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPMK);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFQAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFQAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFQAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFROAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFROAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFROAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFROTH);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFROTH);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFROTH);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFDIS);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFDIS);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFDIS);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFENTAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFENTAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFENTAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFENTLI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFENTLI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFENTLI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFOAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFOAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFOAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFTAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFTAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFTAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFRAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFRAP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFRAP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFLCST);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFLCST);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFLCST);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPWM);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPWM);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPWM);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPCH);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPCHLIST);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPCHLIST);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPCHLIST);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPBI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPBI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPBI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPUI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPUI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPUI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPRT);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPRT);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPRT);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPDE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPDE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPDE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPDI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPDI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPDI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFWMM);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFWMM);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFWMM);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFWMP);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFWMP);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFWMP);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFFP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFFP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFFP2P);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSP2P);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCP2P);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFDP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFDP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFDP2P);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPP2P);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPP2P);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPLCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPLCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPLCH);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPOCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPOCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPOCH);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPGI);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPGI);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPGI);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPFT);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPFT);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPFT);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPDN);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPDN);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPDN);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSMSAVE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSMSAVE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSMSAVE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPSMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPSMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPSMODE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMODESWTCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMODESWTCH);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMODESWTCH);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSETBAND);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSETBAND);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSETBAND);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFINIT);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFINIT);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFINIT);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFINITMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFINITMODE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFINITMODE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(HOSTINITDONE);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(HOSTINITDONE);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(HOSTINITDONE);

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(GETNVRWIFIPF);
RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(GETNVRWIFIPF);
RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(GETNVRWIFIPF);

static fsp_err_atcmd_err_code rm_atcmd_w_parse_channel_arg(int                    * argc,
                                                           char * const           * argv,
                                                           WIFINetworkParamsExt_t * net_params);
static fsp_err_atcmd_err_code rm_atcmd_w_core_wifi_parse_channels(const char             * channel_arg,
                                                                  WIFINetworkParamsExt_t * net_params);
static int                    rm_atcmd_w_core_wifi_init_network_params(WIFINetworkParamsExt_t * p_net_params);
static fsp_err_atcmd_err_code rm_atcmd_w_core_wifi_get_channel(int interface, int * p_channel);
static int                    rm_atcmd_w_core_wifi_cp_str(char       * p_dest,
                                                          const size_t destlen,
                                                          const char * p_src,
                                                          const size_t srclen);
static void update_wifi_stat_buf(WIFIConnectionInfoExt_t * connection_info_ext,
                                 char                    * p_buf,
                                 size_t                    offset);
static int    hex_2_num(char c);
static int    hex_2_byte(const char * hex);
static size_t str_decode(u8 * buf, size_t maxlen, const char * str);

static int  atcmd_chk_wifi_conn(void);
static int  atcmd_chk_valid_macaddr(char * macaddr_str);
static void atcmd_rsp_wifi_conn(atcmd_w_ctrl_t * const p_at_ctrl);
static void atcmd_wfjap_conn_sent(void);

 #if defined(__SUPPORT_SETBAND_5GHZ__)
static void atcmd_set_wifi_mode(int auth_mode, int chan, WIFINetworkParamsExt_t * net_params);

 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */
void atcmd_wf_jap_dap_print_with_cause(atcmd_w_ctrl_t * const p_at_ctrl, int is_jap);

unsigned int RM_ATCMD_W_CORE_WIFI_asynchony_event_cb(void * const    p_ctrl,
                                                     int             index,
                                                     unsigned char * p_in,
                                                     unsigned int    inlen);
unsigned int RM_ATCMD_W_CORE_WIFI_startup_event_cb(void * const p_ctrl, unsigned char * p_in, unsigned int inlen);

BaseType_t   rm_atcmd_w_core_wifi_start_async_task(void);
BaseType_t   rm_atcmd_w_core_wifi_stop_async_task (void);
static void  rm_atcmd_w_core_wifi_async_task_entry(void * p_params);
unsigned int rm_atcmd_w_core_wifi_set_async_msg(void * const p_ctrl, int index, unsigned char * p_in,
                                                unsigned int inlen);
unsigned int rm_atcmd_w_core_wifi_oper_async_msg(rm_atcmd_w_core_wifi_async_msg_t * p_async_msg);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_wifi_module[] =
{
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFMODE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFMODE),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFMAC),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFMAC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFMAC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFMAC),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSPF),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSPF),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSPF),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSPF),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFOTP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFOTP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFOTP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFOTP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSTAT),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSTAT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSTAT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSTAT),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSTA),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSTA),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSTA),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSTA),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPBC),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPBC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPBC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPBC),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPIN),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPIN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPIN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPIN),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFCWPS),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFCWPS),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFCWPS),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFCWPS),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFCC),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFCC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFCC),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFCC),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSCAN),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSCAN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSCAN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSCAN),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFRSSI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFRSSI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFRSSI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFRSSI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFJAP),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFJAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFJAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFJAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFJAPA),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFJAPA),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFJAPA),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFJAPA),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFJAPA3),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFJAPA3),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFJAPA3),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFJAPA3),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFJAPMK),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFJAPMK),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFJAPMK),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFJAPMK),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFCAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFCAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFCAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFCAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFQAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFQAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFQAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFQAP),
    },
 #ifndef ATCMD_ROAMING_CONFIG
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFROAP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFROAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFROAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFROAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFROTH),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFROTH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFROTH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFROTH),
    },
 #endif                                /* ATCMD_ROAMING_CONFIG */
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFDIS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFDIS),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFDIS),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFDIS),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFENTAP),
        ATCMD_W_TYPE_A,
        7,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFENTAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFENTAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFENTAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFENTLI),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFENTLI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFENTLI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFENTLI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSAP),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFOAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFOAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFOAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFOAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFTAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFTAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFTAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFTAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFRAP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFRAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFRAP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFRAP),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFLCST),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFLCST),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFLCST),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFLCST),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPWM),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPWM),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPWM),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPWM),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPCH),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPCH),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPCHLIST),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPCHLIST),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPCHLIST),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPCHLIST),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPBI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPBI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPBI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPBI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPUI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPUI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPUI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPUI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPRT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPRT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPRT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPRT),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPDE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPDE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPDE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPDE),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFAPDI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFAPDI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFAPDI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFAPDI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFWMM),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFWMM),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFWMM),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFWMM),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFWMP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFWMP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFWMP),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFWMP),
    },
 #ifndef ATCMD_P2P_CONFIG
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFFP2P),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFFP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFFP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFFP2P),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSP2P),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSP2P),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFCP2P),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFCP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFCP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFCP2P),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFDP2P),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFDP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFDP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFDP2P),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPP2P),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPP2P),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPP2P),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPLCH),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPLCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPLCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPLCH),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPOCH),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPOCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPOCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPOCH),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPGI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPGI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPGI),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPGI),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPFT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPFT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPFT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPFT),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPDN),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPDN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPDN),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPDN),
    },
 #endif                                /* ATCMD_P2P_CONFIG */
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSMSAVE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSMSAVE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSMSAVE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSMSAVE),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFPSMODE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFPSMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFPSMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFPSMODE),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFMODESWTCH),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFMODESWTCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFMODESWTCH),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFMODESWTCH),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFSETBAND),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFSETBAND),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFSETBAND),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFSETBAND),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFINIT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFINIT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFINIT),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFINIT),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(WFINITMODE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(WFINITMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(WFINITMODE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(WFINITMODE),
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(HOSTINITDONE),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(HOSTINITDONE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(HOSTINITDONE),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(HOSTINITDONE)
    },
    {
        RM_ATCMD_W_CORE_WIFI_ATCMD_CODE(GETNVRWIFIPF),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_WIFI_ATCMD_CB_P(GETNVRWIFIPF),
        RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB_P(GETNVRWIFIPF),
        RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB_P(GETNVRWIFIPF)
    },
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

/*
 *  TRUE : 1) at boot (POR/Wakeup), 2) no +WFDAP:0 is sent,
 *         3) no +WFJAP:0 is sent,  4) +WFJAP:1 is sent
 *  FALSE: 5) +WFDAP:0 is sent,     6) +WFJAP:0 is sent
 */
static int                is_waiting_for_wf_jap_result_in_progress = FALSE;
static EventGroupHandle_t gp_evt_grp_wfjap = NULL;

atcmd_w_ctrl_t * gp_at_ctrl = NULL;
char             atcmd_mac_table[6][18];

static QueueHandle_t rm_atcmd_w_core_wifi_async_queue = NULL;
static TaskHandle_t  rm_atcmd_w_core_wifi_async_task  = NULL;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_WIFI_register (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_wifi_module);
}

uint32_t RM_ATCMD_W_CORE_WIFI_deregister (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    rm_atcmd_w_core_deregister(p_list, at_core_wifi_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_WIFI_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    gp_at_ctrl = p_at_ctrl;

    if (gp_evt_grp_wfjap == NULL)
    {
        gp_evt_grp_wfjap = xEventGroupCreate();
    }

    err = rm_wifi_set_atcmd_event_callback(p_at_ctrl, &RM_ATCMD_W_CORE_WIFI_asynchony_event_cb);

    err += rm_wifi_set_startup_atcmd_event_callback(p_at_ctrl, &RM_ATCMD_W_CORE_WIFI_startup_event_cb);

 #if (ATCMD_IF_SUPPORT == 1)
    dhcp_set_atcmd_event_callback(p_at_ctrl, &RM_ATCMD_W_CORE_WIFI_asynchony_event_cb);
 #endif

    rm_atcmd_w_core_wifi_start_async_task();

    return err;
}

uint32_t RM_ATCMD_W_CORE_WIFI_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    gp_at_ctrl = NULL;

    if (gp_evt_grp_wfjap)
    {
        vEventGroupDelete(gp_evt_grp_wfjap);
        gp_evt_grp_wfjap = NULL;
    }

    err = (fsp_err_atcmd_err_code) rm_wifi_set_atcmd_event_callback(NULL, NULL);

    rm_atcmd_w_core_wifi_set_async_msg(NULL, RM_ATCMD_W_CORE_WIFI_ASYNC_CLOSE, NULL, 0);

    if (rm_atcmd_w_core_wifi_stop_async_task())
    {
        err = FSP_ERR_AT_CMD_ERR_SYS_BUSY;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMODE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[32] = {0, };
    int  mode;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFMODE=? */
        sprintf(result_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), get_sys_mode());
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }
    else if (argc == 2)
    {
        /* AT+WFMODE=<mode> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &mode, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_RUN_MODE_TYPE;
            goto end;
        }

        if ((mode < WIFI_DEVICE_MODE_EXT_STATION)
 #if defined(__SUPPORT_MESH__)
            || (mode > WIFI_DEVICE_MODE_EXT_MESH_PORTAL)
 #endif                                /* __SUPPORT_MESH__ */
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
  #if defined(__SUPPORT_P2P__)
            || (mode > WIFI_DEVICE_MODE_EXT_P2P_STATION)
  #else
            || (mode > WIFI_DEVICE_MODE_EXT_AP_STATION)
  #endif                               /* __SUPPORT_P2P__ */
 #endif                                /* __SUPPORT_WIFI_CONCURRENT__ */
            )
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_RUN_MODE_RANGE;
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_SYS_MODE,
                                          mode);
 #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMODE)
{
    const char * p_usage = "<mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMODE)
{
    const char * p_description = "Set Wi-Fi Operation Mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMAC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    unsigned int           ret = 0;

    char   resp_str[64]    = {0x00, };
    char * p_result        = NULL;
    char   mac_addr_01[18] = {0x00, };
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
    char mac_addr_02[18] = {0x00, };
 #endif                                // __SUPPORT_WIFI_CONCURRENT__

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* Added prefix for response */
        sprintf(resp_str, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_result = (resp_str + strlen(resp_str));

        /* AT+WFMAC=? */
        switch (get_run_mode())
        {
            case WIFI_DEVICE_MODE_EXT_STATION:
            {
                rm_wifi_get_mac_address_string(WLAN0_IFACE, mac_addr_01, 1);
                memcpy(p_result, mac_addr_01, strlen(mac_addr_01));
                break;
            }

            case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
            case WIFI_DEVICE_MODE_EXT_P2P:
            case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // __SUPPORT_P2P__
            {
                rm_wifi_get_mac_address_string(WLAN1_IFACE, mac_addr_01, 1);
                memcpy(p_result, mac_addr_01, strlen(mac_addr_01));
                break;
            }

 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
        {
            rm_wifi_get_mac_address_string(WLAN0_IFACE, mac_addr_01, 1);
            rm_wifi_get_mac_address_string(WLAN1_IFACE, mac_addr_02, 1);

            sprintf(p_result, "%s,%s", mac_addr_01, mac_addr_02);
            break;
        }
 #endif                                // __SUPPORT_WIFI_CONCURRENT__

        default:
        {
            break;
        }
    }

    if (strlen(p_result))
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
}
else if (argc == 2)
{
    /* AT+WFMAC=<mac> */
    ret = rm_wifi_write_mac_address(argv[1], 1);

    if (ret)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_MAC_ADDR;
    }
}
else
{
    err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
}

return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMAC)
{
    const char * p_usage = "<mac>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMAC)
{
    const char * p_description = "Current MAC_addr Setting/Inquiry";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSPF)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    unsigned int ret           = 0;
    char resp_str[32]          = {0x00, };
    char mac_addr_01[18]       = {0x00, };

    if (argc == 2)
    {
        /* AT+WFSPF=? */
        if (rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
        {
            ret = rm_wifi_get_mac_address_string(WLAN0_IFACE, mac_addr_01, 1);
            memcpy(resp_str, mac_addr_01, strlen(mac_addr_01));
            sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), mac_addr_01);
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        }
        else
        {
            /* AT+WFSPF=<mac> (set only) */
            ret = rm_wifi_write_mac_address(argv[1], 0);
        }

        if (ret)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_MAC_ADDR;
        }
    }
    else if (argc < 2)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSPF)
{
    const char * p_usage = "<mac>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSPF)
{
    const char * p_description = "MAC Spoofing for Station (set only)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFOTP)
{
    int status                 = 0;
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 2)
    {
        /* AT+WFOTP=<mac> (set only) */
        status = rm_wifi_write_mac_address(argv[1], 2);
        if (status)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_MAC_ADDR;
        }
    }
    else if (argc < 2)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFOTP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFOTP)
{
    const char * p_description = "OTP MAC (set only)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSTAT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    WIFIReturnCode_t wifi_err;
    WIFIConnectionInfoExt_t connection_info_ext[2];

    char * p_buf               = NULL;
    const int prefix_reply_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len        = (512 + prefix_reply_len);
    size_t offset              = 0;

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }
    else
    {
        memset(p_buf, 0x00, p_buf_len);

        /* Added prefix for response */
        offset = sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        wifi_err = WIFI_GetConnectionInfoExt(connection_info_ext, 2);
        if (eWiFiSuccess != wifi_err)
        {
            printf("WIFI_GetConnectionInfoExt ERROR=%d\n", wifi_err);
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_STATUS;
        }
        else if (wifi_err == eWiFiSuccess)
        {
            /* Send response */
            update_wifi_stat_buf(connection_info_ext, p_buf, offset);
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSTAT)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSTAT)
{
    const char * p_description = "Get Wi-Fi profile";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSTA)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[16] = {0x00, };

    if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_AP)
 #if defined(__SUPPORT_P2P__)
        || (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P) ||
        (get_run_mode() == WIFI_DEVICE_MODE_EXT_P2P_GO)
 #endif                                // __SUPPORT_P2P__
        )
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }
    else
    {
        if (chk_network_ready(WLAN0_IFACE) == 1)
        {
            sprintf(result_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), 1);
        }
        else
        {
            sprintf(result_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), 0);
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSTA)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSTA)
{
    const char * p_description = "Get STA status";

    return p_description;
}

 #define RM_ATCMD_W_CORE_WIFI_STR_LEN    (32 + (MAX_SSID_LEN + MAX_PASSKEY_LEN))

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPBC)
{
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    wifi_err = WIFI_WpsPbc("any");
    if (wifi_err != eWiFiSuccess)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_WPS_PBC_ANY;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPBC)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPBC)
{
    const char * p_description = "Push WSC Button";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPIN)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    WIFIReturnCode_t wifi_err  = eWiFiSuccess;

    char * p_buf               = NULL;
    char * p_reply             = NULL;
    const int prefix_reply_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len        = (512 + prefix_reply_len);

    p_buf = (char *) pvPortCalloc(p_buf_len, sizeof(char));
    if (p_buf == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_reply = (p_buf + strlen(p_buf));

    if (rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPIN=? */
        wifi_err = WIFI_WpsPin("get", NULL, p_reply);
        if (wifi_err != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_WPS_PIN_GET;
        }
        else
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else if (argc == 1)
    {
        /* AT+WFPIN : generation pin */
        wifi_err = WIFI_WpsPin("any", NULL, p_reply);
        if (wifi_err != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_WPS_PIN_ANY;
        }
        else
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else if ((argc == 2) || (argc == 3))
    {
        /* AT+WFPIN=<pin> : try connecting by pin */
        char str_pin[8 + 1] = {0x00, };
        char str_mac[18]    = {0x00, };
        int pin             = 0;

        if (rm_atcmd_w_core_common_stoi(argv[1], &pin, POL_2) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_WPS_PIN_NUM;
            goto end;
        }

        if (strlen(argv[1]) != 8)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_WPS_PIN_NUM;
            goto end;
        }

        sprintf(str_pin, "%08d", pin);

        printf("[%s:%d]str_pin(%s)\n", __func__, __LINE__, str_pin);

        if (argc == 3)
        {
            // MAC address
            if (!atcmd_chk_valid_macaddr(argv[2]))
            {
                printf("[%s:%d]Invaild mac address\n", __func__, __LINE__);
                err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                goto end;
            }

            bsp_safe_strcpy(str_mac, argv[2], sizeof(str_mac));
        }
        else
        {
            bsp_safe_strcpy(str_mac, "any", sizeof(str_mac));
        }

        wifi_err = WIFI_WpsPin(str_mac, str_pin, NULL);
        if (wifi_err != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_WPS_PIN_NUM;
        }
        else
        {
            size_t reply_remaining = (size_t) (p_buf_len - (int) (p_reply - p_buf));
            bsp_safe_strcpy(p_reply, str_pin, reply_remaining);
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf   = NULL;
        p_reply = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPIN)
{
    const char * p_usage = "<pin>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPIN)
{
    const char * p_description = "Enter WSC PIN/Generate PIN";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCWPS)
{
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    wifi_err = WIFI_WpsCancel();
    if (wifi_err != eWiFiSuccess)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_WPS_CANCEL;
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCWPS)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCWPS)
{
    const char * p_description = "Stop WPS Operation";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    WIFIReturnCode_t ret     = eWiFiSuccess;
    char resp_str[32]        = {0x00, };
    char * p_country_code    = NULL;
    char * p_nv_country_code = NULL;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFCC=? */
        sprintf(resp_str, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_country_code = (resp_str + strlen(resp_str));

        ret = WIFI_GetCountryCode(p_country_code);

        if ((ret != eWiFiSuccess) || (strlen(p_country_code) == 0))
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_COUNTRY_CODE,
                                            &p_nv_country_code);
 #endif

            if (p_nv_country_code && strlen(p_nv_country_code))
            {
                sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), p_nv_country_code);
            }
            else
            {
                sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), DEFAULT_AP_COUNTRY);
            }
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFCC=<country> */
        p_country_code = argv[1];

        if ((strlen(p_country_code) != 2) && (strlen(p_country_code) != 3))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_CC;
        }
        else
        {
            ret = WIFI_SetCountryCode(p_country_code);

            if (ret == eWiFiSuccess)
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_COUNTRY_CODE,
                                                 p_country_code);
 #endif
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_COUNTRY;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCC)
{
    const char * p_usage = "<code>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCC)
{
    const char * p_description = "Set Country Code";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSCAN)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char * p_buf = NULL;
 #if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
    const int p_buf_len = ATCMD_W_RESP_LEN_MAX;
 #else
    const int prefix_reply_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len        = (CLI_SCAN_RSP_BUF_SIZE + prefix_reply_len);
 #endif
    size_t offset = 0;
    int band      = 0;
    int idx;
    uint8_t bss_count = DEFAULT_BSS_MAX_COUNT;

    WIFIReturnCode_t wifi_err;
    WIFIScanExtendedConfig_t pxScanConfigExtended = {0};
 #if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
    bss_count = DEFAULT_BSS_MAX_COUNT_AT_SPI_SDIO;
 #endif
    int scan_res_size              = (bss_count * sizeof(WIFIScanResult_t));
    WIFIScanResult_t * scan_result = pvPortMalloc(scan_res_size);
    if (scan_result == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    if ((argc == 1) || (argc == 2))
    {
        p_buf = (char *) pvPortMalloc(p_buf_len);
        if (!p_buf)
        {
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto end;
        }

        memset(p_buf, 0x00, p_buf_len);

        /* Added prefix for response */
        offset = sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        if (argc == 2)
        {
            if (rm_atcmd_w_core_common_stoi(argv[1], &band, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SCAN_BAND_TYPE;
                goto end;
            }

            if ((band != 2) && (band != 5))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SCAN_BAND_RANGE;
                goto end;
            }
        }

        if (band == 2)
        {
            pxScanConfigExtended.ucBand = eWiFiBand2G;
        }
        else if (band == 5)
        {
            pxScanConfigExtended.ucBand = eWiFiBand5G;
        }
        else
        {
            pxScanConfigExtended.ucBand = eWiFiBandDual;
        }

        wifi_err = WIFI_ScanExtended(scan_result, bss_count, &pxScanConfigExtended);
        if (wifi_err != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
            goto end;
        }

        for (idx = 0; scan_result[idx].ucSSIDLength && idx < bss_count; idx++)
        {
            char ssid[wificonfigMAX_SSID_LEN + 1] = {0};
            const char * p_security_str           = "(Unknown Security)";

            /* BSSID */
            offset += snprintf(p_buf + offset,
                               p_buf_len - offset,
                               "%02x:%02x:%02x:%02x:%02x:%02x\t",
                               scan_result[idx].ucBSSID[0],
                               scan_result[idx].ucBSSID[1],
                               scan_result[idx].ucBSSID[2],
                               scan_result[idx].ucBSSID[3],
                               scan_result[idx].ucBSSID[4],
                               scan_result[idx].ucBSSID[5]);

            /* Signal and Channel */
            offset += snprintf(p_buf + offset,
                               p_buf_len - offset,
                               "%d\t%d\t",
                               scan_result[idx].cRSSI,
                               scan_result[idx].ucChannel);

            /* Security */
            switch ((WIFISecurityExt_t) scan_result[idx].xSecurity)
            {
                case eWiFiSecurityOpen_ext:
                {
                    p_security_str = "(No Security)";
                    break;
                }

                case eWiFiSecurityWEP_ext:
                {
                    p_security_str = "(WEP Security)";
                    break;
                }

                case eWiFiSecurityWPA_ext:
                {
                    p_security_str = "(WPA Security)";
                    break;
                }

                case eWiFiSecurityWPA2_ext:
                {
                    p_security_str = "(WPA2 Security)";
                    break;
                }

                case eWiFiSecurityWPA2_ent_ext:
                {
                    p_security_str = "(WPA2 Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA3_ext:
                {
                    p_security_str = "(WPA3 Security)";
                    break;
                }

                case eWiFiSecurityWPA_ent_ext:
                {
                    p_security_str = "(WPA Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA_WPA2_ent_ext:
                {
                    p_security_str = "(WPA/WPA2 Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA2_WPA3_ent_ext:
                {
                    p_security_str = "(WPA2/WPA3 Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA3_ent_ext:
                {
                    p_security_str = "(WPA3 Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA3_192B_ent_ext:
                {
                    p_security_str = "(WPA3 192B Enterprise Security)";
                    break;
                }

                case eWiFiSecurityWPA_WPA2_ext:
                {
                    p_security_str = "(WPA/WPA2 Security)";
                    break;
                }

                case eWiFiSecurityWPA2_WPA3_ext:
                {
                    p_security_str = "(WPA2/WPA3 Security)";
                    break;
                }

                case eWiFiSecurityWPA3_OWE_ext:
                {
                    p_security_str = "(OWE Security)";
                    break;
                }

                default:
                {
                    p_security_str = "(Unknown Security)";
                    break;
                }
            }

            offset += snprintf(p_buf + offset, p_buf_len - offset, "%s\t", p_security_str);

            /* SSID */
            memcpy(ssid, scan_result[idx].ucSSID, scan_result[idx].ucSSIDLength);
            ssid[scan_result[idx].ucSSIDLength] = '\0';
            offset += snprintf(p_buf + offset, p_buf_len - offset, "%s\n", ssid);
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    if (p_buf)
    {
        vPortFree(scan_result);
        vPortFree(p_buf);
        p_buf       = NULL;
        scan_result = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSCAN)
{
    const char * p_usage = "<band>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSCAN)
{
    const char * p_description = "[STA] Get Scan Results. If <band> is specified : 2(2GHz), 5(5GHz)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFRSSI)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_RSSI_CMD__)
    char result_str[32] = {0x00, };
    int rssi            = -1;

    if (get_sys_mode() != WIFI_DEVICE_MODE_EXT_STATION)
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        rssi = get_current_rssi();

        if (rssi == -9999)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_NOT_CONNECTED;

            bsp_safe_strcpy(result_str, "\r\n+RSSI:NOT_CONN\r\n", sizeof(result_str));

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
            goto end;
        }
        else
        {
            sprintf(result_str, "\r\n+RSSI:%d", rssi);

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
end:
 #endif                                // __SUPPORT_RSSI_CMD__

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFRSSI)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFRSSI)
{
    const char * p_description = "Get current RSSI";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    const int run_mode = get_run_mode();

    char * p_buf              = NULL;
    char * p_resp             = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    char * p_nv_str = NULL;
    int tmp_int     = 0;
    int band        = WPA_SETBAND_AUTO;

    WIFIReturnCode_t wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));

    if (!net_params)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    rm_atcmd_w_core_wifi_init_network_params(net_params);

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (!p_buf)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

        return err;
    }

    memset(p_buf, 0x00, p_buf_len);

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFJAP=? */
        char val_0[33] = {0x00, };
        char val_3[66] = {0x00, };
        int val_1      = 0;
        int val_2      = 0;

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_resp = (p_buf + strlen(p_buf));

        /* Read SSID */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_SSID_0,
                                        &p_nv_str);
 #endif

        if (!p_nv_str || (rm_atcmd_w_core_wifi_cp_str(val_0, sizeof(val_0), p_nv_str, strlen(p_nv_str)) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SSID_NO_VALUE;
            goto end;
        }

        /* Read AUTH Type */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFICFG,
                                        NVR_KEY_AUTH_TYPE_0,
                                        &p_nv_str);
 #endif

        if (p_nv_str == NULL)
        {
            /* Read WEP Key */
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_WEPKEY0_0,
                                            &p_nv_str);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_WEPINDEX_0, &tmp_int) && (p_nv_str == NULL))
 #endif
            {
                val_1 = CC_VAL_AUTH_OPEN;
            }
            else
            {
                val_1 = CC_VAL_AUTH_WEP;
            }
        }
        else if (strcmp(p_nv_str, "WPA-PSK") == 0)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
 #endif

            if (p_nv_str == NULL)
            {
                val_1 = CC_VAL_AUTH_WPA_AUTO;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                val_1 = CC_VAL_AUTH_WPA;
            }
            else
            {
                val_1 = CC_VAL_AUTH_WPA2;
            }
        }
        else if (strcmp(p_nv_str, "WPA-EAP") == 0)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
 #endif

            if (p_nv_str == NULL)
            {
                val_1 = CC_VAL_AUTH_WPA_WPA2_EAP;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                val_1 = CC_VAL_AUTH_WPA_EAP;
            }
            else
            {
                val_1 = CC_VAL_AUTH_WPA2_EAP;
            }

 #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
        }
        else if (strcmp(p_nv_str, "OWE") == 0)
        {
            val_1 = CC_VAL_AUTH_OWE;
        }
        else if (strcmp(p_nv_str, "SAE") == 0)
        {
            val_1 = CC_VAL_AUTH_SAE;
        }
        else if (strcmp(p_nv_str, "WPA-PSK SAE") == 0)
        {
            val_1 = CC_VAL_AUTH_RSN_SAE;
 #endif                                // __SUPPORT_WPA3_PERSONAL_CORE__
        }
        else
        {
            val_1 = CC_VAL_UNKNOWN;
        }

        if (val_1 == CC_VAL_AUTH_OPEN)
        {
            sprintf(p_resp, "'%s',%d", val_0, val_1);
        }
        else if (val_1 == CC_VAL_AUTH_WEP)
        {
 #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_WEPINDEX_0, &tmp_int))
 #endif
            {
                val_2 = 0;
            }
            else
            {
                val_2 = tmp_int;
            }

            if (val_2 == 0)
            {
                /* RRQ61X_CONF_STR_WEP_KEY0 */
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_WEPKEY0_0,
                                                &p_nv_str);
 #endif
            }
            else if (val_2 == 1)
            {
                /* RRQ61X_CONF_STR_WEP_KEY1 */
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFICFG,
                                                NVR_KEY_WEPKEY1_0,
                                                &p_nv_str);
 #endif
            }
            else if (val_2 == 2)
            {
                /* RRQ61X_CONF_STR_WEP_KEY2 */
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFICFG,
                                                NVR_KEY_WEPKEY2_0,
                                                &p_nv_str);
 #endif
            }
            else
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFICFG,
                                                NVR_KEY_WEPKEY3_0,
                                                &p_nv_str);
 #endif
            }

            if (p_nv_str && strlen(p_nv_str))
            {
                if ((strlen(p_nv_str) == (5 + 2)) || (strlen(p_nv_str) == (13 + 2)))
                {
                    memcpy(val_3, (p_nv_str + 1), (strlen(p_nv_str) - 2));
                }
                else if ((strlen(p_nv_str) == 10) || (strlen(p_nv_str) == 26))
                {
                    bsp_safe_strcpy(val_3, p_nv_str, sizeof(val_3));
                }
            }

            sprintf(p_resp, "'%s',%d,%d,'%s'", val_0, val_1, val_2, val_3);
        }
        else if (val_1 > CC_VAL_AUTH_WEP)
        {
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            if (val_1 == CC_VAL_AUTH_OWE)
            {
                sprintf(p_resp, "'%s',%d", val_0, val_1);
            }
            else
            {
                /*  WPA / WPA2 / WPA_AUTO / SAE / RSN_SAE ... */
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFICFG,
                                                NVR_KEY_ENC_TYPE_0,
                                                &p_nv_str);
  #endif

                if (p_nv_str == NULL)
                {
                    val_2 = CC_VAL_ENC_AUTO;
                }
                else if (strcmp(p_nv_str, "TKIP") == 0)
                {
                    val_2 = CC_VAL_ENC_TKIP;
                }
                else
                {
                    val_2 = CC_VAL_ENC_CCMP;
                }

                if ((val_1 == CC_VAL_AUTH_SAE) || (val_1 == CC_VAL_AUTH_RSN_SAE))
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFICFG,
                                                    NVR_KEY_SAE_PASS_0,
                                                    &p_nv_str);
  #endif

                    if (p_nv_str && strlen(p_nv_str))
                    {
                        memcpy(val_3, (p_nv_str + 1), (strlen(p_nv_str) - 2));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_ENCKEY_0,
                                                    &p_nv_str);
  #endif

                    if (p_nv_str && strlen(p_nv_str))
                    {
                        memcpy(val_3, (p_nv_str + 1), (strlen(p_nv_str) - 2));
                    }
                }

                sprintf(p_resp, "'%s',%d,%d,'%s'", val_0, val_1, val_2, val_3);
            }

 #else
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_ENC_TYPE_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str == NULL)
            {
                val_2 = CC_VAL_ENC_AUTO;
            }
            else if (strcmp(p_nv_str, "TKIP") == 0)
            {
                val_2 = CC_VAL_ENC_TKIP;
            }
            else
            {
                val_2 = CC_VAL_ENC_CCMP;
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_ENCKEY_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str && strlen(p_nv_str))
            {
                memcpy(val_3, p_nv_str + 1, strlen(p_nv_str) - 2);
            }
            sprintf(p_resp, "'%s',%d,%d,'%s'", val_0, val_1, val_2, val_3);
 #endif                                // __SUPPORT_WPA3_PERSONAL__
        }

        /* Response */
        if (p_resp && strlen(p_resp))
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        int auth_mode      = 0;
        int is_ssid_hidden = pdTRUE;
        char * p_psk       = NULL;
        size_t psk_len     = 0;
        char input[64]     = {0x00, };

        /* AT+WFJAP=<ssid>,<sec>[<,wep_idx|enc><,key>][,<hidden>] */

        /*
         * OPEN
         * argv[0]  argv[1] argv[2]
         * AT+WFJAP=<ssid>,<sec>
         * ---> argc==3 && sec==0
         *
         * OPEN_hidden
         * argv[0]  argv[1] argv[2] argv[3]
         * AT+WFJAP=<ssid>,<sec>,<1=hidden>
         * ---> argc==4 && sec==0
         *
         *  WEP
         * argv[0]  argv[1] argv[2] argv[3]  argv[4]
         * AT+WFJAP=<ssid>,<sec>,<wep_idx>,<key>
         * ---> argc==5 && sec==1
         *
         *  WPA/WPA2/...
         * argv[0]  argv[1] argv[2] argv[3]  argv[4]
         * AT+WFJAP=<ssid>,<sec>,<enc>,<key>
         * ---> argc==5 && sec==2/3/4
         *
         * WEP_hidden
         * argv[0]  argv[1] argv[2] argv[3]  argv[4] argv[5]
         * AT+WFJAP=<ssid>,<sec>,<wep_idx>,<key>,<hidden>
         * ---> argc==6 && sec==1
         *
         * WPA/WPA2/.... hidden
         * argv[0]  argv[1] argv[2] argv[3]  argv[4] argv[5]
         * AT+WFJAP=<ssid>,<sec>,<enc>,<key>,<1=hidden>
         * ---> argc==6 && sec==2/3/4
         */

        /* Check argc range */
        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (argc > 7)
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }

        /* Check for optional channel argument at the end */
        if (argc >= 4)
        {
            err = rm_atcmd_w_parse_channel_arg(&argc, argv, net_params);
            if (FSP_ERR_AT_CMD_ERR_CMD_OK != err)
            {
                goto end;
            }
        }

        /* Get auth_mode and verify */
        if (rm_atcmd_w_core_common_stoi(argv[2], &auth_mode, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_ARG_TYPE;
            goto end;
        }
        else if (
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            (rm_atcmd_w_core_common_is_in_valid_range(auth_mode, CC_VAL_AUTH_OPEN, CC_VAL_AUTH_RSN_SAE) == pdFALSE)
 #else
            (rm_atcmd_w_core_common_is_in_valid_range(auth_mode, CC_VAL_AUTH_OPEN, CC_VAL_AUTH_WPA_AUTO) == pdFALSE)
 #endif                                // __SUPPORT_WPA3_PERSONAL__
            )
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_ARG_RANGE;
            goto end;
        }

        /* Get <hidden> */
 #if defined(__SUPPORT_WPA3_PERSONAL__)
        if ((auth_mode == CC_VAL_AUTH_OPEN) || (auth_mode == CC_VAL_AUTH_OWE))
 #else
        if (auth_mode == CC_VAL_AUTH_OPEN)
 #endif                                // __SUPPORT_WPA3_PERSONAL__
        {
            if (argc > 4)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_OPEN_TOO_MANY_ARG;
                goto end;
            }

            if (argc == 4)
            {
                /* OPEN / ... HIDDEN */
                if (rm_atcmd_w_core_common_stoi(argv[3], &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_OPEN_HIDDEN_TYPE;
                    goto end;
                }
                else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_OPEN_HIDDEN_RANGE;
                    goto end;
                }
            }
        }
        else
        {
            /* WEP / WPA / WPA2 /.... HIDDEN */
            if (argc == 6)
            {
                if (rm_atcmd_w_core_common_stoi(argv[5], &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_HIDDEN_TYPE;
                    goto end;
                }
                else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_HIDDEN_RANGE;
                    goto end;
                }
            }
        }

        if (auth_mode == CC_VAL_AUTH_WEP)
        {
            /* Validate wep_idx, wep_key */
            if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WEP_IDX_TYPE;
                goto end;
            }
            else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, 0, 3) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WEP_IDX_RANGE;
                goto end;
            }

            tmp_int = strlen(argv[4]);

            if ((tmp_int != 5) && (tmp_int != 13) && (tmp_int != 10) && (tmp_int != 26))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WEP_KEY_LEN;
                goto end;
            }
        }

 #if defined(__SUPPORT_WPA3_PERSONAL__)
        else if (auth_mode == CC_VAL_AUTH_OWE)
        {
            /* Skip ... */
        }
 #endif                                // __SUPPORT_WPA3_PERSONAL__

        else if (auth_mode > CC_VAL_AUTH_WEP)
        {
            /* WPA / WPA2 / WPA1+WPA2 / ... (except OWE) ... */

            /* validate <enc>, <key> */
            if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_MODE_TYPE;
                goto end;
            }
            else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int, CC_VAL_ENC_TKIP, CC_VAL_ENC_AUTO) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_MODE_RANGE;
                goto end;
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            }
            else if (((auth_mode == CC_VAL_AUTH_SAE) || (auth_mode == CC_VAL_AUTH_RSN_SAE)) &&
                     (tmp_int != CC_VAL_ENC_CCMP))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_MODE_RANGE;
                goto end;
 #endif                                // __SUPPORT_WPA3_PERSONAL__
            }
        }

 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                         (int *) &band) != FSP_SUCCESS)
        {
            /* Dual band configuration */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_BAND,
                                          WPA_SETBAND_AUTO);
            net_params->ucBand = eWiFiBandDual;
        }
        else
 #endif
        {
            if (band == WPA_SETBAND_2G)
            {
                net_params->ucBand = eWiFiBand2G;
            }
            else if (band == WPA_SETBAND_5G)
            {
                net_params->ucBand = eWiFiBand5G;
            }
            else
            {
                net_params->ucBand = eWiFiBandDual;
            }
        }

        /*
         * Support Extended ASCII or UTF-8 for SSID
         * Do not use the argv[1] string directly.
         */
        tmp_int = str_decode((u8 *) p_buf, MAX_SSID_LEN, argv[1]);

        if (tmp_int > MAX_SSID_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SSID_LEN;
            goto end;
        }

        /* Support Extended ASCII or UTF-8 for SSID */
        snprintf(input, sizeof(input), "'%s'", p_buf);

        net_params->xNetworkParams.ucSSIDLength = strlen(p_buf);
        memcpy(net_params->xNetworkParams.ucSSID, p_buf, net_params->xNetworkParams.ucSSIDLength);

        snprintf(input, sizeof(input), "\"%s\"", p_buf);
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_0, 1);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_SYS_MODE,
                                      run_mode);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_SSID_0,
                                         input);
 #endif

        switch (auth_mode)
        {
            case CC_VAL_AUTH_OPEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityOpen_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityOpen_ext);
 #endif
                break;
            }

            case CC_VAL_AUTH_WEP:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWEP_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWEP_ext);
 #endif
                break;
            }

            case CC_VAL_AUTH_WPA:
            case CC_VAL_AUTH_WPA2:
            case CC_VAL_AUTH_WPA_AUTO:
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_PSK);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
 #endif

                if (auth_mode == CC_VAL_AUTH_WPA)
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_WPA);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_ext);
 #endif
                }
                else if (auth_mode == CC_VAL_AUTH_WPA2)
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_RSN);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_ext);
 #endif
                }
                else
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_WPA2_ext);
 #endif
                }

                break;
            }

 #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
            case CC_VAL_AUTH_OWE:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_OWE_ext;
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA3_OWE);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
  #endif

                /* Save proto to nvram */
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityNotSupported_ext);
  #endif

                break;
            }

            case CC_VAL_AUTH_SAE:
            case CC_VAL_AUTH_RSN_SAE:
            {
                /* Run set_network 0/1 key_mgmt */
                if (auth_mode == CC_VAL_AUTH_SAE)
                {
                    /* cli set_network 0/1 key_mgmt SAE */
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_ext;
                }
                else
                {
                    /* CC_VAL_AUTH_RSN_SAE */
                    /* cli set_network 0/1 key_mgmt WPA-PSK SAE */
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_WPA3_ext;
                }

                /* STA mode */

                /* Save key_mgmt to nvram */
                if (auth_mode == CC_VAL_AUTH_SAE)
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_0,
                                                     key_mgmt_WPA3_SAE);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA3_ext);
  #endif
                }
                else
                {
                    /* CC_VAL_AUTH_RSN_SAE */
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_0,
                                                     key_mgmt_WPA_PSK_WPA3_SAE);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_WPA3_ext);
  #endif
                }

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, NVR_KEY_WEPINDEX_0);
  #endif

                // save proto to nvram
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
  #endif
                break;
            }
 #endif                                // __SUPPORT_WPA3_PERSONAL_CORE__

            case CC_VAL_AUTH_WPA_EAP:
            case CC_VAL_AUTH_WPA2_EAP:
            case CC_VAL_AUTH_WPA_WPA2_EAP:
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_EAP);
 #endif

                if (auth_mode == CC_VAL_AUTH_WPA_EAP)
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_ent_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_WPA);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_ent_ext);
 #endif
                }
                else if (auth_mode == CC_VAL_AUTH_WPA2_EAP)
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_ent_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_RSN);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_ent_ext);
 #endif
                }
                else
                {
                    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ent_ext;
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_WPA2_ent_ext);
 #endif
                }

                break;
            }
        }

        if (auth_mode == CC_VAL_AUTH_WEP)
        {
            net_params->xNetworkParams.ucDefaultWEPKeyIndex = atoi(argv[3]);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0,
                                          atoi(argv[3]));
 #endif

            char tmp_str[64];
            char cmd[32] = {0, };

            if ((strlen(argv[4]) == 5) || (strlen(argv[4]) == 13))
            {
                sprintf(input, "'%s'", argv[4]);
            }
            else if ((strlen(argv[4]) == 10) || (strlen(argv[4]) == 26))
            {
                bsp_safe_strcpy(input, argv[4], sizeof(input));
            }
            else
            {
                return CC_FAILURE_STRING_LENGTH;
            }

            tmp_int = atoi(argv[3]);
            sprintf(cmd, "wep_key%d", tmp_int);

            net_params->xNetworkParams.xPassword.xWEP[0].ucLength = strlen(argv[4]);
            memcpy(net_params->xNetworkParams.xPassword.xWEP[0].cKey,
                   argv[4],
                   net_params->xNetworkParams.xPassword.xWEP[0].ucLength);

            if ((strlen(argv[4]) == 5) || (strlen(argv[4]) == 13))
            {
                sprintf(input, "\"%s\"", argv[4]);
            }

            sprintf(tmp_str, "N0_%s", cmd);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, tmp_str, input); // We only have tmp_str as WIFI_PROFILE_WEPKEY0_0
 #endif
        }

 #if defined(__SUPPORT_WPA3_PERSONAL__)
        else if (auth_mode == CC_VAL_AUTH_OWE)
        {
            char inputa[66] = {0, };

            /* CCMP */
            bsp_safe_strcpy(inputa, "CCMP", sizeof(inputa));
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFICFG,
                                             NVR_KEY_ENC_TYPE_0,
                                             inputa);
  #endif

            /* PMF */
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0, 2);
  #endif
        }
 #endif                                // __SUPPORT_WPA3_PERSONAL__
        else if (auth_mode > CC_VAL_AUTH_WEP)
        {
            char inputb[wificonfigMAX_PSK_LEN + 3] = {0, };
 #if defined(__SUPPORT_WPA3_PERSONAL__)

            /* WPA/WPA2/WPA3 ... */

            if ((auth_mode == CC_VAL_AUTH_SAE) || (auth_mode == CC_VAL_AUTH_RSN_SAE))
            {
                /* cli set_network 0 pairwise    ccmp */
                bsp_safe_strcpy(inputb, "CCMP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 inputb);
  #endif

                /* Default sae_groups is used, no need to save */

                /* cli set_network 0 ieee80211w x */
                if (auth_mode == CC_VAL_AUTH_SAE)
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_PMF_0,
                                                  2);
  #endif
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_PMF_0,
                                                  1);
  #endif
                }

                /* <key> */
                p_psk   = argv[4];
                psk_len = strlen(p_psk);

                if ((psk_len < 8) || (psk_len > wificonfigMAX_PSK_LEN))
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_KEY_LEN;
                    goto end;
                }

                net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
                memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                       p_psk,
                       net_params->xNetworkParams.xPassword.xWPA.ucLength);

                snprintf(inputb, sizeof(inputb), "\"%s\"", p_psk);

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 inputb);
  #endif
            }
            else
            {
                /* <enc> */
                switch (atoi(argv[3]))
                {
                    case CC_VAL_ENC_TKIP:
                    {
                        bsp_safe_strcpy(inputb, "TKIP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFICFG,
                                                         NVR_KEY_ENC_TYPE_0,
                                                         inputb);
  #endif
                        break;
                    }

                    case CC_VAL_ENC_CCMP:
                    {
                        bsp_safe_strcpy(inputb, "CCMP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFICFG,
                                                         NVR_KEY_ENC_TYPE_0,
                                                         inputb);
  #endif
                        break;
                    }

                    case CC_VAL_ENC_AUTO:
                    {
                        bsp_safe_strcpy(inputb, "TKIP CCMP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG,
                                                  NVR_KEY_ENC_TYPE_0);
  #endif
                        break;
                    }
                }

                /* <key> */
                p_psk   = argv[4];
                psk_len = strlen(p_psk);
                if ((psk_len < 8) || (psk_len > wificonfigMAX_PSK_LEN))
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_KEY_LEN;
                    goto end;
                }

                net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
                memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                       p_psk,
                       net_params->xNetworkParams.xPassword.xWPA.ucLength);

                snprintf(inputb, sizeof(inputb), "\"%s\"", p_psk);

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 inputb);
  #endif
            }

 #else

            /* <enc> */
            switch (atoi(argv[3]))
            {
                case CC_VAL_ENC_TKIP:
                {
                    bsp_safe_strcpy(inputb, "TKIP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_ENC_TYPE_0,
                                                     inputb);
  #endif
                    break;
                }

                case CC_VAL_ENC_CCMP:
                {
                    bsp_safe_strcpy(inputb, "CCMP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_ENC_TYPE_0,
                                                     inputb);
  #endif
                    break;
                }

                case CC_VAL_ENC_AUTO:
                {
                    bsp_safe_strcpy(inputb, "TKIP CCMP", sizeof(inputb));
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif
                    break;
                }
            }

            /* <key> */
            p_psk   = argv[4];
            psk_len = strlen(p_psk);
            if ((psk_len < 8) || (psk_len > wificonfigMAX_PSK_LEN))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_KEY_LEN;
                goto end;
            }

            net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
            memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                   p_psk,
                   net_params->xNetworkParams.xPassword.xWPA.ucLength);

            snprintf(inputb, sizeof(inputb), "\"%s\"", p_psk);

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_ENCKEY_0,
                                             inputb);
  #endif
 #endif                                // __SUPPORT_WPA3_PERSONAL__
        }

        /* Process hidden */
        if (is_ssid_hidden == pdTRUE)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_HIDDEN_SSID,
                                          (int) is_ssid_hidden);
 #endif
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_HIDDEN_SSID);
 #endif
        }

        net_params->hidden_ssid = is_ssid_hidden;

        wifi_err = WIFI_ConnectAPExt(net_params);
        if (wifi_err)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SELECT_NETWORK;
            WIFI_Disconnect();
            goto end;
        }
        else
        {
            /* After giving all the parameters set the profile as complete so that on next reboot it will fetch the value
             * from persistant memory and connect , only in case of connect AP CMD's we will set this to true
             */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_COMPLETE,
                                          1);

            char resp_str[16] = "\r\nOK\r\n";

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

            err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;

            atcmd_rsp_wifi_conn(p_at_ctrl);
        }
    }

end:

    /* Free dynamically allocated channel list */
    if (net_params->pucChannelList != NULL)
    {
        vPortFree(net_params->pucChannelList);
        net_params->pucChannelList = NULL;
    }

    if (net_params)
    {
        vPortFree(net_params);
        net_params = NULL;
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAP)
{
    const char * p_usage = "<ssid>,<auth>,<enc>,<key>[,<hidden>][,'ch=<channel1>[,<channel2>']]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAP)
{
    const char * p_description = "[STA] Connect to AP";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPA)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char * p_nv_str = NULL;

    char * p_buf              = NULL;
    char * p_resp             = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    const int run_mode = get_run_mode();
    int band           = WPA_SETBAND_AUTO;

    WIFIReturnCode_t wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));

    if (!net_params)
    {
        goto end;
    }

    rm_atcmd_w_core_wifi_init_network_params(net_params);

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (!p_buf)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

        return err;
    }

    memset(p_buf, 0x00, p_buf_len);

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFJAPA=? */
        char val_0[33] = {0x00, };
        char val_1[66] = {0x00, };
        int temp       = 0;

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_resp = (p_buf + strlen(p_buf));

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_SSID_0,
                                        &p_nv_str);
 #endif

        if (!p_nv_str || (rm_atcmd_w_core_wifi_cp_str(val_0, sizeof(val_0), p_nv_str, strlen(p_nv_str)) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_SSID_NO_VALUE;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFICFG,
                                        NVR_KEY_AUTH_TYPE_0,
                                        &p_nv_str);
 #endif

        if (p_nv_str == NULL)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_WEPKEY0_0,
                                            &p_nv_str);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_WEPINDEX_0, &temp) && (p_nv_str == NULL))
 #endif
            {
                temp = CC_VAL_AUTH_OPEN;
            }
            else
            {
                temp = CC_VAL_AUTH_WEP;
            }
        }
        else if (strcmp(p_nv_str, "WPA-PSK") == 0)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
 #endif

            if (p_nv_str == NULL)
            {
                temp = CC_VAL_AUTH_WPA_AUTO;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                temp = CC_VAL_AUTH_WPA;
            }
            else
            {
                temp = CC_VAL_AUTH_WPA2;
            }
        }
        else if (strcmp(p_nv_str, "WPA-EAP") == 0)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
 #endif

            if (p_nv_str == NULL)
            {
                temp = CC_VAL_AUTH_WPA_WPA2_EAP;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                temp = CC_VAL_AUTH_WPA_EAP;
            }
            else
            {
                temp = CC_VAL_AUTH_WPA2_EAP;
            }

 #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
        }
        else if (strcmp(p_nv_str, "OWE") == 0)
        {
            temp = CC_VAL_AUTH_OWE;
        }
        else if (strcmp(p_nv_str, "SAE") == 0)
        {
            temp = CC_VAL_AUTH_SAE;
        }
        else if (strcmp(p_nv_str, "WPA-PSK SAE") == 0)
        {
            temp = CC_VAL_AUTH_RSN_SAE;
 #endif                                // __SUPPORT_WPA3_PERSONAL_CORE__
        }
        else
        {
            temp = CC_VAL_UNKNOWN;
        }

        if (temp == CC_VAL_AUTH_OPEN)
        {
            sprintf(p_resp, "'%s'", val_0);
        }
        else if (temp == CC_VAL_AUTH_WEP)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WEP_NOT_SUPPORT;
            goto end;
        }
        else if (temp > CC_VAL_AUTH_WEP)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_ENCKEY_0,
                                            &p_nv_str);
 #endif

            if (p_nv_str)
            {
                rm_atcmd_w_core_wifi_cp_str(val_1, sizeof(val_1), p_nv_str, strlen(p_nv_str));
            }

            sprintf(p_resp, "'%s','%s'", val_0, val_1);
        }

        if (p_resp && strlen(p_resp))
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        char * ssid        = NULL;
        char * p_psk       = NULL;
        size_t psk_len     = 0;
        int is_ssid_hidden = pdTRUE;
        char input[wificonfigMAX_PSK_LEN + 3]     = {0x00, };

        enum _ap_sec_type
        {
            OPEN,
            OPEN_HIDDEN,
            WPA,
            WPA_HIDDEN
        } ap_sec_type;
        ap_sec_type = OPEN;
        unsigned short idx_adj = 0;

        /* AT+WFJAPA=<ssid>[,<psk>][,<hidden>] */

        /*
         * OPEN
         * argv[0]   argv[1]
         * AT+WFJAPA=<ssid>
         * ---> argc=2
         *
         * OPEN + hidden
         * argv[0]   argv[1] argv[2]
         * AT+WFJAPA=<ssid>,<hidden>
         * ---> argc=3, strlen(argv[2])==1
         *
         * WPA ...
         * argv[0]  argv[1] argv[2]
         * AT+WFJAPA=<ssid>,<psk>
         * ---> argc=3
         *
         * WPA + hidden
         * argv[0]   argv[1] argv[2] argv[3]
         * AT+WFJAPA=<ssid>,<psk>,<hidden>
         * ---> argc=4
         */

        /* Check argc range */
        if (argc < (2 + idx_adj))
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (argc > (5 + idx_adj))
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }

        if (argc >= (3 + idx_adj))
        {
            err = rm_atcmd_w_parse_channel_arg(&argc, argv, net_params);
            if (FSP_ERR_AT_CMD_ERR_CMD_OK != err)
            {
                goto end;
            }
        }

        ssid = argv[1 + idx_adj];

        /*
         * Support Extended ASCII or UTF-8 for SSID
         * Do not use the argv[1] string directly.
         */

        memset(p_buf, 0x00, p_buf_len);

        if (str_decode((u8 *) p_buf, MAX_SSID_LEN, ssid) > MAX_SSID_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_SSID_LEN;
            goto end;
        }

        // Decide ap_sec_type
        if (argc == 2 + idx_adj)
        {
            /* AT+WFJAPA=<ssid> */
            ap_sec_type = OPEN_HIDDEN;
        }
        else if (argc == (3 + idx_adj))
        {
            char * temp_str = argv[2 + idx_adj];

            if (strlen(temp_str) == 1)
            {
                /* psk len should be over 8 characters, hence, this is hidden field */

                /* AT+WFJAPA=<ssid>,<hidden> */
                if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_TYPE;
                    goto end;
                }
                else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_TYPE;
                    goto end;
                }

                if (is_ssid_hidden == pdTRUE)
                {
                    ap_sec_type = OPEN_HIDDEN;
                }
                else
                {
                    ap_sec_type = OPEN;
                }
            }
            else
            {
                /* AT+WFJAPA=<ssid>,<psk> */
                ap_sec_type = WPA_HIDDEN;
            }
        }
        else if (argc == 4 + idx_adj)
        {
            char * temp_str = argv[3 + idx_adj];

            /* AT+WFJAPA=<ssid>,<psk>,<hidden> */
            if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_TYPE;
                goto end;
            }
            else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_RANGE;
                goto end;
            }

            if (is_ssid_hidden == pdTRUE)
            {
                ap_sec_type = WPA_HIDDEN;
            }
            else
            {
                ap_sec_type = WPA;
            }
        }

        /* Verify psk */
        switch (ap_sec_type)
        {
            case WPA:
            case WPA_HIDDEN:
            {
                p_psk   = argv[2 + idx_adj];
                psk_len = strlen(p_psk);

                if ((psk_len < 8) || (psk_len > wificonfigMAX_PSK_LEN))
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_PSK_LEN;
                    goto end;
                }

                break;
            }

            default:
            {
                break;
            }
        }

        /* Support Extended ASCII or UTF-8 for SSID */
        snprintf(input, sizeof(input), "'%s'", p_buf);

        net_params->xNetworkParams.ucSSIDLength = strlen(p_buf);
        memcpy(net_params->xNetworkParams.ucSSID, p_buf, net_params->xNetworkParams.ucSSIDLength);

        snprintf(input, sizeof(input), "\"%s\"", p_buf);

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_0, 1);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_SYS_MODE,
                                      run_mode);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_SSID_0,
                                         input);
 #endif

        /* Set config parameters per ap_sec_type */
        switch (ap_sec_type)
        {
            case OPEN:
            case OPEN_HIDDEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityOpen_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityOpen_ext);
 #endif
                break;
            }

            case WPA:
            case WPA_HIDDEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_PSK);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif

                bsp_safe_strcpy(input, "TKIP CCMP", sizeof(input));
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif

                net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
                memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                       p_psk,
                       net_params->xNetworkParams.xPassword.xWPA.ucLength);

                snprintf(input, sizeof(input), "\"%s\"", p_psk);
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 input);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA_WPA2_ext);
 #endif
                break;
            }

            default:
            {
                break;
            }
        }

        /* Process hidden */
        switch (ap_sec_type)
        {
            case OPEN_HIDDEN:
            case WPA_HIDDEN:
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_HIDDEN_SSID,
                                              (int) is_ssid_hidden);
 #endif
                break;
            }

            default:
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_HIDDEN_SSID);
 #endif
                break;
            }
        }

        net_params->hidden_ssid = is_ssid_hidden;

 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                         (int *) &band) != FSP_SUCCESS)
        {
            /* Dual band configuration */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_BAND,
                                          WPA_SETBAND_AUTO);
            net_params->ucBand = eWiFiBandDual;
        }
        else
 #endif
        {
            if (band == WPA_SETBAND_2G)
            {
                net_params->ucBand = eWiFiBand2G;
            }
            else if (band == WPA_SETBAND_5G)
            {
                net_params->ucBand = eWiFiBand5G;
            }
            else
            {
                net_params->ucBand = eWiFiBandDual;
            }
        }

        wifi_err = WIFI_ConnectAPExt(net_params);
        if (wifi_err)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SELECT_NETWORK;
            WIFI_Disconnect();
            goto end;
        }
        else
        {
            /* After giving all the parameters set the profile as complete so that on next reboot it will fetch the value
             * from persistant memory and connect , only in case of connect AP CMD's we will set this to true
             */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_COMPLETE,
                                          1);

            char resp_str[16] = "\r\nOK\r\n";

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

            err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;

            atcmd_rsp_wifi_conn(p_at_ctrl);
        }
    }

end:

    /* Free dynamically allocated channel list */
    if (net_params->pucChannelList != NULL)
    {
        vPortFree(net_params->pucChannelList);
        net_params->pucChannelList = NULL;
    }

    if (net_params)
    {
        vPortFree(net_params);
        net_params = NULL;
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPA)
{
    const char * p_usage = "<ssid>[,<key>][,<hidden>][,'ch=<channel1>[,<channel2>']]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPA)
{
    const char * p_description = "[STA] Connect to WPA/WPA2-AP with SSID/PSK only";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPA3)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_WPA3_PERSONAL__)
    char * p_nv_str = NULL;

    char * p_buf              = NULL;
    char * p_resp             = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    const int run_mode = get_run_mode();
    int band           = WPA_SETBAND_AUTO;

    WIFIReturnCode_t wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));

    if (!net_params)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    rm_atcmd_w_core_wifi_init_network_params(net_params);

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (!p_buf)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

        return err;
    }

    memset(p_buf, 0x00, p_buf_len);

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFJAPA3=? */
        char val_0[33] = {0x00, };
        char val_1[66] = {0x00, };
        int temp       = 0;

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_resp = (p_buf + strlen(p_buf));

        /* Read SSID */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_SSID_0,
                                        &p_nv_str);
  #endif

        if (!p_nv_str || (rm_atcmd_w_core_wifi_cp_str(val_0, sizeof(val_0), p_nv_str, strlen(p_nv_str)) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_SSID_NO_VALUE;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFICFG,
                                        NVR_KEY_AUTH_TYPE_0,
                                        &p_nv_str);
  #endif

        if (p_nv_str == NULL)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_WEPKEY0_0,
                                            &p_nv_str);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_WEPINDEX_0, &temp) && (p_nv_str == NULL))
  #endif
            {
                temp = CC_VAL_AUTH_OPEN;
            }
            else
            {
                temp = CC_VAL_AUTH_WEP;
            }
        }
        else if (strcmp(p_nv_str, "WPA-PSK") == 0)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str == NULL)
            {
                temp = CC_VAL_AUTH_WPA_AUTO;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                temp = CC_VAL_AUTH_WPA;
            }
            else
            {
                temp = CC_VAL_AUTH_WPA2;
            }
        }
        else if (strcmp(p_nv_str, "WPA-EAP") == 0)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str == NULL)
            {
                temp = CC_VAL_AUTH_WPA_WPA2_EAP;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                temp = CC_VAL_AUTH_WPA_EAP;
            }
            else
            {
                temp = CC_VAL_AUTH_WPA2_EAP;
            }

  #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
        }
        else if (strcmp(p_nv_str, "OWE") == 0)
        {
            temp = CC_VAL_AUTH_OWE;
        }
        else if (strcmp(p_nv_str, "SAE") == 0)
        {
            temp = CC_VAL_AUTH_SAE;
        }
        else if (strcmp(p_nv_str, "WPA-PSK SAE") == 0)
        {
            temp = CC_VAL_AUTH_RSN_SAE;
  #endif                               // __SUPPORT_WPA3_PERSONAL_CORE__
        }
        else
        {
            temp = CC_VAL_UNKNOWN;
        }

        if (temp == CC_VAL_AUTH_OPEN)
        {
            sprintf(p_resp, "'%s'", val_0);
        }
        else if (temp == CC_VAL_AUTH_WEP)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WEP_NOT_SUPPORT;
            goto end;
        }
        else if (temp > CC_VAL_AUTH_WEP)
        {
            if ((temp == CC_VAL_AUTH_SAE) || (temp == CC_VAL_AUTH_RSN_SAE))
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_ENCKEY_0,
                                                &p_nv_str);
  #endif

                if (p_nv_str && strlen(p_nv_str))
                {
                    memcpy(val_1, (p_nv_str + 1), (strlen(p_nv_str) - 2));
                }
            }
            else
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_ENCKEY_0,
                                                &p_nv_str);
  #endif

                if (p_nv_str && strlen(p_nv_str))
                {
                    memcpy(val_1, (p_nv_str + 1), (strlen(p_nv_str) - 2));
                }
            }

            sprintf(p_resp, "'%s','%s'", val_0, val_1);
        }

        if (p_resp && strlen(p_resp))
        {
            /* Send response */
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        char * ssid            = NULL;
        char * p_psk           = NULL;
        size_t psk_len         = 0;
        int is_ssid_hidden     = pdTRUE;
        int is_wpa3            = 0;
        unsigned short idx_adj = 1;
        char input[wificonfigMAX_PSK_LEN + 3]         = {0x00, };
        enum _ap_sec_type
        {
            OPEN,
            OPEN_HIDDEN,
            WPA,
            WPA_HIDDEN,
            OWE,
            OWE_HIDDEN,
            WPA3,
            WPA3_HIDDEN
        } ap_sec_type;

        ap_sec_type = OPEN;

        /*
         * AT+WFJAPA3=<is_wpa3>,<ssid>[,<key>][,<hidden>]
         *
         * OPEN
         * argv[0]   argv[1]   argv[2]
         * AT+WFJAPA3=<is_wpa3>,<ssid>
         * ---> argc=3, is_wpa3=0
         *
         * OPEN+hidden
         * argv[0]   argv[1]   argv[2] argv[3]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<hidden>
         * ---> argc=4, is_wpa3=0 strlen(argv[3])==1
         *
         * WPA
         * argv[0]  argv[1]  argv[2]  argv[3]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<psk>
         * ---> argc=4, is_wpa3=0, strlen(argv[3])>1
         *
         * WPA+hidden
         * argv[0]  argv[1]  argv[2]  argv[3] argv[4]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<psk>,<hidden>
         * ---> argc=5, is_wpa3=0
         *
         * OWE
         * argv[0]   argv[1]   argv[2]
         * AT+WFJAPA3=<is_wpa3>,<ssid>
         * ---> argc=3, is_wpa3=1
         *
         * OWE+hidden
         * argv[0]   argv[1]   argv[2] argv[3]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<hidden>
         * ---> argc=4, is_wpa3=1 strlen(argv[3])==1
         *
         * WPA3 (SAE)
         * argv[0]  argv[1]  argv[2]  argv[3]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<psk>
         * ---> argc=4, is_wpa3=1, strlen(argv[3])>1
         *
         * WPA3+hidden
         * argv[0]  argv[1]  argv[2]  argv[3] argv[4]
         * AT+WFJAPA3=<is_wpa3>,<ssid>,<psk>,<hidden>
         * ---> argc=5, is_wpa3=1
         */

        /* Check argc range */
        if (argc < (2 + idx_adj))
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }
        else if (argc > (5 + idx_adj))
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
            goto end;
        }

        if (argc >= (3 + idx_adj))
        {
            err = rm_atcmd_w_parse_channel_arg(&argc, argv, net_params);
            if (FSP_ERR_AT_CMD_ERR_CMD_OK != err)
            {
                goto end;
            }
        }

        if (rm_atcmd_w_core_common_stoi(argv[idx_adj], &is_wpa3, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_MODE_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(is_wpa3, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_MODE_RANGE;
            goto end;
        }

        ssid = argv[1 + idx_adj];

        /*
         * Support Extended ASCII or UTF-8 for SSID
         * Do not use the argv[1] string directly.
         */
        memset(p_buf, 0x00, p_buf_len);

        if (str_decode((u8 *) p_buf, MAX_SSID_LEN, ssid) > MAX_SSID_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_SSID_LEN;
            goto end;
        }

        // Decide ap_sec_type
        if (argc == 2 + idx_adj)
        {
            if (is_wpa3 == pdTRUE)
            {
                /* AT+WFJAPA3=<is_wpa>,<ssid> */
                ap_sec_type = OWE_HIDDEN;
            }
            else
            {
                /* AT+WFJAPA3=<ssid> */
                ap_sec_type = OPEN_HIDDEN;
            }
        }
        else if (argc == (3 + idx_adj))
        {
            char * temp_str = argv[2 + idx_adj];

            if (strlen(temp_str) == 1)
            {
                /* psk len should be over 8 characters, hence, this is hidden field */

                if (is_wpa3 == pdTRUE)
                {
                    /* AT+WFJAPA3=<is_wpa>,<ssid>,<hidden> */
                    if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_HIDDEN_TYPE;
                        goto end;
                    }
                    else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_HIDDEN_RANGE;
                        goto end;
                    }

                    if (is_ssid_hidden == pdTRUE)
                    {
                        ap_sec_type = OWE_HIDDEN;
                    }
                    else
                    {
                        ap_sec_type = OWE;
                    }
                }
                else
                {
                    /* AT+WFJAPA3=<ssid>,<hidden> */
                    if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_TYPE;
                        goto end;
                    }
                    else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_RANGE;
                        goto end;
                    }

                    if (is_ssid_hidden == pdTRUE)
                    {
                        ap_sec_type = OPEN_HIDDEN;
                    }
                    else
                    {
                        ap_sec_type = OPEN;
                    }
                }
            }
            else
            {
                if (is_wpa3 == pdTRUE)
                {
                    /* AT+WFJAPA3=<is_wpa>,<ssid>,<psk> */
                    ap_sec_type = WPA3_HIDDEN;
                }
                else
                {
                    /* AT+WFJAPA3=<ssid>,<psk> */
                    ap_sec_type = WPA;
                }
            }
        }
        else if (argc == 4 + idx_adj)
        {
            char * temp_str = argv[3 + idx_adj];

            if (is_wpa3 == pdTRUE)
            {
                /* AT+WFJAPA3=<is_wpa3>,<ssid>,<psk>,<hidden> */
                if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_HIDDEN_TYPE;
                    goto end;
                }
                else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_HIDDEN_RANGE;
                    goto end;
                }

                if (is_ssid_hidden == pdTRUE)
                {
                    ap_sec_type = WPA3_HIDDEN;
                }
                else
                {
                    ap_sec_type = WPA3;
                }
            }
            else
            {
                /* AT+WFJAPA3=<ssid>,<psk>,<hidden> */
                if (rm_atcmd_w_core_common_stoi(temp_str, &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_TYPE;
                    goto end;
                }
                else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_HIDDEN_RANGE;
                    goto end;
                }

                if (is_ssid_hidden == pdTRUE)
                {
                    ap_sec_type = WPA_HIDDEN;
                }
                else
                {
                    ap_sec_type = WPA;
                }
            }
        }

        switch (ap_sec_type)
        {
            case WPA:
            case WPA_HIDDEN:
            case WPA3:
            case WPA3_HIDDEN:
            {
                p_psk   = argv[2 + idx_adj];
                psk_len = strlen(p_psk);

                if ((psk_len < 8) || (psk_len > wificonfigMAX_PSK_LEN))
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_JAPA_WPA3_PSK_LEN;
                    goto end;
                }

                break;
            }

            default:
            {
                break;
            }
        }

        /* Support Extended ASCII or UTF-8 for SSID */
        snprintf(input, sizeof(input), "'%s'", p_buf);

        net_params->xNetworkParams.ucSSIDLength = strlen(p_buf);
        memcpy(net_params->xNetworkParams.ucSSID, p_buf, net_params->xNetworkParams.ucSSIDLength);

        snprintf(input, sizeof(input), "\"%s\"", p_buf);
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_0, 1);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_SYS_MODE,
                                      run_mode);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_SSID_0,
                                         input);
  #endif

        // set config parameters per ap_sec_type
        switch (ap_sec_type)
        {
            case OPEN:
            case OPEN_HIDDEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityOpen_ext;
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, NVR_KEY_ENCKEY_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityOpen_ext);
  #endif

                break;
            }

            case WPA:
            case WPA_HIDDEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_PSK);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif

                bsp_safe_strcpy(input, "TKIP CCMP", sizeof(input));
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif

                net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
                memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                       p_psk,
                       net_params->xNetworkParams.xPassword.xWPA.ucLength);

                snprintf(input, sizeof(input), "\"%s\"", p_psk);

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 input);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA_WPA2_ext);
  #endif

                break;
            }

            case WPA3:
            case WPA3_HIDDEN:
            {
                char inputa[wificonfigMAX_PSK_LEN + 3] = {0, };

                /* connect as WPA2+WPA3(SAE) ... */

                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_ext;

                /* CC_VAL_AUTH_RSN_SAE */
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_PSK_WPA3_SAE);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);
  #endif

                /* save proto to nvram */
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
  #endif

                bsp_safe_strcpy(inputa, "CCMP", sizeof(inputa));
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 inputa);
  #endif

                /* cli set_network 0 sae_password ********* */
                snprintf(inputa, sizeof(inputa), "'%s'", p_psk);

                net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
                memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                       p_psk,
                       net_params->xNetworkParams.xPassword.xWPA.ucLength);

                snprintf(inputa, sizeof(inputa), "\"%s\"", p_psk);

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 inputa);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA3_ext);
  #endif

                /* PMF */
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_PMF_0,
                                              2);
  #endif

                break;
            }

            case OWE:
            case OWE_HIDDEN:
            {
                /* Connect as WPA3 OWE */
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_OWE_ext;

  #ifdef RM_MAP_PERSISTANT_W
                char inputb[66] = {0, };

                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA3_OWE);
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPKEY0_0);

                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");

                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");

                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");

                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WEPINDEX_0);

                /* Save proto to nvram */
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);

                /* CCMP */
                bsp_safe_strcpy(inputb, "CCMP", sizeof(inputb));
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 inputb);

                /* PMF */
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_PMF_0,
                                              2);

                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA3_OWE_ext);
  #endif
                break;
            }

            default:
            {
                break;
            }
        }

        /* Process hidden */
        switch (ap_sec_type)
        {
            case OPEN_HIDDEN:
            case WPA_HIDDEN:
            case OWE_HIDDEN:
            case WPA3_HIDDEN:
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_HIDDEN_SSID,
                                              (int) is_ssid_hidden);
  #endif
                break;
            }

            default:
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_HIDDEN_SSID);
                break;
            }
  #endif
            }

                net_params->hidden_ssid = is_ssid_hidden;

  #ifdef RM_MAP_PERSISTANT_W
                if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_BAND, (int *) &band) != FSP_SUCCESS)
                {
                    /* Dual band configuration */
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_BAND,
                                                  WPA_SETBAND_AUTO);
                    net_params->ucBand = eWiFiBandDual;
                }
                else
  #endif
                {
                    if (band == WPA_SETBAND_2G)
                    {
                        net_params->ucBand = eWiFiBand2G;
                    }
                    else if (band == WPA_SETBAND_5G)
                    {
                        net_params->ucBand = eWiFiBand5G;
                    }
                    else
                    {
                        net_params->ucBand = eWiFiBandDual;
                    }
                }

                wifi_err = WIFI_ConnectAPExt(net_params);
                if (wifi_err)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SELECT_NETWORK;
                    WIFI_Disconnect();
                    goto end;
                }
                else
                {
                    /* After giving all the parameters set the profile as complete so that on next reboot it will fetch the value
                     * from persistant memory and connect , only in case of connect AP CMD's we will set this to true
                     */
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_COMPLETE,
                                                  1);

                    char resp_str[16] = "\r\nOK\r\n";

                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

                    err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;

                    atcmd_rsp_wifi_conn(p_at_ctrl);
                }
        }

end:

        if (p_buf)
        {
            vPortFree(p_buf);
            p_buf = NULL;
        }

        /* Free dynamically allocated channel list */
        if (net_params->pucChannelList != NULL)
        {
            vPortFree(net_params->pucChannelList);
            net_params->pucChannelList = NULL;
        }

        if (net_params)
        {
            vPortFree(net_params);
            net_params = NULL;
        }
 #endif                                // __SUPPORT_WPA3_PERSONAL__

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPA3)
{
    const char * p_usage = "<wpa3_flag>,<ssid>[,<key>][,<hidden>][,'ch=<channel1>[,<channel2>']]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPA3)
{
    const char * p_description = "[STA] Connect to WPA/WPA2/WPA3-AP with SSID/PSK only";

    return p_description;
}

/**********************************************************************************************************************//**
 * AT+WFJAPMK=<ssid>,<auth>,<pmk>[,<hidden>][,'ch=<channel>']
 *   <ssid>   : SSID of the target AP (UTF-8 / Extended ASCII supported)
 *   <auth>   : 2=WPA, 3=WPA2, 4=WPA/WPA2 auto (PSK modes only)
 *   <pmk>    : Pre-computed PMK - exactly 64 hexadecimal characters
 *   <hidden> : Optional. 1=hidden SSID scan (default: 1)
 *   ch=<n>   : Optional channel hint
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFJAPMK)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    const int run_mode = get_run_mode();

    char * p_buf              = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    int auth_mode      = 0;
    int band           = WPA_SETBAND_AUTO;
    int is_ssid_hidden = pdTRUE;
    int tmp_int        = 0;
    size_t psk_len     = 0;
    char input[RM_ATCMD_W_CORE_WIFI_PMK_HEX_STR_LEN + 2] = {0};

    WIFIReturnCode_t wifi_err           = 0;
    WIFINetworkParamsExt_t * net_params = NULL;

    /* Require at least 3 user arguments: <ssid>, <auth>, <pmk> (argc includes argv[0]) */
    if (argc < 4)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        goto end;
    }

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!net_params)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    rm_atcmd_w_core_wifi_init_network_params(net_params);

    p_buf = (char *) pvPortMalloc(p_buf_len);
    if (!p_buf)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, p_buf_len);

    /* Strip optional 'ch=<n>' argument if present */
    if (argc >= 5)
    {
        err = rm_atcmd_w_parse_channel_arg(&argc, argv, net_params);
        if (FSP_ERR_AT_CMD_ERR_CMD_OK != err)
        {
            goto end;
        }
    }

    /* After channel stripping: argc==4 (ssid,auth,pmk) or argc==5 (ssid,auth,pmk,hidden) */
    if (argc > 5)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        goto end;
    }

    /* <ssid>: decode UTF-8 / Extended ASCII — do not use argv[1] directly */
    tmp_int = str_decode((u8 *) p_buf, MAX_SSID_LEN, argv[1]);
    if (tmp_int == 0)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SSID_NO_VALUE;
        goto end;
    }

    if (tmp_int > MAX_SSID_LEN)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SSID_LEN;
        goto end;
    }

    net_params->xNetworkParams.ucSSIDLength = (uint8_t) tmp_int;
    memcpy(net_params->xNetworkParams.ucSSID, p_buf, net_params->xNetworkParams.ucSSIDLength);

    /* <auth>: 2=WPA, 3=WPA2, 4=WPA/WPA2 auto (PSK modes only) */
    if (rm_atcmd_w_core_common_stoi(argv[2], &auth_mode, POL_1) != 0)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_ARG_TYPE;
        goto end;
    }

    /* <pmk>: must be exactly 64 hexadecimal characters */
    psk_len = strlen(argv[3]);
    if (psk_len != 64U)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_JAPMK_PMK_FMT;
        goto end;
    }

    for (size_t i = 0U; i < 64U; i++)
    {
        char c = argv[3][i];
        if (!(((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'))))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAPMK_PMK_FMT;
            goto end;
        }
    }

    net_params->xNetworkParams.xPassword.xWPA.ucLength = (uint8_t) psk_len;
    memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
           argv[3],
           net_params->xNetworkParams.xPassword.xWPA.ucLength);

    /* Optional <hidden> argument */
    if (argc == 5)
    {
        if (rm_atcmd_w_core_common_stoi(argv[4], &is_ssid_hidden, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_HIDDEN_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_SECU_HIDDEN_RANGE;
            goto end;
        }
    }

    /* Read WIFI_PROFILE_BAND from NVRAM so band (2G/5G/AUTO) is respected */
 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                     (int *) &band) != FSP_SUCCESS)
    {
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_BAND,
                                      WPA_SETBAND_AUTO);
        net_params->ucBand = eWiFiBandDual;
    }
    else
 #endif
    {
        if (band == WPA_SETBAND_2G)
        {
            net_params->ucBand = eWiFiBand2G;
        }
        else if (band == WPA_SETBAND_5G)
        {
            net_params->ucBand = eWiFiBand5G;
        }
        else
        {
            net_params->ucBand = eWiFiBandDual;
        }
    }

    /* Write common NVRAM profile keys (p_buf holds decoded SSID; quote into input for NVRAM) */
    snprintf(input, sizeof(input), "\"%s\"", p_buf);
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_0, 1);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SYS_MODE,
                                  run_mode);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, input);
 #endif

    switch (auth_mode)
    {
        case CC_VAL_AUTH_WPA:
        case CC_VAL_AUTH_WPA2:
        case CC_VAL_AUTH_WPA_AUTO:
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFICFG,
                                             NVR_KEY_AUTH_TYPE_0,
                                             key_mgmt_WPA_PSK);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPKEY0_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
 #endif
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPINDEX_0);
 #endif
            if (auth_mode == CC_VAL_AUTH_WPA)
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_WPA);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA_ext);
 #endif
 #ifdef RM_MAP_PERSISTANT_W

                /* WPA: auto enc (erase lets wpa_supplicant negotiate TKIP/CCMP) */
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif
            }
            else if (auth_mode == CC_VAL_AUTH_WPA2)
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA2_ext);
 #endif
 #ifdef RM_MAP_PERSISTANT_W

                /* WPA2: CCMP */
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 "CCMP");
 #endif
            }
            else
            {
                /* WPA/WPA2 auto */
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA_WPA2_ext);
 #endif
 #ifdef RM_MAP_PERSISTANT_W

                /* WPA/WPA2 auto: auto enc */
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
 #endif
            }

            /* Store PMK without quotes so rm_wifi_is_raw_pmk detects it on boot reconnect */
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_ENCKEY_0,
                                             argv[3]);
 #endif
            break;
        }

        default:
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_JAP_WPA_MODE_RANGE;
            goto end;
        }
    }

    /* Process hidden SSID */
    if (is_ssid_hidden == pdTRUE)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_HIDDEN_SSID,
                                      (int) is_ssid_hidden);
 #endif
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_HIDDEN_SSID);
 #endif
    }

    net_params->hidden_ssid = is_ssid_hidden;

    wifi_err = WIFI_ConnectAPExt(net_params);
    if (wifi_err)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SELECT_NETWORK;
        WIFI_Disconnect();
        goto end;
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);
 #endif
        char resp_str[16] = "\r\nOK\r\n";
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;
        atcmd_rsp_wifi_conn(p_at_ctrl);
    }

end:

    if (net_params)
    {
        /* Zero PMK before freeing — credential must not linger in heap */
        memset(net_params->xNetworkParams.xPassword.xWPA.cPassphrase, 0x00,
               sizeof(net_params->xNetworkParams.xPassword.xWPA.cPassphrase));

        if (net_params->pucChannelList != NULL)
        {
            vPortFree(net_params->pucChannelList);
            net_params->pucChannelList = NULL;
        }

        vPortFree(net_params);
        net_params = NULL;
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFJAPMK)
{
    const char * p_usage = "<ssid>,<auth>,<pmk>[,<hidden>][,'ch=<channel1>[,<channel2>']]  auth:2=WPA,3=WPA2,4=WPA/WPA2  pmk:64 hex chars";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFJAPMK)
{
    const char * p_description = "[STA] Connect to WPA/WPA2-AP using a pre-computed raw PMK (64 hex chars)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char * p_buf        = NULL;
    const int p_buf_len = RM_ATCMD_W_CORE_WIFI_STR_LEN;

    const int run_mode = get_run_mode();

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (!p_buf)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

        return err;
    }

    memset(p_buf, 0x00, p_buf_len);

    /* Check running mode */
    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if (atcmd_chk_wifi_conn() == pdTRUE)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_ALREADY_CONNECTED;
        goto end;;
    }

    ret = ra6w1_cli_reply("select_network 0", NULL, p_buf);

    if ((ret < 0) || (strncmp(p_buf, "FAIL", 4) == 0))
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SELECT_NETWORK;
        goto end;
    }
    else
    {
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);

        char resp_str[16] = "\r\nOK\r\n";

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

        err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;

        atcmd_rsp_wifi_conn(p_at_ctrl);
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCAP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCAP)
{
    const char * p_description = "[STA] Connect to AP configured currently";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFQAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    WIFIReturnCode_t wifi_err;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    wifi_err = WIFI_Disconnect();

    if (wifi_err)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_DISCONNECT;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFQAP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFQAP)
{
    const char * p_description = "[STA] Disconnect from AP";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFROAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #ifdef CONFIG_SIMPLE_ROAMING
    char resp_buf[32] = {0x00, };
    int roam          = DEFAULT_ROAM_DISABLE;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFROAP=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_ROAM, &roam))
  #endif
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), DEFAULT_ROAM_DISABLE);
        }
        else
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), roam);
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_buf, strlen(resp_buf));
    }
    else if (argc == 2)
    {
        /* AT+WFROAP=<roam> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &roam, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((roam != CC_VAL_ENABLE) && (roam != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
        else
        {
            if (roam == CC_VAL_ENABLE)
            {
                ra6w1_cli_reply("roam run", NULL, NULL);
            }
            else if (roam == CC_VAL_DISABLE)
            {
                ra6w1_cli_reply("roam stop", NULL, NULL);
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_ROAM, roam);
  #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // CONFIG_SIMPLE_ROAMING

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFROAP)
{
    const char * p_usage = "<roam>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFROAP)
{
    const char * p_description = "[STA] Run/Stop STA Roaming";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFROTH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(CONFIG_SIMPLE_ROAMING)
    char resp_buf[32] = {0x00, };
    int roam          = DEFAULT_ROAM_DISABLE;
    char cmdline[64]  = {0x00, };
    int rssi          = 0;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFROTH=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_ROAM_THRESHOLD, &rssi))
  #endif
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ROAM_RSSI_THRESHOLD);
        }
        else
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), rssi);
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_buf, strlen(resp_buf));
    }
    else if (argc == 2)
    {
        /* AT+WFROTH=<rssi> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &rssi, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((rssi < -95) || (rssi > 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
        else
        {
            sprintf(cmdline, "roam_threshold %d", rssi);

            ra6w1_cli_reply(cmdline, NULL, NULL);

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_ROAM_THRESHOLD, rssi);
  #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // CONFIG_SIMPLE_ROAMING

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFROTH)
{
    const char * p_usage = "<rssi>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFROTH)
{
    const char * p_description = "[STA] Set Roaming threshold RSSI value";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFDIS)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_buf[32] = {0x00, };
    int value_int     = CC_VAL_DISABLE;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFDIS=? */
 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE,
                                         &value_int))
 #endif
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), CC_VAL_DISABLE);
        }
        else
        {
            sprintf(resp_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), !value_int);
        }

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_buf, strlen(resp_buf));
    }
    else if (argc == 2)
    {
        /* AT+WFDIS=<disabled> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &value_int, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((value_int != CC_VAL_ENABLE) && (value_int != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_COMPLETE,
                                          !value_int);
 #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFDIS)
{
    const char * p_usage = "<disabled>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFDIS)
{
    const char * p_description = "[STA] Set Wi-Fi Profile Disabled(1) or Enable(0)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFENTAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_WPA_ENTERPRISE__)
    char * p_buf               = NULL;
    char * p_reply             = NULL;
    const int prefix_reply_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len        = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_reply_len);

    char result_str[16] = {0, };
    int wpa             = 0;
    int enc             = 0;
    int p1              = 0;
    int p2              = 0;
    int band            = WPA_SETBAND_AUTO;

    char * p_nv_str = NULL;

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, p_buf_len);

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFENTAP=? */

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_reply = (p_buf + strlen(p_buf));

        /* Read SSID */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_SSID_0,
                                        &p_nv_str);
  #endif

        if (!p_nv_str ||
            (rm_atcmd_w_core_wifi_cp_str(p_reply, ((p_buf + p_buf_len) - p_reply), p_nv_str, strlen(p_nv_str)) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_SSID_NO_VALUE;
            goto end;
        }

        /* Read AUTH Type */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFICFG,
                                        NVR_KEY_AUTH_TYPE_0,
                                        &p_nv_str);
  #endif

        if (p_nv_str == NULL)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_WEPKEY0_0,
                                            &p_nv_str);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
            if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_WEPINDEX_0,
                                             &wpa) &&
                (p_nv_str == NULL))
  #endif
            {
                wpa = CC_VAL_AUTH_OPEN;
            }
            else
            {
                wpa = CC_VAL_AUTH_WEP;
            }
        }
        else if (strcmp(p_nv_str, key_mgmt_WPA_PSK) == 0)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str == NULL)
            {
                wpa = CC_VAL_AUTH_WPA_AUTO;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                wpa = CC_VAL_AUTH_WPA;
            }
            else
            {
                wpa = CC_VAL_AUTH_WPA2;
            }
        }
        else if (strcmp(p_nv_str, key_mgmt_WPA_EAP) == 0)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_PROTO_0,
                                            &p_nv_str);
  #endif

            if (p_nv_str == NULL)
            {
                wpa = CC_VAL_AUTH_WPA_WPA2_EAP;
            }
            else if (strcmp(p_nv_str, "WPA") == 0)
            {
                wpa = CC_VAL_AUTH_WPA_EAP;
            }
            else
            {
                wpa = CC_VAL_AUTH_WPA2_EAP;
            }
        }

  #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
        else if (strcmp(p_nv_str, key_mgmt_WPA3_OWE) == 0)
        {
            wpa = CC_VAL_AUTH_OWE;
        }
        else if (strcmp(p_nv_str, key_mgmt_WPA3_SAE) == 0)
        {
            wpa = CC_VAL_AUTH_SAE;
        }
        else if (strcmp(p_nv_str, "WPA-PSK SAE") == 0)
        {
            wpa = CC_VAL_AUTH_RSN_SAE;
        }
  #endif                               // __SUPPORT_WPA3_PERSONAL_CORE__
        else if (strcmp(p_nv_str, key_mgmt_WPA3_ENT192B) == 0)
        {
            wpa = CC_VAL_AUTH_WPA3_EAP_192B;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_AUTH0_UNSUPPORT;
            goto end;
        }

        if (wpa < CC_VAL_AUTH_WPA_EAP)
        {
            strcat(p_reply, ",0");
            goto print_state;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0,
                                        &p_nv_str);
  #endif

        if (p_nv_str == NULL)
        {
            enc = CC_VAL_ENC_AUTO;
        }
        else if (strcmp(p_nv_str, pairwise_TKIP) == 0)
        {
            enc = CC_VAL_ENC_TKIP;
        }
        else if (strcmp(p_nv_str, pairwise_CCMP) == 0)
        {
            enc = CC_VAL_ENC_CCMP;
        }
        else if (strcmp(p_nv_str, pairwise_GCMP_256) == 0)
        {
            enc = CC_VAL_ENC_WPA3_EAP_192B;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_ENC0_UNSUPPORT;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", &p_nv_str);
  #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            bsp_safe_strcpy(result_str, p_nv_str, sizeof(result_str));
        }
        else
        {
            // return CC_FAILURE_NO_VALUE;
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE1;
            goto end;
        }

        if (strcmp(result_str, "PEAP TTLS FAST") == 0)
        {
            p1 = CC_VAL_EAP_DEFAULT;
        }
        else if (strcmp(result_str, "PEAP") == 0)
        {
            char tmp_reply[2] = {0, };

            ra6w1_cli_reply("peap_ver", NULL, tmp_reply);

            if (atoi(tmp_reply) == 1)
            {
                p1 = CC_VAL_EAP_PEAP1;
            }
            else
            {
                p1 = CC_VAL_EAP_PEAP0;
            }
        }
        else if (strcmp(result_str, "TTLS") == 0)
        {
            p1 = CC_VAL_EAP_TTLS;
        }
        else if (strcmp(result_str, "FAST") == 0)
        {
            p1 = CC_VAL_EAP_FAST;
        }
        else if (strcmp(result_str, "TLS") == 0)
        {
            p1 = CC_VAL_EAP_TLS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE1;
            goto end;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_phase2", &p_nv_str);
  #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            bsp_safe_strcpy(result_str, p_nv_str, sizeof(result_str));
        }
        else
        {
            sprintf((p_reply + strlen(p_reply)), ",%d,%d,%d", wpa, enc, p1);
            goto print_state;
        }

        if (strcmp(result_str + 7, "MSCHAPV2 GTC") == 0)
        {
            p2 = CC_VAL_EAP_PHASE2_MIX;
            sprintf((p_reply + strlen(p_reply)), ",%d,%d,%d,%d", wpa, enc, p1, p2);
            goto print_state;
        }
        else if (strcmp(result_str + 7, "MSCHAPV2") == 0)
        {
            p2 = CC_VAL_EAP_MSCHAPV2;
            sprintf((p_reply + strlen(p_reply)), ",%d,%d,%d,%d", wpa, enc, p1, p2);
            goto print_state;
        }
        else if (strcmp(result_str + 7, "GTC") == 0)
        {
            p2 = CC_VAL_EAP_GTC;
            sprintf((p_reply + strlen(p_reply)), ",%d,%d,%d,%d", wpa, enc, p1, p2);
            goto print_state;
        }
        else if (strstr(result_str, "TLS\"") == 0)
        {
            p2 = CC_VAL_EAP_PHASE2_TLS;
        }
        else
        {
            sprintf((p_reply + strlen(p_reply)), ",%d,%d,%d", wpa, enc, p1);
            goto print_state;
        }

print_state:

        if (p_reply && strlen(p_reply))
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        int is_ssid_hidden = pdTRUE;
        char input[64]     = {0, };
        bool entap         = false;
        char * last_arg    = NULL;

        /* AT+WFENTAP=<ssid>,<auth>,<enc>,<phase1>[,<phase2>][,<hidden>] */
        if (argc < 5)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[2], &wpa, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_SECU_MODE;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[3], &enc, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_ENC_MODE;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[4], &p1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE1;
            goto end;
        }

        if (argc >= 6)
        {
            last_arg = argv[argc - 1];

            /* Check for ch= prefix */
            if (((last_arg[0] == 'c') || (last_arg[0] == 'C')) &&
                ((last_arg[1] == 'h') || (last_arg[1] == 'H')) &&
                (last_arg[2] == '='))
            {
                /* set the entap flag to true to parse the channels later after flush command*/
                entap = true;

                /* Decrement argc so subsequent parsing logic ignores the channel arg */
                argc--;
            }
        }

        if (argc == 6)
        {
            /* AT+WFENTAP=<ssid>,<auth>,<enc>,<phase1>,<hidden> */
            if ((p1 == CC_VAL_EAP_FAST) || (p1 == CC_VAL_EAP_TLS))
            {
                if (rm_atcmd_w_core_common_stoi(argv[5], &is_ssid_hidden, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_WPA_HIDDEN_TYPE;
                    goto end;
                }

                /* AT+WFENTAP=<ssid>,<auth>,<enc>,<phase1>,<phase2>*/
            }
            else if ((p1 == CC_VAL_EAP_DEFAULT) || (p1 == CC_VAL_EAP_PEAP0) || CC_VAL_EAP_PEAP1 || CC_VAL_EAP_TTLS)
            {
                if (rm_atcmd_w_core_common_stoi(argv[5], &p2, POL_1) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE2;
                    goto end;
                }

                if ((p2 < CC_VAL_EAP_PHASE2_MIX) || (p2 > CC_VAL_EAP_PHASE2_TLS)) // EAP Phase #2
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE2;
                    goto end;
                }
            }
        }

        if (argc == 7)
        {
            /* AT+WFENTAP=<ssid>,<auth>,<enc>,<phase1>,<phase2>,<hidden> */
            if ((p1 == CC_VAL_EAP_FAST) || (p1 == CC_VAL_EAP_TLS)) // when argc == 7, p1 cannot be EAP FASE or EAP TLS
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_MODE;      // because it doesn't need a p2 process.
                goto end;
            }

            if (rm_atcmd_w_core_common_stoi(argv[5], &p2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE2;
                goto end;
            }

            if (rm_atcmd_w_core_common_stoi(argv[6], &is_ssid_hidden, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_WPA_HIDDEN_TYPE;
                goto end;
            }
        }

        /* Wi-Fi Auth mode */
        if (((wpa < CC_VAL_AUTH_WPA_EAP) || (wpa > CC_VAL_AUTH_WPA2_WPA3_EAP)))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_SECU_MODE;
            goto end;
        }
        else if ((wpa != CC_VAL_AUTH_WPA3_EAP_192B) && (enc == CC_VAL_ENC_WPA3_EAP_192B))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_SECU_MODE;
            goto end;
        }

        /* Wi-Fi Encryption type */
        if ((enc < CC_VAL_ENC_TKIP) || (enc > CC_VAL_ENC_WPA3_EAP_192B))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_ENC_MODE;
            goto end;
        }
        else if ((wpa == CC_VAL_AUTH_WPA3_EAP_192B) && (enc != CC_VAL_ENC_WPA3_EAP_192B))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_ENC_MODE;
            goto end;
        }

        /* EAP Phase #1 */
        if ((p1 < CC_VAL_EAP_DEFAULT) || (p1 > CC_VAL_EAP_TLS))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_MODE;
            goto end;
        }

        if (argc == 6)
        {
            if ((p1 == CC_VAL_EAP_FAST) || (p1 == CC_VAL_EAP_TLS))
            {
                if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE) // hidden
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_WPA_HIDDEN_RANGE;
                    goto end;
                }
            }
            else if ((p1 == CC_VAL_EAP_DEFAULT) || (p1 == CC_VAL_EAP_PEAP0) || CC_VAL_EAP_PEAP1 || CC_VAL_EAP_TTLS)
            {
                if ((p2 < CC_VAL_EAP_PHASE2_MIX) || (p2 > CC_VAL_EAP_PHASE2_TLS)) // EAP Phase #2
                {
                    err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE2;
                    goto end;
                }
            }
        }

        if (argc == 7)
        {
            if ((p2 < CC_VAL_EAP_PHASE2_MIX) || (p2 > CC_VAL_EAP_PHASE2_TLS)) // EAP Phase #2
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PHASE2;
                goto end;
            }

            if (rm_atcmd_w_core_common_is_in_valid_range(is_ssid_hidden, 0, 1) == pdFALSE) // hidden
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_WPA_HIDDEN_RANGE;
                goto end;
            }
        }

        /*
         * Support Extended ASCII or UTF-8 for SSID
         * Do not use the argv[1] string directly.
         */
        if (str_decode((u8 *) p_buf, MAX_SSID_LEN, argv[1]) > MAX_SSID_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_SSID_LEN;
            goto end;
        }
        else
        {
            ra6w1_cli_reply("flush", NULL, NULL);
        }

        /* Save profile */
        snprintf(input, sizeof(input), "\"%s\"", p_buf);
        cc_set_network_str("ssid", 0, input);

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_0, 1);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_SSID_0,
                                         input);
  #endif

        /* Parse the channels and set the scan_freq if entap flag is true */
        if (entap)
        {
            err = rm_atcmd_w_core_wifi_parse_channels(last_arg, NULL);
            if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
            {
                goto end;
            }
        }

        switch (wpa)
        {
            case CC_VAL_AUTH_OPEN:
            {
                cc_set_network_str("key_mgmt", 0, "NONE");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key0");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_WEPINDEX_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENCKEY_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityOpen_ext);
  #endif
                break;
            }

            case CC_VAL_AUTH_WEP:
            {
                cc_set_network_str("key_mgmt", 0, "NONE");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_AUTH_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENCKEY_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWEP_ext);
  #endif
                break;
            }

            case CC_VAL_AUTH_WPA:
            case CC_VAL_AUTH_WPA2:
            case CC_VAL_AUTH_WPA_AUTO:
            {
                cc_set_network_str("key_mgmt", 0, key_mgmt_WPA_PSK);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_PSK);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key0");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_WEPINDEX_0);
  #endif

                if (wpa == CC_VAL_AUTH_WPA)
                {
                    cc_set_network_str("proto", 0, proto_WPA);
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_WPA);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_ext);
  #endif
                }
                else if (wpa == CC_VAL_AUTH_WPA2)
                {
                    cc_set_network_str("proto", 0, proto_RSN);
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_RSN);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_ext);
  #endif
                }
                else
                {
                    cc_set_network_str("proto", 0, "WPA RSN");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_WPA2_ext);
  #endif
                }

                break;
            }

  #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
            case CC_VAL_AUTH_OWE:
            {
                cc_set_network_str("key_mgmt", 0, key_mgmt_WPA3_OWE);
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA3_OWE);
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key0");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_WEPINDEX_0);
   #endif

                /* wpa_cli set_network 0/1 proto RSN */
                cc_set_network_str("proto", 0, proto_RSN);

                /* Save proto to nvram */
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityNotSupported_ext);
   #endif

                break;
            }

            case CC_VAL_AUTH_SAE:
            case CC_VAL_AUTH_RSN_SAE:
            {
                /* Run set_network 0/1 key_mgmt */
                if (wpa == CC_VAL_AUTH_SAE)
                {
                    /* cli set_network 0/1 key_mgmt SAE */
                    cc_set_network_str("key_mgmt", 0, key_mgmt_WPA3_SAE);
                }
                else
                {
                    /* CC_VAL_AUTH_RSN_SAE */
                    /* cli set_network 0/1 key_mgmt WPA-PSK SAE */
                    cc_set_network_str("key_mgmt", 0, key_mgmt_WPA_PSK_WPA3_SAE);
                }

                /* STA mode */

                /* Save key_mgmt to nvram */
                if (wpa == CC_VAL_AUTH_SAE)
                {
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_0,
                                                     key_mgmt_WPA3_SAE);
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA3_ext);
   #endif
                }
                else
                {
                    /* CC_VAL_AUTH_RSN_SAE */
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_0,
                                                     key_mgmt_WPA_PSK_WPA3_SAE);
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_WPA3_ext);
   #endif
                }

   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key0");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key1");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key2");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_wep_key3");
   #endif
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_WEPINDEX_0);
   #endif

                /* cli set_network 0/1 proto RSN */
                cc_set_network_str("proto", 0, proto_RSN);

                /* Save proto to nvram */
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_PROTO_0,
                                                 proto_RSN);
   #endif
                break;
            }
  #endif                               // __SUPPORT_WPA3_PERSONAL_CORE__

            case CC_VAL_AUTH_WPA_EAP:
            case CC_VAL_AUTH_WPA2_EAP:
            case CC_VAL_AUTH_WPA_WPA2_EAP:
            {
                cc_set_network_str("key_mgmt", 0, key_mgmt_WPA_EAP);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA_EAP);
  #endif

                if (wpa == CC_VAL_AUTH_WPA_EAP)
                {
                    cc_set_network_str("proto", 0, proto_WPA);
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_WPA);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_ent_ext);
  #endif
                }
                else if (wpa == CC_VAL_AUTH_WPA2_EAP)
                {
                    cc_set_network_str("proto", 0, proto_RSN);
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_0,
                                                     proto_RSN);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_ent_ext);
  #endif
                }
                else
                {
                    cc_set_network_str("proto", 0, "WPA RSN");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA_WPA2_ent_ext);
  #endif
                }

                break;
            }

            case CC_VAL_AUTH_WPA3_EAP:
            case CC_VAL_AUTH_WPA2_WPA3_EAP:
            {
                sprintf(input, "%s %s", key_mgmt_WPA_EAP, key_mgmt_WPA_EAP_SHA256);
                cc_set_network_str("key_mgmt", 0, input);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 input);
                if (wpa == CC_VAL_AUTH_WPA3_EAP)
                {
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA3_ent_ext);
                }
                else if (wpa == CC_VAL_AUTH_WPA2_WPA3_EAP)
                {
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_0,
                                                  eWiFiSecurityWPA2_WPA3_ent_ext);
                }
  #endif

                /* PMF Optional */
                cc_set_network_int("ieee80211w", 0, 1);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0,
                                              1);
  #endif

                break;
            }

            case CC_VAL_AUTH_WPA3_EAP_192B:
            {
                cc_set_network_str("key_mgmt", 0, key_mgmt_WPA3_ENT192B);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_AUTH_TYPE_0,
                                                 key_mgmt_WPA3_ENT192B);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SECURITY_0,
                                              eWiFiSecurityWPA3_192B_ent_ext);
  #endif

                cc_set_network_str("proto", 0, "RSN");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_0);
  #endif

                /* PMF Required */
                cc_set_network_int("ieee80211w", 0, 2);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0,
                                              2);
  #endif

                break;
            }
        }

        switch (enc)
        {
            case CC_VAL_ENC_TKIP:
            {
                cc_set_network_str("pairwise", 0, pairwise_TKIP);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 pairwise_TKIP);
  #endif
                break;
            }

            case CC_VAL_ENC_CCMP:
            {
                cc_set_network_str("pairwise", 0, pairwise_CCMP);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 pairwise_CCMP);
  #endif
                break;
            }

            case CC_VAL_ENC_AUTO:
            {
                sprintf(input, "%s %s", pairwise_TKIP, pairwise_CCMP);
                cc_set_network_str("pairwise", 0, input);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_0);
  #endif
                break;
            }

            case CC_VAL_ENC_WPA3_EAP_192B:
            {
                cc_set_network_str("pairwise", 0, pairwise_GCMP_256);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_ENC_TYPE_0,
                                                 pairwise_GCMP_256);
  #endif

                cc_set_network_str("group", 0, pairwise_GCMP_256);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 NVR_KEY_GROUP_0,
                                                 pairwise_GCMP_256);
  #endif
                break;
            }
        }

        if (p1 == CC_VAL_EAP_DEFAULT)
        {
            cc_set_network_str("eap", 0, "PEAP TTLS FAST");
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFICFG,
                                             "N0_eap",
                                             "PEAP TTLS FAST");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_PEAP_TTLS_FAST);
  #endif
        }
        else if (p1 == CC_VAL_EAP_PEAP0)
        {
            cc_set_network_str("eap", 0, "PEAP");
            ra6w1_cli_reply("peap_ver 0", NULL, NULL);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", "PEAP");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_PEAP);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_PEAP_VER, 0);
  #endif
        }
        else if (p1 == CC_VAL_EAP_PEAP1)
        {
            cc_set_network_str("eap", 0, "PEAP");
            ra6w1_cli_reply("peap_ver 1", NULL, NULL);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", "PEAP");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_PEAP);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_PEAP_VER, 1);
  #endif
        }
        else if (p1 == CC_VAL_EAP_TTLS)
        {
            cc_set_network_str("eap", 0, "TTLS");
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", "TTLS");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_TTLS);
  #endif
        }
        else if (p1 == CC_VAL_EAP_FAST)
        {
            cc_set_network_str("eap", 0, "FAST");
            cc_set_network_str("phase1", 0, "'fast_provisioning=2'");
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", "FAST");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_FAST);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFICFG,
                                             "N0_phase1",
                                             "\"fast_provisioning=2\"");
  #endif
        }
        else
        {
            cc_set_network_str("eap", 0, "TLS");
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_eap", "TLS");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_AUTH_MODE,
                                          ENT_AUTH_TYPE_TLS);
  #endif
        }

  #if defined(EAP_FAST)

        /* Erase old fast_pac, fast_pac_len for EAP-FAST */
   #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_FAST_PAC);
   #endif
   #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_FAST_PAC_LEN);
   #endif
  #endif                               /* EAP_FAST */

        if (argc == 6)
        {
            if ((p1 == CC_VAL_EAP_DEFAULT) || (p1 == CC_VAL_EAP_PEAP0) || (p1 == CC_VAL_EAP_PEAP1) || (p1 == CC_VAL_EAP_TTLS))
            {
                if (p2 == CC_VAL_EAP_PHASE2_MIX)
                {
                    cc_set_network_str("phase2", 0, "\'auth=MSCHAPV2 GTC\'");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     "N0_phase2",
                                                     "\"auth=MSCHAPV2 GTC\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_EAP_PHASE2,
                                                  ENT_AUTH_PROTO_MSCHAPv2_GTC);
  #endif
                }
                else if (p2 == CC_VAL_EAP_MSCHAPV2)
                {
                    cc_set_network_str("phase2", 0, "\'auth=MSCHAPV2\'");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     "N0_phase2",
                                                     "\"auth=MSCHAPV2\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_EAP_PHASE2,
                                                  ENT_AUTH_PROTO_MSCHAPv2);
  #endif
                }
                else if (p2 == CC_VAL_EAP_GTC)
                {
                    cc_set_network_str("phase2", 0, "\'auth=GTC\'");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     "N0_phase2",
                                                     "\"auth=GTC\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_EAP_PHASE2,
                                                  ENT_AUTH_PROTO_GTC);
  #endif
                }
                else
                {
                    if (p1 == CC_VAL_EAP_TTLS)
                    {
                        cc_set_network_str("phase2", 0, "\'autheap=TLS\'");
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFICFG,
                                                         "N0_phase2",
                                                         "\"autheap=TLS\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_WIFIPROFILE,
                                                      WIFI_PROFILE_EAP_PHASE2,
                                                      ENT_AUTH_PROTO_TLS);
  #endif
                    }
                    else
                    {
                        cc_set_network_str("phase2", 0, "\'auth=TLS\'");
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFICFG,
                                                         "N0_phase2",
                                                         "\"auth=TLS\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_WIFIPROFILE,
                                                      WIFI_PROFILE_EAP_PHASE2,
                                                      ENT_AUTH_PROTO_TLS);
  #endif
                    }
                }
            }
            else
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_EAP_PHASE2,
                                              ENT_AUTH_PROTO_NONE);
  #endif
            }
        }
        else if (argc == 7)
        {
            if (p2 == CC_VAL_EAP_PHASE2_MIX)
            {
                cc_set_network_str("phase2", 0, "\'auth=MSCHAPV2 GTC\'");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 "N0_phase2",
                                                 "\"auth=MSCHAPV2 GTC\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_EAP_PHASE2,
                                              ENT_AUTH_PROTO_MSCHAPv2_GTC);
  #endif
            }
            else if (p2 == CC_VAL_EAP_MSCHAPV2)
            {
                cc_set_network_str("phase2", 0, "\'auth=MSCHAPV2\'");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 "N0_phase2",
                                                 "\"auth=MSCHAPV2\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_EAP_PHASE2,
                                              ENT_AUTH_PROTO_MSCHAPv2);
  #endif
            }
            else if (p2 == CC_VAL_EAP_GTC)
            {
                cc_set_network_str("phase2", 0, "\'auth=GTC\'");
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFICFG,
                                                 "N0_phase2",
                                                 "\"auth=GTC\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_EAP_PHASE2,
                                              ENT_AUTH_PROTO_GTC);
  #endif
            }
            else
            {
                if (p1 == CC_VAL_EAP_TTLS)
                {
                    cc_set_network_str("phase2", 0, "\'autheap=TLS\'");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     "N0_phase2",
                                                     "\"autheap=TLS\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_EAP_PHASE2,
                                                  ENT_AUTH_PROTO_TLS);
  #endif
                }
                else
                {
                    cc_set_network_str("phase2", 0, "\'auth=TLS\'");
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     "N0_phase2",
                                                     "\"auth=TLS\"");
  #endif
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_EAP_PHASE2,
                                                  ENT_AUTH_PROTO_TLS);
  #endif
                }
            }
        }
        else
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_EAP_PHASE2,
                                          ENT_AUTH_PROTO_MSCHAPv2_GTC);
        }

        if (is_ssid_hidden == pdTRUE)
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_HIDDEN_SSID,
                                          (int) is_ssid_hidden);
  #endif
            cc_set_network_int("scan_ssid", 0, 1);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_scan_ssid", 1);
  #endif
        }
        else
        {
            cc_set_network_int("scan_ssid", 0, 0);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N0_scan_ssid");
  #endif
        }

        /* Set default band */
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                         (int *) &band) != FSP_SUCCESS)
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_BAND,
                                          WPA_SETBAND_AUTO);
        }

        /* Set WIFI_PROFILE_COMPLETE to load profile from easy-setup application. */
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // (__SUPPORT_WPA_ENTERPRISE__)

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFENTAP)
{
    const char * p_usage = "<ssid>,<auth>,<enc>,<phase1>[,<phase2>][,<hidden>][,'ch=<channel1>[,<channel2>']]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFENTAP)
{
    const char * p_description = "[STA] Set Enterprise AP profile";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFENTLI)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_WPA_ENTERPRISE__)
    char * p_nv_str = NULL;

    char p_buf[128] = {0x00, };
    char * p_resp   = NULL;
    char input[(wificonfigMAX_ENT_IDENTITY_LEN * 2) + 3]  = {0x00, };

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_STATION) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFENTLI=? */

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_resp = (p_buf + strlen(p_buf));

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_EAP_ID,
                                        &p_nv_str);
  #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            memcpy(p_resp, (p_nv_str + 1), (strlen(p_nv_str) - 2));

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_EAP_PW,
                                            &p_nv_str);
  #endif

            if (p_nv_str && strlen(p_nv_str))
            {
                strcat(p_resp, ",");
                memcpy((p_resp + strlen(p_resp)), (p_nv_str + 1), (strlen(p_nv_str) - 2));
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_ID_NO_VALUE;
            goto end;
        }

        if (p_resp && strlen(p_resp))
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        /* AT+WFENTLI=<id>[,<pw>] */
        if (strlen(argv[1]) > (wificonfigMAX_ENT_IDENTITY_LEN * 2))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_ID_LEN;
            goto end;
        }
        else if (((argc == 3) && (strlen(argv[2]) > (wificonfigMAX_ENT_PASSWORD_LEN * 2))))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_ENTAP_EAP_PWD_LEN;
            goto end;
        }

        snprintf(input, sizeof(input), "'%s'", argv[1]);
        cc_set_network_str("identity", 0, input);

        snprintf(input, sizeof(input), "\"%s\"", argv[1]);

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_EAP_ID,
                                         input);
  #endif

        if (argc == 3)
        {
            snprintf(input, sizeof(input), "'%s'", argv[2]);
            cc_set_network_str("password", 0, input);

            snprintf(input, sizeof(input), "\"%s\"", argv[2]);

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_EAP_PW,
                                             input);
  #endif
        }
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // (__SUPPORT_WPA_ENTERPRISE__)

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFENTLI)
{
    const char * p_usage = "<id>[,<pw>]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFENTLI)
{
    const char * p_description = "[STA] Set Enterprise Log-in ID/PWD";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSAP)
{
    extern int ra6w1_regdb_is_valid_ch(char * country, int ch, unsigned int exclude_flags);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char * p_nv_str = NULL;

    char * p_buf              = NULL;
    char * p_reply            = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);
    size_t resp_len           = 0;

    WIFIReturnCode_t wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));

    if (!net_params)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    rm_atcmd_w_core_wifi_init_network_params(net_params);

    const int run_mode = get_run_mode();

    p_buf = (char *) pvPortMalloc(p_buf_len);
    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, p_buf_len);

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFSAP=? */
        char val_0[33] = {0x00, };     // SSID
        char val_3[66] = {0x00, };     // Passphrase
        char val_5[3]  = {0x00, };     // Country code
        int val_1      = 0;            // Auth
        int val_2      = 0;            // Enc
        int val_4      = 0;            // Channel

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_reply = (p_buf + strlen(p_buf));

        /* Get SSID */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_SSID_1,
                                        &p_nv_str);
 #endif

        if (!p_nv_str || (rm_atcmd_w_core_wifi_cp_str(val_0, sizeof(val_0), p_nv_str, strlen(p_nv_str)) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_SSID_NO_VALUE;
            goto end;
        }

        if ((run_mode == WIFI_DEVICE_MODE_EXT_AP) || (run_mode == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            /* Get current channel */
            err = rm_atcmd_w_core_wifi_get_channel(1, &val_4);
            if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
            {
                goto end;
            }
        }
        else
        {
            /* Get stored channel */
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_WIFIPROFILE,
                                         WIFI_PROFILE_CHANNEL,
                                         &val_4);
 #endif
        }

        /* Get country code */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_COUNTRY_CODE,
                                        &p_nv_str);
 #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            bsp_safe_strcpy(val_5, p_nv_str, sizeof(val_5));
        }
        else
        {
            bsp_safe_strcpy(val_5, DEFAULT_AP_COUNTRY, sizeof(val_5));
        }

        /* Get Auth */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_SECURITY_1,
                                     &val_1);
 #endif

        switch (val_1)
        {
            case eWiFiSecurityOpen_ext:
            {
                val_1 = CC_VAL_AUTH_OPEN;
                break;
            }

            case eWiFiSecurityWEP_ext:
            {
                val_1 = CC_VAL_AUTH_WEP;
                break;
            }

            case eWiFiSecurityWPA_ext:
            {
                val_1 = CC_VAL_AUTH_WPA;
                break;
            }

            case eWiFiSecurityWPA2_ext:
            {
                val_1 = CC_VAL_AUTH_WPA2;
                break;
            }

            case eWiFiSecurityWPA_WPA2_ext:
            {
                val_1 = CC_VAL_AUTH_WPA_AUTO;
                break;
            }

            case eWiFiSecurityWPA3_OWE_ext:
            {
                val_1 = CC_VAL_AUTH_OWE;
                break;
            }

            case eWiFiSecurityWPA3_ext:
            {
                val_1 = CC_VAL_AUTH_SAE;
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ext:
            {
                val_1 = CC_VAL_AUTH_RSN_SAE;
                break;
            }

            default:
            {
                val_1 = CC_VAL_UNKNOWN;
                break;
            }
        }

        if ((val_1 == CC_VAL_AUTH_OPEN)
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            || (val_1 == CC_VAL_AUTH_OWE)
 #endif                                // __SUPPORT_WPA3_PERSONAL__
            )
        {
            sprintf(p_reply, "'%s',%d,%d,%s", val_0, val_1, val_4, val_5);
        }
        else if (val_1 > CC_VAL_AUTH_WEP)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFICFG,
                                            NVR_KEY_ENC_TYPE_1,
                                            &p_nv_str);
 #endif

            if (p_nv_str == NULL)
            {
                val_2 = CC_VAL_ENC_AUTO;
            }
            else if (strcmp(p_nv_str, "TKIP") == 0)
            {
                val_2 = CC_VAL_ENC_TKIP;
            }
            else
            {
                val_2 = CC_VAL_ENC_CCMP;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_WIFIPROFILE,
                                            WIFI_PROFILE_ENCKEY_1,
                                            &p_nv_str);
 #endif

            if (p_nv_str)
            {
                rm_atcmd_w_core_wifi_cp_str(val_3, sizeof(val_3), p_nv_str, strlen(p_nv_str));
            }

            sprintf(p_reply, "'%s',%d,%d,'%s',%d,%s", val_0, val_1, val_2, val_3, val_4, val_5);
        }

        if (p_reply && strlen(p_reply))
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
        }
    }
    else
    {
        char * p_psk    = NULL;
        size_t psk_len  = 0;
        int auth_mode   = 0;
        int enc_mode    = -1;
        int chan        = 0;
        char country[4] = {0x00, };

        /*
         * AT+WFSAP=<ssid>,<auth>,[enc],[key],[ch],[country]
         *
         * <auth>=OPEN / WPA3_OWE:
         * case1>argc=3 : AT+WFSAP=<ssid>,<auth>
         * case2>argc=4 : AT+WFSAP=<ssid>,<auth>,[ch]
         * case3>argc=5 : AT+WFSAP=<ssid>,<auth>,[ch],[country]
         *
         * <auth>=WPA / WPA2 / WPA+WPA2 / WPA3_SAE / WPA2+WPA3_SAE
         * case4>argc=5 : AT+WFSAP=<ssid>,<auth>,<enc>,<key>
         * case5>argc=6 : AT+WFSAP=<ssid>,<auth>,<enc>,<key>,[ch]
         * case6>argc=7 : AT+WFSAP=<ssid>,<auth>,<enc>,<key>,[ch],[country]
         */

        if (argc < 3)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
            goto end;
        }

        if (rm_atcmd_w_core_common_stoi(argv[2], &auth_mode, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_SECU_MODE;
            goto end;
        }

        if ((auth_mode < 0)
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            || (auth_mode > CC_VAL_AUTH_RSN_SAE)
 #else
            || (auth_mode > CC_VAL_AUTH_WPA_AUTO)
 #endif                                // __SUPPORT_WPA3_PERSONAL__
            )
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_SECU_MODE;
            goto end;
        }

        /* Sanity check: <ch>, <country> */
        if (((auth_mode == CC_VAL_AUTH_OPEN
 #if defined(__SUPPORT_WPA3_PERSONAL__)
              || auth_mode == CC_VAL_AUTH_OWE
 #endif                                /* __SUPPORT_WPA3_PERSONAL__ */
              ) && (argc >= 4)) ||
            ((auth_mode > CC_VAL_AUTH_WEP) && (argc >= 6)))
        {
            /*
             *  cases where channel info exists.
             *      <case2>, <case3>
             *      <case5>, <case6>
             */

            int seq = argc >= 6 ? 6 : 4;

            if (rm_atcmd_w_core_common_stoi(argv[seq - 1], &chan, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_CH_VALUE_TYPE;
                goto end;
            }

            if ((auth_mode == CC_VAL_AUTH_OPEN) && (argc >= 6))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_OPEN_TOO_MANY_ARG;
                goto end;
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            }
            else if ((auth_mode == CC_VAL_AUTH_OWE) && (argc >= 6))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_OWE_TOO_MANY_ARG;
                goto end;
 #endif                                /* __SUPPORT_WPA3_PERSONAL__ */
            }

            if ((argc == 4) || (argc == 6))
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_COUNTRY_CODE,
                                                &p_nv_str);
 #endif

                if (p_nv_str && strlen(p_nv_str))
                {
                    bsp_safe_strcpy(country, p_nv_str, sizeof(country));
                }
                else
                {
                    bsp_safe_strcpy(country, DEFAULT_AP_COUNTRY, sizeof(country));
                }
            }
            else
            {
                bsp_safe_strcpy(country, argv[seq], sizeof(country));
            }

            if (strlen(country) != 2)
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_CC;
                goto end;
            }

 #if defined(__SUPPORT_SETBAND_5GHZ__)
            if (!ra6w1_regdb_is_valid_ch(country, chan, (CH_FLAG_NO_IR | CH_FLAG_DFS)))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_CH_VALUE_RANGE;
                goto end;
            }

 #else
            if (!ra6w1_regdb_is_valid_ch(country, chan, 0) || (chan > 14))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_CH_VALUE_RANGE;
                goto end;
            }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */
        }

        if (auth_mode == CC_VAL_AUTH_WEP)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_WEP_NOT_SUPPORT;
            goto end;
 #if defined(__SUPPORT_WPA3_PERSONAL__)
        }
        else if (auth_mode == CC_VAL_AUTH_OWE)
        {
            /* Skip */
 #endif                                // __SUPPORT_WPA3_PERSONAL__
        }
        else if (auth_mode > CC_VAL_AUTH_WEP)
        {
            /* For cases except where <enc>, and <key> exist */
            if (rm_atcmd_w_core_common_stoi(argv[3], &enc_mode, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_ENC_MODE_TYPE;
                goto end;
            }

            if ((enc_mode < 0) ||
 #if defined(__SUPPORT_WPA3_PERSONAL__)
                ((auth_mode == CC_VAL_AUTH_SAE ||
                  auth_mode == CC_VAL_AUTH_RSN_SAE) && enc_mode != CC_VAL_ENC_CCMP) ||
 #endif                                // __SUPPORT_WPA3_PERSONAL__
                enc_mode > CC_VAL_ENC_AUTO)
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_ENC_MODE_RANGE;
                goto end;
            }

            /* <key> */
            p_psk   = argv[4];
            psk_len = strlen(p_psk);
            if ((psk_len < 8) || (psk_len > wificonfigMAX_PASSPHRASE_LEN))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_PASSKEY_LEN;
                goto end;
            }
        }

        /* SSID */
        if (((strlen(argv[1])) == 0) || (strlen(argv[1]) > wificonfigMAX_SSID_LEN))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_SSID_LEN;
            goto end;
        }
        else
        {
            net_params->xNetworkParams.ucSSIDLength = strlen(argv[1]);
            memcpy(net_params->xNetworkParams.ucSSID, argv[1], net_params->xNetworkParams.ucSSIDLength);
        }

        /* Process <auth> */
        switch (auth_mode)
        {
            case CC_VAL_AUTH_OPEN:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityOpen_ext;
                break;
            }

            case CC_VAL_AUTH_WEP:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWEP_ext;
                break;
            }

            case CC_VAL_AUTH_WPA:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_ext;
                break;
            }

            case CC_VAL_AUTH_WPA2:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_ext;
                break;
            }

            case CC_VAL_AUTH_WPA_AUTO:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA_WPA2_ext;
                break;
            }

 #if defined(__SUPPORT_WPA3_PERSONAL_CORE__)
            case CC_VAL_AUTH_OWE:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_OWE_ext;
                break;
            }

            case CC_VAL_AUTH_SAE:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA3_ext;
                break;
            }

            case CC_VAL_AUTH_RSN_SAE:
            {
                net_params->xNetworkParams.xSecurity = (WIFISecurity_t) eWiFiSecurityWPA2_WPA3_ext;
                break;
            }
 #endif                                // __SUPPORT_WPA3_PERSONAL_CORE__
            case CC_VAL_AUTH_WPA_EAP:
            case CC_VAL_AUTH_WPA2_EAP:
            case CC_VAL_AUTH_WPA_WPA2_EAP:
            {
                break;
            }

            default:
            {
                break;
            }
        }

        if ((auth_mode == CC_VAL_AUTH_OPEN)
 #if defined(__SUPPORT_WPA3_PERSONAL__)
            || (auth_mode == CC_VAL_AUTH_OWE)
 #endif                                // __SUPPORT_WPA3_PERSONAL__
            )
        {
            /* Process <ch>, <country_code> for OPEN / OWE */
            if (argc >= 5)
            {
                WIFI_SetCountryCode(country);
            }

            if (argc >= 4)
            {
                char inputa[64] = {0, };

 #if defined(__SUPPORT_SETBAND_5GHZ__)
                if (chan > 14)
                {
                    net_params->ucBand = eWiFiBand5G;
                }
                else
                {
                    net_params->ucBand = eWiFiBand2G;
                }
                atcmd_set_wifi_mode(auth_mode, chan, net_params);
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_COUNTRY_CODE,
                                                &p_nv_str);
 #endif

                if (p_nv_str && strlen(p_nv_str))
                {
                    bsp_safe_strcpy(inputa, p_nv_str, sizeof(inputa));
                }
                else
                {
                    bsp_safe_strcpy(inputa, DEFAULT_AP_COUNTRY, sizeof(inputa));
                }

                rm_wifi_chk_channel_by_country(inputa, chan, 0, NULL, 1);

                net_params->xNetworkParams.ucChannel = chan;
            }
        }
        else if (auth_mode > CC_VAL_AUTH_WEP)
        {
            /* Process <enc>,<key>,<ch>,<country_code> for the other auth modes */

            /* <enc> */
            switch (enc_mode)
            {
                case CC_VAL_ENC_TKIP:
                    net_params->xApNetParams.ucEncMode = eWiFiEncryptionTKIP;
                    break;
                case CC_VAL_ENC_CCMP:
                    net_params->xApNetParams.ucEncMode = eWiFiEncryptionAES;
                    break;
                case CC_VAL_ENC_AUTO:
                    net_params->xApNetParams.ucEncMode = eWiFiEncryptionTKIP_AES;
                    break;
                default:
                    net_params->xApNetParams.ucEncMode = eWiFiEncryptionNone;
                    break;
            }

            /* <key> */
            net_params->xNetworkParams.xPassword.xWPA.ucLength = psk_len;
            memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                   p_psk,
                   net_params->xNetworkParams.xPassword.xWPA.ucLength);

            /* <country_code> */
            if (argc >= 7)
            {
                WIFI_SetCountryCode(country);
            }

            /* <ch> */
            if (argc >= 6)
            {
                char inputd[64] = {0, };

 #if defined(__SUPPORT_SETBAND_5GHZ__)
                if (chan > 14)
                {
                    net_params->ucBand = eWiFiBand5G;
                }
                else
                {
                    net_params->ucBand = eWiFiBand2G;
                }
                atcmd_set_wifi_mode(auth_mode, chan, net_params);
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_COUNTRY_CODE,
                                                &p_nv_str);
 #endif

                if (p_nv_str && strlen(p_nv_str))
                {
                    bsp_safe_strcpy(inputd, p_nv_str, sizeof(inputd));
                }
                else
                {
                    bsp_safe_strcpy(inputd, DEFAULT_AP_COUNTRY, sizeof(inputd));
                }

                rm_wifi_chk_channel_by_country(inputd, chan, 0, NULL, 1);

                net_params->xNetworkParams.ucChannel = chan;
            }
        }

        /* Process <ssid> remove the current profile */
        if ((run_mode == WIFI_DEVICE_MODE_EXT_AP) || (run_mode == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            WIFI_StopAP();
            vTaskDelay(portCONVERT_MS_2_TICKS(10));
        }

        WIFI_NetworkDelete(1);

        wifi_err = WIFI_ConfigureAPExt(net_params);
        if (wifi_err)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SAVE_CONF;
            goto end;
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            char nv_str[128] = {0x00, };

            /* SSID */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROFILE_1, 1);
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_MODE_1, 2);

            memset(nv_str, 0x00, sizeof(nv_str));
            memcpy(nv_str, net_params->xNetworkParams.ucSSID, net_params->xNetworkParams.ucSSIDLength);
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_SSID_1,
                                             nv_str);

            /* Set to default channel for case 1 and case 4 */
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL,
                                          1);

            /* Auth */
            memset(nv_str, 0x00, sizeof(nv_str));
            switch ((WIFISecurityExt_t) net_params->xNetworkParams.xSecurity)
            {
                case eWiFiSecurityOpen_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     MODE_AUTH_OPEN_STR);
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_1);
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_1);
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENCKEY_1);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityOpen_ext);
                    break;
                }

                case eWiFiSecurityWEP_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWEP_ext);
                    break;
                }

                case eWiFiSecurityWPA_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA_PSK);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_1,
                                                     proto_WPA);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA_ext);

                    memcpy(nv_str,
                           net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                           net_params->xNetworkParams.xPassword.xWPA.ucLength);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFIPROFILE,
                                                     WIFI_PROFILE_ENCKEY_1,
                                                     nv_str);
                    break;
                }

                case eWiFiSecurityWPA2_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA_PSK);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_1,
                                                     proto_RSN);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA2_ext);

                    memcpy(nv_str,
                           net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                           net_params->xNetworkParams.xPassword.xWPA.ucLength);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFIPROFILE,
                                                     WIFI_PROFILE_ENCKEY_1,
                                                     nv_str);
                    break;
                }

                case eWiFiSecurityWPA_WPA2_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA_PSK);
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_PROTO_1);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA_WPA2_ext);

                    memcpy(nv_str,
                           net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                           net_params->xNetworkParams.xPassword.xWPA.ucLength);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFIPROFILE,
                                                     WIFI_PROFILE_ENCKEY_1,
                                                     nv_str);
                    break;
                }

                case eWiFiSecurityWPA3_OWE_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA3_OWE);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_1,
                                                     proto_RSN);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA3_OWE_ext);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_PMF_1,
                                                  2);

                    bsp_safe_strcpy(nv_str, "CCMP", sizeof(nv_str));
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_ENC_TYPE_1,
                                                     nv_str);
                    break;
                }

                case eWiFiSecurityWPA3_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA3_SAE);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA3_ext);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_1,
                                                     proto_RSN);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_PMF_1,
                                                  2);

                    memcpy(nv_str,
                           net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                           net_params->xNetworkParams.xPassword.xWPA.ucLength);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFIPROFILE,
                                                     WIFI_PROFILE_ENCKEY_1,
                                                     nv_str);
                    break;
                }

                case eWiFiSecurityWPA2_WPA3_ext:
                {
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_AUTH_TYPE_1,
                                                     key_mgmt_WPA_PSK_WPA3_SAE);
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                  ENV_GROUP_WIFIPROFILE,
                                                  WIFI_PROFILE_SECURITY_1,
                                                  eWiFiSecurityWPA2_WPA3_ext);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_PROTO_1,
                                                     proto_RSN);

                    memcpy(nv_str,
                           net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
                           net_params->xNetworkParams.xPassword.xWPA.ucLength);
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFIPROFILE,
                                                     WIFI_PROFILE_ENCKEY_1,
                                                     nv_str);
                    break;
                }

                default:
                {
                    /* Invalid auth mode */
                    break;
                }
            }

            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_AP_ENC_MODE_1,
                                          net_params->xApNetParams.ucEncMode);

            if (strlen(country))
            {
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_COUNTRY_CODE,
                                                 country);
            }

            if (net_params->ucBand == eWiFiBand5G)
            {
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_BAND,
                                              WPA_SETBAND_5G);
            }
            else if (net_params->ucBand == eWiFiBand2G)
            {
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_BAND,
                                              WPA_SETBAND_2G);
            }

            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_CHANNEL,
                                          net_params->xNetworkParams.ucChannel);

            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_WIFI_MODE,
                                          net_params->ucWiFi_mode + GAP_USER_CONFIGURE_WIFI_MODE);
            memset(nv_str, 0x00, sizeof(nv_str));
            switch (enc_mode)
            {
                case CC_VAL_ENC_TKIP:
                {
                    bsp_safe_strcpy(nv_str, "TKIP", sizeof(nv_str));
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_ENC_TYPE_1,
                                                     nv_str);
                    break;
                }

                case CC_VAL_ENC_CCMP:
                {
                    bsp_safe_strcpy(nv_str, "CCMP", sizeof(nv_str));
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                     ENV_GROUP_WIFICFG,
                                                     NVR_KEY_ENC_TYPE_1,
                                                     nv_str);
                    break;
                }

                case CC_VAL_ENC_AUTO:
                {
                    bsp_safe_strcpy(nv_str, "TKIP CCMP", sizeof(nv_str));
                    RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, NVR_KEY_ENC_TYPE_1);
                    break;
                }

                default:
                {
                    break;
                }
            }

            if ((run_mode != WIFI_DEVICE_MODE_EXT_AP) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
            {
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_SYS_MODE,
                                              WIFI_DEVICE_MODE_EXT_AP);
            }
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_COMPLETE,
                                          1);
 #endif

            /* +WFSAP:SSID Response */
            if (p_buf)
            {
                memset(p_buf, 0x00, p_buf_len);
                resp_len = sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));
                memcpy(p_buf + resp_len, net_params->xNetworkParams.ucSSID, net_params->xNetworkParams.ucSSIDLength);
                resp_len += net_params->xNetworkParams.ucSSIDLength;

                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, resp_len);
            }
        }

        if ((run_mode == WIFI_DEVICE_MODE_EXT_AP) || (run_mode == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            vTaskDelay(portCONVERT_MS_2_TICKS(10));
            WIFI_StartAP();
        }
    }

end:

    if (net_params)
    {
        vPortFree(net_params);
        net_params = NULL;
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSAP)
{
    const char * p_usage = "<ssid>,<auth>,<enc>,<key>,<ch>,<country>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSAP)
{
    const char * p_description = "[AP] Operation Soft-AP mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFOAP)
{
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_AP) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    wifi_err = WIFI_StartAP();
    if (wifi_err != eWiFiSuccess)
    {
 #ifdef RM_MAP_PERSISTANT_W
        int value = 0;

        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_COMPLETE,
                                     &value);
        if (value == 1)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_CLI_SOFTAP_START;
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
        }

 #else

        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_SOFTAP_START;
 #endif
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFOAP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFOAP)
{
    const char * p_description = "[AP] Operate Soft-AP";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFTAP)
{
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_AP) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    wifi_err = WIFI_StopAP();
    if (wifi_err != eWiFiSuccess)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_SOFTAP_STOP;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFTAP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFTAP)
{
    const char * p_description = "[AP] Stop Soft-AP";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFRAP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char * p_buf              = NULL;
    char * p_reply            = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    const int run_mode = get_run_mode();

    if ((run_mode != WIFI_DEVICE_MODE_EXT_AP) && (run_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;;
    }

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, p_buf_len);

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_reply = (p_buf + strlen(p_buf));

    atcmd_set_init_done_msg_to_mcu_on_softap(pdFALSE);
    ret = ra6w1_cli_reply("ap restart", NULL, p_reply);
    atcmd_set_init_done_msg_to_mcu_on_softap(pdTRUE);

    if ((ret < 0) || (strncmp(p_reply, "FAIL", 4) == 0))
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SOFTAP_RESTART;
    }
    else if (strncmp(p_reply, "OK", 2) != 0)
    {
        /* Send response */
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf   = NULL;
        p_reply = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFRAP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFRAP)
{
    const char * p_description = "[AP] Restart Soft-AP";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFLCST)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char * p_buf              = NULL;
    char * p_reply            = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (2048 + prefix_resp_len);

    if (get_run_mode() != WIFI_DEVICE_MODE_EXT_AP)
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(p_buf, 0x00, p_buf_len);

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_reply = (p_buf + strlen(p_buf));

    ra6w1_cli_reply("all_sta", NULL, p_reply);

    /* Send response */
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf   = NULL;
        p_reply = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFLCST)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFLCST)
{
    const char * p_description = "[AP] Get Connected STA List";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPWM)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_str[16] = {0x00, };
    int mode          = -1;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFAPWM=? */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_WIFI_MODE,
                                     &mode);
 #endif

 #if defined(__SUPPORT_SETBAND_5GHZ__)
        if ((mode < 0) || (mode > WIFI_MODE_MAX))
        {
            mode = WIFI_MODE_BGN;
        }
        else
        {
            mode -= GAP_USER_CONFIGURE_WIFI_MODE;
        }

 #else
        if ((mode < 0) || (mode > WIFI_MODE_B_ONLY))
        {
            mode = WIFI_MODE_BGN;
        }
        else
        {
            mode -= GAP_USER_CONFIGURE_WIFI_MODE;
        }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), mode);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFAPWM=<mode> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &mode, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

 #if defined(__SUPPORT_SETBAND_5GHZ__)
        if ((mode < 0) || (mode > WIFI_MODE_MAX))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

 #else
        if ((mode < 0) || (mode > WIFI_MODE_B_ONLY))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

        int result = 0;
        result = cc_set_network_int("wifi_mode", 1, mode);
        if (CC_STATUS_SUCCESS != result)
        {
            err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
        }

 #ifdef RM_MAP_PERSISTANT_W

        /* Reqruied to add GAP_USER_CONFIGURE_WIFI_MODE to load wifi_mode configuration */
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WIFI_MODE,
                                      (mode + GAP_USER_CONFIGURE_WIFI_MODE));
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPWM)
{
    const char * p_usage = "<mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPWM)
{
    const char * p_description = "[AP] Set Wi-Fi mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPCH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_str[16] = {0x00, };
    char input[64]    = {0x00, };
    char * p_nv_str   = NULL;

    int ch = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFAPCH=? */
        err = rm_atcmd_w_core_wifi_get_channel(1, &ch);

        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            goto end;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ch);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFAPCH=<ch> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &ch, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_COUNTRY_CODE,
                                        &p_nv_str);
 #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            bsp_safe_strcpy(input, p_nv_str, sizeof(input));
        }
        else
        {
            bsp_safe_strcpy(input, DEFAULT_AP_COUNTRY, sizeof(input));
        }

        if (ch < 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if (rm_wifi_chk_channel_by_country(input, ch, 0, NULL, 1) < 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        cc_set_network_int("channel", 1, ch);

 #ifdef RM_MAP_PERSISTANT_W
        if (ch != 0)
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_CHANNEL,
                                          ch);
        }
        else
        {
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL,
                                          0);
        }
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPCH)
{
    const char * p_usage = "<ch>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPCH)
{
    const char * p_description = "[AP] Set Wi-Fi channel (0:auto)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPCHLIST)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
 #if defined(__SUPPORT_SETBAND_5GHZ__)
    char resp_str[128] = {0x00};
 #else
    char resp_str[19] = {0x00};
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */
    char * p_country_code = NULL;

    if (argc < 2)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 2)
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    /* AT+WFAPCHLIST=<country> */
    p_country_code = argv[1];

    if (strlen(p_country_code) != 2)
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_CC;
    }
    else
    {
        if (rm_wifi_helper_country_code_is_valid(p_country_code))
        {
            int min_ch = 0, max_ch = 0;
 #if defined(__SUPPORT_SETBAND_5GHZ__)
            unsigned int ch_bitmap_5g = 0;
            int ch_24g_str_len        = 0;
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

            rm_wifi_helper_get_ch_range_by_country_n_band(p_country_code, 2, &min_ch, &max_ch, NULL, 0);

            sprintf(resp_str, "\r\n%s:%d-%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), min_ch, max_ch);

 #if defined(__SUPPORT_SETBAND_5GHZ__)
            ch_24g_str_len = strlen(resp_str);

            sprintf(&(resp_str[ch_24g_str_len]), ",");

            rm_wifi_helper_get_ch_range_by_country_n_band(p_country_code, 5, NULL, NULL, &ch_bitmap_5g,
                                                          (CH_FLAG_NO_IR | CH_FLAG_DFS));

            rm_wifi_helper_gen_string_5g_ch_range(&(resp_str[ch_24g_str_len + 1]), ch_bitmap_5g, ',');
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_CC;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPCHLIST)
{
    const char * p_usage = "<country>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPCHLIST)
{
    const char * p_description = "[AP] Show supported operating channels in SoftAP mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPBI)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char p_buf[32] = {0x00, };
    char * p_reply = NULL;
    int interval   = 0;

    const int run_mode = get_run_mode();

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFAPBI=? */
 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N1_beacon_int", &interval))
 #endif
        {
            interval = 100;
        }

        sprintf(p_buf, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), interval);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
    }
    else if (argc == 2)
    {
        /* AT+WFAPBI=<interval> */

        /* Added prefix for response */
        sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_reply = (p_buf + strlen(p_buf));

        if (rm_atcmd_w_core_common_stoi(argv[1], &interval, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((interval < 20) || (interval > 1000))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        cc_set_network_int("beacon_int", 1, interval);

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "N1_beacon_int", interval);
 #endif

        if ((run_mode == WIFI_DEVICE_MODE_EXT_AP) || (run_mode == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            ret = ra6w1_cli_reply("ap restart", NULL, p_reply);

            if ((ret < 0) || (strncmp(p_reply, "FAIL", 4) == 0))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_SOFTAP_RESTART;
                goto end;
            }
            else if (strncmp(p_reply, "OK", 2) != 0)
            {
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
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

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPBI)
{
    const char * p_usage = "<interval>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPBI)
{
    const char * p_description = "[AP] Set Beacon Interval";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPUI)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret           = 0;
    char resp_str[64] = {0x00, };
    char input[64]    = {0x00, };
    int timeout       = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFAPUI=? */
        sprintf(input, "ap_max_inactivity");

        ret = ra6w1_cli_reply(input, NULL, resp_str);

        if ((ret < 0) || (strncmp(resp_str, "FAIL", 4) == 0))
        {
            timeout = DEFAULT_AP_MAX_INACTIVITY;
        }
        else
        {
            rm_atcmd_w_core_common_stoi(resp_str, &timeout, POL_1);
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), timeout);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFAPUI=<timeout> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &timeout, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((timeout < 30) || (timeout > 86400) || (timeout % 10 != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
        else
        {
            sprintf(input, "ap_max_inactivity %d", timeout);

            ret = ra6w1_cli_reply(input, NULL, resp_str);

            if ((ret < 0) || (strncmp(resp_str, "FAIL", 4) == 0))
            {
                err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_STATUS;
            }
            else if ((strncmp(resp_str, "OK", 2) == 0))
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_AP_MAX_INACTIVITY_1,
                                              timeout);
 #endif
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

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPUI)
{
    const char * p_usage = "<timeout>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPUI)
{
    const char * p_description = "[AP] Set User inactivity timeout value (30 ~ 86400)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPRT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_str[64] = {0x00, };
    char input[64]    = {0x00, };
    int threshold     = 0;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFAPRT=? */
 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_RTS_THRESHOLD,
                                         &threshold))
 #endif
        {
            threshold = 2347;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), threshold);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFAPRT=<threshold> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &threshold, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if ((threshold <= 0) || (threshold > 2347))
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "rts_threshold",
                                          threshold);
 #endif
            goto end;
        }

        if ((threshold <= 0) || (threshold > 2347))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }
        else
        {
            sprintf(input, "ap_rts %d", threshold);
            ra6w1_cli_reply(input, NULL, NULL);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFICFG,
                                          ENV_RTS_THRESHOLD,
                                          threshold);
 #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPRT)
{
    const char * p_usage = "<threshold>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPRT)
{
    const char * p_description = "[AP] Set RTS threshold (1 ~ 2347)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPDE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char resp_str[64] = {0x00, };
    char * p_reply    = NULL;
    char cmdline[64]  = {0x00, };

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if (argc == 2)
    {
        /* Check validity for input MAC address string */
        if (atcmd_chk_valid_macaddr((char *) (argv[1])) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_MAC_ADDR;
            goto end;
        }

        /* Added prefix for response */
        sprintf(resp_str, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_reply = (resp_str + strlen(resp_str));

        sprintf(cmdline, "deauthenticate %s", argv[1]);

        ret = ra6w1_cli_reply(cmdline, NULL, p_reply);

        if ((ret < 0) || (strncmp(p_reply, "FAIL", 4) == 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_DEAUTHENTICATE;
        }
        else if (strncmp(p_reply, "OK", 2) != 0)
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
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

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPDE)
{
    const char * p_usage = "<mac>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPDE)
{
    const char * p_description = "[AP] Send Deauth packet to a specific STA";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFAPDI)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int ret = 0;

    char resp_str[64] = {0x00, };
    char * p_reply    = NULL;
    char cmdline[64]  = {0x00, };

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        goto end;
    }

    if (argc == 2)
    {
        /*  Check validity for input MAC address string */
        if (atcmd_chk_valid_macaddr((char *) (argv[1])) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_WRONG_MAC_ADDR;
            goto end;
        }

        /* Added prefix for response */
        sprintf(resp_str, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_reply = (resp_str + strlen(resp_str));

        sprintf(cmdline, "disassociate %s", argv[1]);

        ret = ra6w1_cli_reply(cmdline, NULL, p_reply);

        if ((ret < 0) || (strncmp(p_reply, "FAIL", 4) == 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_DISASSOCIATE;
        }
        else if (strncmp(p_reply, "OK", 2) != 0)
        {
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
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

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFAPDI)
{
    const char * p_usage = "<mac>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFAPDI)
{
    const char * p_description = "[AP] Send Disassoc packet to a specific STA";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFWMM)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #ifdef CONFIG_AP_WMM
    WIFIReturnCode_t wifi_ret = eWiFiSuccess;
    int ret           = 0;
    char resp_str[64] = {0x00, };
    char input[64]    = {0x00, };
    int wmm           = 0;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFWMM=? */
        sprintf(input, "wmm_enabled");

        ret = ra6w1_cli_reply(input, NULL, resp_str);

        if ((ret < 0) || (strncmp(resp_str, "FAIL", 4) == 0))
        {
            wmm = CC_VAL_ENABLE;
        }
        else
        {
            rm_atcmd_w_core_common_stoi(resp_str, &wmm, POL_1);
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), wmm);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFWMM=<wmm> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &wmm, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((wmm != CC_VAL_ENABLE) && (wmm != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        /* In case of STA mode or other run-modes */
        if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_AP_WMM_1,
                                          wmm);
  #endif
        }
        else
        {
            wifi_ret = WIFI_WMM(wmm);
            if (wifi_ret == eWiFiSuccess)
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_AP_WMM_1,
                                              wmm);
  #endif
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFWMM)
{
    const char * p_usage = "<wmm>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFWMM)
{
    const char * p_description = "[AP] WMM on/off";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFWMP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #ifdef CONFIG_AP_WMM
    WIFIReturnCode_t wifi_ret = eWiFiSuccess;
    int ret           = 0;
    char resp_str[64] = {0x00, };
    char input[64]    = {0x00, };
    int wmmps         = 0;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFWMP=? */
        sprintf(input, "wmm_ps_enabled");

        ret = ra6w1_cli_reply(input, NULL, resp_str);

        if ((ret < 0) || (strncmp(resp_str, "FAIL", 4) == 0))
        {
            wmmps = CC_VAL_DISABLE;
        }
        else
        {
            rm_atcmd_w_core_common_stoi(resp_str, &wmmps, POL_1);
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), wmmps);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFWMP=<wmmps> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &wmmps, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((wmmps != CC_VAL_ENABLE) && (wmmps != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        /* In case of STA mode or other run-modes */
        if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_AP_WMM_PS_1,
                                          wmmps);
  #endif
        }
        else
        {
            wifi_ret = WIFI_WMM_PS(wmmps);
            if (wifi_ret == eWiFiSuccess)
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_AP_WMM_PS_1,
                                              wmmps);
  #endif
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFWMP)
{
    const char * p_usage = "<wmmps>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFWMP)
{
    const char * p_description = "[AP] WMM-PS on/off";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFFP2P)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    wifi_err = WIFI_P2PFind();
    if (wifi_err != eWiFiSuccess)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_FIND;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFFP2P)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFFP2P)
{
    const char * p_description = "[P2P] Find P2P devices";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSP2P)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_P2P__)
    int ret                   = 0;
    char * p_buf              = NULL;
    char * p_reply            = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (RM_ATCMD_W_CORE_WIFI_STR_LEN + prefix_resp_len);

    p_buf = (char *) pvPortMalloc(p_buf_len);

    if (p_buf == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_reply = (p_buf + strlen(p_buf));

    ret = ra6w1_cli_reply("p2p_stop_find", NULL, p_reply);

    if ((ret < 0) || (strncmp(p_reply, "FAIL", 4) == 0))
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_FIND_STOP;
    }
    else if (strncmp(p_reply, "OK", 2) != 0)
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
    }

end:

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf   = NULL;
        p_reply = NULL;
    }

 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSP2P)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSP2P)
{
    const char * p_description = "[P2P] Stop the P2P Find operation";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFCP2P)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    char wps_method_str[16]   = {0x00, };

    if (argc == 4)
    {
        /* AT+WFCP2P=<mac>,<wps>,<join> */
        if ((atoi(argv[2]) == 0) && (argv[2][0] != '0'))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_CONN_WPS_TYPE;
        }
        else if ((atoi(argv[3]) == 0) && (argv[3][0] != '0'))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_CONN_JOIN_TYPE;
        }

        if (atoi(argv[2]) == 0)
        {
            bsp_safe_strcpy(wps_method_str, "pbc", sizeof(wps_method_str));
        }
        else if (atoi(argv[2]) == 1)
        {
            bsp_safe_strcpy(wps_method_str, "pin", sizeof(wps_method_str));
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_UNSUPPORT_WPS_TYPE;
        }

        if (atoi(argv[3]) == 1)
        {
            strcat(wps_method_str, " join");
        }
        else if (atoi(argv[3]) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_CONN_JOIN_RANGE;
        }

        wifi_err = WIFI_P2PConnect(argv[1], wps_method_str);
        if (wifi_err != eWiFiSuccess)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_JOIN;
        }
    }
    else
    {
        if (argc < 4)
        {
            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // ( __SUPPORT_P2P__ )
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFCP2P)
{
    const char * p_usage = "<mac>,<wps>,<join>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFCP2P)
{
    const char * p_description = "[P2P] Connect to the peer P2P device";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFDP2P)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;

    wifi_err = WIFI_P2PGroupRemove();
    if (wifi_err != eWiFiSuccess)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_GRP_REMOVE;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFDP2P)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFDP2P)
{
    const char * p_description = "[P2P] Disconnect from P2P device";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPP2P)
{
 #if defined(__SUPPORT_P2P__)
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    WIFIReturnCode_t wifi_err  = eWiFiSuccess;

    char * p_buf              = NULL;
    char * p_reply            = NULL;
    const int prefix_resp_len = (strlen(argv[0] + 2) + 4);
    const int p_buf_len       = (2048 + prefix_resp_len);

    p_buf = (char *) pvPortCalloc(p_buf_len, sizeof(char));
    if (p_buf == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    /* Added prefix for response */
    sprintf(p_buf, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

    p_reply = (p_buf + strlen(p_buf));

    wifi_err = WIFI_P2PPeers(p_reply);
    if (wifi_err != eWiFiSuccess)
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_PEER_INFO;
    }
    else if (strncmp(p_reply, "OK", 2) != 0)
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_buf, strlen(p_buf));
    }

    if (p_buf)
    {
        vPortFree(p_buf);
        p_buf   = NULL;
        p_reply = NULL;
    }

    return err;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPP2P)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPP2P)
{
    const char * p_description = "[P2P] Read P2P Peers information";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPLCH)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    char resp_str[64]         =
    {
        0x00,
    };
    int ch = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPLCH=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_LISTEN_CHANNEL,
                                         &ch))
  #endif
        {
            ch = DEFAULT_P2P_LISTEN_CHANNEL;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ch);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFPLCH=<ch> */
        ch = rm_atcmd_w_core_common_atoi(argv[1]);
        if ((ch != 0) && (ch != 1) && (ch != 6) && (ch != 11))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_LISTEN_CH_RANGE;
        }

        wifi_err = WIFI_P2PSetListenChan(ch);
        if (wifi_err != eWiFiSuccess)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_CLI_P2P_LISTEN_CH;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_LISTEN_CHANNEL, ch);
  #endif
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPLCH)
{
    const char * p_usage = "<ch>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPLCH)
{
    const char * p_description = "[P2P] Set listen channel value <0|1|6|11>";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPOCH)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    char resp_str[64]         =
    {
        0x00,
    };
    int ch = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPOCH=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_OPER_CHANNEL, &ch))
  #endif
        {
            ch = DEFAULT_P2P_OPER_CHANNEL;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ch);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFPOCH=<ch> */
        ch = rm_atcmd_w_core_common_atoi(argv[1]);
        if ((ch != 0) && (ch != 1) && (ch != 6) && (ch != 11))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_OPERATION_CH_RANGE;
        }

        wifi_err = WIFI_P2PSetOperChan(ch);
        if (wifi_err != eWiFiSuccess)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_OPERATION_CH;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_OPER_CHANNEL, ch);
  #endif
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPOCH)
{
    const char * p_usage = "<ch>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPOCH)
{
    const char * p_description = "[P2P] Set operation channel value <0|1|6|11>";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPGI)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    char resp_str[64]         =
    {
        0x00,
    };
    int intent = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPGI=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_GO_INTENT, &intent))
  #endif
        {
            intent = DEFAULT_P2P_GO_INTENT;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), intent);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFPGI=<intent> */
        intent = rm_atcmd_w_core_common_atoi(argv[1]);
        if ((intent < 0) || (intent > 15))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_GO_INTENT_RANGE;
        }

        wifi_err = WIFI_P2PSetGoIntent(intent);
        if (wifi_err != eWiFiSuccess)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_GO_INTENT;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_GO_INTENT, intent);
  #endif
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPGI)
{
    const char * p_usage = "<intent>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPGI)
{
    const char * p_description = "[P2P] Set GO Intent (0~15)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPFT)
{
 #if defined(__SUPPORT_P2P__)
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    char resp_str[64]         =
    {
        0x00,
    };
    int timeout = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPFT=? */
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_FIND_TIMEOUT,
                                         &timeout))
  #endif
        {
            timeout = DEFAULT_P2P_FIND_TIMEOUT;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), timeout);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFPFT=<timeout> */
        if ((rm_atcmd_w_core_common_stoi(argv[1], &timeout, POL_1) != 0))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_FIND_TIMEOUT_TYPE;
        }

        if ((timeout <= 0) || (timeout > 86400))
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_FIND_TIMEOUT_RANGE;
        }

        wifi_err = WIFI_P2PSetFindTimeout(timeout);
        if (wifi_err != eWiFiSuccess)
        {
            return FSP_ERR_AT_CMD_ERR_WIFI_P2P_FIND_TIMEOUT;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, ENV_P2P_FIND_TIMEOUT, timeout);
  #endif
    }
    else
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
 #else

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // (__SUPPORT_P2P__)
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPFT)
{
    const char * p_usage = "<timeout>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPFT)
{
    const char * p_description = "[P2P] Set P2P Find timeout value (1 ~ 86400 sec.)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPDN)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_P2P__)
    char resp_str[64] = {0x00, };
    char cmdline[32]  = {0, };
    char * p_nv_str   = NULL;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPDN=? */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "device_name", &p_nv_str);
  #endif

        if (p_nv_str && strlen(p_nv_str))
        {
            sprintf(resp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), p_nv_str);

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
        }
    }
    else if (argc == 2)
    {
        /* AT+WFPDN=<name> */
        if ((strlen(argv[1]) == 0) || (strlen(argv[1]) > 64))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_READ;
        }
        else
        {
            sprintf(cmdline, "device_name %s", argv[1]);

            ra6w1_cli_reply(cmdline, NULL, NULL);

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "device_name", cmdline);
  #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_P2P__

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPDN)
{
    const char * p_usage = "<name>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPDN)
{
    const char * p_description = "[P2P] Set P2P Device Name";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSMSAVE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_WIFI_CONCURRENT__)
    int ret           = 0;
    char resp_str[64] = {0x00, };
    int value         = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFSMSAVE=? */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, ENV_SWITCH_SYSMODE, &value);
  #endif

        if (value == -1)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;
            goto end;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), value);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* Check validity */
        if ((rm_atcmd_w_core_common_stoi(argv[1], &value, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(value, WIFI_DEVICE_MODE_EXT_STATION,
                                                          WIFI_DEVICE_MODE_EXT_AP) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        /* Save "SWITCH_SYSMODE" to NVRAM which will be used as return sysmode */
  #ifdef RM_MAP_PERSISTANT_W
        ret = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_SYSCFG,
                                            (const char *) ENV_SWITCH_SYSMODE,
                                            (int) value);
  #endif

        if (ret != FSP_SUCCESS)
        {
            err = ret;
        }
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // ( __SUPPORT_WIFI_CONCURRENT__ )

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSMSAVE)
{
    const char * p_usage = "<run_mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSMSAVE)
{
    const char * p_description = "[CON] Save Switching system run-mode (0,1)";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFPSMODE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    WIFIReturnCode_t ret;
    bool ps_mode;
    int value         = 0;
    char resp_str[16] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFPSMODE=? */
        ret = WIFI_GetPsMode(&ps_mode);
        if (ret != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_STATUS;
            goto end;
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), ps_mode);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFPSMODE=<mode> */
        if ((rm_atcmd_w_core_common_stoi(argv[1], &value, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(value, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        ret = WIFI_SetPsMode(value);
        if (ret != eWiFiSuccess)
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_CLI_STATUS;
            goto end;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFPSMODE)
{
    const char * p_usage = "<ps_mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFPSMODE)
{
    const char * p_description = "Set/Get ps mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFMODESWTCH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_WIFI_CONCURRENT__)
    int ret           = 0;
    char resp_str[16] = "\r\nOK\r\n";

    ret = factory_reset_btn_onetouch();

    if (ret == pdTRUE)
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

        vTaskDelay(portCONVERT_MS_2_TICKS(10));

        /* Reboot to change runinng mode */
        PORRESET;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_WIFI_CONCURRENT_NO_PROFILE;
    }

 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // __SUPPORT_WIFI_CONCURRENT__

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFMODESWTCH)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFMODESWTCH)
{
    const char * p_description = "[CON] Switch system run-mode";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFSETBAND)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__SUPPORT_SETBAND_5GHZ__)
    int ret = 0;

    char reply[16]    = {0x00, };
    char input[64]    = {0x00, };
    int band          = 0;
    int nv_band       = 0;
    char resp_str[16] = {0x00, };

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFSETBAND=? */
        ra6w1_cli_reply("get setband", NULL, reply);

        if (strcasecmp(reply, "AUTO") == 0)
        {
            band = 0;
        }
        else
        {
            char band_string[2] = {0x00, };
            band_string[0] = reply[0]; /* copy the 1st character only */
            band = atoi(band_string);
        }

        sprintf(resp_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), band);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        /* AT+WFSETBAND=<band> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &band, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((band != 2) && (band != 5) && band != 0 /* AUTO */
  #if defined(__SUPPORT_SETBAND_6GHZ__)
            && band != 6
  #endif /* __SUPPORT_SETBAND_6GHZ__ */
            )
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if ((get_run_mode() == WIFI_DEVICE_MODE_EXT_AP) && (band == 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WIFI_RUN_MODE_TYPE;
            goto end;
        }

        if (band != 0)
        {
            sprintf(input, "set setband %dG", band);
        }
        else
        {
            sprintf(input, "set setband AUTO");
        }

        ret = ra6w1_cli_reply(input, NULL, reply);

        if ((ret < 0) || (strncmp(reply, "FAIL", 4) == 0))
        {
            err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
        }
        else
        {
            if (band == 2)
            {
                nv_band = WPA_SETBAND_2G;
            }
            else if (band == 5)
            {
                nv_band = WPA_SETBAND_5G;
            }
            else                       /* WPA_SETBAND_AUTO */
            {
                nv_band = WPA_SETBAND_AUTO;
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_BAND,
                                          nv_band);
  #endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:
 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif                                // (__SUPPORT_SETBAND_5GHZ__)

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFSETBAND)
{
    const char * p_usage = "<band>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFSETBAND)
{
    const char * p_description = "Set setband param";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFINIT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_str[16] = {0x00, };

    if (argc == 1)
    {
        ra6w1_network_main_init_wlan_with_task();
    }
    else if (rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        snprintf(resp_str, 16, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2),
                 ra6w1_network_main_is_wlaninit());
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        goto end;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFINIT)
{
    const char * p_usage = "[<?>]";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFINIT)
{
    const char * p_description = "Run the Wi-Fi init or query the Wi-Fi init status";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(WFINITMODE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char resp_str[16] = {0x00, };
    int flag          = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+WFINITMODE=? */
        sprintf(resp_str,
                "\r\n%s:%d",
                rm_atcmd_w_core_common_strupr(argv[0] + 2),
                ra6w1_network_main_get_wlaninit_mode());

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else if (argc == 2)
    {
        if ((rm_atcmd_w_core_common_stoi(argv[1], &flag, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (argc == 2)
        {
            /* AT+WFINITMODE=<flag> */
            if (rm_atcmd_w_core_common_is_in_valid_range(flag, 0, 1) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                goto end;
            }

            if (ra6w1_network_main_set_wlaninit_mode(flag) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NVR_WRITE;
            }
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(WFINITMODE)
{
    const char * p_usage = "<flag>";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(WFINITMODE)
{
    const char * p_description = "Set the Wi-Fi initialization enabled";

    return p_description;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(HOSTINITDONE)
{
    fsp_err_t err = FSP_SUCCESS;

    char resp_str[64] = {0x00, };

    err = rm_wifi_get_atcmd_hostinitdone_resp(resp_str, sizeof(resp_str));
    if ((err == FSP_SUCCESS) && (strlen(resp_str) > 0))
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    return FSP_ERR_AT_CMD_ERR_NO_RESULT;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(HOSTINITDONE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(HOSTINITDONE)
{
    const char * p_description = "Customer MCU Init";

    return p_description;
}

static fsp_err_atcmd_err_code getnvrwifipf_write_handle_int(void * p_ctrl, const char * s, int val)
{
    char at_resp[40] = {0x00, };
    int  n;

    n = snprintf(at_resp, sizeof(at_resp), "%s:%d", s, val);
    if (n >= sizeof(at_resp))
    {
        return FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
    }
    else if (n < 0)
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }
    else
    {
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, n);
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static fsp_err_atcmd_err_code getnvrwifipf_write_handle_str(void * p_ctrl, const char * s, char * str)
{
    char at_resp[40] = {0x00, };
    int  n;

    n = snprintf(at_resp, sizeof(at_resp), "%s:%s", s, str);
    if (n >= sizeof(at_resp))
    {
        return FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
    }
    else if (n < 0)
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }
    else
    {
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, n);
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_CB(GETNVRWIFIPF)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  nv_int;
    bool first       = false;;
    char * p_nv_str  = NULL;

#ifdef RM_MAP_PERSISTANT_W
    /* +GETNVRWIFIPF: */
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) "\r\n+GETNVRWIFIPF:", strlen("\r\n+GETNVRWIFIPF:"));

    /* Name:Data, */
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COUNTRY_CODE, &p_nv_str) == FSP_SUCCESS)
    {
        first = true;
        err = getnvrwifipf_write_handle_str(p_at_ctrl, WIFI_PROFILE_COUNTRY_CODE, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SYS_MODE, &nv_int) == FSP_SUCCESS)
    {
        if (first == true)
        {
            err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_SYS_MODE, nv_int);
        }
        else
        {
            first = true;
            err = getnvrwifipf_write_handle_int(p_at_ctrl, WIFI_PROFILE_SYS_MODE, nv_int);
        }
        
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_BAND, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_CHANNEL, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WIFI_MODE, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_WIFI_MODE, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_PMF_0, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENABLE_DPM, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_ENABLE_DPM, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DPM_DEBUG_RUNTIME_FLAG, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_DPM_DEBUG_RUNTIME_FLAG, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DPM_USER_WAKEUP_TIME, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_DPM_USER_WAKEUP_TIME, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DPM_BLE_HIBERNATE, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_DPM_BLE_HIBERNATE, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_SSID_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_SSID_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_HIDDEN_SSID, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_HIDDEN_SSID, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_PMF_0, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_PMF_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SECURITY_0, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_SECURITY_0, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SECURITY_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_SECURITY_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_ENCKEY_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_ENCKEY_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPKEY0_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_WEPKEY0_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPINDEX_0, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_WEPINDEX_0, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SAE_GROUPS_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_SAE_GROUPS_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SAE_GROUPS_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_SAE_GROUPS_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_AUTH_MODE, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_EAP_AUTH_MODE, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_PHASE2, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_EAP_PHASE2, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_ID, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_EAP_ID, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_PW, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_EAP_PW, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_SSID_POSTFIX, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_P2P_SSID_POSTFIX, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_GROUP_IDLE, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_P2P_GROUP_IDLE, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_LISTEN_CH, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_P2P_LISTEN_CH, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_FIND_TIMEOUT, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_P2P_FIND_TIMEOUT, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_GO_INTENT, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_P2P_GO_INTENT, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_0, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_NETMODE_0, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_NETMODE_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_IPADDR_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_IPADDR_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_IPADDR_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_IPADDR_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMASK_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_NETMASK_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMASK_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_NETMASK_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_GATEWAY_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_GATEWAY_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_GATEWAY_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_GATEWAY_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_DNSSVR_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_DNSSVR_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_2ND_0, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_DNSSVR_2ND_0, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_2ND_1, &p_nv_str) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_str(p_at_ctrl, ","WIFI_PROFILE_DNSSVR_2ND_1, p_nv_str);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_MAX_INACTIVITY_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_AP_MAX_INACTIVITY_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_PS_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_AP_WMM_PS_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_1, &nv_int) == FSP_SUCCESS)
    {
        err = getnvrwifipf_write_handle_int(p_at_ctrl, ","WIFI_PROFILE_AP_WMM_1, nv_int);
        if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            return err;
        }
    }
#endif /* RM_MAP_PERSISTANT_W */

    if (first == false)
    {
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) "empty", 5);
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) "\r\n", 2);

    return err;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_FORMAT_CB(GETNVRWIFIPF)
{
    static const char * p_usage = "";
    return p_usage;
}

RM_ATCMD_W_CORE_WIFI_ATCMD_BRIEF_CB(GETNVRWIFIPF)
{
    static const char * p_description = "Get wifiprofile from NVRAM";
    return p_description;
}

static int rm_atcmd_w_core_wifi_init_network_params (WIFINetworkParamsExt_t * p_net_params)
{
    if (!p_net_params)
    {
        return -1;
    }

    memset(p_net_params, 0x00, sizeof(WIFINetworkParamsExt_t));

    p_net_params->pmf = PMF_DEFAULT;

    return 0;
}

static int rm_atcmd_w_core_wifi_cp_str (char * p_dest, const size_t destlen, const char * p_src, const size_t srclen)
{
    if (!p_dest || !p_src || (destlen < srclen))
    {
        return -1;
    }

    /* Check quotation mark. */
    if ((p_src[0] == 0x22) && (p_src[srclen - 1] == 0x22))
    {
        /* Removed quotation mark. */
        memcpy(p_dest, (p_src + 1), (srclen - 2));
    }
    else
    {
        memcpy(p_dest, p_src, srclen);
    }

    return 0;
}

fsp_err_atcmd_err_code rm_atcmd_w_core_wifi_get_channel (int interface, int * p_channel)
{
    extern int ra6w1_regdb_ch_to_freq(int ch);
    extern int i3ed11_freq_to_ch(int);

    int ret        = 0;
    char input[64] = {0x00, };
    char reply[32] = {0x00, };

    int channel = 0;

    sprintf(input, "get_network %d channel", interface);

    /* Read current channel */
    ret = ra6w1_cli_reply(input, NULL, reply);

    if ((ret < 0) || (strncmp(reply, "FAIL", 4) == 0))
    {
        /* Read configuration */
 #ifdef RM_MAP_PERSISTANT_W
        ret = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                           ENV_GROUP_WIFIPROFILE,
                                           WIFI_PROFILE_CHANNEL,
                                           &channel);
 #endif
        if ((ret != 0) || (channel < 0))
        {
            channel = i3ed11_freq_to_ch(FREQUENCE_DEFAULT);
        }
    }
    else
    {
        channel = atoi(reply);
    }

    ret = ra6w1_regdb_ch_to_freq(channel);
    if (ret == -1)
    {
        return FSP_ERR_AT_CMD_ERR_WIFI_SOFTAP_CH_VALUE_RANGE;
    }

    *p_channel = channel;

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static fsp_err_atcmd_err_code rm_atcmd_w_parse_channel_arg (int                    * argc,
                                                            char * const           * argv,
                                                            WIFINetworkParamsExt_t * net_params)
{
    char const * last_arg      = NULL;
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if ((NULL == argc) || (NULL == argv) || (NULL == net_params))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if (*argc <= 0)
    {
        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    last_arg = argv[*argc - 1];

    if (((last_arg[0] == 'c') || (last_arg[0] == 'C')) &&
        ((last_arg[1] == 'h') || (last_arg[1] == 'H')) &&
        (last_arg[2] == '='))
    {
        err = rm_atcmd_w_core_wifi_parse_channels(last_arg, net_params);

        if (FSP_ERR_AT_CMD_ERR_CMD_OK != err)
        {
            return err;
        }

        /* Ignore channel argument in further parsing */
        (*argc)--;
    }

    return err;
}

static fsp_err_atcmd_err_code rm_atcmd_w_core_wifi_parse_channels (const char             * channel_arg,
                                                                   WIFINetworkParamsExt_t * net_params)
{
    extern int  ra6w1_regdb_ch_to_freq(int ch);
    extern bool wifi_parse_channels(const char * channel_arg, WIFINetworkParamsExt_t * net_params);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint8_t ch_count           = 0;
    uint32_t * channel_list    = NULL;
    char channel_str[128]      = "";
    int off    = 0;
    bool entap = false;

    /* Connect to 'Enterprise AP', is detected by net_params == NULL */
    entap = (net_params == NULL);

    if (entap)
    {
        net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));

        if (net_params == NULL)
        {
            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        /* Initialize the structure */
        memset(net_params, 0, sizeof(WIFINetworkParamsExt_t));
    }

    if ((channel_arg == NULL) || (strlen(channel_arg) == 0))
    {
        /* Clear NVRAM if no channels */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, "scan_channel_list");
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, "scan_channel_number");
 #endif

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    if (!wifi_parse_channels(channel_arg, net_params))
    {
        /* Must clean up the heap allocation of entap on failure */
        if (entap && net_params)
        {
            vPortFree(net_params);
        }

        return FSP_ERR_AT_CMD_ERR_NOT_CONNECTED;
    }

    ch_count     = net_params->ucNumChannels;
    channel_list = net_params->pucChannelList;

    /* Save to NVRAM */
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, "scan_channel_number",
                                  ch_count);

    for (int i = 0; i < ch_count; i++)
    {
        off += snprintf(channel_str + off, sizeof(channel_str) - off, (i == 0 ? "%d" : " %d"), (int) channel_list[i]);
    }
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     "scan_channel_list",
                                     channel_str);
 #endif

    /* AT- non ent case -> done, free should be done in calle. */
    if (!entap)
    {
        return err;
    }

    /*  ENTAP MODE: convert channels to freq and send command  */
    if (ch_count > 0)
    {
        char freq_list[256] = {0};
        int offset          = 0;

        for (uint8_t i = 0; i < ch_count; i++)
        {
            int freq = ra6w1_regdb_ch_to_freq(channel_list[i]);

            if (freq > 0)
            {
                offset += snprintf(freq_list + offset, sizeof(freq_list) - offset, (offset == 0) ? "%d" : " %d", freq);

                if (offset >= (int) sizeof(freq_list))
                {
                    break;
                }
            }
        }

        cc_set_network_str("freq_list", 0, freq_list);
    }

    if (channel_list)
    {
        vPortFree(channel_list);
    }

    if (net_params)
    {
        vPortFree(net_params);
    }

    return err;
}

static void update_wifi_stat_buf (WIFIConnectionInfoExt_t * connection_info_ext, char * p_buf, size_t offset)
{
    for (int i = 0; i < 2; i++)
    {
        char ssid[wificonfigMAX_SSID_LEN + 1];

        memcpy(ssid, connection_info_ext[i].connection_info.ucSSID,
               connection_info_ext[i].connection_info.ucSSIDLength);
        ssid[connection_info_ext[i].connection_info.ucSSIDLength] = '\0';
        if (connection_info_ext[i].connection_info.ucSSIDLength == 0)
        {
            continue;
        }

        offset += sprintf(p_buf + offset, "Interface %d:\n", i);
        offset += sprintf(p_buf + offset, "SSID_LENGTH:%d ", connection_info_ext[i].connection_info.ucSSIDLength);
        offset += sprintf(p_buf + offset, "SSID:%s ", ssid);
        offset += sprintf(p_buf + offset,
                          "BSSID:%02x:%02x:%02x:%02x:%02x:%02x ",
                          connection_info_ext[i].connection_info.ucBSSID[0],
                          connection_info_ext[i].connection_info.ucBSSID[1],
                          connection_info_ext[i].connection_info.ucBSSID[2],
                          connection_info_ext[i].connection_info.ucBSSID[3],
                          connection_info_ext[i].connection_info.ucBSSID[4],
                          connection_info_ext[i].connection_info.ucBSSID[5]);
        offset += sprintf(p_buf + offset, "SECURITY:%d", connection_info_ext[i].connection_info.xSecurity);
        switch ((WIFISecurityExt_t) connection_info_ext[i].connection_info.xSecurity)
        {
            case eWiFiSecurityOpen_ext:
            {
                offset += sprintf(p_buf + offset, "(No Security) ");
                break;
            }

            case eWiFiSecurityWEP_ext:
            {
                offset += sprintf(p_buf + offset, "(WEP Security) ");
                break;
            }

            case eWiFiSecurityWPA_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA Security) ");
                break;
            }

            case eWiFiSecurityWPA2_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA2 Security) ");
                break;
            }

            case eWiFiSecurityWPA2_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA2 Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA3_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA3 Security) ");
                break;
            }

            case eWiFiSecurityWPA_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA_WPA2_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA/WPA2 Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA2/WPA3 Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA3_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA3 Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA3_192B_ent_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA3 192B Enterprise Security) ");
                break;
            }

            case eWiFiSecurityWPA_WPA2_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA/WPA2 Security) ");
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ext:
            {
                offset += sprintf(p_buf + offset, "(WPA2/WPA3 Security) ");
                break;
            }

            case eWiFiSecurityWPA3_OWE_ext:
            {
                offset += sprintf(p_buf + offset, "(OWE Security) ");
                break;
            }

            default:
            {
                offset += sprintf(p_buf + offset, "(Unknown Security) ");
                break;
            }
        }

        offset += sprintf(p_buf + offset, "CHANNEL:%d\n", connection_info_ext[i].connection_info.ucChannel);

        if (connection_info_ext[i].wifi_generation)
        {
            offset += sprintf(p_buf + offset, "WIFI GENERATION:%u\n", connection_info_ext[i].wifi_generation);
        }
    }
}

static WIFIReturnCode_t update_persistant_wifi_profile (char * ssid) {
    WIFIReturnCode_t wifi_err = eWiFiSuccess;
    WIFIConnectionInfoExt_t connection_info_ext[2];

    char * p_reply = NULL;

    p_reply = (char *) pvPortMalloc(wificonfigMAX_PASSPHRASE_LEN);

    if (p_reply == NULL)
    {
        wifi_err = eWiFiFailure;
    }
    else
    {
        memset(p_reply, 0x00, wificonfigMAX_PASSPHRASE_LEN);

        wifi_err = WIFI_GetConnectionInfoExt(connection_info_ext, 2);
        if (eWiFiSuccess != wifi_err)
        {
            printf("WIFI_GetConnectionInfo ERROR=%d\n", wifi_err);
        }

 #ifdef RM_MAP_PERSISTANT_W
        else
        {
            memcpy(ssid,
                   (char *) connection_info_ext[0].connection_info.ucSSID,
                   connection_info_ext[0].connection_info.ucSSIDLength);
            ssid[connection_info_ext[0].connection_info.ucSSIDLength] = '\0';
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_SSID_0,
                                             (char *) connection_info_ext[0].connection_info.ucSSID);
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_SECURITY_0,
                                          connection_info_ext[0].connection_info.xSecurity);

            if (strcmp((char *) connection_info_ext[0].connection_info.xSecurity, "eWiFiSecurityOpen") != 0)
            {
                ra6w1_cli_reply("get_network 0 psk", NULL, p_reply);
                p_reply = p_reply + 1;
                p_reply[strlen(p_reply) - 1] = '\0';
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_ENCKEY_0,
                                                 p_reply);
            }

            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_CHANNEL,
                                          connection_info_ext[0].connection_info.ucChannel);

            if ((connection_info_ext[0].connection_info.ucChannel >= 1) &&
                (connection_info_ext[0].connection_info.ucChannel <= 14))
            {
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_BAND,
                                              WPA_SETBAND_2G);
            }
            else if ((connection_info_ext[0].connection_info.ucChannel >= 36) &&
                     (connection_info_ext[0].connection_info.ucChannel <= 165))
            {
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_WIFIPROFILE,
                                              WIFI_PROFILE_BAND,
                                              WPA_SETBAND_5G);
            }

            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_WIFIPROFILE,
                                          WIFI_PROFILE_COMPLETE,
                                          1);
        }
    }
 #endif

        return wifi_err;
    }

    static int hex_2_num (char c)
    {
        if ((c >= '0') && (c <= '9'))
        {
            return c - '0';
        }
        else if ((c >= 'a') && (c <= 'f'))
        {
            return c - 'a' + 10;
        }
        else if ((c >= 'A') && (c <= 'F'))
        {
            return c - 'A' + 10;
        }

        return -1;
    }

    static int hex_2_byte (const char * hex)
    {
        int a, b;

        a = hex_2_num(*hex++);

        if (a < 0)
        {
            return -1;
        }

        b = hex_2_num(*hex++);

        if (b < 0)
        {
            return -1;
        }

        return (a << 4) | b;
    }

    static size_t str_decode (u8 * buf, size_t maxlen, const char * str)
    {
        const char * pos = str;
        size_t len       = 0;
        int val;

        while (*pos)
        {
            if (len >= maxlen)
            {
                /* Too longg string length */
                len = 999;
                break;
            }

            /* Check '\' character */
            switch (*pos)
            {
                case '\\':
                {
                    pos++;

                    /* Check hexa code or not ... */
                    switch (*pos)
                    {
                        case '\\':
                        {
                            buf[len++] = '\\';
                            pos++;
                            break;
                        }

                        case '"':
                        {
                            buf[len++] = '"';
                            pos++;
                            break;
                        }

                        case 'n':
                        {
                            buf[len++] = '\\';
                            buf[len++] = 'n';
                            pos++;
                            break;
                        }

                        case 'r':
                        {
                            buf[len++] = '\\';
                            buf[len++] = 'r';
                            pos++;
                            break;
                        }

                        case 't':
                        {
                            buf[len++] = '\\';
                            buf[len++] = 't';
                            pos++;
                            break;
                        }

                        case 'x':
                        {
                            pos++;
                            val = hex_2_byte(pos);

                            if (val < 0)
                            {
                                val = hex_2_num(*pos);

                                if (val < 0)
                                {
                                    break;
                                }

                                buf[len++] = val;
                                pos++;
                            }
                            else
                            {
                                buf[len++] = val;
                                pos       += 2;
                            }

                            break;
                        }

                        default:
                        {
                            /* Not control character. */
                            buf[len++] = *(pos - 1);
                            break;
                        }
                    }

                    break;
                }

                default:
                {
                    buf[len++] = *pos++;
                    break;
                }
            }
        }

        if (len == maxlen)
        {
            buf[len + 1] = '\0';
        }
        else if (maxlen > len)
        {
            buf[len] = '\0';
        }

        return len;
    }

    static int atcmd_chk_wifi_conn (void)
    {
        char * reply = NULL;
        int result   = pdFALSE;

        reply = (char *) pvPortMalloc(512);

        if (reply == NULL)
        {
            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        memset(reply, 0x00, 512);

        ra6w1_cli_reply("status", NULL, reply);

        if (strstr(reply, "wpa_state=COMPLETED"))
        {
            result = pdTRUE;
        }

        vPortFree(reply);

        return result;
    }

    static int atcmd_chk_valid_macaddr (char * macaddr_str)
    {
 #define MAC_ADDR_LEN_W_DEL    17
 #define MAC_ADDR_DEL          ':'

        int input_mac_len;
        char input_mac_str[MAC_ADDR_LEN_W_DEL + 1];

        input_mac_len = strlen(macaddr_str);

        if (input_mac_len != MAC_ADDR_LEN_W_DEL)
        {
            return pdFALSE;
        }

        memset(input_mac_str, 0, MAC_ADDR_LEN_W_DEL);
        bsp_safe_strcpy(input_mac_str, macaddr_str, sizeof(input_mac_str));

        for (int i = 0; i < MAC_ADDR_LEN_W_DEL; i++)
        {
            if ((i == 2) || (i == 5) || (i == 8) || (i == 11) || (i == 14))
            {
                if (input_mac_str[i] != MAC_ADDR_DEL)
                {
                    return pdFALSE;
                }
                else
                {
                    continue;
                }
            }

            if (!isxdigit((int) input_mac_str[i]))
            {
                return pdFALSE;
            }
        }

        return pdTRUE;
    }

 #define EVT_WFJAP_CONN_SENT    0x01

    static void atcmd_wfjap_conn_sent (void)
    {
        if (gp_evt_grp_wfjap)
        {
            xEventGroupSetBits(gp_evt_grp_wfjap, EVT_WFJAP_CONN_SENT);
        }
    }

    static void atcmd_rsp_wifi_conn (atcmd_w_ctrl_t * const p_at_ctrl)
    {
 #if CFG_PMGR
        extern void dpm_abnormal_chk_hold(void);
        extern void force_dpm_abnormal_sleep_by_wifi_conn_fail(void);
        extern void dpm_abnormal_chk_resume(void);
 #endif                                /* CFG_PMGR */
        extern volatile int wifi_conn_fail_reason_code;

        int old_reason_code = 0;

        EventBits_t event = 0;
        const int wait_count_initial_value = 3000; // Sync with #define MAX_INIT_WIFI_CONN_TIME 30
        TickType_t wait_opt                = wait_count_initial_value / 2;

        is_waiting_for_wf_jap_result_in_progress = TRUE;

 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            dpm_abnormal_chk_hold();
        }
 #endif                                /* CFG_PMGR */

LBL_WIFI_RSP_CHK:
        event = xEventGroupWaitBits(gp_evt_grp_wfjap, EVT_WFJAP_CONN_SENT, pdTRUE, pdFALSE, wait_opt * 5);

        if (event & EVT_WFJAP_CONN_SENT)
        {
            is_waiting_for_wf_jap_result_in_progress = FALSE;

 #if CFG_PMGR
            if (RM_PMGR_W_dpm_is_enabled())
            {
                force_dpm_abnormal_sleep_by_wifi_conn_fail();
                dpm_abnormal_chk_resume();
            }
 #endif                                /* CFG_PMGR */

            goto LBL_WIFI_RSP_CHK_END;
        }
        else
        {
            /* timeout */
            if (wifi_conn_fail_reason_code != 45 /* WLAN_REASON_PEERKEY_MISMATCH */)
            {
                if (wait_opt == wait_count_initial_value / 2)
                {
                    wait_opt = wait_count_initial_value;
                    goto LBL_WIFI_RSP_CHK;
                }
            }
        }

        ra6w1_cli_reply("disconnect", NULL, NULL);
        is_waiting_for_wf_jap_result_in_progress = FALSE;
        old_reason_code = wifi_conn_fail_reason_code;

        if (wifi_conn_fail_reason_code != 45 /* WLAN_REASON_PEERKEY_MISMATCH */)
        {
            wifi_conn_fail_reason_code = WLAN_REASON_TIMEOUT;
        }

        atcmd_wf_jap_dap_print_with_cause(p_at_ctrl, 1); // send connect trial failure
        wifi_conn_fail_reason_code = old_reason_code;

 #if defined(__SUPPORT_MQTT__)
        RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(pdFALSE);
 #endif

 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            force_dpm_abnormal_sleep_by_wifi_conn_fail();
            dpm_abnormal_chk_resume();
        }
 #endif                                /* CFG_PMGR */

LBL_WIFI_RSP_CHK_END:
    }

 #if defined(__SUPPORT_SETBAND_5GHZ__)
    static void atcmd_set_wifi_mode (int auth_mode, int chan, WIFINetworkParamsExt_t * net_params)
    {
        if (auth_mode == CC_VAL_AUTH_WPA)
        {
            if (chan >= 36)
            {
                net_params->ucWiFi_mode = WIFI_MODE_A_ONLY;
            }
            else if ((chan == 0) && (net_params->ucBand == eWiFiBand5G))
            {
                net_params->ucWiFi_mode = WIFI_MODE_A_ONLY;
            }
            else
            {
                net_params->ucWiFi_mode = WIFI_MODE_BG;
            }
        }
        else if ((chan >= 36) || ((chan == 0) && (net_params->ucBand == eWiFiBand5G)))
        {
            net_params->ucWiFi_mode = WIFI_MODE_AN;
        }
        else if ((chan <= 14) || ((chan == 0) && (net_params->ucBand == eWiFiBand2G)))
        {
            net_params->ucWiFi_mode = WIFI_MODE_BGN;
        }
    }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

    unsigned int RM_ATCMD_W_CORE_WIFI_asynchony_event_cb (void * const    p_ctrl,
                                                          int             index,
                                                          unsigned char * p_in,
                                                          unsigned int    inlen)
    {
        return rm_atcmd_w_core_wifi_set_async_msg(p_ctrl, index, p_in, inlen);
    }

    unsigned int RM_ATCMD_W_CORE_WIFI_startup_event_cb (void * const p_ctrl, unsigned char * p_in, unsigned int inlen)
    {
        fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

        if (!p_ctrl && !p_in && (inlen == 0))
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        atcmd_w_core_instance_ctrl_t * at_cmd_ctrl = (atcmd_w_core_instance_ctrl_t *) p_ctrl;
        at_cmd_ctrl->init_rsp_not_sent = 1;
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_in, (size_t) inlen);
        at_cmd_ctrl->init_rsp_not_sent = 0;

        return err;
    }

    void atcmd_wf_jap_dap_print_with_cause (atcmd_w_ctrl_t * const p_at_ctrl, int is_jap)
    {
        extern volatile int wifi_conn_fail_reason_code;
        extern volatile int wifi_disconn_reason_code;
        int n = 0;

        char resp_str[128] =
        {
            0x00,
        };

 #define WLAN_REASON_TIMEOUT                            39
 #define WLAN_REASON_PEERKEY_MISMATCH                   45
 #define WLAN_REASON_AUTHORIZED_ACCESS_LIMIT_REACHED    46

 #define WLAN_REASON_PREV_AUTH_NOT_VALID                2
 #define WLAN_REASON_DEAUTH_LEAVING                     3
 #define WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY         4
 #define WLAN_REASON_DISASSOC_AP_BUSY                   5

        if (p_at_ctrl == NULL)
        {
            return;
        }

        if (is_waiting_for_wf_jap_result_in_progress)
        {
            return;
        }

        if (is_jap)
        {
            switch (wifi_conn_fail_reason_code)
            {
                case WLAN_REASON_TIMEOUT:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFJAP:0,TIMEOUT\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case WLAN_REASON_PEERKEY_MISMATCH:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFJAP:0,WRONGPWD\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case WLAN_REASON_AUTHORIZED_ACCESS_LIMIT_REACHED:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFJAP:0,ACCESSLIMIT\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                default:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFJAP:0,OTHER,%d\r\n", wifi_conn_fail_reason_code);
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, n);
                    }
                    break;
                }
            }
        }
        else
        {
            switch (wifi_disconn_reason_code)
            {
                case WLAN_REASON_PREV_AUTH_NOT_VALID:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFDAP:0,AUTH_NOT_VALID\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case WLAN_REASON_DEAUTH_LEAVING:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFDAP:0,DEAUTH\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFDAP:0,INACTIVITY\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case WLAN_REASON_DISASSOC_AP_BUSY:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFDAP:0,APBUSY\r\n");
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
                    }
                    break;
                }

                case 0:
                {
                    /* undefined reason, skip */
                    break;
                }

                default:
                {
                    n = snprintf(resp_str, sizeof(resp_str), "\r\n+WFDAP:0,OTHER,%d\r\n", wifi_disconn_reason_code);
                    if ((n > 0) && (n < (int) sizeof(resp_str)))
                    {
                        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, n);
                    }
                    break;
                }
            }
        }
    }

    unsigned int rm_atcmd_w_core_wifi_set_async_msg (void * const    p_ctrl,
                                                     int             index,
                                                     unsigned char * p_in,
                                                     unsigned int    inlen)
    {
        unsigned int ret = pdPASS;
        rm_atcmd_w_core_wifi_async_msg_t async_msg = {0x00, };

        if (rm_atcmd_w_core_wifi_async_queue == NULL)
        {
            return pdFAIL;
        }
        
        async_msg.p_ctrl = p_ctrl;
        async_msg.index  = index;
        async_msg.inlen  = inlen;

        if ((inlen > 0) && (p_in != NULL))
        {
            async_msg.p_in = pvPortCalloc(inlen, sizeof(char));
            if (!async_msg.p_in)
            {
                printf("[%s:%d]Failed to allocate memory(size=%d)\n", __func__, __LINE__, inlen);
                return pdFAIL;
            }

            memcpy(async_msg.p_in, p_in, inlen);
        }

        ret = xQueueSend(rm_atcmd_w_core_wifi_async_queue, &async_msg, portNO_DELAY);
        if (ret != pdPASS)
        {
            printf("[%s:%d]Failed to send async queue(%d)\n", __func__, __LINE__, ret);
            if (async_msg.p_in)
            {
                vPortFree(async_msg.p_in);
                async_msg.p_in = NULL;
            }
        }

        return ret;
    }

    unsigned int rm_atcmd_w_core_wifi_oper_async_msg (rm_atcmd_w_core_wifi_async_msg_t * p_async_msg)
    {
        fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

        void * const p_ctrl  = p_async_msg->p_ctrl;
        int index            = p_async_msg->index;
        unsigned char * p_in = p_async_msg->p_in;
        unsigned int inlen   = p_async_msg->inlen;

        char * p_nv_str           = NULL;
        char ssid_str[32 * 4 + 1] =
        {
            0x00,
        };
        char ip_str[16] =
        {
            0x00,
        };
        int netmode = 0;

        char * p_resp                 = NULL;
        const unsigned int p_resp_len = (16 + sizeof(ssid_str) + sizeof(ip_str) + inlen);

        if (p_ctrl == NULL)
        {
            return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
        }

        p_resp = pvPortCalloc(p_resp_len, sizeof(char));
        if (!p_resp)
        {
            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        switch (index)
        {
            case RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_STACONN: /* ATC_EV_STACONN */
            {
                if ((inlen > 0) && (p_in != NULL) && (strncmp((const char *) p_in, "1", 1) == 0))
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                 ENV_GROUP_WIFIPROFILE,
                                                 WIFI_PROFILE_NETMODE_0,
                                                 &netmode);
 #endif
                    if (netmode == -1)
                    {
                        if (get_netmode(0) == STATIC_IP)
                        {
                            netmode = STATIC_IP;
                        }
                        else
                        {
                            netmode = DHCPCLIENT;
                        }
                    }
                    else
                    {
                        if ((netmode != DHCPCLIENT) && (netmode != STATIC_IP))
                        {
                            /* Invalid netmode, force DHCPCLIENT */
                            netmode = DHCPCLIENT;
                        }
                    }

 #if CFG_PMGR
                    if ((netmode == DHCPCLIENT) && !RM_PMGR_W_dpm_is_wakeup())
 #else
                    if (netmode == DHCPCLIENT)
 #endif                                /* CFG_PMGR */
                    {
                        goto end;
                    }
                }

 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_SSID_0,
                                                &p_nv_str);
 #endif

                if (!p_nv_str ||
                    (rm_atcmd_w_core_wifi_cp_str(ssid_str, sizeof(ssid_str), p_nv_str, strlen(p_nv_str)) != 0))
                {
 #if defined(__SUPPORT_MQTT__)
                    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state(pdTRUE);
 #endif
                    if (eWiFiSuccess != update_persistant_wifi_profile(ssid_str))
                    {
 #if defined(__SUPPORT_MQTT__)
                        RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state(pdTRUE);
 #endif
                        goto end;
                    }
                    else
                    {
 #ifdef RM_MAP_PERSISTANT_W
                        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                         ENV_GROUP_WIFIPROFILE,
                                                         WIFI_PROFILE_SSID_0,
                                                         ssid_str);
                        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_WIFIPROFILE,
                                                      WIFI_PROFILE_COMPLETE,
                                                      1);
 #endif
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    get_ip_info(0, GET_IPADDR, ip_str);
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_IPADDR_0,
                                                    &p_nv_str);
 #endif
                    if (p_nv_str && strlen(p_nv_str))
                    {
                        bsp_safe_strcpy(ip_str, p_nv_str, sizeof(ip_str));
                    }
                    else
                    {
                        /* For static IP configuration and if there's no items saved to nvram yet, check netif again */
                        get_ip_info(0, GET_IPADDR, ip_str);
                    }
                }

                if (strcmp(ip_str, "0.0.0.0") != 0)
                {
                    sprintf(p_resp, "\r\n+WFJAP:1,'%s',%s\r\n", ssid_str, ip_str);

                    RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));

                    atcmd_wfjap_conn_sent();

 #if defined(__SUPPORT_MQTT__)
                    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(pdTRUE);
                    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state(pdFALSE);
 #endif
                }

 #if defined(__SUPPORT_MQTT__)
                else
                {
                    RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state(pdFALSE);
                }
 #endif
                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_STADISCONN: /* ATC_EV_STADISCONN: */
            {
                atcmd_wf_jap_dap_print_with_cause(p_ctrl, 0);

 #if defined(__SUPPORT_MQTT__)
                RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state(pdFALSE);
 #endif
                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_APCONN: /* ATC_EV_APCONN: */
            {
                sprintf(p_resp, "\r\n+WFCST:%s\r\n", p_in);

                RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));

                for (int i = 0; i < 6; i++)
                {
                    if (strlen(atcmd_mac_table[i]) == 0)
                    {
                        bsp_safe_strcpy(atcmd_mac_table[i], (const char *) p_in, sizeof(atcmd_mac_table[i]));
                        break;
                    }
                }

                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_ATC_EV_APDISCONN: /* ATC_EV_APDISCONN: */
            {
                sprintf(p_resp, "\r\n+WFDST:%s\r\n", p_in);
                RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));

                for (int i = 0; i < 6; i++)
                {
                    if (strcmp(atcmd_mac_table[i], (const char *) p_in) == 0)
                    {
                        bsp_safe_strcpy(atcmd_mac_table[i], "", sizeof(atcmd_mac_table[i]));
                        break;
                    }
                }

                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_INIT_DONE: /* +INIT:DONE,1 */
            {
                if (get_run_mode() == WIFI_DEVICE_MODE_EXT_AP)
                {
                    sprintf(p_resp, "\r\n+INIT:DONE,%d\r\n", get_run_mode());

                    RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));
                }

                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_CONN_FAILURE: /* Connect trial failure */
            {
                atcmd_wf_jap_dap_print_with_cause(p_ctrl, 1);
                break;
            }

            case RM_ATCMD_W_CORE_WIFI_ASYNC_PRINT:
            {
                RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_in, inlen);
                break;
            }
        }

end:
        if (p_resp)
        {
            vPortFree(p_resp);
            p_resp = NULL;
        }

        return err;
    }

    static void rm_atcmd_w_core_wifi_async_task_entry (void * p_params)
    {
        RA6W1_UNUSED_ARG(p_params);

        BaseType_t ret = pdPASS;
        rm_atcmd_w_core_wifi_async_msg_t async_msg =
        {
            0x00,
        };

        while (1)
        {
            memset(&async_msg, 0x00, sizeof(async_msg));

            ret = xQueueReceive(rm_atcmd_w_core_wifi_async_queue, &async_msg, portMAX_DELAY);
            if (ret == pdTRUE)
            {
                if (async_msg.index == RM_ATCMD_W_CORE_WIFI_ASYNC_CLOSE)
                {
                    break;
                }

                rm_atcmd_w_core_wifi_oper_async_msg(&async_msg);

                if (async_msg.p_in)
                {
                    vPortFree(async_msg.p_in);
                    async_msg.p_in = NULL;
                }
            }
        }

        rm_atcmd_w_core_wifi_async_task = NULL;
        vTaskDelete(NULL);
    }

    BaseType_t rm_atcmd_w_core_wifi_start_async_task (void)
    {
        if (rm_atcmd_w_core_wifi_async_task)
        {
            return pdFALSE;
        }

        rm_atcmd_w_core_wifi_async_queue = xQueueCreate(2, sizeof(rm_atcmd_w_core_wifi_async_msg_t));
        if (rm_atcmd_w_core_wifi_async_queue == NULL)
        {
            return pdFALSE;
        }

        return xTaskCreate(rm_atcmd_w_core_wifi_async_task_entry,
                           "async_task",
                           (512),
                           (void *) NULL,
                           OS_TASK_PRIORITY_LOWEST,
                           &rm_atcmd_w_core_wifi_async_task);
    }

    BaseType_t  rm_atcmd_w_core_wifi_stop_async_task (void)
    {
        for(int i = 0; i < 5; i ++)
        {
            if (rm_atcmd_w_core_wifi_async_task == NULL)
            {
                if (rm_atcmd_w_core_wifi_async_queue)
                {
                    vQueueDelete(rm_atcmd_w_core_wifi_async_queue);
                    rm_atcmd_w_core_wifi_async_queue = NULL;
                    return pdPASS;
                }
            }
            else
            {
                vTaskDelay(portCONVERT_MS_2_TICKS(10));
            }
        }

        return pdFALSE;
    }
#endif                                 /* CFG_WIFI */
