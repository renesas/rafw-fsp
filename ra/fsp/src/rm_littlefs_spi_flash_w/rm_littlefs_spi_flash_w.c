/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/* FSP includes. */
#include "rm_littlefs_spi_flash_w.h"

#include "rm_littlefs_spi_flash_w_cfg.h"

#define RM_LITTLEFS_SPI_FLASH_W_MINIMUM_BLOCK_SIZE    (104)

#ifndef RM_LITTLEFS_SPI_FLASH_W_SEMAPHORE_TIMEOUT
 #define RM_LITTLEFS_SPI_FLASH_W_SEMAPHORE_TIMEOUT    UINT32_MAX
#endif

/** "RLFW" in ASCII, used to determine if channel is open. */
#define RM_LITTLEFS_SPI_FLASH_W_OPEN                  (0x524C4657ULL)

#define RM_LITTLEFS_SPI_BYTES_PER_WORD                (4U)
#define RM_LITTLEFS_SPI_FLASH_W_CLEARED               (0xFF)

/** LittleFS API mapping for LittleFS Port interface */
const rm_littlefs_api_t g_rm_littlefs_on_spi_flash_w =
{
    .open  = RM_LITTLEFS_SPI_FLASH_W_Open,
    .close = RM_LITTLEFS_SPI_FLASH_W_Close,
};

static fsp_err_t rm_littlefs_spi_flash_w_wait_operation_complete(
    rm_littlefs_spi_flash_w_instance_ctrl_t * const p_ctrl);

/* SPI flash commands */
#define RM_LFS_CMD_READ            (0x03U)
#define RM_LFS_CMD_PAGE_PROGRAM    (0x02U)
#define RM_LFS_CMD_SECTOR_ERASE    (0x20U)
#define RM_LFS_CMD_WRITE_ENABLE    (0x06U)
#define RM_LFS_CMD_READ_STATUS     (0x05U)

/*******************************************************************************************************************//**
 * @addtogroup RM_LITTLEFS_SPI_FLASH_W
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Opens the driver and initializes lower layer SPI driver.
 *
 * Implements @ref rm_littlefs_api_t::open().
 *
 * @retval     FSP_SUCCESS                Success.
 * @retval     FSP_ERR_ASSERTION          An input parameter was invalid.
 * @retval     FSP_ERR_ALREADY_OPEN       Module is already open.
 * @retval     FSP_ERR_INVALID_SIZE       The provided block size is invalid.
 * @retval     FSP_ERR_INVALID_ARGUMENT   Invalid configuration parameter.
 * @retval     FSP_ERR_INTERNAL           Failed to create the semaphore.
 *
 * @return     See @ref RENESAS_ERROR_CODES or functions called by this function for other possible return codes. This
 *             function calls:
 *             * @ref spi_api_t::open
 **********************************************************************************************************************/
fsp_err_t RM_LITTLEFS_SPI_FLASH_W_Open (rm_littlefs_ctrl_t * const p_ctrl, rm_littlefs_cfg_t const * const p_cfg)
{
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) p_ctrl;
    rm_littlefs_spi_flash_w_cfg_t           * p_cfg_local     = (rm_littlefs_spi_flash_w_cfg_t *) p_cfg;
    fsp_err_t err = FSP_SUCCESS;

#if RM_LITTLEFS_SPI_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_cfg_local);
    FSP_ASSERT(NULL != p_cfg_local->p_lower_lvl);
    FSP_ERROR_RETURN(RM_LITTLEFS_SPI_FLASH_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);

    /* Check the block size is valid. */
    FSP_ERROR_RETURN(RM_LITTLEFS_SPI_FLASH_W_MINIMUM_BLOCK_SIZE <= p_cfg->p_lfs_cfg->block_size, FSP_ERR_INVALID_SIZE);
#endif

    /* Store user configuration. */
    p_instance_ctrl->p_cfg         = p_cfg;
    p_instance_ctrl->start_address = p_cfg_local->base_address + p_cfg_local->address_offset;
    p_instance_ctrl->size          = p_cfg_local->size;
    p_instance_ctrl->p_spi         = p_cfg_local->p_lower_lvl;

    /* Initialize underlying SPI driver. */
    err = p_cfg_local->p_lower_lvl->p_api->open(p_cfg_local->p_lower_lvl->p_ctrl, p_cfg_local->p_lower_lvl->p_cfg);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

#ifdef LFS_THREADSAFE

    /* Create semaphore for thread safety. */
    p_instance_ctrl->xSemaphore = xSemaphoreCreateMutexStatic(&p_instance_ctrl->xMutexBuffer);
    FSP_ERROR_RETURN(NULL != p_instance_ctrl->xSemaphore, FSP_ERR_INTERNAL);
#endif

    /* Mark driver as open. */
    p_instance_ctrl->open = RM_LITTLEFS_SPI_FLASH_W_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Closes the driver and the lower level SPI driver.
 *
 * Implements @ref rm_littlefs_api_t::close().
 *
 * @retval     FSP_SUCCESS       The driver was closed successfully.
 * @retval     FSP_ERR_ASSERTION An input parameter was invalid.
 * @retval     FSP_ERR_NOT_OPEN  Module is not open.
 *
 * @return     See @ref RENESAS_ERROR_CODES or functions called by this function for other possible return codes. This
 *             function calls:
 *             * @ref spi_api_t::close
 **********************************************************************************************************************/
fsp_err_t RM_LITTLEFS_SPI_FLASH_W_Close (rm_littlefs_ctrl_t * const p_ctrl)
{
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) p_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if RM_LITTLEFS_SPI_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(RM_LITTLEFS_SPI_FLASH_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Close underlying SPI driver. */
    rm_littlefs_spi_flash_w_cfg_t * p_cfg_local = (rm_littlefs_spi_flash_w_cfg_t *) p_instance_ctrl->p_cfg;
    err = p_cfg_local->p_lower_lvl->p_api->close(p_cfg_local->p_lower_lvl->p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

#ifdef LFS_THREADSAFE

    /* Delete semaphore. */
    if (NULL != p_instance_ctrl->xSemaphore)
    {
        vSemaphoreDelete(p_instance_ctrl->xSemaphore);
        p_instance_ctrl->xSemaphore = NULL;
    }
#endif

    /* Mark driver as closed. */
    p_instance_ctrl->open = 0U;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Read data from SPI flash for LittleFS.
 *
 * @param[in]  c        Pointer to LittleFS configuration.
 * @param[in]  block    Block to read from.
 * @param[in]  off      Offset within block.
 * @param[out] buffer   Buffer to store read data.
 * @param[in]  size     Size of data to read.
 *
 * @retval 0             Success.
 * @retval Non-zero      Error occurred.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_read (const struct lfs_config * c,
                                  lfs_block_t               block,
                                  lfs_off_t                 off,
                                  void                    * buffer,
                                  lfs_size_t                size)
{
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) c->context;

    /* Calculate address to read from. */
    uint32_t address = p_ctrl->start_address + (block * c->block_size) + off;

    /* Build read command (0x03 + 24-bit address). */
    uint8_t cmd[4];
    cmd[0] = (uint8_t) RM_LFS_CMD_READ;
    cmd[1] = (uint8_t) ((address >> 16) & 0xFFU);
    cmd[2] = (uint8_t) ((address >> 8) & 0xFFU);
    cmd[3] = (uint8_t) (address & 0xFFU);

    fsp_err_t err = FSP_SUCCESS;

    /* Send command+address. */
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, cmd, 4U, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS != err)
    {
        return -1;
    }

    /* Read requested data by clocking out dummy bytes. */
    err = p_ctrl->p_spi->p_api->read(p_ctrl->p_spi->p_ctrl, buffer, (uint32_t) size, SPI_BIT_WIDTH_8_BITS);

    return (FSP_SUCCESS == err) ? 0 : -1;
}

/*******************************************************************************************************************//**
 * Write data to SPI flash for LittleFS.
 *
 * @param[in] c       Pointer to LittleFS configuration.
 * @param[in] block   Block to write to.
 * @param[in] off     Offset within block.
 * @param[in] buffer  Buffer containing data to write.
 * @param[in] size    Size of data to write.
 *
 * @retval 0          Success.
 * @retval Non-zero   Error occurred.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_write (const struct lfs_config * c,
                                   lfs_block_t               block,
                                   lfs_off_t                 off,
                                   const void              * buffer,
                                   lfs_size_t                size)
{
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) c->context;

    /* Calculate address to write to. */
    uint32_t address = p_ctrl->start_address + (block * c->block_size) + off;

    fsp_err_t err = FSP_SUCCESS;
    uint8_t   cmd[4];

    /* Enable write. */
    uint8_t we = (uint8_t) RM_LFS_CMD_WRITE_ENABLE;
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, &we, 1U, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS != err)
    {
        return -1;
    }

    /* Build program command (page program). */
    cmd[0] = (uint8_t) RM_LFS_CMD_PAGE_PROGRAM;
    cmd[1] = (uint8_t) ((address >> 16) & 0xFFU);
    cmd[2] = (uint8_t) ((address >> 8) & 0xFFU);
    cmd[3] = (uint8_t) (address & 0xFFU);

    /* Send command+address. */
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, cmd, 4U, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS != err)
    {
        return -1;
    }

    /* Send data to program. */
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, buffer, (uint32_t) size, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS == err)
    {
        /* Wait for write operation to complete. */
        err = rm_littlefs_spi_flash_w_wait_operation_complete(p_ctrl);
    }

    return (FSP_SUCCESS == err) ? 0 : -1;
}

/*******************************************************************************************************************//**
 * Erase block from SPI flash for LittleFS.
 *
 * @param[in] c     Pointer to LittleFS configuration.
 * @param[in] block Block to erase.
 *
 * @retval 0        Success.
 * @retval Non-zero Error occurred.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_erase (const struct lfs_config * c, lfs_block_t block)
{
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) c->context;

    /* Calculate address to erase. */
    uint32_t  address = p_ctrl->start_address + (block * c->block_size);
    fsp_err_t err     = FSP_SUCCESS;
    uint8_t   cmd[4];

    /* Enable write. */
    uint8_t we = (uint8_t) RM_LFS_CMD_WRITE_ENABLE;
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, &we, 1U, SPI_BIT_WIDTH_8_BITS);
    if (FSP_SUCCESS != err)
    {
        return -1;
    }

    /* Build sector erase command. */
    cmd[0] = (uint8_t) RM_LFS_CMD_SECTOR_ERASE;
    cmd[1] = (uint8_t) ((address >> 16) & 0xFFU);
    cmd[2] = (uint8_t) ((address >> 8) & 0xFFU);
    cmd[3] = (uint8_t) (address & 0xFFU);

    /* Send erase command. */
    err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, cmd, 4U, SPI_BIT_WIDTH_8_BITS);

    if (FSP_SUCCESS == err)
    {
        /* Wait for erase operation to complete. */
        err = rm_littlefs_spi_flash_w_wait_operation_complete(p_ctrl);
    }

    return (FSP_SUCCESS == err) ? 0 : -1;
}

/*******************************************************************************************************************//**
 * Lock access to SPI flash.
 *
 * @param[in] c   Pointer to LittleFS configuration.
 *
 * @retval 0      Success.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_lock (const struct lfs_config * c)
{
    FSP_PARAMETER_NOT_USED(c);

#ifdef LFS_THREADSAFE
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) c->context;
    BaseType_t err = xSemaphoreTake(p_ctrl->xSemaphore, RM_LITTLEFS_SPI_FLASH_W_SEMAPHORE_TIMEOUT);

    return (pdTRUE == err) ? 0 : -1;
#else

    return 0;
#endif
}

/*******************************************************************************************************************//**
 * Unlock access to SPI flash.
 *
 * @param[in] c   Pointer to LittleFS configuration.
 *
 * @retval 0      Success.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_unlock (const struct lfs_config * c)
{
    FSP_PARAMETER_NOT_USED(c);

#ifdef LFS_THREADSAFE
    rm_littlefs_spi_flash_w_instance_ctrl_t * p_ctrl = (rm_littlefs_spi_flash_w_instance_ctrl_t *) c->context;
    BaseType_t err = xSemaphoreGive(p_ctrl->xSemaphore);

    return (pdTRUE == err) ? 0 : -1;
#else

    return 0;
#endif
}

/*******************************************************************************************************************//**
 * Sync SPI flash operations.
 *
 * @param[in] c   Pointer to LittleFS configuration.
 *
 * @retval 0      Success.
 **********************************************************************************************************************/
int rm_littlefs_spi_flash_w_sync (const struct lfs_config * c)
{
    FSP_PARAMETER_NOT_USED(c);

    /* For SPI flash, sync is typically handled by the wait operation in write/erase functions. */
    return 0;
}

/*******************************************************************************************************************//**
 * Wait for SPI flash operation to complete.
 *
 * @param[in] p_ctrl  Pointer to instance control structure.
 *
 * @retval FSP_SUCCESS            Operation completed successfully.
 * @retval FSP_ERR_TIMEOUT        Operation timed out.
 **********************************************************************************************************************/
static fsp_err_t rm_littlefs_spi_flash_w_wait_operation_complete (rm_littlefs_spi_flash_w_instance_ctrl_t * const p_ctrl)
{
    rm_littlefs_spi_flash_w_cfg_t * p_cfg = (rm_littlefs_spi_flash_w_cfg_t *) p_ctrl->p_cfg;
    fsp_err_t err = FSP_SUCCESS;

    /* Poll status register until WIP bit clears. */
    for (uint32_t i = 0; i < p_cfg->poll_status_count; i++)
    {
        uint8_t cmd    = (uint8_t) RM_LFS_CMD_READ_STATUS;
        uint8_t status = 0U;

        err = p_ctrl->p_spi->p_api->write(p_ctrl->p_spi->p_ctrl, &cmd, 1U, SPI_BIT_WIDTH_8_BITS);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        err = p_ctrl->p_spi->p_api->read(p_ctrl->p_spi->p_ctrl, &status, 1U, SPI_BIT_WIDTH_8_BITS);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        /* If WIP (bit 0) is cleared, operation is complete. */
        if ((status & 0x01U) == 0U)
        {
            return FSP_SUCCESS;
        }
    }

    return FSP_ERR_TIMEOUT;
}

/** @} (end addtogroup RM_LITTLEFS_SPI_FLASH_W) */

/*
 * Compatibility wrappers and default callback
 * - Some code in the project expects RM_LITTLEFS_FLASH_W_Open/Close and a
 *   user-provided callback named g_rm_littlefs_spi_flash_w0_callback. Provide
 *   simple wrappers and a weak/default callback so the link succeeds.
 */

/* Default callback implementation (user may override). */
void g_rm_littlefs_spi_flash_w0_callback (rm_littlefs_spi_flash_w_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
}

/* Backwards-compatible wrappers without "_SPI" in the name used elsewhere. */
fsp_err_t RM_LITTLEFS_FLASH_W_Open (rm_littlefs_ctrl_t * const p_ctrl, rm_littlefs_cfg_t const * const p_cfg)
{
    return RM_LITTLEFS_SPI_FLASH_W_Open(p_ctrl, p_cfg);
}

fsp_err_t RM_LITTLEFS_FLASH_W_Close (rm_littlefs_ctrl_t * const p_ctrl)
{
    return RM_LITTLEFS_SPI_FLASH_W_Close(p_ctrl);
}
