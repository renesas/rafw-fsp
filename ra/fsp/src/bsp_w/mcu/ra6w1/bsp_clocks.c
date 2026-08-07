/**
 * \addtogroup BSP
 \{
 * \addtogroup DEVICES
 \{
 * \addtogroup CLK
 \{
 */

/**
 ****************************************************************************************
 *
 * @file ra6w1/bsp_clocks.c
 *
 * @brief Clock Driver
 *
 * Copyright (c) 2016-2020 Renesas Electronics. All rights reserved.
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

#if (DEVICE_FAMILY == DA1640X)

#define USE_FAST_STARTUP    0

#include <stdint.h>
#include "bsp_api.h"
#include "sdk_defs.h"
#include "bsp_clocks.h"

#if (dg_configUSE_HW_CLK_DEBUG_GPIO == 1)
 #include "hw_gpio.h"
#endif

#if (dg_configUSE_HW_CLK_DEBUG_GPIO == 1)
 #define HW_CLK_DEBUG_GPIO_HIGH()    hw_gpio_set_active(HW_GPIO_PORT_1, HW_GPIO_PIN_10)
 #define HW_CLK_DEBUG_GPIO_LOW()     hw_gpio_set_inactive(HW_GPIO_PORT_1, HW_GPIO_PIN_10)
#else
 #define HW_CLK_DEBUG_GPIO_HIGH()
 #define HW_CLK_DEBUG_GPIO_LOW()
#endif

/* macros to adjust RTC access timing */
#define         RTC_POP_ACCESS_TIMING()                      (RTC->RTC_IF_REG)
#define         RTC_PUSH_ACCESS_TIMING(val)                  (RTC->RTC_IF_REG) = (val)
#define         RTC_SET_ACCESS_TIMING(curval, fz, fh, sz)    do { \
        (RTC->RTC_IF_REG) =                                       \
            (curval & (~0x00000FFFuL))                            \
            | ((sz & 0x0f) << 8)                                  \
            | ((fh & 0x0f) << 4)                                  \
            | ((fz & 0x0f) << 0);                                 \
} while (0)

#if USE_FAST_STARTUP
typedef struct
{
    uint16_t temp_refpt;               // reference point for temperature compensation

    uint16_t base_drive_cycles;        // base drive cycles at room xtal32m_temp_refpt
} xtalm_otp_t;

static xtalm_otp_t xtalm_otp_values;
#endif

static uint32_t g_sys_clock_freq[SYS_CLK_IS_INVALID] =
{
    0,
    BSP_CFG_XTALM_HZ,                  /* SYS_CLK_IS_XTAL40M */
    0,                                 /* SYS_CLK_IS_LP - Can be either BSP_RCX_HZ or BSP_CFG_XTALK_HZ */
    BSP_CFG_PLL240M_HZ,                /* SYS_CLK_IS_PLL240M */
    BSP_CFG_PLL192M_HZ,                /* SYS_CLK_IS_PLL192M */
    BSP_CFG_PLL160M_HZ,                /* SYS_CLK_IS_PLL160M */
    BSP_CFG_PLL137M_HZ,                /* SYS_CLK_IS_PLL137M */
    BSP_CFG_PLL106M_HZ                 /* SYS_CLK_IS_PLL106M */
};

static uint32_t g_pll_spi_clock_freq[BSP_CLOCKS_PLL_SPI_CLOCK_MAX] =
{
    BSP_CFG_PLL120M_HZ,                /* BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_4 */
    BSP_CFG_PLL96M_HZ,                 /* BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_5 */
    BSP_CFG_PLL80M_HZ,                 /* BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_6 */
    BSP_CFG_PLL53M_HZ                  /* BSP_CLOCKS_PLL_SPI_CLOCK_MUL_53_480 */
};

static uint32_t bsp_sys_clock_to_freq (sys_clk_is_t clk)
{
    uint32_t hz = g_sys_clock_freq[clk];

    /*LP clock can be XTAL32K or RCX */
    if (clk == SYS_CLK_IS_LP)
    {
        hz = (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_XTAL32K) ? BSP_CFG_XTALK_HZ : BSP_RCX_HZ;
    }

    return hz;
}

static uint32_t bsp_timer_clock_freq_get (uint32_t peri_hz, uint8_t timer_num)
{
    uint32_t hz = 0;

    /* timer_num range [0-7] */
    TIMER_Type * p_reg = (TIMER_Type *) (TIMER_BASE + timer_num * (TIMER2_BASE - TIMER_BASE));

    if (p_reg->TIMER_CTRL_REG_b.TIM_CLK_EN)
    {
        /* 0 - LP Clock. 1 - DIVN Clock*/
        if (p_reg->TIMER_CTRL_REG_b.TIM_SYS_CLK_EN)
        {
            hz = peri_hz;
        }
        else
        {
            hz = R_BSP_LPClockHzGet();
        }
    }

    return hz;
}

/* Reflects the RRQ61 clock scheme, see also:
 * RA6W1 Data sheet – chapter 8.3 Clock Generation */
uint32_t R_BSP_PeripheralClockHzGet (fsp_priv_clock_t clock)
{
    sys_clk_is_t sys_clk = hw_clk_get_sysclk();

    BSP_CHECK_DEBUG(sys_clk < SYS_CLK_IS_INVALID);

    /* Peri clock can be only LP or XTAL40 */
    sys_clk_is_t peri_clk = (sys_clk == SYS_CLK_IS_LP ? SYS_CLK_IS_LP : SYS_CLK_IS_XTAL40M);

    uint32_t sys_hz  = bsp_sys_clock_to_freq(sys_clk);
    uint32_t peri_hz = bsp_sys_clock_to_freq(peri_clk);
    uint8_t  enable  = 1;
    uint8_t  div     = 0;
    uint8_t  mul     = 1;
    uint32_t hz      = sys_hz;

    /* Divide freq by Div */
    switch (clock)
    {
        /* HCLK Div */
        case FSP_PRIV_CLOCK_SYS_CLK:
        {
            /* SYS CPU always enabled */
            div = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_CC312:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.CC312_CLK_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_PSK:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.PSK_CLK_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_TDES:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.TDES_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_DMA:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.GEN_DMA_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_KDMA:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.KDMA_CLK_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_SYS_MEM:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.SYS_MEM_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
            break;
        }

        /* OQSPI Div */
        case FSP_PRIV_CLOCK_OQSPI:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.OQSPIF_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV;
            break;
        }

        /* QSPI Div */
        case FSP_PRIV_CLOCK_QSPI:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.QSPI2_ENABLE;
            div    = CRG_TOP->CLK_AMBA_REG_b.QSPI2_DIV;
            break;
        }

        case FSP_PRIV_CLOCK_SPI:
        case FSP_PRIV_CLOCK_SPI2:
        {
            hz = peri_hz;

            /* SYS_CLK = SYS-PLL */
            if (BSP_CLOCKS_SOURCE_CLOCK_PLL == CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL)
            {
                uint8_t spi_clk_bit;
                enable      = (enable && CRG_TOP->CLK_CTRL_REG_b.PLL_SPI_ENABLE);
                spi_clk_bit = CRG_TOP->CLK_CTRL_REG_b.PLL_SPI_CLK_SEL;
                hz          = g_pll_spi_clock_freq[spi_clk_bit];
            }

            break;
        }

        /* Peripherals */
        case FSP_PRIV_CLOCK_TIMER:
        case FSP_PRIV_CLOCK_TIMER2:
        case FSP_PRIV_CLOCK_TIMER3:
        case FSP_PRIV_CLOCK_TIMER4:
        case FSP_PRIV_CLOCK_TIMER5:
        case FSP_PRIV_CLOCK_TIMER6:
        case FSP_PRIV_CLOCK_TIMER7:
        case FSP_PRIV_CLOCK_TIMER8:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.TIMER_CLK_ENABLE &&
                     CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;

            if (enable)
            {
                hz = bsp_timer_clock_freq_get(peri_hz, (clock - FSP_PRIV_CLOCK_TIMER));
            }

            break;
        }

        case FSP_PRIV_CLOCK_I2C:
        {
            enable = CRG_PER->CLK_COM_REG_b.I2C_ENABLE &&
                     CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;
            hz  = peri_hz;
            div = (CRG_PER->CLK_COM_REG_b.I2C_CLK_SEL ? 0 : 1);
            mul = (CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE ? 2 : 1);
            break;
        }

        case FSP_PRIV_CLOCK_I2C2:
        {
            enable = CRG_PER->CLK_COM_REG_b.I2C2_ENABLE &&
                     CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;
            hz  = peri_hz;
            div = (CRG_PER->CLK_COM_REG_b.I2C2_CLK_SEL ? 0 : 1);
            mul = (CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE ? 2 : 1);
            break;
        }

        case FSP_PRIV_CLOCK_SDIO:
        {
            enable = CRG_PER->CLK_COM_REG_b.SDIO_ENABLE &&
                     CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;
            hz  = peri_hz;
            mul = (CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE ? 2 : 1);
            break;
        }

        case FSP_PRIV_CLOCK_SDEMMC:
        {
            enable = CRG_PER->CLK_COM_REG_b.SDEMMC_ENABLE &&
                     CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;
            hz  = peri_hz;
            mul = (CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE ? 2 : 1);
            break;
        }

        case FSP_PRIV_CLOCK_UART:
        {
            enable = CRG_TOP->CLK_AMBA_REG_b.PERI_CLK_ENABLE;
            hz     = peri_hz;
            mul    = (CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE ? 2 : 1);
            break;
        }

        case FSP_PRIV_CLOCK_WDT:
        {
            hz = (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_XTAL32K) ? BSP_CFG_XTALK_HZ : BSP_RCX_HZ;
            break;
        }

        case FSP_PRIV_CLOCK_AUX_ADC:
        {
            hz = BSP_CFG_PLL480M_HZ >> BSP_CLOCKS_PLL_AUX_CLOCK_DIV_32; /* 15 MHz */
            break;
        }

        default:
            BSP_CHECK_FATAL(0);
    }

    /* freq = base freq clock * multiplier, shift by divider. */
    return enable ? ((hz * mul) >> div) : 0;
}

/*******************************************************************************************************************//**
 * Checks if the main oscillator (XTALM) has settled.
 *
 * @return  true if XTALM has settled, false if it hasn't.
 **********************************************************************************************************************/
bool bsp_clock_xtalm_is_settled (void)
{
    return true;
}

/***********************************************************************************************************************
 * Configures the XTAL32M clock
 **********************************************************************************************************************/
void bsp_clock_xtalm_configure (void)
{
    /* No configuration for XTAL40M. */
}

fsp_err_t bsp_clock_xtalm_preconditions_check (void)
{

    /* No preconditions for XTAL40M. */
    return FSP_SUCCESS;
}

void bsp_clock_xtalm_enable (bool enable)
{
    if (enable)
    {
        CRG_COM->XTAL40M_CTRL_REG |= CRG_COM_XTAL40M_CTRL_REG_XTAL40M_EN_Msk;
    }
    else
    {
        CRG_COM->XTAL40M_CTRL_REG &= ~CRG_COM_XTAL40M_CTRL_REG_XTAL40M_EN_Msk;
    }
}

#if BSP_FEATURE_CGC_HAS_SOSC
 #if BSP_CLOCK_CFG_SUBCLOCK_POPULATED

/***********************************************************************************************************************
 * Configures the XTAL32K clock. This must be done only once, since the register is retained.
 **********************************************************************************************************************/
void bsp_clock_xtalk_configure (void)
{
    // Placeholder for possible actions needed at startup for Xtalk configuration
}

 #endif                                /* BSP_CLOCK_CFG_SUBCLOCK_POPULATED */
#endif                                 /* BSP_FEATURE_CGC_HAS_SOSC */

#if 0
BSP_PLACE_CODE_IN_RAM void hw_clk_start_calibration(cal_clk_t clk_type, cal_ref_clk_t clk_ref_type, uint16_t cycles);

BSP_PLACE_CODE_IN_RAM void hw_clk_start_calibration (cal_clk_t clk_type, cal_ref_clk_t clk_ref_type, uint16_t cycles)
{
    uint32_t val = 0;

    /* Must be disabled */
    BSP_CHECK_DEBUG(!CLKCAL_BIF->CLK_REF_SEL_REG_b.REF_CAL_START);

    CLKCAL_BIF->CLK_REF_CNT_REG = cycles; // # of cal clock cycles

    if (clk_ref_type == CALIBRATE_REF_EXT)
    {
        FSP_REG_VAR_FIELD_SET(CLKCAL_BIF, CLK_REF_SEL_REG, EXT_CNT_EN_SEL, val, 1);
    }
    else
    {
        FSP_REG_VAR_FIELD_SET(CLKCAL_BIF, CLK_REF_SEL_REG, REF_CLK_SEL, val, clk_ref_type);
    }

    FSP_REG_VAR_FIELD_SET(CLKCAL_BIF, CLK_REF_SEL_REG, CAL_CLK_SEL, val, clk_type);

    CLKCAL_BIF->CLK_REF_SEL_REG = val;

    REG_SET_BIT(CLKCAL_BIF, CLK_CAL_IRQ_REG, CLK_CAL_IRQ_EN);

    REG_SET_BIT(CLKCAL_BIF, CLK_REF_SEL_REG, REF_CAL_START);
}

uint32_t hw_clk_get_calibration_data(void);

uint32_t hw_clk_get_calibration_data (void)
{
    /* Wait until it's finished */
    while (CLKCAL_BIF->CLK_REF_SEL_REG_b.REF_CAL_START)
    {
    }

    return CLKCAL_BIF->CLK_REF_VAL_REG;
}

#endif

#undef CLK_DELAY_SANITY_CHECKS
#pragma GCC push_options
#pragma GCC optimize ("O3")

void hw_clk_delay_usec(uint32_t usec);

void hw_clk_delay_usec (uint32_t usec)
{
    static const uint32_t DIVIDER = 1000000;

#ifdef CLK_DELAY_SANITY_CHECKS
    BSP_CHECK_DEBUG((BSP_CFG_XTALM_HZ % DIVIDER) == 0);
    BSP_CHECK_DEBUG((HW_CLK_DELAY_OVERHEAD_CYCLES % HW_CLK_CYCLES_PER_DELAY_REP) == 0);
#endif

    static const uint8_t OVERHEAD_REPS    = HW_CLK_DELAY_OVERHEAD_CYCLES / HW_CLK_CYCLES_PER_DELAY_REP;
    static uint32_t      sys_freq_table[] =
    {
        0,                             // SYS_CLK_IS_NONE
        BSP_CFG_XTALM_HZ / DIVIDER,    // SYS_CLK_IS_XTAL40M
        BSP_CFG_XTALK_HZ / DIVIDER,    // SYS_CLK_IS_LP
        0,                             // SYS_CLK_IS_PLL240M
        0,                             // SYS_CLK_IS_PLL192M
        BSP_CFG_PLL160M_HZ / DIVIDER,  // SYS_CLK_IS_PLL160M
        BSP_CFG_PLL137M_HZ / DIVIDER,  // SYS_CLK_IS_PLL137M
        BSP_CFG_PLL106M_HZ / DIVIDER,  // SYS_CLK_IS_PLL106M
    };

    const uint32_t cycles_per_usec = sys_freq_table[hw_clk_get_sysclk()] >> hw_clk_get_hclk_div(),
                   reps            = cycles_per_usec * usec / HW_CLK_CYCLES_PER_DELAY_REP;

#ifdef CLK_DELAY_SANITY_CHECKS
    BSP_CHECK_DEBUG(usec <= 0xFFFFFFFF / cycles_per_usec); // The requested delay is greater than the maximum delay this function can achieve
    BSP_CHECK_DEBUG(reps > OVERHEAD_REPS);                 // The requested delay is smaller than the minimum delay this function can achieve.
#endif

    if (reps <= OVERHEAD_REPS)
    {
        return;
    }

    __ASM volatile (
        "       nop                             \n"
        "       nop                             \n"
        "       nop                             \n"
        "       nop                             \n"
        "       nop                             \n"
        "loop:  nop                             \n"
        "       subs %[reps], %[reps], #1       \n"
        "       bne loop                        \n"
        :                                  // outputs
        :[reps] "r" (reps - OVERHEAD_REPS) // inputs
        :                                  // clobbers
        );
}

#pragma GCC pop_options
#if USE_FAST_STARTUP
static void xtal32m_readOTP (void)
{
    xtalm_otp_values.temp_refpt        = 100; // reference point for temperature compensation
    xtalm_otp_values.base_drive_cycles = 175; // base drive cycles at room xtal32m_temp_refpt
}

#endif

int16_t hw_clk_xtalm_update_rdy_cnt (void)
{
    int16_t xtalrdy_stat = 0;

    /*
     * XXX On FPGA the value of XTALRDY_STAT_REG[XTALRDY_STAT] is always zero, so
     * XXX there is no need to update XTALRDY_CTRL_REG[XTALRDY_CNT].
     */
#if 0
    uint8_t xtalrdy_cnt = 0;

    // XXX review the following comments. normal vs fast mode
    // XXX update IRQ time when in NORMAL startup mode:
    // XXX (mis) use irq counter setting to do this

    if (REG_GETF(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CLK_SEL) == 0)
    {
        xtalrdy_cnt  = REG_GETF(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CNT);
        xtalrdy_stat = 3 - REG_GETF(CRG_XTAL, XTALRDY_STAT_REG, XTALRDY_STAT);
        xtalrdy_cnt += xtalrdy_stat;
        REG_SETF(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CNT, xtalrdy_cnt);
    }
#endif

    return xtalrdy_stat;
}

void hw_clk_xtalm_compensate_amp (void)
{
#if USE_FAST_STARTUP
    uint16_t T_drive, T_drive_lsb;
    uint8_t  N;

    // perform amplitude compensation
    if (CRG_XTAL->XTAL32M_CTRL0_REG_b.XTAL32M_RCOSC_XTAL_DRIVE == 1)
    {
        uint16_t divisor = (0x8 << (0x7 - CRG_XTAL->XTAL32M_CTRL1_REG_b.XTAL32M_STARTUP_TDRIVE));

        T_drive     = (xtalm_otp_values.base_drive_cycles * xtal32m_adcread()) / xtalm_otp_values.temp_refpt;
        N           = T_drive / divisor;
        T_drive_lsb = T_drive - divisor * N;

        CRG_XTAL->XTAL32M_CTRL1_REG_b.XTAL32M_STARTUP_TDRIVE_LSB = T_drive_lsb;
        CRG_XTAL->XTAL32M_CTRL1_REG_b.XTAL32M_DRIVE_CYCLES       = (N + 1);
    }
#endif
}

void hw_clk_xtalm_configure (void)
{
#if 0
 #if USE_FAST_STARTUP
    uint8_t settling_time;
    uint8_t cxcomp_phi_trim;

    xtal32m_readOTP();

    REG_SETF(CRG_XTAL, XTAL32M_CTRL0_REG, XTAL32M_CORE_CUR_SET, 3);                         // gmopt cur set.

    uint32_t reg = CRG_XTAL->XTAL32M_CTRL1_REG;
    REG_SET_FIELD(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_RCOSC_SYNC_DELAY_TRIM, reg, 0);      // synchronization delay trim
    REG_SET_FIELD(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TDISCHARGE, reg, 0);         // discharge time in drive sequence
    REG_SET_FIELD(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TSETTLE, reg, 5);            // required settling time
    settling_time = 6;
    REG_SET_FIELD(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TDRIVE, reg, settling_time); // unit drive length
    CRG_XTAL->XTAL32M_CTRL1_REG = reg;

    REG_SETF(CRG_XTAL, XTAL32M_CTRL2_REG, XTAL32M_RCOSC_TRIM_SNS, 118);                     // sensitivity of rcosc

    // sets TSETTLE to half of TDRIVE
    REG_SETF(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TSETTLE, settling_time);

    uint16_t T_drive, T_drive_lsb;
    uint8_t  N;

    uint16_t divisor = (0x8 << (0x7 - REG_GETF(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TDRIVE)));

    // equate FASTBOOT drive-cycles
    T_drive     = xtalm_otp_values.base_drive_cycles;
    N           = T_drive / divisor;
    T_drive_lsb = T_drive - divisor * N;

    REG_SETF(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_STARTUP_TDRIVE_LSB, T_drive_lsb);
    REG_SETF(CRG_XTAL, XTAL32M_CTRL1_REG, XTAL32M_DRIVE_CYCLES, N + 1);

    // fast startup mode
    REG_SETF(CRG_XTAL, TRIM_CTRL_REG, XTAL_TRIM_SELECT, 0x2); // always use direct trimming (disable legacy)

    // ~10us for ldo settling and 2 cycles for xtal voltage settling. IRQ handler also takes some time.
    // hclk divider needs to be set to 0
    uint16_t xtalrdy_cnt = T_drive / 83 + 3 + 2;

    // Setup IRQ:
    REG_SET_BIT(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CLK_SEL); // use 256kHz clock
    REG_SETF(CRG_XTAL, XTALRDY_CTRL_REG, XTALRDY_CNT, xtalrdy_cnt);
    REG_SET_BIT(CRG_XTAL, XTAL32M_CTRL0_REG, XTAL32M_RCOSC_CALIBRATE);
    REG_CLR_BIT(CRG_XTAL, XTAL32M_CTRL0_REG, XTAL32M_CXCOMP_ENABLE);
 #else

    // Configure OSF BOOST
    uint8_t cxcomp_phi_trim = 0;
    uint8_t cxcomp_trim_cap = REG_GETF(CRG_XTAL, XTAL32M_CTRL2_REG, XTAL32M_CXCOMP_TRIM_CAP);

    // set phi compensation
    if (cxcomp_trim_cap < 37)
    {
        cxcomp_phi_trim = 3;
    }
    else
    {
        if (cxcomp_trim_cap < 123)
        {
            cxcomp_phi_trim = 2;
        }
        else
        {
            if (cxcomp_trim_cap < 170)
            {
                cxcomp_phi_trim = 1;
            }
            else
            {
                cxcomp_phi_trim = 0;
            }
        }
    }
    REG_SETF(CRG_XTAL, XTAL32M_CTRL2_REG, XTAL32M_CXCOMP_PHI_TRIM, cxcomp_phi_trim);
 #endif
#endif
}

/**
 * \brief Enable the PLL.
 */
BSP_PLACE_CODE_IN_RAM uint32_t hw_clk_pll_sys_on (void)
{
#if DEVICE_FPGA

    return 0;
#else
    volatile uint32_t pllockcnt = 0, ldoisoff;
    volatile uint32_t ldostabletime;
    ahb_div_t         bkup_ahbdiv, ahbdiv, scaler;
    uint32_t          bkup_rtcacctime;

    GLOBAL_INT_DISABLE();

    // enable LDO for PLL

    /* 231024, NOTICE !!!!!!!!!!
     * According to long-run testing, LDO settling time seems to be about 270~300 us.
     * The NOP-loop (below while-loop) consists of 10 instructions. (approx. 12 cycles) */

    //
    // old condition: We managed to keep the time to 50us at least.
    // 231024, test : unstable 50,55. stable 56(about 270 us),58,60,64 @ CPU 2.5MHz (XTAL 40MHz, AHB div 1/16)
    // 231025, random test :
    // long time, unstable 62(2.5>40).  stable 64
    // short time, unstable 12(137>34). stable 16
    // 231026, pll management model has been updated for speed-up.
    // 231027, (25<<4). long-run failed. changed to (30<<4)
    // ahbdiv control scheme is fixed.
    // 231030, HW_TIMER2 based time measurement was incorrect. changed to (640<<4, about 300 us)

    // backup ahbdiv & rtcacctime
    bkup_ahbdiv     = hw_clk_get_hclk_div();
    bkup_rtcacctime = RTC_POP_ACCESS_TIMING();

    // forcing ahbdiv & rtcacctime
    ahbdiv = ahb_div1;

    // We should access HCLK_DIV directly instead of calling hw_clk_set_hclk_div()
    // to avoid the conflict with the nested call when using GLOBAL_INT_DISABLE.
    // Trick to speed up RTC access.
    HW_CLK_DEBUG_GPIO_HIGH();
    RTC_SET_ACCESS_TIMING(bkup_rtcacctime, 5, 7, 0);
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = ahbdiv;
    HW_CLK_DEBUG_GPIO_LOW();

    ldostabletime = (1 << 4);
    for ( ; ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    switch (ahbdiv)
    {
        case ahb_div1:
        {
            scaler = 4;                /*XTAL 40MHz*/
            break;
        }

        case ahb_div2:
        {
            scaler = 3;                /*XTAL 20MHz*/
            break;
        }

        case ahb_div4:
        {
            scaler = 2;                /*XTAL 10MHz*/
            break;
        }

        case ahb_div8:
        {
            scaler = 1;                /*XTAL 5MHz*/
            break;
        }

        case ahb_div16:                /*XTAL 2.5MHz*/
        default:
        {
            scaler = 0;
            break;
        }
    }

    HW_CLK_DEBUG_GPIO_HIGH();
    if (RTC->LDO_ENABLE_REG_b.LDO_EN_LDO_PLL1 == 0)
    {
        ldostabletime = (140 << 4);    // when LDO is off for a long time. about 320 us

        REG_SET_BIT(RTC, LDO_ENABLE_REG, LDO_EN_LDO_PLL1);
        ldoisoff = 1;
    }
    else
    {
        ldostabletime = 0;             // when LDO and/or DPLL are turned on/off for a moment,
                                       // or when LDO is turned on during waking up from deepsleep.
        ldoisoff = 0;
    }

    ldostabletime = ldostabletime >> (scaler);
    for ( ; ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    HW_CLK_DEBUG_GPIO_LOW();

    // set dpll1en & xtalen to activate PLL  ( XTAL_GAIN(7) )
    // xtalen
    REG_SET_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_EN);

    // xtal switch for DPLL
    HW_CLK_DEBUG_GPIO_HIGH();
    if (CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_DPLL_EN == 0)
    {
        ldostabletime = (0 << 4);      // when XTAL40M_DPLL_EN is off for a long time.

        // 231101, these steps are changed to "gain control before DPLL.xtal_en"
        // xtal gain
        CRG_COM->XTAL40M_CTRL_REG_b.XTAL40_GAIN = 7;

        ldostabletime = ldostabletime >> (scaler);
        for ( ; ldostabletime > 0; ldostabletime--)
        {
            __ASM volatile ("nop");
            __ASM volatile ("nop");
            __ASM volatile ("nop");
            __ASM volatile ("nop");
        }

        // DPLL.xtal_en
        REG_SET_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_DPLL_EN);
        ldostabletime = (0 << 4);
    }
    else
    {
        ldostabletime = 0;             // when XTAL40M_DPLL_EN is enabled,
    }

    ldostabletime = ldostabletime >> (scaler);
    for ( ; ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    HW_CLK_DEBUG_GPIO_LOW();

    // disable PLL ( power-down )
    // first clear to make sure that the logic will be reseted
    REG_CLR_BIT(CRG_COM, PLL1_ARM_CTRL_REG, DPLL1_EN);

    // wait until PLL is unlocked
    pllockcnt = MAX_PLL_LCKCHK_TIME;   /* x1 times */

    HW_CLK_DEBUG_GPIO_HIGH();
    while (pllockcnt-- > 0)
    {
        if (CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK == 0)
        {
            break;
        }
    }

    HW_CLK_DEBUG_GPIO_LOW();

    // setup PLL to operate 480MHz
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_VCO_SEL  = 0;
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_DIV_SEL  = 24;
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_SHORT_S  = 2;
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_LF_RCTRL = 0;

    // enable PLL ( power-up )
    REG_SET_BIT(CRG_COM, PLL1_ARM_CTRL_REG, DPLL1_EN);

    // wait until PLL is locked
    pllockcnt = MAX_PLL_LCKCHK_TIME;   /* x1 times */

    HW_CLK_DEBUG_GPIO_HIGH();
    while (pllockcnt-- > 0)
    {
        if (CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK != 0)
        {
            break;
        }
    }

    HW_CLK_DEBUG_GPIO_LOW();

    // restore ahbdiv
    HW_CLK_DEBUG_GPIO_HIGH();
    RTC_PUSH_ACCESS_TIMING(bkup_rtcacctime);
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = bkup_ahbdiv;

    ldostabletime = (1 << 4) >> (bkup_ahbdiv);
    for ( ; ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    HW_CLK_DEBUG_GPIO_LOW();

    GLOBAL_INT_RESTORE();

    return (ldoisoff << 2) | ((MAX_PLL_LCKCHK_TIME - pllockcnt) << 3);
#endif
}

/**
 * \brief Disable the PLL.
 *
 * \warning The System clock must have been set to XTAL40M before calling this function!
 */
BSP_PLACE_CODE_IN_RAM void hw_clk_pll_sys_off (void)
{
#if DEVICE_FPGA
#else
    volatile uint32_t pllockcnt = 0;
    ahb_div_t         bkup_ahbdiv, ahbdiv;

    GLOBAL_INT_DISABLE();

    // backup ahbdiv
    bkup_ahbdiv = hw_clk_get_hclk_div();

    // forcing ahbdiv
    ahbdiv = ahb_div1;

    // We should access HCLK_DIV directly instead of calling hw_clk_set_hclk_div()
    // to avoid the conflict with the nested call when using GLOBAL_INT_DISABLE.
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = ahbdiv;

    // The PLL is not the system clk.
    BSP_CHECK_DEBUG(!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL);

    /* Turn off PLL. */
    REG_CLR_BIT(CRG_COM, PLL1_ARM_CTRL_REG, DPLL1_EN);

    // wait until PLL is unlocked
    pllockcnt = MAX_PLL_LCKCHK_TIME;   /* x1 times */
    while (pllockcnt-- > 0)
    {
        if (CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK == 0)
        {
            break;
        }
    }

    /* LDO PLL disable. */
    REG_CLR_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_DPLL_EN);

    for (uint32_t ldostabletime = (1 << 4); ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    /* disable LDO for PLL */
    REG_CLR_BIT(RTC, LDO_ENABLE_REG, LDO_EN_LDO_PLL1);

    for (uint32_t ldostabletime = (1 << 4); ldostabletime > 0; ldostabletime--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    // restore ahbdiv
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = bkup_ahbdiv;

    GLOBAL_INT_RESTORE();
#endif
}

static BSP_PLACE_CODE_IN_RAM void pll_enable (void)
{
    volatile uint32_t delay;

    /* Now turn on PLL */
    RTC->LDO_ENABLE_REG_b.LDO_EN_LDO_PLL1 = 1; // enable LDO for PLL

    /* it need 50us at least */
    for (delay = 50; delay > 0; delay--)
    {
        __asm__ volatile ( "nop");
        __asm__ volatile ( "nop");
        __asm__ volatile ( "nop");
        __asm__ volatile ( "nop");
    }

    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_DPLL_EN = 1; // enable xtal path to DPLL

    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_EN = 0;       // first clear to make sure that the logic will be reseted
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_EN = 1;

    /* And wait until lock. */
    while (CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK == 0)
    {
    }
}

BSP_PLACE_CODE_IN_RAM void pll_on (void)
{
#if DEVICE_FPGA
#else
    int i;

    pll_enable();

    CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV      = 1; // oqspi divide first 40Mhz
    CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL     = 2; // 160Mhz
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV        = 0; // AMBA 160Mhz
    CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE = 1; // peri pll clock
    CRG_TOP->CLK_CTRL_REG_b.PLL_CPU_ENABLE  = 1;
    CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL     = 3;

    for (i = 32; i > 0; i--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    // TODO merge tin FIX divider restoration
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = BSP_CFG_HCLK_DIV;
#endif
}

BSP_PLACE_CODE_IN_RAM void pll_off (void)
{
#if DEVICE_FPGA
#else
    CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL    = 1; // XTAL
    CRG_TOP->CLK_CTRL_REG_b.PLL_CPU_ENABLE = 0;
    CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV     = 1; // oqspi divide first 80Mhz

    /* Now turn off PLL */
    CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_EN = 0;
    RTC->LDO_ENABLE_REG_b.LDO_EN_LDO_PLL1 = 0;

    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_DPLL_EN = 0;
#endif
}

/*******************************************************************************************************************//**
 * Enable mirror for acessing RTC register.
 *
 * @note  The RTC block uses a 32khz clock.
 *        Because of this, bus access also uses 32khz.
 *        Using a mirror, you can access the RTC register using the CPU's clock.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM void bsp_prv_rtc_mirror_init (void)
{
    RTC->RTC_REQ_REG_b.RTC_REQ_CLR_IRQ = 1;

    if (RTC->RTC_REQ_REG_b.RTC_REQ_LOAD_MR == 0)
    {
        RTC->RTC_MIRROR_REG_b.RTC_OP_EN = 1;
        RTC->RTC_MIRROR_REG_b.RTC_MR_EN = 1;

        RTC->RTC_REQ_REG_b.RTC_REQ_LOAD_MR = 1;

        /* Wait to done process */
        while ((RTC->RTC_IRQ_STATUS_REG_b.RTC_IRQ_STATUS) != 0x01)
        {
            ;
        }
    }
}

/*******************************************************************************************************************//**
 * Gets the current low power clock source selection.
 *
 * @return The current low power clock source type.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM lp_clk_is_t bsp_prv_lpclk_get ()
{
    uint32_t sel, current_sel;

    sel = RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL;

    if (sel == 0x00)
    {
        current_sel = LP_CLK_IS_RCX;
    }
    else if (sel == 0x01)
    {
        current_sel = LP_CLK_IS_XTAL32K;
    }
    else
    {
        current_sel = LP_CLK_IS_INVALID;
    }

    return current_sel;
}

/*******************************************************************************************************************//**
 * @brief Selects the low power clock source.
 *
 * This function selects the low power clock source based on the input type.
 * It checks the current clock selection and updates it if available.
 *
 * @param[in] type The desired low power clock source type.
 *
 * @return The selected low power clock source type.
 *
 * @note  The lp clock has internal rcx and external xtal32k.
 *        It usually takes about 1 second after POR for XTAL_RDY_BAT_RD_STATUS to become 1.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t bsp_prv_lpclk_select (lp_clk_is_t type)
{
    fsp_err_t result = FSP_SUCCESS;

    if (bsp_prv_lpclk_get() == type)
    {

        /* If sel is same to type, it's return */
        return result;
    }

    switch (type)
    {
        case LP_CLK_IS_RCX:
        {
            RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = 0;
            RTC->CLK_XTAL32K_REG_b.XTAL_BAT_EN  = 0;
            break;
        }

        case LP_CLK_IS_XTAL32K:
        {
#if (BSP_CLOCK_CFG_SUBCLOCK_POPULATED == 1)
            if (RTC->READ_STATUS_REG_b.XTAL_RDY_BAT_RD_STATUS)
            {
                RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = 1;

                /* For decrease current consumption, it's off OSC */
                RTC->RCX32K_REG = 0;
                RTC->CLK_XTAL32K_REG_b.PDB_OSC_EN = 0;
            }
            else
#endif
            {
                result = FSP_ERR_NOT_STABILIZED;
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void bsp_clock_init_rrq61 (void)
{
    int i;

    /*------------------- Configure XTAL32K_CTRL_REG ------------------*/
    CRG_COM->XTAL32K_CTRL_REG_b.XTAL32K_ENABLE = BSP_CLOCKS_CLOCK_ENABLED(BSP_CFG_XTALK_CLK_SOURCE);
#if (BSP_CFG_LP_CLK_SOURCE == BSP_CLOCKS_SOURCE_CLOCK_RCX)
    RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = 0;
#endif

    /*-------------------- Configure CRG_TOP->CLK_AMBA_REG ----------------------*/
    {
        /* Write CLK_AMBA_REG fields into local var, to avoid multiple register accesses.*/
        uint32_t reg_tmp = CRG_TOP->CLK_AMBA_REG;

        /* Enable/Disable */
        FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, PERI_CLK_ENABLE, reg_tmp,
                              BSP_CLOCKS_CLOCK_ENABLED(BSP_CFG_PERI_CLK_SOURCE));
        FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, TIMER_CLK_ENABLE, reg_tmp, 1);

        FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, SPI_CLK_ENABLE, reg_tmp,
                              BSP_CLOCKS_CLOCK_ENABLED(BSP_CFG_SPI_CLK_SOURCE));

        /* Dividers */
        FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, HCLK_DIV, reg_tmp, BSP_CFG_HCLK_DIV);
        FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, QSPI2_DIV, reg_tmp, BSP_CFG_QSPICLK_DIV);

        /* Update the register */
        CRG_TOP->CLK_AMBA_REG = reg_tmp;
    }

    /*------------------ Configure SYS_PLL ----------------------*/

#if ((BSP_CFG_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_CLOCK_PLL) &&      \
    ((BSP_CFG_OQSPICLK_DIV == BSP_CLOCKS_OQSPI_CLK_DIV_1) ||     \
    ((BSP_CFG_PLL_SYS_MUL < BSP_CLOCKS_PLL_SYS_CLOCK_MUL_2_5) && \
    (BSP_CFG_OQSPICLK_DIV <= BSP_CLOCKS_OQSPI_CLK_DIV_2))))
 #error Selected BSP_CFG_OQSPICLK_DIV & BSP_CFG_PLL_SYS_MUL value combination is not supported!
#endif

/* If PLL is selected as system clock, enable and switch to it. */
#if BSP_CFG_CLOCK_SOURCE == BSP_CLOCKS_SOURCE_CLOCK_PLL
    if (BSP_CLOCKS_CLOCK_ENABLED(BSP_CFG_PLL_SYS_SOURCE))
    {
        pll_on();
        CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = BSP_CFG_PLL_SYS_MUL;

        // CRG_TOP->CLK_CTRL_REG_b.PLL_CPU_ENABLE = 1;
    }
#endif

    CRG_TOP->CLK_AMBA_REG_b.OQSPIF_DIV = BSP_CFG_OQSPICLK_DIV;

    /*------------------ Configure SPI_PLL ----------------------*/
#if BSP_CFG_SPI_CLK_SOURCE == BSP_CLOCKS_SOURCE_CLOCK_PLL_SPI
    CRG_TOP->CLK_CTRL_REG_b.PLL_SPI_CLK_SEL = BSP_CFG_PLL_SPI_MUL;
    CRG_TOP->CLK_CTRL_REG_b.PLL_SPI_ENABLE  = 1;
#endif

    /*------------------ Configure PERI_PLL ----------------------*/
#if BSP_CFG_PERI_CLK_SOURCE == BSP_CLOCKS_SOURCE_CLOCK_PLL_PERI
    CRG_TOP->CLK_CTRL_REG_b.PLL_PERI_ENABLE = 1;
#endif

    /* Wait to settle down the clocks */
    for (i = 32; i > 0; i--)
    {
        __ASM volatile ("nop");
        __ASM volatile ("nop");
    }

    /*-------------------Configure CRG_PER->CLK_COM_REG ------------------------*/
    {
        /* Write CLK_COM_REG fields into local var, to avoid multiple register access.*/
        uint32_t reg_tmp = 0;

        FSP_REG_VAR_FIELD_SET(CRG_PER, CLK_COM_REG, SPI_ENABLE, reg_tmp, 0);

        FSP_REG_VAR_FIELD_SET(CRG_PER, CLK_COM_REG, I2C_ENABLE, reg_tmp, 0);

        FSP_REG_VAR_FIELD_SET(CRG_PER,
                              CLK_COM_REG,
                              I2C_CLK_SEL,
                              reg_tmp,
                              BSP_CFG_I2C1CLK_SOURCE ==
                              BSP_CLOCKS_SOURCE_CLOCK_DIV1);

        FSP_REG_VAR_FIELD_SET(CRG_PER,
                              CLK_COM_REG,
                              I2C2_CLK_SEL,
                              reg_tmp,
                              BSP_CFG_I2C2CLK_SOURCE ==
                              BSP_CLOCKS_SOURCE_CLOCK_DIV1);

        FSP_REG_VAR_FIELD_SET(CRG_PER, CLK_COM_REG, UART_ENABLE, reg_tmp, 0);

        FSP_REG_VAR_FIELD_SET(CRG_PER, CLK_COM_REG, SDIO_ENABLE, reg_tmp, 1);

        FSP_REG_VAR_FIELD_SET(CRG_PER, CLK_COM_REG, AUXADC_ENABLE, reg_tmp, 0);

        /* Update the register */
        CRG_PER->CLK_COM_REG = reg_tmp;
    }

    /* Initialize SystemCoreClock variable. */
    SystemCoreClockUpdate();
}

#endif

/**
 \}
 \}
 \}
 */
