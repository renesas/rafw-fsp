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

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "sdk_defs.h"
#include "rm_matter_wifi_cfg.h"
#if defined (__SUPPORT_ATCMD__)
#include "rm_atcmd_w_core_matter_parse.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__USE_MATTER_DPM_APP__)
#include "rm_pmgr_w_dpm_internal.h"
#endif

#include "fsp_common_api.h"
#include "rm_cert.h"

#if defined (__SUPPORT_ATCMD__)
#define ENABLE_CHIP_APP_EXT
#define AT_RESMSG_LEN   256
#endif

#if !defined(__FILENAME__)
#define __FILENAME__ (strrchr(__FILE__,'/')+1)
#endif

#if !defined(ENV_ROOTNAME_CHIPCFG)
#define ENV_ROOTNAME_CHIPCFG      "chipcfg"
#endif

#define _SYS_REBOOT_POR_  1
#define _SYS_REBOOT_  2

/* Set this to the max expected cert size */
#define MAX_CERT_SIZE                            1280
#define CD_CERT_MAX_SIZE                         1280
#define DAC_CERT_MAX_SIZE                        1280
#define PAI_CERT_MAX_SIZE                        1280
#define PRIV_CERT_MAX_SIZE                       1280
#define PUB_CERT_MAX_SIZE                        1280

#if defined(__USE_MATTER_DPM_APP__)

#define	MATTER_JOB_NAME	"matter_start"

// For Case Auth Tag
typedef struct case_tag_info {
    uint32_t caseTagInInt[3];
    uint64_t nodeId;
} case_tag_info;

typedef struct _mat_sess_info {
    // mdns info
    char addr_string[46];
    char inf_name[13];
    uint32_t mrp_idle;
    uint32_t mrp_retrans;
    uint32_t mrp_thresh;
    // matter info
    uint8_t prev_more_chunk;
    uint8_t uc_cmd_done;
    uint8_t success_status_report;
    uint8_t flag_initial_case;
    uint8_t msg_salt[48];
    uint16_t local_port;
    uint16_t active_lsid;
    uint16_t peer_session_id;
    uint32_t uc_cmd_msg_cnt;
    uint32_t msg_cnt;
    uint64_t prev_last_dirty;
    uint8_t rInitiator;
    uint8_t rResponder;
    // CASE Auth Tag info -> changes into gCATsTable on Server.cpp
    case_tag_info caseTagInfo[4];
    uint8_t caseTagIndex;
} mat_sess_info;

extern mat_sess_info *gMatRtmPtr;
#endif

void reboot_func(UINT flag);
void app_print_ext(const char *fmt, ...);

#if 0 //debug off
#define	RENES_LOG(...)  ((void)0);
#define RENES_LOGD(...)  ((void)0);

#define shell_print(...)  ((void)0);
#define CHIP_LOG(...)  ((void)0);

#else //debug on

#if 1 //[rrq61000 matter work]
#define RENES_LOG(...) {    \
        printf( __VA_ARGS__); \
        printf("\r\n"); \
    }
    
#define shell_print(...) printf( __VA_ARGS__);

#define CHIP_LOG(...) {     \
        printf( __VA_ARGS__); \
        printf("\r\n");  \
    }

#else
#define RENES_LOG(...) trc_que_proc_print(0, __VA_ARGS__); \
    trc_que_proc_print(0, "\r\n");

#define shell_print(...) trc_que_proc_print(0, __VA_ARGS__);

#define CHIP_LOG(...) trc_que_proc_print(0, __VA_ARGS__); \
    trc_que_proc_print(0, "\r\n");

#endif
#endif // debug off


#define BLE_ADV_DATA_MAX_SIZE           (0x1F - 3)
#define BLE_SCAN_RESP_DATA_MAX_SIZE     (0x1F)

#ifdef __RRQ61400__
#define DEVICE_SERIAL_SUFFIX    "RRQ61400"
#else
#define DEVICE_SERIAL_SUFFIX    "RRQ61000"
#endif

#define BLE_ADV_DATA_MAX_SIZE           (0x1F - 3)
#define BLE_SCAN_RESP_DATA_MAX_SIZE     (0x1F)

#define MATTER_PRV_CHANGED "+MSTATUS=PROV,CHANGED"
#define MATTER_WIFI_IP4EST "+MSTATUS=WIFI,IP4ESTABLISHED"
#define MATTER_WIFI_IP4LST "+MSTATUS=WIFI,IP4LOST"
#define MATTER_WIFI_IP6EST "+MSTATUS=WIFI,IP6ESTABLISHED"
#define MATTER_WIFI_IP6LST "+MSTATUS=WIFI,IP6LOST"
#define MATTER_BLE_EST "+MSTATUS=BLE,ESTABLISHED"
#define MATTER_BLE_DST "+MSTATUS=BLE,DISCONNECTED"
#define MATTER_COMM_COMPLETE "+MSTATUS=COMMISSION,COMPLETED"

#undef USE_LVL_VALUE_RAM  //Feature for TEST CASE LEVEL Control

void appErrorInt(int err);
typedef enum _matterDeviceStatus {
    _Status_IDLE = -1,
    _Status_factoryreset_done = 0,
    _Status_boot_ready = 1,
    _Status_need_configuration = 5,
    _Status_AP_mode_start = 10,
    _Status_commissioning_mode_start = 11,
    _Status_commissioning_mode_done = 12,
    _Status_network_OK = 15,
    _Status_network_Fail = 16,
    _Status_STA_start = 20,
    _Status_STA_done = 25,
    _Status_MCUOTA = 30
} matterDeviceStatus;
char *getSerialNumber(void);

#ifdef __RRQ61400__
enum ble_matter_event {
    BT_EVT_DEVICE_READY = 0x0001,
    BT_EVT_DEVICE_CONNECT,
    BT_EVT_DEVICE_WRITE_RX,
    BT_EVT_DEVICE_WRITE_CCCD,
    BT_EVT_DEVICE_WRITE_CONFIRM,
    BT_EVT_DEVICE_UPDATE_MTU,
    BT_EVT_DEVICE_DISCONNECTED,
    BT_EVT_DEVICE_MAX
};

struct ble_gatt_context
{
    unsigned char *data;
    unsigned short len;
    unsigned short mtu;
};

struct ble_msg_context
{
    unsigned char conn_handle;
    unsigned short header;
    unsigned short bType;
    unsigned char disconn_reason;

    struct ble_gatt_context gatt;
};

struct ble_advertise
{
    unsigned short interval_min;
    unsigned short interval_max;
    unsigned char advertise_data_len;
    unsigned char advertise_data[32];
    unsigned char scan_response_data_len;
    unsigned char scan_response_data[32];
};

extern struct ble_msg_context evt_ble_matter;
extern struct ble_advertise ble_advertise_matter;
extern void combo_init(void);
extern void app_rst_gap(void);
extern void app_cancel(void);
extern void app_set_adv_forced(int value);
extern void app_disconnect(void);
extern void gtl_init(void *arg);
extern void ble_comm_event(struct ble_msg_context * evt);
extern uint8_t ble_gattc_indicate_send(uint8_t *data, uint16_t length);
#endif

extern void ble_init(void);
extern void ble_deinit(void);
extern void ble_advertising_start(void);
extern void ble_advertising_stop(void);
extern void ble_disconnect(void);
extern void ble_set_advertise_interval(unsigned short min, unsigned short max);
extern int ble_set_advertise_data(char *adv_data, char data_len);
extern int ble_set_scan_response_data(char *res_data, char data_len);
extern int wifi_connect(void);
extern int wifi_provision(char *ssid, char *passkey, unsigned int sec);
extern int wifi_disconnect(void);
extern int is_wifi_connect(void);

extern int wifi_noti_started(void);
extern int wifi_noti_connected(void);
extern int wifi_noti_disconnected(void);
extern int wifi_noti_ipv4(void);
extern int wifi_noti_ipv6(void);
extern void shell_command(int argc, char * argv[]);
extern void app_ext_status_set(matterDeviceStatus _val);
extern int app_ext_status_get(void);
extern int matter_config_check(void);
extern int check_wifi_provisioning_info(void);
extern void set_matter_deviceType(uint16_t edpoint, uint32_t id, uint8_t ver);
extern int set_matter_attribute_control(uint16_t endpoint, uint32_t cluster, uint32_t attributeID, uint8_t *data, uint8_t dataType);
extern int get_matter_attribute_control(uint16_t endpoint, uint32_t cluster, uint32_t attributeID, uint8_t *data, uint16_t dataLen);
extern uint8_t *get_matter_deviceType(uint16_t edpoint);
extern int set_matter_config(char *item, char *value, char *value1, char *value2, char *value3, char *value4);
extern int set_matter_certification_declaration(unsigned char *data, unsigned int size);
extern int set_matter_device_attestation_cert(unsigned char *data, unsigned int size);
extern int set_matter_product_attestation_intermediate_cert(unsigned char *data, unsigned int size);
extern int set_matter_device_attestation_privkey(unsigned char *data, unsigned int size);
extern int set_matter_device_attestation_pubkey(unsigned char *data, unsigned int size);
extern int set_matter_ble_adv_control(int control);
extern int get_matter_onboardingcodes(char *rendezvous);
extern void set_software_fault(const char *faultstring);
extern void set_platform_shutdown(void);
extern int getMacAddrMswLsw(unsigned int iface, unsigned long *macmsw, unsigned long *maclsw);
#if defined(__SUPPORT_ATCMD__)
void RM_MATTER_PRINTF_ATCMD(char *p_str);
#endif
extern void PRINTF_ATCMD_WRAP(const char *fmt, ...);
extern int cc_set_network_str(const char *name, int iface, const char *val);


//EVK LED CONTROL
void set_led_init(void);
void set_led_control(int port, int num, int on);

extern int matter_read_nvram_int(const char *name, int *_val);
extern char* matter_read_nvram_string(const char *name);
extern int matter_read_nvram_binary(const char *name, void *val, unsigned int size);
extern int matter_write_nvram_int(const char *name, int val);
extern int matter_write_nvram_string(const char *name, const char *val);
extern int matter_write_nvram_binary(const char *name, const char *val, unsigned int size);

extern int matter_delete_nvram_env(const char *name);
extern int matter_delete_tmp_nvram_env(const char *name);
extern int matter_save_tmp_nvram(void);
extern void matter_init_nvram(void);
#ifdef __cplusplus
}

#include <lib/core/CHIPError.h>
void appError(CHIP_ERROR error);
CHIP_ERROR fsp_to_chip_error(fsp_err_t error);
#endif
extern int check_net_init(int iface);
extern int RM_WIFI_dpm_supp_is_connected(void);
extern void rrq61x_net_check(int iface_flag, int simple);
