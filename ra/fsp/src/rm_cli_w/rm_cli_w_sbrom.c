/**
 ****************************************************************************************
 *
 * @file rm_cli_w_sbrom.c
 *
 * @brief SBROM console commands
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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "sdk_defs.h"
#if CFG_MBEDTLS
#include "r_cc312_secureboot.h"
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"

#include "bsp_otp.h"

#include "rm_psa_crypto_w_hwcfg.h"

#if defined(MBEDTLS_CONFIG_FILE)
#include MBEDTLS_CONFIG_FILE
#else
#include "mbedtls/config.h"
#endif

#include "r_cc312_common.h"

/* pal */
#include "cc_hal.h"
#include "cc_pal_init.h"
#include "cc_pal_types.h"
#include "cc_pal_types_plat.h"
#include "cc_pal_mem.h"
#include "cc_pal_perf.h"
#include "cc_regs.h"
#include "cc_otp_defs.h"
#include "cc_lib.h"

/* sbrom */
#include "rm_cli_w_sbrom.h"
#include "secureboot_defs.h"
#include "cc_cmpu.h"
#include "cc_dmpu.h"
#include "cc_prod_error.h"

#include "bsv_api.h"

#include "dx_crys_kernel.h"
#include "dx_env.h"
#include "dx_rng.h"
#include "dx_nvm.h"
#include "mbedtls_cc_mng_int.h"
#include "mbedtls_cc_util_asset_prov.h"

#include "r_cc312_crypto.h"
/////////////////////////////////////////////////////////////////////////////////////////

/* NOTICE !
 * We will release SDK with 'SUPPORT_SECURE_PRODUCTION = 0' to protect Secure OTP.
 * After keeping in mind that once this process has bebun and then it cannot be cancelled,
 * please change 'SUPPORT_SECURE_PRODUCTION = 1' to contiue.
 */
#define SUPPORT_SECURE_PRODUCTION       (0)

#define TEST_WKEY_VERIFICATION          (0)

#define APP_MALLOC(...)           pvPortMalloc(__VA_ARGS__)
#define APP_FREE(...)             vPortFree(__VA_ARGS__)
#define PTEST_PRINTF(...)         printf( __VA_ARGS__ )

#define SBROM_TEST_LOG(...)       printf( __VA_ARGS__ )

/******************************************************************************
 *
 ******************************************************************************/

#define ptest_puthexa(x)        ( ((x)<10) ? ('0' + (x)) : ('A' + (x) - 10) )
#define ptest_putascii(x)       ( ((x)<32) ? ('.') : ( ((x)>126) ? ('.') : (x) ) )

static void peritest_dump_print(uint32_t addr, uint8_t *data, uint32_t length, uint32_t endian)
{
        uint32_t  i, h, s, spc, trim, lmask;
        uint8_t  *data8;
        char    *hexadump;
        char    *strdump;
        char    hexa;

        data8 = (uint8_t *)data;
        endian = (endian&0x0f);
        endian = (endian == 0) ? 1 : endian ;

        lmask = (endian == 4) ? 0x03 : ((endian == 2)? 0x01 : 0 );
        lmask = ~lmask;

        hexadump = (char *)APP_MALLOC((32+32*4));
        strdump = (char *)APP_MALLOC(64);

        for(i=0, h=0, s =0; i<length; i++){
                trim = i & (~lmask);
                hexa = data8[(i&lmask)|(~(trim|lmask))] ;

                hexadump[h++] = ptest_puthexa( ((hexa>>4)&0x0f) );
                hexadump[h++] = ptest_puthexa( ((hexa)&0x0f) );
                if( trim == (endian-1) ){
                        hexadump[h++] = ' ';
                }

                strdump[s++] = ptest_putascii( hexa );

                if( ((i%32) == 31) || ((i+1) == length) ){
                        hexadump[h] = '\0';
                        strdump[s] = '\0';

                        PTEST_PRINTF("[%08lX] : ", (addr+i-(i%32)) );
                        PTEST_PRINTF(hexadump);
                        for( spc = (33 - (i%32)); spc > 0; spc-- ){
                                PTEST_PRINTF("   ");
                        }
                        PTEST_PRINTF("%s", strdump);
                        PTEST_PRINTF("\n");
                        h = 0;
                        s = 0;
                }
        }

        APP_FREE(hexadump);
        APP_FREE(strdump);
}

//------------------------------------------------------------
// These Samples is only for Test. Do NOT USE !!
//------------------------------------------------------------
#if (SUPPORT_SECURE_PRODUCTION == 1)
__ALIGNED(4) static const unsigned char cmpu_hex_list[] = {
0x01, 0x00, 0x00, 0x00, 		//uniqueDataType
0xf0, 0x96, 0x32, 0x6c, 0x8d, 0x4a, 0xc2, 0x68, 0xde, 0xb7, 0xec, 0xb8, 0x5b, 0xb5, 0x2f, 0x62,
		//uniqueBuff
0x02, 0x00, 0x00, 0x00, 		//kpicvDataType
0x64, 0x6f, 0x72, 0x50, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x31, 0x76, 0x65, 0x52,
0x32, 0x76, 0x65, 0x52, 0xf7, 0x7b, 0x27, 0xf4, 0xfe, 0x41, 0x3d, 0x02, 0xcf, 0x83, 0xe9, 0x78,
0x11, 0x9f, 0x8e, 0x3b, 0xc2, 0xfe, 0xce, 0xc1, 0xb7, 0xe6, 0x4b, 0xde, 0x87, 0x56, 0x7d, 0xd8,
0x1b, 0x23, 0x73, 0x87, 0x7a, 0xbf, 0x3c, 0x38, 0x7c, 0xc2, 0xf6, 0x57, 0x81, 0xcc, 0x6d, 0xf1,
		//kpicv
0x02, 0x00, 0x00, 0x00, 		//kceicvDataType
0x64, 0x6f, 0x72, 0x50, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x31, 0x76, 0x65, 0x52,
0x32, 0x76, 0x65, 0x52, 0x0b, 0xfc, 0x9b, 0x04, 0x94, 0x38, 0x3a, 0x7f, 0xbe, 0x9d, 0xd2, 0x9e,
0xec, 0x17, 0xf3, 0x98, 0xf7, 0xf1, 0x69, 0xb8, 0x5c, 0x7c, 0xbd, 0xfc, 0xc1, 0x92, 0x52, 0x30,
0x93, 0x1f, 0x11, 0x04, 0xc7, 0x21, 0xb2, 0xf6, 0x33, 0x0e, 0x06, 0x96, 0xb1, 0xb3, 0xa5, 0xbd,
		//kceicv
0x00, 0x00, 0x00, 0x00, 		//icvMinVersion
0x00, 0x00, 0x00, 0x00, 		//icvConfigWord
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
		//icvDcuDefaultLock
};

__ALIGNED(4) static const unsigned char dmpu_hex_list[] = {
0x01, 0x00, 0x00, 0x00, 		//uniqueDataType
0x71, 0x42, 0x15, 0xa0, 0x69, 0xa4, 0x80, 0x52, 0x3a, 0xbc, 0x09, 0xda, 0x2d, 0x46, 0x9d, 0xed,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		//uniqueBuff
0x02, 0x00, 0x00, 0x00, 		//kcpDataType
0x64, 0x6f, 0x72, 0x50, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x31, 0x76, 0x65, 0x52,
0x32, 0x76, 0x65, 0x52, 0xa1, 0x70, 0xc0, 0xf0, 0x4e, 0xa8, 0xa2, 0x39, 0xd2, 0x25, 0xbe, 0x44,
0xa5, 0xb8, 0xfa, 0x7d, 0x82, 0x0d, 0x62, 0x15, 0xb1, 0x31, 0x0e, 0x1f, 0xb7, 0xb9, 0x2c, 0xbe,
0x92, 0x8a, 0xb1, 0x39, 0x98, 0x01, 0x5b, 0x6b, 0xdb, 0x83, 0x69, 0x4e, 0x30, 0x56, 0x6a, 0xe1,
		//kcp
0x02, 0x00, 0x00, 0x00, 		//kceDataType
0x64, 0x6f, 0x72, 0x50, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x00, 0x00, 0x31, 0x76, 0x65, 0x52,
0x32, 0x76, 0x65, 0x52, 0xd6, 0xac, 0x35, 0x42, 0x77, 0xeb, 0x99, 0xae, 0x01, 0xef, 0x68, 0xa2,
0x45, 0x81, 0x5b, 0xbf, 0x3d, 0x68, 0x53, 0x0d, 0x30, 0x4b, 0x4c, 0x14, 0xd1, 0xc3, 0x81, 0x80,
0x6d, 0xf7, 0x5e, 0x08, 0xd3, 0x38, 0x4a, 0x23, 0x15, 0xe1, 0x10, 0xde, 0x31, 0x1a, 0x30, 0x40,
		//kce
0x00, 0x00, 0x00, 0x00, 		//oemMinVersion
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff,
		//oemDcuDefaultLock
};
#endif

__ALIGNED(4) static const unsigned char cm_secure_asset[] = {
0x74, 0x65, 0x73, 0x41, 0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0xc2, 0xf8, 0xcb, 0xf0, 0x91, 0xf0, 0xe9, 0xa4, 0x6e, 0x15, 0x40, 0xfe,
0x6d, 0xc0, 0xd2, 0x03, 0x48, 0x1e, 0xb7, 0xf4, 0xe4, 0xa1, 0xda, 0x51, 0x82, 0xcf, 0x61, 0x7c,
0x58, 0x07, 0x58, 0x6f, 0xd1, 0x8d, 0xa8, 0xa2, 0xcb, 0x2a, 0x4b, 0xee, 0xc9, 0x27, 0x53, 0xc0,
0x15, 0x40, 0xa2, 0x24, 0x98, 0x75, 0x4f, 0xe8, 0x8a, 0xe0, 0x4e, 0xc3, 0x63, 0xb3, 0xb0, 0x6b,
0x3d, 0xe1, 0x52, 0x4c, 0x31, 0x0c, 0x53, 0x4b, 0x7c, 0x71, 0xf5, 0x66, 0x25, 0x58, 0x02, 0x2a,
0x34, 0x92, 0xcb, 0xa8, 0xec, 0x56, 0x9f, 0x1e, 0xd6, 0x4b, 0x00, 0x50, 0xde, 0x3b, 0xa4, 0x39,
};

__ALIGNED(4) static const unsigned char dm_secure_asset[] = {
0x74, 0x65, 0x73, 0x41, 0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x2a, 0xd3, 0x0c, 0xac, 0x0c, 0xd6, 0x4e, 0x18, 0x2e, 0xa2, 0x2c, 0x5f,
0xb7, 0xdf, 0x35, 0x9c, 0x36, 0x1f, 0x30, 0x91, 0x00, 0x21, 0x6c, 0x2d, 0xe4, 0x32, 0x96, 0x61,
0x85, 0x5e, 0x97, 0x4f, 0xa9, 0x65, 0x11, 0xe5, 0x49, 0xec, 0x23, 0xfe, 0x19, 0xdd, 0x9b, 0x19,
0x33, 0x59, 0x9e, 0xbb, 0xf1, 0x5e, 0x75, 0x59, 0x89, 0x78, 0xde, 0x5b, 0x8e, 0xda, 0xd9, 0x98,
0xcd, 0x7f, 0x83, 0x83, 0x4e, 0x8b, 0x19, 0x2e, 0x0b, 0x4b, 0x74, 0xcd, 0xaf, 0x2d, 0xd9, 0x1f,
0xca, 0x45, 0x7a, 0x9d, 0x9e, 0xf7, 0x2e, 0x1e, 0x18, 0xfe, 0x6d, 0xa3, 0x90, 0x5d, 0x10, 0x93,
};

/*
 * These patterns are used to check if the wrapped key is generated correctly
 * using the secret key, Kcp & Kpicv during development.
 */
#if     (TEST_WKEY_VERIFICATION == 1)
__ALIGNED(4) static const unsigned char cm_wkey_asset[] = {
0x81, 0x8f, 0x46, 0xa5, 0x74, 0x65, 0x73, 0x41, 0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x29, 0xef, 0x55, 0x03, 0x57, 0x32, 0x8d,
0x03, 0xe2, 0x6f, 0x5d, 0xd2, 0xc9, 0xd5, 0x35, 0x1f, 0xb6, 0x5e, 0xd5, 0x77, 0x4b, 0x71, 0x29,
0xa7, 0xb3, 0xc7, 0x7b, 0xe6, 0xca, 0x77, 0x83, 0x7e, 0x6d, 0xab, 0xa8, 0x8c, 0x10, 0xf8, 0x0b,
0x93, 0x80, 0xd5, 0xbc, 0x02, 0x98, 0x90, 0xa4, 0xae, 0x0a, 0x17, 0xc0, 0x57, 0x60, 0xf7, 0x06,
0x18, 0x6a, 0x93, 0xdd, 0x11, 0xf6, 0xf6, 0xf7, 0xe9, 0xa5, 0x9a, 0x94, 0x30, 0xab, 0x9e, 0xc1,
0x19, 0x73, 0x43, 0x75, 0xb7, 0x29, 0xeb, 0x43, 0xce, 0x2e, 0x21, 0xf7, 0xc9, 0x4e, 0x4f, 0x8c,
0x4f, 0x70, 0x66, 0x66, };

__ALIGNED(4) static const unsigned char dm_wkey_asset[] = {
0x0f, 0xe1, 0xc8, 0x92, 0x74, 0x65, 0x73, 0x41, 0x00, 0x00, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x84, 0x8d, 0x88, 0x10, 0x59, 0xac,
0xb6, 0x60, 0xd6, 0x69, 0x8c, 0x93, 0x48, 0x18, 0x53, 0x90, 0x4d, 0x24, 0x90, 0x0c, 0x16, 0xcc,
0x73, 0x09, 0xed, 0xc7, 0xe5, 0x28, 0xa7, 0x48, 0xe8, 0x8c, 0xba, 0x3a, 0xfc, 0xea, 0x38, 0x41,
0x95, 0x92, 0x79, 0x18, 0x9f, 0xa2, 0xa3, 0x5f, 0x21, 0x34, 0x5e, 0xf1, 0x6a, 0xf3, 0x2d, 0x46,
0xe8, 0x74, 0x2f, 0xbd, 0x90, 0x93, 0x85, 0xab, 0xb4, 0xf3, 0xf7, 0x56, 0x34, 0x8a, 0x0a, 0x7a,
0x1f, 0xb1, 0xb5, 0x3f, 0xbc, 0xd5, 0xe4, 0xe9, 0x6e, 0x44, 0x5c, 0x10, 0x4d, 0x9b, 0x77, 0xb3,
0xee, 0xa6, 0x39, 0xcf, };
#endif     //(TEST_WKEY_VERIFICATION == 1)

/******************************************************************************
 *
 ******************************************************************************/

static void print_errcode_prod(unsigned int ecode)
{
        switch(ecode){
        case CC_PROD_INIT_ERR:
                PTEST_PRINTF("CC_PROD_INIT_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_INVALID_PARAM_ERR:
                PTEST_PRINTF("CC_PROD_INVALID_PARAM_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_ILLEGAL_ZERO_COUNT_ERR:
                PTEST_PRINTF("CC_PROD_ILLEGAL_ZERO_COUNT_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_ILLEGAL_LCS_ERR:
                PTEST_PRINTF("CC_PROD_ILLEGAL_LCS_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_ASSET_PKG_PARAM_ERR:
                PTEST_PRINTF("CC_PROD_ASSET_PKG_PARAM_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_ASSET_PKG_VERIFY_ERR:
                PTEST_PRINTF("CC_PROD_ASSET_PKG_VERIFY_ERR(%08x)\n", ecode);
                break;
        case CC_PROD_HAL_FATAL_ERR:
                PTEST_PRINTF("CC_PROD_HAL_FATAL_ERR(%08x)\n", ecode);
                break;
        default:
                PTEST_PRINTF("CC_PROD_Unknown(%08x)\n", ecode);
                break;
        }
}

static uint32_t dump_secure_otp(void)
{
    uint32_t i, rval;
    uint32_t *api_dump, *reg_dump;

    api_dump = (uint32_t *)APP_MALLOC(sizeof(uint32_t) * 0x2D );
    reg_dump = (uint32_t *)APP_MALLOC(sizeof(uint32_t) * 0x2D );

    rval = true;
    // PTEST_PRINTF("OTP API Read Access\n");
    for (i = 0; i < 0x2d; i++)
    {
        api_dump[i] = bsp_otp_word_read(i);
    }

    // PTEST_PRINTF("OTP Direct Read Access\n");
    for (i = 0; i < 0x2d; i++)
    {
        reg_dump[i] = *((volatile uint32_t *)((RRQ61X_ACRYPT_BASE|0x02000) | (i<<2)));
    }

    PTEST_PRINTF(    "OTP Read Access :    API   ,   DIRECT ============\n");
    PTEST_PRINTF(    "OTP Read Access : %08x , %08x  \n", 0, (unsigned int)(RRQ61X_ACRYPT_BASE|0x02000));
    for (i = 0; i < 0x2d; i++)
    {
        PTEST_PRINTF("       %08x : %08x , %08x  %s\n"
                        , (unsigned int)i, (unsigned int)api_dump[i], (unsigned int)reg_dump[i]
                        , ((api_dump[i] == reg_dump[i]) ? "" : "mismatch") );

        if(api_dump[i] != reg_dump[i]){
                rval = false;
        }
    }

    APP_FREE(api_dump);
    APP_FREE(reg_dump);

    return rval;
}

/******************************************************************************
 *
 ******************************************************************************/

static uint32_t thread_peripheral_sbrom_lcscm(uint32_t lcs)
{
        uint32_t status;
#if    (SUPPORT_SECURE_PRODUCTION == 1)
        uint8_t *cmpu;
#endif	//(SUPPORT_SECURE_PRODUCTION == 1)

        RA6W1_UNUSED_ARG(lcs);
        PTEST_PRINTF("Security Lifecycle:CM\n");

        {
                PTEST_PRINTF("SecureProduction.CMPU-PATTERN Test @ lcs-CM\n");
#if    (SUPPORT_SECURE_PRODUCTION == 1)
                cmpu = (uint8_t *)cmpu_hex_list;
#endif//(SUPPORT_SECURE_PRODUCTION == 1)

#if    (SUPPORT_SECURE_PRODUCTION == 1)
                status = R_CC312_SecureBoot_CMPU(cmpu, 0);
#else
                SBROM_TEST_LOG(
                        "\n* -----------------------------*\n"
                        "* Secure Production is on hold to protect Secure OTP.\n"
                        "* If you want to continue, \n"
                        "* please set 'SUPPORT_SECURE_PRODUCTION = 1' and re-build.\n"
                        "* -----------------------------*\n"
                        );
                status = true;
#endif  //(SUPPORT_SECURE_PRODUCTION == 1)

                if( status != true ){
                        PTEST_PRINTF("CMPU Error: %X\n", (unsigned int)status);
                        print_errcode_prod((unsigned int)R_CC312_Debug_Get_ErrorCode());
                }else
                {
                        PTEST_PRINTF("CMPU OK\n");
                        SBROM_TEST_LOG(
                                "\n* -----------------------------*\n"
                                "* Please Turn-OFF the system !!\n"
                                "* If you have completed it correctly, you should do POR-Reboot quickly\n"
                                "*  to keep your secret keys securely.\n"
                                "* -----------------------------*\n"
                        );
                }
        }

        return status;
}

///////////////////////////////////////////////////////////////////////

static uint32_t thread_peripheral_sbrom_lcsdm(uint32_t lcs)
{
        uint32_t status;
#if    (SUPPORT_SECURE_PRODUCTION == 1)
        uint8_t *dmpu;
#endif	//(SUPPORT_SECURE_PRODUCTION == 1)

        RA6W1_UNUSED_ARG(lcs);
        PTEST_PRINTF("Security Lifecycle:DM\n");

        {
                PTEST_PRINTF("SecureProduction.DMPU-PATTERN Test @ lcs-DM\n");
#if    (SUPPORT_SECURE_PRODUCTION == 1)
                dmpu = (uint8_t *)dmpu_hex_list;
#endif	//(SUPPORT_SECURE_PRODUCTION == 1)

#if    (SUPPORT_SECURE_PRODUCTION == 1)
                status = R_CC312_SecureBoot_DMPU(dmpu);
#else
                SBROM_TEST_LOG(
                        "\n* -----------------------------*\n"
                        "* Secure Production is on hold to protect Secure OTP.\n"
                        "* If you want to continue, \n"
                        "* please set 'SUPPORT_SECURE_PRODUCTION = 1' and re-build.\n"
                        "* -----------------------------*\n"
                        );
                status = true;
#endif

                if( status != true ){
                        PTEST_PRINTF("DMPU Error : %X\n", (unsigned int)status);
                        print_errcode_prod((unsigned int)R_CC312_Debug_Get_ErrorCode());
                }else{
                        PTEST_PRINTF("DMPU OK\n");
                        SBROM_TEST_LOG(
                                "\n* -----------------------------*\n"
                                "* Please Turn-OFF the system !!\n"
                                "* If you have completed it correctly, you should do POR-Reboot quickly\n"
                                "*  to keep your secret keys securely.\n"
                                "* -----------------------------*\n\n"
                        );
                }
        }

        return status;
}

///////////////////////////////////////////////////////////////////////

static int thread_peripheral_sbrom(int argc, const char **argv)
{
        uint32_t status;
        uint32_t lcs;

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        status = true;
        PTEST_PRINTF("CC312 SBROM Test Routine\n");

        //TEST: FC9K_Crypto_ClkMgmt(false);
        //TEST: FC9K_Crypto_x2ClkMgmt(true);

        if( mbedtls_mng_lcsGet(&lcs) == 0 ){

                PTEST_PRINTF("SBROM lcs[%08x]\n", (unsigned int)lcs);

                R_CC312_SecureSocID();

                switch(lcs){
                case    0x00: // CM
                        status = thread_peripheral_sbrom_lcscm(lcs);
                        break;
                case    0x01: // DM
			status = thread_peripheral_sbrom_lcsdm(lcs);
                        break;
                case    0x05: // Secure
                        {
				PTEST_PRINTF("Scenario Lifecycle:Secure\n");
				status = true;
                        }
                        break;
                case    0x07: // RMA
			status = true;
                        break;
                default:
			status = true;
                        break;
                }
        }
        else
        {
                PTEST_PRINTF("SBROM Error\n");
                status = false;
        }

        return status;
}


static int sbrom_cli_otp_dump(int argc, const char **argv)
{
        uint32_t i, idx, status;
        volatile uint32_t *otpaddr;
        const uint32_t otp_base_list[1] = { (RRQ61X_ACRYPT_BASE|0x02000) };

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);
        // Read Access Test

        status = true;

        for( i = 0; i < 1; i++ ){
                otpaddr = (uint32_t *)(otp_base_list[i]) ;

                idx = 0 ;
                PTEST_PRINTF("\nOTP Secure Area ===============");
                PTEST_PRINTF("\nHUK      [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x08; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nKpicv    [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x0C; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nKceicv   [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x10; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nICV-prog [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x11; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nHBK0     [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x15; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nHBK1     [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x19; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nKcp      [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x1D; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nKce      [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x21; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nOEM-prog [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x22; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }

                PTEST_PRINTF("\nICV-NVC  [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x24; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nOEM-NVC  [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x27; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\nGPPC     [%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x28; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }

                PTEST_PRINTF("\nOTP-DLock[%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x2C; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }

                PTEST_PRINTF("\nOTP-SLock[%p]: ", &(otpaddr[idx]));
                for( ; idx < 0x2D; idx++ ){ PTEST_PRINTF("%08x ", (unsigned int)otpaddr[idx]); }
                PTEST_PRINTF("\n");
        }

        dump_secure_otp();

        return status;
}


typedef         struct  {
        char    *regname;
        uint32_t  regaddr;
        uint32_t  regcheck[9];
} SBROM_DEBUG_TYPE;

#pragma GCC diagnostic ignored "-Wmissing-braces"
static const SBROM_DEBUG_TYPE sbrom_debug_list[] = {   // no-check                                  CM           DM           Secure       CM-Dbg       DM-Dbg       DM-Rma       DM-Rma!       RMA
{ "AO.DCU_EN0"    , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_EN0)                  , {0xA5A5A5A5UL,0x000003ffUL,0x000003ffUL,0x000003fdUL,0x000000c1UL,0x0000000cUL,0x000003bdUL,0xA5A5A5A5UL,0x000003ffUL} },
{ "AO.DCU_EN1"    , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_EN1)                  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_EN2"    , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_EN2)                  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_EN3"    , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_EN3)                  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_LOCK0"  , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_LOCK0)                , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x000003ffUL,0x000003ffUL,0x000003ffUL,0xA5A5A5A5UL,0xA5A5A5A5UL} },
{ "AO.DCU_LOCK1"  , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_LOCK1)                , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_LOCK2"  , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_LOCK2)                , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_LOCK3"  , CC_REG_OFFSET(CRY_KERNEL, HOST_DCU_LOCK3)                , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0xA5A5A5A5UL,0x00000000UL} },
{ "AO.DCU_MSK0"   , CC_REG_OFFSET(CRY_KERNEL, AO_ICV_DCU_RESTRICTION_MASK0)  , {0xA5A5A5A5UL,0x000003c3UL,0x000003c3UL,0x000003c3UL,0x000003c3UL,0x000003c3UL,0x000003c3UL,0x000003c3UL,0x000003c3UL} },
{ "AO.DCU_MSK1"   , CC_REG_OFFSET(CRY_KERNEL, AO_ICV_DCU_RESTRICTION_MASK1)  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL} },
{ "AO.DCU_MSK2"   , CC_REG_OFFSET(CRY_KERNEL, AO_ICV_DCU_RESTRICTION_MASK2)  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL} },
{ "AO.DCU_MSK3"   , CC_REG_OFFSET(CRY_KERNEL, AO_ICV_DCU_RESTRICTION_MASK3)  , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL} },
{ "AO.SEC_DBG_RST", CC_REG_OFFSET(CRY_KERNEL, AO_CC_SEC_DEBUG_RESET)         , {0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL} },
{ "AO.LOCK_BITS"  , CC_REG_OFFSET(CRY_KERNEL, HOST_AO_LOCK_BITS)             , {0xA5A5A5A5UL,0x00000100UL,0x00000100UL,0x00000100UL,0x00000140UL,0x00000100UL,0x00000100UL,0x00000100UL,0x00000100UL} },
{ "AO.APB_FILTER" , CC_REG_OFFSET(CRY_KERNEL, AO_APB_FILTERING)              , {0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL,0xA5A5A5A5UL} },
{ "AO.GPPC"       , CC_REG_OFFSET(CRY_KERNEL, AO_CC_GPPC)                    , {0xA5A5A5A5UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL,0x00000000UL} },
{ "NVM.FUSE_PROG" , CC_REG_OFFSET(CRY_KERNEL, AIB_FUSE_PROG_COMPLETED)       , {0xA5A5A5A5UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL} },
{ "NVM.DBG_STAT"  , CC_REG_OFFSET(CRY_KERNEL, NVM_DEBUG_STATUS)              , {0xA5A5A5A5UL,0x0000000eUL,0x0000000eUL,0x0000000eUL,0x0000000eUL,0x0000000eUL,0x0000000eUL,0x0000000eUL,0x0000000eUL} },
{ "NVM.LCS_VALID" , CC_REG_OFFSET(CRY_KERNEL, LCS_IS_VALID)                  , {0xA5A5A5A5UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL} },
{ "NVM.IS_IDLE"   , CC_REG_OFFSET(CRY_KERNEL, NVM_IS_IDLE)                   , {0xA5A5A5A5UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL,0x00000001UL} },
{ "NVM.LCS_REG"   , CC_REG_OFFSET(CRY_KERNEL, LCS_REG)                       , {0xA5A5A5A5UL,0x00000000UL,0x00000001UL,0x00000005UL,0x00000005UL,0x00000005UL,0x00000005UL,0xA5A5A5A5UL,0x00000007UL} },
{ NULL          , (0), 0x0 }
};
#pragma GCC diagnostic pop

static int sbrom_cli_ao_dump(int argc, const char **argv)
{
        uint32_t i, regval;

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        for( i = 0; sbrom_debug_list[i].regname != NULL; i++ ){
                volatile uint32_t *regaddr ;

                regaddr = (volatile uint32_t *)(RRQ61X_ACRYPT_BASE | sbrom_debug_list[i].regaddr);
                regval = *regaddr;

                PTEST_PRINTF("%s [%p]: %08x \n"
                        , sbrom_debug_list[i].regname
                        , regaddr
                        , (unsigned int)regval
                );
        }
        return true;
}

static int sbrom_cli_socid_dump(int argc, const char **argv)
{
        uint8_t SocIDBuf[32];

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        memset( SocIDBuf, 0x00, (sizeof(uint8_t)*32) );

        if( R_CC312_SecureSocID_internal( SocIDBuf ) == true ){
                PTEST_PRINTF("SocID:\n");
                peritest_dump_print(0, SocIDBuf, (sizeof(uint8_t)*32), 0 );
        }else{
                PTEST_PRINTF("SocID: Err\n");
                return false;
        }

        return true;
}


static int sbrom_cli_secure_asset(int argc, const char **argv)
{
        static uint8_t *AssetData;
#if     (TEST_WKEY_VERIFICATION == 1)
        uint32_t WkeyAssetID;
#endif	//(TEST_WKEY_VERIFICATION == 1)
        uint32_t OutSize;

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        AssetData = (uint8_t *)APP_MALLOC(128);
        OutSize = 64;
        memset(AssetData, 0xFF, 128);

        /* NOTICE !!
         * Asset Provisiong with Kpicv may be rejected due to ICVKeyLock.
         */
        if( R_CC312_Secure_Asset(ASSET_PROV_KEY_TYPE_KPICV, 0x12345678,
                                (uint32_t *)cm_secure_asset, (16*7),
                                (uint32_t *)AssetData, &OutSize ) > 0 )
        {
                PTEST_PRINTF("SecureAsset.CM:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("SecureAsset.CM: Error\n");
        }

        OutSize = 64;
        memset(AssetData, 0xFF, 128);
        /* NOTICE !!
         * Asset Provisiong with Kpicv may be rejected due to ICVKeyLock.
         */
        if( CC_BsvIcvAssetProvisioningOpen(RRQ61X_ACRYPT_BASE,
                                 0x12345678,
                                 (uint32_t *)cm_secure_asset, (16*7),
                                 (uint8_t *)AssetData, (size_t *)&OutSize)== CC_OK )
        {
                PTEST_PRINTF("BsvIcvAssetProvision:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("BsvIcvAssetProvision: Error\n");
        }

        OutSize = 64;
        memset(AssetData, 0xFF, 128);
        if( R_CC312_Secure_Asset(ASSET_PROV_KEY_TYPE_KCP, 0x87654321,
                                (uint32_t *)dm_secure_asset, (16*7),
                                (uint32_t *)AssetData, &OutSize ) > 0 )
        {
                PTEST_PRINTF("SecureAsset.DM:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("SecureAsset.DM: Error\n");
        }

        OutSize = 64;
        memset(AssetData, 0xFF, 128);
        if( CC_BsvOemAssetProvisioningOpen(RRQ61X_ACRYPT_BASE,
                                 0x87654321,
                                 (uint32_t *)dm_secure_asset, (16*7),
                                 (uint8_t *)AssetData, (size_t *)&OutSize)== CC_OK )
        {
                PTEST_PRINTF("BsvOemAssetProvision:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("BsvOemAssetProvision: Error\n");
        }

#if     (TEST_WKEY_VERIFICATION == 1)
        WkeyAssetID = *((uint32_t *)cm_wkey_asset);
        PTEST_PRINTF("CM.WkeyAssetID = %08x\n", WkeyAssetID);

        OutSize = 64;
        memset(AssetData, 0xFF, 128);
        /* NOTICE !!
         * Asset Provisiong with Kpicv may be rejected due to ICVKeyLock.
         */
        if( CC_BsvIcvAssetProvisioningOpen(RRQ61X_ACRYPT_BASE,
                                 WkeyAssetID,
                                 (uint32_t *)&(cm_wkey_asset[4]), (16*7),
                                 (uint8_t *)AssetData, (size_t *)&OutSize)== CC_OK )
        {
                PTEST_PRINTF("BsvIcvAssetProvision:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("BsvIcvAssetProvision: Error\n");
        }


        WkeyAssetID = *((uint32_t *)dm_wkey_asset);
        PTEST_PRINTF("DM.WkeyAssetID = %08x\n", WkeyAssetID);

        OutSize = 64;
        memset(AssetData, 0xFF, 128);
        if( CC_BsvOemAssetProvisioningOpen(RRQ61X_ACRYPT_BASE,
                                 WkeyAssetID,
                                 (uint32_t *)&(dm_wkey_asset[4]), (16*7),
                                 (uint8_t *)AssetData, (size_t *)&OutSize)== CC_OK )
        {
                PTEST_PRINTF("BsvOemAssetProvision:\n");
                peritest_dump_print(0, AssetData, OutSize, 0 );
        }
        else
        {
                PTEST_PRINTF("BsvOemAssetProvision: Error\n");
        }
#endif //(TEST_WKEY_VERIFICATION == 1)

        APP_FREE(AssetData);

        return true;
}

static int sbrom_cli_trng_test(int argc, const char **argv)
{
        uint32_t icnt;
        uint8_t *RndBuff;

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        RndBuff = (uint8_t *)APP_MALLOC(128);

        for( icnt = 0; icnt < 20; icnt ++ ){
                PTEST_PRINTF("xTaskGetTickCount:%ld\n", xTaskGetTickCount());

                R_CC312_Crypto_TRNG(icnt, 128, RndBuff);

                peritest_dump_print(0, RndBuff, 128, 0);
        }

        APP_FREE(RndBuff);

        return true;
}

typedef struct {                          /* Retention memory for Booter */
      uint32_t tin_wakeup_source;
      uint32_t tin_tim_app_address;
      uint32_t tin_gpio_app_address;
      uint32_t tin_sensor_app_address;

      uint32_t tin_fw_active_address;
      uint32_t tin_fw_update_address;
      uint32_t tin_fw_running_address;
      uint32_t tin_fw_fast_boot_option;

      uint32_t tin_otp_state;

      uint32_t tin_spi_pin_map;          /* SPI boot pin map SPI CS SPI CLK SPI MOSI, SPI MISO*/
      uint32_t tin_spi_config;           /* It include start pin and wait time and mode */

      uint32_t tin_sdio_pad;
      uint32_t tin_sdio_config;
      uint32_t tin_sdio_id;
      uint32_t tin_sdio_ocr;

      uint32_t tin_i2c_pin_map;
      uint32_t tin_i2c_config;

      uint32_t tin_uart_config;         /* uart boot config */
      uint32_t tin_life_cycle;

      uint32_t tin_sdemmc_pad;
      uint32_t tin_sdemmc_config;
      uint32_t tin_sdemmc_ph_loc;

} RETENTION_MEM_type;


static int sbrom_cli_bootflag_dump(int argc, const char **argv)
{
        volatile RETENTION_MEM_type *bootflag;
        uint8_t *RBuff;

        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);

        RBuff = (uint8_t *)APP_MALLOC(256);
        bootflag = (volatile RETENTION_MEM_type *)dg_configBOOTER_RTM_ADDR;

        #define PRINTF_INFO(x)         \
        PTEST_PRINTF("%-32s [%p] %08x\n", #x, &(bootflag->x), (unsigned int)(bootflag->x))

        PRINTF_INFO(tin_wakeup_source);
        PRINTF_INFO(tin_tim_app_address);
        PRINTF_INFO(tin_gpio_app_address);
        PRINTF_INFO(tin_sensor_app_address);

        PRINTF_INFO(tin_fw_active_address);
        PRINTF_INFO(tin_fw_update_address);
        PRINTF_INFO(tin_fw_running_address);
        PRINTF_INFO(tin_fw_fast_boot_option);

        PRINTF_INFO(tin_otp_state);

        PRINTF_INFO(tin_uart_config);
        PRINTF_INFO(tin_life_cycle);

        APP_FREE(RBuff);

        return true;
}



/////////////////////////////////////////////////////////////////////////////////////////
//  thread_sbrom ( )
/////////////////////////////////////////////////////////////////////////////////////////


static uint64_t run_sbrom_config = (uint64_t)0x0000FFFFFFFFFFFFuL; // SW Full Test

static bool config_sbrom(int argc, const char **argv)
{
        if( (argc == 2) && (strcmp("set", argv[0]) == 0) ){

                uint32_t idxnum;
                uint64_t test_condition;

                if(strcmp("all", argv[1]) == 0){
                        test_condition  = (uint64_t)0x0000FFFFFFFFFFFFuL; // SW Full Test
                }else{
                        idxnum = (uint32_t) atoi(argv[1]);
                        test_condition  = ((uint64_t)1uL) << idxnum ;
                }
                run_sbrom_config = run_sbrom_config | test_condition;

        }else if( (argc == 2) && (strcmp("clear", argv[0]) == 0) ){

                uint32_t idxnum;
                uint64_t test_condition;

                if(strcmp("all", argv[1]) == 0){
                        test_condition  = (uint64_t)0x0000FFFFFFFFFFFFuL; // SW Full Test
                }else{
                        idxnum = (uint32_t) atoi(argv[1]);
                        test_condition  = ((uint64_t)1uL) << idxnum ;
                }
                run_sbrom_config = run_sbrom_config & (~test_condition);
        }

	PTEST_PRINTF("current-sbrom-config: %x.%x\n"
                , (unsigned int)(run_sbrom_config>>32)
                ,(unsigned int) (run_sbrom_config&((((uint64_t)1uL)<<32)-1))
              );

	return true;
}

static bool thread_sbrom(int argc, const char **argv)
{
        uint32_t idxnum, iternum;
	uint64_t returnval = 0, test_condition;
	static char *funcnamelist[64];
	const int argidx = 1;

        // printf("argc : %d, argv[%d]: %s\n", argc, argidx, argv[argidx]);

	if( (argc >= 2) && (argc <= 3)  ){
                if(strcmp(argv[argidx],"dbg") == 0 ){
                        idxnum = 0x0FFFFFFEuL; // except "secure"
                } else
                if(strcmp(argv[argidx],"secure") == 0 ){
                        idxnum = (0);
                } else
                if(strcmp(argv[argidx],"otp") == 0 ){
                        idxnum = (20);
                } else
                if(strcmp(argv[argidx],"ao") == 0 ){
                        idxnum = (21);
                } else
                if(strcmp(argv[argidx],"socid") == 0 ){
                        idxnum = (22);
                } else
                if(strcmp(argv[argidx],"asset") == 0 ){
                        idxnum = (23);
                } else
                if(strcmp(argv[argidx],"trng") == 0 ){
                        idxnum = (24);
                } else {
                        idxnum = (uint32_t) atoi(argv[argidx]);
                }
	}else{
			idxnum = 0x0FFFFFFEuL; // except "secure"
	}

	if( argc == 3 ){
			iternum = (uint32_t) atoi(argv[argidx+1]);
	}else{
			iternum = 1;
	}

        if( idxnum == 0 ) {
#if (SUPPORT_SECURE_PRODUCTION == 1)
                SBROM_TEST_LOG(
                        "\n *=== SECURE PRODUCTION ===========================*\n"
                        " * While running this,\n"
                        " * the secret keys will be registered in Secure OTP.\n"
                        " * Please note that these keys are irrevocable\n"
                        " *   and can only be removed when you run RMA.\n"
                        " *=== SECURE PRODUCTION ===========================*\n\n"
                );
#endif //(SUPPORT_SECURE_PRODUCTION == 1)
        }

	if( idxnum == 0x0FFFFFFEuL ){
			test_condition  = run_sbrom_config & (~(1ULL<<0));
	}else{
			PTEST_PRINTF("Selected Item[%ld]\n", idxnum);
			test_condition  = ((uint64_t)1uL) << idxnum ;
	}

	// Select Test Suite ======================================

	memset( funcnamelist, 0, (32*sizeof(char *)) );

        while((iternum--) > 0 ){
                #define	TEST_SBROM_SCENARIO( num, func	)		                \
        		funcnamelist[num] = # func;			                \
        		if( (test_condition & (((uint64_t)1uL)<<num)) != 0 ) { 	\
                                R_BSP_SoftwareDelay(10, BSP_DELAY_UNITS_MILLISECONDS);                                        \
        			printf( "[%d][%s]\n", num, funcnamelist[num] );        \
        			if( ( func ( argc, argv ) != true ) ){	                \
        				returnval |= (((uint64_t)1uL)<<num);	        \
        			}					                \
        		}else{						                \
        			if(argc < 0) {                                         \
                                        printf( "[%d][%s] skip\n", num, funcnamelist[num] );\
        			}                                                       \
        		}

                TEST_SBROM_SCENARIO(  0, thread_peripheral_sbrom );

                TEST_SBROM_SCENARIO( 20, sbrom_cli_otp_dump );
                TEST_SBROM_SCENARIO( 21, sbrom_cli_ao_dump );
                TEST_SBROM_SCENARIO( 22, sbrom_cli_socid_dump );
                TEST_SBROM_SCENARIO( 23, sbrom_cli_secure_asset );
                TEST_SBROM_SCENARIO( 24, sbrom_cli_trng_test );

                TEST_SBROM_SCENARIO( 30, sbrom_cli_bootflag_dump );


        	if( returnval != 0 ){
        		uint32_t idx;
        		printf("sbrom-Rslt: %08x.%08x\n"
                                , (unsigned int)(returnval>>32)
                                ,(unsigned int) (returnval&((((uint64_t)1uL)<<32)-1)) );

        		for(idx = 0; idx < 32; idx++ ){
        			if( ((((uint32_t)1uL)<<(idx)) & returnval) != 0 ){
        				printf("\tFailed - [%d][%s]\n", (int)idx, funcnamelist[idx]);
        			}
        		}
        		for(idx = 32; idx < 64; idx++ ){
        			if( ((((uint32_t)1uL)<<(idx-32)) & (uint32_t)(returnval>>32)) != 0 ){
        				printf("\tFailed - [%d][%s]\n", (int)idx, funcnamelist[idx]);
        			}
        		}
        	}else{
        	        printf("sbrom-Rslt: GOOD\n");
        	}
        }

	return (returnval == 0) ? true : false;

}

static const debug_handler_t sbrom_handlers[] = {
    {"run", " [dbg|secure|otp|ao|socid|asset|trng|suite-num] [iter]", thread_sbrom},
    {"view", "view config", config_sbrom},
    {"set", "set config", config_sbrom},
    {"clear", "clear config", config_sbrom},
    {NULL},
};
#endif
bool sbrom_command(int argc, const char *argv[], void *user_data)
{
#if CFG_MBEDTLS
    RA6W1_UNUSED_ARG(user_data);
    return debug_handle_message(argc, argv, sbrom_handlers);
#else
        RA6W1_UNUSED_ARG(argc);
        RA6W1_UNUSED_ARG(argv);
        RA6W1_UNUSED_ARG(user_data);
        printf("MBEDTLS is not enabled. SBROM commands are unavailable.\n");
        return false;
#endif
}
