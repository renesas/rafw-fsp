/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *    Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "OTAImageProcessorImpl.h"
#include <app/clusters/ota-requestor/OTADownloader.h>
#include <app/clusters/ota-requestor/OTARequestorInterface.h>

#include "rnDeviceWrapAPIs.h"
#include <app/EventLogging.h>
#include <app/InteractionModelEngine.h>
#include <app/reporting/reporting.h>

extern "C" {
}

//matterwork[[::
#include "platform/renesas/RnConfig.h"
//]]matterwork


// Renesas OTA ///
#include <app/clusters/ota-requestor/DefaultOTARequestorDriver.h>
#include "matter_ota.h"
#include "rnDeviceWrapAPIs.h"
////

/// No error, operation OK
#define RN_BOOTLOADER_OK 0L

namespace chip {

// Define static memebers
uint8_t OTAImageProcessorImpl::mSlotId                                                  = 0;
uint32_t OTAImageProcessorImpl::mWriteOffset                                            = 0;
uint16_t OTAImageProcessorImpl::writeBufOffset                                          = 0;
uint8_t OTAImageProcessorImpl::writeBuffer[kAlignmentBytes] __attribute__((aligned(4))) = { 0 };

// MCU FW check flag
uint8_t OTA_MCU_FW =0;

CHIP_ERROR OTAImageProcessorImpl::PrepareDownload()
{
    DeviceLayer::PlatformMgr().ScheduleWork(HandlePrepareDownload, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::Finalize()
{
    DeviceLayer::PlatformMgr().ScheduleWork(HandleFinalize, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}
CHIP_ERROR OTAImageProcessorImpl::Apply()
{
    DeviceLayer::PlatformMgr().ScheduleWork(HandleApply, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::Abort()
{
    DeviceLayer::PlatformMgr().ScheduleWork(HandleAbort, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ProcessBlock(ByteSpan & block)
{
    if ((block.data() == nullptr) || block.empty())
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    // Store block data for HandleProcessBlock to access
    CHIP_ERROR err = SetBlock(block);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "Cannot set block data: %" CHIP_ERROR_FORMAT, err.Format());
    }

    DeviceLayer::PlatformMgr().ScheduleWork(HandleProcessBlock, reinterpret_cast<intptr_t>(this));
    return CHIP_NO_ERROR;
}

bool OTAImageProcessorImpl::IsFirstImageRun()
{
    OTARequestorInterface * requestor = chip::GetRequestorInstance();
    if (requestor == nullptr)
    {
        return false;
    }

    return requestor->GetCurrentUpdateState() == OTARequestorInterface::OTAUpdateStateEnum::kApplying;
}

CHIP_ERROR OTAImageProcessorImpl::ConfirmCurrentImage()
{
    OTARequestorInterface * requestor = chip::GetRequestorInstance();
    if (requestor == nullptr)
    {
        return CHIP_ERROR_INTERNAL;
    }

    uint32_t currentVersion;
    uint32_t targetVersion = requestor->GetTargetVersion();
    ReturnErrorOnFailure(DeviceLayer::ConfigurationMgr().GetSoftwareVersion(currentVersion));
    if (currentVersion != targetVersion)
    {
        ChipLogError(SoftwareUpdate, "Current software version = %" PRIu32 ", expected software version = %" PRIu32, currentVersion,
                     targetVersion);
        return CHIP_ERROR_INCORRECT_STATE;
    }

    return CHIP_NO_ERROR;
}

void OTAImageProcessorImpl::HandlePrepareDownload(intptr_t context)
{
    int32_t err           = RN_BOOTLOADER_OK;
    auto * imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);

    if (imageProcessor == nullptr)
    {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    }
    else if (imageProcessor->mDownloader == nullptr)
    {
        ChipLogError(SoftwareUpdate, "mDownloader is null");
        return;
    }

    ChipLogProgress(SoftwareUpdate, "HandlePrepareDownload");

    mSlotId                                 = 0; // Single slot until we support multiple images
    writeBufOffset                          = 0;
    mWriteOffset                            = 0;
    imageProcessor->mParams.downloadedBytes = 0;

    imageProcessor->mHeaderParser.Init();

    // Not calling bootloader_eraseStorageSlot(mSlotId) here because we erase during each write

    imageProcessor->mDownloader->OnPreparedForDownload(err == RN_BOOTLOADER_OK ? CHIP_NO_ERROR : CHIP_ERROR_INTERNAL);
}

void OTAImageProcessorImpl::HandleFinalize(intptr_t context)
{
    uint32_t err          = RN_BOOTLOADER_OK;
    auto * imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr)
    {
        return;
    }

    // Pad the remainder of the write buffer with zeros and write it to bootloader storage
    if (writeBufOffset != 0)
    {
        // Account for last bytes of the image not yet written to storage
        imageProcessor->mParams.downloadedBytes += writeBufOffset;

        while (writeBufOffset != kAlignmentBytes)
        {
            /* No need to fill Null value for Renesas
             writeBuffer[writeBufOffset] = 0;
             writeBufOffset++;
             */
       //matterwork[[::
	        // Renesas OTA //  64byte under in case
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
            app_matter_ota_download(static_cast<unsigned char *>(writeBuffer), static_cast<uint16_t>(writeBufOffset));
#endif
            return;
       //]]matterwork
        }

        if (err)
        {
            ChipLogError(SoftwareUpdate, "ERROR: In HandleFinalize bootloader_eraseWriteStorage() error %ld", err);
            imageProcessor->mDownloader->EndDownload(CHIP_ERROR_WRITE_FAILED);
            return;
        }
    }

    imageProcessor->ReleaseBlock();

    ChipLogProgress(SoftwareUpdate, "OTA image downloaded successfully");
}

void OTAImageProcessorImpl::OnSystemReboot(System::Layer * systemLayer, void * appState)
{
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
    app_matter_ota_renew();
#endif

    // MCU FW case is not system reboot and add it
    OTARequestorInterface * requestor = chip::GetRequestorInstance();
    if (OTA_MCU_FW) {
        requestor->Reset();
        OTA_MCU_FW = 0;
    }
    vTaskDelay(5);

//    reboot_func(_SYS_REBOOT_POR_);    // reboot act on ota_update_start_renew()
}

void OTAImageProcessorImpl::HandleApply(intptr_t context)
{
    uint32_t err = RN_BOOTLOADER_OK;

    ChipLogProgress(SoftwareUpdate, "OTAImageProcessorImpl::HandleApply()");

    RENES_LOG("[%s:%s:%d]", __FILENAME__, __func__, __LINE__);
    // Flush the events to increase chances that they get sent before the shutdown
    chip::app::InteractionModelEngine * imEngine = chip::app::InteractionModelEngine::InteractionModelEngine::GetInstance();
    //InteractionModelEngine::GetInstance()->GetReportingEngine().ScheduleUrgentEventDeliverySync();
    imEngine->GetReportingEngine().ScheduleUrgentEventDeliverySync();
    RENES_LOG("[%s:%s:%d]", __FILENAME__, __func__, __LINE__);

    // Force KVS to store pending keys such as data from StoreCurrentUpdateInfo()
    chip::DeviceLayer::PersistedStorage::KeyValueStoreMgrImpl().ForceKeyMapSave();

//matterwork[[::
    // Renesas OTA //
    chip::DeviceLayer::SystemLayer().StartTimer(
    std::chrono::duration_cast<System::Clock::Timeout>(System::Clock::Seconds32(RENESAS_KVS_SAVE_DELAY_SECONDS)),
    OTAImageProcessorImpl::OnSystemReboot, NULL);

    ChipLogProgress(SoftwareUpdate, "%d secs. wait system reboot for OTA applying",RENESAS_KVS_SAVE_DELAY_SECONDS);
//]]matterwork
}

void OTAImageProcessorImpl::HandleAbort(intptr_t context)
{
    auto * imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr)
    {
        return;
    }

    // Not clearing the image storage area as it is done during each write
    imageProcessor->ReleaseBlock();
}

void OTAImageProcessorImpl::HandleProcessBlock(intptr_t context)
{
    uint32_t err          = RN_BOOTLOADER_OK;
    auto * imageProcessor = reinterpret_cast<OTAImageProcessorImpl *>(context);
    if (imageProcessor == nullptr)
    {
        ChipLogError(SoftwareUpdate, "ImageProcessor context is null");
        return;
    }
    else if (imageProcessor->mDownloader == nullptr)
    {
        ChipLogError(SoftwareUpdate, "mDownloader is null");
        return;
    }

    ByteSpan block        = imageProcessor->mBlock;
    CHIP_ERROR chip_error = imageProcessor->ProcessHeader(block);

    if (chip_error != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "Matter image header parser error %s", chip::ErrorStr(chip_error));
        imageProcessor->mDownloader->EndDownload(CHIP_ERROR_INVALID_FILE_IDENTIFIER);
        return;
    }

    // Copy data into the word-aligned writeBuffer, once it fills write its contents to the bootloader storage
    // Final data block is handled in HandleFinalize().
    uint32_t blockReadOffset = 0;
    while (blockReadOffset < block.size())
    {
        writeBuffer[writeBufOffset] = *((block.data()) + blockReadOffset);
        writeBufOffset++;
        blockReadOffset++;
        if (writeBufOffset == kAlignmentBytes)
        {
            writeBufOffset = 0;
//matterwork[[::
            // Renesas OTA //
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
            err = app_matter_ota_download(static_cast<unsigned char *>(writeBuffer), static_cast<unsigned int>(kAlignmentBytes));
#endif
//]]matterwork
            if (err)
            {
                ChipLogError(SoftwareUpdate, "ERROR: In HandleProcessBlock bootloader_eraseWriteStorage() error %ld", err);
                imageProcessor->mDownloader->EndDownload(CHIP_ERROR_WRITE_FAILED);
                return;
            }
            mWriteOffset += kAlignmentBytes;
            imageProcessor->mParams.downloadedBytes += kAlignmentBytes;
        }
    }

    imageProcessor->mDownloader->FetchNextData();
}

CHIP_ERROR OTAImageProcessorImpl::ProcessHeader(ByteSpan & block)
{
    if (mHeaderParser.IsInitialized())
    {
        OTAImageHeader header;
        CHIP_ERROR error = mHeaderParser.AccumulateAndDecode(block, header);

	    ChipLogProgress(SoftwareUpdate, "Image Header OTA Type: %.*s ", static_cast<unsigned int>(header.mSoftwareVersionString.size()), static_cast<const char *>(header.mSoftwareVersionString.data()));
        // Needs more data to decode the header
        ReturnErrorCodeIf(error == CHIP_ERROR_BUFFER_TOO_SMALL, CHIP_NO_ERROR);
        ReturnErrorOnFailure(error);

        // SL TODO -- store version somewhere
        ChipLogProgress(SoftwareUpdate, "Image Header software version: %ld payload size: %lu", header.mSoftwareVersion,
                        (long unsigned int) header.mPayloadSize);
        mParams.totalFileBytes = header.mPayloadSize;
        mHeaderParser.Clear();
//matterwork[[::
        // Renesas OTA //
        if (strncasecmp(static_cast<const char *>(header.mSoftwareVersionString.data()), "MCU", 3) == 0) {
            app_ext_status_set(_Status_MCUOTA);
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
            app_matter_ota_init(static_cast<unsigned int>(MATTER_OTA_MCU_FW_STREAM), static_cast<unsigned long long>(header.mPayloadSize));
#endif
            OTA_MCU_FW = 1;
	    } else if (strncasecmp(static_cast<const char *>(header.mSoftwareVersionString.data()), "BLE", 3) == 0) {
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
            app_matter_ota_init(static_cast<unsigned int>(MATTER_OTA_BLE_FW), static_cast<unsigned long long>(header.mPayloadSize));
#endif
        } else if (strncasecmp(static_cast<const char *>(header.mSoftwareVersionString.data()), "RTOS", 4) == 0) {
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
	        app_matter_ota_init(static_cast<unsigned int>(MATTER_OTA_RTOS), static_cast<unsigned long long>(header.mPayloadSize));
#endif
	    } else {
            ChipLogError(SoftwareUpdate, "[%s:%d] OTA Type Not Defined", __func__, __LINE__);
            // removed on v1.1.0.1
            //return CHIP_ERROR_NO_SW_UPDATE_AVAILABLE;
            // matterworkup::replaced with this error
            return CHIP_ERROR_VERSION_MISMATCH;
	    }
//]]matterwork
    }
    return CHIP_NO_ERROR;
}

// Store block data for HandleProcessBlock to access
CHIP_ERROR OTAImageProcessorImpl::SetBlock(ByteSpan & block)
{
    if ((block.data() == nullptr) || block.empty())
    {
        return CHIP_NO_ERROR;
    }

    // Allocate memory for block data if we don't have enough already
    if (mBlock.size() < block.size())
    {
        ReleaseBlock();

        mBlock = MutableByteSpan(static_cast<uint8_t *>(chip::Platform::MemoryAlloc(block.size())), block.size());
        if (mBlock.data() == nullptr)
        {
            return CHIP_ERROR_NO_MEMORY;
        }
    }

    // Store the actual block data
    CHIP_ERROR err = CopySpanToMutableSpan(block, mBlock);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(SoftwareUpdate, "Cannot copy block data: %" CHIP_ERROR_FORMAT, err.Format());
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR OTAImageProcessorImpl::ReleaseBlock()
{
    if (mBlock.data() != nullptr)
    {
        chip::Platform::MemoryFree(mBlock.data());
    }

    mBlock = MutableByteSpan();
    return CHIP_NO_ERROR;
}

} // namespace chip
