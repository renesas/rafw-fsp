/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup I2C_SLAVE_W
 * @{
 **********************************************************************************************************************/

#ifndef R_I2C_SLAVE_W_H
#define R_I2C_SLAVE_W_H

#include "r_i2c_slave_w_cfg.h"
#include "r_i2c_slave_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 ***********************************************************************************************************************/

/**  TX/RX FIFO depth */
#define I2C_SLAVE_W_FIFO_DEPTH         (32)

/** Offset to make the I2C HW channels 0-based for the r_i2c_slave_w driver. */
#if (BSP_FEATURE_I2C_VALID_CHANNEL_MASK & 0x1)
 #define I2C_SLAVE_W_CHANNEL_OFFSET    0
#elif (BSP_FEATURE_I2C_VALID_CHANNEL_MASK & 0x2)
 #define I2C_SLAVE_W_CHANNEL_OFFSET    1
#endif

/***********************************************************************************************************************
 * Typedef definitions
 ***********************************************************************************************************************/

/** I2C Slave transaction enumeration */
typedef enum e_i2c_slave_w_transfer_dir_option
{
    I2C_SLAVE_W_TRANSFER_DIR_MASTER_READ_SLAVE_WRITE = 0x0,
    I2C_SLAVE_W_TRANSFER_DIR_MASTER_WRITE_SLAVE_READ = 0x1,
    I2C_SLAVE_W_TRANSFER_DIR_NOT_ESTABLISHED         = 0x2
} i2c_slave_w_transfer_dir_t;

/** I2C interrupt source */
typedef enum e_i2c_slave_w_int
{
    I2C_SLAVE_W_INT_RX_UNDERFLOW     = I2C_I2C_INTR_STAT_REG_R_RX_UNDER_Msk,        ///< Attempt to read from empty RX FIFO has been made
    I2C_SLAVE_W_INT_RX_OVERFLOW      = I2C_I2C_INTR_STAT_REG_R_RX_OVER_Msk,         ///< RX FIFO is full but new data are incoming and being discarded
    I2C_SLAVE_W_INT_RX_FULL          = I2C_I2C_INTR_STAT_REG_R_RX_FULL_Msk,         ///< RX FIFO level is equal or above threshold
    I2C_SLAVE_W_INT_TX_OVERFLOW      = I2C_I2C_INTR_STAT_REG_R_TX_OVER_Msk,         ///< Attempt to write to TX FIFO which is already full
    I2C_SLAVE_W_INT_TX_EMPTY         = I2C_I2C_INTR_STAT_REG_R_TX_EMPTY_Msk,        ///< TX FIFO level is  equal or below threshold
    I2C_SLAVE_W_INT_READ_REQUEST     = I2C_I2C_INTR_STAT_REG_R_RD_REQ_Msk,          ///< I2C master attempts to read data (slave only)
    I2C_SLAVE_W_INT_TX_ABORT         = I2C_I2C_INTR_STAT_REG_R_TX_ABRT_Msk,         ///< TX cannot be completed
    I2C_SLAVE_W_INT_RX_DONE          = I2C_I2C_INTR_STAT_REG_R_RX_DONE_Msk,         ///< I2C master did not acknowledge transmitted byte(slave only)
    I2C_SLAVE_W_INT_ACTIVITY         = I2C_I2C_INTR_STAT_REG_R_ACTIVITY_Msk,        ///< Any I2C activity occurred
    I2C_SLAVE_W_INT_STOP_DETECTED    = I2C_I2C_INTR_STAT_REG_R_STOP_DET_Msk,        ///< STOP condition occurred
    I2C_SLAVE_W_INT_START_DETECTED   = I2C_I2C_INTR_STAT_REG_R_START_DET_Msk,       ///< START/RESTART condition occurred
    I2C_SLAVE_W_INT_GENERAL_CALL     = I2C_I2C_INTR_STAT_REG_R_GEN_CALL_Msk,        ///< General Call address received(slave only)
    I2C_SLAVE_W_INT_RESTART_DETECTED = I2C_I2C_INTR_STAT_REG_R_RESTART_DET_Msk,     ///< RESTART condition occurred
    I2C_SLAVE_W_INT_MASTER_ON_HOLD   = I2C_I2C_INTR_STAT_REG_R_MASTER_ON_HOLD_Msk,  ///< Master is holding the bus and TX FIFO is empty
    I2C_SLAVE_W_INT_SCL_STUCK_AT_LOW = I2C_I2C_INTR_STAT_REG_R_SCL_STUCK_AT_LOW_Msk ///< SCL STUCK AT LOW timeout occurred
} i2c_slave_w_int_t;

/** Holding bus when RX FIFO is full. */
typedef enum e_i2c_slave_w_rx_fifo_full_hld
{
    I2C_SLAVE_W_RX_FIFO_FULL_HLD_DISABLED = 0, ///< Overflow when RX FIFO is full.
    I2C_SLAVE_W_RX_FIFO_FULL_HLD_ENABLED  = 1  ///< Hold bus when RX FIFO is full.
} i2c_slave_w_rx_fifo_full_hld_t;

/** I2C clock settings */
typedef struct i2c_slave_w_clock_settings
{
    uint8_t  digital_filter_stages;    ///< Counts of source clock periods for spike suppression
    uint8_t  sda_setup_counts;         ///< Counts of source clock periods for SDA Setup time
    uint16_t sda_hold_tx_counts;       ///< Counts of source clock periods for SDA Hold time when in Transmitter mode
    uint16_t sda_hold_rx_counts;       ///< Counts of source clock periods for SDA Hold time when in Receiver mode
} i2c_slave_w_clock_settings_t;

/** I2C control structure. DO NOT INITIALIZE. */
typedef struct st_i2c_slave_w_instance_ctrl
{
    i2c_slave_cfg_t const * p_cfg;                                                // Information describing I2C device
    uint32_t                open;                                                 // Flag to determine if the device is open
    I2C_Type              * p_reg;                                                // Base register for this channel

    /* Current transfer information. */
    uint8_t * p_buff;                                                             // Holds the data associated with the transfer

#if (I2C_SLAVE_W_CFG_DTC_ENABLE) || (I2C_SLAVE_W_CFG_DMA_ENABLE)
    uint16_t p_transfer_api_tx_buff[I2C_SLAVE_W_CFG_TRANSFER_API_TX_BUFFER_SIZE]; // Holds the data associated with the DTC/DMA transfer
#endif

    uint32_t      total;                                                          // Holds the total number of data bytes to transfer
    uint32_t      loaded;                                                         // Tracks the number of data bytes written to the register
    uint32_t      transaction_count;                                              // Tracks the actual number of transactions (in case Master requests more Writes/Reads)
    volatile bool notify_request;                                                 // Track whether the master request is notified to the application
    volatile i2c_slave_w_transfer_dir_t direction;                                // Holds the direction of the data byte transfer

    /* Pointer to callback and optional working memory */
    void (* p_callback)(i2c_slave_callback_args_t *);
    i2c_slave_callback_args_t * p_callback_memory;

    /* Pointer to context to be passed into callback function */
    void * p_context;
} i2c_slave_w_instance_ctrl_t;

/** R_I2C_SLAVE_W extended configuration */
typedef struct st_i2c_slave_w_extended_cfg
{
#if I2C_SLAVE_W_CFG_DMA_ENABLE
    bool enable_dma_bursts_tx;                       ///< Enable DMA Burst TX Transactions when the transaction length is 4- or 8-byte aligned
    bool enable_dma_bursts_rx;                       ///< Enable DMA Burst RX Transactions when the transaction length is 4- or 8-byte aligned
#endif

    i2c_slave_w_clock_settings_t   clock_settings;   ///< I2C Clock settings
    i2c_slave_w_rx_fifo_full_hld_t rx_fifo_full_hld; ///< Enable holding bus when RX_FIFO is full.

    bool      select_divn;                           ///< Select the clock source (DIVN/DIV1 clock)
    IRQn_Type gen_irq;                               ///< Generic I2C Interrupt IRQ number.
    uint8_t   gen_ipl;                               ///< Generic I2C Interrupt Priority.
} i2c_slave_w_extended_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 ***********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern i2c_slave_api_t const g_i2c_slave_on_i2c_w;

/** @endcond */

/***********************************************************************************************************************
 * Public Function Prototypes
 **********************************************************************************************************************/

fsp_err_t R_I2C_SLAVE_W_Open(i2c_slave_ctrl_t * const p_api_ctrl, i2c_slave_cfg_t const * const p_cfg);
fsp_err_t R_I2C_SLAVE_W_Read(i2c_slave_ctrl_t * const p_api_ctrl, uint8_t * const p_dest, uint32_t const bytes);
fsp_err_t R_I2C_SLAVE_W_Write(i2c_slave_ctrl_t * const p_api_ctrl, uint8_t * const p_src, uint32_t const bytes);
fsp_err_t R_I2C_SLAVE_W_Close(i2c_slave_ctrl_t * const p_api_ctrl);
fsp_err_t R_I2C_SLAVE_W_CallbackSet(i2c_slave_ctrl_t * const          p_api_ctrl,
                                    void (                          * p_callback)(i2c_slave_callback_args_t *),
                                    void * const                      p_context,
                                    i2c_slave_callback_args_t * const p_callback_memory);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // R_I2C_SLAVE_W_H

/*******************************************************************************************************************//**
 * @} (end defgroup I2C_SLAVE_W)
 ***********************************************************************************************************************/
