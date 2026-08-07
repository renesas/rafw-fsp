/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
 #include "FreeRTOS.h"
 #include "custom_config_sdk.h"

 #include <ctype.h>
 #include "rm_https_api.h"
 #include "rm_https_w.h"
 #include "rm_http_client.h"
 #include "lwip/altcp_tcp.h"
 #include "lwip/dns.h"
 #include "lwip/debug.h"
 #include "lwip/mem.h"
 #include "lwip/altcp_tls.h"
 #include "lwip/init.h"
 #include "mbedtls/ssl.h"
 #include "net_common.h"
 #include "net_dns_client.h"
 #include "rm_cert.h"
 #include "rm_atcmd_w_core_ota_update.h"
 #include "rm_atcmd_w_core_ota_common.h"
 #include "rm_atcmd_w_core_ota_http.h"
 #include "rm_atcmd_w_core_ota_mcu_fw.h"
 #include "rm_lwip_w_helper.h"
 #include "rm_vee_flash_w_rrq_nvram.h"
 #ifdef RM_MAP_PERSISTANT_W
  #include "rm_map_persistant_w.h"
 #endif
 #if (SUPPORT_FSP_RM_OTA_W == 1)
  #include "rm_ota_w.h"
 #endif                                /* SUPPORT_FSP_RM_OTA_W */

 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"

static atcmd_w_ota_update_download_t dw_info;
static ip_addr_t             server_addr;
static httpc_connection_t    at_ota_conn_settings = {0, };
static http_client_request_t at_ota_http_request;

 #if (SUPPORT_FSP_RM_OTA_W == 1)
extern const ota_instance_t * p_ota_instance;
 #endif                                /* SUPPORT_FSP_RM_OTA_W */

static fsp_err_t rm_atcmd_w_http_client_parse_uri (unsigned char * uri, size_t len, http_client_request_t * request)
{
    unsigned char * p = NULL;
    unsigned char * q = NULL;

    if (strlen((const char *) uri) > HTTPC_MAX_PATH_LEN)
    {
        ATCMD_W_OTA_ERR("Invalid URL Length.(max = %d)\n", HTTPC_MAX_PATH_LEN);
        goto error;
    }

    memset(request->path, 0x00, HTTPC_MAX_PATH_LEN);
    memcpy(request->path, uri, strlen((const char *) uri));

    p = uri;
    q = (unsigned char *) "http";
    while (len && *q && tolower(*p) == *q)
    {
        ++p;
        ++q;
        --len;
    }

    if (*q)
    {
        ATCMD_W_OTA_ERR("Invalid prefix(http)\n");
        goto error;
    }

    if (len && (tolower(*p) == 's'))
    {
        ++p;
        --len;
        request->insecure = pdTRUE;
        request->port     = HTTPS_SERVER_PORT;
    }
    else
    {
        request->insecure = pdFALSE;
        request->port     = HTTP_SERVER_PORT;
    }

    q = (unsigned char *) "://";
    while (len && *q && tolower(*p) == *q)
    {
        ++p;
        ++q;
        --len;
    }

    if (*q)
    {
        ATCMD_W_OTA_ERR("Invalid uri\n");
        goto error;
    }

    /* p points to beginning of Uri-Host */
    q = p;
    if (len && (*p == '['))
    {
        /* IPv6 address reference */
 #if defined(__SUPPORT_IPV6__)
        ++p;

        while (len && *q != ']')
        {
            ++q;
            --len;
        }

        if (!len || (*q != ']') || (p == q))
        {
            ATCMD_W_OTA_ERR("Invaild URI\n");
            goto error;
        }

        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, p, q - p);
        ++q;
        --len;
 #else

        // not supported ipv6
        ATCMD_W_OTA_ERR("Not supported IPv6\n");
        goto error;
 #endif
    }
    else
    {
        /* IPv4 address or FQDN */
        if (strstr((const char *) q, "@"))
        {
            p = (unsigned char *) strstr((const char *) q, "@");
            ++p;
            len -= (size_t) (p - q);
            q    = p;
        }

        while (len && *q != ':' && *q != '/' && *q != '?')
        {
            *q = (unsigned char) tolower((int) *q);
            ++q;
            --len;
        }

        if (p == q)
        {
            ATCMD_W_OTA_ERR("Invalid hostname\n");
            goto error;
        }

        memset(request->hostname, 0x00, HTTPC_MAX_HOSTNAME_LEN);
        memcpy(request->hostname, (const char *) p, (size_t) (q - p));
    }

    /* check for Uri-Port */
    if (len && (*q == ':'))
    {
        p = ++q;
        --len;

        while (len && isdigit(*q))
        {
            ++q;
            --len;
        }

        if (p < q)
        {
            /* explicit port number given */
            int port = 0;

            while (p < q)
            {
                port = port * 10 + (*p++ - '0');
            }

            request->port = (UINT) port;
        }
    }

    return FSP_SUCCESS;

error:

    return FSP_ERR_INVALID_ARGUMENT;
}

static void atcmd_w_ota_http_client_clear_alpn (httpc_secure_connection_t * conf)
{
    int idx = 0;

    if (conf->alpn)
    {
        for (idx = 0; idx < conf->alpn_cnt; idx++)
        {
            if (conf->alpn[idx])
            {
                ATCMD_W_OTA_FREE(conf->alpn[idx]);
                conf->alpn[idx] = NULL;
            }
        }

        ATCMD_W_OTA_FREE(conf->alpn);
    }

    conf->alpn     = NULL;
    conf->alpn_cnt = 0;
}

static void atcmd_w_ota_http_client_clear_https_conf (httpc_secure_connection_t * conf)
{
    if (conf)
    {
        if (conf->ca)
        {
            ATCMD_W_OTA_FREE(conf->ca);
            conf->ca = NULL;
        }

        conf->ca_len = 0;

        if (conf->cert)
        {
            ATCMD_W_OTA_FREE(conf->cert);
            conf->cert = NULL;
        }

        conf->cert_len = 0;

        if (conf->privkey)
        {
            ATCMD_W_OTA_FREE(conf->privkey);
            conf->privkey = NULL;
        }

        conf->privkey_len = 0;

        if (conf->dh_param)
        {
            ATCMD_W_OTA_FREE(conf->dh_param);
            conf->dh_param = NULL;
        }

        conf->dh_param_len = 0;

        if (conf->sni)
        {
            ATCMD_W_OTA_FREE(conf->sni);
            conf->sni = NULL;
        }

        conf->sni_len = 0;

        atcmd_w_ota_http_client_clear_alpn(conf);

        memset(conf, 0x00, sizeof(httpc_secure_connection_t));

        conf->auth_mode    = ATCMD_W_OTA_HTTPC_TLS_AUTHMODE_DEF;
        conf->incoming_len = ATCMD_W_OTA_HTTPC_DEF_INCOMING_LEN;
        conf->outgoing_len = ATCMD_W_OTA_HTTPC_DEF_OUTGOING_LEN;
    }
}

static int atcmd_w_ota_http_client_read_cert (int module, int type, unsigned char ** out, size_t * outlen)
{
    int              ret    = 0;
    unsigned char  * buf    = NULL;
    size_t           buflen = CERT_MAX_LENGTH;
    rm_cert_format_t format = RM_CERT_FORMAT_NONE;

    buf = ATCMD_W_OTA_MALLOC(buflen);
    if (!buf)
    {
        ATCMD_W_OTA_ERR("Failed to allocate memory(module:%d, type:%d, len:%lu)\r\n",
                        module,
                        type,
                        (long unsigned int) buflen);

        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
    }

    memset(buf, 0x00, buflen);

    ret = RM_CERT_Read(module, type, &format, buf, &buflen);
    if (ret == RM_CERT_ERR_OK)
    {
        *out    = buf;
        *outlen = buflen;

        return ATCMD_W_OTA_SUCCESS;
    }
    else if (ret == RM_CERT_ERR_EMPTY_CERTIFICATE)
    {
        if (buf)
        {
            ATCMD_W_OTA_FREE(buf);
            buf = NULL;
        }

        return ATCMD_W_OTA_SUCCESS;
    }

    if (buf)
    {
        ATCMD_W_OTA_FREE(buf);
        buf = NULL;
    }

    return ATCMD_W_OTA_FAILED;
}

static void atcmd_w_ota_http_client_read_certs (httpc_secure_connection_t * settings)
{
    int ret = 0;

    /* to read ca certificate */
    ret = atcmd_w_ota_http_client_read_cert(RM_CERT_MODULE_OTA,
                                            RM_CERT_TYPE_CA_CERT, 
                                            &settings->ca, 
                                            &settings->ca_len);
    if (ret)
    {
        ATCMD_W_OTA_ERR("failed to read CA cert\r\n");
        goto err;
    }

    /* to read certificate */
    ret = atcmd_w_ota_http_client_read_cert(RM_CERT_MODULE_OTA, 
                                            RM_CERT_TYPE_CERT, 
                                            &settings->cert, 
                                            &settings->cert_len);
    if (ret)
    {
        ATCMD_W_OTA_ERR("failed to read certificate\r\n");
        goto err;
    }

    /* to read private key */
    ret = atcmd_w_ota_http_client_read_cert(RM_CERT_MODULE_OTA,
                                            RM_CERT_TYPE_PRIVATE_KEY,
                                            &settings->privkey,
                                            &settings->privkey_len);
    if (ret)
    {
        ATCMD_W_OTA_ERR("failed to read private key\r\n");
        goto err;
    }

    /* to read dh param */
    ret = atcmd_w_ota_http_client_read_cert(RM_CERT_MODULE_OTA,
                                            RM_CERT_TYPE_DH_PARAMS,
                                            &settings->dh_param,
                                            &settings->dh_param_len);
    if (ret)
    {
        ATCMD_W_OTA_ERR("failed to read dh param\r\n");
        goto err;
    }

    return;

err:

    if (settings->ca)
    {
        ATCMD_W_OTA_FREE(settings->ca);
    }

    if (settings->cert)
    {
        ATCMD_W_OTA_FREE(settings->cert);
    }

    if (settings->privkey)
    {
        ATCMD_W_OTA_FREE(settings->privkey);
    }

    if (settings->dh_param)
    {
        ATCMD_W_OTA_FREE(settings->dh_param);
    }

    settings->ca           = NULL;
    settings->ca_len       = 0;
    settings->cert         = NULL;
    settings->cert_len     = 0;
    settings->privkey      = NULL;
    settings->privkey_len  = 0;
    settings->dh_param     = NULL;
    settings->dh_param_len = 0;
}

UINT atcmd_w_ota_http_client_set_tls_auth_mode (int tls_auth_mode)
{
    if ((tls_auth_mode >= MBEDTLS_SSL_VERIFY_NONE) && (tls_auth_mode < MBEDTLS_SSL_VERIFY_UNSET))
    {
 #ifdef RM_MAP_PERSISTANT_W
        if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                          ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH, tls_auth_mode) != FSP_SUCCESS)
        {
 #endif
        ATCMD_W_OTA_ERR("Failed to set TLS auth_mode\n");

        return ATCMD_W_OTA_FAILED;
    }

    ATCMD_W_OTA_INFO("WriteNVRAM %s.....%d\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH, tls_auth_mode);

    return ATCMD_W_OTA_SUCCESS;
}

ATCMD_W_OTA_INFO("[%s] TLS Authentication is only %d ~ %d\n",
                 __func__,
                 MBEDTLS_SSL_VERIFY_NONE,
                 MBEDTLS_SSL_VERIFY_REQUIRED);

return ATCMD_W_OTA_FAILED;
}

UINT atcmd_w_ota_http_client_get_tls_auth_mode (void)
{
    int tls_auth_mode = ATCMD_W_OTA_HTTPC_TLS_AUTHMODE_DEF;
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_APPCFG,
                                 ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_AUTH,
                                 &tls_auth_mode);
 #endif
    if (tls_auth_mode == -1)
    {
        tls_auth_mode = ATCMD_W_OTA_HTTPC_TLS_AUTHMODE_DEF;
    }

    return tls_auth_mode;
}

UINT atcmd_w_ota_http_client_set_tls_version (int tls_ver)
{
    if ((tls_ver != ATCMD_W_OTA_HTTPS_TLS12) &&
        (tls_ver != ATCMD_W_OTA_HTTPS_TLS13) &&
        (tls_ver != ATCMD_W_OTA_HTTPS_TLS12_13))
    {
        return ATCMD_W_OTA_FAILED;
    }

 #ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                      ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_VER, tls_ver) != FSP_SUCCESS)
    {
        ATCMD_W_OTA_ERR("Failed to set TLS tls_ver\n");

        return ATCMD_W_OTA_FAILED;
    }
    ATCMD_W_OTA_INFO("WriteNVRAM %s.....%d\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_VER, tls_ver);
 #endif

    return ATCMD_W_OTA_SUCCESS;
}

UINT atcmd_w_ota_http_client_get_tls_version (void)
{
    int tls_ver = ATCMD_W_OTA_HTTPC_TLS_VERSION_DEF;
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_APPCFG,
                                 ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_VER,
                                 &tls_ver);
 #endif
    if (tls_ver == -1)
    {
        tls_ver = ATCMD_W_OTA_HTTPC_TLS_VERSION_DEF;
    }

    return tls_ver;
}

UINT atcmd_w_ota_http_client_set_sni (char * sni)
{
    if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG,
                                         ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI, sni) != 0)
    {
        ATCMD_W_OTA_ERR("Failed to set SNI\n");

        return ATCMD_W_OTA_FAILED;
    }

    ATCMD_W_OTA_INFO("WriteNVRAM %s.....%s\n", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI, sni);

    return ATCMD_W_OTA_SUCCESS;
}

size_t atcmd_w_ota_http_client_get_sni (char * sni, size_t buf_len)
{
    char * read_sni = NULL;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                    ENV_GROUP_APPCFG,
                                    ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI,
                                    &read_sni);
 #endif

    if ((read_sni == NULL) || (strlen(read_sni) <= 0))
    {
        return 0;
    }

    if (sni != NULL)
    {
        bsp_safe_strcpy(sni, read_sni, buf_len);
    }

    return strlen(read_sni);
}

UINT atcmd_w_ota_http_client_set_alpn (char * alpn0, char * alpn1, char * alpn2)
{
    int alpn_cnt      = 0;
    char nvr_name[32] = {0, };

    memset(nvr_name, 0, sizeof(nvr_name));
    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 0);
    if (alpn0 != NULL)
    {
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, alpn0);
        alpn_cnt += 1;
        ATCMD_W_OTA_INFO("WriteNVRAM %s.....%s\n", nvr_name, alpn0);
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
 #endif
    }

    memset(nvr_name, 0, sizeof(nvr_name));
    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 1);
    if (alpn1 != NULL)
    {
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, alpn1);
        alpn_cnt += 1;
        ATCMD_W_OTA_INFO("WriteNVRAM %s.....%s\n", nvr_name, alpn1);
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
 #endif
    }

    memset(nvr_name, 0, sizeof(nvr_name));
    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 2);
    if (alpn2 != NULL)
    {
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, alpn2);
        alpn_cnt += 1;
        ATCMD_W_OTA_INFO("WriteNVRAM %s.....%s\n", nvr_name, alpn2);
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
 #endif
    }

    if (alpn_cnt > 0)
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                      alpn_cnt);
 #endif
    }

    return ATCMD_W_OTA_SUCCESS;
}

size_t atcmd_w_ota_http_client_get_alpn0 (char * alpn0, size_t buf_len)
{
    char * read_alpn  = NULL;
    char nvr_name[32] = {0, };
    int alpn_len;

    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 0);
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &read_alpn);
 #endif

    if ((read_alpn == NULL) || (strlen(read_alpn) <= 0))
    {
        return 0;
    }

    alpn_len = strlen(read_alpn);

    if (alpn0 != NULL)
    {
        bsp_safe_strcpy(alpn0, read_alpn, buf_len);
    }

    return alpn_len;
}

size_t atcmd_w_ota_http_client_get_alpn1 (char * alpn1, size_t buf_len)
{
    char * read_alpn  = NULL;
    char nvr_name[32] = {0, };
    int alpn_len;

    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 1);
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &read_alpn);
 #endif

    if ((read_alpn == NULL) || (strlen(read_alpn) <= 0))
    {
        return 0;
    }

    alpn_len = strlen(read_alpn);

    if (alpn1 != NULL)
    {
        bsp_safe_strcpy(alpn1, read_alpn, buf_len);
    }

    return alpn_len;
}

size_t atcmd_w_ota_http_client_get_alpn2 (char * alpn2, size_t buf_len)
{
    char * read_alpn  = NULL;
    char nvr_name[32] = {0, };
    int alpn_len;

    sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, 2);
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name, &read_alpn);
 #endif

    if ((read_alpn == NULL) || (strlen(read_alpn) <= 0))
    {
        return 0;
    }

    alpn_len = strlen(read_alpn);

    if (alpn2 != NULL)
    {
        bsp_safe_strcpy(alpn2, read_alpn, buf_len);
    }

    return alpn_len;
}

void atcmd_w_ota_http_client_del_all_alpn (void)
{
    char nvr_name[32] = {0, };

    for (int i = 0; i < ATCMD_W_OTA_HTTPC_MAX_ALPN_CNT; i++)
    {
        memset(nvr_name, 0, sizeof(nvr_name));
        sprintf(nvr_name, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, i);
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvr_name);
 #endif
    }
}

UINT atcmd_w_ota_http_client_get_download_status (void)
{
    return dw_info.download_status;
}

void atcmd_w_ota_http_client_set_download_status (UINT status)
{
    dw_info.download_status = status;
}

UINT atcmd_w_ota_http_client_get_result (void)
{
    return dw_info.httpc_result;
}

err_t atcmd_w_ota_update_httpc_cb_headers_done_fn (httpc_state_t * connection,
                                                   void          * arg,
                                                   struct pbuf   * hdr,
                                                   u16_t           hdr_len,
                                                   u32_t           content_len)
{
    RA6W1_UNUSED_ARG(connection);
    RA6W1_UNUSED_ARG(arg);

    atcmd_w_ota_update_evt_send(ATCMD_W_OTA_EVT_RECEIVE);

    ATCMD_W_OTA_DBG("\n[%s:%d] hdr_len : %d, content_len : %ld\n", __func__, __LINE__, hdr_len, content_len);

    if ((hdr == NULL) || (hdr_len <= 0))
    {
        return ERR_UNKNOWN;
    }

    if (content_len == HTTPC_CONTENT_LEN_INVALID)
    {
        return ERR_NOT_FOUND;
    }

    if (strstr(hdr->payload, "200 OK") == NULL)
    {
        return ERR_NOT_FOUND;
    }

    dw_info.content_length     = content_len;
    dw_info.write.total_length = content_len;

    return ERR_OK;
}

void atcmd_w_ota_update_httpc_cb_result_fn (void         * arg,
                                            httpc_result_t httpc_result,
                                            u32_t          rx_content_len,
                                            u32_t          srv_res,
                                            err_t          err)
{
 #if (SUPPORT_FSP_RM_OTA_W == 1)
    UINT sflash_addr;
 #endif

    RA6W1_UNUSED_ARG(arg);
    RA6W1_UNUSED_ARG(rx_content_len);
    RA6W1_UNUSED_ARG(srv_res);

    ATCMD_W_OTA_DBG("[%s]httpc_result = %d\n", __func__, httpc_result);

    if ((httpc_result != HTTPC_RESULT_OK) && (httpc_result != HTTPC_RESULT_LOCAL_ABORT))
    {
        ATCMD_W_OTA_ERR("\n[%s] Failed to get response(%d)\n", __func__, httpc_result);
    }

    if (httpc_result == HTTPC_RESULT_OK)
    {
        if (dw_info.update_type == ATCMD_W_OTA_TYPE_RTOS)
        {
 #if (SUPPORT_FSP_RM_OTA_W == 1)
            p_ota_instance->p_api->getAddr(p_ota_instance->p_ctrl, RM_OTA_W_NEW_ADDR, dw_info.update_type, (uint32_t *) &sflash_addr);

            if ((sflash_addr != RM_OTA_W_STOR_UNKNOWN_ADDR) &&
                (p_ota_instance->p_api->cert(p_ota_instance->p_ctrl, RM_OTA_W_VALIDATE_TYPE_IMG_CRC, sflash_addr) == RM_OTA_W_SUCCESS))
            {
                dw_info.download_status = ATCMD_W_OTA_SUCCESS;
            }
            else
            {
                dw_info.download_status = ATCMD_W_OTA_ERROR_CRC;
            }
 #endif
        }
    }
    else if (httpc_result == HTTPC_RESULT_ERR_UNKNOWN)
    {
        dw_info.download_status = ATCMD_W_OTA_FAILED;
    }
    else if (httpc_result == HTTPC_RESULT_ERR_CONNECT)
    {
        dw_info.download_status = ATCMD_W_OTA_NOT_CONNECTED;
    }
    else if (httpc_result == HTTPC_RESULT_ERR_HOSTNAME)
    {
        dw_info.download_status = ATCMD_W_OTA_ERROR_URL;
    }
    else if (httpc_result == HTTPC_RESULT_ERR_CLOSED)
    {
        dw_info.download_status = ATCMD_W_OTA_NOT_CONNECTED;
    }
    else if (httpc_result == HTTPC_RESULT_ERR_TIMEOUT)
    {
        if (atcmd_w_ota_update_get_download_progress(dw_info.update_type))
        {
            dw_info.download_status = ATCMD_W_OTA_FAILED;
        }
        else if (dw_info.content_length == 0)
        {
            dw_info.download_status = ATCMD_W_OTA_NOT_CONNECTED;
        }
        else
        {
            dw_info.download_status = ATCMD_W_OTA_NOT_FOUND;
        }
    }
    else if (httpc_result == HTTPC_RESULT_ERR_SVR_RESP)
    {
        dw_info.download_status = ATCMD_W_OTA_NOT_FOUND;
    }
    else if (httpc_result == HTTPC_RESULT_ERR_MEM)
    {
        dw_info.download_status = ATCMD_W_OTA_MEM_ALLOC_FAILED;
    }
    else if (httpc_result == HTTPC_RESULT_LOCAL_ABORT)
    {
        if (err == ERR_NOT_FOUND)
        {
            dw_info.download_status = ATCMD_W_OTA_NOT_FOUND;
        }
        else
        {
            if (dw_info.version_check != ATCMD_W_OTA_SUCCESS)
            {
                dw_info.download_status = dw_info.version_check;
            }
            else
            {
                dw_info.download_status = ATCMD_W_OTA_FAILED;
            }
        }
    }
    else if (httpc_result == HTTPC_RESULT_ERR_CONTENT_LEN)
    {
        dw_info.download_status = ATCMD_W_OTA_ERROR_SIZE;
    }
    else
    {
        dw_info.download_status = ATCMD_W_OTA_FAILED;
    }

    if (dw_info.update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        if (dw_info.download_status == ATCMD_W_OTA_SUCCESS)
        {
            /* MCU FW header generation */
            atcmd_w_ota_update_gen_mcu_header(dw_info.content_length);
        }
        else
        {
            /* In case of failure, the downloaded MCU FW is deleted */
            atcmd_w_ota_update_erase_mcu_fw();
        }
    }

    if (dw_info.write.buffer != NULL)
    {
        ATCMD_W_OTA_FREE(dw_info.write.buffer);
        dw_info.write.buffer = NULL;
    }

    dw_info.httpc_result  = httpc_result;
    dw_info.version_check = ATCMD_W_OTA_NOT_FOUND;
    atcmd_w_ota_update_evt_send(ATCMD_W_OTA_EVT_FINISH);
}

static err_t atcmd_w_ota_update_httpc_cb_recv_fn (void * arg, struct altcp_pcb * tpcb, struct pbuf * p, err_t err)
{
    UINT status;
    UINT progress     = 0;
    UCHAR * p_payload = NULL;
    UINT p_len        = 0;

    RA6W1_UNUSED_ARG(arg);
    RA6W1_UNUSED_ARG(tpcb);

    atcmd_w_ota_update_evt_send(ATCMD_W_OTA_EVT_RECEIVE);

    if (p == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] pbuf is null\n", __func__);

        return ERR_BUF;
    }
    else if (err != ERR_OK)
    {
        ATCMD_W_OTA_ERR("[%s] Received failed(err=%d)\n", __func__, err);

        return err;
    }
    else
    {
        if (atcmd_w_ota_update_get_proc_state() != ATCMD_W_OTA_STATE_PROGRESS)
        {
            ATCMD_W_OTA_DBG("[%s] state != ATCMD_W_OTA_STATE_PROGRESS, state = %d\n",
                            __func__,
                            atcmd_w_ota_update_get_proc_state());

            return ERR_VAL;
        }

        if (dw_info.version_check == ATCMD_W_OTA_NOT_FOUND)
        {
            if (atcmd_w_ota_update_check_version(dw_info.update_type, (UCHAR *) p->payload,
                                                 (UINT) p->len) == ATCMD_W_OTA_SUCCESS)
            {
                dw_info.version_check = ATCMD_W_OTA_SUCCESS;
            }
            else
            {
                /* Version mismatch */
                dw_info.version_check = ATCMD_W_OTA_VERSION_INCOMPATI;

                return ERR_VAL;
            }

            if (dw_info.version_check == ATCMD_W_OTA_SUCCESS)
            {
                if (atcmd_w_ota_update_check_available_size(dw_info.update_type,
                                                            dw_info.content_length) != ATCMD_W_OTA_SUCCESS)
                {
                    dw_info.version_check = ATCMD_W_OTA_ERROR_SIZE;

                    return ERR_VAL;
                }

                atcmd_w_ota_update_set_download_progress(dw_info.update_type, 1); // Just for printf
            }
        }

        while (p != NULL)
        {
            p_payload                = (UCHAR *) p->payload;
            p_len                    = p->len;
            dw_info.received_length += p_len;
            status                   = atcmd_w_ota_update_buffer_write_flash(&dw_info.write, p_payload, p_len);
            if (status != ATCMD_W_OTA_SUCCESS)
            {
                ATCMD_W_OTA_ERR("[%s] Failed to write data to sflash(0x%02x)\n", __func__, status);

                return ERR_VAL;
            }

            p = p->next;
        }

        if ((dw_info.received_length > 0) && (dw_info.content_length > 0))
        {
            progress = (dw_info.received_length * 100) / dw_info.content_length;

 #ifdef ENABLE_ATCMD_W_OTA_DBG
            if (progress != atcmd_w_ota_update_get_download_progress(dw_info.update_type))
 #else
            if (((progress == 0) || ((progress % 10) == 0)) &&
                (progress != atcmd_w_ota_update_get_download_progress(dw_info.update_type)))
 #endif                                // ENABLE_ATCMD_W_OTA_DBG
            {
                ATCMD_W_OTA_INFO("\r   >> HTTP(s)-Client Downloading... %d %% (%d/%d bytes)%s",
                                 progress,
                                 dw_info.received_length,
                                 dw_info.content_length,
                                 progress == 100 ? "\n" : " ");
            }
        }
        else
        {
            progress = 0;
        }

        if (dw_info.version_check == ATCMD_W_OTA_SUCCESS)
        {
            atcmd_w_ota_update_set_download_progress(dw_info.update_type, progress);
        }
    }

    return ERR_OK;
}

UINT atcmd_w_ota_update_http_client_request (atcmd_w_ota_update_proc_t * at_ota_proc)
{
    UINT status    = ATCMD_W_OTA_SUCCESS;
    err_t error    = ERR_OK;
    UINT tls_ver   = 0;
    UINT sni_len   = 0;
    char * sni_str = NULL;
    int index      = 0;
    int alpn_cnt   = 0;

    if (at_ota_proc == NULL)
    {
        ATCMD_W_OTA_ERR("[%s] Parameter is null\n", __func__);

        return ATCMD_W_OTA_FAILED;
    }

    memset(&server_addr, 0, sizeof(ip_addr_t));
    atcmd_w_ota_http_client_clear_https_conf(&(at_ota_conn_settings.tls_settings));
    memset(&at_ota_conn_settings, 0, sizeof(httpc_connection_t));
    memset(&at_ota_http_request, 0, sizeof(http_client_request_t));
    if (strlen(at_ota_proc->url) > ATCMD_W_OTA_HTTP_URL_LEN)
    {
        ATCMD_W_OTA_ERR("[%s] Invalid url\n", __func__);

        return ATCMD_W_OTA_ERROR_URL;
    }

    ATCMD_W_OTA_DBG("[%s]parse url(len=%d) = %s\n", __func__, strlen(at_ota_proc->url), at_ota_proc->url);
    error = rm_atcmd_w_http_client_parse_uri((unsigned char *) at_ota_proc->url,
                                             strlen(at_ota_proc->url),
                                             &at_ota_http_request);
    if (error != ERR_OK)
    {
        ATCMD_W_OTA_ERR("[%s]Failed to parse url(err=%d)\n", __func__, error);

        return ATCMD_W_OTA_ERROR_URL;
    }

    /****************************************************/
    /* Initialize at_ota_update download information */
    /****************************************************/
    dw_info.update_type        = at_ota_proc->update_type;
    dw_info.httpc_result       = HTTPC_RESULT_OK;
    dw_info.received_length    = 0;
    dw_info.write.total_length = 0;
    dw_info.write.length       = 0;
    dw_info.write.offset       = 0;
    if (dw_info.write.buffer != NULL)
    {
        ATCMD_W_OTA_FREE(dw_info.write.buffer);
        dw_info.write.buffer = NULL;
    }

    dw_info.download_status = ATCMD_W_OTA_SUCCESS;
    dw_info.version_check   = ATCMD_W_OTA_NOT_FOUND;
    dw_info.content_length  = 0;
    dw_info.received_length = 0;

    if (dw_info.update_type == ATCMD_W_OTA_TYPE_MCU_FW)
    {
        dw_info.write.sflash_addr = atcmd_w_ota_update_get_new_sflash_addr(dw_info.update_type);
        dw_info.write.offset     += ATCMD_W_OTA_MCU_FW_HEADER_SIZE;
    }
    else
    {
        dw_info.write.sflash_addr = atcmd_w_ota_update_get_new_sflash_addr(dw_info.update_type);
    }

    /******************************************/
    /* Initialize http connection settings */
    /******************************************/
    at_ota_conn_settings.use_proxy = 0;
    bsp_safe_strcpy((char *) (at_ota_conn_settings.method), "GET", sizeof(at_ota_conn_settings.method));
    at_ota_conn_settings.altcp_allocator = NULL;
    at_ota_conn_settings.headers_done_fn = atcmd_w_ota_update_httpc_cb_headers_done_fn;
    at_ota_conn_settings.result_fn       = atcmd_w_ota_update_httpc_cb_result_fn;
    at_ota_conn_settings.insecure        = at_ota_http_request.insecure;

    if (at_ota_conn_settings.insecure == pdTRUE)
    {
        atcmd_w_ota_http_client_clear_https_conf(&at_ota_conn_settings.tls_settings);

        /* TLS buffer */
        at_ota_conn_settings.tls_settings.incoming_len = ATCMD_W_OTA_HTTPC_MAX_INCOMING_LEN;
        at_ota_conn_settings.tls_settings.outgoing_len = ATCMD_W_OTA_HTTPC_DEF_OUTGOING_LEN;

        /* AUTH MODE */
        at_ota_conn_settings.tls_settings.auth_mode = atcmd_w_ota_http_client_get_tls_auth_mode();

        /* TLS Version */
        tls_ver = atcmd_w_ota_http_client_get_tls_version();
        if (tls_ver == 0)
        {
            // ONLY_TLS12
            at_ota_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
            at_ota_conn_settings.tls_settings.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
        }
        else if (tls_ver == 1)
        {
            // ONLY_TLS13;
            at_ota_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_3;
            at_ota_conn_settings.tls_settings.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
        }
        else if (tls_ver == 2)
        {
            // TLS12_13;
            at_ota_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
            at_ota_conn_settings.tls_settings.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_3;
        }
        else
        {
            // ONLY_TLS12
            at_ota_conn_settings.tls_settings.tls_ver_min = MBEDTLS_SSL_VERSION_TLS1_2;
            at_ota_conn_settings.tls_settings.tls_ver_max = MBEDTLS_SSL_VERSION_TLS1_2;
        }

        /* Cert */
        atcmd_w_ota_http_client_read_certs(&at_ota_conn_settings.tls_settings);

        /* SNI */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_SNI,
                                        &sni_str);
 #endif
        if (sni_str != NULL)
        {
            sni_len = strlen(sni_str);

            if ((sni_len > 0) && (sni_len < ATCMD_W_OTA_HTTPC_MAX_SNI_LEN))
            {
                if (at_ota_conn_settings.tls_settings.sni != NULL)
                {
                    ATCMD_W_OTA_FREE(at_ota_conn_settings.tls_settings.sni);
                }

                at_ota_conn_settings.tls_settings.sni = ATCMD_W_OTA_MALLOC(sni_len + 1);
                if (at_ota_conn_settings.tls_settings.sni == NULL)
                {
                    ATCMD_W_OTA_ERR("[%s]Failed to allocate SNI(%ld)\n", __func__, (long int) sni_len);

                    return ATCMD_W_OTA_MEM_ALLOC_FAILED;
                }

                at_ota_conn_settings.tls_settings.sni_len = (int) (sni_len + 1);
                bsp_safe_strcpy(at_ota_conn_settings.tls_settings.sni, sni_str,
                                 (size_t) at_ota_conn_settings.tls_settings.sni_len);
            }
        }

        /* ALPN */
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &alpn_cnt);
 #endif
        if (alpn_cnt != -1)
        {
            if (alpn_cnt > 0)
            {
                atcmd_w_ota_http_client_clear_alpn(&at_ota_conn_settings.tls_settings);

                at_ota_conn_settings.tls_settings.alpn = ATCMD_W_OTA_MALLOC((alpn_cnt + 1) * sizeof(char *));
                if (!at_ota_conn_settings.tls_settings.alpn)
                {
                    ATCMD_W_OTA_ERR("[%s]Failed to allocate ALPN\n", __func__);

                    return ATCMD_W_OTA_MEM_ALLOC_FAILED;
                }

                for (index = 0; index < alpn_cnt; index++)
                {
                    char nvrName[32]  = {0, };
                    char * alpn_str   = NULL;
                    size_t alpn_alloc = 0;

                    if (index >= ATCMD_W_OTA_HTTPC_MAX_ALPN_CNT)
                    {
                        break;
                    }

                    sprintf(nvrName, "%s%d", ATCMD_W_OTA_HTTPC_NVRAM_CONFIG_TLS_ALPN, index);
 #ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvrName,
                                                    &alpn_str);
 #endif
                    if (alpn_str != NULL)
                    {
                        alpn_alloc = strlen(alpn_str) + 1;
                        at_ota_conn_settings.tls_settings.alpn[index] = ATCMD_W_OTA_MALLOC(alpn_alloc);
                    }
                    else
                    {
                        ATCMD_W_OTA_ERR("alpn_str == NULL");

                        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
                    }

                    if (!at_ota_conn_settings.tls_settings.alpn[index])
                    {
                        ATCMD_W_OTA_ERR("[%s]Failed to allocate ALPN#%d(len=%d)\n",
                                        __func__,
                                        index + 1,
                                        strlen(alpn_str));
                        atcmd_w_ota_http_client_clear_alpn(&at_ota_conn_settings.tls_settings);

                        return ATCMD_W_OTA_MEM_ALLOC_FAILED;
                    }

                    at_ota_conn_settings.tls_settings.alpn_cnt = index + 1;
                    bsp_safe_strcpy(at_ota_conn_settings.tls_settings.alpn[index], alpn_str, alpn_alloc);
                    ATCMD_W_OTA_INFO("[%s]ReadNVRAM ALPN#%d = %s\n",
                                     __func__,
                                     at_ota_conn_settings.tls_settings.alpn_cnt,
                                     at_ota_conn_settings.tls_settings.alpn[index]);
                }

                at_ota_conn_settings.tls_settings.alpn[index] = NULL;
            }
        }
    }

    if (is_in_valid_ip_class((char *) at_ota_http_request.hostname))
    {
        ip4addr_aton((const char *) (at_ota_http_request.hostname), (ip4_addr_t *) &server_addr);
        error = httpc_get_file(&server_addr,
                               (u16_t) (at_ota_http_request.port),
                               at_ota_proc->url,
                               &at_ota_conn_settings,
                               atcmd_w_ota_update_httpc_cb_recv_fn,
                               NULL,
                               NULL);
    }
    else
    {
        error = httpc_get_file_dns((const char *) &(at_ota_http_request.hostname[0]),
                                   (u16_t) (at_ota_http_request.port),
                                   (const char *) &(at_ota_http_request.path[0]),
                                   &at_ota_conn_settings,
                                   atcmd_w_ota_update_httpc_cb_recv_fn,
                                   NULL,
                                   NULL);
    }

    if (error != ERR_OK)
    {
        ATCMD_W_OTA_ERR("[%s]http-client error = %d\n", __func__, error);
        status = ATCMD_W_OTA_FAILED;
    }

    return status;
}

#endif                                 /* CFG_WIFI */

/* EOF */
