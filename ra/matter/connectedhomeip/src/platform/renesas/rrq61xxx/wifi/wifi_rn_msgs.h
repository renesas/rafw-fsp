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


#ifndef _WIFI_RN_MSGS_H_
#define _WIFI_RN_MSGS_H_

#include <sys/types.h>

typedef struct
{
    uint8_t octet[6];
} wifi_mac_address_t;

/**
 * General Message header structure
 */
typedef struct wifi_header_s
{
    uint16_t length;
    uint8_t id;
    uint8_t info;
} wifi_header_t;

/**
 * Generic message structure for all requests, confirmations and indications
 */
typedef struct wifi_generic_message_s
{
    wifi_header_t header;
    uint8_t body[];
} wifi_generic_message_t;

/**
 * Startup Indication message.
 */
typedef struct wifi_startup_ind_body_s
{
    uint32_t status;
    uint8_t mac_addr[6];
} wifi_startup_ind_body_t;

typedef struct wifi_startup_ind_s
{
    wifi_header_t header;
    wifi_startup_ind_body_t body;
} wifi_startup_ind_t;

/**
 * Indication message body for wifi_connect_ind_t.
 */
typedef struct wifi_connect_ind_body_s
{
    // Status of the connection request.
    uint32_t status;
    // MAC address of the connected access point.
    uint8_t mac[6];
    // Channel of the connected access point.
    // <B>1 - 13</B>: Channel number.
    uint16_t channel;
} wifi_connect_ind_body_t;

/**
 * Indication message used to signal the completion of a connection operation.
 */
typedef struct wifi_connect_ind_s
{
    // Common message header
    wifi_header_t header;
    // Indication message body
    wifi_connect_ind_body_t body;
} wifi_connect_ind_t;

/**
 * Indication message body for wifi_disconnect_ind_t.
 */
typedef struct wifi_disconnect_ind_body_s
{
    // MAC address of the access point
    uint8_t mac[6];
    // Reason for disconnection.
    uint16_t reason;
} wifi_disconnect_ind_body_t;

/**
 * Indication message used to signal the completion of a disconnection operation.
 */
typedef struct wifi_disconnect_ind_s
{
    // Common message header
    wifi_header_t header;
    // Indication message body
    wifi_disconnect_ind_body_t body;
} wifi_disconnect_ind_t;

#endif /* _WIFI_RN_MSGS_H_ */
