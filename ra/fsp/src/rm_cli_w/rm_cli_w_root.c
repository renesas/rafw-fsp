/**
 ****************************************************************************************
 *
 * @file rm_cli_w_root.c
 *
 * @brief Console command functions for root
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
#include "rm_wifi_helper.h"
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "sdk_defs.h"
#include "bsp_dump_mem.h"
#include "sys_clock_mgr.h"
#if CFG_WIFI
#include "net_network_main.h"
#endif
#if defined (__SUPPORT_MQTT__)
#include "mqtt_client.h"
#endif
#include "rm_cert.h"
#include "fw_version.h"
#include "os.h"

#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#include "FreeRTOS.h"

#include "rm_cli_w.h"
#if defined (__SUPPORT_OTA__)
#include "ota_update.h"
#include "ota_update_common.h"
#include "ra6w1_image.h"
#endif // (__SUPPORT_OTA__)
#if CFG_WIFI
#include "rm_wifi.h"
#endif
#include "rm_cli_w_net.h"
#include "common_def.h"
#include "rm_vee_flash_w_rrq_nvram.h"

#if CFG_PMGR
#include "rm_pmgr_w_rtm_internal.h"
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#ifdef __SUPPORT_EASY_SETUP__
#include "rm_cli_w_easysetup.h"
#endif
#include "r_rtc_w.h"

#if (SUPPORT_FSP_RM_OTA_W == 1)
#include "rm_ota_w.h"
#endif /* SUPPORT_FSP_RM_OTA_W */

#if defined (__SUPPORT_USR_NVRAM__)
#include "api_usr_nvram.h"
#endif

// Use for dump memory
#define CYAN_COLOR  "\33[1;36m"
#define RED_COLOR   "\33[1;31m"
#define CLEAR_COLOR "\33[0m"

#define MEM_BYTE_READ(addr, data)		*data = *((volatile uint8_t *)(addr))
#define MEM_WORD_READ(addr, data)		*data = *((volatile uint16_t *)(addr))
#define MEM_LONG_READ(addr, data)		*data = *((volatile uint32_t *)(addr))
#define MEM_BYTE_WRITE(addr, data)		*((volatile uint8_t *)(addr)) = data
#define MEM_WORD_WRITE(addr, data)		*((volatile uint16_t *)(addr)) = data
#define MEM_LONG_WRITE(addr, data)		*((volatile uint32_t *)(addr)) = data

#define puthexa(x)	( ((x)<10) ? ('0' + (x)) : ('A' + (x) - 10) )
#define	putascii(x) ( ((x)<32) ? ('.') : ( ((x)>126) ? ('.') : (x) ) )
#define CHECK_RANGE(min, max, val, ret_val) if( val < min || val > max ){ return( ret_val ); }

extern bool reset(void);
extern int factory_reset(int reboot_flag);

#if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
#endif /* SUPPORT_FSP_RM_OTA_W */

//-----------------------------------------------------------------------
// Command Functions
//-----------------------------------------------------------------------

/******************************************************************************
 *  cmd_dump_print ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/
static void cmd_dump_print(uint32_t addr, uint8_t *data, uint32_t length, uint32_t endian)
{
    uint32_t  i, h, s, spc, trim, lmask;
    uint8_t  *data8;
    char	*hexadump;
    char	*strdump;
    char	hexa;

    data8 = (uint8_t *)data;
    endian = (endian & 0x0f);
    endian = (endian == 0) ? 1 : endian ;

    lmask = (endian == 4) ? 0x03 : ((endian == 2) ? 0x01 : 0 );
    lmask = ~lmask;

    hexadump = (char *)pvPortMalloc(52);
    strdump = (char *)pvPortMalloc(20);

    for (i = 0, h = 0, s = 0; i < length; i++) {
        trim = i & (~lmask);
        hexa = data8[(i & lmask) | (~(trim | lmask))] ;

        hexadump[h++] = puthexa( ((hexa >> 4) & 0x0f) );
        hexadump[h++] = puthexa( ((hexa) & 0x0f) );
        if ( trim == (endian - 1) ) {
            hexadump[h++] = ' ';
        }

        strdump[s++] = putascii( hexa );

        if ( ((i % 16) == 15) || ((i + 1) == length) ) {
            hexadump[h] = '\0';
            strdump[s] = '\0';

            printf("[%08lX] : ", (addr + i - (i % 16)) );
            printf(hexadump);
            for ( spc = (17 - (i % 16)); spc > 0; spc-- ) {
                printf("   ");
            }
            printf("%s", strdump);
            printf("\n");
            h = 0;
            s = 0;
        }
    }

    vPortFree(hexadump);
    vPortFree(strdump);
}


/******************************************************************************
 *  cmd_mem_read ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_read(int argc, char *argv[])
{
    uint32_t  addr;
    uint32_t  length;
    uint8_t   data[16];
    int i, t;

    if (argc == 2) {
        addr = htoi(argv[1]);
        MEM_BYTE_READ(addr, &(data[0]));

        printf("[0x%08lX] : 0x%02X\n", addr, data[0]);
    } else if (argc == 3) {
        addr = htoi(argv[1]);
        length = htoi(argv[2]);

        for (i = 0; i < (int) length; i++) {
            t = (i % 16);
            MEM_BYTE_READ((addr + i), &(data[t]));

            if ( t == 15 || (i + 1) == (int) length ) {
                cmd_dump_print((addr + i - t), data , ((t + 1)*sizeof(uint8_t)), sizeof(uint8_t) );
            }
        }
        printf("\n");
    }

    return true;
}

/******************************************************************************
 *  cmd_mem_write ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_write(int argc, char *argv[])
{
    uint32_t  dest_addr;
    uint8_t   dest_data;
    uint32_t  length;
    int  i;

    if (argc == 3) {
        dest_addr = htoi(argv[1]);
        dest_data = htoi(argv[2]);

        MEM_BYTE_WRITE(dest_addr, dest_data);
    } else if (argc == 4) {
        dest_addr = htoi(argv[1]);
        dest_data = htoi(argv[2]);
        length    = htoi(argv[3]);

        for (i = 0; i < (int) length; i++) {
            MEM_BYTE_WRITE(dest_addr + i, dest_data);
        }
    }

    return true;
}

/******************************************************************************
 *  cmd_mem_wread ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_wread(int argc, char *argv[])
{
    uint32_t length;
    uint32_t dest_addr, target_addr;
    uint16_t data[16];
    int i, t;

    if (argc == 2) {
        dest_addr = htoi(argv[1]);
        MEM_WORD_READ(dest_addr, &(data[0]));
        printf("[0x%08lX] : 0x%04X\n", dest_addr, data[0]);
    } else if (argc == 3) {
        dest_addr = htoi(argv[1]);
        length    = htoi(argv[2]);

        length = length >> 1;
        for (i = 0; i < (int) length; i++) {
            t = i % 16;
            target_addr = dest_addr + (i << 1);
            MEM_WORD_READ(target_addr, &(data[t]));

            if ( t == 15 || (i + 1) == (int) length ) {
                cmd_dump_print((dest_addr + (i << 1) - (t << 1)), (uint8_t *)data
                               , ((t + 1)*sizeof(uint16_t)), sizeof(uint16_t) );
            }
        }
        printf("\n");
    }

    return true;
}

/******************************************************************************
 *  cmd_mem_wwrite ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_wwrite(int argc, char *argv[])
{
    uint32_t  dest_addr;
    uint16_t  dest_data;
    uint32_t  length;
    int  i;

    if (argc == 3) {
        dest_addr = htoi(argv[1]) & 0xfffffffe;
        dest_data = htoi(argv[2]);

        MEM_WORD_WRITE(dest_addr, dest_data);
    } else if (argc == 4) {
        dest_addr = htoi(argv[1]) & 0xfffffffe;
        dest_data = htoi(argv[2]);
        length    = htoi(argv[3]);

        for (i = 0; i < (int) length; i += 2) {
            MEM_WORD_WRITE(dest_addr + i, dest_data);
        }
    }

    return true;
}


/******************************************************************************
 *  cmd_mem_lread ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_lread(int argc, char *argv[])
{
    uint32_t  length;
    uint32_t  dest_addr, target_addr;
    uint32_t  data[16];
    int i, t;

    if (argc == 2) {
        dest_addr = htoi(argv[1]);
        MEM_LONG_READ(dest_addr, &(data[0]));
        printf("[0x%08lX] : 0x%08lX\n", dest_addr, data[0]);
    } else if (argc == 3) {
        dest_addr = htoi(argv[1]);
        length    = htoi(argv[2]);

        length = length >> 2;
        for (i = 0; i < (int) length; i++) {
            t = i % 16;
            target_addr = dest_addr + (i << 2);
            MEM_LONG_READ(target_addr, &(data[t]));

            if ( t == 15 || (i + 1) == (int) length ) {
                cmd_dump_print((dest_addr + (i << 2) - (t << 2)), (uint8_t *)data
                               , ((t + 1)*sizeof(uint32_t)), sizeof(uint32_t) );
            }
        }
        printf("\n");
    }

    return true;
}

/******************************************************************************
 *  cmd_mem_lwrite ( )
 *
 *  Purpose :
 *  Input   :
 *  Output  :
 *  Return  :
 ******************************************************************************/

static bool cmd_mem_lwrite(int argc, char *argv[])
{
    uint32_t  dest_addr;
    uint32_t  dest_data;
    uint32_t  length;
    int  i;

    if (argc == 3) {
        dest_addr = htoi(argv[1]) & 0xfffffffc;
        dest_data = htoi(argv[2]);

        MEM_LONG_WRITE(dest_addr, dest_data);
    } else if (argc == 4) {
        dest_addr = htoi(argv[1]) & 0xfffffffc;
        dest_data = htoi(argv[2]);
        length    = htoi(argv[3]);

        for (i = 0; i < (int) length; i += 4) {
            MEM_LONG_WRITE(dest_addr + i, dest_data);
        }
    }

    return true;
}

void print_version(void)
{
    char fw_ver_str[64] = {0};

#if (SUPPORT_FSP_RM_OTA_W == 1)
    rm_ota_w_image_header_data_t infoImage;
    uint32_t addr = 0x00;
    uint8_t boot_idx = 0;
#endif /* SUPPORT_FSP_RM_OTA_W */

    /* Set system features for RRQ6x SDK */
    BSP_DisplayOopsDump(RED_COLOR, CYAN_COLOR, CLEAR_COLOR);
    vTaskDelay(portCONVERT_MS_2_TICKS(100));
    BSP_InitOopsData();

 #if (dg_configUSE_TRACE_FOR_DEBUG == 1 && CFG_CLI)
    print_function_trace();
    clear_function_trace();
 #endif	// dg_configUSE_TRACE_FOR_DEBUG

#if (SUPPORT_FSP_RM_OTA_W == 1)
    p_ota_instance->p_api->bootIdxGet(p_ota_instance->p_ctrl, &boot_idx);
#endif /* (SUPPORT_FSP_RM_OTA_W) */

    sprintf(fw_ver_str, "%s%s%s", CHIPSET_NAME, "-", FIRMWARE_VERSION);

    printf("\n\t******************************************************\n"
           "\t*              %s SDK Information\n"
           "\t* ---------------------------------------------------\n"
           "\t*\n", CHIPSET_NAME);
    printf("\t* - CHIP Name       : %s (D%c%c%c%c%c)\n",
                                   CHIPSET_NAME,
                                   (char)CHIP_VERSION->CHIP_ID1_REG,
                                   (char)CHIP_VERSION->CHIP_ID2_REG,
                                   (char)CHIP_VERSION->CHIP_ID3_REG,
                                   (char)CHIP_VERSION->CHIP_ID4_REG,
                                   (char)CHIP_VERSION->CHIP_REVISION_REG);
#if CFG_WIFI
    printf("\t* - SKU Type        : 0x%lx, %s\n", rm_wifi_otp_sku_id_get(), rm_wifi_otp_sku_id_get_str());
#endif
#if (BSP_CFG_RADIO_CLOCK_MGR_ENABLE == 1) && defined (CFG_RTC_W)
    printf("\t* - CPU Type        : Cortex-M33 ");

    if ((int)cm_cpu_clk_get() == 2) {
        printf("(2.5MHz)\n");
    } else {
        printf("(%dMHz)\n", (int)cm_cpu_clk_get());
    }
#endif
    printf("\t* - Kernel Version  : FreeRTOS %s\n", tskKERNEL_VERSION_NUMBER);
#if defined ( RENESAS_AT25SL_8MB_OTA ) || defined ( RENESAS_AT25SL_8MB )
    printf("\t* - SFLASH Type     : 8 MB (Renesas AT25SL)\n");
#elif defined ( NORMAL_4MB_OTA ) || defined ( NORMAL_4MB )
    printf("\t* - SFLASH Type     : 4 MB\n");
#elif defined ( NORMAL_8MB_OTA ) || defined ( NORMAL_8MB )
    printf("\t* - SFLASH Type     : 8 MB\n");
#endif //

#ifdef SIGMA_TEST_ENABLE
    printf("\t* - SDK Version     : V%s\n", SIGMA_SDK_VERSION);
#else
    printf("\t* - SDK Version     : V%d.%d.%d.%d.%d\n", SDK_VER_PRODUCT_LINE, SDK_VER_MODE, SDK_VER_TARGET, SDK_VER_BRANCH, SDK_VER_R);
#endif

#if (SUPPORT_FSP_RM_OTA_W == 1)
    if (p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_CURRENT_ADDR, RM_OTA_W_TYPE_RTOS, (uint32_t *) &addr) == 0)
    {
        if (p_ota_instance->p_api->getImageInfo(p_ota_instance->p_ctrl, RM_OTA_W_TYPE_RTOS, addr, &infoImage) == 0)
        {
            if ((strcmp("RRQ61000-1.0.0", (char *) infoImage.name)))
            {
                memset(fw_ver_str, 0, sizeof(fw_ver_str));
                sprintf(fw_ver_str, "%s", infoImage.name);
            }
        }
    }
#endif /* SUPPORT_FSP_RM_OTA_W */

    printf("\t* - F/W Version     : %s\n", fw_ver_str);
    printf("\t* - FSP Version     : %s\n", FSP_VERSION_STRING);

#if (SUPPORT_FSP_RM_OTA_W == 1)
    PRINTF("\t* - Boot Index      : %u\n", boot_idx);
#endif /* (SUPPORT_FSP_RM_OTA_W) */

#ifndef SKIP_FW_BUILD_TIMESTAMP_PRINT
    printf("\t* - F/W Build Time  : %s %s\n", __DATE__, __TIME__);
#endif /* SKIP_FW_BUILD_TIMESTAMP_PRINT */
#if CFG_WIFI
    if (rm_wifi_otp_sku_id_get() != TIN_SKU_BUILD_ID) {
        uint32_t sku_id_otp = rm_wifi_otp_sku_id_get();

        if (sku_id_otp != TIN_SKU_WIFI6_B24_5 && sku_id_otp != TIN_SKU_WIFI6_B24_5_BLE) {
            if ((sku_id_otp == TIN_SKU_WIFI4_B24 && sku_id_otp != TIN_SKU_BUILD_ID)          ||  
                (sku_id_otp == TIN_SKU_WIFI6_B24 && (TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5 || 
                                                     TIN_SKU_BUILD_ID == TIN_SKU_WIFI6_B24_5_BLE))) {
                printf(RED_COLOR "\t* Wrong SDK Configuration for Actual SKU Type !! \n" CLEAR_COLOR);
            }
        }        
    }
#endif
    printf("\t*\n");
    printf("\t******************************************************\n\n");
}

static bool cmd_ver(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    print_version();
    return true;
}

#if CFG_WIFI
extern int getStr(char *get_data, int get_len);
static bool cmd_factory_reset(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    bool ret = pdTRUE;

#ifdef __SUPPORT_APP_CONSOLE_INPUT__
    char input_str[2];

    printf("FACTORY RESET [N/y/?] ");
    getStr(input_str, 1);

    if (toupper(input_str[0]) != 'Y') {
        printf("\nCancel\n");
    } else
#endif /* __SUPPORT_APP_CONSOLE_INPUT__ */
    {
        printf("\n" ANSI_COLOR_LIGHT_RED "Start Factory-Reset ...\n" ANSI_COLOR_DEFULT);

#if defined (__SUPPORT_EASY_SETUP__) && defined (CFG_WIFI)
        easy_setup_suspend();
#endif /* __SUPPORT_EASY_SETUP__ */

#if defined (__SUPPORT_USR_NVRAM__)
        api_usr_nvram_bank_reset(0);
        api_usr_nvram_bank_reset(1);
        api_usr_nvram_bank_reset(2);
#endif

        if (factory_reset(1) != true) {
            printf(ANSI_COLOR_RED "Error\n" ANSI_COLOR_DEFULT);
            ret = pdFAIL;
        }
    }

    return ret;
}
#endif
# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)

static bool cmd_reboot(int argc, char *argv[])
{
    if (strcmp("reset_hard", argv[0]) == 0) {
        por_reset();
    } else {
        if (argc == 2) {
            if (   argv[1][1] == '\0'
                && (argv[1][0] == '0' || argv[1][0] == '1')) {
                reset();
                return pdTRUE;
            } else {
                return pdFALSE;
            }
        } else if (argc == 1) {
            reset();
            return pdFALSE;
        }
    }

    return pdTRUE;
}
#endif

# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static void display_sflash_map(void)
{
#if defined ( RENESAS_AT25SL_8MB_OTA )    // ====================================

    printf("\n");
    printf(" --- SFLASH map (AT25SL 8MB) w/ OTA -----------------------\n");

    printf("\n");
    printf("    PRODUCT_HDR         = 0x%08x (%d KB)\n", SF_PRODUCT_HDR, SF_PRODUCT_HDR_SIZE/1024);
    printf("    PRODUCT_HDR_BACKUP  = 0x%08x (%d KB)\n", SF_PRODUCT_HDR_BACKUP, SF_PRODUCT_HDR_SIZE/1024);
    printf("\n");
    printf("    RTOS_0              = 0x%08x (%d KB)\n", SF_RTOS_0, SF_RTOS_SIZE/1024);
    printf("\n");

    printf(" .. 3 MB ...........................................\n");
    printf("\n");
#if (0)
    printf("    NVRAM_AREA          = 0x%08x (%d KB)\n", SF_NVRAM_AREA, SF_NVRAM_SIZE/1024);
    printf("    NVRAM_BACKUP_AREA   = 0x%08x (%d KB)\n", SF_NVRAM_BACKUP_AREA, (SF_NVRAM_SIZE*(AD_NVMS_VES_MULTIPLIER-1))/1024);
#endif
    printf("\n");

    if (CERT_WPA_ENT_USED == 1)
        printf("    TLS_CERT_WPA_ENT    = 0x%08x (%d KB)\n", SF_TLS_CERT_WPA_ENT,   (SF_TLS_CERT_OTA       - SF_TLS_CERT_WPA_ENT)/1024);
    if (CERT_OTA_USED == 1)
        printf("    TLS_CERT_OTA        = 0x%08x (%d KB)\n", SF_TLS_CERT_OTA,       (SF_TLS_CERT_HTTPS_CLI - SF_TLS_CERT_OTA)/1024);
    if (CERT_HTTPS_CLI_USED == 1)
        printf("    TLS_CERT_HTTPS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_CLI, (SF_TLS_CERT_HTTPS_SVR - SF_TLS_CERT_HTTPS_CLI)/1024);
    if (CERT_HTTPS_SVR_USED == 1)
        printf("    TLS_CERT_HTTPS_SVR  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_SVR, (SF_TLS_CERT_MQTTS_CLI - SF_TLS_CERT_HTTPS_SVR)/1024);
    if (CERT_MQTTS_CLI_USED == 1)
        printf("    TLS_CERT_MQTTS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_MQTTS_CLI, (SF_TLS_CERT_ATCMD     - SF_TLS_CERT_MQTTS_CLI)/1024);
    if (CERT_ATCMD_USED == 1)
        printf("    TLS_CERT_ATCMD      = 0x%08x (%d KB)\n", SF_TLS_CERT_ATCMD,     (SF_TLS_CERT_AWS       - SF_TLS_CERT_ATCMD)/1024);
    if (CERT_AWS_USED == 1)
        printf("    TLS_CERT_AWS        = 0x%08x (%d KB)\n", SF_TLS_CERT_AWS,       (SF_TLS_CERT_MATTER    - SF_TLS_CERT_AWS)/1024);
    if (CERT_MATTER_USED == 1)
        printf("    TLS_CERT_MATTER     = 0x%08x (%d KB)\n", SF_TLS_CERT_MATTER,    (SF_TLS_CERT_MISC1     - SF_TLS_CERT_MATTER)/1024);
    if (CERT_MISC1_USED == 1)
        printf("    TLS_CERT_MISC1      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC1,     (SF_TLS_CERT_MISC2     - SF_TLS_CERT_MISC1)/1024);
    if (CERT_MISC2_USED == 1)
        printf("    TLS_CERT_MISC2      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC2,     (SF_TLS_CERT_MISC3     - SF_TLS_CERT_MISC2)/1024);
    if (CERT_MISC3_USED == 1)
        printf("    TLS_CERT_MISC3      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC3,     (SF_TLS_CERT_MISC4     - SF_TLS_CERT_MISC3)/1024);
    if (CERT_MISC4_USED == 1)
        printf("    TLS_CERT_MISC4      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC4,     (SF_TLS_CERT_MISC5     - SF_TLS_CERT_MISC4)/1024);
    if (CERT_MISC5_USED == 1)
        printf("    TLS_CERT_MISC5      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC5,     (SF_TLS_CERT_MISC6     - SF_TLS_CERT_MISC5)/1024);
    if (CERT_MISC6_USED == 1)
        printf("    TLS_CERT_MISC6      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC6,     (SF_TLS_CERT_MISC7     - SF_TLS_CERT_MISC6)/1024);
    if (CERT_MISC7_USED == 1)
        printf("    TLS_CERT_MISC7      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC7,     (SF_TLS_CERT_MISC8     - SF_TLS_CERT_MISC7)/1024);
    if (CERT_MISC8_USED == 1)
        printf("    TLS_CERT_MISC8      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC8,     (SF_SYS_TLS_ADD_CERT_SIZE)/1024);

    printf("\n");
    printf(" .. 4 MB ...........................................\n");
    printf("\n");
    printf("    RTOS_1              = 0x%08x (%d KB)\n", SF_RTOS_1, SF_RTOS_SIZE/1024);
    printf("\n");
    printf(" ...7 MB ...........................................\n");
    printf("\n");
    printf("    USER_AREA           = 0x%08x (%d KB)\n", SF_USER_AREA, SF_USER_AREA_SIZE/1024);
    printf("    PARTITION_TABLE     = 0x%08x (%d KB)\n", SF_PARTITION_TBL, SF_PARTITION_TBL_SIZE/1024);
    printf("\n");
    printf(" ...8 MB ...........................................\n");

#elif defined ( RENESAS_AT25SL_8MB )    // ====================================

    printf("\n");
    printf(" --- SFLASH map (AT25SL 8MB) w/o OTA -----------------------\n");

    printf("\n");
    printf("    PRODUCT_HDR         = 0x%08x (%d KB)\n", SF_PRODUCT_HDR, SF_PRODUCT_HDR_SIZE/1024);
    printf("    PRODUCT_HDR_BACKUP  = 0x%08x (%d KB)\n", SF_PRODUCT_HDR_BACKUP, SF_PRODUCT_HDR_SIZE/1024);
    printf("\n");
    printf("    RTOS_0              = 0x%08x (%d KB)\n", SF_RTOS_0, SF_RTOS_SIZE/1024);
    printf("\n");
    printf(" .. 3 MB ...........................................\n");
    printf("\n");
    printf("    NVRAM_AREA          = 0x%08x (%d KB)\n", SF_NVRAM_AREA, SF_NVRAM_SIZE/1024);
    printf("    NVRAM_BACKUP_AREA   = 0x%08x (%d KB)\n", SF_NVRAM_BACKUP_AREA, SF_NVRAM_SIZE/1024);
    printf("\n");

    if (CERT_WPA_ENT_USED == 1)
        printf("    TLS_CERT_WPA_ENT    = 0x%08x (%d KB)\n", SF_TLS_CERT_WPA_ENT,   (SF_TLS_CERT_OTA       - SF_TLS_CERT_WPA_ENT)/1024);
    if (CERT_OTA_USED == 1)
        printf("    TLS_CERT_OTA        = 0x%08x (%d KB)\n", SF_TLS_CERT_OTA,       (SF_TLS_CERT_HTTPS_CLI - SF_TLS_CERT_OTA)/1024);
    if (CERT_HTTPS_CLI_USED == 1)
        printf("    TLS_CERT_HTTPS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_CLI, (SF_TLS_CERT_HTTPS_SVR - SF_TLS_CERT_HTTPS_CLI)/1024);
    if (CERT_HTTPS_SVR_USED == 1)
        printf("    TLS_CERT_HTTPS_SVR  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_SVR, (SF_TLS_CERT_MQTTS_CLI - SF_TLS_CERT_HTTPS_SVR)/1024);
    if (CERT_MQTTS_CLI_USED == 1)
        printf("    TLS_CERT_MQTTS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_MQTTS_CLI, (SF_TLS_CERT_ATCMD     - SF_TLS_CERT_MQTTS_CLI)/1024);
    if (CERT_ATCMD_USED == 1)
        printf("    TLS_CERT_ATCMD      = 0x%08x (%d KB)\n", SF_TLS_CERT_ATCMD,     (SF_USER_AREA          - SF_TLS_CERT_ATCMD)/1024);
    
    printf("\n");
    printf(" .. 4 MB ...........................................\n");
    printf("\n");
    printf("    USER_AREA           = 0x%08x (%d KB)\n", SF_USER_AREA, SF_USER_AREA_SIZE/1024);
    printf("    PARTITION_TABLE     = 0x%08x (%d KB)\n", SF_PARTITION_TBL, SF_PARTITION_TBL_SIZE/1024);
    printf("\n");
    printf(" .. 8 MB ...........................................\n");

#elif defined ( NORMAL_4MB_OTA ) // ===================================================

    printf("\n");
    printf(" --- SFLASH map (4MB) w/ OTA -----------------------\n");

    printf("\n");
    printf("    PRODUCT_HDR         = 0x%08x (%d KB)\n", SF_PRODUCT_HDR, SF_PRODUCT_HDR_SIZE/1024);
    printf("    PRODUCT_HDR_BACKUP  = 0x%08x (%d KB)\n", SF_PRODUCT_HDR_BACKUP, SF_PRODUCT_HDR_SIZE/1024);
    printf("    RTOS_0              = 0x%08x (%d KB)\n", SF_RTOS_0, SF_RTOS_SIZE/1024);
    printf("\n");

    if (CERT_WPA_ENT_USED == 1)
        printf("    TLS_CERT_WPA_ENT    = 0x%08x (%d KB)\n", SF_TLS_CERT_WPA_ENT,   (SF_TLS_CERT_OTA       - SF_TLS_CERT_WPA_ENT)/1024);
    if (CERT_OTA_USED == 1)
        printf("    TLS_CERT_OTA        = 0x%08x (%d KB)\n", SF_TLS_CERT_OTA,       (SF_TLS_CERT_HTTPS_CLI - SF_TLS_CERT_OTA)/1024);
    if (CERT_HTTPS_CLI_USED == 1)
        printf("    TLS_CERT_HTTPS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_CLI, (SF_TLS_CERT_HTTPS_SVR - SF_TLS_CERT_HTTPS_CLI)/1024);
    if (CERT_HTTPS_SVR_USED == 1)
        printf("    TLS_CERT_HTTPS_SVR  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_SVR, (SF_TLS_CERT_MQTTS_CLI - SF_TLS_CERT_HTTPS_SVR)/1024);
    if (CERT_MQTTS_CLI_USED == 1)
        printf("    TLS_CERT_MQTTS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_MQTTS_CLI, (SF_TLS_CERT_ATCMD     - SF_TLS_CERT_MQTTS_CLI)/1024);
    if (CERT_ATCMD_USED == 1)
        printf("    TLS_CERT_ATCMD      = 0x%08x (%d KB)\n", SF_TLS_CERT_ATCMD,     (SF_TLS_CERT_AWS       - SF_TLS_CERT_ATCMD)/1024);
    if (CERT_AWS_USED == 1)
        printf("    TLS_CERT_AWS        = 0x%08x (%d KB)\n", SF_TLS_CERT_AWS,       (SF_TLS_CERT_MATTER    - SF_TLS_CERT_AWS)/1024);
    if (CERT_MATTER_USED == 1)
        printf("    TLS_CERT_MATTER     = 0x%08x (%d KB)\n", SF_TLS_CERT_MATTER,    (SF_TLS_CERT_MISC1     - SF_TLS_CERT_MATTER)/1024);
    if (CERT_MISC1_USED == 1)
        printf("    TLS_CERT_MISC1      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC1,     (SF_TLS_CERT_MISC2     - SF_TLS_CERT_MISC1)/1024);
    if (CERT_MISC2_USED == 1)
        printf("    TLS_CERT_MISC2      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC2,     (SF_TLS_CERT_MISC3     - SF_TLS_CERT_MISC2)/1024);
    if (CERT_MISC3_USED == 1)
        printf("    TLS_CERT_MISC3      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC3,     (SF_TLS_CERT_MISC4     - SF_TLS_CERT_MISC3)/1024);
    if (CERT_MISC4_USED == 1)
        printf("    TLS_CERT_MISC4      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC4,     (SF_TLS_CERT_MISC5     - SF_TLS_CERT_MISC4)/1024);
    if (CERT_MISC5_USED == 1)
        printf("    TLS_CERT_MISC5      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC5,     (SF_TLS_CERT_MISC6     - SF_TLS_CERT_MISC5)/1024);
    if (CERT_MISC6_USED == 1)
        printf("    TLS_CERT_MISC6      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC6,     (SF_TLS_CERT_MISC7     - SF_TLS_CERT_MISC6)/1024);
    if (CERT_MISC7_USED == 1)
        printf("    TLS_CERT_MISC7      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC7,     (SF_TLS_CERT_MISC8     - SF_TLS_CERT_MISC7)/1024);
    if (CERT_MISC8_USED == 1)
        printf("    TLS_CERT_MISC8      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC8,     (SF_SYS_TLS_ADD_CERT_SIZE)/1024);

    printf("\n");
    printf("    NVRAM_AREA          = 0x%08x (%d KB)\n", SF_NVRAM_AREA, SF_NVRAM_SIZE/1024);
    printf("    NVRAM_BACKUP_AREA   = 0x%08x (%d KB)\n", SF_NVRAM_BACKUP_AREA, SF_NVRAM_SIZE/1024);

    printf("\n ...................................................\n");
    printf("\n");
    printf("    RTOS_1              = 0x%08x (%d KB)\n", SF_RTOS_1, SF_RTOS_SIZE/1024);
    printf("    USER_AREA_1         = 0x%08x (%d KB)\n", SF_USER_AREA, SF_USER_AREA_SIZE/1024);
    printf("    PARTITION_TABLE     = 0x%08x (%d KB)\n", SF_PARTITION_TBL, SF_PARTITION_TBL_SIZE/1024);
    printf("\n");
    printf(" ---------------------------------------------------\n");

#elif defined ( NORMAL_4MB ) // ===================================================

    printf("\n");
    printf(" --- SFLASH map (4MB) w/o OTA ----------------------\n");

    printf("\n");
    printf("    PRODUCT_HDR         = 0x%08x (%d KB)\n", SF_PRODUCT_HDR, SF_PRODUCT_HDR_SIZE/1024);
    printf("    PRODUCT_HDR_BACKUP  = 0x%08x (%d KB)\n", SF_PRODUCT_HDR_BACKUP, SF_PRODUCT_HDR_SIZE/1024);
    printf("    RTOS_0              = 0x%08x (%d KB)\n", SF_RTOS_0, SF_RTOS_SIZE/1024);
    printf("\n");

    if (CERT_WPA_ENT_USED == 1)
        printf("    TLS_CERT_WPA_ENT    = 0x%08x (%d KB)\n", SF_TLS_CERT_WPA_ENT,   (SF_TLS_CERT_OTA       - SF_TLS_CERT_WPA_ENT)/1024);
    if (CERT_OTA_USED == 1)
        printf("    TLS_CERT_OTA        = 0x%08x (%d KB)\n", SF_TLS_CERT_OTA,       (SF_TLS_CERT_HTTPS_CLI - SF_TLS_CERT_OTA)/1024);
    if (CERT_HTTPS_CLI_USED == 1)
        printf("    TLS_CERT_HTTPS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_CLI, (SF_TLS_CERT_HTTPS_SVR - SF_TLS_CERT_HTTPS_CLI)/1024);
    if (CERT_HTTPS_SVR_USED == 1)
        printf("    TLS_CERT_HTTPS_SVR  = 0x%08x (%d KB)\n", SF_TLS_CERT_HTTPS_SVR, (SF_TLS_CERT_MQTTS_CLI - SF_TLS_CERT_HTTPS_SVR)/1024);
    if (CERT_MQTTS_CLI_USED == 1)
        printf("    TLS_CERT_MQTTS_CLI  = 0x%08x (%d KB)\n", SF_TLS_CERT_MQTTS_CLI, (SF_TLS_CERT_ATCMD     - SF_TLS_CERT_MQTTS_CLI)/1024);
    if (CERT_ATCMD_USED == 1)
    if (CERT_ATCMD_USED == 1)
        printf("    TLS_CERT_ATCMD      = 0x%08x (%d KB)\n", SF_TLS_CERT_ATCMD,     (SF_TLS_CERT_AWS       - SF_TLS_CERT_ATCMD)/1024);
    if (CERT_AWS_USED == 1)
        printf("    TLS_CERT_AWS        = 0x%08x (%d KB)\n", SF_TLS_CERT_AWS,       (SF_TLS_CERT_MATTER    - SF_TLS_CERT_AWS)/1024);
    if (CERT_MATTER_USED == 1)
        printf("    TLS_CERT_MATTER     = 0x%08x (%d KB)\n", SF_TLS_CERT_MATTER,    (SF_TLS_CERT_MISC1     - SF_TLS_CERT_MATTER)/1024);
    if (CERT_MISC1_USED == 1)
        printf("    TLS_CERT_MISC1      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC1,     (SF_TLS_CERT_MISC2     - SF_TLS_CERT_MISC1)/1024);
    if (CERT_MISC2_USED == 1)
        printf("    TLS_CERT_MISC2      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC2,     (SF_TLS_CERT_MISC3     - SF_TLS_CERT_MISC2)/1024);
    if (CERT_MISC3_USED == 1)
        printf("    TLS_CERT_MISC3      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC3,     (SF_TLS_CERT_MISC4     - SF_TLS_CERT_MISC3)/1024);
    if (CERT_MISC4_USED == 1)
        printf("    TLS_CERT_MISC4      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC4,     (SF_TLS_CERT_MISC5     - SF_TLS_CERT_MISC4)/1024);
    if (CERT_MISC5_USED == 1)
        printf("    TLS_CERT_MISC5      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC5,     (SF_TLS_CERT_MISC6     - SF_TLS_CERT_MISC5)/1024);
    if (CERT_MISC6_USED == 1)
        printf("    TLS_CERT_MISC6      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC6,     (SF_TLS_CERT_MISC7     - SF_TLS_CERT_MISC6)/1024);
    if (CERT_MISC7_USED == 1)
        printf("    TLS_CERT_MISC7      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC7,     (SF_TLS_CERT_MISC8     - SF_TLS_CERT_MISC7)/1024);
    if (CERT_MISC8_USED == 1)
        printf("    TLS_CERT_MISC8      = 0x%08x (%d KB)\n", SF_TLS_CERT_MISC8,     (SF_USER_AREA          - SF_TLS_CERT_MISC8)/1024);

    printf("\n");
    printf("    NVRAM_AREA          = 0x%08x (%d KB)\n", SF_NVRAM_AREA, SF_NVRAM_SIZE/1024);
    printf("    NVRAM_BACKUP_AREA   = 0x%08x (%d KB)\n", SF_NVRAM_BACKUP_AREA, SF_NVRAM_SIZE/1024);

    printf("\n ...................................................\n");
    printf("\n");
    printf("    USER_AREA           = 0x%08x (%d KB)\n", SF_USER_AREA, SF_USER_AREA_SIZE/1024);
    printf("    PARTITION_TABLE     = 0x%08x (%d KB)\n", SF_PARTITION_TBL, SF_PARTITION_TBL_SIZE/1024);
    printf("\n");
    printf(" ---------------------------------------------------\n");

#endif // --- ---
}
#endif

# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static void sflash_dump_print(const long addr, const void *buf, size_t len)
{
    size_t i, llen;
    const unsigned char *pos = buf;
    const size_t line_len = 16;
    int hex_index = 0;
    char *buf_prt = NULL;

    buf_prt = pvPortMalloc(64);
    if (buf_prt == NULL) {
        printf("[%s] Failed to allocate the temporary buffer ...\n", __func__);
        return;
    }

    printf(">>> read data ------\n");

    if (buf == NULL) {
        printf(" - hexdump (len=%lu): [NULL]\n", (unsigned long) len);

        vPortFree(buf_prt);

        return;
    }

    printf("- (len=%ld):\n", (unsigned long) len);

    while (len) {
        char tmp_str[4];

        llen = len > line_len ? line_len : len;

        memset(buf_prt, 0, 64);

        sprintf(buf_prt, "[%08lx] ", (addr+hex_index));

        for (i = 0; i < llen; i++) {
            sprintf(tmp_str, " %02x", pos[i]);
            strcat(buf_prt, tmp_str);
        }

        hex_index = hex_index + i;

        for (i = llen; i < line_len; i++) {
            strcat(buf_prt, "   ");  /* _xx */
        }

        printf("%s  ", buf_prt);

        memset(buf_prt, 0, 64);

        for (i = 0; i < llen; i++) {
            if (pos[i] >= 0x20 && pos[i] < 0x7f) {
                sprintf(tmp_str, "%c", pos[i]);
                strcat(buf_prt, tmp_str);
            } else {
                strcat(buf_prt, ".");
            }
        }

        for (i = llen; i < line_len; i++) {
            strcat(buf_prt, " ");
        }

        strcat(buf_prt, "\n");

        printf(buf_prt);

        pos += llen;
        len -= llen;
    }

    vPortFree(buf_prt);
}
#endif

int cli_atoi_custom (char* str)
{
    int res = 0, minus_sign = 0;

    if (str == NULL) {
        return 0;
    }

    for (int i = 0; str[i] != '\0'; ++i) {
        if (i == 0) {
             if (str[i] >= '0' && str[i] <= '9') {
                 res = res * 10 + str[i] - '0';
             } else if (str[i] == '-') {
                 minus_sign = 1;
             } else if (str[i] == '+') {
                 minus_sign = 0;
             } else {
                return 0;
             }
        } else {
            if (str[i] >= '0' && str[i] <= '9') {
                res = res * 10 + str[i] - '0';
            } else {
                return 0;
            }
        }
    }

    return (minus_sign?(res*(-1)):(res));
}

int cli_get_int_val_from_str(char* param, int* int_val, int policy)
{
    int result = -1, param_len, int_val_old;

    if (param == NULL || int_val == NULL) {
        return -1;
    }

    param_len = strlen(param);
    int_val_old = *int_val;

    if (param_len == 1) {
        if (param[0] == '0') {
            // "0" <- non error 0 return
            *int_val = 0;
            result = 0; /* SUCCESS */
        } else {
            // check if valid single digit 1 ~ 9
            *int_val = cli_atoi_custom(param);

            if (*int_val > 0 && *int_val < 10) {
                // valid value: 1~9
                result = 0; /* SUCCESS */
            } else {
                // error: e.g. == 0
                *int_val = int_val_old;
                result = -1;
            }
        }
    } else if (param_len == 0) {
        *int_val = int_val_old;
        result = -1;
    } else {
        // param_len > 1

        if (policy == POL_1) {
            // leading "0" / "+" / "-0" are not allowed
            if (param[0] == '0' || param[0] == '+')
                return -1;

            if (param[0] == '-' && param[1] == '0')
                return -1;
        } else if (policy == POL_2) {
            // leading "+" / "-0" are not allowed
            if (param[0] == '+')
                return -1;

            if (param[0] == '-' && param[1] == '0')
                return -1;
        }

        *int_val = cli_atoi_custom(param);

        if (*int_val > -1 && *int_val < 10) {
            // considered error
            *int_val = int_val_old;
            result = -1;
        } else {
            result = 0; /* SUCCESS */
        }
    }

    return result;
}

int cli_get_int_val_from_date_time_str(char* param, int* int_val)
{
    int param_len;
    char *vaild_digit = "0123456789";

    if (param == NULL || int_val == NULL) {
        return -1;
    }

    param_len = strlen(param);

    if (param_len == 2) {
        if (strchr(vaild_digit, param[0]) == NULL || strchr(vaild_digit, param[1]) == NULL) {
            return -1;
        }
    } else {
        return -1;
    }

    *int_val = cli_atoi_custom(param);

    return 0;
}

int cli_is_date_time_valid(struct tm *t)
{
    /*
        int tm_sec;
        int tm_min;
        int tm_hour;

        int tm_mday;
        int tm_mon;
        int tm_year;
    */

    int month_len;
    int month;

    month = t->tm_mon + 1;
    CHECK_RANGE( 70, 8099, t->tm_year, -2); // 1970 ~ 9999
    CHECK_RANGE( 0, 23,    t->tm_hour, -3);
    CHECK_RANGE( 0, 59,    t->tm_min,  -3);
    CHECK_RANGE( 0, 59,    t->tm_sec,  -3);

    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12:
            month_len = 31;
            break;

        case 4: case 6: case 9: case 11:
            month_len = 30;
            break;

        case 2:
            if ( ( !( t->tm_year % 4 ) && t->tm_year % 100 ) || !( t->tm_year % 400 ) ) {
                month_len = 29;
            } else {
                month_len = 28;
            }
            break;

        default:
            return -2;
    }
    CHECK_RANGE( 1, month_len, t->tm_mday, -2);

    return pdTRUE;
}
# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static bool cmd_sflash(int argc, const char **argv)
{
    if (argc == 1) {
usage :
        printf("sflash [map|read|erase] addr size\n");
        printf("                   - max size 4096 bytes\n");

        return true;
    }

    if ((argc == 2) && (strcmp(argv[1], "map") == 0)) {
        display_sflash_map();
    } else if ((argc >= 4) && (strcmp(argv[1], "read") == 0)) {
        unsigned char *rd_buf;
        unsigned long rd_addr = htoi((char *)argv[2]);
        int rd_size = htoi((char *)argv[3]);

        rd_buf = (unsigned char *)pvPortMalloc(FLASH_SECTOR_SIZE);
        memset(rd_buf, 0, FLASH_SECTOR_SIZE);

        rd_size = (rd_size > FLASH_SECTOR_SIZE) ? FLASH_SECTOR_SIZE : rd_size;
        printf("Read  - Flash Offset 0x%lx (size=%d):\n", rd_addr, rd_size);
        memcpy((void*)rd_buf, (const void*)(BSP_FEATURE_OSPI_DEVICE_0_START_ADDRESS_DATA|rd_addr), rd_size);
        sflash_dump_print(rd_addr, rd_buf, rd_size);

        vPortFree(rd_buf);
    } else if ((argc >= 4) && (strcmp(argv[1], "erase") == 0)) {
        unsigned long erase_addr = htoi((char *)argv[2]);
        unsigned long erase_size = htoi((char *)argv[3]);

        erase_size = (erase_size > FLASH_SECTOR_SIZE) ? FLASH_SECTOR_SIZE : erase_size;

        printf("Erase - Flash Offset 0x%lx:\n", erase_addr);

        //ad_flash_erase_region(erase_addr, erase_size);

    } else {
        goto usage;
    }

    return true;
}
#endif

#ifdef __SUPPORT_EASY_SETUP__
static bool cmd_setup(int argc, char *argv[])
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    vTaskSuspend(cli_handle_task);
#if defined (__SUPPORT_APP_CONSOLE_INPUT__) && defined (CFG_WIFI)
    create_easy_setup_task(false);
#else
    printf("\tUnsupport EasySetup\n");
#endif /* __SUPPORT_APP_CONSOLE_INPUT__ */

    return true;
}
#endif /* __SUPPORT_EASY_SETUP__ */

#define BOOTTIME	"BOOT"
#define UPTIME		"UPTIME"
#define TIMEZONE	"ZONE"
#define SETTIME		"SET"
#define DAYLIGHT	"DST"
#if CFG_RTC_W
int cmd_set_time(char *date_format, char *time_format, int daylight)
{
    struct tm correction;
    char *pos = NULL;
    int tmp_int1 = 0;

    if (!date_format || !time_format) {
        return -1;
    }

    memset(&correction, 0x00, sizeof(struct tm));

    // Year
    pos = strtok(date_format, "-");
    if (pos && (cli_get_int_val_from_str(pos, &tmp_int1, 3) == 0) ) {
        correction.tm_year = tmp_int1 - 1900;
    } else {
        return -2;
    }

    // Month
    pos = strtok(NULL, "-");
    if (pos && (cli_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) ) {
        correction.tm_mon  = tmp_int1 - 1;
    } else {
        return -2;
    }

    // Day
    pos = strtok(NULL, "\0");
    if (pos && (cli_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) ) {
        correction.tm_mday = tmp_int1;
    } else {
        return -3;
    }

    // Hour
    pos = strtok(time_format, ":");
    if (pos && (cli_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) ) {
        correction.tm_hour = tmp_int1;
    } else {
        return -3;
    }

    // Min
    pos = strtok(NULL, ":");
    if (pos && (cli_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) ) {
        correction.tm_min  = tmp_int1;
    } else {
        return -3;
    }

    // Sec
    pos = strtok(NULL, "\0");
    if (pos && (cli_get_int_val_from_date_time_str(pos, &tmp_int1) == 0) ) {
        correction.tm_sec  = tmp_int1;
    } else {
        return -3;
    }

    if ((tmp_int1 = cli_is_date_time_valid(&correction)) != pdTRUE) {
        return tmp_int1;
    }

    /* Season flag, such as daylight saving time */
    if (daylight) {
    	correction.tm_isdst = 1;
    } else {
    	correction.tm_isdst = -1;
    }
    R_RTC_W_CalendarTimeSet(R_RTC_W_GetCtrl(), &correction);

    return 0;
}

static bool cmd_time(int argc, char *argv[])
{
    long timezone;
    
    struct tm ts;
    struct tm *p_ts = &ts;
    
    char buf[80];
    int timezone_offset[2] = {0, 0};
    int abs_minute, abs_hour;

    memset(buf, 0, 80);

    if (argc == 3) {
        if (os_strcasecmp(argv[1], TIMEZONE) == 0) {
            /* Set time zone */
            int multiplier = (argv[2][0] == '-')?(-1):(1);

            timezone_offset[0] = atoi(strtok(argv[2], ":"));
            if (timezone_offset[0] < 0) timezone_offset[0] = (-1) * timezone_offset[0];

            timezone_offset[1] = atoi(strtok(NULL, "\0"));

            if (timezone_offset[0] == 0 && timezone_offset[1] == 0) {
                timezone = 0;
            } else {
                timezone = (multiplier) * (3600 * (long)timezone_offset[0])
                          +(multiplier) * ((long)(timezone_offset[1] * 60));
            }

#if CFG_PMGR
#if LWIP_IPV4
            RM_PMGR_W_rtm_static_set(RTM_STATIC_KEY_TIMEZONE, timezone, 0);
#endif
#endif /* CFG_PMGR */
           R_RTC_W_CalendarTimeZoneSet(R_RTC_W_GetCtrl(), &timezone);
#if CFG_WIFI
           set_time_zone(timezone);		/* save to nvram */
#endif /* CFG_WIFI */
        } else {
help :
            printf("\nUsage: time [option]\n"
                   "\t\t: set [YYYY-MM-DD] [hh:mm:ss]\n"
                   "\t\t: zone [-hh:mm|+hh:mm]\n"
                   "\t\t: boot\n"
                   "\t\t: uptime\n"
                   "\t\t: help\n"
                  );

           return true;
        }
    } else if (argc == 2) {
        if (os_strcasecmp(argv[1], BOOTTIME) == 0) {
            /* print bootting time */
            R_RTC_W_CalendarBootTimeGet(R_RTC_W_GetCtrl(), p_ts);
            R_RTC_W_Time2Str(R_RTC_W_GetCtrl(), p_ts, buf, sizeof (buf), "%Y.%m.%d - %H:%M:%S");
            printf("Boottime: %s\n", buf);
        } else if (os_strcasecmp(argv[1], UPTIME) == 0) {
#ifdef __TIME64__
            __time64_t uptime = 0;
            __uptime(&uptime);
#else
            time_t uptime;
            uptime = __uptime();
#endif /* __TIME64__ */

            printf("Uptime: %lu days %02lu:%02lu.%02lu\n",
                   (unsigned long)uptime / (24 * 3600),
                   (unsigned long)uptime % (24 * 3600) / 3600,
                   (unsigned long)uptime % (3600) / 60,
                   (unsigned long)uptime % 60);
        } else if (os_strcasecmp(argv[1], "ZONE") == 0) {
            /* print time zone */
            R_RTC_W_CalendarTimeZoneGet(R_RTC_W_GetCtrl(), &timezone);
            abs_hour = timezone;
            if (timezone < 0 ) {
                abs_minute = -((timezone % 3600) / 60);
                abs_hour *= (-1);
            } else {
                abs_minute = ((timezone % 3600) / 60);
            }
            printf("Time Zone %s%02d:%02d\n", (timezone < 0)?"-":"+", abs_hour / 3600, abs_minute);
        } else {
        	goto help;
        }
    } else if (argc == 4 || argc == 5) {
        /* time set [YYYY-MM-DD] [hh:mm:ss] dst*/
        if (os_strcasecmp(argv[1], SETTIME) == 0) {
            int daylight = 0;

            /* Season flag, such as daylight saving time */
            if (argc == 5 && os_strcasecmp(argv[4], DAYLIGHT) == 0) {
                daylight = 1;
            }

            if (cmd_set_time(argv[2], argv[3], daylight)) {
                goto help;
            }

            R_RTC_W_CalendarTimeGet(R_RTC_W_GetCtrl(), p_ts);
            
            R_RTC_W_Time2Str(R_RTC_W_GetCtrl(), p_ts, buf, sizeof (buf), "%Y.%m.%d %H:%M:%S");
            
            R_RTC_W_CalendarTimeZoneGet(R_RTC_W_GetCtrl(), &timezone);
            
            abs_hour = timezone;
            if (timezone < 0 ) {
                abs_minute = -((timezone % 3600) / 60);
                abs_hour *= (-1);
            } else {
                abs_minute = ((timezone % 3600) / 60);
            }
            
            printf("SetTime: %s (GMT %s%02d:%02d)\n",
                   buf,
                   (timezone < 0)?"-":"+",
                   abs_hour / 3600,
                   abs_minute);
       } else {
           goto help;
       }
    } else {
        /* current time */
        rtc_w_instance_ctrl_t * rtc_w_ctrl = R_RTC_W_GetCtrl();

        if (rtc_w_ctrl == NULL) {
            printf("rtc_w not initialized !\n");
            return true;
        }

        R_RTC_W_CalendarTimeGet(rtc_w_ctrl, p_ts);
        
        R_RTC_W_Time2Str(rtc_w_ctrl, p_ts, buf, sizeof (buf), "%Y.%m.%d %H:%M:%S");
        
        R_RTC_W_CalendarTimeZoneGet(rtc_w_ctrl, &timezone);
        abs_hour = timezone;
        
        if (timezone < 0 ) {
            abs_minute = -((timezone % 3600) / 60);
            abs_hour *= (-1);
        } else {
            abs_minute = ((timezone % 3600) / 60);
        }
        printf("- Current Time : %s (GMT %s%02d:%02d)\n",
               buf,
               (timezone < 0)?"-":"+",
               abs_hour / 3600,
               abs_minute);
    }

    return true;
}
#endif
static bool cmd_heap_size_read(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    uint32_t size;

    size = xPortGetFreeHeapSize();
    printf("heap available size = %ld\n", size);

    return true;
}

#if 0
static const struct  {
     sleep_mode_t mode;
     char *modestr;
}  pm_mode_list[] = {
    { pm_mode_active         , "active"      },
    { pm_mode_idle           , "idle"        },
    { pm_mode_snooze         , "snooze"      },
    { pm_mode_extended_doze  , "doze"        },
    { pm_mode_extended_sleep , "ext-sleep"   },
    { pm_mode_deep_sleep     , "deep-sleep"  },
    { pm_mode_hibernation    , "hibernation" },
    { pm_mode_sleep_max      , "----"        },
};

static bool cmd_psleep(int argc, const char **argv)
{
    if(argc == 1)
    {
#if (dg_configUSE_PM_STATISTICS == 1)
        {
            extern uint32_t pm_get_statistics_info(uint8_t **slp_cnt_table, sleep_mode_t *cur_usr_mode);
            uint32_t slp_cnt_max, sidx;
            uint8_t  *sleep_cnt_table;
            sleep_mode_t cur_user_mode;

            slp_cnt_max = pm_get_statistics_info(&sleep_cnt_table, &cur_user_mode);

            for(sidx = 0; sidx < slp_cnt_max; sidx++){
                printf("PM[%s] = %u\n", pm_mode_list[sidx].modestr, sleep_cnt_table[sidx]);
            }
            printf("PM-usr-mode:%s\n", pm_mode_list[cur_user_mode].modestr);
        }
#endif //(dg_configUSE_PM_STATISTICS == 1)
    }else{
        if (os_strcasecmp(argv[1], "doze") == 0) {
            printf("Mode:Doze\r\n");
            pm_sleep_mode_set(pm_mode_extended_doze);
        }else if (os_strcasecmp(argv[1], "snooze") == 0) {
            printf("Mode:Snooze\r\n");
            pm_sleep_mode_set(pm_mode_snooze);
        }

        if(pm_sleep_mode_get() == pm_mode_active)
        {
            pm_sleep_mode_release(pm_mode_active);
        }else{
            printf("not available sleep\r\n");
        }
    }
    return true;
}

static bool cmd_pactive(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    //pm_sleep_mode_release(pm_mode_active);
    //console_set_timeout();
    return true;
}
#endif

#if (dg_configUSE_TRACE_FOR_DEBUG == 1)
static bool cmd_trace(int argc, const char **argv)
{
    print_function_trace();
    return true;
}
#endif	// dg_configUSE_TRACE_FOR_DEBUG

# if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static const debug_handler_t root_handlers[] = {
    { "ver",            "version",                   (debug_callback_t)cmd_ver             },
#if CFG_WIFI
    { "factory",        "Factory Reset",             (debug_callback_t)cmd_factory_reset   },
#endif
    { "reboot",         "[0|1]",                     (debug_callback_t)cmd_reboot          },
    { "reset_hard",     "hard reset",                (debug_callback_t)cmd_reboot          },
#ifdef __SUPPORT_EASY_SETUP__
    { "setup",          "Easy Setup",                (debug_callback_t)cmd_setup           },
#endif /* __SUPPORT_EASY_SETUP__ */
    { "sflash",         "SFLASH operation",          (debug_callback_t)cmd_sflash          },
#if CFG_RTC_W
    { "time",           "[option]",	                 (debug_callback_t)cmd_time            },
#endif
    { "lrd",            "[addr] <length>",           (debug_callback_t)cmd_mem_lread       },
    { "lwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_lwrite      },
    { "wrd",            "[addr] <length>",           (debug_callback_t)cmd_mem_wread       },
    { "wwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_wwrite      },
    { "brd",            "[addr] <length>",           (debug_callback_t)cmd_mem_read        },
    { "bwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_write       },
    { "hrd",            "heap available size",       (debug_callback_t)cmd_heap_size_read  },
#if (dg_configUSE_TRACE_FOR_DEBUG == 1)
    { "trace",          "display function trace",    (debug_callback_t)cmd_trace           },
#endif	// dg_configUSE_TRACE_FOR_DEBUG
#if CFG_WIFI
    { "regdb",          "print Regulatory Power table info",                  (debug_callback_t)cmd_regdb     },
#endif
#if CFG_PMGR
#if (dg_configUSE_RETENTION_MEM_INFO == 1)
    { "rtm_info",       "print retention memory info", (debug_callback_t)cmd_rtm_info      },
#endif	// dg_configUSE_RETENTION_MEM_INFO
#endif /* CFG_PMGR */
    { NULL },
};
#else // # if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)
static const debug_handler_t root_handlers[] = {
    { "ver",            "version",                   (debug_callback_t)cmd_ver             },
    { "lrd",            "[addr] <length>",           (debug_callback_t)cmd_mem_lread       },
    { "lwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_lwrite      },
    { "wrd",            "[addr] <length>",           (debug_callback_t)cmd_mem_wread       },
    { "wwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_wwrite      },
    { "brd",            "[addr] <length>",           (debug_callback_t)cmd_mem_read        },
    { "bwr",            "[addr] [data]",             (debug_callback_t)cmd_mem_write       },
    { "hrd",            "heap available size",       (debug_callback_t)cmd_heap_size_read  },
    { NULL },
};
#endif // # if (dg_configCODE_LOCATION == NON_VOLATILE_IS_FLASH)

static bool root_command(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

    return debug_handle_message(argc, argv, root_handlers);
}

bool root_command_handlers(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

    return root_command(argc, argv, (void *)root_handlers);
}

void help_root_cmd(void)
{
    const debug_handler_t *handler;
    print("\nroot commands:");

    for (handler = root_handlers; handler && handler->command; handler++) {
        if (handler->command != NULL && handler->command[0] != '\0' && handler->callback != NULL) {
            print("\t %-15s : %s", handler->command, handler->help != NULL ? handler->help:"");
        } else {
            printf("\n");
        }
    }
#if defined (__SUPPORT_PRODTEST_CONSOLE__)
    print("\t %-15s : help for production test\n", prod_test_help_str);
#endif
}

/* EOF */
