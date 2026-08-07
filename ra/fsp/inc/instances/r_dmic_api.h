/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_CONNECTIVITY_INTERFACES
 * @defgroup DMIC_API DMIC Interface
 * @brief Interface for DMIC audio communication.
 *
 * @section DMIC_API_SUMMARY Summary
 * @brief The DMIC (Digital Microphone) interface provides APIs and definitions for DMIC audio communication.
 *
 * @{
 **********************************************************************************************************************/

#ifndef R_DMIC_W_API_H
#define R_DMIC_W_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Register definitions, common services and error codes. */
#include "bsp_api.h"
#ifndef BSP_OVERRIDE_DMIC_INCLUDE
 #include "r_timer_api.h"
#endif
#include "r_transfer_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
#ifndef BSP_OVERRIDE_DMIC_DIRECTION_T

/** DMIC Direction */
typedef enum e_dmic_direction
{
    DMIC_DIRECTION_INPUT,  ///< DMIC input 
    DMIC_DIRECTION_OUTPUT, ///< DMIC output 
} dmic_direction_t;
#endif

#ifndef BSP_OVERRIDE_DMIC_MODE_T

/** DMIC operation mode */
typedef enum e_dmic_mode
{
    DMIC_MODE_SLAVE = 0,         ///< DMIC interface in slave mode 
    DMIC_MODE_MASTER,            ///< DMIC interface in master mode 
} dmic_mode_t;
#endif

/** DMIC set input delay */
typedef enum e_dmic_delay
{
    DMIC_DI_NO_DELAY = 0,           ///< DMIC no input delay
    DMIC_DI_6NS_DELAY,              ///< DMIC 6ns input delay
    DMIC_DI_12NS_DELAY,             ///< DMIC 12ns input delay
    DMIC_DI_18NS_DELAY,             ///< DMIC 18ns input delay
    DMIC_DI_DELAY_SIZE
} dmic_delay_t;

/** Events that can trigger a callback function */
typedef enum e_dmic_event
{
    DMIC_EVENT_IDLE,                    ///< Communication is idle
    DMIC_EVENT_TX_EMPTY,                ///< Transmit buffer is below FIFO trigger level
    DMIC_EVENT_RX_FULL,                 ///< Receive buffer is above FIFO trigger level
} dmic_event_t;

/** Mute audio samples. */
typedef enum e_dmic_mute
{
    DMIC_MUTE_OFF = 0,                  ///< Disable mute
    DMIC_MUTE_ON  = 1,                  ///< Enable mute
} dmic_mute_t;

/** Possible status values returned by @ref dmic_api_t::statusGet. */
typedef enum e_dmic_state
{
    DMIC_STATE_IN_USE,                  ///< DMIC is in use
    DMIC_STATE_STOPPED                  ///< DMIC is stopped
} dmic_state_t;

/** Callback function parameter data */
typedef struct st_dmic_callback_args
{
    /** Placeholder for user data.  Set in @ref dmic_api_t::open function in @ref dmic_cfg_t. */
    void const * p_context;
    dmic_event_t  event;                ///< The event can be used to identify what caused the callback (overflow or error).
} dmic_callback_args_t;

/** DMIC control block.  Allocate an instance specific control block to pass into the DMIC API calls.
 */
typedef void dmic_ctrl_t;

/** DMIC status. */
typedef struct st_dmic_status
{
    dmic_state_t state;                 ///< Current DMIC state
} dmic_status_t;

/** User configuration structure, used in open function */
typedef struct st_dmic_cfg
{
    /** Select a channel corresponding to the channel number of the hardware. */
    uint32_t           channel;
    dmic_direction_t   direction;       ///< DMIC direction
    dmic_mode_t        mode;            ///< DMIC operation mode
    dmic_delay_t       delay;           ///< DMIC internal delay
    bool               swap_channel;    ///< DMIC L/R channel swap
    uint32_t           clk_frequency;   ///< DMIC clock

    /** To use DMA for receiving link a Transfer instance here.  Set to NULL if unused. */
    transfer_instance_t const * p_transfer_rx;

    /** Callback provided when an DMIC ISR occurs.  Set to NULL for no CPU interrupt. */
    void (* p_callback)(dmic_callback_args_t * p_args);

    /** Placeholder for user data.  Passed to the user callback in @ref dmic_callback_args_t. */
    void const * p_context;
    void const * p_extend;             ///< Extension parameter for hardware specific settings.
    uint8_t      rxi_ipl;              ///< Receive interrupt priority
    uint8_t      idle_err_ipl;         ///< Idle/Error interrupt priority
    IRQn_Type    rxi_irq;              ///< Receive IRQ number
    IRQn_Type    int_irq;              ///< Idle/Error IRQ number
} dmic_cfg_t;

/** DMIC functions implemented at the HAL layer will follow this API. */
typedef struct st_dmic_api
{
    /** Initial configuration.
     *
     * @pre Peripheral clocks and any required output pins should be configured prior to calling this function.
     * @note To reconfigure after calling this function, call @ref dmic_api_t::close first.
     * @param[in]   p_ctrl     Pointer to control block. Must be declared by user. Elements set here.
     * @param[in]   p_cfg      Pointer to configuration structure. All elements of this structure must be set by user.
     */
    fsp_err_t (* open)(dmic_ctrl_t * const p_ctrl, dmic_cfg_t const * const p_cfg);

    /** Stop communication. Communication is stopped when callback is called with DMIC_EVENT_IDLE.
     *
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     */
    fsp_err_t (* stop)(dmic_ctrl_t * const p_ctrl);

    /** Enable or disable mute.
     *
     * @param[in]   p_ctrl       Control block set in @ref dmic_api_t::open call for this instance.
     * @param[in]   mute_enable  Whether to enable or disable mute.
     */
    fsp_err_t (* mute)(dmic_ctrl_t * const p_ctrl, dmic_mute_t const mute_enable);

    /** Write DMIC data.  All transmit data is queued when callback is called with DMIC_EVENT_TX_EMPTY.
     * Transmission is complete when callback is called with DMIC_EVENT_IDLE.
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     * @param[in]   p_src      Buffer of PCM samples.  Must be 4 byte aligned.
     * @param[in]   bytes      Number of bytes in the buffer.  Recommended requesting a multiple of 8 bytes.  If not
     *                         a multiple of 8, padding 0s will be added to transmission to make it a multiple of 8.
     */
    fsp_err_t (* write)(dmic_ctrl_t * const p_ctrl, void const * const p_src, uint32_t const bytes);

    /** Read DMIC data.  Reception is complete when callback is called with DMIC_EVENT_RX_EMPTY.
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     * @param[in]   p_dest     Buffer to store PCM samples.  Must be 4 byte aligned.
     * @param[in]   bytes      Number of bytes in the buffer.  Recommended requesting a multiple of 8 bytes.  If not
     *                         a multiple of 8, receive will stop at the multiple of 8 below requested bytes.
     */
    fsp_err_t (* read)(dmic_ctrl_t * const p_ctrl, void * const p_dest, uint32_t const bytes);

    /** Simultaneously write and read DMIC data.  Transmission and reception are complete when
     * callback is called with DMIC_EVENT_IDLE.
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     * @param[in]   p_src      Buffer of PCM samples.  Must be 4 byte aligned.
     * @param[in]   p_dest     Buffer to store PCM samples.  Must be 4 byte aligned.
     * @param[in]   bytes      Number of bytes in the buffers.  Recommended requesting a multiple of 8 bytes.  If not
     *                         a multiple of 8, padding 0s will be added to transmission to make it a multiple of 8,
     *                         and receive will stop at the multiple of 8 below requested bytes.
     */
    fsp_err_t (* writeRead)(dmic_ctrl_t * const p_ctrl, void const * const p_src, void * const p_dest,
                            uint32_t const bytes);

    /** Get current status and store it in provided pointer p_status.
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     * @param[out]  p_status   Current status of the driver.
     */
    fsp_err_t (* statusGet)(dmic_ctrl_t * const p_ctrl, dmic_status_t * const p_status);

    /** Allows driver to be reconfigured and may reduce power consumption.
     *
     * @param[in]   p_ctrl     Control block set in @ref dmic_api_t::open call for this instance.
     */
    fsp_err_t (* close)(dmic_ctrl_t * const p_ctrl);

    /**
     * Specify callback function and optional context pointer and working memory pointer.
     *
     * @param[in]   p_ctrl                   Pointer to the DMIC control block.
     * @param[in]   p_callback               Callback function
     * @param[in]   p_context                Pointer to send to callback function
     * @param[in]   p_working_memory         Pointer to volatile memory where callback structure can be allocated.
     *                                       Callback arguments allocated here are only valid during the callback.
     */
    fsp_err_t (* callbackSet)(dmic_ctrl_t * const p_ctrl, void (* p_callback)(dmic_callback_args_t *),
                              void const * const p_context, dmic_callback_args_t * const p_callback_memory);
} dmic_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_dmic_instance
{
    dmic_ctrl_t      * p_ctrl;          ///< Pointer to the control structure for this instance
    dmic_cfg_t const * p_cfg;           ///< Pointer to the configuration structure for this instance
    dmic_api_t const * p_api;           ///< Pointer to the API structure for this instance
} dmic_instance_t;

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif

/*******************************************************************************************************************//**
 * @} (end defgroup DMIC_API)
 **********************************************************************************************************************/
