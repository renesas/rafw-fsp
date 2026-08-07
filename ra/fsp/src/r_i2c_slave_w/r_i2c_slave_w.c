/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**********************************************************************************************************************
 * Includes
 *********************************************************************************************************************/
#include "r_i2c_slave_w.h"
#if I2C_SLAVE_W_CFG_DTC_ENABLE
 #include "r_dtc_w.h"
#elif I2C_SLAVE_W_CFG_DMA_ENABLE
 #if BSP_FEATURE_DMAC_W_B
  #include "r_dmac_w_b.h"
 #elif BSP_FEATURE_DMAC_W
  #include "r_dmac_w.h"
 #endif
#endif

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

/* "I2CS" in ASCII, used to determine if channel is open. */
#define I2C_SLAVE_W_OPEN                         (0x49324353ULL)

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
 #define I2C_SLAVE_W_DTC_RX_TRANSFER_SETTINGS    ((TRANSFER_MODE_NORMAL << TRANSFER_SETTINGS_MODE_BITS) | \
                                                  (TRANSFER_SIZE_1_BYTE << TRANSFER_SETTINGS_SIZE_BITS) | \
                                                  (TRANSFER_ADDR_MODE_FIXED <<                            \
                                                   TRANSFER_SETTINGS_SRC_ADDR_BITS) |                     \
                                                  (TRANSFER_IRQ_END << TRANSFER_SETTINGS_IRQ_BITS) |      \
                                                  (TRANSFER_ADDR_MODE_INCREMENTED <<                      \
                                                   TRANSFER_SETTINGS_DEST_ADDR_BITS))
 #define I2C_SLAVE_W_DTC_TX_TRANSFER_SETTINGS    ((TRANSFER_MODE_NORMAL << TRANSFER_SETTINGS_MODE_BITS) | \
                                                  (TRANSFER_SIZE_2_BYTE << TRANSFER_SETTINGS_SIZE_BITS) | \
                                                  (TRANSFER_ADDR_MODE_INCREMENTED <<                      \
                                                   TRANSFER_SETTINGS_SRC_ADDR_BITS) |                     \
                                                  (TRANSFER_IRQ_END << TRANSFER_SETTINGS_IRQ_BITS) |      \
                                                  (TRANSFER_ADDR_MODE_FIXED <<                            \
                                                   TRANSFER_SETTINGS_DEST_ADDR_BITS))

#endif

#define I2C_SLAVE_W_ENABLE_LOOP_LIMIT            (0x100)
#define I2C_SLAVE_W_ENABLE_STATUS_INTERVAL       (0x200)

#if defined(CRG_COM_RESET_CLK_COM_REG_I2C_CLK_SEL_Msk)
 #define RESET_CLK_COM_REG                       CRG_COM->RESET_CLK_COM_REG
 #define SET_CLK_COM_REG                         CRG_COM->SET_CLK_COM_REG
 #define RESET_CLK_COM_I2C_CLK_SEL               CRG_COM_RESET_CLK_COM_REG_I2C_CLK_SEL_Msk
 #define SET_CLK_COM_I2C_CLK_SEL                 CRG_COM_SET_CLK_COM_REG_I2C_CLK_SEL_Msk
 #define RESET_CLK_COM_I2C_ENABLE                CRG_COM_RESET_CLK_COM_REG_I2C_ENABLE_Msk
 #define SET_CLK_COM_I2C_ENABLE                  CRG_COM_SET_CLK_COM_REG_I2C_ENABLE_Msk
#elif defined(CRG_PER_RESET_CLK_COM_REG_I2C_CLK_SEL_Msk)
 #define RESET_CLK_COM_REG                       CRG_PER->RESET_CLK_COM_REG
 #define SET_CLK_COM_REG                         CRG_PER->SET_CLK_COM_REG
 #define RESET_CLK_COM_I2C_CLK_SEL               CRG_PER_RESET_CLK_COM_REG_I2C_CLK_SEL_Msk
 #define SET_CLK_COM_I2C_CLK_SEL                 CRG_PER_SET_CLK_COM_REG_I2C_CLK_SEL_Msk
 #define RESET_CLK_COM_I2C_ENABLE                CRG_PER_RESET_CLK_COM_REG_I2C_ENABLE_Msk
 #define SET_CLK_COM_I2C_ENABLE                  CRG_PER_SET_CLK_COM_REG_I2C_ENABLE_Msk
#endif

/**********************************************************************************************************************
 * Typedef definitions
 *********************************************************************************************************************/
#if defined(__ARMCC_VERSION) || defined(__ICCARM__)
typedef void (BSP_CMSE_NONSECURE_CALL * i2c_slave_w_prv_ns_callback)(i2c_slave_callback_args_t * p_args);
#elif defined(__GNUC__)
typedef BSP_CMSE_NONSECURE_CALL void (*volatile i2c_slave_w_prv_ns_callback)(i2c_slave_callback_args_t * p_args);
#endif

/**********************************************************************************************************************
 * Private function prototypes
 *********************************************************************************************************************/

/* Internal helper functions */
static void      i2c_slave_w_notify(i2c_slave_w_instance_ctrl_t * const p_ctrl, i2c_slave_event_t const slave_event);
static fsp_err_t i2c_slave_w_read_write(i2c_slave_ctrl_t * const   p_api_ctrl,
                                        uint8_t * const            p_buffer,
                                        uint32_t const             bytes,
                                        i2c_slave_w_transfer_dir_t direction);
static void i2c_slave_w_call_callback(i2c_slave_w_instance_ctrl_t * p_ctrl,
                                      i2c_slave_event_t             event,
                                      uint32_t                      transaction_count);

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
static fsp_err_t i2c_slave_w_transfer_open(i2c_slave_cfg_t const * const p_cfg);
static fsp_err_t i2c_slave_w_transfer_configure(transfer_instance_t const * p_transfer,
                                                i2c_slave_w_transfer_dir_t  direction);

#endif

/* Functions that manipulate hardware. */
static fsp_err_t i2c_slave_w_open_hw_slave(i2c_slave_w_instance_ctrl_t * const p_ctrl);
static void      i2c_slave_w_enable_clk(uint8_t channel, bool select_divn);
static void      i2c_slave_w_disable_clk(uint8_t channel);
static fsp_err_t i2c_slave_w_disable(i2c_slave_w_instance_ctrl_t * const p_ctrl);
static void      i2c_slave_w_configure_clock_speed(i2c_slave_w_instance_ctrl_t * const  p_ctrl,
                                                   i2c_slave_w_clock_settings_t * const p_clk_settings);
static void i2c_slave_w_mode_address_set(i2c_slave_w_instance_ctrl_t * const p_ctrl);
static void i2c_slave_w_isr_write_buffer_handler(i2c_slave_w_instance_ctrl_t * p_ctrl, uint16_t event);
static void i2c_slave_w_isr_read_buffer_handler(i2c_slave_w_instance_ctrl_t * p_ctrl, i2c_slave_event_t event);

/* ISR function prototypes. */
void i2c_slave_w_gen_isr(void);

#if !I2C_SLAVE_W_CFG_GENERIC_ONLY
void i2c_slave_w_rxi_isr(void);
void i2c_slave_w_txi_isr(void);
void i2c_slave_w_tei_isr(void);

#endif

#if (I2C_SLAVE_W_CFG_DMA_ENABLE)
void i2c_slave_w_rx_dmac_callback(i2c_slave_w_instance_ctrl_t * const p_ctrl);
void i2c_slave_w_tx_dmac_callback(i2c_slave_w_instance_ctrl_t * const p_ctrl);

#endif

/**********************************************************************************************************************
 * Private global variables
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Global variables
 *********************************************************************************************************************/

/* I2C_W Implementation of I2C device slave interface. */
i2c_slave_api_t const g_i2c_slave_on_i2c_w =
{
    .open        = R_I2C_SLAVE_W_Open,
    .read        = R_I2C_SLAVE_W_Read,
    .write       = R_I2C_SLAVE_W_Write,
    .close       = R_I2C_SLAVE_W_Close,
    .callbackSet = R_I2C_SLAVE_W_CallbackSet
};

/*******************************************************************************************************************//**
 * @addtogroup I2C_SLAVE_W
 * @{
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Functions
 *********************************************************************************************************************/

/******************************************************************************************************************//**
 * Opens the I2C slave device.
 *
 * @param[in]  p_api_ctrl                     Pointer to control block.
 * @param[in]  p_cfg                          Pointer to I2C specific configuration structure.
 *
 * @retval  FSP_SUCCESS                       I2C slave device opened successfully.
 * @retval  FSP_ERR_ALREADY_OPEN              Module is already open.
 * @retval  FSP_ERR_IP_CHANNEL_NOT_PRESENT    Channel is not available on this MCU.
 * @retval  FSP_ERR_INVALID_HW_CONDITION      I2C's power domain is not enabled.
 * @retval  FSP_ERR_ASSERTION                 Parameter check failure due to one or more reasons below:
 *                                            1. p_api_ctrl or p_cfg is NULL.
 *                                            2. p_extend parameter is NULL.
 *                                            3. Invalid IRQ number assigned. Please refer to the documentation
 *                                               of I2C_SLAVE_W_CFG_GENERIC_ONLY
 *                                            4. Invalid driver configuration.
 * @retval  FSP_ERR_TIMEOUT                   Device is stuck & cannot be disabled. To recover from such an issue
 *                                            you can:
 *                                              - reset the SCLK & SDA pins,
 *                                              - disable PD_COM,
 *                                              - perform a HW reset.
 * @return  See @ref RENESAS_ERROR_CODES or functions called by this function for other possible return codes. This
 *          function calls: @ref transfer_api_t::open.
 *********************************************************************************************************************/
fsp_err_t R_I2C_SLAVE_W_Open (i2c_slave_ctrl_t * const p_api_ctrl, i2c_slave_cfg_t const * const p_cfg)
{
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) p_api_ctrl;

#if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_cfg->p_extend);

    i2c_slave_w_extended_cfg_t * p_extend = (i2c_slave_w_extended_cfg_t *) p_cfg->p_extend;
    FSP_ASSERT(p_extend->gen_irq >= (IRQn_Type) 0);
 #if !I2C_SLAVE_W_CFG_GENERIC_ONLY
    FSP_ASSERT(p_cfg->rxi_irq >= (IRQn_Type) 0);
    FSP_ASSERT(p_cfg->txi_irq >= (IRQn_Type) 0);
    FSP_ASSERT(p_cfg->tei_irq >= (IRQn_Type) 0);
 #endif
    FSP_ERROR_RETURN(BSP_FEATURE_I2C_VALID_CHANNEL_MASK & (1 << (p_cfg->channel + I2C_SLAVE_W_CHANNEL_OFFSET)),
                     FSP_ERR_IP_CHANNEL_NOT_PRESENT);
    FSP_ERROR_RETURN(I2C_SLAVE_W_OPEN != p_ctrl->open, FSP_ERR_ALREADY_OPEN);
 #if BSP_FEATURE_I2C_HAS_SEPARATE_PD
    FSP_ERROR_RETURN(bsp_pd_is_up_check(BSP_FEATURE_I2C_HAS_SEPARATE_PD), FSP_ERR_INVALID_HW_CONDITION);
 #endif
#endif

#if I2C_SLAVE_W_CFG_GENERIC_ONLY
    FSP_ASSERT(I2C_SLAVE_W_CFG_DTC_ENABLE == 0);
#endif

    fsp_err_t err = FSP_SUCCESS;

    p_ctrl->p_reg = (I2C_Type *) ((uint32_t) I2C + (p_cfg->channel * ((uint32_t) I2C2 - (uint32_t) I2C)));

    /* Record the configuration on the device for use later. */
    p_ctrl->p_cfg             = p_cfg;
    p_ctrl->p_callback        = p_cfg->p_callback;
    p_ctrl->p_context         = p_cfg->p_context;
    p_ctrl->p_callback_memory = NULL;

    /* Open the hardware in slave mode. Performs I2C initialization as described in hardware manual. */
    err = i2c_slave_w_open_hw_slave(p_ctrl);

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
    if (FSP_SUCCESS == err)
    {
        /* Open the I2C transfer interface if available. */
        err = i2c_slave_w_transfer_open(p_cfg);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
    }
#endif

    /* Finally, we can consider the device opened. */
    p_ctrl->p_buff            = NULL;
    p_ctrl->total             = 0U;
    p_ctrl->loaded            = 0U;
    p_ctrl->transaction_count = 0U;
    p_ctrl->open              = I2C_SLAVE_W_OPEN;
    p_ctrl->notify_request    = false;
    p_ctrl->direction         = I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED;

    return err;
}

/******************************************************************************************************************//**
 * Performs a read from the I2C Master device.
 *
 * This function will fail if there is already an in-progress I2C transfer on the associated channel. Otherwise, the
 * I2C slave read operation will begin. The caller will be notified when the operation has finished by an
 * I2C_SLAVE_EVENT_RX_COMPLETE in the callback.
 * In case the master continues to write more data, an I2C_SLAVE_EVENT_RX_MORE_REQUEST will be issued via callback.
 * In case of errors, an I2C_SLAVE_EVENT_ABORTED will be issued via callback.
 *
 * @param[in]   p_api_ctrl          Pointer to control block.
 * @param[out]  p_dest              Pointer to the destination buffer.
 * @param[in]   bytes               Number of bytes to be read.
 *
 * @retval  FSP_SUCCESS             Function executed without issue.
 * @retval  FSP_ERR_ASSERTION       p_api_ctrl, p_dest or p_callback is NULL.
 * @retval  FSP_ERR_IN_USE          Another transfer was in progress.
 * @retval  FSP_ERR_NOT_OPEN        Device is not open.
 * @retval  FSP_ERR_INVALID_SIZE    Invalid size when reading data via DTC.
 *********************************************************************************************************************/
fsp_err_t R_I2C_SLAVE_W_Read (i2c_slave_ctrl_t * const p_api_ctrl, uint8_t * const p_dest, uint32_t const bytes)
{
    fsp_err_t err = FSP_SUCCESS;

    err = i2c_slave_w_read_write(p_api_ctrl, p_dest, bytes, I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ);

    return err;
}

/******************************************************************************************************************//**
 * Performs a write to the I2C Master device.
 *
 * This function will fail if there is already an in-progress I2C transfer on the associated channel. Otherwise, the
 * I2C slave write operation will begin. The caller will be notified when the operation has finished by an
 * I2C_SLAVE_EVENT_TX_COMPLETE in the callback.
 * In case the master continues to read more data, an I2C_SLAVE_EVENT_TX_MORE_REQUEST will be issued via callback.
 * In case of errors, an I2C_SLAVE_EVENT_ABORTED will be issued via callback.
 *
 * @param[in]  p_api_ctrl           Pointer to control block.
 * @param[in]  p_src                Pointer to the source buffer.
 * @param[in]  bytes                Number of bytes to be write.
 *
 * @retval  FSP_SUCCESS             Function executed without issue.
 * @retval  FSP_ERR_ASSERTION       p_api_ctrl, p_src or p_callback is NULL.
 * @retval  FSP_ERR_IN_USE          Another transfer was in progress.
 * @retval  FSP_ERR_NOT_OPEN        Device is not open.
 * @retval  FSP_ERR_INVALID_SIZE    Invalid size when writing data via DTC.
 *********************************************************************************************************************/
fsp_err_t R_I2C_SLAVE_W_Write (i2c_slave_ctrl_t * const p_api_ctrl, uint8_t * const p_src, uint32_t const bytes)
{
    fsp_err_t err = FSP_SUCCESS;

    err = i2c_slave_w_read_write(p_api_ctrl, p_src, bytes, I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE);

    return err;
}

/*******************************************************************************************************************//**
 * Updates the user callback and has option of providing memory for callback structure.
 * Implements i2c_slave_api_t::callbackSet
 *
 * @param[in]  p_api_ctrl                 Pointer to control block.
 * @param[in]  p_callback                 Pointer to the callback.
 * @param[in]  p_context                  Pointer to context.
 * @param[in]  p_callback_memory          Pointer to the callback memory.
 *
 * @retval  FSP_SUCCESS                  Callback updated successfully.
 * @retval  FSP_ERR_ASSERTION            A required pointer is NULL.
 * @retval  FSP_ERR_NOT_OPEN             The control block has not been opened.
 * @retval  FSP_ERR_NO_CALLBACK_MEMORY   p_callback is non-secure and p_callback_memory is either secure or NULL.
 **********************************************************************************************************************/
fsp_err_t R_I2C_SLAVE_W_CallbackSet (i2c_slave_ctrl_t * const          p_api_ctrl,
                                     void (                          * p_callback)(i2c_slave_callback_args_t *),
                                     void * const                      p_context,
                                     i2c_slave_callback_args_t * const p_callback_memory)
{
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) p_api_ctrl;

#if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_ctrl);
    FSP_ASSERT(p_callback);
    FSP_ERROR_RETURN(I2C_SLAVE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

#if BSP_TZ_SECURE_BUILD

    /* Get security state of p_callback. */
    bool callback_is_secure =
        (NULL == cmse_check_address_range((void *) p_callback, sizeof(void *), CMSE_AU_NONSECURE));

 #if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE

    /* In secure projects, p_callback_memory must be provided in non-secure space if p_callback is non-secure. */
    i2c_slave_callback_args_t * const p_callback_memory_checked = cmse_check_pointed_object(p_callback_memory,
                                                                                            CMSE_AU_NONSECURE);
    FSP_ERROR_RETURN(callback_is_secure || (NULL != p_callback_memory_checked), FSP_ERR_NO_CALLBACK_MEMORY);
 #endif
#endif

    /* Store callback and context. */
#if BSP_TZ_SECURE_BUILD
    p_ctrl->p_callback = callback_is_secure ? p_callback :
                         (void (*)(i2c_slave_callback_args_t *))cmse_nsfptr_create(p_callback);
#else
    p_ctrl->p_callback = p_callback;
#endif
    p_ctrl->p_context         = p_context;
    p_ctrl->p_callback_memory = p_callback_memory;

    return FSP_SUCCESS;
}

/******************************************************************************************************************//**
 * Closes the I2C device.
 *
 * @param[in]  p_api_ctrl       Pointer to control block.
 *
 * @retval  FSP_SUCCESS         Device closed successfully.
 * @retval  FSP_ERR_NOT_OPEN    Device not opened.
 * @retval  FSP_ERR_ASSERTION   p_api_ctrl is NULL.
 * @retval  FSP_ERR_TIMEOUT     Device is stuck & cannot be disabled. To recover from such an issue you can:
 *                                  - reset the SCLK & SDA pins,
 *                                  - disable PD_COM,
 *                                  - perform a HW reset.
 *********************************************************************************************************************/
fsp_err_t R_I2C_SLAVE_W_Close (i2c_slave_ctrl_t * const p_api_ctrl)
{
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) p_api_ctrl;

#if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);

    /* Check if the device is even open, return an error if not. */
    FSP_ERROR_RETURN(I2C_SLAVE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    fsp_err_t err = FSP_SUCCESS;

    /* The device is now considered closed. */
    p_ctrl->open = 0U;

    /* Mask all events both for Generic and dedicated interrupts. */
    p_ctrl->p_reg->I2C_INTR_MASK_REG = 0;
#if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
    p_ctrl->p_reg->I2C_MASK_REG = 0;
#endif

    /* Disable I2C controller. */
    err = i2c_slave_w_disable(p_ctrl);

    /* Disable I2C clock. */
    i2c_slave_w_disable_clk(p_ctrl->p_cfg->channel);

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)

    /* Close the handles for the transfer interfaces. */
    if (NULL != p_ctrl->p_cfg->p_transfer_rx)
    {
        p_ctrl->p_cfg->p_transfer_rx->p_api->close(p_ctrl->p_cfg->p_transfer_rx->p_ctrl);
 #if I2C_SLAVE_W_CFG_DTC_ENABLE
        R_BSP_IrqDisable(p_ctrl->p_cfg->rxi_irq);
 #endif
    }

    if (NULL != p_ctrl->p_cfg->p_transfer_tx)
    {
        p_ctrl->p_cfg->p_transfer_tx->p_api->close(p_ctrl->p_cfg->p_transfer_tx->p_ctrl);
 #if I2C_SLAVE_W_CFG_DTC_ENABLE
        R_BSP_IrqDisable(p_ctrl->p_cfg->txi_irq);
 #endif
    }
#endif

    i2c_slave_w_extended_cfg_t * p_extend = (i2c_slave_w_extended_cfg_t *) p_ctrl->p_cfg->p_extend;

    /* Disable all interrupts in NVIC. */
    R_BSP_IrqDisable(p_extend->gen_irq);

    return err;
}

/******************************************************************************************************************//**
 * @} (end addtogroup I2C_SLAVE_W)
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Helper function for handling I2C Slave Read or Write.
 *
 * @param[in]  p_api_ctrl         Pointer to the control structure.
 * @param[in]  p_buffer           Pointer to the buffer to store read/write data.
 * @param[in]  bytes              Number of bytes to be read/written.
 * @param[in]  direction          Slave Read or Slave Write.
 *
 * @retval  FSP_SUCCESS           Function executed successfully.
 * @retval  FSP_ERR_ASSERTION     p_api_ctrl, p_buffer or p_callback is NULL.
 * @retval  FSP_ERR_INVALID_SIZE  Provided number of bytes more than the configured TX buffer size or more than
 *                                UINT16_MAX(= 65535) while DTC/DMAC is used for data transfer.
 * @retval  FSP_ERR_IN_USE        Another transfer was in progress.
 * @retval  FSP_ERR_NOT_OPEN      Handle is not initialized. Call R_I2C_SLAVE_W_Open to initialize the control block.
 **********************************************************************************************************************/
static fsp_err_t i2c_slave_w_read_write (i2c_slave_ctrl_t * const   p_api_ctrl,
                                         uint8_t * const            p_buffer,
                                         uint32_t const             bytes,
                                         i2c_slave_w_transfer_dir_t direction)
{
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) p_api_ctrl;

#if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_buffer);

    /* Check if the device is even open, return an error if not. */
    FSP_ERROR_RETURN(I2C_SLAVE_W_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Fail if there is already a transfer in progress. */
    FSP_ERROR_RETURN(I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED == p_ctrl->direction, FSP_ERR_IN_USE);

    /* A Callback shall be present in order to handle Master's requests by calling the appropriate Read/Write APIs. */
    FSP_ASSERT(((i2c_slave_w_instance_ctrl_t *) p_api_ctrl)->p_callback != NULL);
 #if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
    FSP_ERROR_RETURN((bytes <= UINT16_MAX), FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN((bytes <= I2C_SLAVE_W_CFG_TRANSFER_API_TX_BUFFER_SIZE), FSP_ERR_INVALID_SIZE);
 #endif
#endif

    /* Record the new information about this transfer. */
    p_ctrl->p_buff    = p_buffer;
    p_ctrl->total     = bytes;
    p_ctrl->direction = direction;

    /* Initialize fields used during transfer. */
    p_ctrl->loaded = 0U;

    return FSP_SUCCESS;
}

/******************************************************************************************************************//**
 * Single point for managing the logic around notifying a transfer has finished.
 *
 * @param[in]       p_ctrl            Pointer to I2C specific control structure.
 * @param[in]       slave_event       The slave event code to pass to the callback.
 *********************************************************************************************************************/
static void i2c_slave_w_notify (i2c_slave_w_instance_ctrl_t * const p_ctrl, i2c_slave_event_t const slave_event)
{
#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
    transfer_properties_t transaction_property;

    /* Check if there are remaining bytes to be transferred & Disable the transfer instance. */
    const transfer_instance_t * p_transfer_tx = p_ctrl->p_cfg->p_transfer_tx;
    if ((NULL != p_transfer_tx) && (I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE == p_ctrl->direction))
    {
        p_ctrl->p_cfg->p_transfer_tx->p_api->infoGet(p_ctrl->p_cfg->p_transfer_tx->p_ctrl, &transaction_property);
        p_ctrl->transaction_count -= transaction_property.transfer_length_remaining;

        p_transfer_tx->p_api->disable(p_transfer_tx->p_ctrl);

        /* Re-enable the READ REQUEST event in the Generic interrupt */
        p_ctrl->p_reg->I2C_INTR_MASK_REG |= I2C_SLAVE_W_INT_READ_REQUEST;
    }

    const transfer_instance_t * p_transfer_rx = p_ctrl->p_cfg->p_transfer_rx;
    if ((NULL != p_transfer_rx) && (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ == p_ctrl->direction))
    {
        p_ctrl->p_cfg->p_transfer_rx->p_api->infoGet(p_ctrl->p_cfg->p_transfer_rx->p_ctrl, &transaction_property);
        p_ctrl->transaction_count -= transaction_property.transfer_length_remaining;

        p_transfer_rx->p_api->disable(p_transfer_rx->p_ctrl);
 #if I2C_SLAVE_W_CFG_GENERIC_ONLY

        /* Re-enable the RX FULL event in the Generic interrupt */
        p_ctrl->p_reg->I2C_INTR_MASK_REG |= I2C_SLAVE_W_INT_RX_FULL;
 #endif
    }
#endif

    /* Indicate that the currect transfer completed. */
    p_ctrl->notify_request = false;
    p_ctrl->direction      = I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED;

    /* Invoke the callback. */
    i2c_slave_w_call_callback(p_ctrl, slave_event, p_ctrl->transaction_count);
}

/******************************************************************************************************************//**
 * Performs the hardware initialization sequence when operating as a slave.
 *
 * @param[in]       p_ctrl     Pointer to I2C specific control structure.
 *
 * @retval  FSP_SUCCESS        I2C slave device opened successfully.
 *********************************************************************************************************************/
static fsp_err_t i2c_slave_w_open_hw_slave (i2c_slave_w_instance_ctrl_t * const p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    i2c_slave_w_extended_cfg_t * p_extend = (i2c_slave_w_extended_cfg_t *) p_ctrl->p_cfg->p_extend;

    /* Enable I2C clock. */
    i2c_slave_w_enable_clk(p_ctrl->p_cfg->channel, p_extend->select_divn);

    /* Disable I2C controller. */
    err = i2c_slave_w_disable(p_ctrl);

    /* Configure clock settings & communication speed. */
    i2c_slave_w_configure_clock_speed(p_ctrl, &p_extend->clock_settings);

    /* Set slave mode, address & configure addressing mode. */
    i2c_slave_w_mode_address_set(p_ctrl);

    /* Enable the slave address and general call based on user config */
    p_ctrl->p_reg->I2C_ACK_GENERAL_CALL_REG = (uint32_t) p_ctrl->p_cfg->general_call_enable;

    /* Set the RX FIFO threshold to 1 entry. */
    p_ctrl->p_reg->I2C_RX_TL_REG = 0;

#if (BSP_FEATURE_I2C_VERSION == 2)

    /* Configure bus holding feature when RX FIFO is full. */
    p_ctrl->p_reg->I2C_CON_REG_b.I2C_RX_FIFO_FULL_HLD_CTRL = p_extend->rx_fifo_full_hld;
#endif

    /* Enable interrupts. */
    uint16_t mask = I2C_SLAVE_W_INT_RX_OVERFLOW | I2C_SLAVE_W_INT_READ_REQUEST | I2C_SLAVE_W_INT_TX_ABORT |
#if I2C_SLAVE_W_CFG_GENERIC_ONLY
                    I2C_SLAVE_W_INT_RX_FULL |
#endif
#if (BSP_FEATURE_I2C_VERSION == 1)
                    I2C_SLAVE_W_INT_START_DETECTED;
#elif (BSP_FEATURE_I2C_VERSION == 2)
                    I2C_SLAVE_W_INT_RESTART_DETECTED;
#endif

    /* Enable Generic interrupt events. */
    p_ctrl->p_reg->I2C_INTR_MASK_REG = mask;

#if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
 #if !I2C_SLAVE_W_CFG_GENERIC_ONLY

    /* Mask dedicated events except for RX Full event. */
    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #else

    /* Mask dedicated events. */
    p_ctrl->p_reg->I2C_MASK_REG = 0;
 #endif
#endif

#if I2C_SLAVE_W_CFG_DTC_ENABLE
    if (NULL != p_ctrl->p_cfg->p_transfer_tx)
    {
        /* If DTC is used, configure TX FIFO threshold to "completely empty". */
        p_ctrl->p_reg->I2C_TX_TL_REG = 0;
    }
#endif

    /* Set valid interrupt contexts and user provided priority. */
    R_BSP_IrqCfgEnable(p_extend->gen_irq, p_extend->gen_ipl, p_ctrl);
#if !I2C_SLAVE_W_CFG_GENERIC_ONLY
    R_BSP_IrqCfgEnable(p_ctrl->p_cfg->rxi_irq, p_ctrl->p_cfg->ipl, p_ctrl);
    R_BSP_IrqCfgEnable(p_ctrl->p_cfg->txi_irq, p_ctrl->p_cfg->ipl, p_ctrl);
    R_BSP_IrqCfgEnable(p_ctrl->p_cfg->tei_irq, p_ctrl->p_cfg->ipl, p_ctrl);
#endif

    /* Enable the I2C Controller. */
    p_ctrl->p_reg->I2C_ENABLE_REG_b.I2C_EN = 1;

    return err;
}

/*******************************************************************************************************************//**
 * Enables I2C clock.
 *
 * @param [in] channel        I2C selected channel.
 * @param [in] select_divn    I2C selected DIVN.
 **********************************************************************************************************************/
static void i2c_slave_w_enable_clk (uint8_t channel, bool select_divn)
{
    if (select_divn)
    {
        RESET_CLK_COM_REG = RESET_CLK_COM_I2C_CLK_SEL << (channel * 2);
        SET_CLK_COM_REG   = SET_CLK_COM_I2C_ENABLE << (channel * 2);
    }
    else
    {
        SET_CLK_COM_REG = (SET_CLK_COM_I2C_CLK_SEL << (channel * 2)) |
                          (SET_CLK_COM_I2C_ENABLE << (channel * 2));
    }
}

/*******************************************************************************************************************//**
 * Disables I2C clock.
 *
 * @param [in] channel        I2C selected channel.
 **********************************************************************************************************************/
static void i2c_slave_w_disable_clk (uint8_t channel)
{
    RESET_CLK_COM_REG = RESET_CLK_COM_I2C_ENABLE << (channel * 2);
}

/*******************************************************************************************************************//**
 * Disables I2C.
 *
 * @param[in]  p_ctrl                Pointer to I2C specific control structure.
 **********************************************************************************************************************/
static fsp_err_t i2c_slave_w_disable (i2c_slave_w_instance_ctrl_t * const p_ctrl)
{
    /* Disable I2C controller. */
    p_ctrl->p_reg->I2C_ENABLE_REG_b.I2C_EN = 0;

    /* Wait until controller is disabled. */
    uint32_t times = 0;
    while (1 == p_ctrl->p_reg->I2C_ENABLE_STATUS_REG_b.IC_EN)
    {
        /* We shouldn't get stuck here, the HW I2C block should eventually be disabled. */
        FSP_ERROR_RETURN((times++ < I2C_SLAVE_W_ENABLE_LOOP_LIMIT), FSP_ERR_TIMEOUT);
        R_BSP_SoftwareDelay(I2C_SLAVE_W_ENABLE_STATUS_INTERVAL, BSP_DELAY_UNITS_MICROSECONDS);
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Configures I2C slave's clock settings and transaction speed.
 *
 * @param[in]  p_ctrl                Pointer to I2C specific control structure.
 * @param[in]  p_clk_settings        Pointer to I2C specific clock settings structure.
 **********************************************************************************************************************/
static void i2c_slave_w_configure_clock_speed (i2c_slave_w_instance_ctrl_t * const  p_ctrl,
                                               i2c_slave_w_clock_settings_t * const p_clk_settings)
{
    /* Data set-up time setting.
     * Adding 1 because the HW IP calculates the data set-up time as (I2C_SDA_SETUP_REG - 1)*ic_clk_period.
     */
    p_ctrl->p_reg->I2C_SDA_SETUP_REG = (uint32_t) (p_clk_settings->sda_setup_counts + 1);

    /* Data hold time setting. */
    p_ctrl->p_reg->I2C_SDA_HOLD_REG =
        (uint32_t) ((p_clk_settings->sda_hold_rx_counts << I2C_I2C_SDA_HOLD_REG_I2C_SDA_RX_HOLD_Pos) |
                    p_clk_settings->sda_hold_tx_counts);

    /* Configure speed & spike suppression length. */
    switch (p_ctrl->p_cfg->rate)
    {
        case I2C_SLAVE_RATE_HIGHSPEED:
        {
            p_ctrl->p_reg->I2C_IC_HS_SPKLEN_REG    = p_clk_settings->digital_filter_stages;
            p_ctrl->p_reg->I2C_CON_REG_b.I2C_SPEED = 3;
            break;
        }

        case I2C_SLAVE_RATE_FASTPLUS:
        case I2C_SLAVE_RATE_FAST:
        {
            p_ctrl->p_reg->I2C_IC_FS_SPKLEN_REG    = p_clk_settings->digital_filter_stages;
            p_ctrl->p_reg->I2C_CON_REG_b.I2C_SPEED = 2;
            break;
        }

        case I2C_SLAVE_RATE_STANDARD:
        default:
        {
            p_ctrl->p_reg->I2C_IC_FS_SPKLEN_REG    = p_clk_settings->digital_filter_stages;
            p_ctrl->p_reg->I2C_CON_REG_b.I2C_SPEED = 1;
            break;
        }
    }
}

/*******************************************************************************************************************//**
 * Sets up an I2C slave device (mode, address, addressing mode).
 *
 * @param[in]  p_ctrl                Pointer to I2C specific control structure.
 **********************************************************************************************************************/
static void i2c_slave_w_mode_address_set (i2c_slave_w_instance_ctrl_t * const p_ctrl)
{
    uint32_t tmp = p_ctrl->p_reg->I2C_CON_REG;

    /* Set slave mode. */
    FSP_REG_VAR_FIELD_SET(I2C, I2C_CON_REG, I2C_MASTER_MODE, tmp, 0);
    FSP_REG_VAR_FIELD_SET(I2C, I2C_CON_REG, I2C_SLAVE_DISABLE, tmp, 0);

    /* Set slave addressing_mode. */
    FSP_REG_VAR_FIELD_SET(I2C, I2C_CON_REG, I2C_10BITADDR_SLAVE, tmp,
                          ((I2C_SLAVE_ADDR_MODE_10BIT == p_ctrl->p_cfg->addr_mode) ? 1 : 0));

    p_ctrl->p_reg->I2C_CON_REG = tmp;

    /* Set slave address. */
    p_ctrl->p_reg->I2C_SAR_REG = p_ctrl->p_cfg->slave & I2C_I2C_SAR_REG_IC_SAR_Msk;
}

/*******************************************************************************************************************//**
 * Calls user callback.
 *
 * @param[in]     p_ctrl                 Pointer to I2C specific control structure.
 * @param[in]     event                  Event code.
 * @param[in]     transaction_count      Transaction count for I2C slave.
 **********************************************************************************************************************/
static void i2c_slave_w_call_callback (i2c_slave_w_instance_ctrl_t * p_ctrl,
                                       i2c_slave_event_t             event,
                                       uint32_t                      transaction_count)
{
    i2c_slave_callback_args_t args;

    /* Store callback arguments in memory provided by user if available.  This allows callback arguments to be
     * stored in non-secure memory so they can be accessed by a non-secure callback function. */
    i2c_slave_callback_args_t * p_args = p_ctrl->p_callback_memory;
    if (NULL == p_args)
    {
        /* Store on stack. */
        p_args = &args;
    }
    else
    {
        /* Save current arguments on the stack in case this is a nested interrupt. */
        args = *p_args;
    }

    p_args->bytes     = transaction_count;
    p_args->event     = event;
    p_args->p_context = p_ctrl->p_context;

#if BSP_TZ_SECURE_BUILD

    /* p_callback can point to a secure function or a non-secure function. */
    if (!cmse_is_nsfptr(p_ctrl->p_callback))
    {
        /* If p_callback is secure, then the project does not need to change security state. */
        p_ctrl->p_callback(p_args);
    }
    else
    {
        /* If p_callback is Non-secure, then the project must change to Non-secure state in order to
         * call the callback. */
        i2c_slave_w_prv_ns_callback p_callback = (i2c_slave_w_prv_ns_callback) (p_ctrl->p_callback);
        p_callback(p_args);
    }

#else

    /* If the project is not Trustzone Secure, then it will never need to change security state in
     * order to call the callback. */
    p_ctrl->p_callback(p_args);
#endif
    if (NULL != p_ctrl->p_callback_memory)
    {
        /* Restore callback memory in case this is a nested interrupt. */
        *p_ctrl->p_callback_memory = args;
    }
}

/*******************************************************************************************************************//**
 * Setup the Slave Read/Write transaction by issuing RX Request or TX request to the application via callback.
 *
 * @param[in]  p_ctrl          Pointer to I2C specific control structure.
 * @param[in]  slave_event     Slave event to be reported via callback.
 **********************************************************************************************************************/
static void i2c_slave_w_initiate_transaction (i2c_slave_w_instance_ctrl_t * p_ctrl, i2c_slave_event_t slave_event)
{
    /* Set the status flag to ensure this conditional clause execution only once. */
    p_ctrl->notify_request = true;

    /* Clear and enable the STOP detection interrupt source after the slave has been addressed
     * to avoid the detection of a STOP condition being raised for another slave on the bus.
     */
    (void) p_ctrl->p_reg->I2C_CLR_STOP_DET_REG;
    p_ctrl->p_reg->I2C_INTR_MASK_REG |= I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk;

    /* Invoke callback for the user to call a valid API. */
    p_ctrl->direction = I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED;

    /* Invoke the callback to notify the read request.
     * The application must call MasterWriteSlaveRead API in the callback.*/
    i2c_slave_w_call_callback(p_ctrl, slave_event, p_ctrl->transaction_count);
}

/*******************************************************************************************************************//**
 * Performs an I2C write transmission.
 *
 * @param [in] p_ctrl    Pointer to I2C specific control structure.
 * @param [in] event     Event from Interrupt Status Register.
 *
 **********************************************************************************************************************/
static void i2c_slave_w_isr_write_buffer_handler (i2c_slave_w_instance_ctrl_t * p_ctrl, uint16_t event)
{
    /* Check if the write request event has been notified through callback, if not provide the callback. */
    if (!p_ctrl->notify_request)
    {
        /* If slave has exhausted the buffer length from application, slave has to notify this event
         * to application via callback. */
        if ((p_ctrl->total == p_ctrl->loaded) && p_ctrl->total)
        {
            event = I2C_SLAVE_EVENT_TX_MORE_REQUEST;
        }

        i2c_slave_w_initiate_transaction(p_ctrl, (i2c_slave_event_t) event);
    }

    /* If MasterReadSlaveWrite API is invoked, proceed writing data. */
    if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE != p_ctrl->direction)
    {
        /* MasterReadSlaveWrite API was not called in the callback.
         * Master will read 0xFF for all the byte(s) for this transaction.
         */
    }
    else
    {
#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
        if ((NULL != p_ctrl->p_cfg->p_transfer_tx) && (p_ctrl->total > 0U) && (p_ctrl->loaded == 0U))
        {
            transfer_instance_t * p_transfer_tx = (transfer_instance_t *) p_ctrl->p_cfg->p_transfer_tx;

            /* Mask READ REQUEST event in Generic Irq. */
            uint32_t mask = (p_ctrl->p_reg->I2C_INTR_MASK_REG & ~(uint32_t) (I2C_SLAVE_W_INT_READ_REQUEST));
            p_ctrl->p_reg->I2C_INTR_MASK_REG = mask;

            while (p_ctrl->loaded < p_ctrl->total)
            {
                /* Add 0 to the 8th bit (CMD) to indicate a write transfer. */
                p_ctrl->p_transfer_api_tx_buff[p_ctrl->loaded] =
                    (uint16_t) (p_ctrl->p_buff[p_ctrl->loaded] & I2C_I2C_DATA_CMD_REG_I2C_DAT_Msk);
                p_ctrl->loaded++;
                p_ctrl->transaction_count++;
            }

            uint32_t volatile const * p_i2c_slave_w_tx_dest_buf = &(p_ctrl->p_reg->I2C_DATA_CMD_REG);
 #if I2C_SLAVE_W_CFG_DTC_ENABLE
            p_transfer_tx->p_api->reset(p_transfer_tx->p_ctrl, (void *) (p_ctrl->p_transfer_api_tx_buff),
                                        (uint16_t *) (p_i2c_slave_w_tx_dest_buf), (uint16_t) (p_ctrl->total));
 #else

            /* Make sure I2C DMA is off so it's not unexpectedly triggered when channels are enabled. */
            p_ctrl->p_reg->I2C_DMA_CR_REG = 0;

            /* Configure TX DMA Channel and TX FIFO threshold level for I3C. */
            i2c_slave_w_extended_cfg_t * p_extend = (i2c_slave_w_extended_cfg_t *) p_ctrl->p_cfg->p_extend;

            if ((p_ctrl->total < 4) || !(p_extend->enable_dma_bursts_tx))
            {
                p_transfer_tx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_DISABLED;
                p_ctrl->p_reg->I2C_DMA_TDLR_REG = p_ctrl->total;
            }
            else if (p_ctrl->total < 8)
            {
                p_transfer_tx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_4X;
                p_ctrl->p_reg->I2C_DMA_TDLR_REG = p_ctrl->total;
            }
            else
            {
                p_transfer_tx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_8X;
                p_ctrl->p_reg->I2C_DMA_TDLR_REG = I2C_SLAVE_W_FIFO_DEPTH - 8;
            }

            p_transfer_tx->p_cfg->p_info->p_src  = (uint16_t *) p_ctrl->p_transfer_api_tx_buff;
            p_transfer_tx->p_cfg->p_info->p_dest = (uint16_t *) p_i2c_slave_w_tx_dest_buf;
            p_transfer_tx->p_cfg->p_info->length = (uint16_t) p_ctrl->total;

            p_transfer_tx->p_api->reconfigure(p_transfer_tx->p_ctrl, p_transfer_tx->p_cfg->p_info);
            p_transfer_tx->p_api->enable(p_transfer_tx->p_ctrl);

            p_ctrl->p_reg->I2C_DMA_CR_REG_b.TDMAE = 1;
 #endif
 #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

            /* Unmask the TX Empty dedicated event. */
            p_ctrl->p_reg->I2C_MASK_REG_b.I2C_TXE_IRQ_MASK = 1;
 #endif
        }
        else
#endif
        {
#if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

            /* Unmask the TX Empty dedicated event. */
            p_ctrl->p_reg->I2C_MASK_REG_b.I2C_TXE_IRQ_MASK = 1;
#endif

#if I2C_SLAVE_W_CFG_GENERIC_ONLY
            if (p_ctrl->p_reg->I2C_STATUS_REG_b.TFNF)
            {
                /* Write byte to TX FIFO, this will also release SCL. */
                p_ctrl->p_reg->I2C_DATA_CMD_REG = p_ctrl->p_buff[p_ctrl->loaded++];
                p_ctrl->transaction_count++;
            }

            /* If the requested bytes have been written, notify the application. */
            if (p_ctrl->loaded == p_ctrl->total)
            {
                /* Notify anyone waiting that the transfer is completed. */
                i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_TX_COMPLETE);
 #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

                /* Enable the Transmit End dedicated event. */
                p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
 #endif
            }
#endif
        }
    }

    /* Clear read request. */
    p_ctrl->p_reg->I2C_CLR_RD_REQ_REG;
}

/*******************************************************************************************************************//**
 * Performs an I2C read transmission.
 *
 * @param [in] p_ctrl    Pointer to I2C specific control structure.
 * @param [in] event     Event from Interrupt Status Register.
 *
 **********************************************************************************************************************/
static void i2c_slave_w_isr_read_buffer_handler (i2c_slave_w_instance_ctrl_t * p_ctrl, i2c_slave_event_t event)
{
    /* Check if the read request event has been notified through callback, if not provide the callback. */
    if (!p_ctrl->notify_request)
    {
        /* Check if this is a General Call by Master */
        if (p_ctrl->p_reg->I2C_RAW_INTR_STAT_REG & I2C_I2C_INTR_STAT_REG_R_GEN_CALL_Msk)
        {
            event = I2C_SLAVE_EVENT_GENERAL_CALL;
            (void) p_ctrl->p_reg->I2C_CLR_GEN_CALL_REG;
        }
        /* If slave has exhausted the buffer length from application, slave has to notify this event
         * to application via callback. */
        else if ((p_ctrl->total == p_ctrl->loaded) && p_ctrl->total)
        {
            event = I2C_SLAVE_EVENT_RX_MORE_REQUEST;
        }

        i2c_slave_w_initiate_transaction(p_ctrl, event);
    }

    /* Proceed reading data */
    if ((I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ != p_ctrl->direction) || (0U == p_ctrl->total))
    {
        /* If the user application incorrectly handles Master Write, NACK the request by disabling the controller. */
        i2c_slave_w_disable(p_ctrl);

        /* Re-enable the controller and notify the application. */
        p_ctrl->p_reg->I2C_ENABLE_REG_b.I2C_EN = 1;

        /* If application incorrectly handles the transaction, then the transaction is consider aborted.
         * If application intentionally NACKs the Master's Write request, then the transaction is consider completed. */
        i2c_slave_event_t resp_event =
            (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ !=
             p_ctrl->direction) ? I2C_SLAVE_EVENT_ABORTED : I2C_SLAVE_EVENT_RX_COMPLETE;
        i2c_slave_w_notify(p_ctrl, resp_event);

        /* Since I2C is disabled, the STOP_DET interrupt does not occur, so the transaction count is cleared here. */
        p_ctrl->total             = 0U;
        p_ctrl->transaction_count = 0U;
    }
    else
    {
#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
        if ((NULL != p_ctrl->p_cfg->p_transfer_rx) && (p_ctrl->total > 0U))
        {
            transfer_instance_t * p_transfer_rx = (transfer_instance_t *) p_ctrl->p_cfg->p_transfer_rx;
 #if I2C_SLAVE_W_CFG_GENERIC_ONLY

            /* Mask RX Full event in Generic Irq. */
            uint32_t mask = (p_ctrl->p_reg->I2C_INTR_MASK_REG & ~(uint32_t) (I2C_SLAVE_W_INT_RX_FULL));
            p_ctrl->p_reg->I2C_INTR_MASK_REG = mask;
 #elif I2C_SLAVE_W_CFG_DMA_ENABLE
            p_ctrl->p_reg->I2C_MASK_REG = 0;
 #endif

            uint32_t volatile const * p_i2c_slave_w_rx_src_buffer = &(p_ctrl->p_reg->I2C_DATA_CMD_REG);
 #if I2C_SLAVE_W_CFG_DTC_ENABLE

            /* Read data until we go below the interrupt generation threshold */
            while (p_ctrl->p_reg->I2C_RXFLR_REG_b.RXFLR && (p_ctrl->loaded < p_ctrl->total))
            {
                /* Get received byte from RX fifo. */
                p_ctrl->transaction_count++;
                p_ctrl->p_buff[p_ctrl->loaded++] = p_ctrl->p_reg->I2C_DATA_CMD_REG_b.I2C_DAT;
            }

            /* If there are any bytes left for DTC, enable the transfer */
            if (p_ctrl->loaded < p_ctrl->total)
            {
                p_transfer_rx->p_api->reset(p_transfer_rx->p_ctrl, (uint8_t *) (p_i2c_slave_w_rx_src_buffer),
                                            (void *) (p_ctrl->p_buff + p_ctrl->loaded),
                                            (uint16_t) (p_ctrl->total - p_ctrl->loaded));
            }

 #else

            /* Make sure I2C DMA is off so it's not unexpectedly triggered when channels are enabled. */
            p_ctrl->p_reg->I2C_DMA_CR_REG = 0;

            /* Configure RX DMA Channel and RX FIFO threshold level for I2C Slave. */
            i2c_slave_w_extended_cfg_t * p_extend = (i2c_slave_w_extended_cfg_t *) p_ctrl->p_cfg->p_extend;

            if ((0 == (p_ctrl->total % 8)) && p_extend->enable_dma_bursts_rx)
            {
                p_transfer_rx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_8X;
                p_ctrl->p_reg->I2C_DMA_RDLR_REG = 7;
            }
            else if ((0 == (p_ctrl->total % 4)) && p_extend->enable_dma_bursts_rx)
            {
                p_transfer_rx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_4X;
                p_ctrl->p_reg->I2C_DMA_RDLR_REG = 3;
            }
            else
            {
                p_transfer_rx->p_cfg->p_info->transfer_settings_word_b.burst_mode = TRANSFER_BURST_MODE_DISABLED;
                p_ctrl->p_reg->I2C_DMA_RDLR_REG = 0;
            }

            p_transfer_rx->p_cfg->p_info->p_src  = (uint8_t *) p_i2c_slave_w_rx_src_buffer;
            p_transfer_rx->p_cfg->p_info->p_dest = (uint8_t *) p_ctrl->p_buff;
            p_transfer_rx->p_cfg->p_info->length = (uint16_t) p_ctrl->total;

            p_transfer_rx->p_api->reconfigure(p_transfer_rx->p_ctrl, p_transfer_rx->p_cfg->p_info);
            p_transfer_rx->p_api->enable(p_transfer_rx->p_ctrl);

            p_ctrl->p_reg->I2C_DMA_CR_REG_b.RDMAE = 1;
 #endif
            p_ctrl->loaded            = p_ctrl->total;
            p_ctrl->transaction_count = p_ctrl->total;
 #if I2C_SLAVE_W_CFG_GENERIC_ONLY && BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

            /* Unmask the RX Full dedicated event. */
            p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #endif
        }
        else
#endif
        {
#if I2C_SLAVE_W_CFG_GENERIC_ONLY && BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

            /* Mask dedicated events except for RX Full event. */
            p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
#endif

            /* Read data */
            while (p_ctrl->p_reg->I2C_RXFLR_REG_b.RXFLR && (p_ctrl->loaded < p_ctrl->total))
            {
                /* Get received byte from RX fifo. */
                p_ctrl->transaction_count++;
                p_ctrl->p_buff[p_ctrl->loaded++] = p_ctrl->p_reg->I2C_DATA_CMD_REG_b.I2C_DAT;
            }

            /* If the requested bytes have been read, notify the application. */
            if (p_ctrl->loaded == p_ctrl->total)
            {
#if I2C_SLAVE_W_CFG_GENERIC_ONLY && BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
                p_ctrl->p_reg->I2C_MASK_REG = 0;
#endif
                i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_RX_COMPLETE);

                return;
            }
        }
    }
}

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)

/*******************************************************************************************************************//**
 * Enable the transfer driver to configure for I2C.
 *
 * @param[in]   p_cfg     Pointer to I2C specific configuration structure.
 *
 * @retval      FSP_SUCCESS                Transfer interface initialized successfully.
 * @retval      FSP_ERR_ASSERTION          Pointer to transfer instance p_api, p_cfg or p_cfg->p_info are NULL.
 **********************************************************************************************************************/
static fsp_err_t i2c_slave_w_transfer_open (i2c_slave_cfg_t const * const p_cfg)
{
    fsp_err_t err = FSP_SUCCESS;

    if (NULL != p_cfg->p_transfer_rx)
    {
        err = i2c_slave_w_transfer_configure(p_cfg->p_transfer_rx, I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }

    if (NULL != p_cfg->p_transfer_tx)
    {
        err = i2c_slave_w_transfer_configure(p_cfg->p_transfer_tx, I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE);
        if (FSP_SUCCESS != err)
        {
            if (NULL != p_cfg->p_transfer_rx)
            {
                p_cfg->p_transfer_rx->p_api->close(p_cfg->p_transfer_rx->p_ctrl);
            }

            return err;
        }
    }

    return err;
}

/*******************************************************************************************************************//**
 * Configures I2C related transfer drivers (if enabled).
 *
 * @param[in]     p_transfer                 Pointer to I2C specific control structure.
 * @param[in]     direction                  Slave Read or Slave Write.
 *
 * @retval        FSP_SUCCESS                Transfer interface is configured with valid parameters.
 * @retval        FSP_ERR_ASSERTION          Pointer to transfer instance p_api, p_cfg or p_cfg->p_info are NULL.
 **********************************************************************************************************************/
static fsp_err_t i2c_slave_w_transfer_configure (transfer_instance_t const * p_transfer,
                                                 i2c_slave_w_transfer_dir_t  direction)
{
    fsp_err_t err;

    /* Set default transfer info and open receive transfer module, if enabled. */
 #if I2C_SLAVE_W_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(NULL != p_transfer->p_api);
    FSP_ASSERT(NULL != p_transfer->p_cfg);
    FSP_ASSERT(NULL != p_transfer->p_cfg->p_info);
 #endif
    transfer_info_t * p_cfg = p_transfer->p_cfg->p_info;
    if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ == direction)
    {
        p_cfg->transfer_settings_word = I2C_SLAVE_W_DTC_RX_TRANSFER_SETTINGS;
    }
    else
    {
        p_cfg->transfer_settings_word = I2C_SLAVE_W_DTC_TX_TRANSFER_SETTINGS;
    }

    err = p_transfer->p_api->open(p_transfer->p_ctrl, p_transfer->p_cfg);

    return err;
}

#endif

/******************************************************************************************************************//**
 * Generic I2C Event interrupt routine.
 *
 * This function implements the Generic ISR routine..
 *********************************************************************************************************************/
void i2c_slave_w_gen_isr (void)
{
    /* Save context if RTOS is used. */
    FSP_CONTEXT_SAVE;

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);
#if BSP_FEATURE_BSP_HAS_ICU

    /* Clear the IR flag. */
    R_BSP_IrqStatusClear(irq);
#endif
    uint32_t mask = p_ctrl->p_reg->I2C_INTR_STAT_REG;

    while (mask)
    {
#if (BSP_FEATURE_I2C_VERSION == 1)
        if (mask & I2C_SLAVE_W_INT_START_DETECTED)
        {
            /* Reset START_DETECTED interrupt state. */
            p_ctrl->p_reg->I2C_CLR_START_DET_REG;

            /* We got here:
             * 1. after a Re-Started transaction
             * 2. which has not been already notified (so less data were read/written)
             * 3. & Read API has not been called before the transaction begins. */
            if (p_ctrl->transaction_count && (p_ctrl->direction != I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED) &&
                p_ctrl->loaded)
#elif (BSP_FEATURE_I2C_VERSION == 2)
        if (mask & I2C_SLAVE_W_INT_RESTART_DETECTED)
        {
            /* Reset RESTART_DETECTED interrupt state. */
            p_ctrl->p_reg->I2C_CLR_RESTART_DET_REG;

            /* We got here:
             * 1. after a Re-Started transaction
             * 2. which has not been already notified (so less data were read/written)
             * 3. & Read API has not been called before the transaction begins. */
            if ((p_ctrl->direction != I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED) && p_ctrl->loaded)
#endif
            {
                i2c_slave_event_t i2c_event = I2C_SLAVE_EVENT_ABORTED;

                if (!(mask & I2C_SLAVE_W_INT_TX_ABORT))
                {
                    if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ == p_ctrl->direction)
                    {
                        i2c_event = I2C_SLAVE_EVENT_RX_COMPLETE;
                    }
                    else if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE == p_ctrl->direction)
                    {
                        i2c_event = I2C_SLAVE_EVENT_TX_COMPLETE;
                    }
                }

                i2c_slave_w_notify(p_ctrl, i2c_event);
            }

            /* Reset transaction count. */
            p_ctrl->total             = 0U;
            p_ctrl->transaction_count = 0U;
        }

        if (mask & I2C_SLAVE_W_INT_RX_OVERFLOW)
        {
            /* Notify anyone waiting that the transfer is Aborted due to error. */
            i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_ABORTED);
#if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
 #if !I2C_SLAVE_W_CFG_GENERIC_ONLY

            /* Mask dedicated events except for RX Full event. */
            p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #else

            /* Mask dedicated events. */
            p_ctrl->p_reg->I2C_MASK_REG = 0;
 #endif
#endif

            /* Clear rx overflow. */
            p_ctrl->p_reg->I2C_CLR_RX_OVER_REG;
        }

        if (mask & I2C_SLAVE_W_INT_TX_ABORT)
        {
#if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
 #if !I2C_SLAVE_W_CFG_GENERIC_ONLY

            /* Mask dedicated events except for RX Full event. */
            p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #else

            /* Mask dedicated events. */
            p_ctrl->p_reg->I2C_MASK_REG = 0;
 #endif
#endif

            if (p_ctrl->notify_request)
            {
                /* Notify anyone waiting that the transfer is Aborted due to error. */
                i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_ABORTED);
            }

            /* Clear abort. */
            p_ctrl->p_reg->I2C_CLR_TX_ABRT_REG;
        }

#if I2C_SLAVE_W_CFG_GENERIC_ONLY
        if (mask & I2C_SLAVE_W_INT_RX_FULL)
        {
            i2c_slave_w_isr_read_buffer_handler(p_ctrl, I2C_SLAVE_EVENT_RX_REQUEST);
        }
#endif
        if (mask & I2C_SLAVE_W_INT_READ_REQUEST)
        {
            i2c_slave_w_isr_write_buffer_handler(p_ctrl, I2C_SLAVE_EVENT_TX_REQUEST);
        }

        if (mask & I2C_SLAVE_W_INT_STOP_DETECTED)
        {
            /* If STOP condition has been detected & i2c_slave_w_notify() has not been called due to
             *    1. an aborted transfer OR.
             *    2. the fact that we try to read/write more than the master wants to write/read
             *       notify the user now.
             */
            if (p_ctrl->notify_request)
            {
#if I2C_SLAVE_W_CFG_GENERIC_ONLY
                i2c_slave_event_t i2c_event = I2C_SLAVE_EVENT_ABORTED;

                if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ == p_ctrl->direction)
                {
 #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
                    p_ctrl->p_reg->I2C_MASK_REG = 0;
 #endif
                    i2c_event = I2C_SLAVE_EVENT_RX_COMPLETE;
                }
                else if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE == p_ctrl->direction)
                {
 #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

                    /* Enable the Transmit End dedicated event. */
                    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
 #endif
                    i2c_event = I2C_SLAVE_EVENT_TX_COMPLETE;
                }

                i2c_slave_w_notify(p_ctrl, i2c_event);
#else
                if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ == p_ctrl->direction)
                {
                    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;

                    i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_RX_COMPLETE);
                }
                else if (I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE == p_ctrl->direction)
                {
                    /* Enable the Transmit End dedicated event. */
                    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
                    __DSB();
                    __ISB();
                }
                else
                {
                    i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_ABORTED);
                }
#endif
            }

            /* Reset the transaction count here */
            p_ctrl->transaction_count = 0U;
            p_ctrl->total             = 0U;

            /* Reset STOP_DET interrupt state & disable the interrupt. */
            p_ctrl->p_reg->I2C_INTR_MASK_REG &=
                ((uint32_t) ~(I2C_I2C_INTR_MASK_REG_M_STOP_DET_Msk));
            p_ctrl->p_reg->I2C_CLR_STOP_DET_REG;
        }

        /* Check if any new events occurred before the ones that fired the IRQ are cleared. */
        mask = p_ctrl->p_reg->I2C_INTR_STAT_REG;
    }

#if BSP_FEATURE_BSP_HAS_ICU

    /* Check if IR was set during the while-loop & there is no interrupt event after the while-loop. */
    volatile uint32_t is_pending = *(&(ICU->ICU_IELSR0_REG) + irq) & ICU_ICU_IELSR0_REG_ICU_IR_Msk;
#else
    volatile uint32_t is_pending = NVIC_GetPendingIRQ(irq);
#endif

    if (is_pending && !p_ctrl->p_reg->I2C_INTR_STAT_REG)
    {
        /* Clear the IR flag & the NVIC Pending status. The interrupt will have been serviced during the while-loop. */
#if BSP_FEATURE_BSP_HAS_ICU
        R_BSP_IrqStatusClear(irq);
#endif
        NVIC_ClearPendingIRQ(irq);
    }

    /* Restore context if RTOS is used. */
    FSP_CONTEXT_RESTORE;
}

#if !I2C_SLAVE_W_CFG_GENERIC_ONLY

/******************************************************************************************************************//**
 * I2C RX FIFO Full Event interrupt routine.
 *
 * This function implements the RXI ISR routine.
 *********************************************************************************************************************/
void i2c_slave_w_rxi_isr (void)
{
    /* Save context if RTOS is used. */
    FSP_CONTEXT_SAVE;

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag. */
    R_BSP_IrqStatusClear(irq);

 #if I2C_SLAVE_W_CFG_DTC_ENABLE
    transfer_instance_t * p_transfer_rx = (transfer_instance_t *) p_ctrl->p_cfg->p_transfer_rx;

    /* If this is the interrupt that got fired after DTC transfer,
     * ignore it as the DTC has already taken care of the data transfer. */
    if ((NULL != p_transfer_rx) && (p_ctrl->loaded == p_ctrl->total) && p_ctrl->total)
    {
        /* Mask the RX full dedicated events.*/
        p_ctrl->p_reg->I2C_MASK_REG = 0;
        R_BSP_IrqClearPending(irq);

        /* Mask dedicated events except for RX Full event. */
        p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;

        /* Notify anyone waiting that the transfer is completed. */
        i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_RX_COMPLETE);

        /* Restore context if RTOS is used. */
        FSP_CONTEXT_RESTORE;

        return;
    }
 #endif

    i2c_slave_w_isr_read_buffer_handler(p_ctrl, I2C_SLAVE_EVENT_RX_REQUEST);

    /* Restore context if RTOS is used. */
    FSP_CONTEXT_RESTORE;
}

/******************************************************************************************************************//**
 * I2C TX FIFO Empty Event interrupt routine.
 *
 * This function implements the TXI ISR routine.
 *********************************************************************************************************************/
void i2c_slave_w_txi_isr (void)
{
    /* Save context if RTOS is used. */
    FSP_CONTEXT_SAVE;

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

 #if I2C_SLAVE_W_CFG_DTC_ENABLE
    transfer_instance_t * p_transfer_tx = (transfer_instance_t *) p_ctrl->p_cfg->p_transfer_tx;

    /* If this is the interrupt that got fired after DTC transfer,
     * ignore it as the DTC has already taken care of the data transfer. */
    if (NULL != p_transfer_tx)
    {
        /* Mask TX empty & unmask the Transmit End dedicated events. */
        p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk | I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
        R_BSP_IrqClearPending(irq);

        /* Unmask READ REQUEST event in Generic Irq. */
        uint16_t mask = p_ctrl->p_reg->I2C_INTR_MASK_REG | I2C_SLAVE_W_INT_READ_REQUEST;
        p_ctrl->p_reg->I2C_INTR_MASK_REG = mask;

        /* Notify anyone waiting that the transfer is completed. */
        i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_TX_COMPLETE);

        /* Restore context if RTOS is used. */
        FSP_CONTEXT_RESTORE;

        return;
    }
 #endif

 #if I2C_SLAVE_W_CFG_DMA_ENABLE
    if (NULL == p_ctrl->p_cfg->p_transfer_tx)
    {
 #endif

    if (p_ctrl->p_reg->I2C_STATUS_REG_b.TFNF && (p_ctrl->loaded < p_ctrl->total))
    {
        /* Write byte to TX FIFO, this will also release SCL. */
        p_ctrl->transaction_count++;
        p_ctrl->p_reg->I2C_DATA_CMD_REG = p_ctrl->p_buff[p_ctrl->loaded++];
    }

    /* If the requested bytes have been written, notify the application. */
    if (p_ctrl->loaded == p_ctrl->total)
    {
        p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk | I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
 #if (BSP_FEATURE_I2C_VERSION == 2)
        if (p_ctrl->notify_request)
        {
            /* Notify anyone waiting that the transfer is completed. */
            i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_TX_COMPLETE);
        }
 #endif
    }
    else
    {
 #if (BSP_FEATURE_I2C_VERSION == 1)
        p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #elif (BSP_FEATURE_I2C_VERSION == 2)
        p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk | I2C_I2C_MASK_REG_I2C_TXR_IRQ_MASK_Msk;
 #endif
    }

 #if I2C_SLAVE_W_CFG_DMA_ENABLE
}
 #endif

    /* Clear the IR flag. */
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used. */
    FSP_CONTEXT_RESTORE;
}

/******************************************************************************************************************//**
 * I2C Transmission End Event interrupt routine.
 *
 * This function implements the TEI ISR routine.
 *********************************************************************************************************************/
void i2c_slave_w_tei_isr (void)
{
    /* Save context if RTOS is used. */
    FSP_CONTEXT_SAVE;

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    i2c_slave_w_instance_ctrl_t * p_ctrl = (i2c_slave_w_instance_ctrl_t *) R_FSP_IsrContextGet(irq);

    /* Clear the IR flag in the ICU. */
    R_BSP_IrqStatusClear(irq);

    if (p_ctrl->notify_request && (p_ctrl->direction != I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ))
    {
        /* Notify anyone waiting that the transfer is completed. */
        i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_TX_COMPLETE);
    }

    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;

    /* Restore context if RTOS is used. */
    FSP_CONTEXT_RESTORE;
}

#endif

#if (I2C_SLAVE_W_CFG_DMA_ENABLE)

/*******************************************************************************************************************//**
 * Callback that must be called after a RX DMAC transfer completes with restart condition.
 *
 * @param[in]     p_ctrl     Pointer to I2C Slave instance control block
 **********************************************************************************************************************/
void i2c_slave_w_rx_dmac_callback (i2c_slave_w_instance_ctrl_t * const p_ctrl)
{
 #if !I2C_SLAVE_W_CFG_GENERIC_ONLY

    /* Mask dedicated events except for RX Full event. */
    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
 #else
  #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS

    /* Mask dedicated events. */
    p_ctrl->p_reg->I2C_MASK_REG = 0;
  #endif

    /* Unask the RX Full generic event. */
    p_ctrl->p_reg->I2C_INTR_MASK_REG |= I2C_SLAVE_W_INT_RX_FULL;
 #endif
    if (p_ctrl->notify_request)
    {
        i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_RX_COMPLETE);
    }
}

/*******************************************************************************************************************//**
 * Callback that must be called after a TX DMAC transfer completes with restart condition.
 *
 * @param[in]     p_ctrl     Pointer to I2C Slave instance control block
 **********************************************************************************************************************/
void i2c_slave_w_tx_dmac_callback (i2c_slave_w_instance_ctrl_t * const p_ctrl)
{
    /* Unask the Read Request generic event. */
    p_ctrl->p_reg->I2C_INTR_MASK_REG |= I2C_SLAVE_W_INT_READ_REQUEST;
 #if BSP_FEATURE_I2C_HAS_DEDICATED_IRQS
  #if !I2C_SLAVE_W_CFG_GENERIC_ONLY

    /* Mask dedicated events except for RX Full event. */
    p_ctrl->p_reg->I2C_MASK_REG = I2C_I2C_MASK_REG_I2C_RX_IRQ_MASK_Msk;
  #else

    /* Mask dedicated events. */
    p_ctrl->p_reg->I2C_MASK_REG = 0;
  #endif
 #endif
    if (p_ctrl->notify_request)
    {
        i2c_slave_w_notify(p_ctrl, I2C_SLAVE_EVENT_TX_COMPLETE);
    }
}

#endif
