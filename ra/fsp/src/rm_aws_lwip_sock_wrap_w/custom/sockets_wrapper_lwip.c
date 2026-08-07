/* Copyright (c) Microsoft Corporation.
 * Licensed under the MIT License. */

/**
 * @file sockets_wrapper_lwip.c
 * @brief LWIP socket wrapper.
 */

#include "sockets_wrapper.h"

/* Standard includes. */
#include <stdbool.h>
#include <string.h>

/* Lwip includes. */
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "app_aws_user_conf.h"
#include "app_dpm_interface.h"

#include "rm_lwip_w_helper.h"
#include "app_common_support.h"

/*-----------------------------------------------------------*/

/*
 * DNS timeouts.
 */
#ifndef lwipdnsresolverMAX_WAIT_SECONDS
 #define lwipdnsresolverMAX_WAIT_SECONDS    (20)
#endif

#define lwipdnsresolverLOOP_DELAY_MS        (250)
#define lwipdnsresolverLOOP_DELAY_TICKS     ((TickType_t) lwipdnsresolverLOOP_DELAY_MS / portTICK_PERIOD_MS)
#define lwipdnsresolverMAX_WAIT_CYCLES            \
    (((lwipdnsresolverMAX_WAIT_SECONDS) * 1000) / \
     (lwipdnsresolverLOOP_DELAY_MS))

/*
 * convert from system ticks to seconds.
 */
#define TICK_TO_S(_t_)     ((_t_) / configTICK_RATE_HZ)

/*
 * convert from system ticks to micro seconds.
 */
#define TICK_TO_US(_t_)    ((_t_) * 1000 / configTICK_RATE_HZ * 1000)

static uint32_t mRcvTimeOutMs = 0;

const char * Socket_Status_strerror (int _errno)
{
    const char * str = NULL;

    switch (_errno)
    {
        /* errno.h: 11 ~ 16 */
        case EAGAIN:
        {
            str = "Try again";
            break;
        }

        case ENOMEM:
        {
            str = "Out of memory";
            break;
        }

        case EACCES:
        {
            str = "Permission denied";
            break;
        }

        case EFAULT:
        {
            str = "Bad address";
            break;
        }

        case ENOTBLK:
        {
            str = "Block device required";
            break;
        }

        case EBUSY:
        {
            str = "Device or resource busy";
            break;
        }

        case ENETDOWN:
        {
            str = "Network is down";
            break;
        }

        /* errno.h: 101 ~ 121 */
        case ENETUNREACH:
        {
            str = "Cannot reach the network";
            break;
        }

        case ENETRESET:
        {
            str = "Network connection dropped after reset";
            break;
        }

        case ECONNABORTED:
        {
            str = "Connection aborted by software";
            break;
        }

        case ECONNRESET:
        {
            str = "Peer reset the connection";
            break;
        }

        case ENOBUFS:
        {
            str = "Insufficient buffer space";
            break;
        }

        case EISCONN:
        {
            str = "Endpoint is already connected";
            break;
        }

        case ENOTCONN:
        {
            str = "Endpoint is not connected";
            break;
        }

        case ETOOMANYREFS:
        {
            str = "Excessive references, cannot splice";
            break;
        }

        case ETIMEDOUT:
        {
            str = "Connection attempt timed out";
            break;
        }

        case ECONNREFUSED:
        {
            str = "Connection was refused";
            break;
        }

        case EHOSTDOWN:
        {
            str = "Target host is down";
            break;
        }

        case EHOSTUNREACH:
        {
            str = "Host cannot be reached (no route)";
            break;
        }

        case EALREADY:
        {
            str = "Action is already in progress";
            break;
        }

        case EINPROGRESS:
        {
            str = "Action currently in progress";
            break;
        }

        case EREMOTEIO:
        {
            str = "I/O error on remote device";
            break;
        }

        default:
        {
            str = "???";
            break;
        }
    }

    return str;
}

/*-----------------------------------------------------------*/

/*
 * Lwip DNS Found callback, compatible with type "dns_found_callback"
 * declared in lwip/dns.h.
 *
 * NOTE: this resolves only ipv4 addresses; calls to dns_gethostbyname_addrtype()
 * must specify dns_addrtype == LWIP_DNS_ADDRTYPE_IPV4.
 */
static void socket_dns_found_callback (const char * ucName, const ip_addr_t * xIPAddr, void * pvCallbackArg)
{
    uint32_t * ulAddr = (uint32_t *) pvCallbackArg;

    if (xIPAddr != NULL)
    {
        *ulAddr = *((uint32_t *) xIPAddr); /* NOTE: IPv4 addresses only */
    }
    else
    {
        *ulAddr = 0;
    }
}

/*-----------------------------------------------------------*/

uint32_t prvGetHostByName (const char * pcHostName)
{
    uint32_t  ulAddr     = 0;
    err_t     xLwipError = ERR_OK;
    ip_addr_t xLwipIpv4Address;
    uint32_t  ulDnsResolutionWaitCycles = 0;

    if (strlen(pcHostName) <= (size_t) SOCKETS_MAX_HOST_NAME_LENGTH)
    {
        xLwipError = dns_gethostbyname_addrtype(pcHostName,
                                                &xLwipIpv4Address,
                                                socket_dns_found_callback,
                                                (void *) &ulAddr,
                                                LWIP_DNS_ADDRTYPE_IPV4);

        switch (xLwipError)
        {
            case ERR_OK:
            {
                ulAddr = *((uint32_t *) &xLwipIpv4Address); /* NOTE: IPv4 addresses only */
                break;
            }

            case ERR_INPROGRESS:
            {
                /*
                 * The DNS resolver is working the request.  Wait for it to complete
                 * or time out; print a timeout error message if configured for debug
                 * printing.
                 */
                do
                {
                    vTaskDelay(lwipdnsresolverLOOP_DELAY_TICKS);
                }   while ((ulDnsResolutionWaitCycles++ < lwipdnsresolverMAX_WAIT_CYCLES) && ulAddr == 0);

                if (ulAddr == 0)
                {
                    IOT_ERROR("Unable to resolve (%s) within (%d) seconds", pcHostName,
                              lwipdnsresolverMAX_WAIT_SECONDS);
                }

                break;
            }

            default:
            {
                IOT_ERROR("Unexpected error (%lu) from dns_gethostbyname_addrtype() while resolving (%s)!",
                          (uint32_t) xLwipError,
                          pcHostName);
                break;
            }
        }
    }
    else
    {
        ulAddr = 0;
        IOT_ERROR("Host name (%s) too long!", pcHostName);
    }

    return ulAddr;
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_Init ()
{
    return SOCKETS_ERROR_NONE;
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_DeInit ()
{
    return SOCKETS_ERROR_NONE;
}

/*-----------------------------------------------------------*/

SocketHandle Sockets_Open ()
{
    int32_t      ulSocketNumber = socket(AF_INET, SOCK_STREAM, IP_PROTO_TCP);
    SocketHandle xSocket;

    if (ulSocketNumber < 0)
    {
        xSocket = (SocketHandle) SOCKETS_INVALID_SOCKET;
    }
    else
    {
        xSocket = (SocketHandle) ulSocketNumber;
    }

// awsupgradeport[[::awsdpmwork::reset recv timeout to zero
    UINT8 flagRcvTimeout;
    UINT8 isReconnected;

    app_is_reconnected(&isReconnected);
    app_dpm_get_recv_timeout_flag(&flagRcvTimeout);

    if (isReconnected && (flagRcvTimeout == DPM_RCV_NO_CONNECT))
    {
        mRcvTimeOutMs = 0;
    }

// ]]

    return xSocket;
}

SocketHandle Sockets_Dpm_Open (void)
{
    SocketHandle       xSocket;
    int32_t            ulSocketNumber;
    dpmAppThreadInfo * _threadInfo = NULL;
    UINT8              flagRcvTimeout;
    UINT8              isReconnected;

    // get AWS app thread information pointer
    app_get_thread_info(&_threadInfo);

    if (_threadInfo)
    {
        ulSocketNumber = socket_dpm(_threadInfo->DPMRegeditName, AF_INET, SOCK_STREAM, IP_PROTO_TCP);
    }
    else                               // if not thread info, use the reserved name
    {
        ulSocketNumber = socket_dpm(AWS_TCP_SOCK_NAME, AF_INET, SOCK_STREAM, IP_PROTO_TCP);
    }

    if (ulSocketNumber < 0)
    {
        IOT_ERROR("failed to open socket_dpm()");
        xSocket = (SocketHandle) SOCKETS_INVALID_SOCKET;
    }
    else
    {
        xSocket = (SocketHandle) ulSocketNumber;
    }

    app_dpm_get_recv_timeout_flag(&flagRcvTimeout);
    app_is_reconnected(&isReconnected);
    if (isReconnected && (flagRcvTimeout == DPM_RCV_NO_CONNECT))
    {
        mRcvTimeOutMs = 0;
    }

    return xSocket;
}

BaseType_t Sockets_Dpm_Bind (SocketHandle xSocket, uint16_t usPort)
{
    uint32_t           ulSocketNumber = (uint32_t) xSocket;
    int32_t            lRetVal        = SOCKETS_ERROR_NONE;
    UINT32             my_port        = usPort;
    struct sockaddr_in local_addr     = {0, };

#if CFG_PMGR
    if (!RM_PMGR_W_dpm_is_enabled())
    {
        return lRetVal;
    }

#else

    return lRetVal;
#endif

    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port        = htons((uint16_t) my_port);

    if (app_get_registered_thing_name() != NULL)
    {
        if (bind((int32_t) ulSocketNumber, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0)
        {
            IOT_ERROR("fail to bind(errno=%d:\"%s\")", errno, Socket_Status_strerror(errno));
            lRetVal = SOCKETS_SOCKET_ERROR;
        }
        else
        {
            IOT_INFO("[%s:%d] local my_port = %u binded", __func__, __LINE__, my_port);
        }
    }
    else
    {
        awsiot_app_print_elapse_time_ms("[%s:%d] bypass bind due to non-registered thing", __func__, __LINE__);
    }

    return lRetVal;
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_Close (SocketHandle xSocket)
{
    IOT_INFO("[%s:%d]", __func__, __LINE__)
    BaseType_t rc;

// awsupgradeport[[::awsdpmwork:: set reconnection flag & set timeout with initial value
    app_set_reconnect_flag(true);
    app_dpm_set_recv_timeout_flag(DPM_RCV_NO_CONNECT);

// ]]

    rc = close((int32_t) xSocket);
    shutdown((int32_t) xSocket, SHUT_RDWR);

    // return ( BaseType_t ) close( ( uint32_t ) xSocket );
    return rc;
}

/*-----------------------------------------------------------*/
BaseType_t Sockets_Connect (SocketHandle xSocket, const char * pcHostName, uint16_t usPort)
{
    uint32_t           ulSocketNumber = (uint32_t) xSocket;
    int32_t            lRetVal        = SOCKETS_ERROR_NONE;
    struct sockaddr_in xSockAddr      = {0};
    char             * ip_str         = NULL;
    UINT8              isReconnected;
    APPSleepMode       sleepMode;

    xSockAddr.sin_family = AF_INET;
    xSockAddr.sin_port   = htons(usPort);
    app_is_reconnected(&isReconnected);
    awsiot_app_print_elapse_time_ms("[%s:%d] dns query start...", __func__, __LINE__);
    appGetSleepMode(&sleepMode);

    if (sleepMode == SLEEP_MODE_2)
    {
        if (SLEEP_2_USE_RTM != 0)
        {
            ip_str = app_common_get_ip_from_fast_dns_with_rtm((char *) pcHostName, AWS_RTM_NAME, isReconnected);
        }
        else
        {
            ip_str = app_common_get_ip_from_fast_dns_with_nvram((char *) pcHostName, isReconnected);
        }
    }
    else
    {
        ip_str = app_common_get_ip_from_fast_dns_with_rtm((char *) pcHostName, AWS_RTM_NAME, isReconnected);
    }

    if (ip_str != NULL)
    {
        if (isvalidip(ip_str))
        {
            xSockAddr.sin_addr.s_addr = inet_addr(ip_str);
            awsiot_app_print_elapse_time_ms("[%s:%d] got the server ip(\"%s\")", __func__, __LINE__, ip_str);
            app_set_peer_ip_str(ip_str);
        }
        else
        {
            goto DNS_QUERY_FAILED;
        }
    }
    else
    {
        goto DNS_QUERY_FAILED;
    }

    if (connect((int32_t) ulSocketNumber, (struct sockaddr *) &xSockAddr, sizeof(xSockAddr)) < 0)
    {
        lRetVal = SOCKETS_SOCKET_ERROR;
    }

    goto EXIT_FUNC;

DNS_QUERY_FAILED:
    IOT_ERROR("DNS query failed...");
    lRetVal = SOCKETS_SOCKET_ERROR;
    app_set_peer_ip_str(NULL);

EXIT_FUNC:
    if (ip_str)
    {
        free(ip_str);
    }

    return lRetVal;
}

/*-----------------------------------------------------------*/

void Sockets_Disconnect (SocketHandle xSocket)
{
    IOT_INFO("[%s:%d]", __func__, __LINE__)

// awsupgradeport[[::awsdpmwork:: set reconnection flag & set timeout with initial value
    app_set_reconnect_flag(true);
    app_dpm_set_recv_timeout_flag(DPM_RCV_NO_CONNECT);

// ]]
    close((int32_t) xSocket);
    shutdown((int32_t) xSocket, SHUT_RDWR);
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_Recv (SocketHandle xSocket, uint8_t * pucReceiveBuffer, size_t xReceiveBufferLength)
{
    uint32_t   ulSocketNumber = (uint32_t) xSocket;
    uint32_t   time_out       = mRcvTimeOutMs;
    TickType_t xRecvTimeout;
#if (1 == AWS_IOT_DPM_APP_ENABLE)
    int remained_data = -1;
#endif
    dpmAppThreadInfo * _threadInfo = NULL;
    UINT8              isReconnected;

    app_get_thread_info(&_threadInfo);
    UINT8 flagRcvTimeout;

    app_dpm_get_recv_timeout_flag(&flagRcvTimeout);
    if (flagRcvTimeout)
    {
#if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            if (flagRcvTimeout == DPM_RCV_OK_CONNECT)
            {
                time_out = AWS_REV_OK_CONNECT_TIMEOUT;
            }
            else
            {
                time_out = AWS_REV_SLEEP_TIMEOUT;
            }
        }
        else                           // for subscription's response in no dpm mode regardless UPSS feature
#endif
        {
            time_out = 400;
        }
    }

    app_is_reconnected(&isReconnected);

    // connection is the highest priority
    if (isReconnected)
    {
        time_out = AWS_REV_NO_CONNECT_TIMEOUT;
    }

    xRecvTimeout = pdMS_TO_TICKS(time_out);
    (void) Sockets_SetSockOpt(xSocket, SOCKETS_SO_RCVTIMEO, &xRecvTimeout, sizeof(xRecvTimeout));

    int lRetVal = recv((int32_t) ulSocketNumber, pucReceiveBuffer, xReceiveBufferLength, 0);

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        if (RM_PMGR_W_dpm_is_wakeup() && ((enum DPM_WAKEUP_TYPE) RM_PMGR_W_dpm_wakeup_type_get(0) == DPM_PACKET_WAKEUP))
        {
            static int firstFlag = 0;
            if (firstFlag == 0)
            {
                awsiot_app_print_elapse_time_ms("[%s:%d][UC] lRetVal = %d, errno = %u:\"%s\"",
                                                __func__,
                                                __LINE__,
                                                lRetVal,
                                                errno,
                                                Socket_Status_strerror(errno));
                firstFlag = 1;
            }
        }

        if ((flagRcvTimeout != DPM_RCV_OK_SLEEP) || (xReceiveBufferLength > 0))
        {
            // RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        }
    }
#endif

    if (lRetVal == SOCKETS_SOCKET_ERROR)
    {
        /*
         * 1. EWOULDBLOCK if the socket is NON-blocking, but there is no data
         *    when recv is called.
         * 2. EAGAIN if the socket would block and have waited long enough but
         *    packet is not received.
         */
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
        {
#if (0 == AWS_IOT_DPM_APP_ENABLE)

            return SOCKETS_ERROR_NONE;     /* timeout or would block */
#else
            if (flagRcvTimeout != DPM_RCV_OK_SLEEP)
            {
                return SOCKETS_ERROR_NONE; /* timeout or would block */
            }
            else
            {
                goto CHK_DPM_ENTER;
            }
#endif
        }

        IOT_ERROR("recv(fd=%lu): NG -> errno = %u:\"%s\"", ulSocketNumber, errno, Socket_Status_strerror(errno));

        /*
         * socket is not connected.
         */
        if (errno == EBADF)
        {
            return SOCKETS_ECLOSED;
        }
    }

    if ((lRetVal == 0) && (errno == ENOTCONN))
    {
        IOT_ERROR("recv(fd=%lu): NG -> errno = %u:\"%s\"", ulSocketNumber, errno, Socket_Status_strerror(errno));
        lRetVal = SOCKETS_ECLOSED;
    }

// awsupgradeport[[::awsdpmwork:: check whether app goto sleep or not
    if ((lRetVal > 0) ||
        ((lRetVal <= 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)))
    {
#if (1 == AWS_IOT_DPM_APP_ENABLE)
        if (flagRcvTimeout == DPM_RCV_OK_SLEEP)
        {
            IOT_INFO("clear dpm sleep due to recv(len=%d, errno=%d:\"%s\")", lRetVal, errno,
                     Socket_Status_strerror(errno))
            awsiot_app_print_elapse_time_ms("[%s:%d] clear dpm sleep due to recv(len=%d, errno=%d)",
                                            __func__,
                                            __LINE__,
                                            lRetVal,
                                            errno);
        }

        if (_threadInfo != NULL)
        {
            _threadInfo->exit_dpm_sleep_cb();
        }
#endif
    }

    if ((lRetVal <= 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)) && (flagRcvTimeout == DPM_RCV_OK_SLEEP))
    {
#if (1 == AWS_IOT_DPM_APP_ENABLE)
CHK_DPM_ENTER:
 #if CFG_PMGR
        if (RM_PMGR_W_dpm_is_enabled())
        {
            remained_data = RM_PMGR_W_socket_rx_data_is_remaining(ulSocketNumber);;

            if (remained_data == 0)
            {
                if (_threadInfo != NULL)
                {
                    _threadInfo->dpm_timer_callback(1, DPM_RTC_NORMAL_MODE);
                }
            }
        }

        lRetVal = SOCKETS_ERROR_NONE;
 #else
        lRetVal = SOCKETS_ERROR_NONE;
 #endif
#endif
    }

// ]]

    return (BaseType_t) lRetVal;
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_Send (SocketHandle xSocket, const uint8_t * pucData, size_t xDataLength)
{
    BaseType_t rcode;
    rcode = (BaseType_t) send((int32_t) xSocket, pucData, xDataLength, 0);
    if (rcode < 0)
    {
        IOT_ERROR("pBuf=0x%p, len=%u, rcode=%lu, errno=%u:\"%s\"",
                  pucData,
                  xDataLength,
                  rcode,
                  errno,
                  Socket_Status_strerror(errno));
    }

    return rcode;
}

/*-----------------------------------------------------------*/

BaseType_t Sockets_SetSockOpt (SocketHandle xSocket,
                               int32_t      lOptionName,
                               const void * pvOptionValue,
                               size_t       xOptionLength)
{
    uint32_t   ulSocketNumber = (uint32_t) xSocket;
    BaseType_t xRetVal        = SOCKETS_ERROR_NONE;
    int        ulRet          = 0;

    switch (lOptionName)
    {
        case SOCKETS_SO_RCVTIMEO:
        case SOCKETS_SO_SNDTIMEO:
        {
            TickType_t     xTicks;
            struct timeval xTV;

            xTicks = *((const TickType_t *) pvOptionValue);

            xTV.tv_sec  = (long) TICK_TO_S(xTicks);
            xTV.tv_usec = (long) TICK_TO_US(xTicks % configTICK_RATE_HZ);

// awsupgradeport[[::awsdpmwork::
            if (lOptionName == SOCKETS_SO_RCVTIMEO)
            {
                uint32_t passedTimeOutMs = (uint32_t) (xTV.tv_usec / 1000 + xTV.tv_sec * 1000);
                if (mRcvTimeOutMs != passedTimeOutMs)
                {
                    ulRet = setsockopt((int32_t) ulSocketNumber,
                                       SOL_SOCKET,
                                       SO_RCVTIMEO,
                                       (struct timeval *) &xTV,
                                       sizeof(xTV));
                    if (ulRet != 0)
                    {
                        IOT_ERROR("Failed to set socket option - SO_RCVTIMEO(%d:%d:\"%s\")",
                                  ulRet,
                                  errno,
                                  Socket_Status_strerror(errno));
                        xRetVal = SOCKETS_EINVAL;
                    }
                    else
                    {
                        IOT_INFO("recv timeout(=%u ms) set OK (socket=%lu)", (int) passedTimeOutMs, ulSocketNumber)
                        mRcvTimeOutMs = passedTimeOutMs;
                        xRetVal       = SOCKETS_ERROR_NONE;
                    }
                }
            }
            else
            {
                IOT_WARN("SOCKETS_SO_SNDTIMEO not supported");
                xRetVal = SOCKETS_ERROR_NONE;
            }

// ]]
            break;
        }

        default:
        {
            xRetVal = SOCKETS_ENOPROTOOPT;
            break;
        }
    }

    return xRetVal;
}

/*-----------------------------------------------------------*/
