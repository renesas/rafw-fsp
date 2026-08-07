/*
 *  Copyright (c) 2025, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/* Copyright (C) 2026 Modified by Renesas Electronics Corporation and/or its affiliates */

#include "platform-ra.h"
#include "openthread/platform/flash.h"
#include "openthread/logging.h"
#include "r_flash_api.h"

/**
 * Non-volatile storage for OT is divided into $FLASH_PAGE_NUM Swap pages, each of size $FLASH_SWAP_SIZE
 * Storage will start at $OPENTHREAD_PORT_CFG_STORAGE_FLASH_SETTINGS_START_ADDRESS and will occupy FLASH_SWAP_SIZE * FLASH_PAGE_NUM bytes.
 */

typedef struct otStorage_s
{
    uint32_t start_address;
    uint32_t word_size;
    uint32_t block_size;
} otStorage_t;

static otStorage_t ot_storage = {0};

__STATIC_INLINE uint32_t align_address_to_flash (uint32_t address)
{
    OT_ASSERT(ot_storage.word_size != 0);

    return (address / ot_storage.word_size) * ot_storage.word_size;
}

__STATIC_INLINE uint32_t num_of_words (uint32_t size)
{
    OT_ASSERT(ot_storage.word_size != 0);

    return size / ot_storage.word_size;
}

static void waitForFlashOperationEnd(void)
{
#if defined(RA_FLASH_BGO)
    flash_status_t status;
    fsp_err_t      err;

    if (((nvmc_w_extended_cfg_t *) gp_openthread_port_flash_instance->p_cfg->p_extend)->rwpem_enable)
    {
        /* Wait until the current flash operation completes. */
        do
        {
            err = gp_openthread_port_flash_instance->p_api->statusGet(gp_openthread_port_flash_instance->p_ctrl, &status);
        } while ((FSP_SUCCESS == err) && (FLASH_STATUS_BUSY == status));
    }
#endif // RA_FLASH_BGO
    return;
}

void otPlatFlashInit (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    /* Open the flash instance. */
    fsp_err_t err = gp_openthread_port_flash_instance->p_api->open(gp_openthread_port_flash_instance->p_ctrl,
                        gp_openthread_port_flash_instance->p_cfg);
    OT_ASSERT(FSP_SUCCESS == err);

    /* Retrieve the flash characteristics */
    flash_info_t flash_info;
    gp_openthread_port_flash_instance->p_api->infoGet(gp_openthread_port_flash_instance->p_ctrl, &flash_info);

    ot_storage.start_address    = OPENTHREAD_PORT_CFG_STORAGE_FLASH_SETTINGS_START_ADDRESS;
    ot_storage.block_size       = flash_info.code_flash.p_block_array[0].block_size;
    ot_storage.word_size        = flash_info.code_flash.p_block_array[0].block_size_write;

    if(ot_storage.word_size > OPENTHREAD_PORT_CFG_STORAGE_FLASH_MAX_EXPECTED_WORD_SIZE)
    {
        otLogCritPlat("Flash Word Size is not compatible with Porting Layer Buffer Size");
        err = FSP_ERR_INVALID_SIZE;
    }

    /* Close the flash instance. */
    err = gp_openthread_port_flash_instance->p_api->close(gp_openthread_port_flash_instance->p_ctrl);
    OT_ASSERT(FSP_SUCCESS == err);
}

uint32_t otPlatFlashGetSwapSize (otInstance * aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return OPENTHREAD_PORT_CFG_STORAGE_FLASH_SWAP_PAGE_SIZE;
}

__STATIC_INLINE uint32_t mapAddress (uint8_t aSwapIndex, uint32_t aOffset)
{
    uint32_t address;

    address = OPENTHREAD_PORT_CFG_STORAGE_FLASH_SETTINGS_START_ADDRESS + aSwapIndex * OPENTHREAD_PORT_CFG_STORAGE_FLASH_SWAP_PAGE_SIZE + aOffset;

    return address;
}

static void eraseWordWithBlankCheck (uint32_t address) {
    flash_result_t blank_check_result;
    fsp_err_t err = FSP_SUCCESS;
    
    err = gp_openthread_port_flash_instance->p_api->blankCheck(gp_openthread_port_flash_instance->p_ctrl, 
            address, ot_storage.word_size, &blank_check_result);
    OT_ASSERT(FSP_SUCCESS == err);

    if (blank_check_result == FLASH_RESULT_NOT_BLANK)
    {
        err = gp_openthread_port_flash_instance->p_api->erase(gp_openthread_port_flash_instance->p_ctrl,address, 0);
        OT_ASSERT(FSP_SUCCESS == err);

        waitForFlashOperationEnd();

        err = gp_openthread_port_flash_instance->p_api->blankCheck(gp_openthread_port_flash_instance->p_ctrl, 
                address, FLASH_WORD_SIZE, &blank_check_result);
        OT_ASSERT(FSP_SUCCESS == err);
        OT_ASSERT(FLASH_RESULT_BLANK == blank_check_result);
    }
}

void otPlatFlashErase (otInstance * aInstance, uint8_t aSwapIndex)
{
    OT_UNUSED_VARIABLE(aInstance);

    /* Open the flash instance. */
    fsp_err_t err = gp_openthread_port_flash_instance->p_api->open(gp_openthread_port_flash_instance->p_ctrl,
                        gp_openthread_port_flash_instance->p_cfg);
    OT_ASSERT(FSP_SUCCESS == err);

    for (uint16_t word_index = 0; word_index < OPENTHREAD_PORT_CFG_STORAGE_FLASH_SWAP_PAGE_SIZE / ot_storage.word_size; word_index++)
    {
        uint32_t flash_address = mapAddress(aSwapIndex, word_index * ot_storage.word_size);

        eraseWordWithBlankCheck(flash_address);
    }

    /* Close the flash instance. */
    err = gp_openthread_port_flash_instance->p_api->close(gp_openthread_port_flash_instance->p_ctrl);
    OT_ASSERT(FSP_SUCCESS == err);
}

void otPlatFlashRead (otInstance * aInstance, uint8_t aSwapIndex, uint32_t aOffset, void * aData, uint32_t aSize)
{
    OT_UNUSED_VARIABLE(aInstance);

    memcpy(aData, (void *) mapAddress(aSwapIndex, aOffset), (size_t) aSize);
}

void otPlatFlashWrite (otInstance * aInstance, uint8_t aSwapIndex, uint32_t aOffset, const void * aData, uint32_t aSize)
{
    OT_UNUSED_VARIABLE(aInstance);

    uint8_t buffer[OPENTHREAD_PORT_CFG_STORAGE_FLASH_MAX_EXPECTED_WORD_SIZE];
    uint32_t flash_address;

    /* Open the flash instance. */
    fsp_err_t err = gp_openthread_port_flash_instance->p_api->open(gp_openthread_port_flash_instance->p_ctrl,
                        gp_openthread_port_flash_instance->p_cfg);
    OT_ASSERT(FSP_SUCCESS == err);

    /* Insert unaligned start */
    if (aOffset % ot_storage.word_size != 0)
    {
        uint32_t headDisalignment = aOffset % ot_storage.word_size;
        flash_address = mapAddress(aSwapIndex, align_address_to_flash(aOffset));

        memcpy((void *) buffer, (void *) flash_address, (size_t) ot_storage.word_size);

        memcpy((uint8_t *) buffer + ot_storage.word_size - headDisalignment, aData, headDisalignment);

        eraseWordWithBlankCheck(flash_address);

        err = gp_openthread_port_flash_instance->p_api->write(gp_openthread_port_flash_instance->p_ctrl,
                (uint32_t)(void *)&buffer, mapAddress(aSwapIndex, ALIGN_TO_FLASH(aOffset)), FLASH_WORD_SIZE);

        OT_ASSERT(FSP_SUCCESS == err);

        waitForFlashOperationEnd();

        /* Align to next full word*/
        aOffset = align_address_to_flash(aOffset) + ot_storage.word_size;
        aSize   = aSize - headDisalignment;
        aData   = ((uint8_t *) aData) + headDisalignment;
    }

    /* Insert aligned section */
    if (num_of_words(aSize) > 0)
    {
        for (uint16_t word_index = 0; word_index < num_of_words(aSize); word_index++)
        {
            flash_address = mapAddress(aSwapIndex, aOffset + word_index * ot_storage.word_size);

            eraseWordWithBlankCheck(flash_address);
        }

        err = gp_openthread_port_flash_instance->p_api->write(gp_openthread_port_flash_instance->p_ctrl,
                         (uint32_t) aData, mapAddress(aSwapIndex, ALIGN_TO_FLASH(aOffset)),
                         NUM_OF_WORDS(aSize) * FLASH_WORD_SIZE);
        OT_ASSERT(FSP_SUCCESS == err);

        waitForFlashOperationEnd();
    }

    /* Insert unaligned end */
    if (aSize % ot_storage.word_size != 0)
    {
        flash_address = mapAddress(aSwapIndex, align_address_to_flash(aOffset + aSize));

        uint32_t tailDisalignment = aSize % ot_storage.word_size;

        memcpy((void *) buffer, (void *) flash_address, (size_t) ot_storage.word_size);
        memcpy((uint8_t *) buffer, (uint8_t *) aData + aSize - tailDisalignment, tailDisalignment);

        eraseWordWithBlankCheck(flash_address);

        err = gp_openthread_port_flash_instance->p_api->write(gp_openthread_port_flash_instance->p_ctrl,
                (uint32_t)(void *) &buffer, flash_address, FLASH_WORD_SIZE);
        OT_ASSERT(FSP_SUCCESS == err);

        waitForFlashOperationEnd();
    }

    /* Close the flash instance. */
    err = gp_openthread_port_flash_instance->p_api->close(gp_openthread_port_flash_instance->p_ctrl);
    OT_ASSERT(FSP_SUCCESS == err);
}
