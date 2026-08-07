/**
 ****************************************************************************************
 *
 * @file app_common_support.h
 *
 * @brief common supporting for app
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
#include "oal.h"

#if !defined(_APP_COMMON_SUPPORT_H_)
#define _APP_COMMON_SUPPORT_H_


/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ***************************************************************************************
 * @brief Getting IP Address from DNS Server
 * @param[in] Host Name to connect
 * @return IP Address String
 ****************************************************************************************
 */
char* app_common_get_ip_from_normal_dns(char *hostname);

/**
 ***************************************************************************************
 * @brief Getting IP Address from Retention Memory
 * @param[in] Host Name to connect
 * @param[in] Retention Memory Region Name
 * @param[in] is_real_query - 1:query directly, 0:saved ip on RTM
 * @return IP Address String
 ****************************************************************************************
 */
char* app_common_get_ip_from_fast_dns_with_rtm(char *hostname, char *rtm_data_name, UINT8 is_real_query);

/** 
 * @brief some name of APIs changed depending SDK version 
 */
extern void fc80211_da16x_pri_pwr_down(unsigned char retention);
extern int RM_PMGR_W_dpm_wakeup_src_get(void);

#endif // _APP_COMMON_SUPPORT_H_

/* EOF */
