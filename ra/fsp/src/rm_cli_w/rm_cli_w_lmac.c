/**
 ****************************************************************************************
 *
 * @file rm_cli_w_lmac.c
 *
 * @brief LMAC command functions
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
#include "bsp_api.h"
#if CFG_WIFI
 #include "rm_cli_w.h"
 #include "rm_cli_w_utils.h"
 #include "rm_cli_w_debug_utils.h"
 #include "sdk_defs.h"
 #include "FreeRTOS.h"
 #include "event_groups.h"
 #include "task.h"
 #include "SEGGER_RTT.h"
 #include "rm_cli_w_lmac.h"
 #include "r_gpio_w.h"
 #include "rwnx_mac_common.h"
 #include "romac4rtos.h"
 #include "bsp_ppm_compensation.h"

 #define REG_PL_RD(addr)           (*(volatile uint32_t *) ((addr)))
 #define REG_PL_WR(addr, value)    (*(volatile uint32_t *) ((addr))) = (value)
 #define TIMP_CMD_EN

/* Worker task that periodically re-applies the temperature-driven trim.
 * A dedicated task (rather than the FreeRTOS timer-service task) is used
 * because the timer task on this build runs with configMINIMAL_STACK_SIZE,
 * which is too small to host a printf() + register access without
 * overflowing.
 */
static TaskHandle_t s_ppm_recomp_task = NULL;

/*
 * Apply temperature-driven XTAL40M_CCTRL trim before an ATE TX command starts.
 *
 * Only invoked from the four LMAC TX entry points (rftx, rftxpkt, cont_tx,
 * rfcw start). The compensation table lives in BSP and is
 * configured via an e2studio BSP property. Lookup is nearest-neighbour on the 7-bit temperature
 * code, with out-of-range codes clamped to the table endpoints. The new
 * CCTRL value is clamped to the unsigned 7-bit range [0, 127].
 *
 * Non-cumulative: the very first invocation after boot snapshots the current
 * CCTRL as the baseline. Every invocation thereafter computes
 *     CCTRL_new = baseline + delta(temp_code)
 * so repeated TX commands at the same temperature produce the same CCTRL
 * value rather than drifting by one delta per call.
 *
 * Volatile only: the value is not persisted across reset.
 */
static void lmac_apply_ppm_compensation_for_tx (void)
{
    bsp_ppm_compensation_result_t result = {0};
    if (!bsp_ppm_compensation_apply_for_tx(&result))
    {
        return;
    }

    int32_t      abs_mdeg = (result.temp_mdeg < 0) ? -result.temp_mdeg : result.temp_mdeg;
    const char * p_sign   = (result.temp_mdeg < 0) ? "-" : "";

    printf("PPM comp: temp_code=0x%02x (%u, %s%ld.%02ld degC) CCTRL %02lx -> %02lx (delta %+d)\n",
           result.temp_code,
           result.temp_code,
           p_sign,
           (long) (abs_mdeg / 1000),
           (long) ((abs_mdeg % 1000) / 10),
           (unsigned long) result.cctrl_old,
           (unsigned long) result.cctrl_new,
           (int) result.delta);
}

/* Worker-task body. Sleeps BSP_CFG_PPM_RECOMP_PERIOD_MS between
 * recompensations and runs until vTaskDelete() is called from
 * lmac_ppm_recomp_timer_stop(). */
static void lmac_ppm_recomp_task (void * pvParameters)
{
    (void) pvParameters;
    for ( ; ; )
    {
        vTaskDelay(pdMS_TO_TICKS(BSP_CFG_PPM_RECOMP_PERIOD_MS));
        lmac_apply_ppm_compensation_for_tx();
    }
}

/* Start the periodic recompensation worker task. Idempotent: if the task
 * already exists it is left running. Called from each TX-start hook except
 * rftxpkt, which is one-shot and short. */
static void lmac_ppm_recomp_timer_start (void)
{
    if (BSP_CFG_PPM_COMP_ENABLE == 0)
    {
        return;
    }

    if (s_ppm_recomp_task != NULL)
    {
        return;
    }

    bsp_ppm_compensation_recomp_start();

    (void) xTaskCreate(lmac_ppm_recomp_task,
                       "ppm_recomp",
  #if (dg_configSYSTEMVIEW == 0)
                       (((400 * sizeof(StackType_t)) - 1) / sizeof(StackType_t) + 1),
  #else
                       (((400 * sizeof(StackType_t) + dg_configSYSTEMVIEW_STACK_OVERHEAD) - 1) / sizeof(StackType_t) + 1),
  #endif
                       NULL,
                       CLI_TASK_PRIORITY,
                       &s_ppm_recomp_task);
}

/* Stop the periodic recompensation worker task. Idempotent. Called from
 * each TX-stop path. */
static void lmac_ppm_recomp_timer_stop (void)
{
    bsp_ppm_compensation_recomp_stop();

    if (s_ppm_recomp_task != NULL)
    {
        TaskHandle_t h = s_ppm_recomp_task;
        s_ppm_recomp_task = NULL;
        vTaskDelete(h);
    }
}

extern enum fc80211_dfs_regions g_region;

extern void radar_enable(uint32_t enable);
extern void radar_pack(uint8_t irqmacradardeten, uint8_t irqmacccatimeouten);
extern void radar_timeclkforce();

extern void       rwnx_radar_detection_init(void);
extern void       rwnx_radar_set_domain(enum fc80211_dfs_regions region);
extern void       rwnx_radar_detection_enable(u8 enable, u8 chain);
extern BaseType_t rwnx_driver_task_initiailize(void);
extern BaseType_t rwnx_mac_task_initiailize(void);
extern void       rwnx_radar_detection_init(void);
extern int        rwnx_send_reset(void);
extern int        rwnx_send_start(void);
extern void       rxl_reset(void);
extern void       set_radar_specs(int argc, char * argv[]);

extern void hal_machw_reset(void);
extern void lmac_cmd_rf_cw_pll_restore();
extern void lmac_cmd_rftx_stop(void);
extern void txl_reset(void);
extern int  lmac_cmd_rftxpkt(struct RFTX * rftx_param);

extern uint32_t lmac_cmd_get_machw_mib_dot11_wep_excluded_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_fcs_error_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_rx_phy_error_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_rd_fifo_overflow_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_rx_mpif_overflow_count(void);

extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_ureceived_mpdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_greceived_mpdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_ureceived_other_mpdu(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_qos_retries_received_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_ureceived_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_greceived_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_ureceived_other_amsdu(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_received_octets_in_amsdu_count(void);

extern uint32_t lmac_cmd_get_machw_mib_rw_uampdu_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_gampdu_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_other_ampdu_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_mpdu_in_received_ampdu_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_received_octets_in_ampdu_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_ampdu_delimiter_crc_error_count(void);

extern uint32_t lmac_cmd_get_machw_mib_dot11_20mhz_frame_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_40mhz_frame_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_80mhz_frame_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_160mhz_frame_received_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_beamforming_received_frame_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_mu_received_framed_count(void);

extern uint32_t            lmac_cmd_get_machw_mib_rw_tx_underrun_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_utransmitted_mpdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_gtransmitted_mpdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_qos_failed_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_qos_retry_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_qos_rts_success_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_qos_rts_failure_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_qos_ack_failure_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_utransmitted_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_rw_gtransmitted_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_failed_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_retry_amsdu_count(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_transmitted_octets_in_amsdu(void);
extern volatile uint32_t * lmac_cmd_get_machw_mib_dot11_amsdu_ack_failure_count(void);

extern uint32_t lmac_cmd_get_machw_mib_dot11_transmitted_ampdu_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_transmitted_mpdus_in_ampdu_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_transmitted_octets_in_ampdu_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_20mhz_frame_transmitted_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_40mhz_frame_transmitted_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_80mhz_frame_transmitted_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_160mhz_frame_transmitted_count(void);
extern uint32_t lmac_cmd_get_machw_mib_dot11_beamforming_frame_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_su_bfr_transmitted_count(void);
extern uint32_t lmac_cmd_get_machw_mib_rw_mu_bfr_transmitted_count(void);

extern const ioport_instance_t g_gpio_w;

const char * str_lmac_hw_state[4] =
{
    "IDLE", "RESERVED", "DOZE", "ACTIVE"
};

static bool lmac_version (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    printf("LMAC Version : %s\n", lmac_cmd_get_version());
    printf("RF Version : %s\n", lmac_cmd_get_rf_version());

    return true;
}

bool lmac_init (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    rwnx_mac_task_initiailize();
    rwnx_driver_task_initiailize();

    return true;
}

bool lmac_start (int argc, const char ** argv)
{
    uint32_t temp;

    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    rwnx_send_reset();
    rwnx_send_start();

    /* MAC HW Active Clock Gating OFF  */

    // Active Clock Gating on 0x60b0004c [7] activeClkGating = 0
    temp = REG_PL_RD(0x60b0004c);
    REG_PL_WR(0x60b0004c, (temp & (~(1 << 7))));

    // temp_cal_off();

    // lmac_cmd_state_set(LMAC_HW_ACTIVE);

    return true;
}

static bool lmac_state (int argc, const char ** argv)
{
    uint8_t state;

    if (argc == 2)
    {
        if (strcmp(argv[1], "i") == 0)
        {
            lmac_cmd_state_set(LMAC_HW_IDLE);
        }
        else if (strcmp(argv[1], "d") == 0)
        {
            if (lmac_cmd_state_set(LMAC_HW_DOZE))
            {
                printf("current HW LMAC state DOZE\n");

                return true;
            }
            else
            {
                printf("ACTIVE to DOZE not allow.\n");
                printf("ACTIVE to IDLE and set to DOZE.\n");

                return true;
            }
        }
        else if (strcmp(argv[1], "a") == 0)
        {
            lmac_cmd_state_set(LMAC_HW_ACTIVE);
        }
        else
        {
            return false;
        }
    }

    state = lmac_cmd_state_get();
    printf("current HW LMAC state %s\n", str_lmac_hw_state[state]);

    return true;
}

 #define MIB_BASE                                  0x60b00800
 #define MIB_OFFSET(x)    ((x - MIB_BASE) / 4)
 #define DOT11_WEP_EXCLUDED_COUNT                  MIB_OFFSET(0x60B00800)
 #define DOT11_FCS_ERROR_COUNT                     MIB_OFFSET(0x60B00804)
 #define RW_RX_PHY_ERROR_COUNT                     MIB_OFFSET(0x60B00808)
 #define RW_RD_FIFO_OVER_COUNT                     MIB_OFFSET(0x60B0080C)
 #define RW_TX_UNDERRUN_COUNT                      MIB_OFFSET(0x60B00810)

 #define RW_RX_MPIF_OVERFLOW_COUNT                 MIB_OFFSET(0x60B00814)
 #define DOT11_QOS_FAILED_COUNT_8                  MIB_OFFSET(0x60B00870)

 #define DOT11_QOS_RETRY_COUNT_8                   MIB_OFFSET(0x60B00890)
 #define RW_QOS_ACK_FAILURE_COUNT_8                MIB_OFFSET(0x60B008F0)
 #define DOT11_QOS_RETRIES_RECEIVED_COUNT_8        MIB_OFFSET(0x60B00970)
 #define DOT11_FAILED_AMSDU_COUNT_8                MIB_OFFSET(0x60B009D0)
 #define DOT11_RETRY_AMSDU_COUNT_8                 MIB_OFFSET(0x60B009F0)

 #define DOT11_AMSDU_ACK_FAILURE_COUNT_8           MIB_OFFSET(0x60B00A30)

 #define DOT11_TRANSMITTED_AMPDU_COUNT             MIB_OFFSET(0x60B00B30)
 #define DOT11_TRANSMITTED_MPDUS_IN_AMPDU_COUNT    MIB_OFFSET(0x60B00B34)
 #define DOT11_MPDU_IN_RECEIVED_AMPDU_COUNT        MIB_OFFSET(0x60B00B48)
 #define DOT11_AMPDU_DELIMITER_CRC_ERROR_COUNT     MIB_OFFSET(0x60B00B50)

 #define DOT11_20MHZ_FRAME_TRANSMITTED_COUNT       MIB_OFFSET(0x60B00B70)
 #define DOT11_20MHZ_FRAME_RECEIVED_COUNT          MIB_OFFSET(0x60B00B80)
 #define RW_FAILED_20MHZ_TXOP                      MIB_OFFSET(0x60B00B90)
 #define RW_SUCCESSFUL_20MHZ_TXOP                  MIB_OFFSET(0x60B00B94)

 #define RW_DYN_BW_DROP_COUNT                      MIB_OFFSET(0x60B00BB0)
 #define RW_STA_BW_FAILED_COUNT                    MIB_OFFSET(0x60B00BB4)

bool lmac_mib (int argc, const char ** argv)
{
    uint32_t * pMib;
    uint32_t * p;
    bool       reset = false;
    if (argc == 2)
    {
        if (strcmp(argv[1], "reset") == 0)
        {
            reset = true;
        }
    }

    pMib = (uint32_t *) lmac_cmd_mib(0);

    printf("dot11_wep_excluded_count                %d\n", (int) pMib[DOT11_WEP_EXCLUDED_COUNT]);
    printf("dot11_fcs_error_count                   %d\n", (int) pMib[DOT11_FCS_ERROR_COUNT]);
    printf("rw_rx_phy_error_count                   %d\n", (int) pMib[RW_RX_PHY_ERROR_COUNT]);
    printf("rw_rd_fifo_overflow_count               %d\n", (int) pMib[RW_RD_FIFO_OVER_COUNT]);
    printf("rw_tx_underrun_count                    %d\n", (int) pMib[RW_TX_UNDERRUN_COUNT]);

    printf("rw_rx_mpif_overflow_count;              %d\n", (int) pMib[RW_RX_MPIF_OVERFLOW_COUNT]);
    p = &pMib[DOT11_QOS_FAILED_COUNT_8];
    printf("dot11_qos_failed_count[8]               %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);

    p = &pMib[DOT11_QOS_RETRY_COUNT_8];
    printf("dot11_qos_retry_count[8]                %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);
    p = &pMib[RW_QOS_ACK_FAILURE_COUNT_8];
    printf("rw_qos_ack_failure_count[8]             %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);
    p = &pMib[DOT11_QOS_RETRIES_RECEIVED_COUNT_8];
    printf("dot11_qos_retries_received_count[8]     %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);
    p = &pMib[DOT11_FAILED_AMSDU_COUNT_8];
    printf("dot11_failed_amsdu_count[8]             %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);
    p = &pMib[DOT11_RETRY_AMSDU_COUNT_8];
    printf("dot11_retry_amsdu_count[8]              %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);

    p = &pMib[DOT11_AMSDU_ACK_FAILURE_COUNT_8];
    printf("dot11_amsdu_ack_failure_count[8]        %d %d %d %d %d %d %d %d\n",
           (int) p[0],
           (int) p[1],
           (int) p[2],
           (int) p[3],
           (int) p[4],
           (int) p[5],
           (int) p[6],
           (int) p[7]);

    printf("dot11_transmitted_ampdu_count           %d\n", (int) pMib[DOT11_TRANSMITTED_AMPDU_COUNT]);
    printf("dot11_transmitted_mpdus_in_ampdu_count  %d\n", (int) pMib[DOT11_TRANSMITTED_MPDUS_IN_AMPDU_COUNT]);
    printf("dot11_mpdu_in_received_ampdu_count      %d\n", (int) pMib[DOT11_MPDU_IN_RECEIVED_AMPDU_COUNT]);
    printf("dot11_ampdu_delimiter_crc_error_count   %d\n", (int) pMib[DOT11_AMPDU_DELIMITER_CRC_ERROR_COUNT]);

    printf("dot11_20mhz_frame_transmitted_count     %d\n", (int) pMib[DOT11_20MHZ_FRAME_TRANSMITTED_COUNT]);
    printf("dot11_20mhz_frame_received_count        %d\n", (int) pMib[DOT11_20MHZ_FRAME_RECEIVED_COUNT]);
    printf("rw_failed_20mhz_txop                    %d\n", (int) pMib[RW_FAILED_20MHZ_TXOP]);
    printf("rw_successful_20mhz_txop                %d\n", (int) pMib[RW_SUCCESSFUL_20MHZ_TXOP]);

    printf("rw_dyn_bw_drop_count                    %d\n", (int) pMib[RW_DYN_BW_DROP_COUNT]);
    printf("rw_sta_bw_failed_count                  %d\n", (int) pMib[RW_STA_BW_FAILED_COUNT]);

    if (reset)
    {
        lmac_cmd_mib(reset);
        printf("Reset MIB count\n");
    }

    return true;
}

// -----------------------------------------------------------------------
// FW stats display
// -----------------------------------------------------------------------
 #ifdef __SUPPORT_WIFI_DBG__
static void print_stat (const char * label, uint32_t value) {
    printf("%-50s: %lu\n", label, value);
}

static void print_stat_array (const char * label, volatile uint32_t values[], int size) {
    printf("%-50s: ", label);
    for (int i = 0; i < size; i++)
    {
        printf("%lu ", values[i]);
    }

    printf("\n");
}

static void fw_stats_display_rx () {
    printf("==== RX Statistics ====\n");
    print_stat("Unencrypted frames discarded", lmac_cmd_get_machw_mib_dot11_wep_excluded_count());
    print_stat("FCS errors", lmac_cmd_get_machw_mib_dot11_fcs_error_count());
    print_stat("PHY errors", lmac_cmd_get_machw_mib_rw_rx_phy_error_count());
    print_stat("FIFO overflows", lmac_cmd_get_machw_mib_rw_rd_fifo_overflow_count());
    print_stat("MPIF FIFO overflows", lmac_cmd_get_machw_mib_rw_rx_mpif_overflow_count());

    print_stat_array("Unicast MPDUs received successfully [8]", lmac_cmd_get_machw_mib_rw_qos_ureceived_mpdu_count(),
                     8);
    print_stat_array("Group addressed MPDUs received successfully [8]",
                     lmac_cmd_get_machw_mib_rw_qos_greceived_mpdu_count(),
                     8);
    print_stat_array("Unicast MPDUs not destined to this device received successfully [8]",
                     lmac_cmd_get_machw_mib_rw_qos_ureceived_other_mpdu(),
                     8);
    print_stat_array("MPDUs received with retry bit set [8]",
                     lmac_cmd_get_machw_mib_dot11_qos_retries_received_count(),
                     8);
    print_stat_array("Unicast A-MSDUs received successfully [8]", lmac_cmd_get_machw_mib_rw_ureceived_amsdu_count(), 8);
    print_stat_array("Group addressed A-MSDUs received successfully [8]",
                     lmac_cmd_get_machw_mib_rw_greceived_amsdu_count(),
                     8);
    print_stat_array("Unicast A-MSDUs not destined to this device received successfully [8]",
                     lmac_cmd_get_machw_mib_rw_ureceived_other_amsdu(),
                     8);
    print_stat_array("Bytes in received A-MSDUs [8]", lmac_cmd_get_machw_mib_dot11_received_octets_in_amsdu_count(), 8);

    print_stat("Unicast A-MPDUs received", lmac_cmd_get_machw_mib_rw_uampdu_received_count());
    print_stat("Group addressed A-MPDUs received", lmac_cmd_get_machw_mib_rw_gampdu_received_count());
    print_stat("Unicast A-MPDUs received not destined to this device",
               lmac_cmd_get_machw_mib_rw_other_ampdu_received_count());
    print_stat("MPDUs received in A-MPDUs", lmac_cmd_get_machw_mib_dot11_mpdu_in_received_ampdu_count());
    print_stat("Bytes received in A-MPDUs", lmac_cmd_get_machw_mib_dot11_received_octets_in_ampdu_count());
    print_stat("CRC errors in MPDU delimiter of A-MPDU",
               lmac_cmd_get_machw_mib_dot11_ampdu_delimiter_crc_error_count());

    print_stat("Frames received at 20 MHz BW", lmac_cmd_get_machw_mib_dot11_20mhz_frame_received_count());
    print_stat("Frames received at 40 MHz BW", lmac_cmd_get_machw_mib_dot11_40mhz_frame_received_count());
    print_stat("Frames received at 80 MHz BW", lmac_cmd_get_machw_mib_dot11_80mhz_frame_received_count());
    print_stat("Frames received at 160 MHz BW", lmac_cmd_get_machw_mib_dot11_160mhz_frame_received_count());
    print_stat("Beamforming frames addressed to the device received",
               lmac_cmd_get_machw_mib_rw_beamforming_received_frame_count());
    print_stat("MU-MIMO frames addressed to the device received", lmac_cmd_get_machw_mib_rw_mu_received_framed_count());
    printf("========================\n");
}

static void fw_stats_display_tx () {
    printf("==== FW TX Statistics ====\n");
    print_stat("Transmit underrun", lmac_cmd_get_machw_mib_rw_tx_underrun_count());

    print_stat_array("Unicast transmitted MPDU [8]", lmac_cmd_get_machw_mib_rw_qos_utransmitted_mpdu_count(), 8);
    print_stat_array("Group addressed transmitted MPDU [8]", lmac_cmd_get_machw_mib_rw_qos_gtransmitted_mpdu_count(),
                     8);
    print_stat_array("MSDUs or MMPDUs discarded due to retry limit [8]",
                     lmac_cmd_get_machw_mib_dot11_qos_failed_count(),
                     8);
    print_stat_array("Unfragmented MSDUs or MMPDUs transmitted successfully with retries [8]",
                     lmac_cmd_get_machw_mib_dot11_qos_retry_count(),
                     8);
    print_stat_array("Successful RTS frame transmissions [8]", lmac_cmd_get_machw_mib_dot11_qos_rts_success_count(), 8);
    print_stat_array("Unsuccessful RTS frame transmissions [8]", lmac_cmd_get_machw_mib_dot11_qos_rts_failure_count(),
                     8);
    print_stat_array("MPDUs not received ACK [8]", lmac_cmd_get_machw_mib_rw_qos_ack_failure_count(), 8);
    print_stat_array("Unicast A-MSDUs transmitted successfully [8]",
                     lmac_cmd_get_machw_mib_rw_utransmitted_amsdu_count(),
                     8);
    print_stat_array("Group-addressed A-MSDUs transmitted successfully [8]",
                     lmac_cmd_get_machw_mib_rw_gtransmitted_amsdu_count(),
                     8);
    print_stat_array("A-MSDUs discarded due to retry limit [8]", lmac_cmd_get_machw_mib_dot11_failed_amsdu_count(), 8);
    print_stat_array("A-MSDUs transmitted successfully with retries [8]",
                     lmac_cmd_get_machw_mib_dot11_retry_amsdu_count(),
                     8);
    print_stat_array("Bytes transmitted in A-MSDUs [8]", lmac_cmd_get_machw_mib_dot11_transmitted_octets_in_amsdu(), 8);
    print_stat_array("A-MSDUs not receiving ACK [8]", lmac_cmd_get_machw_mib_dot11_amsdu_ack_failure_count(), 8);

    print_stat("A-MPDUs transmitted successfully", lmac_cmd_get_machw_mib_dot11_transmitted_ampdu_count());
    print_stat("MPDUs transmitted in A-MPDU", lmac_cmd_get_machw_mib_dot11_transmitted_mpdus_in_ampdu_count());
    print_stat("Bytes transmitted in A-MPDU", lmac_cmd_get_machw_mib_dot11_transmitted_octets_in_ampdu_count());
    print_stat("Frames transmitted at 20 MHz BW", lmac_cmd_get_machw_mib_dot11_20mhz_frame_transmitted_count());
    print_stat("Frames transmitted at 40 MHz BW", lmac_cmd_get_machw_mib_dot11_40mhz_frame_transmitted_count());
    print_stat("Frames transmitted at 80 MHz BW", lmac_cmd_get_machw_mib_dot11_80mhz_frame_transmitted_count());
    print_stat("Frames transmitted at 160 MHz BW", lmac_cmd_get_machw_mib_dot11_160mhz_frame_transmitted_count());
    print_stat("Beamforming frames transmitted", lmac_cmd_get_machw_mib_dot11_beamforming_frame_count());
    print_stat("Beamforming Report frames transmitted with SU reports",
               lmac_cmd_get_machw_mib_rw_su_bfr_transmitted_count());
    print_stat("Beamforming Report frames transmitted with MU reports",
               lmac_cmd_get_machw_mib_rw_mu_bfr_transmitted_count());
    printf("========================\n");
}

static void fw_stats_display_ps () {
  #if RWNX_SLEEP_DBG_EN
    uint32_t sleep_time = lmac_cmd_get_rwnx_total_sleep_time();
    uint32_t wake_time  = lmac_cmd_get_rwnx_total_wake_time();
    uint32_t sleep_cnt  = lmac_cmd_get_rwnx_sleep_cnt();
    uint32_t wakeup_cnt = lmac_cmd_get_rwnx_wakeup_cnt();
    printf("==== FW PS Statistics ====\n");
    print_stat("sleep count", sleep_cnt);
    print_stat("Wakeup count", wakeup_cnt);
    print_stat("Total sleep Time", sleep_time);
    print_stat("Total awake time", wake_time);
    print_stat("Avarge sleep time", sleep_time / sleep_cnt);
    print_stat("Avarge wake time", wake_time / wakeup_cnt);

    printf("========================\n");
  #else
    printf("FW PS Statistics are not supported!\n");
  #endif                               // #if RWNX_SLEEP_DBG_EN
}

static void fw_stats_clear_ps ()
{
    lmac_cmd_clear_ps_stats();
}

static bool cli_fw_stats (int argc, const char ** argv)
{
    for (int i = 0; i < argc; i++)
    {
        printf("argc = %d, argv[%d]= %s \n", argc, i, argv[i]);
    }

    if ((argc <= 3) && (strcmp(argv[0], "stats") == 0))
    {
        if (strcmp(argv[1], "tx") == 0)
        {
            fw_stats_display_tx();
        }
        else if (strcmp(argv[1], "rx") == 0)
        {
            fw_stats_display_rx();
        }
        else if (strcmp(argv[1], "ps") == 0)
        {
            if (strcmp(argv[2], "clear") == 0)
            {
                fw_stats_clear_ps();
                printf("ps stats cleared!\n");
            }
            else
            {
                fw_stats_display_ps();
            }
        }
    }
    else
    {
        printf("\n\r Wrong stats command format!"
               "\n\r Commands for fw tx stats: \"stats tx\""
               "\n\r Commands for fw rx stats: \"stats rx\""
               "\n\r Commands for fw ps stats: \"stats ps <clear>\"\n");
    }

    return true;
}

 #endif                                /* __SUPPORT_WIFI_DBG__ */

 #ifdef TIMP_CMD_EN
static bool cmd_timp (int argc, const char * argv[])
{
    uint8_t usage_en = 1;
    int64_t c;

    switch (argc)
    {
        case 2:
        {
            if (strcmp("u", argv[1]) == 0)
            {
                uint32_t s;

                c = R_BSP_SystemRtcCountGet();
                s = romac4rtos_timp_update(c);

                printf("%s::update clk=%ld, %02lx\n", argv[0], (uint32_t) c, s);
                printf("%s::interface_0 updatetd %ld, weak_sig: %ld\n",
                       argv[0],
                       s & 0x01,       // TIMP_CHK_UPDATED(TIMP_INTERFACE_0, s)
                       s & 0x10        // TIMP_CHK_WEAK_SIG(TIMP_INTERFACE_0, s)
                       );
                printf("%s::interface_1 updatetd %ld, weak_sig: %ld\n",
                       argv[0],
                       s & 0x02,       // TIMP_CHK_UPDATED(TIMP_INTERFACE_1, s)
                       s & 0x20        // TIMP_CHK_WEAK_SIG(TIMP_INTERFACE_1, s)
                       );
                romac4rtos_timp_stat(s);
                usage_en = 0;
            }
            else if (strcmp("U", argv[1]) == 0)
            {
                uint32_t s;

                c = R_BSP_WakeupSourceGet();
                s = romac4rtos_timp_update(1);
                romac4rtos_timp_stat(s);

                printf("%s::update clk=%ld, %02lx\n", argv[0], (uint32_t) c, s);
                printf("%s::interface_0 updatetd %ld, weak_sig: %ld\n",
                       argv[0],
                       s & 0x01,       // TIMP_CHK_UPDATED(TIMP_INTERFACE_0, s)
                       s & 0x10        // TIMP_CHK_WEAK_SIG(TIMP_INTERFACE_0, s)
                       );
                printf("%s::interface_1 updatetd %ld, weak_sig: %ld\n",
                       argv[0],
                       s & 0x02,       // TIMP_CHK_UPDATED(TIMP_INTERFACE_1, s)
                       s & 0x20        // TIMP_CHK_WEAK_SIG(TIMP_INTERFACE_1, s)
                       );
                usage_en = 0;
            }
            else if (strcmp("i", argv[1]) == 0)
            {
                romac4rtos_timp_init();
                printf("%s::init\n", argv[0]);
                usage_en = 0;
            }

            break;
        }

        case 3:
        {
            if (strcmp("r", argv[1]) == 0)
            {
                c = R_BSP_WakeupSourceGet();
                romac4rtos_timp_reset(atoi(argv[2]), c);
                printf("%s::reset %d clk=%ld\n", argv[0], atoi(argv[2]), (uint32_t) c);
                usage_en = 0;
            }

            break;
        }

        case 4:
        {
            if (strcmp("p", argv[1]) == 0)
            {
                romac4rtos_timp_period((uint8_t) atoi(argv[2]), atoi(argv[3]));

                printf("%s::set_period %d=%d\n", argv[0], atoi(argv[2]), atoi(argv[3]));
                usage_en = 0;
            }

            break;
        }

        case 1:
        {
            break;
        }
    }

    if (usage_en)
    {
        printf("%s [p] [interface] [period]: set period\n", argv[0]);
        printf("%s [i]                     : timp initialization\n", argv[0]);
        printf("%s [r] [interface]         : timp reset\n", argv[0]);
        printf("%s [u]                     : timp update (Auto)\n", argv[0]);
        printf("%s [U]                     : timp update (Manual)\n", argv[0]);
    }

    return pdTRUE;
}

 #endif                                /* TIMP_CMD_EN */

// -----------------------------------------------------------------------
// RFTX Functions
// -----------------------------------------------------------------------
TaskHandle_t tx_task_h = NULL;

int         txp_on_off = 0;
struct RFTX param      =
{
    .freq          = 2412,
    .numFrames     = 100000,
    .frameLen      = 100,
    .txRate        = 0,
    .txPower       = 0,
    .destAddr      = {{0x2010, 0x4030, 0x6050}},
    .bssid         = {{0x8070, 0xa090, 0xc0b0}},
    .htEnable      = 0,
    .greenField    = 0,
    .preambleType  = 0,
    .qosEnable     = 0,
    .ackPolicy     = 0,
    .scrambler     = 0,
    .aifsnval      = 1,
    .ant           = 0,
    .BW            = 20,
    .tx_timeout    = 0,
    .cca_threshold = 0,
    .cca_timeout   = 0,
    .data_length   = 0,
    .data          = {0,    }
};

static void rftx_thread (void * pvParameters)
{
    lmac_cmd_rftx(pvParameters);
    txp_on_off = 0;
    lmac_cmd_state_set(LMAC_HW_IDLE);
    vTaskDelete(tx_task_h);
}

void rftx_stop ()
{
    lmac_ppm_recomp_timer_stop();
    lmac_cmd_rftx_stop();
    txp_on_off = 0;

    lmac_cmd_state_set(LMAC_HW_IDLE);
}

/* str2rate
 * b1 b2 b5.5 b11
 * g6 g9 g12 g18 g24 g36 g48 g54
 * n6.5 n13 n19.5 n26 n39 n52 n58.5 n65
 * ac ax-su ax-er
 */
static uint32_t str2rate (const char * str, uint8_t GI, uint8_t greenField, uint8_t preambleType, uint32_t * data_size)
{
    uint8_t invalid        = 0;
    uint8_t mcsIndexTxRCX  = 0;
    uint8_t preTypeTxRCX   = 0;        /* SHORT 0, LONG 1*/
    uint8_t formatModTxRCX = 0;        /* NON-HT 0,NON-HT-DUP-OFDM 1,HT-MF 2, HT-GF 3, VHT 4*/

    // uint8_t protection = 0; /* no protection */
    uint8_t  nss          = 0;
    uint8_t  index        = 0;
    uint32_t rate_control = 0;
    uint32_t size         = 0;

    if (str[0] == 'b')                 /* 11b */
    {
        formatModTxRCX = 0;            /* NON-HT */
        if (str[1] == '1')
        {
            mcsIndexTxRCX = 0, size = 50;
            if (str[2] == '1')
            {
                mcsIndexTxRCX = 3, size = 600;
            }
        }
        else if (str[1] == '2')
        {
            mcsIndexTxRCX = 1, size = 100;
        }
        else if ((str[1] == '5') && (str[3] == '5'))
        {
            mcsIndexTxRCX = 2, size = 300;
        }
        else
        {
            invalid = 1;
        }
    }
    else if (str[0] == 'g')            /* 11g */
    {
        formatModTxRCX = 0;            /* NON-HT */
        if (str[1] == '6')
        {
            mcsIndexTxRCX = 4, size = 500;
        }
        else if (str[1] == '9')
        {
            mcsIndexTxRCX = 5, size = 700;
        }
        else if (str[1] == '1')
        {
            if (str[2] == '2')
            {
                mcsIndexTxRCX = 6, size = 1000;
            }
            else if (str[2] == '8')
            {
                mcsIndexTxRCX = 7, size = 1400;
            }
            else
            {
                invalid = 1;
            }
        }
        else if ((str[1] == '2') && (str[2] == '4'))
        {
            mcsIndexTxRCX = 8, size = 1800;
        }
        else if ((str[1] == '3') && (str[2] == '6'))
        {
            mcsIndexTxRCX = 9, size = 3000;
        }
        else if ((str[1] == '4') && (str[2] == '8'))
        {
            mcsIndexTxRCX = 10, size = 4000;
        }
        else if ((str[1] == '5') && (str[2] == '4'))
        {
            mcsIndexTxRCX = 11, size = 4000;
        }
        else
        {
            invalid = 1;
        }
    }
    else if (str[0] == 'n')            /* 11n */
    {
        if (greenField)
        {
            formatModTxRCX = 3;        /* HT-GF  */
        }
        else
        {
            formatModTxRCX = 2;        /* HT-MF  */
        }

        mcsIndexTxRCX = atoi(&str[1]), size = 500;
    }
    else if ((str[0] == 'a') && (str[1] == 'c')) /* 11ac */
    {
        formatModTxRCX = 4;                      /* VHT  */

        // nss=atoi(&str[3]);
        nss          = 0;
        GI           = atoi(&str[3]);
        preambleType = 0;
        index        = 5;

        mcsIndexTxRCX = atoi(&str[index]), size = 500;

        mcsIndexTxRCX |= (nss & 0x3) << 4;
    }
    else if (strncmp(&str[0], "ax-su", 5) == 0) /* 11ax */
    {
        formatModTxRCX = 5;                     /* HE */

        nss          = 0;
        preambleType = 0;
        GI           = atoi(&str[6]);

        index         = 8;
        mcsIndexTxRCX = atoi(&str[index]), size = 500;

        mcsIndexTxRCX |= (nss & 0x3) << 4;
    }
    else if (strncmp(&str[0], "axsu", 4) == 0) /* 11ax short arguments */
    {
        formatModTxRCX = 5;                    /* HE */

        nss          = 0;
        preambleType = 0;
        GI           = 0;                      // fixed

        index         = 4;
        mcsIndexTxRCX = atoi(&str[index]), size = 500;

        mcsIndexTxRCX |= (nss & 0x3) << 4;
    }
    else if (strncmp(&str[0], "ax-er", 5) == 0) /* 11ax */
    {
        formatModTxRCX = 7;                     /* HE-er */

        nss          = 0;
        preambleType = 0;
        GI           = atoi(&str[6]);

        index         = 8;
        mcsIndexTxRCX = atoi(&str[index]), size = 500;

        mcsIndexTxRCX |= (nss & 0x3) << 4;
    }
    else if (strncmp(&str[0], "axer", 4) == 0) /* 11ax */
    {
        formatModTxRCX = 7;                    /* HE-er */

        nss          = 0;
        preambleType = 0;
        GI           = 0;

        index         = 4;
        mcsIndexTxRCX = atoi(&str[index]), size = 500;

        mcsIndexTxRCX |= (nss & 0x3) << 4;
    }
    else if ((str[0] == '0') && (str[1] == 'x')) /* rate value with hex */
    {
        rate_control = htoi((char *) &str[2]);

        return rate_control;
    }
    else
    {
        invalid = 1;
    }

    if (invalid)
    {
        rate_control = 0;
        size         = 50;
        printf("Wrong txRate. Force to DSSS 1MHz\n");
    }
    else
    {
        rate_control = ((formatModTxRCX & 0x7) << 11) | ((preTypeTxRCX & 0x1) << 10) |
                       (mcsIndexTxRCX & 0x7f);
        rate_control = rate_control | (GI << 9) | (preambleType << 10);
    }

    if (data_size != 0)
    {
        *data_size = size;
    }

    return rate_control;
}

static bool detect_high_rate (const char * str)
{
    int val;

    if (str[0] == 'g')                 /* 11g */
    {
        val = atoi(&str[1]);
        if (val > 36)
        {
            return true;
        }
    }
    else if (str[0] == 'n')            /* 11n */
    {
        val = atoi(&str[1]);
        if (val >= 5)
        {
            return true;
        }
    }
    else if ((strncmp(str, "ax-su", 5) == 0) || (strncmp(str, "ax-er", 5) == 0)) /* 11ax */
    {
        val = atoi(&str[8]);
        if (val >= 5)
        {
            return true;
        }
    }
    else if (strncmp(str, "axsu", 4) == 0) /* 11ax short arguments */
    {
        val = atoi(&str[4]);
        if (val >= 5)
        {
            return true;
        }
    }

    return false;
}

static uint16_t co_bswap16 (uint16_t val16)
{
    return ((val16 << 8) & 0xFF00) | ((val16 >> 8) & 0xFF);
}

static int str2macaddr (struct mac_addr * mac, const char * macaddr)
{
    uint32_t status = 0;
    uint32_t len;
    char     tmp_macstr[13];
    char   * pTemp;
    memset(tmp_macstr, 0, 13);
    len = strlen(macaddr);

    if ((len == 17) || (len == 12))
    {
        if (len == 12)
        {
            memcpy(tmp_macstr, macaddr, 12);
        }
        else
        {
            sprintf(tmp_macstr,
                    "%c%c%c%c%c%c%c%c%c%c%c%c",
                    macaddr[0],
                    macaddr[1],
                    macaddr[3],
                    macaddr[4],
                    macaddr[6],
                    macaddr[7],
                    macaddr[9],
                    macaddr[10],
                    macaddr[12],
                    macaddr[13],
                    macaddr[15],
                    macaddr[16]);
        }

        mac->array[2] = co_bswap16(strtol(&tmp_macstr[8], &pTemp, 16));
        tmp_macstr[8] = '\0';
        mac->array[1] = co_bswap16(strtol(&tmp_macstr[4], &pTemp, 16));
        tmp_macstr[4] = '\0';
        mac->array[0] = co_bswap16(strtol(&tmp_macstr[0], &pTemp, 16));
    }
    else
    {
        printf("Wrong MAC Address Type !!!\n");
        status = 1;
    }

    return status;
}

bool cmd_lmac_rftx (int argc, const char ** argv)
{
    switch (argc)
    {
        case 18:
        {
            param.ant = atoi(argv[17]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 17:
        {
            param.aifsnval = atoi(argv[16]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 16:
        {
            param.scrambler = atoi(argv[15]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 15:
        {
            /* MAC Control Information 1 */
            /*0: No-ACK, 1: Normal ACK, 2: BA, 3: Compressed BA */
            if (strcmp(argv[14], "NO") == 0)
            {
                param.ackPolicy = 0;
            }
            else if (strcmp(argv[14], "NORM") == 0)
            {
                param.ackPolicy = 1;
            }
            else if (strcmp(argv[14], "BA") == 0)
            {
                param.ackPolicy = 2;
            }
            else if (strcmp(argv[14], "CBA") == 0)
            {
                param.ackPolicy = 3;
            }
            else
            {
                printf("Wrong ackPolicy. [NO|NORM|BA|CBA]\n"); // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 14:
        {
            if (strcmp(argv[13], "on") == 0) /* 1(on), 0(off): qosEnable */
            {
                param.qosEnable = 1;
            }
            else if (strcmp(argv[13], "off") == 0)
            {
                param.qosEnable = 0;
            }
            else
            {
                printf("Wrong qosEnable. [off|on]\n"); // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 13:
        {
            if (strcmp("long", argv[12]) == 0) /* 1(long), 0(short) */
            {
                param.preambleType = 1;
            }
            else if (strcmp("short", argv[12]) == 0)
            {
                param.preambleType = 0;
            }
            else
            {
                printf("Wrong preambleType. [short|long]\n"); // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 12:
        {
            if (strcmp(argv[11], "on") == 0) /* 1(on), 0(off): GreenField */
            {
                param.greenField = 1;
            }
            else if (strcmp(argv[11], "off") == 0)
            {
                param.greenField = 0;
            }
            else
            {
                printf("Wrong greenField. [off|on]\n"); // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 11:
        {
            if (strcmp(argv[10], "short") == 0) /* 1(short),0(long): shortGI */
            {
                param.GI = 1;
            }
            else if (strcmp(argv[10], "long") == 0)
            {
                param.GI = 0;
            }
            else
            {
                printf("Wrong GI. [long|short]\n"); // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 10:
        {
            param.htEnable = atoi(argv[9]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 9:
        {
            if (str2macaddr(&param.bssid, argv[8]))
            {
                return false;          // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 8:
        {
            if (str2macaddr(&param.destAddr, argv[7]))
            {
                return false;          // @suppress("No break at end of case")
            }

            [[fallthrough]];
        }

        case 7:
        {
            if (*argv[6] == 'p')
            {
                const char * para = argv[6];
                param.txPower &= 0xfffcffff;
                param.txPower |= (atoi(&para[2]) & 0x3) << 16;
                if ((para[3] == '_') || (para[3] == ';'))
                {
                    param.txPower &= 0xffffff00;
                    param.txPower |= atoi(&para[4]) & 0xff;
                }
                else
                {
                    param.txPower &= 0xffffff00;
                    param.txPower |= 25 & 0xff;
                }
            }
            else
            {
                param.txPower &= 0xffffff00;
                param.txPower |= atoi(argv[6]) & 0xff;
            }                          // @suppress("No break at end of case")

            printf(" power LTF=%ld, level=%d total=%08lx \n",
                   param.txPower >> 16 & 0x3,
                   (int8_t) (param.txPower & 0xff),
                   param.txPower);
            [[fallthrough]];
        }

        case 6:
        {
            param.txRate = str2rate(argv[5], param.GI, param.greenField, param.preambleType, 0); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 5:
        {
            param.frameLen = atoi(argv[4]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 4:
        {
            param.numFrames = atoi(argv[3]); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 3:
        {
            param.BW = atoi(argv[2]);  // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 2:
        {
            param.freq = atoi(argv[1]); // @suppress("No break at end of case")
            break;
        }

        default:

            return false;
    }

    param.high_rate = (param.txRate == 0) ? false : detect_high_rate(argv[5]);

    if (param.greenField && param.GI)
    {
        printf(" Not Supported Mode !!!\n");

        return false;
    }

    if (txp_on_off == 0)
    {
        lmac_apply_ppm_compensation_for_tx();
        lmac_ppm_recomp_timer_start();
        printf("RFtx Start.\n");
        txp_on_off = 1;
        xTaskCreate(rftx_thread,
                    "RFTX Thread",
 #if (dg_configSYSTEMVIEW == 0)
                    (((400 * sizeof(StackType_t)) - 1) / sizeof(StackType_t) + 1),
 #else                                 /* (dg_configSYSTEMVIEW == 1) */
                    (((400 * sizeof(StackType_t) + dg_configSYSTEMVIEW_STACK_OVERHEAD) - 1) / sizeof(StackType_t) + 1),
 #endif /* (dg_configSYSTEMVIEW == 1) */
                    (void *) (&param),
                    CLI_TASK_PRIORITY,
                    &tx_task_h);
    }
    else if (txp_on_off == 1)
    {
        printf("TX ongoing now.\n");
    }

    return true;
}

static uint32_t parse_data (const char * string, uint8_t * data)
{
    const char * p      = string;
    int          length = strlen(string) / 2;

    if (length > TX_MAX_DATA_LENG)
    {
        return -1;
    }

    for (int i = 0; i < length; i++)
    {
        char byte_str[3];
        strncpy(byte_str, p, 2);
        data[i] = htoi(byte_str);
        p      += 2;
    }

    return length;
}

int cmd_lmac_rftxpkt (int argc, const char ** argv)
{
    int ret;
    switch (argc)
    {
        case 9:                        /* Data ASCII encoded */
        {
            param.data_length = parse_data(argv[8], param.data);
            if ((int) param.data_length == -1)
            {
                printf("Data length over %d\n", TX_MAX_DATA_LENG);

                return -1;
            }

            /* Minimum 23 bytes — HW constraint. */
            if (param.data_length < 23)
            {
                printf("Data too short (%lu bytes), minimum 23\n", (unsigned long) param.data_length);

                return -1;
            }

            [[fallthrough]];
        }

        case 8:                        /* TX timeOut */
        {
            param.tx_timeout = atoi(argv[7]);
            [[fallthrough]];
        }

        case 7:                        /* CCA Timeout */
        {
            param.cca_timeout = (uint32_t) atoi(argv[6]);
            [[fallthrough]];
        }

        case 6:                        /* CCA Threshold */
        {
            param.cca_threshold = atoi(argv[5]);
            [[fallthrough]];
        }

        case 5:                        /* TX power */
        {
            if (*argv[4] == 'p')
            {
                const char * para = argv[4];
                param.txPower &= 0xfffcffff;
                param.txPower |= (atoi(&para[2]) & 0x3) << 16;
                if ((para[3] == '_') || (para[3] == ';'))
                {
                    param.txPower &= 0xffffff00;
                    param.txPower |= atoi(&para[4]) & 0xff;
                }
                else
                {
                    param.txPower &= 0xffffff00;
                    param.txPower |= 25 & 0xff;
                }
            }
            else
            {
                param.txPower &= 0xffffff00;
                param.txPower |= atoi(argv[4]) & 0xff;
            }                          // @suppress("No break at end of case")

            [[fallthrough]];
        }

        case 4:                                                                                  /* TX Rate */
        {
            param.txRate = str2rate(argv[3], param.GI, param.greenField, param.preambleType, 0); // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 3:                                                                                  /* BW, Not impacted any valude only 20MHz fixed */
        {
            param.BW = atoi(argv[2]);                                                            // @suppress("No break at end of case")
            [[fallthrough]];
        }

        case 2:                                                                                  /* Channel Frequency */
        {
            param.freq = atoi(argv[1]);                                                          // @suppress("No break at end of case")
            break;
        }

        default:

            return -1;
    }

    if (param.greenField && param.GI)
    {
        printf(" Not Supported Mode !!!\n");

        return -1;
    }

    lmac_apply_ppm_compensation_for_tx();

    ret = lmac_cmd_rftxpkt(&param);

    lmac_cmd_state_set(LMAC_HW_IDLE);

    return ret;
}

static bool lmac_rftx (int argc, const char ** argv)
{
    if (argc == 2)
    {
        if (!strcmp(argv[1], "stop"))
        {
            rftx_stop();
            printf("RFTX Stop\n");

            return true;
        }
    }

    return cmd_lmac_rftx(argc, &argv[0]);
}

static bool lmac_rftxpkt (int argc, const char ** argv)
{
    int ret = cmd_lmac_rftxpkt(argc, &argv[0]);

    if (ret == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool lmac_tx_scale (int argc, const char ** argv)
{
    return lmac_cmd_tx_scale(argc, &argv[0]);
}

int cmd_lmac_scale_mode (int argc, const char ** argv)
{
    return lmac_cmd_scale_mode(argc, argv);
}

static bool lmac_scale_mode (int argc, const char ** argv)
{
    int ret = cmd_lmac_scale_mode(argc, &argv[0]);

    if (ret == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool lmac_rfcw (int argc, const char ** argv)
{
    uint32_t freq = 2412;

    if (strcmp(argv[1], "start") == 0)
    {
        lmac_apply_ppm_compensation_for_tx();
        lmac_ppm_recomp_timer_start();
        if (argc > 2)
        {
            freq = atoi(argv[2]);
            if (freq < 5000)
            {
                // rf tx 2.4G
                REG_PL_WR(0x60D00030, 0x80);
                REG_PL_WR(0x60d0000c, 0x160);
                REG_PL_WR(0x60d2000c, 0x160);
                REG_PL_WR(0x60d20018, 0x80);
                REG_PL_WR(0x60C0F014, 0x68686868);
            }
            else
            {
                // rf tx 5G
                REG_PL_WR(0x60D00030, 0x1cc);
                REG_PL_WR(0x60D0000C, 0x170);
                REG_PL_WR(0x60D2000C, 0x170);
                REG_PL_WR(0x60D20018, 0x180);
                REG_PL_WR(0x60C0F014, 0x68686868);
            }

            lmac_cmd_channel_frequncy_set(freq);
            REG_PL_WR(0x60C03820, 0x1f);
            REG_PL_WR(0x60d30428, 0x1);
            REG_PL_WR(0x60C0F070, 0x100003c);
            REG_PL_WR(0x60C0F074, 0x003003c);
            REG_PL_WR(0x60D30440, 0xf1);
            REG_PL_WR(0x60C0F014, 0x55555555);
            REG_PL_WR(0x60D00030, 0x01ff);
            REG_PL_WR(0x60d30120, 0x80);
            REG_PL_WR(0x60d3042c, 0x3ff1);
        }

        printf("RFCW TX Start\n");
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        lmac_ppm_recomp_timer_stop();

        /* restore register */
        REG_PL_WR(0x60C0F014, 0x13131313);

        /* CW ON */
        REG_PL_WR(0x60c0f070, 0x3c);
        REG_PL_WR(0x60d00030, 0x2);
        REG_PL_WR(0x60d0000c, 0x0);
        REG_PL_WR(0x60d2000c, 0x0);
        REG_PL_WR(0x60d20018, 0x0);

        /* PLL restore */
        lmac_cmd_rf_cw_pll_restore();
        printf("RFCW TX Stop\n");
    }

    return true;
}

// -----------------------------------------------------------------------
// PER Functions
// -----------------------------------------------------------------------
int          duration               = 100;
int          total_received_packets = 0;
int          total_errored_packet   = 0;
int          errored_packet         = 0;
int          received_packet        = 0;
int          phy_error_acc          = 0;
int          rx_ovfl_acc            = 0;
int          phy_error              = 0;
int          rx_ovfl                = 0;
unsigned int pktperduration         = 10;
unsigned int timer_count            = 0;
unsigned int bytesperpkt            = 1000;
unsigned int timer_reset_flag       = 0;
unsigned int per_loop               = 0;

const char terminal_color_red[6]    = {27, '[', '9', '1', 'm', 0};
const char terminal_color_normal[4] = {27, '[', 'm', 0};
const char terminal_color_blue[6]   = {27, '[', '9', '4', 'm', 0};
const char terminal_color_yellow[6] = {27, '[', '9', '3', 'm', 0};

const char terminal_clear[5]          = {27, '[', '1', 'J', 0};
const char terminal_top_left[5]       = {27, '[', '1', 'H', 0};
const char terminal_save_cursor[4]    = {27, '[', 's', 0};
const char terminal_restore_cursor[4] = {27, '[', 'u', 0};

bool per_flag = false;

 #define PRECISION     10000
 #define PRINT_GAIN    0
static void print_float (const char * str, float floatNumber)
{
    int intPart     = (int) floatNumber;
    int decimalPart = (int) ((floatNumber - intPart) * PRECISION);
    if (decimalPart < 0)
    {
        decimalPart = decimalPart * -1;
    }

    printf("%s%d.%04d\n", str, intPart, decimalPart);
}

static void display_per (uint32_t * pMib)
{
    float        per = 0., per_total = 0., per_fixed = 0., per_total_fixed = 0., pkt_estimated = 0.;
    unsigned int rcvd = 0, errored = 0, phy_error_tot = 0, rx_ovfl_tot = 0;
 #if PRINT_GAIN
    unsigned int vga_gain = 0, lna_gain = 0;
 #endif                                // PRINT_GAIN

    timer_count++;
    rcvd          = pMib[DOT11_20MHZ_FRAME_RECEIVED_COUNT];
    errored       = pMib[DOT11_FCS_ERROR_COUNT];
    phy_error_tot = pMib[RW_RX_PHY_ERROR_COUNT];
    rx_ovfl_tot   = pMib[RW_RD_FIFO_OVER_COUNT];

 #if PRINT_GAIN                        /* No RWNX_MODEM_AGCSTAT register for DA16400 */
    vga_gain = *((unsigned int *) (RWNX_MODEM_AGCSTAT));
    lna_gain = (vga_gain >> 5) % 0x3;
    vga_gain = vga_gain & 0x1f;
 #endif
    received_packet        = (int) rcvd - total_received_packets;
    errored_packet         = (int) errored - total_errored_packet;
    total_received_packets = (int) rcvd;
    total_errored_packet   = (int) errored;

    phy_error     = (int) phy_error_tot - phy_error_acc;
    rx_ovfl       = (int) rx_ovfl_tot - rx_ovfl_acc;
    phy_error_acc = (int) phy_error_tot;
    rx_ovfl_acc   = (int) rx_ovfl_tot;

    if (phy_error < 0)
    {
        phy_error = 0;
    }

    if (rx_ovfl < 0)
    {
        rx_ovfl = 0;
    }

    if (received_packet < 0)
    {
        received_packet = 0;
    }

    if (errored_packet < 0)
    {
        errored_packet = 0;
    }

    if (received_packet + errored_packet + phy_error + rx_ovfl)
    {
        per = (float) (errored_packet + phy_error + rx_ovfl) /
              (float) (received_packet + errored_packet + phy_error + rx_ovfl);
    }
    else
    {
        per = 999;
    }

    if (total_received_packets + total_errored_packet + phy_error_acc + rx_ovfl_acc)
    {
        per_total = (float) (total_errored_packet + phy_error_acc + rx_ovfl_acc) /
                    (float) (total_received_packets + total_errored_packet + phy_error_acc +
                             rx_ovfl_acc);
    }
    else
    {
        per_total = 999;
    }

    per_fixed       = ((float) pktperduration - (float) received_packet) / (float) pktperduration;
    pkt_estimated   = (float) (timer_count * pktperduration);
    per_total_fixed = (float) (pkt_estimated - (float) total_received_packets) / pkt_estimated;

    printf("%s%s", terminal_clear, terminal_top_left);
    printf("phyerroracc = %8d , phyerror     = %8d\n", phy_error_acc, phy_error);
    printf("errored acc = %8d , errored pkts = %8d\n", total_errored_packet, errored_packet);
    printf("rxovflowacc = %8d , rxovflow     = %8d\n", rx_ovfl_acc, rx_ovfl);
    printf("passed acc  = %8d , passed pkts  = %8d\n", total_received_packets, received_packet);
    printf("rcvd acc    = %8d , rcvd total   = %8d\n",
           total_received_packets + total_errored_packet + phy_error_acc + rx_ovfl_acc,
           received_packet + errored_packet + phy_error + rx_ovfl);
 #if PRINT_GAIN
    printf("vga_gain    = %8d , lna_gain     = %8d\n", vga_gain, lna_gain);
 #endif
    if ((per_total > 0.1) || ((int) per_total == 999))
    {
        printf("%s", terminal_color_red);
    }
    else
    {
        printf("%s", terminal_color_blue);
    }

    print_float("per acc     = ", per_total);

    if ((per > 0.1) || ((int) per == 999))
    {
        printf("%s", terminal_color_red);
    }
    else
    {
        printf("%s", terminal_color_blue);
    }

    print_float("per         = ", per);

    if ((per_total_fixed > 0.1) || ((int) per_total_fixed == 999))
    {
        printf("%s", terminal_color_red);
    }
    else
    {
        printf("%s", terminal_color_blue);
    }

    print_float("perfixedacc = ", per_total_fixed);

    if ((per_fixed > 0.1) || ((int) per_fixed == 999))
    {
        printf("%s", terminal_color_red);
    }
    else
    {
        printf("%s", terminal_color_blue);
    }

    print_float("per fixed   = ", per_fixed);

    printf("%s", terminal_color_normal);
    printf("\nmeasure time= %8d , pkt/duration = %8d , throughput   = %3dMbps\n", duration, pktperduration,
           (unsigned int) received_packet * bytesperpkt * 8 / 1000000);
}

static void per_thread (void * pvParameters)
{
    bool       mib_reset_flag = true;
    uint32_t * pMib           = (uint32_t *) lmac_cmd_mib(mib_reset_flag);

    RA6W1_UNUSED_ARG(pvParameters);
    while (per_flag)
    {
        vTaskDelay(duration);
        display_per(pMib);
    }

    vTaskDelete(NULL);
}

static bool lmac_per (int argc, const char ** argv)
{
    bool ret            = false;
    bool mib_reset_flag = true;
    if (argc == 2)
    {
        if (!strcmp(argv[1], "start"))
        {
            per_flag    = true;
            timer_count = 0;
            TaskHandle_t per_task_h = NULL;
            xTaskCreate(per_thread,
                        "PER Thread",
 #if (dg_configSYSTEMVIEW == 0)
                        (((400 * sizeof(StackType_t)) - 1) / sizeof(StackType_t) + 1),
 #else                                 /* (dg_configSYSTEMVIEW == 1) */
                        (((400 * sizeof(StackType_t) + dg_configSYSTEMVIEW_STACK_OVERHEAD) - 1) / sizeof(StackType_t) +
                         1),
 #endif /* (dg_configSYSTEMVIEW == 1) */
                        NULL,
                        CLI_TASK_PRIORITY,
                        &per_task_h);
            ret = true;
        }
        else if (!strcmp(argv[1], "reset"))
        {
            lmac_cmd_mib(mib_reset_flag);
            ret = true;
        }
        else if (!strcmp(argv[1], "stop"))
        {
            per_flag = false;
            ret      = true;
        }
    }
    else if (argc == 3)
    {
        if (!strcmp(argv[1], "duration"))
        {
            duration = atoi(argv[2]);
            if (duration == 0)
            {
                duration = 100;
            }

            printf("SET duration %d\n", duration);
            ret = true;
        }
        else if (!strcmp(argv[1], "pkt"))
        {
            pktperduration = (unsigned int) (atoi(argv[2]));
            if (pktperduration == 0)
            {
                pktperduration = 10;
            }

            printf("SET pktperduration %d\n", pktperduration);
            ret = true;
        }
        else if (!strcmp(argv[1], "len"))
        {
            bytesperpkt = (unsigned int) (atoi(argv[2]));
            if (bytesperpkt == 0)
            {
                bytesperpkt = 1000;
            }

            printf("SET bytesperpkt %d\n", bytesperpkt);
            ret = true;
        }
    }

    return ret;
}

static bool lmac_power (int argc, const char ** argv)
{
    uint8_t ofdmminpwrlevel = 0;
    uint8_t dsssmaxpwrlevel = 0;
    uint8_t ofdmmaxpwrlevel = 0;

    if (argc == 4)
    {
        ofdmminpwrlevel = htoi((char *) argv[1]);
        dsssmaxpwrlevel = htoi((char *) argv[2]);
        ofdmmaxpwrlevel = htoi((char *) argv[3]);

        lmac_cmd_power_set(ofdmminpwrlevel, dsssmaxpwrlevel, ofdmmaxpwrlevel);
    }

    lmac_cmd_power_get(&ofdmminpwrlevel, &dsssmaxpwrlevel, &ofdmmaxpwrlevel);
    printf("OFDM MIN PWR LEVEL 0x%x\n", ofdmminpwrlevel);
    printf("DSSS MAX PWR LEVEL 0x%x\n", dsssmaxpwrlevel);
    printf("OFDM MAX PWR LEVEL 0x%x\n", ofdmmaxpwrlevel);

    return true;
}

static bool lmac_ch (int argc, const char ** argv)
{
    uint32_t channel   = 1;            /* RF channel number */
    uint32_t band      = 0;            /* RF band 0: auto, 2: 2G, 5: 5G */
    uint32_t frequency = 0;

    switch (argc)
    {
        case 3:
        {
            if (!parse_u32(argv[2], &band))
            {
                return false;
            }                          // @suppress("No break at end of case")

            [[fallthrough]];
        }

        case 2:
        {
            if (!parse_u32(argv[1], &channel))
            {
                return false;
            }

            break;
        }

        case 1:
        {
            uint16_t ch, freq;
            lmac_cmd_channel_get(&ch, &freq);
            printf("Current RF CH %d, FREQ %d\n", ch, freq);

            return true;
        }

        default:

            return false;
    }

    frequency = lmac_cmd_channel_set(band, channel);

    if (frequency == 0)
    {
        printf("Wrong band(%d) and channel index(%d) value.\n", (int) band, (int) channel);

        return false;
    }

    printf("Set RF CH %ld, FREQ %d\n", channel, (int) frequency);

    return true;
}

static bool lmac_freq (int argc, const char ** argv)
{
    uint32_t freq = 2412;

    switch (argc)
    {
        case 2:
        {
            if (!parse_u32(argv[1], &freq))
            {
                return false;
            }

            break;
        }

        case 1:
        {
            uint16_t ch, freq16;
            lmac_cmd_channel_get(&ch, &freq16);
            printf("Current RF CH %d, FREQ %d\n", ch, freq16);

            return true;
        }

        default:

            return false;
    }

    lmac_cmd_channel_frequncy_set(freq);

    printf("Set RF FREQ %d\n", (int) freq);

    return true;
}

static bool lmac_dbg (int argc, const char ** argv)
{
    int severity, module;
    if (argc == 3)
    {
        severity = atoi(argv[1]);
        module   = htoi((char *) argv[2]);
    }
    else
    {
        return false;
    }

    if (severity > 5)
    {
        return false;
    }

    lmac_cmd_dbg(severity, module);

    printf("dbg filter severity value %d [0:NONE, 1:CRT, 2:ERR, 3:WRN, 4:INF, 5:VRB]\n", severity);
    printf(
        "dbg filter module bit mask 0x%x [10:RADAR, 9:DRV, 8:UMAC, 7:PHY, 6:RX, 5:TX, 4:MM, 3:DMA, 2:TASK, 1:DBG, 0:KE]\n",
        module);

    return true;
}

bool lmac_cont_tx_start (int argc, char * argv[])
{
    u32 frameLen = 100;

    /*<Frequency>, <txPower>, <txRate> */
    switch (argc)
    {
        case 4:
        {
            param.txRate = str2rate(argv[3], 0, 0, 0, (uint32_t *) &frameLen);

            // no break
            [[fallthrough]];
        }

        case 3:
        {
            param.txPower = (u32) atoi(argv[2]);

            // param.txPower2 = (u32)atoi(argv[2]);
            // no break
            [[fallthrough]];
        }

        case 2:
        {
            param.freq = (u32) atoi(argv[1]);

            // no break
        }

        default:
        {
            break;
        }
    }

    param.frameLen  = frameLen;
    param.numFrames = 0;

    if (txp_on_off == 0)
    {
        lmac_apply_ppm_compensation_for_tx();
        lmac_ppm_recomp_timer_start();
        printf("Continuous TX on.  ch = %d, txpower = %d\n\n", (int) param.freq, (int) param.txPower);
        vTaskDelay(1);
        txp_on_off = 1;
        xTaskCreate(rftx_thread,
                    "Continuous TX Thread",
 #if (dg_configSYSTEMVIEW == 0)
                    (((400 * sizeof(StackType_t)) - 1) / sizeof(StackType_t) + 1),
 #else                                 /* (dg_configSYSTEMVIEW == 1) */
                    (((400 * sizeof(StackType_t) + dg_configSYSTEMVIEW_STACK_OVERHEAD) - 1) / sizeof(StackType_t) + 1),
 #endif /* (dg_configSYSTEMVIEW == 1) */
                    (void *) (&param),
                    CLI_TASK_PRIORITY,
                    &tx_task_h);

        return true;
    }
    else if (txp_on_off == 1)
    {
        printf("Continuous TX ongoing now.\n");

        return false;
    }

    return false;
}

bool lmac_cont_tx_stop (int argc, char * argv[])
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    lmac_ppm_recomp_timer_stop();

    hal_machw_reset();
    rxl_reset();
    txl_reset();

    txp_on_off = 0;

    lmac_cmd_rftx_stop();
    printf(" TX Continuous Mode STOP !!! \n");

    lmac_cmd_state_set(LMAC_HW_IDLE);

    return true;
}

 #define WIFI(x)               WIFI_ ## x

 #define PRINT_REG(x, y, z)    {            \
        temp = REG_GETF_EX(WIFI_PTA, y, z); \
        printf("\t"#z ": %ld\r\n", temp);   \
}

static bool cmd_gpio_set (int argc, const char ** argv)
{
 #ifdef dg_configUSE_HW_GPIO
    RA6W1_UNUSED_ARG(argc);

    if (((unsigned int) atoi(argv[1]) < BSP_FEATURE_IO_PORT_COUNT) &&
        ((unsigned int) atoi(argv[2]) < BSP_FEATURE_IO_PORT1_GPIO_COUNT))
    {
        g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ((atoi(argv[1]) << BSP_IO_PORT_OFFSET) | atoi(argv[2])),
                               (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_OUTPUT | GPIO_W_PERIPHERAL_GPIO | atoi(argv[3])));
    }
    else
    {
        printf("unknown param\r\n");
    }
 #endif

    return true;
}

static bool cmd_radar_init (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    rwnx_radar_detection_init();

    return true;
}

enum rwnx_radar_chain
{
    RWNX_RADAR_RIU = 0,
    RWNX_RADAR_FCU,
    RWNX_RADAR_LAST
};

enum rwnx_radar_detector
{
    RWNX_RADAR_DETECT_DISABLE = 0,     /* Ignore radar pulses */
    RWNX_RADAR_DETECT_ENABLE  = 1,     /* Process pattern detection but do not report radar to upper layer (for test) */
    RWNX_RADAR_DETECT_REPORT  = 2      /* Process pattern detection and report radar to upper layer. */
};

static bool cmd_phy_rc_isr_test (int argc, const char ** argv)
{
    enum fc80211_dfs_regions region = FC80211_DFS_FCC;

    if (argc > 1)
    {
        region = atoi(argv[1]);
        if ((region > 0) && (region < 6))
        {
            printf("radar detection enabled. region %d\n", region);

            rwnx_radar_set_domain(region);
            g_region = region;
            rwnx_radar_detection_enable(RWNX_RADAR_DETECT_REPORT, RWNX_RADAR_RIU);
            if (argc > 2)
            {
                if (!strcmp(argv[2], "info"))
                {
                    // set_radar_info_specs(argc, argv);
                }
                else if (!strcmp(argv[2], "specs"))
                {
                    set_radar_specs(argc, (char **) argv);
                }
                else
                {
                    int n = atoi(argv[2]);

                    // int leng;
                    unsigned int datas[30];

                    // unsigned int delays[30];
                    unsigned int t;

                    // unsigned int prv_t;
                    char input_str[25];

                    // delays[0] = 0;
                    printf("Radar SIM Test, Input data :\n");
                    for (int i = 0; i < n; i++)
                    {
                        // getStr(input_str, 25);
                        sscanf(input_str, "[%d] 0x%x\n", &t, &datas[i]);

                        // if (i != 0)
                        // delays[i] = t - prv_t;
                        // prv_t = t;
                    }

                    for (int i = 0; i < n; i++)
                    {
                        // rd_event_ind_sim(datas[i], 1);
                        // u_sleep(delays[i]);
                    }
                }
            }
            else
            {
                // _sys_nvic_write( phy_modem_Interrupt, (void *)_tx_lmac_radar_interrupt );
                printf(" radar test\n");
                radar_enable(1);       // Enable radar detection
                radar_timeclkforce(1); // Enable radar timer clock
                radar_pack(1, 0);      // Enable radar detection interrupt

                // radar_enable(1);
                // radar_pack(1, 0);
                // riu_radardeten_setf(1); /* radar detection enable */
                // riu_rwnxmacinten_pack(1, 0); /* irq radar enable, cca timeout disable */
            }
        }
        else
        {
            printf("radar detection disabled.\n");
            rwnx_radar_detection_enable(RWNX_RADAR_DETECT_DISABLE, RWNX_RADAR_RIU);
            radar_enable(1);
            radar_pack(1, 0);

            // riu_rwnxmacinten_pack(0, 0); /* irq radar disable, cca timeout disable */
            // riu_radardeten_setf(0); /* radar detection disable */
            // _sys_nvic_write( phy_modem_Interrupt, NULL);
        }
    }
    else
    {
        printf("radar test: radar [0:UNSET, 1:FCC, 2:ETSI, 3:JP, 4:KR, 5:CH]\n");
        printf("radar sim : radar [0:UNSET, 1:FCC, 2:ETSI, 3:JP, 4:KR, 5:CH] [input_data_num] \n");
        printf("radar spec: radar [0:UNSET, 1:FCC, 2:ETSI, 3:JP, 4:KR, 5:CH] specs [type] [field] [value] \n");
    }

    return true;
}

extern uint8_t crm_get_mac_freq(void);
extern void    crm_set_mac_freq(int freq);
extern void    hal_machw_setfreq(uint8_t newfreq);
extern void    hal_machw_checkfreq(uint8_t use_printf);

static bool lmac_mac_clock (int argc, const char ** argv)
{
    int mac_freq = crm_get_mac_freq();

    if (argc == 2)
    {
        int value = atoi(argv[1]);
        if (value == 20)
        {
            if (mac_freq != 20)
            {
                mac_freq = 20;
                hal_machw_setfreq(mac_freq);
            }
        }
        else if (value == 40)
        {
            if (mac_freq != 40)
            {
                mac_freq = 40;
                hal_machw_setfreq(mac_freq);
            }
        }
        else if (value == 60)
        {
            if (mac_freq != 60)
            {
                mac_freq = 60;
                hal_machw_setfreq(mac_freq);
            }
        }
        else
        {
            return false;
        }

        crm_set_mac_freq(mac_freq);
    }

    printf("Current MAC clock is %dM\n", mac_freq);
    hal_machw_checkfreq(1);

    return true;
}

static bool lmac_cmd_rf_rx (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    REG_PL_WR(0x60D0000C, 0x180);
    REG_PL_WR(0x60D001B8, 0x1);
    REG_PL_WR(0x60D00010, 0x0);
    REG_PL_WR(0x400B0034, 0x0);
    REG_PL_WR(0x400B0038, 0x0);
    REG_PL_WR(0x400B003c, 0x0);
    REG_PL_WR(0x400B0040, 0x0);
    REG_PL_WR(0x400C021C, 0xfff055);
    REG_PL_WR(0x40038130, 0x79);
    REG_PL_WR(0x60D00030, 0xfe);
    printf("rf rx settings\n");

    return true;
}

static bool lmac_cmd_rf_tx (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    REG_PL_WR(0x60D00030, 0x80);
    REG_PL_WR(0x60D0000C, 0x160);
    REG_PL_WR(0x60D2000C, 0x160);
    REG_PL_WR(0x60D20018, 0x80);
    REG_PL_WR(0x60C0F014, 0x68686868);
    printf("rf tx settings\n");

    return true;
}

static bool lmac_cmd_rf_tx5g (int argc, const char ** argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    REG_PL_WR(0x60D00030, 0x1cc);
    REG_PL_WR(0x60D0000C, 0x170);
    REG_PL_WR(0x60D2000C, 0x170);
    REG_PL_WR(0x60D20018, 0x180);
    REG_PL_WR(0x60C0F014, 0x68686868);
    printf("rf tx 5G settings\n");

    return true;
}

// jeff: BA parabola_test.
static bool dpd_parabola_test (int argc, const char ** argv)
{
    uint16_t k;
    uint16_t h;
    double   a1, a2, y;
    double   k_rate[4];
    uint8_t  result[33], idx;
    uint16_t value;
    uint32_t address;

    RA6W1_UNUSED_ARG(argc);
    h = atoi(argv[1]);
    k = atoi(argv[2]);
    printf("h_value :%d k_value:%d\r\n", h, k);

    k_rate[0] = k * 1.39;
    k_rate[1] = k * 1.00;
    k_rate[2] = k * 0.53;
    k_rate[3] = k * 0.17;

    for (int i = 0; i < 4; i++)
    {
        a1  = -k_rate[i] / (h * h);
        a2  = -k_rate[i] / ((1054 - h) * (1054 - h));
        idx = 0;
        for (int x = 31; x <= 1023; x += 31)
        {
            // Save to T21
            if (x < 690)
            {
                y = a1 * (x - h) * (x - h) + k_rate[i];

                // printf("x = %d y = %f\r\n", x, y);
                result[idx++] = (uint8_t) y;
            }
            else
            {
                y = a2 * (x - h) * (x - h) + k_rate[i];

                // printf("x = %d y = %f\r\n", x, y);
                result[idx++] = (uint8_t) y;
            }
        }

        switch (i)
        {
            // Atten 0
            case 0:
            {
                for (int j = 0; j < 33; j++)
                {
                    printf("atten:0 index:%d data:%02x\r\n", j, result[j]);
                }

                for (int j = 0; j < 32; j += 2)
                {
                    value   = (result[j + 1] << 8) | result[j];
                    address = 0x60D30168 + 4 * (j / 2);
                    REG_PL_WR(address, value);
                    printf("atten:0 address:%08lx value:%04x\n", address, value);
                }

                REG_PL_WR(0x60D301A8, result[32]);
                break;
            }

            case 1:
            {
                for (int j = 0; j < 33; j++)
                {
                    printf("atten:1 index:%d data:%02x\r\n", j, result[j]);
                }

                for (int j = 0; j < 32; j += 2)
                {
                    value   = (result[j + 1] << 8) | result[j];
                    address = 0x60D301AC + 4 * (j / 2);
                    REG_PL_WR(address, value);
                    printf("atten:1 address:%08lx value:%04x\n", address, value);
                }

                REG_PL_WR(0x60D301EC, result[32]);
                break;
            }

            case 2:
            {
                for (int j = 0; j < 33; j++)
                {
                    printf("atten:2 index:%d data:%02x\r\n", j, result[j]);
                }

                for (int j = 0; j < 32; j += 2)
                {
                    value   = (result[j + 1] << 8) | result[j];
                    address = 0x60D301F0 + 4 * (j / 2);
                    REG_PL_WR(address, value);
                    printf("atten:2 address:%08lx value:%04x\n", address, value);
                }

                REG_PL_WR(0x60D30230, result[32]);
                break;
            }

            case 3:
            {
                for (int j = 0; j < 33; j++)
                {
                    printf("atten:3 index:%d data:%02x\r\n", j, result[j]);
                }

                for (int j = 0; j < 32; j += 2)
                {
                    value   = (result[j + 1] << 8) | result[j];
                    address = 0x60D30234 + 4 * (j / 2);
                    REG_PL_WR(address, value);
                    printf("atten:3 address:%08lx value:%04x\n", address, value);
                }

                REG_PL_WR(0x60D30274, result[32]);
                break;
            }

            default:
            {
                break;
            }
        }
    }

    return true;
}

extern bool lmac_cmd_trace(int argc, const char ** argv);

static const debug_handler_t lmac_handlers[] =
{
    {"ver",          "",
     lmac_version},
    {"init",         "",
     lmac_init},
    {"start",        "",
     lmac_start},
    {"state",        "[i: IDLE | d: DOZE | a: ACTIVE]",
     lmac_state},
    {"mib",          "[reset]",
     lmac_mib},
    {"rftx",         "[frequency] [BW] [numFrames] [frameLen] "                           \
                     "[txRate] [txPower] [destAddr] [bssid] [htEnable] [GI] [greenField]" \
                     "[preambleType] [qosEnable] [ackPolicy] [scrambler] [aifsnVal] [ant]",
     lmac_rftx},
    {"rftxpkt",      "[frequency] [BW] [txRate] [txPower] [ccaThreshold] [ccaTimeout] " \
                     "[timeOut] [data-ASCII encoded]", lmac_rftxpkt},
    {"cont_tx",      "[frequency] [txPower] [txRate]",
     (debug_callback_t) lmac_cont_tx_start},
    {"cont_tx_stop", "",
     (debug_callback_t) lmac_cont_tx_stop},
    {"rfcw",         "[start | stop] [freq]",
     lmac_rfcw},
    {"per",          "[start|stop|reset|duration|pkt|len] [duration|pkt|len value]",
     lmac_per},
    {"power",        "[OFDM MIN Power:hex] [DSSS MAX Power:hex] [OFDM MAX Power:hex]",
     lmac_power},
    {"tx_scale",     "[mode] [value:hex]",
     lmac_tx_scale},
    {"scale_mode",   "[mode (0 - EVM, 1 - MASK] - set scale mode (get if no params)",
     lmac_scale_mode},
    {"ch",           "[channel index] [band 2: 2G, 5: 5G]",
     lmac_ch},
    {"freq",         "[channel frequency]",
     lmac_freq},
    {"mac_clock",    "[mac_frequency (20|40|60)]",
     lmac_mac_clock},
    {"dbg",          "[severity:decimal(MAX=5)] [module:hex]",
     lmac_dbg},
    {"trace",        "[component:decimal(MAX=15)] [level:hex]",
     lmac_cmd_trace},
    {"rf_rx",        "[rf rx settings]",
     lmac_cmd_rf_rx},
    {"rf_tx",        "[rf tx settings]",
     lmac_cmd_rf_tx},
    {"rf_tx5g",      "[rf rx5g settings]",
     lmac_cmd_rf_tx5g},
    {"rf_dpd",       "[rf dpd settings]",
     dpd_parabola_test},
    {"gpio",         "gpio [port:0~2] [pin:0~14] [0:low, 1:high] for antenna switch",
     cmd_gpio_set},
    {"radar_init",   "radar_init",
     cmd_radar_init},
    {"radar",        "[region] ([specs|info]) ([type] [field] [value])",
     cmd_phy_rc_isr_test},
 #ifdef __SUPPORT_WIFI_DBG__
    {"stats",        "[tx|rx|ps]",
     cli_fw_stats},
 #endif                                /* __SUPPORT_WIFI_DBG__ */
 #ifdef TIMP_CMD_EN
    {"timp",         "TIMP performance",
     cmd_timp},
 #endif                                /* TIMP_CMD_EN */
    {"ldpc",         "TX LDPC on/off",
     cmd_lmac_ldpc},
    {NULL},
};

#endif
bool lmac_command (int argc, const char * argv[], void * user_data)
{
#if CFG_WIFI
    RA6W1_UNUSED_ARG(user_data);

    return debug_handle_message(argc, argv, lmac_handlers);
#else
    printf("\nnot supported\n");

    return false;
#endif
}
