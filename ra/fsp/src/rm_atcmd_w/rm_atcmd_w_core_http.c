/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#if CFG_WIFI
 #include "custom_config_sdk.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <strings.h>
 #include "mbedtls/ssl.h"
 #include "rm_atcmd_w_core.h"
 #include "rm_atcmd_w_core_http.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #include "rm_map_persistant_w.h"
 #include "rm_cert.h"
 #include "rm_https_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #define ENABLE_HTTPC_DEBUG

 #if defined(ENABLE_HTTPC_DEBUG)
  #define HTTPC_PRINTF    printf
 #else
  #define HTTPC_PRINTF(...)    do {} while (0)
 #endif

/* Debug Log level(Information) for HTTP Client */
/* #undef  ENABLE_HTTPC_DEBUG_INFO */
/* #define   ENABLE_HTTPC_DEBUG_INFO */

 #if defined(ENABLE_HTTPC_DEBUG_INFO)
  #define HTTPC_DEBUG_INFO(fmt, ...)    HTTPC_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
 #else
  #define HTTPC_DEBUG_INFO(...)         do {} while (0)
 #endif                                /* ENABLE_HTTPC_DEBUG_INFO */

/* #define   ENABLE_HTTPC_DEBUG_DUMP */

 #if defined(ENABLE_HTTPC_DEBUG_DUMP)
  #define HTTPC_DEBUG_DUMP(...)    HTTPC_PRINTF(__VA_ARGS__)
 #else
  #define HTTPC_DEBUG_DUMP(...)    do {} while (0)
 #endif                                /* ENABLE_HTTPC_DEBUG_INFO */

/* Debug Log level(Error) for HTTP Client */
 #define ENABLE_HTTPC_DEBUG_ERR

 #if defined(ENABLE_HTTPC_DEBUG_ERR)
  #define HTTPC_DEBUG_ERR(fmt, ...)    HTTPC_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ## __VA_ARGS__)
 #else
  #define HTTPC_DEBUG_ERR(...)         do {} while (0)
 #endif                                /* ENABLE_HTTPC_DEBUG_ERR */

/***********************************************************************************************************************
 * Function prototypes
 ***********************************************************************************************************************/
static void rm_atcmd_w_httpc_recv_callback(https_callback_args_t * p_args);
static void dump_http_payload(void * p_args);

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
unsigned char g_tls_srv_cert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDBjCCAe4CCQCg5xtL5ap/CDANBgkqhkiG9w0BAQsFADBFMQswCQYDVQQGEwJB\n"
    "VTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0\n"
    "cyBQdHkgTHRkMB4XDTE4MDgyMTA3Mzg0M1oXDTI4MDgxODA3Mzg0M1owRTELMAkG\n"
    "A1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0\n"
    "IFdpZGdpdHMgUHR5IEx0ZDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
    "ALMgpDjh0c0ZThAxE/B2mBcDAp2KUaeoXqY5+03bZWRQ7McE0og3DV9u14FGwhDQ\n"
    "J93itsHXnWGZ6Zh1xtPJzoMiaxxaAe+1WdognwlTD8LUr7qS/7VtFXXmvVaIQ7E5\n"
    "AKLxlFsgVc3soSLo4f06DWOGl7pSIC2EttYFKMvBmlWcksEA1+DXP/NJy8h3wqTF\n"
    "MC+iydE5cXd3lKJ3CQ+0zTJli4DOF5ZudbrkWOQXwfZ1ylB24umvbNMSfltlSkuC\n"
    "km0VhF8VCLFwek+WNcrqeDkAAyMyRyVlDi+9r1qTHPF+EYwcJHWh+zFegztGc1w6\n"
    "7mxBBh+JRBH7GzWLT9kBg7kCAwEAATANBgkqhkiG9w0BAQsFAAOCAQEAWJVjI3po\n"
    "zjCpsfrPw1u/K/0AJGRpLChvBPMYz11/Ay2I2vyT8akRM+7km0W/d+wiyL0YT9QI\n"
    "PHT4gLEV4KeiQSAPindtJgbg2Wi53EBGeNrDWHMwH3islDr9h293zhUqZMie2WDb\n"
    "kpEcSh79DAhZDZ43d/qHgWeyWWkyC8JLQIzYPsdTJYf8qq7yNZlr7OZCHXdq2sG8\n"
    "SjE4I1j4ANkkJpnX6yFDZZGDDrUzPcJRPVtZzmytJUIbzoTU9E6nSDIlvBTT273o\n"
    "9gJXvkU7L4cwNCAg0g3RRwyqIUq8zyWsIeNSdqz64EIH2wX4XvWkVzDzfxW76HL7\n"
    "NOds4a5SIHfFZw==\n"
    "-----END CERTIFICATE-----\n";
size_t g_tls_srv_cert_len = sizeof(g_tls_srv_cert);

unsigned char g_tls_srv_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEpAIBAAKCAQEAsyCkOOHRzRlOEDET8HaYFwMCnYpRp6hepjn7TdtlZFDsxwTS\n"
    "iDcNX27XgUbCENAn3eK2wdedYZnpmHXG08nOgyJrHFoB77VZ2iCfCVMPwtSvupL/\n"
    "tW0Vdea9VohDsTkAovGUWyBVzeyhIujh/ToNY4aXulIgLYS21gUoy8GaVZySwQDX\n"
    "4Nc/80nLyHfCpMUwL6LJ0Tlxd3eUoncJD7TNMmWLgM4Xlm51uuRY5BfB9nXKUHbi\n"
    "6a9s0xJ+W2VKS4KSbRWEXxUIsXB6T5Y1yup4OQADIzJHJWUOL72vWpMc8X4RjBwk\n"
    "daH7MV6DO0ZzXDrubEEGH4lEEfsbNYtP2QGDuQIDAQABAoIBADniYG8pOhznAnzk\n"
    "/yaDjF5TULMMEZr2I6/fqL/eGAO0yu79NfNipuWh8e4KqYe5XEitjJVTUb5KeFwW\n"
    "IywpWJyzsJ020M1fcyuzwvDGcJ9rD2ZhPlSobXjuGV0vJ4DLhNMi8egIqPGkd+XK\n"
    "D80+xzjUM4+4HkHXUyYSAL7nTzI+nh9mm3yCntFSJsNj1tRX7IVjnjh1RIS7MR5B\n"
    "f0u9xVGf3UbBveK/RyVGd/QWJacktBcr3WyCxHkwo64TETuj5D3NxqIrmiChBauB\n"
    "ediqoe3RuxPw6vgE3+z3mfDdD0Xb58CsvN27dW//euf1xZjjw6xLqAo3Zd8fAMME\n"
    "EFDURiECgYEA1mX/Z0OoNId3SoFhg5NkQlEBSb2Lna35JtqhuylPLarjapoGj3C/\n"
    "4z0hAIU6d1W3L12P175XaHrDuA51Tc9bnMp/JuSrV6r5CvmZJ81MftYlJzl41zJd\n"
    "JIf1nILxlXvQS9rVHSqIO/0dPwEAkGKJHqqhyWTM5sh3CcgpQy7JsL0CgYEA1eKZ\n"
    "pDdgkLRQgmtkW0FDIkR1aG6J6gE/TpmmSY64xqEYr5rnGRObvYrGQXHp9oD/QuCH\n"
    "fiVeudvuyQYXt0KsEv0NyhoLWmAlTij1eKEjUmvHrTV3ydMSdEIs5p5k6w3Ht8KU\n"
    "4YAjjFglL+0xDV9EC6nwB4swNwKfBpifuwkspK0CgYB1RFjMHJ92C9pdsCKsGwQt\n"
    "ma0ArmIdHrk2XUM04cVjDyNQfWq1LlBmdFsGs9hkyUdm6t/wezXH+c3vcEkNBCvx\n"
    "uHiPx2dIjkWlkRwKPypl/a9YowDLg8qaXpsiviRxRMWLl+gVCdx2I13JxjyOvLaP\n"
    "RXk0dKP2XxNtEEQxcPf0aQKBgQDHXzzcqIopGQvbJoQb1E/iB3Jx8Gg6awM6H1u0\n"
    "QYfYD57VQk2dQHvySQPZSXhPwZswGd/zJJ6SHYMOe9FrkIiaAqzx8SkYC3t6yg9X\n"
    "bM1iLPmqaabJySjwmicEqi1kNiovDwB821dHoXq4nB8XWfAx9yy5u3MsNBNMsMRk\n"
    "Mn8c2QKBgQC0OKboeUgzk9i06KE6CqGO43WpPDKNG+4ylnCnIxjeGIYLNKHJ+RCv\n"
    "4lt+FHWcwciM3JoNJWSfqXGcqp2B97I/YDW87L6He4ShUmzvdlAG1CecGqTYBrYB\n"
    "lcr/+RukWzIMUCA24KeAoheKsC+HJGg4U08nWz5n9K30kqNya+nz9g==\n"
    "-----END RSA PRIVATE KEY-----\n";
size_t g_tls_srv_key_len = sizeof(g_tls_srv_key);

https_server_sec_t g_tls_https;

extern const https_api_t  g_https_w;
const https_api_t * const p_atcmd_https = &g_https_w;
static atcmd_w_ctrl_t   * g_p_at_ctrl   = NULL;

https_w_instance_ctrl_t g_atcmd_https_w0_ctrl;

https_cfg_t g_atcmd_https_w0_cfg =
{
    .p_callback = rm_atcmd_w_httpc_recv_callback,
};
static volatile uint32_t client_transfer_complete = 0;
static volatile uint32_t client_transfer_error    = 0;
static volatile uint32_t server_transfer_complete = 0;
static volatile uint32_t server_transfer_error    = 0;

static http_client_request_t g_request = {0, };

static void http_client_clear_alpn (httpc_secure_connection_t * conf)
{
    int idx = 0;

    if (conf->alpn)
    {
        for (idx = 0; idx < conf->alpn_cnt; idx++)
        {
            if (conf->alpn[idx])
            {
                vPortFree(conf->alpn[idx]);
                conf->alpn[idx] = NULL;
            }
        }

        vPortFree(conf->alpn);
        conf->alpn     = NULL;
        conf->alpn_cnt = 0;
    }
}

static void http_client_clear_https_conf (httpc_secure_connection_t * conf)
{
    if (conf)
    {
        if (conf->ca_len > 0)
        {
            vPortFree(conf->ca);
            conf->ca     = NULL;
            conf->ca_len = 0;
        }

        if (conf->cert_len > 0)
        {
            vPortFree(conf->cert);
            conf->cert     = NULL;
            conf->cert_len = 0;
        }

        if (conf->privkey_len > 0)
        {
            vPortFree(conf->privkey);
            conf->privkey     = NULL;
            conf->privkey_len = 0;
        }

        if (conf->dh_param_len > 0)
        {
            vPortFree(conf->dh_param);
            conf->dh_param     = NULL;
            conf->dh_param_len = 0;
        }

        if (conf->sni_len > 0)
        {
            vPortFree(conf->sni);
            conf->sni     = NULL;
            conf->sni_len = 0;
        }

        http_client_clear_alpn(conf);
    }
}

err_t http_client_set_tls_version (int tls_ver)
{
    if ((tls_ver == ATCMD_W_HTTPS_TLS12) ||
        (tls_ver == ATCMD_W_HTTPS_TLS13) ||
        (tls_ver == ATCMD_W_HTTPS_TLS12_13))
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      HTTPC_NVRAM_CONFIG_TLS_VER,
                                      tls_ver);

        HTTPC_DEBUG_INFO("WriteNVRAM tls_ver = %d\n", tls_ver);
 #endif

        return ERR_OK;
    }

    return ERR_ARG;
}

err_t http_client_get_tls_version (int * p_tls_ver)
{
    int tls_ver = ATCMD_W_HTTPS_TLS12;

 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_VER,
                                     &tls_ver) != FSP_SUCCESS)
    {
        tls_ver = ATCMD_W_HTTPS_TLS12;
    }
 #endif

    *p_tls_ver = tls_ver;

    HTTPC_DEBUG_INFO("ReadNVRAM tls_ver = %d\n", *p_tls_ver);

    return ERR_OK;
}

err_t http_client_set_tls_auth_mode (int tls_auth_mode)
{
    if ((tls_auth_mode >= MBEDTLS_SSL_VERIFY_NONE) &&
        (tls_auth_mode < MBEDTLS_SSL_VERIFY_UNSET))
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      HTTPC_NVRAM_CONFIG_TLS_AUTH,
                                      tls_auth_mode);

        HTTPC_DEBUG_INFO("WriteNVRAM tls_auth_mode = %d\n", tls_auth_mode);
 #endif

        return ERR_OK;
    }

    HTTPC_DEBUG_INFO("TLS Authentication is only %d ~ %d\n", MBEDTLS_SSL_VERIFY_NONE, MBEDTLS_SSL_VERIFY_REQUIRED);

    return ERR_ARG;
}

err_t http_client_get_tls_auth_mode (int * p_tls_auth_mode)
{
    int auth = MBEDTLS_SSL_VERIFY_NONE;

 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_AUTH,
                                     &auth) != FSP_SUCCESS)
    {
        auth = MBEDTLS_SSL_VERIFY_NONE;
    }
 #endif

    *p_tls_auth_mode = auth;

    HTTPC_DEBUG_INFO("tls_auth_mode = %d\n", *p_tls_auth_mode);

    return ERR_OK;
}

err_t http_client_set_sni (int argc, char * argv[])
{
    RA6W1_UNUSED_ARG(argc);

    if (strcmp(argv[1], "AT+NWHTCSNIDEL") == 0)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_SNI);
 #endif

        return ERR_OK;
    }

    if (strlen(argv[1]) > HTTPC_MAX_SNI_LEN)
    {
        return ERR_ARG;
    }

 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_SNI,
                                         argv[1]))
    {
        return ERR_ARG;
    }
 #endif

    return ERR_OK;
}

err_t http_client_set_alpn (int argc, char * argv[])
{
    RA6W1_UNUSED_ARG(argc);

    char nvr_name[32] = {0x00, };
    int  alpn_num     = -1;
    int  tmp          = 0;
    int  num          = 0;

    if (strcmp(argv[0], "AT+NWHTCALPNDEL") == 0)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &alpn_num);
 #endif
        if (alpn_num != -1)
        {
            for (num = 0; num < alpn_num; num++)
            {
                char nvram_name[25] = {0, };

                sprintf(nvram_name, "%s%d", HTTPC_NVRAM_CONFIG_TLS_ALPN, num);
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvram_name);
 #endif
            }

 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                      HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM);
 #endif
        }

        return ERR_OK;
    }

    alpn_num = atoi(argv[1]);

    /* Check number of ALPN. */
    if ((alpn_num > HTTPC_MAX_ALPN_CNT) || (alpn_num <= 0))
    {
        return ERR_ARG;
    }

    /* Check length of ALPN. */
    for (num = 0; num < alpn_num; num++)
    {
        if ((strlen(argv[num + 2]) > HTTPC_MAX_ALPN_LEN) || (strlen(argv[num + 2]) <= 0))
        {
            return ERR_ARG;
        }
    }

    tmp = -1;
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                 &tmp);
 #endif
    if (tmp != -1)
    {
        for (num = 0; num < tmp; num++)
        {
            sprintf(nvr_name, "%s%d", HTTPC_NVRAM_CONFIG_TLS_ALPN, num);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
 #endif
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM);
 #endif
    }

    for (num = 0; num < alpn_num; num++)
    {
        sprintf(nvr_name, "%s%d", HTTPC_NVRAM_CONFIG_TLS_ALPN, num);
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, argv[num + 2]);
 #endif
    }

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_APPCFG,
                                  HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                  alpn_num);
 #endif

    return ERR_OK;
}

static void httpc_atcmd_status (err_t err)
{
    char httpc_status[30];

    memset(httpc_status, 0, sizeof(httpc_status));
    sprintf(httpc_status, "\r\n+NWHTCSTATUS:%d\r\n", err);
    if (g_p_at_ctrl)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_ATCMD_W_CORE_Write(g_p_at_ctrl, (uint8_t *) httpc_status, (uint32_t) strlen(httpc_status));
 #endif
    }
}

static err_t httpc_atcmd_response (char          * payload,
                                   int             len,
                                   u32_t           content_len,
                                   httpc_result_t  httpc_result,
                                   httpc_state_t * connection)
{
    RA6W1_UNUSED_ARG(content_len);
    RA6W1_UNUSED_ARG(httpc_result);
    RA6W1_UNUSED_ARG(connection);

    char * offset = NULL;
    char * buffer = NULL;
    int    buffer_len;

    buffer_len = len + 25;             // "+NWHTCDATA:%d,"

    buffer = pvPortMalloc(buffer_len);
    if (buffer == NULL)
    {
        HTTPC_DEBUG_ERR("Failed to allocate memory for at-cmd (need=%dbyte)\n", buffer_len);

        return ERR_MEM;
    }

    memset(buffer, 0x00, buffer_len);
    sprintf(buffer, "\r\n+NWHTCDATA:%d,", len);

    offset  = buffer;
    offset  = strstr(buffer, ",");
    offset += 1;
    memcpy(offset, payload, (size_t) len);
    memcpy(offset + len, "\r\n", 2);

    if (g_p_at_ctrl)
    {
        // "\r\n+NWHTCDATA:%d,data\r\n"
        RM_ATCMD_W_CORE_Write(g_p_at_ctrl, (uint8_t *) buffer, (uint32_t) ((offset - buffer) + len + 2));
    }

    if (buffer != NULL)
    {
        vPortFree(buffer);
        buffer = NULL;
    }

    return ERR_OK;
}

static void rm_atcmd_w_httpc_recv_callback (https_callback_args_t * p_args)
{
    https_callback_args_t * p = (https_callback_args_t *) p_args;

    switch (p_args->event)
    {
        case HTTPS_EVENT_SERVER_RECVED:
        {
            err_t err = *(int8_t *) p_args->p_param;

            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_SERVER_RECVED\n");
            if ((err != ERR_OK) || !p_args->len)
            {
                HTTPC_DEBUG_ERR("[%s]:%d err=%d, p->len = %d\n", __func__, __LINE__, err, p_args->len);
                server_transfer_error = 1;
            }

            if (err == ERR_OK)
            {
                server_transfer_complete = 1;
            }

            HTTPC_DEBUG_INFO("\n[%s]:%d err = %d, p->len = %d\n", __func__, __LINE__, err, p_args->len);
            dump_http_payload(p_args);

            httpc_atcmd_response((char *) p->payload, (int) p->len, 0, 0, NULL);
            break;
        }

        case HTTPS_EVENT_SERVER_ERR_RESULT:
        {
            err_t err = *(int8_t *) p_args->p_param;

            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_SERVER_ERR_RESULT\n");
            HTTPC_DEBUG_ERR("err: %d\n", err);

            if (err != ERR_OK)
            {
                server_transfer_error = 1;
            }

            break;
        }

        case HTTPS_EVENT_CLIENT_RESULT:
        {
            err_t err = *(int8_t *) p_args->p_param;

            httpc_atcmd_status(*(uint32_t *) p_args->payload);
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_RESULT\n");
            HTTPC_DEBUG_INFO("received: %d byte, err: %d\n", p_args->len, err);

            if (err == ERR_OK)
            {
                client_transfer_complete = 1;
            }
            else
            {
                client_transfer_error = 1;
            }

            http_client_clear_https_conf(&g_request.https_conf);
            p_atcmd_https->close(&g_atcmd_https_w0_ctrl);

            break;
        }

        case HTTPS_EVENT_CLIENT_RECVED:
        {
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_RECVED\n");
            HTTPC_DEBUG_INFO("received: %d byte, err: %d\n", p_args->len, *(int8_t *) p_args->p_param);
            dump_http_payload(p_args);

            httpc_atcmd_response((char *) p->payload, (int) p->len, 0, 0, NULL);

            break;
        }

        case HTTPS_EVENT_CLIENT_RECVED_DECODED:
        {
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_RECVED_DECODED\n");
            break;
        }

        case HTTPS_EVENT_CLIENT_GET_DONE:
        {
            HTTPC_DEBUG_INFO("Event: HTTPS_EVENT_CLIENT_GET_DONE\n");
            break;
        }

        default:
        {
            break;
        }
    }
}

static void dump_http_payload (void * p_args)
{
    https_callback_args_t * p       = (https_callback_args_t *) p_args;
    uint8_t               * payload = (uint8_t *) p->payload;
    int32_t                 len     = p->len;

    for (int i = 0; i < len; i++)
    {
        HTTPC_DEBUG_DUMP("%02x ", payload[i]);
        if (((i + 1) % 16 == 0) || (i == len - 1))
        {
            int start   = (i + 1) % 16 == 0 ? i - 15 : (i / 16) * 16;
            int end     = i;
            int padding = 16 - (end - start + 1);

            HTTPC_DEBUG_DUMP(" ");

            for (int k = 0; k < padding; k++)
            {
                HTTPC_DEBUG_DUMP("   ");
            }

            HTTPC_DEBUG_DUMP("| ");
            for (int j = start; j <= end; j++)
            {
                if ((payload[j] >= 32) && (payload[j] <= 126))
                {
                    HTTPC_DEBUG_DUMP("%c", payload[j]);
                }
                else
                {
                    HTTPC_DEBUG_DUMP(".");
                }
            }

            HTTPC_DEBUG_DUMP("\n");
        }
    }

    HTTPC_DEBUG_DUMP("\n");
}

static void rm_atcmd_w_http_client_display_usage (void)
{
    HTTPC_PRINTF("\nUsage: HTTP Client\n");
    HTTPC_PRINTF("\x1b[93mName\x1b[0m\n");
    HTTPC_PRINTF("\thttp-client - HTTP Client\n");
    HTTPC_PRINTF("\x1b[93mSYNOPSIS\x1b[0m\n");
    HTTPC_PRINTF("\thttp-client [OPTION]...URL\n");
    HTTPC_PRINTF("\x1b[93mDESCRIPTION\x1b[0m\n");
    HTTPC_PRINTF("\tRequest client's method to URL\n");

    HTTPC_PRINTF("\t\x1b[93m-i [wlan0|wlan1]\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet interface of HTTP Client\n");
    HTTPC_PRINTF("\t\x1b[93m-status\x1b[0m\n");
    HTTPC_PRINTF("\t\tDisplay status of HTTP Client\n");
    HTTPC_PRINTF("\t\x1b[93m-help\x1b[0m\n");
    HTTPC_PRINTF("\t\tDisplay help\n");

    HTTPC_PRINTF("\t\x1b[93m-head\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest HEAD method to URI\n");
    HTTPC_PRINTF("\t\x1b[93m-get\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest GET method to URI\n");
    HTTPC_PRINTF("\t\x1b[93m-post RESOURCE\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest POST method to URI with RESOURCE\n");
    HTTPC_PRINTF("\t\x1b[93m-put RESOURCE\x1b[0m\n");
    HTTPC_PRINTF("\t\tRequest PUT method to URI with RESOURCE\n");
    HTTPC_PRINTF("\t\x1b[93m-message header + body\x1b[0m\n");
    HTTPC_PRINTF("\t\tInput header + body in free form\n");

    HTTPC_PRINTF("\t\x1b[93m-incoming Size\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet incoming buffer size of TLS Contents\n");
    HTTPC_PRINTF("\t\x1b[93m-outgoing Size\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet outgoing buffer size of TLS Contents\n");
    HTTPC_PRINTF("\t\x1b[93m-authmode\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet TLS auth_mode\n");
    HTTPC_PRINTF("\t\x1b[93m-sni <Server Name Indicator>\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet SNI for TLS extension\n");
    HTTPC_PRINTF("\t\x1b[93m-alpn <ALPN Protocols>\x1b[0m\n");
    HTTPC_PRINTF("\t\tSet ALPN for TLS extension\n");
}

static fsp_err_t rm_atcmd_w_http_client_parse_request (int argc, char * argv[], http_client_request_t * request)
{
    RA6W1_UNUSED_ARG(argc);

    err_t  err = ERR_OK;
    char * p   = NULL;
    char * q   = NULL;
    int    len = 0;

    request->op_code = HTTP_CLIENT_OPCODE_READY;

    // CMD
    if (argv[0] == NULL)
    {
        err = ERR_ARG;
        goto exit;
    }

    if (strcmp(argv[1], "stop") == 0)
    {
        request->op_code = HTTP_CLIENT_OPCODE_STOP;
        goto exit;
    }

    // URL
    if (argv[1] == NULL)
    {
        err = ERR_ARG;
        goto exit;
    }

    if (strncmp(argv[1], "https", strlen("https")) == 0)
    {
        // https
        request->insecure = pdTRUE;
        request->port     = HTTPS_SERVER_PORT;
    }
    else if (strncmp(argv[1], "http", strlen("http")) == 0)
    {
        // http
        request->insecure = pdFALSE;
        request->port     = HTTP_SERVER_PORT;

        // http_client_clear_https_conf(&request->https_conf);
    }
    else if (strncmp(argv[1], "http", strlen("http")) == 0)
    {
        // http
        request->insecure = pdFALSE;
        request->port     = HTTP_SERVER_PORT;
    }
    else
    {
        err = ERR_ARG;
        goto exit;
    }

    memset(request->path, 0x00, HTTPC_MAX_PATH_LEN);
    memcpy(request->path, argv[1], strlen((const char *) argv[1]));

    p   = (char *) request->path;
    len = strlen((char *) request->path);

    q    = strstr(p, "://");
    q   += (strlen("://"));
    len -= q - p;
    p    = q;

    q = strstr(p, "@");
    if (q != NULL)
    {
        q   += (strlen("@"));
        len -= q - p;
        p    = q;
    }
    else
    {
        q = p;
    }

    q = strstr(p, ":");
    if (q != NULL)
    {
        /* IPv4 address or FQDN */
        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, p, (size_t) (q - p));

        q   += (strlen(":"));
        len -= q - p;
        p    = q;

        while (len && isdigit((int) (*q)))
        {
            ++q;
            --len;
        }

        if (p < q)
        {
            int port = 0;

            while (p < q)
            {
                /* explicit port number given */
                port = port * 10 + (*p++ - '0');
            }

            request->port = (UINT) port;
        }
    }
    else
    {
        q = strstr(p, "/");
        if ((q == NULL) && len)
        {
            err = ERR_ARG;
            goto exit;
        }

        if (strstr(p, "["))
        {
            /* IPv6 address reference */

            // not supported ipv6
            HTTPC_DEBUG_ERR("Not supported IPv6\n");
            err = ERR_ARG;
            goto exit;
        }

        /* IPv4 address or FQDN */
        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, p, (size_t) (q - p));
    }

    // Option
    if (argv[2] == NULL)
    {
        err = ERR_ARG;
        goto exit;
    }

    if (strcmp("head", argv[2]) == 0)
    {
        request->op_code = HTTP_CLIENT_OPCODE_HEAD;
    }
    else if (strcmp("get", argv[2]) == 0)
    {
        request->op_code = HTTP_CLIENT_OPCODE_GET;
    }
    else if ((strcmp("put", argv[2]) == 0) || (strcmp("post", argv[2]) == 0))
    {
        if ((argv[3] == NULL) || (strlen(argv[3]) > HTTPC_MAX_REQ_DATA))
        {
            err = ERR_ARG;
            goto exit;
        }

        if (strcmp("put", argv[2]) == 0)
        {
            request->op_code = HTTP_CLIENT_OPCODE_PUT;
        }
        else
        {
            request->op_code = HTTP_CLIENT_OPCODE_POST;
        }

        memset(request->data, 0x00, HTTPC_MAX_REQ_DATA);
        memcpy(request->data, argv[3], strlen((const char *) argv[3]));
    }
    else if (strcmp(argv[2], "message") == 0)
    {
        if ((argv[3] == NULL) || (strlen(argv[3]) > HTTPC_MAX_REQ_DATA))
        {
            err = ERR_ARG;
            goto exit;
        }

        request->op_code = HTTP_CLIENT_OPCODE_MESSAGE;

        memset(request->data, 0x00, HTTPC_MAX_REQ_DATA);
        memcpy(request->data, argv[3], strlen((const char *) argv[3]));
    }
    else
    {
        if (strlen((const char *) (request->hostname)) > 0)
        {
            request->op_code = HTTP_CLIENT_OPCODE_GET;
        }
        else
        {
            request->op_code = HTTP_CLIENT_OPCODE_HELP;
            err              = ERR_ARG;
            goto exit;
        }
    }

exit:

    return err;
}

static int http_tls_read_cert (int module, int type, unsigned char ** out, size_t * outlen)
{
    int              ret    = 0;
    unsigned char  * buf    = NULL;
    size_t           buflen = RM_CERT_MAX_LENGTH;
    rm_cert_format_t format = RM_CERT_FORMAT_NONE;

    buf = pvPortMalloc(buflen);
    if (!buf)
    {
        HTTPC_DEBUG_ERR("Failed to allocate memory(module:%d, type:%d, len:%d)\r\n", module, type, (int) buflen);

        return -1;
    }

    memset(buf, 0x00, buflen);

    ret = RM_CERT_Read(module, type, &format, buf, &buflen);
    if (ret == RM_CERT_ERR_OK)
    {
        *out    = buf;
        *outlen = buflen;

        return 0;
    }
    else if (ret == RM_CERT_ERR_EMPTY_CERTIFICATE)
    {
        if (buf)
        {
            vPortFree(buf);
            buf     = NULL;
            *out    = NULL;
            *outlen = 0;
        }

        return 0;
    }

    if (buf)
    {
        vPortFree(buf);
        buf     = NULL;
        *out    = NULL;
        *outlen = 0;
    }

    return -1;
}

static void http_server_read_certs (https_server_sec_t * conf)
{
    int ret = 0;

    // to read certificate
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_SERVER,
                             RM_CERT_TYPE_CERT,
                             &conf->p_tls_srv_cert,
                             &conf->tls_srv_cert_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("Failed to read certificate\r\n");
    }
    else
    {
        HTTPC_DEBUG_INFO("Read Cert(length = %d)\n", conf->tls_srv_cert_len);
    }

    // to read private key
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_SERVER,
                             RM_CERT_TYPE_PRIVATE_KEY,
                             &conf->p_tls_srv_key,
                             &conf->tls_srv_key_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("Failed to read private key\r\n");
    }
    else
    {
        HTTPC_DEBUG_INFO("Read Privkey(length = %d)\n", conf->tls_srv_key_len);
    }
}

static void http_client_read_certs (httpc_secure_connection_t * conf)
{
    int ret = 0;

    // to read ca certificate
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_CLIENT, RM_CERT_TYPE_CA_CERT, &conf->ca, &conf->ca_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("failed to read CA cert\r\n");
        goto err;
    }

    if (conf->ca_len > 0)
    {
        HTTPC_DEBUG_INFO("Read CA(length = %d)\n", conf->ca_len);
    }

    // to read certificate
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_CLIENT, RM_CERT_TYPE_CERT, &conf->cert, &conf->cert_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("failed to read certificate\r\n");
        goto err;
    }

    if (conf->cert_len > 0)
    {
        HTTPC_DEBUG_INFO("Read Cert(length = %d)\n", conf->cert_len);
    }

    // to read private key
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_CLIENT, RM_CERT_TYPE_PRIVATE_KEY, &conf->privkey, &conf->privkey_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("failed to read private key\r\n");
        goto err;
    }

    if (conf->privkey_len > 0)
    {
        HTTPC_DEBUG_INFO("Read Privkey(length = %d)\n", conf->privkey_len);
    }

    // to read dh param
    ret = http_tls_read_cert(RM_CERT_MODULE_HTTPS_CLIENT, RM_CERT_TYPE_DH_PARAMS, &conf->dh_param, &conf->dh_param_len);
    if (ret)
    {
        HTTPC_DEBUG_ERR("failed to read dh param\r\n");
        goto err;
    }

    if (conf->dh_param_len > 0)
    {
        HTTPC_DEBUG_INFO("Read DH Param(length = %d)\n", conf->dh_param_len);
    }

    return;

err:

    if (conf->ca)
    {
        vPortFree(conf->ca);
        conf->ca     = NULL;
        conf->ca_len = 0;
    }

    if (conf->cert)
    {
        vPortFree(conf->cert);
        conf->cert     = NULL;
        conf->cert_len = 0;
    }

    if (conf->privkey)
    {
        vPortFree(conf->privkey);
        conf->privkey     = NULL;
        conf->privkey_len = 0;
    }

    if (conf->dh_param)
    {
        vPortFree(conf->dh_param);
        conf->dh_param     = NULL;
        conf->dh_param_len = 0;
    }
}

static fsp_err_t http_client_set_tls_conf (http_client_request_t * request)
{
    fsp_err_t    err       = FSP_SUCCESS;
    int          index     = 0;
    int          alpn_cnt  = -1;
    int          auth_mode = -1;
    int          tls_ver   = -1;
    char       * sni_str   = NULL;
    unsigned int sni_len   = 0;

    // For tls
    if (request->insecure == pdTRUE)
    {
        request->https_conf.incoming_len = HTTPC_MAX_INCOMING_LEN;
        request->https_conf.outgoing_len = HTTPC_MAX_OUTGOING_LEN;

        // auth_mode
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     HTTPC_NVRAM_CONFIG_TLS_AUTH,
                                     &auth_mode);
 #endif
        if (auth_mode != -1)
        {
            if ((auth_mode >= MBEDTLS_SSL_VERIFY_NONE) && (auth_mode < MBEDTLS_SSL_VERIFY_UNSET))
            {
                request->https_conf.auth_mode = (u32_t) auth_mode;
            }
            else
            {
                request->https_conf.auth_mode = MBEDTLS_SSL_VERIFY_NONE;
            }
        }
        else
        {
            request->https_conf.auth_mode = MBEDTLS_SSL_VERIFY_NONE;
        }

        // tls_ver
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     HTTPC_NVRAM_CONFIG_TLS_VER,
                                     &tls_ver);
 #endif
        if (tls_ver != -1)
        {
            if (tls_ver == 0)
            {
                // ONLY_TLS12
                request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
            }
            else if (tls_ver == 1)
            {
                // ONLY_TLS13;
                request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_3;
                request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
            }
            else if (tls_ver == 2)
            {
                // TLS12_13;
                request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
            }
            else
            {
                // ONLY_TLS12
                request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
                request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
            }
        }
        else
        {
            // ONLY_TLS12
            request->https_conf.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
            request->https_conf.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        // Read certificate
        http_client_read_certs(&request->https_conf);

        // sni
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        HTTPC_NVRAM_CONFIG_TLS_SNI,
                                        &sni_str);
 #endif
        if (sni_str != NULL)
        {
            sni_len = strlen(sni_str);

            if ((sni_len > 0) && (sni_len < HTTPC_MAX_SNI_LEN))
            {
                if (request->https_conf.sni != NULL)
                {
                    vPortFree(request->https_conf.sni);
                    request->https_conf.sni = NULL;
                }

                request->https_conf.sni = pvPortMalloc(sni_len + 1);
                if (request->https_conf.sni == NULL)
                {
                    HTTPC_DEBUG_ERR("Failed to allocate SNI(%d)\n", sni_len);
                    err = FSP_ERR_OUT_OF_MEMORY;
                    goto exit;
                }

                bsp_safe_strcpy(request->https_conf.sni, sni_str, sni_len + 1);
                request->https_conf.sni_len = (int) (sni_len + 1);

                HTTPC_DEBUG_INFO("ReadNVRAM SNI = %s\n", request->https_conf.sni);
            }
        }

        // alpn
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &alpn_cnt);
 #endif
        if (alpn_cnt != -1)
        {
            if (alpn_cnt > 0)
            {
                http_client_clear_alpn(&request->https_conf);

                request->https_conf.alpn = pvPortMalloc((size_t) (alpn_cnt + 1) * sizeof(char *));
                if (!request->https_conf.alpn)
                {
                    HTTPC_DEBUG_ERR("Failed to allocate ALPN\n");
                    err = FSP_ERR_OUT_OF_MEMORY;
                    goto exit;
                }

                for (index = 0; index < alpn_cnt; index++)
                {
                    char   nvrName[16] = {0, };
                    char * alpn_str    = NULL;

                    if (index >= HTTPC_MAX_ALPN_CNT)
                    {
                        break;
                    }

                    sprintf(nvrName, "%s%d", HTTPC_NVRAM_CONFIG_TLS_ALPN, index);
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvrName,
                                                    &alpn_str);
 #endif

                    if (alpn_str != NULL)
                    {
                        request->https_conf.alpn[index] = pvPortMalloc(strlen(alpn_str) + 1);
                    }
                    else
                    {
                        HTTPC_DEBUG_ERR("alpn_str = NULL");
                        err = FSP_ERR_OUT_OF_MEMORY;
                        goto exit;
                    }

                    if (!request->https_conf.alpn[index])
                    {
                        HTTPC_DEBUG_ERR("Failed to allocate ALPN#%d(len=%d)\n", index + 1, strlen(alpn_str));
                        http_client_clear_alpn(&request->https_conf);
                        err = FSP_ERR_OUT_OF_MEMORY;
                        goto exit;
                    }

                    request->https_conf.alpn_cnt = index + 1;
                    bsp_safe_strcpy(request->https_conf.alpn[index], alpn_str, strlen(alpn_str) + 1);
                    HTTPC_DEBUG_INFO("ReadNVRAM ALPN#%d = %s\n", request->https_conf.alpn_cnt,
                                     request->https_conf.alpn[index]);
                }

                request->https_conf.alpn[index] = NULL;
            }
        }
    }

exit:
    if (err != FSP_SUCCESS)
    {
        http_client_clear_alpn(&request->https_conf);
    }

    return err;
}

fsp_err_t rm_atcmd_w_run_user_http_client (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_t err = FSP_SUCCESS;

    g_p_at_ctrl = p_at_ctrl;

    err = p_atcmd_https->open(&g_atcmd_https_w0_ctrl, &g_atcmd_https_w0_cfg);
    if (err == FSP_SUCCESS)
    {
        err = rm_atcmd_w_http_client_parse_request(argc, argv, &g_request);
        if (err != FSP_SUCCESS)
        {
            rm_atcmd_w_http_client_display_usage();
        }
        else
        {
            http_client_set_tls_conf(&g_request);

            err = p_atcmd_https->clientSendRequest(&g_atcmd_https_w0_ctrl, &g_request);
            if (err != FSP_SUCCESS)
            {
                HTTPC_DEBUG_ERR("clientSendRequest failed(err = %d)\n", err);
            }
        }
    }

    return err;
}

fsp_err_t rm_atcmd_w_run_user_http_server (atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
{
    fsp_err_t err = FSP_SUCCESS;

    int     index    = 0;
    char ** cur_argv = ++argv;

    if (argc < 1)
    {
        HTTPC_DEBUG_ERR("Invaild parameters\n");

        return FSP_ERR_INVALID_ARGUMENT;
    }

    g_p_at_ctrl = p_at_ctrl;

    for (index = 1; index < argc; index++, cur_argv++)
    {
        // parse operation
        if (strcmp("start", *cur_argv) == 0)
        {
            err = p_atcmd_https->open(&g_atcmd_https_w0_ctrl, &g_atcmd_https_w0_cfg);
            if (err == FSP_SUCCESS)
            {
                http_server_read_certs(&g_tls_https);
                if (g_tls_https.tls_srv_key_len == 0)
                {
                    g_tls_https.p_tls_srv_key   = &g_tls_srv_key[0];
                    g_tls_https.tls_srv_key_len = g_tls_srv_key_len;
                    HTTPC_DEBUG_INFO("Read default Privkey(length = %d)\n", g_tls_https.tls_srv_key_len);
                }

                if (g_tls_https.tls_srv_cert_len == 0)
                {
                    g_tls_https.p_tls_srv_cert   = &g_tls_srv_cert[0];
                    g_tls_https.tls_srv_cert_len = g_tls_srv_cert_len;
                    HTTPC_DEBUG_INFO("Read default Cert(length = %d)\n", g_tls_https.tls_srv_cert_len);
                }

                err = p_atcmd_https->serverStart(&g_atcmd_https_w0_ctrl, &g_tls_https);
            }

            if (err != FSP_SUCCESS)
            {
                HTTPC_DEBUG_ERR("Failed to start http(s)-server (err=%d)\n", err);
            }
            else
            {
                HTTPC_DEBUG_INFO("Start http(s)-server\n");
            }
        }
        else if (strcmp("stop", *cur_argv) == 0)
        {
            err = p_atcmd_https->serverStop(&g_atcmd_https_w0_ctrl);
            if (err == FSP_SUCCESS)
            {
                err = p_atcmd_https->close(&g_atcmd_https_w0_ctrl);
            }

            if (err != FSP_SUCCESS)
            {
                HTTPC_DEBUG_ERR("Failed to stop http(s)-server (err=%d)\n", err);
            }
            else
            {
                HTTPC_DEBUG_INFO("Stop http(s)-server\n");
            }
        }
        else
        {
            HTTPC_DEBUG_ERR("Invaild parameters(%s)\n", *cur_argv);

            return FSP_ERR_INVALID_ARGUMENT;
        }
    }

    return err;
}

#endif                                 /* CFG_WIFI */

/* EOF */
