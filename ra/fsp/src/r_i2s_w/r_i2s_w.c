/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_i2s_api.h"
#include "r_i2s_w.h"
#include "r_i2s_w_cfg.h"
#if I2S_W_CFG_DTC_ENABLE
 #include "r_dtc.h"
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* I2S protocol always has 2 channels (left and right). */
#define I2S_W_PRV_I2S_CHANNELS             (2U)
#define I2S_W_PRV_BITS_PER_BYTE            (8U)

/* "I2S" in ASCII, used to determine if driver is open. */
#define I2S_W_PRV_OPEN                     (0x535349U)

/* TIN_AA version only. */
#undef  BSP_FEATURE_I2S_FIFO_NUM_STAGES
#define BSP_FEATURE_I2S_FIFO_NUM_STAGES    (2)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

#if defined(__ARMCC_VERSION) || defined(__ICCARM__)
typedef void (BSP_CMSE_NONSECURE_CALL * i2s_prv_ns_callback)(i2s_callback_args_t * p_args);
#elif defined(__GNUC__)
typedef BSP_CMSE_NONSECURE_CALL void (*volatile i2s_prv_ns_callback)(i2s_callback_args_t * p_args);
#endif

/* I2S communication direction */
typedef enum e_i2s_dir
{
    I2S_DIR_RX    = 1U,                ///< Receive direction only
    I2S_DIR_TX    = 2U,                ///< Transmit direction only
    I2S_DIR_TX_RX = 3U,                ///< Transmit and receive direction
} i2s_dir_t;

/* Arguments are stored in a structure for transfer reset subroutine to avoid having more than 4 arguments. */
typedef struct e_i2s_prv_transfer_reset
{
    void const * p_data;
    uint32_t     bytes;
    void const * p_src;
    void       * p_dest;
} i2s_prv_transfer_reset_t;

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/* ISRs */
void DAI_TX_Handler(void);
void DAI_RX_Handler(void);

static void r_i2s_w_call_callback(i2s_w_instance_ctrl_t * p_ctrl, i2s_event_t event);

/* ISR subroutines */
static void r_i2s_w_tx_fifo_write(i2s_w_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_write);
static void r_i2s_w_rx_fifo_read(i2s_w_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_read);

/* FIFO subroutines */
static void r_i2s_w_fifo_write(i2s_w_instance_ctrl_t * p_instance_ctrl);
static void r_i2s_w_fifo_read(i2s_w_instance_ctrl_t * p_instance_ctrl);

/* Open subroutines */

static void r_i2s_w_interrupts_configure(i2s_w_instance_ctrl_t * const p_instance_ctrl, i2s_cfg_t const * const p_cfg);

#if I2S_W_CFG_DTC_ENABLE
static fsp_err_t r_i2s_w_dependent_drivers_configure(R_I2S0_Type           * p_reg,
                                                     i2s_cfg_t const * const p_cfg,
                                                     transfer_size_t         fifo_access_size);

#endif

/* Stop subroutines */
static void r_i2s_w_stop_sub(i2s_w_instance_ctrl_t * const p_instance_ctrl);

/* Start subroutines */
static fsp_err_t r_i2s_w_start(i2s_w_instance_ctrl_t * const p_instance_ctrl, i2s_dir_t dir);

/* Read and write subroutines. */
fsp_err_t r_i2s_w_tx_load_fifo(i2s_w_instance_ctrl_t * const p_instance_ctrl,
                               void const * const            p_src,
                               uint32_t const                bytes);
fsp_err_t r_i2s_w_rx_unload_fifo(i2s_w_instance_ctrl_t * const p_instance_ctrl,
                                 void * const                  p_dest,
                                 uint32_t const                bytes);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/** I2S Implementation of I2S interface.  */
const i2s_api_t g_i2s_on_i2s_w =
{
    .open        = R_I2S_W_Open,
    .stop        = R_I2S_W_Stop,
    .write       = R_I2S_W_Write,
    .read        = R_I2S_W_Read,
    .writeRead   = R_I2S_W_WriteRead,
    .mute        = R_I2S_W_Mute,
    .statusGet   = R_I2S_W_StatusGet,
    .close       = R_I2S_W_Close,
    .callbackSet = R_I2S_W_CallbackSet,
};

/*******************************************************************************************************************//**
 * @addtogroup I2S
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Opens the I2S. Implements @ref i2s_api_t::open.
 *
 * This function sets this clock divisor and the configurations specified in i2s_cfg_t.  It also opens the timer and
 * transfer instances if they are provided.
 *
 * @retval FSP_SUCCESS                     Ready for I2S communication.
 * @retval FSP_ERR_ASSERTION               The pointer to p_ctrl or p_cfg is null.
 * @retval FSP_ERR_ALREADY_OPEN            The control block has already been opened.
 * @retval FSP_ERR_IP_CHANNEL_NOT_PRESENT  Channel number is not available on this MCU.
 * @return                                 See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                         possible return codes. This function calls:
 *                                             * @ref transfer_api_t::open
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Open (i2s_ctrl_t * const p_ctrl, i2s_cfg_t const * const p_cfg)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;
    uint32_t                frame_len;
    uint32_t                w_len;

#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_callback);
    FSP_ASSERT(p_cfg->int_irq >= 0);
    if (I2S_MODE_MASTER == p_cfg->operating_mode)
    {
        FSP_ASSERT(NULL != p_cfg->p_extend);
    }

 #if I2S_W_CFG_DTC_ENABLE
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

    FSP_ERROR_RETURN(I2S_W_PRV_OPEN != p_instance_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ERROR_RETURN(0U != ((1U << p_cfg->channel) & BSP_FEATURE_I2S_VALID_CHANNEL_MASK),
                     FSP_ERR_IP_CHANNEL_NOT_PRESENT);
#endif

    uint32_t base_address = (uint32_t) DAI;

    DAI_Type * p_reg = (DAI_Type *) base_address;

    /* Determine how to access the FIFO (1 byte, 2 byte, or 4 byte access). */
    transfer_size_t fifo_access_size = TRANSFER_SIZE_4_BYTE;
    if (I2S_PCM_WIDTH_8_BITS == p_cfg->pcm_width)
    {
        fifo_access_size = TRANSFER_SIZE_1_BYTE;
    }

    if (I2S_PCM_WIDTH_16_BITS == p_cfg->pcm_width)
    {
        fifo_access_size = TRANSFER_SIZE_2_BYTE;
    }

#if I2S_W_CFG_DTC_ENABLE

    /* Configure dependent timer and transfer drivers. */
    fsp_err_t err = r_i2s_w_dependent_drivers_configure(p_reg, p_cfg, fifo_access_size);
    FSP_ERROR_RETURN((FSP_SUCCESS == err), err);
#endif

    /* Initialize the control structure. */
    p_instance_ctrl->p_reg            = p_reg;
    p_instance_ctrl->p_cfg            = p_cfg;
    p_instance_ctrl->fifo_access_size = fifo_access_size;

    /* Configure interrupts. */
    r_i2s_w_interrupts_configure(p_instance_ctrl, p_cfg);

    /* Calculate register settings. */

    /* Configure operating mode. */
    p_instance_ctrl->p_reg->DAI_MODE_REG_b.MODE = (uint32_t) p_cfg->operating_mode & DAI_DAI_MODE_REG_MODE_Msk;

    /* Configure sample size, and word length. */

    switch (p_cfg->word_length)
    {
        case I2S_WORD_LENGTH_32_BITS:
        {
            frame_len = 0;
            break;
        }

        case I2S_WORD_LENGTH_64_BITS:
        {
            frame_len = 1;
            break;
        }

        case I2S_WORD_LENGTH_128_BITS:
        {
            frame_len = 2;
            break;
        }

        case I2S_WORD_LENGTH_256_BITS:
        {
            frame_len = 3;
            break;
        }

        default:
        {
            return FSP_ERR_INVALID_ARGUMENT;
            break;
        }
    }

    p_instance_ctrl->p_reg->DAI_CONFIG_REG_b.FRAME_LEN = frame_len & 0x03;

    switch (p_cfg->pcm_width)
    {
        case I2S_PCM_WIDTH_16_BITS:
        {
            w_len = 0;
            break;
        }

        case I2S_PCM_WIDTH_20_BITS:
        {
            w_len = 1;
            break;
        }

        case I2S_PCM_WIDTH_24_BITS:
        {
            w_len = 2;
            break;
        }

        case I2S_PCM_WIDTH_32_BITS:
        {
            w_len = 3;
            break;
        }

        default:
        {
            return FSP_ERR_INVALID_ARGUMENT;
            break;
        }
    }

    p_instance_ctrl->p_reg->DAI_W_LEN_REG_b.W_LEN = w_len & DAI_DAI_W_LEN_REG_W_LEN_Msk;

    /* Configure audio clock and WS continue in master mode only. */

    // uint32_t i2sofr = 0U;
    if (I2S_MODE_MASTER == p_cfg->operating_mode)
    {
        // jason need to clock set here
    }

    /* Initialize the I2SE following the procedure in Figure 41.53 "Procedure to start communication (CPU operation
     * procedure)" of the RA6M3 manual R01UH0886EJ0100. This function follows this procedure except for enabling
     * interrupts and enabling communication, which are done before communication begins. */

    p_instance_ctrl->p_callback        = p_cfg->p_callback;
    p_instance_ctrl->p_context         = p_cfg->p_context;
    p_instance_ctrl->p_callback_memory = NULL;

    REG_SETF(SRC_IF, APU_DAI_FIFO_CTRL_REG, FIFO_CTRL, 0);

    REG_SETF(DAI, DAI_DATA_OUT_CTRL_REG, BCLK_POL, 1);      // 1, bit clock polarity

    /* The BCLK edge used to sample incoming data
     * 0: rising, 1:falling(default) */
    REG_SETF(DAI, DAI_DATA_OUT_CTRL_REG, WCLK_POL, 1);      // 1, word clock polarity :

    /* The WCLK edge defining the start of the PCM frame. depends on frame format
     * 0x0: Rising (LJF, RJF, DSP), Falling (I2S), 0x1: Falling (LJF, RJF, DSP), Rising (I2S) */
    REG_SETF(DAI, DAI_DATA_OUT_CTRL_REG, DATA_OUT_EN, 2);   // 2, output enable.

    /* 0x0: Data output is tristate, 0x1: Data output is tristate
     *  0x2: Data driven on all slots, 0x3: Data driven only during enabled slots*/
    REG_SETF(DAI, DAI_DATA_OUT_CTRL_REG, TDM_EARLY_RLS, 0); // 0

    /* 0x0: (POR) Data is driven until the end of the slot
     *  0x1: Data is driven until half of BCLK before the end of the slot */
    REG_SETF(DAI, DAI_OFFSET_MSB_REG, OFFSET_MSB, 0);       // 0, offset (msb)
    REG_SETF(DAI, DAI_OFFSET_LSB_REG, OFFSET_LSB, 0);       // 0, offset (lsb)

    REG_SETF(DAI, DAI_RX1_CH_REG, RX1_CH, 1);               // 1, slot index
    REG_SETF(DAI, DAI_RX2_CH_REG, RX2_CH, 2);               // 2, slot index
    REG_SETF(DAI, DAI_TX1_CH_REG, TX1_CH, 1);               // 1, slot index
    REG_SETF(DAI, DAI_TX2_CH_REG, TX2_CH, 2);               // 2, slot index

    REG_SETF(DAI, DAI_TX_MUX_REG, DAI_TX1_SEL, 0);          // 0, 0:from reg, 1: from router
    REG_SETF(DAI, DAI_TX_MUX_REG, DAI_TX2_SEL, 0);          // 0

    REG_SETF(CRG_APU, APU_AUD_CLK_REG, AUD_CLK_DIV, 1);     // 1

    /* 3 : 98.304 / (3+1) = 24.576*/

    REG_SETF(APU_DSP, APU_PCM1_DIV_REG, PCM1_DIV, 1);       // 1

    REG_SETF(CRG_APU, APU_AUD_CLK_REG, AUD_CLK_EN, 1);      // 1
    REG_SETF(APU_DSP, APU_PCM_CLK_REG, PCM1_CLK_EN, 1);     // 2

    /////////////////////////////////////////////
    ///////// APU_DSP
    /////////////////////////////////////////////
    REG_SETF(DAI, DAI_SLOT_CNT_REG, SLOT_CNT, 2);                                               // 2, number of slot : 1~8
    REG_SETF(DAI, DAI_SR_CONFIG_REG, DAI_SR, ((i2s_w_extended_cfg_t *) (p_cfg->p_extend))->sr); // symbol rate
    REG_SETF(DAI, DAI_CONFIG_REG, FORMAT, 0);                                                   // 0x0, I2S

    REG_SETF(APU_DSP, APU_CTRL_REG, DAI1_EN, 1);                                                // 1, dai enable in apu

    /* Initialization complete. */
    p_instance_ctrl->open = I2S_W_PRV_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Writes data buffer to I2S. Implements @ref i2s_api_t::write.
 *
 * This function resets the transfer if the transfer interface is used, or writes the length of data that fits in the
 * FIFO then stores the remaining write buffer in the control block to be written in the ISR.
 *
 * Write() cannot be called if another write(), read() or writeRead() operation is in progress.  Write can be called
 * when the I2S is idle, or after the I2S_EVENT_TX_EMPTY event.
 *
 * @retval FSP_SUCCESS                 Write initiated successfully.
 * @retval FSP_ERR_ASSERTION           The pointer to p_ctrl or p_src was null, or bytes requested was 0.
 * @retval FSP_ERR_IN_USE              Another transfer is in progress, data was not written.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_UNDERFLOW           A transmit underflow error is pending. Wait for the I2S to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Write (i2s_ctrl_t * const p_ctrl, void const * const p_src, uint32_t const bytes)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;

#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ASSERT(NULL != p_src);

    /* bytes must be a non-zero */
    FSP_ASSERT(bytes > 0U);
#endif

    /* If a transfer instance is provided for write, reset the transfer. Otherwise store data to transmit in the
     * transmit interrupt. */
    fsp_err_t err = r_i2s_w_tx_load_fifo(p_instance_ctrl, p_src, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Make sure transmission is enabled. */
    err = r_i2s_w_start(p_instance_ctrl, I2S_DIR_TX);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Reads data into provided buffer. Implements @ref i2s_api_t::read.
 *
 * This function resets the transfer if the transfer interface is used, or reads the length of data available in the
 * FIFO then stores the remaining read buffer in the control block to be filled in the ISR.
 *
 * Read() cannot be called if another write(), read() or writeRead() operation is in progress.  Read can be called
 * when the I2S is idle, or after the I2S_EVENT_RX_FULL event.
 *
 * @retval FSP_SUCCESS                 Read initiated successfully.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 * @retval FSP_ERR_ASSERTION           The pointer to p_ctrl or p_dest was null, or bytes requested was 0.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_OVERFLOW            A receive overflow error is pending. Wait for the I2S to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Read (i2s_ctrl_t * const p_ctrl, void * const p_dest, uint32_t const bytes)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;

#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ASSERT(NULL != p_dest);

    /* bytes must be a non-zero */
    FSP_ASSERT(bytes > 0U);
#endif

    /* If a transfer instance is provided for read, reset the transfer. Otherwise store data to receive in the receive
     * interrupt. */
    fsp_err_t err = r_i2s_w_rx_unload_fifo(p_instance_ctrl, p_dest, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a receive overflow is pending, return an error.  This could happen if the application is in an interrupt
     * context with a priority greater than or equal to the idle/error interrupt priority and does not read from the
     * receive FIFO before it overflows. */

// FSP_ERROR_RETURN(0U == p_instance_ctrl->p_reg->I2SSR_b.ROIRQ, FSP_ERR_OVERFLOW);

    /* Make sure reception is enabled. */
    err = r_i2s_w_start(p_instance_ctrl, I2S_DIR_RX);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Writes from source buffer and reads data into destination buffer. Implements @ref i2s_api_t::writeRead.
 *
 * writeRead() cannot be called if another write(), read() or writeRead() operation is in progress.  writeRead() can be
 * called when the I2S is idle, or after the I2S_EVENT_RX_FULL event.
 *
 * @retval FSP_SUCCESS                 Write and read initiated successfully.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 * @retval FSP_ERR_ASSERTION           An input parameter was invalid.
 * @retval FSP_ERR_NOT_OPEN            The channel is not opened.
 * @retval FSP_ERR_UNDERFLOW           A transmit underflow error is pending. Wait for the I2S to go idle before
 *                                     resuming communication.
 * @retval FSP_ERR_OVERFLOW            A receive overflow error is pending. Wait for the I2S to go idle before
 *                                     resuming communication.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_WriteRead (i2s_ctrl_t * const p_ctrl,
                             void const * const p_src,
                             void * const       p_dest,
                             uint32_t const     bytes)
{
#if 1
    (void) p_ctrl;
    (void) p_src;
    (void) p_dest;
    (void) bytes;
#else
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;

 #if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_src);
    FSP_ASSERT(NULL != p_dest);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* bytes must be a non-zero */
    FSP_ASSERT(bytes > 0U);
 #endif

    /* If a transfer instance is provided for write, reset the transfer. Reset the transmit FIFO first since the
     * transmit FIFO will underflow before the receive FIFO overflows during full duplex communication. */
    fsp_err_t err = r_i2s_w_tx_load_fifo(p_instance_ctrl, p_src, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a transfer instance is provided for read, reset the transfer. */
    err = r_i2s_w_rx_unload_fifo(p_instance_ctrl, p_dest, bytes);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* If a transmit underflow or receive overflow is pending, return an error.  This could happen if the application
     * is in an interrupt context with a priority greater than or equal to the idle/error interrupt priority and does
     * not reload the FIFOs before they underflow/overflow. */
    uint32_t i2ssr = p_instance_ctrl->p_reg->I2SSR;
    FSP_ERROR_RETURN(0U == (i2ssr & (1U << I2S_PRV_I2SSR_TUIRQ_BIT)), FSP_ERR_UNDERFLOW);
    FSP_ERROR_RETURN(0U == (i2ssr & (1U << I2S_PRV_I2SSR_ROIRQ_BIT)), FSP_ERR_OVERFLOW);

    /* Make sure transmission and reception are enabled. */
    err = r_i2s_w_start(p_instance_ctrl, I2S_DIR_TX_RX);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Stops I2S. Implements @ref i2s_api_t::stop.
 *
 * This function disables both transmission and reception, and disables any transfer instances used.
 *
 * The I2S will stop on the next frame boundary.  Do not restart I2S until it is idle.
 *
 * @retval FSP_SUCCESS           I2S communication stop request issued.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 * @return                       See @ref RENESAS_ERROR_CODES or lower level drivers for other possible return codes.
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Stop (i2s_ctrl_t * const p_ctrl)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;

#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Stop is complete after an I2S_EVENT_IDLE interrupt. */
    r_i2s_w_stop_sub(p_instance_ctrl);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Mutes I2S on the next frame boundary. Implements @ref i2s_api_t::mute.
 *
 * Data is still written while mute is enabled, but the transmit line outputs zeros.
 *
 * @retval FSP_SUCCESS           Transmission is muted.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Mute (i2s_ctrl_t * const p_ctrl, i2s_mute_t const mute_enable)
{
    (void) mute_enable;
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;

#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    // jsaon. need to add mute function.
    p_instance_ctrl->p_reg->DAI_ENABLE_REG_b.EN = 0;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Gets I2S status and stores it in provided pointer p_status. Implements @ref i2s_api_t::statusGet.
 *
 * @retval FSP_SUCCESS           Information stored successfully.
 * @retval FSP_ERR_ASSERTION     The p_instance_ctrl or p_status parameter was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_StatusGet (i2s_ctrl_t * const p_ctrl, i2s_status_t * const p_status)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;
#if I2S_W_CFG_PARAM_CHECKING_ENABLE

    /* Make sure parameters are valid. */
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ASSERT(NULL != p_status);
#endif

    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
    p_status->state = 0;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Closes I2S. Implements @ref i2s_api_t::close.
 *
 * This function powers down the I2S and closes the lower level timer and transfer drivers if they are used.
 *
 * @retval FSP_SUCCESS           Device closed successfully.
 * @retval FSP_ERR_ASSERTION     The pointer to p_ctrl was null.
 * @retval FSP_ERR_NOT_OPEN      The channel is not opened.
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_Close (i2s_ctrl_t * const p_ctrl)
{
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) p_ctrl;
#if I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_instance_ctrl);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    p_instance_ctrl->open = 0U;

    /* Stop I2SE. */
    p_instance_ctrl->p_reg->DAI_ENABLE_REG_b.EN = 0;

    // need to add more disable sequence -- jason

    /* Disable interrupts. */
    R_BSP_IrqDisable(p_instance_ctrl->p_cfg->int_irq);
    if (p_instance_ctrl->p_cfg->rxi_irq >= 0)
    {
        R_BSP_IrqDisable(p_instance_ctrl->p_cfg->rxi_irq);
    }

    if (p_instance_ctrl->p_cfg->txi_irq >= 0)
    {
        R_BSP_IrqDisable(p_instance_ctrl->p_cfg->txi_irq);
    }

#if I2S_W_CFG_DTC_ENABLE

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

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Updates the user callback and has option of providing memory for callback structure.
 * Implements i2s_api_t::callbackSet
 *
 * @retval  FSP_SUCCESS                  Callback updated successfully.
 * @retval  FSP_ERR_ASSERTION            A required pointer is NULL.
 * @retval  FSP_ERR_NOT_OPEN             The control block has not been opened.
 * @retval  FSP_ERR_NO_CALLBACK_MEMORY   p_callback is non-secure and p_callback_memory is either secure or NULL.
 **********************************************************************************************************************/
fsp_err_t R_I2S_W_CallbackSet (i2s_ctrl_t * const          p_api_ctrl,
                               void (                    * p_callback)(i2s_callback_args_t *),
                               void * const                p_context,
                               i2s_callback_args_t * const p_callback_memory)
{
    i2s_w_instance_ctrl_t * p_ctrl = (i2s_w_instance_ctrl_t *) p_api_ctrl;

#if (I2S_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_callback);
    FSP_ERROR_RETURN(I2S_W_PRV_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Store callback and context */
    p_ctrl->p_callback        = p_callback;
    p_ctrl->p_context         = p_context;
    p_ctrl->p_callback_memory = p_callback_memory;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup R_I2S_W)
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
static void r_i2s_w_interrupts_configure (i2s_w_instance_ctrl_t * const p_instance_ctrl, i2s_cfg_t const * const p_cfg)
{
    /* Set interrupt priority based on user configuration of interrupt number for FSP_SIGNAL_I2S_RXI
     * or FSP_SIGNAL_I2S_TXI_RXI signals */
    if (p_cfg->rxi_irq >= 0)
    {
        R_BSP_IrqCfgEnable(p_cfg->rxi_irq, p_cfg->rxi_ipl, p_instance_ctrl);
    }

    /* Set interrupt priority based on user configuration of interrupt number for FSP_SIGNAL_I2S_TXI
     * or FSP_SIGNAL_I2S_TXI_RXI signals */
    if (p_cfg->txi_irq >= 0)
    {
        R_BSP_IrqCfgEnable(p_cfg->txi_irq, p_cfg->txi_ipl, p_instance_ctrl);
    }
}

#if I2S_W_CFG_DTC_ENABLE

/*******************************************************************************************************************//**
 * Configures any dependent drivers selected by the user, including transfer and timer drivers.
 *
 * @param[in] p_reg                    Pointer to I2SE base register address for this channel.
 * @param[in] p_cfg                    Pointer to the configuration structure.
 *
 * @retval FSP_SUCCESS                 Dependent drivers configured successfully.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::open
 **********************************************************************************************************************/
static fsp_err_t r_i2s_w_dependent_drivers_configure (R_I2S0_Type           * p_reg,
                                                      i2s_cfg_t const * const p_cfg,
                                                      transfer_size_t         fifo_access_size)
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
        p_cfg->p_transfer_tx->p_cfg->p_info->p_dest = (void *) &(p_reg->I2SFTDR);
        uint32_t transfer_settings_tx = transfer_settings |
                                        (TRANSFER_ADDR_MODE_INCREMENTED << TRANSFER_SETTINGS_SRC_ADDR_BITS);
        p_cfg->p_transfer_tx->p_cfg->p_info->transfer_settings_word = transfer_settings_tx;
        p_cfg->p_transfer_tx->p_cfg->p_info->length                 = I2S_PRV_TRANSFER_BLOCK_SIZE;
        err_transfer_tx = p_cfg->p_transfer_tx->p_api->open(p_cfg->p_transfer_tx->p_ctrl, p_cfg->p_transfer_tx->p_cfg);
    }

  #if  I2S_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ERROR_RETURN((FSP_SUCCESS == err_transfer_tx), err_transfer_tx);
  #else
    FSP_PARAMETER_NOT_USED(err_transfer_tx);
  #endif

    /* If a transfer instance is provided for read, open the transfer instance. */
    if (NULL != p_cfg->p_transfer_rx)
    {
        p_cfg->p_transfer_rx->p_cfg->p_info->p_src = (void *) &(p_reg->I2SFRDR);
        uint32_t transfer_settings_rx = transfer_settings |
                                        (TRANSFER_ADDR_MODE_INCREMENTED << TRANSFER_SETTINGS_DEST_ADDR_BITS) |
                                        (TRANSFER_REPEAT_AREA_SOURCE << TRANSFER_SETTINGS_REPEAT_AREA_BITS);
        p_cfg->p_transfer_rx->p_cfg->p_info->transfer_settings_word = transfer_settings_rx;
        p_cfg->p_transfer_rx->p_cfg->p_info->length                 = I2S_PRV_TRANSFER_BLOCK_SIZE;
        err_transfer_rx = p_cfg->p_transfer_rx->p_api->open(p_cfg->p_transfer_rx->p_ctrl, p_cfg->p_transfer_rx->p_cfg);
    }

  #if  I2S_W_CFG_PARAM_CHECKING_ENABLE

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
 * Disables I2S transmission and reception.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 **********************************************************************************************************************/
static void r_i2s_w_stop_sub (i2s_w_instance_ctrl_t * const p_instance_ctrl)
{
    /* Stop communication following the procedure from Figure 41.56 "Procedure to halt communication (CPU operation
     * procedure)" in the RA6M3 manual R01UH0886EJ0100. */

    p_instance_ctrl->p_reg->DAI_ENABLE_REG_b.EN = 0;

#if I2S_W_CFG_DTC_ENABLE

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

    // p_instance_ctrl->p_reg->I2SFCR = (uint32_t) p_instance_ctrl->p_cfg->operating_mode << I2S_PRV_I2SFCR_AUCKE_BIT;

    /* Clear control structure data. */
    p_instance_ctrl->p_tx_src        = NULL;
    p_instance_ctrl->tx_src_samples  = 0U;
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
 * @retval FSP_SUCCESS                 Transmit FIFO successfully loaded.
 * @return                             See @ref RENESAS_ERROR_CODES or functions called by this function for other
 *                                     possible return codes. This function calls:
 *                                         * @ref transfer_api_t::reset
 **********************************************************************************************************************/
fsp_err_t r_i2s_w_tx_load_fifo (i2s_w_instance_ctrl_t * const p_instance_ctrl,
                                void const * const            p_src,
                                uint32_t const                bytes)
{
    void   * p_data  = (void *) p_src;
    uint32_t samples = bytes >> p_instance_ctrl->fifo_access_size;
#if I2S_W_CFG_DTC_ENABLE

    /* By default, bytes are written in the ISR. */

    /* If a transfer instance is provided, reset the transfer. */
    transfer_instance_t const * const p_transfer = p_instance_ctrl->p_cfg->p_transfer_tx;
    if (NULL != p_transfer)
    {
        /* We have already verified that the 'samples' is a non-zero multiple of 2 in parameter checking. */
        uint32_t transfer_blocks = samples / I2S_PRV_TRANSFER_BLOCK_SIZE;

        fsp_err_t err = p_transfer->p_api->reset(p_transfer->p_ctrl, p_src, NULL, (uint16_t) transfer_blocks);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

        p_data  = NULL;
        samples = 0U;
    }
#endif

    p_instance_ctrl->p_tx_src       = p_data;
    p_instance_ctrl->tx_src_samples = samples;

    return FSP_SUCCESS;
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
fsp_err_t r_i2s_w_rx_unload_fifo (i2s_w_instance_ctrl_t * const p_instance_ctrl,
                                  void * const                  p_dest,
                                  uint32_t const                bytes)
{
    void   * p_data  = p_dest;
    uint32_t samples = bytes >> p_instance_ctrl->fifo_access_size;
#if I2S_W_CFG_DTC_ENABLE

    /* By default, bytes are written in the ISR. */

    /* If a transfer instance is provided for reception, reset the transfer. */
    transfer_instance_t const * const p_transfer = p_instance_ctrl->p_cfg->p_transfer_rx;
    if (NULL != p_transfer)
    {
        /* Always read at least one sample from the receive interrupt. This ensures that the DTC transfer will be over
         * by the time a transmit underflow occurs during R_I2S_W_WriteRead processing. This is important so the receive
         * buffer can be flushed in the transmit underflow error processing. Without this, the last frame (two samples)
         * could be lost during R_I2S_W_WriteRead. */
        uint32_t transfer_blocks = (samples / I2S_PRV_TRANSFER_BLOCK_SIZE) - 1U;
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
 * Enables I2S transmission and/or reception.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] dir                      Start transmit, receive, or both.
 *
 * @retval FSP_SUCCESS                 Ready for I2S transmission.
 * @retval FSP_ERR_IN_USE              Peripheral is in the wrong mode or not idle.
 **********************************************************************************************************************/
static fsp_err_t r_i2s_w_start (i2s_w_instance_ctrl_t * const p_instance_ctrl, i2s_dir_t dir)
{
#if 1
    (void) dir;
    uint32_t current_i2s_en = p_instance_ctrl->p_reg->DAI_ENABLE_REG_b.EN;

    /* If the peripheral is not already in the correct mode, attempt to start it. */
    if (!current_i2s_en)
    {
        /* If the peripheral is in the wrong mode or not idle, return an error. The I2S must be idle before setting
         * REN or TEN. Reference 41.11.3.4 "Switching transfer modes" in the RA6M3 manual R01UH0886EJ0100. */
        FSP_ERROR_RETURN(0U == current_i2s_en, FSP_ERR_IN_USE);

        // FSP_ERROR_RETURN(1U == p_instance_ctrl->p_reg->I2SSR_b.IIRQ, FSP_ERR_IN_USE);

        /* Reset I2SE FIFOs. Set TFRST and RFRST, then clear them and wait for them to clear. This operation empties
         * the FIFOs. */

        // jason. need to add FIFO Clear

        /* If starting transmission, enable transmit interrupts. */
        /* Enabling communication clears related error flags in I2SSR. */
        p_instance_ctrl->p_reg->DAI_ENABLE_REG_b.EN = 1;
    }

#else
    uint32_t i2scr                 = p_instance_ctrl->p_reg->I2SCR;
    uint32_t current_i2scr_ren_ten = i2scr & I2S_PRV_I2SCR_REN_TEN_MASK;
    uint32_t desired_i2scr_ren_ten = (uint32_t) dir;

    /* If the peripheral is not already in the correct mode, attempt to start it. */
    if (desired_i2scr_ren_ten != (current_i2scr_ren_ten & desired_i2scr_ren_ten))
    {
        /* If the peripheral is in the wrong mode or not idle, return an error. The I2S must be idle before setting
         * REN or TEN. Reference 41.11.3.4 "Switching transfer modes" in the RA6M3 manual R01UH0886EJ0100. */
        FSP_ERROR_RETURN(0U == current_i2scr_ren_ten, FSP_ERR_IN_USE);
        FSP_ERROR_RETURN(1U == p_instance_ctrl->p_reg->I2SSR_b.IIRQ, FSP_ERR_IN_USE);

        /* Reset I2SE FIFOs. Set TFRST and RFRST, then clear them and wait for them to clear. This operation empties
         * the FIFOs. */
        uint32_t i2sfcr = p_instance_ctrl->p_reg->I2SFCR;
        p_instance_ctrl->p_reg->I2SFCR = i2sfcr | I2S_PRV_I2SFCR_TFRST_RFRST_MASK;
        p_instance_ctrl->p_reg->I2SFCR = i2sfcr;

        /* Wait for TFRST and RFRST to clear before continuing.  Reference TFRST and RFRST in section 41.4.3 "FIFO
         * Control Register (I2SFCR)" of the RA6M3 manual R01UH0886EJ0100. */
        FSP_HARDWARE_REGISTER_WAIT(p_instance_ctrl->p_reg->I2SFCR, i2sfcr);

        /* If starting transmission, enable transmit interrupts. */
        i2scr |= desired_i2scr_ren_ten;
        if ((desired_i2scr_ren_ten & I2S_PRV_I2SCR_TEN_BIT) > 0U)
        {
            /* Setting to start I2S Tx */
            i2scr  |= (1U << I2S_PRV_I2SCR_TUIEN_BIT);
            i2sfcr |= (1U << I2S_PRV_I2SFCR_TIE_BIT);
        }

        /* If starting reception, enable receive interrupts. */
        if ((desired_i2scr_ren_ten & I2S_PRV_I2SCR_REN_BIT) > 0U)
        {
            /* Setting to start I2S Rx */
            i2scr  |= (1U << I2S_PRV_I2SCR_ROIEN_BIT);
            i2sfcr |= (1U << I2S_PRV_I2SFCR_RIE_BIT);
        }

        /* Enabling communication clears related error flags in I2SSR. */
        p_instance_ctrl->p_reg->I2SFCR = i2sfcr;
        p_instance_ctrl->p_reg->I2SCR  = i2scr;
    }
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Writes data to FIFO.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 *
 * @return The number of stages written
 **********************************************************************************************************************/
static void r_i2s_w_fifo_write (i2s_w_instance_ctrl_t * p_instance_ctrl)
{
#if 1

    /* Calculate the number of words to write, limited by the space available in the FIFO. */
    uint32_t stages_to_write = p_instance_ctrl->tx_src_samples;

    /* Calculate the number of free spaces in transmit FIFO. */
    uint32_t fifo_free_stages = BSP_FEATURE_I2S_FIFO_NUM_STAGES;
    if (stages_to_write > fifo_free_stages)
    {
        stages_to_write = fifo_free_stages;
    }

    r_i2s_w_tx_fifo_write(p_instance_ctrl, stages_to_write);
    uint32_t bytes_written = stages_to_write << p_instance_ctrl->fifo_access_size;
    p_instance_ctrl->tx_src_samples -= stages_to_write;
    p_instance_ctrl->p_tx_src        = (void *) ((uint32_t) p_instance_ctrl->p_tx_src + bytes_written);

    /* Clear TDE only if something was written to the FIFO, otherwise the transmit interrupt will fire repeatedly. */
    if (stages_to_write > 0U)
    {
        /* The TDE bit must be read as 1 before it can be cleared. */

        // p_instance_ctrl->p_reg->I2SFSR;
        // p_instance_ctrl->p_reg->I2SFSR = I2S_PRV_I2SFSR_TDE_CLEAR;
    }

    /* If transmission is complete, clear transmit buffer to NULL. */
    if (0U == p_instance_ctrl->tx_src_samples)
    {
        p_instance_ctrl->p_tx_src = NULL;
    }

#else

    /* Calculate the number of words to write, limited by the space available in the FIFO. */
    uint32_t stages_to_write = p_instance_ctrl->tx_src_samples;

    /* Calculate the number of free spaces in transmit FIFO. */
    uint32_t fifo_free_stages = BSP_FEATURE_I2S_FIFO_NUM_STAGES - p_instance_ctrl->p_reg->I2SFSR_b.TDC;
    if (stages_to_write > fifo_free_stages)
    {
        stages_to_write = fifo_free_stages;
    }

    r_i2s_w_tx_fifo_write(p_instance_ctrl, stages_to_write);
    uint32_t bytes_written = stages_to_write << p_instance_ctrl->fifo_access_size;
    p_instance_ctrl->tx_src_samples -= stages_to_write;
    p_instance_ctrl->p_tx_src        = (void *) ((uint32_t) p_instance_ctrl->p_tx_src + bytes_written);

    /* Clear TDE only if something was written to the FIFO, otherwise the transmit interrupt will fire repeatedly. */
    if (stages_to_write > 0U)
    {
        /* The TDE bit must be read as 1 before it can be cleared. */
        p_instance_ctrl->p_reg->I2SFSR;
        p_instance_ctrl->p_reg->I2SFSR = I2S_PRV_I2SFSR_TDE_CLEAR;
    }

    /* If transmission is complete, clear transmit buffer to NULL. */
    if (0U == p_instance_ctrl->tx_src_samples)
    {
        p_instance_ctrl->p_tx_src = NULL;
    }
#endif
}

/*******************************************************************************************************************//**
 *  Reads data from FIFO.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 **********************************************************************************************************************/
static void r_i2s_w_fifo_read (i2s_w_instance_ctrl_t * p_instance_ctrl)
{
    /* Calculate the number of available bytes of data in receive FIFO. */
    uint32_t fifo_filled_stages = BSP_FEATURE_I2S_FIFO_NUM_STAGES;

    /* Calculate the number of FIFO stages requested to read. */
    uint32_t stages_to_read = 0U;
    stages_to_read = p_instance_ctrl->rx_dest_samples;
    if (stages_to_read > fifo_filled_stages)
    {
        stages_to_read = fifo_filled_stages;
    }

    r_i2s_w_rx_fifo_read(p_instance_ctrl, stages_to_read);
    uint32_t bytes_read = stages_to_read << p_instance_ctrl->fifo_access_size;
    p_instance_ctrl->rx_dest_samples -= stages_to_read;
    p_instance_ctrl->p_rx_dest        = (void *) ((uint32_t) p_instance_ctrl->p_rx_dest + bytes_read);

    /* Clear RDF only if something was written to the FIFO, otherwise the receive interrupt will fire repeatedly. */
    if (stages_to_read > 0)
    {
        // p_instance_ctrl->p_reg->I2SFSR;
        // p_instance_ctrl->p_reg->I2SFSR = I2S_PRV_I2SFSR_RDF_CLEAR;
    }

    /* If reception is complete, clear receive buffer to NULL. */
    if (0U == p_instance_ctrl->rx_dest_samples)
    {
        p_instance_ctrl->p_rx_dest = NULL;
    }
}

/*******************************************************************************************************************//**
 * Writes data to the transmit FIFO based on the FIFO access size.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] stages_to_write          Number of times to write to the FIFO.
 **********************************************************************************************************************/
static void r_i2s_w_tx_fifo_write (i2s_w_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_write)
{
#if 0                                  // TIN_BA version. not implemented yet
    if (TRANSFER_SIZE_4_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint32_t * p_src32 = (uint32_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write; i++)
        {
            SRC_FIFO_IF->APU_DAI_FIFO_IN1_REG = *p_src32;

            // p_instance_ctrl->p_reg->I2SFTDR = *p_src32;
            p_src32++;
        }
    }

    if (TRANSFER_SIZE_2_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint16_t * p_src16 = (uint16_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write; i++)
        {
            SRC_FIFO_IF->APU_DAI_FIFO_IN1_REG = *p_src16;

            // p_instance_ctrl->p_reg->I2SFTDR16 = *p_src16;
            p_src16++;
        }
    }

    if (TRANSFER_SIZE_1_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint8_t * p_src8 = (uint8_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write; i++)
        {
            SRC_FIFO_IF->APU_DAI_FIFO_IN1_REG = *p_src8;

            // p_instance_ctrl->p_reg->I2SFTDR8 = *p_src8;
            p_src8++;
        }
    }

#else                                  // TIN_AA version
    if (TRANSFER_SIZE_4_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint32_t * p_src32 = (uint32_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write / 2; i++)
        {
            DAI->DAI_TX1_REG = *p_src32;
            p_src32++;
            DAI->DAI_TX2_REG = *p_src32;
            p_src32++;
        }
    }

    if (TRANSFER_SIZE_2_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint16_t * p_src16 = (uint16_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write / 2; i++)
        {
            DAI->DAI_TX1_REG = (uint32_t) (*p_src16) << 16;
            p_src16++;
            DAI->DAI_TX2_REG = (uint32_t) (*p_src16) << 16;
            p_src16++;
        }
    }

    if (TRANSFER_SIZE_1_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint8_t * p_src8 = (uint8_t *) p_instance_ctrl->p_tx_src;
        for (uint32_t i = 0; i < stages_to_write / 2; i++)
        {
            DAI->DAI_TX1_REG = (uint32_t) (*p_src8) << 24;
            p_src8++;
            DAI->DAI_TX2_REG = (uint32_t) (*p_src8) << 24;
            p_src8++;
        }
    }
#endif
}

/*******************************************************************************************************************//**
 * Reads data from the receive FIFO based on the FIFO access size.
 *
 * @param[in] p_instance_ctrl          Pointer to the control block.
 * @param[in] stages_to_read           Number of times to read from the FIFO.
 **********************************************************************************************************************/
static void r_i2s_w_rx_fifo_read (i2s_w_instance_ctrl_t * p_instance_ctrl, uint32_t stages_to_read)
{
#if 0                                  // TIN_BA version. not implemented yet
    if (TRANSFER_SIZE_4_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint32_t * p_dest32 = (uint32_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read; i++)
        {
            *p_dest32 = (uint32_t) SRC_FIFO_IF->APU_DAI_FIFO_OUT1_REG;
            p_dest32++;
        }
    }

    if (TRANSFER_SIZE_2_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint16_t * p_dest16 = (uint16_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read; i++)
        {
            *p_dest16 = (uint16_t) SRC_FIFO_IF->APU_DAI_FIFO_OUT1_REG;
            p_dest16++;
        }
    }

    if (TRANSFER_SIZE_1_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint8_t * p_dest8 = (uint8_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read; i++)
        {
            *p_dest8 = (uint8_t) SRC_FIFO_IF->APU_DAI_FIFO_OUT1_REG;
            p_dest8++;
        }
    }

#else
    if (TRANSFER_SIZE_4_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint32_t * p_dest32 = (uint32_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read / 2; i++)
        {
            *p_dest32 = (uint32_t) DAI->DAI_RX1_REG;
            p_dest32++;
            *p_dest32 = (uint32_t) DAI->DAI_RX2_REG;
            p_dest32++;
        }
    }

    if (TRANSFER_SIZE_2_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint16_t * p_dest16 = (uint16_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read / 2; i++)
        {
            *p_dest16 = (uint16_t) (DAI->DAI_RX1_REG >> 16);
            p_dest16++;
            *p_dest16 = (uint16_t) (DAI->DAI_RX2_REG >> 16);
            p_dest16++;
        }
    }

    if (TRANSFER_SIZE_1_BYTE == p_instance_ctrl->fifo_access_size)
    {
        uint8_t * p_dest8 = (uint8_t *) p_instance_ctrl->p_rx_dest;
        for (uint32_t i = 0; i < stages_to_read / 2; i++)
        {
            *p_dest8 = (uint8_t) (DAI->DAI_RX1_REG >> 24);
            p_dest8++;
            *p_dest8 = (uint8_t) (DAI->DAI_RX2_REG >> 24);
            p_dest8++;
        }
    }
#endif
}

/*******************************************************************************************************************//**
 * Calls user callback.
 *
 * @param[in]     p_ctrl     Pointer to I2S instance control block
 * @param[in]     event      Event code
 **********************************************************************************************************************/
static void r_i2s_w_call_callback (i2s_w_instance_ctrl_t * p_ctrl, i2s_event_t event)
{
    i2s_callback_args_t args;

    /* Store callback arguments in memory provided by user if available.  This allows callback arguments to be
     * stored in non-secure memory so they can be accessed by a non-secure callback function. */
    i2s_callback_args_t * p_args = p_ctrl->p_callback_memory;
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
 * Transmit ISR. Calls callback when transmission is complete.  Fills FIFO if transfer interface is not used.
 **********************************************************************************************************************/
void DAI_TX_Handler (void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE;

    IRQn_Type               irq             = R_FSP_CurrentIrqGet();
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag in the ICU */
    R_BSP_IrqClearPending(irq);

    if (NULL != p_instance_ctrl->p_tx_src)
    {
        /* If transfer is not used, write data. */
        r_i2s_w_fifo_write(p_instance_ctrl);
    }

    /* If there are more samples to write to the FIFO or the FIFO is above the watermark, don't call the callback. */
    if ((p_instance_ctrl->tx_src_samples == 0))
    {
        r_i2s_w_call_callback(p_instance_ctrl, I2S_EVENT_TX_EMPTY);
    }

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE;
}

/*******************************************************************************************************************//**
 * Receive ISR.  Calls callback when reception is complete.  Empties FIFO if transfer interface is not used.
 **********************************************************************************************************************/
void DAI_RX_Handler (void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE;

    IRQn_Type               irq             = R_FSP_CurrentIrqGet();
    i2s_w_instance_ctrl_t * p_instance_ctrl = (i2s_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag in the ICU */
    R_BSP_IrqClearPending(irq);

    bool call_callback = true;

    if (NULL != p_instance_ctrl->p_rx_dest)
    {
        /* If transfer is not used, read data into the destination buffer. */
        r_i2s_w_fifo_read(p_instance_ctrl);

        /* If there is more space in the buffer, don't call the callback. */
        if (p_instance_ctrl->rx_dest_samples > 0U)
        {
            call_callback = false;
        }
    }

    if (call_callback)
    {
        r_i2s_w_call_callback(p_instance_ctrl, I2S_EVENT_RX_FULL);
    }

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE;
}
