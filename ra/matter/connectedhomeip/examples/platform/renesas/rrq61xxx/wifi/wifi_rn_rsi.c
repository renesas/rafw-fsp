/*
 *
 *    Copyright (c) 2023 Renesas Electronics Corporation and/or its affiliates
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0

 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#if 1 //[rrq61000 matter work]
#include "rm_wifi_api.h"
#else
#include "da16x_network_common.h"
#endif

#if 1 //[rrq61000 matter work]
//#include "cli_net.h"  //must check again
#else
#include "command_net.h"
#endif

#include "wifi_rn_events.h"
#include "wifi_rn_rsi.h"

/*----------------------------------------------------------------------------------------
 * @brief Called from ConnectivityManagerImpl.cpp - to enable the device
 * @param[in]  None
 * @return  Returns RN_STATUS_OK if successful, RN_STATUS_FAIL otherwise
 ---------------------------------------------------------------------------------------*/
rn_status_t wifi_start(void)
{
    return RN_STATUS_OK;
}

/*----------------------------------------------------------------------------------------
 * @brief driver enable the STA mode
 * @param[in]  None
 * @return   None
 ---------------------------------------------------------------------------------------*/
void wifi_enable_sta_mode(void)
{
#if 1 //[rrq61000 matter work]
    WIFI_SetMode(eWiFiModeStation);
#else
    setSysMode(WIFI_DEVICE_MODE_EXT_STATION);
#endif
    wifi_rsi.dev_state |= WIFI_RSI_ST_STA_MODE;
}

/*----------------------------------------------------------------------------------------
 * @brief driver enabled the STA mode
 * @param[in]  None
 * @return   mode
 ---------------------------------------------------------------------------------------*/
bool wifi_is_sta_mode_enabled(void)
{
#if 1 //[rrq61000 matter work]
    WIFIDeviceMode_t wifi_Mode;
    WIFI_GetMode(&wifi_Mode);
    int mode = (wifi_Mode != eWiFiModeStation) ? false : true; // change due to fault exception
#else
    int mode = (getSysMode() != WIFI_DEVICE_MODE_EXT_STATION) ? false : true;
#endif

    return mode;
}

/*----------------------------------------------------------------------------------------
 * @brief get the wifi mac address
 * @param[in]  Interface:
 * @param[in]  addr : address
 * @return None
 ---------------------------------------------------------------------------------------*/
void wifi_get_wifi_mac_addr(wifi_interface_t interface, wifi_mac_address_t * addr)
{
    wifi_mac_address_t * mac;

    int status = 0;
    unsigned long macmsw, maclsw;

    /* Get MAC Address */
#if __MATTER_FSP_TEMP_NO_API__
#else
    status = getMacAddrMswLsw(interface, &macmsw, &maclsw);
#endif
    if (((int) status) >= 0)
    {
        if (interface == RN_WIFI_SOFTAP_INTERFACE)
        {
            wifi_rsi.softap_mac.octet[0] = (macmsw >> 8);
            wifi_rsi.softap_mac.octet[1] = (macmsw & 0xff);
            wifi_rsi.softap_mac.octet[2] = (maclsw >> 24);
            wifi_rsi.softap_mac.octet[3] = (maclsw >> 16 & 0xff);
            wifi_rsi.softap_mac.octet[4] = (maclsw >> 8 & 0xff);
            wifi_rsi.softap_mac.octet[5] = (maclsw & 0xff);
        }
        else
        {
            wifi_rsi.sta_mac.octet[0] = (macmsw >> 8);
            wifi_rsi.sta_mac.octet[1] = (macmsw & 0xff);
            wifi_rsi.sta_mac.octet[2] = (maclsw >> 24);
            wifi_rsi.sta_mac.octet[3] = (maclsw >> 16 & 0xff);
            wifi_rsi.sta_mac.octet[4] = (maclsw >> 8 & 0xff);
            wifi_rsi.sta_mac.octet[5] = (maclsw & 0xff);
        }
    }

    mac   = (interface == RN_WIFI_SOFTAP_INTERFACE) ? &wifi_rsi.softap_mac : &wifi_rsi.sta_mac;
    *addr = *mac;
    WIFI_RSI_LOG("%s: %02x:%02x:%02x:%02x:%02x:%02x", __func__, mac->octet[0], mac->octet[1], mac->octet[2], mac->octet[3],
                 mac->octet[4], mac->octet[5]);
}

/*----------------------------------------------------------------------------------------
 * @brief Driver set the wifi provision
 * @param[in]  cfg - wifi configuration
 * @return None
 ---------------------------------------------------------------------------------------*/
void wifi_set_wifi_provision(wifi_provision_t * cfg)
{
    WIFI_RSI_LOG("%s: SSID: %s", __func__, &wifi_rsi.sec.ssid[0]);

    wifi_provision(cfg->ssid, cfg->passkey, cfg->security);
    wifi_rsi.sec = *cfg;
    wifi_rsi.dev_state |= WIFI_RSI_ST_STA_PROVISIONED;
}

/*----------------------------------------------------------------------------------------
 * @brief Driver get the wifi provision
 * @param[in]  wifiConfig - wifi configuration
 * @return  return false if successful, true otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_get_wifi_provision(wifi_provision_t * wifiConfig)
{
    if (wifiConfig != NULL)
    {
        if (wifi_rsi.dev_state & WIFI_RSI_ST_STA_PROVISIONED)
        {
            *wifiConfig = wifi_rsi.sec;
            return true;
        }
    }
    return false;
}

/*----------------------------------------------------------------------------------------
 * @brief Driver is clear the wifi provision
 * @param[in]  None
 * @return  None
 ---------------------------------------------------------------------------------------*/
void wifi_clear_wifi_provision(void)
{
    memset(&wifi_rsi.sec, 0, sizeof(wifi_rsi.sec));
    wifi_rsi.dev_state &= ~WIFI_RSI_ST_STA_PROVISIONED;
    WIFI_RSI_LOG("%s: completed.", __func__);
}

/*----------------------------------------------------------------------------------------
 * @brief Start a JOIN command to the AP - Done by the wifi_rsi task
 * @param[in]   None
 * @return  returns RN_STATUS_OK if successful, RN_STATUS_INVALID_CONFIGURATION otherwise
 ---------------------------------------------------------------------------------------*/
rn_status_t wifi_connect_to_ap(void)
{
#if defined(__RRQ61400__)
    return wifi_connect();
#else
    return 0;
#endif
}

/*---------------------------------------------------------------------------------------*
 * @brief Implement the ipv6 setup
 * @param[in]  whichif:
 * @return  None
 ---------------------------------------------------------------------------------------*/
void wifi_setup_ip6_link_local(wifi_interface_t whichif)
{
    WIFI_RSI_LOG("%s: warning: not implemented.", __func__);
}

/*----------------------------------------------------------------------------------------
 * @brief called fuction when driver is connected to STA
 * @param[in]  None
 * @return  returns ture if successful, false otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_is_sta_connected(void)
{
    bool status;

#if defined(__RRQ61400__)
    if (is_wifi_connect())
        wifi_rsi.dev_state &= ~WIFI_RSI_ST_STA_CONNECTED;
    else
        wifi_rsi.dev_state |= WIFI_RSI_ST_STA_CONNECTED;
#else
    wifi_rsi.dev_state |= WIFI_RSI_ST_STA_CONNECTED;
#endif
    status = (wifi_rsi.dev_state & WIFI_RSI_ST_STA_CONNECTED) ? true : false;
    WIFI_RSI_LOG("%s: status: %s", __func__, (status ? "connected" : "not connected"));

    return status;
}

/*----------------------------------------------------------------------------------------
 * @brief get the wifi mode
 * @param[in]  None
 * @return  return SYS_MODE_NULL if successful, SYS_MODE_STA otherwise
 ---------------------------------------------------------------------------------------*/
system_mode_t wifi_get_wifi_mode()
{
    wifi_rsi.dev_state |= WIFI_RSI_ST_DEV_READY;

    if (wifi_rsi.dev_state & WIFI_RSI_ST_DEV_READY)
        return SYS_MODE_STA;

    return SYS_MODE_NULL;
}

/*----------------------------------------------------------------------------------------
 * @brief called fuction when STA disconnected
 * @param[in]  None
 * @return  return RN_STATUS_OK if successful, RN_STATUS_FAIL otherwise
 ---------------------------------------------------------------------------------------*/
rn_status_t wifi_sta_discon(void)
{
    WIFI_RSI_LOG("%s: started.", __func__);
    int32_t status;
#if defined(__RRQ61400__)
    status = wifi_disconnect();
#else
    status = wifi_rsi_disconnect();
#endif
    wifi_rsi.dev_state &= ~WIFI_RSI_ST_STA_CONNECTED;
    WIFI_RSI_LOG("%s: completed.", __func__);
    return status;
}

#if CHIP_DEVICE_CONFIG_ENABLE_IPV4
/*----------------------------------------------------------------------------------------
 * @brief called fuction when driver have ipv4 address
 * @param[in]  which_if
 * @return  returns ture if successful, false otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_have_ipv4_addr(wifi_interface_t which_if)
{
    bool status = false;
    if (which_if == RN_WIFI_STA_INTERFACE)
    {
        status = (wifi_rsi.dev_state & WIFI_RSI_ST_STA_DHCP_DONE) ? true : false;
    }
    else
    {
        status = false;
    }
    WIFI_RSI_LOG("%s: status: %d", __func__, status);

    return status;
}
#endif /* CHIP_DEVICE_CONFIG_ENABLE_IPV4 */

/*----------------------------------------------------------------------------------------
 * @brief called fuction when driver have ipv6 address
 * @param[in]  which_if
 * @return  returns ture if successful, false otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_have_ipv6_addr(wifi_interface_t which_if)
{
    struct netif *cnetif = netif_list;
    int idx;
    bool status = false;

    for (idx = 0; cnetif != NULL; cnetif = cnetif->next, idx++)
    {
        if (idx == which_if)
        {
            if (ip6_addr_ispreferred(netif_ip6_addr_state(cnetif, 0)))
            {
                status = true;
            }
            break;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------------------
 * @brief called fuction when driver ready
 * @param[in]  None
 * @return  returns ture if successful, false otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_hw_ready(void)
{
    wifi_rsi.dev_state |= WIFI_RSI_ST_DEV_READY;

    return (wifi_rsi.dev_state & WIFI_RSI_ST_DEV_READY) ? true : false;
}

/*----------------------------------------------------------------------------------------
 * @brief get the access point information
 * @param[in]  ap - access point
 * @return access point information
 ---------------------------------------------------------------------------------------*/
int32_t wifi_get_ap_info(wifi_scan_result_t * ap)
{
    WIFI_RSI_LOG("%s:%d] ", __func__, __LINE__);

    return wifi_rsi_get_ap_info(ap);
}

/*----------------------------------------------------------------------------------------
 * @brief get the access point extra information
 * @param[in]  extra_info - access point extra information
 * @return access point extra information
 ---------------------------------------------------------------------------------------*/
int32_t wifi_get_ap_ext(wifi_scan_ext_t * extra_info)
{
    WIFI_RSI_LOG("%s:%d] ", __func__, __LINE__);

    return wifi_rsi_get_ap_ext(extra_info);
}

/*----------------------------------------------------------------------------------------
 * @brief get the driver reset count
 * @param[in]  None
 * @return reset count
 ---------------------------------------------------------------------------------------*/
int32_t wifi_reset_counts()
{
    WIFI_RSI_LOG("%s:%d] ", __func__, __LINE__);

    return wifi_rsi_reset_count();
}

/*----------------------------------------------------------------------------------------
 * @brief called fuction when driver start scaning
 * @param[in]  ssid
 * @return returns ture if successful, false otherwise
 ---------------------------------------------------------------------------------------*/
bool wifi_start_scan(char * ssid, void (*callback)(wifi_scan_result_t *))
{
    int sz;
    // scan test
    char *scan_res_text, *p;
    int scan_res_num    = -1;
    char scan_max_num   = 20;
    char ch             = 0;
    char result_str[16] = {
        0,
    };
    wifi_scan_result_t * scan_res_array;
    int status = 0;
    //

    WIFI_RSI_LOG("%s:%d] ", __func__, __LINE__);

    if (wifi_rsi.scan_cb)
        return false;
#if 0 // org
    if (ssid)
    {
        sz = strlen(ssid);
        if ((wifi_rsi.scan_ssid = (char *) pvPortMalloc(sz + 1)) == (char *) 0)
        {
            return false;
        }
        strcpy(wifi_rsi.scan_ssid, ssid);
    }
    wifi_rsi.scan_cb = callback;
    xEventGroupSetBits(wifi_rsi.events, WIFI_EVT_SCAN);
#else
    // scan test

    scan_res_array = (wifi_scan_result_t *) pvPortMalloc(sizeof(wifi_scan_result_t) * scan_max_num);
    if (scan_res_array == NULL)
    {
        WIFI_RSI_LOG("[%s] malloc failed. \n", __func__);
        return 0;
    }
    memset(scan_res_array, 0, sizeof(wifi_scan_result_t) * scan_max_num);

    scan_res_text = (char *) pvPortMalloc(4096);
    if (scan_res_text == NULL)
    {
        WIFI_RSI_LOG("[%s] malloc failed. \n", __func__);
        return 0;
    }
    memset(scan_res_text, '\0', 4096);

    /* cli scan */
    strcpy(result_str, "scan");
    // status = WIFI_Scan((WIFIScanResult_t *)result_str, scan_max_num); // need to change to this function TBD
    status = ra6w1_cli_reply(result_str, NULL, scan_res_text);
    if (status < 0 || strcmp(scan_res_text, "FAIL") == 0)
    {
        WIFI_RSI_LOG("[%s] Wi-Fi Scan request failed. \n", __func__);
        vPortFree(scan_res_text);
        return 0;
    }

    if (strtok(scan_res_text, "\n") == NULL) /* Title Skip */
    {
        vPortFree(scan_res_text);
        return 0;
    }

    for (scan_res_num = 0; scan_res_num < scan_max_num;)
    {
        if (strtok(NULL, "\t") == NULL) /* BSSID => Skip! */
        {
            break;
        }

        p = strtok(NULL, "\t"); /* frequency */
        if (atoi(p) >= 5000)
        {
            ch = (atoi(p) - 5000) / 5; // 5G  ,36ch ??
        }
        else
        {
            if (atoi(p) == 2484)
                ch = 14;
            else
                ch = (atoi(p) - 2407) / 5; // 2.4G, 14ch
        }
        scan_res_array[scan_res_num].chan = ch;

        p                                 = strtok(NULL, "\t"); /* RSSI */
        scan_res_array[scan_res_num].rssi = atoi(p);

        p = strtok(NULL, "\t"); /* Security */

        if (strstr(p, "WPA2"))
        {
            scan_res_array[scan_res_num].security = 4; /* WPA2 */
        }
        else if (strstr(p, "WPA"))
        {
            scan_res_array[scan_res_num].security = 3; /* WPA */
        }
        else if (strstr(p, "WEP"))
        {
            scan_res_array[scan_res_num].security = 2; /* WEP */
        }
        else
        {
            scan_res_array[scan_res_num].security = 1; /* OPEN */
        }

        p = strtok(NULL, "\n"); /* SSID */

        /* Discard hidden SSID. */
        if (p[0] == HIDDEN_SSID_DETECTION_CHAR)
        {
            continue;
        }

        memcpy(scan_res_array[scan_res_num].ssid, p, strlen(p));

        WIFI_RSI_LOG("[%s:%d] (%d) %s / %d / %d / %d \n", __func__, __LINE__, scan_res_num, scan_res_array[scan_res_num].ssid,
                     scan_res_array[scan_res_num].security, scan_res_array[scan_res_num].rssi, scan_res_array[scan_res_num].chan);
        if (ssid)
        {
            if (strcmp(ssid, scan_res_array[scan_res_num].ssid) == 0)
            {
                callback(&scan_res_array[scan_res_num]);
                break;
            }
        }
        else
        {
            callback(&scan_res_array[scan_res_num]);
        }
        scan_res_num++;
    }

    callback(NULL); // need to finish handle

#endif

    return true;
}

/*----------------------------------------------------------------------------------------
 * @brief called function when driver cancel scaning
 * @param[in]  None
 * @return  None
 ---------------------------------------------------------------------------------------*/
void wifi_cancel_scan(void)
{
    WIFI_RSI_LOG("%s:%d] ", __func__, __LINE__);

    WIFI_RSI_LOG("%s: cannot cancel scan", __func__);
}

int32_t wifi_rsi_get_ap_info(wifi_scan_result_t * ap)
{
    return RN_STATUS_FAIL;
}

int32_t wifi_rsi_get_ap_ext(wifi_scan_ext_t * extra_info)
{
    return RN_STATUS_FAIL;
}

int32_t wifi_rsi_reset_count(void)
{
    return RN_STATUS_FAIL;
}

int32_t wifi_rsi_disconnect(void)
{
    return RN_STATUS_FAIL;
}
