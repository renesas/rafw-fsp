/**
 ****************************************************************************************
 *
 * @file app_dpm_interface.h
 *
 * @brief DPM related APIs used to operate device on  platform.
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

#if !defined(_APP_DPM_INTERFACE_H_)
#define _APP_DPM_INTERFACE_H_

#include "aws_iot_error.h"
//#include "lwip/sockets.h"
//#include "lwip/netdb.h"
#include "user_dpm.h"

#include <stdbool.h>

#include "mbedtls/ssl.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif
#include "app_common_support.h"
/* Referring Feature for AWS-IOT-W */
#include "rm_awsiot_w_cfg.h"
#include "rm_aws_lwip_sock_wrap_w_api.h"

/** Used to indicate receiving timeout on DPM mode */
#define DPM_RCV_NO_CONNECT	0 ///< before connection with server
#define DPM_RCV_OK_CONNECT	1 ///< connection checked
#define DPM_RCV_OK_SLEEP	2 ///< ready to sleep

/** Used to reboot logic when connection failed on wakeup mode */
#define MAX_RECONN_COUNT_FOR_REBOOT		3 ///< max failure count to reboot

/*! print function for APP debugging */
#if !defined(APRINTF)
#define APRINTF(...) printf(__VA_ARGS__)
#endif

#if !defined(APRINTF_Y)
#define APRINTF_Y       APRINTF
#endif
#if !defined(APRINTF_I)
#define APRINTF_I       APRINTF
#endif
#if !defined(APRINTF_S)
#define APRINTF_S       APRINTF
#endif
#if !defined(APRINTF_E)
#define APRINTF_E       APRINTF
#endif

/**
 ****************************************************************************************
 * @brief Save the current TLS context on RTM region for TLS session
 * @param[in] _name Session name to save
 * @param[in] _tlsDataParams Handle for TLS parameters
 * @return IoT_Error_t type error
 ****************************************************************************************
 */
IoT_Error_t app_tls_save(const char *name, mbedtls_ssl_context *sslCtx);

/**
 ****************************************************************************************
 * @brief Load the saved TLS context from RTM region for TLS session
 * @param[in] _name Sesson name to load
 * @param[in] _tlsDataParams Handle for TLS parameters
 * @return IoT_Error_t type error
 ****************************************************************************************
 */
IoT_Error_t app_tls_restore(const char *name, mbedtls_ssl_context *sslCtx);

/**
 ****************************************************************************************
 * @brief Terminate the previous created TLS context on RTM region
 * @param[in] _name Session name to load
 * @return IoT_Error_t type error
 ****************************************************************************************
 */
IoT_Error_t app_tls_clear(const char *_name);

/**
 ****************************************************************************************
 * @brief Set the IP string of server to connect
 * @param[in] _ipStr IP string
 * @return void
 ****************************************************************************************
 */
void app_set_peer_ip_str(char *_ipStr);

/**
 ****************************************************************************************
 * @brief Set client port
 * @param[in] _port Port to save
 * @return UINT32 ER_ error type
 ****************************************************************************************
 */
UINT32 app_dpm_set_client_socket_port(UINT32 _port);

#if defined(__SUPPORT_AWS_IOT_W__) //awsupgradeport[[::for fleet provisioning
/**
 ****************************************************************************************
 * @brief Get the flag whether fleet provisioning needed or not
 * @return 0 or 1
 ****************************************************************************************
 */
UINT32 app_is_needed_fleet_provisioning(void);

/**
 ****************************************************************************************
 * @brief Save the provisioned info (thing name, private key, and certificate) into NVRAM
 * @param[in] _pcThingName the provisioned thing name from peer
 * @return 0 or -1
 ****************************************************************************************
 */
INT32 app_save_provisioned_info(char *_pcThingName);

/**
 ****************************************************************************************
 * @brief Get the registered thing name on peer
 * @return the registered thing name or NULL
 ****************************************************************************************
 */
char *app_get_registered_thing_name(void);

/**
 ****************************************************************************************
 * @brief Set the flag whether TLS restored or not
 * @param[in] _flag restored OK (1), restore NG (0)
 * @return void
 ****************************************************************************************
 */
void app_dpm_set_restoration_flag(UINT8 _flag);

/**
 ****************************************************************************************
 * @brief Get the flag whether TLS restored or not
 * @return 1(restored) or 0(not restored)
 ****************************************************************************************
 */
UINT8 app_dpm_get_tls_restoration_flag(void);

#endif //]]
#endif /* _APP_DPM_INTERFACE_H_ */

