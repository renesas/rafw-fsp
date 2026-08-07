/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef BSP_CLOCKS_RA6W1_H
#define BSP_CLOCKS_RA6W1_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "../all/bsp_delay.h"
#include "../all/bsp_mcu_api.h"
#include "../all/bsp_clocks.h"

/* TODO get rid of the following legacy definitions from sdk_defs.h: */
#define REG_GETF(base, reg, field) \
    (((base->reg) & (base ## _ ## reg ## _ ## field ## _Msk)) >> (base ## _ ## reg ## _ ## field ## _Pos))

#define REG_SETF(base, reg, field, new_val)                                \
    base->reg = ((base->reg & ~(base ## _ ## reg ## _ ## field ## _Msk)) | \
                 ((base ## _ ## reg ## _ ## field ## _Msk) &((new_val) << (base ## _ ## reg ## _ ## field ## _Pos))))

#define GLOBAL_INT_DISABLE()                      \
    do {                                          \
        unsigned int __l_irq_rest;                \
        __ASM volatile ("mrs   %0, primask  \n\t" \
                        "mov   r1, $1     \n\t"   \
                        "msr   primask, r1  \n\t" \
                        : "=r" (__l_irq_rest)     \
                        :                         \
                        : "r1"                    \
                        );                        \
        /*DBG_CONFIGURE_HIGH(CMN_TIMING_DEBUG, CMNDBG_CRITICAL_SECTION);*/

#define GLOBAL_INT_RESTORE()                                              \
    if (__l_irq_rest == 0) {                                              \
        /*DBG_CONFIGURE_LOW(CMN_TIMING_DEBUG, CMNDBG_CRITICAL_SECTION);*/ \
    }                                                                     \
    __ASM volatile ("msr   primask, %0  \n\t"                             \
                    :                                                     \
                    : "r" (__l_irq_rest)                                  \
                    :                                                     \
                    );                                                    \
    }                                                                     \
    while (0)

#define REG_SET_BIT(base, reg, field)                                 \
    do {                                                              \
        base->reg |= (1 << (base ## _ ## reg ## _ ## field ## _Pos)); \
    } while (0)

#define REG_CLR_BIT(base, reg, field)                           \
    do {                                                        \
        base->reg &= ~(base ## _ ## reg ## _ ## field ## _Msk); \
    } while (0)

/** Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define BSP_CFG_PLL_MUL_DIV(hz, mul, div)    (((hz) / (div)) * (mul))

/* Sys clock PLL */
#define BSP_CFG_PLL240M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 1, 2)        /* PLL 240MHz */
#define BSP_CFG_PLL192M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 2, 5)        /* PLL 192MHz */
#define BSP_CFG_PLL160M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 1, 3)        /* PLL 160MHz */
#define BSP_CFG_PLL137M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 6857, 24000) /* PLL 137MHz 137142857*/
#define BSP_CFG_PLL106M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 53, 240)     /* PLL 106MHz 106666666 */

/* SPI clock PLL */
#define BSP_CFG_PLL120M_HZ                         BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 1, 4)        /* PLL 120MHz */
#define BSP_CFG_PLL96M_HZ                          BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 1, 5)        /* PLL 96MHz */
#define BSP_CFG_PLL80M_HZ                          BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 1, 6)        /* PLL 80MHz */
#define BSP_CFG_PLL53M_HZ                          BSP_CFG_PLL_MUL_DIV(BSP_CFG_PLL480M_HZ, 53, 480)     /* PLL 53MHz */

/* E2Studio macros */

/* Disabled Vs Enable source clocks */
#define BSP_CLOCKS_CLOCK_DISABLED                  255
#define BSP_CLOCKS_CLOCK_ENABLED(clk)    ((clk) != BSP_CLOCKS_CLOCK_DISABLED)

#define BSP_CLOCKS_SOURCE_CLOCK_HCLK               5
#define BSP_CLOCKS_SOURCE_CLOCK_QSPI_CLK           6
#define BSP_CLOCKS_SOURCE_CLOCK_OQSPI_CLK          7
#define BSP_CLOCKS_SOURCE_CLOCK_PLL_SPI            9
#define BSP_CLOCKS_SOURCE_CLOCK_SPI_CLK            10
#define BSP_CLOCKS_SOURCE_CLOCK_PLL_PERI           13
#define BSP_CLOCKS_SOURCE_CLOCK_I2C_CLK            14
#define BSP_CLOCKS_SOURCE_CLOCK_I2C2_CLK           15
#define BSP_CLOCKS_SOURCE_CLOCK_PERI_CLK           16
#define BSP_CLOCKS_SOURCE_CLOCK_PLL_AUX            17
#define BSP_CLOCKS_SOURCE_CLOCK_FPLL               18
#define BSP_CLOCKS_SOURCE_CLOCK_SRC_CLK            19
#define BSP_CLOCKS_SOURCE_CLOCK_APU_CLK            20
#define BSP_CLOCKS_SOURCE_CLOCK_DMIC_CLK           21
#define BSP_CLOCKS_SOURCE_CLOCK_PCM_CLK            22
#define BSP_CLOCKS_SOURCE_CLOCK_LP_CLK             23

/* Value for BSP_CFG_PLL_MUL */
#define BSP_CLOCKS_PLL_CLOCK_MUL_12_1              1

/* Value for BSP_CFG_PLL_SYS_MUL*/
#define BSP_CLOCKS_PLL_SYS_CLOCK_MUL_1_2           0 /* 240M */
#define BSP_CLOCKS_PLL_SYS_CLOCK_MUL_2_5           1 /* 192M */
#define BSP_CLOCKS_PLL_SYS_CLOCK_MUL_1_3           2 /* 160M */
#define BSP_CLOCKS_PLL_SYS_CLOCK_MUL_6857_24000    3 /* 137M */
#define BSP_CLOCKS_PLL_SYS_CLOCK_MUL_53_240        4 /* 106M */

/* Values for BSP_CFG_PLL_SPI_MUL */
#define BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_4           0
#define BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_5           1
#define BSP_CLOCKS_PLL_SPI_CLOCK_MUL_1_6           2
#define BSP_CLOCKS_PLL_SPI_CLOCK_MUL_53_480        3
#define BSP_CLOCKS_PLL_SPI_CLOCK_MAX               (BSP_CLOCKS_PLL_SPI_CLOCK_MUL_53_480 + 1)

/* Value for BSP_CFG_PLL_PERI_DIV */
#define BSP_CLOCKS_PLL_PERI_CLOCK_DIV_1_6          1

/* Values for BSP_CFG_PLL_AUX_DIV */
#define BSP_CLOCKS_PLL_AUX_CLOCK_DIV_32            5

/* Value for BSP_CLOCKS_PLL_PHY_DIV */
#define BSP_CLOCKS_PLL_PHY_CLOCK_DIV_2             1

/* Values for BSP_CFG_FPLL_MUL */
#define BSP_CLOCKS_FPLL_CLOCK_MUL_1536_625         0 /* ~98M */
#define BSP_CLOCKS_FPLL_CLOCK_MUL_7056_3125        1 /* ~90M */

/* Value for WD Div */

// #define BSP_CLOCKS_WDOG_DIV_320                1

#define HW_CLK_DELAY_OVERHEAD_CYCLES    (72)
#define HW_CLK_CYCLES_PER_DELAY_REP     (4)

#define MAX_PLL_LCKCHK_TIME             (0x00007FFF)
#define BSP_DIVN_FREQ_HZ                (32000000U) // DIVN frequency is 32 MHz

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/**
 * \brief The type of the LP clock
 */
typedef enum lp_clk_is_type
{
    LP_CLK_IS_RCX,                     // 32K internal RC oscillator
    LP_CLK_IS_XTAL32K,                 // 32K Crystal oscillator
    LP_CLK_IS_SWCLK,                   // Test_clk
    LP_CLK_IS_INVALID
} lp_clk_is_t;

/**
 * \}
 */

/**
 * \brief The AMBA High-Performance Bus (AHB) clock divider
 */
typedef enum ahbdiv_type
{
    ahb_div1 = 0,                      //!< Divide by 1
    ahb_div2,                          //!< Divide by 2
    ahb_div4,                          //!< Divide by 4
    ahb_div8,                          //!< Divide by 8
    ahb_div16,                         //!< Divide by 16
} ahb_div_t;

/**
 * \brief The AMBA Peripheral Bus (APB) clock divider
 */
typedef enum apbdiv_type
{
    apb_div1 = 0,                      //!< Divide by 1
    apb_div2,                          //!< Divide by 2
    apb_div4,                          //!< Divide by 4
    apb_div8,                          //!< Divide by 8
    apb_div16,                         //!< Divide by 16
} apb_div_t;

/**
 * \brief Get the divider of the AMBA High Speed Bus.
 *
 * \return The AMBA High Speed Bus divider
 */
__STATIC_FORCEINLINE ahb_div_t hw_clk_get_hclk_div (void)
{
    return (ahb_div_t) CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV;
}

/**
 * \brief Set the divider of the AMBA High Speed Bus.
 *
 * \param div The AMBA High Speed Bus divider
 */
__STATIC_FORCEINLINE void hw_clk_set_hclk_div (ahb_div_t div)
{
    BSP_CHECK_DEBUG(div <= ahb_div16);

    GLOBAL_INT_DISABLE();
    CRG_TOP->CLK_AMBA_REG_b.HCLK_DIV = div;
    GLOBAL_INT_RESTORE();
}

/**
 * \brief The type of the system clock
 */
typedef enum sys_clk_is_type
{
    SYS_CLK_IS_NONE = 0,               /* RUNNING_AT_LP_CLK */
    SYS_CLK_IS_XTAL40M,                /* RUNNING_AT_XTAL40M */
    SYS_CLK_IS_LP,
    SYS_CLK_IS_PLL240M,                /* RUNNING_AT_PLL240M : descoped */
    SYS_CLK_IS_PLL192M,                /* RUNNING_AT_PLL192M : descoped */
    SYS_CLK_IS_PLL160M,                /* RUNNING_AT_PLL160M */
    SYS_CLK_IS_PLL137M,                /* SYS_CLK_IS_PLL137M */
    SYS_CLK_IS_PLL106M,                /* SYS_CLK_IS_PLL106M */
    SYS_CLK_IS_INVALID
} sys_clk_is_t;

/**
 * \brief The type of clock to be calibrated
 */
typedef enum cal_clk_sel_type
{
    CALIBRATE_XTAL32K = 0,
    CALIBRATE_RCX32K,
    CALIBRATE_DIVN_CLK,
} cal_clk_t;

/**
 * \brief The reference clock used for calibration
 */
typedef enum cal_ref_clk_sel_type
{
    CALIBRATE_REF_XTAL32K = 0,
    CALIBRATE_REF_RCX32K,
    CALIBRATE_REF_DIVN_CLK,
    CALIBRATE_REF_EXT,
} cal_ref_clk_t;

/**
 * \brief The system clock type
 *
 * \note Must only be used with functions cm_sys_clk_init/set()
 */
typedef enum sysclk_type
{
    sysclk_RC32    = 0,                //!< RC32
    sysclk_XTAL40M = 2,                //!< 40MHz
    sysclk_PLL480  = 6,                //!< 480MHz
    sysclk_LP      = 255,              //!< not applicable
} sys_clk_t;

/**
 * \brief The CPU clock type (speed)
 *
 */
typedef enum cpu_clk_type
{
    cpuclk_2M  = 2,                    //!< 2.5 MHz, divided by 16 in XTAL
    cpuclk_5M  = 5,                    //!< 5  MHz, divided by 8 in XTAL
    cpuclk_10M = 10,                   //!< 10 MHz, divided by 4 in XTAL
    cpuclk_20M = 20,                   //!< 20 MHz, divided by 2 in XTAL

    cpuclk_26M = 26,                   //!< 26 MHz, divided by 4 in SYSCLK 106MHz
    cpuclk_34M = 34,                   //!< 34 MHz, divided by 4 in SYSCLK 137MHz
    cpuclk_40M = 40,                   //!< 40 MHz, divided by 4 in SYSCLK 160MHz

    cpuclk_53M = 53,                   //!< 53 MHz, divided by 2 in SYSCLK 106MHz
    cpuclk_68M = 68,                   //!< 68 MHz, divided by 2 in SYSCLK 137MHz
    cpuclk_80M = 80,                   //!< 80 MHz, divided by 2 in SYSCLK 160MHz

    cpuclk_106M = 106,                 //!< 106 MHz
    cpuclk_137M = 137,                 //!< 137 MHz
    cpuclk_160M = 160                  //!< 160 MHz
} cpu_clk_t;

/**
 * \brief The type of the fpll clock
 */
typedef enum fpll_clock_type
{
    FPLL_98M = 0,
    FPLL_90M = 1,
    DIVN_40M = 2,
} fpll_clk_t;

/**
 * \brief The type of the fpll mode
 */
typedef enum fpll_clock_mode
{
    FPLL_MODE_USE_IRM = 0,
    FPLL_MODE_NO_IRM  = 1
} fpll_mode_t;

/**
 * \brief Get the XTAL32M settling time.
 *
 * \return The number of 256KHz clock cycles required for XTAL40M to settle
 */
__STATIC_FORCEINLINE uint16_t hw_clk_get_xtalm_settling_time (void)
{
    /* TODO for DA1640x */
    return 0;
}

/**
 * \brief Check if the XTAL40M is enabled.
 *
 * \return true if the XTAL40M is enabled, else false.
 */
__STATIC_INLINE bool hw_clk_check_xtal40m_status (void)
{
    /* TODO for DA1640x */
    return true;
}

/**
 * \brief Activate the XTAL40M.
 */
__STATIC_INLINE void hw_clk_enable_xtal40m (void)
{
    /* Do nothing if XTAL40M is already up and running. */
    if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_XTAL40M)
    {
        return;
    }

    // Check if TIM power domain is enabled
#if DEVICE_FPGA
#else
    BSP_CHECK_DEBUG(CRG_TOP->SYS_STATUS_REG_b.SYS_IS_UP);
#endif

#if DEVICE_FPGA
    REG_SET_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_EN);
#else
    REG_SET_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_EN);

    // Delay
    R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
    BSP_CHECK_DEBUG(CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_RDY);
#endif
}

/**
 * \brief Deactivate the XTAL40M.
 */
__STATIC_INLINE void hw_clk_disable_xtal40m (void)
{
    REG_CLR_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_EN);
}

/**
 * \brief Check if the XTAL40M has settled.
 *
 * \return true if the XTAL40M has settled, else false.
 */
__STATIC_INLINE bool hw_clk_is_xtalm_started (void)
{
    return true;
}

/**
 * \brief Return the clock used as the system clock.
 *
 * \return The type of the system clock
 */
__STATIC_FORCEINLINE sys_clk_is_t hw_clk_get_sysclk (void)
{
    sys_clk_is_t clk = SYS_CLK_IS_NONE;

    if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_XTAL40M)
    {
        clk = SYS_CLK_IS_XTAL40M;
    }
    else if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_LP_CLK)
    {
        clk = SYS_CLK_IS_LP;
    }
    else if (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
    {
        uint8_t mode = CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL;
        clk = (sys_clk_is_t) (SYS_CLK_IS_PLL240M + mode);
    }

    BSP_CHECK_DEBUG(clk < SYS_CLK_IS_INVALID);

    return clk;
}

/**
 * \brief Check whether the XTAL32K is the Low Power clock.
 *
 * \return true if XTAL32K is the LP clock, else false.
 */
__STATIC_INLINE bool hw_clk_lp_is_xtal32k (void)
{
    if (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_XTAL32K)
    {
        return true;
    }

    return false;
}

/**
 * \brief Set RCX as the Low Power clock.
 *
 * \warning The RCX must have been enabled before calling this function!
 *
 * \note Call with interrupts disabled to ensure that CLK_CTRL_REG
 *       read/modify/write operation is not interrupted
 */
__STATIC_INLINE void hw_clk_lp_set_rcx (void)
{
#if DEVICE_FPGA
#else
    BSP_CHECK_DEBUG(__get_PRIMASK() == 1 || __get_BASEPRI());
    BSP_CHECK_DEBUG(CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_LP_CLK);

    REG_SET_BIT(RTC, CLK_XTAL32K_REG, PDB_OSC_EN);
    RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = LP_CLK_IS_RCX;
#endif
}

/**
 * \brief Set XTAL32K as the Low Power clock.
 *
 * \warning The XTAL32K must have been enabled before calling this function!
 *
 * \note Call with interrupts disabled to ensure that CLK_CTRL_REG
 *       read/modify/write operation is not interrupted
 */
__STATIC_INLINE void hw_clk_lp_set_xtal32k (void)
{
    BSP_CHECK_DEBUG(__get_PRIMASK() == 1 || __get_BASEPRI());
#if DEVICE_FPGA
#else
    BSP_CHECK_DEBUG(CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_LP_CLK);
#endif
    REG_SET_BIT(RTC, CLK_XTAL32K_REG, XTAL_BAT_EN);
    RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = LP_CLK_IS_XTAL32K;
}

/**
 * \brief Set an external SWCLK as the Low Power clock.
 *
 * \note Call with interrupts disabled to ensure that CLK_CTRL_REG
 *       read/modify/write operation is not interrupted
 */
__STATIC_INLINE void hw_clk_lp_set_swclk (void)
{
    BSP_CHECK_DEBUG(__get_PRIMASK() == 1 || __get_BASEPRI());
#if DEVICE_FPGA
#else
    BSP_CHECK_DEBUG(CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_LP_CLK);
#endif
    RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL = LP_CLK_IS_SWCLK;
}

/**
 * \brief Enable RCX but does not set it as the LP clock.
 */
__STATIC_INLINE void hw_clk_enable_rcx (void)
{
    REG_SET_BIT(RTC, CLK_XTAL32K_REG, PDB_OSC_EN);
}

/**
 * \brief Disable RCX.
 *
 * \warning RCX must not be the LP clock
 */
__STATIC_INLINE void hw_clk_disable_rcx (void)
{
    if (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL != LP_CLK_IS_RCX)
    {
        REG_CLR_BIT(RTC, CLK_XTAL32K_REG, PDB_OSC_EN);
    }
}

/**
 * \brief Enable XTAL32K but do not set it as the LP clock.
 */
__STATIC_INLINE void hw_clk_enable_xtal32k (void)
{
    REG_SET_BIT(RTC, CLK_XTAL32K_REG, XTAL_BAT_EN);
}

/**
 * \brief Enable XTAL32K from XTAL40MHz but do not set it as the LP clock.
 */
__STATIC_INLINE void hw_clk_enable_external (void)
{
}

/**
 * \brief Disable XTAL32K.
 *
 * \warning XTAL32K must not be the LP clock.
 */
__STATIC_INLINE void hw_clk_disable_xtal32k (void)
{
    if (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL != LP_CLK_IS_XTAL32K)
    {
        REG_CLR_BIT(RTC, CLK_XTAL32K_REG, XTAL_BAT_EN);
    }
}

/**
 * \brief Disable XTAL32K from XTAL40M.
 *
 * \warning XTAL32K must not be the LP clock.
 */
__STATIC_INLINE void hw_clk_disable_external (void)
{
    BSP_CHECK_DEBUG(RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_XTAL32K);

    REG_CLR_BIT(RTC, CLK_XTAL32K_REG, XTAL_BAT_EN);
}

/**
 * \brief Set System clock.
 *
 * \param[in] mode The new system clock.
 *
 * \note System clock switch to PLL is only allowed when current system clock is XTAL40M.
 * System clock switch from PLL is only allowed when new system clock is XTAL40M.
 */
__STATIC_FORCEINLINE void hw_clk_set_sysclk (sys_clk_is_t mode)
{
    /* Make sure a valid sys clock is requested */
    BSP_CHECK_DEBUG(mode <= SYS_CLK_IS_PLL106M);

    GLOBAL_INT_DISABLE();
    if (mode < SYS_CLK_IS_PLL240M)
    {
        // sys_clk_is_t clk = hw_clk_get_sysclk();
        if (mode == SYS_CLK_IS_XTAL40M)
        {
            REG_SET_BIT(CRG_COM, XTAL40M_CTRL_REG, XTAL40M_EN);
        }
        else if (mode == SYS_CLK_IS_LP)
        {
            REG_SET_BIT(CRG_COM, XTAL32K_CTRL_REG, XTAL32K_ENABLE);
            BSP_CHECK_DEBUG(0);
        }

        CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL = mode;
    }
    else
    {
        sys_clk_is_t clk = hw_clk_get_sysclk();

        if ((clk != SYS_CLK_IS_XTAL40M) && (clk != SYS_CLK_IS_LP))
        {
            CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL = SYS_CLK_IS_XTAL40M; // XTAL
            while (CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
            {
            }
        }

        /* PLL_CLK_SEL,
         * 0: set to SYS_CLK=240Mhz
         * 1: set to SYS_CLK=192Mhz
         * 2: set to SYS_CLK=160Mhz
         * 3: set to SYS_CLK=137.14Mhz
         * 4: set to SYS_CLK=106Mhz
         */
        CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 0x3 & (uint32_t) (mode - SYS_CLK_IS_PLL240M);

        CRG_TOP->CLK_CTRL_REG_b.PLL_CPU_ENABLE = 1;

        CRG_TOP->CLK_CTRL_REG_b.SYS_CLK_SEL = 3; // PLL //bug here
    }

    GLOBAL_INT_RESTORE();

    /* Wait until the switch is done! */
    switch (mode)
    {
        case SYS_CLK_IS_XTAL40M:
        {
            while (!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_XTAL40M)
            {
            }

            return;
        }

        case SYS_CLK_IS_LP:
        {
            while (!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_LP_CLK)
            {
            }

            return;
        }

        case SYS_CLK_IS_PLL160M:
        case SYS_CLK_IS_PLL137M:
        case SYS_CLK_IS_PLL106M:
        {
            while (!CRG_TOP->CLK_CTRL_REG_b.RUNNING_AT_PLL)
            {
            }

            return;
        }

        default:
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Enable the PLL.
 */
uint32_t hw_clk_pll_sys_on(void);

/**
 * \brief Disable the PLL.
 *
 * \warning The System clock must have been set to XTAL40M before calling this function!
 */
void hw_clk_pll_sys_off(void);

/**
 * \brief Check if the PLL is enabled.
 *
 * \return true if the PLL is enabled, else false.
 */
__STATIC_FORCEINLINE bool hw_clk_check_pll_status (void)
{
    return CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_EN;
}

/**
 * \brief Check if the PLL is on and has locked.
 *
 * \return true if the PLL has locked, else false.
 */
__STATIC_FORCEINLINE bool hw_clk_is_pll_locked (void)
{
    return CRG_COM->PLL1_ARM_CTRL_REG_b.PLL_LOCK;
}

/**
 * \brief Activate a System clock.
 *
 * \param[in] clk The clock to activate.
 */
__STATIC_FORCEINLINE void hw_clk_enable_sysclk (sys_clk_is_t clk)
{
    switch (clk)
    {
        case SYS_CLK_IS_LP:
        {
            if (hw_clk_lp_is_xtal32k() == false)
            {
                hw_clk_enable_xtal32k();
            }

            break;
        }

        case SYS_CLK_IS_XTAL40M:
        {
            // CRG_DIGPLL->PLL320M_CFG1_REG_b.ENABLE_PLL320M = 0;
            hw_clk_enable_xtal40m();

            // CRG_TOP->CLK_CTRL_REG_b.DIVC_CLK_SEL = 0;  // 0: div1, 1: div2
            break;
        }

        case SYS_CLK_IS_PLL160M:
        {
            CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 2;
            break;
        }

        case SYS_CLK_IS_PLL137M:
        {
            // CRG_DIGPLL->PLL320M_CFG1_REG_b.ENABLE_PLL320M = 1;
            // while(CRG_DIGPLL->PLL320M_STATUS_REG_b.PLL320M_PLL_OK==0);
            CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 3;
            break;
        }

        case SYS_CLK_IS_PLL106M:
        {
            // CRG_DIGPLL->PLL320M_CFG1_REG_b.ENABLE_PLL320M = 1;
            // while(CRG_DIGPLL->PLL320M_STATUS_REG_b.PLL320M_PLL_OK==0);
            CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL = 4;
            break;
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Deactivate a System clock.
 *
 * \param[in] clk The clock to deactivate.
 */
__STATIC_FORCEINLINE void hw_clk_disable_sysclk (sys_clk_is_t clk)
{
    switch (clk)
    {
        case SYS_CLK_IS_XTAL40M:
        {
            hw_clk_disable_xtal40m();

            return;
        }

        case SYS_CLK_IS_LP:
        {
            hw_clk_disable_rcx();

            return;
        }

        case SYS_CLK_IS_PLL160M:
        {
            // TODO:
            return;
        }

        case SYS_CLK_IS_PLL137M:
        {
            // TODO:
            return;
        }

        case SYS_CLK_IS_PLL106M:
        {
            // TODO:
            return;
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Check if a System clock is enabled.
 *
 * \return true if the System clock is enabled, else false.
 */
__STATIC_INLINE bool hw_clk_is_enabled_sysclk (sys_clk_is_t clk)
{
    switch (clk)
    {
        case SYS_CLK_IS_XTAL40M:
        {
            return hw_clk_check_xtal40m_status();
        }

        case SYS_CLK_IS_PLL160M:
        {
            if (CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL == 2)
            {
                return true;
            }

            break;
        }

        case SYS_CLK_IS_PLL137M:
        {
            if (CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL == 3)
            {
                return true;
            }

            break;
        }

        case SYS_CLK_IS_PLL106M:
        {
            if (CRG_TOP->CLK_CTRL_REG_b.PLL_CLK_SEL == 4)
            {
                return true;
            }

            break;
        }

        default:
        {
            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
            break;
        }
    }

    return false;
}

/**
 * \brief Configure pin to connect an external digital clock.
 */
__STATIC_INLINE void hw_clk_configure_ext32k_pins (void)
{
#if DEVICE_FPGA
#else

    // GPIO-> P0_23_MODE_REG = 0;
#endif
}

/**
 * \brief Activate a FPLL clock.
 *
 * \param[in] freq_type The clock to activate.
 * \param[in] fpll_mode The mode of clock.
 */
__STATIC_INLINE void hw_clk_enable_fpll (fpll_clk_t freq_type, fpll_mode_t fpll_mode)
{
    RTC->LDO_ENABLE_REG_b.LDO_EN_LDO_PLL1  = 1;
    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_EN = 1;

    // CRG_COM->PLL1_ARM_CTRL_REG_b.DPLL1_EN = 1;
    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_FPLL_EN = 1;
    if (freq_type == DIVN_40M)
    {
        FPLL->PLLD_CTRL_REG_b.FPLL_EN = 1;

        FPLL->PLLD_CTRL_REG_b.PFD_CP_EN = 0;
        FPLL->PLLD_CTRL_REG_b.CLKOUT_EN = 0;
        FPLL->PLLD_CTRL_REG_b.VCO_EN    = 0;
    }
    else if ((freq_type == FPLL_98M) || (freq_type == FPLL_90M))
    {
        // FPLL->PLLD_CTRL_REG_b.FBDIV_SEL = fpll_mode;
        FPLL->PLLD_IRQ_MASK_REG_b.MIRQ_PLL_LOCK      = 1;
        FPLL->PLLD_IRQ_MASK_REG_b.MIRQ_PLL_LOST_LOCK = 1;

        FPLL->PLLD_CONFIG_REG_b.INDIV     = 5;
        FPLL->PLLD_CONFIG_REG_b.OUTDIV    = 3;
        FPLL->PLLD_CONFIG_REG_b.BIAS_HOLD = 0;
        if (fpll_mode == FPLL_MODE_NO_IRM)
        {
            if (freq_type == FPLL_98M)
            {
                // DIV = 98.304*2 / 5 (freq after input divider)=39.3216, INT = int(DIV) = 39 (0x27), FRAC = (DIV-INT)*2^13 = 2634 (0x0A4A)
                FPLL->PLLD_FBDIV_REG_b.FBDIV = (0x27 << 13 | (0xA << 8) | (0x4A));
            }
            else if (freq_type == FPLL_90M)
            {
                // DIV = 36.12672, INT = 36 (0x24), FRAC = 1038 (0x040E)
                FPLL->PLLD_FBDIV_REG_b.FBDIV = (0x24 << 13 | (0x4 << 8) | (0x0E));
            }
        }

        FPLL->PLLD_CTRL_REG_b.VCO_EN    = 1;
        FPLL->PLLD_CTRL_REG_b.FPLL_EN   = 1;
        FPLL->PLLD_CTRL_REG_b.PFD_CP_EN = 1;
        FPLL->PLLD_CTRL_REG_b.CLKOUT_EN = 1;

        // Wait for flag_lock to go HIGH
        while (FPLL->PLLD_STATUS_REG_b.STA_PLL_LOCK == 0)
        {
            ;
        }

        FPLL->PLLD_CTRL_REG_b.CLKOUT_EN = 1;
    }
}

/**
 * \brief Deactivate a System clock.
 *
 * \param[in] clk The clock to deactivate.
 */
__STATIC_INLINE void hw_clk_disable_fpll (sys_clk_is_t clk)
{
    switch (clk)
    {
        case SYS_CLK_IS_XTAL40M:
        {
            hw_clk_disable_xtal40m();

            return;
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Configure XTAL40M.
 */
void hw_clk_xtalm_configure(void);

/**
 * \brief Perform XTAL40M RCOSC amplitude temperature compensation.
 */
void hw_clk_xtalm_compensate_amp(void);

/**
 * \brief Update XTAL40M Ready IRQ counter.
 *
 * \return The difference between the new and the old XTAL40M Ready IRQ counter
 *         in cycles of 32KHz clocks.
 */
int16_t hw_clk_xtalm_update_rdy_cnt(void);

/**
 * \brief Enable PLL
 *
 * \details
 */
void pll_on(void);

/**
 * \brief Disable PLL
 *
 * \details
 */
void pll_off(void);

/**
 * \brief Set Low Power clock.
 *
 * \param[in] mode The new low power clock.
 */
__STATIC_INLINE void hw_clk_set_lpclk (lp_clk_is_t mode)
{
    GLOBAL_INT_DISABLE();
    switch (mode)
    {
        case LP_CLK_IS_RCX:
        {
            hw_clk_lp_set_rcx();
            break;
        }

        case LP_CLK_IS_SWCLK:
        {
            hw_clk_lp_set_swclk();
            break;
        }

        case LP_CLK_IS_XTAL32K:
        {
            hw_clk_lp_set_xtal32k();
            break;
        }

        default:
        {
            BSP_CHECK_DEBUG(0);
            break;
        }
    }

    GLOBAL_INT_RESTORE();
}

/**
 * \brief Activate a Low Power clock.
 *
 * \param[in] clk The clock to activate.
 */
__STATIC_INLINE void hw_clk_enable_lpclk (lp_clk_is_t clk)
{
    switch (clk)
    {
        case LP_CLK_IS_RCX:
        {
            hw_clk_enable_rcx();

            return;
        }

        case LP_CLK_IS_XTAL32K:
        {
            hw_clk_enable_xtal32k();

            return;
        }

        case LP_CLK_IS_SWCLK:
        {
            // Nothing to do for SWCLK LP clock
            return;
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Deactivate a Low Power clock.
 *
 * \param[in] clk The clock to deactivate.
 */
__STATIC_INLINE void hw_clk_disable_lpclk (lp_clk_is_t clk)
{
    switch (clk)
    {
        case LP_CLK_IS_RCX:
        {
            hw_clk_disable_rcx();

            return;
        }

        case LP_CLK_IS_XTAL32K:
        {
            hw_clk_disable_xtal32k();

            return;
        }

        case LP_CLK_IS_SWCLK:
        {
            // Nothing to do for SWCLK LP clock
            return;
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);
    }
}

/**
 * \brief Enable clock for specific UART channel
 *
 * \param[in] channel UART channel to activate clock
 */
__STATIC_INLINE void hw_clk_enable_uart_w_clk (uint8_t channel)
{
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    switch (channel)
    {
        case 0:
        {
            CRG_PER->CLK_COM_REG_b.UART_ENABLE = 1;
            break;
        }

        case 1:
        {
            CRG_PER->CLK_COM_REG_b.UART2_ENABLE = 1;
            break;
        }

        case 2:
        {
            CRG_PER->CLK_COM_REG_b.UART3_ENABLE = 1;
            break;
        }
    }

    FSP_CRITICAL_SECTION_EXIT;
}

/**
 * \brief Disable clock for specific UART channel
 *
 * \param[in] channel UART channel to deactivate clock
 */
__STATIC_INLINE void hw_clk_disable_uart_w_clk (uint8_t channel)
{
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    switch (channel)
    {
        case 0:
        {
            CRG_PER->CLK_COM_REG_b.UART_ENABLE = 0;
            break;
        }

        case 1:
        {
            CRG_PER->CLK_COM_REG_b.UART2_ENABLE = 0;
            break;
        }

        case 2:
        {
            CRG_PER->CLK_COM_REG_b.UART3_ENABLE = 0;
            break;
        }
    }

    FSP_CRITICAL_SECTION_EXIT;
}

/**
 * \brief Check whether the RCX is the Low Power clock.
 *
 * \return true if RCX is the LP clock, else false.
 */
__STATIC_INLINE bool hw_clk_lp_is_rcx (void)
{
    if (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_RCX)
    {
        return true;
    }

    return false;
}

/**
 * \brief Check whether the RCX is the Low Power clock.
 *
 * \return true if RCX is the LP clock, else false.
 */
__STATIC_INLINE bool hw_clk_lp_is_swclk (void)
{
    if (RTC->CLK_XTAL32K_REG_b.XTAL_CLK_SEL == LP_CLK_IS_SWCLK)
    {
        return true;
    }

    return false;
}

/**
 * \brief Check whether a clock is the Low Power clock.
 *
 * \param[in] clk The clock to check.
 *
 * \return true if clk is the Low Power clock, else false.
 */
__STATIC_INLINE bool hw_clk_lpclk_is (lp_clk_is_t clk)
{
    switch (clk)
    {
        case LP_CLK_IS_RCX:
        {
            return hw_clk_lp_is_rcx();
        }

        case LP_CLK_IS_XTAL32K:
        {
            return hw_clk_lp_is_xtal32k();
        }

        case LP_CLK_IS_SWCLK:
        {
            return hw_clk_lp_is_swclk();
        }

        default:

            /* An invalid clock is requested */
            BSP_CHECK_DEBUG(0);

            return false;
    }
}

/**
 * \brief Return the clock used as the Low Power clock.
 *
 * \return The type of the Low Power clock
 *
 */
__STATIC_INLINE lp_clk_is_t hw_clk_get_lpclk (void)
{
    unsigned int lp_clk;
    for (lp_clk = LP_CLK_IS_RCX; lp_clk < LP_CLK_IS_INVALID; (unsigned int) lp_clk++)
    {
        if (hw_clk_lpclk_is((lp_clk_is_t) lp_clk))
        {
            return (lp_clk_is_t) lp_clk;
        }
    }

    BSP_CHECK_DEBUG(0);

    return LP_CLK_IS_INVALID;
}

void bsp_clock_init_rrq61(void);

/*******************************************************************************************************************//**
 * @brief Enable the low-power crystal oscillator.
 *
 * This function enables the low-power 32kHz crystal oscillator by setting the XTAL_BAT_EN bit in the RTC register.
 **********************************************************************************************************************/
__STATIC_INLINE void bsp_prv_lpclk_xtal_on ()
{
    RTC->CLK_XTAL32K_REG_b.XTAL_BAT_EN = 1;
}

/*******************************************************************************************************************//**
 * @brief Enable the low-power oscillator.
 *
 * This function enables the low-power internal RC oscillator by setting the PDB_OSC_EN bit in the RTC register.
 **********************************************************************************************************************/
__STATIC_INLINE void bsp_prv_lpclk_osc_on ()
{
    RTC->CLK_XTAL32K_REG_b.PDB_OSC_EN = 1;
}

lp_clk_is_t bsp_prv_lpclk_get();
fsp_err_t   bsp_prv_lpclk_select(lp_clk_is_t type);
void        bsp_prv_rtc_mirror_init(void);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
