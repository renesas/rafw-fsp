/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_dmic_api.h"
#include "r_dmic_w.h"
#if DMIC_CFG_DTC_ENABLE
 #include "r_dtc.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* "DMIC" in ASCII, used to determine if driver is open. */
#define DMIC_PRV_OPEN                       (0x444D4943U)

/* FIFO depth */
#define BSP_FEATURE_DMIC_FIFO_NUM_STAGES    (4)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

#if defined(__ARMCC_VERSION) || defined(__ICCARM__)
typedef void (BSP_CMSE_NONSECURE_CALL * dmic_prv_ns_callback)(dmic_callback_args_t * p_args);
#elif defined(__GNUC__)
typedef BSP_CMSE_NONSECURE_CALL void (*volatile dmic_prv_ns_callback)(dmic_callback_args_t * p_args);
#endif

/* DMIC communication direction */
typedef enum e_dmic_dir
{
    DMIC_DIR_RX    = 1U,               ///< Receive direction only
    DMIC_DIR_TX    = 2U,               ///< Transmit direction only
    DMIC_DIR_TX_RX = 3U,               ///< Transmit and receive direction
} dmic_dir_t;

/* Arguments are stored in a structure for transfer reset subroutine to avoid having more than 4 arguments. */
typedef struct e_dmic_prv_transfer_reset
{
    void const * p_data;
    uint32_t     bytes;
    void const * p_src;
    void       * p_dest;
} dmic_prv_transfer_reset_t;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/* ISRs */
void SRC_Out_Handler(void);
void SRC_In_Handler(void);

static void r_dmic_w_call_callback(dmic_instance_ctrl_t * p_ctrl, dmic_event_t event);

/* ISR subroutines */

// static void r_dmic_w_tx_fifo_write(dmic_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_write);
static void r_dmic_w_rx_fifo_read(dmic_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_read);

/* FIFO subroutines */

// static void r_dmic_w_fifo_write(dmic_instance_ctrl_t * p_instance_ctrl);
static void r_dmic_w_fifo_read(dmic_instance_ctrl_t * p_instance_ctrl);

/* Open subroutines */
static void r_dmic_w_interrupts_configure(dmic_instance_ctrl_t * const p_instance_ctrl, dmic_cfg_t const * const p_cfg);
void        dmic_clk_init(uint32_t frequency);

#if DMIC_CFG_DTC_ENABLE
static fsp_err_t r_dmic_w_dependent_drivers_configure(R_DMIC0_Type           * p_reg,
                                                      dmic_cfg_t const * const p_cfg,
                                                      transfer_size_t          fifo_access_size);

#endif

/* Stop subroutines */
static void r_dmic_w_stop_sub(dmic_instance_ctrl_t * const p_instance_ctrl);

/* Start subroutines */
static fsp_err_t r_dmic_w_start(dmic_instance_ctrl_t * const p_instance_ctrl, dmic_dir_t dir);

/* Read and write subroutines. */
fsp_err_t r_dmic_w_tx_load_fifo(dmic_instance_ctrl_t * const p_instance_ctrl,
                                void const * const           p_src,
                                uint32_t const               bytes);
fsp_err_t r_dmic_w_rx_unload_fifo(dmic_instance_ctrl_t * const p_instance_ctrl,
                                  void * const                 p_dest,
                                  uint32_t const               bytes);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/** DMIC Implementation of DMIC interface.  */
const dmic_api_t g_dmic_on_dmic =
{
    .open        = R_DMIC_W_Open,
    .stop        = R_DMIC_W_Stop,
    .write       = R_DMIC_W_Write,
    .read        = R_DMIC_W_Read,
    .writeRead   = R_DMIC_W_WriteRead,
    .mute        = R_DMIC_W_Mute,
    .statusGet   = R_DMIC_W_StatusGet,
    .close       = R_DMIC_W_Close,
    .callbackSet = R_DMIC_W_CallbackSet,
};

/*******************************************************************************************************************//**
 * @addtogroup DMIC
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Opens the DMIC. Implements  dmic_api_t::open.
 *
 * This function sets this clock divisor and the configurations specified in dmic_cfg_t.  It also opens the timer and
 * transfer instances if they are provided.
 *
 * @retval FSP_SUCCESS                     Ready for DMIC communication.
 * @retval FSP_ERR_ASSERTION               The pointer to p_ctrl or p_cfg is null.
 * @retval FSP_ERR_ALREADY_OPEN            The control block has already been opened.
 * @retval FSP_ERR_IP_CHANNEL_NOT_PRESENT  Channel number is not available on this MCU.
 * @return                                 See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                         possible return codes. This function calls:
 *                                             *  transfer_api_t::open
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Open (dmic_ctrl_t * const p_ctrl, dmic_cfg_t const * const p_cfg)
{
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;

#if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_callback);
    FSP_ASSERT(p_cfg->int_irq >= 0);
    if (DMIC_MODE_MASTER == p_cfg->operating_mode)
    {
        FSP_ASSERT(NULL != p_cfg->p_extend);
    }

 #if DMIC_CFG_DTC_ENABLE
    if (NULL != p_cfg->p_transfer_rx)
    {
        dtc_extended_cfg_t * p_dtc_rx_cfg = (dtc_extended_cfg_t *) p_cfg->p_transfer_rx->p_cfg->p_extend;
        FSP_ASSERT(NULL != p_dtc_rx_cfg);
        FSP_ASSERT(p_cfg->rxi_irq == p_dtc_rx_cfg->activation_source);
    }

    if (NULL != p_cfg->p_transfer_tx)
    {
        dtc_extended_cfg_t * p_dtc_tx_cfg = (dtc_extended_cfg_t *) p_cfg->p_transfer_tx->p_cfg->p_extend;
        FSP_ASSERT(NULL != p_dtc_tx_cfg);
        FSP_ASSERT(p_cfg->txi_irq == p_dtc_tx_cfg->activation_source);
    }
 #endif

    FSP_ERROR_RETURN(DMIC_PRV_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ERROR_RETURN(0U != ((1U << p_cfg->channel) & BSP_FEATURE_DMIC_VALID_CHANNEL_MASK),
                     FSP_ERR_IP_CHANNEL_NOT_PRESENT);
#endif
    dmic_clk_init(p_cfg->clk_frequency);

    if (p_cfg->direction == DMIC_DIRECTION_INPUT)
    {
        APU_AUD->APU_DMIC_CTRL_REG_b.DMIC1_IN_DELAY = p_cfg->delay;
        SRC_IF->APU_SRC_CTRL_REG_b.SRC_PDM_IN_INV   = !p_cfg->swap_channel;
    }
    else
    {
    }

    APU_AUD->APU_DMIC_CTRL_REG_b.DMIC_MASTER_MODE = p_cfg->mode;

#if DMIC_CFG_DTC_ENABLE

    /* Configure dependent timer and transfer drivers. */
    fsp_err_t err = r_dmic_w_dependent_drivers_configure(p_reg, p_cfg, fifo_access_size);
    FSP_ERROR_RETURN((FSP_SUCCESS == err), err);
#endif
    uint32_t base_address = (uint32_t) APU_AUD;

    APU_AUD_Type * p_reg = (APU_AUD_Type *) base_address;

    /* Determine how to access the FIFO (1 byte, 2 byte, or 4 byte access). */
    transfer_size_t fifo_access_size = TRANSFER_SIZE_4_BYTE;

    /* Initialize the control structure. */
    p_instance_ctrl->p_reg            = p_reg;
    p_instance_ctrl->p_cfg            = p_cfg;
    p_instance_ctrl->fifo_access_size = fifo_access_size;

    /* Configure interrupts. */
    r_dmic_w_interrupts_configure(p_instance_ctrl, p_cfg);

    /* Calculate register settings. */

    /* Configure operating mode. */
    p_instance_ctrl->p_reg->APU_DMIC_CTRL_REG_b.DMIC_MASTER_MODE = p_cfg->mode;

    /* Initialize the DMICE following the procedure in Figure 41.53 "Procedure to start communication (CPU operation
     * procedure)" of the RA6M3 manual R01UH0886EJ0100. This function follows this procedure except for enabling
     * interrupts and enabling communication, which are done before communication begins. */

    /* Enable PCLK to DMICE. */

    // R_BSP_MODULE_START(FSP_IP_DMIC, p_instance_ctrl->p_cfg->channel);

    /* Initialization complete. */
    p_instance_ctrl->open = DMIC_PRV_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Writes data buffer to DMIC. Implements  dmic_api_t::write.
 *
 * This function resets the transfer if the transfer interface is used, or writes the length of data that fits in the
 * FIFO then stores the remaining write buffer in the control block to be written in the ISR.
 *
 * Write() cannot be called if another write(), read() or writeRead() operation is in progress.  Write can be called
 * when the DMIC is idle, or after the DMIC_EVENT_TX_EMPTY event.
 *
 * @retval FSP_SUCCESS                 Write initiated successfully.
 * @retval FSP_ERR_ASSERTION           The pointer to p_ctrl or p_src was null, or bytes requested was 0.
 * @retval FSP_ERR_IN_USE              Another transfer is in progress, data was not written.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_UNDERFLOW           A transmit underflow error is pending. Wait for the DMIC to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Write (dmic_ctrl_t * const p_ctrl, void const * const p_src, uint32_t const bytes)
{
#if 1
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_src);
    FSP_PARAMETER_NOT_USED(bytes);
#else
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;
#endif

    return FSP_ERR_UNSUPPORTED;
}

/*******************************************************************************************************************//**
 * Reads data into provided buffer. Implements  dmic_api_t::read.
 *
 * This function resets the transfer if the transfer interface is used, or reads the length of data available in the
 * FIFO then stores the remaining read buffer in the control block to be filled in the ISR.
 *
 * Read() cannot be called if another write(), read() or writeRead() operation is in progress.  Read can be called
 * when the DMIC is idle, or after the DMIC_EVENT_RX_FULL event.
 *
 * @retval FSP_SUCCESS                 Read initiated successfully.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 * @retval FSP_ERR_ASSERTION           The pointer to p_ctrl or p_dest was null, or bytes requested was 0.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_OVERFLOW            A receive overflow error is pending. Wait for the DMIC to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Read (dmic_ctrl_t * const p_ctrl, void * const p_dest, uint32_t const bytes)
{
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;

#if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ASSERT(NULL != p_dest);

    /* bytes must be a non-zero */
    FSP_ASSERT(bytes > 0U);
#endif

    /* If a transfer instance is provided for read, reset the transfer. Otherwise store data to receive in the receive
     * interrupt. */
    fsp_err_t err = r_dmic_w_rx_unload_fifo(p_instance_ctrl, p_dest, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a receive overflow is pending, return an error.  This could happen if the application is in an interrupt
     * context with a priority greater than or equal to the idle/error interrupt priority and does not read from the
     * receive FIFO before it overflows. */

// FSP_ERROR_RETURN(0U == p_instance_ctrl->p_reg->DMICSR_b.ROIRQ, FSP_ERR_OVERFLOW);

    /* Make sure reception is enabled. */
    err = r_dmic_w_start(p_instance_ctrl, DMIC_DIR_RX);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Writes from source buffer and reads data into destination buffer. Implements  dmic_api_t::writeRead.
 *
 * writeRead() cannot be called if another write(), read() or writeRead() operation is in progress.  writeRead() can be
 * called when the DMIC is idle, or after the DMIC_EVENT_RX_FULL event.
 *
 * @retval FSP_SUCCESS                 Write and read initiated successfully.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 * @retval FSP_ERR_ASSERTION           An input parameter was invalid.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_UNDERFLOW           A transmit underflow error is pending. Wait for the DMIC to go idle before
 *                                     resuming communication.
 * @retval FSP_ERR_OVERFLOW            A receive overflow error is pending. Wait for the DMIC to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_WriteRead (dmic_ctrl_t * const p_ctrl,
                              void const * const  p_src,
                              void * const        p_dest,
                              uint32_t const      bytes)
{
#if 1
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_src);
    FSP_PARAMETER_NOT_USED(p_dest);
    FSP_PARAMETER_NOT_USED(bytes);
#else
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;

 #if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_src);
    FSP_ASSERT(NULL != p_dest);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* bytes must be a non-zero */
    FSP_ASSERT(bytes > 0U);
 #endif

    /* If a transfer instance is provided for write, reset the transfer. Reset the transmit FIFO first since the
     * transmit FIFO will underflow before the receive FIFO overflows during full duplex communication. */
    fsp_err_t err = r_dmic_w_tx_load_fifo(p_instance_ctrl, p_src, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a transfer instance is provided for read, reset the transfer. */
    err = r_dmic_w_rx_unload_fifo(p_instance_ctrl, p_dest, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a transmit underflow or receive overflow is pending, return an error.  This could happen if the application
     * is in an interrupt context with a priority greater than or equal to the idle/error interrupt priority and does
     * not reload the FIFOs before they underflow/overflow. */
    uint32_t dmicsr = p_instance_ctrl->p_reg->DMICSR;
    FSP_ERROR_RETURN(0U == (dmicsr & (1U << DMIC_PRV_DMICSR_TUIRQ_BIT)), FSP_ERR_UNDERFLOW);
    FSP_ERROR_RETURN(0U == (dmicsr & (1U << DMIC_PRV_DMICSR_ROIRQ_BIT)), FSP_ERR_OVERFLOW);

    /* Make sure transmission and reception are enabled. */
    err = r_dmic_w_start(p_instance_ctrl, DMIC_DIR_TX_RX);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
#endif

    return FSP_ERR_UNSUPPORTED;
}

/*******************************************************************************************************************//**
 * Stops DMIC. Implements  dmic_api_t::stop.
 *
 * This function disables both transmission and reception, and disables any transfer instances used.
 *
 * The DMIC will stop on the next frame boundary.  Do not restart DMIC until it is idle.
 *
 * @retval FSP_SUCCESS           DMIC communication stop request issued.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 * @return                       See @ref RENESAS_ERROR_CODES or lower level drivers for other possible return codes.
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Stop (dmic_ctrl_t * const p_ctrl)
{
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;

#if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Stop is complete after an DMIC_EVENT_IDLE interrupt. */
    r_dmic_w_stop_sub(p_instance_ctrl);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Mutes DMIC on the next frame boundary. Implements  dmic_api_t::mute.
 *
 * Data is still written while mute is enabled, but the transmit line outputs zeros.
 *
 * @retval FSP_SUCCESS           Transmission is muted.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Mute (dmic_ctrl_t * const p_ctrl, dmic_mute_t const mute_enable)
{
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;
    FSP_PARAMETER_NOT_USED(mute_enable);

#if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    // jsaon. need to add mute function.
    p_instance_ctrl->p_reg->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN = 0;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Gets DMIC status and stores it in provided pointer p_status. Implements  dmic_api_t::statusGet.
 *
 * @retval FSP_SUCCESS           Information stored successfully.
 * @retval FSP_ERR_ASSERTION     The p_instance_ctrl or p_status parameter was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_StatusGet (dmic_ctrl_t * const p_ctrl, dmic_status_t * const p_status)
{
#if DMIC_W_CFG_PARAM_CHECKING_ENABLE

    /* Make sure parameters are valid. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_status);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif
    FSP_PARAMETER_NOT_USED(p_ctrl);

    p_status->state = 0;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Closes DMIC. Implements  dmic_api_t::close.
 *
 * This function powers down the DMIC and closes the lower level timer and transfer drivers if they are used.
 *
 * @retval FSP_SUCCESS           Device closed successfully.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_Close (dmic_ctrl_t * const p_ctrl)
{
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) p_ctrl;
#if DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_instance_ctrl->open = 0U;

    /* Stop DMICE. */
    p_instance_ctrl->p_reg->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN = 0;

    // need to add more disable sequence -- jason

    /* Disable interrupts. */
    R_BSP_IrqDisable(p_instance_ctrl->p_cfg->int_irq);
    if (p_instance_ctrl->p_cfg->rxi_irq >= 0)
    {
        R_BSP_IrqDisable(p_instance_ctrl->p_cfg->rxi_irq);
    }

#if DMIC_CFG_DTC_ENABLE

    /* If transfer is used, disable transfer when stop is requested. */
    if (NULL != p_instance_ctrl->p_cfg->p_transfer_rx)
    {
        (void) p_instance_ctrl->p_cfg->p_transfer_rx->p_api->close(p_instance_ctrl->p_cfg->p_transfer_rx->p_ctrl);
    }

    if (NULL != p_instance_ctrl->p_cfg->p_transfer_tx)
    {
        (void) p_instance_ctrl->p_cfg->p_transfer_tx->p_api->close(p_instance_ctrl->p_cfg->p_transfer_tx->p_ctrl);
    }
#endif

    /* Stop feeding clock to DMIC peripheral to deactivate it. */

    // R_BSP_MODULE_STOP(FSP_IP_DMIC, p_instance_ctrl->p_cfg->channel);
    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Updates the user callback and has option of providing memory for callback structure.
 * Implements dmic_api_t::callbackSet
 *
 * @retval  FSP_SUCCESS                  Callback updated successfully.
 * @retval  FSP_ERR_ASSERTION            A required pointer is NULL.
 * @retval  FSP_ERR_NOT_OPEN             The control block has not been opened.
 * @retval  FSP_ERR_NO_CALLBACK_MEMORY   p_callback is non-secure and p_callback_memory is either secure or NULL.
 **********************************************************************************************************************/
fsp_err_t R_DMIC_W_CallbackSet (dmic_ctrl_t * const          p_api_ctrl,
                                void (                     * p_callback)(dmic_callback_args_t *),
                                void const * const           p_context,
                                dmic_callback_args_t * const p_callback_memory)
{
    dmic_instance_ctrl_t * p_ctrl = (dmic_instance_ctrl_t *) p_api_ctrl;

#if (DMIC_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_callback);
    FSP_ERROR_RETURN(DMIC_PRV_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Store callback and context */
    p_ctrl->p_callback        = p_callback;
    p_ctrl->p_context         = p_context;
    p_ctrl->p_callback_memory = p_callback_memory;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup R_DMIC_W)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * For each interrupt, if it is enabled, sets the interrupt priority based on user configuration and stores the control
 * block so it can be accessed in the ISR.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] p_cfg                    Pointer to the configuration structure.
 **********************************************************************************************************************/
static void r_dmic_w_interrupts_configure (dmic_instance_ctrl_t * const p_instance_ctrl, dmic_cfg_t const * const p_cfg)
{
    /* Set interrupt priority based on user configuration of interrupt number for FSP_SIGNAL_DMIC_RXI
     * or FSP_SIGNAL_DMIC_TXI_RXI signals */
    if (p_cfg->rxi_irq >= 0)
    {
        R_BSP_IrqCfgEnable(p_cfg->rxi_irq, p_cfg->rxi_ipl, p_instance_ctrl);
    }
}

#if DMIC_CFG_DTC_ENABLE

/*******************************************************************************************************************//**
 * Configures any dependent drivers selected by the user, including transfer and timer drivers.
 *
 * @param[in] p_reg                    Pointer to DMICE base register address for this channel.
 * @param[in] p_cfg                    Pointer to the configuration structure.
 *
 * @retval FSP_SUCCESS                 Dependent drivers configured successfully.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::open
 **********************************************************************************************************************/
static fsp_err_t r_dmic_w_dependent_drivers_configure (R_DMIC0_Type           * p_reg,
                                                       dmic_cfg_t const * const p_cfg,
                                                       transfer_size_t          fifo_access_size)
{
 #if 0

    /* Prepare transfer configuration. */
    fsp_err_t err_transfer_tx = FSP_SUCCESS;
    fsp_err_t err_transfer_rx = FSP_SUCCESS;

    /* Use block mode to fill or empty half the FIFO at a time. Reference Figure 41.53 "Procedure to start
     * communication (CPU operation procedure)" in the RA6M3 manual R01UH0886EJ0100. */
    uint32_t transfer_settings = (uint32_t) TRANSFER_MODE_BLOCK << TRANSFER_SETTINGS_MODE_BITS;
    transfer_settings |= (uint32_t) fifo_access_size << TRANSFER_SETTINGS_SIZE_BITS;

    /* If a transfer instance is provided for write, open the transfer instance. */
    if (NULL != p_cfg->p_transfer_tx)
    {
        p_cfg->p_transfer_tx->p_cfg->p_info->p_dest = (void *) &(p_reg->DMICFTDR);
        uint32_t transfer_settings_tx = transfer_settings |
                                        (TRANSFER_ADDR_MODE_INCREMENTED << TRANSFER_SETTINGS_DMIC_ADDR_BITS);
        p_cfg->p_transfer_tx->p_cfg->p_info->transfer_settings_word = transfer_settings_tx;
        p_cfg->p_transfer_tx->p_cfg->p_info->length                 = DMIC_PRV_TRANSFER_BLOCK_SIZE;
        err_transfer_tx = p_cfg->p_transfer_tx->p_api->open(p_cfg->p_transfer_tx->p_ctrl, p_cfg->p_transfer_tx->p_cfg);
    }

  #if  DMIC_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ERROR_RETURN((FSP_SUCCESS == err_transfer_tx), err_transfer_tx);
  #else
    FSP_PARAMETER_NOT_USED(err_transfer_tx);
  #endif

    /* If a transfer instance is provided for read, open the transfer instance. */
    if (NULL != p_cfg->p_transfer_rx)
    {
        p_cfg->p_transfer_rx->p_cfg->p_info->p_src = (void *) &(p_reg->DMICFRDR);
        uint32_t transfer_settings_rx = transfer_settings |
                                        (TRANSFER_ADDR_MODE_INCREMENTED << TRANSFER_SETTINGS_DEST_ADDR_BITS) |
                                        (TRANSFER_REPEAT_AREA_SOURCE << TRANSFER_SETTINGS_REPEAT_AREA_BITS);
        p_cfg->p_transfer_rx->p_cfg->p_info->transfer_settings_word = transfer_settings_rx;
        p_cfg->p_transfer_rx->p_cfg->p_info->length                 = DMIC_PRV_TRANSFER_BLOCK_SIZE;
        err_transfer_rx = p_cfg->p_transfer_rx->p_api->open(p_cfg->p_transfer_rx->p_ctrl, p_cfg->p_transfer_rx->p_cfg);
    }

  #if  DMIC_W_CFG_PARAM_CHECKING_ENABLE

    /* If there was an error opening the receive transfer, close the transmit transfer before returning. */
    if (FSP_SUCCESS != err_transfer_rx)
    {
        if (NULL != p_cfg->p_transfer_tx)
        {
            p_cfg->p_transfer_tx->p_api->close(p_cfg->p_transfer_tx->p_ctrl);
        }
    }

    FSP_ERROR_RETURN((FSP_SUCCESS == err_transfer_rx), err_transfer_rx);
  #else
    FSP_PARAMETER_NOT_USED(err_transfer_rx);
  #endif
 #endif

    return FSP_SUCCESS;
}

#endif

/*******************************************************************************************************************//**
 * Disables DMIC transmission and reception.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 **********************************************************************************************************************/
static void r_dmic_w_stop_sub (dmic_instance_ctrl_t * const p_instance_ctrl)
{
    /* Stop communication following the procedure from Figure 41.56 "Procedure to halt communication (CPU operation
     * procedure)" in the RA6M3 manual R01UH0886EJ0100. */

    p_instance_ctrl->p_reg->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN = 0;

#if DMIC_CFG_DTC_ENABLE

    /* If transfer is used, disable transfer when stop is requested. */
    if (NULL != p_instance_ctrl->p_cfg->p_transfer_rx)
    {
        (void) p_instance_ctrl->p_cfg->p_transfer_rx->p_api->disable(p_instance_ctrl->p_cfg->p_transfer_rx->p_ctrl);
    }

    if (NULL != p_instance_ctrl->p_cfg->p_transfer_tx)
    {
        (void) p_instance_ctrl->p_cfg->p_transfer_tx->p_api->disable(p_instance_ctrl->p_cfg->p_transfer_tx->p_ctrl);
    }
#endif

    /* Disable interrupt output. Clear RIE and TIE by clearing all bits except AUCKE, which is set or cleared depending
     * on the mode. All other bits can be set to 0. */

    // p_instance_ctrl->p_reg->DMICFCR = (uint32_t) p_instance_ctrl->p_cfg->operating_mode << DMIC_PRV_DMICFCR_AUCKE_BIT;

    /* Clear control structure data. */
    p_instance_ctrl->p_rx_dest       = NULL;
    p_instance_ctrl->rx_dest_samples = 0U;
}

/*******************************************************************************************************************//**
 * Configures the transmit FIFO to be loaded by DTC or stores the source data to be loaded into the FIFO in the transmit
 * interrupt.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] p_src                    Pointer to the source buffer.
 * @param[in] bytes                    Length of source buffer.
 *
 * @retval FSP_ERR_UNSUPPORTED         Not supported function
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t r_dmic_w_tx_load_fifo (dmic_instance_ctrl_t * const p_instance_ctrl,
                                 void const * const           p_src,
                                 uint32_t const               bytes)
{
    FSP_PARAMETER_NOT_USED(p_instance_ctrl);
    FSP_PARAMETER_NOT_USED(p_src);
    FSP_PARAMETER_NOT_USED(bytes);

    return FSP_ERR_UNSUPPORTED;
}

/*******************************************************************************************************************//**
 * Configures the receive FIFO to be unloaded by DTC or stores the destination buffer for data to be unloaded in the
 * receive interrupt.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] p_dest                   Pointer to the destination buffer.
 * @param[in] bytes                    Length of destination buffer.
 *
 * @retval FSP_SUCCESS                 Receive FIFO successfully unloaded.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t r_dmic_w_rx_unload_fifo (dmic_instance_ctrl_t * const p_instance_ctrl,
                                   void * const                 p_dest,
                                   uint32_t const               bytes)
{
    void   * p_data  = p_dest;
    uint32_t samples = bytes >> p_instance_ctrl->fifo_access_size;
#if DMIC_CFG_DTC_ENABLE

    /* By default, bytes are written in the ISR. */

    /* If a transfer instance is provided for reception, reset the transfer. */
    transfer_instance_t const * const p_transfer = p_instance_ctrl->p_cfg->p_transfer_rx;
    if (NULL != p_transfer)
    {
        /* Always read at least one sample from the receive interrupt. This ensures that the DTC transfer will be over
         * by the time a transmit underflow occurs during R_DMIC_W_WriteRead processing. This is important so the receive
         * buffer can be flushed in the transmit underflow error processing. Without this, the last frame (two samples)
         * could be lost during R_DMIC_W_WriteRead. */
        uint32_t transfer_blocks = (samples / DMIC_PRV_TRANSFER_BLOCK_SIZE) - 1U;
        if (transfer_blocks > 0)
        {
            fsp_err_t err = p_transfer->p_api->reset(p_transfer->p_ctrl, NULL, p_dest, (uint16_t) transfer_blocks);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
        }

        /* The last frame (2 samples) is read in the interrupt. */
        samples = 2U;
        p_data  = (void *) ((uint32_t) p_data + (bytes - (2U << p_instance_ctrl->fifo_access_size)));
    }
#endif

    p_instance_ctrl->p_rx_dest       = p_data;
    p_instance_ctrl->rx_dest_samples = samples;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Enables DMIC transmission and/or reception.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] dir                      Start transmit, receive, or both.
 *
 * @retval FSP_SUCCESS                 Ready for DMIC transmission.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 **********************************************************************************************************************/
static fsp_err_t r_dmic_w_start (dmic_instance_ctrl_t * const p_instance_ctrl, dmic_dir_t dir)
{
    uint32_t current_dmic_en = p_instance_ctrl->p_reg->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN;
    FSP_PARAMETER_NOT_USED(dir);

    /* If the peripheral is not already in the correct mode, attempt to start it. */
    if (!current_dmic_en)
    {
        /* If the peripheral is in the wrong mode or not idle, return an error. The DMIC must be idle before setting
         * REN or TEN. Reference 41.11.3.4 "Switching transfer modes" in the RA6M3 manual R01UH0886EJ0100. */
        FSP_ERROR_RETURN(0U == current_dmic_en, FSP_ERR_IN_USE);

        // FSP_ERROR_RETURN(1U == p_instance_ctrl->p_reg->DMICSR_b.IIRQ, FSP_ERR_IN_USE);

        /* Reset DMICE FIFOs. Set TFRST and RFRST, then clear them and wait for them to clear. This operation empties
         * the FIFOs. */

        // jason. need to add FIFO Clear

        /* If starting transmission, enable transmit interrupts. */
        /* Enabling communication clears related error flags in DMICSR. */
        p_instance_ctrl->p_reg->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN = 1;
    }

    return FSP_SUCCESS;
}

#if 0

/*******************************************************************************************************************//**
 *  Writes data to FIFO.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 *
 * @return The number of stages written
 **********************************************************************************************************************/
static void r_dmic_w_fifo_write (dmic_instance_ctrl_t * p_instance_ctrl)
{
}

#endif

/*******************************************************************************************************************//**
 *  Reads data from FIFO.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 **********************************************************************************************************************/
static void r_dmic_w_fifo_read (dmic_instance_ctrl_t * p_instance_ctrl)
{
    /* Calculate the number of available bytes of data in receive FIFO. */
    uint32_t fifo_filled_stages = BSP_FEATURE_DMIC_FIFO_NUM_STAGES;

    /* Calculate the number of FIFO stages requested to read. */
    uint32_t stages_to_read = 0U;
    stages_to_read = p_instance_ctrl->rx_dest_samples;
    if (stages_to_read > fifo_filled_stages)
    {
        stages_to_read = fifo_filled_stages;
    }

    r_dmic_w_rx_fifo_read(p_instance_ctrl, stages_to_read);
    uint32_t bytes_read = stages_to_read << p_instance_ctrl->fifo_access_size;
    p_instance_ctrl->rx_dest_samples -= stages_to_read;
    p_instance_ctrl->p_rx_dest        = (void *) ((uint32_t) p_instance_ctrl->p_rx_dest + bytes_read);

    /* Clear RDF only if something was written to the FIFO, otherwise the receive interrupt will fire repeatedly. */
    if (stages_to_read > 0)
    {
        // p_instance_ctrl->p_reg->DMICFSR;
        // p_instance_ctrl->p_reg->DMICFSR = DMIC_PRV_DMICFSR_RDF_CLEAR;
    }

    /* If reception is complete, clear receive buffer to NULL. */
    if (0U == p_instance_ctrl->rx_dest_samples)
    {
        p_instance_ctrl->p_rx_dest = NULL;
    }
}

#if 0

/*******************************************************************************************************************//**
 * Writes data to the transmit FIFO based on the FIFO access size.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] stages_to_write          Number of times to write to the FIFO.
 **********************************************************************************************************************/
static void r_dmic_w_tx_fifo_write (dmic_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_write)
{
}

#endif

/*******************************************************************************************************************//**
 * Reads data from the receive FIFO based on the FIFO access size.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] stages_to_read           Number of times to read from the FIFO.
 **********************************************************************************************************************/
static void r_dmic_w_rx_fifo_read (dmic_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_read)
{
    uint32_t * p_dest32 = (uint32_t *) p_instance_ctrl->p_rx_dest;
    for (uint32_t i = 0; i < stages_to_read / 2; i++)
    {
        *p_dest32 = (uint32_t) SRC_IF->APU_SRC_OUT1_REG;
        p_dest32++;
        *p_dest32 = (uint32_t) SRC_IF->APU_SRC_OUT2_REG;
        p_dest32++;
    }
}

/*******************************************************************************************************************//**
 * Calls user callback.
 *
 * @param[in]     p_ctrl     Pointer to DMIC instance control block
 * @param[in]     event      Event code
 **********************************************************************************************************************/
static void r_dmic_w_call_callback (dmic_instance_ctrl_t * p_ctrl, dmic_event_t event)
{
    dmic_callback_args_t args;

    /* Store callback arguments in memory provided by user if available.  This allows callback arguments to be
     * stored in non-secure memory so they can be accessed by a non-secure callback function. */
    dmic_callback_args_t * p_args = p_ctrl->p_callback_memory;
    if (NULL == p_args)
    {
        /* Store on stack */
        p_args = &args;
    }
    else
    {
        /* Save current arguments on the stack in case this is a nested interrupt. */
        args = *p_args;
    }

    p_args->event     = event;
    p_args->p_context = p_ctrl->p_context;

    /* If the project is not Trustzone Secure, then it will never need to change security state in order to call the callback. */
    p_ctrl->p_callback(p_args);
    if (NULL != p_ctrl->p_callback_memory)
    {
        /* Restore callback memory in case this is a nested interrupt. */
        *p_ctrl->p_callback_memory = args;
    }
}

/*******************************************************************************************************************//**
 * DMIC clock initialize
 *
 * @param[in]     frequency     DMIC ckock frequency in Hz (Range : 768,000 ~ 4,096,000 Hz)
 **********************************************************************************************************************/
void dmic_clk_init (uint32_t frequency)
{
    uint32_t div;

    CRG_APU->APU_AUD_CLK_REG_b.AUD_PCLK_DIV    = 1;
    CRG_APU->APU_SRC_CLK_REG_b.SRC_DMIC_CLK_EN = 0;

    /* Translate main clk frequency and requested frequency to proper divider */
    div = (BSP_CFG_FPLL_FREQ_HZ / frequency);

    /* Calculate the achievable frequency */
    if (BSP_CFG_FPLL_FREQ_HZ % frequency)
    {
        frequency = (BSP_CFG_FPLL_FREQ_HZ / div);
    }

    /* DMIC_CLK frequency according to specification is in the range of 76.8 kHz - 4.096 MHz */

    if (div == 0x20)
    {
        div = 0x0;
    }

    /////////////////////////////////////////////
    ///////// CRG_APU
    /////////////////////////////////////////////

    CRG_APU->APU_AUD_CLK_REG_b.AUD_CLK_DIV = 1;
    CRG_APU->APU_AUD_CLK_REG_b.AUD_CLK_EN  = 1;

    APU_AUD->APU_MAIN_DIV_REG_b.APU_MAIN_DIV_EN = 1;
    APU_AUD->APU_MAIN_DIV_REG_b.APU_MAIN_DIV    = 1;

    APU_AUD->APU_DMIC_DIV_REG_b.DMIC1_CLK_EN   = 1;
    APU_AUD->APU_DMIC_DIV_REG_b.DMIC_DIV       = (div & 0x1F);
    CRG_APU->APU_SRC_CLK_REG_b.SRC_DMIC_CLK_EN = 1;
}

/*******************************************************************************************************************//**
 * DMIC ISR. Calls callback when transmission is complete.  Fills FIFO if transfer interface is not used.
 **********************************************************************************************************************/
void SRC_Out_Handler (void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE;

    IRQn_Type              irq             = R_FSP_CurrentIrqGet();
    dmic_instance_ctrl_t * p_instance_ctrl = (dmic_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag in the ICU */
    R_BSP_IrqClearPending(irq);

    if (NULL != p_instance_ctrl->p_rx_dest)
    {
        /* If transfer is not used, write data. */
        r_dmic_w_fifo_read(p_instance_ctrl);
    }

    /* If there are more samples to write to the FIFO or the FIFO is above the watermark, don't call the callback. */
    if ((p_instance_ctrl->rx_dest_samples == 0))
    {
        r_dmic_w_call_callback(p_instance_ctrl, DMIC_EVENT_TX_EMPTY);
    }

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE;
}

void SRC_In_Handler (void)
{
    /* Default_Handler */              // TIN-TODO

    CRG_TOP->SYS_CTRL_REG_b.DEBUGGER_ENABLE = 1;

    BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);

    while (1)
    {
        __NOP();
    }

    ;
}
