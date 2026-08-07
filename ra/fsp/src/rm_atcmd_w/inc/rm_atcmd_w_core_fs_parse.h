/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/

#ifndef RM_ATCMD_W_CORE_FS_PARSE_H
#define RM_ATCMD_W_CORE_FS_PARSE_H

#include "rm_atcmd_w_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Configuration
 **********************************************************************************************************************/
#ifndef ATCMD_W_FS_EXIST
#define ATCMD_W_FS_EXIST   (1)
#endif

#if SUPPORT_FSP_RM_FS_W
/***********************************************************************************************************************
 * API functions
 **********************************************************************************************************************/

/**
 * @brief Register the LittleFS AT commands with the AT core module list.
 * @param[in,out] p_list Pointer to AT core module list.
 * @retval FSP_ERR_AT_CMD_ERR_CMD_OK on success.
 */
uint32_t RM_ATCMD_W_CORE_FS_register(atcmd_w_core_module_list_t * p_list);

/**
 * @brief Deregister the LittleFS AT commands from the AT core module list.
 * @param[in,out] p_list Pointer to AT core module list.
 * @retval FSP_ERR_AT_CMD_ERR_CMD_OK on success.
 */
uint32_t RM_ATCMD_W_CORE_FS_deregister(atcmd_w_core_module_list_t * p_list);

/**
 * @brief Open the LittleFS AT command handler.
 * @param[in] p_at_ctrl AT core control block.
 * @retval FSP_ERR_AT_CMD_ERR_CMD_OK on success.
 */
uint32_t RM_ATCMD_W_CORE_FS_open(atcmd_w_ctrl_t * const p_at_ctrl);

/**
 * @brief Close the LittleFS AT command handler.
 * @param[in] p_at_ctrl AT core control block.
 * @retval FSP_ERR_AT_CMD_ERR_CMD_OK on success.
 */
uint32_t RM_ATCMD_W_CORE_FS_close(atcmd_w_ctrl_t * const p_at_ctrl);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RM_ATCMD_W_CORE_FS_PARSE_H */
