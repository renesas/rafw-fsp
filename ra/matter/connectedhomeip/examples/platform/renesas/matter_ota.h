/**
 ****************************************************************************************
 *
 * @file matter_ota.h
 *
 * @brief Sensor reference board configuration
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

 
#ifndef __MATTER_OTA_H__
#define __MATTER_OTA_H__


#ifdef __cplusplus
 extern "C" {
#endif

#include "sdk_defs.h"

/// Operation step of process
typedef enum {
    /// Init value
	MATTER_OTA_INIT,
    /// RTOS
	MATTER_OTA_RTOS,
    /// BLE firmware, for DA166x
	MATTER_OTA_BLE_FW,
    /// RTOS and BLE firmware, for DA166x
	MATTER_OTA_BLE_COMBO,
    /// MCU firmware, not DA16x
	MATTER_OTA_MCU_FW,
    /// Certificate or Key
	MATTER_OTA_CERT_KEY,
    //APP_CORE
    /// MCU firmware by Stream, not DA16x
	MATTER_OTA_MCU_FW_STREAM,
    /// Unknown value
	MATTER_OTA_UNKNOWN
} matter_ota_update_type;

UINT app_matter_ota_init(UINT fw_type, UINT64 len);
UINT app_matter_ota_download(UCHAR *rev_data, UINT rev_data_len);
UINT app_matter_ota_renew(void);
//UINT matter_ota_update_get_download_progress(matter_ota_update_type update_type);

#if defined (__IMG_UPDATE_BY_MCU__)
UINT matter_ota_update_by_mcu_init(UINT fw_type, UINT len);
UINT matter_ota_update_by_mcu_download(UCHAR *rev_data, UINT rev_data_len);
#endif
#ifdef __cplusplus
}

#include <lib/core/CHIPError.h>
void appError(CHIP_ERROR error);
#endif

#endif  // __MATTER_OTA_H__


