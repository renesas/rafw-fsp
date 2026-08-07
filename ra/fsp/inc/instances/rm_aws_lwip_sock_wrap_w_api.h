/*
 * rm_aws_lwip_sock_wrap_w_api.h
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

#ifndef IOT_APP_SUPPORT_SOCKET_WRAPPER_INTERFACE_H_
#define IOT_APP_SUPPORT_SOCKET_WRAPPER_INTERFACE_H_
#include <stdbool.h>

/** Used to indicate receiving timeout on DPM mode */
#define DPM_RCV_NO_CONNECT	0 ///< before connection with server
#define DPM_RCV_OK_CONNECT	1 ///< connection checked
#define DPM_RCV_OK_SLEEP	2 ///< ready to sleep

#define WAKEUP_SOURCE_FN      RM_PMGR_W_dpm_wakeup_src_get

typedef enum {
    PROP_OP_GET,
    PROP_OP_SET,
	PROP_OP_DEL,
} property_op_t;


/******************************************************************************************************************//**
 * @ingroup RENESAS_NETWORKING_INTERFACES
 * @defgroup RM_AWS_LWIP_SOCK_WRAP_W_API RM_AWS_LWIP_SOCK_WRAP_W Interface
 * @brief Interface for accessing AWS_LWIP_SOCK_WRAP_W Storage.
 *
 * @section RM_AWS_LWIP_SOCK_WRAP_W_API_SUMMARY Summary
 * This section defines the API for the AWS_LWIP_SOCK_WRAP_W ( Networking) Module.
 * The AWS_LWIP_SOCK_WRAP_W Module provides interface to control, access, write to lwIP .
 *
 *
 *
 * @{
 *********************************************************************************************************************/
/**
 * @brief data used to save persistent information on RTM for  platform
 */
typedef struct _app_dpm_info_rtm {
    INT32 tid; ///< doorlock close user timer id
    INT32 interval; ///< doorlock close user timer timeout(sec) default 10sec.
    INT32 bindPort; ///< tcp client bind port number
    INT32 deltaDir; ///< delta direction
    INT32 FOTAStat; ///< FOTA status
    UINT8 FOTAUrl[256]; ///< Server S3 url for FOTA
    char ServrIp[16]; ///< server ip string of the  server
#if defined(__SUPPORT_AZURE_IOT__)
    UINT16 packetIdSub; ///< subscribe packet ID when connected first
    UINT8 qosSubCount; ///< subscribe item count when connected first
    UINT32 qosSubReturn[8]; ///< subscribe QOS policy when connected first
#endif // (__SUPPORT_AZURE_IOT__)
} app_dpm_info_rtm;

typedef enum {
    PROP_SVR_IP,
    PROP_SNTP,
    PROP_DNS_IP,
} property_id_t;

typedef struct {
    property_id_t id;
    property_op_t op;
    void *value; // pointer to value (can be int*, float*, struct*, etc.)
} property_msg_t;

/**
 * @brief Sleep mode enum
 *
 * Enumeration structure for Sleep mode
 */
typedef enum {
    SLEEP_MODE_NONE = 0,
#if 0 //org: not supported
    SLEEP_MODE_1,       ///< PMGR_LLD_STATE1: Full power down , not a SW feature
    SLEEP_MODE_2,       ///< PMGR_LLD_STATE2 or PMGR_LLD_STATE3
#else
    SLEEP_MODE_2 = 2,   ///< PMGR_LLD_STATE2 or PMGR_LLD_STATE3
#endif
    SLEEP_MODE_3,       ///< PMGR_LLD_DPM: DPM mode

} APPSleepMode;

/**
 * @brief The mode types of KA or RTC wakeup timer
 *
 * Enumeration structure for wakeup timer mode
 */
typedef enum {
    DPM_RTC_NORMAL_MODE = 0,
    DPM_RTC_ABNORMAL_MODE_1,
    DPM_RTC_ABNORMAL_MODE_2,
    DPM_RTC_ABNORMAL_MODE_3,
    DPM_RTC_ABNORMAL_MODE_4,
    DPM_RTC_ABNORMAL_MODE_5,
    DPM_RTC_ABNORMAL_MODE_6,
    DPM_RTC_ABNORMAL_MODE_7,
    DPM_RTC_ABNORMAL_MODE_8,
    DPM_RTC_ABNORMAL_MODE_9,

} APPTimerMode;

typedef void (*app_dpm_timer_callback)(UINT8 _resetTimer, APPTimerMode _mode);
typedef void (*exit_sleep_thread_callback)(void);

/// internal used RTM structure
typedef struct _InternalRTM {
    /// timer id
    INT32 tid;
    /// timer timeout (by seconds)
    INT32 interval;
    /// timer purpose - 0: normal, 1~9: abnormal
    UINT8 mode;
} InternalRTM;

/// structure for DPM App thread
typedef struct _dpmAppThreadInfo {
    TaskHandle_t thread;

    UINT32 entryInput;
    UINT32 pAppData;
    UINT32 RTMDataSize;
    UINT32 internalStatus;
    UINT32 wakeUpTime;
    UINT32 dpmPort;

    char *threadName;
    char *DPMRegeditName;
    char *internalRTMName;
    char *externalRTMName;
    char *DNSAddr;
    char *secDNSAddr;

    UINT32 externalRTM;
    InternalRTM *internalRTM;
    INT32 currentState;

    bool DpmMode;
    bool DpmWakeUp;

    UINT32 sntpCount;
    app_dpm_timer_callback dpm_timer_callback;
    exit_sleep_thread_callback exit_dpm_sleep_cb;
} dpmAppThreadInfo;

typedef void *(*persistant_storage_read_cb_t)(property_id_t id);
typedef int   (*persistant_storage_write_cb_t)(property_id_t id, const void *value);

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Retrieves the flag indicating whether the API to enter or exit DPM
 * @param[out] p_flag Pointer to a variable where the current DPM flag value will be stored.
 *
 * @return fsp_err_t   Function execution status.
 *                     - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_get_sleep_flag(UINT8 *p_flag);

/**
 ****************************************************************************************
 * @brief Sets the flag indicating whether the system should enter or exit DPM
 * @param[in] _flag go DPM (1) or exit DPM (0)
 * @return fsp_err_t   Function execution status.
 *                     - FSP_SUCCESS: Operation successful.
 *                     - FSP_ERR_INVALID_ARGUMENT: Invalid flag value.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_sleep_flag(UINT8 _flag);

/**
 ****************************************************************************************
 * @brief Get if exist or allocate if not exist the RTM resource for Server apps
 * @param[in,out] rtm_info Pointer to store the address of the retrieved or newly allocated RTM resource
 * @param[in] _name if null passed, _name will be assigned as predefined name
 * @return NULL if failure, an app_dpm_info_rtm pointer if success
 ****************************************************************************************
 */

fsp_err_t app_dpm_info_get(char *_name, app_dpm_info_rtm** rtm_info);

/**
 ****************************************************************************************
 * @brief Get the saved client' binding port from app_dpm_info_rtm struct
 * @param[out] p_port Pointer to a variable where the retrieved client binding port
 *                    number will be stored.
 * @return The saved client' binding port if DPM mode or NX_SUCCESS if not DPM mode
 ****************************************************************************************
 */
fsp_err_t app_dpm_get_client_socket_port(UINT32 *p_port);

/**
 ****************************************************************************************
 * @brief Set the flag whether device publish to server or not
 * @param[in] _flag 1(increase) or 0(decrease)
 * @return fsp_err_t   Function execution status.
 *                     - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_send_pub_flag(UINT8 _flag);

/**
 ****************************************************************************************
 * @brief Get the flag whether device publish to server or not
 * @param[out] p_flag Pointer to a variable where the publish flag will be stored.
 *                    - 0: Device has not published to the server.
 *                    - 1: Device has published to the server.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
 fsp_err_t app_dpm_get_send_pub_flag(UINT8  *p_flag);

/**
 ****************************************************************************************
 * @brief Set the flag whether device need to wait the flag for next job or not
 * @param[in] _flag 1 or 0
 * @return fsp_err_t   Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_wait_job_next_flag(UINT8 _flag);

/**
 ****************************************************************************************
 * @brief Get the flag whether device need to wait the flag for next job or not
 * @param[out] p_flag Pointer to a variable where the wait flag will be stored.
 *                    - 0: Device does not need to wait for the next job.
 *                    - 1: Device needs to wait for the next job.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_get_wait_job_next_flag(UINT8 *p_flag);

/**
 ****************************************************************************************
 * @brief Get the IP string of server to connect
 * @param[out] p_ipStr Pointer to a variable that will receive the address of the
 *                     server IP string.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful
 ****************************************************************************************
 */
fsp_err_t app_get_peer_ip_str(char** p_ipStr);

/**
 ****************************************************************************************
 * @brief Retrieves a randomly generated local port number for socket binding.
 * @param[out] p_port Pointer to a variable where the generated local port number
 *                    will be stored.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_get_random_local_port(UINT32 *p_port);

/**
 ****************************************************************************************
 * @brief Get the number of reconnection attempts on DPM wakeup mode
 * @param[out] p_count Pointer to a variable where the reconnection attempt count
 *                     will be stored.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_get_count_of_reconnection(UINT32 *p_count);

/**
 ****************************************************************************************
 * @brief Register a binded local port for DPM mode and set port filter for DPM wakeup
 * @param[in] _defLocalPort port number for registration
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_socket_set_port_n_filter(UINT32 _defLocalPort);

/**
 ****************************************************************************************
 * @brief Set the flag depending on whether the persistent session exists or not
 * @param[in] _value 1 or 0
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_set_persistent_session(UINT8 _value);

/**
 ****************************************************************************************
 * @brief  Retrieves the flag indicating whether a persistent session is enabled.
 *  @param[out] p_value Pointer to a variable where the persistent session flag will be stored.
 *                     - 0: Persistent session disabled.
 *                     - 1: Persistent session enabled.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_is_persistent_session(UINT8 *p_value);

/**
 ****************************************************************************************
 * @brief Get the flag whether reconnection happened or not
 * @param[out] p_value Pointer to a variable where the reconnection flag will be stored.
 *                     - 0: No reconnection has occurred.
 *                     - 1: Reconnection has occurred.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_is_reconnected(UINT8 *p_value);

/**
 ****************************************************************************************
 * @brief Set the reconnection flag to the input paramter
 * @param[in] _value true or false
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 *                    - FSP_ERR_INVALID_ARGUMENT: _value is not 0 or 1.
 ****************************************************************************************
 */
fsp_err_t app_set_reconnect_flag(UINT8 _value);

/**
 ****************************************************************************************
 * @brief Get the receive timeout flag
 * @return DPM_RCV_NO_CONNECT, DPM_RCV_OK_CONNECT, or DPM_RCV_OK_SLEEP
 ****************************************************************************************
 */
fsp_err_t app_dpm_get_recv_timeout_flag(UINT8 *p_value);

/**
 ****************************************************************************************
 * @brief Set the receive timeout flag to the input paramter
 * @param[in] _value Receive timeout flag value. Possible values:
 *                   - DPM_RCV_NO_CONNECT: No connection received.
 *                   - DPM_RCV_OK_CONNECT: Connection received successfully.
 *                   - DPM_RCV_OK_SLEEP: Connection received, then system entered sleep.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 *                    - FSP_ERR_INVALID_ARGUMENT: _value is not a valid flag.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_recv_timeout_flag(UINT8 _value);

/**
 ****************************************************************************************
 * @brief Get the unknown UC flag
    *
 * @param[out] p_value Pointer to a variable where the unknown UC flag will be stored.
 *                     - 0: No unknown UC detected.
 *                     - 1: Unknown UC detected.
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_get_unknown_uc_flag(UINT8 *p_value);

/**
 ****************************************************************************************
 * @brief Set the unknown UC flag to the input paramter
 * @param[in] _value 1 or 0
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_unknown_uc_flag(UINT8 _value);

/**
 ****************************************************************************************
 * @brief Set the keepalive time interval for connection peer to the input parmeter by seconds
 * @param[in] _kaVal keepalive time interval by seconds
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_set_ka_value(UINT16 _kaVal);

/**
 ****************************************************************************************
 * @brief Get the keepalive time interval for communication with peer
 * @param[out] p_kaVal  Pointer to a UINT16 variable where the keepalive interval
 *                          (in seconds) will be stored.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_get_ka_value(UINT16 *p_kaVal);

/**
 ****************************************************************************************
 * @brief  Notify the DPM module that the receiver is ready after DPM wakeup.
 * @retval FSP_SUCCESS      Notification sent successfully.
 * @retval FSP_ERR_FAILURE  Failed to notify the DPM module.
 ****************************************************************************************
 */
fsp_err_t app_dpm_set_rcv_ready(void);
/**
 ***************************************************************************************
 * @brief set read callback function to get server IP Address from nvram
 * @param[in] cb callback function
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t persistant_read_callback_set(persistant_storage_read_cb_t cb);
/**
 ***************************************************************************************
 * @brief set write callback function to set server IP Address to nvram
 * @param[in] cb callback function
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t persistant_write_callback_set(persistant_storage_write_cb_t cb);
/**
 ***************************************************************************************
 * @brief Check passed time
 * @param[in] fmt formatted string for debug
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t awsiot_app_print_elapse_time_ms(const char *fmt, ...);
#if defined(__SUPPORT_AWS_IOT_W__) //awsupgradeport[[::for fleet provisioning

/**
 ****************************************************************************************
 * @brief Set the pre-registered thing name
 * @param[in] _pcThingName the pre-registered thing name [0 or -1]
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_set_registered_thing_name(char *_pcThingName);

/**
 ****************************************************************************************
 * @brief Retrieve the flag indicating whether subscription with the server is needed.
 * @param[out] p_flag  Pointer to a UINT8 variable where the subscription flag will be stored. [0 or 1]
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_get_subscription_need_to_server(UINT8 *p_flag);

/**
 ****************************************************************************************
 * @brief Set the flag whether need to subscribe with server or not
 * @param[in] _enable enabled or disabled (1 or 0)
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_set_subscription_need_to_server(UINT8 _enable);

/**
 ****************************************************************************************
 * @brief Get the pointer of main application's thread pointer
 * @param[out] _threadInfo  Pointer to a variable that will receive the address of the 
 *                          main application's thread information structure.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful
 ****************************************************************************************
 */
fsp_err_t app_get_thread_info(dpmAppThreadInfo ** _threadInfo);

/**
 ****************************************************************************************
 * @brief Set the pointer of main application's thread pointer
 * @param[in] _threadInfo  Pointer to the thread information structure to be set.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 ****************************************************************************************
 */
fsp_err_t app_set_thread_info(dpmAppThreadInfo * _threadInfo);

/**
****************************************************************************************
 * @brief Sets the sleep mode and it's factors on sleep mode 1/2/3(DPM)
 * @param [in] _mode The value of one of the APPSleepMode enums
 *
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 *****************************************************************************************
 */
fsp_err_t appSetSleepMode(APPSleepMode _mode);

/**
****************************************************************************************
 * @brief Gets the current sleep mode
 * @param[out] p_mode  Pointer to an APPSleepMode variable where the current sleep mode
 *                     will be stored.
 * @return fsp_err_t  Function execution status.
 *                    - FSP_SUCCESS: Operation successful.
 *****************************************************************************************
 */
fsp_err_t appGetSleepMode(APPSleepMode *p_mode);


#endif /* __SUPPORT_AWS_IOT_W__ */

/******************************************************************************************************************//**
 * @} (end defgroup RM_AWS_LWIP_SOCK_WRAP_W_API)
 *********************************************************************************************************************/

#endif /* IOT_APP_SUPPORT_SOCKET_WRAPPER_INTERFACE_H_ */
