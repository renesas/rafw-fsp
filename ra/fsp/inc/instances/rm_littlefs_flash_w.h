/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef RM_LITTLEFS_FLASH_W_H
#define RM_LITTLEFS_FLASH_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_littlefs_api.h"
#include "r_flash_api.h"
#include "rm_block_media_api.h"
#include "lfs.h"
#if LFS_THREAD_SAFE
 #include "FreeRTOS.h"
 #include "semphr.h"

#endif

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/*******************************************************************************************************************//**
 * @addtogroup RM_LITTLEFS_FLASH_W
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* If you are not using the block media as st_rm_littlefs_flash_w_cfg, 
   set RM_LFS_FLASH_DF_MEDIA_SIZE to 0 to reduce a memory usage.  
   If flash sector size is not 4096 bytes, 
   set RM_LFS_FLASH_DF_MEDIA_SIZE to the desired size. */
#ifndef RM_LFS_FLASH_DF_MEDIA_SIZE
 #define RM_LFS_FLASH_DF_MEDIA_SIZE    (4096)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** User configuration structure, used in open function */
typedef struct st_rm_littlefs_flash_w_cfg
{
    flash_instance_t const * p_flash;  ///< Pointer to a flash instance
    rm_block_media_instance_t const *p_media; ///< Pointer to a block media instance
    uint8_t         media_buffer[RM_LFS_FLASH_DF_MEDIA_SIZE];
} rm_littlefs_flash_w_cfg_t;

/** Instance control block.  This is private to the FSP and should not be used or modified by the application. */
typedef struct st_rm_littlefs_flash_w_instance_ctrl
{
    uint32_t open;
    rm_littlefs_cfg_t const * p_cfg;
#if LFS_THREAD_SAFE
    SemaphoreHandle_t xSemaphore;
    StaticSemaphore_t xMutexBuffer;
#endif
} rm_littlefs_flash_w_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const rm_littlefs_api_t g_rm_littlefs_on_flash;

/** @endcond */

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
fsp_err_t RM_LITTLEFS_FLASH_W_Open(rm_littlefs_ctrl_t * const p_ctrl, rm_littlefs_cfg_t const * const p_cfg);

fsp_err_t RM_LITTLEFS_FLASH_W_Close(rm_littlefs_ctrl_t * const p_ctrl);

int rm_littlefs_flash_w_read(const struct lfs_config * c, lfs_block_t block, lfs_off_t off, void * buffer,
                           lfs_size_t size);

int rm_littlefs_flash_write(const struct lfs_config * c,
                            lfs_block_t               block,
                            lfs_off_t                 off,
                            const void              * buffer,
                            lfs_size_t                size);

int rm_littlefs_flash_w_erase(const struct lfs_config * c, lfs_block_t block);

int rm_littlefs_flash_w_lock(const struct lfs_config * c);

int rm_littlefs_flash_w_unlock(const struct lfs_config * c);

int rm_littlefs_flash_w_sync(const struct lfs_config * c);

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // RM_LITTLEFS_FLASH_W_H

/*******************************************************************************************************************//**
 * @} (end addtogroup RM_LITTLEFS_FLASH_W)
 **********************************************************************************************************************/
