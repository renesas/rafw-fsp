/**
 ****************************************************************************************
 *
 * @file matter_ota.c
 *
 * @brief Config table to start user applications
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */



#include "FreeRTOS.h"
#include "custom_config_sdk.h"
#include "rnDeviceWrapAPIs.h"
#if (CHIP_DEVICE_CONFIG_ENABLE_OTA)
#if defined (__SUPPORT_OTA__)
#if (DEVICE_FAMILY != DA1640X)
#include "da16x_system.h"
#include "application.h"
#endif

#include "sdk_defs.h"
#include "common_def.h"
#include "ota_update_common.h"
#include "ota_update.h"

#if defined(__SUPPORT_MATTER_IOT__)
#if (DEVICE_FAMILY != DA1640X)
#include "app_common_util.h"
#endif
#include "rnDeviceWrapAPIs.h"
#endif // ( __SUPPORT_MATTER_IOT__ )

#include "matter_ota.h"
#if (DEVICE_FAMILY != DA1640X)
#include "command.h"
#include "sflash.h"
#endif
#include "common_utils.h"
#include "timers.h"

#if defined (__BLE_COMBO_REF__)
#include "ota_update_ble_fw.h"
#endif  // (__BLE_COMBO_REF__)

#include "rm_vee_flash_w_rrq_nvram.h"

UINT ota_update_progress_rtos = 0;
UINT ota_update_progress_ble = 0;
#if defined (__SUPPORT_ATCMD__)
UINT ota_update_progress_mcu = 0;
#endif
UINT ota_update_renew_type = 0;

static ota_update_download_t by_matter;

#if (DEVICE_FAMILY == DA1640X)
static UINT CUSTOM_SFLASH_ADDR = SF_USER_AREA;
#else
static UINT CUSTOM_SFLASH_ADDR = SFLASH_USER_AREA_START;
#endif

UINT app_matter_ota_init(UINT fw_type, UINT64 len)
{
	UINT erase_addr = 0;
	UINT erase_len = 0;
	UINT erase_totlen = 0;
	UINT erase_progress = 0;
	UINT erase_cnt = 0;

    PRINTF("[%s] type: %d, len: %lld\n", __func__, fw_type, len);

    if (ota_update_check_available_size(fw_type, len) != OTA_SUCCESS) {
        return OTA_ERROR_SIZE;
    }

    by_matter.write.sflash_addr = ota_update_get_new_sflash_addr(fw_type);
#if defined (__BLE_COMBO_REF__)
#if defined (__SUPPORT_ATCMD__)
    if ((by_matter.write.sflash_addr != OTA_STOR_RTOS_0_ADDR)
        && (by_matter.write.sflash_addr != OTA_STOR_RTOS_1_ADDR)
        && (by_matter.write.sflash_addr != SFLASH_BLE_FW_BASE)
        && (by_matter.write.sflash_addr != OTA_SUCCESS)) {
    PRINTF("[%s:%d] OTA_ERROR_SFLASH_ADDR\n",__func__,__LINE__);
    return OTA_ERROR_SFLASH_ADDR;
    }    	
#else
    if ((by_matter.write.sflash_addr != OTA_STOR_RTOS_0_ADDR)
    && (by_matter.write.sflash_addr != OTA_STOR_RTOS_1_ADDR)
    && (by_matter.write.sflash_addr != SFLASH_BLE_FW_BASE)) {
        PRINTF("[%s:%d] OTA_ERROR_SFLASH_ADDR\n",__func__,__LINE__);
        return OTA_ERROR_SFLASH_ADDR;
    }
#endif // (__SUPPORT_ATCMD__)
#else
    if (fw_type == OTA_TYPE_RTOS) {
#if (DEVICE_FAMILY == DA1640X)
        if ((by_matter.write.sflash_addr != SF_RTOS_0)
            && (by_matter.write.sflash_addr != SF_RTOS_1)) {
#else
        if ((by_matter.write.sflash_addr != OTA_STOR_RTOS_0_ADDR)
            && (by_matter.write.sflash_addr != OTA_STOR_RTOS_1_ADDR)) {
#endif
            PRINTF("[%s:%d] OTA_ERROR_SFLASH_ADDR\n",__func__,__LINE__);
            return OTA_ERROR_SFLASH_ADDR;
        }    	
#endif	// (__BLE_COMBO_REF__)
    }
    by_matter.update_type = fw_type;
    by_matter.received_length = 0;
    by_matter.write.total_length = len;
    by_matter.write.offset = 0;
    by_matter.download_status = OTA_SUCCESS;
    by_matter.version_check = OTA_NOT_FOUND;
    by_matter.content_length = len;
    by_matter.received_length = 0;
    ota_update_set_proc_state(OTA_STATE_READY);

    return OTA_SUCCESS;
}

UINT matter_ota_update_write_mcu(ota_update_sflash_t *sflash_ctx,
                             UCHAR *data, UINT length)
{
    UINT status = OTA_SUCCESS;
    UINT buff_offset = 0;
    UINT copyToBufLen = 0;
    UINT writeToFlashLen = 0;
    UINT input_len = 0;
    UCHAR *input_data = NULL;

    if (sflash_ctx == NULL) {
        PRINTF("[%s:%d] sflash_ctx error\n", __func__, __LINE__);
        status = OTA_FAILED;
        goto finish;
    }

    if (length == 0) {
        PRINTF("[%s:%d] Data length error\n", __func__, __LINE__);
        status = OTA_FAILED;
        goto finish;
    }

    if (sflash_ctx->buffer == NULL) {
        sflash_ctx->buffer = (UCHAR *)OTA_MALLOC(OTA_SFLASH_BUF_SZ);
        if (sflash_ctx->buffer == NULL) {
            PRINTF("[%s:%d] Failed to allocate receive buffer(%d bytes)\n", __func__, __LINE__,
                    OTA_SFLASH_BUF_SZ);
            status = OTA_MEM_ALLOC_FAILED;
            goto finish;
        }
        memset(sflash_ctx->buffer, 0x00, OTA_SFLASH_BUF_SZ);
    }

    input_data = data;
    input_len = length;

    sflash_ctx->length += input_len;

    if (sflash_ctx->offset > 0 ) {
        writeToFlashLen = sflash_ctx->offset;
    }

    writeToFlashLen += input_len;

    while (writeToFlashLen > 0) {
        copyToBufLen = OTA_SFLASH_BUF_SZ - sflash_ctx->offset;

        if (copyToBufLen > (input_len - buff_offset)) {
            copyToBufLen = (input_len - buff_offset);
        }

        if (copyToBufLen > 0) {
            memcpy(&sflash_ctx->buffer[sflash_ctx->offset],
                   (input_data + buff_offset),
                   copyToBufLen);
            buff_offset += copyToBufLen;
            sflash_ctx->offset += copyToBufLen;
        }

        if ((sflash_ctx->offset == OTA_SFLASH_BUF_SZ)
                || (sflash_ctx->offset == sflash_ctx->total_length)
                || ((sflash_ctx->length == sflash_ctx->total_length) && (sflash_ctx->offset > 0))) {
            if (app_writeDataToMCU(sflash_ctx->sflash_addr, (UINT)sflash_ctx->total_length
                , sflash_ctx->length, (UINT32 *)&sflash_ctx->buffer[0]
                , (UINT)OTA_SFLASH_BUF_SZ) != OTA_SFLASH_BUF_SZ) {
                PRINTF("[%s:%d] MCU write failed\n", __func__, __LINE__);
                status = OTA_FAILED;
                goto finish;
            }
            sflash_ctx->sflash_addr += sflash_ctx->offset;

            memset(sflash_ctx->buffer, 0x00, OTA_SFLASH_BUF_SZ);
            sflash_ctx->offset = 0;

            if (writeToFlashLen >= OTA_SFLASH_BUF_SZ) {
                writeToFlashLen = writeToFlashLen - OTA_SFLASH_BUF_SZ;
            }
        } else {
            break;
        }
    }

finish:

    if ((status != OTA_SUCCESS)
            || (sflash_ctx->length == sflash_ctx->total_length)) {
        sflash_ctx->sflash_addr = 0x00;
        sflash_ctx->total_length = 0;
        sflash_ctx->length = 0;
        sflash_ctx->offset = 0;

        if (sflash_ctx->buffer != NULL) {
            OTA_FREE(sflash_ctx->buffer);
            sflash_ctx->buffer = NULL;
        }
    }

    return status;
}

UINT app_matter_ota_download(UCHAR *rev_data, UINT rev_data_len)
{
    UINT status = OTA_SUCCESS;
    UINT progress = 0;
#if defined (__BLE_COMBO_REF__)
    UINT offset_t = 0;
#endif // __BLE_COMBO_REF__

#if defined (__BLE_COMBO_REF__)    
#if defined (__SUPPORT_ATCMD__)
    if ((by_matter.update_type != OTA_TYPE_RTOS) && (by_matter.update_type != OTA_TYPE_BLE_FW) && (by_matter.update_type != OTA_TYPE_MCU_FW_STREAM)) 
#else
    if ((by_matter.update_type != OTA_TYPE_RTOS) && (by_matter.update_type != OTA_TYPE_BLE_FW)) 
#endif
    {
        status = OTA_ERROR_TYPE;
        PRINTF("[%s:%d] OTA_ERROR_SFLASH_ADDR\n",__func__,__LINE__);
        goto finish;
    }

    if (by_matter.update_type != OTA_TYPE_MCU_FW_STREAM)
    	ota_update_ble_set_exist_url(by_matter.update_type, 1);
#else
#if defined (__SUPPORT_ATCMD__)
    if ((by_matter.update_type != OTA_TYPE_RTOS) && (by_matter.update_type != OTA_TYPE_MCU_FW_STREAM))
#else
    if (by_matter.update_type != OTA_TYPE_RTOS)
#endif
    {
        status = OTA_ERROR_TYPE;
        PRINTF("[%s:%d] OTA_ERROR_SFLASH_ADDR\n",__func__,__LINE__);
        goto finish;
    }
#endif // (__BLE_COMBO_REF__)

    ota_update_set_proc_state(OTA_STATE_PROGRESS);

    if (rev_data == NULL) {
        PRINTF("[%s] rev_data is null\n", __func__);
        status = OTA_FAILED;
        goto finish;

    } else {

#if defined (DISABLE_OTA_VER_CHK)
        by_matter.version_check = OTA_SUCCESS;
#else
        if (by_matter.version_check == OTA_NOT_FOUND) {
            if (ota_update_check_version(by_matter.update_type,
                                         rev_data, rev_data_len) == OTA_SUCCESS) {
                by_matter.version_check = OTA_SUCCESS;
            } else {
                /* Version mismatch */
                by_matter.version_check = OTA_VERSION_INCOMPATI;
                status = OTA_VERSION_INCOMPATI;
                PRINTF("[%s:%d] OTA_VERSION_INCOMPATI\n",__func__,__LINE__);
                goto finish;
            }

            if (by_matter.version_check == OTA_SUCCESS) {
                if (ota_update_check_available_size(by_matter.update_type,
                                                 by_matter.content_length) != OTA_SUCCESS) {
                    by_matter.version_check = OTA_VERSION_INCOMPATI;
                    status = OTA_VERSION_INCOMPATI;
                    PRINTF("[%s:%d] OTA_VERSION_INCOMPATI\n",__func__,__LINE__);
                    goto finish;
                }
            }
        }
#endif // (DISABLE_OTA_VER_CHK)

        by_matter.received_length += rev_data_len;
#if defined (__BLE_COMBO_REF__)
        if (by_matter.update_type == OTA_TYPE_BLE_FW) {
            ota_update_ble_crc_calcu(rev_data, rev_data_len);
            offset_t = ota_update_ble_first_packet_offset();
            rev_data += offset_t;
            rev_data_len -= offset_t;
            by_matter.write.total_length -= offset_t;
        }
#endif //(__BLE_COMBO_REF__)
#if defined (__SUPPORT_ATCMD__)
        if (by_matter.update_type == OTA_TYPE_MCU_FW_STREAM)
            status = matter_ota_update_write_mcu(&by_matter.write, rev_data, rev_data_len);
        else
#endif //(__SUPPORT_ATCMD__)
        status = ota_update_buffer_write_flash(&by_matter.write, rev_data, rev_data_len);

        if (status != OTA_SUCCESS) {
            PRINTF("[%s] Failed to write data to sflash(0x%02x)\n", __func__, status);
            goto finish;
        }

        if ((by_matter.received_length > 0) && (by_matter.content_length > 0)) {
            progress = (by_matter.received_length * 100) / by_matter.content_length;
            if (by_matter.update_type == OTA_TYPE_RTOS) {
                 if (ota_update_progress_rtos< progress) {
                     PRINTF("\r   >> [sflash_addr:0x%x] RTOS Downloading... %d %% (%d/%d Bytes)%s\n",
                     by_matter.write.sflash_addr,
                     progress,
                     by_matter.received_length,
                     by_matter.content_length, progress == 100 ? "\n" : " ");
                 }
                 ota_update_progress_rtos  = progress;
#if defined (__BLE_COMBO_REF__)
              } else if (by_matter.update_type == OTA_TYPE_BLE_FW) {
                if (ota_update_progress_ble < progress) {
                    PRINTF("\r   >> [sflash_addr:0x%x] BLE Downloading... %d % (%d/%d Bytes)%s\n",
                    by_matter.write.sflash_addr,
                    progress,
                    by_matter.received_length,
                    by_matter.content_length, progress == 100 ? "\n" : " ");
                }
                ota_update_progress_ble  = progress;
#endif // (__BLE_COMBO_REF__)			 
#if defined (__SUPPORT_ATCMD__)
             } else if (by_matter.update_type == OTA_TYPE_MCU_FW_STREAM) {
                 if (ota_update_progress_mcu < progress) {
                     PRINTF("\r   >> [sflash_addr:0x%x] MCU Downloading... %d (%d/%d Bytes)%s\n",
                     by_matter.write.sflash_addr,
                     progress,
                     by_matter.received_length,
                     by_matter.content_length, progress == 100 ? "\n" : " ");
                 }
                 ota_update_progress_mcu  = progress;
#endif // (__SUPPORT_ATCMD__)
            }
        } else {
            progress = 0;
        }
    }

finish:

    if ((status != OTA_SUCCESS) || progress == 100) {
        if (progress == 100) {
        	if (by_matter.version_check == OTA_SUCCESS) {
        		ota_update_set_download_progress(by_matter.update_type, progress);
                ota_update_write_nvram_download_progress(by_matter.update_type, progress);
        	}
        	ota_update_print_status(by_matter.update_type, OTA_SUCCESS);
            ota_update_renew_type = by_matter.update_type;
            by_matter.update_type = OTA_TYPE_INIT;
            by_matter.received_length = 0;
            by_matter.write.sflash_addr = 0;
            by_matter.write.total_length = 0;
            by_matter.write.offset = 0;
            ota_update_set_proc_state(OTA_STATE_READY);	// add for update ota_update_get_proc_state();
            if (ota_update_renew_type == OTA_TYPE_RTOS) {
                by_matter.download_status = OTA_SUCCESS;
#if defined (__BLE_COMBO_REF__)
            } else if (ota_update_renew_type == OTA_TYPE_BLE_FW) {
                by_matter.download_status = ota_update_ble_crc();
#endif // (__BLE_COMBO_REF__)
#if defined (__SUPPORT_ATCMD__)
            } else if (ota_update_renew_type == OTA_TYPE_MCU_FW_STREAM) {
                by_matter.download_status = OTA_SUCCESS;
#endif // (__SUPPORT_ATCMD__)
            }
            by_matter.version_check = OTA_NOT_FOUND;
            by_matter.content_length = 0;
            by_matter.received_length = 0;
        }
#if defined (__SUPPORT_ATCMD__)
        if (ota_update_renew_type == OTA_TYPE_MCU_FW_STREAM) {
            app_ext_status_set(_Status_STA_start);
        }
#endif // (__SUPPORT_ATCMD__)
        PRINTF("status:0x%02x\r\n", status);
    }

    return status;

}

 UINT app_matter_ota_renew(void)
{
    UINT status = OTA_SUCCESS;
    OTA_UPDATE_CONFIG *ota_update_conf = NULL;

#if defined (__SUPPORT_ATCMD__)
    if (ota_update_renew_type == OTA_TYPE_MCU_FW_STREAM)
        return status;
#endif  //(__SUPPORT_ATCMD__)
    ota_update_conf = OTA_MALLOC(sizeof(OTA_UPDATE_CONFIG));

    if (ota_update_conf == NULL) {
        PRINTF("[%s] Failed to alloc memory\n", __func__);
        return OTA_FAILED;
    }

    memset(ota_update_conf, 0x00, sizeof(OTA_UPDATE_CONFIG));

    ota_update_conf->renew_notify = NULL;

    status = ota_update_start_renew(ota_update_conf);

    if (ota_update_conf != NULL) {
        OTA_FREE(ota_update_conf);
    }

    return status;
}

#if defined (__IMG_UPDATE_BY_MCU__)
#define OTA_REV_ACK_TIMEOUT 5000

static ota_update_download_t by_matter_mcu;
static TimerHandle_t		ota_rev_ack_timer = NULL;

VOID ota_update_direct_write_timeout(void* arg)
{
#if (DEVICE_FAMILY == DA1640X)
    RA6W1_UNUSED_ARG(arg);
#else
    DA16X_UNUSED_ARG(arg);
#endif
    atcmd_asynchony_event(11, NULL); //+FWUPDATE
    by_matter_mcu.download_status = OTA_STATE_READY;
    by_matter_mcu.update_type = MATTER_OTA_INIT;
    PRINTF("[%s] byMCU Write timeout\n", __func__);
    return;
}

UINT matter_ota_update_by_mcu_init(UINT fw_type, UINT len)
{
    int ret;

    if (ota_update_check_accept_size(fw_type, len) != OTA_SUCCESS) {
        return OTA_ERROR_SIZE;
    }

    by_matter_mcu.write.sflash_addr = ota_update_get_new_sflash_addr(fw_type);
    if ((by_matter_mcu.write.sflash_addr != OTA_STOR_RTOS_0_ADDR)
            && (by_matter_mcu.write.sflash_addr != OTA_STOR_RTOS_1_ADDR)) {
        return OTA_ERROR_SFLASH_ADDR;
    }

    by_matter_mcu.update_type = fw_type;
    by_matter_mcu.received_length = 0;
    by_matter_mcu.write.total_length = len;
    by_matter_mcu.write.offset = 0;
    by_matter_mcu.download_status = OTA_STATE_READY;
    by_matter_mcu.version_check = OTA_NOT_FOUND;
    by_matter_mcu.content_length = len;
    by_matter_mcu.received_length = 0;

    if (ota_rev_ack_timer != NULL) {
        xTimerStop(ota_rev_ack_timer, 0);
        xTimerDelete(ota_rev_ack_timer, 0);
        memset(&ota_rev_ack_timer, 0, sizeof(TimerHandle_t));
    }

    /* Create timer to trigger the dpm_sleep_daemon periodically. */
    ota_rev_ack_timer = xTimerCreate("ota_rev_ack_timer",
                                                    pdMS_TO_TICKS(OTA_REV_ACK_TIMEOUT),
                                                    pdFALSE,
                                                    (void *)NULL,
                                                    (TimerCallbackFunction_t) ota_update_direct_write_timeout);
    if (ota_rev_ack_timer == NULL) {
        PRINTF("[%s] Failed to create ota_rev_ack_timer !!!\n", __func__);
        return OTA_FAILED;
    }

    ret = xTimerStart(ota_rev_ack_timer, 0);
    if (ret != pdPASS) {
        PRINTF("[%s] Failed to start timer(%d)\r\n", __func__, ret);
        xTimerDelete(ota_rev_ack_timer, 0);
        return OTA_FAILED;
    }

    return OTA_SUCCESS;
}

UINT matter_ota_update_by_mcu_download(UCHAR *rev_data, UINT rev_data_len)
{
    UINT status = OTA_SUCCESS;
    UINT progress = 0;

    if (by_matter_mcu.update_type != OTA_TYPE_RTOS) {
        status = OTA_ERROR_TYPE;
        goto finish;
    }

    if ((by_matter_mcu.write.sflash_addr < ota_update_get_new_sflash_addr(OTA_TYPE_RTOS))
        || (by_matter_mcu.write.sflash_addr > (ota_update_get_new_sflash_addr(OTA_TYPE_RTOS) + by_matter_mcu.content_length))){
        status = OTA_ERROR_SFLASH_ADDR;
        goto finish;
    }

    if (ota_rev_ack_timer != NULL) {
        xTimerReset(ota_rev_ack_timer, 0);
    } else {
        status = OTA_FAILED_TIMER;
        goto finish;
    }

    by_matter_mcu.download_status = OTA_STATE_PROGRESS;

    if (rev_data == NULL) {
        PRINTF("[%s] rev_data is null\n", __func__);
        status = OTA_FAILED;
        goto finish;
    } else {
#if defined (DISABLE_OTA_VER_CHK)
        by_matter_mcu.version_check = OTA_SUCCESS;
#else
        if (by_matter_mcu.version_check == OTA_NOT_FOUND) {
            if (ota_update_check_version(by_matter_mcu.update_type,
                      rev_data, rev_data_len) == OTA_SUCCESS) {
                by_matter_mcu.version_check = OTA_SUCCESS;
            } else {
                /* Version mismatch */
                by_matter_mcu.version_check = OTA_VERSION_INCOMPATI;
                status = OTA_VERSION_INCOMPATI;
                goto finish;
            }

            if (by_matter_mcu.version_check == OTA_SUCCESS) {
                if (ota_update_check_available_size(by_matter_mcu.update_type,
                                  by_matter_mcu.content_length) != OTA_SUCCESS) {
                    by_matter_mcu.version_check = OTA_VERSION_INCOMPATI;
                    status = OTA_VERSION_INCOMPATI;
                    goto finish;
                }
            }
        }
#endif // (DISABLE_OTA_VER_CHK)

        by_matter_mcu.received_length += rev_data_len;
        status = ota_update_buffer_write_sflash(&by_matter_mcu.write, rev_data, rev_data_len);

        if (status != OTA_SUCCESS) {
            PRINTF("[%s] Failed to write data to sflash(0x%02x)\n", __func__, status);
            goto finish;
        }

        if ((by_matter_mcu.received_length > 0) && (by_matter_mcu.content_length > 0)) {
            progress = (by_matter_mcu.received_length * 100) / by_matter_mcu.content_length;

            PRINTF("\r   >> By MCU Downloading... %d % (%d/%d Bytes)%s",
                progress,
                by_matter_mcu.received_length,
                by_matter_mcu.content_length, progress == 100 ? "\n" : " ");
            ota_update_progress_rtos  = progress;
        } else {
            progress = 0;
        }
    }

    finish:
    PRINTF_ATCMD("\r\n+FWUPDATE:0x%02x\r\n", status);

    if ((status != OTA_SUCCESS) || progress == 100) {
        xTimerStop(ota_rev_ack_timer, 0);
        xTimerDelete(ota_rev_ack_timer, 0);
        memset(&ota_rev_ack_timer, 0, sizeof(TimerHandle_t));

        if (progress == 100) {
            ota_update_print_status(by_matter_mcu.update_type, OTA_SUCCESS);
            by_matter_mcu.received_length = 0;
            by_matter_mcu.write.sflash_addr = 0;
            by_matter_mcu.write.total_length = 0;
            by_matter_mcu.write.offset = 0;
            by_matter_mcu.version_check = OTA_NOT_FOUND;
            by_matter_mcu.content_length = 0;
            by_matter_mcu.received_length = 0;
            ota_update_renew_type = by_matter_mcu.update_type;
        }
        by_matter_mcu.update_type = OTA_TYPE_INIT;
        by_matter_mcu.download_status = OTA_STATE_READY;
        PRINTF_ATCMD("\r\n+NWOTABYMCU:0x%02x\r\n", status);
    }

    return status;
}
#endif // (__IMG_UPDATE_BY_MCU__)
#endif // (__SUPPORT_OTA__)
#endif // (CHIP_DEVICE_CONFIG_ENABLE_OTA)

