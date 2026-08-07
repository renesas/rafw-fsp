/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup PLA_DRI_PER_ANALOG
 * \{
 * \addtogroup HW_SYS System
 * \{
 * \brief System Driver
 */

/**
 ****************************************************************************************
 *
 * @file hw_sys.h
 *
 * @brief System Driver header file.
 *
 ****************************************************************************************
 */

#ifndef HW_SYS_H_
#define HW_SYS_H_

/**
 *  The bit mask used for the shared memory offset
 */
#define SHARED_MEMORY_OFFSET_MASK    0x3FFFF

/**
 * !!!TODO
 * The start address of the shared memory. This is an imported symbol from the linker script
 */
extern uint8_t * shared_data_ptr;

/**
 * Macro to calculate the pointer in the IPC shared memory.
 * The memory address is calculated based on the start address of the shared memory and the given value which is considered as offset.
 * Only the 18 bits (bits 17:0) of the provided value are used.
 */
#define GET_SHARED_RAM_PTR(offset)        ((void *) ((uint32_t) shared_data_ptr + \
                                                     (uint32_t) (offset & SHARED_MEMORY_OFFSET_MASK)))

/**
 * Macro that can be used to extract the offset from the given address
 */

#define GET_SHARED_RAM_OFFSET(address)    ((uint32_t) (address - (uint32_t) shared_data_ptr))

#endif
