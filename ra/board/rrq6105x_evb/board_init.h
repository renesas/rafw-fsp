/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @ingroup BOARD_RRQ6105X_EVB
 * @defgroup BOARD_RRQ6105x_EVB_INIT
 * @brief Board specific code for the RRQ6105X EVB
 *
 * This include file is specific to the RRQ6105X EVB.
 *
 * @{
 **********************************************************************************************************************/

#ifndef BOARD_INIT_H
#define BOARD_INIT_H

/** Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Exported global functions (to be accessed by other files)
 **********************************************************************************************************************/
void bsp_init(void * p_args);

/** Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of t
 * his file. */
FSP_FOOTER

#endif

/** @} (end defgroup BOARD_RRQ6105x_EVB_INIT) */
