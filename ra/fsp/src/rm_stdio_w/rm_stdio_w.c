/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <errno.h>
#include <stdatomic.h>
#include "rm_comms_api.h"
#include "rm_comms_uart_w.h"
#include "../rm_comms_lock/rm_comms_lock.h"

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

static atomic_flag g_init_flag = ATOMIC_FLAG_INIT;

/***********************************************************************************************************************
 * Externs
 **********************************************************************************************************************/

extern rm_comms_instance_t g_comms_stdio_w;

/***********************************************************************************************************************
 * Exported global functions (to be accessed by other files)
 **********************************************************************************************************************/
int _write (int fd, char *ptr, int len);
int _read (int fd, char *ptr, int len);
fsp_err_t rm_stdio_w_locked_write(const char * p_str);

int _write (int fd, char *ptr, int len)
{
    (void)fd;

    fsp_err_t err;

    if (!atomic_flag_test_and_set(&g_init_flag))
    {
        err = g_comms_stdio_w.p_api->open(g_comms_stdio_w.p_ctrl, g_comms_stdio_w.p_cfg);
        if (err != FSP_SUCCESS)
        {
            errno = EIO;
            return -1;
        }
    }

    err = g_comms_stdio_w.p_api->write(g_comms_stdio_w.p_ctrl, (uint8_t *)ptr, (uint32_t) len);
    if (err != FSP_SUCCESS)
    {
        errno = EIO;
        return -1;
    }
    return len;
}

int _read (int fd, char *ptr, int len)
{
    (void)fd;
    (void)len;

    fsp_err_t err;

    if (!atomic_flag_test_and_set(&g_init_flag))
    {
        err = g_comms_stdio_w.p_api->open(g_comms_stdio_w.p_ctrl, g_comms_stdio_w.p_cfg);
        if (err != FSP_SUCCESS)
        {
            errno = EIO;
            return -1;
        }
    }

    err = g_comms_stdio_w.p_api->read(g_comms_stdio_w.p_ctrl, (uint8_t *)ptr, 1);
    if (err != FSP_SUCCESS)
    {
        errno = EIO;
        return -1;
    }
    return 1;
}

fsp_err_t rm_stdio_w_locked_write(const char * p_str)
{

    rm_comms_uart_w_instance_ctrl_t  * p_ctrl = (rm_comms_uart_w_instance_ctrl_t *) g_comms_stdio_w.p_ctrl;
    rm_comms_uart_w_extended_cfg_t const * p_extend = p_ctrl->p_extend;

    if (p_extend->p_tx_mutex)
    {
        fsp_err_t err = rm_comms_recursive_mutex_acquire(p_extend->p_tx_mutex, p_extend->mutex_timeout);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }

    printf("\n%s\n", p_str);
    fflush(stdout);

    if (p_extend->p_tx_mutex)
    {
        fsp_err_t err = rm_comms_recursive_mutex_release(p_extend->p_tx_mutex);
        if (FSP_SUCCESS != err)
        {
            return err;
        }  
    }

    return FSP_SUCCESS;
}

