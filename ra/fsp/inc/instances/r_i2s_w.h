/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup I2S_W
 * @{
 **********************************************************************************************************************/

#ifndef R_I2S_W_H
#define R_I2S_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_i2s_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Audio clock source. */
typedef enum e_i2s_w_audio_clock
{
    I2S_W_AUDIO_CLOCK_CRYSTAL = 0,      ///< Audio clock source is the AUDIO_CLK input pin
    I2S_W_AUDIO_CLOCK_FPLL    = 1,      ///< Audio clock source is internal connection to a MCU specific GPT channel output
} i2s_w_audio_clock_t;

/** Bit clock division ratio.  Bit clock frequency = audio clock frequency / bit clock division ratio. */
typedef enum e_i2s_w_clock_div
{
    I2S_W_CLOCK_DIV_1   = 0,             ///< Clock divisor 1
    I2S_W_CLOCK_DIV_2   = 1,             ///< Clock divisor 2
    I2S_W_CLOCK_DIV_4   = 2,             ///< Clock divisor 4
    I2S_W_CLOCK_DIV_6   = 8,             ///< Clock divisor 6
    I2S_W_CLOCK_DIV_8   = 3,             ///< Clock divisor 8
    I2S_W_CLOCK_DIV_12  = 9,             ///< Clock divisor 12
    I2S_W_CLOCK_DIV_16  = 4,             ///< Clock divisor 16
    I2S_W_CLOCK_DIV_24  = 10,            ///< Clock divisor 24
    I2S_W_CLOCK_DIV_32  = 5,             ///< Clock divisor 32
    I2S_W_CLOCK_DIV_48  = 11,            ///< Clock divisor 48
    I2S_W_CLOCK_DIV_64  = 6,             ///< Clock divisor 64
    I2S_W_CLOCK_DIV_96  = 12,            ///< Clock divisor 96
    I2S_W_CLOCK_DIV_128 = 7,             ///< Clock divisor 128
} i2s_w_clock_div_t;


/** Sampling rate.  */
typedef enum e_i2s_w_sr{
        I2S_W_SR_RESERVED               = 0,   /**< 0, N/A */
        I2S_W_SR_8000Hz,                       /**< 1, 8000Hz */
        I2S_W_SR_11025Hz,                      /**< 2, 11025Hz */
        I2S_W_SR_12000Hz,                      /**< 3, 12000Hz */
        I2S_W_SR_RESERVED2,                    /**< 4, N/A */
        I2S_W_SR_16000Hz,                      /**< 5, 16000Hz */
        I2S_W_SR_22050Hz,                      /**< 6, 22050Hz */
        I2S_W_SR_24000Hz,                      /**< 7, 24000Hz */
        I2S_W_SR_RESERVED3,                    /**< 8, N/A */
        I2S_W_SR_32000Hz,                      /**< 9, 32000Hz */
        I2S_W_SR_44100Hz,                      /**< 10, 44100Hz */
        I2S_W_SR_48000Hz,                      /**< 11, 48000Hz */
        I2S_W_SR_RESERVED4,                    /**< 12, N/A */
        I2S_W_SR_RESERVED5,                    /**< 13, N/A */
        I2S_W_SR_88200Hz,                      /**< 14, 88200Hz , descoped*/
        I2S_W_SR_96000Hz,                      /**< 15, 96000Hz , descoped*/
        I2S_W_SR_RESERVED6,                    /**< 16, N/A */
        I2S_W_SR_RESERVED7,                    /**< 17, N/A */
        I2S_W_SR_176400Hz,                     /**< 18, 176400Hz , descoped*/
        I2S_W_SR_192000Hz,                     /**< 19, 192000Hz , descoped*/
} i2s_w_sr_t;

/** Channel instance control block. DO NOT INITIALIZE.  Initialization occurs when @ref i2s_api_t::open is called. */
typedef struct st_i2s_w_instance_ctrl
{
    uint32_t          open;            // Whether or not this control block is initialized
    i2s_cfg_t const * p_cfg;           // Initial configurations.
    DAI_Type        * p_reg;           // Pointer to I2S register base address

    /* Source buffer pointer used to fill hardware FIFO from transmit ISR. */
    void const * p_tx_src;

    /* Size of source buffer used to fill hardware FIFO from transmit ISR. */
    uint32_t tx_src_samples;

    /* Destination buffer pointer used to fill from hardware FIFO in receive ISR. */
    void * p_rx_dest;

    /* Size of destination buffer used to fill from hardware FIFO in receive ISR. */
    uint32_t        rx_dest_samples;
    transfer_size_t fifo_access_size;  // Access the FIFO as 1 byte, 2 bytes, or 4 bytes

    /* Pointer to callback and optional working memory */
    void (* p_callback)(i2s_callback_args_t *);
    i2s_callback_args_t * p_callback_memory;
    void * p_context;   // < User defined context passed into callback function
} i2s_w_instance_ctrl_t;

/** I2S configuration extension. This extension is optional. */
typedef struct st_i2s_w_extended_cfg
{
    i2s_w_audio_clock_t audio_clock;     ///< Audio clock source, default is I2S_W_AUDIO_CLOCK_CRYSTAL
    i2s_w_clock_div_t   bit_clock_div;   ///< Select bit clock division ratio
    i2s_w_sr_t          sr;              ///< sampling rate
} i2s_w_extended_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
extern const i2s_api_t g_i2s_on_i2s_w;

/** @endcond */

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

fsp_err_t R_I2S_W_Open(i2s_ctrl_t * const p_ctrl, i2s_cfg_t const * const p_cfg);
fsp_err_t R_I2S_W_Stop(i2s_ctrl_t * const p_ctrl);
fsp_err_t R_I2S_W_StatusGet(i2s_ctrl_t * const p_ctrl, i2s_status_t * const p_status);
fsp_err_t R_I2S_W_Write(i2s_ctrl_t * const p_ctrl, void const * const p_src, uint32_t const bytes);
fsp_err_t R_I2S_W_Read(i2s_ctrl_t * const p_ctrl, void * const p_dest, uint32_t const bytes);
fsp_err_t R_I2S_W_WriteRead(i2s_ctrl_t * const p_ctrl, void const * const p_src, void * const p_dest,
                          uint32_t const bytes);
fsp_err_t R_I2S_W_Mute(i2s_ctrl_t * const p_ctrl, i2s_mute_t const mute_enable);
fsp_err_t R_I2S_W_Close(i2s_ctrl_t * const p_ctrl);
fsp_err_t R_I2S_W_CallbackSet(i2s_ctrl_t * const          p_api_ctrl,
                            void (                    * p_callback)(i2s_callback_args_t *),
                            void * const          p_context,
                            i2s_callback_args_t * const p_callback_memory);

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // R_I2S_W_H

/*******************************************************************************************************************//**
 * @} (end defgroup I2S_W)
 **********************************************************************************************************************/
