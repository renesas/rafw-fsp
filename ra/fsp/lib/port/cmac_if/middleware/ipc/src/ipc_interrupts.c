/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 **************************************************************************************
 **
 *
 * @file ipc_interrupts.c
 *
 * @brief Intrrupts handling for IPC. V0_4_0
 *
 **************************************************************************************
 **
 */

/**
 * \addtogroup MIDDLEWARE
 \{
 * \addtogroup IPC
 \{
 */

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
#include "sdk_defs.h"
#include "ipc_config.h"
#include "ipc.h"
#include "ipc_interrupts.h"
#if defined(CMAC_CPU)
 #include "renesas_cmac.h"

 #if BSP_FEATURE_CODE_READY
  #include "cmac_slp_timer.h"
 #endif
#endif

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/// \cond
#if defined(CORTEX_M33)
 #if (dg_configSYSTEMVIEW)
  #include "SEGGER_SYSVIEW_FreeRTOS.h"
 #else
  #define SEGGER_SYSTEMVIEW_ISR_ENTER()
  #define SEGGER_SYSTEMVIEW_ISR_EXIT()
 #endif                                // dg_configSYSTEMVIEW
#endif

/// \endcond

/****************************************************************************
 *                          Function prototypes
 ****************************************************************************/

/** \addtogroup IPC_INTERRUPT_HANDLERS
 *  @{
 */
#if defined(CORTEX_M33)
 #if !BSP_FEATURE_BSP_HAS_ICU
void CMAC2SYS_Handler(void);

 #endif

 #if BSP_FEATURE_HAS_DSP
void DSP_Handler(void);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

/** @}*/

/****************************************************************************
 *                          External function prototypes
 ****************************************************************************/

/** \addtogroup IPC_PRIVATE_FUNCTIONS
 *  @{
 */
#if defined(CORTEX_M33)
extern void ipc_cmac2sys_isr_cb(void);

 #if BSP_FEATURE_HAS_DSP
extern void ipc_dsp2sys_isr_cb(void);

 #endif                                /* BSP_FEATURE_HAS_DSP */

#endif

/** @}*/

/****************************************************************************
 *                                Implementation
 ****************************************************************************/

/** \addtogroup IPC_INTERRUPT_HANDLERS
 *  @{
 */
#if defined(CORTEX_M33)

/**
 * \brief CMAC On Error critical event hook.
 * Weakly defined here as it may be defined in application
 */
__WEAK void sys_cmac_on_error_handler (void)
{
    ASSERT_ERROR(0);
}

 #if !BSP_FEATURE_BSP_HAS_ICU

/**
 * \brief CMAC2SYS interrupt handler
 *
 */
void CMAC2SYS_Handler (void)
{
    SEGGER_SYSTEMVIEW_ISR_ENTER();

    if (cmac_error_status & cmac_error)
    {
        // CMAC core on HF/NMI/WDOG, probably non-recoverable state
  #if (CONFIG_CMAC_NMI_ASSERTS_SYSCPU == 1)
        ASSERT_ERROR(0);               // block here
  #else
        sys_cmac_on_error_handler();   // let the application handle it
  #endif
    }

    // Handle the IPC event
    if (MEMCTRL->SYS_IRQ_CTRL_REG & MEMCTRL_SYS_IRQ_CTRL_REG_CMAC2SYS_IRQ_BIT_Msk)
    {
        ipc_cmac2sys_interrupt_clear();
        ipc_cmac2sys_isr_cb();
    }

  #if (CONFIG_CMAC_ASSERT_TRIGGERS_SYSCPU == 1)
    ipc_status_t sts = ipc_cmac2sys_status();
    if (sts.bits.error == IPC_STATUS_ERROR_CPU_ASSERT)
    {
        // CMAC core on software error state, block here
        ASSERT_ERROR(0);
    }
  #endif

    SEGGER_SYSTEMVIEW_ISR_EXIT();
}

 #endif

 #if BSP_FEATURE_HAS_DSP

/**
 * \brief DSP2SYS interrupt handler
 *
 */
void DSP_Handler (void)
{
    SEGGER_SYSTEMVIEW_ISR_ENTER();
    if (MEMCTRL->SYS_IRQ_CTRL_REG & MEMCTRL_SYS_IRQ_CTRL_REG_DSP2SYS_IRQ_BIT_Msk)
    {
        ipc_dsp2sys_interrupt_clear();
        ipc_dsp2sys_isr_cb();
    }

    SEGGER_SYSTEMVIEW_ISR_EXIT();
}

 #endif                                /* BSP_FEATURE_HAS_DSP */
#endif

/** @}*/

/** \addtogroup IPC_PUBLIC_FUNCTIONS
 *  @{
 */
#if defined(CORTEX_M33)
inline void ipc_sys2cmac_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, SYS2CMAC_IRQ_BIT);
}

 #if BSP_FEATURE_HAS_DSP
inline void ipc_sys2dsp_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, SYS2DSP_IRQ_BIT);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)
inline void ipc_cmac2sys_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, CMAC2SYS_IRQ_BIT);
}

 #if BSP_FEATURE_HAS_DSP
inline void ipc_cmac2dsp_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, CMAC2DSP_IRQ_BIT);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#else
inline void ipc_dsp2sys_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, DSP2SYS_IRQ_BIT);
}

inline void ipc_dsp2cmac_interrupt_set ()
{
    MEMCTRL->SET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, SET_SYS_IRQ_CTRL_REG, DSP2CMAC_IRQ_BIT);
}

#endif

#if defined(CORTEX_M33)
inline void ipc_cmac2sys_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, CMAC2SYS_IRQ_BIT);
}

 #if BSP_FEATURE_HAS_DSP
inline void ipc_dsp2sys_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, DSP2SYS_IRQ_BIT);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#elif defined(CMAC_CPU)
inline void ipc_sys2cmac_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, SYS2CMAC_IRQ_BIT);
}

 #if BSP_FEATURE_HAS_DSP
inline void ipc_dsp2cmac_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, DSP2CMAC_IRQ_BIT);
}

 #endif                                /* BSP_FEATURE_HAS_DSP */

#else
inline void ipc_sys2dsp_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, SYS2DSP_IRQ_BIT);
}

inline void ipc_cmac2dsp_interrupt_clear ()
{
    MEMCTRL->RESET_SYS_IRQ_CTRL_REG = REG_MSK(MEMCTRL, RESET_SYS_IRQ_CTRL_REG, CMAC2DSP_IRQ_BIT);
}

#endif

/** @}*/

/**
 \}
 \}
 */
