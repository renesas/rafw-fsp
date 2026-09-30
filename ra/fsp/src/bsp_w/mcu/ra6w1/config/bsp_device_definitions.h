/**
 * \addtogroup BSP_CONFIG_DEFINITIONS
 * \{
 * \addtogroup BSP_CFG_DEF_DEVICE_MAP Device-Map Definitions
 *
 * \brief Device-Map Definitions. Macros for all the devices supported by SDK10.
 *
 *\{
 */

/**
 ****************************************************************************************
 *
 * @file bsp_device_definitions.h
 *
 * @brief Board Support Package. Device-Map definitions.
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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

#ifndef BSP_DEVICE_DEFINITIONS_H_
#define BSP_DEVICE_DEFINITIONS_H_

/*
 * Available public macros characterizing each product supported by SDK10.
 * The variable dg_configDEVICE *MUST* take one of the following values
 * i.e. dg_configDEVICE=DA14680_01
 * The variable *MUST* be visible to both the compiler and the assembler.
 */
#define DA14680_01          (DA14680 | _DEVICE_MK_IC_VER(A, E))
#define DA14681_01          (DA14681 | _DEVICE_MK_IC_VER(A, E))
#define DA14682_00          (DA14682 | _DEVICE_MK_IC_VER(B, B))
#define DA14683_00          (DA14683 | _DEVICE_MK_IC_VER(B, B))

#define DA14691_00          (DA14691 | _DEVICE_MK_IC_VER(A, B))
#define DA14693_00          (DA14693 | _DEVICE_MK_IC_VER(A, B))
#define DA14695_00          (DA14695 | _DEVICE_MK_IC_VER(A, B))
#define DA14697_00          (DA14697 | _DEVICE_MK_IC_VER(A, B))
#define DA14699_00          (DA14699 | _DEVICE_MK_IC_VER(A, B))

#define DA14870_00          (DA14870 | _DEVICE_MK_IC_VER(A, A))
#define DA14871_00          (DA14871 | _DEVICE_MK_IC_VER(A, A))
#define DA14872_00          (DA14872 | _DEVICE_MK_IC_VER(A, A))
#define DA14873_00          (DA14873 | _DEVICE_MK_IC_VER(A, A))

#define DA16400_00          (DA16400 | _DEVICE_MK_IC_VER(A, A))
#define DA16400_10          (DA16400 | _DEVICE_MK_IC_VER(B, A))

#define D3095_10            (D3095 | _DEVICE_MK_IC_VER(B, A))

#ifndef dg_configDEVICE
 #define dg_configDEVICE    DA16400_00
#endif

#include "bsp_device_definitions_internal.h"

/*
 * Backward compatibility macros.
 * Useful for applications developed with older SDK versions.
 * DEVICE_DA146XX can be assigned the desired device.
 */

/* DA14680 family substitution (with the exception of uartboot loader) */
#define DEVICE_DA14680    DA14683_00

/* DA14690 family substitution */
#define DEVICE_DA1469x    DA14699_00

#endif                                 /* BSP_DEVICE_DEFINITIONS_H_ */

/**
 \}
 \}
 */
