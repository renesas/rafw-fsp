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


#pragma once

#include "CHIPDevicePlatformConfig.h"
#include <wifi_rn_msgs.h>

/* Wi-Fi events*/
#define RN_WIFI_STARTUP_IND_ID 1
#define RN_WIFI_CONNECT_IND_ID 2
#define RN_WIFI_DISCONNECT_IND_ID 3

/* LwIP includes. */
#include "lwip/apps/httpd.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/tcpip.h"

/* Wi-Fi bitmask events - for the task */
#define RN_WIFI_CONNECT (1 << 1)
#define RN_WIFI_DISCONNECT (1 << 2)
#define RN_WIFI_START_AP (1 << 3)
#define RN_WIFI_STOP_AP (1 << 4)
#define RN_WIFI_SCAN_START (1 << 5)
#define RN_WIFI_SCAN_COMPLETE (1 << 6)
#define RN_WIFI_RETRY_CONNECT (1 << 7)

#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "stdbool.h"
#include "rn_status.h"

typedef enum
{
    RN_WIFI_STA_INTERFACE    = 0, ///< Interface 0, linked to the station
    RN_WIFI_SOFTAP_INTERFACE = 1, ///< Interface 1, linked to the softap
} wifi_interface_t;

typedef enum
{
    WIFI_EVENT,
    IP_EVENT,
} wifi_event_base_t;

typedef enum
{
    IP_EVENT_STA_GOT_IP,
    IP_EVENT_GOT_IP6,
    IP_EVENT_STA_LOST_IP,
} ip_event_id_t;

/* Note that these are same as RSI_security */
typedef enum
{
    WIFI_SEC_UNSPECIFIED = 0,
    WIFI_SEC_NONE        = 1,
    WIFI_SEC_WEP         = 2,
    WIFI_SEC_WPA         = 3,
    WIFI_SEC_WPA2        = 4,
    WIFI_SEC_WPA3        = 5
} wifi_sec_t;

typedef struct
{
    char ssid[32 + 1];
    char passkey[64 + 1];
    wifi_sec_t security;
} wifi_provision_t;

typedef enum
{
    SYS_MODE_NULL = 0,
    SYS_MODE_STA,
    SYS_MODE_AP,
    SYS_MODE_APSTA,
    SYS_MODE_MAX,
} system_mode_t;

typedef struct wifi_scan_result
{
    char ssid[32 + 1];
    wifi_sec_t security;
    uint8_t bssid[6];
    uint8_t chan;
    int16_t rssi; /* I suspect this is in dBm - so signed */
} wifi_scan_result_t;

typedef struct wifi_scan_ext
{
    uint32_t beacon_lost_count;
    uint32_t beacon_rx_count;
    uint32_t mcast_rx_count;
    uint32_t mcast_tx_count;
    uint32_t ucast_rx_count;
    uint32_t ucast_tx_count;
    uint32_t overrun_count;
} wifi_scan_ext_t;

#ifdef __cplusplus
extern "C" {
#endif

rn_status_t wifi_start(void);
void wifi_enable_sta_mode(void);
void wifi_get_wifi_mac_addr(wifi_interface_t interface, wifi_mac_address_t * addr);
void wifi_set_wifi_provision(wifi_provision_t * wifiConfig);
bool wifi_get_wifi_provision(wifi_provision_t * wifiConfig);
bool wifi_is_sta_mode_enabled(void);
int32_t wifi_get_ap_info(wifi_scan_result_t * ap);
int32_t wifi_get_ap_ext(wifi_scan_ext_t * extra_info);
int32_t wifi_reset_counts();

void wifi_clear_wifi_provision(void);
rn_status_t wifi_connect_to_ap(void);
void wifi_setup_ip6_link_local(wifi_interface_t);
bool wifi_is_sta_connected(void);
rn_status_t wifi_sta_discon(void);
#if CHIP_DEVICE_CONFIG_ENABLE_IPV4
bool wifi_have_ipv4_addr(wifi_interface_t);
#endif /* CHIP_DEVICE_CONFIG_ENABLE_IPV4 */
bool wifi_have_ipv6_addr(wifi_interface_t);
system_mode_t wifi_get_wifi_mode(void);
bool wifi_start_scan(char * ssid, void (*scan_cb)(wifi_scan_result_t *)); /* true returned if successfuly started */
void wifi_cancel_scan(void);

/*
 * Call backs into the Matter Platform code
 */
bool wifi_hw_ready(void);


#ifdef __cplusplus
}
#endif
