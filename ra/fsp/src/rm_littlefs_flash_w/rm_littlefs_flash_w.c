/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/* FSP includes. */
#include "rm_littlefs_flash_w.h"
#include "rm_littlefs_flash_w_cfg.h"

/* Optional small delay after sync to allow flash controller to settle.
 * Configurable via CMake/defs or in a header by defining
 * RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS (milliseconds). Default: 20ms.
 */
#ifndef RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS
 #define RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS    30
#endif

#if LFS_THREAD_SAFE
 #include "FreeRTOS.h"
 #include "task.h"
#endif

/* Get the data flash block size defined in bsp_feature.h for this MCU. */
#if BSP_FEATURE_FLASH_LP_DF_BLOCK_SIZE != 0
 #define RM_LITTLEFS_FLASH_W_DATA_BLOCK_SIZE      BSP_FEATURE_FLASH_LP_DF_BLOCK_SIZE
#elif BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE != 0
 #define RM_LITTLEFS_FLASH_W_DATA_BLOCK_SIZE      BSP_FEATURE_FLASH_HP_DF_BLOCK_SIZE
#elif BSP_FEATURE_BLOCK_MEDIA_SPI_W_BLOCK_SIZE != 0
 #define RM_LITTLEFS_FLASH_W_DATA_BLOCK_SIZE      BSP_FEATURE_BLOCK_MEDIA_SPI_W_BLOCK_SIZE
#else
 #error "Missing data flash block size as defined in bsp_feature.h"
#endif

#define RM_LITTLEFS_FLASH_W_MINIMUM_BLOCK_SIZE    (104)

#ifndef RM_LITTLEFS_FLASH_W_SEMAPHORE_TIMEOUT
 #define RM_LITTLEFS_FLASH_W_SEMAPHORE_TIMEOUT    UINT32_MAX
#endif

#ifdef RM_LITTLEFS_FLASH_W_DATA_START
static const uint32_t rm_littlefs_flash_w_data_start = RM_LITTLEFS_FLASH_W_DATA_START;
#else
 #define rm_littlefs_flash_w_data_start    BSP_FEATURE_FLASH_DATA_FLASH_START
#endif

/** "RLFS" in ASCII, used to determine if channel is open. */
#define RM_LITTLEFS_FLASH_W_OPEN           (0x524C4653ULL)

/** LittleFS API mapping for LittleFS Port interface */
const rm_littlefs_api_t g_rm_littlefs_on_flash =
{
    .open  = RM_LITTLEFS_FLASH_W_Open,
    .close = RM_LITTLEFS_FLASH_W_Close,
};

/*******************************************************************************************************************//**
 * @addtogroup RM_LITTLEFS_FLASH_W
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Opens the driver and initializes lower layer driver.
 *
 * Implements @ref rm_littlefs_api_t::open().
 *
 * @retval     FSP_SUCCESS                Success.
 * @retval     FSP_ERR_ASSERTION          An input parameter was invalid.
 * @retval     FSP_ERR_ALREADY_OPEN       Module is already open.
 * @retval     FSP_ERR_INVALID_SIZE       The provided block size is invalid.
 * @retval     FSP_ERR_INVALID_ARGUMENT   Flash BGO mode must be disabled.
 * @retval     FSP_ERR_INTERNAL           Failed to create the semaphore.
 *
 * @return     See @ref RENESAS_ERROR_CODES or functions called by this function for other possible return codes. This
 *             function calls:
 *             * @ref flash_api_t::open
 **********************************************************************************************************************/
fsp_err_t RM_LITTLEFS_FLASH_W_Open (rm_littlefs_ctrl_t * const p_ctrl, rm_littlefs_cfg_t const * const p_cfg)
{
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) p_ctrl;

#if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_lfs_cfg);
    FSP_ASSERT(NULL != p_cfg->p_extend);

    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_cfg->p_extend;
    FSP_ASSERT((NULL != p_extend->p_flash) || (NULL != p_extend->p_media));

    if (NULL != p_extend->p_flash)
    {
        FSP_ERROR_RETURN(false == ((flash_instance_t *) p_extend->p_flash)->p_cfg->data_flash_bgo,
                         FSP_ERR_INVALID_ARGUMENT);
    }

    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ERROR_RETURN(p_cfg->p_lfs_cfg->block_size >= RM_LITTLEFS_FLASH_W_MINIMUM_BLOCK_SIZE, FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN((p_cfg->p_lfs_cfg->block_size % RM_LITTLEFS_FLASH_W_DATA_BLOCK_SIZE) == 0, FSP_ERR_INVALID_SIZE);

    FSP_ERROR_RETURN((p_cfg->p_lfs_cfg->block_size * p_cfg->p_lfs_cfg->block_count) <= BSP_DATA_FLASH_SIZE_BYTES,
                     FSP_ERR_INVALID_SIZE);
#else
    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_cfg->p_extend;
#endif

    p_instance_ctrl->p_cfg = p_cfg;

    /* Open the underlying driver. */
    flash_instance_t const          * p_flash;
    rm_block_media_instance_t const * p_media;
    fsp_err_t err;

    if (NULL != p_extend->p_flash)
    {
        p_flash = p_extend->p_flash;

        err = p_flash->p_api->open(p_flash->p_ctrl, p_flash->p_cfg);
    }
    else if (NULL != p_extend->p_media)
    {
        p_media = p_extend->p_media;

        err = p_media->p_api->open(p_media->p_ctrl, p_media->p_cfg);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

        err = p_media->p_api->mediaInit(p_media->p_ctrl);
    }
    else
    {
        err = FSP_ERR_ASSERTION;
    }

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

#if LFS_THREAD_SAFE
    p_instance_ctrl->xSemaphore = xSemaphoreCreateMutexStatic(&p_instance_ctrl->xMutexBuffer);

    if (NULL == p_instance_ctrl->xSemaphore)
    {
        if (NULL != p_extend->p_flash)
        {
            p_flash->p_api->close(p_flash->p_ctrl);
        }
        else if (NULL != p_extend->p_media)
        {
            p_media->p_api->close(p_media->p_ctrl);
        }

        return FSP_ERR_INTERNAL;
    }
#endif

    /* This module is now open. */
    p_instance_ctrl->open = RM_LITTLEFS_FLASH_W_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Closes the lower level driver.
 *
 * Implements @ref rm_littlefs_api_t::close().
 *
 * @retval FSP_SUCCESS           Media device closed.
 * @retval FSP_ERR_ASSERTION     An input parameter was invalid.
 * @retval FSP_ERR_NOT_OPEN      Module not open.
 *
 * @return See @ref RENESAS_ERROR_CODES or functions called by this function for other possible return codes.
 *         This function calls:
 *             * @ref flash_api_t::close
 **********************************************************************************************************************/
fsp_err_t RM_LITTLEFS_FLASH_W_Close (rm_littlefs_ctrl_t * const p_ctrl)
{
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) p_ctrl;
#if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_instance_ctrl->open = 0;

    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (NULL != p_extend->p_flash)
    {
        flash_instance_t const * p_flash = p_extend->p_flash;

        p_flash->p_api->close(p_extend->p_flash->p_ctrl);
    }
    else if (NULL != p_extend->p_media)
    {
        rm_block_media_instance_t const * p_media = p_extend->p_media;

        p_media->p_api->close(p_extend->p_media->p_ctrl);
    }

#if LFS_THREAD_SAFE
    vSemaphoreDelete(p_instance_ctrl->xSemaphore);
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup RM_LITTLEFS_FLASH_W)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Read from the flash driver. Negative error codes are propogated to the user.
 *
 * @param[in]  c           Pointer to the LittleFS config block.
 * @param[in]  block       The block number
 * @param[in]  off         Offset in bytes
 * @param[out] buffer      The buffer to copy data into
 * @param[in]  size        The size in bytes
 *
 * @retval     LFS_ERR_OK  Read is complete.
 * @retval     LFS_ERR_IO  Lower level driver is not open.
 **********************************************************************************************************************/
int rm_littlefs_flash_w_read (const struct lfs_config * c,
                              lfs_block_t               block,
                              lfs_off_t                 off,
                              void                    * buffer,
                              lfs_size_t                size)
{
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) c->context;
#if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, LFS_ERR_IO);
#endif

    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    /* Read directly from the flash. */
    if (NULL != p_extend->p_flash)
    {
        memcpy(buffer,
               (uint8_t *) (rm_littlefs_flash_w_data_start + (p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block) +
                            off),
               size);
    }
    else if (NULL != p_extend->p_media)
    {
        rm_block_media_instance_t const * p_media = p_extend->p_media;
        rm_block_media_info_t             mediainfo;
        lfs_size_t lfsoff, blkidx, blkoff, numblk, remsiz;

        p_media->p_api->infoGet(p_media->p_ctrl, &mediainfo);

        lfsoff = ((p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block) + off);

        /* Calc start address */
        blkidx = lfsoff / mediainfo.sector_size_bytes;
        blkoff = lfsoff % mediainfo.sector_size_bytes;

        /* Calc end address */
        numblk = (size + blkoff) / mediainfo.sector_size_bytes;
        remsiz = (size + blkoff) % mediainfo.sector_size_bytes;

        uint8_t * tmpbuffer = (uint8_t *) (p_extend->media_buffer);
        fsp_err_t err       = FSP_SUCCESS;

        if (numblk == 0)
        {
            err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

            memcpy(buffer, &(tmpbuffer[blkoff]), size);
        }
        else if (numblk > 0)
        {
            if (blkoff > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }

            memcpy(buffer, &(tmpbuffer[blkoff]), (mediainfo.sector_size_bytes - blkoff));
            buffer  = (void *) ((uint8_t *) buffer + (mediainfo.sector_size_bytes - blkoff));
            blkidx += 1;
            numblk -= 1;

            if (numblk > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, buffer, blkidx, numblk);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                buffer  = (void *) ((uint8_t *) buffer + (mediainfo.sector_size_bytes * numblk));
                blkidx += numblk;
            }

            if (remsiz > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                memcpy(buffer, &(tmpbuffer[0]), remsiz);
            }
        }
        else
        {
            return LFS_ERR_IO;
        }
    }

    return LFS_ERR_OK;
}

/*******************************************************************************************************************//**
 * Writes requested bytes to flash.
 *
 * @param[in]  c           Pointer to the LittleFS config block.
 * @param[in]  block       The block number
 * @param[in]  off         Offset in bytes
 * @param[in]  buffer      The buffer containing data to be written.
 * @param[in]  size        The size in bytes
 *
 * @retval     LFS_ERR_OK  Success.
 * @retval     LFS_ERR_IO  Lower layer is not open or failed to write the flash.
 **********************************************************************************************************************/
int rm_littlefs_flash_write (const struct lfs_config * c,
                             lfs_block_t               block,
                             lfs_off_t                 off,
                             const void              * buffer,
                             lfs_size_t                size)
{
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) c->context;
#if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, LFS_ERR_IO);
#endif

    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (NULL != p_extend->p_flash)
    {
        flash_instance_t const * p_flash = p_extend->p_flash;

        /* Call the underlying driver. */
        fsp_err_t err =
            p_flash->p_api->write(p_flash->p_ctrl,
                                  (uint32_t) buffer,
                                  (rm_littlefs_flash_w_data_start +
                                   (p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block) + off),
                                  size);

        /* Write failed. Return IO error. Negative error codes are propogated to the user. */
        FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
    }
    else if (NULL != p_extend->p_media)
    {
        rm_block_media_instance_t const * p_media = p_extend->p_media;
        rm_block_media_info_t             mediainfo;
        lfs_size_t lfsoff, blkidx, blkoff, numblk, remsiz;

        p_media->p_api->infoGet(p_media->p_ctrl, &mediainfo);

        lfsoff = ((p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block) + off);

        /* Calc start address */
        blkidx = lfsoff / mediainfo.sector_size_bytes;
        blkoff = lfsoff % mediainfo.sector_size_bytes;

        /* Calc end address */
        numblk = (size + blkoff) / mediainfo.sector_size_bytes;
        remsiz = (size + blkoff) % mediainfo.sector_size_bytes;

        uint8_t * tmpbuffer = (uint8_t *) (p_extend->media_buffer);
        fsp_err_t err       = FSP_SUCCESS;

        if (numblk == 0)
        {
            err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

            /* Blank Check ? */
            memcpy(&(tmpbuffer[blkoff]), buffer, size);

            err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
        }
        else if (numblk > 0)
        {
            if (blkoff > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }

            /* Blank Check ? */
            memcpy(&(tmpbuffer[blkoff]), buffer, (mediainfo.sector_size_bytes - blkoff));

            err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

            buffer  = (void *) ((uint8_t *) buffer + (mediainfo.sector_size_bytes - blkoff));
            blkidx += 1;
            numblk -= 1;

            if (numblk > 0)
            {
                err = p_media->p_api->write(p_media->p_ctrl, buffer, blkidx, numblk);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                buffer  = (void *) ((uint8_t *) buffer + (mediainfo.sector_size_bytes * numblk));
                blkidx += numblk;
            }

            if (remsiz > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                /* Blank Check ? */
                memcpy(tmpbuffer, buffer, remsiz);

                err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }
        }
        else
        {
            return LFS_ERR_IO;
        }
    }

    return LFS_ERR_OK;
}

/*******************************************************************************************************************//**
 * Erase the logical block. The location and number of blocks to be erased will depend on block size.
 *
 * @param[in]  c           Pointer to the LittleFS config block.
 * @param[in]  block       The logical block number
 *
 * @retval     LFS_ERR_OK  Success.
 * @retval     LFS_ERR_IO  Lower layer is not open or failed to erase the flash.
 **********************************************************************************************************************/
int rm_littlefs_flash_w_erase (const struct lfs_config * c, lfs_block_t block)
{
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) c->context;
#if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, LFS_ERR_IO);
#endif
    rm_littlefs_flash_w_cfg_t const * p_extend = (rm_littlefs_flash_w_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (NULL != p_extend->p_flash)
    {
        flash_instance_t const * p_flash = p_extend->p_flash;

        /* Call the underlying driver. */
        fsp_err_t err =
            p_flash->p_api->erase(p_flash->p_ctrl,
                                  (rm_littlefs_flash_w_data_start +
                                   (p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block)),
                                  p_instance_ctrl->p_cfg->p_lfs_cfg->block_size / RM_LITTLEFS_FLASH_W_DATA_BLOCK_SIZE);

        /* Erase failed. Return IO error. Negative error codes are propogated to the user. */
        FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
    }
    else if (NULL != p_extend->p_media)
    {
        rm_block_media_instance_t const * p_media = p_extend->p_media;
        rm_block_media_info_t             mediainfo;
        lfs_size_t lfsoff, lfssiz, blkidx, blkoff, numblk, remsiz;

        p_media->p_api->infoGet(p_media->p_ctrl, &mediainfo);

        lfsoff = ((p_instance_ctrl->p_cfg->p_lfs_cfg->block_size * block) + 0);
        lfssiz = p_instance_ctrl->p_cfg->p_lfs_cfg->block_size;

        /* Calc start address */
        blkidx = lfsoff / mediainfo.sector_size_bytes;
        blkoff = lfsoff % mediainfo.sector_size_bytes;

        /* Calc end address */
        numblk = (lfssiz + blkoff) / mediainfo.sector_size_bytes;
        remsiz = (lfssiz + blkoff) % mediainfo.sector_size_bytes;

        uint8_t * tmpbuffer = (uint8_t *) (p_extend->media_buffer);
        fsp_err_t err       = FSP_SUCCESS;

        if (numblk == 0)
        {
            if (blkoff > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }

            err = p_media->p_api->erase(p_media->p_ctrl, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

            if (blkoff > 0)
            {
                memset(&(tmpbuffer[blkoff]), 0xFF, (mediainfo.sector_size_bytes - blkoff - lfssiz));

                err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }
        }
        else if (numblk > 0)
        {
            if (blkoff > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }

            err = p_media->p_api->erase(p_media->p_ctrl, blkidx, 1);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

            if (blkoff > 0)
            {
                memset(&(tmpbuffer[blkoff]), 0xFF, (mediainfo.sector_size_bytes - blkoff));

                err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }

            blkidx += 1;
            numblk -= 1;

            if (numblk > 0)
            {
                err = p_media->p_api->erase(p_media->p_ctrl, blkidx, numblk);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                blkidx += numblk;
            }

            if (remsiz > 0)
            {
                err = p_media->p_api->read(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                err = p_media->p_api->erase(p_media->p_ctrl, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);

                memset(tmpbuffer, 0xFF, remsiz);

                err = p_media->p_api->write(p_media->p_ctrl, tmpbuffer, blkidx, 1);
                FSP_ERROR_RETURN(FSP_SUCCESS == err, LFS_ERR_IO);
            }
        }
        else
        {
            return LFS_ERR_IO;
        }
    }

    return LFS_ERR_OK;
}

/*******************************************************************************************************************//**
 * Returns the version of this module.
 *
 * @retval     LFS_ERR_OK  Success.
 * @retval     LFS_ERR_IO  Lower layer is not open or failed to lock the flash.
 **********************************************************************************************************************/
int rm_littlefs_flash_w_lock (const struct lfs_config * c)
{
#if LFS_THREAD_SAFE
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) c->context;
 #if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif
    BaseType_t err = xSemaphoreTake(p_instance_ctrl->xSemaphore, RM_LITTLEFS_FLASH_W_SEMAPHORE_TIMEOUT);

    FSP_ERROR_RETURN(true == err, LFS_ERR_IO);

    return LFS_ERR_OK;
#else
    FSP_PARAMETER_NOT_USED(c);

    return LFS_ERR_IO;
#endif
}

/*******************************************************************************************************************//**
 * Returns the version of this module.
 *
 * @retval     LFS_ERR_OK  Success.
 * @retval     LFS_ERR_IO  Lower layer is not open or failed to unlock the flash.
 **********************************************************************************************************************/
int rm_littlefs_flash_w_unlock (const struct lfs_config * c)
{
#if LFS_THREAD_SAFE
    rm_littlefs_flash_w_instance_ctrl_t * p_instance_ctrl = (rm_littlefs_flash_w_instance_ctrl_t *) c->context;
 #if RM_LITTLEFS_FLASH_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(RM_LITTLEFS_FLASH_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Optional short delay before releasing the flash semaphore so the
     * flash controller has time to settle after program/sync operations. */
 #if defined(RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS) && (RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS > 0)
    vTaskDelay(pdMS_TO_TICKS(RM_LITTLEFS_FLASH_W_SYNC_DELAY_MS));
 #endif
    BaseType_t err = xSemaphoreGive(p_instance_ctrl->xSemaphore);

    FSP_ERROR_RETURN(true == err, LFS_ERR_IO);

    return LFS_ERR_OK;
#else
    FSP_PARAMETER_NOT_USED(c);

    return LFS_ERR_IO;
#endif
}

/*******************************************************************************************************************//**
 * Stub function required by LittleFS. All calls immedialy write/erase the lower layer.
 * @param[in]  c           Pointer to the LittleFS config block.
 * @retval     LFS_ERR_OK  Success.
 **********************************************************************************************************************/
int rm_littlefs_flash_w_sync (const struct lfs_config * c)
{
    FSP_PARAMETER_NOT_USED(c);

    return LFS_ERR_OK;
}
