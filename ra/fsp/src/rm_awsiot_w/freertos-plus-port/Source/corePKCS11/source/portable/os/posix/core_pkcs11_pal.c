/*
 * corePKCS11 v3.6.2
 * Copyright (C) 2024 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
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
 */

/**
 * @file core_pkcs11_pal.c
 * @brief Linux file save and read implementation
 * for PKCS #11 based on mbedTLS with for software keys. This
 * file deviates from the FreeRTOS style standard for some function names and
 * data types in order to maintain compliance with the PKCS #11 standard.
 */
/*-----------------------------------------------------------*/

/* PKCS 11 includes. */
#include "rm_awsiot_w_cfg.h"
#include "core_pkcs11_config.h"
#include "core_pkcs11_config_defaults.h"
#include "core_pkcs11.h"
#include "core_pkcs11_pal_utils.h"

/* C runtime includes. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__USE_AWS_TLS_ALT__) //awsupgradework[[::
#include "app_aws_user_conf.h"
#include "util_api.h"
#include "rm_cert.h"

#define malloc  	pvPortMalloc
#define free    	vPortFree

extern uint8_t *pGenPublicKey;
extern size_t lenGenPublicKey;
#endif //]]
/*-----------------------------------------------------------*/

/**
 * @brief Checks to see if a file exists
 *
 * @param[in] pcFileName         The name of the file to check for existence.
 *
 * @returns CKR_OK if the file exists, CKR_OBJECT_HANDLE_INVALID if not.
 */
static CK_RV prvFileExists( const char * pcFileName )
{
#if !defined(__USE_AWS_TLS_ALT__)//orig SDK[[
    FILE * pxFile = NULL;
    CK_RV xReturn = CKR_OK;

    /* fopen returns NULL if the file does not exist. */
    pxFile = fopen( pcFileName, "r" );

    if( pxFile == NULL )
    {
        xReturn = CKR_OBJECT_HANDLE_INVALID;
        LogDebug( ( "File %s does not exist or could not opened for reading.", pcFileName ) );
    }
    else
    {
        ( void ) fclose( pxFile );
        LogDebug( ( "Found file %s and was able to open it for reading.", pcFileName ) );
    }
#else //awsupgradeport[[::file system not supported
    CK_RV xReturn = CKR_OK;
    LogWarn(("file system not supported"));
#endif //]]

    return xReturn;
}

/**
 * @brief Reads object value from file system.
 *
 * @param[in] pcLabel            The PKCS #11 label to convert to a file name
 * @param[out] pcFileName        The name of the file to check for existence.
 * @param[out] pHandle           The type of the PKCS #11 object.
 *
 */
static CK_RV prvReadData( const char * pcFileName,
                          CK_BYTE_PTR * ppucData,
                          CK_ULONG_PTR pulDataSize )
{
    CK_RV xReturn = CKR_OK;
    unsigned char *buf = NULL;
    size_t buflen = CERT_MAX_LENGTH;

#if !defined(__USE_AWS_TLS_ALT__) //orig SDK[[::
    FILE * pxFile = NULL;
    size_t lSize = 0;

    pxFile = fopen( pcFileName, "r" );

    if( NULL == pxFile )
    {
        LogError( ( "PKCS #11 PAL failed to get object value. "
                    "Could not open file named %s for reading.", pcFileName ) );
        xReturn = CKR_FUNCTION_FAILED;
    }
    else
    {
        ( void ) fseek( pxFile, 0, SEEK_END );
        lSize = ftell( pxFile );
        ( void ) fseek( pxFile, 0, SEEK_SET );

        if( lSize > 0UL )
        {
            *pulDataSize = lSize;
            *ppucData = malloc( *pulDataSize );

            if( NULL == *ppucData )
            {
                LogError( ( "Could not get object value. Malloc failed to allocate memory." ) );
                xReturn = CKR_HOST_MEMORY;
            }
        }
        else
        {
            LogError( ( "Could not get object value. Failed to determine object size." ) );
            xReturn = CKR_FUNCTION_FAILED;
        }
    }

    if( CKR_OK == xReturn )
    {
        lSize = 0;
        lSize = fread( *ppucData, sizeof( uint8_t ), *pulDataSize, pxFile );

        if( lSize != *pulDataSize )
        {
            LogError( ( "PKCS #11 PAL Failed to get object value. Expected to read %ld "
                        "from %s but received %ld", *pulDataSize, pcFileName, lSize ) );
            xReturn = CKR_FUNCTION_FAILED;
        }
    }

    if( NULL != pxFile )
    {
        ( void ) fclose( pxFile );
    }
#else //awsupgradeport[[::file system not supported
	CK_OBJECT_HANDLE xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;

	PAL_UTILS_FilenameToHandle(pcFileName, &xHandle);

	switch (xHandle)
	{
		case eAwsDevicePrivateKey:

			*ppucData = malloc( buflen );
			if (*ppucData)
			{
			    memset(*ppucData, 0, buflen);
			    buf = malloc( buflen );
			    if (buf)
			    {
			    	memset(buf, 0, buflen);
			        RM_CERT_Read(RM_CERT_GetModule(SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR),
					     RM_CERT_TYPE_UNIQUE_PRIV_KEY, RM_CERT_FORMAT_DER, buf, &buflen);
			        memcpy (*ppucData, buf, buflen);
			        *pulDataSize = buflen;
				free(buf);
			    }
			    else
			    {
				    LogError( ("malloc failed") );
			    }
			}
			else
			{
				LogError( ("malloc failed") );
			}
		    break;

		case eAwsDeviceCertificate:

		*ppucData = malloc( buflen );
		if (*ppucData)
		{
		    memset(*ppucData, 0, buflen);
		    buf = malloc( buflen );
		    if (buf)
		    {
		    	memset(buf, 0, buflen);
		        RM_CERT_Read(RM_CERT_GetModule(SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR),
				     RM_CERT_TYPE_UNIQUE_CERT, RM_CERT_FORMAT_DER, buf, &buflen);
			memcpy (*ppucData, buf, buflen);
			*pulDataSize = buflen;
			free(buf);
		    }
		    else
		    {
		        LogError( ("malloc failed") );
		    }
		}
		else
		{
			 LogError( ("malloc failed") );
		}
		break;

		case eAwsDevicePublicKey:
			if (pGenPublicKey && lenGenPublicKey > 0)
			{
				*pulDataSize = lenGenPublicKey;
			}
			if (*pulDataSize > 0)
			{
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsDevicePublicKey: *pulDataSize = %d", __func__, __LINE__, *pulDataSize);
				*ppucData = malloc( *pulDataSize);
				if (*ppucData)
				{
					memcpy (*ppucData, pGenPublicKey, *pulDataSize);
				}
				else
				{
					xReturn = CKR_HOST_MEMORY;
					LogError( ("malloc failed") );
				}
			}
			else
			{
	            LogError( ( "Could not get object value. Failed to determine object size." ) );
	            xReturn = CKR_FUNCTION_FAILED;
			}
		break;

		case eAwsClaimPrivateKey:
#if !defined(__SUPPORT_AWS_IOT_W__) //orig SDK[[::
			*pulDataSize = sizeof(democonfigCLIENT_PRIVATE_KEY_PEM);
#else //awsupgradeport[[::
			*pulDataSize = sizeof(AWS_IOT_CLIENT_PRIV_KEY);
#endif //]]
			if (*pulDataSize > 0)
			{
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsClaimPrivateKey: *pulDataSize = %d", __func__, __LINE__, *pulDataSize);
				*ppucData = malloc( *pulDataSize );
				if (*ppucData)
				{
#if !defined(__SUPPORT_AWS_IOT_W__) //orig SDK[[::
					memcpy (*ppucData, democonfigCLIENT_PRIVATE_KEY_PEM, *pulDataSize);
#else //awsupgradeport[[::
					memcpy (*ppucData, AWS_IOT_CLIENT_PRIV_KEY, *pulDataSize);
#endif //]]
				}
				else
				{
					xReturn = CKR_HOST_MEMORY;
					LogError( ("malloc failed") );
				}
			}
			else
			{
				LogError( ( "Could not get object value. Failed to determine object size." ) );
				xReturn = CKR_FUNCTION_FAILED;
			}
		break;
		
		case eAwsClaimCertificate:
#if !defined(__SUPPORT_AWS_IOT_W__) //orig SDK[[::
			*pulDataSize = sizeof(democonfigCLIENT_CERTIFICATE_PEM);
#else //awsupgradeport[[::
			*pulDataSize = sizeof(AWS_IOT_CLIENT_CERT);
#endif //]]
			if (*pulDataSize > 0)
			{
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsClaimCertificate: *pulDataSize = %d", __func__, __LINE__, *pulDataSize);
				*ppucData = malloc( *pulDataSize );
				if (*ppucData)
				{
#if !defined(__SUPPORT_AWS_IOT_W__) //orig SDK[[:
					memcpy (*ppucData, democonfigCLIENT_CERTIFICATE_PEM, *pulDataSize);
#else //awsupgradeport[[::
					memcpy (*ppucData, AWS_IOT_CLIENT_CERT, *pulDataSize);
#endif //]]
				}
				else
				{
					xReturn = CKR_HOST_MEMORY;
					LogError( ("malloc failed") );
				}
			}
			else
			{
				LogError( ( "Could not get object value. Failed to determine object size." ) );
				xReturn = CKR_FUNCTION_FAILED;
			}
		break;
		
		case eAwsCodeSigningKey:
		case eAwsHMACSecretKey:
		case eAwsCMACSecretKey:
			LogError( ("not yet supported : (%d)", (int)xHandle) );
            LogError( ( "Could not get object value. Failed to determine object size." ) );
            xReturn = CKR_FUNCTION_FAILED;
		break;

		default:
			LogError( ("Invalid operation") );
            LogError( ( "Could not get object value. Failed to determine object size." ) );
            xReturn = CKR_FUNCTION_FAILED;
		break;
	}
#endif //]]

    return xReturn;
}

/*-----------------------------------------------------------*/

CK_RV PKCS11_PAL_Initialize( void )
{
    return CKR_OK;
}

CK_OBJECT_HANDLE PKCS11_PAL_SaveObject( CK_ATTRIBUTE_PTR pxLabel,
                                        CK_BYTE_PTR pucData,
                                        CK_ULONG ulDataSize )
{
#if !defined(__USE_AWS_TLS_ALT__) //orig SDK[[::
    FILE * pxFile = NULL;
    size_t ulBytesWritten;
#endif //]]
    const char * pcFileName = NULL;
    CK_OBJECT_HANDLE xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;
    rm_cert_err_t err = RM_CERT_ERR_OK;

    if( ( pxLabel != NULL ) && ( pucData != NULL ) )
    {
        /* Converts a label to its respective filename and handle. */
        PAL_UTILS_LabelToFilenameHandle( pxLabel->pValue,
                                         &pcFileName,
                                         &xHandle );
    }
    else
    {
        LogError( ( "Could not save object. Received invalid parameters." ) );
    }

    if( pcFileName != NULL )
    {
#if !defined(__USE_AWS_TLS_ALT__) //orig SDK[[::
        /* Overwrite the file every time it is saved. */
        pxFile = fopen( pcFileName, "w" );

        if( NULL == pxFile )
        {
            LogError( ( "PKCS #11 PAL was unable to save object to file. "
                        "The PAL was unable to open a file with name %s in write mode.", pcFileName ) );
            xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;
        }
        else
        {
            ulBytesWritten = fwrite( pucData, sizeof( uint8_t ), ulDataSize, pxFile );

            if( ulBytesWritten != ulDataSize )
            {
                LogError( ( "PKCS #11 PAL was unable to save object to file. "
                            "Expected to write %lu bytes, but wrote %lu bytes.", ulDataSize, ulBytesWritten ) );
                xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;
            }
            else
            {
                LogDebug( ( "Successfully wrote %lu to %s", ulBytesWritten, pcFileName ) );
            }
        }

        if( NULL != pxFile )
        {
            ( void ) fclose( pxFile );
        }
#else //awsupgradeport[[::file system not supported
		LogWarn( ( "file system not supported" ) );

		PAL_UTILS_FilenameToHandle(pcFileName, &xHandle);

		switch (xHandle)
		{
			case eAwsDevicePrivateKey:
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsDevicePrivateKey: ulDataSize = %d", __func__, __LINE__, ulDataSize);
			        err = RM_CERT_Write(RM_CERT_GetModule(SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR),
					            RM_CERT_TYPE_UNIQUE_PRIV_KEY, RM_CERT_FORMAT_DER,
						    pucData, ulDataSize);
				if (err)
				{
					xHandle = eInvalidHandle;
				}
			break;

			case eAwsDeviceCertificate:
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsDeviceCertificate: ulDataSize = %d", __func__, __LINE__, ulDataSize);
				err = RM_CERT_Write(RM_CERT_GetModule(SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR),
						    RM_CERT_TYPE_UNIQUE_CERT, RM_CERT_FORMAT_DER,
						    pucData, ulDataSize);
				if (err)
				{
				    xHandle = eInvalidHandle;
				}
			break;

			case eAwsDevicePublicKey:
				awsiot_app_print_elapse_time_ms("[%s:%d] eAwsDevicePublicKey: ulDataSize = %d", __func__, __LINE__, ulDataSize);
				//hex_dump(pucData, ulDataSize);
				if (pGenPublicKey)
				{
					memset(pGenPublicKey, 0, MAX_PKCS11_GEN_KEY_LEN);
					memcpy(pGenPublicKey, pucData, ulDataSize);
					lenGenPublicKey = ulDataSize;
				}
				else
				{
					lenGenPublicKey = 0;
					xHandle = eInvalidHandle;
				}
			break;

			case eAwsCodeSigningKey:
			case eAwsHMACSecretKey:
			case eAwsCMACSecretKey:
			case eAwsClaimPrivateKey:
			case eAwsClaimCertificate:
				LogError( ("not yet supported : (%d)", (int)xHandle) );
				xHandle = eInvalidHandle;
			break;

			default:
				LogError( ("Invalid operation") );
			break;
		}
#endif //]]
    }
    else
    {
        LogError( ( "Could not save object. Unable to find the correct file." ) );
    }

    return xHandle;
}

/*-----------------------------------------------------------*/


CK_OBJECT_HANDLE PKCS11_PAL_FindObject( CK_BYTE_PTR pxLabel,
                                        CK_ULONG usLength )
{
    const char * pcFileName = NULL;
    CK_OBJECT_HANDLE xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;

    ( void ) usLength;

    if( pxLabel != NULL )
    {
        PAL_UTILS_LabelToFilenameHandle( ( const char * ) pxLabel,
                                         &pcFileName,
                                         &xHandle );

        if( CKR_OK != prvFileExists( pcFileName ) )
        {
            xHandle = ( CK_OBJECT_HANDLE ) eInvalidHandle;
        }
    }
    else
    {
        LogError( ( "Could not find object. Received a NULL label." ) );
    }

    return xHandle;
}
/*-----------------------------------------------------------*/

CK_RV PKCS11_PAL_GetObjectValue( CK_OBJECT_HANDLE xHandle,
                                 CK_BYTE_PTR * ppucData,
                                 CK_ULONG_PTR pulDataSize,
                                 CK_BBOOL * pIsPrivate )
{
    CK_RV xReturn = CKR_OK;
    const char * pcFileName = NULL;


    if( ( ppucData == NULL ) || ( pulDataSize == NULL ) || ( pIsPrivate == NULL ) )
    {
        xReturn = CKR_ARGUMENTS_BAD;
        LogError( ( "Could not get object value. Received a NULL argument." ) );
    }
    else
    {
        xReturn = PAL_UTILS_HandleToFilename( xHandle, &pcFileName, pIsPrivate );
    }

    if( xReturn == CKR_OK )
    {
        xReturn = prvReadData( pcFileName, ppucData, pulDataSize );
    }

    return xReturn;
}

/*-----------------------------------------------------------*/

void PKCS11_PAL_GetObjectValueCleanup( CK_BYTE_PTR pucData,
                                       CK_ULONG ulDataSize )
{
    /* Unused parameters. */
    ( void ) ulDataSize;

    if( NULL != pucData )
    {
        free( pucData );
    }
}

/*-----------------------------------------------------------*/

CK_RV PKCS11_PAL_DestroyObject( CK_OBJECT_HANDLE xHandle )
{
    const char * pcFileName = NULL;
    CK_BBOOL xIsPrivate = CK_TRUE;
    CK_RV xResult = CKR_OBJECT_HANDLE_INVALID;
#if !defined(__USE_AWS_TLS_ALT__) //orig SDK[[::
    int ret = 0;
#endif //]]


    xResult = PAL_UTILS_HandleToFilename( xHandle,
                                          &pcFileName,
                                          &xIsPrivate );

    if( ( xResult == CKR_OK ) && ( prvFileExists( pcFileName ) == CKR_OK ) )
    {
#if !defined(__USE_AWS_TLS_ALT__) //orig SDK[[::
        ret = remove( pcFileName );

        if( ret != 0 )
        {
            xResult = CKR_FUNCTION_FAILED;
        }
#else //awsupgradeport[[::
		LogWarn( ( "file system not supported" ) );
		xResult = CKR_OK;
#endif //]]
    }

    return xResult;
}

/*-----------------------------------------------------------*/
