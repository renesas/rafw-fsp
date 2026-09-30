/**
 ****************************************************************************************
 *
 * @file app_aws_user_conf.h
 *
 * @brief User defines used to operate device on AWS platform.
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

#if !defined(_APP_AWS_USER_CONF_H_)
#define _APP_AWS_USER_CONF_H_

#include "FreeRTOS.h"
#include "rm_wifi.h"

/* Referring Feature for AWS-IOT-W */
#include "rm_awsiot_w_cfg.h"

/** Define whether persistent session used or not */
#if defined(__RUN_APP_SLEEP_2__)
#define USE_PERSISTENT_SESSION					0
#else
#define USE_PERSISTENT_SESSION					!(AWS_IOT_CLEAN_SESSION_ENABLE)
#endif

/** Define timer's id depending on SDK type */
#define APP_DPM_TIMER_ID	                    -1   ///< empty slot used

#define NETWORK_BUFFER_SIZE    ( 4096U )

/**
 * @brief The AWS IoT broker endpoint to connect to in the demo.
 *
 * @note Your AWS IoT Core endpoint can be found in the AWS IoT console under
 * Settings/Custom Endpoint, or using the DescribeEndpoint REST API (that can
 * be called with AWS CLI command line tool).
 *
 * #define democonfigMQTT_BROKER_ENDPOINT	  "...insert here..."
 */
#define democonfigMQTT_BROKER_ENDPOINT	        AWS_IOT_HOST_NAME

/*! Decide sample whether Dialog DPM sample or Amazaon original sample */
#define	APP_AWS_SHADOW							"awsMaT" ///< main thread name: length of name must be shorter than 6 characters
#define APP_AWS_S3_SIGV4						"sigv4_iot"
#define AWS_MQTT_PORT					 		AWS_IOT_MQTT_PORT ///< IoTHub endpoint port
#define AWS_HTTP_PORT					 		443 ///< IoTHub endpoint port

/**
 * @brief The port to use for the demo.
 *
 * In general, port 8883 is for secured MQTT connections.
 *
 * @note Port 443 requires use of the ALPN TLS extension with the ALPN protocol
 * name. Using ALPN with this demo would require additional changes, including
 * setting the `pAlpnProtos` member of the `NetworkCredentials_t` struct before
 * forming the TLS connection. When using port 8883, ALPN is not required.
 *
 * #define democonfigMQTT_BROKER_PORT    ( insert here. )
 */
#define democonfigMQTT_BROKER_PORT				AWS_MQTT_PORT

/**
 * @brief ALPN (Application-Layer Protocol Negotiation) protocol name for AWS IoT MQTT.
 *
 * This will be used if democonfigMQTT_BROKER_PORT is configured as 443 for the AWS IoT MQTT broker.
 * Please see more details about the ALPN protocol for AWS IoT MQTT endpoint
 * in the link below.
 * https://aws.amazon.com/blogs/iot/mqtt-with-tls-client-authentication-on-port-443-why-it-is-useful-and-how-it-works/
 */
#define AWS_IOT_MQTT_ALPN           "x-amzn-mqtt-ca"

/**
 * @brief This is the ALPN (Application-Layer Protocol Negotiation) string
 * required by AWS IoT for password-based authentication using TCP port 443.
 */
#define AWS_IOT_CUSTOM_AUTH_ALPN    "mqtt"

/* for certification method */
#define _AWS_UART_ADD_CERT_						0 ///< 0 is using internal key value of app__certi.h file, 1 is using externel cert- key by uart interface

#define AWS_CONFIG_PORT_DEF						AWS_MQTT_PORT ///< Default AWS server port
#define AWS_CONFIG_KEEP_INTERVAL_MIN			60 ///< Minimux interval for keepalive with AWS server
#define AWS_CONFIG_KEEP_INTERVAL_TST			120 ///< Short testing interval for keepalive with AWS server
#define AWS_CONFIG_KEEP_INTERVAL_DEF			1200 ///< Default interval for keepalive with AWS server
#define AWS_CONFIG_KEEP_INTERVAL_MAX			AWS_IOT_KEEP_ALIVE_SECONDS ///< Maximum interval for keepalive with AWS server, about 29 minutes

#define AWS_REV_NO_CONNECT_TIMEOUT              AWS_IOT_RX_NO_CONNECT_TIMEOUT ///Timeout for the AWS-IOT MQTT Receive function when not being connected
#define AWS_REV_OK_CONNECT_TIMEOUT              AWS_IOT_RX_OK_CONNECT_TIMEOUT ///Timeout for the AWS-IOT MQTT Receive function when being connected
#define AWS_REV_SLEEP_TIMEOUT                   AWS_IOT_RX_SLEEP_TIMEOUT ///Timeout for the AWS-IOT MQTT Receive function when going to sleep

#define AWS_RTM_NAME							"aws_rtm_dat" ///< RTM's name for AWS app

#define AWS_NVRAM_CONFIG_BROKER_URL				"AWS_BROKER" ///< AWS broker's URL on NVRAM

#if defined(__BLE_COMBO_REF__) //rrq61400work::FW imamges for combo
#define RTOS_NAME								"RRQ61400_FRTOS-GEN01.img"	///< OTA firmware name for RTOS
#define BLE_NAME								"RRQ61400_BLE_OTA.img"	///< OTA firmware name for Combo BLE f/w
#else
#define RTOS_NAME								"RRQ61000_FRTOS-GEN01.img" ///< OTA firmware name for RTOS
#endif

#define _NVRAM_SAVE_THING_FACORY_

#define SLEEP_MODE_FOR_NVRAM 					"setsleepMode" ///< sleep mode value on NVRAM
#define SLEEP_MODE2_RTC_TIME 					"sleepmodertctime" ///< wakeup timer interval by seconds on NVRAM

/* Platform Provisioning Mode Type */
#define AWS_MODE_GEN			10 //AWS
#define AWS_MODE_ATCMD	        11 //AWS + ATCMD	
#define AWS_MODE_FPGEN	        12 //AWS Fleet Provisionig
#define AWS_MODE_FPATCMD        13 //AWS Fleet Provisionig + ATCMD

#define AWS_TLS_SOCK_NAME						"aws_sockl" ///< AWS TLS session name on DPM mode
#define AWS_TCP_SOCK_NAME						APP_AWS_SHADOW ///< AWS TCP session name on DPM mode

#define SLEEP_2_USE_RTM							0		///< define whether RTM used or not on sleep mode 2: used - 1, not used - 0
#define SLEEP_2_WAKEUP_TIME_INTERVAL_SEC		30		///< wakeup timer's default interval: 30 seconds

/**
 * @brief The name of the operating system that the application is running on.
 * The current value is given as an example. Please update for your specific
 * operating system.
 */
#define democonfigOS_NAME                   	"FreeRTOS"

/**
 * @brief The version of the operating system that the application is running
 * on. The current value is given as an example. Please update for your specific
 * operating system version.
 */
#define democonfigOS_VERSION                	tskKERNEL_VERSION_NUMBER

/**
 * @brief The name of the hardware platform the application is running on. The
 * current value is given as an example. Please update for your specific
 * hardware platform.
 */
#define democonfigHARDWARE_PLATFORM_NAME    	"RA6Wx"

/**
 * @brief The name of the MQTT library used and its version, following an "@"
 * symbol.
 */
#define democonfigMQTT_LIB               		"core-mqtt@""v1.1.2"

/**
 * @brief The MQTT metrics string expected by AWS IoT.
 */
#define AWS_IOT_METRICS_STRING                                 \
    "?SDK=" democonfigOS_NAME "&Version=" democonfigOS_VERSION \
    "&Platform=" democonfigHARDWARE_PLATFORM_NAME "&MQTTLib=" democonfigMQTT_LIB

#endif /* _APP_AWS_USER_CONF_H_ */

/* EOF */
