/*
 *
 *    Copyright (c) 2023 Renesas Electronics Corporation and/or its  affiliates
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


#ifndef _WIFI_RN_RSI_H_
#define _WIFI_RN_RSI_H_

#define WIFI_EVT_SCAN 0x10

#define WIFI_RSI_ST_DEV_READY 0x01
#define WIFI_RSI_ST_STA_PROVISIONED 0x02
#define WIFI_RSI_ST_STA_CONNECTED 0x04
#define WIFI_RSI_ST_STA_DHCP_DONE 0x08
#define WIFI_RSI_ST_STA_MODE 0x10

struct wifi_rsi_s
{
    EventGroupHandle_t events;
    TaskHandle_t drv_task;
    TaskHandle_t wlan_task;
    uint16_t dev_state;
    uint16_t ap_chan;
    wifi_provision_t sec;
    void (*scan_cb)(wifi_scan_result_t *);
    char * scan_ssid;
    wifi_mac_address_t softap_mac;
    wifi_mac_address_t sta_mac;
    wifi_mac_address_t ap_mac;
    wifi_mac_address_t ap_bssid;
    uint16_t join_retries;
    uint8_t ip4_addr[4];
};

struct wifi_rsi_s wifi_rsi;

#ifdef __cplusplus
extern "C" {
#endif

int32_t wifi_rsi_get_ap_info(wifi_scan_result_t * ap);
int32_t wifi_rsi_get_ap_ext(wifi_scan_ext_t * extra_info);
int32_t wifi_rsi_reset_count();
int32_t wifi_rsi_disconnect();

#if 0 //[rrq61000 matter work]
#define WIFI_RSI_LOG(...) trc_que_proc_print(0, __VA_ARGS__); \
    trc_que_proc_print(0, "\r\n");
#else
//#define WIFI_RSI_LOG(...) ((void)0)
#define WIFI_RSI_LOG(...) printf(__VA_ARGS__); \
		printf("\r\n");
#endif
#ifdef __cplusplus
}
#endif

#endif /* _WIFI_RN_RSI_H_ */
