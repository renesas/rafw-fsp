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
#include "bsp_api.h"
#include "bsp_definitions.h"
#include "bsp_sflash_map_ra6w1.h"
#include "bsp_defaults.h"

#include "rm_atcmd_w_cfg.h"
 #if (ATCMD_RF_TEST_SUPPORT == 1)
#include <stdlib.h>
#include "rm_atcmd_w_core_rf_test_parse.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "rm_cli_w_lmac.h"
#include "rwnx_mac_common.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(atcmd)         "AT+" # atcmd

#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char *argv[])
#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(atcmd)  \
    const char *RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(atcmd)   \
    const char *RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(atcmd)         RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(atcmd)  RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(atcmd)   RM_ATCMD_W_CORE_RF_TEST_ ## atcmd ## _brief_cb

#define RM_ATCMD_W_CORE_RF_TEST_DEBUG(fmt, ...)
#define RM_ATCMD_W_CORE_RF_TEST_ERROR(fmt, ...)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(TMLMACINIT);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(TMLMACINIT);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(TMLMACINIT);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTX);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTX);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTX);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTXSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTXSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTXSTOP);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCWTEST);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCWTEST);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCWTEST);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCWSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCWSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCWSTOP);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTART);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTART);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTART);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTATISTICS);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTATISTICS);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTATISTICS);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTATISTICSRESET);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTATISTICSRESET);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTATISTICSRESET);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTOP);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCONTSTART);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCONTSTART);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCONTSTART);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCONTSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCONTSTOP);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCONTSTOP);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCHANNEL);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCHANNEL);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCHANNEL);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFFREQ);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFFREQ);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFFREQ);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFVER);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFVER);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFVER);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTXPKT);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTXPKT);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTXPKT);

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFSCALEMODE);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFSCALEMODE);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFSCALEMODE);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_RF_test_module[] =
{
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(TMLMACINIT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(TMLMACINIT),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(TMLMACINIT),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(TMLMACINIT)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFTX),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFTX),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFTX),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFTX)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFTXSTOP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFTXSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFTXSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFTXSTOP)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFCWTEST),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFCWTEST),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFCWTEST),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFCWTEST)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFCWSTOP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFCWSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFCWSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFCWSTOP)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFRXSTART),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFRXSTART),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFRXSTART),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFRXSTART)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFRXSTATISTICS),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFRXSTATISTICS),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFRXSTATISTICS),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFRXSTATISTICS)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFRXSTATISTICSRESET),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFRXSTATISTICSRESET),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFRXSTATISTICSRESET),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFRXSTATISTICSRESET)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFRXSTOP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFRXSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFRXSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFRXSTOP)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFCONTSTART),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFCONTSTART),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFCONTSTART),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFCONTSTART)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFCONTSTOP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFCONTSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFCONTSTOP),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFCONTSTOP)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFCHANNEL),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFCHANNEL),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFCHANNEL),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFCHANNEL)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFFREQ),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFFREQ),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFFREQ),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFFREQ)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFVER),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFVER),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFVER),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFVER)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFTXPKT),
        ATCMD_W_TYPE_A,
        8,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFTXPKT),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFTXPKT),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFTXPKT)
    },
    {
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CODE(RFSCALEMODE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB_P(RFSCALEMODE),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB_P(RFSCALEMODE),
        RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB_P(RFSCALEMODE)
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

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_RF_TEST_register(atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_RF_test_module);
}

uint32_t RM_ATCMD_W_CORE_RF_TEST_deregister(atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_RF_test_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_RF_TEST_open(atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    return err;
}

uint32_t RM_ATCMD_W_CORE_RF_TEST_close(atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(TMLMACINIT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if CFG_PMGR
    romac4rtos_initialize(true, dg_configPTIMG_HDR_ADDR);
#endif
    lmac_init(0, NULL);
    lmac_start(argc, (const char **)argv);

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(TMLMACINIT)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(TMLMACINIT)
{
    const char * p_description = "Initialize and start the LMAC task.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTX)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if(!cmd_lmac_rftx(argc, (const char **)argv)) {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }
    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTX)
{
    const char * p_usage = "<freq>,<BW>,<numFrames>,<frameLen>,<txRate>,<txPower>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTX)
{
    const char * p_description = "RF TX test.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTXSTOP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    rftx_stop();

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTXSTOP)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTXSTOP)
{
    const char * p_description = "RF TX stop.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCWTEST)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    const char * rfcw_argv[] = {"rfcw", "start", "2412"};

    if(argc>1) {
            rfcw_argv[2] = argv[1]; // frequency
    }

    if (!lmac_rfcw(sizeof(rfcw_argv)/sizeof(char *), rfcw_argv)) {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCWTEST)
{
    const char * p_usage = "<freq>,<power>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCWTEST)
{
    const char * p_description = "RF CW test.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCWSTOP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    const char * rfcw_stop_argv[] = {"rfcw", "stop"};

    lmac_rfcw(sizeof(rfcw_stop_argv)/sizeof(char *), rfcw_stop_argv);

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCWSTOP)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCWSTOP)
{
    const char * p_description = "RF CW stop.";
    return p_description;
}

extern bool lmac_cmd_state_set(uint8_t state);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTART)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    lmac_cmd_state_set(LMAC_HW_ACTIVE);

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTART)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTART)
{
    const char * p_description = "RF RX test start.";
    return p_description;
}

#define MIB_BASE        0x60b00800
#define MIB_OFFSET(x) ((x - MIB_BASE)/4)
#define MIB_FCS_ERROR      MIB_OFFSET(0x60b00804)
#define MIB_RX_PHY_ERROR   MIB_OFFSET(0x60b00808)
#define MIB_RD_FIFO_OVER   MIB_OFFSET(0x60b0080C)
#define MIB_20M_FRAME_RX   MIB_OFFSET(0x60b00b80)
/// Address of the AGCINBDPOW20PSTAT register
#define RIU_AGCINBDPOW20PSTAT_ADDR   0x60C0320C
/// Address of the AGCINBDPOW20PNOISESTAT register
#define RIU_AGCINBDPOW20PNOISESTAT_ADDR   0x60C03228

extern void * lmac_cmd_mib(bool reset);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTATISTICS)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    bool reset = false;
    uint32_t result, pass, fcs, phy, overflow;
    uint32_t * pMib = (uint32_t *)lmac_cmd_mib(reset);
    char result_str[64] = {0, };
    int8_t rssi, dsss_rssi;

    pass = pMib[MIB_20M_FRAME_RX];
    fcs = pMib[MIB_FCS_ERROR];
    phy = pMib[MIB_RX_PHY_ERROR];
    overflow = pMib[MIB_RD_FIFO_OVER];

    rssi = *(int8_t *)(RIU_AGCINBDPOW20PSTAT_ADDR);
    dsss_rssi = *(int8_t *)(RIU_AGCINBDPOW20PNOISESTAT_ADDR);

    result = ((fcs + phy + overflow) * 100) / (fcs + phy + overflow + pass);
    sprintf(result_str, "Error rate=%ld\r\n", result);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    sprintf(result_str, "\r\nPass %04ld / fcs_error %04ld / phy_error %04ld / overflow %04ld\r\n", pass, fcs, phy, overflow);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    sprintf(result_str, "%s\r\n", (result > 10)? "Error":"Pass");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    sprintf(result_str, "\r\nRSSI=%d\r\n", rssi);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    sprintf(result_str, "\r\nDSSS RSSI=%d\r\n", dsss_rssi);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTATISTICS)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTATISTICS)
{
    const char * p_description = "RF Display PER.";
    return p_description;
}


RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTATISTICSRESET)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    bool reset = true;
    char result_str[64] = {0, };

    lmac_cmd_mib(reset);
    sprintf(result_str, "\r\nPER count reset \r\n");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTATISTICSRESET)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTATISTICSRESET)
{
    const char * p_description = "Reset PER.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFRXSTOP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    lmac_cmd_state_set(LMAC_HW_IDLE);

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFRXSTOP)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFRXSTOP)
{
    const char * p_description = "RF RX test stop.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCONTSTART)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[64] = {0, };

    if(lmac_cont_tx_start(argc, argv))
    {
        sprintf(result_str, "Continuos TX Started\n");
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    } else {
        sprintf(result_str, "TX ongoing ...\n");
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCONTSTART)
{
    const char * p_usage = "<freq>,<power>,<txRate>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCONTSTART)
{
    const char * p_description = "RF TX continuous.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCONTSTOP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[64] = {0, };

    lmac_cont_tx_stop(argc, argv);

    sprintf(result_str, "Continuos TX Stoped\n");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCONTSTOP)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCONTSTOP)
{
    const char * p_description = "RF TX continuous stop.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFCHANNEL)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[64] = {0, };
    uint32_t ch = 1; // Default channel 1, 2412
    uint32_t band = 2; // Default 2GHz Band
    uint32_t freq;

    if(argc > 1) {
        ch = atoi(argv[1]);
    }
    if(argc > 2) {
        band = atoi(argv[2]);
    }

    freq = lmac_cmd_channel_set(band, ch);

    if (freq) {
        sprintf(result_str, "Set RF %ldGHz Band, Channel %ld, Frequency %ldMHz\n", band, ch, freq);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    } else {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFCHANNEL)
{
    const char * p_usage = "<channel>,<band>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFCHANNEL)
{
    const char * p_description = "Change RF channel frequency.";
    return p_description;
}

extern uint32_t lmac_cmd_channel_frequncy_set(uint32_t freq);
RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFFREQ)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t freq = 2412; // Default 2412
    uint32_t band = 0;
    char result_str[64] = {0, };

    if(argc > 1) {
        freq = atoi(argv[1]);
    }

    if (!IS_FREQ_2P4GHZ(freq) && !IS_FREQ_5GHZ(freq)) {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    band = lmac_cmd_channel_frequncy_set(freq);
    if(band == 0) {
        band = 2;
    } else if(band == 1) {
        band = 5;
    }

    sprintf(result_str, "Set RF %ldGHz band, Frequency %ldMHz\n", band, freq);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFFREQ)
{
    const char * p_usage = "<freq>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFFREQ)
{
    const char * p_description = "Change RF frequency.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFVER)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[64] = {0, };
    extern char * lmac_cmd_get_rf_version(void);
    sprintf(result_str, "RF Driver version %s\n", (char *)lmac_cmd_get_rf_version());
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFVER)
{
    const char * p_usage = "<none>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFVER)
{
    const char * p_description = "Display RF driver version.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFTXPKT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret = cmd_lmac_rftxpkt(argc, (const char **) argv);

    if(ret == -1) 
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    } 
    else if(ret == -2)
    {
        err = FSP_ERR_AT_CMD_ERR_TIMEOUT;
    }
    
    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFTXPKT)
{
    const char * p_usage = "<freq>,<BW>,<txRate>,<txPower>,<ccaThreshold>,<ccaTimeout>,<timeOut>,<data-ASCII encoded>";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFTXPKT)
{
    const char * p_description = "RF TX Packet test.";
    return p_description;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_CB(RFSCALEMODE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int ret = lmac_cmd_scale_mode(argc, (const char **) argv);

    if (ret == -1)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else if (ret == -2)
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }
    else if (ret == -3)
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_FORMAT_CB(RFSCALEMODE)
{
    const char * p_usage = "<scale mode> for set command, <none> for get command";
    return p_usage;
}

RM_ATCMD_W_CORE_RF_TEST_ATCMD_BRIEF_CB(RFSCALEMODE)
{
    const char * p_description = "RF scale mode get/set";
    return p_description;
}
#endif
#endif /* CFG_WIFI */
