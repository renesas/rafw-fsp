/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 ****************************************************************************************
 *
 * @file rm_ble_recovery.h
 *
 * @brief Header of BLE recovery
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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
#ifndef RM_BLE_RECOVERY_H
#define RM_BLE_RECOVERY_H

#if defined(RM_BLE_RECOVERY_ENABLE)

/*******************************************************************************************************************//**
 * @addtogroup BLE_RECOVERY
 * @{
 ***********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "r_ble_api.h"

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

#if RM_BLE_RECOVERY_CB_ON_FAIL
typedef void (* rm_ble_recovery_fail_callback_t)(void);
#endif

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

#if RM_BLE_RECOVERY_CB_ON_FAIL
/*******************************************************************************************************************//****
 * @brief function to set fail callback 
***********************************************************************************************************************/
void RM_BLE_RECOVERY_set_fail_callback(void *callback);
#endif 

/*******************************************************************************************************************//****
 * @brief function to set fail callback 
***********************************************************************************************************************/
uint32_t RM_BLE_RECOVERY_running(void);

/*******************************************************************************************************************//****
 * @brief function to open and start the recovery 
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed
***********************************************************************************************************************/
fsp_err_t RM_BLE_RECOVERY_Open(void);

/*******************************************************************************************************************//****
 * @brief function to stop and close the recovery
 * @retval FSP_SUCCESS              - on case of success
 * @retval FSP_ERR_BLE_INIT_FAILED  - on case of failed
**********************************************************************************************************************/
fsp_err_t RM_BLE_RECOVERY_Close(void);

/*******************************************************************************************************************//**
 * @} (end addtogroup BLE_LOADER)
 **********************************************************************************************************************/

#endif  //defined(RM_BLE_RECOVERY_ENABLE)
#endif //RM_BLE_RECOVERY_H

/* EOF */