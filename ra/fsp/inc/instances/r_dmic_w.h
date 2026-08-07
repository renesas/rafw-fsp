/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup DMIC
 * @{
 **********************************************************************************************************************/

#ifndef R_DMIC_W_H
#define R_DMIC_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_dmic_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* FPLL frequency in Hz */
#define BSP_CFG_FPLL_FREQ_HZ      (24576000)


/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Audio clock source. */
typedef enum e_dmic_audio_clock
{
    DMIC_AUDIO_CLOCK_CRYSTAL = 0,      ///< Audio clock source is the AUDIO_CLK input pin
    DMIC_AUDIO_CLOCK_FPLL    = 1,      ///< Audio clock source is internal connection to a MCU specific GPT channel output
} dmic_audio_clock_t;


/** Channel instance control block. DO NOT INITIALIZE.  Initialization occurs when  dmic_api_t::open is called. */
typedef struct st_dmic_instance_ctrl
{
    uint32_t          open;            // Whether or not this control block is initialized
    dmic_cfg_t const * p_cfg;           // Initial configurations.
    APU_AUD_Type     * p_reg;           // Pointer to I2S register base address

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
    void (* p_callback)(dmic_callback_args_t *);
    dmic_callback_args_t * p_callback_memory;
    void const          * p_context;   // < User defined context passed into callback function
} dmic_instance_ctrl_t;

/** I2S configuration extension. This extension is optional. */
typedef struct st_dmic_extended_cfg
{
    dmic_audio_clock_t audio_clock;     ///< Audio clock source, default is I2S_W_AUDIO_CLOCK_CRYSTAL
} dmic_extended_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
extern const dmic_api_t g_dmic_on_dmic;

/** @endcond */

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/

fsp_err_t R_DMIC_W_Open(dmic_ctrl_t * const p_ctrl, dmic_cfg_t const * const p_cfg);
fsp_err_t R_DMIC_W_Stop(dmic_ctrl_t * const p_ctrl);
fsp_err_t R_DMIC_W_StatusGet(dmic_ctrl_t * const p_ctrl, dmic_status_t * const p_status);
fsp_err_t R_DMIC_W_Write(dmic_ctrl_t * const p_ctrl, void const * const p_src, uint32_t const bytes);
fsp_err_t R_DMIC_W_Read(dmic_ctrl_t * const p_ctrl, void * const p_dest, uint32_t const bytes);
fsp_err_t R_DMIC_W_WriteRead(dmic_ctrl_t * const p_ctrl, void const * const p_src, void * const p_dest,
                          uint32_t const bytes);
fsp_err_t R_DMIC_W_Mute(dmic_ctrl_t * const p_ctrl, dmic_mute_t const mute_enable);
fsp_err_t R_DMIC_W_Close(dmic_ctrl_t * const p_ctrl);
fsp_err_t R_DMIC_W_CallbackSet(dmic_ctrl_t * const          p_api_ctrl,
                            void (                    * p_callback)(dmic_callback_args_t *),
                            void const * const          p_context,
                            dmic_callback_args_t * const p_callback_memory);

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 // R_DMIC_W_H

/*******************************************************************************************************************//**
 * @} (end defgroup DMIC)
 **********************************************************************************************************************/
