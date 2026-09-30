/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "net_dns_client.h"
#if CFG_WIFI

 #include "rm_atcmd_w_core_network_parse.h"
 #include "rm_atcmd_w_core_basic_parse.h"
 #include "rm_atcmd_w_core_err_code.h"
 #include "rm_atcmd_w_core.h"
 #include "ctype.h"
 #include "strings.h"
 #include "FreeRTOS.h"
 #include "task.h"
 #include "rm_wifi.h"
 #include "util_api.h"
 #include "fw_version.h"
 #include "iface_defs.h"
 #include "supp_config.h"
 #include "lwip/ip_addr.h"
 #include "rm_lwip_w_helper.h"
 #include "rm_wifi.h"
 #include "rm_wifi_helper.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "dhcpserver.h"

 #include "rm_cert.h"
 #include "lwip/ip_addr.h"
 #include "lwip/netif.h"
 #include "lwip/apps/dhcpserver_options.h"
 #include "lwip/dhcp.h"

 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
  #include dg_configADNVPARAM_PROJ_FILE
 #endif

 #if !CFG_PMGR
  #include "defs.h"
 #endif                                /* !CFG_PMGR */

 #pragma GCC diagnostic ignored "-Waggregate-return"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(atcmd)    "AT+NW" # atcmd

 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _format_cb(void)
 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _brief_cb(void)

 #define RM_ATCMD_W_CORE_NETWORK_UNFIXED_ATCMD_CB(atcmd)                                                    \
    uint32_t RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, \
                                                          size_t inlen)

 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _cmd_cb
 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _format_cb
 #define RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(atcmd) \
    RM_ATCMD_W_CORE_NETWORK_ ## atcmd ## _brief_cb

 #define RM_ATCMD_W_CORE_NETWORK_ERROR(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct _list_node
{
    void              * pnode;
    struct _list_node * pnext;
} list_node;

typedef signed char s8_t;
typedef s8_t        err_t;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(IP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(IP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(IP);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DNS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DNS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DNS);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DNS2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DNS2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DNS2);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(HOST);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(HOST);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(HOST);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(PING);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(PING);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(PING);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHC);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHC);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHC);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHCHN);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHCHN);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHCHN);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHCHNDEL);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHCHNDEL);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHCHNDEL);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHR);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHR);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHR);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHLT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHLT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHLT);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHS);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHIP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHIP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHIP);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS1);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS1);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS1);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS2);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNUP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNUP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNUP);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP1);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP1);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP1);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP2);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP2);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(CCRT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CCRT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CCRT);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(CCRTR);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CCRTR);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CCRTR);

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DCRT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DCRT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DCRT);

RM_ATCMD_W_CORE_NETWORK_UNFIXED_ATCMD_CB(CERT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CERT);
RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CERT);

 #if (ATCMD_SECURE_CHANNEL == 1)

/* Secure channel needs fixed parse */
uint32_t RM_ATCMD_W_CORE_NETWORK_CERT_fixed_cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[]);

 #endif

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
extern char atcmd_mac_table[6][18];

const atcmd_w_core_module_t at_core_network_module[] =
{
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(IP),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(IP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(IP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(IP),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DNS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DNS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DNS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DNS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DNS2),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DNS2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DNS2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DNS2),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(HOST),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(HOST),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(HOST),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(HOST),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(PING),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(PING),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(PING),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(PING),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHC),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHC),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHC),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHC),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHCHN),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHCHN),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHCHN),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHCHN),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHCHNDEL),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHCHNDEL),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHCHNDEL),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHCHNDEL),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHR),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHR),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHR),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHLT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHLT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHLT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHLT),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHS),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DHIP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DHIP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DHIP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DHIP),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNS),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNS),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNS),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNS1),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNS1),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNS1),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNS1),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNS2),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNS2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNS2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNS2),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNUP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNUP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNUP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNUP),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNTP),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNTP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNTP),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNTP),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNTP1),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNTP1),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNTP1),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNTP1),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(SNTP2),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(SNTP2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(SNTP2),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(SNTP2),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(CCRT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(CCRT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(CCRT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(CCRT),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(CCRTR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(CCRTR),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(CCRTR),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(CCRTR),
    },
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(DCRT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(DCRT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(DCRT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(DCRT),
    },
 #if (ATCMD_SECURE_CHANNEL == 1)
    {
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CODE(CERT), // "\x1B""CERT",
        ATCMD_W_TYPE_SECURE_UNFIXED,
        1,
        0,
        RM_ATCMD_W_CORE_NETWORK_CERT_fixed_cmd_cb,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(CERT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(CERT),
    },
 #endif
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

const atcmd_w_core_unfixed_module_t at_core_network_unfixed_module[] =
{
    {
        {AT_CMD_ESC_KEY_CHAR, 'C', 'E', 'R', 'T', 0x00, },
        5,
        RM_ATCMD_W_CORE_NETWORK_ATCMD_CB_P(CERT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB_P(CERT),
        RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB_P(CERT),
    },
    {
        "",
        0,
        NULL,
        NULL,
        NULL
    },
};

/***********************************************************************************************************************
 * Extern variables
 **********************************************************************************************************************/
extern char atcmd_mac_table[6][18];

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*
 * Calculate number of digit ex) 1234 -> 4
 */
static unsigned int getDigitNum (unsigned long num)
{
    unsigned int count = 0;

    while (num != 0)
    {
        num = num / 10;
        count++;
    }

    return count;
}

static int rm_atcmd_w_core_network_is_query_arg (int argc, char * str)
{
    return argc == 2 && strcmp(str, AT_CMD_GET_MRK) == 0;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_register (atcmd_w_core_module_list_t * p_list)
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

    if (rm_atcmd_w_core_register_module_node(p_list, at_core_network_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (rm_atcmd_w_core_register_unfixed_module_node(p_list,
                                                     at_core_network_unfixed_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_NETWORK_deregister (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    rm_atcmd_w_core_deregister(p_list, at_core_network_module);
    rm_atcmd_w_core_unfixed_deregister(p_list, at_core_network_unfixed_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(IP)
{
    extern int  get_netmode(int iface);
    extern UINT get_ip_info(int iface_flag, int info_flag, char * result_str);
    extern int  ip_change(UINT iface, char * ipaddress, char * netmask, char * gateway, UCHAR save);
    extern void ra6w1_set_temp_staticip_mode(int mode, int save);

    int    tmp_int1        = 0;
    int    at_resp_len     = 0;
    char   at_resp[128]    = {0, };
    char   result_str[115] = {0, };
    char * result_ptr      = NULL;

    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWIP=? */
        char ip_str[16] = {0, }, sm_str[16] = {0, }, gw_str[16] = {0, };

        switch (get_run_mode())
        {
            case WIFI_DEVICE_MODE_EXT_STATION:
            {
                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_IPADDR, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(ip_str, result_str, sizeof(ip_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_IPADDR_0,
                                                    &result_ptr);
 #endif
                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(ip_str, result_ptr, sizeof(ip_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_SUBNET, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(sm_str, result_str, sizeof(sm_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_NETMASK_0,
                                                    &result_ptr);
 #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(sm_str, result_ptr, sizeof(sm_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_GATEWAY, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(gw_str, result_str, sizeof(gw_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_GATEWAY_0,
                                                    &result_ptr);
 #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(gw_str, result_ptr, sizeof(gw_str));
                    }
                }

                if (strlen(gw_str) == 0)
                {
                    bsp_safe_strcpy(gw_str, "0.0.0.0", sizeof(gw_str));
                }

                sprintf(result_str, "%d,%s,%s,%s", 0, ip_str, sm_str, gw_str);
                break;
            }

            case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
            case WIFI_DEVICE_MODE_EXT_P2P:
            case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // ( __SUPPORT_P2P__ )
            {
                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_IPADDR, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(ip_str, result_str, sizeof(ip_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_IPADDR_1,
                                                    &result_ptr);
 #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(ip_str, result_ptr, sizeof(ip_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_SUBNET, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(sm_str, result_str, sizeof(sm_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_NETMASK_1,
                                                    &result_ptr);
 #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(sm_str, result_ptr, sizeof(sm_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_GATEWAY, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(gw_str, result_str, sizeof(gw_str));
                    }
                }
                else
                {
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_GATEWAY_1,
                                                    &result_ptr);
 #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(gw_str, result_ptr, sizeof(gw_str));
                    }
                }

                if (strlen(gw_str) == 0)
                {
                    bsp_safe_strcpy(gw_str, "0.0.0.0", sizeof(gw_str));
                }

                sprintf(result_str, "%d,%s,%s,%s", 1, ip_str, sm_str, gw_str);
                break;
        }

 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
            {
                char tmp_str[64] = {0x00, };

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_IPADDR, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(ip_str, result_str, sizeof(ip_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_IPADDR_0,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(ip_str, result_ptr, sizeof(ip_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_SUBNET, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(sm_str, result_str, sizeof(sm_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_NETMASK_0,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(sm_str, result_ptr, sizeof(sm_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(0, GET_GATEWAY, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(gw_str, result_str, sizeof(gw_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_GATEWAY_0,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(gw_str, result_ptr, sizeof(gw_str));
                    }
                }

                if (strlen(gw_str) == 0)
                {
                    bsp_safe_strcpy(gw_str, "0.0.0.0", sizeof(gw_str));
                }

                sprintf(tmp_str, "%d,%s,%s,%s", 0, ip_str, sm_str, gw_str);

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_IPADDR, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(ip_str, result_str, sizeof(ip_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_IPADDR_1,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(ip_str, result_ptr, sizeof(ip_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_SUBNET, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(sm_str, result_str, sizeof(sm_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_NETMASK_1,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(sm_str, result_ptr, sizeof(sm_str));
                    }
                }

                if (get_netmode(0) == DHCPCLIENT)
                {
                    if (get_ip_info(1, GET_GATEWAY, result_str) == pdPASS)
                    {
                        bsp_safe_strcpy(gw_str, result_str, sizeof(gw_str));
                    }
                }
                else
                {
  #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                    ENV_GROUP_WIFIPROFILE,
                                                    WIFI_PROFILE_GATEWAY_1,
                                                    &result_ptr);
  #endif

                    if (result_ptr && strlen(result_ptr))
                    {
                        bsp_safe_strcpy(gw_str, result_ptr, sizeof(gw_str));
                    }
                }

                if (strlen(gw_str) == 0)
                {
                    bsp_safe_strcpy(gw_str, "0.0.0.0", sizeof(gw_str));
                }

                sprintf(result_str, "%s,%d,%s,%s,%s", tmp_str, (int) 1, ip_str, sm_str, gw_str);
            }
            break;
 #endif                                // __SUPPORT_WIFI_CONCURRENT__
    }                                  // end of switch

    at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
    RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
}
else if (argc == 5)
{
    /* AT+NWIP=<interface>,<ipaddress>,<netmask>,<gateway> */
    if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_IP_IFACE_TYPE;
        goto end;
    }
    else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 1) == pdFALSE)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_IP_IFACE_RANGE;
        goto end;
    }

 #if defined(__SUPPORT_IPV4__)         // temp for build
    if (is_in_valid_ip_class(argv[2]) == pdFALSE)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_IP_INVALID_ADDR;
        goto end;
    }
    else if (is_in_valid_ip_class(argv[4]) == pdFALSE)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_IP_GATEWAY;
        goto end;
    }
    else if ((isvalidip(argv[3]) == pdFALSE) ||
             (ip4_addr_netmask_valid(ipaddr_addr(argv[3])) == pdFALSE))
    {
        err = FSP_ERR_AT_CMD_ERR_NW_IP_NETMASK;
        goto end;
    }
    else
    {
        ip_addr_t tmp_addr;
        uint32_t  ipaddress;           // argv[2]
        uint32_t  netmask;             // argv[3]
        uint32_t  gateway;             // argv[4]

        ipaddr_aton(argv[2], &tmp_addr);
        ipaddress = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(argv[3], &tmp_addr);
        netmask = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(argv[4], &tmp_addr);
        gateway = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        if (isvalidIPsubnetRange(gateway, ipaddress, netmask) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_IP_GATEWAY;
            goto end;
        }
        else if (isvalidIPsubnetRange(ipaddress, ipaddress, netmask) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_IP_INVALID_ADDR;
            goto end;
        }
    }
 #endif                                // __SUPPORT_IPV4__

 #if LWIP_DHCP
    if (get_netmode(tmp_int1) == DHCPCLIENT)
    {
        struct netif * netif = netif_get_by_index(tmp_int1 + 2);
        dhcp_stop(netif);
    }
 #endif                                /* LWIP_DHCP */

    if (ip_change(tmp_int1, argv[2], argv[3], argv[4], 1) != pdPASS)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_NET_IF_NOT_INITIALIZE;
        goto end;
    }

    if (tmp_int1 == WLAN0_IFACE)
    {
        ra6w1_set_temp_staticip_mode(pdFALSE, pdFALSE);
    }
}
else if (argc < 5)
{
    err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    goto end;
}
else                                   // argc > 5
{
    err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    goto end;
}

end:

return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(IP)
{
    const char * p_usage = "<iface>,<ip_addr>,<netmask>,<gw>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(IP)
{
    const char * p_description = "Setting for IP Address";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DNS)
{
    extern int  get_netmode(int iface);
    extern UINT get_ip_info(int iface_flag, int info_flag, char * result_str);
    extern int  set_dns_addr(int iface, char * ip_addr);

    int at_resp_len = 0;

    char * result_ptr   = NULL;
    char result_str[16] = {0, };
    char dns_str[16]    = {0, };
    char at_resp[25]    = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDNS=? */
        switch (get_run_mode())
        {
            case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
            case WIFI_DEVICE_MODE_EXT_P2P:
            case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // __SUPPORT_P2P__
        {
                err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
                break;
        }

        case WIFI_DEVICE_MODE_EXT_STATION:
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
 #endif                                // __SUPPORT_WIFI_CONCURRENT__
        {
            if (get_netmode(0) == DHCPCLIENT)
            {
                if ((get_ip_info(0, GET_DNS, result_str) == pdPASS) &&
                    (strcmp(result_str, "0.0.0.0") != 0))
                {
                    bsp_safe_strcpy(dns_str, result_str, sizeof(dns_str));
                }
                else
                {
                    err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
                }
            }
            else
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_DNSSVR_0,
                                                &result_ptr);
 #endif

                if (result_ptr && strlen(result_ptr))
                {
                    bsp_safe_strcpy(dns_str, result_ptr, sizeof(dns_str));
                }
                else
                {
                    bsp_safe_strcpy(dns_str, DEFAULT_DNS_WLAN0, sizeof(dns_str));
                }
            }

            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), dns_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
            break;
    }
}
}
else if (argc == 2)
{
    /* AT+NWDNS=<dns_ip> */
    switch (get_run_mode())
    {
        case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P:
        case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // __SUPPORT_P2P__
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
            break;
    }

    case WIFI_DEVICE_MODE_EXT_STATION:
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
    case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
    case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
 #endif                                // __SUPPORT_WIFI_CONCURRENT__
    {

        if (!is_in_valid_ip_class(argv[1]))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_IP_ADDR_CLASS;
        }
        else
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_WIFIPROFILE,
                                             WIFI_PROFILE_DNSSVR_0,
                                             argv[1]);
 #endif
            set_dns_addr(0, argv[1]);  // iface: 0
        }

        break;
}
}
}
else
{
    err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
}

return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DNS)
{
    const char * p_usage = "<dns_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DNS)
{
    const char * p_description = "Setting for DNS";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DNS2)
{
    extern int  get_netmode(int iface);
    extern UINT get_ip_info(int iface_flag, int info_flag, char * result_str);
    extern int  set_dns_addr_2nd(int iface, char * ip_addr);

    int at_resp_len = 0;

    char * result_ptr   = NULL;
    char result_str[16] = {0, };
    char dns_str_2[16]  = {0, };
    char at_resp[128]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDNS2=? */
        switch (get_run_mode())
        {
            case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
            case WIFI_DEVICE_MODE_EXT_P2P:
            case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // __SUPPORT_P2P__
            {
                err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
                break;
        }

        case WIFI_DEVICE_MODE_EXT_STATION:
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
        case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
 #endif                                // __SUPPORT_WIFI_CONCURRENT__
        {
            if (get_netmode(0) == DHCPCLIENT)
            {
                if ((get_ip_info(0, GET_DNS_2ND, result_str) == pdPASS) && (strcmp(result_str, "0.0.0.0") != 0))
                {
                    bsp_safe_strcpy(dns_str_2, result_str, sizeof(dns_str_2));
                }
                else
                {
                    err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
                }
            }
            else
            {
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_WIFIPROFILE,
                                                WIFI_PROFILE_DNSSVR_2ND_0,
                                                &result_ptr);
 #endif

                if (result_ptr && strlen(result_ptr))
                {
                    bsp_safe_strcpy(dns_str_2, result_ptr, sizeof(dns_str_2));
                }
                else
                {
                    bsp_safe_strcpy(dns_str_2, DEFAULT_DNS_2ND, sizeof(dns_str_2));
                }
            }

            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), dns_str_2);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
            break;
    }
}
}
else if (argc == 2)
{
    /* AT+NWDNS2=<dns_2nd_ip> */

    switch (get_run_mode())
    {
        case WIFI_DEVICE_MODE_EXT_AP:
 #if defined(__SUPPORT_P2P__)
        case WIFI_DEVICE_MODE_EXT_P2P:
        case WIFI_DEVICE_MODE_EXT_P2P_GO:
 #endif                                // __SUPPORT_P2P__
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            break;
    }

    case WIFI_DEVICE_MODE_EXT_STATION:
 #if defined(__SUPPORT_WIFI_CONCURRENT__)
    case WIFI_DEVICE_MODE_EXT_AP_STATION:
  #if defined(__SUPPORT_P2P__)
    case WIFI_DEVICE_MODE_EXT_P2P_STATION:
  #endif                               // __SUPPORT_P2P__
 #endif                                // __SUPPORT_WIFI_CONCURRENT__
    {

        if (!is_in_valid_ip_class(argv[1]))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_IP_ADDR_CLASS;
        }
        else
        {
 #if 0                                 // def RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_SYSCFG,
                                             WIFI_PROFILE_DNSSVR_2ND_0,
                                             argv[1]);
 #endif
            set_dns_addr_2nd(0, argv[1]);
        }

        break;
}
}
}
else
{
    err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
}

return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DNS2)
{
    const char * p_usage = "<dns_2nd_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DNS2)
{
    const char * p_description = "Setting for 2nd DNS";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(HOST)
{
    extern bool dns_A_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option);

    int temp_str_len   = 0;
    char temp_str[256] = {0, };
    bool result = pdFALSE;

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if (argc == 2)
    {
        /* AT+NWHOST=<ip> */
        ULONG dns_query_wait_option = DNS_RESOLVE_DEFAULT_TIMEOUT_MS;
        char ip[IPADDR_LEN] = {0,};
        result = dns_A_Query(argv[1], ip, dns_query_wait_option);

        if (result == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DNS_A_QUERY_FAIL;
        }
        else
        {
            temp_str_len = sprintf(temp_str, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), ip);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) temp_str, temp_str_len);
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(HOST)
{
    const char * p_usage = "<name>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(HOST)
{
    const char * p_description = "Get Host IP Address By Name";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(PING)
{
    extern unsigned int ra6w1_network_main_check_network_ready(unsigned char iface);
    extern unsigned int ra6w1_ping_client(int             iface,
                                          char          * domain_str,
                                          unsigned long   ipaddr,
                                          unsigned long * ipv6dst,
                                          unsigned long * ipv6src,
                                          int             len,
                                          uint64_t        max_count,
                                          int             wait,
                                          int             interval,
                                          int             nodisplay,
                                          char          * ping_result);

    int at_resp_len = 0;
    int tmp_int1    = 0;

    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if (argc == 4)
    {
        ip_addr_t tmp_addr;

        /* AT+NWPING=<iface>,<dst_ip>,<count> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_PING_IFACE_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_PING_IFACE_ARG_RANGE;
            goto end;
        }

        if (!ra6w1_network_main_check_network_ready(tmp_int1))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
            goto end;
        }

 #if defined(__SUPPORT_IPV4__)         // temp for build
        if ((is_in_valid_ip_class(argv[2])) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_PING_DST_ADDR;
            goto end;
        }

        if ((atol(argv[3]) < 1) || (strlen(argv[3]) != getDigitNum(atol(argv[3]))))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_PING_TX_COUNT;
            goto end;
        }

        ipaddr_aton(argv[2], &tmp_addr);

        err = ra6w1_ping_client(tmp_int1,
                                NULL,
                                lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr))),
                                NULL,
                                NULL,
                                DEFAULT_PING_SIZE,
                                (atol(argv[3])),
                                DEFAULT_PING_WAIT,
                                DEFAULT_INTERVAL,
                                pdTRUE,
                                result_str);

        if (err == pdFAIL)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_PING_DST_ADDR;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
 #endif                                // __SUPPORT_IPV4__
    }
    else
    {
        if (argc < 4)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && (strlen(result_str) > 0))
    {
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(PING)
{
    const char * p_usage = "<iface>,<dst_ip_addr>,<count>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(PING)
{
    const char * p_description = "Ping test";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHC)
{
    extern struct netif * netif_get_by_index(u8_t idx);
    extern UINT           set_netmode(UCHAR iface, UCHAR mode, UCHAR save);

 #if CFG_PMGR
    extern int RM_WIFI_dpm_supp_is_connected(void);
 #endif                                /* CFG_PMGR */

    int tmp         = 0;
    int result_int  = 0;
    int tmp_int1    = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDHC=? */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, WIFI_PROFILE_NETMODE_0,
                                     (int *) &tmp);
 #endif

        if (tmp == -1)
        {
            result_int = CC_VAL_ENABLE;
        }
        else
        {
            if (tmp == DHCPCLIENT)
            {
                result_int = CC_VAL_ENABLE;
            }
            else
            {
                result_int = CC_VAL_DISABLE;
            }
        }

        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWDHC=<dhcpc> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 0, 1) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

 #if LWIP_DHCP
        UINT iface           = WLAN0_IFACE;
        struct netif * netif = netif_get_by_index(iface + 2);
        int dhcp_err         = 0;      // Success
 #endif                                /* LWIP_DHCP */

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        }

        if (tmp_int1 == CC_VAL_ENABLE)
        {
 #if CFG_WIFI
  #if CFG_PMGR
            if (RM_WIFI_dpm_supp_is_connected())
  #else
            if (rm_wifi_is_wpa_state(WPA_COMPLETED, 0) == FSP_SUCCESS)
  #endif                               /* CFG_PMGR */
 #endif                                /* CFG_WIFI */
            {
 #if LWIP_DHCP
                dhcp_err = dhcp_start(netif);

                if (dhcp_err == 0)     // Success
                {
                    RM_ATCMD_W_CORE_NETWORK_ERROR("\nDHCP Client Started WLAN%u.\n", iface);
                }
                else
                {
                    RM_ATCMD_W_CORE_NETWORK_ERROR("\nDHCP Client Start Error(%d) WLAN%u.\n", err, iface);
                    err = FSP_ERR_AT_CMD_ERR_NW_DHCPC_START_FAIL;
                }
 #endif                                /* LWIP_DHCP */
            }

            set_netmode(0, DHCPCLIENT, pdTRUE);
        }
        else
        {
 #if LWIP_DHCP
            dhcp_stop(netif);
 #endif                                /* LWIP_DHCP */
            set_netmode(0, STATIC_IP, pdTRUE);
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHC)
{
    const char * p_usage = "<dhcpc>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHC)
{
    const char * p_description = "Enable/Disable DHCP Client";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHCHN)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__USER_DHCP_HOSTNAME__)
    int at_resp_len = 0;
    int status      = 0;

    char * result_ptr   = NULL;
    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDHCHN=? */
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_SYSCFG,
                                        NVR_DHCPC_HOSTNAME,
                                        &result_ptr);
  #endif

        if (result_ptr && (strlen(result_ptr) > 0))
        {
            bsp_safe_strcpy(result_str, result_ptr, sizeof(result_str));
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_NO_RESULT;
        }
    }
    else if (argc == 2)
    {
        if (strlen(argv[1]) > DHCP_HOSTNAME_MAX_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPC_HOSTNAME_LEN;
            goto end;
        }

        /* AT+NWDHCHN=<hostname> */
        int str_len = strlen(argv[1]);

        if (str_len > 0)
        {
            char ch;

            /* Check DHCP hostname validity : 0 .. 9, a .. z, A .. Z, - */
            for (int i = 0; i < str_len; i++)
            {
                ch = argv[1][i];

                if ((ch == '-') ||
                    ((ch >= '0') && (ch <= '9')) ||
                    ((ch >= 'a') && (ch <= 'z')) ||
                    ((ch >= 'A') && (ch <= 'Z')))
                {
                    /* Okay,,, next character... */
                    continue;
                }
                else
                {
                    err = FSP_ERR_AT_CMD_ERR_NW_DHCPC_HOSTNAME_TYPE;
                    goto end;
                }
            }

            /* Save hostname to NVRAM */
  #ifdef RM_MAP_PERSISTANT_W
            status = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                      ENV_GROUP_SYSCFG,
                                                      NVR_DHCPC_HOSTNAME,
                                                      argv[1]);
  #endif
        }
        else
        {
            /* Delete hostname to NVRAM */
  #ifdef RM_MAP_PERSISTANT_W
            status = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME);
  #endif
        }

        if (status != FSP_SUCCESS)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPC_HOSTNAME_TYPE;
            goto end;
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

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHCHN)
{
    const char * p_usage = "<hostname>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHCHN)
{
    const char * p_description = "Save User DHCP client hostname";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHCHNDEL)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

 #if defined(__USER_DHCP_HOSTNAME__)
    int status = 0;

    if (argc == 1)
    {
        /* Remove saved DHCP Client hostname in NVRAM area. */

        /* Delete hostname to NVRAM */
  #ifdef RM_MAP_PERSISTANT_W
        status = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME);
  #endif

        if (status == -1)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_ERASE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

 #else
    err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
 #endif

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHCHNDEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHCHNDEL)
{
    const char * p_description = "Remove User DHCP client hostname";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHR)
{
    extern struct netif * netif_get_by_index(u8_t idx);
    extern int            ra6w1_network_main_get_sysmode(void);

    int tmp         = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    dhcps_lease_t iprange;

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDHR=? */
        char s_ip[16] = {0, }, e_ip[16] = {0, };
        iprange = dhcps_get_ip_range();
        sprintf(s_ip, ipaddr_ntoa((const ip_addr_t *) &iprange.start_ip));

        iprange = dhcps_get_ip_range();
        sprintf(e_ip, ipaddr_ntoa((const ip_addr_t *) &iprange.end_ip));

        sprintf(result_str, "%s,%s", s_ip, e_ip);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 3)
    {
        /* AT+NWDHR=<start_ip>,<end_ip> */
        ULONG start_ip, end_ip, ip_addr, net_mask, gw_addr = 0;
        extern struct netif * dhcps_netif;
        int startip_chk, endip_chk;
        ip_addr_t tmp_addr;

        ipaddr_aton(argv[1], &tmp_addr);
        start_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(argv[2], &tmp_addr);
        end_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, &tmp);
 #endif

        if (tmp == -1)
        {
            result_int = CC_VAL_DISABLE;
        }
        else
        {
            if (tmp == 1)
            {
                result_int = CC_VAL_ENABLE;
            }
            else
            {
                result_int = CC_VAL_DISABLE;
            }
        }

        if (result_int == 1 /* DHCPD is in running state */)
        {
            RM_ATCMD_W_CORE_NETWORK_ERROR("Info: DHCP is in running state\n");

            ip_addr  = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->ip_addr));
            net_mask = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->netmask));
            gw_addr  = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->gw));
        }
        else
        {
            /* DHCPD is in stopped state */
            struct netif * netif = netif_get_by_index(WLAN1_IFACE + 2); // assume Softap mode

            RM_ATCMD_W_CORE_NETWORK_ERROR("Info: DHCP is NOT in running state\n");

            ip_addr  = ip_addr_get_ip4_u32(&netif->ip_addr);
            net_mask = ip_addr_get_ip4_u32(&netif->netmask);
            gw_addr  = ip_addr_get_ip4_u32(&netif->gw);
        }

        startip_chk = is_in_valid_ip_class(argv[1]);
        endip_chk   = is_in_valid_ip_class(argv[2]);

        if ((startip_chk == pdTRUE) && (endip_chk == pdTRUE))
        {
            if (((ip_addr >> 8) != (start_ip >> 8)) || ((ip_addr >> 8) != (end_ip >> 8)))
            {
                RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: Failed to set range of IP_addr list.\n");
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
                goto end;
            }

            RM_ATCMD_W_CORE_NETWORK_ERROR("ip_addr=%lu, net_mask=%lu, gw_addr=%lu, start_ip=%lu, end_ip=%lu \n",
                                          ip_addr,
                                          net_mask,
                                          gw_addr,
                                          start_ip,
                                          end_ip);

            if (isvalidIPsubnetRange(start_ip, gw_addr, net_mask) == pdFALSE)
            {
                RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: Start IP_addr is out of range. \n");
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
                goto end;
            }

            if (isvalidIPsubnetRange(end_ip, gw_addr, net_mask) == pdFALSE)
            {
                RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: End IP_addr is out of range. \n");
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
                goto end;
            }

            if (isvalidIPrange(ip_addr, start_ip, end_ip))
            {
                RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: IP_addr is out of DHCP range. \n");
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
                goto end;
            }

            if (start_ip > end_ip)
            {
                RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: start_ip is bigger than end_ip. \n");
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
                goto end;
            }

            if ((end_ip - start_ip + 1) > DHCPS_MAX_LEASE)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_OVERFLOW;
                goto end;
            }

            if (!is_in_valid_ip_class(argv[1]))
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_START_IP_CLASS;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_SYSCFG,
                                             DHCP_SERVER_START_IP,
                                             argv[1]);
 #endif

            if (!is_in_valid_ip_class(argv[2]))
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_END_IP_CLASS;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_END_IP,
                                             argv[2]);
 #endif
        }
        else
        {
            if (startip_chk == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_START_IP_CLASS;
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_END_IP_CLASS;
            }
        }
    }
    else
    {
        if (argc == 2)
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

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHR)
{
    const char * p_usage = "<start_ip>,<end_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHR)
{
    const char * p_description = "Configure IP address range of DHCP Server";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHLT)
{
    extern int ra6w1_network_main_get_sysmode(void);

    int tmp         = 0;
    int result_int  = 0;
    int tmp_int1    = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDHLT=? */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_LEASE_TIME, &tmp);
 #endif

        if (tmp == -1)
        {
            result_int = 10 * 50;      /* NX_DHCP_SLOW_PERIODIC_TIME_INTERVAL == 50 */
        }
        else
        {
            result_int = tmp;
        }

        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWDHLT=<time> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, MIN_DHCP_SERVER_LEASE_TIME,
                                                          MAX_DHCP_SERVER_LEASE_TIME) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if (tmp_int1 <= 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_LEASE_TIME,
                                      tmp_int1);
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHLT)
{
    const char * p_usage = "<lease_time>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHLT)
{
    const char * p_description = "Lease time of DHCP Server";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHS)
{
    extern struct netif * netif_get_by_index(u8_t idx);
    extern int            ra6w1_network_main_get_sysmode(void);

    int tmp = 0;
    int tmp_int1, tmp_int2 = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[128]   = {0, };

    dhcps_lease_t iprange;
    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWDHS=? */
        char dhcps_use_val[4] = {0, };

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, &tmp);
 #endif

        if (tmp == -1)
        {
            result_int = CC_VAL_DISABLE;
        }
        else
        {
            if (tmp == 1)
            {
                result_int = CC_VAL_ENABLE;
            }
            else
            {
                result_int = CC_VAL_DISABLE;
            }
        }

        sprintf(dhcps_use_val, "%d", result_int);

        if (result_int == 1)
        {
            char s_ip[16] = {0, }, e_ip[16] = {0, };
 #ifdef __SUPPORT_MESH__
            char tmp_ip[16] = {0, };
 #endif                                // __SUPPORT_MESH__

            iprange = dhcps_get_ip_range();
            sprintf(s_ip, ipaddr_ntoa((const ip_addr_t *) &iprange.start_ip));

            iprange = dhcps_get_ip_range();
            sprintf(e_ip, ipaddr_ntoa((const ip_addr_t *) &iprange.end_ip));

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_LEASE_TIME,
                                         &tmp);
 #endif

            if (tmp == -1)
            {
                result_int = 10 * 50;  /* NX_DHCP_SLOW_PERIODIC_TIME_INTERVAL == 50 */
            }
            else
            {
                result_int = tmp;
            }

            sprintf(result_str, "%s,%s,%s,%d", dhcps_use_val, s_ip, e_ip, result_int);

 #ifdef __SUPPORT_MESH__
            ULONG utmp;

            get_dns_information(&utmp, 1);
            sprintf(tmp_ip, "%lu.%lu.%lu.%lu", (utmp >> 24) & 0x0ff, (utmp >> 16) & 0x0ff, (utmp >> 8) & 0x0ff,
                    (utmp >> 0) & 0x0ff);
            sprintf(result_str, "%s,%s", result_str, tmp_ip);
 #endif                                // __SUPPORT_MESH__
        }

        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWDHS=<dhcpd> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        {
 #if (LWIP_DHCPS && LWIP_IPV4)
            dhcps_cmd_param * param = NULL;

            param = pvPortMalloc(sizeof(dhcps_cmd_param));
            memset(param, 0, sizeof(dhcps_cmd_param));

            if (tmp_int1 == CC_VAL_ENABLE)
            {
                param->cmd = DHCP_SERVER_STATE_STOP;
                dhcps_run(param);

                vTaskDelay(portCONVERT_MS_2_TICKS(100 * 5));

                param = pvPortMalloc(sizeof(dhcps_cmd_param));
                memset(param, 0, sizeof(dhcps_cmd_param));

                param->cmd             = DHCP_SERVER_STATE_START;
                param->dhcps_interface = WLAN1_IFACE;
                dhcps_run(param);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, pdTRUE);
  #endif
            }
            else
            {
                param->cmd = DHCP_SERVER_STATE_STOP;
                dhcps_run(param);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD);
  #endif
            }
 #endif                                /*LWIP_DHCPS*/
        }
    }
    else if (argc >= 4)
    {
        /* AT+NWDHS=<dhcpd(=1)>,<start_ip>,<end_ip>(,<lease_time>) */
        ULONG start_ip, end_ip, ip_addr, net_mask, gw_addr = 0;
        extern struct netif * dhcps_netif;
        int startip_chk, endip_chk;
        struct netif * netif;
        ip_addr_t tmp_addr;

        /* verify <start_ip>, <end_ip> : argv[2], argv[3] */
        ipaddr_aton(argv[2], &tmp_addr);
        start_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        ipaddr_aton(argv[3], &tmp_addr);
        end_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
        memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

        if (tmp == -1)
        {
            result_int = CC_VAL_DISABLE;
        }
        else
        {
            if (tmp == 1)
            {
                result_int = CC_VAL_ENABLE;
            }
            else
            {
                result_int = CC_VAL_DISABLE;
            }
        }

        if (result_int == 1 /* DHCPD is in running state */)
        {
            RM_ATCMD_W_CORE_NETWORK_ERROR("Info: DHCPD is in running state\n");

            ip_addr  = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->ip_addr));
            net_mask = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->netmask));
            gw_addr  = lwip_htonl(ip_addr_get_ip4_u32(&dhcps_netif->gw));
        }
        else
        {
            /* DHCPD is in stopped state */
            netif = netif_get_by_index(WLAN1_IFACE + 2); // assume Softap mode

            RM_ATCMD_W_CORE_NETWORK_ERROR("Info: DHCPD is NOT in running state\n");

            ip_addr  = lwip_htonl(ip_addr_get_ip4_u32(&netif->ip_addr));
            net_mask = lwip_htonl(ip_addr_get_ip4_u32(&netif->netmask));
            gw_addr  = lwip_htonl(ip_addr_get_ip4_u32(&netif->gw));
        }

        startip_chk = is_in_valid_ip_class(argv[2]);
        endip_chk   = is_in_valid_ip_class(argv[3]);

        if (startip_chk == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_START_IP_CLASS;
            goto end;
        }
        else if (endip_chk == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_WRONG_END_IP_CLASS;
            goto end;
        }

        if (((ip_addr >> 8) != (start_ip >> 8)) || ((ip_addr >> 8) != (end_ip >> 8)))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
            goto end;
        }

        if (isvalidIPsubnetRange(start_ip, gw_addr, net_mask) == pdFALSE)
        {
            RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: Start IP_addr is out of range. \n");
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
            goto end;
        }

        /* AT+NWDHS=<dhcpd(=1)>,<start_ip>,<end_ip>(,<lease_time>) */
        if (isvalidIPsubnetRange(end_ip, gw_addr, net_mask) == pdFALSE)
        {
            RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: End IP_addr is out of range. \n");
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
            goto end;
        }

        if (start_ip > end_ip)
        {
            RM_ATCMD_W_CORE_NETWORK_ERROR("ERR: start_ip is bigger than end_ip. \n");
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_MISMATCH;
            goto end;
        }

        if ((end_ip - start_ip + 1) > DHCPS_MAX_LEASE)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_IPADDR_RANGE_OVERFLOW;
            goto end;
        }

        /* verify <dhcpd(=1)> : argv[1] */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_RUN_FLAG_TYPE;
            goto end;
        }

        if (tmp_int1 != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_RUN_FLAG_VAL;
            goto end;
        }

        if (argc == 5)
        {
            /* verify and set <lease_time> : argv[4] */
            if (rm_atcmd_w_core_common_stoi(argv[4], &tmp_int2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_LEASE_TIME_TYPE;
                goto end;
            }
            else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int2, MIN_DHCP_SERVER_LEASE_TIME,
                                                              MAX_DHCP_SERVER_LEASE_TIME) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_LEASE_TIME_RANGE;
                goto end;
            }

            if (tmp_int2 <= 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_LEASE_TIME_RANGE;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          DHCP_SERVER_LEASE_TIME,
                                          tmp_int2);
 #endif

 #ifdef __SUPPORT_MESH__
        }
        else if ((argc == 5) && is_in_valid_ip_class(argv[4]))
        {
            if (!is_in_valid_ip_class(argv[4]))
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_DNS_SVR_ADDR_CLASS;
                goto end;
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS,
                                             argv[4]);
  #endif
        }
        else if (argc == 6)
        {
            if (rm_atcmd_w_core_common_stoi(argv[4], &tmp_int2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_DNS_SVR_ADDR_CLASS;
                goto end;
            }

            if (tmp_int2 <= 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_LEASE_TIME_RANGE;
                goto end;
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          DHCP_SERVER_LEASE_TIME,
                                          tmp_int2);
  #endif

            if (!is_in_valid_ip_class(argv[5]))
            {
                err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_DNS_SVR_ADDR_CLASS;
                goto end;
            }

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS,
                                             argv[5]);
  #endif
 #endif                                // __SUPPORT_MESH__
        }

        /* Set <start_ip>, <end_ip>, <dhcpd=1> */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_START_IP,
                                         argv[2]);
 #endif

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_END_IP, argv[3]);
 #endif

 #if (LWIP_DHCPS && LWIP_IPV4)
        dhcps_cmd_param * param = NULL;

        param = pvPortMalloc(sizeof(dhcps_cmd_param));
        memset(param, 0, sizeof(dhcps_cmd_param));

        if (tmp_int1 == CC_VAL_ENABLE)
        {
            param->cmd = DHCP_SERVER_STATE_STOP;
            dhcps_run(param);

            vTaskDelay(100 * 5);

            param = pvPortMalloc(sizeof(dhcps_cmd_param));
            memset(param, 0, sizeof(dhcps_cmd_param));

            param->cmd             = DHCP_SERVER_STATE_START;
            param->dhcps_interface = WLAN1_IFACE;
            dhcps_run(param);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD, pdTRUE);
  #endif
        }
        else
        {
            param->cmd = DHCP_SERVER_STATE_STOP;
            dhcps_run(param);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_KEY_DHCPD);
  #endif
        }
 #endif                                /*LWIP_DHCPS*/
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHS)
{
    const char * p_usage = "<dhcpd>(,<start_ip>,<end_ip>,<lease_time>)";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHS)
{
    const char * p_description = "Enable/Disable DHCP Server";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DHIP)
{
    extern int ra6w1_network_main_get_sysmode(void);

    int at_resp_len = 0;

    char tmp[18]         = {0, };
    char at_resp[210]    = {0, };
    UCHAR count_assigned = 0;
    char * dyn_mem       = NULL;

    list_node * p                         = NULL;
    list_node * p_list                    = NULL;
    struct dhcps_pool * pdhcps_pool       = NULL;
    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    const int sys_mode = ra6w1_network_main_get_sysmode();

    if ((sys_mode != WIFI_DEVICE_MODE_EXT_AP) && (sys_mode != WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    dyn_mem = (char *) pvPortMalloc(34 * 6); // max up to ~204 bytes (1 sta = 34 x 6)

    if (dyn_mem == NULL)
    {
        err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        goto end;
    }

    memset(dyn_mem, 0x00, 34 * 6);

    p_list = (list_node *) dhcps_option_info(CLIENT_POOL, 1);

    if (p_list != NULL)
    {
        p = p_list;

        while (p != NULL)
        {
            int is_valid = 0;
            memset(tmp, 0x00, 18);
            pdhcps_pool = p->pnode;

            sprintf(tmp,
                    "%02x:%02x:%02x:%02x:%02x:%02x",
                    pdhcps_pool->mac[0],
                    pdhcps_pool->mac[1],
                    pdhcps_pool->mac[2],
                    pdhcps_pool->mac[3],
                    pdhcps_pool->mac[4],
                    pdhcps_pool->mac[5]);

            for (UCHAR iii = 0; iii < 6; iii++)
            {
                if (strlen(atcmd_mac_table[iii]) == 0)
                {
                    continue;
                }
                else if (strcmp(tmp, atcmd_mac_table[iii]) == 0)
                {
                    is_valid = 1;
                    break;
                }
            }

            if (!is_valid)
            {
                p = p->pnext;
                continue;
            }

            if (count_assigned != 0)
            {
                strcat(dyn_mem, ";");
            }

            strcat(dyn_mem, tmp);
            strcat(dyn_mem, ",");
            strcat(dyn_mem, ipaddr_ntoa((ip_addr_t *) &pdhcps_pool->ip));

            p = p->pnext;
            count_assigned++;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_NO_CONNECTED_CLIENT;
        goto end;
    }

    if (!count_assigned)
    {
        err = FSP_ERR_AT_CMD_ERR_NW_DHCPS_NO_CONNECTED_CLIENT;
        goto end;
    }

end:

    if (dyn_mem)
    {
        if ((err == FSP_ERR_AT_CMD_ERR_CMD_OK) && (strlen(dyn_mem) > 0))
        {
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), dyn_mem);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }

        vTaskDelay(portCONVERT_MS_2_TICKS(10));
        vPortFree(dyn_mem);
        dyn_mem = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DHIP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DHIP)
{
    const char * p_description = "Read Clients IP info";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS)
{
    extern void get_sntp_server(char * svraddr, unsigned int index);

    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNS=? */
        get_sntp_server(result_str, 0);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWSNS=<server_ip> */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN,
                                         argv[1]);
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS)
{
    const char * p_usage = "<server_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS)
{
    const char * p_description = "Configure IP address for Time Server";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS1)
{
    extern void get_sntp_server(char * svraddr, unsigned int index);

    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNS1=? */
        get_sntp_server(result_str, 1);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWSNS1=<server_ip> */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_1,
                                         argv[1]);
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS1)
{
    const char * p_usage = "<server_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS1)
{
    const char * p_description = "Configure IP address for Time Server1";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNS2)
{
    extern void get_sntp_server(char * svraddr, unsigned int index);

    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNS1=? */
        get_sntp_server(result_str, 2);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWSNS1=<server_ip> */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_2,
                                         argv[1]);
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNS2)
{
    const char * p_usage = "<server_ip>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNS2)
{
    const char * p_description = "Configure IP address for Time Server2";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNUP)
{
    extern int sntp_get_period(void);

    int tmp_int1    = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNUP=? */
        result_int = sntp_get_period();
        sprintf(result_str, "%d", result_int);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }
    else if (argc == 2)
    {
        /* AT+NWSNUP=<period> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }
        else if (rm_atcmd_w_core_common_is_in_valid_range(tmp_int1, 60, 129600) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if (tmp_int1 <= 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_SYSCFG,
                                      NVR_KEY_SNTP_SYNC_PERIOD,
                                      tmp_int1);
 #endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNUP)
{
    const char * p_usage = "<period>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNUP)
{
    const char * p_description = "Configure update period of SNTP Client";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP)
{
    extern int          sntp_get_period(void);
    extern unsigned int get_sntp_use(void);
    extern unsigned int set_sntp_use(int use);
    extern u8_t         sntp_get_use(void);
    extern void         get_sntp_server(char * svraddr, unsigned int index);

    int tmp_int1, tmp_int2 = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        char sntp_use_val[4] = {0, };

        /* AT+NWSNTP=? */
        result_int = sntp_get_use();
        sprintf(sntp_use_val, "%d", result_int);

        if (result_int == 1)
        {
            char tmp_ip[16] = {0, };

            get_sntp_server(tmp_ip, 0);
            result_int = sntp_get_period();
            sprintf(result_str, "%s,%s,%d", sntp_use_val, tmp_ip, result_int);
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else if (argc == 2)
    {
        /* AT+NWSNTP=<sntp> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            goto end;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }
    else if (argc > 2)
    {
        /* AT+NWSNTP=<sntp(=1)>,<server_ip>(,<period>) */
        if (atoi(argv[1]) != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN,
                                         argv[2]);
 #endif

        if (argc == 4)
        {
            if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_TYPE;
                goto end;
            }

            if (tmp_int2 <= 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_RANGE;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          NVR_KEY_SNTP_SYNC_PERIOD,
                                          tmp_int2);
 #endif
        }

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            return err;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP)
{
    const char * p_usage = "<sntp>[,<server_ip>,<period>]";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP)
{
    const char * p_description = "Enable/Disable SNTP Client service";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP1)
{
    extern int          sntp_get_period(void);
    extern unsigned int get_sntp_use(void);
    extern unsigned int set_sntp_use(int use);
    extern u8_t         sntp_get_use(void);
    extern void         get_sntp_server(char * svraddr, unsigned int index);

    int tmp_int1, tmp_int2 = 0;
    int result_int  = 0;
    int at_resp_len = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNTP1=? */
        char sntp_use_val[4] = {0, };

        result_int = sntp_get_use();
        sprintf(sntp_use_val, "%d", result_int);

        if (result_int == 1)
        {
            char tmp_ip[16] = {0, };

            get_sntp_server(tmp_ip, 1);
            result_int = sntp_get_period();
            sprintf(result_str, "%s,%s,%d", sntp_use_val, tmp_ip, result_int);
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else if (argc == 2)
    {
        /* AT+NWSNTP1=<sntp> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            return err;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }
    else if (argc > 2)
    {
        /* AT+NWSNTP1=<sntp(=1)>,<server_ip>(,<period>) */
        if (atoi(argv[1]) != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_1,
                                         argv[2]);
 #endif

        if (argc == 4)
        {
            if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_TYPE;
                goto end;
            }

            if (tmp_int2 <= 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_RANGE;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          NVR_KEY_SNTP_SYNC_PERIOD,
                                          tmp_int2);
 #endif
        }

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            return err;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP1)
{
    const char * p_usage = "<sntp>(,<server_ip>,<period>)";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP1)
{
    const char * p_description = "Enable/Disable SNTP Client service";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(SNTP2)
{
    extern int          sntp_get_period(void);
    extern unsigned int get_sntp_use(void);
    extern unsigned int set_sntp_use(int use);
    extern u8_t         sntp_get_use(void);
    extern void         get_sntp_server(char * svraddr, unsigned int index);

    int at_resp_len = 0;
    int tmp_int1, tmp_int2 = 0;
    int result_int = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if ((argc == 1) || rm_atcmd_w_core_network_is_query_arg(argc, argv[1]))
    {
        /* AT+NWSNTP2=? */
        char sntp_use_val[4] = {0, };

        result_int = sntp_get_use();
        sprintf(sntp_use_val, "%d", result_int);

        if (result_int == 1)
        {
            char tmp_ip[16] = {0, };
            get_sntp_server(tmp_ip, 2);
            result_int = sntp_get_period();
            sprintf(result_str, "%s,%s,%d", sntp_use_val, tmp_ip, result_int);
            at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else if (argc == 2)
    {
        /* AT+NWSNTP2=<sntp> */
        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            goto end;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }
    else if (argc > 2)
    {
        /* AT+NWSNTP2=<sntp(=1)>,<server_ip>(,<period>) */
        if (atoi(argv[1]) != 1)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_SYSCFG,
                                         NVR_KEY_SNTP_SERVER_DOMAIN_2,
                                         argv[2]);
 #endif

        if (argc == 4)
        {
            if (rm_atcmd_w_core_common_stoi(argv[3], &tmp_int2, POL_1) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_TYPE;
                goto end;
            }

            if (tmp_int2 <= 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_SNTP_PERIOD_RANGE;
                goto end;
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                          ENV_GROUP_SYSCFG,
                                          NVR_KEY_SNTP_SYNC_PERIOD,
                                          tmp_int2);
 #endif
        }

        if (rm_atcmd_w_core_common_stoi(argv[1], &tmp_int1, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_TYPE;
            goto end;
        }

        if ((tmp_int1 != CC_VAL_ENABLE) && (tmp_int1 != CC_VAL_DISABLE))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_SNTP_FLAG_VAL;
            goto end;
        }

        if ((tmp_int1 == CC_VAL_ENABLE) && (get_sntp_use() == 1))
        {
            goto end;
        }

        /* Set run flag and start */
        set_sntp_use(tmp_int1);
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(SNTP2)
{
    const char * p_usage = "<sntp>[,<server_ip>,<period>]";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(SNTP2)
{
    const char * p_description = "Enable/Disable SNTP Client service";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(CCRT)
{
    int module      = 0;
    int at_resp_len = 0;
    UINT16 status   = 0;

    char result_str[64] = {0, };
    char at_resp[210]   = {0, };

    fsp_err_atcmd_err_code err            = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    int module_reserved_status[] =
    {
        CERT_MQTTS_CLI_USED,
        CERT_HTTPS_CLI_USED,
        CERT_WPA_ENT_USED,
        CERT_OTA_USED,
        CERT_HTTPS_SVR_USED,
        CERT_ATCMD_USED,
        CERT_AWS_USED,
        CERT_MATTER_USED,
        CERT_MISC1_USED,
        CERT_MISC2_USED,
        CERT_MISC3_USED,
        CERT_MISC4_USED,
        CERT_MISC5_USED,
        CERT_MISC6_USED,
        CERT_MISC7_USED,
        CERT_MISC8_USED
    };

    if (argc != 2)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (rm_atcmd_w_core_common_stoi(argv[1], &module, POL_1) != 0)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
    }

    if (rm_atcmd_w_core_common_is_in_valid_range(module, 0, 15) == pdFALSE)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
    }

    if (module_reserved_status[module] == 1)
    {
        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_CA_CERT))
        {
            status |= BIT(0);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_CERT))
        {
            status |= BIT(1);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_PRIVATE_KEY))
        {
            status |= BIT(2);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_DH_PARAMS))
        {
            status |= BIT(3);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_INITIAL_CERT))
        {
            status |= BIT(4);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_INITIAL_PRIV_KEY))
        {
            status |= BIT(5);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_UNIQUE_CERT))
        {
            status |= BIT(6);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_UNIQUE_PRIV_KEY))
        {
            status |= BIT(7);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_EXCHANGE))
        {
            status |= BIT(8);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_CD))
        {
            status |= BIT(9);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_DAC_CERT))
        {
            status |= BIT(10);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_PAI_CERT))
        {
            status |= BIT(11);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_DAC_PRIV_KEY))
        {
            status |= BIT(12);
        }

        if (RM_CERT_IsExistCert(module, RM_CERT_TYPE_DAC_PUB_KEY))
        {
            status |= BIT(13);
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NW_MODULE_NOT_RESERVED;
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        sprintf(result_str, "%d,%d", module, status);
        at_resp_len = sprintf(at_resp, "\r\n%s:%s", rm_atcmd_w_core_common_strupr(argv[0] + 2), result_str);
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CCRT)
{
    const char * p_usage = "<module>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CCRT)
{
    const char * p_description = "Check if Certificates exist for the module specified";

    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(CCRTR)
{
    int at_resp_len = 0;
    uint8_t * buf = NULL;
    size_t len = RM_CERT_MAX_LENGTH;
    int module = 0;
    int type = 0;
    rm_cert_format_t format = 0;
    char  at_resp[30] = {0, };

    rm_cert_err_t read_err = RM_CERT_ERR_OK;
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    int module_reserved_status[] =
    {
        CERT_MQTTS_CLI_USED,
        CERT_HTTPS_CLI_USED,
        CERT_WPA_ENT_USED,
        CERT_OTA_USED,
        CERT_HTTPS_SVR_USED,
        CERT_ATCMD_USED,
        CERT_AWS_USED,
        CERT_MATTER_USED,
        CERT_MISC1_USED,
        CERT_MISC2_USED,
        CERT_MISC3_USED,
        CERT_MISC4_USED,
        CERT_MISC5_USED,
        CERT_MISC6_USED,
        CERT_MISC7_USED,
        CERT_MISC8_USED
    };

    if (argc != 3)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (rm_atcmd_w_core_common_stoi(argv[1], &module, POL_1) != 0)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
    }

    if (rm_atcmd_w_core_common_is_in_valid_range(module, 0, 15) == pdFALSE)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
    }

    if (rm_atcmd_w_core_common_stoi(argv[2], &type, POL_1) != 0)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
    }

    if (rm_atcmd_w_core_common_is_in_valid_range(type, 0, 13) == pdFALSE)
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
    }

    if (module_reserved_status[module] == 1)
    {
        buf = pvPortMalloc(RM_CERT_MAX_LENGTH);
        if (!buf)
        {
            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        } 

        read_err = RM_CERT_Read(module, type, &format, buf, &len);
        if (read_err == RM_CERT_ERR_OK)
        {
            at_resp_len = sprintf(at_resp, "\r\n%s:%d,%d,%d,%d,", rm_atcmd_w_core_common_strupr(argv[0] + 2), module, type, format, len);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) buf, len);
        }
        else if (read_err == RM_CERT_ERR_INVALID_MODULE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_MODULE;
        }
        else if (read_err == RM_CERT_ERR_INVALID_TYPE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
        }
        else if (read_err == RM_CERT_ERR_INVALID_FORMAT)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FORMAT;
        }
        else if (read_err == RM_CERT_ERR_INVALID_LENGTH)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_LENGTH;
        }
        else if (read_err == RM_CERT_ERR_INVALID_FLASH_ADDR)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FLASH_ADDR;
        }
        else if (read_err == RM_CERT_ERR_INVALID_PARAMS)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_ERR_INVALID_PARAMS;
        }
        else if (read_err == RM_CERT_ERR_FOPEN_FAILED)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_ERR_FOPEN_FAILED;
        }
        else if (read_err == RM_CERT_ERR_MEM_FAILED)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_ERR_MEM_FAILED;
        }
        else if (read_err == RM_CERT_ERR_EMPTY_CERTIFICATE)
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_EMPTY_CERT;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_SSL_CERT_ERR_NOK;
        }

        vPortFree(buf);
        buf = NULL;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NW_MODULE_NOT_RESERVED;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CCRTR)
{
    const char * p_usage = "<module>,<type>";
    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CCRTR)
{
    const char * p_description = "Reads the certificate for the specified module";
    return p_description;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_CB(DCRT)
{
    int module = 0;
    int i, j = 0;

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int module_reserved_status[] =
    {
        CERT_MQTTS_CLI_USED,
        CERT_HTTPS_CLI_USED,
        CERT_WPA_ENT_USED,
        CERT_OTA_USED,
        CERT_HTTPS_SVR_USED,
        CERT_ATCMD_USED,
        CERT_AWS_USED,
        CERT_MATTER_USED,
        CERT_MISC1_USED,
        CERT_MISC2_USED,
        CERT_MISC3_USED,
        CERT_MISC4_USED,
        CERT_MISC5_USED,
        CERT_MISC6_USED,
        CERT_MISC7_USED,
        CERT_MISC8_USED
    };

    if (argc != 2)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (strcasecmp(argv[1], "ALL") == 0)
    {
        for (i = 0; i < RM_CERT_MODULE_MISC8 + 1; i++)
        {
            if (module_reserved_status[i] == 0)
            {
                continue;
            }

            for (j = 0; j < RM_CERT_TYPE_DAC_PUB_KEY + 1; j++)
            {
                RM_CERT_Delete(i, j);
            }
        }
    }
    else
    {
        if (rm_atcmd_w_core_common_stoi(argv[1], &module, POL_1) != 0)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
            goto end;
        }

        if (rm_atcmd_w_core_common_is_in_valid_range(module, 0, 15) == pdFALSE)
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_ARG_RANGE;
            goto end;
        }

        if (module_reserved_status[module] == 0)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_MODULE_NOT_RESERVED;
            goto end;
        }

        for (i = 0; i < RM_CERT_TYPE_DAC_PUB_KEY + 1; i++)
        {
            RM_CERT_Delete(module, i);
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(DCRT)
{
    const char * p_usage = "<module>";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(DCRT)
{
    const char * p_description = "Delete Certificates of the module specified";

    return p_description;
}

 #if (ATCMD_SECURE_CHANNEL == 1)
uint32_t RM_ATCMD_W_CORE_NETWORK_CERT_fixed_cmd_cb (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    typedef enum
    {
        READ_CERT_MODULE,              // 0
        READ_CERT_TYPE,                // 1
        READ_CERT_MODE,                // 2
        READ_CERT_FORMAT,              // 3
        READ_CERT_LENGTH,              // 4
        READ_CERT_DATA                 // 5
    } atcmd_esc_cert_cmd_parameter_step;

    char param_atcmd[40] = {0x00, };
    int param_atcmd_idx  = 0;

    int done = false;
    char ch  = 0;
    atcmd_esc_cert_cmd_parameter_step param_step = READ_CERT_MODULE;

    int cert_err = RM_CERT_ERR_OK;
    int module   = RM_CERT_MODULE_NONE;
    int type     = RM_CERT_TYPE_NONE;
    int mode     = RM_CERT_MODE_NONE;
    int format   = RM_CERT_FORMAT_NONE;
    int cert_len = 0;
    int cert_idx = 0;
    int arg_idx  = 0;

    unsigned char * p_cert = NULL;

    char resp_str[32] = {0x00, };

    /* AT+NWCERT=<module>,<certificate type>,<mode>[,<format>,<length>,<content>] */

    /* Input comma(,) */
    ch = argv[1][arg_idx++];
    printf("at+nwcert\r\n");
    printf("argc %d, argv %s\r\n", argc, argv[1]);

    return err;

    while (err == FSP_ERR_AT_CMD_ERR_CMD_OK && !done)
    {
        switch (param_step)
        {
            case READ_CERT_MODULE:
            case READ_CERT_TYPE:
            case READ_CERT_MODE:
            case READ_CERT_FORMAT:
            case READ_CERT_LENGTH:
            {
                memset(param_atcmd, 0x00, sizeof(param_atcmd));
                param_atcmd_idx = 0;
                ch              = 0x00;

                while (ch != 0x2C)
                {
                    if (param_atcmd_idx >= (int) sizeof(param_atcmd))
                    {
                        err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                        break;
                    }

                    ch = argv[1][arg_idx++];

                    param_atcmd[param_atcmd_idx++] = ch;

                    if ((param_step == READ_CERT_MODE) && (param_atcmd_idx == 1))
                    {
                        param_atcmd[param_atcmd_idx] = '\0';

                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &mode, POL_1) != 0)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }

                        if (mode == RM_CERT_MODE_DELETION)
                        {
                            done = true;
                            break;
                        }
                    }
                }

                /* Update param step */
                if (param_step == READ_CERT_MODULE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &module, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_TYPE;
                }
                else if (param_step == READ_CERT_TYPE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &type, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_MODE;
                }
                else if (param_step == READ_CERT_MODE)
                {
                    if ((mode == RM_CERT_MODE_DELETION) && done)
                    {
                        break;
                    }

                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &mode, POL_1) != 0)
                    {
                        RM_ATCMD_W_CORE_NETWORK_ERROR("Failed to get mode\n");
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_FORMAT;
                }
                else if (param_step == READ_CERT_FORMAT)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &format, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_LENGTH;
                }
                else if (param_step == READ_CERT_LENGTH)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &cert_len, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

                    param_step = READ_CERT_DATA;
                }

                break;
            }

            case READ_CERT_DATA:
            {
                p_cert = pvPortMalloc(RM_CERT_MAX_LENGTH);

                if (p_cert == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                    break;
                }

                memset(p_cert, 0x00, RM_CERT_MAX_LENGTH);

                for (int idx = 0; idx < cert_len; idx++)
                {
                    ch = argv[1][arg_idx++];
                    if ((format == RM_CERT_FORMAT_PEM) && (ch == 0x0D))
                    {
                        ch = 0x0A;
                    }

                    p_cert[cert_idx] = ch;
                    cert_idx++;
                }

                if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
                {
                    break;
                }

                done = true;
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        if (mode == RM_CERT_MODE_STORE)
        {
            /* Store certificate */
            cert_err = RM_CERT_Write(module, type, format, (unsigned char *) p_cert, (size_t)cert_idx);
        }
        else if (mode == RM_CERT_MODE_DELETION)
        {
            /* Delete certificate */
            cert_err = RM_CERT_Delete(module, type);
        }

        /* Convert error code */
        switch (cert_err)
        {
            case RM_CERT_ERR_OK:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RM_CERT_ERR_INVALID_MODULE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_MODULE;
                break;
            }

            case RM_CERT_ERR_INVALID_TYPE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
                break;
            }

            case RM_CERT_ERR_INVALID_FORMAT:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FORMAT;
                break;
            }

            case RM_CERT_ERR_INVALID_LENGTH:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_LENGTH;
                break;
            }

            case RM_CERT_ERR_INVALID_FLASH_ADDR:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FLASH_ADDR;
                break;
            }

            case RM_CERT_ERR_INVALID_PARAMS:
            {
                err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                break;
            }

            case RM_CERT_ERR_FOPEN_FAILED:
            {
                err = FSP_ERR_AT_CMD_ERR_SFLASH_ACCESS;
                break;
            }

            case RM_CERT_ERR_MEM_FAILED:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            case RM_CERT_ERR_EMPTY_CERTIFICATE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_EMPTY_CERT;
                break;
            }

            case RM_CERT_ERR_NOK:
            default:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_INTERNAL;
                break;
            }
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        ATCMD_ESC_OK_STR(resp_str);
    }
    else
    {
        ATCMD_ESC_ERROR_STR(resp_str, err);
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

    if (p_cert)
    {
        vPortFree(p_cert);
        p_cert = NULL;
    }

    return err;
}

 #endif

 #if (ATCMD_SECURE_CHANNEL == 1)
static uint32_t cert_data_decrypted (atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_cert, uint32_t * p_cert_len)
{
    uint8_t iv_local[AES_IV_SIZE_AT];

    if ((p_at_ctrl == NULL) || (p_cert == NULL) || (p_cert_len == NULL))
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    size_t size = (size_t) (*p_cert_len);

    // AES-CBC requires block-size multiple (16 bytes)
    if ((size == 0) || ((size % 16u) != 0u))
    {
        return FSP_ERR_AT_CMD_ERR_SECURE_RESYNC_REQ;
    }

    memcpy(iv_local, p_ctrl->rx_iv, AES_IV_SIZE_AT);

    int ret =
        mbedtls_aes_crypt_cbc(p_ctrl->ctx_rx,
                                    MBEDTLS_AES_DECRYPT,
                                    size,
                                    iv_local,
                                    (const uint8_t *) p_cert,
                                    (uint8_t *) p_cert);

    if (ret != 0)
    {
        return FSP_ERR_AT_CMD_ERR_SECURE_RESYNC_REQ;
    }

    int new_len = rm_atcmd_secchan_unpad((char *) p_cert, (int) size);
    if ((new_len <= 0) || ((size_t) new_len > size))
    {
        return FSP_ERR_AT_CMD_ERR_SECURE_RESYNC_REQ;
    }

    memcpy(p_ctrl->rx_iv, iv_local, AES_IV_SIZE_AT);

    *p_cert_len = (uint32_t) new_len;

    return FSP_SUCCESS;
}

 #endif

RM_ATCMD_W_CORE_NETWORK_UNFIXED_ATCMD_CB(CERT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t fsp_err          = FSP_SUCCESS;

    typedef enum
    {
        READ_CERT_MODULE,              // 0
        READ_CERT_TYPE,                // 1
        READ_CERT_MODE,                // 2
        READ_CERT_FORMAT,              // 3
        READ_CERT_LENGTH,              // 4
        READ_CERT_DATA                 // 5
    } atcmd_esc_cert_cmd_parameter_step;

    char param_atcmd[40] = {0x00, };
    int param_atcmd_idx  = 0;

    int done = false;
    char ch  = 0;
    atcmd_esc_cert_cmd_parameter_step param_step = READ_CERT_MODULE;

    int cert_err           = RM_CERT_ERR_OK;
    int module             = RM_CERT_MODULE_NONE;
    int type               = RM_CERT_TYPE_NONE;
    int mode               = RM_CERT_MODE_NONE;
    int format             = RM_CERT_FORMAT_NONE;
    int cert_len_signed    = 0;
    uint32_t cert_len      = 0;
    int cert_idx           = 0;
    unsigned char * p_cert = NULL;
    char resp_str[32]      = {0x00, };
 #if (ATCMD_SECURE_CHANNEL == 1)
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
 #endif

    /* <ESC>CERT,<module>,<certificate type>,<mode>[,<format>,<length>,<content>] */

    /* Input comma(,) */
    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

    if (ch != 0x2C)
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        goto end;
    }

    while (err == FSP_ERR_AT_CMD_ERR_CMD_OK && !done)
    {
        switch (param_step)
        {
            case READ_CERT_MODULE:
            case READ_CERT_TYPE:
            case READ_CERT_MODE:
            case READ_CERT_FORMAT:
            case READ_CERT_LENGTH:
            {
                memset(param_atcmd, 0x00, sizeof(param_atcmd));
                param_atcmd_idx = 0;
                ch              = 0x00;

                while (ch != 0x2C)
                {
                    if (param_atcmd_idx >= (int) sizeof(param_atcmd))
                    {
                        err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                        break;
                    }

                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_atcmd[param_atcmd_idx++] = ch;

                    if ((param_step == READ_CERT_MODE) && (param_atcmd_idx == 1))
                    {
                        param_atcmd[param_atcmd_idx] = '\0';

                        if (rm_atcmd_w_core_common_stoi(param_atcmd, &mode, POL_1) != 0)
                        {
                            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                            break;
                        }

                        if (mode == RM_CERT_MODE_DELETION)
                        {
                            done = true;
                            break;
                        }
                    }
                }

                /* Update param step */
                if (param_step == READ_CERT_MODULE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &module, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_TYPE;
                }
                else if (param_step == READ_CERT_TYPE)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &type, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_MODE;
                }
                else if (param_step == READ_CERT_MODE)
                {
                    if ((mode == RM_CERT_MODE_DELETION) && done)
                    {
                        break;
                    }

                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &mode, POL_1) != 0)
                    {
                        RM_ATCMD_W_CORE_NETWORK_ERROR("Failed to get mode\n");
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_FORMAT;
                }
                else if (param_step == READ_CERT_FORMAT)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &format, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_step = READ_CERT_LENGTH;
                }
                else if (param_step == READ_CERT_LENGTH)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &cert_len_signed, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

                    if (cert_len_signed <= 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        break;
                    }

                    cert_len   = (uint32_t) cert_len_signed;
                    param_step = READ_CERT_DATA;
                }

                break;
            }

            case READ_CERT_DATA:
            {
                p_cert = pvPortMalloc(RM_CERT_MAX_LENGTH);

                if (p_cert == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                    break;
                }

                memset(p_cert, 0x00, RM_CERT_MAX_LENGTH);

                for (uint32_t idx = 0; idx < cert_len; idx++)
                {
                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

 #if (ATCMD_SECURE_CHANNEL == 1)
                    if (!p_ctrl->secure_channel)
 #endif
                    {
                        if ((format == RM_CERT_FORMAT_PEM) && (ch == 0x0D))
                        {
                            ch = 0x0A;
                        }
                    }

                    p_cert[cert_idx] = ch;
                    cert_idx++;
                }

                if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
                {
                    break;
                }

 #if (ATCMD_SECURE_CHANNEL == 1)
                if (p_ctrl->secure_channel)
                {
                    fsp_err_t dec_res = cert_data_decrypted(p_at_ctrl, p_cert, &cert_len);

                    if (dec_res != FSP_SUCCESS)
                    {
                        err = dec_res;
                        break;
                    }
                }
 #endif
                done = true;
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        if (mode == RM_CERT_MODE_STORE)
        {
            /* Store certificate */
            cert_err = RM_CERT_Write(module, type, format, (unsigned char *) p_cert, (size_t) cert_idx);
        }
        else if (mode == RM_CERT_MODE_DELETION)
        {
            /* Delete certificate */
            cert_err = RM_CERT_Delete(module, type);
        }

        /* Convert error code */
        switch (cert_err)
        {
            case RM_CERT_ERR_OK:
            {
                err = FSP_ERR_AT_CMD_ERR_CMD_OK;
                break;
            }

            case RM_CERT_ERR_INVALID_MODULE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_MODULE;
                break;
            }

            case RM_CERT_ERR_INVALID_TYPE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_TYPE;
                break;
            }

            case RM_CERT_ERR_INVALID_FORMAT:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FORMAT;
                break;
            }

            case RM_CERT_ERR_INVALID_LENGTH:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_LENGTH;
                break;
            }

            case RM_CERT_ERR_INVALID_FLASH_ADDR:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_FLASH_ADDR;
                break;
            }

            case RM_CERT_ERR_INVALID_PARAMS:
            {
                err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                break;
            }

            case RM_CERT_ERR_FOPEN_FAILED:
            {
                err = FSP_ERR_AT_CMD_ERR_SFLASH_ACCESS;
                break;
            }

            case RM_CERT_ERR_MEM_FAILED:
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                break;
            }

            case RM_CERT_ERR_EMPTY_CERTIFICATE:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_EMPTY_CERT;
                break;
            }

            case RM_CERT_ERR_NOK:
            default:
            {
                err = FSP_ERR_AT_CMD_ERR_SSL_CERT_INTERNAL;
                break;
            }
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
 #if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
        bsp_safe_strcpy(resp_str, "\e\r\nOK\r\n", sizeof(resp_str));
 #else
        bsp_safe_strcpy(resp_str, "\r\nOK\r\n", sizeof(resp_str));
 #endif
    }
    else
    {
 #if (ATCMD_TRANSPORT_SDIO_W == 1 || ATCMD_TRANSPORT_SPI_W == 1)
        sprintf(resp_str, "\e\r\nERROR:0x%02x\r\n", (uint8_t) (err + 0x00000020));
 #else
        sprintf(resp_str, "\r\nERROR:%d\r\n", err);
 #endif
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

    if (p_cert)
    {
        vPortFree(p_cert);
        p_cert = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_FORMAT_CB(CERT)
{
    const char * p_usage = "<ESC>CERT,<module>, <certificate type>, <mode>[, <format>, <length>, <content>]";

    return p_usage;
}

RM_ATCMD_W_CORE_NETWORK_ATCMD_BRIEF_CB(CERT)
{
    const char * p_description = "Save or delete certificate/CA/private key/DH params";

    return p_description;
}
#endif                                 /* CFG_WIFI */
