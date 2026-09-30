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
 #include "rm_atcmd_w_core_mqtt_parse.h"
 #include "rm_atcmd_w_core_err_code.h"
 #include "rm_atcmd_w_core.h"
 #if (TEST_WIFI)
  #include "rm_atcmd_w_cfg.h"
 #endif
 #if ATCMD_W_MQTT_EXIST
  #include "rm_mqtt_port_w.h"
  #include "ctype.h"
  #include "strings.h"
  #include "FreeRTOS.h"
  #include "task.h"
  #include "rm_wifi.h"
  #include "util_api.h"
  #include "fw_version.h"
  #include "supp_config.h"
  #include "lwip/ip_addr.h"
  #include "rm_lwip_w_helper.h"
  #include "rm_wifi.h"
  #include "rm_wifi_helper.h"
  #include "rm_vee_flash_w_rrq_nvram.h"
  #include "dhcpserver.h"
  #include "mqtt_client.h"

  #ifdef RM_MAP_PERSISTANT_W
   #include "rm_map_persistant_w.h"
  #endif

#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(atcmd)    "AT+NWMQ" # atcmd

  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(atcmd)                                                     \
    uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, \
                                                               char * argv[])
  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _format_cb(void)
  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _brief_cb(void)

  #define RM_ATCMD_W_CORE_NETWORK_MQTT_UNFIXED_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, \
                                                               size_t inlen)

  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _cmd_cb
  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _format_cb
  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_MQTT_ ## atcmd ## _brief_cb

  #define RM_ATCMD_W_CORE_NETWORK_MQTT_ERROR(fmt, ...)    printf("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)

  #define ATCMD_RSP_ALPN_RESULT_LEN    ((MQTT_TLS_ALPN_MAX_LEN * 3) + 10)
  #define ATCMD_RSP_INT_MAX_LEN        (10)
  #define ATCMD_RSP_TP_MAX_LEN         (MQTT_TOPIC_MAX_LEN + 11) // 11 = \r\n+NWMQTP:
  #define ATCMD_RSP_WILL_MAX_LEN       (MQTT_TOPIC_MAX_LEN + MQTT_WILL_MSG_MAX_LEN + 16)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(BR);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(BR);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(BR);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(QOS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(QOS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(QOS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSVER);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSVER);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSVER);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(ALPN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(ALPN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(ALPN);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(SNI);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(SNI);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(SNI);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CSUIT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CSUIT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CSUIT);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(ATS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(ATS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(ATS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(DTS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(DTS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(DTS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(UTS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(UTS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(UTS);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TP);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TP);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TP);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(PING);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(PING);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(PING);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(V311);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(V311);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(V311);

#if (ATCMD_IF_SUPPORT == 1)
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(MSGFMVER);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSGFMVER);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSGFMVER);
#endif /* ATCMD_IF_SUPPORT == 1 */

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CID);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CID);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CID);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(LI);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(LI);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(LI);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(WILL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(WILL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(WILL);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(DEL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(DEL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(DEL);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CL);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CL);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(MSG);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSG);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSG);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TT);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(AUTO);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(AUTO);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(AUTO);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSBUFIN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSBUFIN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSBUFIN);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSBUFOUT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSBUFOUT);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSBUFOUT);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSAUTH);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSAUTH);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSAUTH);

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CS);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CS);

RM_ATCMD_W_CORE_NETWORK_MQTT_UNFIXED_ATCMD_CB(MSGBIN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSGBIN);
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSGBIN);

int atcmd_mqtt_qos0_msg_send_done_in_dpm;
int atcmd_mqtt_qos0_msg_send_rc;

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_asynchony_event_cb(void * const    p_ctrl,
                                                         int             index,
                                                         unsigned char * p_in,
                                                         unsigned int    inlen);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_network_mqtt_module[] =
{
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(BR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(BR),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(BR),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(BR),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(QOS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(QOS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(QOS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(QOS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TLS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TLS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TLS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TLS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TLSVER),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TLSVER),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TLSVER),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TLSVER),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(ALPN),
        ATCMD_W_TYPE_A,
        (2 + MQTT_TLS_MAX_ALPN),
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(ALPN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(ALPN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(ALPN),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(SNI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(SNI),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(SNI),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(SNI),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(CSUIT),
        ATCMD_W_TYPE_A,
        (1 + MQTT_TLS_MAX_CSUITS),
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(CSUIT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(CSUIT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(CSUIT),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TS),
        ATCMD_W_TYPE_A,
        (1 + MQTT_MAX_TOPIC),
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(ATS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(ATS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(ATS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(ATS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(DTS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(DTS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(DTS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(DTS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(UTS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(UTS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(UTS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(UTS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TP),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TP),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TP),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(PING),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(PING),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(PING),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(PING),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(V311),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(V311),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(V311),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(V311),
    },
#if (ATCMD_IF_SUPPORT == 1)
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(MSGFMVER),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(MSGFMVER),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(MSGFMVER),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(MSGFMVER),
    },
#endif /* ATCMD_IF_SUPPORT == 1 */
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(CID),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(CID),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(CID),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(CID),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(LI),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(LI),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(LI),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(LI),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(WILL),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(WILL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(WILL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(WILL),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(DEL),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(DEL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(DEL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(DEL),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(CL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(CL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(CL),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(CL),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(MSG),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(MSG),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(MSG),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(MSG),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TT),
        ATCMD_W_TYPE_A,
        8,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TT),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(AUTO),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(AUTO),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(AUTO),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(AUTO),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TLSBUFIN),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TLSBUFIN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TLSBUFIN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TLSBUFIN),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TLSBUFOUT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TLSBUFOUT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TLSBUFOUT),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TLSBUFOUT),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(TLSAUTH),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(TLSAUTH),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(TLSAUTH),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(TLSAUTH),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(CS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(CS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(CS),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(CS),
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

const atcmd_w_core_unfixed_module_t at_core_network_mqtt_unfixed_module[] =
{
    {
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CODE(MSGBIN),
        13,
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB_P(MSGBIN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB_P(MSGBIN),
        RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB_P(MSGBIN)
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
const atcmd_conf_str atcmd_mqtt_config_str_with_nvram_name[] =
{
  #if defined(__SUPPORT_MQTT__)
    { RRQ61X_CONF_STR_MQTT_BROKER_IP,     MQTT_NVRAM_CONFIG_BROKER,         MQTT_BROKER_MAX_LEN    },
    { RRQ61X_CONF_STR_MQTT_SUB_TOPIC,     MQTT_NVRAM_CONFIG_SUB_TOPIC,      MQTT_TOPIC_MAX_LEN     },
    { RRQ61X_CONF_STR_MQTT_SUB_TOPIC_ADD, "",                               MQTT_TOPIC_MAX_LEN     },
    { RRQ61X_CONF_STR_MQTT_SUB_TOPIC_DEL, "",                               MQTT_TOPIC_MAX_LEN     },
    { RRQ61X_CONF_STR_MQTT_PUB_TOPIC,     MQTT_NVRAM_CONFIG_PUB_TOPIC,      MQTT_TOPIC_MAX_LEN     },
    { RRQ61X_CONF_STR_MQTT_USERNAME,      MQTT_NVRAM_CONFIG_USERNAME,       MQTT_USERNAME_MAX_LEN  },
    { RRQ61X_CONF_STR_MQTT_PASSWORD,      MQTT_NVRAM_CONFIG_PASSWORD,       MQTT_PASSWORD_MAX_LEN  },
    { RRQ61X_CONF_STR_MQTT_WILL_TOPIC,    MQTT_NVRAM_CONFIG_WILL_TOPIC,     MQTT_TOPIC_MAX_LEN     },
    { RRQ61X_CONF_STR_MQTT_WILL_MSG,      MQTT_NVRAM_CONFIG_WILL_MSG,       MQTT_WILL_MSG_MAX_LEN  },
    { RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, MQTT_NVRAM_CONFIG_SUB_CID,        MQTT_CLIENT_ID_MAX_LEN },
    { RRQ61X_CONF_STR_MQTT_PUB_CLIENT_ID, MQTT_NVRAM_CONFIG_PUB_CID,        MQTT_CLIENT_ID_MAX_LEN },
    { RRQ61X_CONF_STR_MQTT_TLS_SNI,       MQTT_NVRAM_CONFIG_TLS_SNI,        MQTT_BROKER_MAX_LEN    },
  #endif
    { 0,                                  "",                               0                      }
};
/* UNCRUSTIFY-ON */

/* UNCRUSTIFY-OFF */

atcmd_conf_int atcmd_mqtt_config_int_with_nvram_name[] = {
#if defined (__SUPPORT_MQTT__)
  { RRQ61X_CONF_INT_MQTT_SUB,             "",                               0,        1,        0                              },
  { RRQ61X_CONF_INT_MQTT_PUB,             "",                               0,        1,        0                              },
  { RRQ61X_CONF_INT_MQTT_AUTO,            MQTT_NVRAM_CONFIG_AUTO,           0,        1,        MQTT_CONFIG_AUTO_DEF           },
  { RRQ61X_CONF_INT_MQTT_PORT,            MQTT_NVRAM_CONFIG_PORT,           0,        65535,    MQTT_CONFIG_PORT_DEF           },
  { RRQ61X_CONF_INT_MQTT_QOS,             MQTT_NVRAM_CONFIG_QOS,            0,        2,        MQTT_CONFIG_QOS_DEF            },
  { RRQ61X_CONF_INT_MQTT_TLS,             MQTT_NVRAM_CONFIG_TLS,            0,        1,        MQTT_CONFIG_TLS_DEF            },
  { RRQ61X_CONF_INT_MQTT_WILL_QOS,        MQTT_NVRAM_CONFIG_WILL_QOS,       0,        2,        MQTT_CONFIG_QOS_DEF            },
  { RRQ61X_CONF_INT_MQTT_PING_PERIOD,     MQTT_NVRAM_CONFIG_PING_PERIOD,    0,        86400,    MQTT_CONFIG_PING_DEF           },
  { RRQ61X_CONF_INT_MQTT_CLEAN_SESSION,   MQTT_NVRAM_CONFIG_CLEAN_SESSION,  0,        1,        MQTT_CONFIG_CLEAN_SESSION_DEF  },
  { RRQ61X_CONF_INT_MQTT_SAMPLE,          MQTT_NVRAM_CONFIG_SAMPLE,         0,        1,        0                              },
  { RRQ61X_CONF_INT_MQTT_VER311,          MQTT_NVRAM_CONFIG_VER311,         0,        1,        MQTT_CONFIG_VER311_DEF         },
  { RRQ61X_CONF_INT_MQTT_TLS_INCOMING,    MQTT_NVRAM_CONFIG_TLS_INCOMING,   (2*1024), (8*1024), MQTT_CONFIG_TLS_INCOMING_DEF   },
  { RRQ61X_CONF_INT_MQTT_TLS_OUTGOING,    MQTT_NVRAM_CONFIG_TLS_OUTGOING,   (2*1024), (8*1024), MQTT_CONFIG_TLS_OUTGOING_DEF   },
  { RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE,    MQTT_NVRAM_CONFIG_TLS_AUTHMODE,   0,        2,        MQTT_CONFIG_TLS_AUTHMODE_DEF   },
  { RRQ61X_CONF_INT_MQTT_TLS_NO_TIME_CHK, MQTT_NVRAM_CONFIG_TLS_NO_TIME_CHK,0,        1,        MQTT_CONFIG_TLS_NO_TIME_CHK_DEF}, // debug purpose
  { RRQ61X_CONF_INT_MQTT_TLS_VERSION,     MQTT_NVRAM_CONFIG_TLS_VERSION,    0,        2,        MQTT_CONFIG_TLS_VERSION_DEF    },
#if (ATCMD_IF_SUPPORT == 1)
  { RRQ61X_CONF_INT_MQTT_AT_MSGFMT_VER,   MQTT_NVRAM_CONFIG_AT_MSGFMT_VER,  0,        1,        MQTT_CONFIG_AT_MSGFMT_VER_DEF  },
#endif /* ATCMD_IF_SUPPORT == 1 */
#endif // (__SUPPORT_MQTT__)
  { 0, "", 0, 0, 0 }
};
/* UNCRUSTIFY-ON */

const atcmd_conf_str * atcmd_mqtt_conf_str_table = atcmd_mqtt_config_str_with_nvram_name;
const atcmd_conf_int * atcmd_mqtt_conf_int_table = atcmd_mqtt_config_int_with_nvram_name;

static int atcmd_mqtt_connected_ind_sent;
static int atcmd_mqtt_disconnected_ind_sent;

/*
 *  TRUE : 1) at boot (POR/Wakeup), 2) no +WFDAP:0 is sent,
 *         3) no +WFJAP:0 is sent,  4) +WFJAP:1 is sent
 *  FALSE: 5) +WFDAP:0 is sent,     6) +WFJAP:0 is sent
 */
static int atcmd_wfdap_condition_resolved = pdTRUE;;
static int atcmd_wfjap_not_send_by_err    = pdFALSE;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
extern int  is_disconn_by_too_long_msg_rx_in_cs0(void);
extern int  get_INIT_DONE_sent_flag(void);
extern void set_disconn_by_too_long_msg_rx_in_cs0(int onff);
extern void set_INIT_DONE_msg_to_MCU_on_SoftAP_mode(int flag);
extern int  is_connect_retrial_needed(void);
extern void set_connect_retrial_needed(int flag);

void rm_atcmd_w_mqtt_client_convert_to_atcmd_err_code(int * err_code, int * atcmd_err_code);

static int set_atcmd_param_str (int name, char * value)
{
    const atcmd_conf_str * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_mqtt_conf_str_table; cmd_ptr->id; cmd_ptr++)
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

    switch (name)
    {
  #if defined(__SUPPORT_MQTT__)
        case RRQ61X_CONF_STR_MQTT_SUB_TOPIC_ADD:
        {
            if (mqtt_client_add_sub_topic(value))
            {
                result = CC_FAILURE_INVALID;
            }

            break;
        }

        case RRQ61X_CONF_STR_MQTT_SUB_TOPIC_DEL:
        {
            if (mqtt_client_del_sub_topic(value))
            {
                result = CC_FAILURE_INVALID;
            }

            break;
        }
  #endif                               // (__SUPPORT_MQTT__)
    }

    return result;
}

static int get_atcmd_param_str (int name, char * value, size_t value_len)
{
    const atcmd_conf_str * cmd_ptr      = NULL;
    char                 * nvram_string = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_mqtt_conf_str_table; cmd_ptr->id; cmd_ptr++)
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
                bsp_safe_strcpy(value, nvram_string, value_len);
                result = CC_STATUS_SUCCESS;
            }

            break;
        }
    }

    return result;
}

static int set_atcmd_param_int (int name, int value)
{
    const atcmd_conf_int * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;

    for (cmd_ptr = atcmd_mqtt_conf_int_table; cmd_ptr->id; cmd_ptr++)
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

    switch (name)
    {
  #if defined(__SUPPORT_MQTT__)
        case RRQ61X_CONF_INT_MQTT_SUB:
        {
            if (get_run_mode() != WIFI_DEVICE_MODE_EXT_AP)
            {
                if (value == CC_VAL_ENABLE)
                {
                    if (mqtt_client_is_running() == TRUE)
                    {
                        mqtt_client_force_stop();
                        mqtt_client_stop_sub();
                    }

                    if (mqtt_client_start_sub() == 0)
                    {
                        result = CC_STATUS_SUCCESS;
                    }
                    else
                    {
                        result = CC_FAILURE_UNKNOWN;
                    }
                }
                else
                {
                    if (mqtt_client_is_running() == TRUE)
                    {
                        mqtt_client_force_stop();
                        mqtt_client_stop_sub();
                        result = CC_STATUS_SUCCESS;
                    }
                    else
                    {
                        result = CC_FAILURE_NOT_READY;
                    }
                }
            }

            break;
        }

        case RRQ61X_CONF_INT_MQTT_PUB:
        {
            result = CC_FAILURE_NOT_SUPPORTED;
            break;
        }

        case RRQ61X_CONF_INT_MQTT_AUTO:
        {
            if (value == 1)
            {
   #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                              ENV_GROUP_APPCFG,
                                              cmd_ptr->nvram_name,
                                              MQTT_INIT_MAGIC);
   #endif
            }

            break;
        }
  #endif                               // (__SUPPORT_MQTT__)
    }

    return result;
}

static int get_atcmd_param_int (int name, int * value)
{
    const atcmd_conf_int * cmd_ptr = NULL;
    int result = CC_FAILURE_NOT_SUPPORTED;
    int nvram_int;

    for (cmd_ptr = atcmd_mqtt_conf_int_table; cmd_ptr->id; cmd_ptr++)
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

    switch (name)
    {
  #if defined(__SUPPORT_MQTT__)
        case RRQ61X_CONF_INT_MQTT_SUB:
        {
            *value = mqtt_client_check_sub_conn();
            break;
        }

        case RRQ61X_CONF_INT_MQTT_PUB:
        {
            *value = mqtt_client_check_sub_conn();
            break;
        }
  #endif                               // (__SUPPORT_MQTT__)
    }

    return result;
}

static int rm_atcmd_w_core_network_mqtt_is_query_arg (int argc, char * str)
{
    return argc == 2 && strcmp(str, AT_CMD_GET_MRK) == 0;
}

void rm_atcmd_w_mqtt_client_convert_to_atcmd_err_code (int * err_code, int * atcmd_err_code)
{
    int mosq_err_code = *err_code;

    if ((mosq_err_code == MOSQ_ERR_INVAL) || (mosq_err_code == MOSQ_ERR_PAYLOAD_SIZE))
    {
        *atcmd_err_code = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }
    else if ((mosq_err_code == MOSQ_ERR_NO_CONN) || (mosq_err_code == MOSQ_ERR_CONN_LOST))
    {
        *atcmd_err_code = FSP_ERR_AT_CMD_ERR_NOT_CONNECTED;
    }
    else
    {
        *atcmd_err_code = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }
}

static uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_WFMQCL_resp (void * const p_ctrl, const char * p_in, size_t inlen)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int          mqtt_connected      = atoi(p_in);
    char       * p_resp              = NULL;
    const char * str_resp_cs0        = "\r\n+NWMQCL:0\r\n";
    const char * str_resp_cs0_long   = "\r\n+NWMQCL:0,TOO_LONG_MSG_RX\r\n";
    const char * str_resp_cs1        = "\r\n+NWMQCL:1\r\n";
    const char * str_resp_conn_retry = "\r\n+NWMQCL:0,IN_RETRY\r\n";
    int          p_resp_len          = 0;

    if (p_ctrl == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    if (mqtt_connected)
    {
        int wait_count = 100;          // 1 sec

        while (!atcmd_wfdap_condition_resolved)
        {
            if (atcmd_wfjap_not_send_by_err || (--wait_count == 0))
            {
                printf("atcmd_wfjap_not_send_by_err(%d), wait_count(%d)\n", atcmd_wfjap_not_send_by_err, wait_count);
                break;
            }

            vTaskDelay(portCONVERT_MS_2_TICKS(10));
        }

        if (!atcmd_wfjap_not_send_by_err)
        {
            if (atcmd_mqtt_connected_ind_sent == FALSE)
            {
                p_resp_len = strlen(str_resp_cs1) + inlen;
                p_resp     = pvPortMalloc(p_resp_len);

                if (!p_resp)
                {
                    return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                }

                memset(p_resp, 0x00, p_resp_len);

                sprintf(p_resp, "%s", str_resp_cs1);
                RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));
                atcmd_mqtt_connected_ind_sent    = TRUE;
                atcmd_mqtt_disconnected_ind_sent = FALSE;
            }
        }
    }
    else
    {
        if (is_disconn_by_too_long_msg_rx_in_cs0())
        {
            if (atcmd_mqtt_disconnected_ind_sent == FALSE)
            {
                p_resp_len = strlen(str_resp_cs0_long) + inlen;
                p_resp     = pvPortMalloc(p_resp_len);

                if (!p_resp)
                {
                    return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                }

                memset(p_resp, 0x00, p_resp_len);

                sprintf(p_resp, "%s", str_resp_cs0_long);

                RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));
                set_disconn_by_too_long_msg_rx_in_cs0(FALSE);
                atcmd_mqtt_disconnected_ind_sent = TRUE;
                atcmd_mqtt_connected_ind_sent    = FALSE;
            }
        }
        else if (is_connect_retrial_needed())
        {
            p_resp_len = strlen(str_resp_conn_retry) + inlen;
            p_resp     = pvPortMalloc(p_resp_len);

            if (!p_resp)
            {
                return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            }

            memset(p_resp, 0x00, p_resp_len);

            sprintf(p_resp, "%s", str_resp_conn_retry);

            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));
            set_connect_retrial_needed(FALSE);
            atcmd_mqtt_connected_ind_sent    = FALSE;
            atcmd_mqtt_disconnected_ind_sent = FALSE;
        }
        else
        {
            p_resp_len = strlen(str_resp_cs0) + inlen;
            p_resp     = pvPortMalloc(p_resp_len);

            if (!p_resp)
            {
                return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            }

            memset(p_resp, 0x00, p_resp_len);

            sprintf(p_resp, "%s", str_resp_cs0);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_resp, strlen(p_resp));
            atcmd_mqtt_disconnected_ind_sent = TRUE;
            atcmd_mqtt_connected_ind_sent    = FALSE;
        }
    }

    if (p_resp)
    {
        vPortFree(p_resp);
        p_resp = NULL;
    }

    return err;
}

static void RM_ATCMD_W_CORE_NETWORK_MQTT_ASYNCMSG_Publish (atcmd_w_ctrl_t * const p_at_ctrl, int result, int err_code)
{
    int  p_resp_len = 0;
    char p_resp[25] = {0, };
    p_resp_len = sprintf(p_resp, "\r\n+NWMQMSGSND:%d\r\n", result);

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) p_resp, p_resp_len);
}

void RM_ATCMD_W_CORE_NETWORK_MQTT_RESP_Handle (atcmd_w_ctrl_t * const p_at_ctrl)
{
    if (atcmd_mqtt_qos0_msg_send_done_in_dpm)
    {
        if (atcmd_mqtt_qos0_msg_send_rc == 0)
        {
            RM_ATCMD_W_CORE_NETWORK_MQTT_ASYNCMSG_Publish(p_at_ctrl, 1, 0);
        }
        else
        {
            RM_ATCMD_W_CORE_NETWORK_MQTT_ASYNCMSG_Publish(p_at_ctrl, 0, atcmd_mqtt_qos0_msg_send_rc);
        }

        atcmd_mqtt_qos0_msg_send_done_in_dpm = pdFALSE;
    }
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_register (atcmd_w_core_module_list_t * p_list)
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

    if (rm_atcmd_w_core_register_module_node(p_list, at_core_network_mqtt_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (rm_atcmd_w_core_register_unfixed_module_node(p_list, at_core_network_mqtt_unfixed_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_deregister (atcmd_w_core_module_list_t * p_list)
{
  #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
  #endif
    rm_atcmd_w_core_deregister(p_list, at_core_network_mqtt_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    err = (fsp_err_atcmd_err_code) rm_mqtt_client_set_atcmd_event_callback(p_at_ctrl,
                                                                           &RM_ATCMD_W_CORE_NETWORK_MQTT_asynchony_event_cb);

    return err;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    err = (fsp_err_atcmd_err_code) rm_mqtt_client_set_atcmd_event_callback(NULL, NULL);

    return err;
}

void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_state (int state)
{
    if (state)
    {
        atcmd_wfdap_condition_resolved = pdTRUE;
    }
    else
    {
        atcmd_wfdap_condition_resolved = pdFALSE;
    }
}

int RM_ATCMD_W_CORE_NETWORK_MQTT_get_wfadp_state (void)
{
    return atcmd_wfdap_condition_resolved;
}

void RM_ATCMD_W_CORE_NETWORK_MQTT_set_wfdap_err_state (int state)
{
    if (state)
    {
        atcmd_wfjap_not_send_by_err = pdTRUE;
    }
    else
    {
        atcmd_wfjap_not_send_by_err = pdFALSE;
    }
}

int RM_ATCMD_W_CORE_NETWORK_MQTT_get_wfdap_err_state (void)
{
    return atcmd_wfjap_not_send_by_err;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_asynchony_event_cb (void * const    p_ctrl,
                                                          int             index,
                                                          unsigned char * p_in,
                                                          unsigned int    inlen)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (p_ctrl == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    switch (index)
    {
        case 0:                        /* Response p_in */
        {
            if (inlen > 0)
            {
                err = (fsp_err_atcmd_err_code) RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_in, (uint32_t) inlen);
            }

            break;
        }

        case 1:
        {
            err = RM_ATCMD_W_CORE_NETWORK_MQTT_WFMQCL_resp(p_ctrl, (const char *) p_in, (size_t) inlen);
            break;
        }

        default:
        {
            err = (fsp_err_atcmd_err_code) FSP_ERR_INVALID_ARGUMENT;
            break;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(BR)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1, tmp_int2 = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100]   = {0, };
    char result_str[75] = {0, };

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        char broker_ip[MQTT_BROKER_MAX_LEN + 1] = {0, };

        /* AT+NWMQBR=? */
        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, broker_ip, sizeof(broker_ip)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_NAME_NOT_FOUND;

            return err;
        }

        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_PORT, &result_int);
        sprintf(result_str, "%s,%d", broker_ip, result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 3)
    {
  #if CFG_PMGR
        if ((RM_PMGR_W_dpm_is_enabled() == pdTRUE) && (is_mqtt_client_thd_alive() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        /* AT+NWMQBR=<ip>,<port> */
        if (rm_atcmd_w_core_common_stoi(argv[2], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_PORT_NUM_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_PORT, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_PORT_NUM_RANGE;

            return err;
        }

        tmp_int2 = set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, argv[1]);
        if (tmp_int2 != CC_STATUS_SUCCESS)
        {
            if (tmp_int2 == CC_FAILURE_STRING_LENGTH)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_NAME_LEN;
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_UNKNOWN_OP_ID;
            }

            return err;
        }

  #if CFG_PMGR
        mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_BROKER_IP, argv[1], 0, 0);
        mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_PORT, tmp_int1);
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(BR)
{
    const char * p_usage = "<ip><port>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(BR)
{
    const char * p_description = "Configure IP address and Port number for MQTT Broker";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(QOS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int status      = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQQOS=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_QOS, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQQOS=<qos> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 2) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;

            return err;
        }

        status = set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_QOS, tmp_int1);
        switch (status)
        {
            case CC_FAILURE_RANGE_OUT:
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                break;
            }

            case CC_FAILURE_NOT_SUPPORTED:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_UNKNOWN_OP_ID;
                break;
            }
        }

        if (status != CC_STATUS_SUCCESS)
        {
            return err;
        }

  #if CFG_PMGR
        mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_QOS, tmp_int1);
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(QOS)
{
    const char * p_usage = "<qos>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(QOS)
{
    const char * p_description = "Configure MQTT QoS level (0~2)";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int status      = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTLS=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQTLS=<tls> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            printf("Stop mqtt_client first before (re)configuration.\n");
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;

            return err;
        }

        status = set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS, tmp_int1);
        switch (status)
        {
            case CC_FAILURE_RANGE_OUT:
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                break;
            }

            case CC_FAILURE_NOT_SUPPORTED:
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_UNKNOWN_OP_ID;
                break;
            }
        }

        if (status != CC_STATUS_SUCCESS)
        {
            return err;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_TLS, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLS)
{
    const char * p_usage = "<tls>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLS)
{
    const char * p_description = "Enable/Disable tls mode for MQTT";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSVER)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int status      = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTLSVER=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_VERSION, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQTLSVER=<tls_ver> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            printf("Stop mqtt_client first before (re)configuration.\n");
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 2) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;

            return err;
        }

        status = set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_VERSION, tmp_int1);
        if (status != CC_STATUS_SUCCESS)
        {
            return err;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_TLS_VERSION, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSVER)
{
    const char * p_usage = "<tls_ver>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSVER)
{
    const char * p_description = "0 (1.2) 1 (1.3) 2 (1.2 and 1.3)";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(ALPN)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int tmp         = 0;
    int at_resp_len = 0;

    char   at_resp[100] = {0, };
    char   result_str[ATCMD_RSP_ALPN_RESULT_LEN] = {0, };
    char * dyn_mem = NULL;

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQALPN=? */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &result_int);
  #endif
        if (result_int == -1)
        {
            char * result_str_pos = result_str;

            bsp_safe_strcpy(result_str_pos, "1,\"", ATCMD_RSP_ALPN_RESULT_LEN);
            result_str_pos = result_str_pos + 3;

            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_ALPN_NOT_EXIST;

            return err;
        }
        else
        {
            char * result_str_pos;

            if (result_int > 1)
            {
                char * res_str;
                int    alloc_bytes = 2;                             // "[alpn_count],"

                alloc_bytes = alloc_bytes +
                              (result_int - 1) +                    // num (,)
                              (2 * result_int) +                    // num (double quotation)
                              (MQTT_TLS_ALPN_MAX_LEN * result_int); // num(alpn)

                res_str = pvPortMalloc(alloc_bytes + 1);
                if (res_str == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

                    return err;
                }

                dyn_mem = res_str;     // set for freeing later

                memset(res_str, 0, alloc_bytes + 1);
                result_str_pos = res_str;
            }
            else
            {
                result_str_pos = result_str;
            }

            sprintf(result_str_pos, "%d", result_int);
            result_str_pos = result_str_pos + 1;

            tmp = (char) result_int;
            for (int i = 0; i < tmp; i++)
            {
                char   nvrName[20] = {0, };
                char * tmp_str     = NULL;

                sprintf(nvrName, "%s%d", MQTT_NVRAM_CONFIG_TLS_ALPN, (char) i);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvrName, &tmp_str);
  #endif
                if (tmp_str)
                {
                    sprintf(result_str_pos, ",\"%s\"", tmp_str);
                    result_str_pos = result_str_pos + (strlen(tmp_str) + 3); // 3 = 2x(") + 1x(,)
                }
            }

            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), dyn_mem);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
            vTaskDelay(portCONVERT_MS_2_TICKS(10));
            vPortFree(dyn_mem);
        }
    }
    else if (argc >= 3)
    {
        /* AT+NWMQALPN=<#>,<alpn#n>,... */
        char c_tmp;
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && RM_PMGR_W_dpm_is_enabled())
        {
            printf("Stop mqtt_client first before (re)configuration.\n");

            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_ALPN_COUNT_TYPE;

            return err;
        }

        /* Sanity check */
        if (argc - 2 > tmp_int1)
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
        else if (argc - 2 < tmp_int1)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        if ((tmp_int1 > MQTT_TLS_MAX_ALPN) || (tmp_int1 <= 0))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_ALPN_COUNT_RANGE;

            return err;
        }

        for (int i = 0; i < tmp_int1; i++)
        {
            if (strlen(argv[i + 2]) > MQTT_TLS_ALPN_MAX_LEN)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_ALPN_NAME_LEN;

                return err;
            }
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &tmp);
  #endif
        if (tmp != -1)
        {
            c_tmp = (char) tmp;
            for (int i = 0; i < c_tmp; i++)
            {
                char nvr_name[17] = {0, };

                sprintf(nvr_name, "%s%d", MQTT_NVRAM_CONFIG_TLS_ALPN, i);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
  #endif
            }
        }

        tmp = (char) tmp_int1;
        for (int i = 0; i < tmp; i++)
        {
            char nvr_name[24] = {0, };
            sprintf(nvr_name, "%s%d", MQTT_NVRAM_CONFIG_TLS_ALPN, i);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, argv[i + 2]);
  #endif
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                      tmp_int1);
  #endif

  #if CFG_PMGR
        if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && mqtt_client_is_cfg_dpm_mem_intact())
        {
            int alpn_count = tmp_int1;

            mqttParams.tls_alpn_cnt = 0;
            for (int i = 0; i < alpn_count; i++)
            {
                bsp_safe_strcpy(mqttParams.tls_alpn[i], argv[i + 2], sizeof(mqttParams.tls_alpn[i]));
                mqttParams.tls_alpn_cnt++;
            }

            mqtt_client_save_to_dpm_user_mem();
        }
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(ALPN)
{
    const char * p_usage = "<#><tls_alpn 1>...";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(ALPN)
{
    const char * p_description = "Configure TLS ALPN protocol name";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(SNI)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;
  #endif                               /* CFG_PMGR */

    int at_resp_len = 0;

    char at_resp[100] = {0, };
    char result_str[MQTT_BROKER_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQSNI=? */
        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, result_str, sizeof(result_str)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_SNI_NOT_EXIST;

            return err;
        }
        else
        {
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else if (argc == 2)
    {
        /* AT+NWMQSNI=<sni> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_SNI_LEN;
        }

  #if CFG_PMGR
        if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && (mqtt_client_is_cfg_dpm_mem_intact()))
        {
            bsp_safe_strcpy(mqttParams.tls_sni, argv[1], sizeof(mqttParams.tls_sni));
            mqtt_client_save_to_dpm_user_mem();
        }
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(SNI)
{
    const char * p_usage = "<sni>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(SNI)
{
    const char * p_description = "Configure TLS SNI";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CSUIT)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */
    extern mqttParamForRtm mqttParams;

    int result_int  = 0;
    int at_resp_len = 0;

    char   at_resp[100] = {0, };
    char * dyn_mem      = NULL;

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    // MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM
    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        char * result_str_pos = NULL;
        char * tmp_str = NULL;
        char * res_str = NULL;
        int    num_length, alloc_bytes = 0;

        /* AT+NWMQCSUIT=? */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM,
                                     &result_int);
  #endif
        if (result_int == -1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_CSUITE_NUM_NOT_EXIST;

            return err;
        }

        num_length = (result_int >= 10 ? 2 : 1);

        /* Assume that a length of each cipher suit is 4 .
         * Total length = a length of number of cipher suits + Number of comma +
         *                (a length of cipher suit(4) * number of cipher suits).
         */
        alloc_bytes = num_length + (result_int - 1) + (4 * result_int);
        res_str     = pvPortMalloc(alloc_bytes + 1);
        if (res_str == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

            return err;
        }

        /* Set for freeing later */
        dyn_mem = res_str;
        memset(res_str, 0, alloc_bytes + 1);
        result_str_pos = res_str;

        /* Write number of cipher suits and cipher suits */
        sprintf(result_str_pos, "%d", result_int);
        result_str_pos = result_str_pos + num_length;

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        MQTT_NVRAM_CONFIG_TLS_CSUITS,
                                        &tmp_str);
  #endif
        if (tmp_str == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_CSUITE_NOT_EXIST;
        }
        else
        {
            sprintf(result_str_pos, ",%s", tmp_str);
        }

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), dyn_mem);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        vTaskDelay(portCONVERT_MS_2_TICKS(10));
        vPortFree(dyn_mem);
    }
    else if (argc >= 2)
    {
        char * result_str_pos;
        char * res_str, num_cipher_suits = 0;
        int    arg_idx, alloc_bytes = 0;
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (argc > (MQTT_TLS_MAX_CSUITS + 1))
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;

            return err;
        }

        num_cipher_suits = argc - 1;

        /* Assume that a length of each cipher suit is 4 .
         * Total length = Number of comma +
         *                (a length of cipher suit(4) * number of cipher suits).
         */
        alloc_bytes = (num_cipher_suits - 1) + (4 * num_cipher_suits);
        res_str     = pvPortMalloc(alloc_bytes + 1);
        if (res_str == NULL)
        {
            err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

            return err;
        }

        /* Delete the data stored */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_TLS_CSUITS);
  #endif

        result_str_pos = res_str;
        memset(res_str, 0, alloc_bytes + 1);
  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM, num_cipher_suits))
        {
  #endif
        err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_CSUITE_NUM_NVRAM_WR;

        return err;
    }

    arg_idx = 1;
    sprintf(result_str_pos, "%s", argv[arg_idx]);
    result_str_pos          += strlen(argv[arg_idx]);
    mqttParams.tls_csuits[0] = rm_atcmd_w_core_common_htoi_custom(argv[arg_idx++]);

    for (int i = 0; i < argc - 2; i++, arg_idx++)
    {
        sprintf(result_str_pos, ",%s", argv[arg_idx]);
        mqttParams.tls_csuits[i] = rm_atcmd_w_core_common_htoi_custom(argv[arg_idx]);
        result_str_pos          += (strlen(argv[arg_idx]) + 1);
    }

    mqttParams.tls_csuits_cnt = num_cipher_suits;
  #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_TLS_CSUITS,
                                         res_str))
    {
  #endif
    err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_CSUITE_NVRAM_WR;

    return err;
}

  #if CFG_PMGR
if (mqtt_client_is_cfg_dpm_mem_intact())
{
    mqtt_client_save_to_dpm_user_mem();
}

  #endif                               /* CFG_PMGR */

if (res_str)
{
    vPortFree(res_str);
}
}

return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CSUIT)
{
    const char * p_usage = "<cipher suit 1><cipher suit 2>...";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CSUIT)
{
    const char * p_description = "Configure cipher suits";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;
  #endif                               /* CFG_PMGR */

    int tmp_int1 = 0;

    char at_resp[(MQTT_TOPIC_MAX_LEN * MQTT_MAX_TOPIC) + 36] = {0x00, };
    char * p_result = NULL;
    char tmp;
    char topics[18] = {0x00, };

    int nv_topic_num = 0;
    int cnt          = 0;

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTS=? */

        /* Added prefix for response. */
        sprintf(at_resp, "\r\n%s:", rm_atcmd_w_core_common_strupr(argv[0] + 2));

        p_result = (at_resp + strlen(at_resp));

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                     &nv_topic_num);
  #endif

        if (nv_topic_num == -1)
        {
            /* Added Topic count. */
            bsp_safe_strcpy(p_result, "1,\"", sizeof("1,\""));
            p_result = p_result + (sizeof("1,\"") - 1);

            /* Added Topic. */
            if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_SUB_TOPIC, p_result, MQTT_TOPIC_MAX_LEN + 1) != CC_STATUS_SUCCESS)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NOT_EXIST;

                return err;
            }

            p_result = p_result + strlen(p_result);
            bsp_safe_strcpy(p_result, "\"", sizeof("\""));
        }
        else
        {
            /* Added Topic count. */
            sprintf(p_result, "%d", nv_topic_num);
            p_result = p_result + 1;

            /* Addedd Topics. */
            tmp = (char) nv_topic_num;
            for (cnt = 0; cnt < tmp; cnt++)
            {
                char * tmp_str = NULL;

                sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, cnt);

  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, &tmp_str);
  #endif
                if (tmp_str)
                {
                    sprintf(p_result, ",\"%s\"", tmp_str);
                    p_result = p_result + (strlen(tmp_str) + 3); // 3 = 2x(") + 1x(,)
                }
            }
        }

        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, strlen(at_resp));
    }
    else if (argc >= 3)
    {
        /* AT+NWMQTS=<#>,<topic#n>,... */
        int tmp_sub_topic_cnt;

  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            return FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NUM_TYPE;
        }

        /* Check argument count */
        if (argc - 2 > tmp_int1)
        {
            return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
        else if (argc - 2 < tmp_int1)
        {
            return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        /* Check topic count */
        if ((tmp_int1 > MQTT_MAX_TOPIC) || (tmp_int1 <= 0))
        {
            return FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NUM_RANGE;
        }

        /* Check topic length */
        for (cnt = 0; cnt < tmp_int1; cnt++)
        {
            if (strlen(argv[cnt + 2]) > MQTT_TOPIC_MAX_LEN)
            {
                return FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_LEN;
            }
        }

        /* Check duplicate topic */
        if (rm_atcmd_w_core_common_is_duplicate_string_found(&(argv[2]), tmp_int1))
        {
            return FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_DUP;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                     &tmp_sub_topic_cnt);
  #endif

        if (tmp_sub_topic_cnt != -1)
        {
            tmp = (char) tmp_sub_topic_cnt;
            for (cnt = 0; cnt < tmp; cnt++)
            {
                sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, cnt);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics);
  #endif
            }
        }
        else
        {
            char * p_topic = NULL;

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_APPCFG,
                                            MQTT_NVRAM_CONFIG_SUB_TOPIC,
                                            &p_topic);
  #endif

            if (p_topic && strlen(p_topic))
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          MQTT_NVRAM_CONFIG_SUB_TOPIC);
  #endif
            }
        }

        tmp = (char) tmp_int1;
        for (cnt = 0; cnt < tmp; cnt++)
        {
            sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, cnt);

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, argv[cnt + 2]);
  #endif
        }

  #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM, tmp_int1))
  #endif
        {
            return FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NUM_NVRAM_WR;
        }

  #if CFG_PMGR
        if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && mqtt_client_is_cfg_dpm_mem_intact())
        {
            int topic_cnt = tmp_int1;

            mqttParams.topic_count = 0;

            for (cnt = 0; cnt < topic_cnt; cnt++)
            {
                mqttParams.topic_count++;
                bsp_safe_strcpy(mqttParams.topics[mqttParams.topic_count - 1], argv[cnt + 2],
                                       sizeof(mqttParams.topics[mqttParams.topic_count - 1]));
            }

            mqtt_client_save_to_dpm_user_mem();
        }
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TS)
{
    const char * p_usage = "<#><topic#n>...";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TS)
{
    const char * p_description = "Set topics for MQTT Subscriber";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(ATS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;
  #endif                               /* CFG_PMGR */

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 2)
    {
        char * nvram_read_topic = NULL;
        char nvram_tag[20]      = {0, };
        int sub_topic_num       = 0;
        char tmp;
        char * tmp_topics  = NULL;
        int free_topic_pos = 0;

        if (rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;

            return err;
        }

  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (strlen(argv[1]) > MQTT_TOPIC_MAX_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_LEN;

            return err;
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                     &sub_topic_num);
  #endif
        if (sub_topic_num != -1)
        {
            if (sub_topic_num >= MQTT_MAX_TOPIC)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NUM_OVERFLOW;

                return err;
            }

            tmp_topics = pvPortMalloc(MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));
            if (tmp_topics == NULL)
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

                return err;
            }

            memset(tmp_topics, 0x00, MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));

            tmp = (char) sub_topic_num;
            for (int i = 0; i < tmp; i++)
            {
                memset(nvram_tag, 0x00, 16);
                sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_APPCFG,
                                                nvram_tag,
                                                &nvram_read_topic);
  #endif

                if (nvram_read_topic)
                {
                    if (strcmp(nvram_read_topic, argv[1]) == 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_ALREADY_EXIST;
                        vPortFree(tmp_topics);

                        return err;
                    }

                    if (strlen(nvram_read_topic) > 0)
                    {
                        int str_pos = i * (MQTT_TOPIC_MAX_LEN + 1);

                        bsp_safe_strcpy((char *) (tmp_topics + str_pos), nvram_read_topic, MQTT_TOPIC_MAX_LEN + 1);
                    }
                }
            }

            sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, tmp);
            free_topic_pos = sub_topic_num * (MQTT_TOPIC_MAX_LEN + 1);
        }
        else
        {
            tmp_topics = pvPortMalloc(MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));
            if (tmp_topics == NULL)
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

                return err;
            }

            memset(tmp_topics, 0x00, MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_APPCFG,
                                            MQTT_NVRAM_CONFIG_SUB_TOPIC,
                                            &nvram_read_topic);
  #endif

            if (nvram_read_topic && strlen(nvram_read_topic))
            {
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          MQTT_NVRAM_CONFIG_SUB_TOPIC);
  #endif
            }

            sub_topic_num = 0;
            memset(nvram_tag, 0x00, 16);
            sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, sub_topic_num);
        }

        sub_topic_num += 1;
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                      sub_topic_num);
  #endif
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvram_tag, argv[1]);
  #endif
        bsp_safe_strcpy((char *) (tmp_topics + free_topic_pos), argv[1], MQTT_TOPIC_MAX_LEN + 1);

  #if CFG_PMGR
        if (mqtt_client_is_cfg_dpm_mem_intact())
        {
            mqttParams.topic_count = sub_topic_num;
            memcpy(mqttParams.topics, tmp_topics, MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));
            mqtt_client_save_to_dpm_user_mem();
        }
  #endif                               /* CFG_PMGR */

        vPortFree(tmp_topics);
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(ATS)
{
    const char * p_usage = "<topic>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(ATS)
{
    const char * p_description = "Add a topic for MQTT Subscriber";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(DTS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;

    char result_str[MQTT_TOPIC_MAX_LEN + 1] = {0, };
  #endif                               /* CFG_PMGR */

    int tmp_int1 = 0;

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 2)
    {
  #if CFG_PMGR
        int ret_num;
        char tmp;
        char * tmp_topics  = NULL;
        int free_topic_pos = 0;

        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        tmp_int1 = mqtt_client_del_sub_topic(argv[1]);

        /* AT+NWMQDTP=<topic> */
        if (tmp_int1 != 0)
        {
            if (tmp_int1 == 100)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NOT_EXIST;
            }
            else if (tmp_int1 == 101)
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
            }

            return err;
        }

  #if CFG_PMGR
        if (mqtt_client_is_cfg_dpm_mem_intact())
        {
            tmp_topics = pvPortMalloc(MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));
            if (tmp_topics == NULL)
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

                return err;
            }

            memset(tmp_topics, 0x00, MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));

   #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_APPCFG,
                                         MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                         &ret_num);
   #endif
            if (ret_num == -1)
            {
                if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_SUB_TOPIC, result_str, sizeof(result_str)))
                {
                    result_str[0] = '\0';
                }

                bsp_safe_strcpy((char *) (tmp_topics + free_topic_pos), result_str, MQTT_TOPIC_MAX_LEN + 1);
            }
            else
            {
                memset(result_str, '\0', MQTT_TOPIC_MAX_LEN + 1);

                tmp = (char) ret_num;
                for (int i = 0; i < tmp; i++)
                {
                    char topics[18] = {0, };
                    char * tmp_str  = NULL;

                    sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
   #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, &tmp_str);
   #endif

                    if (tmp_str && (strlen(tmp_str) > 0))
                    {
                        int str_pos = i * (MQTT_TOPIC_MAX_LEN + 1);
                        bsp_safe_strcpy((char *) (tmp_topics + str_pos), tmp_str, MQTT_TOPIC_MAX_LEN + 1);
                    }
                }
            }

            mqttParams.topic_count = ret_num;
            memcpy(mqttParams.topics, tmp_topics, MQTT_MAX_TOPIC * (MQTT_TOPIC_MAX_LEN + 1));
            mqtt_client_save_to_dpm_user_mem();

            vPortFree(tmp_topics);
        }
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(DTS)
{
    const char * p_usage = "<topic>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(DTS)
{
    const char * p_description = "Delete a topic for MQTT Subscriber";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(UTS)
{
    int tmp_int1 = 0;

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 2)
    {
        /* AT+NWMQUTS=<topic> */

        tmp_int1 = mqtt_client_unsub_topic(argv[1]);
        if (tmp_int1 != MOSQ_ERR_SUCCESS)
        {
            if (tmp_int1 == MOSQ_ERR_NO_CONN)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NOT_CONNECTED;
            }
            else if (tmp_int1 == MOSQ_ERR_NOT_FOUND)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_NOT_EXIST;
            }
            else if (tmp_int1 == MOSQ_ERR_PROTOCOL)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PROTOCOL;
            }
            else if (tmp_int1 == MOSQ_ERR_NOMEM)
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(UTS)
{
    const char * p_usage = "<topic>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(UTS)
{
    const char * p_description = "Unsubscribe from the specified topic";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TP)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int at_resp_len = 0;

    char at_resp[ATCMD_RSP_TP_MAX_LEN]      = {0, };
    char result_str[MQTT_TOPIC_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTP=? */
        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, result_str, sizeof(result_str)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_NOT_EXIST;

            return err;
        }
        else
        {
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else if (argc == 2)
    {
        /* AT+NWMQTP=<topic> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_LEN;

            return err;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, argv[1], 0, 0);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TP)
{
    const char * p_usage = "<topic>...";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TP)
{
    const char * p_description = "Topics for MQTT Publisher";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(PING)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int result_int  = 0;
    int tmp_int1    = 0;
    int at_resp_len = 0;

    char at_resp[100] = {0, };
    char result_str[ATCMD_RSP_INT_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQPING=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_PING_PERIOD, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQPING=<period> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PING_PERIOD_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_PING_PERIOD, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PING_PERIOD_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_PING_PERIOD, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(PING)
{
    const char * p_usage = "<period>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(PING)
{
    const char * p_description = "Configure MQTT ping period";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(V311)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQV311=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_VER311, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQV311=<1|0> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_VER311, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_VER311, MQTT_PROTOCOL_V31 + tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(V311)
{
    const char * p_usage = "<use_v311>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(V311)
{
    const char * p_description = "Use MQTT protocol v3.1.1. default is v3.1";

    return p_description;
}

#if (ATCMD_IF_SUPPORT == 1)
RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(MSGFMVER)
{
#if CFG_PMGR
    extern int RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
#endif /* CFG_PMGR */

    int   tmp_int1 = 0;
    int   result_int = 0;
    int   at_resp_len = 0;

    char  at_resp[100] = {0};
    char  result_str[2] = {0};

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if (argc == 1 || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQMSGFMVER=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_AT_MSGFMT_VER, &result_int);
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    } 
    else if (argc == 2)
    {
        /* AT+NWMQMSGFMVER=<0|1> */
#if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;
            return err;
        }
#endif /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_AT_MSGFMT_VER, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
#if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_AT_MSGFMT_VER, tmp_int1);
#endif /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }
    
    return err;
}   

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSGFMVER)
{
    const char * p_usage = "<format_version>";
    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSGFMVER)
{
    const char * p_description = "Specify message format for +NWMQMSG. 0 (<message>,<topic>,<length>), 1 (<topic>,<length>,<message>)";
    return p_description;
}
#endif /* ATCMD_IF_SUPPORT == 1 */

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CID)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int at_resp_len = 0;

    char at_resp[100] = {0, };
    char result_str[MQTT_CLIENT_ID_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQCID=? */
        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, result_str, sizeof(result_str)) != CC_STATUS_SUCCESS)
        {
            // generate default cid if there's no cid stored in NVM
            char mac_id[5] = {0, };
            extern void id_number_output(char * id_num);

            id_number_output(mac_id);

            sprintf(result_str, "%s_%s", "ra6w1", mac_id);
        }

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQCID=<client_id> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_LEN;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, argv[1], 0, 0);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CID)
{
    const char * p_usage = "<client_id>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CID)
{
    const char * p_description = "MQTT Client ID";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(LI)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int at_resp_len = 0;

    char at_resp[250] = {0, };
    char result_str[MQTT_USERNAME_MAX_LEN + MQTT_PASSWORD_MAX_LEN + 2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQLI=? */
        char tmp_name[MQTT_USERNAME_MAX_LEN + 1] = {0, };
        char tmp_pw[MQTT_PASSWORD_MAX_LEN + 1]   = {0, };

        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, tmp_name, sizeof(tmp_name)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_USERNAME_NOT_EXIST;

            return err;
        }

        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, tmp_pw, sizeof(tmp_pw)) != CC_STATUS_SUCCESS)
        {
            sprintf(result_str, "%s", tmp_name);
        }
        else
        {
            sprintf(result_str, "%s,%s", tmp_name, tmp_pw);
        }

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);

        return err;
    }
    else if (argc == 2)
    {
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_USERNAME_LEN;

            return err;
        }
    }
    else if (argc == 3)
    {
        /* AT+NWMQLI=<name>,<pw> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_USERNAME_LEN;

            return err;
        }

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, argv[2]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PASSWORD_LEN;

            return err;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && ((argc == 2) || (argc == 3)))
    {
  #if CFG_PMGR
        if (argc == 2)
        {
            mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_USERNAME, argv[1], 0, 0);
        }
        else if (argc == 3)
        {
            mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_USERNAME, argv[1], 0, 0);
            mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_PASSWORD, argv[2], 0, 0);
        }
  #endif                               /* CFG_PMGR */
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(LI)
{
    const char * p_usage = "<name><pw>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(LI)
{
    const char * p_description = "Login information for MQTT operation";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(WILL)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[ATCMD_RSP_WILL_MAX_LEN] = {0, };
    char result_str[MQTT_TOPIC_MAX_LEN + MQTT_WILL_MSG_MAX_LEN + 4] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQWILL=? */
        char tmp_topic[MQTT_TOPIC_MAX_LEN + 1]  = {0, };
        char tmp_msg[MQTT_WILL_MSG_MAX_LEN + 1] = {0, };

        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, tmp_topic, sizeof(tmp_topic)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_TOPIC_NOT_EXIST;

            return err;
        }

        if (get_atcmd_param_str(RRQ61X_CONF_STR_MQTT_WILL_MSG, tmp_msg, sizeof(tmp_msg)) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_MESSAGE_NOT_EXIST;

            return err;
        }

        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_WILL_QOS, &result_int);
        sprintf(result_str, "%s,%s,%d", tmp_topic, tmp_msg, result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 4)
    {
        /* AT+NWMQWILL=<topic>,<msg>,<qos> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_TOPIC_LEN;

            return err;
        }

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_WILL_MSG, argv[2]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_MESSAGE_LEN;

            return err;
        }

        if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_QOS_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_WILL_QOS, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_QOS_RANGE;

            return err;
        }

  #if CFG_PMGR
        mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, argv[1], 0, 0);
        mqtt_client_cfg_sync_rtm(RRQ61X_CONF_STR_MQTT_WILL_MSG, argv[2], 0, 0);
        mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_WILL_QOS, tmp_int1);
  #endif                               /* CFG_PMGR */
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(WILL)
{
    const char * p_usage = "<topic><msg><qos>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(WILL)
{
    const char * p_description = "Will information for MQTT";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(DEL)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);

    extern mqttParamForRtm mqttParams;
  #endif                               /* CFG_PMGR */

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

  #if CFG_PMGR
    if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
    {
        err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

        return err;
    }
  #endif                               /* CFG_PMGR */

    mqtt_client_config_initialize();

  #if CFG_PMGR
    if (mqtt_client_is_cfg_dpm_mem_intact())
    {
        memset(&mqttParams, 0x00, sizeof(mqttParamForRtm));
        mqttParams.port      = MQTT_CONFIG_PORT_DEF;
        mqttParams.keepalive = MQTT_CONFIG_PING_DEF;

        mqtt_client_save_to_dpm_user_mem();
    }
  #endif                               /* CFG_PMGR */
    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(DEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(DEL)
{
    const char * p_description = "Initialize MQTT Configurations";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CL)
{
    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQCL=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_SUB, &result_int);
        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQCL=<mqtt_client> */

        int ops_result = CC_STATUS_SUCCESS;

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;

            return err;
        }

        if (!(tmp_int1 == 0 && mqtt_client_is_running() == false))
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK_WO_PRINT;
            at_resp_len = sprintf(at_resp, "\r\n%s\r\n", "OK");
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }

        ops_result = set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_SUB, tmp_int1);
        if (ops_result != CC_STATUS_SUCCESS)
        {
            if (ops_result == CC_FAILURE_NOT_READY)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NOT_CONNECTED;
            }
            else
            {
                // possible return : 9 (mqtt start fail)
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_CLIENT_TASK_START;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CL)
{
    const char * p_usage = "<mqtt_client>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CL)
{
    const char * p_description = "Enable/Disable MQTT Client";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(MSG)
{
  #if CFG_PMGR
    extern int RM_PMGR_W_dpm_is_enabled(void);
  #endif                               /* CFG_PMGR */
    extern int mqtt_client_get_qos(void);
    extern int mqtt_client_send_message(char * top, char * publish);

    char * top                 = NULL;
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    if ((argc == 2) || (argc == 3))
    {
        int rsp_val;

        /* AT+NWMQMSG=<msg>(,<topic>) (set only) */
        if (strlen(argv[1]) > MQTT_MSG_MAX_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_MESSAGE_LEN;

            return err;
        }

        if (argc == 2)
        {
            top = NULL;
        }
        else if (argc == 3)
        {
            if (strlen(argv[2]) > MQTT_TOPIC_MAX_LEN)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_LEN;

                return err;
            }
            else
            {
                top = argv[2];
            }
        }

        rsp_val = mqtt_client_send_message(top, argv[1]);

        if (rsp_val == -1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NOT_CONNECTED;
        }
        else if (rsp_val == -2)
        {
            // in-flight message is in progress
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TX_IN_PROGRESS;
        }
        else if (rsp_val == -3)
        {
            // no topic in mqtt configuration
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_NOT_EXIST;
        }
        else if (rsp_val == 9 /* MOSQ_ERR_PAYLOAD_SIZE */)
        {
            // paylod too long
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_MESSAGE_LEN;
        }
        else
        {
            // pre-check passed ...
  #if CFG_PMGR
            if (RM_PMGR_W_dpm_is_enabled() == pdTRUE)
            {
                if (mqtt_client_get_qos() > 0)                                                    // qos 1 or 2
                {
                    if (rsp_val != 0)
                    {
                        rm_atcmd_w_mqtt_client_convert_to_atcmd_err_code(&rsp_val, (int *) &err); // tmp
                    }
                }
                else                                                                              // qos 0
                {
                    if (rsp_val != 0)
                    {
                        rm_atcmd_w_mqtt_client_convert_to_atcmd_err_code(&rsp_val, (int *) &err); // tmp
                    }
                    else
                    {
                        // make async +NWMQMSGSND:1 to sync print
                        atcmd_mqtt_qos0_msg_send_done_in_dpm = TRUE;
                        atcmd_mqtt_qos0_msg_send_rc          = 0;
                    }
                }
            }
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSG)
{
    const char * p_usage = "<msg>(<topic>)";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSG)
{
    const char * p_description = "Send message by MQTT Publisher";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TT)
{
    int tmp_int1, tmp_int2 = 0;

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if ((argc == 7) || (argc == 9))
    {
        int tmp_int3;

        if (rm_atcmd_w_core_common_stoi(argv[2], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_PORT_NUM_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_stoi(argv[5], &tmp_int2, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_QOS_TYPE;

            return err;
        }

        if (rm_atcmd_w_core_common_stoi(argv[6], &tmp_int3, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_TYPE;

            return err;
        }

        /* AT+NWMQTT=<ip>,<port>,<sub_topic>,<pub_topic>,<qos>,<tls>(,<username>,<password>) (set only) */
        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, argv[1]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_NAME_LEN;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_PORT, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_BROKER_PORT_NUM_RANGE;

            return err;
        }

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_SUB_TOPIC, argv[3]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_SUBS_TOPIC_LEN;

            return err;
        }

        if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, argv[4]) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_LEN;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_QOS, tmp_int2) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_WILL_QOS_RANGE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS, tmp_int3) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_TLS_RANGE;

            return err;
        }

        if (argc == 9)
        {
            if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, argv[7]) != CC_STATUS_SUCCESS)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_USERNAME_LEN;

                return err;
            }

            if (set_atcmd_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, argv[8]) != CC_STATUS_SUCCESS)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PASSWORD_LEN;

                return err;
            }
        }

        set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_AUTO, 1);

        reset();
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TT)
{
    const char * p_usage = "<ip><port><sub_topic><pub_topic><qos><tls>(<username><password>)";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TT)
{
    const char * p_description = "MQTT Client (Subscriber/Publisher) Operation";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(AUTO)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char  at_resp[100] = {0, };

    char  result_str[2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQAUTO=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_AUTO, &result_int);

        if (result_int == MQTT_INIT_MAGIC)
        {
            result_int = 1;
        }
        else
        {
            result_int = 0;
        }

        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQAUTO=<auto> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_AUTO, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_AUTO, (!tmp_int1) ? 0 : MQTT_INIT_MAGIC);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(AUTO)
{
    const char * p_usage = "<auto>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(AUTO)
{
    const char * p_description = "Configure MQTT Auto Start";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSBUFIN)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100] = {0, };
    char result_str[ATCMD_RSP_INT_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTLSBUFIN=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_INCOMING, &result_int);
        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQTLSBUFIN=<size> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_INCOMING, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_TLS_INCOMING, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSBUFIN)
{
    const char * p_usage = "<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSBUFIN)
{
    const char * p_description = "Set MQTT TLS INCOMING buffer size";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSBUFOUT)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100] = {0, };
    char result_str[ATCMD_RSP_INT_MAX_LEN + 1] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTLSBUFOUT=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_OUTGOING, &result_int);
        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQTLSBUFOUT=<size> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_OUTGOING, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_TLS_OUTGOING, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSBUFOUT)
{
    const char * p_usage = "<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSBUFOUT)
{
    const char * p_description = "Set MQTT TLS OUTGOING buffer size";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(TLSAUTH)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQTLSAUTH=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE, &result_int);
        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQTLSAUTH=<mode> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(TLSAUTH)
{
    const char * p_usage = "<authmode>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(TLSAUTH)
{
    const char * p_description = "Set MQTT TLS AUTHMODE";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_CB(CS)
{
  #if CFG_PMGR
    extern int    RM_PMGR_W_dpm_is_enabled(void);
    extern USHORT is_mqtt_client_thd_alive(void);
  #endif                               /* CFG_PMGR */

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char at_resp[100]  = {0, };
    char result_str[2] = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_mqtt_is_query_arg(argc, argv[1]))
    {
        /* AT+NWMQCS=? */
        get_atcmd_param_int(RRQ61X_CONF_INT_MQTT_CLEAN_SESSION, &result_int);
        sprintf(result_str, "%d", result_int);

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWMQCS=<clean_session> */
  #if CFG_PMGR
        if ((is_mqtt_client_thd_alive() == pdTRUE) && (RM_PMGR_W_dpm_is_enabled() == pdTRUE))
        {
            printf("Stop mqtt_client first before (re)configuration.\n");
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NEED_TO_STOP;

            return err;
        }
  #endif                               /* CFG_PMGR */

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;

            return err;
        }

        if (set_atcmd_param_int(RRQ61X_CONF_INT_MQTT_CLEAN_SESSION, tmp_int1) != CC_STATUS_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
  #if CFG_PMGR
            mqtt_client_cfg_sync_rtm(0, NULL, RRQ61X_CONF_INT_MQTT_CLEAN_SESSION, tmp_int1);
  #endif                               /* CFG_PMGR */
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(CS)
{
    const char * p_usage = "<clean_session>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(CS)
{
    const char * p_description = "set clean session mode for MQTT";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_UNFIXED_ATCMD_CB(MSGBIN)
{
    typedef enum
    {
        MSG_BIN_IS_DEF_TOP, // 0
        MSG_BIN_TOP_LEN,    // 1
        MSG_BIN_TOP,        // 2
        MSG_BIN_DATA_LEN,   // 3
        MSG_BIN_DATA        // 4
    } atcmd_msgbin_cmd_parameter_step;

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t fsp_err = FSP_SUCCESS;

    char ch = 0;
    char param_atcmd[MQTT_TOPIC_MAX_LEN + 1] = {0x00};
    int param_atcmd_idx = 0;
    unsigned int is_done = pdFALSE;
    atcmd_msgbin_cmd_parameter_step param_step = MSG_BIN_IS_DEF_TOP;

    int is_def_top = 0;
    int data_len = 0;
    char * p_data = NULL;
    int top_len = 0;
    char * p_top = NULL;
    char resp_str[32] = {0x00};
    int rsp_val = 0;

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

    /* Get ATCMD parameters: AT+NWMQMSGBIN=<is_def_top>,<top_len>,<top>,<data_len>,<data> */
    while (err == FSP_ERR_AT_CMD_ERR_CMD_OK && !is_done)
    {
        switch (param_step)
        {
            case MSG_BIN_IS_DEF_TOP:
            case MSG_BIN_TOP_LEN:
            case MSG_BIN_TOP:
            case MSG_BIN_DATA_LEN:

                /* Read a parameter */
                memset(param_atcmd, 0x00, sizeof(param_atcmd));
                param_atcmd_idx = 0;
                ch = 0x00;

                if (param_step != MSG_BIN_TOP || (param_step == MSG_BIN_TOP && (is_def_top && top_len == 0)))
                {
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
                    }
                }
                else
                {
                    while (param_atcmd_idx < top_len + 1)
                    {
                        fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(uint8_t));
                        if (fsp_err != FSP_SUCCESS)
                        {
                            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                            goto end;
                        }

                        if (param_atcmd_idx == top_len && ch == 0x2C)
                        {
                            break;
                        }
                        else
                        {
                            param_atcmd[param_atcmd_idx++] = ch;
                        }
                    }

                    param_atcmd[param_atcmd_idx] = '\0';
                }

                /* For each parameter, check the validity and store */
                if (param_step == MSG_BIN_IS_DEF_TOP)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &is_def_top, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
                        goto end;
                    }

                    if (rm_atcmd_w_core_common_is_in_valid_range(is_def_top, 0, 1) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
                        goto end;
                    }

                    param_step = MSG_BIN_TOP_LEN;

                }
                else if (param_step == MSG_BIN_TOP_LEN)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &top_len, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
                        goto end;
                    }

                    if (rm_atcmd_w_core_common_is_in_valid_range(top_len, 1, MQTT_TOPIC_MAX_LEN) == pdFALSE)
                    {
                        if (!(is_def_top && top_len == 0))
                        {
                            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_LEN;
                            goto end;
                        }
                    }

                    param_step = MSG_BIN_TOP;
                }
                else if (param_step == MSG_BIN_TOP)
                {
                   if (!(is_def_top && top_len == 0))
                   {
                       p_top = pvPortMalloc(top_len + 1);
                       if (p_top == NULL)
                       {
                           err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                           break;
                       }
                       memset(p_top, 0x00, top_len + 1);

                       memcpy(p_top, param_atcmd, top_len + 1);
                   }

                    param_step = MSG_BIN_DATA_LEN;
                }
                else if (param_step == MSG_BIN_DATA_LEN)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &data_len, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    if (rm_atcmd_w_core_common_is_in_valid_range(data_len, 1, MQTT_MSG_MAX_LEN) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_LEN;
                        goto end;
                    }

                    param_step = MSG_BIN_DATA;
                }
                break;

            /* Store message */

            case MSG_BIN_DATA:
                p_data = pvPortMalloc(data_len);
                if (p_data == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                    break;
                }
                memset(p_data, 0x00, data_len);

                fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) p_data, data_len);
                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                    break;
                }

                is_done = pdTRUE;
                break;
        }
    }

    /* Send the message */
    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        rsp_val = mqtt_client_send_message_v2(p_top, p_data, data_len);

        if (rsp_val == -1) 
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_NOT_CONNECTED;
        }
        else if (rsp_val == -2)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TX_IN_PROGRESS;
        }
        else if (rsp_val == -3)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_TOPIC_NOT_EXIST;
        }
        else if (rsp_val == 9 /* MOSQ_ERR_PAYLOAD_SIZE */)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MQTT_PUB_MESSAGE_LEN;
        }
        else
        {
            // pre-check passed ...
#if CFG_PMGR
            if (RM_PMGR_W_dpm_is_enabled() == pdTRUE) 
            {
                if (mqtt_client_get_qos() ==  0 && rsp_val == 0)
                {
                    // make async +NWMQMSGSND:1 to sync print
                    atcmd_mqtt_qos0_msg_send_done_in_dpm = TRUE;
                    atcmd_mqtt_qos0_msg_send_rc = 0;
                }

                if (rsp_val != 0)              
                {
                    rm_atcmd_w_mqtt_client_convert_to_atcmd_err_code(&rsp_val, (int* )&err);   
                }
            }
#endif /* CFG_PMGR */
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

    if (p_data)
    {
        vPortFree(p_data);
        p_data = NULL;
    }

    if (p_top)
    {
        vPortFree(p_top);
        p_top = NULL;
    }    

    return err;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_FORMAT_CB(MSGBIN)
{
    const char * p_usage = "<is_default_topic>,<topic>,<data_len>,<data>";
    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_MQTT_ATCMD_BRIEF_CB(MSGBIN)
{
    const char * p_description = "Send a Publish message in binary format";
    return p_description;
}

 #else                                 /* ATCMD_W_MQTT_EXIST */
void RM_ATCMD_W_CORE_NETWORK_MQTT_RESP_Handle (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_register (atcmd_w_core_module_list_t * p_list)
{
    FSP_PARAMETER_NOT_USED(p_list);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_deregister (atcmd_w_core_module_list_t * p_list)
{
    FSP_PARAMETER_NOT_USED(p_list);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_MQTT_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

 #endif                                /* ATCMD_W_MQTT_EXIST */
#endif                                 /* CFG_WIFI */
