/*
 * FreeRTOS V202411.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#include <stdlib.h>
#include <string.h>

/* Referring Feature for AWS-IOT-W */
#include "rm_awsiot_w_cfg.h"
#include "FreeRTOSConfig.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "semphr.h"

/* mbed TLS includes. */
#if defined( MBEDTLS_CONFIG_FILE )
    #include MBEDTLS_CONFIG_FILE
#else
    #include "mbedtls/config.h"
#endif
#include "mbedtls/entropy.h"

#include "entropy_poll.h"

#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
#include "mbedtls_freertos_port.h"
#else
//awsupgradeport[[::
#include "mbedtls_freertos_port_alt.h"
#include "threading_alt.h"
#include "lwip/sockets.h"
#include "sockets_wrapper.h"
#endif//]]
/*-----------------------------------------------------------*/

/**
 * @brief Allocates memory for an array of members.
 *
 * @param[in] nmemb Number of members that need to be allocated.
 * @param[in] size Size of each member.
 *
 * @return Pointer to the beginning of newly allocated memory.
 */
void * mbedtls_platform_calloc( size_t nmemb,
                                size_t size )
{
    size_t totalSize = nmemb * size;
    void * pBuffer = NULL;

    /* Check that neither nmemb nor size were 0. */
    if( totalSize > 0 )
    {
        /* Overflow check. */
        if( ( totalSize / size ) == nmemb )
        {
            pBuffer = pvPortMalloc( totalSize );

            if( pBuffer != NULL )
            {
                ( void ) memset( pBuffer, 0U, totalSize );
            }
        }
    }

    return pBuffer;
}

/*-----------------------------------------------------------*/

/**
 * @brief Frees the space previously allocated by calloc.
 *
 * @param[in] ptr Pointer to the memory to be freed.
 */
void mbedtls_platform_free( void * ptr )
{
    if( ptr != NULL )
    {
        vPortFree( ptr );
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief Sends data over FreeRTOS+TCP sockets.
 *
 * @param[in] ctx The network context containing the socket handle.
 * @param[in] buf Buffer containing the bytes to send.
 * @param[in] len Number of bytes to send from the buffer.
 *
 * @return Number of bytes sent on success; else a negative value.
 */
int mbedtls_platform_send( void * ctx,
                           const unsigned char * buf,
                           size_t len )
{
    SocketHandle socket;

    configASSERT( buf != NULL );

    socket = ( SocketHandle ) ctx;

#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
    return ( int ) Sockets_Send( socket, buf, len );
#else//awsupgradeport[[::
	int rc;
	rc = ( int ) Sockets_Send( socket, buf, len );
	if (rc < 0)
	{
/*
		int sock_error;
		int len = sizeof(sock_error);

        getsockopt((uint32_t)socket, 0xfff, 0x1007, &sock_error, (socklen_t *)&len);
		PRINTF("\n[%s:%d] errno = %d:%s\n", __func__, __LINE__, sock_error, Socket_Status_strerror(sock_error));
*/
		PRINTF("\n[%s:%d] rc = %d, errno = %d:%s\n", __func__, __LINE__, rc, errno, Socket_Status_strerror(errno));
	}

	return rc;
#endif//]]
}
/*-----------------------------------------------------------*/

/**
 * @brief Receives data from FreeRTOS+TCP socket.
 *
 * @param[in] ctx The network context containing the socket handle.
 * @param[out] buf Buffer to receive bytes into.
 * @param[in] len Number of bytes to receive from the network.
 *
 * @return Number of bytes received if successful; Negative value on error.
 */
int mbedtls_platform_recv( void * ctx,
                           unsigned char * buf,
                           size_t len )
{
    SocketHandle socket;

    configASSERT( buf != NULL );

    socket = ( SocketHandle ) ctx;

#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
    return ( int ) Sockets_Recv( socket, buf, len );
#else//awsupgradeport[[::
	int rc;
	rc = ( int ) Sockets_Recv( socket, buf, len );
	if (rc < 0)
	{
/*
		int sock_error;
		int len = sizeof(sock_error);

        getsockopt((uint32_t)socket, 0xfff, 0x1007, &sock_error, (socklen_t *)&len);
		PRINTF("\n[%s:%d] errno = %d:%s\n", __func__, __LINE__, sock_error, Socket_Status_strerror(sock_error));
*/
	PRINTF("\n[%s:%d] rc = %d, errno = %d:%s\n", __func__, __LINE__, rc, errno, Socket_Status_strerror(errno));
	}

	return rc;
#endif//]]
}

/*-----------------------------------------------------------*/
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[:: 
#if defined( MBEDTLS_THREADING_C )

/**
 * @brief Creates a mutex.
 *
 * @param[in, out] pMutex mbedtls mutex handle.
 */
    static void mbedtls_platform_mutex_init( mbedtls_threading_mutex_t * pMutex )
    {
        configASSERT( pMutex != NULL );
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
        #if ( configSUPPORT_STATIC_ALLOCATION == 1 )

            /* Create a statically-allocated FreeRTOS mutex. This should never fail as
             * storage is provided. */

            pMutex->mutexHandle = xSemaphoreCreateMutexStatic( &( pMutex->mutexStorage ) );
        #elif ( configSUPPORT_DYNAMIC_ALLOCATION == 1 )
            pMutex->mutexHandle = xSemaphoreCreateMutex();
        #endif

        configASSERT( pMutex->mutexHandle != NULL );
#else//awsupgradeport[[::
        pMutex->mutex = (SemaphoreHandle_t)xSemaphoreCreateMutex();
        configASSERT( pMutex->mutex != NULL );
#endif//]]
    }

/*-----------------------------------------------------------*/

/**
 * @brief Frees a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @note This function is an empty stub as nothing needs to be done to free
 * a statically allocated FreeRTOS mutex.
 */
    static void mbedtls_platform_mutex_free( mbedtls_threading_mutex_t * pMutex )
    {
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
        vSemaphoreDelete( pMutex->mutexHandle );
        pMutex->mutexHandle = NULL;
#else//awsupgradeport[[::
        if (pMutex->mutex)
	{
		vSemaphoreDelete(pMutex->mutex);
		pMutex->mutex = NULL;  // clear handle so a repeated free is a safe no-op (prevents double vSemaphoreDelete)
	}
#endif//]]
    }

/*-----------------------------------------------------------*/

/**
 * @brief Function to lock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 (success) is always returned as any other failure is asserted.
 */
    static int mbedtls_platform_mutex_lock( mbedtls_threading_mutex_t * pMutex )
    {
        BaseType_t mutexStatus = 0;

        configASSERT( pMutex != NULL );
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
        configASSERT( pMutex->mutexHandle != NULL );
#else//awsupgradeport[[::
        configASSERT( pMutex->mutex != NULL );
#endif//]]

        /* mutexStatus is not used if asserts are disabled. */
        ( void ) mutexStatus;

        /* This function should never fail if the mutex is initialized. */
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::    
        mutexStatus = xSemaphoreTake( pMutex->mutexHandle, portMAX_DELAY );
#else//awsupgradeport[[::
        mutexStatus = xSemaphoreTake( pMutex->mutex, portMAX_DELAY );
#endif//]]
        configASSERT( mutexStatus == pdTRUE );

        return 0;
    }

/*-----------------------------------------------------------*/

/**
 * @brief Function to unlock a mutex.
 *
 * @param[in] pMutex mbedtls mutex handle.
 *
 * @return 0 is always returned as any other failure is asserted.
 */
    static int mbedtls_platform_mutex_unlock( mbedtls_threading_mutex_t * pMutex )
    {
        BaseType_t mutexStatus = 0;

        configASSERT( pMutex != NULL );
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::
        configASSERT( pMutex->mutexHandle != NULL );
#else//awsupgradeport[[::
        configASSERT( pMutex->mutex != NULL );
#endif//]]
        /* mutexStatus is not used if asserts are disabled. */
        ( void ) mutexStatus;

        /* This function should never fail if the mutex is initialized. */
#if !defined(__SUPPORT_AWS_IOT_W__)//orig SDK[[::    
        mutexStatus = xSemaphoreGive( pMutex->mutexHandle );
#else//awsupgradeport[[::
        mutexStatus = xSemaphoreGive( pMutex->mutex );
#endif//]]
        configASSERT( mutexStatus == pdTRUE );

        return 0;
    }

/*-----------------------------------------------------------*/
    #if defined( MBEDTLS_THREADING_ALT )
        int mbedtls_platform_threading_init( void )
        {
            mbedtls_threading_set_alt( mbedtls_platform_mutex_init,
                                       mbedtls_platform_mutex_free,
                                       mbedtls_platform_mutex_lock,
                                       mbedtls_platform_mutex_unlock );
            return 0;
        }

    #else /* !MBEDTLS_THREADING_ALT */

        void (* mbedtls_mutex_init)( mbedtls_threading_mutex_t * mutex ) = mbedtls_platform_mutex_init;
        void (* mbedtls_mutex_free)( mbedtls_threading_mutex_t * mutex ) = mbedtls_platform_mutex_free;
        int (* mbedtls_mutex_lock)( mbedtls_threading_mutex_t * mutex ) = mbedtls_platform_mutex_lock;
        int (* mbedtls_mutex_unlock)( mbedtls_threading_mutex_t * mutex ) = mbedtls_platform_mutex_unlock;

    #endif /* !MBEDTLS_THREADING_ALT */
#endif /* MBEDTLS_THREADING_C */
#endif//]]
/*-----------------------------------------------------------*/

#if defined( MBEDTLS_ENTROPY_HARDWARE_ALT )
    /* Determine which API is available */
    #if defined( _WIN32 )
        #define RNG_SOURCE_WINDOWS_CRYPT
    #elif defined( __linux__ )
        #include <unistd.h>
        #include <sys/syscall.h>
        #if defined( SYS_getrandom )
            #define RNG_SOURCE_GETRANDOM
        #endif /* SYS_getrandom */
    #elif defined( ARM_RDI_MONITOR ) || defined( SEMIHOSTING )
        #define RNG_SOURCE_SEMIHOST
    #else
        #define RNG_SOURCE_DEV_RANDOM
    #endif /* if defined( _WIN32 ) */

    #if defined( RNG_SOURCE_WINDOWS_CRYPT )
        #include <windows.h>
        #include <wincrypt.h>
        int mbedtls_hardware_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen )
        {
            int lStatus = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            HCRYPTPROV hProv = 0;

            /* Unreferenced parameter. */
            ( void ) data;

            /*
             * This is port-specific for the Windows simulator, so just use Crypto API.
             */

            if( TRUE == CryptAcquireContextA(
                    &hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )
            {
                if( TRUE == CryptGenRandom( hProv, len, output ) )
                {
                    lStatus = 0;
                    *olen = len;
                }

                CryptReleaseContext( hProv, 0 );
            }

            return lStatus;
        }
    #elif defined( RNG_SOURCE_GETRANDOM )
        int mbedtls_hardware_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen )
        {
            ( void ) data;
            int rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;

            configASSERT( olen != NULL );

            rslt = getrandom( output, len, 0 );

            if( rslt >= 0 )
            {
                *olen = ( size_t ) rslt;
                rslt = 0;
            }
            else
            {
                rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            }

            return rslt;
        }
    #elif defined( RNG_SOURCE_SEMIHOST )
        int mbedtls_hardware_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen )
        {
            int rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            int file;

            ( void ) data;

            configASSERT( olen != NULL );
            configASSERT( output != NULL );

            file = _open( "/dev/urandom", O_RDONLY );

            if( file >= 0 )
            {
                rslt = _read( file, ( char * ) output, len );
            }

            if( rslt >= 0 )
            {
                *olen = len;
            }

            if( rslt >= 0 )
            {
                *olen = len;
                rslt = 0;
            }
            else
            {
                rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            }

            ( void ) _close( file );
            return rslt;
        }
    #elif defined( RNG_SOURCE_WINDOWS_CRYPT )
        #include <stdio.h>
        int mbedtls_hardware_poll( void * data,
                                   unsigned char * output,
                                   size_t len,
                                   size_t * olen )
        {
            int rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            FILE * file;
            size_t read_length = 0U;

            configASSERT( olen != NULL );
            configASSERT( output != NULL );

            file = fopen( "/dev/urandom", "rb" );

            if( file != NULL )
            {
                rslt = fread( output, 1, len, file );
                fclose( file );
            }

            if( rslt >= 0 )
            {
                *olen = len;
                rslt = 0;
            }
            else
            {
                rslt = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            }

            return rslt;
        }
    #else  /* if defined( RNG_SOURCE_WINDOWS_CRYPT ) */
        /*renesas*/
    #endif /* if defined( RNG_SOURCE_WINDOWS_CRYPT ) */
#endif /* if defined( MBEDTLS_ENTROPY_HARDWARE_ALT ) */
/*-----------------------------------------------------------*/
