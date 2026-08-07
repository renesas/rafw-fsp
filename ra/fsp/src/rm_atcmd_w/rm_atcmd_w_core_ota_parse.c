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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include "FreeRTOS.h"

#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "rm_atcmd_w_core_ota_parse.h"
#include "rm_atcmd_w_core_ota_update.h"
#include "rm_atcmd_w_core_ota_common.h"
#include "rm_atcmd_w_core_ota_http.h"
#include "rm_atcmd_w_core_ota_mcu_fw.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
#endif
#if (SUPPORT_FSP_RM_OTA_W == 1)
 #include "rm_ota_w.h"
#endif                                 /* SUPPORT_FSP_RM_OTA_W */
#include "rm_wifi_helper.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_OTA_ATCMD_CODE(atcmd)    "AT+NW" # atcmd

#define RM_ATCMD_W_CORE_OTA_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_OTA_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_OTA_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_OTA_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_OTA_UNFIXED_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_OTA_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, size_t inlen)

#define RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_OTA_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_OTA_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_OTA_ ## atcmd ## _brief_cb

#define RM_ATCMD_W_CORE_OTA_DEBUG(fmt, ...)
#define RM_ATCMD_W_CORE_OTA_ERROR(fmt, ...)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/* __SUPPORT_OTA__ */
RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWSTART);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWSTART);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWSTART);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWSTOP);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWSTOP);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWSTOP);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWPROG);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWPROG);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWPROG);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTARENEW);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTARENEW);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTARENEW);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASETADDR);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASETADDR);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASETADDR);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAGETADDR);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAGETADDR);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAGETADDR);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAREADFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAREADFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAREADFLASH);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAERASEFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAERASEFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAERASEFLASH);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTACOPYFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTACOPYFLASH);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTACOPYFLASH);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATLSAUTH);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATLSAUTH);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATLSAUTH);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAALPN);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAALPN);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAALPN);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASNI);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASNI);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASNI);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAALPNDEL);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAALPNDEL);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAALPNDEL);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASNIDEL);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASNIDEL);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASNIDEL);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATLSVER);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATLSVER);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATLSVER);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASETBIDX);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASETBIDX);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASETBIDX);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAGETBIDX);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAGETBIDX);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAGETBIDX);

/* OTA_UPDATE_MCU_FW */
RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWNAME);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWNAME);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWNAME);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWSIZE);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWSIZE);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWSIZE);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWCRC);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWCRC);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWCRC);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAREADFW);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAREADFW);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAREADFW);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATRANSFW);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATRANSFW);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATRANSFW);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAERASEFW);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAERASEFW);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAERASEFW);

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTABYMCU);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTABYMCU);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTABYMCU);

RM_ATCMD_W_CORE_OTA_UNFIXED_ATCMD_CB(tx_size);
RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(tx_size);
RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(tx_size);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static ATCMD_W_OTA_UPDATE_CONFIG   _atcmd_ota_conf = {0, };
static ATCMD_W_OTA_UPDATE_CONFIG * atcmd_ota_conf  = (ATCMD_W_OTA_UPDATE_CONFIG *) &_atcmd_ota_conf;

#if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
#endif                                 /* SUPPORT_FSP_RM_OTA_W */

const atcmd_w_core_module_t at_core_ota_module[] =
{
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTADWSTART),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTADWSTART),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTADWSTART),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTADWSTART)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTADWSTOP),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTADWSTOP),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTADWSTOP),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTADWSTOP)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTADWPROG),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTADWPROG),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTADWPROG),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTADWPROG)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTARENEW),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTARENEW),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTARENEW),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTARENEW)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTASETADDR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTASETADDR),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTASETADDR),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTASETADDR)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAGETADDR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAGETADDR),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAGETADDR),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAGETADDR)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAREADFLASH),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAREADFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAREADFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAREADFLASH)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAERASEFLASH),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAERASEFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAERASEFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAERASEFLASH)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTACOPYFLASH),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTACOPYFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTACOPYFLASH),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTACOPYFLASH)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTATLSAUTH),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTATLSAUTH),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTATLSAUTH),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTATLSAUTH)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAALPN),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAALPN),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAALPN),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAALPN)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTASNI),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTASNI),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTASNI),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTASNI)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAALPNDEL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAALPNDEL),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAALPNDEL),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAALPNDEL)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTASNIDEL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTASNIDEL),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTASNIDEL),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTASNIDEL)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTATLSVER),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTATLSVER),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTATLSVER),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTATLSVER)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTASETBIDX),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTASETBIDX),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTASETBIDX),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTASETBIDX)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAGETBIDX),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAGETBIDX),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAGETBIDX),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAGETBIDX)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAFWNAME),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAFWNAME),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAFWNAME),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAFWNAME)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAFWSIZE),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAFWSIZE),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAFWSIZE),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAFWSIZE)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAFWCRC),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAFWCRC),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAFWCRC),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAFWCRC)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAREADFW),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAREADFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAREADFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAREADFW)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTATRANSFW),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTATRANSFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTATRANSFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTATRANSFW)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTAERASEFW),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTAERASEFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTAERASEFW),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTAERASEFW)
    },
    {
        RM_ATCMD_W_CORE_OTA_ATCMD_CODE(OTABYMCU),
        ATCMD_W_TYPE_A,
        3,
        0,
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(OTABYMCU),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(OTABYMCU),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(OTABYMCU)
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

const atcmd_w_core_unfixed_module_t at_core_ota_unfixed_module[] =
{
    {
        {PREFIX_OTA_BY_MCU, },
        strlen(PREFIX_OTA_BY_MCU),
        RM_ATCMD_W_CORE_OTA_ATCMD_CB_P(tx_size),
        RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB_P(tx_size),
        RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB_P(tx_size),
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
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_OTA_register (atcmd_w_core_module_list_t * p_list)
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

    if (rm_atcmd_w_core_register_module_node(p_list, at_core_ota_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (rm_atcmd_w_core_register_unfixed_module_node(p_list,
                                                     at_core_ota_unfixed_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_OTA_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_ota_module);
    rm_atcmd_w_core_unfixed_deregister(p_list, at_core_ota_unfixed_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_OTA_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

uint32_t RM_ATCMD_W_CORE_OTA_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

/* __SUPPORT_OTA__ */
RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWSTART)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int result_int             = get_run_mode();

    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
        (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
        (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
    {
        /* Init */
        atcmd_ota_conf->download_notify      = NULL;
        atcmd_ota_conf->renew_notify         = NULL;
        atcmd_ota_conf->download_sflash_addr = 0;

        if (strcmp(argv[1], "rtos") == 0)
        {
            atcmd_ota_conf->update_type = ATCMD_W_OTA_TYPE_RTOS;
            memset(atcmd_ota_conf->url, 0x00, ATCMD_W_OTA_HTTP_URL_LEN);
            memcpy(atcmd_ota_conf->url, argv[2], ATCMD_W_OTA_HTTP_URL_LEN);
        }
        else if (strcmp(argv[1], "cert_key") == 0)
        {
            atcmd_ota_conf->update_type = ATCMD_W_OTA_TYPE_CERT_KEY;
            memset(atcmd_ota_conf->url, 0x00, ATCMD_W_OTA_HTTP_URL_LEN);
            memcpy(atcmd_ota_conf->url, argv[2], ATCMD_W_OTA_HTTP_URL_LEN);
        }
        else if ((strcmp(argv[1], "other_fw") == 0) ||
                 (strcmp(argv[1], "mcu_fw") == 0) ||
                 (strcmp(argv[1], "fw_1") == 0))
        {
            atcmd_ota_conf->update_type = ATCMD_W_OTA_TYPE_MCU_FW;
            memset(atcmd_ota_conf->url, 0x00, ATCMD_W_OTA_HTTP_URL_LEN);
            memcpy(atcmd_ota_conf->url, argv[2], ATCMD_W_OTA_HTTP_URL_LEN);

            /* MCU FW name */
            if ((argv[3] != NULL) && (argc == 4))
            {
                if (atcmd_w_ota_update_set_mcu_fw_name(argv[3]) != 0)
                {
                    err = FSP_ERR_AT_CMD_ERR_NW_OTA_SET_MCU_FW_NAME;
                }
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NW_OTA_WRONG_FW_TYPE;
        }

        if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
        {
            if (atcmd_w_ota_update_start_download(p_at_ctrl, atcmd_ota_conf) != 0)
            {
                err = FSP_ERR_AT_CMD_ERR_NW_OTA_DOWN_OK_AND_WAIT_RENEW;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWSTART)
{
    const char * p_usage = "<fw_type>,<uri>,<mcu_fw_name>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWSTART)
{
    const char * p_descrption = "Start OTA download";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWSTOP)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 1)
    {
        int result_int = get_run_mode();
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            atcmd_w_ota_update_stop_download();
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWSTOP)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWSTOP)
{
    const char * p_descrption = "Stop OTA download";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTADWPROG)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               ret = 0;
    int  result_int            = get_run_mode();
    char result_str[32]        = {0, };

    if (argc == 2)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (strcmp(argv[1], "rtos") == 0)
            {
                ret = atcmd_w_ota_update_get_progress(ATCMD_W_OTA_TYPE_RTOS);
            }
            else if ((strcmp(argv[1], "mcu_fw") == 0) ||
                     (strcmp(argv[1], "other_fw") == 0) ||
                     (strcmp(argv[1], "fw_1") == 0))
            {
                ret = atcmd_w_ota_update_get_download_progress(ATCMD_W_OTA_TYPE_MCU_FW);
            }
            else if (strcmp(argv[1], "cert_key") == 0)
            {
                ret = atcmd_w_ota_update_get_progress(ATCMD_W_OTA_TYPE_CERT_KEY);
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_NW_OTA_WRONG_FW_TYPE;
            }

            if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
            {
                sprintf(result_str, "+NWOTADWPROG:%lu\r\n", ret);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTADWPROG)
{
    const char * p_usage = "<fw_type>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTADWPROG)
{
    const char * p_descrption = "Get OTA download progress";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTARENEW)
{
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    UINT status                = ATCMD_W_OTA_SUCCESS;
    char atc_buf[32]           = {0, };

    if (argc == 1)
    {
        int result_int = get_run_mode();
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
#if (SUPPORT_FSP_RM_OTA_W == 1)
            status = p_ota_instance->p_api->swap(p_ota_instance->p_ctrl);
#else
            status = atcmd_w_ota_update_start_renew(p_at_ctrl, atcmd_ota_conf);
#endif
            memset(atc_buf, 0x00, sizeof(atc_buf));
            sprintf(atc_buf, "+NWOTARENEW:0x%02x\r\n", status);
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) atc_buf, strlen(atc_buf));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTARENEW)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTARENEW)
{
    const char * p_descrption = "Replace with downloaded FW";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASETADDR)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int       result_int      = get_run_mode();
    char      result_str[256] = {0, };
    int       result_len      = 0;
    fsp_err_t ret             = 0;
    UINT      addr            = 0x00;

    FSP_PARAMETER_NOT_USED(addr);

    if (argc == 2)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            char * end = NULL;

            addr = strtol(argv[1], &end, 16);
#if (SUPPORT_FSP_RM_OTA_W == 1)
            ret = p_ota_instance->p_api->setAddr(p_ota_instance->p_ctrl, RM_OTA_W_USER_ADDR, addr);
#endif
            result_len = sprintf(result_str, "%s:0x%02x\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), ret);
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASETADDR)
{
    const char * p_usage = "<sflash_addr>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASETADDR)
{
    const char * p_descrption = "Set SFLASH address to store downloaded data from the server";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAGETADDR)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int      result_int      = get_run_mode();
    char     result_str[256] = {0, };
    int      result_len      = 0;
    uint32_t sflash_addr     = 0x00;

    if (argc == 2)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (strcmp(argv[1], "cert_key") == 0)
            {
#if (SUPPORT_FSP_RM_OTA_W == 1)
                p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl,
                                      RM_OTA_W_NEW_ADDR,
                                      (rm_ota_w_update_type_t) ATCMD_W_OTA_TYPE_CERT_KEY,
                                      &sflash_addr);
#endif
                result_len =
                    sprintf(result_str, "%s:0x%lx\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), sflash_addr);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
            }
            else if ((strcmp(argv[1], "mcu_fw") == 0) ||
                     (strcmp(argv[1], "other_fw") == 0) ||
                     (strcmp(argv[1], "fw_1") == 0))
            {
#if (SUPPORT_FSP_RM_OTA_W == 1)
                p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl,
                                      RM_OTA_W_NEW_ADDR,
                                      (rm_ota_w_update_type_t) ATCMD_W_OTA_TYPE_MCU_FW,
                                      &sflash_addr);
#endif
                result_len =
                    sprintf(result_str, "%s:0x%lx\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), sflash_addr);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_NW_OTA_WRONG_FW_TYPE;
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAGETADDR)
{
    const char * p_usage = "<fw_type>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAGETADDR)
{
    const char * p_descrption = "Get SFLASH address to store downloaded data from the server";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAREADFLASH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int result_int = get_run_mode();

    if (argc == 3)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            UINT   i, send_cnt = 0;
            UINT   blk_size = 0, last_blk_size = 0;
            char * buffer = NULL;
            UINT   addr, size;
            char * end = NULL;

            addr = strtol(argv[1], &end, 16);
            size = atoi(argv[2]);

            buffer = pvPortMalloc(4096);
            if (buffer == NULL)
            {
                err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            }
            else
            {
                memset(buffer, 0x00, 4096);

                send_cnt = size / 4096;
                if (size >= 4096)
                {
                    blk_size      = 4096;
                    last_blk_size = size % 4096;
                }
                else
                {
                    last_blk_size = size;
                }

                for (i = 0; i < send_cnt; i++)
                {
                    atcmd_w_ota_update_read_flash(addr, (VOID *) buffer, blk_size);
                    addr += 4096;
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) buffer, blk_size);
                }

                if (last_blk_size > 0)
                {
                    atcmd_w_ota_update_read_flash(addr, (VOID *) buffer, last_blk_size);
                    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) buffer, last_blk_size);
                }
            }

            if (buffer != NULL)
            {
                vPortFree(buffer);
                buffer = NULL;
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAREADFLASH)
{
    const char * p_usage = "<sflash_addr>,<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAREADFLASH)
{
    const char * p_descrption = "Read SFLASH as much as the input address and length";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAERASEFLASH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int  result_int     = get_run_mode();
    char result_str[32] = {0, };

    FSP_PARAMETER_NOT_USED(argv);

    if (argc == 3)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
#if (SUPPORT_FSP_RM_OTA_W == 1)
            UINT   addr, size;
            char * end = NULL;

            addr = strtol(argv[1], &end, 16);
            size = atoi(argv[2]);

            if (atcmd_w_ota_update_erase_flash(addr, size) == size)
            {
                sprintf(result_str, "+NWOTAERASEFLASH:%s\r\n", "COMPLETE");
            }
            else
            {
                sprintf(result_str, "+NWOTAERASEFLASH:%s\r\n", "FAIL");
            }
#endif
            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAERASEFLASH)
{
    const char * p_usage = "<sflash_addr>,<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAERASEFLASH)
{
    const char * p_descrption = "Erase SFLASH as much as the input address and length";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTACOPYFLASH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int  result_int            = get_run_mode();
    char result_str[32]        = {0, };

    FSP_PARAMETER_NOT_USED(argv);

    if (argc == 4)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            UINT   dest_addr, src_addr, size;
            char * end = NULL;

            dest_addr = strtol(argv[1], &end, 16);
            src_addr  = strtol(argv[2], &end, 16);
            size      = atoi(argv[3]);

            if (atcmd_w_ota_update_copy_flash(dest_addr, src_addr, size))
            {
                sprintf(result_str, "+NWOTACOPYFLASH:%s\r\n", "COMPLETE");
            }
            else
            {
                sprintf(result_str, "+NWOTACOPYFLASH:%s\r\n", "FAIL");
            }

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTACOPYFLASH)
{
    const char * p_usage = "<dest_sflash_addr>,<src_sflash_addr>,<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTACOPYFLASH)
{
    const char * p_descrption = "Copy as much as the length from SFLASH address src_addr to dest_addr";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATLSAUTH)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[32] = {0, };
    int  result_len     = 0;
    int  tmp_int;
    int  tmp_int1;

    if (argc == 1)
    {
        tmp_int    = atcmd_w_ota_update_get_tls_auth_mode();
        result_len = sprintf(result_str, "%s:%d\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), tmp_int);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
    }
    else if (argc == 2)
    {
        tmp_int1 = atoi(argv[1]);

        if ((tmp_int1 < MBEDTLS_SSL_VERIFY_NONE) || (tmp_int1 > MBEDTLS_SSL_VERIFY_UNSET))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_OTA_SET_TLS_AUTH_MODE_NVRAM;
        }
        else
        {
            if (atcmd_w_ota_update_set_tls_auth_mode(tmp_int1))
            {
                err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
            }
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATLSAUTH)
{
    const char * p_usage = "<tls_auth_mode>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATLSAUTH)
{
    const char * p_descrption = "Set mbedtls_ssl_conf_authmode";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAALPN)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[32] = {0, };
    int  result_int, tmp_int1;

    if (argc == 1)
    {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &result_int);
#endif
        if (result_int == -1)
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_NOT_SAVED_VALUE;

            return err;
        }
        else
        {
            char * result_str_pos;
            char   nvrName[32] = {0, };
            char * tmp_str     = NULL;
            char   tmp;

            if (result_int > 1)
            {
                char * res_str;
                int    alloc_bytes = 2;                                      // "[alpn_count],"

                alloc_bytes = alloc_bytes +
                              (result_int - 1) +                             // num (,)
                              (2 * result_int) +                             // num (double quotation)
                              (ATCMD_W_OTA_HTTPC_MAX_ALPN_LEN * result_int); // num(alpn)

                res_str = pvPortMalloc(alloc_bytes + 1);
                if (res_str == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;

                    return err;
                }

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
            for (char i = 0; i < tmp; i++)
            {
                sprintf(nvrName, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, i);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvrName, &tmp_str);
#endif
                if (tmp_str)
                {
                    sprintf(result_str_pos, ",\"%s\"", tmp_str);
                    result_str_pos = result_str_pos + (strlen(tmp_str) + 3); // 3 = 2x(") + 1x(,)
                }
            }

            if (result_str_pos != NULL)
            {
                vPortFree(result_str_pos);
            }
        }
    }
    else if (argc >= 3)
    {
        /* AT+NWOTAALPN=<#>,<alpn#n>,... */
        char tmp;

        tmp_int1 = atoi(argv[1]);

        /* Sanity check */
        if (argc - 2 > tmp_int1)
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
        else if (argc - 2 < tmp_int1)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }

        if ((tmp_int1 > ATCMD_W_OTA_HTTPC_MAX_ALPN_CNT) || (tmp_int1 <= 0))
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTC_ALPN_CNT_RANGE;

            return err;
        }

        for (int i = 0; i < tmp_int1; i++)
        {
            if (strlen(argv[i + 2]) > ATCMD_W_OTA_HTTPC_MAX_ALPN_LEN)
            {
                switch (i)
                {
                    case 0:
                    {
                        err = FSP_ERR_AT_CMD_ERR_NW_HTC_ALPN1_STR_LEN;
                        break;
                    }

                    case 1:
                    {
                        err = FSP_ERR_AT_CMD_ERR_NW_HTC_ALPN2_STR_LEN;
                        break;
                    }

                    case 2:
                    {
                        err = FSP_ERR_AT_CMD_ERR_NW_HTC_ALPN3_STR_LEN;
                        break;
                    }
                }

                return err;
            }
        }

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     (int *) &tmp);
#endif
        if (tmp != -1)
        {
            for (char i = 0; i < tmp; i++)
            {
                char nvr_name[32] = {0, };

                sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, i);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
#endif
            }

#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM);
#endif
        }

        tmp = (char) tmp_int1;
        for (char i = 0; i < tmp; i++)
        {
            char nvr_name[32] = {0, };

            sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, i);
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, argv[i + 2]);
#endif
        }

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                      tmp_int1);
#endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAALPN)
{
    const char * p_usage = "<alpn_count>,<alpn_1>,<alpn_2>,<alpn_3>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAALPN)
{
    const char * p_descrption = "Configure TLS ALPN protocol name";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASNI)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc == 2)
    {
        /* AT+NWOTASNI=<sni> */
        if (strlen(argv[1]) > ATCMD_W_OTA_HTTPC_MAX_SNI_LEN)
        {
            err = FSP_ERR_AT_CMD_ERR_NW_HTC_SNI_LEN;
        }
        else
        {
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                             ENV_GROUP_APPCFG,
                                             ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI,
                                             argv[1]);
#endif
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASNI)
{
    const char * p_usage = "<sni>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASNI)
{
    const char * p_descrption = "Configure TLS SNI";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAALPNDEL)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char tmp;

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_APPCFG,
                                 ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                 (int *) &tmp);
#endif

    if (tmp != -1)
    {
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_APPCFG,
                                  ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM);
#endif

        for (char i = 0; i < tmp; i++)
        {
            char nvr_name[32] = {0, };

            sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, i);
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
#endif
        }
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAALPNDEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAALPNDEL)
{
    const char * p_descrption = "Delete ALPN";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASNIDEL)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

#ifdef RM_MAP_PERSISTANT_W
    char * tmp_str = NULL;
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_APPCFG,
                                    ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI,
                                    &tmp_str);

    if (tmp_str)
    {
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_APPCFG,
                                  ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI);
    }
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASNIDEL)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASNIDEL)
{
    const char * p_descrption = "Delete SNI";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATLSVER)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int tls_version            = 0;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        /* AT+NWOTATLSVER=? */
        char result_str[32] = {0x00, };

        tls_version = atcmd_w_ota_http_client_get_tls_version();

        snprintf(result_str, sizeof(result_str), "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), tls_version);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }
    else if (argc == 2)
    {
        tls_version = atoi(argv[1]);

        if (rm_atcmd_w_core_common_stoi(argv[1], &tls_version, POL_1) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        if (atcmd_w_ota_http_client_set_tls_version(tls_version))
        {
            err = FSP_ERR_AT_CMD_ERR_NVRAM_WRITE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATLSVER)
{
    const char * p_usage = "<tls_version>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATLSVER)
{
    const char * p_descrption = "Set TLS version";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTASETBIDX)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char    result_str[32] = {0, };
    int     result_len     = 0;
    int     tmp_int1;
    uint8_t current_boot_idx = 0;

    FSP_PARAMETER_NOT_USED(tmp_int1);

    if (argc == 1)
    {
        /* AT+NWOTABIDX=? */
#if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->bootIdxGet(p_ota_instance->p_ctrl, &current_boot_idx);
#endif
        result_len = sprintf(result_str, "%s:%u\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), current_boot_idx);

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
    }
    else if (argc == 2)
    {
        /* AT+NWOTABIDX=<idx> */
        tmp_int1 = atoi(argv[1]);
#if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->bootIdxSet(p_ota_instance->p_ctrl, tmp_int1);
#endif
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTASETBIDX)
{
    const char * p_usage = "<boot_index>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTASETBIDX)
{
    const char * p_descrption = "Set boot index (0 or 1)";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAGETBIDX)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char    result_str[32]   = {0, };
    int     result_len       = 0;
    uint8_t current_boot_idx = 0;

    if (argc == 1)
    {
#if (SUPPORT_FSP_RM_OTA_W == 1)
        p_ota_instance->p_api->bootIdxGet(p_ota_instance->p_ctrl, &current_boot_idx);
#endif
        result_len = sprintf(result_str, "%s:%u\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), current_boot_idx);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAGETBIDX)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAGETBIDX)
{
    const char * p_descrption = "Get boot index";

    return p_descrption;
}

/* OTA_UPDATE_MCU_FW */
RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWNAME)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if (SUPPORT_FSP_RM_OTA_W == 1)
    char result_str[32] = {0, };
    int  result_len     = 0;
    int  result_int     = get_run_mode();

    if (argc == 1)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            char name[ATCMD_W_OTA_MCU_FW_NAME_LEN + 1];

            memset(name, 0x00, ATCMD_W_OTA_MCU_FW_NAME_LEN + 1);

            if (atcmd_w_ota_update_get_mcu_fw_info(&name[0], NULL, NULL))
            {
                err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
            }
            else
            {
                result_len = sprintf(result_str, "%s:%s\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), name);

                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWNAME)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWNAME)
{
    const char * p_descrption = "Get downloaded MCU FW name in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWSIZE)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if (SUPPORT_FSP_RM_OTA_W == 1)
    char result_str[32] = {0, };
    int  result_len     = 0;
    int  result_int     = get_run_mode();
    int  ret;

    if (argc == 1)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (atcmd_w_ota_update_get_mcu_fw_info(NULL, (UINT *) &ret, NULL) != ATCMD_W_OTA_SUCCESS)
            {
                err = FSP_ERR_AT_CMD_ERR_SFLASH_READ;
            }
            else
            {
                result_len = sprintf(result_str, "%s:%d\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), ret);
                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWSIZE)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWSIZE)
{
    const char * p_descrption = "Get downloaded MCU FW size in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAFWCRC)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if (SUPPORT_FSP_RM_OTA_W == 1)
    char result_str[32] = {0, };
    int  result_len     = 0;
    int  result_int     = get_run_mode();
    int  ret;

    if (argc == 1)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (atcmd_w_ota_update_get_mcu_fw_info(NULL, NULL, (UINT *) &ret))
            {
                err = FSP_ERR_AT_CMD_ERR_SFLASH_READ;
            }
            else
            {
                result_len = sprintf(result_str, "%s:0x%x\r\n", rm_atcmd_w_core_common_strupr(argv[0] + 2), ret);

                RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, result_len);
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAFWCRC)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAFWCRC)
{
    const char * p_descrption = "Get downloaded MCU FW CRC in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAREADFW)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[32] = {0, };
    int  result_int     = get_run_mode();

    if (argc == 3)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            UINT   addr, size;
            char * end = NULL;

            addr = strtol(argv[1], &end, 16);
            size = atoi(argv[2]);

            if (atcmd_w_ota_update_read_mcu_fw(p_at_ctrl, addr, size) != ATCMD_W_OTA_SUCCESS)
            {
                sprintf(result_str, "+NWOTAREADFW:%s\r\n", "FAIL");
            }
            else
            {
                sprintf(result_str, "+NWOTAREADFW:%s\r\n", "COMPLETE");
            }

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAREADFW)
{
    const char * p_usage = "<sflash_addr>,<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAREADFW)
{
    const char * p_descrption = "Read downloaded MCU FW in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTATRANSFW)
{
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    char result_str[32] = {0, };
    int  result_int     = get_run_mode();

    if (argc == 1)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (atcmd_w_ota_update_trans_mcu_fw(p_at_ctrl))
            {
                sprintf(result_str, "+NWOTATRANSFW:%s\r\n", "FAIL");
            }
            else
            {
                sprintf(result_str, "+NWOTATRANSFW:%s\r\n", "COMPLETE");
            }

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTATRANSFW)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTATRANSFW)
{
    const char * p_descrption = "Transfer downloaded MCU FW in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTAERASEFW)
{
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if (SUPPORT_FSP_RM_OTA_W == 1)
    char result_str[32] = {0, };
    int  result_int     = get_run_mode();

    if (argc == 1)
    {
        if ((result_int == WIFI_DEVICE_MODE_EXT_STATION) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP) ||
            (result_int == WIFI_DEVICE_MODE_EXT_AP_STATION))
        {
            if (atcmd_w_ota_update_erase_mcu_fw())
            {
                sprintf(result_str, "+NWOTAERASEFW:%s\r\n", "FAIL");
            }
            else
            {
                sprintf(result_str, "+NWOTAERASEFW:%s\r\n", "COMPLETE");
            }

            RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_COMMON_SYS_MODE;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

#else
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTAERASEFW)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTAERASEFW)
{
    const char * p_descrption = "Erase downloaded MCU FW in User Sflash area";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_ATCMD_CB(OTABYMCU)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
#if (SUPPORT_FSP_RM_OTA_W == 1)
    if (argc == 3)
    {
        /* Initialization to receive RA6W1/RA6W2 RTOS from MCU */
        if (strcasecmp(argv[1], "rtos") == 0)
        {
            if (atcmd_w_ota_update_by_mcu_init(ATCMD_W_OTA_TYPE_RTOS, atoi(argv[2])))
            {
                err = FSP_ERR_AT_CMD_ERR_NW_OTA_BY_MCU_INIT;
                goto atcmd_network_end;
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NW_OTA_WRONG_FW_TYPE;
            goto atcmd_network_end;
        }
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

atcmd_network_end:
#else
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);
#endif

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(OTABYMCU)
{
    const char * p_usage = "<fw_type>,<size>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(OTABYMCU)
{
    const char * p_descrption = "Receive RA6W1/RA6W2 RTOS from MCU";

    return p_descrption;
}

RM_ATCMD_W_CORE_OTA_UNFIXED_ATCMD_CB(tx_size)
{
    FSP_PARAMETER_NOT_USED(p_in);
    FSP_PARAMETER_NOT_USED(inlen);

    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err = FSP_SUCCESS;

    char         ch             = 0;
    char       * buffer         = NULL;
    char         result_str[32] = { };
    unsigned int ret            = ATCMD_W_OTA_FAILED;
    unsigned int tx_size        = 0;
    unsigned int rx_size        = 0;
    int          rev_idx        = 0;

    buffer  = pvPortMalloc(ATCMD_W_OTA_SFLASH_BUF_SZ + 16);
    rx_size = ATCMD_W_OTA_SFLASH_BUF_SZ + 16;

recv_start:
    if (buffer != NULL)
    {
        memset(buffer, 0x00, ATCMD_W_OTA_SFLASH_BUF_SZ + 16);
        for (rev_idx = 0; (unsigned int) rev_idx < rx_size; rev_idx++)
        {
            fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));
            if (fsp_err == FSP_SUCCESS)
            {
                if ((tx_size == 0) && (ch == ','))
                {
                    tx_size = strtol(buffer, NULL, 10);
                    if (tx_size > 0)
                    {
                        rx_size = tx_size;
                        goto recv_start;
                    }

                    err = FSP_ERR_AT_CMD_ERR_NW_OTA_BY_MCU_INIT;
                    memset(result_str, 0x00, sizeof(result_str));
                    sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ATCMD_W_OTA_FAILED);
                    goto recv_finish;
                }
                else
                {
                    buffer[rev_idx] = ch;
                }
            }
            else
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ATCMD_W_OTA_FAILED);
                goto recv_finish;
            }
        }

        ret = atcmd_w_ota_update_by_mcu_download((UCHAR *) buffer, rev_idx);

        if (ret == ATCMD_W_OTA_SUCCESS)
        {
            if (atcmd_w_ota_update_get_download_progress(ATCMD_W_OTA_TYPE_RTOS) == 100)
            {
                memset(result_str, 0x00, sizeof(result_str));
                sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ret);
            } 
            else 
            {
                if (tx_size == (unsigned int) rev_idx)
                {
                    sprintf(result_str, "\r\nOK\r\n");
                }
                else
                {
                    err = FSP_ERR_AT_CMD_ERR_NW_OTA_BY_MCU_INIT;
                    memset(result_str, 0x00, sizeof(result_str));
                    sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ATCMD_W_OTA_FAILED);
                }

                tx_size = 0;
                rev_idx = 0;
            }
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NW_OTA_BY_MCU_INIT;
            memset(result_str, 0x00, sizeof(result_str));
            sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ret);
        }
    }
    else
    {
        memset(result_str, 0x00, sizeof(result_str));
        sprintf(result_str, "+NWOTABYMCU:0x%02x\r\n", ATCMD_W_OTA_MEM_ALLOC_FAILED);
    }

recv_finish:
    if (buffer != NULL)
    {
        vPortFree(buffer);
        buffer = NULL;
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_OTA_ATCMD_FORMAT_CB(tx_size)
{
    const char * p_usage = "tx_size=<size>,<HexData>";

    return p_usage;
}

RM_ATCMD_W_CORE_OTA_ATCMD_BRIEF_CB(tx_size)
{
    const char * p_descrption = "Receive RA6W1/RA6W2 RTOS from MCU";

    return p_descrption;
}
#endif                                 /* CFG_WIFI */
