/*
 *
 *    Copyright (c) 2023 Renesas Electronics Corporation and/or its  affiliates
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0

 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
/**
 ****************************************************************************************
 *
 * @file rn_status.h
 *
 * @brief Renesas Status Codes.
 *
 ****************************************************************************************
 */

#ifndef RN_STATUS_H
#define RN_STATUS_H

// -----------------------------------------------------------------------------
// Status Defines

// -----------------------------------------------------------------------------
// Generic Errors

#define RN_STATUS_OK    ((rn_status_t)0x0000)  ///< No error.
#define RN_STATUS_FAIL  ((rn_status_t)0x0001)  ///< Generic error.

// State Errors
#define RN_STATUS_INVALID_STATE         ((rn_status_t)0x0002)
#define RN_STATUS_NOT_SUPPORTED         ((rn_status_t)0x000F)

// Invalid Parameters Errors
#define RN_STATUS_INVALID_PARAMETER     ((rn_status_t)0x0010)

// Bluetooth controller status codes
#define RN_STATUS_BT_CTRL_REMOTE_USER_TERMINATED                                      ((rn_status_t)0x0101)
#define RN_STATUS_BT_CTRL_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_LOW_RESOURCES    ((rn_status_t)0x0102)
#define RN_STATUS_BT_CTRL_REMOTE_POWERING_OFF                                         ((rn_status_t)0x0103)
#define RN_STATUS_BT_CTRL_CONNECTION_TERMINATED_BY_LOCAL_HOST                         ((rn_status_t)0x0104)

// Bluetooth attribute status codes
#define RN_STATUS_BT_ATT_INVALID_ATT_LENGTH                                           ((rn_status_t)0x0201)

// -----------------------------------------------------------------------------
// Data Types

typedef uint32_t rn_status_t;

#endif /* RN_STATUS_H */
