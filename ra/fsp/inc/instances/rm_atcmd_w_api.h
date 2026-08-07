/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup RENESAS_CONNECTIVITY_INTERFACES
 * @defgroup AT_API AT Interface
 * @brief Interface for AT communications.
 *
 * @section AT_INTERFACE_SUMMARY Summary
 * The AT interface provides common APIs for AT HAL drivers. The AT interface supports the following features:
 * - Full-duplex AT communication
 * - Interrupt driven transmit/receive processing
 * - Callback function with returned event code
 *
 * @{
 **********************************************************************************************************************/

#ifndef RM_ATCMD_W_API_H
#define RM_ATCMD_W_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Includes board and MCU related header files. */
#include "bsp_api.h"
#include "r_transfer_api.h"
#include "r_i2c_master_api.h"
#include "r_spi_api.h"
#include "r_spi_flash_api.h"
#include "r_adc_api.h"
#include "r_timer_api.h"
#include "rm_atcmd_transport_w_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** AT driver specific information */
typedef struct st_atcmd_w_info
{
    /** Maximum bytes that can be written at this time.
     * Only applies if atcmd_w_cfg_t::p_transfer_tx is not NULL. */
    uint32_t write_bytes_max;

    /** Maximum bytes that are available to read at one time.
     * Only applies if atcmd_w_cfg_t::p_transfer_rx is not NULL. */
    uint32_t read_bytes_max;
} atcmd_w_info_t;

/** AT Configuration */
typedef struct st_atcmd_w_cfg
{
    /** Optional transfer instance used to receive multiple bytes without interrupts.  Set to NULL if unused.
     * If NULL, the number of bytes allowed in the read API is limited to one byte at a time. */
    transfer_instance_t const * p_transfer_rx;

    /** Optional transfer instance used to send multiple bytes without interrupts.  Set to NULL if unused.
     * If NULL, the number of bytes allowed in the write APIs is limited to one byte at a time. */
    transfer_instance_t const * p_transfer_tx;

    /* Pointer to i2c master instance */
    i2c_master_instance_t const * p_i2c_master;

    /* Pointer to spi instance */
    spi_instance_t const * p_spi;

    /* Pointer to SPI Flash instance */
    spi_flash_instance_t const * p_spi_flash;

    /* Pointer to adc instance */
    adc_instance_t const * p_adc;

    /* Pointer to gpt instance */
    timer_instance_t const * p_gpt;

    /* Pointer to atcmd_transport_w instance */
    atcmd_transport_w_instance_t const * p_transport_instance;

    /* Pointer to AT peripheral specific configuration */
    void const * p_extend;                              ///< AT hardware dependent configuration
} atcmd_w_cfg_t;

/** AT control block.  Allocate an instance specific control block to pass into the AT API calls.
 */
typedef void atcmd_w_ctrl_t;

/** Shared Interface definition for AT */
typedef struct st_atcmd_w_api
{
    /** Open AT device.
     *
     * @param[in,out]  p_ctrl   Pointer to the AT control block. Must be declared by user. Value set here.
     * @param[in]      atcmd_w_cfg_t Pointer to AT configuration structure. All elements of this structure must be
     *                          set by user.
     */
    fsp_err_t (* open)(atcmd_w_ctrl_t * const p_ctrl, atcmd_w_cfg_t const * const p_cfg);

    /** Read from AT device. The internal read buffer is used until the available data is read.
     * When internal read buffer is empty, a new data is requested from the device.
     * The maximum transfer size is reported by infoGet().
     *
     * @param[in]   p_ctrl     Pointer to the AT control block for the channel.
     * @param[in]   p_dest     Destination address to read data from.
     * @param[in]   bytes      Read data length.
     */
    fsp_err_t (* read)(atcmd_w_ctrl_t * const p_ctrl, uint8_t * const p_dest, uint32_t const bytes);

    /** Read from AT device.  The read buffer is used until the available data is read.
     * When internal read buffer is empty, a new data is requested from the device only if the command is not complete.
     * In case of complete command is expected and no data in the buffer left - corresponding error is thrown.
     * The maximum transfer size is reported by infoGet().
     *
     * @param[in]   p_ctrl     Pointer to the AT control block for the channel.
     * @param[in]   p_dest     Destination address to read data from.
     * @param[in]   bytes      Read data length.
     */
    fsp_err_t (* dataRead)(atcmd_w_ctrl_t * const p_ctrl, uint8_t * const p_dest, uint32_t const bytes);

    /** Write to AT device.
     * The maximum transfer size is reported by infoGet().
     *
     * @param[in]   p_ctrl     Pointer to the AT control block.
     * @param[in]   p_src      Source address  to write data to.
     * @param[in]   bytes      Write data length.
     */
    fsp_err_t (* write)(atcmd_w_ctrl_t * const p_ctrl, uint8_t const * const p_src, uint32_t const bytes);

    /** Get the driver specific information.
     *
     * @param[in]   p_ctrl     Pointer to the AT control block.
     * @param[in]   p_info     Pointer to AT status information.
     */
    fsp_err_t (* infoGet)(atcmd_w_ctrl_t * const p_ctrl, atcmd_w_info_t * const p_info);

    /** Close AT device.
     *
     * @param[in]   p_ctrl     Pointer to the AT control block.
     */
    fsp_err_t (* close)(atcmd_w_ctrl_t * const p_ctrl);
} atcmd_w_api_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_atcmd_w_instance
{
    atcmd_w_ctrl_t      * p_ctrl;         ///< Pointer to the control structure for this instance
    atcmd_w_cfg_t const * p_cfg;          ///< Pointer to the configuration structure for this instance
    atcmd_w_api_t const * p_api;          ///< Pointer to the API structure for this instance
} atcmd_w_instance_t;

/** @} (end defgroup AT_API) */

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
