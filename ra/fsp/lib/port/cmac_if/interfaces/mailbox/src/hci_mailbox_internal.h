/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * @file hci_mbox_internal.h
 *
 * @brief Internal definitions used by HCI Mailbox implementation
 *
 */
#if !defined(HCI_MBOX_INTERNAL_H)
#define HCI_MBOX_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/*
 *      Defines
 */

/**
 *  @brief Packed. Aligned Structure defintions for the packet
 *  and configuration representation
 */
#define __HCI_MBOX_PACKED_STRUCT    struct __attribute__((packed, aligned(1)))

/*
 *      Data conversion
 */

/** @brief convert little endian byte buffer to uint16_t. */
#define BYTES_TO_UINT16(n, p)                               \
    {                                                       \
        n = ((uint16_t) (p)[0] + ((uint16_t) (p)[1] << 8)); \
    }

/** @brief convert little endian byte stream to uint16_t, incrementing two bytes. */
#define BSTREAM_TO_UINT16(n, p) \
    {                           \
        BYTES_TO_UINT16(n, p);  \
        p += 2;                 \
    }

/** @brief convert uint16_t to little endian byte stream, incrementing two bytes. */
#define UINT16_TO_BSTREAM(p, n)        \
    {                                  \
        *(p)++ = (uint8_t) (n);        \
        *(p)++ = (uint8_t) ((n) >> 8); \
    }

/*
 *      HCI Mailbox Events
 */

// Sub-event codes of HCI Mailbox
#define HCI_MBOX_EVENT_H2C_TRIGGER_HCI_RESET_CMPLT    1U  // Request Controller for HCI_RESET confirmation
#define HCI_MBOX_EVENT_H2C_TRIGGER_HCI_HW_ERR         2U  // Request Controller for HW Error Code
#define HCI_MBOX_EVENT_H2C_REQUEST_CTRL_CONFIG        3U  // Request Controller for the configuration
#define HCI_MBOX_EVENT_C2H_CMAC_READY                 4U  // Notify HCI Mailbox Host that the Controller is ready
#define HCI_MBOX_EVENT_C2H_CTRL_CONFIG                5U  // Notify HCI Mailbox Host that the configuration was sent

// Other definitions
#define MBOX_BT_HCI_CONFIG_DATA_SIZE                  12U // the size of mbox_bt_hci_cfg_t struct
#if defined(MAILBOX_API_V0_COMPATIBILITY)
 #define MBOX_PKT_HEADER_SZ                           2U  // the size of mailbox packet header. [len LSB, len MSB] */
#else
 #define MBOX_PKT_HEADER_SZ                           3U  // the size of mailbox packet header. [len LSB, len MSB, protocol] */
#endif

// Event helper functions
#define _HCI_MBOX_EVENT_CODE_MASK                     0xFF
#define _HCI_MBOX_EVENT_DATA_POS                      8U
#define _HCI_MBOX_EVENT_DATA_MASK                     0xFFFF

/**
 *  @brief Create HCI Mailbox Event with code c and payload d
 */
#define HCI_MBOX_EVENT_CREATE_PAYLOAD(c, d)                \
    (ipc_event_payload) ((c & _HCI_MBOX_EVENT_CODE_MASK) | \
                         ((d & _HCI_MBOX_EVENT_DATA_MASK) << _HCI_MBOX_EVENT_DATA_POS))

/**
 *  @brief Return event code of given event
 */
#define HCI_MBOX_EVENT_GET_CODE(p)    (uint8_t) (p & _HCI_MBOX_EVENT_CODE_MASK)

/**
 *  @brief Return event payload of given event
 */
#define HCI_MBOX_EVENT_GET_DATA(p) \
    (uint16_t) ((p >> _HCI_MBOX_EVENT_DATA_POS) & _HCI_MBOX_EVENT_DATA_MASK)

/**
 *  @brief HCI Packet types
 */
typedef __HCI_MBOX_PACKED_STRUCT
{
    uint8_t  pkt_type;
    uint16_t opcode;
    uint8_t  length;
    uint8_t  parameters[];
}
hci_mbox_pkt_hci_cmd_t;

typedef __HCI_MBOX_PACKED_STRUCT
{
    uint8_t  pkt_type;
    uint16_t handle;
    uint16_t length;
    uint8_t  parameters[];
}
hci_mbox_pkt_hci_acl_t;

typedef __HCI_MBOX_PACKED_STRUCT
{
    uint8_t  pkt_type;
    uint16_t handle;
    uint8_t  length;
    uint8_t  parameters[];
}
hci_mbox_pkt_hci_syn_t;

typedef __HCI_MBOX_PACKED_STRUCT
{
    uint8_t pkt_type;
    uint8_t code;
    uint8_t length;
    uint8_t parameters[];
}
hci_mbox_pkt_hci_evt_t;

typedef __HCI_MBOX_PACKED_STRUCT
{
    uint8_t  pkt_type;
    uint16_t handle;
    uint16_t length;
    uint8_t  parameters[];
}
hci_mbox_pkt_hci_iso_t;

#endif                                 /* HCI_MBOX_INTERNAL_H */
