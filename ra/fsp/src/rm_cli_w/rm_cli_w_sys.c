/**
 ****************************************************************************************
 *
 * @file rm_cli_w_sys.c
 *
 * @brief SYSTEM command functions
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
#include <FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>
#include "rm_cli_w_utils.h"
#include "rm_cli_w_sys.h"
#include "rm_cli_w_debug_utils.h"
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_rand.h"
#include "SEGGER_RTT.h"

#include "sys_clock_mgr.h"
#include "common.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#if CFG_PMGR
 #include "r_pm_if.h"
#endif                                 /* CFG_PMGR */

#include "common_def.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
#endif

#if CFG_PMGR
extern int RM_PMGR_W_dpm_is_enabled(void);

#endif                                 /* CFG_PMGR */

#define RA6W1_TRNG_ENTROPY_THRESHOLD_BYTES    32

#if (BSP_CFG_RADIO_CLOCK_MGR_ENABLE == 1) && defined (CFG_RTC_W)
///////////////////////////////////////////////////////////////////////////////
 #define TEST_MAC_CLK_SETUP                   (0)
 #define TEST_MAC_PWR_SETUP                   (0)

static void ulltoa (unsigned long long value, char * buf, int radix)
{
    char              tmp[64 + 1];     /* Lowest radix is 2, so 64-bits plus a null */
    char            * p1 = tmp, * p2;
    static const char xlat[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (!buf)
    {
        return;
    }

    if ((radix < 2) || (radix > 36))
    {
        return;
    }

    do
    {
        *p1++ = xlat[value % (unsigned) radix];
    } while ((value /= (unsigned) radix));

    for (p2 = buf; p1 != tmp; *p2++ = *--p1)
    {
        /* nothing to do */
    }

    *p2 = '\0';
}

static char * lltoa (long long value, char * buf, int radix)
{
    char * save = buf;

    if (!buf)
    {
        return NULL;
    }

    if ((radix == 10) && (value < 0))
    {
        *buf++ = '-';
        value  = -value;
    }

    ulltoa(value, buf, radix);

    return save;
}

 #define MAX_CHAR_FOR_64BITS    (21)

///////////////////////////////////////////////////////////////////////////////

static bool rtc_command (int argc, const char ** argv)
{
    (void) argc;
    (void) argv;

    uint64_t                 rtccnt;
    char                     str_dec[MAX_CHAR_FOR_64BITS];
    char                     str_hex[MAX_CHAR_FOR_64BITS];
    bsp_wakeup_source_mask_t curwkupsrc = R_BSP_WakeupSourceGet();

    printf("current wakup source : %x\n", curwkupsrc);
    printf("  sleep id : %d, (%04x)\n", (int) bsp_prv_pd_sleep_id_get(), (unsigned int) bsp_prv_pd_sleep_id_get());
    printf("  retention flag : %ld\n", R_BSP_RetainedMemFlagGet());

    rtccnt = R_BSP_SystemRtcCountGet();
    printf("  R_BSP_SystemRtcCountGet : %s (%s)\n", lltoa(rtccnt, str_dec, 10), lltoa(rtccnt, str_hex, 16));

    rtccnt = bsp_prv_pd_wakeup_counter_get();
    printf("  bsp_prv_pd_wakeup_counter_get : %s, %s\n", lltoa(rtccnt, str_dec, 10), lltoa(rtccnt, str_hex, 16));

    return true;
}

///////////////////////////////////////////////////////////////////////////////

extern void hal_machw_setfreq(uint8_t newfreq);
extern void hal_machw_checkfreq(uint8_t use_printf);
extern void crm_set_mac_freq(int freq);

static void mycevt_callback_test (sys_clk_is_t clksrc, uint32_t freq, void * param)
{
    (void) param;

    int          mac_core_clock_freq;
    const char * clksrclist[] =
    {
        "IS_NONE",    "IS_XTAL40M",    "IS_LP",
        "IS_PLL240M", "IS_PLL192M",    "IS_PLL160M",
        "IS_PLL137M", "IS_PLL106M",    "INVALID"
    };

    printf("SYS_CLK_%s, freq: %ld\n", clksrclist[(uint32_t) (clksrc - SYS_CLK_IS_NONE)], freq);

    if (freq == 0)
    {
        return;
    }

    if (freq > 40)
    {
        mac_core_clock_freq = 40;
    }
    else
    {
        mac_core_clock_freq = 20;
    }

    crm_set_mac_freq(mac_core_clock_freq);
    hal_machw_setfreq(mac_core_clock_freq);
    printf("MAC core clock freq: %dMhz\n", mac_core_clock_freq);
    hal_machw_checkfreq(1);
}

typedef enum cpu_clock_chg_ret_type
{
    apply_nok,
    apply_ok,
    req_reboot
} cpu_clock_chg_ret_t;

static bool chk_support_cpu_clk (int cpu_clock)
{
    switch (cpu_clock)
    {
        case cpuclk_2M:                //!< 2.5 MHz, divided by 16 in XTAL
        case cpuclk_5M:                //!< 5  MHz, divided by 8 in XTAL
        case cpuclk_10M:               //!< 10 MHz, divided by 4 in XTAL
        case cpuclk_20M:               //!< 20 MHz, divided by 2 in XTAL

        case cpuclk_26M:               //!< 26 MHz, divided by 4 in SYSCLK 106MHz
        case cpuclk_34M:               //!< 34 MHz, divided by 4 in SYSCLK 137MHz
        case cpuclk_40M:               //!< 40 MHz, divided by 4 in SYSCLK 160MHz

        case cpuclk_53M:               //!< 53 MHz, divided by 2 in SYSCLK 106MHz
        case cpuclk_68M:               //!< 68 MHz, divided by 2 in SYSCLK 137MHz
        case cpuclk_80M:               //!< 80 MHz, divided by 2 in SYSCLK 160MHz

        case cpuclk_106M:              //!< 106 MHz
        case cpuclk_137M:              //!< 137 MHz
        case cpuclk_160M:              //!< 160 MHz
 #if (HW_DESCOPED_CLOCK == 1)
        case cpuclk_192M:              //!< 192 MHz, descoped
        case cpuclk_240M:              //!< 240 MHz, descoped
 #endif

            return TRUE;

    default:

            return FALSE;
    }
}

static cpu_clock_chg_ret_t cpu_clock_change (int clock_rate_mhz)
{
    static cm_clock_callback_t mycevt_callback;
    cpu_clk_t curcpuclk;

    if (chk_support_cpu_clk(clock_rate_mhz))
    {
        if ((cm_cpu_clk_get() > cpuclk_26M) && (clock_rate_mhz > cpuclk_26M))
        {
            mycevt_callback.func     = &mycevt_callback_test;
            mycevt_callback.priority = cm_cb_priority_coupled_hyper_loosely;
            mycevt_callback.param    = NULL;

            cm_register_clock_callback(&mycevt_callback);

            cm_cpu_clk_set(clock_rate_mhz);

            curcpuclk = cm_cpu_clk_get();

            printf("CPU Clock : %s %d.%d MHz\n",
                   (REG_GETF(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_PLL) ? "PLL" : "XTAL"),
                   curcpuclk,
                   curcpuclk == cpuclk_2M ? 5 : 0);

            cm_deregister_clock_callback(&mycevt_callback);
        }
        else if ((cm_cpu_clk_get() < cpuclk_26M) && (clock_rate_mhz < cpuclk_26M))
        {
            cm_cpu_clk_set(clock_rate_mhz);

            curcpuclk = cm_cpu_clk_get();

            printf("CPU Clock : %s %d.%d MHz\n",
                   (REG_GETF(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_PLL) ? "PLL" : "XTAL"),
                   curcpuclk,
                   curcpuclk == cpuclk_2M ? 5 : 0);
        }
        else
        {
            return req_reboot;
        }
    }
    else
    {
        return apply_nok;
    }

    return apply_ok;
}

static void print_support_cpu_clk (void)
{
    printf("Support clock rate:\n");
    printf("\t%d(%d.5)", cpuclk_2M, cpuclk_2M); //!< 2.5 MHz, divided by 16 in XTAL
    printf(", %d", cpuclk_5M);                  //!< 5  MHz, divided by 8 in XTAL
    printf(", %d", cpuclk_10M);                 //!< 10 MHz, divided by 4 in XTAL
    printf(", %d", cpuclk_20M);                 //!< 20 MHz, divided by 2 in XTAL

    printf(", %d", cpuclk_26M);                 //!< 26 MHz, divided by 4 in SYSCLK 106MHz
    printf(", %d", cpuclk_34M);                 //!< 34 MHz, divided by 4 in SYSCLK 137MHz
    printf(", %d", cpuclk_40M);                 //!< 40 MHz, divided by 4 in SYSCLK 160MHz

    printf(", %d", cpuclk_53M);                 //!< 53 MHz, divided by 2 in SYSCLK 106MHz
    printf(", %d", cpuclk_68M);                 //!< 68 MHz, divided by 2 in SYSCLK 137MHz
    printf(", %d", cpuclk_80M);                 //!< 80 MHz, divided by 2 in SYSCLK 160MHz

    printf(", %d", cpuclk_106M);                //!< 106 MHz
    printf(", %d", cpuclk_137M);                //!< 137 MHz
    printf(", %d", cpuclk_160M);                //!< 160 MHz
 #if (HW_DESCOPED_CLOCK == 1)
    printf(", %d", cpuclk_192M);                //!< 192 MHz, descoped
    printf(", %d", cpuclk_240M);                //!< 240 MHz, descoped
 #endif
    printf(" Mhz\n");
}

// cpu clock save nvram
static int set_cpu_clock_nvram (int cpu_clock)
{
    int status = 0;

    if (chk_support_cpu_clk(cpu_clock))
    {
 #ifdef RM_MAP_PERSISTANT_W
        status = RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                ENV_GROUP_BOOTCFG,
                                                "clk.cpu",
                                                (void *) &cpu_clock,
                                                4);
 #endif                                /* RM_MAP_PERSISTANT_W */
    }

    if (status != FSP_SUCCESS)
    {
        return FALSE;
    }

    return TRUE;
}

static bool cmd_cpu_clock_change (int argc, const char ** argv)
{
#if (BSP_CFG_RADIO_CLOCK_MGR_ENABLE == 1)
    uint32_t        clock_mhz;
    cpu_clk_t       curcpuclk ;
#if 0 // Unused code
    static cm_clock_callback_t mycevt_callback;
  #endif // 0

    curcpuclk = cm_cpu_clk_get();

    if ((argc > 1) && (parse_u32(argv[1], &clock_mhz) == true))
    {
        switch (cpu_clock_change(clock_mhz))
        {
            case req_reboot:
            {
                if ((argc == 3) && (strcmp(argv[2], "save") == 0)) // save nvram
                {
                    printf("\nCPU's clock speed keeps changing on reboot.\n");
                }
                else
                {
                    printf("\nTo change the current CPU clock(%ldMhz) to %ldMhz, save parameter is required.\n\n",
                           (long int) curcpuclk, clock_mhz);

                    return false;
                }

                // No Break
            }

            case apply_ok:
            {
                if ((argc == 3) && (strcmp(argv[2], "save") == 0)) // save nvram
                {
                    if (set_cpu_clock_nvram(clock_mhz))
                    {
                        printf("NVRAM save OK\n");
                    }
                    else
                    {
                        printf("NVRAM save failure\n");
                    }
                }

                break;
            }

            case apply_nok:

            default:
            {
                printf("\nUnsupported rate: %ld\n", clock_mhz);
                print_support_cpu_clk();
                break;
            }
        }
    }
    else if (argc == 1)
    {
        curcpuclk = cm_cpu_clk_get();
        printf("CPU clock: %s %d.%d MHz\n\n",
               (REG_GETF(CRG_TOP, CLK_CTRL_REG, RUNNING_AT_PLL) ? "PLL" : "XTAL"),
               curcpuclk,
               curcpuclk == cpuclk_2M ? 5 : 0);
    }
    else if (strcmp(argv[1], "help") == 0)
    {
        printf("\nUsage: clock [<rate>|help] [<none>|save]\n");
        print_support_cpu_clk();
    }
    else
    {
        return false;
    }

    return true;
#else
    printf("enable BSP_CFG_RADIO_CLOCK_MGR_ENABLE !!\n");
    return false;
 #endif
}

static int cmd_tcs_xtal_on_otp (int argc, const char * argv[])
{
    (void) argc;

    uint32_t value        = 0;
    uint32_t option_check = 0;

    if (strncmp(argv[1], "wr", 2) == 0)
    {
        option_check = htoi((char *) argv[3]);
        value        = htoi((char *) argv[2]);

        if (bsp_tcs_otp_write_xtal(&value, option_check))
        {
            printf("successed\r\n");

            return true;
        }
        else
        {
            printf("failed\r\n");

            return false;
        }
    }
    else if (strncmp(argv[1], "rd", 2) == 0)
    {
        if (bsp_tcs_otp_read_xtal(&value))
        {
            printf("\r\n0x%08x\r\n", (unsigned int) value);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

static int cmd_tcs_mac_on_otp (int argc, const char * argv[])
{
    (void) argc;

    uint32_t i;
    uint32_t option_check = 0;
    uint32_t mac_addr[2]  = {0, };
    uint8_t tmp_mac[6]    = {0, };

    if (strncmp(argv[1], "wr", 2) == 0)
    {
        // option_check = htoi((char *)argv[3]);
        for (i = 0; i < 6; i++)
        {
            tmp_mac[i] = toint(argv[2][i * 2]) << 4 | toint(argv[2][(i * 2) + 1]);
        }

        // convert  MAC_TYPE
        memcpy(&mac_addr[0], tmp_mac + 2, 4);
        memcpy(((uint8_t *) (&mac_addr[1])) + 2, tmp_mac, 2);

        mac_addr[0] = (tmp_mac[2] << 24 & 0xff000000) | (tmp_mac[3] << 16 & 0xff0000) | (tmp_mac[4] << 8 & 0xff00) |
                      (tmp_mac[5] & 0xff);
        mac_addr[1] = (tmp_mac[0] << 8 & 0xff00) | (tmp_mac[1] & 0xff);

        printf("mac_addr raw %08lx,%08lx\r\n", mac_addr[0], mac_addr[1]);

        if (bsp_tcs_otp_write_mac(mac_addr, option_check))
        {
            printf("successed\r\n");

            return true;
        }
        else
        {
            printf("failed\r\n");

            return false;
        }
    }
    else if (strncmp(argv[1], "rd", 2) == 0)
    {
        if (bsp_tcs_otp_read_mac(mac_addr))
        {
            printf("\r\n%04x%08x\r\n", (unsigned int) (mac_addr[1] & 0xffff), (unsigned int) mac_addr[0]);

            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return false;
}

static bool cmd_tcs (int argc, const char ** argv)
{
    if (argc > 3)
    {
        if ((strcmp(argv[1], "wr.xtal")) == 0)
        {
            cmd_tcs_xtal_on_otp(argc, argv);
        }
        else if ((strcmp(argv[1], "wr.mac")) == 0)
        {
            cmd_tcs_mac_on_otp(argc, argv);
        }
        else
        {
            return false;
        }
    }
    else if (argc > 1)
    {
        if ((strcmp(argv[1], "rd.xtal")) == 0)
        {
            cmd_tcs_xtal_on_otp(argc, argv);
        }
        else if ((strcmp(argv[1], "rd.mac")) == 0)
        {
            cmd_tcs_mac_on_otp(argc, argv);
        }
        else if ((strcmp(argv[1], "info")) == 0)
        {
            extern void bsp_tcs_info_printf(bool match);

            bsp_tcs_info_printf(true);
        }
        else
        {
            return false;
        }
    }
    else
    {
        printf("---example----\r\n");
        printf("tcs info\r\n");
        printf("tcs wr.xtal 0020076f [0~3]  - 0: force write on otp...\r\n");
        printf("tcs wr.mac 749050b0c100 [0~3]  - 0: force write on otp...\r\n");
        printf("tcs rd.xtal\r\n");
        printf("tcs rd.mac\r\n");
    }

    return true;
}

 #define TRNG_EHR_DATA_REG_NUM    6

enum trng_output_modes
{
    TRNG_OUTPUT_MODE_HEX,
    TRNG_OUTPUT_MODE_DEC,
    TRNG_OUTPUT_MODE_HEXDUMP,

    TRNG_OUTPUT_MODE_MAX
};

static int ra6w1_entropy_poll (void * data, unsigned char * output, size_t len, size_t * olen)
{
    (void) data;
    static uint32_t last_w = 0;
    static int have_last   = 0;
    size_t produced        = 0;

    *olen = 0;

    while (produced < len)
    {
        uint32_t w = 0;

        for (int tries = 0; tries < TRNG_RETRY_LIMIT; tries++)
        {
            w = trng_rand();

            if (w == 0)
            {
                continue;
            }

            if (!have_last || (w != last_w))
            {
                break;
            }
        }

        if ((w == 0) || (have_last && (w == last_w)))
        {
            *olen = produced;

            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        last_w    = w;
        have_last = 1;

        size_t chunk = (len - produced >= 4) ? 4 : (len - produced);

        // little-endian bytes
        for (size_t i = 0; i < chunk; i++)
        {
            output[produced + i] = (unsigned char) ((w >> (8u * i)) & 0xFFu);
        }

        produced += chunk;
    }

    *olen = produced;

    return 0;
}

static int ra6w1_drbg_get_ctx (mbedtls_ctr_drbg_context ** ctr, mbedtls_entropy_context ** entropy)
{
    static mbedtls_entropy_context s_entropy;
    static mbedtls_ctr_drbg_context s_ctr;
    static int s_inited = 0;

    if (!s_inited)
    {
        mbedtls_entropy_init(&s_entropy);
        mbedtls_ctr_drbg_init(&s_ctr);

        mbedtls_entropy_add_source(&s_entropy,
                                   ra6w1_entropy_poll,
                                   NULL,
                                   RA6W1_TRNG_ENTROPY_THRESHOLD_BYTES,
                                   MBEDTLS_ENTROPY_SOURCE_STRONG);

        const unsigned char pers[] = "ra6w1_cc312_trng_seed";
        int rc = mbedtls_ctr_drbg_seed(&s_ctr, mbedtls_entropy_func, &s_entropy, pers, sizeof(pers) - 1);
        if (rc != 0)
        {
            return rc;
        }

        s_inited = 1;
    }

    *ctr     = &s_ctr;
    *entropy = &s_entropy;

    return 0;
}

static bool get_drbg_trng_output (int num, int out_mode)
{
    int i;
    int rc;
    mbedtls_ctr_drbg_context * ctr;
    mbedtls_entropy_context * entropy;

    rc = ra6w1_drbg_get_ctx(&ctr, &entropy);
    if (rc != 0)
    {
        printf("DRBG init failed: %d\n", rc);

        return false;
    }

    for (i = 0; i < num; i++)
    {
        uint32_t w;
        rc = mbedtls_ctr_drbg_random(ctr, (unsigned char *) &w, sizeof(w));

        if (rc != 0)
        {
            printf("DRBG random failed: %d\n", rc);
            printf("\n\n%s:%d: failed produced Random numbers !!!\n", __func__, __LINE__);

            return false;
        }

        if (out_mode == TRNG_OUTPUT_MODE_DEC)
        {
            printf("%lu\n", (unsigned long) w);
        }
        else if (out_mode == TRNG_OUTPUT_MODE_HEX)
        {
            printf("0x%08lX\n", (unsigned long) w);
        }
        else
        {
            if ((i % 4) == 0)
            {
                printf("\n%08lX", (unsigned long) w);
            }
            else
            {
                printf("%08lX", (unsigned long) w);
            }
        }
    }

    printf("\n\n%s:%d: successfully produced %d Random numbers !!!\n", __func__, __LINE__, num);

    return true;
}

static bool get_trng_output (int num, int out_mode)
{
    int i;

    for (i = 0; i < num; i++)
    {
        if (out_mode == TRNG_OUTPUT_MODE_DEC)
        {
            printf("%ld\n", prng_rand());
        }
        else if (out_mode == TRNG_OUTPUT_MODE_HEX)
        {
            printf("0x%08lx\n", prng_rand());
        }
        else
        {
            if ((i % 4) == 0)
            {
                printf("\n%08lX", prng_rand());
            }
            else
            {
                printf("%08lX", prng_rand());
            }
        }
    }

    printf("\n\n%s:%d: successfully produced %d Random numbers !!!\n", __func__, __LINE__, num);

    return true;
 #if 0
    int i, j;
    uint32_t ehr_data[TRNG_EHR_DATA_REG_NUM] = {0};

    if (!trng_init_regs())
    {
        return false;
    }

    for (i = 0; i < num; i += TRNG_EHR_DATA_REG_NUM)
    {
        if (!trng_is_valid())
        {
            return false;
        }

        /* Clear up the interrupt status register */
        CC312->CC312_RNG_ICR_REG = ~0UL;

        /* Grab the TRNG data */
        ehr_data[0] = CC312->CC312_EHR_DATA_0_REG;
        ehr_data[1] = CC312->CC312_EHR_DATA_1_REG;
        ehr_data[2] = CC312->CC312_EHR_DATA_2_REG;
        ehr_data[3] = CC312->CC312_EHR_DATA_3_REG;
        ehr_data[4] = CC312->CC312_EHR_DATA_4_REG;
        ehr_data[5] = CC312->CC312_EHR_DATA_5_REG;

        for (j = 0; (j < TRNG_EHR_DATA_REG_NUM) && ((i + j) < num); j++)
        {
            if (out_mode == TRNG_OUTPUT_MODE_DEC)
            {
                printf("%ld\n", ehr_data[j]);
            }
            else if (out_mode == TRNG_OUTPUT_MODE_HEX)
            {
                printf("0x%08lx\n", ehr_data[j]);
            }
            else
            {
                if (((i + j) % 4) == 0)
                {
                    printf("\n%08lX", ehr_data[j]);
                }
                else
                {
                    printf("%08lX", ehr_data[j]);
                }
            }
        }
    }

    /* Disable signal for the random source */
    CC312->CC312_RND_SOURCE_ENABLE_REG = 0;

    /* Disable the HW RNG clock */
    CC312->CC312_RNG_CLK_ENABLE_REG = 0;

    printf("\n\n%s:%d: successfully produced %d True Random numbers !!!\n", __func__, __LINE__, num);

    return true;
 #endif
}

static void cmd_trng_help (void)
{
    printf("\nUsage: trng [<mode>] [<num>] [<out_mode>]\n");
    printf("<mode>\n1: HW random\n2: SW random\n<num> - number of integers\n<out_mode>\n0: HEX\n1: DEC\n2: HEXDUMP\n");
}

static bool cmd_trng (int argc, const char ** argv)
{
    int mode   = 0;
    int num    = 0;
    int output = 0;

    if ((argc < 4) || (strcmp(argv[1], "help") == 0))
    {
        cmd_trng_help();

        return true;
    }

    mode   = atoi((char *) argv[1]);
    num    = atoi((char *) argv[2]);
    output = atoi((char *) argv[3]);

    if (num < 1)
    {
        printf("wrong number of integers %d\n", num);
        cmd_trng_help();

        return false;
    }

    if ((output < TRNG_OUTPUT_MODE_HEX) || (output >= TRNG_OUTPUT_MODE_MAX))
    {
        printf("wrong output mode %d\n", output);
        cmd_trng_help();

        return false;
    }

    if (mode == 1)
    {
        return get_trng_output(num, output);
    }
    else if (mode == 2)
    {
        return get_drbg_trng_output(num, output);
    }
    else
    {
        printf("wrong trng mode %d\n", mode);
        cmd_trng_help();

        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////
 #define  WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE    (0x60d00030)
 #define  WIFI_RFHPI_MAIN_CTRL_DEPENDENCY            (0x60d00014)
 #define  WIFI_RFHPI_TEMP_SENSOR_CAL                 (0x60d0006c)
 #define REG_PL_WR(addr, value)    (*(volatile uint32_t *) ((addr))) = (value)
 #define REG_PL_RD(addr)           (*(volatile uint32_t *) ((addr)))

static bool cmd_chip_temp (int argc, const char ** argv)
{
    (void) argc;
    (void) argv;

    uint32_t reg      = REG_PL_RD(WIFI_RFHPI_TEMP_SENSOR_CAL);
    uint32_t temp_val = reg & 0x7Fu;   /* TEMP_VALUE_AFTER_PROCESS_CAL [6:0] */

    /* Temperature (degC) = TEMP_VALUE * (-1.667) + 153
     * Computed in millidegrees using integer arithmetic to avoid float printf. */
    int32_t temp_mdeg   = (int32_t) temp_val * (-1667) + 153000;
    int32_t abs_mdeg    = (temp_mdeg < 0) ? -temp_mdeg : temp_mdeg;
    int32_t whole       = abs_mdeg / 1000;
    int32_t frac2       = (abs_mdeg % 1000) / 10; /* two decimal digits */
    const char * p_sign = (temp_mdeg < 0) ? "-" : "";

    printf("TEMP_VALUE=0x%02x (%u), temperature=%s%d.%02d degC\n",
           (unsigned int) temp_val,
           (unsigned int) temp_val,
           p_sign,
           (int) whole,
           (int) frac2);

    return true;
}

static bool cmd_hpi_test_command (int argc, const char ** argv)
{
 #define PRINT_REG_VALUE(x)     do {                           \
        printf(#x "(%p) => %08x\n", &(x), (unsigned int) (x)); \
} while (0)

 #define PRINT_REG_SET(x, v)    do {                           \
        printf(#x "(%p) <= %08x\n", &(x), (unsigned int) (v)); \
        (x) = (v);                                             \
        printf(#x "(%p) => %08x\n", &(x), (unsigned int) (x)); \
} while (0)

    if ((argc > 1) && (strcmp(argv[1], "hpion") == 0))
    {
        // 0x60d00030 0x86  ; MAIN_CTRL_INTERFACE_MAN_MODE
        // PRINT_REG_SET(WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE, 0x86);
        printf("WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE (%p) => %08x\n",
               (void *) WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE));
        REG_PL_WR(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE, 0x86);
        printf("WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE (%p) => %08x\n",
               (void *) WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE));

        // 0x60d00014 0x198 ; MAIN_CTRL_DEPENDENCY
        // PRINT_REG_SET(WIFI_RFHPI->MAIN_CTRL_DEPENDENCY, 0x198);
        printf("WIFI_RFHPI->MAIN_CTRL_DEPENDENCY (%p) => %08x\n", (void *) WIFI_RFHPI_MAIN_CTRL_DEPENDENCY,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY));
        REG_PL_WR(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY, 0x198);
        printf("WIFI_RFHPI->MAIN_CTRL_DEPENDENCY (%p) => %08x\n", (void *) WIFI_RFHPI_MAIN_CTRL_DEPENDENCY,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY));

        // 0x40038124 0x4AC00001 ; CNT_TESTI_REG
        PRINT_REG_SET(RTC->CNT_TESTI_REG, 0x4AC00001);

        // 0x400B0098 0x0   ; P1_15_MODE_REG
        PRINT_REG_SET(GPIO->P1_15_MODE_REG, 0x0);

        // 0x400C0274 0x8840 ; TEST_CFG_REG
        PRINT_REG_SET(CRG_COM->TEST_CFG_REG, 0x8840);
    }
    else if ((argc > 2) && (strcmp(argv[1], "rfhpi.if") == 0))
    {
        uint32_t regval;
        if (parse_u32(argv[2], &regval) == true)
        {
            // PRINT_REG_SET(WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE, regval);
            printf("WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE (%p) => %08x\n",
                   (void *) WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE,
                   (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE));
            REG_PL_WR(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE, regval);
            printf("WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE (%p) => %08x\n",
                   (void *) WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE,
                   (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE));
        }
    }
    else if ((argc > 2) && (strcmp(argv[1], "rfhpi.dp") == 0))
    {
        uint32_t regval;
        if (parse_u32(argv[2], &regval) == true)
        {
            // PRINT_REG_SET(WIFI_RFHPI->MAIN_CTRL_DEPENDENCY, regval);

            printf("WIFI_RFHPI->MAIN_CTRL_DEPENDENCY (%p) => %08x\n", (void *) WIFI_RFHPI_MAIN_CTRL_DEPENDENCY,
                   (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY));
            REG_PL_WR(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY, regval);
            printf("WIFI_RFHPI->MAIN_CTRL_DEPENDENCY (%p) => %08x\n", (void *) WIFI_RFHPI_MAIN_CTRL_DEPENDENCY,
                   (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY));
        }
    }
    else if ((argc > 2) && (strcmp(argv[1], "rtc.test") == 0))
    {
        uint32_t regval;
        if (parse_u32(argv[2], &regval) == true)
        {
            PRINT_REG_SET(RTC->CNT_TESTI_REG, regval);
        }
    }
    else if ((argc > 2) && (strcmp(argv[1], "gpio.p1_15") == 0))
    {
        uint32_t regval;
        if (parse_u32(argv[2], &regval) == true)
        {
            PRINT_REG_SET(GPIO->P1_15_MODE_REG, regval);
        }
    }
    else if ((argc > 2) && (strcmp(argv[1], "crg.test") == 0))
    {
        uint32_t regval;
        if (parse_u32(argv[2], &regval) == true)
        {
            PRINT_REG_SET(CRG_COM->TEST_CFG_REG, regval);
        }
    }
    else
    {
        // PRINT_REG_VALUE(WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE);
        printf("WIFI_RFHPI->MAIN_CTRL_INTERFACE_MAN_MODE (%p) => %08x\n",
               (void *) WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_INTERFACE_MAN_MODE));

        // PRINT_REG_VALUE(WIFI_RFHPI->MAIN_CTRL_DEPENDENCY);
        printf("WIFI_RFHPI->MAIN_CTRL_DEPENDENCY (%p) => %08x\n", (void *) WIFI_RFHPI_MAIN_CTRL_DEPENDENCY,
               (unsigned int) REG_PL_RD(WIFI_RFHPI_MAIN_CTRL_DEPENDENCY));

        PRINT_REG_VALUE(RTC->CNT_TESTI_REG);
        PRINT_REG_VALUE(GPIO->P1_15_MODE_REG);
        PRINT_REG_VALUE(CRG_COM->TEST_CFG_REG);
    }

    return true;
}

static const debug_handler_t sys_handlers[] =
{
    {"rtc",   "RTC cmds, ...",                       rtc_command                                                    },
    {"clock", "[<rate>|help] [<none>|save]",         cmd_cpu_clock_change                                           },
    {"temp",  "read chip temperature register",      cmd_chip_temp                                                  },
    {"test",  "hpi.test cmds",                       cmd_hpi_test_command                                           },
 #if dg_configSYS_TCS
    {"tcs",   "tcs.test cmds ",                      cmd_tcs                                                        },
 #endif
    {"trng",  "[<mode>|help] [<num>] [<out_mode>]",  cmd_trng                                                       },
    {NULL},
};
#endif

bool sys_command (int argc, const char * argv[], void * user_data)
{
#if CFG_RTC_W
    (void) user_data;

    return debug_handle_message(argc, argv, sys_handlers);
#else

    return false;
#endif
}
