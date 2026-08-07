/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __DPMCONF_H__
#define __DPMCONF_H__

#if CFG_WIFI
/**
 * @file     dpmconf.h
 * @brief    DPM configuration
 * @details  This file defines the DPM configuration.
 *           This header file is used in RTOS & PTIM.
 */

/* Simulation */
/*#define TIM_SIMULATION*/ /* TIM_SIMULATION is defined in vcxproj of Visual Studio. */
#ifdef TIM_SIMULATION
#define DPM_RTC_CLK_SPEED_EN
#define DPM_RTC_CLK_SPEED 0
#endif

#undef  DPM_APTRK_CCA_MEASURE /* for SLR */
#define TIM_XTAL_CHANGE_EN /* TIM changes OSC into Xtal 32K */
#undef DPM_RTM_AUTO_CLEAR /* PTIM TEST FEATURE */

//#define NX_MDM_VER              20

//#define PTIM_SYS_CLK		80
//#define PTIM_MAC_CLK		40

/**
 * DEBUG FEATURE
 */
#undef DPM_TEST_ALWAYS_HI_CURRENT_CONTROL
#undef DPM_TEST_PBR_EVT_OFF
#undef DPM_TEST_FADC_V_CONTROL

/**
 * ##### RA6W1-20190113-LOW_CURRENT_MODE
 * 	DPM_TEST_ALWAYS_HI_CURRENT_CONTROL
 *	DPM_TEST_PBR_EVT_OFF
 *	DPM_TEST_FADC_V_CONTROL
 **/
#endif /*CFG_WIFI*/
#endif
