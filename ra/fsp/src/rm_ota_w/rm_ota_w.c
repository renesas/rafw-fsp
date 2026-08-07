/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include <inttypes.h>
#include "bsp_api.h"
#include "bsp_clocks.h"
#include "rm_ota_w.h"
#include "r_ospi_w.h"
#include "rm_ota_w_util_api.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_cert.h"
#include "common_def.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define BIT(x) (1UL << (x))
#define CRC_PRELOAD                     0xFFFF
#define CRC16_CCITT                     0x1021

#define PRODUCT_HEADER_CRC_OFFSET       29
#define ACTIVE_ADDR_OFFSET              2
#define UPGRADE_ADDR_OFFSET             6
#define BOOTER_RTM_SYNC_NOT_DONE        0U
#define BOOTER_RTM_SYNC_DONE            1U

#define RM_OTA_W_SWAP_TASK_NAME         "OTA_SWAP_REBOOT"
#define RM_OTA_W_SWAP_STACK_SIZE        (512)
#define RM_OTA_W_SWAP_TASK_PRI          (OS_TASK_PRIORITY_USER + 2)

extern const unsigned int rm_ota_w_update_crc32_tab[];
static uint8_t g_booter_rtm_sync_done = BOOTER_RTM_SYNC_NOT_DONE;

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef struct  BOOTER_RETENTION_INFO
{
    uint32_t tin_wakeup_source;
    uint32_t tin_tim_app_address;
    uint32_t tin_gpio_app_address;
    uint32_t tin_sensor_app_address;

    uint32_t tin_fw_active_address;
    uint32_t tin_fw_update_address;
    uint32_t tin_fw_running_address;

    /* more */
} RETENTION_MEM_type;

/***********************************************************************************************************************
 * Private function declarations
 **********************************************************************************************************************/
static uint32_t rm_ota_w_offset_address_complement(uint32_t offset);
static void rm_ota_w_init_download_info(rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type, uint32_t offset);
static UINT rm_ota_w_set_proc_state(rm_ota_w_instance_ctrl_t * p_ctrl, UINT state);
static uint8_t rm_ota_w_get_download_progress(rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type);
static void rm_ota_w_set_download_progress (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t type, UINT progress);
static void rm_ota_w_download_notify(rm_ota_w_update_type_t update_type, uint32_t ret_status, uint32_t progress);
static void rm_ota_w_swap_notify(uint32_t status);
static void rm_ota_w_swap_reboot_countdown(void * arg);
static UINT rm_ota_w_start_swap(rm_ota_w_instance_ctrl_t * p_ctrl);
static UINT rm_ota_w_current_fw_swap(rm_ota_w_instance_ctrl_t * p_ctrl);
static UINT rm_ota_w_read_current_fw_version(UINT fw_addr, rm_ota_w_fw_version_info_t * fw_ver);
static UINT rm_ota_w_parse_version_string (UCHAR * version, rm_ota_w_fw_version_info_t * fw_ver);
static UINT rm_ota_w_check_version(rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type, UCHAR * data, UINT data_len);
static UINT rm_ota_w_compare_fw_version(rm_ota_w_update_type_t update_type,
                                       rm_ota_w_fw_version_info_t cur_ver,
                                       rm_ota_w_fw_version_info_t new_ver);
static UINT rm_ota_w_set_user_sflash_addr(rm_ota_w_instance_ctrl_t * p_ctrl, UINT sflash_addr);
static UINT rm_ota_w_read_new_fw_version(UCHAR * data, rm_ota_w_fw_version_info_t * fw_ver);
static UINT rm_ota_w_check_refuse_flag(rm_ota_w_instance_ctrl_t * p_ctrl);
static void rm_ota_w_print_fw_header(rm_ota_w_image_header_data_t * info_image);
static fsp_err_t rm_ota_w_check_state(rm_ota_w_instance_ctrl_t * p_ctrl);
static uint8_t rm_ota_w_get_image_info(uint32_t sector_addr, rm_ota_w_image_header_data_t * info_image);
static UINT rm_ota_w_process_swap(rm_ota_w_instance_ctrl_t * p_ctrl, bool force);
static uint8_t rm_ota_w_set_boot_index(uint8_t boot_idx);
static uint8_t rm_ota_w_get_boot_index(void);
static uint8_t rm_ota_w_get_booter_rtm_index(void);
static uint8_t rm_ota_w_sync_and_get_boot_index(void);
static UINT rm_ota_w_get_curr_sflash_addr(rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type);
static UINT rm_ota_w_get_new_sflash_addr(rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type);
static uint8_t rm_ota_w_toggle_boot_index(rm_ota_w_instance_ctrl_t * p_ctrl);
static uint8_t rm_ota_w_sflash_product_header_crc(void);
static uint8_t rm_ota_w_sflash_rtos_crc(rm_ota_w_instance_ctrl_t * p_ctrl, uint32_t sector_addr);
static size_t rm_ota_w_read_flash(uint32_t addr, void * buf, uint32_t len);
static size_t rm_ota_w_erase_flash(uint32_t sflash_addr, int len);
static size_t rm_ota_w_write_flash(uint32_t addr, void * buf, uint32_t len);
static void rm_ota_w_system_reboot(unsigned int flag);
static uint32_t rm_ota_w_get_mcu_fw_info(rm_ota_w_instance_ctrl_t * p_ctrl, char * name, uint32_t * size, uint32_t * crc);
const char * rm_ota_w_type_to_text(rm_ota_w_update_type_t update_type);

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/
const ota_api_t g_ota_on_ota_w =
{
    .open               = RM_OTA_W_Open,
    .swap               = RM_OTA_W_Swap,
    .getImageInfo       = RM_OTA_W_GetImageInfo,
    .bootIdxSet         = RM_OTA_W_BootIdxSet,
    .bootIdxGet         = RM_OTA_W_BootIdxGet,
    .getAddr            = RM_OTA_W_GetAddr,
    .setAddr            = RM_OTA_W_SetAddr,
    .cert               = RM_OTA_W_Cert,
    .close              = RM_OTA_W_Close,
};

/*******************************************************************************************************************//**
 * @addtogroup RM_OTA_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Initializes the high performance OTA peripheral. Implements @ref ota_api_t::open.
 *
 * The Open function initializes the ota.
 *
 * Example:
 * @snippet rm_ota_w_example.c RM_OTA_W_Open
 *
 * @retval     FSP_SUCCESS                  Initialization was successful and timer has started.
 * @retval     FSP_ERR_ALREADY_OPEN         The OTA control block is already open.
 * @retval     FSP_ERR_ASSERTION            NULL provided for p_ctrl or p_cfg.
 * @retval     FSP_ERR_IRQ_BSP_DISABLED     Caller is requesting BGO but the OTA interrupts are not enabled.
 * @retval     FSP_ERR_FCLK                 FCLK must be a minimum of 4 MHz for OTA operations.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_Open (ota_ctrl_t * const p_api_ctrl, ota_cfg_t const * const p_cfg)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (RM_OTA_W_CFG_PARAM_CHECKING_ENABLE)

    /* If null pointers return error. */
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_ctrl);

    /* If open return error. */
    FSP_ERROR_RETURN((RM_OTA_W_OPEN != p_ctrl->opened), FSP_ERR_ALREADY_OPEN);

    /* Background operations for data flash are enabled but the flash interrupt is disabled. */
    if (p_cfg->data_ota_bgo)
    {
        FSP_ERROR_RETURN(p_cfg->irq >= (IRQn_Type) 0, FSP_ERR_IRQ_BSP_DISABLED);
        FSP_ERROR_RETURN(p_cfg->err_irq >= (IRQn_Type) 0, FSP_ERR_IRQ_BSP_DISABLED);
    }
#endif

    /* Set the parameters struct based on the user supplied settings */
    p_ctrl->p_cfg = p_cfg;

    if (true == p_cfg->data_ota_bgo)
    {
        p_ctrl->p_callback        = p_cfg->p_callback;
        p_ctrl->p_context         = p_cfg->p_context;
        p_ctrl->p_callback_memory = NULL;

        /* Enable FCU interrupts. */
        R_BSP_IrqCfgEnable(p_cfg->irq, p_cfg->ipl, p_ctrl);

        /* Enable Error interrupts. */
        R_BSP_IrqCfgEnable(p_cfg->err_irq, p_cfg->err_ipl, p_ctrl);
    }

    rm_ota_w_init_download_info(p_ctrl, RM_OTA_W_TYPE_INIT, 0);
    rm_ota_w_set_proc_state(p_ctrl, RM_OTA_W_STATE_READY);
    rm_ota_w_set_download_progress (p_ctrl, RM_OTA_W_TYPE_RTOS, 0);
    rm_ota_w_set_download_progress (p_ctrl, RM_OTA_W_TYPE_BLE_FW, 0);
    rm_ota_w_set_download_progress (p_ctrl, RM_OTA_W_TYPE_MCU_FW, 0);
    rm_ota_w_set_download_progress (p_ctrl, RM_OTA_W_TYPE_CERT_KEY, 0);
#if defined (__SUPPORT_MATTER_IOT__)
    rm_ota_w_set_download_progress (p_ctrl, RM_OTA_W_TYPE_MCU_FW_STREAM, 0);
#endif

    /* Set user default address for custom objects */
    p_ctrl->ota_custom_address = RM_OTA_W_STOR_USER_START;

    /* Set MCU FW default name */
    memcpy(p_ctrl->mcu_fw_name, RM_OTA_W_MCU_FW_NAME, sizeof(RM_OTA_W_MCU_FW_NAME));

    /* If successful mark the control block as open. Otherwise release the hardware lock. */
    p_ctrl->opened = RM_OTA_W_OPEN;

    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return err;
}

/*******************************************************************************************************************//**
 * Releases any resources that were allocated by the Open() or any subsequent OTA operations.
 * 
 * Implements @ref ota_api_t::close.
 *
 * @retval     FSP_SUCCESS              Successfully close.
 * @retval     FSP_ERR_NOT_OPEN         The control block is not open.
 * @retval     FSP_ERR_ASSERTION        NULL provided for p_ctrl or p_cfg.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_Close (ota_ctrl_t * const p_api_ctrl)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

#if (RM_OTA_W_CFG_PARAM_CHECKING_ENABLE)
    /* If null pointer return error. */
    FSP_ASSERT(NULL != p_ctrl);

    /* If the flash api is not open return an error. */
    FSP_ERROR_RETURN(RM_OTA_W_OPEN == p_ctrl->opened, FSP_ERR_NOT_OPEN);
#endif

    /* Close the API */
    p_ctrl->opened = RM_OTA_W_CLOSE;

    return err;
}

/*******************************************************************************************************************//**
 * Trigger versions swap. Swap the programs located at address 0x00002000 and address 0x00400000. 
 * Before version swap is done, there will be version check and boot index toggle.
 * 
 * Implements @ref ota_api_t::swap.
 *
 * @retval  FSP_SUCCESS                 Version swap finished successfully.
 * @retval  FSP_ERR_ASSERTION           NULL provided for p_ctrl.
 * @retval  FSP_ERR_IN_USE              OTA process is in progress, we need to wait for last process to finished.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_Swap (ota_ctrl_t * const p_api_ctrl)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    /* Update_type force */
    err = rm_ota_w_process_swap(p_ctrl, RM_OTA_W_SWAP_FORCED);

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Returns the information about the image installed in a specific address.
 * 
 * Implements @ref ota_api_t::getImageInfo.
 *
 * @retval  FSP_SUCCESS                 Callback updated successfully.
 * @retval  FSP_ERR_INVALID_ADDRESS     Invalid image address was input.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_GetImageInfo (ota_ctrl_t * const p_api_ctrl,
                           rm_ota_w_update_type_t update_type,
                           uint32_t sector_addr,
                           rm_ota_w_image_header_data_t * info_image)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;
    uint32_t fw_size = 0;

    if (update_type == RM_OTA_W_TYPE_RTOS)
    {
        if (rm_ota_w_get_image_info(sector_addr, info_image) == 0)
        {
            err = FSP_ERR_INVALID_ADDRESS;
        }
    }
    else if (update_type == RM_OTA_W_TYPE_MCU_FW)
    {
        return (rm_ota_w_get_mcu_fw_info(p_ctrl, NULL, &fw_size, NULL));
    }
    else
    {
        err = FSP_ERR_INVALID_ADDRESS;
    }

    return err;
}

/*******************************************************************************************************************//**
 * Updates the boot index value.
 * 
 * Implements @ref ota_api_t::bootIdxSet.
 *
 * @retval  FSP_SUCCESS                 Successfully set the request data.
 * @retval  FSP_ERR_INVALID_ADDRESS     Invalid boot index value
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_BootIdxSet (ota_ctrl_t * const p_api_ctrl, uint8_t boot_idx)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    if (boot_idx == RM_OTA_W_BOOT_IDX_TOGGLE)
    {
        err = rm_ota_w_toggle_boot_index(p_ctrl);
    }
    else
    {
        err = rm_ota_w_set_boot_index(boot_idx);
    }

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Query the current boot index.
 * 
 * Note: This function may also update/synchronize the boot index before returning it.
 * 
 * Implements @ref ota_api_t::bootIdxGet.
 *
 * @retval  FSP_SUCCESS                 Successfully retrieved the request information.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_BootIdxGet (ota_ctrl_t * const p_api_ctrl, uint8_t * boot_idx)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    FSP_PARAMETER_NOT_USED(p_ctrl);

    *boot_idx = rm_ota_w_sync_and_get_boot_index();

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Query the address information regarding the current version address and swap version address.
 * 
 * Implements @ref ota_api_t::getAddr.
 *
 * @retval  FSP_SUCCESS                 Successfully retrieved the request information.
 * @retval  FSP_ERR_INVALID_ADDRESS     Invalid state address value
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_GetAddr (ota_ctrl_t * const p_api_ctrl, uint8_t state, rm_ota_w_update_type_t update_type, uint32_t * addr)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    switch (state)
    {
        case RM_OTA_W_CURRENT_ADDR:
        *addr = rm_ota_w_get_curr_sflash_addr(p_ctrl, update_type);
        break;

        case RM_OTA_W_NEW_ADDR:
        *addr = rm_ota_w_get_new_sflash_addr(p_ctrl, update_type);
        break;

        default:
        err = FSP_ERR_INVALID_ADDRESS;
        break;
    }

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Set Address value for user device.
 * 
 * Implements @ref ota_api_t::setAddr.
 *
 * @retval  FSP_SUCCESS                 Key variable was changed successfully.
 * @retval  FSP_ERR_INVALID_ADDRESS     Invalid data flash address was input.
 * @retval  FSP_ERR_INVALID_DATA        Invalid key type or unknown data source.
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_SetAddr (ota_ctrl_t * const p_api_ctrl, uint8_t update_type, uint32_t addr)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    switch(update_type)
    {
        case RM_OTA_W_USER_ADDR:
        err = rm_ota_w_set_user_sflash_addr(p_ctrl, addr);
        break;

        default:
        return FSP_ERR_INVALID_DATA;
    }

    /* Return status. */
    return err;
}

/*******************************************************************************************************************//**
 * Performs a certification check over the program data, base program or downloaded data based on a specified address
 * area.
 * 
 * Implements @ref ota_api_t::cert.
 *
 * @retval  FSP_SUCCESS                 Operation check passed successfully.
 * @retval  FSP_ERR_INVALID_DATA        Operation check failed due to calculation failure
 **********************************************************************************************************************/
fsp_err_t RM_OTA_W_Cert(ota_ctrl_t * const p_api_ctrl, uint8_t cert_type, uint32_t sector_addr)
{
    rm_ota_w_instance_ctrl_t * p_ctrl = (rm_ota_w_instance_ctrl_t *) p_api_ctrl;
    fsp_err_t err = FSP_SUCCESS;

    switch (cert_type)
    {
        case RM_OTA_W_VALIDATE_TYPE_PROD_CRC:
        err = rm_ota_w_sflash_product_header_crc();
        break;

        case RM_OTA_W_VALIDATE_TYPE_IMG_CRC:
        err = rm_ota_w_sflash_rtos_crc(p_ctrl, sector_addr);
        break;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup RM_OTA_W)
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Private Functions
 **********************************************************************************************************************/
static uint32_t rm_ota_w_offset_address_complement (uint32_t offset)
{
    uint32_t ret = 0;

    /* Address offset field must be a multiple of 4096 due to 32-byte aligned of the memory block */
    ret = (offset > 0) ? (offset + 0x0FFF) & ~0x0FFF : 0x00;

    return ret;
}

static void rm_ota_w_init_download_info (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type, uint32_t offset)
{
    p_ctrl->download_info.update_type = update_type;
    p_ctrl->download_info.received_length = 0;
    p_ctrl->download_info.write.total_length = 0;
    p_ctrl->download_info.write.length = 0;
    p_ctrl->download_info.write.offset = 0;
    p_ctrl->download_info.download_status = RM_OTA_W_SUCCESS;
    p_ctrl->download_info.version_check = RM_OTA_W_NOT_FOUND;
    p_ctrl->download_info.content_length = 0;
    p_ctrl->download_info.received_length = 0;

#if defined (__OTA_UPDATE_MCU_FW__)
    if (p_ctrl->download_info.update_type == RM_OTA_W_TYPE_MCU_FW)
    {
        p_ctrl->download_info.write.sflash_addr = rm_ota_w_get_new_sflash_addr(p_ctrl, p_ctrl->download_info.update_type);
        p_ctrl->download_info.write.offset += RM_OTA_W_MCU_FW_HEADER_SIZE;
    }
    else
    {
        p_ctrl->download_info.write.sflash_addr = rm_ota_w_get_new_sflash_addr(p_ctrl, p_ctrl->download_info.update_type);
        p_ctrl->download_info.write.sflash_addr += rm_ota_w_offset_address_complement(offset);
    }
#else
    p_ctrl->download_info.write.sflash_addr = rm_ota_w_get_new_sflash_addr(p_ctrl, p_ctrl->download_info.update_type);
    p_ctrl->download_info.write.sflash_addr += rm_ota_w_offset_address_complement(offset);
#endif
    p_ctrl->source_start_offset = rm_ota_w_offset_address_complement(offset);
}

static size_t rm_ota_w_erase_flash (uint32_t sflash_addr, int len)
{
    if (rm_ota_w_util_api_sflash_erase(sflash_addr, len) != pdTRUE)
    {
        /* Error */
        return 0; 
    }

    return len;
}

static size_t rm_ota_w_write_flash (uint32_t addr, void * buf, uint32_t len)
{
    if (rm_ota_w_util_api_sflash_write(addr, buf, len) != pdTRUE)
    {
        return 0;
    }

    return len;
}

static size_t rm_ota_w_read_flash (uint32_t addr, void * buf, uint32_t len)
{
    rm_ota_w_util_api_sflash_read(addr, buf, len);

    return len;
}

static uint32_t rm_ota_w_crc16 (uint32_t crcValue, unsigned char newByte)
{
    for (unsigned char i = 0; i < 8; i++)
    {
        if (((crcValue & 0x8000) >> 8) ^ (newByte & 0x80))
        {
            crcValue = (crcValue << 1)  ^ CRC16_CCITT;
        }
        else
        {
            crcValue = (crcValue << 1);
        }

        newByte <<= 1;
    }
    return crcValue;
}

static uint16_t rm_ota_w_calc_crc16 (uint8_t * data, uint32_t size)
{
    uint32_t crc_calc;

    crc_calc = CRC_PRELOAD;

    for(unsigned char i = 0; i < size; i++)
    {
        crc_calc = rm_ota_w_crc16(crc_calc, data[i]);
    }

    return (crc_calc & 0xFFFF);
}

static UINT rm_ota_w_check_refuse_flag (rm_ota_w_instance_ctrl_t * p_ctrl)
{
    return p_ctrl->ota_refuse_flag;
}

static uint8_t rm_ota_w_get_image_info (uint32_t sector_addr, rm_ota_w_image_header_data_t * info_image)
{
    return rm_ota_w_read_flash(sector_addr, (void *) info_image, sizeof(rm_ota_w_image_header_data_t));
}

static void rm_ota_w_print_fw_header (rm_ota_w_image_header_data_t * info_image)
{
    RM_OTA_W_INFO("\t*Magic---------- %04X\n", (UINT)info_image->magic_code);
    RM_OTA_W_INFO("\t*Version-------- %04X\n", (UINT)info_image->version);
    if (info_image->magic_code == IMAGE_HEADER_MAGIC_CODE)
    {
        RM_OTA_W_INFO("\t*Name----------- %s\n", info_image->name);
    }
    else
    {
        RM_OTA_W_INFO("\t*Name----------- NONE\n");
    }
    RM_OTA_W_INFO("\t*Data Size------ %u\n", (UINT)info_image->size);
    RM_OTA_W_INFO("\t*HCRC----------- 0x%x\n", (unsigned int)(info_image->header_crc));
    RM_OTA_W_INFO("\t*IVT------------ 0x%x\n", (unsigned int)(info_image->ivt_location));
    RM_OTA_W_INFO("\t*DCRC----------- 0x%x\n\n", (unsigned int)(info_image->crc));
}

static uint8_t rm_ota_w_sflash_rtos_crc(rm_ota_w_instance_ctrl_t * p_ctrl, uint32_t sector_addr)
{
    uint32_t addr_offset = 0;
    uint32_t tot_len = 0;
    uint32_t cal_len = 0;
    uint32_t cal_crc = 0;
    uint32_t crc;
    uint32_t retry_crc = 0;
    size_t size;
    UINT status = RM_OTA_W_SUCCESS;
    rm_ota_w_image_header_data_t info_image;
    unsigned char * buf = NULL;

    buf = RM_OTA_W_MALLOC(RM_OTA_W_SFLASH_BUF_SZ);

    if (buf == NULL)
    {
        RM_OTA_W_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, RM_OTA_W_SFLASH_BUF_SZ);
        return RM_OTA_W_MEM_ALLOC_FAILED;
    }

    if (rm_ota_w_get_image_info(sector_addr, &info_image))
    {
        if (RM_OTA_W_GetImageInfo(p_ctrl, RM_OTA_W_TYPE_RTOS, sector_addr, &info_image) == FSP_SUCCESS)
        {
            RM_OTA_W_INFO("- RTOS (addr = 0x%lx)\n", sector_addr);
            rm_ota_w_print_fw_header(&info_image);
        }
    }
    else
    {
        RM_OTA_W_ERR("[%s:%d] Failed to get image info\n", __func__, __LINE__);
        status = RM_OTA_W_FAILED;
        goto finish;
    }

    if ((info_image.ivt_location == RM_OTA_W_STOR_UNKNOWN_ADDR) || (info_image.size == RM_OTA_W_CONTENT_LEN_INVALID))
    {
        RM_OTA_W_ERR("[%s:%d] Invalid image info\n", __func__, __LINE__);
        status = RM_OTA_W_FAILED;
        goto finish;
    }

retry:
    addr_offset = sector_addr + info_image.ivt_location;
    tot_len = (int) info_image.size;
    crc = ~0U;

    while (tot_len > 0)
    {
        if (tot_len > RM_OTA_W_SFLASH_BUF_SZ)
        {
            cal_len = RM_OTA_W_SFLASH_BUF_SZ;
        }
        else
        {
            cal_len = tot_len;
        }

        memset(buf, 0x00, RM_OTA_W_SFLASH_BUF_SZ);
        rm_ota_w_read_flash(addr_offset, (uint8_t *) buf, cal_len);

        uint8_t * p = buf;
        size = cal_len;

        while (size--)
        {
            crc = rm_ota_w_update_crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }

        addr_offset += cal_len;
        tot_len -= cal_len; 
    }

    cal_crc = crc ^ ~0U;

    if (info_image.crc != cal_crc)
    {
        if (retry_crc++ <= 2)
        {
            RM_OTA_W_ERR("\tRecalculate due to CRC error(%ld)\n", retry_crc);
            goto retry;
        }

        RM_OTA_W_ERR("  CRC: CRC mismatch!!(0x%lx != 0x%lx)\n", info_image.crc, cal_crc);
        status = RM_OTA_W_ERROR_CRC;
    }
    else
    {
        RM_OTA_W_INFO("\tDCRC(calc)----- 0x%lx\n", cal_crc);
    }

finish:
    if (buf != NULL)
    {
        RM_OTA_W_FREE(buf);
    }

    return status;
}

static uint8_t rm_ota_w_sflash_product_header_crc (void)
{
    uint16_t read_crc = 0;
    uint16_t cal_crc;
    unsigned char header_crc[2] = {0x00, };
    unsigned char * buf = NULL;

    buf = RM_OTA_W_MALLOC(SF_PRODUCT_HDR_SIZE);

    if (buf == NULL)
    {
        RM_OTA_W_ERR("[%s:%d] Failed to allocate buffer(%d bytes)\n", __func__, __LINE__, SF_PRODUCT_HDR_SIZE);

        return RM_OTA_W_MEM_ALLOC_FAILED;
    }

    memset(buf, 0x00, SF_PRODUCT_HDR_SIZE);
    rm_ota_w_read_flash(SF_PRODUCT_HDR, buf, SF_PRODUCT_HDR_SIZE);

    rm_ota_w_read_flash(SF_PRODUCT_HDR + PRODUCT_HEADER_CRC_OFFSET, header_crc, sizeof(header_crc));
    read_crc = ((int) header_crc[0] << 0) | ((int) header_crc[1] << 8);
    RM_OTA_W_INFO("CRC: <0x%08x> Read PRODUCT_HDR CRC = 0x%08x\n", SF_PRODUCT_HDR + PRODUCT_HEADER_CRC_OFFSET, read_crc);

    cal_crc = rm_ota_w_calc_crc16(buf, PRODUCT_HEADER_CRC_OFFSET);
    RM_OTA_W_INFO("CRC: Calculated PRODUCT_HDR CRC = 0x%08x\n", cal_crc);

    if (buf != NULL)
    {
        RM_OTA_W_FREE(buf);
        buf = NULL;
    }

    if (read_crc != cal_crc)
    {
        RM_OTA_W_ERR("CRC: CRC mismatch!!\n");
        return RM_OTA_W_ERROR_CRC;
    }

    return RM_OTA_W_SUCCESS;
}

static UINT rm_ota_w_set_proc_state (rm_ota_w_instance_ctrl_t * p_ctrl, UINT state)
{
    p_ctrl->update_state = state;
    return p_ctrl->update_state;
}

static void rm_ota_w_download_notify (rm_ota_w_update_type_t update_type, uint32_t ret_status, uint32_t progress)
{
    FSP_PARAMETER_NOT_USED(update_type);
    FSP_PARAMETER_NOT_USED(progress);
    RM_OTA_W_DBG("[%s] status = 0x%02lx\n", __func__, ret_status);
}

static void rm_ota_w_swap_notify (uint32_t status)
{
    RM_OTA_W_DBG("[%s] status = 0x%02lx\n", __func__, status);
}

static UINT rm_ota_w_get_curr_sflash_addr (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type)
{
    uint8_t current_boot_idx;

    if (update_type == RM_OTA_W_TYPE_INIT)
    {
        return RM_OTA_W_SUCCESS;
    }

    if (update_type >= RM_OTA_W_TYPE_UNKNOWN)
    {
        RM_OTA_W_ERR("- OTA: Unknown FW type\n");

        return RM_OTA_W_ERROR_TYPE;
    }

    if (update_type == RM_OTA_W_TYPE_RTOS)
    {
        RM_OTA_W_BootIdxGet(p_ctrl, &current_boot_idx);

        if (current_boot_idx == RM_OTA_W_BOOT_IDX_1)
        {
            return RM_OTA_W_STOR_RTOS_1_ADDR;
        }
        else
        {
            return RM_OTA_W_STOR_RTOS_0_ADDR;
        }

    }
    else if ((update_type == RM_OTA_W_TYPE_MCU_FW) || (update_type == RM_OTA_W_TYPE_CERT_KEY))
    {
        return p_ctrl->ota_custom_address;
    }

    RM_OTA_W_ERR("- OTA: Wrong FW type (%d)\n", update_type);

    return RM_OTA_W_STOR_UNKNOWN_ADDR;
}

static UINT rm_ota_w_get_new_sflash_addr (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type)
{
    uint8_t current_boot_index;

    if (update_type == RM_OTA_W_TYPE_INIT)
    {
        return RM_OTA_W_SUCCESS;
    }

    if (update_type >= RM_OTA_W_TYPE_UNKNOWN)
    {
        RM_OTA_W_ERR("- OTA: Unknown FW type\n");

        return RM_OTA_W_ERROR_TYPE;
    }

    if (update_type == RM_OTA_W_TYPE_RTOS)
    {
        RM_OTA_W_BootIdxGet(p_ctrl, &current_boot_index);

        if (current_boot_index == RM_OTA_W_BOOT_IDX_1)
        {
            return RM_OTA_W_STOR_RTOS_0_ADDR;
        }
        else
        {
            return RM_OTA_W_STOR_RTOS_1_ADDR;
        }
    }
#if defined (__SUPPORT_MATTER_IOT__) && defined (__SUPPORT_ATCMD__)
    else if (update_type == RM_OTA_W_TYPE_MCU_FW_STREAM)
    {
        return RM_OTA_W_SUCCESS;
    }
#endif
    else if ((update_type == RM_OTA_W_TYPE_MCU_FW) || (update_type == RM_OTA_W_TYPE_CERT_KEY))
    {
        return p_ctrl->ota_custom_address;
    }

    RM_OTA_W_ERR("- OTA: Wrong FW type (%d)\n", update_type);

    return RM_OTA_W_STOR_UNKNOWN_ADDR;
}

static UINT rm_ota_w_parse_version_string (UCHAR * version, rm_ota_w_fw_version_info_t * fw_ver)
{
    CHAR * p_rev_a = NULL;
    CHAR * p_rev_b = NULL;
    UINT str_len = 0;
    UINT total_str_len = 0;
    UINT sum_str_len = 0;
    UINT status = RM_OTA_W_SUCCESS;

    if (version == NULL || fw_ver == NULL)
    {
        return RM_OTA_W_FAILED;
    }

    memset(fw_ver, 0x00, sizeof(rm_ota_w_fw_version_info_t));

    total_str_len = strlen((char *) version);

    /* Extract FW_Type */
    p_rev_a = (CHAR *) version;
    p_rev_b = (CHAR *) strstr((char *) p_rev_a, RM_OTA_W_VER_DELIMITER);
    if (p_rev_b == NULL)
    {
        RM_OTA_W_ERR("  > Delimiter (%s) not found\n", RM_OTA_W_VER_DELIMITER);
        return RM_OTA_W_FAILED;
    }

    str_len = p_rev_b - p_rev_a;
    if (str_len > RM_OTA_W_UPDATE_TYPE_MAX)
    {
        RM_OTA_W_ERR("  > FW_TYPE is too long (max = %d)\n", RM_OTA_W_UPDATE_TYPE_MAX);
        status = RM_OTA_W_FAILED;
    }
    memcpy(fw_ver->update_type, p_rev_a, str_len > RM_OTA_W_UPDATE_TYPE_MAX ? RM_OTA_W_UPDATE_TYPE_MAX : str_len);
    sum_str_len += str_len;

    /* Extract Module name */
    p_rev_b ++;
    sum_str_len ++;
    p_rev_a = (CHAR *) strstr((char *) p_rev_b, RM_OTA_W_VER_DELIMITER);
    if (p_rev_a == NULL)
    {
        RM_OTA_W_ERR("  > Delimiter (%s) not found\n", RM_OTA_W_VER_DELIMITER);
        return RM_OTA_W_FAILED;
    }

    str_len = p_rev_a - p_rev_b;
    sum_str_len += str_len;
    if (str_len > RM_OTA_W_MODULE_MAX)
    {
        RM_OTA_W_ERR("  > Module name is too long (max = %d)\n", RM_OTA_W_MODULE_MAX);
        status = RM_OTA_W_FAILED;
    }
    memcpy(fw_ver->module, p_rev_b, str_len > RM_OTA_W_MODULE_MAX ? RM_OTA_W_MODULE_MAX : str_len);

    /* Extract SDK version */
    p_rev_a ++;
    sum_str_len ++;
    p_rev_b = (CHAR *) strstr((char *) p_rev_a, RM_OTA_W_VER_DELIMITER);
    if (p_rev_b == NULL)
    {
        RM_OTA_W_ERR("  > Delimiter (%s) not found\n", RM_OTA_W_VER_DELIMITER);
        return RM_OTA_W_FAILED;
    }

    str_len = p_rev_b - p_rev_a;
    sum_str_len += str_len;
    if (str_len > RM_OTA_W_SDK_MAX)
    {
        RM_OTA_W_ERR("  > SDK version is too long (max = %d)\n", RM_OTA_W_SDK_MAX);
        status = RM_OTA_W_FAILED;
    }
    memcpy(fw_ver->sdk, p_rev_a, str_len > RM_OTA_W_SDK_MAX ? RM_OTA_W_SDK_MAX : str_len);

    /* Extract Customer Version */
    p_rev_b ++;
    sum_str_len ++;
    str_len = total_str_len - sum_str_len;
    if (str_len > RM_OTA_W_CUSTOMER_MAX)
    {
        RM_OTA_W_ERR("  > CUSTOMER is too long (max = %d)\n", RM_OTA_W_CUSTOMER_MAX);
        status = RM_OTA_W_FAILED;
    }
    memcpy(fw_ver->customer, p_rev_b, str_len > RM_OTA_W_CUSTOMER_MAX ? RM_OTA_W_CUSTOMER_MAX : str_len);

    if (!strlen((char *) fw_ver->update_type)
     || !strlen((char *) fw_ver->module)
     || !strlen((char *) fw_ver->sdk)
     || !strlen((char *) fw_ver->customer))
    {
        memset(fw_ver, 0x00, sizeof(rm_ota_w_fw_version_info_t));
        return RM_OTA_W_FAILED;
    }

    return status;
}

static UINT rm_ota_w_read_current_fw_version(UINT fw_addr, rm_ota_w_fw_version_info_t * fw_ver)
{
    rm_ota_w_image_header_data_t info_image = { 0, };

    if (fw_ver == NULL)
    {
        return RM_OTA_W_FAILED;
    }

    /* Read sflash */
    if (rm_ota_w_get_image_info(fw_addr, &info_image) == 0)
    {
        return RM_OTA_W_FAILED;
    }

    /* Parsing version name */
    if (rm_ota_w_parse_version_string(info_image.name, fw_ver))
    {
        RM_OTA_W_ERR("   > Failed to parse Current FW version : %s \n", info_image.name);
    }

    return RM_OTA_W_SUCCESS;
}

static UINT rm_ota_w_read_new_fw_version (UCHAR * data, rm_ota_w_fw_version_info_t * fw_ver)
{
    UCHAR new_ver[IMAGE_HEADER_NAME_LEN] = {0x00, };

    if (data == NULL)
    {
        return RM_OTA_W_FAILED;
    }

    memset(new_ver, 0x00, IMAGE_HEADER_NAME_LEN);
    memcpy(new_ver, &data[RM_OTA_W_VER_START_OFFSET], IMAGE_HEADER_NAME_LEN);

    /* Parse received version */
    if (rm_ota_w_parse_version_string(new_ver, fw_ver))
    {
        RM_OTA_W_ERR("   > Failed to parse Server FW version : %s \n", new_ver);

        return RM_OTA_W_VERSION_UNKNOWN;
    }
    else
    {
        RM_OTA_W_INFO("   > Server FW version : %s-%s-%s-%s \n", 
                    fw_ver->update_type, fw_ver->module, fw_ver->sdk, fw_ver->customer);
    }

    return RM_OTA_W_SUCCESS;
}

static UINT rm_ota_w_set_user_sflash_addr (rm_ota_w_instance_ctrl_t * p_ctrl, UINT sflash_addr)
{
    /* Check the FLASH address */
    if ((sflash_addr >= SF_TLS_CERT_BASE_ADDR) ||
        ((sflash_addr >= SF_USER_AREA) && (sflash_addr < SF_PARTITION_TBL)))
    {
        RM_OTA_W_INFO("- OTA : download_sflash_addr = 0x%x \n", sflash_addr);

        p_ctrl->ota_custom_address = sflash_addr;
    }
    else
    {
        RM_OTA_W_ERR("- OTA : sflash address(0x%x) is incorrect \n", sflash_addr);

        return RM_OTA_W_ERROR_SFLASH_ADDR;
    }

    return RM_OTA_W_SUCCESS;
}

const char * rm_ota_w_type_to_text (rm_ota_w_update_type_t update_type)
{
    if (update_type == RM_OTA_W_TYPE_RTOS)
    {
        return RM_OTA_W_RTOS_NAME;
    }
    else if (update_type == RM_OTA_W_TYPE_MCU_FW)
    {
        return RM_OTA_W_MCU_FW_NAME;
#if defined (__SUPPORT_MATTER_IOT__) && defined (__SUPPORT_ATCMD__)
    }
    else if (update_type == RM_OTA_W_TYPE_MCU_FW_STREAM)
    {
        return RM_OTA_W_MCU_FW_NAME;
#endif
    }
    else if (update_type == RM_OTA_W_TYPE_CERT_KEY)
    {
        return RM_OTA_W_CERT_KEY_NAME;
    }

    return "UNKNOWN";
}

static UINT rm_ota_w_compare_fw_version (rm_ota_w_update_type_t update_type,
                                        rm_ota_w_fw_version_info_t cur_ver,
                                        rm_ota_w_fw_version_info_t new_ver)
{
    uint32_t ver_check_bit = 0x00;

    RM_OTA_W_INFO("- OTA Update : <%s> Compare Versions\n", rm_ota_w_type_to_text(update_type));

    /* Update_type */
    if (memcmp(new_ver.update_type, cur_ver.update_type, strlen((char *) new_ver.update_type)))
    {
        ver_check_bit |= BIT(RM_OTA_W_HEADER_DIFF_FW_TYPE);

        RM_OTA_W_ERR("   > Incompatible Image type : %s\n", new_ver.update_type);

        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(RM_OTA_W_HEADER_SAME_FW_TYPE);
    }

    /* module name */
    if (memcmp(new_ver.module, cur_ver.module, strlen((char *) new_ver.module)))
    {
        ver_check_bit = BIT(RM_OTA_W_HEADER_DIFF_MODULE);

        RM_OTA_W_INFO("   > Incompatible Image module : %s\n", new_ver.module);

        goto chk_finish;
    }
    else
    {
        ver_check_bit |= BIT(RM_OTA_W_HEADER_SAME_MODULE);
    }

    /* SDK version */
    if (memcmp(new_ver.sdk, cur_ver.sdk, strlen((char *) new_ver.sdk)))
    {
        RM_OTA_W_INFO("   > Different SDK ver : Cur-%s, New-%s \n", cur_ver.sdk, new_ver.sdk);

        ver_check_bit |= BIT(RM_OTA_W_HEADER_DIFF_SDK);
    }

    /* Customer version */
    if (memcmp(new_ver.customer, cur_ver.customer, strlen((char *) new_ver.customer)))
    {
        RM_OTA_W_INFO("   > Different Customer : Cur-%s, New-%s\n", cur_ver.customer, new_ver.customer);

        ver_check_bit |= BIT(RM_OTA_W_HEADER_DIFF_CUST);
    }

    if (ver_check_bit & BIT(RM_OTA_W_HEADER_ERROR))
    {
        RM_OTA_W_INFO("   > Version comparison failed\n");

        goto chk_finish;
    }

    if (((ver_check_bit & BIT(RM_OTA_W_HEADER_DIFF_SDK)) == 0) && ((ver_check_bit & BIT(RM_OTA_W_HEADER_DIFF_CUST)) == 0))
    {
        RM_OTA_W_INFO("   > Same Version : %s-%s-%s \n", new_ver.update_type, new_ver.sdk, new_ver.customer);
    }

chk_finish:
    return ver_check_bit;
}

static UINT rm_ota_w_check_version (rm_ota_w_instance_ctrl_t * p_ctrl,
                                   rm_ota_w_update_type_t update_type,
                                   UCHAR * data,
                                   UINT data_len)
{
    rm_ota_w_fw_version_info_t curr_ver;
    rm_ota_w_fw_version_info_t new_ver;
    UINT ret_val = 0;
    CHAR magic[4] = RM_OTA_W_FW_MAGIC_NUM;

#if defined (DISABLE_OTA_VER_CHK)
    RM_OTA_W_INFO("- OTA: NO Version check!!\n");
    return RM_OTA_W_SUCCESS;
#endif

    if ((data == NULL) || data_len == 0)
    {
        RM_OTA_W_ERR("- OTA: Unknown version\n");

        return RM_OTA_W_VERSION_UNKNOWN;
    }

    RM_OTA_W_INFO("[%s]\nupdate_type=%d\ndata_len=%d\ndata=%s\n", __func__, update_type, data_len, data);

    if (update_type == RM_OTA_W_TYPE_INIT)
    {
        return RM_OTA_W_SUCCESS;

    }
    else if (update_type >= RM_OTA_W_TYPE_UNKNOWN)
    {
        RM_OTA_W_ERR("- OTA: Unknown FW type\n");

        return RM_OTA_W_ERROR_TYPE;
    }
    else if (update_type == RM_OTA_W_TYPE_RTOS)
    {
        if (data_len < sizeof(rm_ota_w_fw_version_info_t))
        {
            RM_OTA_W_ERR("- OTA: Data size is too small to check version \n");

            return RM_OTA_W_VERSION_UNKNOWN;
        }

        /* Get current version */
        if (rm_ota_w_read_current_fw_version(rm_ota_w_get_curr_sflash_addr(p_ctrl, update_type), &curr_ver) != RM_OTA_W_SUCCESS)
        {
            RM_OTA_W_ERR("- OTA: Failed to read current version \n");
        }

        /* Get received version */
        if (rm_ota_w_read_new_fw_version(data, &new_ver) != RM_OTA_W_SUCCESS)
        {
            return RM_OTA_W_VERSION_UNKNOWN;
        }

        /* Check magic number */
        if (memcmp(&data[0], &magic[0], 4) != 0)
        {
            RM_OTA_W_ERR("- OTA: Wrong magic number (0x%02x 0x%02x 0x%02x 0x%02x)\n", data[0], data[1], data[2], data[3]);

            return RM_OTA_W_VERSION_UNKNOWN;
        }

        /* Compare current and received */
        ret_val = rm_ota_w_compare_fw_version(update_type, curr_ver, new_ver);

        if ((ret_val & BIT(RM_OTA_W_HEADER_DIFF_FW_TYPE)) ||
            (ret_val & BIT(RM_OTA_W_HEADER_INCOMPATI_MAGIC)) ||
            (ret_val & BIT(RM_OTA_W_HEADER_INIT)) ||
            (ret_val & BIT(RM_OTA_W_HEADER_ERROR)))
        {
            return RM_OTA_W_VERSION_UNKNOWN;
        }
    }

    return RM_OTA_W_SUCCESS;
}

static UINT rm_ota_w_current_fw_swap (rm_ota_w_instance_ctrl_t * p_ctrl)
{
    UCHAR * rd_buf = NULL;
    UINT len = 0;
    UINT status = RM_OTA_W_FAILED;

    RM_OTA_W_INFO("\n- OTA Update : Swap - Start\n");

    if (rm_ota_w_check_refuse_flag(p_ctrl) == RM_OTA_W_REFUSE_SET)
    {
        RM_OTA_W_ERR("- OTA: Try again after reboot\n");

        status = RM_OTA_W_FAILED;
        goto _swap_fail;
    }

    /* CRC - RTOS */
    if (rm_ota_w_sflash_rtos_crc(p_ctrl, rm_ota_w_get_new_sflash_addr(p_ctrl, RM_OTA_W_TYPE_RTOS)) != RM_OTA_W_SUCCESS)
    {
        RM_OTA_W_ERR("- OTA: <%s> CRC Error\n", RM_OTA_W_RTOS_NAME);

        status = RM_OTA_W_ERROR_CRC;
        goto _swap_fail;
    }

    if (rm_ota_w_get_download_progress(p_ctrl, RM_OTA_W_TYPE_RTOS) == RM_OTA_W_DOWNLOAD_DONE)
    {
        len = RM_OTA_W_VER_NAME_SIZE;
        rd_buf = RM_OTA_W_MALLOC(len);

        if (rd_buf != NULL)
        {
            if (rm_ota_w_read_flash(rm_ota_w_get_new_sflash_addr(p_ctrl, RM_OTA_W_TYPE_RTOS), rd_buf, len) != 0)
            {
                if (rm_ota_w_check_version(p_ctrl, RM_OTA_W_TYPE_RTOS, rd_buf, len) != RM_OTA_W_SUCCESS)
                {
                    RM_OTA_W_ERR("- OTA: <%s> Incompatible new version\n", RM_OTA_W_RTOS_NAME);

                    status = RM_OTA_W_VERSION_INCOMPATI;
                    RM_OTA_W_FREE(rd_buf);
                    goto _swap_fail;
                }
            }
            else
            {
                RM_OTA_W_ERR("- OTA: <%s> Failed to read new version\n", RM_OTA_W_RTOS_NAME);

                status = RM_OTA_W_FAILED;
                RM_OTA_W_FREE(rd_buf);
                goto _swap_fail;
            }

            RM_OTA_W_FREE(rd_buf);
        }
        else
        {
            RM_OTA_W_ERR("[%s:%d] Failed to allocate read buffer(%d bytes)\n", __func__, __LINE__, len);

            status = RM_OTA_W_MEM_ALLOC_FAILED;
            goto _swap_fail;
        }

        /* Toggle the boot index */
        if (rm_ota_w_toggle_boot_index(p_ctrl) == RM_OTA_W_SUCCESS)
        {
            return RM_OTA_W_SUCCESS;
        }
    }

_swap_fail:
    /* Swap fail */
    RM_OTA_W_ERR("\n>>> OTA FW update failed(0x%02x) <<<\n\n", status);

    return status;
}

static void rm_ota_w_swap_reboot_countdown(void * arg)
{
    UINT reboot_wait_cnt = RM_OTA_W_WAIT_TIME;

    while (reboot_wait_cnt-- > 0)
    {
        vTaskDelay(portCONVERT_MS_2_TICKS(100));
        if ((reboot_wait_cnt % 10) == 0)
        {
            RM_OTA_W_INFO("\r- OTA: Reboot after %d secs ...", reboot_wait_cnt / 10);
        }
    }
    /* Reset the device */
    rm_ota_w_system_reboot(SYS_REBOOT_POR);
}

static UINT rm_ota_w_start_swap (rm_ota_w_instance_ctrl_t * p_ctrl)
{
    UINT status = RM_OTA_W_SUCCESS;
    BaseType_t xReturned;

    status = rm_ota_w_check_state(p_ctrl);

    if (status == RM_OTA_W_SUCCESS)
    {
        status = rm_ota_w_current_fw_swap(p_ctrl);
    }

    if ((p_ctrl != NULL) && (p_ctrl->swap_notify != NULL))
    {
        /* Call notify function */
        p_ctrl->swap_notify(status);
    }

    if (status == RM_OTA_W_SUCCESS)
    {
        xReturned = xTaskCreate(rm_ota_w_swap_reboot_countdown,
                                RM_OTA_W_SWAP_TASK_NAME,
                                RM_OTA_W_SWAP_STACK_SIZE,
                                NULL,
                                RM_OTA_W_SWAP_TASK_PRI,
                                NULL);
        if (pdPASS != xReturned)
        {
            status = RM_OTA_W_FAILED;
        }
    }

    return status;
}

static UINT rm_ota_w_process_swap (rm_ota_w_instance_ctrl_t * p_ctrl, bool force)
{
    UINT status = RM_OTA_W_SUCCESS;

    if (p_ctrl == NULL)
    {
        RM_OTA_W_ERR("[%s] Failed to alloc memory\n", __func__);

        return RM_OTA_W_FAILED;
    }

    p_ctrl->update_state = 0x00;
    p_ctrl->ota_auto_swap = 0x00;
    p_ctrl->download_sflash_addr = 0x00;
    p_ctrl->download_notify = rm_ota_w_download_notify;
    p_ctrl->swap_notify = rm_ota_w_swap_notify;

    if (force)
    {
        p_ctrl->progress_rtos = RM_OTA_W_DOWNLOAD_DONE;
    }

    status = rm_ota_w_start_swap(p_ctrl);

    return status;
}

static uint8_t rm_ota_w_get_download_progress (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t update_type)
{
    uint8_t progress;

    switch(update_type)
    {
        case RM_OTA_W_TYPE_RTOS:
        progress = p_ctrl->progress_rtos;
        break;

        case RM_OTA_W_TYPE_BLE_FW:
        progress = p_ctrl->progress_ble;
        break;

        case RM_OTA_W_TYPE_MCU_FW:
        progress = p_ctrl->progress_mcu_fw;
        break;

        case RM_OTA_W_TYPE_CERT_KEY:
        progress = p_ctrl->progress_cert_key;
        break;

#if defined (__SUPPORT_MATTER_IOT__)
        case RM_OTA_W_TYPE_MCU_FW_STREAM:
        progress = p_ctrl->progress_mcu_fw;
        break;
#endif

        default:
        progress = UNKNOWN_PROGRESS;
        break;
    }

    return progress;
}

static void rm_ota_w_set_download_progress (rm_ota_w_instance_ctrl_t * p_ctrl, rm_ota_w_update_type_t type, UINT progress)
{
    switch(type)
    {
        case RM_OTA_W_TYPE_RTOS:
        p_ctrl->progress_rtos = progress;
        break;

        case RM_OTA_W_TYPE_BLE_FW:
        p_ctrl->progress_ble = progress;
        break;

        case RM_OTA_W_TYPE_MCU_FW:
        p_ctrl->progress_mcu_fw = progress;
        break;

        case RM_OTA_W_TYPE_CERT_KEY:
        p_ctrl->progress_cert_key = progress;
        break;

#if defined (__SUPPORT_MATTER_IOT__)
        case RM_OTA_W_TYPE_MCU_FW_STREAM:
        p_ctrl->progress_mcu_fw = progress;
        break;
#endif

        default:
        progress = UNKNOWN_PROGRESS;
        break;
    }
}

static uint8_t rm_ota_w_set_boot_index(uint8_t boot_idx)
{
    char RTOS_0_addr[4] = {0x00, };
    char RTOS_1_addr[4] = {0x00, };
    char prod_crc[2] = {0x00, };
    char * buf;
    int lit_end;
    int len;
    uint16_t crc_16;

    /* little endian */
    lit_end = SF_RTOS_0;
    RTOS_0_addr[3] = lit_end >> 24;
    RTOS_0_addr[2] = lit_end >> 16;
    RTOS_0_addr[1] = lit_end >> 8;
    RTOS_0_addr[0] = lit_end >> 0;

    lit_end = SF_RTOS_1;
    RTOS_1_addr[3] = lit_end >> 24;
    RTOS_1_addr[2] = lit_end >> 16;
    RTOS_1_addr[1] = lit_end >> 8;
    RTOS_1_addr[0] = lit_end >> 0;

    buf = RM_OTA_W_MALLOC(SF_PRODUCT_HDR_SIZE);
    if (buf == NULL)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(malloc failed)\n", __func__, __LINE__);
        return RM_OTA_W_FAILED;
    }

    len = rm_ota_w_read_flash(SF_PRODUCT_HDR, buf, SF_PRODUCT_HDR_SIZE);
    if (len != SF_PRODUCT_HDR_SIZE)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(Read primary header)\n", __func__, __LINE__);
        goto finish;
    }

    switch (boot_idx)
    {
        case RM_OTA_W_BOOT_IDX_0:
        /* Active FW image address */
        memcpy(buf + UPGRADE_ADDR_OFFSET, RTOS_0_addr, sizeof(RTOS_0_addr));
        /* Upgrade FW image address */
        memcpy(buf + ACTIVE_ADDR_OFFSET, RTOS_1_addr, sizeof(RTOS_1_addr));
        break;

        case RM_OTA_W_BOOT_IDX_1:
        /* Active FW image address */
        memcpy(buf + UPGRADE_ADDR_OFFSET, RTOS_1_addr, sizeof(RTOS_1_addr));
        /* Upgrade FW image address */
        memcpy(buf + ACTIVE_ADDR_OFFSET, RTOS_0_addr, sizeof(RTOS_0_addr));
        break;

        default:
        return RM_OTA_W_FAILED;
        break;
    }

    /* Update product header crc */
    crc_16 = rm_ota_w_calc_crc16((uint8_t *) buf, PRODUCT_HEADER_CRC_OFFSET);
    prod_crc[1] = crc_16 >> 8;
    prod_crc[0] = crc_16 >> 0;
    memcpy(buf + PRODUCT_HEADER_CRC_OFFSET, prod_crc, sizeof(prod_crc));

    /* Erase primary header */
    len = rm_ota_w_erase_flash(SF_PRODUCT_HDR, SF_PRODUCT_HDR_SIZE);
    if (len != SF_PRODUCT_HDR_SIZE)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(Erase primary header)\n", __func__, __LINE__);
        goto finish;
    }

    /* Write primary header */
    len = rm_ota_w_write_flash(SF_PRODUCT_HDR, buf, SF_PRODUCT_HDR_SIZE);
    if (len != SF_PRODUCT_HDR_SIZE)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(Write primary header)\n", __func__, __LINE__);
        goto finish;
    }

    /* Erase backup header */
    len = rm_ota_w_erase_flash(SF_PRODUCT_HDR_BACKUP, SF_PRODUCT_HDR_SIZE);
    if (len != SF_PRODUCT_HDR_SIZE)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(Erase backup header)\n", __func__, __LINE__);
        goto finish;
    }

    /* Write backup header */
    len = rm_ota_w_write_flash(SF_PRODUCT_HDR_BACKUP, buf, SF_PRODUCT_HDR_SIZE);
    if (len != SF_PRODUCT_HDR_SIZE)
    {
        RM_OTA_W_ERR("[%s:%d] Boot index setup failed(Write backup header)\n", __func__, __LINE__);
        goto finish;
    }

finish:
    if (buf != NULL)
    {
        RM_OTA_W_FREE(buf);
    }

    if (len != SF_PRODUCT_HDR_SIZE)
    {
        return RM_OTA_W_FAILED;
    }
    return RM_OTA_W_SUCCESS;
}

static uint8_t rm_ota_w_get_boot_index (void)
{
    uint8_t  upg_addr[4] = {0};
    uint32_t big_end;
    uint8_t boot_idx = RM_OTA_W_BOOT_IDX_0;

    rm_ota_w_read_flash(SF_PRODUCT_HDR + UPGRADE_ADDR_OFFSET, upg_addr, sizeof(upg_addr));

    big_end = ((uint32_t) upg_addr[3] << 24) |
              ((uint32_t) upg_addr[2] << 16) |
              ((uint32_t) upg_addr[1] << 8)  |
              ((uint32_t) upg_addr[0]);

    if (big_end == (uint32_t) SF_RTOS_1)
    {
        boot_idx = RM_OTA_W_BOOT_IDX_1;
    }

    return boot_idx;
}

static uint8_t rm_ota_w_get_booter_rtm_index(void)
{
    volatile RETENTION_MEM_type *bootflag;
    uint32_t rtm_addr;
    uint8_t rtm_idx = RM_OTA_W_BOOT_IDX_0;

    bootflag = (volatile RETENTION_MEM_type *)dg_configBOOTER_RTM_ADDR;
    rtm_addr = bootflag->tin_fw_running_address;

    if (rtm_addr == (uint32_t) SF_RTOS_1)
    {
        rtm_idx = RM_OTA_W_BOOT_IDX_1;
    }

    return rtm_idx;
}

static uint8_t rm_ota_w_sync_and_get_boot_index(void)
{
    fsp_err_t err = RM_OTA_W_SUCCESS;
    uint8_t rtm_idx = RM_OTA_W_BOOT_IDX_0;
    uint8_t boot_idx = rm_ota_w_get_boot_index();

    if (g_booter_rtm_sync_done == BOOTER_RTM_SYNC_DONE)
    {
        return boot_idx;
    }

    rtm_idx = rm_ota_w_get_booter_rtm_index();

    /* Bootloader workaround - synchronize boot index with booter RTM */
    if (rtm_idx != boot_idx)
    {
        err = rm_ota_w_set_boot_index(rtm_idx);

        if (FSP_SUCCESS == err)
        {
            boot_idx = rtm_idx;
            g_booter_rtm_sync_done = BOOTER_RTM_SYNC_DONE;
        }
    }
    else
    {
        g_booter_rtm_sync_done = BOOTER_RTM_SYNC_DONE;
    }

    return boot_idx;
}

static uint8_t rm_ota_w_toggle_boot_index (rm_ota_w_instance_ctrl_t * p_ctrl)
{
    uint8_t boot_idx = 0;

    RM_OTA_W_BootIdxGet(p_ctrl, &boot_idx);
    RM_OTA_W_BootIdxSet(p_ctrl, !boot_idx);
    RM_OTA_W_BootIdxGet(p_ctrl, &boot_idx);
    RM_OTA_W_INFO(">>> %s is updated and system new boot_idx=%u\n", RM_OTA_W_RTOS_NAME, boot_idx);

    return RM_OTA_W_SUCCESS;
}

static fsp_err_t rm_ota_w_check_state (rm_ota_w_instance_ctrl_t * p_ctrl)
{
    uint32_t curr_state = p_ctrl->update_state;

    if (curr_state == RM_OTA_W_STATE_PROGRESS)
    {
        RM_OTA_W_INFO("- OTA : is downloading... Try again after finishing.\n");

        return RM_OTA_W_FAILED;
    }
    else if ((curr_state == RM_OTA_W_STATE_FINISH) || (curr_state == RM_OTA_W_STATE_STOP))
    {
        RM_OTA_W_INFO("- OTA : In progressing(%lu)... Please wait.\n", curr_state);

        return RM_OTA_W_FAILED;
    }

    return FSP_SUCCESS;
}

static uint32_t rm_ota_w_get_mcu_fw_info (rm_ota_w_instance_ctrl_t * p_ctrl, char * name, uint32_t * size, uint32_t * crc)
{
    uint32_t addr;
    uint32_t name_len;
    rm_ota_w_mcu_fw_info_t mcu_fw_info = {0x00, };

    addr = rm_ota_w_get_curr_sflash_addr(p_ctrl, RM_OTA_W_TYPE_MCU_FW);
    memset(mcu_fw_info.name, 0x00, RM_OTA_W_MCU_FW_NAME_LEN);

    if (rm_ota_w_read_flash(addr, &mcu_fw_info, sizeof(rm_ota_w_mcu_fw_info_t)) != 0)
    {
        /* Check Size */
        if ((mcu_fw_info.size <= 0) || (mcu_fw_info.size >= 0xffffffff))
        {
            mcu_fw_info.size = 0;
        }

        /* Check CRC */
        if ((mcu_fw_info.crc <= 0) || (mcu_fw_info.crc >= 0xffffffff))
        {
            mcu_fw_info.crc = 0;
        }

        /* Check Name */
        name_len = strlen(mcu_fw_info.name);

        if ((name_len <= 0) || (mcu_fw_info.size <= 0))
        {
            bsp_safe_strcpy(mcu_fw_info.name, "NULL", sizeof(mcu_fw_info.name));
        }

        /* Copy Size */
        if (size != NULL)
        {
            memcpy(size, &mcu_fw_info.size, sizeof(mcu_fw_info.size));
        }

        /* Copy CRC */
        if (crc != NULL)
        {
            memcpy(crc, &mcu_fw_info.crc, sizeof(mcu_fw_info.crc));
        }

        /* Copy Name */
        if (name != NULL)
        {
            if (name_len > RM_OTA_W_MCU_FW_NAME_LEN)
            {
                name_len = RM_OTA_W_MCU_FW_NAME_LEN;
            }

            memcpy(name, mcu_fw_info.name, name_len);
        }

        return RM_OTA_W_SUCCESS;
    }

    return RM_OTA_W_FAILED;
}

static void rm_ota_w_system_reboot (unsigned int flag)
{
    RM_OTA_W_INFO("\n > Reboot....\n");

    if (flag == 0)
    {
        SWRESET;
    }
    else if (flag == 1)
    {
        PORRESET;
    }
    else if (flag == 2)
    {
        PORRESET;
    }
    else
    {
        RM_OTA_W_INFO(" Unknown flag\n");
    }

    return;
}

/* End of file RM_OTA_W. */
