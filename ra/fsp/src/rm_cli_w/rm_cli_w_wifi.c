/**
 ****************************************************************************************
 *
 * @file rm_cli_w_wifi.c
 *
 * @brief wifi command functions
 *
 * Copyright (c) 2025 Renesas Electronics. All rights reserved.
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"

#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "rm_cli_w_wifi.h"

#include "rm_wifi_api.h"
#include "rm_wifi.h"

#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#define COUNTRY_CODE_LEN 2
#define P2P_GET_LEN 256
#define P2P_SET_LEN 24
#define RM_CLI_W_WIFI_MAC_LEN 18 /* 17 charactors of "XX:XX:XX:XX:XX:XX" + 1 for null terminator */
#define P2P_CMD_RESP_LEN 1024

extern int hex2byte(const char *hex);
extern bool twt_setup_req_valid(struct twt_setup_req *req);

/** Wi-Fi Security wanted order extended types, aligned to the expected order from the cicd */
typedef enum
{
    eWiFiSecurityWantedOrderOpen_ext,            ///< Open - No Security
    eWiFiSecurityWantedOrderWEP_ext,             ///< WEP Security
    eWiFiSecurityWantedOrderWPA_ext,             ///< WPA Security
    eWiFiSecurityWantedOrderWPA2_ext,            ///< WPA2 Security
    eWiFiSecurityWantedOrderWPA_WPA2_ext,        ///< WPA + WPA2 Security
    eWiFiSecurityWantedOrderWPA3_ext,            ///< WPA3 Security
    eWiFiSecurityWantedOrderWPA2_WPA3_ext,       ///< WPA2 + WPA3 Security
    eWiFiSecurityWantedOrderWPA3_OWE_ext,        ///< WPA3-OWE Security
    eWiFiSecurityWantedOrderWPA_ent_ext,         ///< WPA Enterprise Security
    eWiFiSecurityWantedOrderWPA_WPA2_ent_ext,    ///< WPA + WPA2 Enterprise Security
    eWiFiSecurityWantedOrderWPA2_WPA3_ent_ext,   ///< WPA2 + WPA3 Enterprise Security
    eWiFiSecurityWantedOrderWPA3_ent_ext,        ///< WPA3 128 Bits Enterprise Security
    eWiFiSecurityWantedOrderWPA3_192B_ent_ext,   ///< WPA3 192 Bits Enterprise Security
    eWiFiSecurityWantedOrderMax_ext
} WIFISecurityWantedOrderExt_t ;

bool wifi_parse_channels(const char * channel_arg, WIFINetworkParamsExt_t * net_params)
{
    extern int ra6w1_regdb_ch_to_freq(int ch);
    bool err = pdTRUE;
    char channels[128];
    char * ch_ptr;
    int ch_val;
    uint32_t * channel_list = NULL;
    uint8_t ch_count = 0;
    char * parse_ptr = NULL;

    if (channel_arg == NULL || strlen(channel_arg) == 0)
    {
        if (net_params)
        {
            net_params->ucNumChannels = 0;
            net_params->pucChannelList = NULL;
        }
        return pdTRUE;
    }

    /* Copy channels string for parsing */
    bsp_safe_strcpy(channels, channel_arg, sizeof(channels));

    /* Skip "ch=" (case-insensitive) prefix if present */
    parse_ptr = channels;

    if ((parse_ptr[0] == 'c' || parse_ptr[0] == 'C') &&
        (parse_ptr[1] == 'h' || parse_ptr[1] == 'H') &&
        (parse_ptr[2] == '='))
    {
        parse_ptr += 3;
    }

    /* Allocate memory for channel list */
    channel_list = pvPortMalloc(wificonfigMAX_CHANNEL_LIST * sizeof(uint32_t));
    if (!channel_list)
    {
        printf("!!!!Memory allocation Failure for channel_list\n");
        return pdFALSE;
    }

    /* Parse and validate channels */
    ch_ptr = strtok(parse_ptr, " \t,");
    while (ch_ptr != NULL && ch_count < wificonfigMAX_CHANNEL_LIST)
    {
        ch_val = atoi(ch_ptr);

        /* Validate channel range: 2G (1-14) or 5G (36-165) */
        if (ch_val < 1 || (ch_val > 14 && ch_val < 36) || ch_val > 165)
        {
            vPortFree(channel_list);
            printf("!!!Invalid channels");
            return pdFALSE;
        }

        /* Add to channel list */
        channel_list[ch_count++] = (uint32_t) ch_val;

        ch_ptr = strtok(NULL, " \t,");
    }

    if (ch_count > 0)
    {
        /* Populate API structure */
        net_params->ucNumChannels = ch_count;
        net_params->pucChannelList = channel_list;

    }
    else
    {
        vPortFree(channel_list);
        net_params->ucNumChannels = 0;
        net_params->pucChannelList = NULL;
    }

    return err;
}

static bool cmd_disconnect(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_Disconnect();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_start_disconnect(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_StartDisconnect();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_on(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_On();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_on_auto(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_OnAuto();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_disconnect_station(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    uint8_t          pucMacStr[RM_CLI_W_WIFI_MAC_LEN] = {0};
    uint8_t          pucMacHex[6]                   = {0};

    if (argc != 2)
    {
        return pdFALSE;
    }

    /* Convert the MAC address from string to hex */
    bsp_safe_strcpy((char *) pucMacStr, (char *) argv[1], RM_CLI_W_WIFI_MAC_LEN);
    for (int i = 0; i < 6; i++)
    {
        char   hex[3] = {pucMacStr[i * 3], pucMacStr[i * 3 + 1], '\0'};
        char * endptr;
        pucMacHex[i] = strtol(hex, &endptr, 16);
        if (*endptr != '\0')
        {
            return pdFALSE;
        }
    }

    ret = WIFI_StartDisconnectStation(pucMacHex);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_off(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_Off();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_reset(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_Reset();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_start_ap(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_StartAP();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_stop_ap(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_StopAP();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static uint8_t parse_ent_security(int sec_input)
{
    switch (sec_input)
    {
        case 0:
            return eWiFiSecurityWPA_ent_ext;
        case 1:
            return eWiFiSecurityWPA2_ent_ext;
        case 2:
            return eWiFiSecurityWPA_WPA2_ent_ext;
        case 3:
            return eWiFiSecurityWPA2_WPA3_ent_ext;
        case 4:
            return eWiFiSecurityWPA3_ent_ext;
        case 5:
            return eWiFiSecurityWPA3_192B_ent_ext;
        default:
            return eWiFiSecurityNotSupported;
    }
}

static void read_cert_n_priv_key(WIFINetworkParamsExt_t *network_params)
{
    extern int make_message(char *title, char *buf, size_t buflen);
    int cert_len = 0;
    int priv_key_len = 0;

    cert_len = make_message("Certificate value", network_params->xEntNetParams.ucCert, wificonfigMAX_CERT_LEN);

    if (cert_len == 0)
    {
        printf("\n\nCanceled entering certificate\n");
        return;
    }
    else
    {
        network_params->xEntNetParams.ucCertLength = cert_len;
    }

    priv_key_len = make_message("Private key value", network_params->xEntNetParams.ucPrivKey, wificonfigMAX_PRIV_KEY_LEN);

    if (priv_key_len == 0)
    {
        printf("\n\nCanceled entering private key\n");
        return;
    }
    else
    {
        network_params->xEntNetParams.ucPrivKeyLength = priv_key_len;
    }
}

#define DEAFULT_SAE_GROUPS "19 20 21"
#define SAE_GROUP_19 "19"
#define SAE_GROUP_20 "20"
#define SAE_GROUP_21 "21"

static void parse_sae_groups(char * p_sae_groups, int input)
{
    switch (input)
    {
        case 0:
            return;
        case 1:
            bsp_safe_strcpy(p_sae_groups, SAE_GROUP_19, wificonfigMAX_SAE_GROUPS_LEN);
            break;
        case 2:
            bsp_safe_strcpy(p_sae_groups, SAE_GROUP_20, wificonfigMAX_SAE_GROUPS_LEN);
            break;
        case 3:
            bsp_safe_strcpy(p_sae_groups, SAE_GROUP_21, wificonfigMAX_SAE_GROUPS_LEN);
            break;
        case 4:
            bsp_safe_strcpy(p_sae_groups, DEAFULT_SAE_GROUPS, wificonfigMAX_SAE_GROUPS_LEN);
            break;
        default:
            bsp_safe_strcpy(p_sae_groups, DEAFULT_SAE_GROUPS, wificonfigMAX_SAE_GROUPS_LEN);
    }
}

static uint8_t parse_security(int sec_input)
{
    switch (sec_input)
    {
        case 0:
            return eWiFiSecurityOpen;
        case 1:
            return eWiFiSecurityWPA;
        case 2:
            return eWiFiSecurityWPA2;
        case 3:
            return eWiFiSecurityWPA3;
        case 4:
            return eWiFiSecurityWPA_WPA2_ext;
        case 5:
            return eWiFiSecurityWPA2_WPA3_ext;
        default:
            return eWiFiSecurityNotSupported;
    }
}

static bool rm_cli_w_wifi_parse_channel_arg(int argc, const char * const *argv,
                                            WIFINetworkParamsExt_t *network_params,
                                            int * p_arg_idx)
{
    int idx = 0;

    if ((p_arg_idx == NULL) || (argv == NULL) || (network_params == NULL))
    {
        return pdFALSE;
    }

    if (argc <= 0)
    {
        return pdTRUE;
    }

    for (idx = 0; idx < argc; idx++)
    {
        /* Check for ch= prefix */
        if ((argv[idx][0] == 'c' || argv[idx][0] == 'C') &&
            (argv[idx][1] == 'h' || argv[idx][1] == 'H') &&
            (argv[idx][2] == '='))
        {
            if (wifi_parse_channels(argv[idx], network_params) != pdTRUE)
            {
                printf("Parsing the channel was wrong\n");
                return pdFALSE;
            }

            *p_arg_idx = idx;

            return pdTRUE;
        }
    }

    return pdTRUE;
}

static bool rm_cli_w_wifi_parse_bssid_arg(int argc, const char * const *argv,
                                          WIFINetworkParamsExt_t *network_params,
                                          int * p_arg_idx)
{
    const char * p_bssid_prefix = "bssid=";
    const size_t bssid_len = 17;
    const char * p_bssid;
    int arg_idx = 0;
    int bssid_idx = 0;
    unsigned int bssid[wificonfigMAX_BSSID_LEN] = {0x00,};
    int ret = 0;

    if ((p_arg_idx == NULL) || (argv == NULL) || (network_params == NULL))
    {
        return pdFALSE;
    }

    if (argc <= 0)
    {
        return pdTRUE;
    }

    for (arg_idx = 0; arg_idx < argc; arg_idx++)
    {
        if (argv[arg_idx] == NULL)
        {
            continue;
        }

        /* Check string length */
        if (strlen(argv[arg_idx]) < strlen(p_bssid_prefix))
        {
            continue;
        }

        /* Check for bssid= prefix */
        if (strncasecmp(argv[arg_idx], p_bssid_prefix, strlen(p_bssid_prefix)) == 0)
        {
            if (wificonfigMAX_BSSID_LEN != 6)
            {
                /* Not supported bssid parameter */
                return pdFALSE;
            }

            p_bssid = &argv[arg_idx][strlen(p_bssid_prefix)];
            if (strlen(p_bssid) != bssid_len)
            {
                return pdFALSE;
            }

            for (bssid_idx = 0; bssid_idx < bssid_len; bssid_idx++)
            {
                if ((bssid_idx + 1) % 3 == 0)
                {
                    if (p_bssid[bssid_idx] != ':')
                    {
                        return pdFALSE;
                    }
                }
                else
                {
                    if (!isxdigit((unsigned char) p_bssid[bssid_idx]))
                    {
                        return pdFALSE;
                    }
                }
            }

            ret = sscanf(p_bssid, "%x:%x:%x:%x:%x:%x",
                         &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
            if (ret != wificonfigMAX_BSSID_LEN)
            {
                /* Failed to parse bssid */
                return pdFALSE;
            }

            for (int idx = 0; idx < wificonfigMAX_BSSID_LEN; idx++)
            {
                network_params->xNetworkParams.ucBSSID[idx] = (uint8_t) bssid[idx];
            }

            *p_arg_idx = arg_idx;

            return pdTRUE;
        }
    }

    return pdTRUE;
}

static bool cmd_connect_ap(int argc, const char **argv)
{
    bool ret = pdTRUE;
    WIFIReturnCode_t err;
    WIFINetworkParamsExt_t *network_params = NULL;
    uint8_t security = 0;

    int copied_argc = 0;
    const char * p_copied_argv[10];

    if (argc < 4 || argc > 10)
    {
        return pdFALSE;
    }

    security = parse_security(atoi(argv[3]));

    if (security > eWiFiSecurityOpen && argc < 5)
    {
        printf("You must enter a passphrase for non OPEN security modes\n");

        return pdFALSE;
    }

    network_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!network_params)
    {
        return pdFALSE;
    }

    memset(network_params, 0, sizeof(WIFINetworkParamsExt_t));

    copied_argc = argc;
    for (int idx = 0; idx < argc; idx++)
    {
        p_copied_argv[idx] = argv[idx];
    }

    if (argc >= 5)
    {
        int ch_arg_idx = 0;
        int bssid_arg_idx = 0;
        int copied_idx = 0;

        if (rm_cli_w_wifi_parse_channel_arg(argc, argv, network_params, &ch_arg_idx) != pdTRUE)
        {
            ret = pdFALSE;
            goto fail;
        }

        if (rm_cli_w_wifi_parse_bssid_arg(argc, argv, network_params, &bssid_arg_idx) != pdTRUE)
        {
            ret = pdFALSE;
            goto fail;
        }

        if (ch_arg_idx > 0 || bssid_arg_idx > 0)
        {
            copied_argc = 0;

            for (int idx = 0; idx < argc; idx++)
            {
                if ((ch_arg_idx > 0 && idx == ch_arg_idx)
                    || (bssid_arg_idx > 0 && idx == bssid_arg_idx))
                {
                    continue;
                }

                p_copied_argv[copied_idx++] = argv[idx];
                copied_argc++;
            }
        }
    }

    network_params->xNetworkParams.ucSSIDLength = (strlen(p_copied_argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(p_copied_argv[1]);
    memcpy((char *) network_params->xNetworkParams.ucSSID, p_copied_argv[1], network_params->xNetworkParams.ucSSIDLength);
    network_params->ucBand = atoi(p_copied_argv[2]);
    if (network_params->ucBand > 2)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xNetworkParams.xSecurity = security;
    if (network_params->xNetworkParams.xSecurity == eWiFiSecurityNotSupported)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xNetworkParams.xPassword.xWPA.ucLength =
            (strlen(p_copied_argv[4]) > wificonfigMAX_PASSPHRASE_LEN) ? wificonfigMAX_PASSPHRASE_LEN : strlen(p_copied_argv[4]);
    memcpy((char *) network_params->xNetworkParams.xPassword.xWPA.cPassphrase, p_copied_argv[4],
                    network_params->xNetworkParams.xPassword.xWPA.ucLength);

    if (copied_argc >= 6)
    {
        if (atoi(p_copied_argv[5]) > 1)
        {
            ret = pdFALSE;
            goto fail;
        }
        else
        {
            network_params->hidden_ssid = atoi(p_copied_argv[5]);
        }
    }

    network_params->pmf = copied_argc >= 7 ? atoi(p_copied_argv[6]) : PMF_DEFAULT;
    if (network_params->pmf > 3)
    {
        ret = pdFALSE;
        goto fail;
    }

    if (copied_argc == 8)
    {
        if (atoi(p_copied_argv[7]) > 4)
        {
            ret = pdFALSE;
            goto fail;
        }
        else
        {
            parse_sae_groups(network_params->sae_groups, atoi(p_copied_argv[7]));
        }
    }

    err = WIFI_ConnectAPExt(network_params);
    if (eWiFiSuccess != err)
    {
        printf("ERROR:%d\n", err);
        ret = pdFALSE;
    }

fail:
    /* Free dynamically allocated channel list */
    if (network_params->pucChannelList != NULL)
    {
        vPortFree(network_params->pucChannelList);
        network_params->pucChannelList = NULL;
    }

    vPortFree(network_params);
    return ret;
}

static bool cmd_connect_ap_wep(int argc, const char **argv)
{
    bool ret = pdTRUE;
    WIFIReturnCode_t err;
    WIFINetworkParamsExt_t *network_params = NULL;

    if (argc < 6 || argc > 10)
    {
        return pdFALSE;
    }

    network_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!network_params)
    {
        return pdFALSE;
    }

    memset(network_params, 0, sizeof(WIFINetworkParamsExt_t));

    if (argc >= 7)
    {
        int ch_arg_idx = 0;

        if (rm_cli_w_wifi_parse_channel_arg(argc, argv, network_params, &ch_arg_idx) != pdTRUE)
        {
            ret = pdFALSE;
            goto fail;
        }

        if (ch_arg_idx > 0 && ch_arg_idx == (argc - 1))
        {
            argc -= 1;
        }
    }

    network_params->xNetworkParams.ucSSIDLength = (strlen(argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[1]);
    memcpy((char *) network_params->xNetworkParams.ucSSID, argv[1], network_params->xNetworkParams.ucSSIDLength);
    network_params->ucBand = atoi(argv[2]);
    if (network_params->ucBand > 2)
    {
        ret = pdFALSE;
        goto fail;
    }

    if (atoi(argv[3]) > 1)
    {
        ret = pdFALSE;
        goto fail;
    }
    else
    {
        network_params->hidden_ssid = atoi(argv[3]);
    }

    network_params->xNetworkParams.xSecurity = eWiFiSecurityWEP;
    network_params->xNetworkParams.ucDefaultWEPKeyIndex = atoi(argv[4]);
    if (network_params->xNetworkParams.ucDefaultWEPKeyIndex > 3)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xNetworkParams.xPassword.xWEP[0].ucLength = (strlen(argv[5]) > wificonfigMAX_WEPKEY_LEN) ?
                                                                wificonfigMAX_WEPKEY_LEN : strlen(argv[5]);
    memcpy((char *) network_params->xNetworkParams.xPassword.xWEP[0].cKey, argv[5],
           network_params->xNetworkParams.xPassword.xWEP[0].ucLength);

    if (argc >= 7)
    {
        network_params->xNetworkParams.xPassword.xWEP[1].ucLength = (strlen(argv[6]) > wificonfigMAX_WEPKEY_LEN) ?
                                                                    wificonfigMAX_WEPKEY_LEN : strlen(argv[6]);
        memcpy((char *) network_params->xNetworkParams.xPassword.xWEP[1].cKey, argv[6],
               network_params->xNetworkParams.xPassword.xWEP[1].ucLength);
    }
    if (argc >= 8)
    {
        network_params->xNetworkParams.xPassword.xWEP[2].ucLength = (strlen(argv[7]) > wificonfigMAX_WEPKEY_LEN) ?
                                                                    wificonfigMAX_WEPKEY_LEN : strlen(argv[7]);
        memcpy((char *) network_params->xNetworkParams.xPassword.xWEP[2].cKey, argv[7],
               network_params->xNetworkParams.xPassword.xWEP[2].ucLength);
    }
    if (argc == 9)
    {
        network_params->xNetworkParams.xPassword.xWEP[3].ucLength = (strlen(argv[8]) > wificonfigMAX_WEPKEY_LEN) ?
                                                                    wificonfigMAX_WEPKEY_LEN : strlen(argv[8]);
        memcpy((char *) network_params->xNetworkParams.xPassword.xWEP[3].cKey, argv[8],
               network_params->xNetworkParams.xPassword.xWEP[3].ucLength);
    }

    err = WIFI_ConnectAPExt(network_params);
    if (eWiFiSuccess != err)
    {
        printf("ERROR:%d\n", err);
        ret = pdFALSE;
    }

fail:

    /* Free dynamically allocated channel list */
    if (network_params->pucChannelList != NULL)
    {
        vPortFree(network_params->pucChannelList);
        network_params->pucChannelList = NULL;
    }

    vPortFree(network_params);
    return ret;
}

static bool cmd_connect_ap_ent(int argc, const char **argv)
{
    bool ret = pdTRUE;
    WIFIReturnCode_t err;
    WIFINetworkParamsExt_t *network_params = NULL;

#define ENT_AUTH_TYPE_OFFSET 1

    if (argc < 8 || argc > 11)
    {
        return pdFALSE;
    }

    network_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!network_params)
    {
        return pdFALSE;
    }

    memset(network_params, 0, sizeof(WIFINetworkParamsExt_t));

    if (argc >= 9)
    {
        int ch_arg_idx = 0;

        if (rm_cli_w_wifi_parse_channel_arg(argc, argv, network_params, &ch_arg_idx) != pdTRUE)
        {
            ret = pdFALSE;
            goto fail;
        }

        if (ch_arg_idx > 0 && ch_arg_idx == (argc - 1))
        {
            argc -= 1;
        }
    }

    network_params->xNetworkParams.ucSSIDLength = (strlen(argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[1]);
    memcpy((char *) network_params->xNetworkParams.ucSSID, argv[1], network_params->xNetworkParams.ucSSIDLength);
    network_params->ucBand = atoi(argv[2]);
    if (network_params->ucBand > 2)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xNetworkParams.xSecurity = parse_ent_security(atoi(argv[3]));
    if (network_params->xNetworkParams.xSecurity == eWiFiSecurityNotSupported)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xEntNetParams.ucEntAuthType = atoi(argv[4]) + ENT_AUTH_TYPE_OFFSET;
    if (network_params->xEntNetParams.ucEntAuthType >= ENT_AUTH_TYPE_UNSUPPORTED)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xEntNetParams.ucEntAuthProto = atoi(argv[5]);
    if (network_params->xEntNetParams.ucEntAuthProto >= ENT_AUTH_PROTO_UNSUPPORTED)
    {
        ret = pdFALSE;
        goto fail;
    }

    network_params->xEntNetParams.ucIDLength = (strlen(argv[6]) > wificonfigMAX_ENT_IDENTITY_LEN) ?
                                               wificonfigMAX_ENT_IDENTITY_LEN : strlen(argv[6]);
    memcpy((char *) network_params->xEntNetParams.ucID, argv[6], network_params->xEntNetParams.ucIDLength);
    if ((network_params->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_TLS) ||
        (network_params->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_TLS))
    {
        memset(network_params->xEntNetParams.ucPassword, 0, wificonfigMAX_ENT_PASSWORD_LEN);
    }
    else
    {
        network_params->xEntNetParams.ucPasswordLength = (strlen(argv[7]) > wificonfigMAX_ENT_PASSWORD_LEN) ?
                                                         wificonfigMAX_ENT_PASSWORD_LEN : strlen(argv[7]);
        memcpy((char *) network_params->xEntNetParams.ucPassword, argv[7], network_params->xEntNetParams.ucPasswordLength);
    }

    if (argc >= 9 )
    {
        if (argc != 10)
        {
            ret = pdFALSE;
            goto fail;
        }

        if (((network_params->xEntNetParams.ucEntAuthType == ENT_AUTH_TYPE_TLS) ||
            (network_params->xEntNetParams.ucEntAuthProto == ENT_AUTH_PROTO_TLS)) &&
            ((atoi(argv[8]) == 0) || (atoi(argv[9]) == 0)))
        {
            ret = pdFALSE;
            goto fail;
        }

        read_cert_n_priv_key(network_params);
    }

    err = WIFI_ConnectAPExt(network_params);
    if (eWiFiSuccess != err)
    {
        printf("ERROR:%d\n", err);
        ret = pdFALSE;
    }

fail:

    /* Free dynamically allocated channel list */
    if (network_params->pucChannelList != NULL)
    {
        vPortFree(network_params->pucChannelList);
        network_params->pucChannelList = NULL;
    }

    vPortFree(network_params);
    return ret;
}

static bool cmd_auto_config(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    WIFINetworkParams_t network_params = {0};

    if (argc < 3)
    {
        return pdFALSE;
    }

    network_params.ucSSIDLength = (strlen(argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[1]);
    memcpy((char *)network_params.ucSSID, argv[1], network_params.ucSSIDLength);
    network_params.xSecurity = parse_security(atoi(argv[2]));
    if (network_params.xSecurity == eWiFiSecurityNotSupported)
    {
        return pdFALSE;
    }

    if (network_params.xSecurity != eWiFiSecurityOpen)
    {
        network_params.xPassword.xWPA.ucLength = (strlen(argv[3]) > wificonfigMAX_PASSPHRASE_LEN) ?
                                                 wificonfigMAX_PASSPHRASE_LEN : strlen(argv[3]);
        memcpy((char *)network_params.xPassword.xWPA.cPassphrase, argv[3], network_params.xPassword.xWPA.ucLength);
    }

    ret = WIFI_AutoConfig(&network_params);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_set_mode(int argc, const char ** argv)
{
    WIFIReturnCode_t         ret;
    e_wifi_device_mode_ext_t device_mode_ext;

    if (argc != 2)
    {
        return pdFALSE;
    }

    device_mode_ext = atoi(argv[1]);

    ret = WIFI_SetModeExt(device_mode_ext);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_get_mode(int argc, const char ** argv)
{
    WIFIReturnCode_t         ret;
    e_wifi_device_mode_ext_t device_mode_ext;
    FSP_PARAMETER_NOT_USED(argv);

    if (argc != 1)
    {
        return pdFALSE;
    }

    ret = WIFI_GetModeExt(&device_mode_ext);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("WIFIDeviceMode:%d", device_mode_ext);
    switch (device_mode_ext)
    {
        case WIFI_DEVICE_MODE_EXT_STATION:
        {
            printf("(Station mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_AP:
        {
            printf("(Access point mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_P2P:
        {
            printf("(P2P mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_P2P_GO:
        {
            printf("(P2P GO mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
        {
            printf("(AP+Station mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
        {
            printf("(P2P+Station mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_MESH_POINT:
        {
            printf("(Mesh point mode)");
            break;
        }
        case WIFI_DEVICE_MODE_EXT_MESH_PORTAL:
        {
            printf("(Mesh portal mode)");
            break;
        }
        default:
        {
            printf("(Unsupported mode)");
            break;
        }
    }
    printf("\n");

    return pdTRUE;
}

static bool cmd_network_add(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    WIFINetworkProfile_t network_profile = {0};
    uint16_t usIndex;
    char strbssid[wificonfigMAX_BSSID_LEN * 3]; // (12 characters for bssid) + (5 characters for ':') + '\0'
    char * pbssid;

    if (argc != 6)
    {
        return pdFALSE;
    }

    usIndex = atoi(argv[1]);
    memset(&network_profile, 0, sizeof(network_profile));
    network_profile.ucSSIDLength = (strlen(argv[2]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[2]);
    memcpy((char *)network_profile.ucSSID, (char *)argv[2], network_profile.ucSSIDLength);
    bsp_safe_strcpy((char *)strbssid, (char *)argv[3], sizeof(strbssid));
    pbssid = (char *)strbssid;
    for (uint8_t i = 0; i < wificonfigMAX_BSSID_LEN; i++)
    {
        if (('\0' == *pbssid) || ('\0' == *(pbssid + 1)))
        {
            break;
        }
        network_profile.ucBSSID[i] = hex2byte(pbssid);
        pbssid += 3;
    }

    network_profile.xSecurity = parse_security(atoi(argv[4]));
    if (network_profile.xSecurity == eWiFiSecurityNotSupported)
    {
        return pdFALSE;
    }

    network_profile.ucPasswordLength = (strlen(argv[5]) > wificonfigMAX_PASSPHRASE_LEN) ? wificonfigMAX_PASSPHRASE_LEN : strlen(argv[5]);
    memcpy((char *)network_profile.cPassword, (char *)argv[5], network_profile.ucPasswordLength);

    ret = WIFI_NetworkAdd(&network_profile, &usIndex);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_network_get(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    return pdTRUE;
}

static bool cmd_network_delete(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    uint16_t usIndex;

    if (argc != 2)
    {
        return pdFALSE;
    }

    usIndex = atoi(argv[1]);

    ret = WIFI_NetworkDelete(usIndex);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_get_mac(int argc, const char ** argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    uint8_t          pucMac[RM_CLI_W_WIFI_MAC_LEN] = {0};

    ret = WIFI_GetMAC(pucMac);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x", pucMac[0], pucMac[1], pucMac[2], pucMac[3], pucMac[4], pucMac[5]);
    return pdTRUE;
}

static bool cmd_set_mac(int argc, const char ** argv)
{
    WIFIReturnCode_t ret;
    uint8_t          pucMacStr[RM_CLI_W_WIFI_MAC_LEN] = {0};
    uint8_t          pucMacHex[6]                   = {0};

    if ((argc != 2) || ((RM_CLI_W_WIFI_MAC_LEN - 1) != strlen(argv[1]))) /* -1 for null terminator */
    {
        return pdFALSE;
    }

    /* Convert the MAC address from string to hex */
    bsp_safe_strcpy((char *) pucMacStr, (char *) argv[1], RM_CLI_W_WIFI_MAC_LEN);
    for (int i = 0; i < 6; i++)
    {
        char   hex[3] = {pucMacStr[i * 3], pucMacStr[i * 3 + 1], '\0'}; /* discard ":" in pucMacStr[i * 3 + 2] */
        char * endptr;
        pucMacHex[i] = strtol(hex, &endptr, 16);
        if (*endptr != '\0')
        {
            return pdFALSE;
        }
    }

    /* Pass the MAC adress to the API */
    ret = WIFI_SetSpoofingMAC(pucMacHex);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }
    return pdTRUE;
}

static WIFISecurityWantedOrderExt_t convert_security_ext_to_wanted_order (WIFISecurityExt_t security)
{
    switch (security)
    {
        case eWiFiSecurityOpen_ext:
            return eWiFiSecurityWantedOrderOpen_ext;
        case eWiFiSecurityWEP_ext:
            return eWiFiSecurityWantedOrderWEP_ext;
        case eWiFiSecurityWPA_ext:
            return eWiFiSecurityWantedOrderWPA_ext;
        case eWiFiSecurityWPA2_ext:
            return eWiFiSecurityWantedOrderWPA2_ext;
        case eWiFiSecurityWPA_WPA2_ext:
            return eWiFiSecurityWantedOrderWPA_WPA2_ext;
        case eWiFiSecurityWPA3_ext:
            return eWiFiSecurityWantedOrderWPA3_ext;
        case eWiFiSecurityWPA2_WPA3_ext:
            return eWiFiSecurityWantedOrderWPA2_WPA3_ext;
        case eWiFiSecurityWPA3_OWE_ext:
            return eWiFiSecurityWantedOrderWPA3_OWE_ext;
        case eWiFiSecurityWPA_ent_ext:
	    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
                return eWiFiSecurityWantedOrderWPA_ent_ext;
	    break;
        case eWiFiSecurityWPA_WPA2_ent_ext:
	    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
                return eWiFiSecurityWantedOrderWPA_WPA2_ent_ext;
	    break;
        case eWiFiSecurityWPA2_WPA3_ent_ext:
	    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
                return eWiFiSecurityWantedOrderWPA2_WPA3_ent_ext;
	    break;
        case eWiFiSecurityWPA3_ent_ext:
	    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
                return eWiFiSecurityWantedOrderWPA3_ent_ext;
	    break;
        case eWiFiSecurityWPA3_192B_ent_ext:
	    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
                return eWiFiSecurityWantedOrderWPA3_192B_ent_ext;
	    break;
        default:
            return eWiFiSecurityWantedOrderMax_ext;
    }
        return eWiFiSecurityWantedOrderMax_ext;
}

static void print_scan_results(WIFIScanResult_t * pxBuffer, uint8_t ucNumNetworks)
{
    printf("\n");
    for (uint8_t i = 0; pxBuffer[i].ucSSIDLength && i < ucNumNetworks; i++)
    {
        printf("INDEX:%d ", i);
        printf("SSID_LENGTH:%d ", pxBuffer[i].ucSSIDLength);
        if ('\t' == pxBuffer[i].ucSSID[0])
        {
            printf("SSID:%s ", "[Hidden]");
        }
        else
        {
            printf("SSID:%s ", pxBuffer[i].ucSSID);
        }
        printf("BSSID:%02x:%02x:%02x:%02x:%02x:%02x ", pxBuffer[i].ucBSSID[0],
                                                       pxBuffer[i].ucBSSID[1],
                                                       pxBuffer[i].ucBSSID[2],
                                                       pxBuffer[i].ucBSSID[3],
                                                       pxBuffer[i].ucBSSID[4],
                                                       pxBuffer[i].ucBSSID[5]);
        switch ((WIFISecurityExt_t)pxBuffer[i].xSecurity)
        {
        case eWiFiSecurityOpen_ext:
            printf("SECURITY:No Security (0 for connectap) ");
            break;
        case eWiFiSecurityWEP_ext:
            printf("SECURITY:WEP Security (connectapwep) ");
            break;
        case eWiFiSecurityWPA_ext:
            printf("SECURITY:WPA Security (1 for connectap) ");
            break;
        case eWiFiSecurityWPA2_ext:
            printf("SECURITY:WPA2 Security (2 for connectap) ");
            break;
        case eWiFiSecurityWPA2_ent_ext:
            printf("SECURITY:WPA2 Enterprise Security (1 for connectapent) ");
            break;
        case eWiFiSecurityWPA3_ext:
            printf("SECURITY:WPA3 Security (3 for connectap) ");
            break;
        case eWiFiSecurityWPA_ent_ext:
            printf("SECURITY:WPA Enterprise Security (0 for connectapent) ");
            break;
        case eWiFiSecurityWPA_WPA2_ent_ext:
            printf("SECURITY:WPA + WPA2 Enterprise Security (2 for connectapent) ");
            break;
        case eWiFiSecurityWPA2_WPA3_ent_ext:
            printf("SECURITY:WPA2 + WPA3 Enterprise Security (3 for connectapent) ");
            break;
        case eWiFiSecurityWPA3_ent_ext:
            printf("SECURITY:WPA3 Enterprise Security (4 for connectapent) ");
            break;
        case eWiFiSecurityWPA3_192B_ent_ext:
            printf("SECURITY:WPA3 192B Enterprise Security (5 for connectapent) ");
            break;
        case eWiFiSecurityWPA_WPA2_ext:
            printf("SECURITY:WPA + WPA2 Security (4 for connectap) ");
            break;
        case eWiFiSecurityWPA2_WPA3_ext:
            printf("SECURITY:WPA2 + WPA3 Security (5 for connectap) ");
            break;
        case eWiFiSecurityWPA3_OWE_ext:
            printf("(WPA3 OWE Security) ");
            break;
        default:
            printf("(Unknown Security) ");
        }
        printf("RSSI:%d ", pxBuffer[i].cRSSI);
        printf("CHANNEL:%d\n", pxBuffer[i].ucChannel);
    }
}

static bool cmd_scan(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    WIFIScanResult_t * pxBuffer;
    uint8_t ucNumNetworks;

    if (argc != 2)
    {
        return pdFALSE;
    }

    ucNumNetworks = atoi(argv[1]);

    if (ucNumNetworks < 1)
    {
        printf("ERROR: [num_networks] should be positive\n");
        return pdFALSE;
    }

    pxBuffer = pvPortMalloc(sizeof(WIFIScanResult_t) * ucNumNetworks);
    if (NULL == pxBuffer)
    {
        return pdFALSE;
    }

    memset(pxBuffer, 0, sizeof(WIFIScanResult_t) * ucNumNetworks);

    ret = WIFI_Scan(pxBuffer, ucNumNetworks);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        vPortFree(pxBuffer);
        return pdFALSE;
    }

    print_scan_results(pxBuffer, ucNumNetworks);

    vPortFree(pxBuffer);

    return pdTRUE;
}

static bool cmd_scan_extended(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    WIFIScanResult_t * pxBuffer;
    uint8_t ucNumNetworks;
    WIFIScanExtendedConfig_t scan_config_extended;

    if (argc != 4)
    {
        return pdFALSE;
    }

    ucNumNetworks = atoi(argv[1]);
    pxBuffer = pvPortMalloc(sizeof(WIFIScanResult_t) * ucNumNetworks);
    if (NULL == pxBuffer)
    {
        return pdFALSE;
    }

    memset(pxBuffer, 0, sizeof(WIFIScanResult_t) * ucNumNetworks);
    if (0 == atoi(argv[2]))
    {
        scan_config_extended.pxScanConfig.ucChannel = atoi(argv[3]);
    }
    else
    {
        scan_config_extended.pxScanConfig.ucChannel = 0;
        scan_config_extended.ucBand = atoi(argv[3]);
    }

    ret = WIFI_ScanExtended(pxBuffer, ucNumNetworks, &scan_config_extended);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        vPortFree(pxBuffer);
        return pdFALSE;
    }

    print_scan_results(pxBuffer, ucNumNetworks);

    vPortFree(pxBuffer);

    return pdTRUE;
}

#define BAND_5G_CHAN_NUM_MIN 36
#define BAND_24G_CHAN_NUM_MAX 14

static e_wifi_phy_mode_ext_t set_default_wifi_mode(WIFINetworkParamsExt_t * net_params)
{
    if (net_params->xNetworkParams.xSecurity == eWiFiSecurityWPA)
    {
        if (net_params->xNetworkParams.ucChannel >= BAND_5G_CHAN_NUM_MIN)
        {
            return WIFI_MODE_A_ONLY;
        }
        else
        {
            return WIFI_MODE_BG;
        }
    }
    else if (net_params->xNetworkParams.ucChannel >= BAND_5G_CHAN_NUM_MIN)
    {
        return WIFI_MODE_AN;
    }
    else if (net_params->xNetworkParams.ucChannel <= BAND_24G_CHAN_NUM_MAX)
    {
        return WIFI_MODE_BGN;
    }
    else
    {
        return WIFI_MODE_MAX;
    }
}

static bool cmd_configure_ap(int argc, const char **argv)
{
    bool ret = pdTRUE;
    WIFIReturnCode_t err;
    WIFINetworkParamsExt_t * net_params = NULL;

    if (argc < 7 || argc > 9)
    {
        return pdFALSE;
    }

    net_params = pvPortMalloc(sizeof(WIFINetworkParamsExt_t));
    if (!net_params)
        return pdFALSE;

    memset(net_params, 0, sizeof(WIFINetworkParamsExt_t));

    net_params->xNetworkParams.ucSSIDLength =
        (strlen(argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[1]);
    memcpy(net_params->xNetworkParams.ucSSID, argv[1],
           net_params->xNetworkParams.ucSSIDLength);

    net_params->ucBand = atoi(argv[2]);
    if (net_params->ucBand > eWiFiBandDual)
    {
        ret = pdFALSE;
        goto fail;
    }

    net_params->xNetworkParams.xSecurity = parse_security(atoi(argv[3]));
    if (net_params->xNetworkParams.xSecurity == eWiFiSecurityNotSupported)
    {
        ret = pdFALSE;
        goto fail;
    }

    net_params->xNetworkParams.xPassword.xWPA.ucLength =
        (strlen(argv[4]) > wificonfigMAX_PASSPHRASE_LEN) ? wificonfigMAX_PASSPHRASE_LEN : strlen(argv[4]);
    memcpy((char *) net_params->xNetworkParams.xPassword.xWPA.cPassphrase, argv[4],
           net_params->xNetworkParams.xPassword.xWPA.ucLength);

    net_params->xNetworkParams.ucChannel = (uint8_t) atoi(argv[5]);
    net_params->ucWiFi_mode = (uint8_t) atoi(argv[6]) ? (uint8_t) atoi(argv[6]) - GAP_USER_CONFIGURE_WIFI_MODE :
                              set_default_wifi_mode(net_params);
    if (net_params->ucWiFi_mode >= WIFI_MODE_MAX)
    {
        ret = pdFALSE;
        goto fail;
    }

    net_params->pmf = argc >= 8 ? atoi(argv[7]) : PMF_DEFAULT;
    if (net_params->pmf > PMF_DEFAULT)
    {
        ret = pdFALSE;
        goto fail;
    }

    if (argc == 9)
    {
        if (atoi(argv[8]) > 4)
        {
            ret = pdFALSE;
            goto fail;
        }
        else
        {
            parse_sae_groups(net_params->sae_groups, atoi(argv[8]));
        }
    }

    err = WIFI_ConfigureAPExt(net_params);
    if (eWiFiSuccess != err)
    {
        printf("%s: WIFI_ConfigureAPExt failed with wifi_err=%d\n", __func__, ret);
        ret = pdFALSE;
    }

fail:
    vPortFree(net_params);
    return ret;
}

static bool cmd_is_connected(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    WIFINetworkParams_t network_params = {0};

    if (argc != 5)
    {
        return pdFALSE;
    }

    network_params.ucSSIDLength = (strlen(argv[1]) > wificonfigMAX_SSID_LEN) ? wificonfigMAX_SSID_LEN : strlen(argv[1]);
    memcpy((char *)network_params.ucSSID, argv[1], network_params.ucSSIDLength);
    network_params.xSecurity = atoi(argv[2]);
    network_params.xPassword.xWPA.ucLength = (strlen(argv[3]) > wificonfigMAX_PASSPHRASE_LEN) ? wificonfigMAX_PASSPHRASE_LEN : strlen(argv[3]);
    memcpy((char *)network_params.xPassword.xWPA.cPassphrase, argv[3], network_params.xPassword.xWPA.ucLength);
    network_params.ucChannel = atoi(argv[4]);

    ret = WIFI_IsConnected(&network_params);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_start_scan(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    return pdTRUE;
}

static bool cmd_get_scan_results(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    return pdTRUE;
}

static bool cmd_start_connect_ap(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    return pdTRUE;
}

static bool cmd_get_connection_info(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    WIFIConnectionInfoExt_t connection_info_ext[2];

    int wifi_security;

    ret = WIFI_GetConnectionInfoExt(connection_info_ext, 2);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    for (int i = 0; i < 2; i++)
    {
        char ssid[wificonfigMAX_SSID_LEN + 1];

        memcpy(ssid, connection_info_ext[i].connection_info.ucSSID, connection_info_ext[i].connection_info.ucSSIDLength);
        ssid[connection_info_ext[i].connection_info.ucSSIDLength] = '\0';

        if (connection_info_ext[i].connection_info.ucSSIDLength == 0)
        {
            continue;
        }

        printf("Interface %d:\n", i);
        printf("SSID_LENGTH:%d ", connection_info_ext[i].connection_info.ucSSIDLength);
        printf("SSID:%s ", ssid);
        printf("BSSID:%02x:%02x:%02x:%02x:%02x:%02x ", connection_info_ext[i].connection_info.ucBSSID[0],
                                                       connection_info_ext[i].connection_info.ucBSSID[1],
                                                       connection_info_ext[i].connection_info.ucBSSID[2],
                                                       connection_info_ext[i].connection_info.ucBSSID[3],
                                                       connection_info_ext[i].connection_info.ucBSSID[4],
                                                       connection_info_ext[i].connection_info.ucBSSID[5]);

        wifi_security = convert_security_ext_to_wanted_order((WIFISecurityExt_t)connection_info_ext[i].connection_info.xSecurity);
        printf("SECURITY:%d", wifi_security + GAP_USER_CONFIGURE_SECURITY);
        switch ((WIFISecurityExt_t)connection_info_ext[i].connection_info.xSecurity)
        {
        case eWiFiSecurityOpen:
            printf("(No Security) ");
            break;
        case eWiFiSecurityWEP:
            printf("(WEP Security) ");
            break;
        case eWiFiSecurityWPA:
            printf("(WPA Security) ");
            break;
        case eWiFiSecurityWPA2:
            printf("(WPA2 Security) ");
            break;
        case eWiFiSecurityWPA2_ent:
            printf("(WPA2 Enterprise Security) ");
            break;
        case eWiFiSecurityWPA3:
            printf("(WPA3 Security) ");
            break;
        case eWiFiSecurityWPA_ent_ext:
            printf("(WPA Enterprise Security) ");
            break;
        case eWiFiSecurityWPA_WPA2_ent_ext:
            printf("(WPA/WPA2 Enterprise Security) ");
            break;
        case eWiFiSecurityWPA2_WPA3_ent_ext:
            printf("(WPA2/WPA3 Enterprise Security) ");
            break;
        case eWiFiSecurityWPA3_ent_ext:
            printf("(WPA3 Enterprise Security) ");
            break;
        case eWiFiSecurityWPA3_192B_ent_ext:
            printf("(WPA3 192B Enterprise Security) ");
            break;
        case eWiFiSecurityWPA_WPA2_ext:
            printf("(WPA/WPA2-PSK Security) ");
            break;
        case eWiFiSecurityWPA2_WPA3_ext:
            printf("(WPA2-PSK+WPA3-SAE Security) ");
            break;
        case eWiFiSecurityWPA3_OWE_ext:
            printf("(WPA3 OWE Security) ");
            break;
        default:
            printf("(Unknown Security) ");
        }
        printf("CHANNEL:%d\n", connection_info_ext[i].connection_info.ucChannel);

        if (connection_info_ext[i].wifi_generation)
        {
            printf("WIFI GENERATION:%u\n", connection_info_ext[i].wifi_generation);
        }
    }

    return pdTRUE;
}

static bool cmd_get_rssi(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    int8_t rssi;

    ret = WIFI_GetRSSI(&rssi);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("RSSI:%d\n", rssi);

    return pdTRUE;
}

static bool cmd_get_station_list(int argc, const char **argv)
{
    WIFIReturnCode_t    ret;
    WIFIStationInfo_t * pxStationList;
    uint8_t             get_sta_num;

    if (argc != 2)
    {
        return pdFALSE;
    }

    get_sta_num = (uint8_t) atoi(argv[1]);

    if (get_sta_num < 1)
    {
        printf("ERROR: [max_station_list_size] should be positive\n");
        return pdFALSE;
    }

    pxStationList = pvPortMalloc(sizeof(WIFIStationInfo_t) * get_sta_num);
    if (NULL == pxStationList)
    {
        return pdFALSE;
    }

    memset(pxStationList, 0, sizeof(WIFIStationInfo_t) * get_sta_num);

    ret = WIFI_GetStationList(pxStationList, &get_sta_num);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        vPortFree(pxStationList);
        return pdFALSE;
    }

    printf("STATION_LIST_SIZE:%d\n", get_sta_num);
    for (uint8_t i = 0; i < get_sta_num; i++)
    {
        printf("MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x\n",
                pxStationList[i].ucMAC[0], pxStationList[i].ucMAC[1], pxStationList[i].ucMAC[2],
                pxStationList[i].ucMAC[3], pxStationList[i].ucMAC[4], pxStationList[i].ucMAC[5]);
    }

    vPortFree(pxStationList);

    return pdTRUE;
}

static bool cmd_set_country_code(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if (argc != 2)
    {
        return pdFALSE;
    }

    ret = WIFI_SetCountryCode(argv[1]);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_get_country_code(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    char cCountryCode[COUNTRY_CODE_LEN + 1];

    ret = WIFI_GetCountryCode(cCountryCode);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("COUNTRY_CODE:%s\n", cCountryCode);

    return pdTRUE;
}

static bool cmd_p2p_find(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_P2PFind();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_p2p_connect(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if(argc != 3)
    {
       return pdFALSE; 
    }

    ret = WIFI_P2PConnect(argv[1], argv[2]);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }
    
    return pdTRUE;
}

static bool cmd_p2p_group_remove(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_P2PGroupRemove();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_p2p_group_add(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_P2PGroupAdd();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_p2p_peers(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    char * reply;
 
    reply = pvPortMalloc(P2P_CMD_RESP_LEN);

    if (NULL == reply)
    {
        return pdFALSE;
    }
    ret = WIFI_P2PPeers(reply);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("%s\n", reply);
    vPortFree(reply);

    return pdTRUE;
}

static bool cmd_p2p_accept(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;

    ret = WIFI_P2PAccept();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_p2p_set(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    unsigned char value;
    unsigned char set_param;
    char nvram_field[P2P_SET_LEN];

    if (argc != 3)
    {
        return pdFALSE;
    }

    set_param = atoi(argv[1]);
    value = atoi(argv[2]);

    switch (set_param)
    {
    case 0:
        ret = WIFI_P2PSetOperChan(value);
        bsp_safe_strcpy(nvram_field, "channel", sizeof(nvram_field));
        break;

    case 1:
        ret = WIFI_P2PSetListenChan(value);
        bsp_safe_strcpy(nvram_field, "p2p_listen_chan", sizeof(nvram_field));
        break;
    case 2:
        ret = WIFI_P2PSetGoIntent(value);
        bsp_safe_strcpy(nvram_field, "p2p_go_intent", sizeof(nvram_field));
        break;
    case 3:
        ret = WIFI_P2PSetFindTimeout(value);
        bsp_safe_strcpy(nvram_field, "p2p_find_timeout", sizeof(nvram_field));
        break;
    case 4:
        ret = WIFI_P2PSetSsidPostfix(argv[2]);
        bsp_safe_strcpy(nvram_field, "p2p_ssid_postfix", sizeof(nvram_field));
        break;

    default:
        printf("ERROR: Invalid parameter\n set: 0:oper_channel 1:listen_channel 2:go_intent 3:find_timeout 4:ssid_postfix\n");

        return pdFALSE;
    }

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

#ifdef RM_MAP_PERSISTANT_W
if (set_param == 4)
{
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, nvram_field, argv[2]);
}
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, nvram_field, value);
#endif

    return pdTRUE;
}


static bool cmd_p2p_get(int argc, const char **argv)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    WIFIReturnCode_t ret;
    char P2PConfig[P2P_GET_LEN];

    ret = WIFI_P2PGet(P2PConfig);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }
    
    printf("P2PGET:%s\n", P2PConfig);
    return pdTRUE;
}

static bool cmd_isolate(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    char c;

    if (argc != 2)
    {
       return pdFALSE;
    }

    c = argv[1][0];

    if ((c != '0') && (c != '1'))
    {
        return pdFALSE;
    }

    ret = WIFI_Isolate(ctoi(&c));

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_acl_mac(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if (argc != 2)
    {
       return pdFALSE; 
    }

    ret = WIFI_AclMac(argv[1]);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }
    
    return pdTRUE;
}

static bool cmd_acl(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if (argc != 2)
    {
       return pdFALSE; 
    }

    ret = WIFI_Acl(argv[1]);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }
    
    return pdTRUE;
}

static bool cmd_wmm(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    char c;

    if (argc != 2)
    {
       return pdFALSE;
    }

    c = argv[1][0];

    if ((c != '0') && (c != '1'))
    {
        return pdFALSE;
    }

    ret = WIFI_WMM(ctoi(&c));

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_wmm_ps(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    char c;

    if (argc != 2)
    {
       return pdFALSE;
    }

    c = argv[1][0];

    if ((c != '0') && (c != '1'))
    {
        return pdFALSE;
    }

    ret = WIFI_WMM_PS(ctoi(&c));

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_ps_mode_config_set(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    char c;

    if (argc != 2)
    {
       return pdFALSE; 
    }

    c = argv[1][0];
    
    if ((c != '0') && (c != '1'))
    {
        return pdFALSE;
    }

    ret = WIFI_SetPsMode(ctoi(&c));
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_ps_mode_config_get(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    bool ps_mode;

    RA6W1_UNUSED_ARG(argv);

    if (argc != 1)
    {
       return pdFALSE; 
    }

    ret = WIFI_GetPsMode(&ps_mode);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("WIFI PS MODE config %u", ps_mode);

    return pdTRUE;
}

static bool cmd_listen_interval_set(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    int interval;

    if (argc != 2)
    {
       return pdFALSE; 
    }

    interval = atoi(argv[1]);
    // atoi returns 0 when input is invalid
    if (interval == 0 && argv[1][0] != '0')
    {
        return pdFALSE;
    }

    if (interval > 65535)
    {
        return pdFALSE;
    }

    ret = WIFI_SetListenInterval(interval);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_wps_pbc(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if (argc != 2)
    {
       return pdFALSE; 
    }

    ret = WIFI_WpsPbc(argv[1]);
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_wps_pin(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    if (argc != 2 && argc != 3)
    {
       return pdFALSE; 
    }

    if (argc == 2)
    {
        char reply[RM_WIFI_PIN_BUFF_SIZE] = {0};

        ret = WIFI_WpsPin(argv[1], NULL, reply);
        printf("Generated pin = %s\n", reply);
    } else {
        ret = WIFI_WpsPin(argv[1], argv[2], NULL);
    }

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_wps_cancel(int argc, const char **argv)
{
    WIFIReturnCode_t ret;

    RA6W1_UNUSED_ARG(argv);

    if (argc != 1)
    {
       return pdFALSE; 
    }

    ret = WIFI_WpsCancel();
    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_wps_ap_pin(int argc, const char **argv)
{
    WIFIReturnCode_t ret;
    char reply[RM_WIFI_PIN_BUFF_SIZE] = {0};

    if (argc != 2 && argc != 3 && argc != 4)
    {
       return pdFALSE; 
    }

    if (argc == 2) {
        ret = WIFI_WpsApPin(argv[1], NULL, NULL, reply);
    }
    else if (argc == 3) {
        ret = WIFI_WpsApPin(argv[1], argv[2], NULL, reply);
    }
    else {
        ret = WIFI_WpsApPin(argv[1], argv[2], argv[3], reply);
    }

    if (eWiFiSuccess != ret)
    {
        printf("ERROR:%d\n", ret);
        return pdFALSE;
    }

    printf("%s\n", reply);

    return pdTRUE;
}

#define MAC_TWT_SETUP_SUGGEST 1

static bool cmd_twt_setup(int argc, const char **argv)
{
    struct twt_setup_req params = {
        .vif_idx = 0,
        .setup_type = MAC_TWT_SETUP_SUGGEST,
    };

    if (argc == 8) {
        params.conf.wake_int_mantissa = ctoi((char *)argv[1]);
        params.conf.wake_int_exp      = ctoi((char *)argv[2]);
        params.conf.min_twt_wake_dur  = ctoi((char *)argv[3]);
        params.conf.wake_dur_unit     = 1;
        params.conf.flow_type         = ctoi((char *)argv[4]);
        params.conf.trigger           = ctoi((char *)argv[5]);
        params.conf.neg_type          = ctoi((char *)argv[6]);
        params.auto_setup             = ctoi((char *)argv[7]);

	if (!twt_setup_req_valid(&params)) {
              printf("ERROR: Invalid set-up request parameters\n");
              return pdFALSE;
        } else {
              WIFI_TwtSetup(&params);
              return pdTRUE;
        }
    }
    return pdFALSE;
}

static bool cmd_twt_teardown(int argc, const char **argv)
{
    struct twt_teardown_req params = {0};

    if (argc == 2) {
        params.neg_type = ctoi((char*) argv[1]);
        params.all_twt = 1;
        return (eWiFiSuccess == WIFI_TwtTeardown(&params));
    }
    return pdFALSE;
}

static const debug_handler_t wifi_handlers[] = {
    { "disconnect",         "",                          (debug_callback_t)cmd_disconnect                },
    { "startdisconnect",    "",                          (debug_callback_t)cmd_start_disconnect          },
    { "disconnectstation",  "[mac_addr]",                (debug_callback_t)cmd_disconnect_station        },
    { "on",                 "",                          (debug_callback_t)cmd_on                        },
    { "onauto",             "",                          (debug_callback_t)cmd_on_auto                   },
    { "off",                "",                          (debug_callback_t)cmd_off                       },
    { "reset",              "",                          (debug_callback_t)cmd_reset                     },
    { "startap",            "",                          (debug_callback_t)cmd_start_ap                  },
    { "stopap",             "",                          (debug_callback_t)cmd_stop_ap                   },
    { "connectap",          "<ssid> <band> <security> <passphrase> [<hidden> [<pmf> [<sae groups> [<channels>]]] <bssid>]\n"
                            "\t\tBand:           0: 2.4G, 1: 5G, 2: Dual band\n"
                            "\t\tSecurity modes: 0: No Security, 1: WPA, 2: WPA2, 3: WPA3, 4: WPA + WPA2, 5: WPA2 + WPA3\n"
                            "\t\tHidden SSID:    0: Not hidden, 1: Hidden.\n"
                            "\t\tPMF modes:      0: Disable, 1: Optional, 2: Mandatory, 3: Default\n"
                            "\t\tSAE groups:     0: Not configured, 1: 19, 2: 20, 3: 21, 4: 19/20/21\n"
                            "\t\tChannels:       ch=<ch1>[,<ch2>[,<ch3>...]] (e.g., ch=1,2,6)\n"
                            "\t\tBSSID:          bssid=<xx:xx:xx:xx:xx:xx>\n"
                            "\t\tExamples:\n"
                            "\t\t1. wifi connectap SSID_24G 0 1 12345678 0 0 0\n"
                            "\t\t2. wifi connectap SSID_5G 1 3 12345678\n"
                            "\t\t3. wifi connectap SSID_5G 1 3 12345678 0\n"
                            "\t\t4. wifi connectap SSID_5G 1 3 12345678 0 2\n"
                            "\t\t5. wifi connectap SSID_5G 1 3 12345678 0 2 4\n"
                            "\t\t6. wifi connectap SSID_5G 1 3 12345678 0 2 4 ch=36,40,44\n"
                            "\t\t7. wifi connectap SSID_5G 1 3 12345678 0 20 4 ch=36,40,44 bssid=xx:xx:xx:xx:xx:xx",
                                                         (debug_callback_t)cmd_connect_ap                },
    { "connectapwep",       "<ssid> <band> <hidden> <wep_key_index> <wep_key0> [<wep_key1> [<wep_key2> [<wep_key3>] [<channels>]]]\n"
                            "\t\tBand:                 0: 2.4G, 1: 5G, 2: Dual band\n"
                            "\t\tHidden SSID:          0: Not hidden, 1: Hidden.\n"
                            "\t\tWEP key index:        Between 0 and 3\n"
                            "\t\tWEP key0 to WEP key3: ASCII key [length - 5(64 bit) or 13(128 bit)] / "
                            "Hexadecimal key [length - 10(64 bit) or 26(128 bit)]\n"
			    "\t\tChannels:       ch=<ch1>[,<ch2>[,<ch3>...]]  (e.g., ch=1,2,6)\n"
                            "\t\tExamples:\n"
                            "\t\t1. wifi connectapwep SSID_WEP 0 0 MySecurePass12\n"
                            "\t\t2. wifi connectapwep SSID_WEP 0 1 MySecurePass12 MySecurePass23\n"
                            "\t\t3. wifi connectapwep SSID_WEP 0 2 MySecurePass12 MySecurePass23 MySecurePass34\n"
                            "\t\t4. wifi connectapwep SSID_WEP 0 3 MySecurePass12 MySecurePass23 MySecurePass34 MySecurePass45\n"
			    "\t\t4. wifi connectapwep SSID_WEP 0 3 MySecurePass12 MySecurePass23 MySecurePass34 MySecurePass45 ch=36,40,44",
                                                         (debug_callback_t)cmd_connect_ap_wep            },
    { "connectapent",       "<ssid> <band> <security> <auth_type> <auth_proto> <identity> <password> [<certificate> <private_key>][<channels>]\n"
                            "\t\tBand:           0: 2.4G, 1: 5G, 2: Dual band\n"
                            "\t\tSecurity modes: 0: WPA Enterprise, 1: WPA2 Enterprise, 2: WPA + WPA2 Enterprise, "
                            "3: WPA2 + WPA3 Enterprise, 4: WPA3 Enterprise (128-bit), 5: WPA3 192-bit Enterprise (192-bit)\n"
                            "\t\tAuth Type:      0: PEAP or TTLS or FAST (Recommend), 1: PEAP, 2: TTLS, 3: FAST, 4: TLS\n"
                            "\t\tAuth Proto:     0: NONE, 1:MSCHAPv2 or GTC (Recommend), 2: MSCHAPv2, 3: GTC, 4: TLS\n"
                            "\t\tPassword:       If no password, enter 0\n"
                            "\t\tCertificate:    0: No certificate, 1: Enter certificate\n"
                            "\t\tPrivate key:    0: No private key, 1: Enter private key\n"
			    "\t\tChannels:       ch=<ch1>[,<ch2>[,<ch3>...]]  (e.g., ch=1,2,6)\n"
                            "\t\tExamples:\n"
                            "\t\t1. wifi connectapent SSID_WPA2_ENT_5G 1 1 3 0 UserID SecurePass\n"
                            "\t\t2. wifi connectapent SSID_WPA2_ENT_5G 1 1 4 0 UserID 0 1 1\n"
                            "\t\t3. wifi connectapent SSID_WPA2_ENT_5G 1 1 1 4 UserID 0 1 1\n"
                            "\t\t4. wifi connectapent SSID_WPA2_ENT_5G 1 1 0 1 UserID SecurePass\n"
			    "\t\t4. wifi connectapent SSID_WPA2_ENT_5G 1 1 0 1 UserID SecurePass ch=36,40,44",
                                                         (debug_callback_t)cmd_connect_ap_ent            },
    { "autoconfig",         "<ssid> <security> [passphrase]\n"
                            "\t\tSecurity modes: 0: No Security, 1: WPA (unsupported), 2: WPA2, 3: WPA3 (unsupported), "
                            "4: WPA + WPA2 (unsupported), 5: WPA2 + WPA3 (unsupported)",
                                                         (debug_callback_t)cmd_auto_config               },
    { "setmode",            "[wifi_device_mode_ext]\n[wifi_device_mode_ext] 0: Station mode, 1: Access point mode, "
                            "2: P2P mode, 3: P2P GO mode, 4: AP+Station mode, 5: P2P+Station mode, 6: Mesh point mode, "
                            "7: Mesh portal mode",
                                                         (debug_callback_t)cmd_set_mode                  },
    { "getmode",            "",                          (debug_callback_t)cmd_get_mode                  },
    { "networkadd",         "[index] [ssid] [bssid] [security] [passphrase]\n"
                            "\t\t[security] 0: No Security(unsupported), 1: WPA (unsupported), 2: WPA2, 3: WPA3 (unsupported), "
                            "4: WPA + WPA2 (unsupported), 5: WPA2 + WPA3 (unsupported)\n"
                            "\t\tExample: wifi networkadd 0 SSID_WPA2 00:1a:2b:3c:4d:5e 2 12345678\n",
                                                         (debug_callback_t)cmd_network_add               },
    { "networkget",         "",                          (debug_callback_t)cmd_network_get               },
    { "networkdelete",      "[index]",                   (debug_callback_t)cmd_network_delete            },
    { "getmac",             "",                          (debug_callback_t)cmd_get_mac                   },
    { "setmac",             "[mac_addr]\n[mac_addr] MAC address. Example: 00:11:22:FF:EE:DD",
                                                         (debug_callback_t)cmd_set_mac                   },
    { "scan",               "[num_networks]",            (debug_callback_t)cmd_scan                      },
    { "scanextended",       "[num_networks] [scan_type] [channel | band]\n"
                            "\t\t[scan_type] 0:Channel, 1:Band\n"
                            "\t\t[band] 0: 2.4G band, 1: 5G band, 2:Dual band", (debug_callback_t)cmd_scan_extended             },
    { "configureap",        "<ssid> <band> <security> <passphrase> <channel> <wifi_mode> [<pmf> [<sae groups>]]\n"
                            "\t\tBand:          0: 2.4G, 1: 5G, 2: Dual band\n"
                            "\t\tSecurity modes 0: No Security, 1: WPA, 2: WPA2, 3: WPA3, 4: WPA + WPA2, 5: WPA2 + WPA3\n"
                            "\t\tWifi Mode:     0: Default, 1: 11b/g/n (2.4GHz), 2: 11g/n (2.4GHz), 3: 11b/g (2.4GHz), "
                            "4: 11n only (2.4GHz), 5: 11g only (2.4GHz), 6: 11b only (2.4GHz), 7: 11a/n (5GHz), 8: 11a only (5GHz) "
                            "9: 11n only (5GHz)\n"
                            "\t\tPMF modes:     0: Disable, 1: Optional, 2: Mandatory, 3: Default\n"
                            "\t\tSAE groups:     0: Not configured, 1: 19, 2: 20, 3: 21, 4: 19/20/21\n"
                            "\t\tExamples:\n"
                            "\t\t1. wifi configureap SSID_WPA2_24G 0 2 12345678 1 0\n"
                            "\t\t2. wifi configureap SSID_WPA3_24G 0 3 12345678 1 1 2 4\n"
                            "\t\t3. wifi configureap SSID_WPA12_5G 1 4 12345678 36 8 1\n"
                            "\t\t4. wifi configureap SSID_WPA23_5G 2 5 12345678 36 7 3 1\n",
                                                         (debug_callback_t)cmd_configure_ap              },
    { "isconnected",        "[ssid] [security] [passphrase] [channel]\n"
                            "\t\t[security] 0:No Security, 1:WEP Security, 2:WPA Security, 3:WPA2 Security, 4:WPA2 Enterprise Security, 5:WPA3 Security", (debug_callback_t)cmd_is_connected              },
    { "startscan",          "",                          (debug_callback_t)cmd_start_scan                },
    { "getscanresults",     "",                          (debug_callback_t)cmd_get_scan_results          },
    { "startconnectap",     "",                          (debug_callback_t)cmd_start_connect_ap          },
    { "getconnectioninfo",  "",                          (debug_callback_t)cmd_get_connection_info       },
    { "getrssi",            "",                          (debug_callback_t)cmd_get_rssi                  },
    { "getstationlist",     "[max_station_list_size]",   (debug_callback_t)cmd_get_station_list          },
    { "setcountrycode",     "[country_code]",            (debug_callback_t)cmd_set_country_code          },
    { "getcountrycode",     "",                          (debug_callback_t)cmd_get_country_code          },
    { "p2pfind",            "",                          (debug_callback_t)cmd_p2p_find                  },
    { "p2pconnect",         "[p2p_addr] [pbc|PIN]",       (debug_callback_t)cmd_p2p_connect              },
    { "p2pgroupremove",     "",                          (debug_callback_t)cmd_p2p_group_remove          },
    { "p2pgroupadd",        "",                          (debug_callback_t)cmd_p2p_group_add             },
    { "p2ppeers",           "",                          (debug_callback_t)cmd_p2p_peers                 },
    { "p2paccept",          "",                          (debug_callback_t)cmd_p2p_accept                },
    { "p2pset",             "set <param_id> <value>\n"
                            "\t\t<param_id>: Parameter to configure:\n"
                            "\t\t  0: oper_channel      Operating channel for P2P\n"
                            "\t\t  1: listen_channel    Listening channel for P2P discovery\n"
                            "\t\t  2: go_intent         Group Owner intent (0-15)\n"
                            "\t\t  3: find_timeout      Timeout (in seconds) for discovery\n"
                            "\t\t  4: ssid_postfix      SSID postfix for P2P group name\n"
                            "\t\tExamples:\n"
                            "\t\t  wifi p2pset 0 6\n"
                            "\t\t  wifi p2pset 2 7\n",
                                                         (debug_callback_t)cmd_p2p_set                   },
    { "p2pget",             "",                          (debug_callback_t)cmd_p2p_get                   },
    { "isolate",            "[enable_isolation]",        (debug_callback_t)cmd_isolate                   },
    { "aclmac",             "[mac_addr]",                (debug_callback_t)cmd_acl_mac                   },
    { "acl",                "[filter_mode]",             (debug_callback_t)cmd_acl                       },
    { "wmm",                "[enable_wmm]",              (debug_callback_t)cmd_wmm                       },
    { "wmm_ps",             "[enable_wmm_ps]",           (debug_callback_t)cmd_wmm_ps                    },
    { "wps_pbc",            "[mac_addr]",                (debug_callback_t)cmd_wps_pbc                   },
    { "wps_pin",            "[mac_addr][pin(optional)]\n"
                            "\t\tPin - if not specified, then generated pin would be printed.\n",
                                                         (debug_callback_t)cmd_wps_pin                   },
    { "wps_cancel",         "",                          (debug_callback_t)cmd_wps_cancel                },
    { "wps_ap_pin",         "[state][arg1(optional)][arg2(optional)]\n"
                            "\t\tState - 'disable', 'random', 'get' or 'set'.\n"
                            "\t\tArg1 - For 'random': timeout. For 'set': PIN to assign.\n"
                            "\t\tArg2 - For 'set': timeout in seconds.\n",
                                                         (debug_callback_t)cmd_wps_ap_pin                },
    { "twt_setup",          "[mantissa] [exponent] [min_twt] [flow_type] [trigger] [negot_type] [auto_setup]",
                                                         (debug_callback_t)cmd_twt_setup                 },
    { "twt_teardown",       "[negot_type]",              (debug_callback_t)cmd_twt_teardown              },
    { "ps_mode_set",        "[enable]  0 - disabled, 1 - enabled",
                                                         (debug_callback_t)cmd_ps_mode_config_set        },
    { "ps_mode_get",        "",                          (debug_callback_t)cmd_ps_mode_config_get        },
    { "listen_interval",    "[listen_interval] 0:65535", (debug_callback_t)cmd_listen_interval_set       },

    { NULL },
};

bool wifi_command(int argc, const char *argv[], void *user_data)
{
    FSP_PARAMETER_NOT_USED(user_data);

    return debug_handle_message(argc, argv, wifi_handlers);
}
