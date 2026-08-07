/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_crc_w.h"
#if CRC_W_CFG_DMAC_ENABLE
 #include "r_dmac_w.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* "CRC" in ASCII, used to determine if channel is open. */
#define CRC_W_OPEN              (0x00435243ULL)

#define CRC_W_PRV_TIMEOUT_US    (500000U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/* Filled in Interface API structure for this Instance. */
const crc_api_t g_crc_on_crc_w =
{
    .open         = R_CRC_W_Open,
    .close        = R_CRC_W_Close,
    .calculate    = R_CRC_W_Calculate,
    .crcResultGet = R_CRC_W_CalculatedValueGet,
    .snoopEnable  = R_CRC_W_SnoopEnable,
    .snoopDisable = R_CRC_W_SnoopDisable,
};

/*******************************************************************************************************************//**
 * @addtogroup CRC_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

static void r_crc_w_calc_0_pseudo8(const uint8_t * p_value, uint32_t length);
static void r_crc_w_calc_0_pseudo16(const uint16_t * p_value, uint32_t length);
static void r_crc_w_calc_0_pseudo32(const uint32_t * p_value, uint32_t length);

static void r_crc_w_calc_1_pseudo8(const uint8_t * p_value, uint32_t length);
static void r_crc_w_calc_1_pseudo16(const uint16_t * p_value, uint32_t length);
static void r_crc_w_calc_1_pseudo32(const uint32_t * p_value, uint32_t length);

static uint32_t r_crc_w_sw_remapper(uint32_t vaddr);

#if CRC_W_CFG_DMAC_ENABLE
static fsp_err_t r_crc_w_transfer_open(crc_cfg_t const * const p_cfg);

#endif

/*******************************************************************************************************************//**
 * Open the CRC driver module
 *
 * Implements @ref crc_api_t::open
 *
 * Open the CRC driver module and initialize the driver control block according to the passed-in
 * configuration structure.
 *
 * @retval FSP_SUCCESS             Configuration was successful.
 * @retval FSP_ERR_ASSERTION       p_ctrl or p_cfg is NULL.
 * @retval FSP_ERR_ALREADY_OPEN    Module already open
 * @retval FSP_ERR_TIMEOUT         CRC is busy.
 * @return                         See @ref RENESAS_ERROR_CODES for other possible return codes.
 *                                 This function internally calls @ref transfer_api_t::open.
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_Open (crc_ctrl_t * const p_ctrl, crc_cfg_t const * const p_cfg)
{
    crc_w_instance_ctrl_t * p_instance_ctrl = (crc_w_instance_ctrl_t *) p_ctrl;
    crc_w_extended_cfg_t  * p_cfg_extend;

#if CRC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_cfg);

    /* Verify the control block has not already been initialized. */
    FSP_ERROR_RETURN(CRC_W_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    uint32_t timeout = CRC_W_PRV_TIMEOUT_US;

    /* Save the configuration  */
    p_instance_ctrl->p_cfg = p_cfg;

#if CRC_W_CFG_DMAC_ENABLE

    /* Open the transfer interface if available */
    fsp_err_t err;
    err = r_crc_w_transfer_open(p_cfg);
    if (FSP_SUCCESS != err)
    {
        FSP_RETURN(err);
    }
#endif

    p_cfg_extend = (crc_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (CRC_W_CHANNEL_1 == p_cfg_extend->channel)
    {
        while (CRC_W_CAL_STATUS_BUSY == HW_ACC->CRC_1_STA_REG)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_OP_TYPE     = p_cfg_extend->polynomial;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_PAR_TYPE    = p_cfg_extend->parallel_type;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_SWAP_EN     = p_cfg_extend->swap_enable;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_ENDIAN_TYPE = p_cfg_extend->endian_type;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_PAR_TYPE    = p_cfg_extend->parallel_type;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_ACC_TYPE    = p_cfg_extend->access_type;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_PATH_SEL    = p_cfg_extend->path_selection;
        HW_ACC->CRC_1_CONFIG_REG_b.CRC_1_CHK_ADDR    = CRC_W_CHECKING_ENABLE;

        /* Operation enable. */
        HW_ACC->CRC_1_OP_EN_REG = 1;
    }
    else
    {
        while (CRC_W_CAL_STATUS_BUSY == HW_ACC->CRC_STA_REG)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        HW_ACC->CRC_CONFIG_REG_b.CRC_OP_TYPE     = p_cfg_extend->polynomial;
        HW_ACC->CRC_CONFIG_REG_b.CRC_PAR_TYPE    = p_cfg_extend->parallel_type;
        HW_ACC->CRC_CONFIG_REG_b.CRC_SWAP_EN     = p_cfg_extend->swap_enable;
        HW_ACC->CRC_CONFIG_REG_b.CRC_ENDIAN_TYPE = p_cfg_extend->endian_type;
        HW_ACC->CRC_CONFIG_REG_b.CRC_PAR_TYPE    = p_cfg_extend->parallel_type;
        HW_ACC->CRC_CONFIG_REG_b.CRC_ACC_TYPE    = p_cfg_extend->access_type;
        HW_ACC->CRC_CONFIG_REG_b.CRC_PATH_SEL    = p_cfg_extend->path_selection;
        HW_ACC->CRC_CONFIG_REG_b.CRC_CHK_ADDR    = CRC_W_CHECKING_ENABLE;

        /* Operation enable. */
        HW_ACC->CRC_OP_EN_REG = 1;
    }

    /* Mark driver as initialized by setting the open value to the ASCII equivalent of "CRC" */
    p_instance_ctrl->open = CRC_W_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Close the CRC module driver.
 *
 * Implements @ref crc_api_t::close
 *
 * @retval FSP_SUCCESS             Configuration was successful.
 * @retval FSP_ERR_ASSERTION       p_ctrl is NULL. Status of CRC calculation is ERROR.
 * @retval FSP_ERR_NOT_OPEN        The driver is not opened.
 * @retval FSP_ERR_TIMEOUT         CRC is not stopped.
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_Close (crc_ctrl_t * const p_ctrl)
{
    crc_w_instance_ctrl_t * p_instance_ctrl = (crc_w_instance_ctrl_t *) p_ctrl;
    crc_w_extended_cfg_t  * p_cfg_extend;

#if CRC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ERROR_RETURN(CRC_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint32_t timeout = CRC_W_PRV_TIMEOUT_US;

    p_cfg_extend = (crc_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (CRC_W_CHANNEL_1 == p_cfg_extend->channel)
    {
        FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA, FSP_ERR_ASSERTION);

        HW_ACC->CRC_1_REQ_CTRL_REG = CRC_W_REQ_CLEAR | CRC_W_REQ_START;
        HW_ACC->CRC_1_REQ_CTRL_REG = CRC_W_REQ_STOP;

        while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        /* Operation disable. */
        HW_ACC->CRC_1_OP_EN_REG = 0;
    }
    else
    {
        FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA, FSP_ERR_ASSERTION);

        HW_ACC->CRC_REQ_CTRL_REG = CRC_W_REQ_CLEAR | CRC_W_REQ_START;
        HW_ACC->CRC_REQ_CTRL_REG = CRC_W_REQ_STOP;

        while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        /* Operation disable. */
        HW_ACC->CRC_OP_EN_REG = 0;
    }

#if CRC_W_CFG_DMAC_ENABLE
    p_cfg_extend->p_transfer->p_api->close(p_cfg_extend->p_transfer->p_ctrl);
#endif

    /* Mark driver as closed */
    p_instance_ctrl->open = 0U;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Perform a CRC calculation on a block of data.
 *
 * Implements @ref crc_api_t::calculate
 *
 * If not using DMA, returns the calculated CRC value.
 * If using DMA, use @ref R_CRC_W_CalculatedValueGet to get the calculated value.
 *
 * @retval FSP_SUCCESS              Calculation successful.
 * @retval FSP_ERR_ASSERTION        Either p_ctrl, inputBuffer, or p_calculatedValue is NULL.
 *                                  Status of CRC calculation is ERROR.
 * @retval FSP_ERR_INVALID_ARGUMENT length value is NULL.
 * @retval FSP_ERR_NOT_OPEN         The driver is not opened.
 * @retval FSP_ERR_TIMEOUT          CRC is not stopped. CRC is busy.
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_Calculate (crc_ctrl_t * const p_ctrl, crc_input_t * const p_crc_input, uint32_t * p_calculatedValue)
{
    crc_w_instance_ctrl_t * p_instance_ctrl = (crc_w_instance_ctrl_t *) p_ctrl;
    crc_w_extended_cfg_t  * p_cfg_extend;

#if CRC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_crc_input);
 #if (CRC_W_CFG_DMAC_ENABLE == 0)
    FSP_ASSERT(p_calculatedValue);
 #endif
    FSP_ERROR_RETURN(CRC_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ERROR_RETURN((0UL != p_crc_input->num_bytes), FSP_ERR_INVALID_ARGUMENT);
#endif

    uint32_t timeout = CRC_W_PRV_TIMEOUT_US;
    uint32_t address;

    p_cfg_extend = (crc_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    address = r_crc_w_sw_remapper((uint32_t) p_crc_input->p_input_buffer);
    FSP_ERROR_RETURN(CRC_W_MEMORY_NONE != address, FSP_ERR_ASSERTION);
    p_crc_input->p_input_buffer = (void *) address;

    if (CRC_W_CHANNEL_1 == p_cfg_extend->channel)
    {
        HW_ACC->CRC_1_ADDR_MIN_REG = (uint32_t) p_crc_input->p_input_buffer;
        HW_ACC->CRC_1_ADDR_MAX_REG = (uint32_t) p_crc_input->p_input_buffer + p_crc_input->num_bytes;

        HW_ACC->CRC_1_SEED_VAL_REG = p_crc_input->crc_seed;

        HW_ACC->CRC_1_PSEUDO_VAL_REG = 0;
        HW_ACC->CRC_1_REQ_CTRL_REG   = CRC_W_REQ_CLEAR;

        while (CRC_W_CAL_STATUS_BUSY == HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        HW_ACC->CRC_1_REQ_CTRL_REG = CRC_W_REQ_CLEAR | CRC_W_REQ_START;
    }
    else
    {
        HW_ACC->CRC_ADDR_MIN_REG = (uint32_t) p_crc_input->p_input_buffer;
        HW_ACC->CRC_ADDR_MAX_REG = (uint32_t) p_crc_input->p_input_buffer + p_crc_input->num_bytes;

        HW_ACC->CRC_SEED_VAL_REG = p_crc_input->crc_seed;

        HW_ACC->CRC_PSEUDO_VAL_REG = 0;
        HW_ACC->CRC_REQ_CTRL_REG   = CRC_W_REQ_CLEAR;

        while (CRC_W_CAL_STATUS_BUSY == HW_ACC->CRC_STA_REG_b.CRC_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        HW_ACC->CRC_REQ_CTRL_REG = CRC_W_REQ_CLEAR | CRC_W_REQ_START;
    }

    if (CRC_W_PATH_SELECT_CPU == p_cfg_extend->path_selection)
    {
        if (CRC_W_CHANNEL_1 == p_cfg_extend->channel)
        {
            switch (p_cfg_extend->parallel_type)
            {
                case CRC_W_PARALLEL_TYPE_32:
                {
                    r_crc_w_calc_1_pseudo32(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }

                case CRC_W_PARALLEL_TYPE_16:
                {
                    r_crc_w_calc_1_pseudo16(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }

                default:               /* CRC_W_PARALLEL_TYPE_8 */
                {
                    r_crc_w_calc_1_pseudo8(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }
            }

            FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA, FSP_ERR_ASSERTION);

            HW_ACC->CRC_1_REQ_CTRL_REG = CRC_W_REQ_STOP;
            while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA)
            {
                R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
                timeout--;
                FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
            }

            *p_calculatedValue = HW_ACC->CRC_1_CAL_VAL_REG;
        }
        else
        {
            switch (p_cfg_extend->parallel_type)
            {
                case CRC_W_PARALLEL_TYPE_32:
                {
                    r_crc_w_calc_0_pseudo32(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }

                case CRC_W_PARALLEL_TYPE_16:
                {
                    r_crc_w_calc_0_pseudo16(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }

                default:               /* CRC_W_PARALLEL_TYPE_8 */
                {
                    r_crc_w_calc_0_pseudo8(p_crc_input->p_input_buffer, p_crc_input->num_bytes);
                    break;
                }
            }

            FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA, FSP_ERR_ASSERTION);

            HW_ACC->CRC_REQ_CTRL_REG = CRC_W_REQ_STOP;
            while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA)
            {
                R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
                timeout--;
                FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
            }

            *p_calculatedValue = HW_ACC->CRC_CAL_VAL_REG;
        }
    }

#if CRC_W_CFG_DMAC_ENABLE
    else
    {
        /* Enable DMAC to operate */
        address = r_crc_w_sw_remapper((uint32_t) p_cfg_extend->p_transfer->p_cfg->p_info->p_src);
        FSP_ERROR_RETURN(CRC_W_MEMORY_NONE != address, FSP_ERR_ASSERTION);
        p_cfg_extend->p_transfer->p_cfg->p_info->p_src = (void *) address;

        address = r_crc_w_sw_remapper((uint32_t) p_cfg_extend->p_transfer->p_cfg->p_info->p_dest);
        FSP_ERROR_RETURN(CRC_W_MEMORY_NONE != address, FSP_ERR_ASSERTION);
        p_cfg_extend->p_transfer->p_cfg->p_info->p_dest = (void *) address;

        p_cfg_extend->p_transfer->p_api->reconfigure(p_cfg_extend->p_transfer->p_ctrl,
                                                     p_cfg_extend->p_transfer->p_cfg->p_info);
        p_cfg_extend->p_transfer->p_api->enable(p_cfg_extend->p_transfer->p_ctrl);
    }
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Return the calculated value.
 *
 * Implements @ref crc_api_t::crcResultGet
 *
 * CRC calculation operates on a running value. This function returns the current calculated value.
 *
 * @retval FSP_SUCCESS             Return of calculated value successful.
 * @retval FSP_ERR_ASSERTION       Either p_ctrl or p_calculatedValue is NULL.
 * @retval FSP_ERR_NOT_OPEN        The driver is not opened.
 * @retval FSP_ERR_TIMEOUT         CRC calculation is not stopped.
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_CalculatedValueGet (crc_ctrl_t * const p_ctrl, uint32_t * p_calculatedValue)
{
    crc_w_instance_ctrl_t * p_instance_ctrl = (crc_w_instance_ctrl_t *) p_ctrl;
    crc_w_extended_cfg_t  * p_cfg_extend;

#if CRC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_calculatedValue);
    FSP_ERROR_RETURN(CRC_W_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    uint32_t timeout = CRC_W_PRV_TIMEOUT_US;

    p_cfg_extend = (crc_w_extended_cfg_t *) p_instance_ctrl->p_cfg->p_extend;

    if (CRC_W_CHANNEL_1 == p_cfg_extend->channel)
    {
        FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA, FSP_ERR_ASSERTION);

        HW_ACC->CRC_1_REQ_CTRL_REG = CRC_W_REQ_STOP;
        while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_1_STA_REG_b.CRC_1_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        *p_calculatedValue = HW_ACC->CRC_1_CAL_VAL_REG;
    }
    else
    {
        FSP_ERROR_RETURN(CRC_W_CAL_STATUS_ERROR != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA, FSP_ERR_ASSERTION);

        HW_ACC->CRC_REQ_CTRL_REG = CRC_W_REQ_STOP;
        while (CRC_W_CAL_STATUS_STOP != HW_ACC->CRC_STA_REG_b.CRC_CAL_STA)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            timeout--;
            FSP_ERROR_RETURN(0U < timeout, FSP_ERR_TIMEOUT);
        }

        *p_calculatedValue = HW_ACC->CRC_CAL_VAL_REG;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Configure the snoop channel and set the CRC seed.
 *
 *  Implements @ref crc_api_t::snoopEnable
 *
 * The CRC calculator can operate on reads and writes over any of the first ten SCI channels.
 * For example, if set to channel 0, transmit, every byte written out SCI channel 0 is also
 * sent to the CRC calculator as if the value was explicitly written directly to the CRC calculator.
 *
 * @retval FSP_ERR_UNSUPPORTED          SNOOP operation is not supported.
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_SnoopEnable (crc_ctrl_t * const p_ctrl, uint32_t crc_seed)
{
    FSP_PARAMETER_NOT_USED(crc_seed);
    FSP_PARAMETER_NOT_USED(p_ctrl);

    return FSP_ERR_UNSUPPORTED;
}

/*******************************************************************************************************************//**
 * Disable snooping.
 *
 * Implements @ref crc_api_t::snoopDisable
 *
 * @retval FSP_ERR_UNSUPPORTED     SNOOP operation is not supported.
 *
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_SnoopDisable (crc_ctrl_t * const p_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

    return FSP_ERR_UNSUPPORTED;
}

/** @} (end addtogroup CRC_W) */

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * CRC calculation with software. 8 bit data width. CRC channel 0.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_0_pseudo8 (const uint8_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < length; cnt++)
    {
        HW_ACC->CRC_PSEUDO_VAL_REG = p_value[cnt];
    }
}

/*******************************************************************************************************************//**
 * CRC calculation with software. 16 bit data width. CRC channel 0.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_0_pseudo16 (const uint16_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < (length >> 1); cnt++)
    {
        HW_ACC->CRC_PSEUDO_VAL_REG = p_value[cnt];
    }
}

/*******************************************************************************************************************//**
 * CRC calculation with software. 32 bit data width. CRC channel 0.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_0_pseudo32 (const uint32_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < (length >> 2); cnt++)
    {
        HW_ACC->CRC_PSEUDO_VAL_REG = p_value[cnt];
    }
}

/*******************************************************************************************************************//**
 * CRC calculation with software. 8 bit data width. CRC channel 1.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_1_pseudo8 (const uint8_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < length; cnt++)
    {
        HW_ACC->CRC_1_PSEUDO_VAL_REG = p_value[cnt];
    }
}

/*******************************************************************************************************************//**
 * CRC calculation with software. 16 bit data width. CRC channel 1.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_1_pseudo16 (const uint16_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < (length >> 1); cnt++)
    {
        HW_ACC->CRC_1_PSEUDO_VAL_REG = p_value[cnt];
    }
}

/*******************************************************************************************************************//**
 * CRC calculation with software. 32 bit data width. CRC channel 1.
 *
 * @param[in]  p_value          Pointer of data buffer.
 * @param[in]  length           Data length.
 **********************************************************************************************************************/
static void r_crc_w_calc_1_pseudo32 (const uint32_t * p_value, uint32_t length)
{
    uint32_t cnt;

    for (cnt = 0; cnt < (length >> 2); cnt++)
    {
        HW_ACC->CRC_1_PSEUDO_VAL_REG = p_value[cnt];
    }
}

#if CRC_W_CFG_DMAC_ENABLE

/*******************************************************************************************************************//**
 * Open DMAC.
 *
 * @param[in]  p_cfg            Pointer to specific configuration structure.
 *
 * @retval  FSP_SUCCESS         Transfer interface is configured with valid parameters.
 * @retval  FSP_ERR_ASSERTION   DMAC open error.
 **********************************************************************************************************************/
static fsp_err_t r_crc_w_transfer_open (crc_cfg_t const * const p_cfg)
{
    fsp_err_t              err;
    crc_w_extended_cfg_t * p_cfg_extend;

    p_cfg_extend = (crc_w_extended_cfg_t *) p_cfg->p_extend;

    err = p_cfg_extend->p_transfer->p_api->open(p_cfg_extend->p_transfer->p_ctrl, p_cfg_extend->p_transfer->p_cfg);
    FSP_ASSERT(FSP_SUCCESS == err);

    return err;
}

#endif

/*******************************************************************************************************************//**
 * Convert to physical address of remapped memory.
 *
 * @param[in]  vaddr        Data buffer address.
 * @retval                  Physical address of the remapped memory.
 **********************************************************************************************************************/
static uint32_t r_crc_w_sw_remapper (uint32_t vaddr)
{
    uint32_t                phy_addr;
    uint32_t                flash_region_base_offset;
    crc_w_remap_address_0_t remap_addr0;

    static const uint32_t remap[] =
    {
        CRC_W_MEMORY_ROM_BASE,
        CRC_W_MEMORY_NONE,
        CRC_W_MEMORY_OQSPIC_S_BASE,
        CRC_W_MEMORY_SYSRAM_BASE,
        CRC_W_MEMORY_NONE,
        CRC_W_MEMORY_NONE,
        CRC_W_MEMORY_NONE,
        CRC_W_MEMORY_NONE,
    };

    FSP_CRITICAL_SECTION_DEFINE;
    FSP_CRITICAL_SECTION_ENTER;
    remap_addr0 = CRG_TOP->SYS_CTRL_REG_b.REMAP_ADR0;
    FSP_CRITICAL_SECTION_EXIT;

    if (CRC_W_MEMORY_NONE == remap[remap_addr0])
    {
        return CRC_W_MEMORY_NONE;
    }

    if (remap_addr0 != CRC_W_REMAP_ADDRESS_0_TO_OQSPI_FLASH)
    {
        if (vaddr >= CRC_W_MEMORY_REMAPPED_END)
        {
            phy_addr = vaddr;
        }
        else
        {
            phy_addr = vaddr + remap[remap_addr0];
        }
    }
    else
    {
        flash_region_base_offset  = CRC_W_MEMORY_OQSPIC_S_BASE;
        flash_region_base_offset += CACHE->CACHE_FLASH_REG_b.FLASH_REGION_OFFSET << 2;

        if (vaddr < CRC_W_MEMORY_REMAPPED_END)
        {
            phy_addr = flash_region_base_offset + vaddr;
        }
        else
        {
            phy_addr = vaddr;
        }
    }

    return phy_addr;
}

/* End of file R_CRC_W. */
