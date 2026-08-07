/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef R_CRC_W_H
#define R_CRC_W_H

/*******************************************************************************************************************//**
 * @addtogroup CRC_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_crc_w_cfg.h"
#include "r_crc_api.h"
#include "r_transfer_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define	CRC_W_REQ_STOP              (0x0004)
#define	CRC_W_REQ_START             (0x0002)
#define	CRC_W_REQ_CLEAR             (0x0001)

#define CRC_W_MEMORY_ROM_BASE       (0x0F020000UL)
#define CRC_W_MEMORY_NONE           (0x00000000UL)
#define CRC_W_MEMORY_OQSPIC_S_BASE  (0x2A000000UL)
#define CRC_W_MEMORY_SYSRAM_BASE    (0x20000000UL)
#define CRC_W_MEMORY_REMAPPED_END   (0x08000000UL)

#define CRC_W_CHECKING_ENABLE       (1)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Driver instance control structure. */
typedef struct st_crc_w_instance_ctrl
{
    uint32_t          open;         ///< Whether or not driver is open.
    const crc_cfg_t * p_cfg;        ///< Pointer to initial configuration.
} crc_w_instance_ctrl_t;

/** CRC channel. */
typedef enum e_crc_w_channel
{
    CRC_W_CHANNEL_0 = 0,            ///< CRC channel 0.
    CRC_W_CHANNEL_1 = 1,            ///< CRC channel 1.
} crc_w_channel_t;

/** CRC Generating Polynomial. */
typedef enum e_crc_w_polynomial
{
    /** X^32 + X^26 + X^23 + X^22 + X^16 + X^12 + X^11 + X^10 + X^8 + X^7 + X^5 + X^4 + X^2 + X + 1 */
    CRC_W_POLYNOMIAL_CRC_32       = 0,  ///< 32-bit CRC-32
    /** X^32 + X^28 + X^27 + X^26 + X^25 + X^23 + X^22 + X^20 + X^19 + X^18 + X^14 + X^13 + X^11 + X^10 + X^9 + X^8 + X^6 + 1 */
    CRC_W_POLYNOMIAL_CRC_32C      = 1,  ///< 32-bit CRC-32C
    /** X^16 + X^12 + X^5 + 1 */
    CRC_W_POLYNOMIAL_CRC_16_CCITT = 2,  ///< 16-bit CRC-16-CCITT
    /** X^16 + X^15 + X^2 + 1 */
    CRC_W_POLYNOMIAL_CRC_16_IBM   = 3,  ///< 16-bit CRC-16-IBM
} crc_w_polynomial_t;

typedef enum e_crc_w_path_selection
{
    CRC_W_PATH_SELECT_CPU = 0,      ///< CPU.
    CRC_W_PATH_SELECT_DMA = 1,      ///< DMA.
} crc_w_path_selection_t;

typedef enum e_crc_w_swap_enable
{
    CRC_W_SWAP_NORMAL = 0,          ///< Data swap normal.
    CRC_W_SWAP_BYTE   = 1,          ///< Data swap byte.
} crc_w_swap_enable_t;

typedef enum e_crc_w_endian_type
{
    CRC_W_ENDIAN_BIG    = 0,        ///< Big endian.
    CRC_W_ENDIAN_LITTLE = 1,        ///< Little endian.
} crc_w_endian_type_t;

typedef enum e_crc_w_parallel_type
{
    CRC_W_PARALLEL_TYPE_8  = 0,     ///< 8 bit
    CRC_W_PARALLEL_TYPE_16 = 1,     ///< 16 bit
    CRC_W_PARALLEL_TYPE_32 = 2,     ///< 32 bit
} crc_w_parallel_type_t;

typedef enum e_crc_w_access_type
{
    CRC_W_ACCESS_TYPE_READ  = 0,    ///< Read access.
    CRC_W_ACCESS_TYPE_WRITE = 1,    ///< Write access.
} crc_w_access_type_t;

typedef enum e_crc_w_calculation_status
{
    CRC_W_CAL_STATUS_IDLE  = 0,     ///< CRC calculation status idle.
    CRC_W_CAL_STATUS_BUSY  = 1,     ///< CRC calculation status busy.
    CRC_W_CAL_STATUS_STOP  = 2,     ///< CRC calculation status stop.
    CRC_W_CAL_STATUS_ERROR = 3,     ///< CRC calculation status error.
} crc_w_calculation_status_t;

/** Remap address 0. */
typedef enum e_crc_w_remap_address_0
{
    CRC_W_REMAP_ADDRESS_0_TO_ROM         = 0,   ///< ROM.
    CRC_W_REMAP_ADDRESS_0_TO_OQSPI_FLASH = 2,   ///< OQSPI.
    CRC_W_REMAP_ADDRESS_0_TO_RAM         = 3,   ///< RAM.
} crc_w_remap_address_0_t;

/** Extended configuration. */
typedef struct st_crc_w_extended_cfg
{
    crc_w_channel_t         channel;            ///< CRC channel.
    crc_w_polynomial_t      polynomial;         ///< CRC Generating Polynomial.  
    crc_w_swap_enable_t     swap_enable;        ///< Input Data Swap Enable.
    crc_w_endian_type_t     endian_type;        ///< Bit Endian Type.
    crc_w_parallel_type_t   parallel_type;      ///< Parallel Type of CRC Calculation.
    crc_w_access_type_t     access_type;        ///< Access Type of CRC Calculation.
    crc_w_path_selection_t  path_selection;     ///< Path Selection of CRC Calculation.

    transfer_instance_t     const * p_transfer; ///< DMAC instance. Set to NULL if unused.
} crc_w_extended_cfg_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const crc_api_t g_crc_on_crc_w;

/** @endcond */

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
fsp_err_t R_CRC_W_Open(crc_ctrl_t * const p_ctrl, crc_cfg_t const * const p_cfg);
fsp_err_t R_CRC_W_Close(crc_ctrl_t * const p_ctrl);
fsp_err_t R_CRC_W_Calculate(crc_ctrl_t * const p_ctrl, crc_input_t * const p_crc_input, uint32_t * calculatedValue);
fsp_err_t R_CRC_W_CalculatedValueGet(crc_ctrl_t * const p_ctrl, uint32_t * calculatedValue);
fsp_err_t R_CRC_W_SnoopEnable(crc_ctrl_t * const p_ctrl, uint32_t crc_seed);
fsp_err_t R_CRC_W_SnoopDisable(crc_ctrl_t * const p_ctrl);

/*******************************************************************************************************************//**
 * @} (end defgroup CRC_W)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif
