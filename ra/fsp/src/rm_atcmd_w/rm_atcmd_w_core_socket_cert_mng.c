/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"
#if CFG_WIFI
#include "rm_atcmd_w_core_socket_cert_mng.h"

#include "rm_wifi_helper.h"
#include "rm_atcmd_w_core_socket_internal.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#endif /* CFG_PMGR */

#undef  ENABLE_ATCMD_CM_DBG_INFO
#undef  ENABLE_ATCMD_CM_DBG_ERR

#define ATCMD_CM_DBG    printf

#if defined (ENABLE_ATCMD_CM_DBG_INFO)
#define ATCMD_CM_INFO(fmt, ...)   \
    ATCMD_CM_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_CM_INFO(...)        do {} while (0)
#endif  // (ENABLE_ATCMD_CM_DBG_INFO)

#if defined (ENABLE_ATCMD_CM_DBG_ERR)
#define ATCMD_CM_ERR(fmt, ...)    \
    ATCMD_CM_DBG("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define ATCMD_CM_ERR(...)         do {} while (0)
#endif // (ENABLE_ATCMD_CM_DBG_ERR)

void * (*atcmd_cm_malloc)(size_t n) = pvPortMalloc;
void (*atcmd_cm_free)(void * ptr) = vPortFree;

atcmd_cm_cert_info_t * g_atcmd_cm_cert_info = NULL;

void atcmd_cm_set_malloc_free(void * (*malloc_func)(size_t), void (*free_func)(void *))
{
    atcmd_cm_malloc = malloc_func;
    atcmd_cm_free = free_func;

    return ;
}

void atcmd_cm_display_cert_info(atcmd_cm_cert_info_t * cert, unsigned int addr)
{
    RA6W1_UNUSED_ARG(addr);

    if (cert->flag == ATCMD_CM_INIT_FLAG)
    {
        ATCMD_CM_INFO("atcmd_cm_cert_info_t: %p\n", cert);
        ATCMD_CM_INFO("* Size: %d\n", sizeof(atcmd_cm_cert_info_t));
        ATCMD_CM_INFO("* Name: %s(%d)\n", cert->name, strlen(cert->name));
        ATCMD_CM_INFO("* Type: %d\n", cert->type);
        ATCMD_CM_INFO("* Sequence: %d\n", cert->seq);
        ATCMD_CM_INFO("* Format: %d\n", cert->format);
        ATCMD_CM_INFO("* Cert Length: %d\n", cert->cert_len);
        ATCMD_CM_INFO("* sFlash addr: 0x%x\n", addr);
    }
    else
    {
        ATCMD_CM_INFO("Not init atcmd_cm_cert_info_t\n");
    }

    return ;
}

void atcmd_cm_display_info()
{
    int idx = 0;
    int status = RRQ_APP_SUCCESS;

    atcmd_cm_cert_t * cert = NULL;

    ATCMD_CM_INFO("sizeof(atcmd_cm_cert_t): %d\n", sizeof(atcmd_cm_cert_t));

    cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

    if (!cert)
    {
        ATCMD_CM_INFO("Failed to allocate memory for cert(%d)\n", sizeof(atcmd_cm_cert_t));
        return ;
    }

    #if defined (ENABLE_ATCMD_CM_DBG_INFO)

    if (g_atcmd_cm_cert_info)
    {
        ATCMD_CM_INFO("Cert info(%p:%d) * %d\n", g_atcmd_cm_cert_info,
                      sizeof(atcmd_cm_cert_info_t), ATCMD_CM_MAX_CERT_NUM);

        for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
        {
            atcmd_cm_display_cert_info(&(g_atcmd_cm_cert_info[idx]), g_atcmd_cm_cert_addr_list[idx]);
        }
    }

    #endif // ENABLE_ATCMD_CM_DBG_INFO

    ATCMD_CM_INFO("Cert Address\n");

    for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
    {
        atcmd_cm_init_cert_t(cert);

        ATCMD_CM_INFO("#%d. certificate(0x%x)\n", idx + 1, g_atcmd_cm_cert_addr_list[idx]);

        status = atcmd_cm_read_cert_by_idx(idx, cert);

        if (status)
        {
            ATCMD_CM_INFO("Failed to read certificate(0x%x)\n", status);
            continue;
        }
        else     // suceess
        {
            atcmd_cm_display_cert_info(&cert->info, g_atcmd_cm_cert_addr_list[idx]);
        }
    }

    if (cert)
    {
        atcmd_cm_free(cert);
    }

    return ;
}

void atcmd_cm_init_cert_info_t(atcmd_cm_cert_info_t * cert)
{
    memset(cert, 0x00, sizeof(atcmd_cm_cert_info_t));
    cert->flag = ATCMD_CM_INIT_FLAG;
}

void atcmd_cm_init_cert_t(atcmd_cm_cert_t * cert)
{
    memset(cert, 0x00, sizeof(atcmd_cm_cert_t));

    atcmd_cm_init_cert_info_t(&cert->info);
}

int atcmd_cm_init_cert_info()
{
    int status = RRQ_APP_SUCCESS;

    int idx = 0;
    atcmd_cm_cert_t * cert = NULL;
    UINT32 addr = 0x00;

    #ifndef RRQ61X_OSPI_W_ENABLED
    int offset = 0;
    size_t w;
    size_t ad_ret = 0;
    #endif //!RRQ61X_OSPI_W_ENABLED

    size_t cert_info_len = sizeof(atcmd_cm_cert_info_t) * ATCMD_CM_MAX_CERT_NUM;

    if (g_atcmd_cm_cert_info)
    {
        //Already initialized.
        return status;
    }

    cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

    if (!cert)
    {
        ATCMD_CM_ERR("Failed to allocate memory for cert(%d)\n", sizeof(atcmd_cm_cert_t));
        return RRQ_APP_NOT_CREATED;

    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        status = RM_PMGR_W_user_rtm_get(ATCMD_CM_INFO_NAME,
                                  (unsigned char **)&g_atcmd_cm_cert_info);

        if (status == 0)
        {
            status = RM_PMGR_W_user_rtm_pool_alloc(ATCMD_CM_INFO_NAME,
                                           (VOID **)&g_atcmd_cm_cert_info,
                                           cert_info_len, 100);

            if (status)
            {
                ATCMD_CM_ERR("Failed to allocate rtm memory(0x%x,%d)\n", status, cert_info_len);
                return status;
            }

            memset(g_atcmd_cm_cert_info, 0x00, cert_info_len);
        }

        status = RRQ_APP_SUCCESS;
    }
    else
#endif /* CFG_PMGR */
    {
        g_atcmd_cm_cert_info = atcmd_cm_malloc(cert_info_len);

        if (!g_atcmd_cm_cert_info)
        {
            ATCMD_CM_ERR("Failed to allocate memory(%d)\n", cert_info_len);
            status = RRQ_APP_NOT_CREATED;
            goto end;
        }

        memset(g_atcmd_cm_cert_info, 0x00, cert_info_len);
    }

    #ifndef RRQ61X_OSPI_W_ENABLED
    ad_flash_init();
    #endif //!RRQ61X_OSPI_W_ENABLED

    for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
    {
        addr = g_atcmd_cm_cert_addr_list[idx];

        ATCMD_CM_INFO("addr(0x%x)\n", addr);

        #ifdef RRQ61X_OSPI_W_ENABLED

        if (util_sflash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t)) == pdTRUE)
        {
            if (cert->info.flag != ATCMD_CM_INIT_FLAG)
            {
                ATCMD_CM_INFO("Init sflash memory(0x%x)\n", addr);

                atcmd_cm_init_cert_t(cert);

                if (pdTRUE != rm_wifi_util_sflash_write(addr, (char *)cert, sizeof(atcmd_cm_cert_t)))
                {
                    ATCMD_CM_ERR("Failed to init cert\n");
                }

            }

            memcpy(&(g_atcmd_cm_cert_info[idx]), &cert->info, sizeof(atcmd_cm_cert_info_t));

            atcmd_cm_display_cert_info(&(g_atcmd_cm_cert_info[idx]), addr);

            status = RRQ_APP_SUCCESS;
        }
        else
        {
            status = RRQ_APP_NOT_SUCCESSFUL;
        }

        #else /////////////////////////////////////////////////////////////////////////

        ad_ret = ad_flash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t));

        if (ad_ret == sizeof(atcmd_cm_cert_t))
        {
            if (cert->info.flag != ATCMD_CM_INIT_FLAG)
            {
                ATCMD_CM_INFO("Init sflash memory(%p)\n", addr);

                atcmd_cm_init_cert_t(cert);

                offset = ad_flash_update_possible(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t));

                if (offset == sizeof(atcmd_cm_cert_t))
                {
                    /* same content existing, no need to write to flash */
                    status = RRQ_APP_SUCCESS;
                }
                else
                {
                    if (offset >= 0)
                    {
                        /* no need to erase flash */
                    }
                    else
                    {
                        /* offset < 0, erasing flash is needed */
                        ad_flash_erase_region(addr, sizeof(atcmd_cm_cert_t));
                        offset = 0;
                    }

                    w = offset + ad_flash_write(addr + offset, cert + offset, sizeof(atcmd_cm_cert_t) - offset);

                    /* check that the intended content-size is actually written in flash */
                    if (w != sizeof(atcmd_cm_cert_t))
                    {
                        ATCMD_CM_ERR("Failed to init cert\n");
                    }
                }
            }

            memcpy(&(g_atcmd_cm_cert_info[idx]), &cert->info, sizeof(atcmd_cm_cert_info_t));

            atcmd_cm_display_cert_info(&(g_atcmd_cm_cert_info[idx]), addr);

            status = RRQ_APP_SUCCESS;
        }
        else
        {
            status = RRQ_APP_NOT_SUCCESSFUL;
        }

        #endif //RRQ61X_OSPI_W_ENABLED
    }

    atcmd_cm_display_info();

end:

    if (cert)
    {
        atcmd_cm_free(cert);
    }

    return status;
}

int atcmd_cm_deinit_cert_info()
{
    if (!g_atcmd_cm_cert_info)
    {
        return RRQ_APP_SUCCESS;
    }

#if CFG_PMGR
    if (RM_PMGR_W_dpm_is_enabled())
    {
        RM_PMGR_W_user_rtm_free(ATCMD_CM_INFO_NAME);
    }
    else
#endif /* CFG_PMGR */
    {
        atcmd_cm_free(g_atcmd_cm_cert_info);
    }

    g_atcmd_cm_cert_info = NULL;

    return RRQ_APP_SUCCESS;
}

int atcmd_cm_set_cert(char * name, unsigned char type, unsigned char seq,
                      unsigned char format, char * in, size_t inlen)
{
    int status = RRQ_APP_SUCCESS;
    int idx = 0;
    atcmd_cm_cert_t * cert = NULL;

    // Check validation
    if (name == NULL || strlen(name) >= ATCMD_CM_MAX_NAME)
    {
        ATCMD_CM_ERR("Invalid Name\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (type != ATCMD_CM_CERT_TYPE_CA_CERT && type != ATCMD_CM_CERT_TYPE_CERT)
    {
        ATCMD_CM_ERR("Invalid Type\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (   (type == ATCMD_CM_CERT_TYPE_CERT)
            && (seq != ATCMD_CM_CERT_SEQ_CERT && seq != ATCMD_CM_CERT_SEQ_KEY))
    {
        ATCMD_CM_ERR("Invalid Sequence number\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (format != ATCMD_CM_CERT_FORMAT_DER && format != ATCMD_CM_CERT_FORMAT_PEM)
    {
        ATCMD_CM_ERR("Invalid format\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (inlen > ATCMD_CM_MAX_CERT_BODY)
    {
        ATCMD_CM_ERR("Invalid Certificate Length(Max:%d)\n", ATCMD_CM_MAX_CERT_BODY);
        return RRQ_APP_INVALID_PARAMETERS;
    }

    idx = atcmd_cm_find_idx(name, type, seq);

    if (idx > -1)
    {
        ATCMD_CM_INFO("Already existed certificate\n");
        return RRQ_APP_DUPLICATED_ENTRY;
    }

    idx = atcmd_cm_find_empty_idx();

    if (idx < 0)
    {
        ATCMD_CM_INFO("Not enough space\n");
        return RRQ_APP_OVERFLOW;
    }

    cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

    if (!cert)
    {
        ATCMD_CM_ERR("Failed to allocate memory(%d)\n", sizeof(atcmd_cm_cert_t));
        return RRQ_APP_NOT_CREATED;
    }

    atcmd_cm_init_cert_t(cert);

    bsp_safe_strcpy(cert->info.name, name, ATCMD_CM_MAX_NAME);
    cert->info.type = type;
    cert->info.seq = seq;
    cert->info.format = format;
    memcpy(cert->cert, in, inlen);
    cert->info.cert_len = inlen;

    status = atcmd_cm_write_cert_by_idx(idx, cert);

    if (status)
    {
        ATCMD_CM_ERR("Failed to write certificate(%d)\n", idx);
        goto end;
    }
    else
    {
        memcpy(&(g_atcmd_cm_cert_info[idx]), &(cert->info), sizeof(atcmd_cm_cert_info_t));
    }

end:

    if (cert)
    {
        atcmd_cm_free(cert);
    }

    return status;
}

int atcmd_cm_get_cert_len(char * name, unsigned char type, unsigned char seq, size_t * outlen)
{
    int ret = 0;
    int idx_list[ATCMD_CM_MAX_CERT_NUM] = {0x00,};
    int idx_num = 0;
    int idx = 0;
    int num = 0;
    size_t total_len = 0;

    ATCMD_CM_INFO("To get certificate(%s:%d)\n", name, strlen(name));

    *outlen = 0;

    // Find certificate by name
    ret = atcmd_cm_find_idx_list(name, idx_list, &idx_num);

    if (ret || idx_num <= 0)
    {
        ATCMD_CM_ERR("Failed to find certificate\n");
        return RRQ_APP_NOT_FOUND;
    }

    // Calculate total length
    for (num = 0 ; num < idx_num ; num++)
    {
        idx = idx_list[num];

        atcmd_cm_display_cert_info(&g_atcmd_cm_cert_info[idx],
                                   g_atcmd_cm_cert_addr_list[idx]);

        if (type == ATCMD_CM_CERT_TYPE_CA_CERT)
        {
            total_len += g_atcmd_cm_cert_info[idx].cert_len;
        }
        else if ((type == ATCMD_CM_CERT_TYPE_CERT) && (g_atcmd_cm_cert_info[idx].seq == seq))
        {
            total_len += g_atcmd_cm_cert_info[idx].cert_len;
        }

        ATCMD_CM_INFO("cert len: %d, total len: %d\n", g_atcmd_cm_cert_info[idx].cert_len, total_len);
    }

    *outlen = total_len;

    ATCMD_CM_INFO("Total length(%s,%d)\n", name, total_len);

    return RRQ_APP_SUCCESS;
}

int atcmd_cm_get_cert(char * name, unsigned char type, unsigned char seq, char * out, size_t * outlen)
{
    int status = RRQ_APP_SUCCESS;

    // Check validation
    if (name == NULL || strlen(name) >= ATCMD_CM_MAX_NAME)
    {
        ATCMD_CM_ERR("Invalid Name\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (type != ATCMD_CM_CERT_TYPE_CA_CERT && type != ATCMD_CM_CERT_TYPE_CERT)
    {
        ATCMD_CM_ERR("Invalid Type\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (   (type == ATCMD_CM_CERT_TYPE_CERT)
            && (seq != ATCMD_CM_CERT_SEQ_CERT && seq != ATCMD_CM_CERT_SEQ_KEY))
    {
        ATCMD_CM_ERR("Invalid Sequence number\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (type == ATCMD_CM_CERT_TYPE_CA_CERT)
    {
        status = atcmd_cm_get_ca_cert_type(name, out, outlen);

        if (status)
        {
            ATCMD_CM_ERR("Failed to get CA certificate(0x%x)\n", status);
            return status;
        }
    }
    else if (type == ATCMD_CM_CERT_TYPE_CERT)
    {
        status = atcmd_cm_get_cert_type(name, seq, out, outlen);

        if (status)
        {
            ATCMD_CM_ERR("Failed to get certificate(0x%x)\n", status);
            return status;
        }
    }

    return status;
}

int atcmd_cm_get_ca_cert_type(char * name, char * out, size_t * outlen)
{
    #ifndef RRQ61X_OSPI_W_ENABLED
    int status = RRQ_APP_SUCCESS;
    #endif //!RRQ61X_OSPI_W_ENABLED
    atcmd_cm_cert_t * cert = NULL;

    int ret = 0;
    int idx_list[ATCMD_CM_MAX_CERT_NUM] = {0x00,};
    int idx_num = 0;
    int num = 0;
    UINT32 addr = 0x00;

    char * pos = NULL;
    size_t total_len = 0;

    ATCMD_CM_INFO("To get certificate(%s:%d)\n", name, strlen(name));

    // Find certificate by name
    ret = atcmd_cm_find_idx_list(name, idx_list, &idx_num);

    if (ret || idx_num <= 0)
    {
        ATCMD_CM_ERR("Failed to find certificate\n");
        return RRQ_APP_NOT_FOUND;
    }

    // Sort by seq
    for (int i = 0 ; i < idx_num - 1 ; i++)
    {
        for (int j = 0 ; j < idx_num - i - 1 ; j++)
        {
            // Swap
            if (g_atcmd_cm_cert_info[idx_list[j]].seq > g_atcmd_cm_cert_info[idx_list[j + 1]].seq)
            {
                ATCMD_CM_ERR("Swap idx:%d, seq:%d/%d\n",
                             idx_list[j],
                             g_atcmd_cm_cert_info[idx_list[j]].seq,
                             g_atcmd_cm_cert_info[idx_list[j + 1]].seq);

                int tmp = idx_list[j];
                idx_list[j] = idx_list[j + 1];
                idx_list[j + 1] = tmp;
            }
        }
    }

    // Calculate total length
    for (num = 0 ; num < idx_num ; num++)
    {
        atcmd_cm_display_cert_info(&g_atcmd_cm_cert_info[idx_list[num]],
                                   g_atcmd_cm_cert_addr_list[idx_list[num]]);

        if (g_atcmd_cm_cert_info[idx_list[num]].type == ATCMD_CM_CERT_TYPE_CA_CERT)
        {
            total_len += g_atcmd_cm_cert_info[idx_list[num]].cert_len;
        }
    }

    ATCMD_CM_INFO("outlen(%d), total_len(%d)\n", *outlen, total_len);

    if (*outlen < total_len)
    {
        ATCMD_CM_ERR("Not enough buffer size(%d,%d)\n", *outlen, total_len);
        return RRQ_APP_NOT_SUCCESSFUL;
    }

    cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

    if (!cert)
    {
        ATCMD_CM_ERR("Failed to allocate memory(%d)\n", sizeof(atcmd_cm_cert_t));
        return RRQ_APP_NOT_CREATED;
    }

    pos = out;

    #ifndef RRQ61X_OSPI_W_ENABLED
    ad_flash_init();
    #endif //!RRQ61X_OSPI_W_ENABLED

    for (num = 0 ; num < idx_num ; num++)
    {
        atcmd_cm_init_cert_t(cert);

        addr = g_atcmd_cm_cert_addr_list[idx_list[num]];

        #ifdef RRQ61X_OSPI_W_ENABLED

        if (pdTRUE == util_sflash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t)))
        {
        #else
        status = ad_flash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t));

        if (status == sizeof(atcmd_cm_cert_t))
        {
        #endif //RRQ61X_OSPI_W_ENABLED
            memcpy(pos, cert->cert, cert->info.cert_len);
            pos += cert->info.cert_len;
            *(pos - 1) = '\n';

            ret += RRQ_APP_SUCCESS;
        }
        else
        {
            ret += RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    *(pos - 1) = '\0';
    *outlen = total_len;

    if (cert)
    {
        atcmd_cm_free(cert);
    }

    return ret;
}

int atcmd_cm_get_cert_type(char * name, unsigned char seq, char * out, size_t * outlen)
{
    int status = RRQ_APP_SUCCESS;
    int idx = 0;
    atcmd_cm_cert_t * cert = NULL;

    int used_alloc_mem = pdTRUE;

    ATCMD_CM_INFO("To get certificate(%s(%d))\n", name, strlen(name));

    idx = atcmd_cm_find_idx(name, ATCMD_CM_CERT_TYPE_CERT, seq);

    if (idx < 0)
    {
        ATCMD_CM_ERR("Failed to find certificate\n");
        return RRQ_APP_NOT_FOUND;
    }

    if (*outlen >= sizeof(atcmd_cm_cert_t))
    {
        cert = (atcmd_cm_cert_t *)out;
        used_alloc_mem = pdFALSE;
    }
    else
    {
        cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

        if (!cert)
        {
            ATCMD_CM_ERR("Failed to allocate memory(%d)\n", sizeof(atcmd_cm_cert_t));
            return RRQ_APP_NOT_CREATED;
        }
    }

    atcmd_cm_init_cert_t(cert);

    status = atcmd_cm_read_cert_by_idx(idx, cert);

    if (status)
    {
        ATCMD_CM_ERR("Failed to read certificate(%d. %s(%d))\n", idx, name, strlen(name));
        goto end;
    }

    if (used_alloc_mem)
    {
        if (*outlen < cert->info.cert_len)
        {
            ATCMD_CM_ERR("Buffer size is not enough(%d, %d)\n", *outlen, cert->info.cert_len);
            status = RRQ_APP_UNDERFLOW;
            goto end;
        }

        *outlen = cert->info.cert_len;
        memcpy(out, cert->cert, cert->info.cert_len);
    }
    else
    {
        *outlen = cert->info.cert_len;
        memmove(cert, cert->cert, cert->info.cert_len);
    }

end:

    if (used_alloc_mem && cert)
    {
        atcmd_cm_free(cert);
    }

    return status;
}

int atcmd_cm_clear_cert(char * name, unsigned char type)
{
    int status = 0;

    int idx_list[ATCMD_CM_MAX_CERT_NUM] = {0x00,};
    int idx_num = 0;

    int idx = 0;
    int num = 0;

    // Check validation
    if (name == NULL || strlen(name) >= ATCMD_CM_MAX_NAME)
    {
        ATCMD_CM_ERR("Invalid Name\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    if (type != ATCMD_CM_CERT_TYPE_CA_CERT && type != ATCMD_CM_CERT_TYPE_CERT)
    {
        ATCMD_CM_ERR("Invalid Type\n");
        return RRQ_APP_INVALID_PARAMETERS;
    }

    status = atcmd_cm_find_idx_list(name, idx_list, &idx_num);

    if (status || idx_num <= 0)
    {
        ATCMD_CM_ERR("Failed to find certificate(%s(%d), %d)\n", name, strlen(name), type);
        return RRQ_APP_NOT_FOUND;
    }

    for (num = 0 ; num < idx_num ; num++)
    {
        idx = idx_list[num];

        if (g_atcmd_cm_cert_info[idx].type == type)
        {
            ATCMD_CM_INFO("Deleted certificate(%d)\n", idx);

            atcmd_cm_display_cert_info(&(g_atcmd_cm_cert_info[idx]),
                                       g_atcmd_cm_cert_addr_list[idx]);

            status = atcmd_cm_clear_cert_by_idx(idx);

            if (status)
            {
                ATCMD_CM_ERR("Failed to clear certificate(%d)\n", idx);
                return status;
            }
        }
    }

    return RRQ_APP_SUCCESS;
}

const atcmd_cm_cert_info_t * atcmd_cm_get_cert_info(void)
{
    int status = RRQ_APP_SUCCESS;

    if (g_atcmd_cm_cert_info == NULL)
    {
        status = atcmd_cm_init_cert_info();

        if (status)
        {
            return NULL;
        }
    }

    return g_atcmd_cm_cert_info;
}

int atcmd_cm_is_exist_cert_with_seq(char * name, unsigned char type, unsigned char seq)
{
    return ((atcmd_cm_find_idx(name, type, seq) == -1) ? pdFALSE : pdTRUE);
}

int atcmd_cm_is_exist_cert(char * p_name, unsigned char type)
{
    int seq = 0;

    if (type == ATCMD_CM_CERT_TYPE_CA_CERT)
    {
        for (seq = ATCMD_CM_CERT_MIN_SEQ_CA_CERT ; seq < ATCMD_CM_CERT_MAX_SEQ_CA_CERT ; seq++)
        {
            if (atcmd_cm_find_idx(p_name, type, seq) != -1)
            {
                return pdTRUE;
            }

        }
    }
    else if (type == ATCMD_CM_CERT_TYPE_CERT)
    {
        if ((atcmd_cm_find_idx(p_name, type, ATCMD_CM_CERT_SEQ_CERT) != -1)
            && (atcmd_cm_find_idx(p_name, type, ATCMD_CM_CERT_SEQ_KEY) != -1))
        {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

int atcmd_cm_find_idx(char * name, unsigned char type, unsigned char seq)
{
    int idx = 0;

    if (!g_atcmd_cm_cert_info)
    {
        if (atcmd_cm_init_cert_info())
        {
            ATCMD_CM_ERR("Failed to init certificate info\n");
            return -1;
        }
    }

    ATCMD_CM_INFO("To find certificate\n"
                  "* Name: %s(%d)\n"
                  "* Type: %d\n"
                  "* Sequence: %d\n",
                  name, strlen(name), type, seq);

    for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
    {
        atcmd_cm_display_cert_info(&g_atcmd_cm_cert_info[idx],
                                   g_atcmd_cm_cert_addr_list[idx]);

        if (   (strcmp(name, g_atcmd_cm_cert_info[idx].name) == 0)
                && (type == g_atcmd_cm_cert_info[idx].type)
                && (seq == g_atcmd_cm_cert_info[idx].seq))
        {
            ATCMD_CM_INFO("Found certificate(%d)\n", idx);
            return idx;
        }

        ATCMD_CM_INFO("Not matched certificate(%d)\n", idx);
    }

    return -1;
}

int atcmd_cm_find_idx_list(char * name, int idx_list[ATCMD_CM_MAX_CERT_NUM], int * idx_num)
{
    int idx = 0;

    memset(idx_list, 0x00, sizeof(int) * ATCMD_CM_MAX_CERT_NUM);
    *idx_num = 0;

    if (!g_atcmd_cm_cert_info)
    {
        if (atcmd_cm_init_cert_info())
        {
            return -1;
        }
    }

    ATCMD_CM_INFO("To find certificate\n"
                  "* Name: %s(%d)\n", name, strlen(name));

    for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
    {
        atcmd_cm_display_cert_info(&g_atcmd_cm_cert_info[idx], g_atcmd_cm_cert_addr_list[idx]);

        if (strcmp(name, g_atcmd_cm_cert_info[idx].name) == 0)
        {
            ATCMD_CM_INFO("Found certificate(%d)\n", idx);

            idx_list[*idx_num] = idx;
            (*idx_num)++;
        }
        else
        {
            ATCMD_CM_INFO("Not matched certificate(%d)\n", idx);
        }
    }

    return (*idx_num == 0 ? -1 : 0);
}

int atcmd_cm_find_empty_idx(void)
{
    int idx = 0;

    if (!g_atcmd_cm_cert_info)
    {
        if (atcmd_cm_init_cert_info())
        {
            return -1;
        }
    }

    for (idx = 0 ; idx < ATCMD_CM_MAX_CERT_NUM ; idx++)
    {
        if (strlen(g_atcmd_cm_cert_info[idx].name) == 0)
        {
            ATCMD_CM_INFO("Found empty idx(%d)\n", idx);
            return idx;
        }
    }

    return -1;
}

int atcmd_cm_read_cert_by_idx(unsigned int idx, atcmd_cm_cert_t * cert)
{
    if (idx >= ATCMD_CM_MAX_CERT_NUM)
    {
        ATCMD_CM_ERR("invalid index(%d)\n", idx);
    }

    return atcmd_cm_read_cert_by_addr(g_atcmd_cm_cert_addr_list[idx], cert);
}

int atcmd_cm_read_cert_by_addr(unsigned int addr, atcmd_cm_cert_t * cert)
{
    int status = 0;

    ATCMD_CM_INFO("addr(0x%x)\n", addr);

    memset(cert, 0x00, sizeof(atcmd_cm_cert_t));

    #ifdef RRQ61X_OSPI_W_ENABLED

    if (pdTRUE != util_sflash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t)))
    {
        ATCMD_CM_ERR("Failed to read SFLASH memory(addr=0x%x, size=%d)\n", addr, sizeof(atcmd_cm_cert_t));
        status = RRQ_APP_NOT_SUCCESSFUL;
    }
    else
    {
        status = RRQ_APP_SUCCESS;
    }

    #else
    ad_flash_init();

    status = ad_flash_read(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t));

    if (status != sizeof(atcmd_cm_cert_t))
    {
        ATCMD_CM_ERR("Failed to read SFLASH memory(addr=0x%x, size=%d)\n", addr, sizeof(atcmd_cm_cert_t));
        status = RRQ_APP_NOT_SUCCESSFUL;
    }
    else
    {
        status = RRQ_APP_SUCCESS;
    }

    #endif //RRQ61X_OSPI_W_ENABLED

    return status;
}

int atcmd_cm_write_cert_by_idx(unsigned int idx, atcmd_cm_cert_t * cert)
{
    if (idx >= ATCMD_CM_MAX_CERT_NUM)
    {
        ATCMD_CM_ERR("invalid index(%d)\n", idx);
    }

    return atcmd_cm_write_cert_by_addr(g_atcmd_cm_cert_addr_list[idx], cert);
}

int atcmd_cm_write_cert_by_addr(unsigned int addr, atcmd_cm_cert_t * cert)
{
    int status = 0;
    #ifdef RRQ61X_OSPI_W_ENABLED

    ATCMD_CM_INFO("addr(0x%x)\n", addr);

    if (rm_wifi_util_sflash_write(addr, (char *)cert, sizeof(atcmd_cm_cert_t)) == pdTRUE)
    {
        status = RRQ_APP_SUCCESS;
    }
    else
    {
        status = RRQ_APP_NOT_SUCCESSFUL;
    }

    #else ///////////////////////////////////////////////////////////////

    int offset = 0;
    size_t w;

    ATCMD_CM_INFO("addr(0x%x)\n", addr);

    ad_flash_init();

    offset = ad_flash_update_possible(addr, (uint8_t *)cert, sizeof(atcmd_cm_cert_t));

    if (offset == sizeof(atcmd_cm_cert_t))
    {
        /* same content existing, no need to write to flash */
        status = RRQ_APP_SUCCESS;
    }
    else
    {
        if (offset >= 0)
        {
            /* no need to erase flash */
        }
        else
        {
            /* offset < 0, erasing flash is needed */
            ad_flash_erase_region(addr, sizeof(atcmd_cm_cert_t));
            offset = 0;
        }

        w = offset + ad_flash_write(addr + offset, cert + offset, sizeof(atcmd_cm_cert_t) - offset);

        /* check that the intended content-size is actually written in flash */
        if (w == sizeof(atcmd_cm_cert_t))
        {
            status = RRQ_APP_SUCCESS;
        }
        else
        {
            status = RRQ_APP_NOT_SUCCESSFUL;
        }
    }

    #endif //RRQ61X_OSPI_W_ENABLED

    return status;
}

int atcmd_cm_clear_cert_by_idx(unsigned int idx)
{
    atcmd_cm_cert_t * cert = NULL;

    if (idx >= ATCMD_CM_MAX_CERT_NUM)
    {
        ATCMD_CM_ERR("invalid index(%d)\n", idx);
    }

    cert = atcmd_cm_malloc(sizeof(atcmd_cm_cert_t));

    if (!cert)
    {
        ATCMD_CM_ERR("Failed to allocate memory for cert(%d)\n", sizeof(atcmd_cm_cert_t));
        return RRQ_APP_NOT_CREATED;
    }

    atcmd_cm_init_cert_t(cert);

    atcmd_cm_write_cert_by_addr(g_atcmd_cm_cert_addr_list[idx], cert);

    atcmd_cm_free(cert);

    if (g_atcmd_cm_cert_info)
    {
        atcmd_cm_init_cert_info_t(&(g_atcmd_cm_cert_info[idx]));
    }

    return RRQ_APP_SUCCESS;
}

int atcmd_cm_clear_cert_by_addr(unsigned int addr)
{
    int status = 0;

    ATCMD_CM_INFO("addr(0x%x)\n", addr);

    #ifdef RRQ61X_OSPI_W_ENABLED

    if (util_sflash_erase(addr, sizeof(atcmd_cm_cert_t)) != pdTRUE)
    {
        ATCMD_CM_ERR("Failed to erase SFLASH memory(addr=0x%x, size=%d)\n", addr, sizeof(atcmd_cm_cert_t));
        status = RRQ_APP_NOT_SUCCESSFUL;
    }
    else
    {
        status = RRQ_APP_SUCCESS;
    }

    #else ///////////////////////////////////////////////////////////////

    ad_flash_init();

    status = ad_flash_erase_region(addr, sizeof(atcmd_cm_cert_t));

    if (status == 0)
    {
        ATCMD_CM_ERR("Failed to erase SFLASH memory(addr=0x%x, size=%d)\n", addr, sizeof(atcmd_cm_cert_t));
        status = RRQ_APP_NOT_SUCCESSFUL;
    }
    else
    {
        status = RRQ_APP_SUCCESS;
    }

    #endif //RRQ61X_OSPI_W_ENABLED

    return status;
}
#endif /* CFG_WIFI */

/* EOF */
