/**
 ****************************************************************************************
 *
 * @file rm_cli_w_easysetup.c
 *
 * @brief Network system application
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

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#if defined(__SUPPORT_EASY_SETUP__) && defined(__SUPPORT_APP_CONSOLE_INPUT__) && defined(CFG_WIFI)
 #include "sys_feature.h"
 #include <string.h>
 #include <stdio.h>
 #include <osal.h>

 #include "rm_cli_w_net.h"             /* For print_separate_bar */
 #include "common_def.h"
 #include "common_utils.h"

 #include "iface_defs.h"
 #include "nvedit.h"

 #include "common/defs.h"
 #include "supp_def.h"
 #include "supp_config.h"              /* For wpa_config_delete_nvram, os_calloc */
 #include "net_common.h"
 #include "net_sntp_client.h"
 #include "net_dns_client.h"

 #include "lwipopts.h"
 #include "lwip/ip_addr.h"
 #include "lwip/netif.h"
 #include "lwip/dhcp.h"
 #include "lwip/dns.h"
 #include "dhcpserver.h"
 #include "dhcpserver_options.h"
 #include "lwip/etharp.h"
 #include "dhcpserver.h"
 #include "net_arp.h"
 #include "rm_wifi_reg_pwr_db.h"
 #include "rm_cli_w_easysetup.h"
 #include "rm_wifi.h"
 #include "rm_cert.h"
 #include "rm_wifi_helper.h"
 #include "rm_lwip_w_helper.h"
 #include "rm_dhcp.h"
 #include "r_gpio_w.h"
 #include "rm_map_persistant_w.h"
 #include "r_rtc_w.h"
 #include "base64.h"
 #if CFG_CLI
  #include "rm_cli_w.h"
 #endif
 #include dg_configADNVPARAM_PROJ_FILE
 #if CFG_PMGR
  #include "rm_pmgr_w_instance.h"
  #include "rm_pmgr_w_dpm_internal.h"
 #endif                                /* CFG_PMGR */

extern const char * ra6w1_regdb_get_cty_from_idx(int cty_idx);
extern int          ra6w1_regdb_ch_to_freq(int ch);
extern int          getStr(char * get_data, int get_len);

extern UINT  set_netInit_flag(UINT iface);
extern void  clear_netInit_flag(void);
extern err_t ethernetif_init(struct netif * netif);
extern err_t ethernet_input(struct pbuf * p, struct netif * netif);

 #if CFG_PMGR
extern int RM_PMGR_W_dpm_is_enabled(void);

 #endif                                /* CFG_PMGR */
static unsigned char parse_ssid_input(char * p_ssid, bool gen_default);

static TaskHandle_t ra6w1_setup_task;

/* EasySetup mem pool space */
 #define RRQ61X_SETUP_POOL_SIZE    ((1024 / 4) * 7) /* 7K bytes */
 #define RRQ61X_SETUP_PRIORITY     1

enum
{
    E_WLAN_SETUP = 200,
    E_INPUT_SSID,
 #ifdef __SUPPORT_WPA_ENTERPRISE__
    E_INPUT_EAP_METHOD,
 #endif                                /* __SUPPORT_WPA_ENTERPRISE__ */
    E_INPUT_PSK_KEY,
    E_INPUT_WEP,
    E_AVD_CFG,
    E_SETUP_WIFI_END,
    E_NETWORK_SETUP,
    E_SETUP_APPLY_START,
    E_INPUT_IPADDRESS,
    E_SETUP_APPLY_SYSMODE,
    E_INPUT_SKIP_ALL_1,
    E_INPUT_SKIP_ALL_2,
    E_CONTINUE,
    E_ERROR = 254,
    E_QUIT  = 255
};

/***********************************************************************************************************************
 * External variables
 **********************************************************************************************************************/
extern const ioport_instance_t g_gpio_w;

/*---------------------------------------------------------------------------
 *      Internal variables
 *---------------------------------------------------------------------------*/
const char * wifi_mode_str[] =
{
    "11b/g/n  (2.4GHz)",
    "11g/n    (2.4GHz)",
    "11b/g    (2.4GHz)",
    "11n only (2.4GHz)",
    "11g only (2.4GHz)",
    "11b only (2.4GHz)",
    "11a/n    (5GHz)",
    "11a only (5GHz)",
    "11n only (5GHz)"
};

static const char * wifi_mode_names[] =
{
    [WIFI_DEVICE_MODE_EXT_STATION]       = "Station",
    [WIFI_DEVICE_MODE_EXT_AP]            = "Soft-AP",
    [WIFI_DEVICE_MODE_EXT_P2P]           = "WiFi Direct",
    [WIFI_DEVICE_MODE_EXT_P2P_GO]        = "WiFi Direct P2P GO Fixed",
    [WIFI_DEVICE_MODE_EXT_AP_STATION]    = "Station & Soft-AP",
    [WIFI_DEVICE_MODE_EXT_P2P_STATION]   = "WiFi Direct + Station",
    [WIFI_DEVICE_MODE_EXT_MESH_POINT]    = "Mesh Point",
    [WIFI_DEVICE_MODE_EXT_MESH_PORTAL]   = "Mesh Portal",
    [WIFI_DEVICE_MODE_EXT_NOT_SUPPORTED] = "Not Supported"
};

/*
 *  wpas_mode WPAS_MODE_INFRA = 0,
 *  WPAS_MODE_IBSS = 1,
 *  WPAS_MODE_AP = 2,
 *  WPAS_MODE_P2P_GO = 3,
 *  WPAS_MODE_P2P_GROUP_FORMATION = 4,
 */

/*---------------------------------------------------------------------------
 *      Internal Functions
 *---------------------------------------------------------------------------*/

 #define GETNUM_LENGTH    9
static int getNum (void)
{
    char get_data[GETNUM_LENGTH];
    int  i       = 0;
    int  get_len = 0;
    int  num;
    memset(get_data, 0, GETNUM_LENGTH);

    get_len = getStr(get_data, GETNUM_LENGTH - 1);

    if (get_len > 0)
    {
        if (get_len > GETNUM_LENGTH - 1)
        {
            return RET_OVERFLOW;
        }

        if (((get_data[0] == 'Q') || (get_data[0] == 'q')) && (strlen(get_data) == 1))
        {
            return RET_QUIT;           /* quit */
        }
        else if (((get_data[0] == 'M') || (get_data[0] == 'm')))
        {
            if (strlen(get_data) == 1)
            {
                return RET_MANUAL;
            }
            else if ((strlen(get_data) == 2) || (strlen(get_data) == 3)) /* M## (for Select SSID only) */
            {
                i = 1;
            }
            else
            {
                return RET_OVERFLOW;
            }
        }
        else
        {
            i = 0;
        }

        while (get_data[i] != 0)
        {
            if (isdigit((int) (get_data[i])) == 0)
            {
                return RET_NODIGIT;
            }

            i++;
        }

        if (((get_data[0] == 'M') || (get_data[0] == 'm')))
        {
            num = -(atoi(get_data + 1));
        }
        else
        {
            num = atoi(get_data);
        }

        if ((num > 90000000) || (num < -80)) /* Max List */
        {
            return RET_OVERFLOW;
        }

        return num;
    }
    else if (get_len == RET_QUIT)
    {
        return RET_QUIT;
    }

    return RET_DEFAULT;
}

static int get_yes_no (char * prompt, bool allow_default)
{
    char input_str[3];
    int  len;

    do
    {
        printf(prompt);
        len = getStr(input_str, 2);

        if (len == RET_QUIT)
        {
            return E_QUIT;
        }
        else if (len == 0)
        {
            if (allow_default)
            {
                return E_CONTINUE;
            }
        }
        else if (len == 1)
        {
            switch (toupper(input_str[0]))
            {
                case 'Q':
                {
                    return E_QUIT;
                }

                case 'M':
                {
                    return 2;
                }

                case 'Y':
                {
                    return 1;
                }

                case 'N':

                    return 0;
            }
        }
    } while (1);
}

static int isvalid_domain (char * str)
{
    unsigned char count = 0;

    for (unsigned int i = 0; i < strlen(str); i++)
    {
        if (str[i] == '.')
        {
            count++;
        }
    }

 #ifdef __SUPPORT_IPV4__
    if ((is_in_valid_ip_class(str) && (count == 3)))
    {
        return 2;
    }

 #else                                 // __SUPPORT_IPV6
    if (count == 3)
    {
        return 2;
    }
 #endif
    else if (count > 0)
    {
        return 1;
    }

    return pdFALSE;
}

static inline bool is_sntp_supported_sysmode (e_wifi_device_mode_ext_t mode)
{
    if (mode == WIFI_DEVICE_MODE_EXT_AP)
    {
        return false;
    }

 #if defined(__SUPPORT_P2P__)
    if (mode == WIFI_DEVICE_MODE_EXT_P2P_GO)
    {
        return false;
    }
 #endif                                /*__SUPPORT_P2P__*/
    return true;
}

static inline bool is_sntp_configurable_mode (e_wifi_device_mode_ext_t mode, unsigned char iface)
{
    if (mode == WIFI_DEVICE_MODE_EXT_STATION)
    {
        return true;
    }

    if ((iface == WLAN0_IFACE) &&
        (mode == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
         || mode == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                /*__SUPPORT_P2P__*/
        ))
    {
        return true;
    }

    return false;
}

 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || defined __SUPPORT_MESH__
static int check_sae_groupid (int id)
{
    switch (id)                        /* Check Support Group ID */
    {
        case 19:
        case 20:
        case 21:
        {
            return 1;                  /* support */
        }

        default:

            return 0;
    }
}

 #endif                                /* __SUPPORT_WPA3_SAE__ || defined __SUPPORT_MESH__ */

static void setup_display_title (unsigned char sysmode, unsigned char cur_iface)
{
    /* Print Title */
    if ((cur_iface == WLAN0_IFACE) &&
        ((sysmode == WIFI_DEVICE_MODE_EXT_STATION) || (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)))
    {
        printf("\n" ANSI_COLOR_BLACK ANSI_BCOLOR_YELLOW
               "[ STATION CONFIGURATION ]" ANSI_BCOLOR_DEFULT ANSI_COLOR_DEFULT "\n");
    }
    else if ((sysmode == WIFI_DEVICE_MODE_EXT_AP) ||
             ((cur_iface == WLAN1_IFACE) && (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)))
    {
        printf("\n" ANSI_COLOR_BLACK ANSI_BCOLOR_YELLOW
               "[ SOFT-AP CONFIGURATION ]" ANSI_BCOLOR_DEFULT ANSI_COLOR_DEFULT "\n");
    }

 #if defined(__SUPPORT_P2P__)
    else if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) ||
             ((cur_iface == WLAN1_IFACE) && (sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION)))
    {
        printf("\n" ANSI_COLOR_BLACK ANSI_BCOLOR_YELLOW
               "[ WiFi Direct CONFIGURATION ]" ANSI_BCOLOR_DEFULT ANSI_COLOR_DEFULT "\n");
    }
 #endif                                /* __SUPPORT_P2P__ */
}

 #define DEFAULT_BSS_MAX_COUNT    80
static int parse_scan_list (struct setup_params * params, unsigned char e_cur_iface, bool * mode_changed)
{
    WIFIReturnCode_t         wifi_err;
    int                      idx;
    int                      sel_ssid;
    e_wifi_device_mode_ext_t prev_sysmode         = WIFI_DEVICE_MODE_EXT_NOT_SUPPORTED;
    WIFIScanExtendedConfig_t pxScanConfigExtended = {0};
    const int                scan_res_size        = (DEFAULT_BSS_MAX_COUNT * sizeof(WIFIScanResult_t));
    WIFIScanResult_t       * scan_result          = pvPortMalloc(scan_res_size);
    char                   * security_str;

    params->hidden_ssid = false;

    if (scan_result == NULL)
    {
        printf("[%s] Failed to allocate %d bytes for scan results\n", __func__, scan_res_size);

        return E_ERROR;
    }

SCAN_SSID:
    if (params->band == WPA_SETBAND_5G)
    {
        pxScanConfigExtended.ucBand = eWiFiBand5G;
    }
    else if (params->band == WPA_SETBAND_2G)
    {
        pxScanConfigExtended.ucBand = eWiFiBand2G;
    }
    else
    {
        pxScanConfigExtended.ucBand = eWiFiBandDual;
    }

    /* Set system mode so that WIFI_ScanExtended shows relevant results */
    wifi_err = WIFI_GetModeExt(&prev_sysmode);
    if (wifi_err)
    {
        printf("%s: WIFI_GetMode failed with wifi_err=%d\n", __func__, wifi_err);
    }

    if ((prev_sysmode != WIFI_DEVICE_MODE_EXT_STATION) &&
        (prev_sysmode != WIFI_DEVICE_MODE_EXT_AP_STATION) &&
        (prev_sysmode != WIFI_DEVICE_MODE_EXT_P2P_STATION))
    {
        WIFI_SetModeExt(params->sysmode);
        *mode_changed = true;
    }

    wifi_err = WIFI_SetCountryCode(params->country_code);
    if (wifi_err)
    {
        printf("%s: WIFI_SetCountryCode failed with wifi_err=%d\n", __func__, wifi_err);
    }

    wifi_err = WIFI_ScanExtended(scan_result, DEFAULT_BSS_MAX_COUNT, &pxScanConfigExtended);
    if (wifi_err != eWiFiSuccess)
    {
        printf("%s: WIFI_Scan failed with wifi_err=%d\n", __func__, wifi_err);
        vPortFree(scan_result);

        return E_ERROR;
    }

    if (scan_result[0].ucSSIDLength == 0)
    {
        printf(ANSI_COLOR_RED "No Scan Results" ANSI_COLOR_DEFULT "\n");
    }
    else
    {
        print_separate_bar('=', 60, 1);
        printf("[NO] %-29s %9s [CH] [SECURITY]\n", "[SSID]", "[SIGNAL]");
        print_separate_bar('-', 60, 1);
    }

    for (idx = 0; scan_result[idx].ucSSIDLength && idx < DEFAULT_BSS_MAX_COUNT; idx++)
    {
        if ((idx % 2) == 1)
        {
            printf(ANSI_COLOR_BLACK ANSI_BCOLOR_WHITE);
        }

        /* NO */
        printf("[%2d]", idx);

        /* SSID */
        if (scan_result[idx].ucSSID[0] == HIDDEN_SSID_DETECTION_CHAR)
        {
            printf(" %-32s", "[Hidden]");
        }
        else
        {
            printf(" %-32.*s", scan_result[idx].ucSSIDLength, scan_result[idx].ucSSID);
        }

        /* SIGNAL,  CH */
        printf(" %5d %3d ", scan_result[idx].cRSSI, scan_result[idx].ucChannel);

        /* Security */
        switch ((WIFISecurityExt_t) scan_result[idx].xSecurity)
        {
            case eWiFiSecurityOpen_ext:
            {
                security_str = "OPEN";
                break;
            }

            case eWiFiSecurityWEP_ext:
            {
                security_str = "WEP";
                break;
            }

            case eWiFiSecurityWPA_ext:
            {
                security_str = "WPA";
                break;
            }

            case eWiFiSecurityWPA2_ext:
            {
                security_str = "WPA2";
                break;
            }

            case eWiFiSecurityWPA2_ent_ext:
            {
                security_str = "WPA2 ENT";
                break;
            }

            case eWiFiSecurityWPA3_ext:
            {
                security_str = "WPA3";
                break;
            }

            case eWiFiSecurityWPA_ent_ext:
            {
                security_str = "WPA ENT";
                break;
            }

            case eWiFiSecurityWPA_WPA2_ent_ext:
            {
                security_str = "WPA + WPA2 ENT";
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ent_ext:
            {
                security_str = "WPA2 +WPA3 ENT";
                break;
            }

            case eWiFiSecurityWPA3_ent_ext:
            {
                security_str = "WPA3 ENT";
                break;
            }

            case eWiFiSecurityWPA3_192B_ent_ext:
            {
                security_str = "WPA3 192B ENT";
                break;
            }

            case eWiFiSecurityWPA_WPA2_ext:
            {
                security_str = "WPA + WPA2";
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ext:
            {
                security_str = "WPA2 + WPA3";
                break;
            }

            case eWiFiSecurityWPA3_OWE_ext:
            {
                security_str = "WPA3 OWE";
                break;
            }

            default:
            {
                /* Mark that its currently not supported */
                printf(ANSI_COLOR_LIGHT_RED);
                security_str = "UNKNOWN";
                break;
            }
        }

        printf("%12s", security_str);
        printf(ANSI_BCOLOR_DEFULT ANSI_COLOR_DEFULT "\n");
    }

    print_separate_bar('-', 60, 1);
    printf(ANSI_COLOR_CYAN "[Enter] Rescan" ANSI_COLOR_DEFULT "\n");
    print_separate_bar('=', 60, 1);

SELECT_SSID:
    if (scan_result[0].ucSSIDLength == 0)
    {
        printf("\n" ANSI_REVERSE " Select SSID ?" ANSI_NORMAL " (" ANSI_BOLD "M" ANSI_NORMAL "annual/"
               ANSI_BOLD "Q" ANSI_NORMAL "uit) : ");
    }
    else
    {
        printf("\n" ANSI_REVERSE " Select SSID ?" ANSI_NORMAL " (0~%d/" ANSI_BOLD "M" ANSI_NORMAL "annual/"
               ANSI_BOLD "Q" ANSI_NORMAL "uit) : ",
               idx - 1);
    }

    sel_ssid = getNum();

    if (sel_ssid == RET_QUIT)
    {
        vPortFree(scan_result);

        return E_QUIT;
    }
    else if (sel_ssid == RET_MANUAL)
    {
        vPortFree(scan_result);

        return E_INPUT_SSID;
    }
    else if (sel_ssid == RET_DEFAULT)
    {
        /* Rescan */
        goto SCAN_SSID;
    }
    else if ((sel_ssid < 0) || (sel_ssid >= idx))
    {
        goto SELECT_SSID;
    }

    if (scan_result[sel_ssid].ucSSID[0] == HIDDEN_SSID_DETECTION_CHAR)
    {
        if (E_QUIT == parse_ssid_input(params->ssid[e_cur_iface], false))
        {
            vPortFree(scan_result);

            return E_QUIT;
        }

        params->hidden_ssid = true;
    }
    else
    {
        params->ssid[e_cur_iface][0] = '\0';
        strncat(params->ssid[e_cur_iface], (char *) scan_result[sel_ssid].ucSSID, scan_result[sel_ssid].ucSSIDLength);
    }

    params->channel               = scan_result[sel_ssid].ucChannel;
    params->security[e_cur_iface] = (WIFISecurityExt_t) scan_result[sel_ssid].xSecurity;

    vPortFree(scan_result);

    return E_CONTINUE;
}

static unsigned char setup_stop_services (void)
{
    char reply[16] = {0};

    if (1 != get_yes_no("\n" ANSI_BOLD ANSI_COLOR_GREEN "Stop all services for the setting." ANSI_NORMAL
                        "\n" ANSI_REVERSE " Are you sure ?" ANSI_NORMAL " [" ANSI_BOLD "Y" ANSI_NORMAL "es/" ANSI_BOLD
                        "N" ANSI_NORMAL "o] : ",
                        false))
    {
        printf("\nSetting canceled.\n");

        return E_QUIT;
    }

    sntp_stop();

 #ifdef __SUPPORT_IPV4__
  #if LWIP_DHCP
    set_debug_dhcpc(0);                /* dhcp client debug level */
  #endif /* LWIP_DHCP */
 #endif /* __SUPPORT_IPV4__ */

    ra6w1_cli_reply("flush", NULL, reply);

    if (strcmp(reply, "OK"))
    {
        return E_ERROR;
    }

    ra6w1_cli_reply("set_log all 0", NULL, reply);

    if (strcmp(reply, "OK"))
    {
        return E_ERROR;
    }

    return E_CONTINUE;
}

/**
 * Set default values for advanced settings.
 *
 * @returns true if advanced setup might be needed (need to ask user)
 */
static bool set_advanced_defaults (struct setup_params * params, unsigned char cur_iface)
{
    WIFISecurityExt_t security = params->security[cur_iface];

    bool ask_advanced = (security == eWiFiSecurityWPA2_ext ||
                         security == eWiFiSecurityWPA_WPA2_ext ||
                         security == eWiFiSecurityWPA2_WPA3_ext ||
                         security == eWiFiSecurityWPA3_ext);

    params->pmf[cur_iface] = PMF_DEFAULT;

    params->ap_max_inactivity = AP_MAX_INACTIVITY;

    params->ap_wmm_enabled = -1;

    params->ap_wmm_ps_enabled = -1;

    if ((params->sysmode < WIFI_DEVICE_MODE_EXT_AP_STATION) && (cur_iface == WLAN1_IFACE))
    {
        ask_advanced = true;
        if (security == eWiFiSecurityWPA_ext)
        {
            if (params->channel >= 36)
            {
                params->wifi_mode = WIFI_MODE_A_ONLY + GAP_USER_CONFIGURE_WIFI_MODE;
            }
            else if ((params->channel == 0) && (params->band == WPA_SETBAND_5G))
            {
                params->wifi_mode = WIFI_MODE_A_ONLY + GAP_USER_CONFIGURE_WIFI_MODE;
            }
            else
            {
                params->wifi_mode = WIFI_MODE_BG + GAP_USER_CONFIGURE_WIFI_MODE;
            }
        }
        else if ((params->channel >= 36) ||
                 ((params->channel == 0) && (params->band == WPA_SETBAND_5G)))
        {
            params->wifi_mode = WIFI_MODE_AN + GAP_USER_CONFIGURE_WIFI_MODE;
        }
        else if ((params->channel <= 14) ||
                 ((params->channel == 0) && (params->band == WPA_SETBAND_2G)))
        {
            params->wifi_mode = WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE;
        }
    }

    return ask_advanced;
}

 #define COUNTRY_MAX            CC_NUM
 #define COUNTRY_LINE_MAX       20
 #define COUNTRY_LINE_CR_CNT    (COUNTRY_MAX / COUNTRY_LINE_MAX)

static unsigned char parse_country_code (char * p_country)
{
    char * buff_str       = NULL;
    char   tmp_country[8] = {0};

    int str_len = (4 * COUNTRY_MAX) + COUNTRY_LINE_CR_CNT + 1;

    buff_str = pvPortMalloc(str_len);

    if (buff_str == NULL)
    {
        printf("[%s] Failed to malloc (buff_str)\n", __func__);

        return E_QUIT;
    }

    memset(buff_str, 0, str_len);

    for (str_len = 0; (UINT) str_len < COUNTRY_MAX; str_len++)
    {
        sprintf(tmp_country, "%-3s ", ra6w1_regdb_get_cty_from_idx(str_len));
        strcat(buff_str, tmp_country);

        if ((str_len % COUNTRY_LINE_MAX == 19) && (str_len > 0))
        {
            strcat(buff_str, "\n");
        }
    }

    do
    {
        printf("\nCountry Code List:\n%s\n", buff_str);
        printf("\n" ANSI_REVERSE " COUNTRY CODE ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL
               "uit] (Default %s, ZZ : for debug) : ",
               COUNTRY_CODE_DEFAULT);

        str_len = getStr(p_country, 3); /* Max 3 */

        for (int i = 0; i < str_len; i++)
        {
            p_country[i] = (char) toupper(p_country[i]);
        }

        if (((str_len == 1) && (p_country[0] == 'Q')) || (str_len == RET_QUIT))
        {
            vPortFree(buff_str);

            return E_QUIT;
        }
        else if (!str_len)
        {
            bsp_safe_strcpy(p_country, COUNTRY_CODE_DEFAULT, sizeof(COUNTRY_CODE_DEFAULT));
            printf("Default Country: %s\n", p_country);
        }
        else
        {
            int ch = chk_channel_by_country(p_country, -1, 0, NULL, 0);

            if ((ch < CHANNEL_AUTO) && (ch != -2))
            {
                printf(ANSI_COLOR_LIGHT_RED "\nIncorrect Country Code!!\n" ANSI_NORMAL);
                continue;
            }
        }

        break;
    } while (1);

    vPortFree(buff_str);

    return E_CONTINUE;
}

 #if defined(__SUPPORT_SETBAND_5GHZ__)
static unsigned char parse_band (unsigned char e_sysmode, unsigned char * p_band)
{
    int input_num;

  #if defined(__SUPPORT_P2P__)
    if ((e_sysmode == WIFI_DEVICE_MODE_EXT_P2P) || (e_sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO))
    {
        return E_CONTINUE;
    }
  #endif                               /* __SUPPORT_P2P__ */

    /* Configuration Menu - START ************************************************/
    do
    {
        printf("\nBAND ?\n");
        printf("\t" ANSI_BOLD "1." ANSI_NORMAL " 2.4 GHz\n");
        printf("\t" ANSI_BOLD "2." ANSI_NORMAL " 5 GHz\n");
        if ((e_sysmode == WIFI_DEVICE_MODE_EXT_STATION) || (e_sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            printf("\t" ANSI_BOLD "3." ANSI_NORMAL " AUTO\n");
        }

        printf(ANSI_REVERSE " BAND ? " ANSI_NORMAL " [1/");
        if ((e_sysmode == WIFI_DEVICE_MODE_EXT_STATION) || (e_sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)) /* 2.4G/5G/AUTO */
        {
            printf("2/3/" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default 3. AUTO) : ");
        }
        else
        {
            if (e_sysmode == WIFI_DEVICE_MODE_EXT_AP) /* 2.4G/5G */
            {
                printf("2/");
            }

            /* 2.4G */
            printf(ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default 1. 2.4GHz) : ");
        }

        input_num = getNum();

        if (input_num == RET_DEFAULT)
        {
            /* default */
            if ((e_sysmode == WIFI_DEVICE_MODE_EXT_STATION) || (e_sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION))
            {
                *p_band = WPA_SETBAND_AUTO;
            }
            else if (e_sysmode == WIFI_DEVICE_MODE_EXT_AP)
            {
                *p_band = WPA_SETBAND_2G;
            }
            else
            {
                printf(" Define the default value for sysmode <%d> !!! \n", e_sysmode);
                *p_band = WPA_SETBAND_DEF;

                return E_QUIT;
            }

            break;
        }
        else if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
        else
        {
            switch ((unsigned short) input_num)
            {
                case 1:
                {
                    *p_band = WPA_SETBAND_2G;
                    break;
                }

                case 2:
                {
                    *p_band = WPA_SETBAND_5G;
                    break;
                }

                case 3:
                {
                    *p_band = WPA_SETBAND_AUTO;
                    break;
                }
            }
        }
    } while (((e_sysmode == WIFI_DEVICE_MODE_EXT_STATION || e_sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION) &&
              (input_num < 1 || input_num > 3)) ||
             (e_sysmode == WIFI_DEVICE_MODE_EXT_AP && (input_num != 1 && input_num != 2)));

    /* Configuration Menu - END  **************************************************/
    return E_CONTINUE;
}

 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

static unsigned char parse_sysmode (unsigned char * p_sysmode)
{
    int input_num;

    /* Configuration Menu - START ************************************************/
    do
    {
        printf("\nSYSMODE(WLAN MODE) ?\n");
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " Station\n", WIFI_DEVICE_MODE_EXT_STATION + GAP_USER_CONFIGURE_MODE);
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " Soft-AP\n", WIFI_DEVICE_MODE_EXT_AP + GAP_USER_CONFIGURE_MODE);
 #if defined(__SUPPORT_P2P__)
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " WiFi Direct\n", WIFI_DEVICE_MODE_EXT_P2P + GAP_USER_CONFIGURE_MODE);
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " WiFi Direct P2P GO Fixed\n",
               WIFI_DEVICE_MODE_EXT_P2P_GO + GAP_USER_CONFIGURE_MODE);
 #endif                                /* __SUPPORT_P2P__ */
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " Station & Soft-AP\n",
               WIFI_DEVICE_MODE_EXT_AP_STATION + GAP_USER_CONFIGURE_MODE);
  #if defined(__SUPPORT_P2P__)
   #ifdef CONFIG_P2P_CONCURRENT
        printf("\t" ANSI_BOLD "%d." ANSI_NORMAL " Station & WiFi Direct\n",
               WIFI_DEVICE_MODE_EXT_P2P_STATION + GAP_USER_CONFIGURE_MODE);
   #endif                              /* CONFIG_P2P_CONCURRENT */
  #endif                               /* __SUPPORT_P2P__ */
 #endif                                /* __SUPPORT_WIFI_CONCURRENT__ */

        printf(ANSI_REVERSE " MODE ? " ANSI_NORMAL " [1/2/");

 #if defined(__SUPPORT_P2P__)
        printf("%d/", WIFI_DEVICE_MODE_EXT_P2P + GAP_USER_CONFIGURE_MODE);         /* WiFi Direct */
        printf("%d/", WIFI_DEVICE_MODE_EXT_P2P_GO + GAP_USER_CONFIGURE_MODE);      /* WiFi Direct P2P GO Fixed */
 #endif /* __SUPPORT_P2P__ */
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        printf("%d/", WIFI_DEVICE_MODE_EXT_AP_STATION + GAP_USER_CONFIGURE_MODE);  /* Station & SOFT-AP */
  #if defined(__SUPPORT_P2P__)
   #ifdef CONFIG_P2P_CONCURRENT
        printf("%d/", WIFI_DEVICE_MODE_EXT_P2P_STATION + GAP_USER_CONFIGURE_MODE); /* Station & WiFi Direct */
   #endif /* CONFIG_P2P_CONCURRENT */
  #endif /* __SUPPORT_P2P__ */
 #endif /* __SUPPORT_WIFI_CONCURRENT__ */

        printf(ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default Station) : ");

        input_num = getNum();

        *p_sysmode = (unsigned char) input_num;
        *p_sysmode = *p_sysmode - GAP_USER_CONFIGURE_MODE; /* User give value GAP_USER_CONFIGURE_MODE + wanted_ sys_mode */

        if (input_num == RET_DEFAULT)
        {
            /* default mode */
            *p_sysmode = WIFI_DEVICE_MODE_EXT_STATION;

            return E_CONTINUE;
        }
        else if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
    } while ((input_num - GAP_USER_CONFIGURE_MODE) < WIFI_DEVICE_MODE_EXT_STATION
 #if defined(__SUPPORT_MESH_PORTAL__)
             || (input_num - GAP_USER_CONFIGURE_MODE) > WIFI_DEVICE_MODE_EXT_MESH_PORTAL
 #elif defined(__SUPPORT_MESH_POINT_ONLY__)
             || (input_num - GAP_USER_CONFIGURE_MODE) > WIFI_DEVICE_MODE_EXT_MESH_POINT
 #elif defined(__SUPPORT_WIFI_CONCURRENT__)
             || (input_num - GAP_USER_CONFIGURE_MODE) > WIFI_DEVICE_MODE_EXT_AP_STATION
 #else
  #if defined(__SUPPORT_P2P__)
             || (input_num - GAP_USER_CONFIGURE_MODE) > WIFI_DEVICE_MODE_EXT_P2P_GO
  #else
             || (input_num - GAP_USER_CONFIGURE_MODE) > WIFI_DEVICE_MODE_EXT_AP
  #endif                               /* __SUPPORT_P2P__ */
 #endif                                /* __SUPPORT_MESH_PORTAL__ */
             );

    /* Configuration Menu - END  **************************************************/
    return E_CONTINUE;
}

static void setup_select_interface (unsigned char sysmode, unsigned char * p_iface_start, unsigned char * p_iface_end)
{
    if (sysmode == WIFI_DEVICE_MODE_EXT_AP
 #if defined(__SUPPORT_P2P__)
        || sysmode == WIFI_DEVICE_MODE_EXT_P2P ||
        sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO
 #endif                                /* __SUPPORT_P2P__ */
        )
    {
        /* AP only , P2P Device only , P2P GO only, MESH Point Only */
        *p_iface_start = WLAN1_IFACE;
        *p_iface_end   = WLAN1_IFACE;
    }
    else if (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
             || sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                /* __SUPPORT_P2P__ */
             )
    {
        /* Station & AP, Station & P2P Device, Station & MESH Portal*/
        *p_iface_start = WLAN0_IFACE;
        *p_iface_end   = WLAN1_IFACE;
    }
    else
    {
        /* Station only */
        *p_iface_start = WLAN0_IFACE;
        *p_iface_end   = WLAN0_IFACE;
    }
}

static unsigned char parse_ssid_input (char * p_ssid, bool gen_default)
{
    char ssid_str[wificonfigMAX_SSID_LEN + 1] = {0};
    int  len;

    if (gen_default)
    {
        if (gen_ssid(CHIPSET_NAME, WLAN1_IFACE, 0, ssid_str, sizeof(ssid_str)) == -1)
        {
            printf("SSID Error\n");
        }
    }

PROMPT_INPUT_SSID:
    do
    {
        if (gen_default)
        {
            /* Soft AP */
            printf("\n" ANSI_REVERSE " SSID(NETWORK NAME) ? (Default %s)" ANSI_NORMAL " : ", ssid_str);
        }
        else
        {
            /* Station */
            printf("\n" ANSI_REVERSE " SSID ?" ANSI_NORMAL " : ");
        }

        len = getStr(p_ssid, wificonfigMAX_SSID_LEN);

        if (len == 0)
        {
            if (gen_default && (strlen(ssid_str) > 0))
            {
                bsp_safe_strcpy(p_ssid, ssid_str, wificonfigMAX_SSID_LEN + 1);
                break;
            }
            else
            {
                continue;
            }
        }
        else if (len == RET_QUIT)
        {
            return E_QUIT;
        }

        /* Verify SSID string is valid */
        for (int i = 0; i < len; i++)
        {
            if (iscntrl((int) p_ssid[i]))
            {
                goto PROMPT_INPUT_SSID;
            }
        }

        break;
    } while (1);

    return E_CONTINUE;
}

static unsigned char parse_is_hidden_ssid (bool * is_hidden_ssid, bool station)
{
    if (station)
    {
        int ret = get_yes_no("\n" ANSI_REVERSE " Connect to hidden SSID ?" ANSI_NORMAL " [" ANSI_BOLD "Y"
                             ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                             false);
        if (E_QUIT == ret)
        {
            return E_QUIT;
        }

        *is_hidden_ssid = ret;
    }

    return 0;
}

static unsigned char parse_channel_input (unsigned char   sysmode,
                                          unsigned char   cur_iface,
                                          char          * p_country,
                                          unsigned char   band,
                                          unsigned char * p_channel)
{
    int          firstCH, finshCH;
    unsigned int ch_bitmap_5g;
    char       * ch_str_buf_5g = NULL;
    int          input_num;

    /* AP Country Code && Channel */
    if (sysmode == WIFI_DEVICE_MODE_EXT_AP)
    {
        /* 1: SoftAP Only (wlan0)  */

        if (band == WPA_SETBAND_2G)
        {
            ra6w1_regdb_get_ch_range_by_country_n_band(p_country, 2, &firstCH, &finshCH, NULL, 0);
        }
        else if (band == WPA_SETBAND_5G)
        {
 #if defined(__SUPPORT_DFS_CH_IN_SOFTAP__)
            ra6w1_regdb_get_ch_range_by_country_n_band(p_country, 5, NULL, NULL, &ch_bitmap_5g,
                                                       RRQ61X_REG_FLAG_NO_IR /* include all but NO_IR */);
 #else
            ra6w1_regdb_get_ch_range_by_country_n_band(p_country,
                                                       5,
                                                       NULL,
                                                       NULL,
                                                       &ch_bitmap_5g,
                                                       (RRQ61X_REG_FLAG_NO_IR |
                                                        RRQ61X_REG_FLAG_DFS) /* exclude DFS|NO_IR channel */);
 #endif                                        /* __SUPPORT_DFS_CH_IN_SOFTAP__ */

            ch_str_buf_5g = pvPortMalloc(104); /* can include all 28 5GHz channels including NULL */
            if (ch_str_buf_5g == NULL)
            {
                printf("[%s] Failed to malloc (ch_str_buf_5g) \n", __func__);

                return E_QUIT;
            }

            memset(ch_str_buf_5g, 0x00, 104);
            ra6w1_regdb_gen_5g_ch_range_string(ch_str_buf_5g, ch_bitmap_5g, ',');
        }

        /* INPUT_AP_CHANNEL */
INPUT_AP_CHANNEL:

        do
        {
            if (band == WPA_SETBAND_2G)
            {
                printf(
                    "\n" ANSI_REVERSE " CHANNEL ?" ANSI_NORMAL " [%d~%d, %s/" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default %s) : ",
                    firstCH,
                    finshCH,
                    "Auto:0",
                    "Auto");
            }
            else if (band == WPA_SETBAND_5G)
            {
                printf(
                    "\n" ANSI_REVERSE " CHANNEL ?" ANSI_NORMAL " %s, %s/" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default %s) : ",
                    ch_str_buf_5g,
                    "Auto:0",
                    "Auto");
            }

            input_num  = getNum();
            *p_channel = (unsigned char) input_num;

            if (input_num == RET_DEFAULT)
            {
                *p_channel = CHANNEL_DEFAULT;
            }
            else if (input_num == RET_QUIT)
            {
                if (band == WPA_SETBAND_5G)
                {
                    vPortFree(ch_str_buf_5g);
                }

                return E_QUIT;
            }
            else if ((input_num < 0) && (input_num != RET_QUIT))
            {
                continue;
            }

            if (input_num < CHANNEL_AUTO)
            {
                *p_channel = CHANNEL_DEFAULT;
            }
            else if (input_num == CHANNEL_AUTO)
            {
                *p_channel = CHANNEL_AUTO;
            }
            else
            {
                if (band == WPA_SETBAND_5G)
                {
                    int ii, found = 0;

                    for (ii = 0; ii < MAX_CH_NUM_5G; ii++)
                    {
                        if (ch_bitmap_5g & (1 << ii))
                        {
                            if (ra6w1_regdb_get_5g_ch(ii) == input_num)
                            {
                                /* The input channel is supported */
                                *p_channel = (unsigned char) input_num;
                                found      = 1;
                            }
                        }
                    }

                    if (!found)
                    {
                        goto INPUT_AP_CHANNEL;
                    }
                }
                else
                {
                    if ((input_num >= firstCH) && (input_num <= finshCH))
                    {
                        *p_channel = (unsigned char) input_num;
                    }
                    else if ((input_num == 0) || (input_num == RET_DEFAULT))
                    {
                        *p_channel = CHANNEL_DEFAULT;
                    }
                    else
                    {
                        goto INPUT_AP_CHANNEL;
                    }
                }
            }

            if (band == WPA_SETBAND_5G)
            {
                vPortFree(ch_str_buf_5g);
            }

            break;
        } while (1);
    }

    return E_CONTINUE;
}

static unsigned char convert_sec_input_to_security_ext (unsigned char cur_iface, int input)
{
    switch (input)
    {
        case 1:
        {
            return eWiFiSecurityOpen_ext;
        }

        case 2:
        {
            if (cur_iface == WLAN0_IFACE)
            {
                return eWiFiSecurityWEP_ext;
            }

            break;
        }

        case 3:
        {
            return eWiFiSecurityWPA_ext;
        }

        case 4:
        {
            return eWiFiSecurityWPA2_ext;
        }

        case 5:
        {
            return eWiFiSecurityWPA_WPA2_ext;
        }

        case 6:
        {
            return eWiFiSecurityWPA3_ext;
        }

        case 7:
        {
            return eWiFiSecurityWPA2_WPA3_ext;
        }

        case 8:
        {
            return eWiFiSecurityWPA3_OWE_ext;
        }

        case 9:
        {
            if (cur_iface == WLAN0_IFACE)
            {
                return eWiFiSecurityWPA_WPA2_ent_ext;
            }

            break;
        }

        case 10:
        {
            if (cur_iface == WLAN0_IFACE)
            {
                return eWiFiSecurityWPA2_WPA3_ent_ext;
            }

            break;
        }

        case 11:
        {
            if (cur_iface == WLAN0_IFACE)
            {
                return eWiFiSecurityWPA3_ent_ext;
            }

            break;
        }

        case 12:
        {
            if (cur_iface == WLAN0_IFACE)
            {
                return eWiFiSecurityWPA3_192B_ent_ext;
            }

            break;
        }

        default:

            return eWiFiSecurityMax_ext;
    }

    return eWiFiSecurityMax_ext;
}

static unsigned char parse_security_input (unsigned char cur_iface, unsigned char * p_security)
{
    int input_num;

    *p_security = eWiFiSecurityMax_ext;

    /* INPUT AUTH */
    do
    {
        printf("\n\nAUTHENTICATION ?\n"
               "\t 1. OPEN\n");
        if (cur_iface == WLAN0_IFACE)
        {
            printf("\t 2. WEP\n");
        }
        else
        {
            printf("\t 2. WEP(Unsupported)\n");
        }

        printf("\t 3. WPA-PSK\n");
        printf("\t 4. WPA2-PSK\n");
        printf("\t 5. WPA/WPA2-PSK\n");
        printf("\t 6. WPA3-SAE (WPA3-Personal)\n");
        printf("\t 7. WPA2-PSK+WPA3-SAE (Recommended)\n");
        printf("\t 8. WPA3-OWE\n");
        if (cur_iface == WLAN0_IFACE)
        {
            printf("\t 9. WPA/WPA2-Enterprise\n");
            printf("\t10. WPA2/WPA3-Enterprise (128bits)\n");
            printf("\t11. WPA3-Enterprise (128bits)\n");
            printf("\t12. WPA3-Enterprise (192Bits)\n");
        }

        if (cur_iface == WLAN0_IFACE)
        {
            printf(" AUTHENTICATION ? [1/2/3/4/5/6/7/9/10/11/12/"ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");
        }
        else
        {
            printf(" AUTHENTICATION ? [1/3/4/5/6/7/8/"ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");
        }

        input_num = getNum();

        if (input_num == RET_DEFAULT)
        {
            *p_security = eWiFiSecurityOpen_ext;
        }
        else if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }

        *p_security = convert_sec_input_to_security_ext(cur_iface, input_num);
    } while (*p_security == eWiFiSecurityMax_ext);

    printf("\n");

    return E_CONTINUE;
}

static unsigned char parse_psk_input (WIFISecurityExt_t security, char * p_psk_key)
{
    int getStr_len = 0;

    if ((security == eWiFiSecurityWEP_ext) ||
        (security == eWiFiSecurityWPA_ent_ext) ||
        (security == eWiFiSecurityWPA2_ent_ext) ||
        (security == eWiFiSecurityWPA_WPA2_ent_ext) ||
        (security == eWiFiSecurityWPA2_WPA3_ent_ext) ||
        (security == eWiFiSecurityWPA3_ent_ext) ||
        (security == eWiFiSecurityWPA3_192B_ent_ext) ||
        (security == eWiFiSecurityOpen_ext) ||
        (security == eWiFiSecurityWPA3_OWE_ext))
    {
        return E_CONTINUE;
    }

    do
    {
        if (security == eWiFiSecurityWPA3_ext)
        {
            printf("\n" ANSI_REVERSE " SAE-PASSWORD(ASCII characters 1~128) ?"
                   ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit]\n");
        }
        else if (security == eWiFiSecurityWPA2_WPA3_ext)
        {
            printf("\n" ANSI_REVERSE " PSK-KEY(ASCII characters 8~63) ?"
                   ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit]\n");
        }
        else
        {
            printf("\n" ANSI_REVERSE " PSK-KEY(ASCII characters 8~63 or Hexadecimal characters 64) ?"
                   ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit]\n");
        }

        getStr_len = getStr(p_psk_key, wificonfigMAX_PASSPHRASE_LEN);

        if (((getStr_len == 1) && (toupper(p_psk_key[0]) == 'Q')) || (getStr_len == RET_QUIT))
        {
            return E_QUIT;
        }

        /* CHECK LENGTH */
        if ((
 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || defined __SUPPORT_MESH__
                security != eWiFiSecurityWPA3_ext &&
 #endif                                // __SUPPORT_WPA3_SAE__ || __SUPPORT_MESH__
                getStr_len < 8)
 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || defined __SUPPORT_MESH__
            || (security == eWiFiSecurityWPA3_ext && getStr_len < 1)
 #endif                                // __SUPPORT_WPA3_SAE__ || __SUPPORT_MESH__
            )
        {
            continue;
        }

        break;
    } while (1);

    return E_CONTINUE;
}

static unsigned char parse_wep_key_input (WIFISecurityExt_t security,
                                          unsigned char     cur_iface,
                                          char            * p_wep_key,
                                          uint8_t         * p_wep_key_idx,
                                          unsigned char   * p_wep_key_type,
                                          unsigned char   * p_wep_bit)
{
    int input_num = 0;

    if ((security != eWiFiSecurityWEP_ext) || (cur_iface != WLAN0_IFACE))
    {
        return E_CONTINUE;
    }

    /* WEP-KEY */
    do
    {
        printf("\n" ANSI_REVERSE
               " WEP KEY (ASCII characters 5 or 13, Hexadecimal characters 10 or 26) ?" ANSI_NORMAL " ["
               ANSI_BOLD "Q" ANSI_NORMAL "uit]\n");

        printf("[1234" ANSI_BOLD "5" ANSI_NORMAL "6789" ANSI_BOLD "|" ANSI_NORMAL "12" ANSI_BOLD
               "3" ANSI_NORMAL "456789|12345" ANSI_BOLD "6" ANSI_NORMAL "]\n:");

        input_num = getStr(p_wep_key, wificonfigMAX_WEPKEY_LEN);

        if ((input_num == 5) || (input_num == 13))       /* ASCII */
        {
            *p_wep_key_type = WEP_KEY_TYPE_ASCII;
        }
        else if ((input_num == 10) || (input_num == 26)) /* HEXA */
        {
            *p_wep_key_type = WEP_KEY_TYPE_HEXA;
        }
        else if (input_num > 1)
        {
            continue;
        }

        if ((input_num == 5) || (input_num == 10))       /* 64bit */
        {
            *p_wep_bit = WEP_KEY_64BIT;
        }
        else if ((input_num == 13) || (input_num == 26)) /* 128bit */
        {
            *p_wep_bit = WEP_KEY_128BIT;
        }

        if (((input_num == 1) && (toupper(p_wep_key[0]) == 'Q')) || (input_num == RET_QUIT))
        {
            return E_QUIT;
        }

        /* Check Hexa */
        if (*p_wep_key_type == WEP_KEY_TYPE_HEXA)
        {
            int keyindex = 0;

            for (keyindex = 0; keyindex < input_num; keyindex++)
            {
                if (isxdigit((int) (p_wep_key[keyindex])) == 0)
                {
                    break;
                }
            }

            if (keyindex < input_num)
            {
                continue;
            }
        }

        break;
    } while (1);

    /* KEY INDEX */
    do
    {
        printf("\nWEP KEY INDEX ?\n" \
               ANSI_REVERSE " INDEX ?" ANSI_NORMAL " [%d/%d/%d/%d/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
               E_WEP_KEY_IDX_1,
               E_WEP_KEY_IDX_2,
               E_WEP_KEY_IDX_3,
               E_WEP_KEY_IDX_4);

        input_num      = getNum();
        *p_wep_key_idx = (unsigned char) input_num;

        if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
    } while (input_num < E_WEP_KEY_IDX_1 || input_num > E_WEP_KEY_IDX_4);

    return E_CONTINUE;
}

static unsigned char parse_eap_input (WIFISecurityExt_t security,
                                      unsigned char     cur_iface,
                                      unsigned char   * p_eap_auth_mode,
                                      unsigned char   * p_eap_phase2)
{
    int input_num;

    if (((security != eWiFiSecurityWPA2_ent_ext) && (security != eWiFiSecurityWPA_ent_ext) &&
         (security != eWiFiSecurityWPA_WPA2_ent_ext) && (security != eWiFiSecurityWPA2_WPA3_ent_ext) &&
         (security != eWiFiSecurityWPA3_ent_ext) && (security != eWiFiSecurityWPA3_192B_ent_ext)) ||
        (cur_iface != WLAN0_IFACE))
    {
        return E_CONTINUE;
    }

    printf("\n<WPA-Enterprise Security>\n");

    /* INPUT AUTH */
    do
    {
        printf("\nAuthentication Type(802.1x) ?\n");
        printf("\t%2d. PEAP or TTLS or FAST (Recommend)\n", E_EAP_AUTH_MODE_PEAP_TTLS_FAST);
        printf("\t%2d. PEAP\n", E_EAP_AUTH_MODE_PEAP);
        printf("\t%2d. TTLS\n", E_EAP_AUTH_MODE_TTLS);
        printf("\t%2d. FAST\n", E_EAP_AUTH_MODE_FAST);
        printf("\t%2d. TLS \n", E_EAP_AUTH_MODE_TLS);

        printf(ANSI_REVERSE " Type ?" ANSI_NORMAL " [%d/", E_EAP_AUTH_MODE_PEAP_TTLS_FAST);
        printf("%d/", E_EAP_AUTH_MODE_PEAP);
        printf("%d/", E_EAP_AUTH_MODE_TTLS);
        printf("%d/", E_EAP_AUTH_MODE_FAST);
        printf("%d/", E_EAP_AUTH_MODE_TLS);
        printf(ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");

        *p_eap_auth_mode = input_num = getNum();

        if (input_num == RET_DEFAULT)  /* default  */
        {
            *p_eap_auth_mode = E_EAP_AUTH_MODE_PEAP_TTLS_FAST;
            break;
        }
        else if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
    } while (input_num <= E_EAP_AUTH_MODE_NONE || input_num >= E_EAP_AUTH_MODE_MAX);

    if (E_EAP_AUTH_MODE_FAST > input_num)
    {
        /* INPUT Phase2 */
        do
        {
            printf("\n\nAuthentication Protocols (802.1x) ?\n");
            printf("\t%2d. MSCHAPv2 or GTC (Recommend)\n", E_EAP_PHASE2_MODE_MSCHAPV2_N_GTC);
            printf("\t%2d. MSCHAPv2\n", E_EAP_PHASE2_MODE_MSCHAPV2);
            printf("\t%2d. GTC\n", E_EAP_PHASE2_MODE_GTC);
            printf("\t%2d. TLS\n", E_EAP_PHASE2_MODE_TLS);

            printf(ANSI_REVERSE " Protocol ?" ANSI_NORMAL " [%d/", E_EAP_PHASE2_MODE_MSCHAPV2_N_GTC);
            printf("%d/", E_EAP_PHASE2_MODE_MSCHAPV2);
            printf("%d/", E_EAP_PHASE2_MODE_GTC);
            printf("%d/", E_EAP_PHASE2_MODE_TLS);

            printf(ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");

            *p_eap_phase2 = input_num = getNum();

            if (input_num == RET_DEFAULT) /* default */
            {
                *p_eap_phase2 = E_EAP_PHASE2_MODE_MSCHAPV2_N_GTC;
                break;
            }
            else if (input_num == RET_QUIT)
            {
                return E_QUIT;
            }
        } while (input_num < E_EAP_PHASE2_MODE_MSCHAPV2_N_GTC || input_num >= E_EAP_PHASE2_MODE_MAX);
    }

    printf("\n");

    return E_CONTINUE;
}

static unsigned char parse_enterprise_id_cert (WIFISecurityExt_t security,
                                               unsigned char     cur_iface,
                                               unsigned char     eap_auth_mode,
                                               unsigned char     eap_phase2,
                                               char            * p_eap_id,
                                               char            * p_eap_pw)
{
    int getStr_len = 0;

    if (((security != eWiFiSecurityWPA2_ent_ext) && (security != eWiFiSecurityWPA_ent_ext) &&
         (security != eWiFiSecurityWPA_WPA2_ent_ext) && (security != eWiFiSecurityWPA2_WPA3_ent_ext) &&
         (security != eWiFiSecurityWPA3_ent_ext) && (security != eWiFiSecurityWPA3_192B_ent_ext)) ||
        (cur_iface != WLAN0_IFACE))
    {
        return E_CONTINUE;
    }

    /* Enterprise ID */
    do
    {
        printf("\n" ANSI_REVERSE " ID ?" ANSI_NORMAL " [" ANSI_BOLD "Q"
               ANSI_NORMAL "uit] : ");

        getStr_len = getStr(p_eap_id, wificonfigMAX_ENT_IDENTITY_LEN);

        if (((getStr_len == 1) && (toupper(p_eap_id[0]) == 'Q')) || (getStr_len == RET_QUIT))
        {
            return E_QUIT;
        }

        if (getStr_len == 1)
        {
            p_eap_id[0] = toupper(p_eap_id[0]);
        }

        if (getStr_len > 0)
        {
            break;
        }
    } while (1);

    if ((eap_auth_mode == E_EAP_AUTH_MODE_PEAP_TTLS_FAST) ||
        ((eap_auth_mode == E_EAP_AUTH_MODE_PEAP) && (eap_phase2 != E_EAP_PHASE2_MODE_TLS)) ||
        ((eap_auth_mode == E_EAP_AUTH_MODE_TTLS) && (eap_phase2 != E_EAP_PHASE2_MODE_TLS)) ||
        (eap_auth_mode == E_EAP_AUTH_MODE_FAST))
    {
        /* Enterprise Password */
        do
        {
            printf("\n" ANSI_REVERSE " Password ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");

            getStr_len = getStr(p_eap_pw, wificonfigMAX_ENT_PASSWORD_LEN);

            if (((getStr_len == 1) && (toupper(p_eap_pw[0]) == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }

            if (getStr_len == 1)
            {
                p_eap_pw[0] = toupper(p_eap_pw[0]);
            }

            if (getStr_len > 0)
            {
                break;
            }
        } while (1);

        // Delete Enterprise Certificate
        cert_rwds(ACT_DELETE, CA_CERT3);
        cert_rwds(ACT_DELETE, CLIENT_CERT3);
        cert_rwds(ACT_DELETE, CLIENT_KEY3);
    }
    else if ((eap_auth_mode == E_EAP_AUTH_MODE_TLS) ||
             (((eap_auth_mode == E_EAP_AUTH_MODE_PEAP) || (eap_auth_mode == E_EAP_AUTH_MODE_TTLS)) &&
              (eap_phase2 == E_EAP_PHASE2_MODE_TLS)))
    {
        printf(ANSI_REVERSE "\n<Requires a certificate when using EAP-TLS>\n" ANSI_NORMAL);
        do
        {
            int ret = get_yes_no("\n" ANSI_REVERSE "Do you want to save the certificate ?" ANSI_NORMAL
                                 " [" ANSI_BOLD "Y" ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q"
                                 ANSI_NORMAL "uit] (Default Yes) : ",
                                 true);
            if (E_QUIT == ret)
            {
                return E_QUIT;
            }
            else if (1 == ret)
            {
                int status;
                vTaskSuspend(cli_handle_task);
                printf(ANSI_REVERSE "\n\n<Certificate(PEM format)>\n\n" ANSI_NORMAL);
                status = cert_rwds(ACT_WRITE, CLIENT_CERT3);
                printf("\nCert Write %s.\n", (status == pdPASS) ? "success" : "failed");
                if (status == pdFAIL)
                {
                    continue;
                }

                printf(ANSI_REVERSE "\n\n<Private Key>\n\n" ANSI_NORMAL);
                status = cert_rwds(ACT_WRITE, CLIENT_KEY3);
                printf("\nKey Write %s.\n", (status == pdPASS) ? "success" : "failed");
                if (status == pdFAIL)
                {
                    continue;
                }
            }

            break;
        } while (1);

        printf("\n\n");
    }

    return E_CONTINUE;
}

 #if CFG_PMGR
static unsigned char parse_pmgr_input (unsigned char   sysmode,
                                       unsigned char   cur_iface,
                                       unsigned char * p_enable_dpm,
                                       int           * p_dpm_keep_alive_time,
                                       int           * p_dpm_user_wakeup_time,
                                       int           * p_dpm_TIM_wakeup_count)
{
    /* DPM [enable/disable] */
    if ((cur_iface == WLAN0_IFACE) && (sysmode == WIFI_DEVICE_MODE_EXT_STATION))
    {
INPUT_PMGR_MODE:

        switch (get_yes_no(ANSI_REVERSE " Enable DPM (Dynamic Power Management) ?" ANSI_NORMAL " [" ANSI_BOLD
                           "Y" ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "M" ANSI_NORMAL "annual/"
                           ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                           true))
        {
            case 2:
            {
                *p_enable_dpm = 2;
                break;
            }

            case 1:
            {
                *p_enable_dpm = 1;
                break;
            }

            case 0:
            {
                *p_enable_dpm = 0;
                break;
            }

            case E_QUIT:
            {
                return E_QUIT;
            }

            case E_CONTINUE:
            {
                *p_enable_dpm = true;
                printf("Default enable DPM\n");
                break;
            }
        }
    }
    else
    {
        return E_CONTINUE;
    }

    *p_dpm_keep_alive_time  = DFLT_DPM_KEEPALIVE_TIME;
    *p_dpm_user_wakeup_time = DFLT_DPM_USER_WAKEUP_TIME;
    *p_dpm_TIM_wakeup_count = DFLT_DPM_TIM_WAKEUP_COUNT;

    if (*p_enable_dpm)
    {
        /* Detail Config */
        switch (get_yes_no("\n" ANSI_REVERSE " DPM factors : Defaults ?" ANSI_NORMAL " [" ANSI_BOLD "Y"
                           ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                           false))
        {
            case 1:
            {
                goto DEFULT_PMGR_VALUE;
            }

            case E_QUIT:

                return E_QUIT;
        }

        /* DPM Keep Alive Time */
        do
        {
            printf("\n" ANSI_REVERSE " DPM Keep Alive Time(%d~%d ms) ?" ANSI_NORMAL " [" ANSI_BOLD "Q"
                   ANSI_NORMAL "uit] (Default %d ms) : ",
                   MIN_DPM_KEEPALIVE_TIME,
                   MAX_DPM_KEEPALIVE_TIME,
                   DFLT_DPM_KEEPALIVE_TIME);

            *p_dpm_keep_alive_time = getNum();

            if (*p_dpm_keep_alive_time == RET_QUIT)
            {
                return E_QUIT;
            }
            else if (*p_dpm_keep_alive_time == RET_DEFAULT)
            {
                *p_dpm_keep_alive_time = DFLT_DPM_KEEPALIVE_TIME;
            }
        } while (*p_dpm_keep_alive_time < MIN_DPM_KEEPALIVE_TIME ||
                 *p_dpm_keep_alive_time > MAX_DPM_KEEPALIVE_TIME);

        /* DPM User Wakeup Time */
        do
        {
            printf("\n" ANSI_REVERSE " DPM User Wakeup Time(%d~%d ms) ?" ANSI_NORMAL " [" ANSI_BOLD
                   "Q" ANSI_NORMAL "uit] (Default %d ms) : ",
                   MIN_DPM_USER_WAKEUP_TIME,
                   MAX_DPM_USER_WAKEUP_TIME,
                   DFLT_DPM_USER_WAKEUP_TIME);

            *p_dpm_user_wakeup_time = getNum();

            if (*p_dpm_user_wakeup_time == RET_QUIT)
            {
                return E_QUIT;
            }
            else if (*p_dpm_user_wakeup_time == RET_DEFAULT)
            {
                *p_dpm_user_wakeup_time = DFLT_DPM_USER_WAKEUP_TIME;
            }
        } while ((*p_dpm_user_wakeup_time < MIN_DPM_USER_WAKEUP_TIME ||
                  *p_dpm_user_wakeup_time > MAX_DPM_USER_WAKEUP_TIME) &&
                 *p_dpm_user_wakeup_time != 0);

        /* DPM TIM Wakeup Time */
        do
        {
            printf("\n" ANSI_REVERSE " DPM TIM Wakeup Count(%d~%d dtim) ?" ANSI_NORMAL " [" ANSI_BOLD
                   "Q" ANSI_NORMAL "uit] (Default %d) : ",
                   MIN_DPM_TIM_WAKEUP_COUNT,
                   MAX_DPM_TIM_WAKEUP_COUNT,
                   DFLT_DPM_TIM_WAKEUP_COUNT);

            *p_dpm_TIM_wakeup_count = getNum();

            if (*p_dpm_TIM_wakeup_count == RET_QUIT)
            {
                return E_QUIT;
            }
            else if (*p_dpm_TIM_wakeup_count == RET_DEFAULT)
            {
                *p_dpm_TIM_wakeup_count = DFLT_DPM_TIM_WAKEUP_COUNT;
            }
        } while (*p_dpm_TIM_wakeup_count < MIN_DPM_TIM_WAKEUP_COUNT ||
                 *p_dpm_TIM_wakeup_count > (MAX_DPM_TIM_WAKEUP_COUNT)
                 );

DEFULT_PMGR_VALUE:

        VT_COLORGREEN;

        /*============================================\n*/
        print_separate_bar('=', 44, 1);
        printf("DPM MODE           : Enable\n");
        printf("Keep Alive Time    : %d ms\n", *p_dpm_keep_alive_time);
        printf("User Wakeup Time   : %d ms\n", *p_dpm_user_wakeup_time);
        printf("TIM Wakeup Count   : %d dtim\n", *p_dpm_TIM_wakeup_count);

        /*============================================\n*/
        print_separate_bar('=', 44, 1);
        VT_NORMAL;

        switch (get_yes_no(ANSI_REVERSE " DPM CONFIGURATION CONFIRM ?" ANSI_NORMAL " [" ANSI_BOLD "Y"
                           ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                           false))
        {
            case 0:
            {
                goto INPUT_PMGR_MODE;
            }

            case E_QUIT:

                return E_QUIT;
        }
    }

    return E_CONTINUE;
}

 #endif                                // CFG_PMGR

 #if defined(__SUPPORT_P2P__)
static unsigned char parse_p2p_config (unsigned char   sysmode,
                                       unsigned char   cur_iface,
                                       unsigned char * p_oper_chan,
                                       unsigned char * p_listen_chan,
                                       unsigned char * p_go_intent,
                                       char          * p_ssid_postfix)
{
    int  input            = 0;
    int  ssid_postfix_len = 0;
    char ssid_postfix_input[wificonfigSSID_POSTFIX_MAX_LEN] = {0};

    if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) || (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
        ((cur_iface == WLAN1_IFACE) && (sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION)))
    {
        if (sysmode != WIFI_DEVICE_MODE_EXT_P2P_STATION)
        {
            do
            {
                /* SSID POSTFIX */
                printf("\n" ANSI_REVERSE " SSID POSTFIX ?" ANSI_NORMAL " [0~22/" ANSI_BOLD "Q" ANSI_NORMAL
                       "uit] (Default : NONE) : ");

                ssid_postfix_len = getStr(ssid_postfix_input, wificonfigSSID_POSTFIX_MAX_LEN);

                if (input == RET_QUIT)
                {
                    return E_QUIT;
                }
                else if (ssid_postfix_len == 0)
                {
                    break;
                }
                else
                {
                    bsp_safe_strcpy(p_ssid_postfix, ssid_postfix_input, wificonfigSSID_POSTFIX_MAX_LEN + 1);
                }
            } while (ssid_postfix_len > wificonfigSSID_POSTFIX_MAX_LEN);

            /* Operation Channel */
            do
            {
                printf(
                    "\n" ANSI_REVERSE " OPERATION CHANNEL ?" ANSI_NORMAL " [1/6/11/36/40/44/48/" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default : Auto) : ");

                input = getNum();

                if (input == RET_QUIT)
                {
                    return E_QUIT;
                }
                else if (input == RET_DEFAULT)
                {
                    *p_oper_chan = WIFI_DIR_OPERATION_CH_DFLT;
                    break;
                }
                else
                {
                    *p_oper_chan = input;
                }
            } while (input != CHANNEL_AUTO && input != 1 && input != 6 && input != 11 &&
                     input != 36 && input != 40 && input != 44 && input != 48);

            if (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO)
            {
                return E_SETUP_WIFI_END;
            }

            do
            {
                /* Listen Channel */
                printf("\n" ANSI_REVERSE " LISTEN CHANNEL ?" ANSI_NORMAL " [1/6/11/" ANSI_BOLD "Q"
                       ANSI_NORMAL "uit] (Default : Auto) : ");

                input = getNum();

                if (input == RET_QUIT)
                {
                    return E_QUIT;
                }
                else if (input == RET_DEFAULT)
                {
                    *p_listen_chan = WIFI_DIR_LISTEN_CH_DFLT;
                    break;
                }
                else
                {
                    *p_listen_chan = input;
                }
            } while (input != CHANNEL_AUTO && input != 1 && input != 6 && input != 11);
        }

        do
        {
            /* GO Intent */
            printf("\n" ANSI_REVERSE " GO INTENT ?" ANSI_NORMAL " [0~15/" ANSI_BOLD "Q" ANSI_NORMAL
                   "uit] (Default : 3) : ");

            input = getNum();

            if (input == RET_QUIT)
            {
                return E_QUIT;
            }
            else if (input == RET_DEFAULT)
            {
                *p_go_intent = WIFI_DIR_GO_INTENT_DFLT;
                break;
            }
            else
            {
                *p_go_intent = input;
            }
        } while (input < 0 || input > 15);

        return E_SETUP_WIFI_END;
    }

    return E_CONTINUE;
}

 #endif                                /* __SUPPORT_P2P__ */

static void setup_display_wlan_preset (struct setup_params * params, unsigned char cur_iface)
{
    char * display_str = NULL;
 #if defined(__SUPPORT_SETBAND_5GHZ__)
    int band = 0;
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */
    unsigned char sysmode   = params->sysmode;
    unsigned char wifi_mode = params->wifi_mode;

    VT_COLORGREEN;

    /* PRINT CONFIGURATION */
    /*============================================\n*/
    print_separate_bar('=', 44, 1);

 #if defined(__SUPPORT_SETBAND_5GHZ__)
    if (params->band == WPA_SETBAND_2G)
    {
        band = 2;
    }
    else if (params->band == WPA_SETBAND_5G)
    {
        band = 5;
    }

    if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) || (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
        ((sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION) && (cur_iface == WLAN1_IFACE)))
    {
        printf("BAND          : ");
    }
    else
    {
        printf("BAND        : ");
    }

    if (band == 2)
    {
        printf("%d.4 GHz\n", band);
    }
    else if (band == 0)
    {
        printf("AUTO\n");
    }
    else
    {
        printf("%d GHz\n", band);
    }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

 #if defined(__SUPPORT_P2P__)

    /* WiFi Direct */
    if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) ||
        (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
        ((sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION) && (cur_iface == WLAN1_IFACE)))
    {
        printf("[ WiFi Direct ]\n");

        if (sysmode != WIFI_DEVICE_MODE_EXT_P2P_STATION)
        {
            printf("Operation Ch. : ");

            if (params->channel == CHANNEL_AUTO)
            {
                printf("Auto\n");
            }
            else
            {
                printf("%2d\n", params->channel);
            }

            if (sysmode == WIFI_DEVICE_MODE_EXT_P2P)
            {
                printf("Listen Ch.    : ");
                if (params->p2p_listen_chan == CHANNEL_AUTO)
                {
                    printf("Auto\n");
                }
                else
                {
                    printf("%2d\n", params->p2p_listen_chan);
                }
            }
        }

        if (sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO)
        {
            printf("GO Intent     : %2d\n", params->p2p_go_intent);
        }
    }

    if ((cur_iface == WLAN0_IFACE) ||
        ((sysmode != WIFI_DEVICE_MODE_EXT_P2P) && (sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO) &&
         ((sysmode != WIFI_DEVICE_MODE_EXT_P2P_STATION) && (cur_iface == WLAN1_IFACE))))
    {
        printf("SSID        : %s\n", params->ssid[cur_iface]);
    }

 #else
    printf("SSID        : %s\n", params->ssid[cur_iface]);
 #endif                                /* __SUPPORT_P2P__ */

 #ifdef __SUPPORT_P2P__

    /* WiFi Direct */
    if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) ||
        (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
        ((sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION) && (cur_iface == WLAN1_IFACE)))
    {
        goto DISPLAY_END;
    }
 #endif                                /* __SUPPORT_P2P__ */

    if (sysmode == WIFI_DEVICE_MODE_EXT_AP)
    {
        if (params->channel == CHANNEL_AUTO)
        {
            printf("CHANNEL     : AUTO(ACS)\n");
        }
        else
        {
            printf("CHANNEL     : %d\n", params->channel);
        }
    }

    switch (params->security[cur_iface])
    {
        case eWiFiSecurityOpen_ext:
        {
            display_str = "OPEN";
            break;
        }

        case eWiFiSecurityWEP_ext:
        {
            display_str = "WEP";
            break;
        }

        case eWiFiSecurityWPA_ext:
        {
            display_str = "WPAPSK";
            break;
        }

        case eWiFiSecurityWPA2_ext:
        {
            display_str = "WPA2PSK";
            break;
        }

        case eWiFiSecurityWPA2_ent_ext:
        {
            display_str = "WPA2 Enterprise";
            break;
        }

        case eWiFiSecurityWPA3_ext:
        {
            display_str = "WPA3";
            break;
        }

        case eWiFiSecurityWPA_ent_ext:
        {
            display_str = "WPA Enterprise";
            break;
        }

        case eWiFiSecurityWPA_WPA2_ent_ext:
        {
            display_str = "WPA + WPA2 Enterprise";
            break;
        }

        case eWiFiSecurityWPA2_WPA3_ent_ext:
        {
            display_str = "WPA2 + WPA3 Enterprise";
            break;
        }

        case eWiFiSecurityWPA3_ent_ext:
        {
            display_str = "WPA3 Enterprise";
            break;
        }

        case eWiFiSecurityWPA3_192B_ent_ext:
        {
            display_str = "WPA3 192B Enterprise";
            break;
        }

        case eWiFiSecurityWPA_WPA2_ext:
        {
            display_str = "WPA + WPA2";
            break;
        }

        case eWiFiSecurityWPA2_WPA3_ext:
        {
            display_str = "WPA2 + WPA3";
            break;
        }

        case eWiFiSecurityWPA3_OWE_ext:
        {
            display_str = "WPA3 OWE";
            break;
        }

        default:
        {
            display_str = "UNKNOWN";
            break;
        }
    }

    printf("Security    : %s\n", display_str);

    if (((params->security[cur_iface] == eWiFiSecurityWPA_ent_ext) ||
         (params->security[cur_iface] == eWiFiSecurityWPA2_ent_ext) ||
         (params->security[cur_iface] == eWiFiSecurityWPA_WPA2_ent_ext) ||
         (params->security[cur_iface] == eWiFiSecurityWPA2_WPA3_ent_ext) ||
         (params->security[cur_iface] == eWiFiSecurityWPA3_ent_ext) ||
         (params->security[cur_iface] == eWiFiSecurityWPA3_192B_ent_ext)) && (cur_iface == WLAN0_IFACE))
    {
        switch (params->eap_auth_mode)
        {
            case E_EAP_AUTH_MODE_PEAP_TTLS_FAST:
            {
                display_str = "PEAP or TTLS or FAST";
                break;
            }

            case E_EAP_AUTH_MODE_PEAP:
            {
                display_str = "PEAP";
                break;
            }

            case E_EAP_AUTH_MODE_TTLS:
            {
                display_str = "TTLS";
                break;
            }

            case E_EAP_AUTH_MODE_FAST:
            {
                display_str = "FAST";
                break;
            }

            case E_EAP_AUTH_MODE_TLS:
            {
                display_str = "TLS";
                break;
            }

            case E_EAP_AUTH_MODE_NONE:
            default:
            {
                display_str = "";
                break;
            }
        }

        printf("Auth. Type  : %s\n", display_str);

        if (params->eap_auth_mode != E_EAP_AUTH_MODE_TLS)
        {
            if ((params->eap_auth_mode == E_EAP_AUTH_MODE_PEAP_TTLS_FAST) ||
                (params->eap_auth_mode == E_EAP_AUTH_MODE_PEAP) ||
                (params->eap_auth_mode == E_EAP_AUTH_MODE_TTLS) ||
                (params->eap_auth_mode != E_EAP_AUTH_MODE_FAST))
            {
                switch (params->eap_phase2)
                {
                    case E_EAP_PHASE2_MODE_MSCHAPV2:
                    {
                        display_str = "MSCHAPV2";
                        break;
                    }

                    case E_EAP_PHASE2_MODE_GTC:
                    {
                        display_str = "GTC";
                        break;
                    }

                    case E_EAP_PHASE2_MODE_TLS:
                    {
                        display_str = "TLS";
                        break;
                    }

                    case E_EAP_PHASE2_MODE_MSCHAPV2_N_GTC:
                    default:
                    {
                        display_str = "MSCHAPV2 or GTC";
                        break;
                    }
                }

                printf("Auth. Proto : %s\n", display_str);
            }
        }

        printf("ID          : %s\n", params->eap_id);
        if ((params->eap_auth_mode == E_EAP_AUTH_MODE_TLS) || (params->eap_phase2 == E_EAP_PHASE2_MODE_TLS))
        {
            printf("Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT3) == pdTRUE ? "Found" : "Empty");
            printf("Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY3) == pdTRUE ? "Found" : "Empty");
        }
        else
        {
            printf("Password    : %s\n", params->eap_pw);
        }
    }

    /* Print the encryption configuration type */
    if ((params->security[cur_iface] == eWiFiSecurityWPA_ext) ||
        (params->security[cur_iface] == eWiFiSecurityWPA2_ext) ||
        (params->security[cur_iface] == eWiFiSecurityWPA3_ext) ||
        (params->security[cur_iface] == eWiFiSecurityWPA3_OWE_ext) ||
        (params->security[cur_iface] == eWiFiSecurityWPA_WPA2_ext) ||
        (params->security[cur_iface] == eWiFiSecurityWPA2_WPA3_ext))
    {
        switch (params->security[cur_iface])
        {
            case eWiFiSecurityWPA_ext:
            {
                display_str = "TKIP";
                break;
            }

            case eWiFiSecurityWPA2_ext:
            case eWiFiSecurityWPA3_ext:
            case eWiFiSecurityWPA3_OWE_ext:
            {
                display_str = "AES(CCMP)";
                break;
            }

            case eWiFiSecurityWPA_WPA2_ext:
            {
                display_str = "TKIP/AES(CCMP)";
                break;
            }

            case eWiFiSecurityWPA2_WPA3_ext:
            {
                display_str = "AES(CCMP)/GCMP";
                break;
            }

            default:
            {
                display_str = "UNKNOWN";
                break;
            }
        }

        printf("ENCRYPTION  : %s\n", display_str);
    }

    if ((params->security[cur_iface] == eWiFiSecurityWEP_ext) && (cur_iface == WLAN0_IFACE))
    {
        printf("WEP KEY     : %s\n", params->wep_key);
        printf("KEY BIT     : %d\n", params->wep_bit == WEP_KEY_64BIT ? 64 : 128);
        printf("KEY INDEX   : %d\n", params->wep_key_idx);
        printf("KEY TYPE    : %s\n", params->wep_key_type == WEP_KEY_TYPE_ASCII ? "ASCII" : "HEXA");
    }

    if (params->security[cur_iface] > eWiFiSecurityOpen_ext)
    {
        printf("Password    : %s\n", params->password[cur_iface]);
    }

 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || defined __SUPPORT_MESH__
    if (strlen(params->sae_groups[cur_iface]) > 1)
    {
        printf("SAE_Groups  : %s\n", params->sae_groups[cur_iface]);
    }
 #endif                                /* __SUPPORT_WPA3_SAE__ || __SUPPORT_MESH__ */

    if ((sysmode == WIFI_DEVICE_MODE_EXT_AP) && (cur_iface == WLAN1_IFACE))
    {
        printf("WIFI MODE   : %s\n",
               params->channel == 14 ? wifi_mode_str[(int) WIFI_MODE_B_ONLY] :
               wifi_mode_str[wifi_mode - GAP_USER_CONFIGURE_WIFI_MODE]);
    }

    if (params->security[cur_iface] >= eWiFiSecurityWPA2_ext)
    {
        switch (params->pmf[cur_iface])
        {
            case PMF_NONE:
            {
                display_str = "Disable";
                break;
            }

            case PMF_CAPABLE:
            {
                display_str = "Optional";
                break;
            }

            case PMF_REQUIRED:
            {
                display_str = "Mandatory";
                break;
            }

            case PMF_DEFAULT:
            default:
            {
                display_str = "Default";
                break;
            }
        }

        printf("PMF         : %s\n", display_str);
    }

 #if defined(__SUPPORT_P2P__)
DISPLAY_END:
 #endif                                /* __SUPPORT_P2P__ */

    /*============================================\n*/
    print_separate_bar('=', 44, 1);
    VT_NORMAL;
}

static unsigned char setup_wifi_config_confirm (unsigned char sysmode, unsigned char cur_iface)
{
    switch (get_yes_no(ANSI_REVERSE " WIFI CONFIGURATION CONFIRM ?" ANSI_NORMAL " [" ANSI_BOLD "Y" ANSI_NORMAL "es/"
                       ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                       false))
    {
        case 0:
        {
            return E_WLAN_SETUP;
        }

        case E_QUIT:

            return E_QUIT;
    }

 #if defined(__SUPPORT_P2P__)
    if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P) || (sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO))
    {
        return E_SETUP_APPLY_START;
    }
    else if ((sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION) && (cur_iface == WLAN1_IFACE))
    {
        return E_SETUP_APPLY_START;
    }
    else
 #endif                                /* __SUPPORT_P2P__ */
    {
        return E_CONTINUE;
    }
}

static unsigned char setup_netmode (unsigned char sysmode, unsigned char cur_iface, int * p_netmode)
{
    int  getStr_len = 0;
    char input_str[3];

    if ((sysmode == WIFI_DEVICE_MODE_EXT_STATION) ||
        ((cur_iface == WLAN0_IFACE) &&
         (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION
 #if defined(__SUPPORT_P2P__)
          || sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION
 #endif                                /* __SUPPORT_P2P__ */
         )
        ))
    {
        /* Station mode || concurrent Station */
        /* NETMODE */
        do
        {
            printf(ANSI_REVERSE " IP Connection Type ?" ANSI_NORMAL " [" ANSI_BOLD "A" ANSI_NORMAL
                   "utomatic IP/" ANSI_BOLD "S" ANSI_NORMAL "tatic IP/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ");
            memset(input_str, 0, 3);
            getStr_len = getStr(input_str, 2);

            input_str[0] = (char) toupper(input_str[0]);

            if (getStr_len <= 1)
            {
                if ((input_str[0] == 'A') || (getStr_len == 0))
                {
                    *p_netmode = E_NETMODE_DYNAMIC_IP;
                    break;
                }
                else if (input_str[0] == 'S')
                {
                    *p_netmode = E_NETMODE_STATIC_IP;
                    break;
                }
                else if ((input_str[0] == 'Q') || (getStr_len == RET_QUIT))
                {
                    return E_QUIT;
                }
            }
        } while (1);

        VT_COLORGREEN;
        printf("\nIP Connection Type: %s" ANSI_NORMAL "\n\n",
               *p_netmode == E_NETMODE_DYNAMIC_IP ? "Automatic IP" : "Static IP");
    }
    else
    {
        *p_netmode = E_NETMODE_STATIC_IP; /* Static IP */
    }

    return E_CONTINUE;
}

static unsigned char setup_static_ip (unsigned char cur_iface, char * p_ipaddress)
{
    int getStr_len = 0;

    do
    {
        printf("\n" ANSI_REVERSE " IP ADDRESS ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL
               "uit] (Default %s) : ",
               cur_iface == WLAN0_IFACE ? DEFAULT_IPADDR_WLAN0 : DEFAULT_IPADDR_WLAN1);

        getStr_len = getStr(p_ipaddress, 15);

        p_ipaddress[0] = (char) toupper(p_ipaddress[0]);

        if (((getStr_len == 1) && (p_ipaddress[0] == 'Q')) || (getStr_len == RET_QUIT))
        {
            return E_QUIT;
        }

        if (getStr_len == 0)
        {
            bsp_safe_strcpy(p_ipaddress, cur_iface == WLAN0_IFACE ? DEFAULT_IPADDR_WLAN0 : DEFAULT_IPADDR_WLAN1, SETUP_IP_STR_LEN);
        }
    } while (!is_in_valid_ip_class(p_ipaddress));

    return E_CONTINUE;
}

static unsigned char setup_static_subnet (unsigned char cur_iface, char * p_subnetmask)
{
    int getStr_len = 0;

    do
    {
        printf("\n" ANSI_REVERSE " SUBNET ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL
               "uit] (Default %s) : ",
               cur_iface == WLAN0_IFACE ? DEFAULT_SUBNET_WLAN0 : DEFAULT_SUBNET_WLAN1);

        getStr_len = getStr(p_subnetmask, 15);

        p_subnetmask[0] = (char) toupper(p_subnetmask[0]);

        if (((getStr_len == 1) && (p_subnetmask[0] == 'Q')) || (getStr_len == RET_QUIT))
        {
            return E_QUIT;
        }

        if (getStr_len == 0)
        {
            bsp_safe_strcpy(p_subnetmask, cur_iface == WLAN0_IFACE ? DEFAULT_SUBNET_WLAN0 : DEFAULT_SUBNET_WLAN1, SETUP_IP_STR_LEN);
        }
    } while (!isvalidmask(p_subnetmask));

    return E_CONTINUE;
}

static unsigned char setup_static_gateway (unsigned char sysmode,
                                           unsigned char cur_iface,
                                           char        * p_ipaddress,
                                           char        * p_subnetmask,
                                           char        * p_gateway)
{
    int       getStr_len = 0;
    ip_addr_t tmp_addr;
    uint32_t  ipaddress, subnetmask, gateway;

    ipaddr_aton(p_ipaddress, &tmp_addr);
    ipaddress = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

    ipaddr_aton(p_subnetmask, &tmp_addr);
    subnetmask = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

 #if defined(__SUPPORT_P2P__)
    if (
  #if !defined(__SUPPORT_SOFTAP_DFLT_GW__)
        sysmode != WIFI_DEVICE_MODE_EXT_AP &&
  #endif                               /* !__SUPPORT_SOFTAP_DFLT_GW__ */
        sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO)
 #else
    RA6W1_UNUSED_ARG(sysmode);

    if (
  #if !defined(__SUPPORT_SOFTAP_DFLT_GW__)
        sysmode != WIFI_DEVICE_MODE_EXT_AP &&
  #endif                               /* !__SUPPORT_SOFTAP_DFLT_GW__ */
        pdTRUE)
 #endif                                /* __SUPPORT_P2P__ */
    {
        /* Gateway */
        do
        {
            printf("\n" ANSI_REVERSE " GATEWAY ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL
                   "uit] (Default %s) : ",
                   cur_iface == WLAN0_IFACE ? DEFAULT_GATEWAY_WLAN0 : DEFAULT_GATEWAY_WLAN1);

            getStr_len   = getStr(p_gateway, 15);
            p_gateway[0] = (char) toupper(p_gateway[0]);

            if (((getStr_len == 1) && (p_gateway[0] == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }

            if (getStr_len == 0)
            {
                bsp_safe_strcpy(p_gateway, cur_iface == WLAN0_IFACE ? DEFAULT_GATEWAY_WLAN0 : DEFAULT_GATEWAY_WLAN1, SETUP_IP_STR_LEN);
            }

            ipaddr_aton(p_gateway, &tmp_addr);
            gateway = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        } while ((!is_in_valid_ip_class(p_gateway) && strcmp(p_gateway, "0.0.0.0") != 0) ||
                 (cur_iface == WLAN0_IFACE && strcmp(p_ipaddress, p_gateway) == 0) ||
                 (!isvalidIPsubnetRange(gateway, ipaddress, subnetmask) &&
                  strcmp(p_gateway, "0.0.0.0") != 0));
    }

    return E_CONTINUE;
}

static unsigned char setup_static_dns (unsigned char sysmode, unsigned char cur_iface, char * p_dns)
{
    int getStr_len = 0;

    if (sysmode != WIFI_DEVICE_MODE_EXT_AP
 #if defined(__SUPPORT_P2P__)
        && sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO
 #endif                                /* __SUPPORT_P2P__ */
        && !(sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION && cur_iface == WLAN1_IFACE))
    {
        /* DNS */
        do
        {
            printf("\n" ANSI_REVERSE " DNS ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL
                   "uit] (Default %s) : ",
                   cur_iface == WLAN0_IFACE ? DEFAULT_DNS_WLAN0 : DEFAULT_DNS_WLAN1);

            getStr_len = getStr(p_dns, 15);
            p_dns[0]   = (char) toupper(p_dns[0]);

            if (((getStr_len == 1) && (p_dns[0] == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }

            if (getStr_len == 0)
            {
                bsp_safe_strcpy(p_dns, cur_iface == WLAN0_IFACE ? DEFAULT_DNS_WLAN0 : DEFAULT_DNS_WLAN1, SETUP_IP_STR_LEN);
            }
        } while (!is_in_valid_ip_class(p_dns) && strcmp(p_dns, "0.0.0.0") != 0);
    }

    return E_CONTINUE;
}

static void setup_display_network_preset (unsigned char cur_iface,
                                          unsigned char sysmode,
                                          char        * p_ipaddress,
                                          char        * p_subnetmask,
                                          char        * p_gateway,
                                          char        * p_dns)

{
    VT_COLORGREEN;

    /*============================================\n*/
    print_separate_bar('=', 44, 1);

    printf("[WLAN%d]\n", cur_iface);
    printf("IP ADDRESS: %s\n", p_ipaddress);
    printf("SUBNET    : %s\n", p_subnetmask);

 #if defined(__SUPPORT_SOFTAP_DFLT_GW__)
    if ((sysmode == WIFI_DEVICE_MODE_EXT_AP) ||
        ((sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION) && (cur_iface == WLAN1_IFACE)))
    {
        printf("GATEWAY   : %s\n", p_gateway);
    }
    else
 #endif                                /* __SUPPORT_SOFTAP_DFLT_GW__ */
    if (
 #if !defined(__SUPPORT_SOFTAP_DFLT_GW__)
        sysmode != WIFI_DEVICE_MODE_EXT_AP &&
 #endif                                /* !__SUPPORT_SOFTAP_DFLT_GW__ */
 #if defined(__SUPPORT_P2P__)
        sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO
 #else
        pdTRUE
 #endif                                /* __SUPPORT_P2P__ */
        )
    {
        printf("GATEWAY   : %s\n", p_gateway);
        printf("DNS       : %s\n", p_dns);
    }

    /*============================================\n*/
    print_separate_bar('=', 44, 1);

    VT_NORMAL;
}

static unsigned char setup_check_network (unsigned char cur_iface,
                                          unsigned char sysmode,
                                          char        * p_ipaddress,
                                          char        * p_subnetmask,
                                          char        * p_gateway)
{
 #if !defined(__SUPPORT_P2P__)
    RA6W1_UNUSED_ARG(sysmode);
 #endif                                /* __SUPPORT_P2P__ */

    if (
 #if !defined(__SUPPORT_SOFTAP_DFLT_GW__)
        sysmode != WIFI_DEVICE_MODE_EXT_AP &&
 #endif                                /* !__SUPPORT_SOFTAP_DFLT_GW__ */
 #if defined(__SUPPORT_P2P__)
        sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO
 #else
        pdTRUE
 #endif                                                                                              /* __SUPPORT_P2P__ */
        )
    {
        /* Check Validity */
        if (((is_in_valid_ip_class(p_gateway) == pdFALSE) && (strcmp(p_gateway, "0.0.0.0") != 0)) || /* check gateway */
            ((cur_iface == WLAN0_IFACE) && (strcmp(p_ipaddress, p_gateway) == 0)))
        {
            VT_COLORRED;
            printf("\nInvalid GATEWAY Address !!!\n");
            VT_NORMAL;

            return E_NETWORK_SETUP;
        }
        else if ((isvalidmask(p_subnetmask) == pdFALSE) || (is_in_valid_ip_class(p_ipaddress) == pdFALSE))
        {
            VT_COLORRED;
            printf("\nInvalid IP Address !!!\n");
            VT_NORMAL;

            return E_INPUT_IPADDRESS;
        }
        else
        {
            ip_addr_t     tmp_addr;
            unsigned long ipaddr_val, gateway_val, subnetmask_val;

            ipaddr_aton(p_ipaddress, &tmp_addr);
            ipaddr_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
            memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

            ipaddr_aton(p_gateway, &tmp_addr);
            gateway_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
            memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

            ipaddr_aton(p_subnetmask, &tmp_addr);
            subnetmask_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

            if ((isvalidIPsubnetRange(ipaddr_val, gateway_val,
                                      subnetmask_val) == pdFALSE) && (strcmp(p_gateway, "0.0.0.0") != 0))
            {
                VT_COLORRED;
                printf("\nInvalid IP Address !!!\n");
                VT_NORMAL;

                return E_INPUT_IPADDRESS;
            }
        }
    }

    return E_CONTINUE;
}

static unsigned char setup_network_config_confirm (unsigned char sysmode)
{
    switch (get_yes_no(ANSI_REVERSE " IP CONFIGURATION CONFIRM ?" ANSI_NORMAL " [" ANSI_BOLD "Y"
                       ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                       false))
    {
        case 0:
        {
            return sysmode == WIFI_DEVICE_MODE_EXT_AP ? E_INPUT_IPADDRESS : E_NETWORK_SETUP;
        }

        case E_QUIT:

            return E_QUIT;
    }

    return E_CONTINUE;
}

 #define NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL    3600 * 36 /*131072*/
static unsigned char setup_sntp_client (struct sntp_params * params)
{
    int getStr_len = 0;
    params->sntp_client_period_time = NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL;

    /* Station mode || concurrent Station */
    /* NETMODE */
SNTP_CLIENT_START:

    switch (get_yes_no(ANSI_REVERSE " SNTP Client enable ?" ANSI_NORMAL " [" ANSI_BOLD "Y" ANSI_NORMAL
                       "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                       true))
    {
        case E_CONTINUE:
        case 1:
        {
            params->sntp_client = E_SNTP_CLIENT_START;
            break;
        }

        case 0:
        {
            params->sntp_client = E_SNTP_CLIENT_STOP;

            return E_CONTINUE;
        }

        case E_QUIT:

            return E_QUIT;
    }

    /* Set SNTP period */
    do
    {
        printf("\n" ANSI_REVERSE " SNTP Period time (%d ~ %d hours) ? (default : %d hours)" \
               ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit] ",
               1,
               NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL / 3600,
               NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL / 3600);
        params->sntp_client_period_time = getNum();

        if (params->sntp_client_period_time == RET_QUIT)
        {
            return E_QUIT;
        }
        else if (params->sntp_client_period_time == RET_DEFAULT)
        {
            params->sntp_client_period_time = 3600 * (NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL / 3600);
            break;
        }
        else
        {
            params->sntp_client_period_time *= 3600;
        }
    } while (params->sntp_client_period_time < 3600 ||
             params->sntp_client_period_time > NX_SNTP_CLIENT_MAX_UNICAST_POLL_INTERVAL);

    /* Set GMT Timezone */
    do
    {
        printf("\n" ANSI_REVERSE
               " GMT Timezone +xx:xx|-xx:xx (-12:00 ~ +12:00) ? (default : 00:00)" \
               ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit] ");
        memset(params->sntp_gmt_timezone, 0, 8);
        getStr_len = getStr(params->sntp_gmt_timezone, 8);

        if (((getStr_len == 1) && (toupper(params->sntp_gmt_timezone[0]) == 'Q')) || (getStr_len == RET_QUIT))
        {
            return E_QUIT;
        }
        else if ((getStr_len == 0) || (strncmp(params->sntp_gmt_timezone, "00:00", 5) == 0))
        {
            bsp_safe_strcpy(params->sntp_gmt_timezone, "00:00", sizeof(params->sntp_gmt_timezone));
            params->sntp_timezone_int = 0;
            break;
        }
        else if (getStr_len < 0)
        {
            continue;
        }
        else if ((getStr_len == 6) && (params->sntp_gmt_timezone[3] == ':'))
        {
            char timezone_temp[8]   = {0, 0};
            int  timezone_offset[2] = {0, 0};

            bsp_safe_strcpy(timezone_temp, params->sntp_gmt_timezone, sizeof(timezone_temp));

            timezone_offset[0] = atoi(strtok(timezone_temp, ":"));
            timezone_offset[1] = atoi(strtok(NULL, "\0"));

            if ((timezone_offset[0] == 0) && (timezone_offset[1] == 0))
            {
                params->sntp_timezone_int = 0;
            }
            else if ((timezone_offset[0] > 12) ||
                     (timezone_offset[0] < -12) ||
                     (timezone_offset[1] >= 60) ||
                     ((timezone_offset[0] == 0) && (params->sntp_gmt_timezone[1] != '0')))
            {
                continue;
            }
            else
            {
                params->sntp_timezone_int = ((3600 * (long) timezone_offset[0]) +
                                             ((timezone_offset[0] < 0) ? (-1 * (long) timezone_offset[1] * 60) : ((
                                                                                                                      long)
                                                                                                                  timezone_offset
                                                                                                                  [1] *
                                                                                                                  60)));
            }

            break;
        }
    } while (1);

    /* Set SNTP Server address */
    do
    {
        if (params->sntp_svr_index == 0)
        {
            printf("\n" ANSI_REVERSE " SNTP Server %d addr ? (default : %s)" ANSI_NORMAL " [" ANSI_BOLD
                   "Q" ANSI_NORMAL "uit]\n",
                   params->sntp_svr_index,
                   DFLT_SNTP_SERVER_DOMAIN);
            printf("Input : ");
            memset(params->sntp_svr_addr, 0, 256);
            getStr_len = getStr(params->sntp_svr_addr, 256);

            if (((getStr_len == 1) && (toupper(params->sntp_svr_addr[0]) == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }
            else if (getStr_len == 0)
            {
                bsp_safe_strcpy(params->sntp_svr_addr, DFLT_SNTP_SERVER_DOMAIN, sizeof(params->sntp_svr_addr));
            }
            else if (isvalid_domain(params->sntp_svr_addr) == 0)
            {
                continue;
            }
        }
        else if (params->sntp_svr_index == 1)
        {
            printf("\n" ANSI_REVERSE " SNTP Server %d addr ? (default : %s)" ANSI_NORMAL " [" ANSI_BOLD
                   "Q" ANSI_NORMAL "uit]\n",
                   params->sntp_svr_index,
                   DFLT_SNTP_SERVER_DOMAIN_1);
            printf("Input : ");
            memset(params->sntp_svr_addr1, 0, 256);
            getStr_len = getStr(params->sntp_svr_addr1, 256);

            if (((getStr_len == 1) && (toupper(params->sntp_svr_addr1[0]) == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }
            else if (getStr_len == 0)
            {
                bsp_safe_strcpy(params->sntp_svr_addr1, DFLT_SNTP_SERVER_DOMAIN_1, sizeof(params->sntp_svr_addr1));
            }
            else if (isvalid_domain(params->sntp_svr_addr1) == 0)
            {
                continue;
            }
        }
        else if (params->sntp_svr_index == 2)
        {
            printf("\n" ANSI_REVERSE " SNTP Server %d addr ? (default : %s)" ANSI_NORMAL " [" ANSI_BOLD
                   "Q" ANSI_NORMAL "uit]\n",
                   params->sntp_svr_index,
                   DFLT_SNTP_SERVER_DOMAIN_2);
            printf("Input : ");
            memset(params->sntp_svr_addr2, 0, 256);
            getStr_len = getStr(params->sntp_svr_addr2, 256);

            if (((getStr_len == 1) && (toupper(params->sntp_svr_addr2[0]) == 'Q')) || (getStr_len == RET_QUIT))
            {
                return E_QUIT;
            }
            else if (getStr_len == 0)
            {
                bsp_safe_strcpy(params->sntp_svr_addr2, DFLT_SNTP_SERVER_DOMAIN_2, sizeof(params->sntp_svr_addr2));
            }
            else if (isvalid_domain(params->sntp_svr_addr2) == 0)
            {
                continue;
            }
        }
        else
        {
            params->sntp_svr_index = 0;
            break;
        }

        if (params->sntp_svr_index > 2)
        {
            params->sntp_svr_index = 0;
            break;
        }

        params->sntp_svr_index++;
    } while (1);

    VT_COLORGREEN;

    /*============================================\n*/
    print_separate_bar('=', 44, 1);

    printf("SNTP Client      : ");
    if (params->sntp_client == E_SNTP_CLIENT_START)
    {
        printf("Enable\n");
        printf("SNTP Period time : %d hours\n", params->sntp_client_period_time / 3600);
        printf("SNTP GMT Timezone: %s\n", params->sntp_gmt_timezone);
        printf("SNTP Server addr : %s\n", params->sntp_svr_addr);
        printf("SNTP Server addr1: %s\n", params->sntp_svr_addr1);
        printf("SNTP Server addr2: %s\n", params->sntp_svr_addr2);
    }
    else
    {
        printf("Disable\n");
    }

    /*============================================\n*/
    print_separate_bar('=', 44, 1);
    VT_NORMAL;

    switch (get_yes_no(ANSI_REVERSE " SNTP Client CONFIRM ?" ANSI_NORMAL " [" ANSI_BOLD "Y" ANSI_NORMAL
                       "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                       false))
    {
        case 0:
        {
            goto SNTP_CLIENT_START;
        }

        case E_QUIT:

            return E_QUIT;
    }

    return E_CONTINUE;
}

 #ifdef __SUPPORT_IPV4__
  #ifdef __SUPPORT_DHCP_SVR__
static unsigned char setup_dhcp_server (unsigned char   sysmode,
                                        unsigned char   cur_iface,
                                        char          * p_ipaddress,
                                        char          * p_subnetmask,
                                        char          * p_gateway,
                                        char          * p_dns,
                                        unsigned char * p_use_dhcps,
                                        char          * p_dhcp_lease_start,
                                        char          * p_dhcp_lease_end,
                                        int           * p_dhcp_lease_time,
                                        int           * p_dhcp_lease_count)
{
    RA6W1_UNUSED_ARG(sysmode);
    RA6W1_UNUSED_ARG(p_ipaddress);
    RA6W1_UNUSED_ARG(p_subnetmask);
    RA6W1_UNUSED_ARG(p_gateway);
    RA6W1_UNUSED_ARG(p_dns);

    ip_addr_t     tmp_addr;
    unsigned long ipaddr_val, gateway_val, subnetmask_val;

    /* Setup DHCP Server */
    if (((sysmode == WIFI_DEVICE_MODE_EXT_AP) || (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)) &&
        (cur_iface == WLAN1_IFACE))
    {
        /* AP Mode */

        switch (get_yes_no("\n" ANSI_REVERSE " DHCP SERVER CONFIGURATION ?" ANSI_NORMAL " ["
                           ANSI_BOLD "Y" ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL
                           "uit] : ",
                           true))
        {
            case 0:
            {
                return E_CONTINUE;
            }

            case E_QUIT:

                return E_QUIT;
        }

        *p_use_dhcps = 1;

        /* Lease ip count */
INPUT_LEASE_IP_COUNT:

        do
        {
            printf(
                "\n" ANSI_REVERSE " DHCP SERVER LEASE IP Count(MAX %d) ?" ANSI_NORMAL " [" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default %d) : ",
                MAX_DHCP_LEASE_COUNT,
                DEFAULT_DHCP_LEASE_COUNT);
            *p_dhcp_lease_count = getNum();

            if (*p_dhcp_lease_count == RET_QUIT)
            {
                return E_QUIT;
            }

            if (*p_dhcp_lease_count == RET_DEFAULT)
            {
                *p_dhcp_lease_count = DEFAULT_DHCP_LEASE_COUNT;
            }
            else if ((*p_dhcp_lease_count < 1) || (*p_dhcp_lease_count > MAX_DHCP_LEASE_COUNT))
            {
                continue;
            }

            /*
             *  How to specify a default IP range :
             *  1. Assign as many as specified from interface IP +1 IP.
             *  2. If the specified number exceeds ServerNet, allocate from the ServerNet starting IP.
             *  3. Do not include the gateway IP in the specified range..
             */
            ipaddr_aton(p_ipaddress, &tmp_addr);
            ipaddr_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
            memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

            ipaddr_aton(p_gateway, &tmp_addr);
            gateway_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
            memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

            ipaddr_aton(p_subnetmask, &tmp_addr);
            subnetmask_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

            if (isvalidIPsubnetRange((ipaddr_val + *p_dhcp_lease_count), ipaddr_val, subnetmask_val) &&
                ((!isvalidIPrange(gateway_val, ipaddr_val + 1, ipaddr_val + *p_dhcp_lease_count)) ||
                 (sysmode != WIFI_DEVICE_MODE_EXT_AP
   #if defined(__SUPPORT_P2P__)
                  && sysmode != WIFI_DEVICE_MODE_EXT_P2P_GO
   #endif                              /* __SUPPORT_P2P__ */
                 )
                ))
            {
                /* Lease First ip */
                longtoip((ipaddr_val + 1), p_dhcp_lease_start);

                /* Lease Last ip */
                longtoip((ipaddr_val + *p_dhcp_lease_count), p_dhcp_lease_end);
            }
            else
            {
                long firstIP;

                firstIP = subnetRangeFirstIP(ipaddr_val, subnetmask_val);

                if ((unsigned long) firstIP == gateway_val)
                {
                    /* If firstIP and Gateway IP are the same.*/
                    firstIP++;
                }

                if (isvalidIPrange(ipaddr_val, firstIP, firstIP + (*p_dhcp_lease_count - 1)) ||
                    isvalidIPrange(gateway_val, firstIP, firstIP + (*p_dhcp_lease_count - 1)))
                {
                    VT_COLORRED;
                    printf("\nERR : DHCP Server Lease Range\n");
                    VT_NORMAL;
                    continue;
                }

                /* Lease First IP */
                longtoip(firstIP, p_dhcp_lease_start);

                /* Lease List IP */
                longtoip(firstIP + (*p_dhcp_lease_count - 1), p_dhcp_lease_end);
            }

            break;
        } while (1);

        /* Lease Time */
        do
        {
            printf("\n" ANSI_REVERSE " DHCP SERVER LEASE TIME(%d ~ %d SEC) ?" ANSI_NORMAL " ["
                   ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default %d) : ",
                   MIN_DHCP_SERVER_LEASE_TIME,
                   MAX_DHCP_SERVER_LEASE_TIME,
                   DEFAULT_DHCP_SERVER_LEASE_TIME);

            *p_dhcp_lease_time = getNum();

            if (*p_dhcp_lease_time == RET_QUIT)
            {
                return E_QUIT;
            }

            if (*p_dhcp_lease_time == RET_DEFAULT)
            {
                *p_dhcp_lease_time = DEFAULT_DHCP_SERVER_LEASE_TIME;
            }
        } while (*p_dhcp_lease_time < MIN_DHCP_SERVER_LEASE_TIME ||
                 *p_dhcp_lease_time > MAX_DHCP_SERVER_LEASE_TIME);

        VT_COLORGREEN;

        /*============================================\n*/
        print_separate_bar('=', 44, 1);
        printf("[DHCP SERVER]\n");
        printf("Start IP  : %s\n", p_dhcp_lease_start);
        printf("END IP    : %s\n", p_dhcp_lease_end);
   #if defined __SUPPORT_NAT__
        if (sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)
        {
            printf("DNS       : %s\n", p_dns);
        }
   #endif                              /* __SUPPORT_NAT__ */
        printf("LEASE TIME: %d\n", *p_dhcp_lease_time);

        /*============================================\n*/
        print_separate_bar('=', 44, 1);
        VT_NORMAL;

        switch (get_yes_no("\n" ANSI_REVERSE " DHCP SERVER CONFIGURATION CONFIRM ?" ANSI_NORMAL " [" ANSI_BOLD
                           "Y" ANSI_NORMAL "es/" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
                           false))
        {
            case 0:
            {
                goto INPUT_LEASE_IP_COUNT;
            }

            case E_QUIT:

                return E_QUIT;
        }
    }

    return E_CONTINUE;
}

  #endif                               /* __SUPPORT_DHCP_SVR__ */
 #endif                                /* __SUPPORT_IPV4__ */

static unsigned char setup_pmf (WIFISecurityExt_t security, WIFIPmf_t * p_pmf)
{
    int input_num;

    *p_pmf = PMF_DEFAULT;

    if (!((security == eWiFiSecurityWPA2_ext) ||
          (security == eWiFiSecurityWPA_WPA2_ext) ||
          (security == eWiFiSecurityWPA2_WPA3_ext)
          ))
    {
        return E_CONTINUE;
    }

    do
    {
        printf(
            "\nProtected Management Frame(PMF, MFP, IEEE80211W) ?\n" ANSI_REVERSE " Mode ?" ANSI_NORMAL " [%s1:Optional/2:Mandatory/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
            security == eWiFiSecurityWPA2_WPA3_ext ? "" : "0:Disable/");

        input_num = getNum();

        if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
    } while (!((input_num >= PMF_CAPABLE || (input_num >= PMF_NONE && security != eWiFiSecurityWPA2_WPA3_ext)) &&
               input_num <= PMF_REQUIRED));

    *p_pmf = input_num;

    return E_CONTINUE;
}

 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || (defined __SUPPORT_MESH__)
static unsigned char setup_input_sae_group (char * p_sae_groups)
{
  #define DEFAULT_SAE_GROUPS    "19 20 21"

    int  getStr_len = 0;
    char group_params[wificonfigMAX_SAE_GROUPS_LEN];
    bool start_over;

    do
    {
        start_over = FALSE;

        printf("\n" ANSI_REVERSE " SAE_Groups ? [19/20/21](Default %s)" ANSI_NORMAL " : ", DEFAULT_SAE_GROUPS);

        getStr_len = getStr(p_sae_groups, wificonfigMAX_SAE_GROUPS_LEN - 1);

        if (getStr_len == 0)
        {
            bsp_safe_strcpy(p_sae_groups, DEFAULT_SAE_GROUPS, wificonfigMAX_SAE_GROUPS_LEN);
        }
        else if (getStr_len == RET_QUIT)
        {
            memset(p_sae_groups, 0x0, wificonfigMAX_SAE_GROUPS_LEN);

            return E_QUIT;
        }

        bsp_safe_strcpy(group_params, p_sae_groups, sizeof(group_params));

        /* check : num, space or tab */
        for (size_t idx = 0; idx < strlen(group_params); idx++)
        {
            if (!isdigit((int) group_params[idx]) && !isblank((int) group_params[idx]))
            {
                start_over = TRUE;
                break;
            }
        }

        if (start_over)
        {
            continue;
        }

        for (int i = 0; i < getStr_len; i++)
        {
            char * tmp;

            if (i == 0)
            {
                tmp = strtok(group_params, " ");
            }
            else
            {
                tmp = strtok(NULL, " ");
            }

            if (tmp == NULL)
            {
                break;
            }

            if (check_sae_groupid(atoi(tmp)) == 0)
            {
                start_over = TRUE;
                break;
            }
        }
    } while (start_over);

    return E_CONTINUE;
}

 #endif                                // __SUPPORT_WPA3_SAE__) || ( __SUPPORT_MESH__)

static unsigned char setup_wifimode (unsigned char   cur_iface,
                                     unsigned char   e_band,
                                     unsigned char   sysmode,
                                     unsigned char * wifi_mode)
{
    int input_num;

    if ((sysmode >= WIFI_DEVICE_MODE_EXT_AP_STATION) || (cur_iface != WLAN1_IFACE))
    {
        return E_CONTINUE;
    }

    /* SoftAP Only */
    /* WiFi Mode */
    do
    {
        printf("\nWiFi MODE ?\n" \
               "\t%d. %s%s\n"          /* auto */
               "\t%d. %s%s\n"          /* 11gn */
               "\t%d. %s%s\n"          /* 11bg */
               "\t%d. %s%s\n"          /* 11n */
               "\t%d. %s%s\n"          /* 11g */
               "\t%d. %s%s\n"          /* 11b */
               "\t%d. %s%s\n"          /* 11an */
               "\t%d. %s%s\n"          /* 11a */
               "\t%d. %s%s\n"          /* 11n_5GHz */
               ANSI_REVERSE " MODE ?" ANSI_NORMAL " [%d~%d/" ANSI_BOLD "Q" ANSI_NORMAL "uit] : ",
               WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_BGN],
                                       /* Auto */
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "(Recommend)",
               WIFI_MODE_GN + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_GN],
                                       /* 11gn 2.4Ghz*/
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_BG + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_BG],
                                       /* 11bg 2.4Ghz */
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_N_ONLY + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_N_ONLY],
                                       /* 11n 2.4Ghz */
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_G_ONLY + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_G_ONLY],
                                       /* 11g 2.4Ghz*/
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_B_ONLY + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_B_ONLY],
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_5G) ? ANSI_COLOR_RED "(Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_AN + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_AN],
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_2G) ? ANSI_COLOR_RED " (Unsupported)" ANSI_NORMAL : "(Recommend)",
               WIFI_MODE_A_ONLY + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_A_ONLY],
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_2G) ? ANSI_COLOR_RED " (Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_N_ONLY_5G + GAP_USER_CONFIGURE_WIFI_MODE,
               wifi_mode_str[WIFI_MODE_N_ONLY_5G],
               cur_iface == WLAN1_IFACE && (e_band == WPA_SETBAND_2G) ? ANSI_COLOR_RED " (Unsupported)" ANSI_NORMAL : "",
               WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE,
               WIFI_MODE_N_ONLY_5G + GAP_USER_CONFIGURE_WIFI_MODE);

        input_num  = getNum();
        *wifi_mode = (unsigned char) input_num;

        if (input_num == RET_QUIT)
        {
            return E_QUIT;
        }
        else if (input_num == RET_DEFAULT)
        {
            *wifi_mode = WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE;
        }
    } while ((input_num < (WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE) ||
              input_num > (WIFI_MODE_N_ONLY_5G + GAP_USER_CONFIGURE_WIFI_MODE)) ||
             (e_band == WPA_SETBAND_5G && (input_num == (WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_GN + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_BG + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_G_ONLY + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_B_ONLY + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_N_ONLY + GAP_USER_CONFIGURE_WIFI_MODE))) ||
             (e_band == WPA_SETBAND_2G && (input_num == (WIFI_MODE_AN + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_A_ONLY + GAP_USER_CONFIGURE_WIFI_MODE) ||
                                           input_num == (WIFI_MODE_N_ONLY_5G + GAP_USER_CONFIGURE_WIFI_MODE))));

    return E_CONTINUE;
}

 #if CFG_PMGR
static void apply_pmgr (struct setup_params * params)
{
    unsigned char              enable_dpm           = params->enable_dpm;
    int                        dpm_keep_alive_time  = params->dpm_keepalive_time;
    int                        dpm_user_wakeup_time = params->dpm_user_wakeup_time;
    int                        dpm_TIM_wakeup_count = params->dpm_TIM_wakeup_count;
    int                        prev_user_wu_time    = 0;
    user_dpm_supp_ip_info_t  * dpm_ip_info;
    user_dpm_supp_net_info_t * dpm_netinfo;

    if (enable_dpm)
    {
        RM_PMGR_W_dpm_enable();

  #if defined(__SUPPORT_P2P__)

        /* DPM manages wifi PS mode with its own logic */
        WIFI_SetPsMode(false);
  #endif

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_USER_WAKEUP_TIME,
                                     &prev_user_wu_time);
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME,
                                      dpm_keep_alive_time);
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_DPM_USER_WAKEUP_TIME,
                                      dpm_user_wakeup_time);

        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE,
                                      WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT,
                                      dpm_TIM_wakeup_count);
  #endif

        /* DPM Keepalive time */
        RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_DPM_KEEPALIVE, dpm_keep_alive_time, 0);

        if (dpm_user_wakeup_time && (prev_user_wu_time <= 0))
        {
            RM_PMGR_W_dpm_user_wakeup_timer_init();
        }

        /* DPM TIM Wakeup Time */
        RM_WIFI_dpm_ptim_wakeup_count_set((unsigned int) dpm_TIM_wakeup_count, 0);

        if (params->netmode[WLAN0_IFACE] == STATIC_IP)
        {
            ip4_addr_t ipaddr_out, netmask_out, gateway_out, dns_out;

            if (!ip4addr_aton(params->ipaddress[WLAN0_IFACE], &ipaddr_out) ||
                !ip4addr_aton(params->subnetmask[WLAN0_IFACE], &netmask_out) ||
                !ip4addr_aton(params->gateway[WLAN0_IFACE], &gateway_out) ||
                !ip4addr_aton(params->dns[WLAN0_IFACE], &dns_out))
            {
                printf("[%s]: Invalid static IP configuration\n", __func__);

                return;
            }

            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_IP_INFO_PTR, NULL, NULL, (void **) (&dpm_ip_info));
            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **) (&dpm_netinfo));

            dpm_netinfo->net_mode        = STATIC_IP;
            dpm_ip_info->dpm_ip_addr     = ipaddr_out.addr;
            dpm_ip_info->dpm_netmask     = netmask_out.addr;
            dpm_ip_info->dpm_gateway     = gateway_out.addr;
            dpm_ip_info->dpm_dns_addr[0] = dns_out.addr;
            RM_WIFI_dpm_arp_filter_set(dpm_ip_info->dpm_ip_addr, dpm_ip_info->dpm_netmask);
        }
        else
        {
            RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_SUPPL_NET_INFO_PTR, NULL, NULL, (void **) (&dpm_netinfo));
            dpm_netinfo->net_mode = DHCPCLIENT;
        }

        if (!RM_PMGR_W_dpm_sleep_is_lld_task_running())
        {
            RM_PMGR_W_dpm_lld_task_start(RM_WIFI_dpm_ptim_event_get());
        }

        if (enable_dpm == 2)
        {
            RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
            printf("\nNote: Manually remove RAM constraint: \"pmgr remove_constraint PMGR_CONSTRAINT_POWER_RAM\"\n");
        }
    }
    else
    {
        RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);

        /* In TIN, dpm disable action at Easy setup should explicitly set dpm_mode to 0 */
        RM_PMGR_W_dpm_disable();
  #if defined(__SUPPORT_P2P__)

        /* Easy Setup disables WiFi PS mode by default */
        WIFI_SetPsMode(false);
  #endif
    }
}

 #endif                                /* CFG_PMGR */

static int parse_params (struct setup_params * params,
                         struct sntp_params  * sntp,
                         struct dhcp_params  * dhcp,
                         bool                  stop_services_needed,
                         bool                * mode_changed)
{
    unsigned char ret = 0;
    unsigned char e_pre_run_mode = (unsigned char) get_run_mode();
    unsigned char e_cur_iface, e_iface_start, e_iface_end;

    /* If wlan initialization is required */
    if (!ra6w1_network_main_get_wlaninit_mode())
    {
        ra6w1_network_main_set_wlaninit_mode(pdTRUE);

        if (!ra6w1_network_main_is_wlaninit())
        {
            ra6w1_network_main_init_wlan();
        }
    }

    if (stop_services_needed)
    {
        ret = setup_stop_services();

        switch (ret)
        {
            case E_ERROR:
            {
                goto CMD_ERROR;
            }

            case E_QUIT:
            {
                goto CMD_QUIT;
            }

            case E_CONTINUE:
            default:
            {
                break;
            }
        }
    }

    VT_CLEAR;                          /* Clear screen */
    VT_CURPOS(0, 0);                   /* move cursor position : 0,0 */
    printf("\n" ANSI_COLOR_BLACK ANSI_BCOLOR_YELLOW "[%s EASY SETUP - V%d]" ANSI_BCOLOR_DEFULT
           ANSI_COLOR_DEFULT "\n",
           CHIPSET_NAME,
           EASYSETUP_VERSION);

    ret = parse_country_code(params->country_code);

    if (ret == E_QUIT)
    {
        goto CMD_QUIT;
    }

WLAN_SETUP:
    params->wifi_mode = WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE;
    ret               = parse_sysmode(&params->sysmode);
    if (ret == E_QUIT)
    {
        goto CMD_QUIT;
    }

 #if defined(__SUPPORT_SETBAND_5GHZ__)
    ret = parse_band(params->sysmode, &params->band);
    if (ret == E_QUIT)
    {
        goto CMD_QUIT;
    }
 #endif                                /* __SUPPORT_SETBAND_5GHZ__ */

    /* select configuration Interface */
    setup_select_interface(params->sysmode, &e_iface_start, &e_iface_end);

    /* input configuration */
    for (e_cur_iface = e_iface_start; e_cur_iface <= e_iface_end; e_cur_iface++)
    {
        /* Print Title */
        setup_display_title(params->sysmode, e_cur_iface);

        /* SCAN SSID */
        if ((params->sysmode == WIFI_DEVICE_MODE_EXT_STATION) ||
            ((e_cur_iface == WLAN0_IFACE) && (params->sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION))
 #if defined(__SUPPORT_P2P__)
            || (e_cur_iface == WLAN0_IFACE && params->sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION)
 #endif                                /* __SUPPORT_P2P__ */
            )
        {
            ret = parse_scan_list(params, e_cur_iface, mode_changed);
            switch (ret)
            {
                case E_ERROR:
                {
                    goto CMD_ERROR;
                }

                case E_INPUT_SSID:
                {
                    goto INPUT_SSID;
                }

                case E_QUIT:
                {
                    goto CMD_QUIT;
                }

                case E_CONTINUE:
                default:
                {
                    break;
                }
            }
        }
        else
        {
 #if defined(__SUPPORT_P2P__)
            ret = parse_p2p_config(params->sysmode,
                                   e_cur_iface,
                                   &params->channel,
                                   &params->p2p_listen_chan,
                                   &params->p2p_go_intent,
                                   params->p2p_ssid_postfix);

            switch (ret)
            {
                case E_SETUP_WIFI_END:
                {
                    goto SETUP_WIFI_END;
                }

                case E_QUIT:
                {
                    goto CMD_QUIT;
                }

                case E_CONTINUE:
                default:
                {
                    break;
                }
            }
 #endif                                // __SUPPORT_P2P__

INPUT_SSID:
            ret = parse_ssid_input(params->ssid[e_cur_iface], e_cur_iface == WLAN1_IFACE);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            ret = parse_is_hidden_ssid(&params->hidden_ssid, e_cur_iface == WLAN0_IFACE);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            ret =
                parse_channel_input(params->sysmode, e_cur_iface, params->country_code, params->band, &params->channel);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            ret = parse_security_input(e_cur_iface, &params->security[e_cur_iface]);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }
        }

        ret = parse_eap_input(params->security[e_cur_iface], e_cur_iface, &params->eap_auth_mode, &params->eap_phase2);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

        ret = parse_enterprise_id_cert(params->security[e_cur_iface],
                                       e_cur_iface,
                                       params->eap_auth_mode,
                                       params->eap_phase2,
                                       params->eap_id,
                                       params->eap_pw);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

        ret = parse_psk_input(params->security[e_cur_iface], params->password[e_cur_iface]);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

        ret = parse_wep_key_input(params->security[e_cur_iface],
                                  e_cur_iface,
                                  params->wep_key,
                                  &params->wep_key_idx,
                                  &params->wep_key_type,
                                  &params->wep_bit);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

        if (set_advanced_defaults(params, e_cur_iface))
        {
            ret = get_yes_no("\n" ANSI_REVERSE " Do you want to set advanced WiFi configuration ?" ANSI_NORMAL
                             " [" ANSI_BOLD "N" ANSI_NORMAL "o/" ANSI_BOLD "Y" ANSI_NORMAL "es/" ANSI_BOLD "Q" ANSI_NORMAL "uit] (Default No) : ",
                             true);
            switch (ret)
            {
                case 0:
                case E_CONTINUE:
                {
                    goto SETUP_WIFI_END;
                }

                case E_QUIT:
                    goto CMD_QUIT;
            }
        }
        else                           /* No advanced options to set - skip */
        {
            goto SETUP_WIFI_END;
        }

 #if (defined __SUPPORT_WPA3_SAE__ && defined __SUPPORT_WPA3_PERSONAL__) || (defined __SUPPORT_MESH__)

        /* SAE Groups */
        if (((params->security[e_cur_iface] == eWiFiSecurityWPA3_ext) ||
             (params->security[e_cur_iface] == eWiFiSecurityWPA2_WPA3_ext)))
        {
            ret = setup_input_sae_group(params->sae_groups[e_cur_iface]);

            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }
        }
 #endif                                /* __SUPPORT_WPA3_SAE__ || __SUPPORT_MESH__ */

 #if defined __SUPPORT_IEEE80211W__
        ret = setup_pmf(params->security[e_cur_iface], &params->pmf[e_cur_iface]);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }
 #endif                                // __SUPPORT_IEEE80211W__

        ret = setup_wifimode(e_cur_iface, params->band, params->sysmode, &params->wifi_mode);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

SETUP_WIFI_END:
 #if CFG_PMGR
        if (params->sysmode == WIFI_DEVICE_MODE_EXT_STATION)
        {
            ret = parse_pmgr_input(params->sysmode,
                                   e_cur_iface,
                                   &params->enable_dpm,
                                   &params->dpm_keepalive_time,
                                   &params->dpm_user_wakeup_time,
                                   &params->dpm_TIM_wakeup_count);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }
        }
 #endif                                // CFG_PMGR

        setup_display_wlan_preset(params, e_cur_iface);

        ret = setup_wifi_config_confirm(params->sysmode, e_cur_iface);
        switch (ret)
        {
            case E_WLAN_SETUP:
            {
                goto WLAN_SETUP;
            }

            case E_SETUP_APPLY_START:
            {
                goto SETUP_APPLY_START;
            }

            case E_QUIT:
            {
                goto CMD_QUIT;
            }

            case E_CONTINUE:
            default:
            {
                break;
            }
        }

        /* --- NETWORK --- */
 #ifdef __SUPPORT_IPV4__
NETWORK_SETUP:

        ret = setup_netmode(params->sysmode, e_cur_iface, &params->netmode[e_cur_iface]);
        if (ret == E_QUIT)
        {
            goto CMD_QUIT;
        }

        /* Static IP */
        if (params->netmode[e_cur_iface] == E_NETMODE_STATIC_IP)
        {
INPUT_IPADDRESS:

            /*  IP address*/
            ret = setup_static_ip(e_cur_iface, params->ipaddress[e_cur_iface]);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            /* Subnet mask */
            ret = setup_static_subnet(e_cur_iface, params->subnetmask[e_cur_iface]);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            /* Gateway */
            ret =
                setup_static_gateway(params->sysmode, e_cur_iface, params->ipaddress[e_cur_iface],
                                     params->subnetmask[e_cur_iface], params->gateway[e_cur_iface]);
            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            /* DNS */
            ret = setup_static_dns(params->sysmode, e_cur_iface, params->dns[e_cur_iface]);

            if (ret == E_QUIT)
            {
                goto CMD_QUIT;
            }

            /* Display config stats */
            setup_display_network_preset(e_cur_iface,
                                         params->sysmode,
                                         params->ipaddress[e_cur_iface],
                                         params->subnetmask[e_cur_iface],
                                         params->gateway[e_cur_iface],
                                         params->dns[e_cur_iface]);

            ret =
                setup_check_network(e_cur_iface, params->sysmode, params->ipaddress[e_cur_iface],
                                    params->subnetmask[e_cur_iface], params->gateway[e_cur_iface]);

            switch (ret)
            {
                case E_NETWORK_SETUP:
                {
                    goto NETWORK_SETUP;
                }

                case E_INPUT_IPADDRESS:
                {
                    goto INPUT_IPADDRESS;
                }

                case E_QUIT:
                {
                    goto CMD_QUIT;
                }

                case E_CONTINUE:
                default:
                {
                    break;
                }
            }
        }
        else
        {
  #ifdef __SUPPORT_DHCP_SVR__
            dhcp->use_dhcps = 0;
  #endif                               /*__SUPPORT_DHCP_SVR__*/
        }

        ret = setup_network_config_confirm(params->sysmode);

        switch (ret)
        {
            case E_NETWORK_SETUP:
            {
                goto NETWORK_SETUP;
            }

            case E_INPUT_IPADDRESS:
            {
                goto INPUT_IPADDRESS;
            }

            case E_QUIT:
            {
                goto CMD_QUIT;
            }

            case E_CONTINUE:
            default:
            {
                break;
            }
        }
 #endif                                // __SUPPORT_IPV4__

        /* SNTP */
        if (is_sntp_supported_sysmode(params->sysmode))
        {
            if (is_sntp_configurable_mode(params->sysmode, e_cur_iface))
            {
                ret = setup_sntp_client(sntp);
                if (ret == E_QUIT)
                {
                    goto CMD_QUIT;
                }
            }
            else if (sntp->sntp_client != E_SNTP_CLIENT_START)
            {
                /* In case SNTP client isn't configured yet,
                 * change the value as E_SNTP_CLIENT_STOP */
                sntp->sntp_client = E_SNTP_CLIENT_STOP;
            }
        }

 #ifdef __SUPPORT_IPV4__
  #ifdef __SUPPORT_DHCP_SVR__
        setup_dhcp_server(params->sysmode,
                          e_cur_iface,
                          params->ipaddress[e_cur_iface],
                          params->subnetmask[e_cur_iface],
                          params->gateway[e_cur_iface],
                          params->dns[e_cur_iface],
                          &dhcp->use_dhcps,
                          dhcp->dhcp_lease_start,
                          dhcp->dhcp_lease_end,
                          &dhcp->dhcp_lease_time,
                          &dhcp->dhcp_lease_count);
  #endif                               /* __SUPPORT_DHCP_SVR__ */
 #endif                                /* __SUPPORT_IPV4__ */
    }

SETUP_APPLY_START:

    return 0;

CMD_ERROR:
    printf("\nSETUP: ERR(ret=%d) Please try again after factory reset!\n", ret);
CMD_QUIT:

    printf("\n" ANSI_BOLD ANSI_COLOR_RED
           "All services have been stopped.\nPlease reboot or setup again." ANSI_NORMAL ANSI_COLOR_DEFULT "\n\n");

    vTaskDelay(portCONVERT_MS_2_TICKS(100));

    set_run_mode(e_pre_run_mode);

    return ret;
}

static bool apply_static_ip (char * p_ipaddr_str,
                             char * p_netmask_str,
                             char * p_gateway_str,
                             char * p_dns_str,
                             int    iface_index)
{
    struct netif * iface = WIFI_GetNetIf(iface_index);
    ip4_addr_t     ipaddr, netmask, gateway, dns;
    extern int     netmode[];
 #if CFG_PMGR
    RM_PMGR_W_dpm_job_name_clear(NET_IFCONFIG);
 #endif

    /* CONFIG NETMODE */
    netmode[iface_index] = STATIC_IP;

    if (!iface)
    {
        printf("%s: Invalid interface index: %d\n", __func__, iface_index);

        return false;
    }

    if (!ipaddr_aton(p_ipaddr_str, (ip_addr_t *) &ipaddr) ||
        !ipaddr_aton(p_netmask_str, (ip_addr_t *) &netmask) ||
        !ipaddr_aton(p_gateway_str, (ip_addr_t *) &gateway))
    {
        return false;
    }

    netif_set_addr(iface, &ipaddr, &netmask, &gateway);
    netif_set_up(iface);

    if (strlen(p_dns_str) == 0)
    {
        p_dns_str = DEFAULT_DNS_WLAN0;
    }

    if (p_dns_str && ipaddr_aton(p_dns_str, (ip_addr_t *) &dns))
    {
        dns_setserver(0, (ip_addr_t *) &dns);
    }
    else
    {
        return false;
    }

    return true;
}

static bool apply_dhcp_client (int iface_index)
{
    struct netif * iface = WIFI_GetNetIf(iface_index);
    ip4_addr_t     ipaddr, netmask, gateway, dns;
    extern int     netmode[];
 #if CFG_PMGR
    RM_PMGR_W_dpm_job_name_clear(NET_IFCONFIG);
 #endif

    /* CONFIG NETMODE */
    netmode[iface_index] = DHCPCLIENT;

    if (!iface)
    {
        printf("%s: Invalid interface index: %d\n", __func__, iface_index);

        return false;
    }

    if (!ipaddr_aton("0.0.0.0", (ip_addr_t *) &ipaddr) ||
        !ipaddr_aton("0.0.0.0", (ip_addr_t *) &netmask) ||
        !ipaddr_aton("0.0.0.0", (ip_addr_t *) &gateway))
    {
        return false;
    }

    netif_set_addr(iface, &ipaddr, &netmask, &gateway);
    netif_set_up(iface);

    if (ipaddr_aton("0.0.0.0", (ip_addr_t *) &dns))
    {
        dns_setserver(0, (ip_addr_t *) &dns);
        dns_setserver(1, (ip_addr_t *) &dns);
    }
    else
    {
        return false;
    }

    return true;
}

static int apply_ent_cert_key (WIFINetworkParamsExt_t * net_params)
{
    rm_cert_format_t format = RM_CERT_FORMAT_PEM;
    size_t           outlen = CERT_MAX_LENGTH;
    int              status = 0;

    memset(net_params->xEntNetParams.ucCert, 0xff, CERT_MAX_LENGTH);

    status =
        RM_CERT_Read(RM_CERT_GetModule(SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR),
                     RM_CERT_GetType(SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR),
                     &format,
                     (unsigned char *) net_params->xEntNetParams.ucCert,
                     &outlen);

    if (((UCHAR) (net_params->xEntNetParams.ucCert[0]) != 0xFF) && (status == 0))
    {
        net_params->xEntNetParams.ucCertLength = outlen;
        outlen = CERT_MAX_LENGTH;
        memset(net_params->xEntNetParams.ucPrivKey, 0xff, CERT_MAX_LENGTH);
        status =
            RM_CERT_Read(RM_CERT_GetModule(SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR),
                         RM_CERT_GetType(SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR),
                         &format,
                         (unsigned char *) net_params->xEntNetParams.ucPrivKey,
                         &outlen);

        if (((UCHAR) (net_params->xEntNetParams.ucPrivKey[0]) != 0xFF) && (status == 0))
        {
            net_params->xEntNetParams.ucPrivKeyLength = outlen;

            return pdPASS;
        }
        else
        {
            net_params->xEntNetParams.ucCertLength = 0;
            printf("%s: Applying private key failed\n", __func__);

            return pdFAIL;
        }
    }
    else
    {
        printf("%s: Applying certificate failed\n", __func__);

        return pdFAIL;
    }
}

static void apply_sta (struct setup_params * params)
{
    WIFIReturnCode_t         wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!net_params)
    {
        return;
    }

    memset(net_params, 0, sizeof(WIFINetworkParamsExt_t));

    net_params->xNetworkParams.ucSSIDLength = strlen(params->ssid[WLAN0_IFACE]);
    memcpy(net_params->xNetworkParams.ucSSID, params->ssid[WLAN0_IFACE], net_params->xNetworkParams.ucSSIDLength);

    net_params->hidden_ssid              = params->hidden_ssid;
    net_params->pmf                      = params->pmf[WLAN0_IFACE];
    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) params->security[WLAN0_IFACE];

    if (params->band == WPA_SETBAND_2G)
    {
        net_params->ucBand = eWiFiBand2G;
    }
    else if (params->band == WPA_SETBAND_5G)
    {
        net_params->ucBand = eWiFiBand5G;
    }
    else
    {
        net_params->ucBand = eWiFiBandDual;
    }

    if ((net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA) ||
        (net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA2) ||
        (net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA3) ||
        (params->security[WLAN0_IFACE] == eWiFiSecurityWPA_WPA2_ext) ||
        (params->security[WLAN0_IFACE] == eWiFiSecurityWPA2_WPA3_ext))
    {
        net_params->xNetworkParams.xPassword.xWPA.ucLength = strlen(params->password[WLAN0_IFACE]);
        memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
               params->password[WLAN0_IFACE],
               net_params->xNetworkParams.xPassword.xWPA.ucLength);

        if ((net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA3) ||
            (params->security[WLAN0_IFACE] == eWiFiSecurityWPA2_WPA3_ext))
        {
            bsp_safe_strcpy(net_params->sae_groups, params->sae_groups[WLAN0_IFACE], sizeof(net_params->sae_groups));
        }
    }
    else if (net_params->xNetworkParams.xSecurity == eWiFiSecurityWEP)
    {
        net_params->xNetworkParams.xPassword.xWEP[0].ucLength = strlen(params->wep_key);
        memcpy(net_params->xNetworkParams.xPassword.xWEP[0].cKey,
               params->wep_key,
               net_params->xNetworkParams.xPassword.xWEP[0].ucLength);
        net_params->xNetworkParams.ucDefaultWEPKeyIndex = params->wep_key_idx;
    }
    else if ((net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA2_ent) ||
             (params->security[WLAN0_IFACE] == eWiFiSecurityWPA_ent_ext) ||
             (params->security[WLAN0_IFACE] == eWiFiSecurityWPA_WPA2_ent_ext) ||
             (params->security[WLAN0_IFACE] == eWiFiSecurityWPA2_WPA3_ent_ext) ||
             (params->security[WLAN0_IFACE] == eWiFiSecurityWPA3_ent_ext) ||
             (params->security[WLAN0_IFACE] == eWiFiSecurityWPA3_192B_ent_ext))
    {
        net_params->xEntNetParams.ucEntAuthType  = params->eap_auth_mode;
        net_params->xEntNetParams.ucEntAuthProto = params->eap_phase2;

        net_params->xEntNetParams.ucIDLength = strlen(params->eap_id);
        memcpy(net_params->xEntNetParams.ucID, params->eap_id, net_params->xEntNetParams.ucIDLength);

        net_params->xEntNetParams.ucPasswordLength = strlen(params->eap_pw);
        memcpy(net_params->xEntNetParams.ucPassword, params->eap_pw, net_params->xEntNetParams.ucPasswordLength);

        if ((params->eap_auth_mode == E_EAP_AUTH_MODE_TLS) || (params->eap_phase2 == E_EAP_PHASE2_MODE_TLS))
        {
            if (apply_ent_cert_key(net_params) == pdFAIL)
            {
                printf("%s: Applying enterprise params failed\n", __func__);
            }
        }
    }

    /* Populate channel list from params (loaded via load_params or set during this session) */
    if (params->channel_number > 0)
    {
        /* Allocate and copy channel list from params */
        uint32_t * channel_list = pvPortMalloc(params->channel_number * sizeof(uint32_t));

        if (channel_list)
        {
            memcpy(channel_list, params->channel_list, params->channel_number * sizeof(uint32_t));

            net_params->ucNumChannels  = params->channel_number;
            net_params->pucChannelList = channel_list;
        }
        else
        {
            net_params->ucNumChannels  = 0;
            net_params->pucChannelList = NULL;
        }
    }
    else
    {
        net_params->ucNumChannels  = 0;
        net_params->pucChannelList = NULL;
    }

    /* Connect to the Access Point */
    wifi_err = WIFI_ConnectAPExt(net_params);

    if (wifi_err)
    {
        printf("%s: WIFI_ConnectAP failed with wifi_err=%d\n", __func__, wifi_err);
    }

    /* Enable DHCP client in STA mode */
    if (params->netmode[WLAN0_IFACE] == E_NETMODE_DYNAMIC_IP)
    {
        int            err;
        struct netif * iface_sta = WIFI_GetNetIf(WLAN0_IFACE);

        if (WIFI_IsConnected((const WIFINetworkParams_t *) net_params) == eWiFiSuccess)
        {
            err = dhcp_start(iface_sta);
            if (err)
            {
                printf("%s: dhcp_client start failed with wifi_err=%d\n", __func__, err);
            }
        }
    }

    if (net_params->pucChannelList)
    {
        vPortFree(net_params->pucChannelList);
        net_params->pucChannelList = NULL;
    }

    vPortFree(net_params);
}

static void apply_ap (struct setup_params * p_params)
{
    WIFIReturnCode_t         wifi_err;
    WIFINetworkParamsExt_t * net_params = NULL;

    if (!p_params)
    {
        return;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!net_params)
    {
        return;
    }

    memset(net_params, 0, sizeof(WIFINetworkParamsExt_t));
    net_params->xNetworkParams.ucChannel = p_params->channel;

    net_params->xNetworkParams.ucSSIDLength = strlen(p_params->ssid[WLAN1_IFACE]);
    memcpy(net_params->xNetworkParams.ucSSID, p_params->ssid[WLAN1_IFACE], net_params->xNetworkParams.ucSSIDLength);

    net_params->pmf = p_params->pmf[WLAN1_IFACE];

    /* Casting only for solve compilation error */
    net_params->xNetworkParams.xSecurity = (WIFISecurity_t) p_params->security[WLAN1_IFACE];

    if (!(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA_ent_ext) &&
        !(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA2_ent_ext) &&
        !(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA_WPA2_ent_ext) &&
        !(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA2_WPA3_ent_ext) &&
        !(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA3_ent_ext) &&
        !(p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA3_192B_ent_ext))
    {
        net_params->xNetworkParams.xPassword.xWPA.ucLength = strlen(p_params->password[WLAN1_IFACE]);
        memcpy(net_params->xNetworkParams.xPassword.xWPA.cPassphrase,
               p_params->password[WLAN1_IFACE],
               net_params->xNetworkParams.xPassword.xWPA.ucLength);
    }

    if (p_params->band == WPA_SETBAND_2G)
    {
        net_params->ucBand = eWiFiBand2G;
    }
    else if (p_params->band == WPA_SETBAND_5G)
    {
        net_params->ucBand = eWiFiBand5G;
    }
    else
    {
        net_params->ucBand = eWiFiBandDual;
    }

    net_params->ucWiFi_mode = p_params->wifi_mode - GAP_USER_CONFIGURE_WIFI_MODE;

    if ((p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA3_ext) ||
        (p_params->security[WLAN1_IFACE] == eWiFiSecurityWPA2_WPA3_ext))
    {
        bsp_safe_strcpy(net_params->sae_groups, p_params->sae_groups[WLAN1_IFACE], sizeof(net_params->sae_groups));
    }

    net_params->xApNetParams.ap_max_inactivity = p_params->ap_max_inactivity;

    if (p_params->ap_enc_mode == -1)
    {
        net_params->xApNetParams.ucEncMode = eWiFiEncryptionNone;
    }
    else
    {
        net_params->xApNetParams.ucEncMode = p_params->ap_enc_mode;
    }

    wifi_err = WIFI_ConfigureAPExt(net_params);

    if (wifi_err)
    {
        printf("%s: WIFI_ConfigureAPExt failed with wifi_err=%d\n", __func__, wifi_err);
    }

    wifi_err = WIFI_StartAP();
    if (wifi_err)
    {
        printf("%s: WIFI_StartAP failed with wifi_err=%d\n", __func__, wifi_err);
    }
    else
    {
        /* Set ap_wmm_enabled after SoftAP interface is started */
        if ((p_params->ap_wmm_enabled == 0) || (p_params->ap_wmm_enabled == 1))
        {
            WIFI_WMM(p_params->ap_wmm_enabled);
        }

        /* Set ap_wmm_ps_enabled after SoftAP interface is started */
        if ((p_params->ap_wmm_ps_enabled == 0) || (p_params->ap_wmm_ps_enabled == 1))
        {
            WIFI_WMM_PS(p_params->ap_wmm_ps_enabled);
        }
    }

    if (net_params)
    {
        vPortFree(net_params);
        net_params = NULL;
    }
}

static void apply_p2p (unsigned char channel,
                       unsigned char p2p_listen_chan,
                       unsigned char p2p_go_intent,
                       const char  * p2p_ssid_postfix)
{
    WIFIReturnCode_t wifi_err;
    wifi_p2p_ext     p2p_params = {0};

    p2p_params.oper_chan      = channel;
    p2p_params.listen_chan    = p2p_listen_chan;
    p2p_params.go_intent_chan = p2p_go_intent;
    bsp_safe_strcpy(p2p_params.ssid_postfix, p2p_ssid_postfix, sizeof(p2p_params.ssid_postfix));
    wifi_err = WIFI_SetupP2P(&p2p_params);
    if (wifi_err)
    {
        printf("%s: WIFI_SetupP2P failed failed with wifi_err=%d\n", __func__, wifi_err);
    }
}

static void apply_sntp (unsigned char sntp_client,
                        int           client_period_time,
                        int           timezone_int,
                        char        * p_svr_addr,
                        char        * p_svr_addr1,
                        char        * p_svr_addr2)
{
    long temp;

    if (sntp_client == 0)
    {
        return;
    }

    if (sntp_client == E_SNTP_CLIENT_START)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_SYSCFG,
                                      NVR_KEY_SNTP_SYNC_PERIOD,
                                      client_period_time);
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN,
                                         p_svr_addr);
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_1,
                                         p_svr_addr1);
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_2,
                                         p_svr_addr2);
 #endif

        temp = (timezone_int / 60) * 60;
        if (timezone_int != 0)
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          NVR_KEY_TIMEZONE,
                                          timezone_int);
 #endif
            R_RTC_W_CalendarTimeZoneSet(R_RTC_W_GetCtrl(), &temp);
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_TIMEZONE);
 #endif
        }

        /* Set run flag */
        set_sntp_use(1);
        start_sntp();
    }
    else
    {
        /* Clear run flag and disable */
        set_sntp_use(0);
    }
}

static void apply_dhcp (char        * p_dns,
                        unsigned char use_dhcps,
                        char        * p_dhcp_lease_start,
                        char        * p_dhcp_lease_end,
                        int           dhcp_lease_time,
                        int           dhcp_lease_count)
{
 #if (LWIP_DHCPS && LWIP_IPV4)
    dhcps_cmd_param * dhcps_param = NULL;
 #endif                                /*LWIP_DHCPS*/

    /* Always stop the DHCP server at the start */
 #if (LWIP_DHCPS && LWIP_IPV4)
    dhcps_param = pvPortMalloc(sizeof(dhcps_cmd_param));
    if (!dhcps_param)
    {
        printf("[%s] Failed to allocate memory for dhcps_param\n", __func__);

        return;
    }

    memset(dhcps_param, 0, sizeof(dhcps_cmd_param));
    dhcps_param->cmd             = DHCP_SERVER_STATE_STOP;
    dhcps_param->dhcps_interface = WLAN1_IFACE;
    printf("Stop DHCP Server\n");

    /* Note: dhcps_run will take ownership of dhcps_param and handle cleanup. */
    dhcps_run(dhcps_param);
 #endif                                /*LWIP_DHCPS*/

    if (use_dhcps == CC_VAL_ENABLE)
    {
        if (!is_in_valid_ip_class(p_dhcp_lease_start) ||
            !is_in_valid_ip_class(p_dhcp_lease_end) ||
            (dhcp_lease_time <= 0))
        {
            printf("ERR: Invalid DHCP configuration\n");

            return;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS, p_dns);
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         DHCP_SERVER_START_IP,
                                         p_dhcp_lease_start);
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         DHCP_SERVER_END_IP,
                                         p_dhcp_lease_end);
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_SYSCFG,
                                      DHCP_SERVER_LEASE_TIME,
                                      dhcp_lease_time);
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, pdTRUE);
 #endif

        /* DHCP Server boot Start */
 #if (LWIP_DHCPS && LWIP_IPV4)
        dhcps_param = pvPortMalloc(sizeof(dhcps_cmd_param));
        if (!dhcps_param)
        {
            printf("[%s] Failed to allocate memory for dhcps_param\n", __func__);

            return;
        }

        memset(dhcps_param, 0, sizeof(dhcps_cmd_param));
        dhcps_param->cmd             = DHCP_SERVER_STATE_START;
        dhcps_param->dhcps_interface = WLAN1_IFACE;

        printf("Start DHCP Server\n");
        dhcps_run(dhcps_param);
 #endif                                /* LWIP_DHCPS */
    }
    else
    {
        /* Clear configuration if DHCP is disabled */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD);
 #endif
    }
}

/* Helper for Station: parse IP addresses according to netmode */
static bool configure_sta_netparams (struct setup_params * params,
                                     int                   iface_index,
                                     ip4_addr_t          * ipaddr_out,
                                     ip4_addr_t          * netmask_out,
                                     ip4_addr_t          * gateway_out,
                                     ip4_addr_t          * dns_out)
{
    const char * p_dns_str = params->dns[iface_index];

    if (!params || !ipaddr_out || !netmask_out || !gateway_out || !dns_out)
    {
        printf("[%s]: Invalid arguments\n", __func__);

        return false;
    }

    if (params->netmode[iface_index] == E_NETMODE_DYNAMIC_IP)
    {
        /* For DHCP, leave addresses zero */
        ip_addr_set_zero((ip_addr_t *) ipaddr_out);
        ip_addr_set_zero((ip_addr_t *) netmask_out);
        ip_addr_set_zero((ip_addr_t *) gateway_out);
        ip_addr_set_zero((ip_addr_t *) dns_out);
    }
    else if (params->netmode[iface_index] == STATIC_IP)
    {
        if (!ipaddr_aton(params->ipaddress[iface_index], (ip_addr_t *) ipaddr_out) ||
            !ipaddr_aton(params->subnetmask[iface_index], (ip_addr_t *) netmask_out) ||
            !ipaddr_aton(params->gateway[iface_index], (ip_addr_t *) gateway_out))
        {
            printf("[%s]: Invalid static IP configuration\n", __func__);

            return false;
        }

        if (!p_dns_str || (strlen(p_dns_str) == 0))
        {
            p_dns_str = DEFAULT_DNS_WLAN0;
        }

        if (!ipaddr_aton(p_dns_str, (ip_addr_t *) dns_out))
        {
            printf("[%s]: Invalid DNS string: %s\n", __func__, p_dns_str);

            return false;
        }

        dns_setserver(0, (ip_addr_t *) dns_out);

        /* If you want to set a second DNS, you can do so similarly: */
        ip_addr_set_zero((ip_addr_t *) dns_out);
        dns_setserver(2, (ip_addr_t *) dns_out);
    }

    return true;
}

/* Helper for AP: parse IP addresses */
static bool configure_ap_netparams (struct setup_params * params,
                                    int                   iface_index,
                                    ip4_addr_t          * ipaddr_out,
                                    ip4_addr_t          * netmask_out,
                                    ip4_addr_t          * gateway_out,
                                    ip4_addr_t          * dns_out)
{
    const char * p_dns_str = params->dns[iface_index];

    if (!params || !ipaddr_out || !netmask_out || !gateway_out || !dns_out)
    {
        printf("[%s]: Invalid arguments\n", __func__);

        return false;
    }

    if (!ipaddr_aton(params->ipaddress[iface_index], (ip_addr_t *) ipaddr_out) ||
        !ipaddr_aton(params->subnetmask[iface_index], (ip_addr_t *) netmask_out) ||
        !ipaddr_aton(params->gateway[iface_index], (ip_addr_t *) gateway_out))
    {
        printf("[%s]: Invalid IP settings for AP mode\n", __func__);

        return false;
    }

    if (!p_dns_str || (strlen(p_dns_str) == 0))
    {
        p_dns_str = DEFAULT_DNS_WLAN1;
    }

    if (ipaddr_aton(p_dns_str, (ip_addr_t *) dns_out))
    {
        dns_setserver(0, (ip_addr_t *) dns_out);
    }

    return true;
}

static bool configure_p2p_netparams (ip4_addr_t * ipaddr_out,
                                     ip4_addr_t * netmask_out,
                                     ip4_addr_t * gateway_out,
                                     ip4_addr_t * dns_out)
{
    const char * p_ipaddr_str  = IPADDR_ANY_STR;
    const char * p_netmask_str = IPADDR_ANY_STR;
    const char * p_gateway_str = IPADDR_ANY_STR;
    const char * p_dns_str     = DEFAULT_DNS_WLAN1;

    if (!ipaddr_aton(p_ipaddr_str, (ip_addr_t *) ipaddr_out) ||
        !ipaddr_aton(p_netmask_str, (ip_addr_t *) netmask_out) ||
        !ipaddr_aton(p_gateway_str, (ip_addr_t *) gateway_out))
    {
        printf("[%s]: Invalid IP settings for P2P mode\n", __func__);

        return false;
    }

    if (ipaddr_aton(p_dns_str, (ip_addr_t *) dns_out))
    {
        dns_setserver(0, (ip_addr_t *) dns_out);
    }

    return true;
}

static bool is_netif_in_list (struct netif * netif)
{
    struct netif * current_netif;

    for (current_netif = netif_list; current_netif != NULL; current_netif = current_netif->next)
    {
        if (current_netif == netif)
        {

            /* netif is in the list */
            return true;
        }
    }

    /* netif is not in the list */
    return false;
}

static bool init_net_ifaces (struct setup_params * params)
{
    ip4_addr_t     ipaddr_wlan0, netmask_wlan0, gateway_wlan0, dns_wlan0;
    ip4_addr_t     ipaddr_wlan1, netmask_wlan1, gateway_wlan1, dns_wlan1;
    struct netif * wlan0_iface = WIFI_GetNetIf(WLAN0_IFACE);
    struct netif * wlan1_iface = WIFI_GetNetIf(WLAN1_IFACE);

    if ((!params) || (!wlan0_iface) || (!wlan1_iface))
    {
        return false;
    }

    /* Bring down both interfaces */
    netif_set_down(wlan0_iface);
    netif_set_down(wlan1_iface);
    clear_netInit_flag();

    printf("[%s]: Switching WiFi mode to: %s\n", __func__, wifi_mode_names[params->sysmode]);

    switch (params->sysmode)
    {
        case WIFI_DEVICE_MODE_EXT_STATION:
        {
            printf("[%s]: Setting up STA (wlan0)\n", __func__);

            if (!configure_sta_netparams(params, WLAN0_IFACE, &ipaddr_wlan0, &netmask_wlan0, &gateway_wlan0,
                                         &dns_wlan0))
            {
                printf("[%s]: STA netparam config failed\n", __func__);

                return false;
            }

            netif_set_addr(wlan0_iface, &ipaddr_wlan0, &netmask_wlan0, &gateway_wlan0);
            set_netInit_flag(WLAN0_IFACE);
            netif_set_default(wlan0_iface);

            break;
        }

        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
        {
            printf("[%s]: Setting up P2P STATION (wlan0)\n", __func__);

            if (!configure_p2p_netparams(&ipaddr_wlan0, &netmask_wlan0, &gateway_wlan0, &dns_wlan0))
            {
                printf("[%s]: P2P STATION netparam config failed\n", __func__);

                return false;
            }

            netif_set_addr(wlan0_iface, &ipaddr_wlan0, &netmask_wlan0, &gateway_wlan0);
            set_netInit_flag(WLAN0_IFACE);
            netif_set_default(wlan0_iface);

            break;
        }

        case WIFI_DEVICE_MODE_EXT_AP:
        {
            printf("[%s]: Setting up AP (wlan1)\n", __func__);

            if (!configure_ap_netparams(params, WLAN1_IFACE, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, &dns_wlan1))
            {
                printf("[%s]: Invalid IP settings for AP mode\n", __func__);

                return false;
            }

            if (!is_netif_in_list(wlan1_iface))
            {
                if (netif_add(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, NULL, ethernetif_init,
                              ethernet_input) == NULL)
                {
                    printf("[%s]: Failed to initialize AP interface\n", __func__);

                    return false;
                }
            }
            else
            {
                netif_set_addr(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1);
            }

            set_netInit_flag(WLAN1_IFACE);
            netif_set_default(wlan1_iface);
            netif_set_down(wlan0_iface);

            break;
        }

        case WIFI_DEVICE_MODE_EXT_P2P:
        case WIFI_DEVICE_MODE_EXT_P2P_GO:
        {
            printf("[%s]: Setting up P2P (wlan1)\n", __func__);

            if (!configure_p2p_netparams(&ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, &dns_wlan1))
            {
                printf("[%s]: Invalid IP settings for P2P mode\n", __func__);

                return false;
            }

            if (!is_netif_in_list(wlan1_iface))
            {
                if (netif_add(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, NULL, ethernetif_init,
                              ethernet_input) == NULL)
                {
                    printf("[%s]: Failed to initialize P2P interface\n", __func__);

                    return false;
                }
            }
            else
            {
                netif_set_addr(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1);
            }

            set_netInit_flag(WLAN1_IFACE);
            netif_set_default(wlan1_iface);
            netif_set_down(wlan0_iface);

            break;
        }

        case WIFI_DEVICE_MODE_EXT_AP_STATION:
        case WIFI_DEVICE_MODE_EXT_MESH_POINT:
        {
            printf("[%s]: Setting up Concurrent mode (AP + STA)\n", __func__);

            /* For STA (wlan0) */
            if (!configure_sta_netparams(params, WLAN0_IFACE, &ipaddr_wlan0, &netmask_wlan0, &gateway_wlan0,
                                         &dns_wlan0))
            {
                printf("[%s]: STA netparam config failed\n", __func__);

                return false;
            }

            set_netInit_flag(WLAN0_IFACE);

            /* For AP (wlan1) */
            if (!configure_ap_netparams(params, WLAN1_IFACE, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, &dns_wlan1))
            {
                printf("[%s]: Invalid IP settings for AP mode\n", __func__);

                return false;
            }

            if (!is_netif_in_list(wlan1_iface))
            {
                if (netif_add(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1, NULL, ethernetif_init,
                              ethernet_input) == NULL)
                {
                    printf("[%s]: Failed to initialize AP interface\n", __func__);

                    return false;
                }
            }
            else
            {
                netif_set_addr(wlan1_iface, &ipaddr_wlan1, &netmask_wlan1, &gateway_wlan1);
            }

            set_netInit_flag(WLAN1_IFACE);
            netif_set_default(wlan1_iface);

            break;
        }

        case WIFI_DEVICE_MODE_EXT_NOT_SUPPORTED:
        default:
            printf("[%s]: Unsupported sysmode = %d\n", __func__, params->sysmode);

            return false;
    }

    return true;
}

static WIFIReturnCode_t apply_country_code (void)
{
    char             nv_country_code[4] = {0};
    char           * result_ptr         = NULL;
    WIFIReturnCode_t ret                = 0;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_COUNTRY_CODE,
                                    &result_ptr);

    if (result_ptr)
    {
        bsp_safe_strcpy(nv_country_code, result_ptr, sizeof(nv_country_code));
    }
    else
 #endif
    {
        bsp_safe_strcpy(nv_country_code, COUNTRY_CODE_DEFAULT, sizeof(nv_country_code));
    }

    ret = WIFI_SetCountryCode(nv_country_code);
    if (ret)
    {
        printf("%s: WIFI_SetCountryCode failed with wifi_err=%d\n", __func__, ret);
    }

    return ret;
}

static int apply_band (void)
{
    int  band;
    char band_cmd[32] = {0};
    char reply[16]    = {0};

 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                     (int *) &band) != FSP_SUCCESS)
    {
        band = WPA_SETBAND_AUTO;
    }
 #endif

    if (band == WPA_SETBAND_2G)
    {
        sprintf(band_cmd, "set setband %s", "2G");
    }
    else if (band == WPA_SETBAND_5G)
    {
        sprintf(band_cmd, "set setband %s", "5G");
    }
    else                               /* WPA_SETBAND_AUTO */
    {
        sprintf(band_cmd, "set setband AUTO");
    }

    ra6w1_cli_reply(band_cmd, NULL, reply);

    if (strcmp(reply, "OK"))
    {
        return 1;
    }

    return 0;
}

static int apply_mandatory_params (void)
{
    int ret = 0;

    ret = apply_country_code();
    ret = apply_band();

    return ret;
}

static void apply_params (struct setup_params * params,
                          struct sntp_params  * sntp,
                          struct dhcp_params  * dhcp,
                          bool                  mode_changed)
{
    WIFIReturnCode_t         wifi_err;
    e_wifi_device_mode_ext_t prev_sysmode;

    if (!mode_changed)
    {
        wifi_err = WIFI_GetModeExt(&prev_sysmode);
        if (wifi_err)
        {
            printf("%s: WIFI_GetMode failed with wifi_err=%d\n", __func__, wifi_err);
        }

        wifi_err = WIFI_SetModeExt(params->sysmode);
        if (wifi_err)
        {
            printf("%s: WIFI_SetModeExt failed with wifi_err=%d\n", __func__, wifi_err);
        }
    }

    if (mode_changed || (prev_sysmode != params->sysmode))
    {
        if (!init_net_ifaces(params))
        {
            printf("%s: init_net_ifaces failed!\n", __func__);
        }
    }

    wifi_err = WIFI_SetCountryCode(params->country_code);
    if (wifi_err)
    {
        printf("%s: WIFI_SetCountryCode failed with wifi_err=%d\n", __func__, wifi_err);
    }

 #if CFG_PMGR

    /* DPM MODE */
    if (params->sysmode == WIFI_DEVICE_MODE_EXT_STATION)
    {
        apply_pmgr(params);
    }
 #endif                                /* CFG_PMGR */

    for (unsigned char e_cur_iface = 0; e_cur_iface < 2; e_cur_iface++)
    {
        if (params->netmode[e_cur_iface] == E_NETMODE_STATIC_IP)
        {
            if (!apply_static_ip(params->ipaddress[e_cur_iface], params->subnetmask[e_cur_iface],
                                 params->gateway[e_cur_iface], params->dns[e_cur_iface], e_cur_iface))
            {
                printf("%s: wlan%d apply_static_ip failed\n", __func__, e_cur_iface);
            }
        }
        else if ((e_cur_iface == WLAN0_IFACE) && (params->netmode[e_cur_iface] == E_NETMODE_DYNAMIC_IP)) // DHCP Client
        {
            if (!apply_dhcp_client(WLAN0_IFACE))
            {
                printf("%s: wlan%d apply_dhcp_client failed\n", __func__, e_cur_iface);
            }
        }
    }

    if (params->sysmode == WIFI_DEVICE_MODE_EXT_STATION)
    {
        apply_sntp(sntp->sntp_client,
                   sntp->sntp_client_period_time,
                   sntp->sntp_timezone_int,
                   sntp->sntp_svr_addr,
                   sntp->sntp_svr_addr1,
                   sntp->sntp_svr_addr2);
        apply_sta(params);
    }
    else if (params->sysmode == WIFI_DEVICE_MODE_EXT_AP)
    {
 #ifdef __SUPPORT_DHCP_SVR__
        apply_dhcp(params->dns[WLAN1_IFACE],
                   dhcp->use_dhcps,
                   dhcp->dhcp_lease_start,
                   dhcp->dhcp_lease_end,
                   dhcp->dhcp_lease_time,
                   dhcp->dhcp_lease_count);
 #endif                                /*__SUPPORT_DHCP_SVR__*/
        apply_ap(params);
    }
    else if (params->sysmode == WIFI_DEVICE_MODE_EXT_AP_STATION)
    {
 #ifdef __SUPPORT_DHCP_SVR__
        apply_dhcp(params->dns[WLAN1_IFACE],
                   dhcp->use_dhcps,
                   dhcp->dhcp_lease_start,
                   dhcp->dhcp_lease_end,
                   dhcp->dhcp_lease_time,
                   dhcp->dhcp_lease_count);
 #endif                                /*__SUPPORT_DHCP_SVR__*/
        apply_ap(params);
        apply_sntp(sntp->sntp_client,
                   sntp->sntp_client_period_time,
                   sntp->sntp_timezone_int,
                   sntp->sntp_svr_addr,
                   sntp->sntp_svr_addr1,
                   sntp->sntp_svr_addr2);
        apply_sta(params);
    }
    else if ((params->sysmode == WIFI_DEVICE_MODE_EXT_P2P) || (params->sysmode == WIFI_DEVICE_MODE_EXT_P2P_GO) ||
             (params->sysmode == WIFI_DEVICE_MODE_EXT_P2P_STATION))
    {
        apply_p2p(params->channel, params->p2p_listen_chan, params->p2p_go_intent, params->p2p_ssid_postfix);
    }

 #ifdef RM_MAP_PERSISTANT_W
    int edge_type  = 0;
    int wakeup_pin = 0;

    if ((RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, "GPIO_WAKEUP_SOURCE_PIN",
                                      &wakeup_pin) == FSP_SUCCESS) &&
        ((RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, "GPIO_WAKEUP_SOURCE_EDGE_TYPE",
                                       &edge_type)) == FSP_SUCCESS))
    {
        R_BSP_WakeupSourcePinSetRetained((uint32_t) wakeup_pin, (uint32_t) edge_type);
    }
 #endif
}

static inline int RM_MAP_PERSISTANT_W_Read_INT_or_zero (const char * name)
{
    int       ret;
    fsp_err_t err = RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, name, &ret);

    return FSP_SUCCESS == err ? ret : 0;
}

/**
 * Load setup_params struct from persistant memory into params 1 by 1
 * returns FSP_SUCCESS if successful
 */
static fsp_err_t load_params (struct setup_params * params) {
    char * result_ptr;
    int    channel_count = 0;

    map_persistant_w_instance_ctrl_t * p_ctrl = RM_MAP_PERSISTANT_W_get_ctrl();
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    /* Read from nvram and update params struct */
    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_COUNTRY_CODE,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->country_code, result_ptr, sizeof(params->country_code));
    }
    else
    {
        bsp_safe_strcpy(params->country_code, COUNTRY_CODE_DEFAULT, sizeof(params->country_code));
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SYS_MODE,
                                     (int *) &params->sysmode) != FSP_SUCCESS)
    {
        params->sysmode = WIFI_DEVICE_MODE_EXT_STATION;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND,
                                     (int *) &params->band) != FSP_SUCCESS)
    {
        params->band = WPA_SETBAND_AUTO;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL,
                                     (int *) &params->channel) != FSP_SUCCESS)
    {
        params->channel = CHANNEL_DEFAULT;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WIFI_MODE,
                                     (int *) &params->wifi_mode) != FSP_SUCCESS)
    {
        params->wifi_mode = WIFI_MODE_BGN + GAP_USER_CONFIGURE_WIFI_MODE;
    }

 #if CFG_PMGR
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENABLE_DPM,
                                     (int *) &params->enable_dpm) != FSP_SUCCESS)
    {
        params->enable_dpm = DFLT_DPM;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME, &params->dpm_keepalive_time) != FSP_SUCCESS)
    {
        params->dpm_keepalive_time = DFLT_DPM_KEEPALIVE_TIME;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_USER_WAKEUP_TIME, &params->dpm_user_wakeup_time) != FSP_SUCCESS)
    {
        params->dpm_user_wakeup_time = DFLT_DPM_USER_WAKEUP_TIME;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT, &params->dpm_TIM_wakeup_count) != FSP_SUCCESS)
    {
        params->dpm_TIM_wakeup_count = DFLT_DPM_TIM_WAKEUP_COUNT;
    }
 #endif                                /* CFG_PMGR */

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_SSID_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->ssid[0], result_ptr, sizeof(params->ssid[0]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_SSID_1,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->ssid[1], result_ptr, sizeof(params->ssid[1]));
    }

    params->hidden_ssid = (bool) RM_MAP_PERSISTANT_W_Read_INT_or_zero(WIFI_PROFILE_HIDDEN_SSID);

    params->pmf[0] = RM_MAP_PERSISTANT_W_Read_INT_or_zero(WIFI_PROFILE_PMF_0);
    params->pmf[1] = RM_MAP_PERSISTANT_W_Read_INT_or_zero(WIFI_PROFILE_PMF_1);

    params->security[0] = RM_MAP_PERSISTANT_W_Read_INT_or_zero(WIFI_PROFILE_SECURITY_0);
    params->security[1] = RM_MAP_PERSISTANT_W_Read_INT_or_zero(WIFI_PROFILE_SECURITY_1);

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENCKEY_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->password[0], result_ptr, sizeof(params->password[0]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_ENCKEY_1,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->password[1], result_ptr, sizeof(params->password[1]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_SAE_GROUPS_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->sae_groups[0], result_ptr, sizeof(params->sae_groups[0]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_SAE_GROUPS_1,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->sae_groups[1], result_ptr, sizeof(params->sae_groups[1]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_WEPKEY0_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->wep_key, result_ptr, sizeof(params->wep_key));
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_WEPINDEX_0,
                                 (int *) &params->wep_key_idx);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_WEPTYPE_0,
                                 (int *) &params->wep_key_type);

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_AUTH_MODE,
                                 (int *) &params->eap_auth_mode);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_EAP_PHASE2,
                                 (int *) &params->eap_phase2);

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_EAP_ID,
                                    &result_ptr);
    if (result_ptr && (strlen(result_ptr) > 0))
    {
        bsp_safe_strcpy(params->eap_id, result_ptr, sizeof(params->eap_id));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_EAP_PW,
                                    &result_ptr);
    if (result_ptr && (strlen(result_ptr) > 0))
    {
        bsp_safe_strcpy(params->eap_pw, result_ptr, sizeof(params->eap_pw));
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_LISTEN_CH,
                                 (int *) &params->p2p_listen_chan);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_GO_INTENT,
                                 (int *) &params->p2p_go_intent);
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_P2P_SSID_POSTFIX,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->p2p_ssid_postfix, result_ptr, sizeof(params->p2p_ssid_postfix));
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_NETMODE_0,
                                 &params->netmode[0]);

    if (params->netmode[0] != DHCPCLIENT)
    {
        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_IPADDR_0,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->ipaddress[0], result_ptr, sizeof(params->ipaddress[0]));
        }

        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_NETMASK_0,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->subnetmask[0], result_ptr, sizeof(params->subnetmask[0]));
        }

        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_GATEWAY_0,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->gateway[0], result_ptr, sizeof(params->gateway[0]));
        }
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_NETMODE_1,
                                 &params->netmode[1]);

    if (params->netmode[1] != DHCPCLIENT)
    {
        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_IPADDR_1,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->ipaddress[1], result_ptr, sizeof(params->ipaddress[1]));
        }

        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_NETMASK_1,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->subnetmask[1], result_ptr, sizeof(params->subnetmask[1]));
        }

        result_ptr = NULL;
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_WIFIPROFILE,
                                        WIFI_PROFILE_GATEWAY_1,
                                        &result_ptr);
        if (result_ptr)
        {
            bsp_safe_strcpy(params->gateway[1], result_ptr, sizeof(params->gateway[1]));
        }
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_DNSSVR_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->dns[0], result_ptr, sizeof(params->dns[0]));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    WIFI_PROFILE_DNSSVR_2ND_0,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->dns[1], result_ptr, sizeof(params->dns[1]));
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_AP_MAX_INACTIVITY_1,
                                     (int *) &params->ap_max_inactivity) != FSP_SUCCESS)
    {
        params->ap_max_inactivity = AP_MAX_INACTIVITY;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_PS_1,
                                     (int *) &params->ap_wmm_ps_enabled) != FSP_SUCCESS)
    {
        params->ap_wmm_ps_enabled = -1;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_1,
                                     (int *) &params->ap_wmm_enabled) != FSP_SUCCESS)
    {
        params->ap_wmm_enabled = -1;
    }

    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_ENC_MODE_1,
                                     (int *) &params->ap_enc_mode) != FSP_SUCCESS)
    {
        params->ap_enc_mode = -1;
    }

    /* Load channel configuration for STA mode */
    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_WIFIPROFILE,
                                    "scan_channel_list",
                                    &result_ptr);
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 "scan_channel_number",
                                 &channel_count);

    if (result_ptr && (channel_count > 0) && (channel_count <= wificonfigMAX_CHANNEL_LIST))
    {
        /* Parse channel list string */
        char    ch_copy[128];
        uint8_t ch_idx   = 0;
        char  * ch_token = NULL;

        bsp_safe_strcpy(ch_copy, result_ptr, sizeof(ch_copy));

        ch_token = strtok(ch_copy, " ,");

        while (ch_token != NULL && ch_idx < wificonfigMAX_CHANNEL_LIST)
        {
            int channel = atoi(ch_token);

            if (channel > 0)
            {
                params->channel_list[ch_idx++] = (uint32_t) channel;
            }

            ch_token = strtok(NULL, " ,");
        }

        params->channel_number = ch_idx;
    }
    else
    {
        params->channel_number = 0;
    }

    return FSP_SUCCESS;
}

/**
 * Load dhcp_params struct from persistant memory into params 1 by 1
 * returns FSP_SUCCESS if successful
 */
static fsp_err_t load_dhcp_params (struct setup_params * params, struct dhcp_params * dhcp_params) {
    char * result_ptr;

    map_persistant_w_instance_ctrl_t * p_ctrl = RM_MAP_PERSISTANT_W_get_ctrl();
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    /* Read from nvram and update params struct */
    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS, &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(params->dns[WLAN1_IFACE], result_ptr, sizeof(params->dns[WLAN1_IFACE]));
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_SYSCFG,
                                 NVR_KEY_DHCPD,
                                 (int *) &dhcp_params->use_dhcps);

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_START_IP,
                                    &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(dhcp_params->dhcp_lease_start, result_ptr, sizeof(dhcp_params->dhcp_lease_start));
    }

    result_ptr = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_END_IP, &result_ptr);
    if (result_ptr)
    {
        bsp_safe_strcpy(dhcp_params->dhcp_lease_end, result_ptr, sizeof(dhcp_params->dhcp_lease_end));
    }

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_SYSCFG,
                                 DHCP_SERVER_LEASE_TIME,
                                 (int *) &dhcp_params->dhcp_lease_time);

    return FSP_SUCCESS;
}

/**
 * Store given setup_params into persistant memory 1 by 1
 * returns 0 if successful
 */
static fsp_err_t store_params (struct setup_params * params) {
    map_persistant_w_instance_ctrl_t * p_ctrl = RM_MAP_PERSISTANT_W_get_ctrl();
    FSP_ERROR_RETURN(MAP_PERSISTANT_W_OPEN == p_ctrl->map_persistant_w_open, FSP_ERR_NOT_OPEN);

    /* Write params struct */
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COUNTRY_CODE, params->country_code);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SYS_MODE, params->sysmode);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_BAND, params->band);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_CHANNEL, params->channel);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WIFI_MODE, params->wifi_mode);
 #if CFG_PMGR
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENABLE_DPM, params->enable_dpm);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl,
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_DPM_DPM_KEEPALIVE_TIME,
                                  params->dpm_keepalive_time);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl,
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_DPM_USER_WAKEUP_TIME,
                                  params->dpm_user_wakeup_time);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl,
                                  ENV_GROUP_WIFIPROFILE,
                                  WIFI_PROFILE_DPM_TIM_WAKEUP_COUNT,
                                  params->dpm_TIM_wakeup_count);
 #endif                                /* CFG_PMGR */
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_0, params->ssid[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SSID_1, params->ssid[1]);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_HIDDEN_SSID, (int) params->hidden_ssid);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_0, params->pmf[0]);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_PMF_1, params->pmf[1]);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SECURITY_0, params->security[0]);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SECURITY_1, params->security[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_0, params->password[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_ENCKEY_1, params->password[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SAE_GROUPS_0, params->sae_groups[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_SAE_GROUPS_1, params->sae_groups[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPKEY0_0, params->wep_key);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPINDEX_0, params->wep_key_idx);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_WEPTYPE_0, params->wep_key_type);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_AUTH_MODE, params->eap_auth_mode);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_PHASE2, params->eap_phase2);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_ID, params->eap_id);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_EAP_PW, params->eap_pw);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_LISTEN_CH, params->p2p_listen_chan);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_P2P_GO_INTENT, params->p2p_go_intent);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl,
                                     ENV_GROUP_WIFIPROFILE,
                                     WIFI_PROFILE_P2P_SSID_POSTFIX,
                                     params->p2p_ssid_postfix);

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_0, params->netmode[0]);
    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_1, params->netmode[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_IPADDR_0, params->ipaddress[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_IPADDR_1, params->ipaddress[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMASK_0, params->subnetmask[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMASK_1, params->subnetmask[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_GATEWAY_0, params->gateway[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_GATEWAY_1, params->gateway[1]);

    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_0, params->dns[0]);
    RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_DNSSVR_2ND_0, params->dns[1]);

    /* Save channel configuration for STA mode */
    if (params->channel_number > 0)
    {
        char channel_str[128] = "";
        int  ch_offset        = 0;

        for (uint8_t i = 0; i < params->channel_number; i++)
        {
            if (ch_offset > 0)
            {
                ch_offset += snprintf(channel_str + ch_offset,
                                      sizeof(channel_str) - ch_offset,
                                      " %lu",
                                      params->channel_list[i]);
            }
            else
            {
                ch_offset += snprintf(channel_str + ch_offset,
                                      sizeof(channel_str) - ch_offset,
                                      "%lu",
                                      params->channel_list[i]);
            }
        }

        RM_MAP_PERSISTANT_W_Write_STRING(p_ctrl, ENV_GROUP_WIFIPROFILE, "scan_channel_list", channel_str);
        RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, "scan_channel_number", params->channel_number);
    }
    else
    {
        /* Clear channel configuration if no channels specified */
        RM_MAP_PERSISTANT_W_Erase(p_ctrl, ENV_GROUP_WIFIPROFILE, "scan_channel_list");
        RM_MAP_PERSISTANT_W_Erase(p_ctrl, ENV_GROUP_WIFIPROFILE, "scan_channel_number");
    }

    RM_MAP_PERSISTANT_W_Write_INT(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_COMPLETE, 1);

    {
        /*
         * NOTE: Removed configuration which Easy-Setup doesn't support.
         */
        RM_MAP_PERSISTANT_W_Erase(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_MAX_INACTIVITY_1);

        RM_MAP_PERSISTANT_W_Erase(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_1);

        RM_MAP_PERSISTANT_W_Erase(p_ctrl, ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_AP_WMM_PS_1);
    }

    return FSP_SUCCESS;
}

static void easy_setup_task (void * pvPraram)
{
    bool load_from_nvram       = (bool) pvPraram;
    bool loaded_basic          = false;
    bool loaded_dhcp           = false;
    bool parsed                = false;
    char reply[16]             = {0};
    int  wifi_profile_complete = false;

    /* Flag for indicating parse_scan_list updated the Wifi mode */
    bool mode_changed = false;

    struct setup_params * params = pvPortMalloc(sizeof(struct setup_params));
    struct sntp_params  * p_sntp = pvPortMalloc(sizeof(struct sntp_params));
    struct dhcp_params  * p_dhcp = pvPortMalloc(sizeof(struct dhcp_params));

    if (!params || !p_sntp || !p_dhcp)
    {
        printf("[%s] Failed to allocate one or more parameter structures\n", __func__);
        goto CLEANUP;
    }

    memset(params, 0, sizeof(struct setup_params));
    params->band = WPA_SETBAND_DEF;

    memset(p_sntp, 0, sizeof(struct sntp_params));
    memset(p_dhcp, 0, sizeof(struct dhcp_params));

 #if CFG_PMGR
    RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
 #endif                                /* CFG_PMGR */

    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_WIFIPROFILE,
                                 WIFI_PROFILE_COMPLETE,
                                 &wifi_profile_complete);

    if (load_from_nvram)
    {
        if (true == wifi_profile_complete)
        {
            loaded_basic = (FSP_SUCCESS == load_params(params));
            loaded_dhcp  = (FSP_SUCCESS == load_dhcp_params(params, p_dhcp));
        }
        else
        {
            apply_mandatory_params();
        }
    }

    if (!loaded_basic)
    {
        /* get params from user via CLI */
        parsed = (0 == parse_params(params, p_sntp, p_dhcp, !load_from_nvram, &mode_changed));
        if (!parsed)
        {
            printf("[%s] Failed to parse params from user\n", __func__);
        }
    }

    configASSERT(cli_handle_task);
    xTaskNotify(cli_handle_task, EASY_SETUP_TASK_EVENT_FINISH, eSetBits);
    vTaskResume(cli_handle_task);

    /* Apply supplicant log prints to default after it was disabled before */
    ra6w1_cli_reply("set_log default 0", NULL, reply);

    if (parsed)
    {
        /* we have valid params struct - apply it */
        apply_params(params, p_sntp, p_dhcp, mode_changed);

        /* store only if not loaded (if loaded it is redundant to store same params again) */
        if (FSP_SUCCESS != store_params(params))
        {
            printf("[%s] Failed to store params\n", __func__);
        }
    }
    else if (loaded_basic)
    {
        void * p_loaded_dhcp = loaded_dhcp ? p_dhcp : NULL;

        apply_params(params, p_sntp, (struct dhcp_params *) p_loaded_dhcp, mode_changed);
    }
    else
    {
        printf("[%s] No valid params available\n", __func__);
    }

CLEANUP:
    if (p_dhcp)
    {
        vPortFree(p_dhcp);
    }

    if (p_sntp)
    {
        vPortFree(p_sntp);
    }

    if (params)
    {
        vPortFree(params);
    }

 #if CFG_PMGR
    RM_PMGR_W_remove_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_SLEEP_PROHIBITED);
 #endif                                /* CFG_PMGR */

    vTaskResume(cli_handle_task);
    ra6w1_setup_task = NULL;
    vTaskDelete(NULL);
}

void create_easy_setup_task (bool load_from_nvram)
{
 #if CFG_PMGR
    if (RM_PMGR_W_dpm_is_wakeup())
    {
        goto notify_cli;
    }
 #endif                                /* CFG_PMGR */

    if (!ra6w1_network_main_get_wlaninit_mode() || !ra6w1_network_main_is_wlaninit())
    {
        goto notify_cli;
    }

    xTaskCreate(easy_setup_task,
                "EasySetup",
                RRQ61X_SETUP_POOL_SIZE,
                (void *) load_from_nvram,
                RRQ61X_SETUP_PRIORITY,
                &ra6w1_setup_task);

    return;

notify_cli:

    configASSERT(cli_handle_task);
    xTaskNotify(cli_handle_task, EASY_SETUP_TASK_EVENT_FINISH, eSetBits);
    vTaskResume(cli_handle_task);
}

void easy_setup_suspend (void)
{
    if (ra6w1_setup_task)
    {
        vTaskSuspend(ra6w1_setup_task);
    }
}

#else

void create_easy_setup_task (bool load_from_nvram)
{
    RA6W1_UNUSED_ARG(load_from_nvram);
}

#endif                                 /* __SUPPORT_EASY_SETUP__ && __SUPPORT_APP_CONSOLE_INPUT__ */

/* EOF */
