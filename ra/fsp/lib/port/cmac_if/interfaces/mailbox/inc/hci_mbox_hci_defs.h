/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#if !defined(HCI_MBOX_HCI_DEFS_H)
#define HCI_MBOX_HCI_DEFS_H

/** \name Packet definitions
 *
 */

/**@{*/
#define HCI_CMD_HDR_LEN          3      /*!< Command packet header length */
#define HCI_ACL_HDR_LEN          4      /*!< ACL packet header length */
#define HCI_SYN_HDR_LEN          3      /*!< Synchronous packet header length */
#define HCI_ISO_HDR_LEN          4      /*!< ISO packet header length */
#define HCI_EVT_HDR_LEN          2      /*!< Event packet header length */
#define HCI_EVT_PARAM_MAX_LEN    255    /*!< Maximum length of event packet parameters */
#define HCI_ACL_DEFAULT_LEN      27     /*!< Default maximum ACL packet length */

/* See BLUETOOTH CORE SPECIFICATION Version 5.2 | Vol 4, Part E page 1892 Figure 5.2. */
#define HCI_PB_FLAG_MASK         0x3000 /*!< ACL packet boundary flag mask */
#define HCI_PB_START_H2C         0x0000 /*!< Packet boundary flag, start, host-to-controller */
#define HCI_PB_CONTINUE          0x1000 /*!< Packet boundary flag, Continuing fragment of
                                         * a higher layer message. */
#define HCI_PB_START_C2H         0x2000 /*!< Packet boundary flag, start, controller-to-host */
#define HCI_BC_FLAG_MASK         0xC000 /*!< ACL Broadcast Flag mask */
#define HCI_BC_APB_U             0x4000 /*!< BR/EDR broadcast (APB-U) */

/**@}*/

/** \name Packet types
 *
 */
#define HCI_CMD_TYPE             0x01   /*!< HCI command packet */
#define HCI_ACL_TYPE             0x02   /*!< HCI ACL data packet */
#define HCI_SYN_TYPE             0x03   /*!< HCI synchronous data packet */
#define HCI_EVT_TYPE             0x04   /*!< HCI event packet */
#define HCI_ISO_TYPE             0x05   /*!< HCI ISO data packet */
/**@}*/

#define HCI_HANDLE_MASK          0x0FFF /*!< Mask for handle bits in ACL or Synchronous Data packet */
#define HCI_HANDLE_NONE          0xFFFF /*!< Value for invalid handle */

#endif /* HCI_MBOX_HCI_DEFS_H */
