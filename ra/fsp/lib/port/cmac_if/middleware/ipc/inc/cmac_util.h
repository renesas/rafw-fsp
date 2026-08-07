/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 ****************************************************************************************
 *
 * @file cmac_util.h
 *
 * @brief Miscellaneous Utilities
 *
 ****************************************************************************************
 */

/**
 * \brief An optimized version of the memcpy STD function
 *
 * \param [out] dst The destination where the data will be placed
 * \param [in] src The source of the data
 * \param [in] n Number of bytes to copy
 *
 * \returns A pointer to destination
 *
 */
void * memcpy_op(void * dst, const void * src, size_t n);
