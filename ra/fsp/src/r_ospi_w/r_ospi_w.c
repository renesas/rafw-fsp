/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "r_ospi_w.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define OSPI_W_PRV_OPEN                      (0x4F535049U) // ASCII characters "OSPI"

/* Read pipe delay based on voltage level of power rail. */
#define OSPI_W_READ_PIPE_DELAY               (OSPI_W_READ_PIPE_DELAY_0)

/* Page size. */
#define OSPI_W_FLASH_PAGE_SIZE               (0XFFU)

/* Max command length for directTransfer API. */
#define OSPI_W_DIRECT_TRANSFER_MAX_CMD_LEN   (2U)

/* Max address length for directTransfer API. */
#define OSPI_W_DIRECT_TRANSFER_MAX_ADDR_LEN  (4U)

/* Max dummy cycle length for directTransfer API. */
#define OSPI_W_DIRECT_TRANSFER_MAX_DUMMY_LEN (4U)

/* Max data length of directTransfer API. */
#define OSPI_W_DIRECT_TRANSFER_MAX_DATA_LEN  (8U)

/* OSPI_W base register access macro. */
#define OSPI_REG(channel)                    ((OQSPIF_Type *) ((uint32_t) OQSPIF - ((uint32_t) OQSPIF) * (channel)))

/* Macro definitions to make the register field prefix configurable. */
#define OSPI_W_CAT(a, b)                     a ## b
#define OSPI_W_EXPAND_CAT(a, b)              OSPI_W_CAT(a, b)
#define OSPI_W_REG_FIELD(field)              OSPI_W_EXPAND_CAT(BSP_FEATURE_OSPI_REG_FIELD_PREFIX, field)
#define OSPI_W_REG_FIELD_MSK(reg, field)     OSPI_W_EXPAND_CAT(OQSPIF_ ## reg ## _,                                 \
                                                               OSPI_W_EXPAND_CAT(BSP_FEATURE_OSPI_REG_FIELD_PREFIX, \
                                                                                 field ## _Msk))
#define OSPI_W_REG_FIELD_POS(reg, field)     OSPI_W_EXPAND_CAT(OQSPIF_ ## reg ## _,                                 \
                                                               OSPI_W_EXPAND_CAT(BSP_FEATURE_OSPI_REG_FIELD_PREFIX, \
                                                                                 field ## _Pos))
#define OSPI_W_REG_VAR_FIELD_SET(reg, field, var, val)                                                              \
                                             var = (((uint32_t) var & ~OSPI_W_REG_FIELD_MSK(reg, field)) |          \
                                                    ((((uint32_t) (val)) << OSPI_W_REG_FIELD_POS(reg, field)) &     \
                                                     OSPI_W_REG_FIELD_MSK(reg, field)))
#define OSPI_W_REG_FIELD_SET_BITS32(reg, field, v)                                                                  \
                                             ((uint32_t) (((uint32_t) (v) << OSPI_W_REG_FIELD_POS(reg, field)) &    \
                                                          OSPI_W_REG_FIELD_MSK(reg, field)))

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/*
 * Union used in order to allow different size access when reading/writing to
 * OQSPIF_READDATA_REG, OQSPIF_WRITEDATA_REG, OQSPIF_DUMMYDATA_REG.
 */
typedef union u_ospi_w_data
{
    __IO uint32_t data32;
    __IO uint16_t data16;
    __IO uint8_t  data8;
} ospi_w_data_t;

/***********************************************************************************************************************
 * Private global variables and functions.
 **********************************************************************************************************************/

__STATIC_FORCEINLINE void r_ospi_w_set_io(uint8_t channel, ospi_w_bus_mode_t bus_mode);
__STATIC_FORCEINLINE void r_ospi_w_write32(uint8_t channel, uint32_t data);
__STATIC_FORCEINLINE void r_ospi_w_write8(uint8_t channel, uint8_t data);
__STATIC_FORCEINLINE void r_ospi_w_dummy8(uint8_t channel);
__STATIC_FORCEINLINE void r_ospi_w_read_status_instr_init(uint8_t                                channel,
                                                          const ospi_w_read_status_instr_cfg_t * p_read_status_instr_cfg);
__STATIC_FORCEINLINE void r_ospi_w_write_enable_instr_init(uint8_t                                 channel,
                                                           const ospi_w_write_enable_instr_cfg_t * p_wrt_enble_instr_cfg);
__STATIC_FORCEINLINE void r_ospi_w_erase_instr_init(uint8_t                          channel,
                                                    const ospi_w_erase_instr_cfg_t * p_erase_instr_cfg);
__STATIC_FORCEINLINE void r_ospi_w_suspend_resume_instr_init(uint8_t                                   channel,
                                                             const ospi_w_suspend_resume_instr_cfg_t * p_sus_res_instr_cfg);
__STATIC_FORCEINLINE void r_ospi_w_write_instr_init(uint8_t                          channel,
                                                    const ospi_w_write_instr_cfg_t * p_wrt_instr_cfg);
__STATIC_FORCEINLINE void r_ospi_w_break_instr_init(uint8_t                          channel,
                                                    const ospi_w_break_instr_cfg_t * p_break_instr_cfg);
__STATIC_FORCEINLINE void         r_ospi_w_memblen_init(uint8_t channel, const ospi_w_memblen_cfg_t * p_memblen_cfg);
__STATIC_FORCEINLINE ospi_w_ers_t r_ospi_w_get_erase_status(uint8_t channel);
__STATIC_FORCEINLINE uint8_t      r_ospi_w_read8(uint8_t channel);
__STATIC_FORCEINLINE void         r_ospi_w_fast_write_to_fifo32(uint32_t start, uint32_t end, uint32_t dest);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_enter_manual_access_mode(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_xip(ospi_w_instance_ctrl_t * p_instance_ctrl, uint8_t code, bool enter_mode);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_manual_access_bus_mode_set(uint8_t channel, ospi_w_bus_mode_t bus_mode);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_enter_auto_access_mode(ospi_w_instance_ctrl_t * p_instance_ctrl);

BSP_PLACE_CODE_IN_RAM static uint32_t r_ospi_w_flash_write_page(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                uint32_t                 addr,
                                                                const uint8_t          * p_buf,
                                                                uint32_t                 size);
BSP_PLACE_CODE_IN_RAM static uint32_t r_ospi_w_get_chip_address(uint8_t * const p_addr);
BSP_PLACE_CODE_IN_RAM static void     r_ospi_w_erase_block(uint8_t channel, uint32_t addr);
BSP_PLACE_CODE_IN_RAM static void     r_ospi_w_erase_flash_sector(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                  uint32_t                 addr);
BSP_PLACE_CODE_IN_RAM static uint16_t r_ospi_w_get_erase_opcode(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                uint32_t                 byte_count);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_erase_chip(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                      uint8_t                  erase_command);

BSP_PLACE_CODE_IN_RAM static void r_ospi_w_instructions_init(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_controller_init(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_get_bus_mode(spi_flash_protocol_t spi_protocol,
                                                             ospi_w_bus_mode_t  * p_bus_mode);

BSP_PLACE_CODE_IN_RAM static bool r_ospi_w_flash_is_writable(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_flash_cmd(uint8_t channel, const uint8_t opcode);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_flash_write_enable(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static bool r_ospi_w_flash_is_busy(ospi_w_instance_ctrl_t * p_instance_ctrl);

BSP_PLACE_CODE_IN_RAM static void r_ospi_w_set_extra_byte(uint8_t                         channel,
                                                          uint8_t                         extra_byte,
                                                          ospi_w_read_instr_cfg_t const * p_ospi_cfg);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_read_instr_init(uint8_t                         channel,
                                                           const ospi_w_read_instr_cfg_t * p_read_instr_cfg);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_transact(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                    const uint8_t          * p_wbuf,
                                                    uint32_t                 wlen,
                                                    uint8_t                * p_rbuf,
                                                    uint32_t                 rlen);
BSP_PLACE_CODE_IN_RAM static uint8_t r_ospi_w_read_status_register(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_qpi(ospi_w_instance_ctrl_t * p_instance_ctrl, bool enter_mode);

#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_param_checking_dcom(ospi_w_instance_ctrl_t * p_instance_ctrl);
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_program_param_check(ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                    uint8_t const * const    p_src,
                                                                    uint8_t * const          p_dest,
                                                                    uint32_t                 byte_count);

#endif

/*******************************************************************************************************************//**
 * @addtogroup OSPI_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

const spi_flash_api_t g_ospi_w_on_spi_flash BSP_PLACE_IN_SECTION(OSPI_W_SECTION_CONST_DATA) =
{
    .open           = R_OSPI_W_Open,
    .directWrite    = R_OSPI_W_DirectWrite,
    .directRead     = R_OSPI_W_DirectRead,
    .directTransfer = R_OSPI_W_DirectTransfer,
    .spiProtocolSet = R_OSPI_W_SpiProtocolSet,
    .write          = R_OSPI_W_Write,
    .erase          = R_OSPI_W_Erase,
    .statusGet      = R_OSPI_W_StatusGet,
    .xipEnter       = R_OSPI_W_XipEnter,
    .xipExit        = R_OSPI_W_XipExit,
    .bankSet        = R_OSPI_W_BankSet,
    .autoCalibrate  = R_OSPI_W_AutoCalibrate,
    .close          = R_OSPI_W_Close,
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Open the OSPI_W driver module. After the driver is open, the OSPI_W can be accessed like internal flash memory.
 *
 * OSPI_W is configured to operate in AUTO MODE after the driver is open.
 *
 * OSPI_W memory is expected to be in single SPI mode before calling open.
 *
 * Implements @ref spi_flash_api_t::open.
 *
 * @param[in] p_ctrl                        Pointer to the instance control structure.
 * @param[in] p_cfg                         Configuration structure which contains all the user provided configurations.
 *
 * @retval FSP_SUCCESS                      Configuration was successful.
 * @retval FSP_ERR_ASSERTION                The pointer parameters are NULL.
 * @retval FSP_ERR_ALREADY_OPEN             Driver has already been opened with the same p_instance_ctrl.
 * @retval FSP_ERR_IP_CHANNEL_NOT_PRESENT   The channel number is invalid.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Open (spi_flash_ctrl_t * p_ctrl, spi_flash_cfg_t const * const p_cfg)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_cfg);
#endif
    ospi_w_extended_cfg_t * p_cfg_extend = (ospi_w_extended_cfg_t *) p_cfg->p_extend;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_cfg_extend);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ERROR_RETURN(BSP_FEATURE_OSPI_MAX_CHANNEL > p_cfg_extend->channel, FSP_ERR_IP_CHANNEL_NOT_PRESENT);
#endif
    uint8_t channel = p_cfg_extend->channel;

    /* If the bus mode in manual mode is not aligned with the opcode mode used for burst-read in auto mode,
       a mode-change command would need to be issued to the flash every time the driver switches between manual
       and auto mode, resulting in degraded performance. Therefore, the bus mode in manual mode should, by default,
       be set to the same mode as the opcode used for burst-read in auto mode. */
    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE] = 
        p_cfg_extend->p_ospi_flash_cfg->p_read_instr_cfg->opcode_bus_mode;
    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = 
        p_cfg_extend->p_ospi_flash_cfg->p_read_instr_cfg->addr_bus_mode;
    p_instance_ctrl->flash_opcode_bus_mode = OSPI_REG(channel)->OQSPIF_BURSTCMDA_REG_b.OSPI_W_REG_FIELD(INST_TX_MD);
    p_instance_ctrl->xip_mode_is_enabled   = OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG_b.OSPI_W_REG_FIELD(INST_MD);
    p_instance_ctrl->p_cfg                 = p_cfg;

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);
    r_ospi_w_controller_init(p_instance_ctrl);
    r_ospi_w_instructions_init(p_instance_ctrl);
    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;

    p_instance_ctrl->total_size_bytes      = 0;
    p_instance_ctrl->manual_command_length = 1;
    p_instance_ctrl->open                  = OSPI_W_PRV_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Writes raw data directly to the OSPI_W. Unsupported by OSPI_W.
 *
 * Implements @ref spi_flash_api_t::directWrite.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 * @param[in] p_src                    Pointer to the data to write.
 * @param[in] bytes                    Number of bytes to write.
 * @param[in] read_after_write         Whether or not to close the SPI bus cycle.
 *
 * @retval FSP_ERR_UNSUPPORTED         API not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectWrite (spi_flash_ctrl_t    * p_ctrl,
                                                      uint8_t const * const p_src,
                                                      uint32_t const        bytes,
                                                      bool const            read_after_write)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_src);
    FSP_PARAMETER_NOT_USED(bytes);
    FSP_PARAMETER_NOT_USED(read_after_write);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
}

/*******************************************************************************************************************//**
 * Reads raw data directly from the OSPI_W. This API can only be called after R_OSPI_W_DirectWrite with read_after_write
 * set to true. Unsupported by OSPI_W.
 *
 * Implements @ref spi_flash_api_t::directRead.
 *
 * @param[in]  p_ctrl                   Pointer to the instance control structure.
 * @param[out] p_dest                   Pointer to the destination buffer.
 * @param[in]  bytes                    Number of bytes to read.
 *
 * @retval FSP_ERR_UNSUPPORTED          API not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectRead (spi_flash_ctrl_t * p_ctrl,
                                                     uint8_t * const    p_dest,
                                                    uint32_t const      bytes)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_dest);
    FSP_PARAMETER_NOT_USED(bytes);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
}

/*******************************************************************************************************************//**
 * Read/Write raw data directly with the OctaFlash/OctaRAM device.
 *
 * @note Use R_OSPI_W_SpiProtocolSet before a direct transfer to set the bus mode in the appropriate manual mode.
 *
 * Implements @ref spi_flash_api_t::directTransfer.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 * @param[in] p_transfer               Pointer to @ref spi_flash_direct_transfer_t.
 * @param[in] direction                Direct read or write direction.
 *
 * @retval FSP_SUCCESS                 Directly read raw data successfully.
 * @retval FSP_ERR_ASSERTION           A required pointer is NULL.
 * @retval FSP_ERR_NOT_OPEN            Driver is not opened.
 * @retval FSP_ERR_DEVICE_BUSY         The device is busy.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectTransfer (spi_flash_ctrl_t                  * p_ctrl,
                                                         spi_flash_direct_transfer_t * const p_transfer,
                                                         spi_flash_direct_transfer_dir_t     direction)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    fsp_err_t err = r_ospi_w_param_checking_dcom(p_instance_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    FSP_ASSERT(NULL != p_transfer);
    FSP_ASSERT((OSPI_W_DIRECT_TRANSFER_MAX_CMD_LEN >= p_transfer->command_length) && (0 != p_transfer->command_length));
    FSP_ASSERT(OSPI_W_DIRECT_TRANSFER_MAX_ADDR_LEN >= p_transfer->address_length);
    FSP_ASSERT(OSPI_W_DIRECT_TRANSFER_MAX_DUMMY_LEN >= p_transfer->dummy_cycles);
    FSP_ASSERT(OSPI_W_DIRECT_TRANSFER_MAX_DATA_LEN >= p_transfer->data_length);
#endif
    uint8_t   command_length = p_transfer->command_length;
    uint8_t   address_length = p_transfer->address_length;
    uint8_t   dummy_cycles   = p_transfer->dummy_cycles;
    uint8_t   data_length    = p_transfer->data_length;
    uint8_t * p_rbuf         = (uint8_t *) &p_transfer->data_u64;
    uint8_t   wbuf[OSPI_W_DIRECT_TRANSFER_MAX_CMD_LEN +
                   OSPI_W_DIRECT_TRANSFER_MAX_ADDR_LEN +
                   OSPI_W_DIRECT_TRANSFER_MAX_DUMMY_LEN +
                   OSPI_W_DIRECT_TRANSFER_MAX_DATA_LEN] = { 0 };
    uint8_t   wbuf_offset;
    uint8_t * p_data;

    /* Set command. */
    p_data = (uint8_t * ) &p_transfer->command;
    for (uint32_t i = 0; i < command_length; i++)
    {
        wbuf[i] = p_data[i];
    }
    wbuf_offset                            = command_length;
    p_instance_ctrl->manual_command_length = command_length;

    /* Set address. */
    p_data = (uint8_t * ) &p_transfer->address;
    for (uint32_t i = 0; i < address_length; i++)
    {
        wbuf[wbuf_offset + i] = p_data[address_length - i - 1];
    }

    /* Set data and length. */
    uint32_t wlen;
    uint32_t rlen;
    if (SPI_FLASH_DIRECT_TRANSFER_DIR_READ == direction)
    {
        wlen = (uint32_t) (command_length + address_length + dummy_cycles);
        rlen = data_length;
    }
    else
    {
        wlen = (uint32_t) (command_length + address_length + dummy_cycles + data_length);
        rlen = 0;
        wbuf_offset += (uint8_t) (address_length + dummy_cycles);
        p_data = (uint8_t * ) &p_transfer->data_u64;
        for (uint32_t i = 0; i < data_length; i++)
        {
            wbuf[wbuf_offset + i] = p_data[i];
        }
    }

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);
    r_ospi_w_transact(p_instance_ctrl, wbuf, wlen, p_rbuf, rlen);
    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Enters XiP (execute in place) mode.
 *
 * @note XiP mode refers to random access read mode, also known as Fast Read Quad I/O (EBh),
 *       4READ: 4 x I/O READ (EBh), etc.
 *
 * Implements @ref spi_flash_api_t::xipEnter.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 *
 * @retval    FSP_SUCCESS              The flash enters XiP mode successfully.
 * @retval    FSP_ERR_ASSERTION        A required pointer is NULL.
 * @retval    FSP_ERR_NOT_OPEN         Driver is not opened.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_XipEnter (spi_flash_ctrl_t * p_ctrl)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (OSPI_W_INSTR_MD_TX_ONLY_IN_FIRST_ACCESS == p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg->instr_md)
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_enter_command, true);

        p_instance_ctrl->xip_mode_is_enabled = true;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Exits XiP (execute in place) mode.
 *
 * Implements @ref spi_flash_api_t::xipExit.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 *
 * @retval FSP_SUCCESS                 The flash exits XiP mode successfully.
 * @retval FSP_ERR_ASSERTION           A required pointer is NULL.
 * @retval FSP_ERR_NOT_OPEN            Driver is not opened.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_XipExit (spi_flash_ctrl_t * p_ctrl)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (OSPI_W_INSTR_MD_TX_ONLY_IN_FIRST_ACCESS == p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg->instr_md)
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_exit_command, false);

        p_instance_ctrl->xip_mode_is_enabled = false;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Program a page of data to the flash.
 *
 * Implements @ref spi_flash_api_t::write.
 *
 * @param[in]  p_ctrl                  Pointer to the instance control structure.
 * @param[in]  p_src                   Pointer to the source data.
 * @param[out] p_dest                  Pointer to the destination.
 * @param[in]  byte_count              Number of bytes to write.
 *
 * @retval FSP_SUCCESS                 The flash was programmed successfully.
 * @retval FSP_ERR_ASSERTION           p_instance_ctrl, p_dest or p_src is NULL, or byte_count crosses a page boundary.
 * @retval FSP_ERR_NOT_OPEN            Driver is not opened.
 * @retval FSP_ERR_DEVICE_BUSY         The device is busy.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Write (spi_flash_ctrl_t    * p_ctrl,
                                                uint8_t const * const p_src,
                                                uint8_t * const       p_dest,
                                                uint32_t              byte_count)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    fsp_err_t err = r_ospi_w_program_param_check(p_instance_ctrl, p_src, p_dest, byte_count);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
#endif
    uint32_t written_bytes = 0;

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);
    while (r_ospi_w_flash_is_busy(p_instance_ctrl));
    while (written_bytes < byte_count)
    {
        written_bytes += r_ospi_w_flash_write_page(p_instance_ctrl,
                                                   r_ospi_w_get_chip_address(p_dest + written_bytes),
                                                   p_src + written_bytes,
                                                   byte_count - written_bytes);
        while (r_ospi_w_flash_is_busy(p_instance_ctrl));
    }
    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Erase a block or sector of flash. The byte_count must exactly match one of the erase sizes defined in spi_flash_cfg_t.
 * For chip erase, byte_count must be SPI_FLASH_ERASE_SIZE_CHIP_ERASE.
 *
 * Implements @ref spi_flash_api_t::erase.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 * @param[in] p_device_address         Pointer to the device address.
 * @param[in] byte_count               Number of bytes to be erased.
 *
 * @retval FSP_SUCCESS                 The command to erase the flash was executed successfully.
 * @retval FSP_ERR_ASSERTION           p_instance_ctrl or p_device_address is NULL, or byte_count doesn't match an erase
 *                                     size defined in spi_flash_cfg_t, or device is in XiP mode.
 * @retval FSP_ERR_NOT_OPEN            Driver is not opened.
 * @retval FSP_ERR_DEVICE_BUSY         The device is busy.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Erase (spi_flash_ctrl_t * p_ctrl,
                                                uint8_t * const    p_device_address,
                                                uint32_t           byte_count)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    fsp_err_t err = r_ospi_w_param_checking_dcom(p_instance_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    FSP_ASSERT(NULL != p_device_address);
#endif
    uint8_t erase_command = (uint8_t) r_ospi_w_get_erase_opcode(p_instance_ctrl, byte_count);

    if (SPI_FLASH_ERASE_SIZE_CHIP_ERASE == byte_count)
    {
        /* MANUAL MODE chip erase. */
        r_ospi_w_erase_chip(p_instance_ctrl, erase_command);
    }
    else
    {
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
        FSP_ASSERT(0U != erase_command);
#endif
        ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
        uint8_t                 channel       = p_inst_extend->channel;

        /* AUTO MODE block erase. */
        OSPI_REG(channel)->OQSPIF_ERASECMDA_REG_b.OSPI_W_REG_FIELD(ERS_INST) = erase_command;
        r_ospi_w_erase_flash_sector(p_instance_ctrl, r_ospi_w_get_chip_address(p_device_address));
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Gets the write or erase status of the flash.
 *
 * Implements @ref spi_flash_api_t::statusGet.
 *
 * @param[in]  p_ctrl                   Pointer to the instance control structure.
 * @param[out] p_status                 Pointer to the status @ref spi_flash_status_t.
 *
 * @retval FSP_SUCCESS                  The write status is in p_status.
 * @retval FSP_ERR_ASSERTION            p_instance_ctrl or p_status is NULL.
 * @retval FSP_ERR_NOT_OPEN             Driver is not opened.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_StatusGet (spi_flash_ctrl_t * p_ctrl, spi_flash_status_t * const p_status)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_status);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    /* Read device status. */
    if (OSPI_W_ERS_NO != r_ospi_w_get_erase_status(channel))
    {
        p_status->write_in_progress = true;
    }
    else
    {
        p_status->write_in_progress = !(r_ospi_w_flash_is_writable(p_instance_ctrl));
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Selects the bank to access.
 *
 * Implements @ref spi_flash_api_t::bankSet.
 *
 * @param[in] p_ctrl                   Pointer to the instance control structure.
 * @param[in] bank                     The bank which need to access.
 *
 * @retval FSP_ERR_UNSUPPORTED         API not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_BankSet (spi_flash_ctrl_t * p_ctrl, uint32_t bank)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(bank);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
}

/*******************************************************************************************************************//**
 * Sets the SPI protocol in R_OSPI_W_DirectTransfer API.
 *
 * @note Updates OSPI_W access bus mode for R_OSPI_W_DirectTransfer API.
 *
 * Implements @ref spi_flash_api_t::spiProtocolSet.
 *
 * @param[in] p_ctrl              Pointer to the instance control structure.
 * @param[in] spi_protocol        The type of SPI Flash protocol.
 *
 * @retval FSP_SUCCESS            SPI protocol updated successfully.
 * @retval FSP_ERR_ASSERTION      A required pointer is NULL.
 * @retval FSP_ERR_NOT_OPEN       Driver is not opened.
 * @retval FSP_ERR_UNSUPPORTED    Protocol is not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_SpiProtocolSet (spi_flash_ctrl_t * p_ctrl, spi_flash_protocol_t spi_protocol)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    fsp_err_t err = r_ospi_w_get_bus_mode(spi_protocol, p_instance_ctrl->manual_access_bus_mode);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Auto-calibrate the OctaRAM device using the preamble pattern.
 * Implements @ref spi_flash_api_t::autoCalibrate.
 *
 * @param[in] p_ctrl              Pointer to the instance control structure.
 *
 * @retval FSP_ERR_UNSUPPORTED    API not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_AutoCalibrate (spi_flash_ctrl_t * p_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

    FSP_RETURN(FSP_ERR_UNSUPPORTED);
}

/*******************************************************************************************************************//**
 * Close the OSPI_W driver module.
 *
 * Implements @ref spi_flash_api_t::close.
 *
 * @param[in] p_ctrl            Pointer to the instance control structure.
 *
 * @retval FSP_SUCCESS          Close module successfully.
 * @retval FSP_ERR_ASSERTION    p_instance_ctrl is NULL.
 * @retval FSP_ERR_NOT_OPEN     Driver is not opened.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Close (spi_flash_ctrl_t * p_ctrl)
{
    ospi_w_instance_ctrl_t * p_instance_ctrl = (ospi_w_instance_ctrl_t *) p_ctrl;
#if OSPI_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_instance_ctrl->open = 0U;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup OSPI_W)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Set the direction and the level of OQSPIF IOs based on the Bus Mode.
 *
 * @param[in] channel     OSPI controller id.
 * @param[in] bus_mode    The OQSPIF Bus Mode.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_set_io (uint8_t channel, ospi_w_bus_mode_t bus_mode)
{
    uint32_t ctrlmode_reg = OSPI_REG(channel)->OQSPIF_CTRLMODE_REG;

    switch (bus_mode)
    {
        case OSPI_W_BUS_MODE_SINGLE:
        case OSPI_W_BUS_MODE_DUAL:
        {
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO2_OEN, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO2_DAT, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO3_OEN, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO3_DAT, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO_UH_OEN, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO_UH_DAT, ctrlmode_reg, 15);
            break;
        }

        case OSPI_W_BUS_MODE_QUAD:
        {
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO2_OEN, ctrlmode_reg, 0);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO3_OEN, ctrlmode_reg, 0);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO_UH_OEN, ctrlmode_reg, 1);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO_UH_DAT, ctrlmode_reg, 15);
            break;
        }

        case OSPI_W_BUS_MODE_OCTAL:
        default:
        {
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO2_OEN, ctrlmode_reg, 0);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO3_OEN, ctrlmode_reg, 0);
            OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, IO_UH_OEN, ctrlmode_reg, 0);
            break;
        }
    }

    OSPI_REG(channel)->OQSPIF_CTRLMODE_REG = ctrlmode_reg;
}

/*******************************************************************************************************************//**
 * Generate 32 bits data transfer from the OQSPIF to the external device (manual mode).
 *
 * @param[in] channel    OSPI controller id.
 * @param[in] data       32 bits value to be written on the device.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_write32 (uint8_t channel, uint32_t data)
{
    volatile ospi_w_data_t * p_tmp = (volatile ospi_w_data_t *) &(OSPI_REG(channel)->OQSPIF_WRITEDATA_REG);

    p_tmp->data32 = FSP_SWAP32(data);
}

/*******************************************************************************************************************//**
 * Generate 8 bits data transfer from the OQSPIF to the external device (manual mode).
 *
 * @param[in] channel    OSPI controller id.
 * @param[in] data       8 bits value to be written on the device.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_write8 (uint8_t channel, uint8_t data)
{
    volatile ospi_w_data_t * p_tmp = (volatile ospi_w_data_t *) &(OSPI_REG(channel)->OQSPIF_WRITEDATA_REG);

    p_tmp->data8 = data;
}

/*******************************************************************************************************************//**
 * Generate 8 bits dummy transfer from the OQSPIF to the external device (manual mode).
 *
 * @param[in] channel    OSPI controller id.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_dummy8 (uint8_t channel)
{
    volatile ospi_w_data_t * p_tmp = (volatile ospi_w_data_t *) &(OSPI_REG(channel)->OQSPIF_DUMMYDATA_REG);

    p_tmp->data8 = 0;
}

/*******************************************************************************************************************//**
 * Initialize the read status register instruction of the OQSPIF.
 *
 * @param[in] channel                    OSPI controller id.
 * @param[in] p_read_status_instr_cfg    Pointer to configuration structure of the read status register
 *                                       instruction.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_read_status_instr_init (uint8_t                                channel,
                                                           const ospi_w_read_status_instr_cfg_t * p_read_status_instr_cfg)
{
    if (p_read_status_instr_cfg)
    {
#if OSPI_W_REG_FIELD_MSK(OQSPIF_STATUSCMDA_REG, BUSY_VAL)
        uint32_t statuscmd_reg = OSPI_REG(channel)->OQSPIF_STATUSCMDA_REG;

        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, BUSY_VAL, statuscmd_reg,
                                 p_read_status_instr_cfg->busy_level);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, STSDLY_SEL, statuscmd_reg,
                                 p_read_status_instr_cfg->stsdly_sel);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, RESSTS_DLY, statuscmd_reg,
                                 p_read_status_instr_cfg->delay_cycles);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, BUSY_POS, statuscmd_reg,
                                 p_read_status_instr_cfg->busy_pos);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, RSTAT_RX_MD, statuscmd_reg,
                                 p_read_status_instr_cfg->receive_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, RSTAT_TX_MD, statuscmd_reg,
                                 p_read_status_instr_cfg->opcode_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDA_REG, RSTAT_INST, statuscmd_reg,
                                 p_read_status_instr_cfg->opcode);

        OSPI_REG(channel)->OQSPIF_STATUSCMDA_REG = statuscmd_reg;

        statuscmd_reg = OSPI_REG(channel)->OQSPIF_STATUSCMDB_REG;

        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_REQ, statuscmd_reg,
                                 p_read_status_instr_cfg->rstat_req);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_SPLIT_EN, statuscmd_reg,
                                 p_read_status_instr_cfg->rstat_split_en);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_RDB_EN,
                                 statuscmd_reg, p_read_status_instr_cfg->rstat_rdb_en);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_DMY_EN,
                                 statuscmd_reg, p_read_status_instr_cfg->rstat_dmy_en);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_DMY_ZERO,
                                 statuscmd_reg, p_read_status_instr_cfg->dummy_value);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_DMY_TX_MD,
                                 statuscmd_reg, p_read_status_instr_cfg->dummy_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMDB_REG, RSTAT_DMY_NUM,
                                 statuscmd_reg, p_read_status_instr_cfg->rstat_dmy_num);

        OSPI_REG(channel)->OQSPIF_STATUSCMDB_REG = statuscmd_reg;
#else
        uint32_t statuscmd_reg = OSPI_REG(channel)->OQSPIF_STATUSCMD_REG;

        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, BUSY_VAL, statuscmd_reg,
                                 p_read_status_instr_cfg->busy_level);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, STSDLY_SEL, statuscmd_reg,
                                 p_read_status_instr_cfg->stsdly_sel);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RESSTS_DLY, statuscmd_reg,
                                 p_read_status_instr_cfg->delay_cycles);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, BUSY_POS, statuscmd_reg,
                                 p_read_status_instr_cfg->busy_pos);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_RX_MD, statuscmd_reg,
                                 p_read_status_instr_cfg->receive_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_TX_MD, statuscmd_reg,
                                 p_read_status_instr_cfg->opcode_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_INST, statuscmd_reg,
                                p_read_status_instr_cfg->opcode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_DMY_EN,
                                 statuscmd_reg, p_read_status_instr_cfg->rstat_dmy_en);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_DMY_ZERO,
                                 statuscmd_reg, p_read_status_instr_cfg->dummy_value);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_DMY_TX_MD,
                                 statuscmd_reg, p_read_status_instr_cfg->dummy_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_STATUSCMD_REG, RSTAT_DMY_NUM,
                                 statuscmd_reg, p_read_status_instr_cfg->rstat_dmy_num);

        OSPI_REG(channel)->OQSPIF_STATUSCMD_REG = statuscmd_reg;
#endif
    }
}

/*******************************************************************************************************************//**
 * Initialize the write enable instruction of the OQSPIF.
 *
 * @param[in] channel                  OSPI controller id.
 * @param[in] p_wrt_enble_instr_cfg    Pointer to configuration structure of the write enable instruction.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_write_enable_instr_init (uint8_t                                 channel,
                                                            const ospi_w_write_enable_instr_cfg_t * p_wrt_enble_instr_cfg)
{
    if (p_wrt_enble_instr_cfg)
    {
        OSPI_REG(channel)->OQSPIF_ERASECMDA_REG_b.OSPI_W_REG_FIELD(WEN_INST)  = p_wrt_enble_instr_cfg->opcode;
        OSPI_REG(channel)->OQSPIF_ERASECMDB_REG_b.OSPI_W_REG_FIELD(WEN_TX_MD) = p_wrt_enble_instr_cfg->opcode_bus_mode;
    }
}

/*******************************************************************************************************************//**
 * Initialize the erase instruction of the OQSPIF.
 *
 * @param[in] channel              OSPI controller id.
 * @param[in] p_erase_instr_cfg    Pointer to configuration structure of the erase instruction.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_erase_instr_init (uint8_t                          channel,
                                                     const ospi_w_erase_instr_cfg_t * p_erase_instr_cfg)
{
    if (p_erase_instr_cfg)
    {
        OSPI_REG(channel)->OQSPIF_ERASECMDA_REG_b.OSPI_W_REG_FIELD(ERS_INST) = p_erase_instr_cfg->opcode;

        uint32_t erasecmdb_reg = OSPI_REG(channel)->OQSPIF_ERASECMDB_REG;

        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, ERS_TX_MD, erasecmdb_reg,
                                 p_erase_instr_cfg->opcode_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, EAD_TX_MD, erasecmdb_reg,
                                 p_erase_instr_cfg->addr_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, ERSRES_HLD, erasecmdb_reg,
                                 p_erase_instr_cfg->hclk_cycles);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, ERS_CS_HI, erasecmdb_reg,
                                 p_erase_instr_cfg->cs_idle_delay_cycles);

        OSPI_REG(channel)->OQSPIF_ERASECMDB_REG = erasecmdb_reg;
    }
}

/*******************************************************************************************************************//**
 * Initialize the program and erase suspend/resume instruction of the OQSPIF.
 *
 * @param[in] channel                OSPI controller id.
 * @param[in] p_sus_res_instr_cfg    Pointer to configuration structure of the program and erase suspend/resume
 *                                   instruction.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_suspend_resume_instr_init (uint8_t                                   channel,
                                                              const ospi_w_suspend_resume_instr_cfg_t * p_sus_res_instr_cfg)
{
    if (p_sus_res_instr_cfg)
    {
        uint32_t erasecmda_reg = OSPI_REG(channel)->OQSPIF_ERASECMDA_REG;
        uint32_t erasecmdb_reg = OSPI_REG(channel)->OQSPIF_ERASECMDB_REG;

        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDA_REG, SUS_INST, erasecmda_reg, p_sus_res_instr_cfg->suspend_opcode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDA_REG, RES_INST, erasecmda_reg, p_sus_res_instr_cfg->resume_opcode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, SUS_TX_MD, erasecmdb_reg, p_sus_res_instr_cfg->suspend_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, RES_TX_MD, erasecmdb_reg, p_sus_res_instr_cfg->resume_bus_mode);
        OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECMDB_REG, RESSUS_DLY, erasecmdb_reg,
                                 p_sus_res_instr_cfg->res_sus_latency_clk_cycles);

        OSPI_REG(channel)->OQSPIF_ERASECMDA_REG = erasecmda_reg;
        OSPI_REG(channel)->OQSPIF_ERASECMDB_REG = erasecmdb_reg;

        OQSPIF->OQSPIF_ERASECMDC_REG_b.OSPI_W_REG_FIELD(SUSSTS_DLY) = p_sus_res_instr_cfg->sussts_dly;
    }
}

/*******************************************************************************************************************//**
 * Initialize the write instruction of the OQSPIF in auto mode.
 *
 * @param[in] channel            OSPI controller id.
 * @param[in] p_wrt_instr_cfg    Pointer to configuration structure of the write instruction (auto mode).
 *
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_write_instr_init (uint8_t channel, const ospi_w_write_instr_cfg_t * p_wrt_instr_cfg)
{
#if OSPI_W_REG_FIELD_MSK(OQSPIF_AWRITECMDA_REG, WR_INST)
    uint32_t reg_val = OSPI_REG(channel)->OQSPIF_AWRITECMDB_REG;

    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDB_REG, SEND_WEN_REQ, reg_val, p_wrt_instr_cfg->send_wen_req);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDB_REG, WR_CS_HIGH_MIN, reg_val, p_wrt_instr_cfg->cs_hi_min_clk_cycles);

    OSPI_REG(channel)->OQSPIF_AWRITECMDB_REG = reg_val;

    reg_val = OSPI_REG(channel)->OQSPIF_AWRITECMDA_REG;

    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_INST, reg_val, p_wrt_instr_cfg->write_opcode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_INST_TX_MD, reg_val, p_wrt_instr_cfg->opcode_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_ADR_TX_MD, reg_val, p_wrt_instr_cfg->addr_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_DAT_TX_MD, reg_val, p_wrt_instr_cfg->data_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_INST_WB, reg_val, p_wrt_instr_cfg->write_opcode_wb);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_DMY_TX_MD, reg_val, p_wrt_instr_cfg->dummy_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_DMY_EN, reg_val, p_wrt_instr_cfg->dummy_en);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_DMY_NUM, reg_val, p_wrt_instr_cfg->dummy_bytes);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_AWRITECMDA_REG, WR_WDEX_EN, reg_val, p_wrt_instr_cfg->wdex_en);

    OSPI_REG(channel)->OQSPIF_AWRITECMDA_REG = reg_val;
#else
    FSP_PARAMETER_NOT_USED(channel);
    FSP_PARAMETER_NOT_USED(p_wrt_instr_cfg);
#endif
}

/*******************************************************************************************************************//**
 * Initialize the burst break instruction of the OQSPIF.
 *
 * @param[in] channel              OSPI controller id.
 * @param[in] p_break_instr_cfg    Pointer to configuration structure of Burst break instruction.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_break_instr_init (uint8_t                          channel,
                                                     const ospi_w_break_instr_cfg_t * p_break_instr_cfg)
{
    OSPI_REG(channel)->OQSPIF_BURSTBRK_REG =
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTBRK_REG, SEC_HF_DS, p_break_instr_cfg->break_sec_hf) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTBRK_REG, BRK_SZ, p_break_instr_cfg->break_sz) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTBRK_REG, BRK_TX_MD, p_break_instr_cfg->break_tx_md) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTBRK_REG, BRK_EN, p_break_instr_cfg->break_en) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTBRK_REG, BRK_WRD, p_break_instr_cfg->break_opcode);
}

/*******************************************************************************************************************//**
 * Initialize the External memory burst of the OQSPIF.
 *
 * @param[in] channel          OSPI controller id.
 * @param[in] p_memblen_cfg    Pointer to configuration structure of External memory burst.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_memblen_init (uint8_t channel, const ospi_w_memblen_cfg_t * p_memblen_cfg)
{
#if OSPI_W_REG_FIELD_MSK(OQSPIF_MEMBLEN_REG, MEMBLEN)
    OSPI_REG(channel)->OQSPIF_MEMBLEN_REG =
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, MEMBLEN, p_memblen_cfg->memblen) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, T_CEM_EN, p_memblen_cfg->tcem_en) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, T_CEM_CC, p_memblen_cfg->tcem_cc) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, RD_LIN_EN, p_memblen_cfg->rd_lin_en) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, KEEP_ACTIVE, p_memblen_cfg->keep_active) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, WCMD_HYBRID, p_memblen_cfg->wcmd_hybrid) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, DIELEN, p_memblen_cfg->dielen) |
        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_MEMBLEN_REG, ACTIVE_THR, p_memblen_cfg->active_thr);
#else
    FSP_PARAMETER_NOT_USED(channel);
    FSP_PARAMETER_NOT_USED(p_memblen_cfg);
#endif
}

/*******************************************************************************************************************//**
 * Get erase status.
 *
 * @param[in] channel             OSPI controller id.
 *
 * @retval    OSPI_W_ERS_NO         No erase
 * @retval    OSPI_W_ERS_PENDING    Pending erase request.
 * @retval    OSPI_W_ERS_RUNNING    Erase procedure is running.
 * @retval    OSPI_W_ERS_SUSPENDED  Suspended erase procedure.
 * @retval    OSPI_W_ERS_FINISHING  Finishing the erase procedure.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE ospi_w_ers_t r_ospi_w_get_erase_status (uint8_t channel)
{
    OSPI_REG(channel)->OQSPIF_CHCKERASE_REG = 0;

    return OSPI_REG(channel)->OQSPIF_ERASECTRL_REG_b.OSPI_W_REG_FIELD(ERS_STATE);
}

/*******************************************************************************************************************//**
 * Generate 8 bits data transfer from the external device to the OQSPIF (manual mode).
 *
 * @param[in] channel    OSPI controller id.
 *
 * @return    uint8_t    8 bits value read from the bus.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE uint8_t r_ospi_w_read8 (uint8_t channel)
{
    volatile ospi_w_data_t * p_tmp = (volatile ospi_w_data_t *) &(OSPI_REG(channel)->OQSPIF_READDATA_REG);

    return p_tmp->data8;
}

/*******************************************************************************************************************//**
 * Fast copy of a buffer to a FIFO.
 *
 * @details Implementation of a fast copy of the contents of a buffer to a FIFO in assembly.
 *          All addresses are word aligned.
 *
 * @param[in]  start    Pointer to the beginning of the buffer.
 * @param[in]  end      Pointer to the end of the buffer.
 * @param[out] dest     Pointer to the FIFO.
 *
 * @warning No validity checks are made! It is the responsibility of the caller to make sure that
 *          sane values are passed to this function.
 **********************************************************************************************************************/
__STATIC_FORCEINLINE void r_ospi_w_fast_write_to_fifo32 (uint32_t start, uint32_t end, uint32_t dest)
{
    __asm volatile ("copy:                                  \n"
                    "       ldmia %[start]!, {r3}           \n"
                    "       str r3, [%[dest]]               \n"
                    "       cmp %[start], %[end]            \n"
                    "       blt copy                        \n"
                    :
                    :                                                         /* output */
                    [start] "l" (start), [end] "r" (end), [dest] "l" (dest) : /* inputs (%0, %1, %2) */
                    "r3");                                                    /* registers that are destroyed. */
}

/*******************************************************************************************************************//**
 * Enter Manual Access Mode.
 *
 * @note This function does not turn the OSPI Flash memory out of the XiP Mode of operation, if enabled.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_enter_manual_access_mode (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    /* XIP mode must be exited before switching to manual mode. */
    if (p_instance_ctrl->xip_mode_is_enabled)
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_exit_command, false);
    }

    OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(AUTO_MD) = OSPI_W_ACCESS_MODE_MANUAL;
}

/*******************************************************************************************************************//**
 * Enters or exits XiP (execute in place) mode.
 *
 * @note Quad SPI instructions require the non-volatile Quad Enable bit (QE) in Status Register to be set.
 * This is a user task.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] code               Code to place in M7-M0.
 * @param[in] enter_mode         True to enter, false to exit.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_xip (ospi_w_instance_ctrl_t * p_instance_ctrl, uint8_t code, bool enter_mode)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    if (enter_mode)
    {
        OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG_b.OSPI_W_REG_FIELD(INST_MD) = OSPI_W_INSTR_MD_TX_ONLY_IN_FIRST_ACCESS;
        r_ospi_w_set_extra_byte(channel, code, p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg);
    }
    else
    {
        OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG_b.OSPI_W_REG_FIELD(INST_MD) = OSPI_W_INSTR_MD_TX_AT_ANY_BURST_ACCESS;
        r_ospi_w_set_extra_byte(channel, code, p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg);

        /* Read from OSPI_W (preferably non-cached access) to send
         * the XiP exit request (i.e. code - extra byte - mode bits). */
        (void) *((volatile uint8_t *) OSPI_W_DEVICE_START_ADDRESS_DATA);
    }
}

/*******************************************************************************************************************//**
 * Set OQSPIF bus mode.
 *
 * @param[in] channel     OSPI controller id.
 * @param[in] bus_mode    The OQSPIF Bus Mode.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_manual_access_bus_mode_set (uint8_t channel, ospi_w_bus_mode_t bus_mode)
{
    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = 1U << bus_mode;
    r_ospi_w_set_io(channel, bus_mode);
}

/*******************************************************************************************************************//**
 * Set device to auto mode.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_enter_auto_access_mode (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    ospi_w_extended_cfg_t * p_inst_extend         = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    ospi_w_bus_mode_t       opcode_mode_auto      = p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg->opcode_bus_mode;
    ospi_w_bus_mode_t       bus_mode_auto         = p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg->data_bus_mode;
    uint8_t                 channel               = p_inst_extend->channel;

    /* When the opcode bus mode differs between manual mode and auto mode,
       a mode-change command must be sent to the flash. */
    r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == opcode_mode_auto);

    /* Restore the XIP mode before returning to auto mode. */
    if (p_instance_ctrl->xip_mode_is_enabled)
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_enter_command, true);
    }

    /*
     * Before switching to Auto Access Mode set the direction of all OQSPIF IOs so that they are
     * selected by the controller.
     */
    r_ospi_w_set_io(channel, bus_mode_auto);
    OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(AUTO_MD) = OSPI_W_ACCESS_MODE_AUTO;
}

/*******************************************************************************************************************//**
 * Write data to flash memory.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] addr               Zero based address of the flash memory, where the
 *                               content of the p_buf is to be written.
 * @param[in] p_buf              Pointer to the source data.
 * @param[in] size               Number of bytes to write.
 *
 * @return    Number of written bytes by the function call.
 *
 * @warning   This function switches and leaves the OQSPIF to manual access mode. Therefore, it must
 *            be called with disabled interrupts. It's up to the caller to switch the OQSPIF back to
 *            auto access mode, in order to re-enable XiP.
 *
 * @warning   The write operation will not exceed the page boundary. Thus, It's up to the caller to
 *            issue another call of the function, in order to write the remaining data to the next page.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static uint32_t r_ospi_w_flash_write_page (ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                 uint32_t                 addr,
                                                                 const uint8_t          * p_buf,
                                                                 uint32_t                 size)
{
    ospi_w_extended_cfg_t   * p_inst_extend   = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                   channel         = p_inst_extend->channel;
    uint8_t                   opcode          = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->write_opcode;
    ospi_w_bus_mode_t         opcode_bus_mode = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->opcode_bus_mode;
    ospi_w_bus_mode_t         addr_bus_mode   = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->addr_bus_mode;
    ospi_w_bus_mode_t         dummy_bus_mode  = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->dummy_bus_mode;
    ospi_w_bus_mode_t         data_bus_mode   = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->data_bus_mode;
    uint8_t                   dummy_bytes     = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->dummy_bytes;
    bool                      dummy_en        = p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg->dummy_en;
    spi_flash_address_bytes_t addr_bytes      = p_instance_ctrl->p_cfg->address_bytes;

    /* Reduce max write size, that can reduce interrupt latency time. */
    if (size > p_instance_ctrl->p_cfg->page_size_bytes)
    {
        size = p_instance_ctrl->p_cfg->page_size_bytes;
    }

    /* Make sure write will not cross page boundary. */
    uint32_t page_boundary = p_instance_ctrl->p_cfg->page_size_bytes - (addr & OSPI_W_FLASH_PAGE_SIZE);
    if (size > page_boundary)
    {
        size = page_boundary;
    }

    /* Send Write Enable command. */
    r_ospi_w_flash_write_enable(p_instance_ctrl);

    /* Send Write command. */
    r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == opcode_bus_mode);
    r_ospi_w_manual_access_bus_mode_set(channel, opcode_bus_mode);
    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, EN_CS);
    r_ospi_w_write8(channel, opcode);

    /* Send address for Write command. */
    r_ospi_w_manual_access_bus_mode_set(channel, addr_bus_mode);
    if (SPI_FLASH_ADDRESS_BYTES_4 == addr_bytes)
    {
        r_ospi_w_write32(channel, addr);
    }
    else
    {
        r_ospi_w_write8(channel, (uint8_t) ((addr >> 16U) & 0xFF));
        r_ospi_w_write8(channel, (uint8_t) ((addr >> 8U) & 0xFF));
        r_ospi_w_write8(channel, (uint8_t) (addr & 0xFF));
    }

    /* Send dummy bytes. */
    if (dummy_en)
    {
        r_ospi_w_manual_access_bus_mode_set(channel, dummy_bus_mode);
        for (uint32_t i = 0; i < (uint32_t) dummy_bytes + 1; i++)
        {
            r_ospi_w_dummy8(channel);
        }
    }

    /* Write the unaligned head (not aligned to a 4-byte address). */
    r_ospi_w_manual_access_bus_mode_set(channel, data_bus_mode);
    uint32_t written_bytes = 0;
    uint32_t odd_bytes;
    if (size < 4)
    {
        odd_bytes = size;
    }
    else
    {
        odd_bytes = (4 - (((uint32_t) p_buf) % 4)) % 4;
    }
    for (written_bytes = 0; written_bytes < odd_bytes && written_bytes < size; ++written_bytes)
    {
        r_ospi_w_write8(channel, p_buf[written_bytes]);
    }

    /* Write the aligned middle (aligned to a 4-byte address). */
    uint32_t size_aligned32 = (size - written_bytes) & 0xFFFFFFFC;
    if (size_aligned32)
    {
        r_ospi_w_fast_write_to_fifo32((uint32_t) (p_buf + written_bytes),
                                      (uint32_t) (p_buf + written_bytes + size_aligned32),
                                      (uint32_t) &((OSPI_REG(channel))->OQSPIF_WRITEDATA_REG));
        written_bytes += size_aligned32;
    }

    /* Write the unaligned tail (not aligned to a 4-byte address). */
    for ( ; written_bytes < size; ++written_bytes)
    {
        r_ospi_w_write8(channel, p_buf[written_bytes]);
    }
    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, DIS_CS);

    return written_bytes;
}

/*******************************************************************************************************************//**
 * Return the zero-based chip address from the memory mapped address.
 *
 * @param[in] p_addr    Pointer to memory mapped address.
 *
 * @return    uint32_t  Zero-based chip address.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static uint32_t r_ospi_w_get_chip_address (uint8_t * const p_addr)
{
    if ((uint32_t) p_addr >= OSPI_W_DEVICE_START_ADDRESS_DATA)
    {
        return (uint32_t) p_addr - OSPI_W_DEVICE_START_ADDRESS_DATA;
    }
    else
    {
        return (uint32_t) p_addr - OSPI_W_DEVICE_START_ADDRESS;
    }
}

/*******************************************************************************************************************//**
 * Erase block/sector of flash memory.
 *
 * @note        Before erasing the flash memory, it is mandatory to set up the erase instructions
 *              first by calling r_ospi_w_erase_instr_init().
 *
 * @note        Call r_ospi_w_get_erase_status() to check whether the erase operation has finished.
 *
 * @note        Before switching the OSPI controller to manual mode check that
 *              r_ospi_w_get_erase_status() == OSPI_W_ERASE_STATUS_NO.
 *
 * @param[in]  channel    OSPI controller id.
 * @param[in]  addr       Memory address of the block/sector to be erased.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_erase_block (uint8_t channel, uint32_t addr)
{
    uint32_t block_sector = addr / OSPI_W_FLASH_SECTOR_SIZE;

    /* Wait for previous erase to end. */
    while (OSPI_W_ERS_NO != r_ospi_w_get_erase_status(channel));

    if (OSPI_W_ACCESS_MODE_AUTO != OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(AUTO_MD))
    {
        OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(AUTO_MD) = OSPI_W_ACCESS_MODE_AUTO;
    }

    /*
     * In order to calculate the sector number and set the proper value for the ERS_ADDR
     * field in the OQSPIF_ERASECTRL_REG, the given address needs to be divided by the sector size.
     * However, when using 24 bits addressing size, the sector number needs to be stored in the
     * higher 12 bits of the field, as the remaining bits are disregarded. As a result, the sector
     * number must be shifted left by 8 bits to account for this.
     *
     *                          OQSPIF_ERASECTRL_REG[ERS_ADDR]
     *     =====================================================================
     *              19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
     *     =====================================================================
     *     24 bits:  v  v  v  v  v  v  v  v  v  v  v  v  x  x  x  x  x  x  x  x
     *     32 bits:  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v  v
     */
    if (0 == OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(USE_32BA))
    {
        block_sector <<= 8U;
    }

    uint32_t erasectrl_reg = OSPI_REG(channel)->OQSPIF_ERASECTRL_REG;

    /* Setup erase block page. */
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECTRL_REG, ERS_ADDR, erasectrl_reg, block_sector);

    /* Trigger erase. */
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_ERASECTRL_REG, ERASE_EN, erasectrl_reg, 1);

    OSPI_REG(channel)->OQSPIF_ERASECTRL_REG = erasectrl_reg;
}

/*******************************************************************************************************************//**
 * Erase sector of flash memory.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] addr               Memory address of the sector to be erased.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_erase_flash_sector (ospi_w_instance_ctrl_t * p_instance_ctrl, uint32_t addr)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);
    while (r_ospi_w_flash_is_busy(p_instance_ctrl));
    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    r_ospi_w_erase_block(channel, addr);

    /* Ensure erase block has started. */
    while (OSPI_W_ERS_NO == r_ospi_w_get_erase_status(channel));

    /* Wait until the erase is finished. */
    while (OSPI_W_ERS_NO != r_ospi_w_get_erase_status(channel));

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;
}

/*******************************************************************************************************************//**
 * Get erase instruction code.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] byte_count         Number of bytes to be erased.
 *
 * @return    uint16_t           Erase instruction code.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static uint16_t r_ospi_w_get_erase_opcode (ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                 uint32_t                 byte_count)
{
    uint16_t erase_command = 0;

    for (uint32_t idx = 0; idx < p_instance_ctrl->p_cfg->erase_command_list_length; idx++)
    {
        /* If requested byte_count is supported by underlying flash, store the command. */
        if (byte_count == p_instance_ctrl->p_cfg->p_erase_command_list[idx].size)
        {
            erase_command = p_instance_ctrl->p_cfg->p_erase_command_list[idx].command;
            break;
        }
    }

    return erase_command;
}

/*******************************************************************************************************************//**
 * Erase chip.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] erase_command      Erase instruction code.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_erase_chip (ospi_w_instance_ctrl_t * p_instance_ctrl, uint8_t erase_command)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;
    ospi_w_bus_mode_t       bus_mode      = p_inst_extend->p_ospi_flash_cfg->p_erase_instr_cfg->opcode_bus_mode;

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);
    while (r_ospi_w_flash_is_busy(p_instance_ctrl));
    r_ospi_w_flash_write_enable(p_instance_ctrl);
    r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == bus_mode);
    r_ospi_w_manual_access_bus_mode_set(channel, bus_mode);
    r_ospi_w_flash_cmd(channel, erase_command);
    while (r_ospi_w_flash_is_busy(p_instance_ctrl));
    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;
}

/*******************************************************************************************************************//**
 * Initialize the OSPI controller instruction registers.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_instructions_init (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    r_ospi_w_read_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg);
    r_ospi_w_break_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_break_instr_cfg);
    r_ospi_w_memblen_init(channel, p_inst_extend->p_ospi_flash_cfg->p_memblen_cfg);
    r_ospi_w_write_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_write_instr_cfg);
    r_ospi_w_read_status_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg);
    r_ospi_w_erase_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_erase_instr_cfg);
    r_ospi_w_suspend_resume_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_suspend_resume_instr_cfg);
    r_ospi_w_write_enable_instr_init(channel, p_inst_extend->p_ospi_flash_cfg->p_write_enable_instr_cfg);

#if OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLDDRA_REG, DDR_CMD_EN)
    if (p_inst_extend->p_ospi_flash_cfg->p_ctrl_ddr)
    {
        OSPI_REG(channel)->OQSPIF_CTRLDDRA_REG = p_inst_extend->p_ospi_flash_cfg->p_ctrl_ddr->ctrl_ddra;
        OSPI_REG(channel)->OQSPIF_CTRLDDRB_REG = p_inst_extend->p_ospi_flash_cfg->p_ctrl_ddr->ctrl_ddrb;
    }
#endif

#if OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLMR_REG, MR_R_INST)
    if (p_inst_extend->p_ospi_flash_cfg->p_extra_regs)
    {
        OSPI_REG(channel)->OQSPIF_CTRLMR_REG   = p_inst_extend->p_ospi_flash_cfg->p_extra_regs->ctrl_mr_reg;
        OSPI_REG(channel)->OQSPIF_DRST_CMD_REG = p_inst_extend->p_ospi_flash_cfg->p_extra_regs->drst_cmd_reg;
    }
#endif

    /* If the configuration is set to use XIP mode, it automatically transitions into XIP mode. */
    if (OSPI_W_INSTR_MD_TX_ONLY_IN_FIRST_ACCESS == p_inst_extend->p_ospi_flash_cfg->p_read_instr_cfg->instr_md)
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_enter_command, true);
        p_instance_ctrl->xip_mode_is_enabled = true;
    }
    else
    {
        r_ospi_w_xip(p_instance_ctrl, p_instance_ctrl->p_cfg->xip_exit_command, false);
        p_instance_ctrl->xip_mode_is_enabled = false;
    }
}

/*******************************************************************************************************************//**
 * Initialize the OSPI controller based on the OSPI_W flash driver.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_controller_init (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    uint32_t clkamba_reg = CRG_TOP->CLK_AMBA_REG;

#if CRG_TOP_CLK_AMBA_REG_QSPI_ENABLE_Msk
    FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, QSPI_SDR_MODE, clkamba_reg, p_inst_extend->ospi_mode);
    FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, QSPI_SDR_DIV2, clkamba_reg, p_inst_extend->ospi_clk_div);
    FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, QSPI_ENABLE, clkamba_reg, 1);
#else
    FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, OQSPIF_DIV, clkamba_reg, p_inst_extend->ospi_clk_div);
    FSP_REG_VAR_FIELD_SET(CRG_TOP, CLK_AMBA_REG, OQSPIF_ENABLE, clkamba_reg, 1);
#endif

    CRG_TOP->CLK_AMBA_REG = clkamba_reg;

    /* CLK_AMBA_REG is in APB (PCLK might be slower than HCLK). */
    FSP_HARDWARE_REGISTER_WAIT(CRG_TOP->CLK_AMBA_REG, clkamba_reg);

    uint32_t ctrlmode_reg = OSPI_REG(channel)->OQSPIF_CTRLMODE_REG;
    uint32_t gp_reg       = OSPI_REG(channel)->OQSPIF_GP_REG;

    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, USE_32BA, ctrlmode_reg,
                             (SPI_FLASH_ADDRESS_BYTES_4 == p_instance_ctrl->p_cfg->address_bytes));
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, CLK_MD, ctrlmode_reg, p_inst_extend->p_ospi_flash_cfg->clk_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, RXD_NEG, ctrlmode_reg, OSPI_W_SAMPLING_EDGE_POS);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, RPIPE_EN, ctrlmode_reg, OSPI_W_READ_PIPE_ENABLE);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, PCLK_MD, ctrlmode_reg, OSPI_W_READ_PIPE_DELAY);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_CTRLMODE_REG, HRDY_MD, ctrlmode_reg, OSPI_W_HREADY_MODE_WAIT);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_GP_REG, PADS_SLEW, gp_reg, p_inst_extend->ospi_slew_rate);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_GP_REG, PADS_DRV, gp_reg, p_inst_extend->ospi_drive_current);

    OSPI_REG(channel)->OQSPIF_CTRLMODE_REG = ctrlmode_reg;
    OSPI_REG(channel)->OQSPIF_GP_REG       = gp_reg;

    OSPI_REG(channel)->OQSPIF_CTRLMODE_REG_b.OSPI_W_REG_FIELD(MAN_DIRCHG_MD) = 0;
}

/*******************************************************************************************************************//**
 * Based on the requested protocol, return the bus mode of the instruction (or opcode) and addr/extra/dummy/data bus.
 *
 * @note The protocol mode xS-xS-xS means that the bus (opcode, address and data) is in Single Data Rate.
 *       The protocol mode xD-xD-xD means that the bus (opcode, address and data) is in Double Data Rate.
 *
 * @param[in]  spi_protocol       Requested protocol.
 * @param[out] p_bus_mode         Pointer to store data.
 *
 * @retval FSP_SUCCESS            Configuration was successful.
 * @retval FSP_ERR_UNSUPPORTED    Protocol is not supported by OSPI_W.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_get_bus_mode (spi_flash_protocol_t spi_protocol,
                                                              ospi_w_bus_mode_t  * p_bus_mode)
{
    switch (spi_protocol)
    {
        case SPI_FLASH_PROTOCOL_EXTENDED_SPI: // SPI_FLASH_PROTOCOL_1S_1S_1S is the same enum value.
        {
            p_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE]    = OSPI_W_BUS_MODE_SINGLE;
            p_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = OSPI_W_BUS_MODE_SINGLE;
            break;
        }

        case SPI_FLASH_PROTOCOL_1S_2S_2S:
        {
            p_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE]    = OSPI_W_BUS_MODE_SINGLE;
            p_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = OSPI_W_BUS_MODE_DUAL;
            break;
        }

        case SPI_FLASH_PROTOCOL_1S_4S_4S:
        {
            p_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE]    = OSPI_W_BUS_MODE_SINGLE;
            p_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = OSPI_W_BUS_MODE_QUAD;
            break;
        }

        case SPI_FLASH_PROTOCOL_QPI:
        case SPI_FLASH_PROTOCOL_4S_4S_4S:
        {
            p_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE]    = OSPI_W_BUS_MODE_QUAD;
            p_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = OSPI_W_BUS_MODE_QUAD;
            break;
        }

        case SPI_FLASH_PROTOCOL_2S_2S_2S:
        case SPI_FLASH_PROTOCOL_4S_4D_4D:
        case SPI_FLASH_PROTOCOL_SOPI:
        case SPI_FLASH_PROTOCOL_DOPI:
        case SPI_FLASH_PROTOCOL_8D_8D_8D:
        default:
        {
            FSP_RETURN(FSP_ERR_UNSUPPORTED);
            break;
        }
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Check if the Flash can accept commands.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 *
 * @return    bool               True if the Flash is not busy else false.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static bool r_ospi_w_flash_is_writable (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    bool writable;

    /* Disable the interrupts as long as the OQSPIF remains in manual access mode. */
    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;

    r_ospi_w_enter_manual_access_mode(p_instance_ctrl);

    /* Check if flash is ready. */
    writable = !(r_ospi_w_flash_is_busy(p_instance_ctrl));

    r_ospi_w_enter_auto_access_mode(p_instance_ctrl);

    /* Re-enable the interrupts since the OQSPIF switched back to auto access mode. */
    FSP_CRITICAL_SECTION_EXIT;

    return writable;
}

/*******************************************************************************************************************//**
 * Send flash command.
 *
 * @param[in]  channel    OSPI controller id.
 * @param[in]  opcode     Command code to be send to the flash device.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_flash_cmd (uint8_t channel, const uint8_t opcode)
{
    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, EN_CS);
    r_ospi_w_write8(channel, opcode);
    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, DIS_CS);
}

/*******************************************************************************************************************//**
 * Set WEL (Write Enable Latch) bit of the Status Register of the Flash.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 *
 * @details The WEL bit must be set prior to every Page Program, Quad Page Program, Sector Erase,
 *          Block Erase, Chip Erase, Write Status Register and Erase/Program Security Registers
 *          instruction. In the case of Write Status Register command, any status bits will be written
 *          as non-volatile bits.
 *
 * @note    This function blocks until the Flash has processed the command and it will be repeated if,
 *          for any reason, the command was not successfully executed by the Flash.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_flash_write_enable (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    uint8_t           status;
    uint8_t           opcode   = p_inst_extend->p_ospi_flash_cfg->p_write_enable_instr_cfg->opcode;
    ospi_w_bus_mode_t bus_mode = p_inst_extend->p_ospi_flash_cfg->p_write_enable_instr_cfg->opcode_bus_mode;

    /* Wait till the Write enable bit in the flash device status register is set. */
    do
    {
        r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == bus_mode);
        r_ospi_w_manual_access_bus_mode_set(channel, bus_mode);
        r_ospi_w_flash_cmd(channel, opcode);

        /* Wait till the WIP(Write in progress bit) in the flash device status register is cleared. */
        do
        {
            status = r_ospi_w_read_status_register(p_instance_ctrl);
        } while ((status & (1U << p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->busy_pos)) ==
                 p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->busy_level);
    } while (!(status & (1U << p_instance_ctrl->p_cfg->write_enable_bit)));
}

/*******************************************************************************************************************//**
 * Check if the device is busy.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 *
 * @return    bool               True if the BUSY bit is set else false.
 *
 * @warning This function checks the value of the BUSY bit in the Status Register 1 of the Flash. It
 *          is the responsibility of the caller to call the function in the right context. The
 *          function must be called with interrupts disabled.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static bool r_ospi_w_flash_is_busy (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    uint8_t                 status_reg    = r_ospi_w_read_status_register(p_instance_ctrl);
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    bool                    status_wip;
    
    status_wip = (status_reg & (1U << p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->busy_pos)) ==
                 p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->busy_level;

    return status_wip;
}

/*******************************************************************************************************************//**
 * Set an extra byte to use with read instructions, used to enter/stay/exit XiP mode.
 *
 * @param[in] channel       OSPI controller id.
 * @param[in] extra_byte    An extra byte transferred after the address asking memory to
 *                          stay in XiP mode or wait for a normal instruction
 *                          after CS goes inactive.
 * @param[in] p_ospi_cfg    Pointer to configuration structure of the read instruction.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_set_extra_byte (uint8_t                         channel,
                                                           uint8_t                         extra_byte,
                                                           ospi_w_read_instr_cfg_t const * p_ospi_cfg)
{
    OSPI_REG(channel)->OQSPIF_BURSTCMDA_REG =
                        (OSPI_REG(channel)->OQSPIF_BURSTCMDA_REG &
                         ~(OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDA_REG, EXT_BYTE) |
                           OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDA_REG, EXT_TX_MD))) |
                        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTCMDA_REG, EXT_BYTE, extra_byte) |
                        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTCMDA_REG, EXT_TX_MD, p_ospi_cfg->extra_byte_bus_mode);

    OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG =
                        (OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG &
                         ~(OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, EXT_BYTE_EN) |
                           OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, EXT_HF_DS))) |
                        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTCMDB_REG, EXT_BYTE_EN, p_ospi_cfg->extra_byte_en) |
                        OSPI_W_REG_FIELD_SET_BITS32(OQSPIF_BURSTCMDB_REG, EXT_HF_DS, p_ospi_cfg->extra_byte_half_cfg);
}

/*******************************************************************************************************************//**
 * Initialize the read instruction of the OQSPIF.
 *
 * @param[in] channel             OSPI controller id.
 * @param[in] p_read_instr_cfg    Pointer to configuration structure of the read instruction.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_read_instr_init (uint8_t                         channel,
                                                            const ospi_w_read_instr_cfg_t * p_read_instr_cfg)
{
    uint32_t burstcmda_reg = OSPI_REG(channel)->OQSPIF_BURSTCMDA_REG;
    uint32_t burstcmdb_reg = OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG;

    /* OQSPIF_BURSTCMDA_REG. */
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, INST, burstcmda_reg, p_read_instr_cfg->opcode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, INST_WB, burstcmda_reg, p_read_instr_cfg->opcode_wb);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, EXT_BYTE, burstcmda_reg, p_read_instr_cfg->extra_byte_value);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, INST_TX_MD, burstcmda_reg, p_read_instr_cfg->opcode_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, ADR_TX_MD, burstcmda_reg, p_read_instr_cfg->addr_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, EXT_TX_MD, burstcmda_reg, p_read_instr_cfg->extra_byte_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDA_REG, DMY_TX_MD, burstcmda_reg, p_read_instr_cfg->dummy_bus_mode);

    /* OQSPIF_BURSTCMDB_REG. */
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, DAT_RX_MD, burstcmdb_reg, p_read_instr_cfg->data_bus_mode);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, EXT_BYTE_EN, burstcmdb_reg, p_read_instr_cfg->extra_byte_en);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, EXT_HF_DS, burstcmdb_reg, p_read_instr_cfg->extra_byte_half_cfg);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, INST_MD, burstcmdb_reg, p_read_instr_cfg->instr_md);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, DMY_NUM, burstcmdb_reg, p_read_instr_cfg->dummy_bytes);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, DMY_EN, burstcmdb_reg, p_read_instr_cfg->dummy_en);
#if OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, WRAP_SIZE)
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, WRAP_SIZE, burstcmdb_reg, p_read_instr_cfg->wrap_size);
#endif
#if OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, WRAP_BLEN)
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, WRAP_BLEN, burstcmdb_reg, p_read_instr_cfg->wrap_blen);
#else
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, WRAP_LEN, burstcmdb_reg, p_read_instr_cfg->wrap_blen);
#endif
#if OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, WRAP_WR_EN)
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, WRAP_WR_EN, burstcmdb_reg, p_read_instr_cfg->wrap_wr_en);
#endif
#if OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, RD_BEND_MD)
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, RD_BEND_MD, burstcmdb_reg, p_read_instr_cfg->rd_bend_md);
#endif
#if OSPI_W_REG_FIELD_MSK(OQSPIF_BURSTCMDB_REG, RD_RDB_EN)
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, RD_RDB_EN, burstcmdb_reg, p_read_instr_cfg->rd_rdb_en);
#endif
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, CS_HIGH_MIN, burstcmdb_reg, p_read_instr_cfg->cs_high_min_cycles);
    OSPI_W_REG_VAR_FIELD_SET(OQSPIF_BURSTCMDB_REG, WRAP_MD, burstcmdb_reg, p_read_instr_cfg->wrap_md);

    OSPI_REG(channel)->OQSPIF_BURSTCMDA_REG = burstcmda_reg;
    OSPI_REG(channel)->OQSPIF_BURSTCMDB_REG = burstcmdb_reg;
}

/*******************************************************************************************************************//**
 * Write an arbitrary number of bytes to the Flash and then read an arbitrary number of bytes
 * from the Flash in one transaction.
 *
 * @param[in]  p_instance_ctrl  Pointer to a driver handle.
 * @param[in]  p_wbuf           Pointer to the beginning of the buffer that contains the data to be written.
 * @param[in]  wlen             The number of bytes to be written.
 * @param[out] p_rbuf           Pointer to the beginning of the buffer that the read data are stored.
 * @param[in]  rlen             The number of bytes to be read.
 *
 * @note The data are transferred as bytes (8 bits wide). No optimization is done in trying to use
 *       faster access methods (i.e. transfer words instead of bytes whenever it is possible).
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_transact (ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                     const uint8_t          * p_wbuf,
                                                     uint32_t                 wlen,
                                                     uint8_t                * p_rbuf,
                                                     uint32_t                 rlen)
{
    ospi_w_extended_cfg_t * p_inst_extend     = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel           = p_inst_extend->channel;
    ospi_w_bus_mode_t       instr_bus_mode    = p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE];
    ospi_w_bus_mode_t       noninstr_bus_mode = p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE];

    r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == instr_bus_mode);
    r_ospi_w_manual_access_bus_mode_set(channel, instr_bus_mode);

    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, EN_CS);

    uint32_t i;
    for (i = 0; i < p_instance_ctrl->manual_command_length; ++i)
    {
        r_ospi_w_write8(channel, p_wbuf[i]);
    }

    if (instr_bus_mode != noninstr_bus_mode)
    {
        r_ospi_w_manual_access_bus_mode_set(channel, noninstr_bus_mode);
    }

    for ( ; i < wlen; ++i)
    {
        r_ospi_w_write8(channel, p_wbuf[i]);
    }

    for (i = 0; i < rlen; ++i)
    {
        p_rbuf[i] = r_ospi_w_read8(channel);
    }

    OSPI_REG(channel)->OQSPIF_CTRLBUS_REG = OSPI_W_REG_FIELD_MSK(OQSPIF_CTRLBUS_REG, DIS_CS);
}

/*******************************************************************************************************************//**
 * Read the Status Register of the Flash.
 *
 * @param[in]  p_instance_ctrl    Pointer to a driver handle.
 *
 * @return     uint8_t            The value of the Status Register of the Flash.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static uint8_t r_ospi_w_read_status_register (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    uint8_t                 status;
    ospi_w_extended_cfg_t * p_inst_extend        = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 cmd[]                = {p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->opcode};
    ospi_w_bus_mode_t       instr_bus_mode_bk    = p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE];
    ospi_w_bus_mode_t       noninstr_bus_mode_bk = p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE];

    p_instance_ctrl->manual_command_length = 1;
    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE] =
        p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->opcode_bus_mode;
    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] =
        p_inst_extend->p_ospi_flash_cfg->p_read_status_instr_cfg->receive_bus_mode;

    r_ospi_w_transact(p_instance_ctrl, cmd, 1, &status, 1);

    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_INSTR_BUS_MODE]    = instr_bus_mode_bk;
    p_instance_ctrl->manual_access_bus_mode[OSPI_W_IDX_NONINSTR_BUS_MODE] = noninstr_bus_mode_bk;

    r_ospi_w_qpi(p_instance_ctrl, OSPI_W_BUS_MODE_QUAD == instr_bus_mode_bk);

    return status;
}

/*******************************************************************************************************************//**
 * Enters or exits QPI mode.
 *
 * @note Quad SPI instructions require the non-volatile Quad Enable bit (QE) in Status Register to be set.
 * This is a user task.
 *
 * @param[in] p_instance_ctrl    Pointer to a driver handle.
 * @param[in] enter_mode         True to enter, false to exit.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static void r_ospi_w_qpi (ospi_w_instance_ctrl_t * p_instance_ctrl, bool enter_mode)
{
    ospi_w_extended_cfg_t * p_inst_extend = (ospi_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;
    uint8_t                 channel       = p_inst_extend->channel;

    if (enter_mode)
    {
        if (OSPI_W_BUS_MODE_SINGLE == p_instance_ctrl->flash_opcode_bus_mode)
        {
            r_ospi_w_manual_access_bus_mode_set(channel, OSPI_W_BUS_MODE_SINGLE);
            r_ospi_w_flash_cmd(channel, p_inst_extend->p_ospi_flash_cfg->p_qpi_instr_cfg->opcode_enter);
            p_instance_ctrl->flash_opcode_bus_mode = OSPI_W_BUS_MODE_QUAD;
        }
    }
    else
    {
        if (OSPI_W_BUS_MODE_QUAD == p_instance_ctrl->flash_opcode_bus_mode)
        {
            r_ospi_w_manual_access_bus_mode_set(channel, OSPI_W_BUS_MODE_QUAD);
            r_ospi_w_flash_cmd(channel, p_inst_extend->p_ospi_flash_cfg->p_qpi_instr_cfg->opcode_exit);
            p_instance_ctrl->flash_opcode_bus_mode = OSPI_W_BUS_MODE_SINGLE;
        }
    }
}

#if OSPI_W_CFG_PARAM_CHECKING_ENABLE

/*******************************************************************************************************************//**
 * Ensures that the parameters are valid and also that there is no write or erase operation inprogress.
 *
 * @param[in]  p_instance_ctrl     Pointer to a driver handle.
 *
 * @retval FSP_SUCCESS             Parameters are valid.
 * @retval FSP_ERR_ASSERTION       A required pointer is NULL.
 * @retval FSP_ERR_NOT_OPEN        Driver is not opened.
 * @retval FSP_ERR_DEVICE_BUSY     The device is busy.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_param_checking_dcom (ospi_w_instance_ctrl_t * p_instance_ctrl)
{
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(OSPI_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Verify device is not busy. */
    FSP_ERROR_RETURN(r_ospi_w_flash_is_writable(p_instance_ctrl), FSP_ERR_DEVICE_BUSY);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Parameter checking for R_OSPI_W_Write.
 *
 * @param[in] p_instance_ctrl      Pointer to a driver handle.
 * @param[in] p_src                The source of the data to write to OSPI_W.
 * @param[in] p_dest               The address in OSPI_W to write to.
 * @param[in] byte_count           The number of bytes to write.
 *
 * @retval FSP_SUCCESS             Parameters are valid.
 * @retval FSP_ERR_ASSERTION       p_instance_ctrl,p_src or p_dest is NULL.
 * @retval FSP_ERR_NOT_OPEN        Driver is not opened.
 * @retval FSP_ERR_DEVICE_BUSY     The device is busy.
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM static fsp_err_t r_ospi_w_program_param_check (ospi_w_instance_ctrl_t * p_instance_ctrl,
                                                                     uint8_t const * const    p_src,
                                                                     uint8_t * const          p_dest,
                                                                     uint32_t                 byte_count)
{
    fsp_err_t err = r_ospi_w_param_checking_dcom(p_instance_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    FSP_ASSERT(NULL != p_src);
    FSP_ASSERT(NULL != p_dest);

    /* Check if byte_count is valid. */
    FSP_ASSERT(byte_count > 0);

    return FSP_SUCCESS;
}

#endif
