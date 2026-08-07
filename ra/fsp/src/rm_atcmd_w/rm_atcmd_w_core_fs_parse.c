/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_atcmd_w_core_fs_parse.h"
#include "common_data.h"

#if SUPPORT_FSP_RM_FS_W
 #ifndef RM_LITTLEFS_SPI_FLASH_W_CFG_H_
  #include "rm_littlefs_flash_w_cfg.h"
 #endif

 #include "rm_atcmd_w_core.h"
 #include "rm_atcmd_w_core_err_code.h"
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <stdbool.h>

 #include "lfs.h"
 #include "lfs_lock.h"

/* Check which LittleFS stack is available */
 #ifdef RM_LITTLEFS_SPI_FLASH_W_CFG_H_
  #include "r_spi_w.h"
  #define USE_SPI_FLASH_STACK
 #elif defined(RM_LITTLEFS_FLASH_W_CFG_H_)
  #include "rm_littlefs_flash_w.h"
  #define USE_FLASH_W_STACK
 #else
  #error "No supported LittleFS stack configuration found"
 #endif

 #define FSP_ERR_AT_CMD_ERR_NOT_FOUND         4000
 #define FSP_ERR_AT_CMD_ERR_ALREADY_EXISTS    4001
 #define FSP_ERR_AT_CMD_ERR_NO_SPACE          4002
 #define FSP_ERR_AT_CMD_ERR_FS_CORRUPTED      4003
 #define FSP_ERR_AT_CMD_ERR_INVALID_PATH      4004
 #define FSP_ERR_AT_CMD_ERR_RX_TIMEOUT        4005
 #define MAX_WRITE_SIZE                       102400
 #define MAX_PATH_LENGTH                      256
 #define MS_TO_TICKS_1000                     pdMS_TO_TICKS(1000)

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
 #define RM_ATCMD_W_CORE_FS_ATCMD_CODE(atcmd)           "AT+FS" # atcmd

 #define RM_ATCMD_W_CORE_FS_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_FS_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * p_argv[])
 #define RM_ATCMD_W_CORE_FS_ATCMD_FORMAT_CB(atcmd)      const char * RM_ATCMD_W_CORE_FS_ ## atcmd ## _format_cb(void)
 #define RM_ATCMD_W_CORE_FS_ATCMD_BRIEF_CB(atcmd)       const char * RM_ATCMD_W_CORE_FS_ ## atcmd ## _brief_cb(void)

 #define RM_ATCMD_W_CORE_FS_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_FS_ ## atcmd ## _cmd_cb
 #define RM_ATCMD_W_CORE_FS_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_FS_ ## atcmd ## _format_cb
 #define RM_ATCMD_W_CORE_FS_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_FS_ ## atcmd ## _brief_cb

 #define FS_AT_ERROR(fmt, ...)                          printf("[FS:%s:%d] " fmt, __func__, __LINE__, ## __VA_ARGS__)

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(RD);
RM_ATCMD_W_CORE_FS_ATCMD_CB(WR);
RM_ATCMD_W_CORE_FS_ATCMD_CB(DEL);
RM_ATCMD_W_CORE_FS_ATCMD_CB(LST);
RM_ATCMD_W_CORE_FS_ATCMD_CB(FMT);
RM_ATCMD_W_CORE_FS_ATCMD_CB(MEM);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
typedef fsp_err_atcmd_err_code at_err_t;

/* UNCRUSTIFY-OFF */
const atcmd_w_core_module_t at_core_fs_module[] =
{
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(RD),   ATCMD_W_TYPE_A, 3, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(RD),   NULL, NULL },
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(WR),   ATCMD_W_TYPE_A, 2, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(WR),   NULL, NULL },
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(DEL),  ATCMD_W_TYPE_A, 1, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(DEL),  NULL, NULL },
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(LST),  ATCMD_W_TYPE_A, 1, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(LST),  NULL, NULL },
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(FMT),  ATCMD_W_TYPE_A, 0, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(FMT),  NULL, NULL },
    { RM_ATCMD_W_CORE_FS_ATCMD_CODE(MEM), ATCMD_W_TYPE_A, 0, 0, RM_ATCMD_W_CORE_FS_ATCMD_CB_P(MEM), NULL, NULL },
    {NULL, ATCMD_W_TYPE_MAX, 0, 0, NULL, NULL, NULL },
};
/* UNCRUSTIFY-ON */

/***********************************************************************************************************************
 * Local helpers
 **********************************************************************************************************************/
static bool is_query_form (int argc, const char * p_arg1)
{
    return (argc == 2) && (0 == strcmp(p_arg1, AT_CMD_GET_MRK));
}

static int stoi_safe (const char * p_str, int * p_out)
{
    char * p_end = NULL;
    long   v     = strtol(p_str, &p_end, 10);

    if ((!p_str) || ((*p_str) == '\0') || (!p_end) || ((*p_end) != '\0'))
    {
        return -1;
    }

    *p_out = (int) v;

    return 0;
}

static at_err_t fs_map_err (int lfs_rc)
{
    if (lfs_rc == 0)
    {
        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    switch (lfs_rc)
    {
        case LFS_ERR_NOENT:
        {
            return FSP_ERR_AT_CMD_ERR_NOT_FOUND;
        }

        case LFS_ERR_EXIST:
        {
            return FSP_ERR_AT_CMD_ERR_ALREADY_EXISTS;
        }

        case LFS_ERR_NOSPC:
        {
            return FSP_ERR_AT_CMD_ERR_NO_SPACE;
        }

        case LFS_ERR_NOMEM:
        {
            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        case LFS_ERR_INVAL:
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        case LFS_ERR_CORRUPT:
        {
            return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
        }

        default:

            return FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }
}

static at_err_t fs_validate_path (const char * p_path)
{
    char   c;
    size_t len;

    if (!p_path)
    {
        return FSP_ERR_AT_CMD_ERR_INVALID_PATH;
    }

    len = strlen(p_path);

    /* Length check */
    if ((len == 0) || (len >= (MAX_PATH_LENGTH - 1)))
    {
        return FSP_ERR_AT_CMD_ERR_INVALID_PATH;
    }

    /* Allow both absolute and relative paths:
     * Absolute: "/log/file.txt"
     * Relative: "file.txt", "log/file.txt"
     *//* Disallow trailing slash except "/" */
    if ((len > 1) && (p_path[len - 1] == '/'))
    {
        return FSP_ERR_AT_CMD_ERR_INVALID_PATH;
    }

    /* Validate characters */
    for (size_t i = 0; i < len; i++)
    {
        c = p_path[i];

        /* Allow alphanumeric, _, -, ., and / */
        if (!(isalnum((unsigned char) c) ||
              (c == '_') || (c == '.') || (c == '-') || (c == '/')))
        {
            return FSP_ERR_AT_CMD_ERR_INVALID_PATH;
        }
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static at_err_t ensure_parent_dir (const char * p_path)
{
    char            dir_path[MAX_PATH_LENGTH];
    struct lfs_info info;
    int             res;
    char          * p_slash;

    /* Copy path */
    strncpy(dir_path, p_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    /* Find last slash */
    p_slash = strrchr(dir_path, '/');

    /* Case 1: relative file like "log1.txt" → no directory */
    if (p_slash == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    /* Case 2: absolute root with file "/log1.txt" → parent = "/" */
    if (p_slash == dir_path)
    {
        return FSP_ERR_AT_CMD_ERR_CMD_OK; // nothing to create
    }

    /* Cut to parent directory */
    *p_slash = '\0';

    /* Check if directory exists */
    res = lfs_stat(&g_rm_littlefs0_lfs, dir_path, &info);
    if (res == 0)
    {
        /* Exists — but ensure it's a directory */
        if (info.type == LFS_TYPE_DIR)
        {
            return FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
        else
        {
            return FSP_ERR_AT_CMD_ERR_INVALID_PATH;
        }
    }

    /* NOT found: create directory */
    res = lfs_mkdir(&g_rm_littlefs0_lfs, dir_path);

    if (res < 0)
    {
        if (res != LFS_ERR_EXIST)
        {
            return fs_map_err(res);
        }
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

static int atcmd_core_recv_exact (atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_buf, uint32_t len)
{
    if ((NULL == p_buf) || (0 == len))
    {
        FS_AT_ERROR(" Invalid parameter(s)\n");

        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    fsp_err_t err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, p_buf, len);
    if (err != FSP_SUCCESS)
    {
        FS_AT_ERROR("\n\r Read error\n");

        switch (err)
        {
            case FSP_ERR_TIMEOUT:
            {
                return FSP_ERR_AT_CMD_ERR_RX_TIMEOUT;
            }

            default:
            {
                return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            }
        };
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

/***********************************************************************************************************************
 * AT+FSRD — Read file
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(RD)
{
    at_err_t     err;
    int          offset = 0;
    int          length = -1;
    const char * p_usage;
    const char * p_path;
    lfs_file_t   file;
    lfs_soff_t   fsz;
    int          rc;
    int          to_read;
    uint8_t    * p_buf;
    char         hdr[32];
    int          hdrlen;
    int          rlen;

    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSRD:<path>[,<offset>,<len>]\r\n";
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) p_usage, (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    if (!((argc == 2) || (argc == 4)))
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    p_path = p_argv[1];
    err    = fs_validate_path(p_path);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        return err;
    }

    if (argc == 4)
    {
        if (stoi_safe(p_argv[2], &offset) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        }

        if (stoi_safe(p_argv[3], &length) != 0)
        {
            return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
        }

        if (length <= 0)
        {
            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
    }

    /* Take global LFS mutex for the duration of this read operation */
    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    rc = lfs_file_open(&g_rm_littlefs0_lfs, &file, p_path, LFS_O_RDONLY);
    if (rc < 0)
    {
        lfs_mutex_give();

        return fs_map_err(rc);
    }

    fsz = lfs_file_size(&g_rm_littlefs0_lfs, &file);
    if (fsz < 0)
    {
        lfs_file_close(&g_rm_littlefs0_lfs, &file);
        lfs_mutex_give();

        return fs_map_err((int) fsz);
    }

    if (offset > (int) fsz)
    {
        lfs_file_close(&g_rm_littlefs0_lfs, &file);

        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    // This fixes an issue where lfs_file_size() moves the file position
    if (argc == 2)
    {
        // Read entire file in two steps: byte 0, then bytes 1 to end
        uint8_t first_byte;

        // Read first byte
        rc = lfs_file_seek(&g_rm_littlefs0_lfs, &file, 0, LFS_SEEK_SET);
        if (rc < 0)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);
            lfs_mutex_give();

            return fs_map_err(rc);
        }

        rlen = lfs_file_read(&g_rm_littlefs0_lfs, &file, &first_byte, 1);
        if (rlen != 1)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);
            lfs_mutex_give();

            return fs_map_err(rlen);
        }

        // Allocate buffer for entire file
        to_read = (int) fsz;
        p_buf   = (uint8_t *) pvPortMalloc((size_t) to_read);
        if (!p_buf)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);

            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        // Copy first byte
        p_buf[0] = first_byte;

        // Seek to position 1 and read remaining bytes
        if (fsz > 1)
        {
            rc = lfs_file_seek(&g_rm_littlefs0_lfs, &file, 1, LFS_SEEK_SET);
            if (rc < 0)
            {
                vPortFree(p_buf);
                lfs_file_close(&g_rm_littlefs0_lfs, &file);
                lfs_mutex_give();

                return fs_map_err(rc);
            }

            rlen = lfs_file_read(&g_rm_littlefs0_lfs, &file, p_buf + 1, (lfs_size_t) (fsz - 1));
            if (rlen < 0)
            {
                vPortFree(p_buf);
                lfs_file_close(&g_rm_littlefs0_lfs, &file);
                lfs_mutex_give();

                return fs_map_err(rlen);
            }

            rlen = rlen + 1;           // Total bytes read = 1 + remaining
        }
        else
        {
            rlen = 1;
        }
    }
    else
    {
        // Normal path with offset/length specified
        rc = lfs_file_seek(&g_rm_littlefs0_lfs, &file, offset, LFS_SEEK_SET);
        if (rc < 0)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);
            lfs_mutex_give();

            return fs_map_err(rc);
        }

        // Calculate bytes to read
        if (length < 0)
        {
            to_read = (int) fsz - offset;
        }
        else
        {
            to_read = length;
            if ((offset + length) > (int) fsz)
            {
                to_read = (int) fsz - offset;
            }
        }

        if (to_read < 0)
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);

            return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        p_buf = (uint8_t *) pvPortMalloc((size_t) to_read);
        if ((!p_buf) && (to_read > 0))
        {
            lfs_file_close(&g_rm_littlefs0_lfs, &file);

            return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
        }

        if (to_read > 0)
        {
            rlen = lfs_file_read(&g_rm_littlefs0_lfs, &file, p_buf, (lfs_size_t) to_read);
        }
        else
        {
            rlen = 0;
        }
    }

    if (rlen < 0)
    {
        if (p_buf)
        {
            vPortFree(p_buf);
        }

        lfs_file_close(&g_rm_littlefs0_lfs, &file);
        lfs_mutex_give();

        return fs_map_err(rlen);
    }

    lfs_file_close(&g_rm_littlefs0_lfs, &file);
    lfs_mutex_give();
    hdrlen = snprintf(hdr, sizeof(hdr), "\r\n+FSRD:%d\r\n", rlen);
    RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) hdr, (uint32_t) hdrlen);
    if (rlen > 0)
    {
        RM_ATCMD_W_CORE_Write(p_ctrl, p_buf, (uint32_t) rlen);
    }

    RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) "\r\n", 2);
    if (p_buf)
    {
        vPortFree(p_buf);
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_FS_ATCMD_CB(WR)
{
    at_err_t      err;
    lfs_file_t    file;
    uint8_t     * p_buf = NULL;
    int           rc    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    int           wlen  = 0;
    char          dir_path[256];
    const char  * p_path;
    char        * p_slash;
    const char  * p_usage;
    int           lfs_rc;
    int           mkres;
    int           wr;
    int           err_val;
    unsigned long total_bytes;
    unsigned long min_free_bytes;
    lfs_ssize_t   used_blocks;
    unsigned long used_bytes;
    unsigned long free_bytes;
    unsigned long expected_file_size;

    /* Query form help */
    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSWR:<path>,<len>\r\n";
        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl,
                              (uint8_t *) p_usage,
                              (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    /* Check args count */
    if (argc != 3)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    p_path = p_argv[1];

    /* Validate path (must be file, not dir) */
    err = fs_validate_path(p_path);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        return fs_map_err(err);
    }

    /* 2. Ensure parent directory exists */
    err = ensure_parent_dir(p_path);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        return fs_map_err(err);
    }

    /* Parse write length */
    if ((stoi_safe(p_argv[2], &wlen) != 0) || (wlen < 0))
    {
        return FSP_ERR_AT_CMD_ERR_COMMON_ARG_TYPE;
    }

    /* Optional: prevent excessive memory usage */
    if (wlen > MAX_WRITE_SIZE)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    /* Check filesystem space before writing (5% threshold) */
    /* Get expected file size from write length parameter */
    expected_file_size = (unsigned long) wlen;

    /* Calculate total filesystem size in bytes */
    total_bytes = (unsigned long) g_rm_littlefs0_lfs_cfg.block_size *
                  (unsigned long) g_rm_littlefs0_lfs_cfg.block_count;

    /* Calculate minimum free space threshold (5% of total) */
    min_free_bytes = (total_bytes * 5) / 100;

    /* Acquire filesystem mutex with timeout */
    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to get LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(pdMS_TO_TICKS(500)) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex for space check\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    /* Get used blocks count */
    used_blocks = lfs_fs_size(&g_rm_littlefs0_lfs);
    if (used_blocks < 0)
    {
        lfs_mutex_give();
        FS_AT_ERROR(" Failed to get filesystem size\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    /* Release mutex */
    lfs_mutex_give();

    /* Calculate free space */
    used_bytes = (unsigned long) used_blocks * (unsigned long) g_rm_littlefs0_lfs_cfg.block_size;
    if (used_bytes > total_bytes)
    {
        free_bytes = 0;
    }
    else
    {
        free_bytes = total_bytes - used_bytes;
    }

    /* Check if free space is below threshold */
    if (free_bytes < min_free_bytes)
    {
        unsigned long free_percent;

        free_percent = (free_bytes * 100) / total_bytes;
        FS_AT_ERROR(" Filesystem full: only %lu%% free space remaining (threshold: 5%%)\n", free_percent);

        return FSP_ERR_AT_CMD_ERR_NO_SPACE;
    }

    /* Check if writing this file would violate the 5% threshold */
    if (expected_file_size > 0)
    {
        unsigned long free_after_write;
        unsigned long free_percent_after;

        if (free_bytes > expected_file_size)
        {
            free_after_write = free_bytes - expected_file_size;
        }
        else
        {
            free_after_write = 0;
        }

        if (free_after_write < min_free_bytes)
        {
            free_percent_after = (free_after_write * 100) / total_bytes;
            FS_AT_ERROR(
                " Write would exceed space limit: %lu%% free after write (threshold: 5%%, file size: %lu bytes)\n",
                free_percent_after,
                expected_file_size);

            return FSP_ERR_AT_CMD_ERR_NO_SPACE;
        }
    }

    /* Take global LFS mutex, then open file for writing (truncate if exists) */
    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    /* Open file for writing (truncate if exists) */
    lfs_rc = lfs_file_open(&g_rm_littlefs0_lfs, &file, p_path, (LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC));
    if (lfs_rc < 0)
    {
        lfs_mutex_give();

        return fs_map_err(lfs_rc);
    }

    /* If length > 0, receive and write */
    if (wlen > 0)
    {
        p_buf = pvPortMalloc((size_t) wlen);
        if (!p_buf)
        {
            rc = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
            goto cleanup;
        }

        rc = atcmd_core_recv_exact(p_at_ctrl, p_buf, (uint32_t) wlen);
        if (FSP_ERR_AT_CMD_ERR_CMD_OK != rc)
        {
            goto cleanup;
        }

        wr = lfs_file_write(&g_rm_littlefs0_lfs, &file, p_buf, (lfs_size_t) wlen);

        if (wr < 0)
        {
            err_val = wr;
        }
        else
        {
            err_val = LFS_ERR_NOSPC;
        }

        rc = fs_map_err(err_val);
    }

    /* All good */
    rc = FSP_ERR_AT_CMD_ERR_CMD_OK;

cleanup:
    if (p_buf)
    {
        vPortFree(p_buf);
    }

    lfs_file_close(&g_rm_littlefs0_lfs, &file);
    lfs_mutex_give();

    return rc;
}

/***********************************************************************************************************************
 * AT+FSDEL — Delete file
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(DEL)
{
    at_err_t     err;
    const char * p_usage;
    const char * p_path;
    int          rc;

    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSDEL:<path>\r\n";
        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl,
                              (uint8_t *) p_usage,
                              (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    if (argc != 2)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    p_path = p_argv[1];
    err    = fs_validate_path(p_path);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        return err;
    }

    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    rc = lfs_remove(&g_rm_littlefs0_lfs, p_path);
    lfs_mutex_give();

    return fs_map_err(rc);
}

/***********************************************************************************************************************
 * AT+FSLST — List directory
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(LST)
{
    at_err_t        err;
    lfs_dir_t       d;
    struct lfs_info info;
    const char    * p_dir;
    const char    * p_usage;
    int             rc;
    int             n;
    int             t;
    int             r;
    char            line[320];
    const int       MAX_ZERO_FILES = 16;
    const int       MAX_NAME_LEN   = 256;
    char            delnames[MAX_ZERO_FILES][MAX_NAME_LEN];
    int             delcount = 0;

    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSLST[=<dir>]\r\n";
        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl,
                              (uint8_t *) p_usage,
                              (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    p_dir = (argc == 1) ? "/" : p_argv[1];
    err   = fs_validate_path(p_dir);
    if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        return err;
    }

    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    rc = lfs_dir_open(&g_rm_littlefs0_lfs, &d, p_dir);
    if (rc < 0)
    {
        lfs_mutex_give();

        return fs_map_err(rc);
    }

    /* Collect up to a small number of zero-length files to delete after
     * closing the directory. Avoid deleting while iterating the open
     * directory to prevent iterator invalidation. */
    while (true)
    {
        r = lfs_dir_read(&g_rm_littlefs0_lfs, &d, &info);
        if (r < 0)
        {
            lfs_dir_close(&g_rm_littlefs0_lfs, &d);
            lfs_mutex_give();

            return fs_map_err(r);
        }

        if (r == 0)
        {
            break;
        }

        if ((strcmp(info.name, ".") == 0) || (strcmp(info.name, "..") == 0))
        {
            continue;
        }

        t = (info.type == LFS_TYPE_DIR) ? 1 : 0;

        /* If this is a regular file with zero size, schedule it for deletion
         *     and skip reporting it in the listing. */
        if ((t == 0) && (info.size == 0))
        {
            if (delcount < MAX_ZERO_FILES)
            {
                strncpy(delnames[delcount], info.name, MAX_NAME_LEN - 1);
                delnames[delcount][MAX_NAME_LEN - 1] = '\0';
                delcount++;
            }

            continue;
        }

        n = snprintf(line, sizeof(line), "\r\n+FSLST:%d,%u,%s\r\n", t, (unsigned) info.size, info.name);
        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl, (uint8_t *) line, (uint32_t) n);
    }

    /* Close directory before performing deletions */
    lfs_dir_close(&g_rm_littlefs0_lfs, &d);

    /* Remove zero-length files we collected. Build full path for each. */
    for (int i = 0; i < delcount; i++)
    {
        char fullpath[320];
        if ((p_dir[0] == '/') && (p_dir[1] == '\0'))
        {
            /* root directory, path is just the name */
            snprintf(fullpath, sizeof(fullpath), "%s", delnames[i]);
        }
        else
        {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", p_dir, delnames[i]);
        }

        (void) lfs_remove(&g_rm_littlefs0_lfs, fullpath);
    }

    /* Release the mutex and finish */
    lfs_mutex_give();

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

/***********************************************************************************************************************
 * AT+FSFMT — Format filesystem
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(FMT)
{
    const char * p_usage;
    int          rc;

    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSFMT\r\n";

        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl,
                              (uint8_t *) p_usage,
                              (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    if (argc != 1)
    {
        return FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    lfs_format(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
    rc = lfs_mount(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
    if (rc < 0)
    {
        FS_AT_ERROR("\n\rFailed to lfs_mount file system (%d)\n", rc);
    }

    lfs_mutex_give();

    return fs_map_err(rc);
}

/***********************************************************************************************************************
 * AT+FSMEM — Report total and free filesystem memory (bytes)
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_FS_ATCMD_CB(MEM)
{
    const char  * p_usage;
    int           rc;
    lfs_ssize_t   used_blocks;
    unsigned long total_bytes = 0UL;
    unsigned long used_bytes  = 0UL;
    unsigned long free_bytes  = 0UL;
    char          out[80];

    if (is_query_form(argc, p_argv[1]))
    {
        p_usage = "\r\n+FSMEM\r\n";
        RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl,
                              (uint8_t *) p_usage,
                              (uint32_t) strlen(p_usage));

        return FSP_ERR_AT_CMD_ERR_CMD_OK;
    }

    if (argc != 1)
    {
        return FSP_ERR_AT_CMD_ERR_UNKNOWN;
    }

    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    /* Get used blocks count (thread-safe wrapper if enabled) */
    used_blocks = lfs_fs_size(&g_rm_littlefs0_lfs);
    if (used_blocks < 0)
    {
        rc = (int) used_blocks;

        lfs_mutex_give();

        return fs_map_err(rc);
    }

    total_bytes = (unsigned long long) g_rm_littlefs0_lfs_cfg.block_size *
                  (unsigned long long) g_rm_littlefs0_lfs_cfg.block_count;

    used_bytes = (unsigned long long) used_blocks * (unsigned long long) g_rm_littlefs0_lfs_cfg.block_size;
    if (used_bytes > total_bytes)
    {
        free_bytes = 0UL;
    }
    else
    {
        free_bytes = total_bytes - used_bytes;
    }

    lfs_mutex_give();

    snprintf(out, sizeof(out), "\r\n+FSMEM:%lu,%lu\r\n", total_bytes, free_bytes);
    RM_ATCMD_W_CORE_Write((atcmd_w_core_instance_ctrl_t *) p_at_ctrl, (uint8_t *) out, (uint32_t) strlen(out));

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_FS_register (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_fs_module);
}

uint32_t RM_ATCMD_W_CORE_FS_deregister (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    rm_atcmd_w_core_deregister(p_list, at_core_fs_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_FS_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    int       rc;
    fsp_err_t fsp_err = FSP_SUCCESS;

    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    /* Open the appropriate stack based on configuration */
 #ifdef USE_SPI_FLASH_STACK

    /* For SPI Flash stack, open SPI interface first */
    fsp_err = R_SPI_W_Open(&g_spi0_ctrl, &g_spi0_cfg);
    if (FSP_SUCCESS != fsp_err)
    {
        FS_AT_ERROR("\n\rFailed to open SPI interface (%d)\n", fsp_err);

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

 #elif defined(USE_FLASH_W_STACK)

    /* For Flash W stack, open LittleFS flash interface */
    fsp_err = RM_LITTLEFS_FLASH_W_Open(&g_rm_littlefs0_ctrl, &g_rm_littlefs0_cfg);
    if (FSP_SUCCESS != fsp_err)
    {
        FS_AT_ERROR("\n\rFailed to open LittleFS flash interface (%d)\n", fsp_err);

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) != pdTRUE)
    {
        FS_AT_ERROR(" Failed to acquire LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

 #else
  #error "No supported LittleFS stack defined"
 #endif

    rc = lfs_mount(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
    if (rc < 0)
    {
        lfs_mutex_give();
        FS_AT_ERROR("\n\rFailed to lfs_mount file system (%d)\n", rc);

        return FSP_ERR_AT_CMD_ERR_FS_CORRUPTED;
    }

    lfs_mutex_give();

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_FS_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_t fsp_err = FSP_SUCCESS;

    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    if (!lfs_mutex_get())
    {
        FS_AT_ERROR(" Failed to create LFS mutex\n");

        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (lfs_mutex_take(MS_TO_TICKS_1000) == pdTRUE)
    {
        lfs_unmount(&g_rm_littlefs0_lfs);
        lfs_mutex_give();
    }

    /* Close the appropriate stack based on configuration */
 #ifdef USE_SPI_FLASH_STACK

    /* For SPI Flash stack, close SPI interface */
    fsp_err = R_SPI_W_Close(&g_spi0_ctrl);
    if (FSP_SUCCESS != fsp_err)
    {
        FS_AT_ERROR("\n\rFailed to close SPI interface (%d)\n", fsp_err);

        /* Continue execution even if close fails */
    }

 #elif defined(USE_FLASH_W_STACK)

    /* For Flash W stack, close LittleFS flash interface */
    fsp_err = RM_LITTLEFS_FLASH_W_Close(&g_rm_littlefs0_ctrl);
    if (FSP_SUCCESS != fsp_err)
    {
        FS_AT_ERROR("\n\rFailed to close LittleFS flash interface (%d)\n", fsp_err);

        /* Continue execution even if close fails */
    }

 #else
  #error "No supported LittleFS stack defined"
 #endif

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

#else                                  /* SUPPORT_FSP_RM_FS_W  */

/* Stub implementations when no LittleFS stack is configured */
uint32_t RM_ATCMD_W_CORE_FS_register (atcmd_w_core_module_list_t * p_list)
{
    FSP_PARAMETER_NOT_USED(p_list);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_FS_deregister (atcmd_w_core_module_list_t * p_list)
{
    FSP_PARAMETER_NOT_USED(p_list);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_FS_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

uint32_t RM_ATCMD_W_CORE_FS_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
}

#endif                                 /* RM_LITTLEFS_SPI_FLASH_W_CFG_H_ || RM_LITTLEFS_FLASH_W_CFG_H_ */
