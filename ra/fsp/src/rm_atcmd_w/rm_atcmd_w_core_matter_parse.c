/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#ifdef __SUPPORT_MATTER_IOT__

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "FreeRTOS.h"
#include "event_groups.h"
#include <stdlib.h>
#include "rm_atcmd_w_core_matter_parse.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "rnDeviceWrapAPIs.h"
#include "rm_atcmd_transport_uart_w.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(atcmd)    "AT+M" # atcmd

#define RM_ATCMD_W_CORE_MATTER_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_MATTER_UNFIXED_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, uint8_t * p_in, size_t inlen)

#define RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_MATTER_ ## atcmd ## _brief_cb

#define RM_ATCMD_W_CORE_MATTER_DEBUG(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define RM_ATCMD_W_CORE_MATTER_ERROR(fmt, ...) // printf("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_MATTER_ATCMD_CB(CONFIG);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CONFIG);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CONFIG);

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(CONTROL);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CONTROL);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CONTROL);

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(STATUS);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(STATUS);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(STATUS);

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(ATTR);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(ATTR);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(ATTR);

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(AT);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(AT);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(AT);

RM_ATCMD_W_CORE_MATTER_UNFIXED_ATCMD_CB(CERT);
RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CERT);
RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CERT);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_matter_module[] =
{
    {
        RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(CONFIG),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(CONFIG),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(CONFIG),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(CONFIG)
    },
    {
        RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(CONTROL),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(CONTROL),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(CONTROL),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(CONTROL)
    },
    {
        RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(STATUS),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(STATUS),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(STATUS),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(STATUS)
    },
    {
        RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(ATTR),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(ATTR),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(ATTR),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(ATTR)
    },
    {
        RM_ATCMD_W_CORE_MATTER_ATCMD_CODE(AT),
        ATCMD_W_TYPE_A,
        5,
        0,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(AT),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(AT),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(AT)
    },
    {
        NULL,
        ATCMD_W_TYPE_MAX,
        0,
        0,
        NULL,
        NULL,
        NULL
    },
};

const atcmd_w_core_unfixed_module_t at_core_matter_unfixed_module[] =
{
    {
        PREFIX_MATTER_CERT,
        15,
        RM_ATCMD_W_CORE_MATTER_ATCMD_CB_P(CERT),
        RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB_P(CERT),
        RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB_P(CERT)
    },
    {
        "",
        0,
        NULL,
        NULL,
        NULL
    },
};

atcmd_w_ctrl_t * gp_matter_at_ctrl = NULL;

#define swap16(x)    (((x) & 0xff) << 8) | (((x) & 0xff00) >> 8)
EventGroupHandle_t matter_ota_evt;
#define MATTER_OTA_STATUS_EVT_OK     (0x1 << 0)
#define MATTER_OTA_STATUS_EVT_NOK    (0x1 << 1)
static const uint16_t RO_fast_crc_nbit_LUT[4][16] =
{
    {
        swap16(0x0000), swap16(0x3331), swap16(0x6662), swap16(0x5553),
        swap16(0xccc4), swap16(0xfff5), swap16(0xaaa6), swap16(0x9997),
        swap16(0x89a9), swap16(0xba98), swap16(0xefcb), swap16(0xdcfa),
        swap16(0x456d), swap16(0x765c), swap16(0x230f), swap16(0x103e)
    },
    {
        swap16(0x0000), swap16(0x0373), swap16(0x06e6), swap16(0x0595),
        swap16(0x0dcc), swap16(0x0ebf), swap16(0x0b2a), swap16(0x0859),
        swap16(0x1b98), swap16(0x18eb), swap16(0x1d7e), swap16(0x1e0d),
        swap16(0x1654), swap16(0x1527), swap16(0x10b2), swap16(0x13c1)
    },
    {
        swap16(0x0000), swap16(0x1021), swap16(0x2042), swap16(0x3063),
        swap16(0x4084), swap16(0x50a5), swap16(0x60c6), swap16(0x70e7),
        swap16(0x8108), swap16(0x9129), swap16(0xa14a), swap16(0xb16b),
        swap16(0xc18c), swap16(0xd1ad), swap16(0xe1ce), swap16(0xf1ef)
    },
    {
        swap16(0x0000), swap16(0x1231), swap16(0x2462), swap16(0x3653),
        swap16(0x48c4), swap16(0x5af5), swap16(0x6ca6), swap16(0x7e97),
        swap16(0x9188), swap16(0x83b9), swap16(0xb5ea), swap16(0xa7db),
        swap16(0xd94c), swap16(0xcb7d), swap16(0xfd2e), swap16(0xef1f)
    }
};

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
int matter_set_cert_configuration (int argc, char * argv[])
{
    unsigned char * data;
    uint32_t        data_len;

    if (argc < 4)
    {
        return 0;
    }

    data     = (unsigned char *) argv[3];
    data_len = atoi(argv[2]);

    switch (argv[1][4])
    {
        case '0':
        {
            set_matter_certification_declaration(data, data_len);
            break;
        }

        case '1':
        {
            set_matter_device_attestation_cert(data, data_len);
            break;
        }

        case '2':
        {
            set_matter_product_attestation_intermediate_cert(data, data_len);
            break;
        }

        case '3':
        {
            set_matter_device_attestation_privkey(data, data_len);
            break;
        }

        case '4':
        {
            set_matter_device_attestation_pubkey(data, data_len);
            break;
        }

        default:
        {
            break;
        }
    }

    return 0;
}

static uint16_t fast_crc_nbit_lookup (const void * data, int length, uint16_t CrcLUT[4][16], uint16_t previousCrc16)
{
    uint16_t         crc     = swap16(previousCrc16);
    const uint16_t * current = (const uint16_t *) data;

    while (length > 1)
    {
        uint16_t one = *current++ ^ (crc);

        crc = CrcLUT[0][(one >> 0) & 0x0f] ^ CrcLUT[1][(one >> 4) & 0x0f] ^ CrcLUT[2][(one >> 8) & 0x0f] ^
              CrcLUT[3][(one >> 12) & 0x0f];
        length -= 2;
    }

    if (length > 0)
    {
        uint16_t one = *current;

        one = ((one ^ crc) << 8);
        crc = crc >> 8;

        crc = crc ^ CrcLUT[0][(one >> 0) & 0x0f] ^ CrcLUT[1][(one >> 4) & 0x0f] ^ CrcLUT[2][(one >> 8) & 0x0f] ^
              CrcLUT[3][(one >> 12) & 0x0f];
    }

    return swap16(crc);
}

static uint16_t swcrc16 (uint8_t * data, uint16_t length, uint16_t prevCrc16)
{
    uint16_t crcdata;
    crcdata = fast_crc_nbit_lookup(data, length, (uint16_t(*)[16])RO_fast_crc_nbit_LUT, prevCrc16);

    return crcdata;
}

UINT app_writeDataToMCU (UINT offset, UINT tot_len, UINT r_len, UINT * srcMemAddr, UINT size)
{
    UINT8      * imgbuffer;
    UINT32       imgsize;
    UINT32       malloc_size;
    unsigned int mask_evt;
    unsigned int matter_evt_mask = MATTER_OTA_STATUS_EVT_OK | MATTER_OTA_STATUS_EVT_NOK;

    static ota_mcu_fw_stream_info_t imginfo;
    if (matter_ota_evt == NULL)
    {
        matter_ota_evt = xEventGroupCreate();
        if (matter_ota_evt == NULL)
        {
            printf("[%s] matter_ota_evt Event Flags Create Error!\n", __func__);

            return 0;
        }
    }

    malloc_size = size + OTA_MCU_FW_STREAM_HEADER_SIZE + 17;
    imgbuffer   = (UINT8 *) pvPortMalloc(malloc_size);
    if (imgbuffer != NULL)
    {
        sprintf((char *) imgbuffer, "\r\n+MATOTA=");
        imgsize = strlen((char *) imgbuffer);
        memcpy(&imgbuffer[imgsize + OTA_MCU_FW_STREAM_HEADER_SIZE], srcMemAddr, size);
        imginfo.offset          = offset;
        imginfo.content_length  = tot_len;
        imginfo.received_length = r_len;
        imginfo.size            = size;
        imginfo.crc             = swcrc16(&imgbuffer[imgsize + OTA_MCU_FW_STREAM_HEADER_SIZE], size, 0);
        if (offset == 0)
        {
            imginfo.imgcrc = 0;
        }

        imginfo.imgcrc = swcrc16(&imgbuffer[imgsize + OTA_MCU_FW_STREAM_HEADER_SIZE], size, imginfo.imgcrc);
        memcpy(&imgbuffer[imgsize], &imginfo, OTA_MCU_FW_STREAM_HEADER_SIZE);
        imgsize += OTA_MCU_FW_STREAM_HEADER_SIZE + size;
        memcpy(&imgbuffer[imgsize], "\r\n", 2);
        imgsize += 2;
          
        RM_ATCMD_W_CORE_Write(gp_matter_at_ctrl, (uint8_t *) imgbuffer, imgsize);

        mask_evt = xEventGroupWaitBits(matter_ota_evt, matter_evt_mask, pdTRUE, pdFALSE, 1000);

        if (!(mask_evt & MATTER_OTA_STATUS_EVT_OK))
        {
            printf("[%s] OTA res fail 0x%x 0x%x\n", __func__, mask_evt, matter_evt_mask);
            vPortFree(imgbuffer);

            return 0;
        }

        vPortFree(imgbuffer);
    }
    else
    {
        printf("[%s] Malloc fail!\n", __func__);

        return 0;
    }

    return size;
}

/***********************************************************************************************************************
 * AT Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_MATTER_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    if (p_list->unfixed_module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    if (rm_atcmd_w_core_register_module_node(p_list, at_core_matter_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    if (rm_atcmd_w_core_register_unfixed_module_node(p_list,
                                                     at_core_matter_unfixed_module) == FSP_ERR_AT_CMD_ERR_MEM_ALLOC)
    {
        return FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_MATTER_deregister (atcmd_w_core_module_list_t * p_list)
{
    atcmd_w_core_module_node_t         * p_module_node;
    atcmd_w_core_module_node_t         * p_module_prev = NULL;
    atcmd_w_core_unfixed_module_node_t * p_unfixed_module_node;
    atcmd_w_core_unfixed_module_node_t * p_unfixed_module_prev = NULL;

#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    for (p_module_node = p_list->p_module_head; p_module_node != NULL;
         p_module_prev = p_module_node, p_module_node = p_module_node->next)
    {
        if (p_module_node->module == at_core_matter_module)
        {
            if (p_module_node == p_list->p_module_head) // First node
            {
                p_list->p_module_head = p_list->p_module_head->next;
            }
            else
            {
                p_module_prev->next = p_module_node->next;
            }

            vPortFree(p_module_node);
            p_list->module_cnt--;
            break;
        }
    }

    for (p_unfixed_module_node = p_list->p_unfixed_module_head; p_unfixed_module_node != NULL;
         p_unfixed_module_prev = p_unfixed_module_node, p_unfixed_module_node = p_module_node->next)
    {
        if (p_unfixed_module_node->module == at_core_matter_unfixed_module)
        {
            if (p_unfixed_module_node == p_list->p_unfixed_module_head) // First node
            {
                p_list->p_unfixed_module_head = p_list->p_unfixed_module_head->next;
            }
            else
            {
                p_unfixed_module_prev->next = p_unfixed_module_node->next;
            }

            vPortFree(p_unfixed_module_node);
            p_list->unfixed_module_cnt--;
            break;
        }
    }

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_MATTER_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    gp_matter_at_ctrl = p_at_ctrl;

    return err;
}

uint32_t RM_ATCMD_W_CORE_MATTER_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    gp_matter_at_ctrl = NULL;

    return err;
}

void RM_MATTER_PRINTF_ATCMD (char * p_str)
{
    RM_ATCMD_W_CORE_Write(gp_matter_at_ctrl, (uint8_t *) p_str, strlen(p_str));
}

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(CONFIG)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    // atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    int    i;
    int    k          = 0;
    char * params[20] = {0, };

    printf("\n======================================================= \n");
    printf("argc num = %d \n", argc);
    for (i = 0; i < 20; i++)
    {
        if (argv[i] == NULL)
        {
            break;
        }

        if ((strncasecmp(argv[1], "CERT", 4) == 0) && (i == 3))
        {
            continue;
        }

        printf("argv[%d]: %s\n", i, argv[i]);
    }

    printf("\n======================================================= \n");

    if (argc > 2)
    {
        k = 1;
        for (int j = 0; j < i - 1; j++)
        {
            params[j] = argv[k];
            k++;
        }
    }
    else
    {
        params[k] = (char *) strtok(argv[1], " ");

        while (params[k] != NULL)
        {
            k++;
            params[k] = strtok(NULL, " ");
        }
    }

    set_matter_config(params[0], params[1], params[2], params[3], params[4], params[6]);

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CONFIG)
{
    const char * p_usage = "<parameter>,[<size>],<value>";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CONFIG)
{
    const char * p_descrption = "Set Matter parameters";

    return p_descrption;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(CONTROL)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    // atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;

    if (strncasecmp(argv[1], "BLE", 3) == 0)
    {
        if (strncasecmp(argv[2], "STOP", 4) == 0)
        {
            set_matter_ble_adv_control(0);
        }
        else if (strncasecmp(argv[2], "START", 5) == 0)
        {
            set_matter_ble_adv_control(1);
        }
    }

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CONTROL)
{
    const char * p_usage = "<module>,<command>";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CONTROL)
{
    const char * p_descrption = "Control Matter";

    return p_descrption;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(STATUS)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    int  at_resp_len  = 0;
    char at_resp[100] = {0, };

    if (argc > 1)
    {
        memset(at_resp, 0x00, 100);
        memcpy(at_resp, argv[1], strlen(argv[1]));
        if (strncasecmp(argv[1], "BRDINFO", 7) == 0)
        {
            get_matter_onboardingcodes(argv[2]);
        }
    }
    else
    {
        at_resp_len = sprintf(at_resp, "\r\n+MSTATUS=%d\r\n", app_ext_status_get());
        RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
    }

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(STATUS)
{
    const char * p_usage = "[<BRDINFO>]";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(STATUS)
{
    const char * p_descrption = "Get Matter status";

    return p_descrption;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(ATTR)
{
    fsp_err_atcmd_err_code         err    = FSP_ERR_AT_CMD_ERR_CMD_OK;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    int  at_resp_len  = 0;
    char at_resp[100] = {0, };

    if (argc > 6)
    {
        int    i;
        int    k          = 0;
        char * params[10] = {0, };

        for (i = 0; i < 20; i++)
        {
            if (argv[i] == NULL)
            {
                break;
            }
        }

        if (argc > 2)
        {
            k = 1;
            for (int j = 0; j < i - 1; j++)
            {
                params[j] = argv[k];
                k++;
            }
        }
        else
        {
            params[k] = (char *) strtok(argv[1], " ");

            while (params[k] != NULL)
            {
                k++;
                params[k] = strtok(NULL, " ");
            }
        }

        uint16_t endpoint    = atoi(params[0]);
        uint32_t cluster     = atoi(params[1]);
        uint32_t attributeID = atoi(params[2]);
        uint32_t data        = atoi(params[3]);
        uint8_t  write       = atoi(params[5]);

        if (write)
        {
            uint8_t dataType = atoi(params[4]);
            set_matter_attribute_control(endpoint, cluster, attributeID, (uint8_t *) &data, dataType);
            printf("\r\n matter_attribute w: %d (%s, %s, %s, %s, %s, %s, %s)\r\n",
                   argc,
                   params[0],
                   params[1],
                   params[2],
                   params[3],
                   params[4],
                   params[5],
                   params[6]);
        }
        else
        {
            uint16_t datalen = atoi(params[4]);
            get_matter_attribute_control(endpoint, cluster, attributeID, (uint8_t *) &data, datalen);
            printf("\r\n matter_attribute r: %d, %ld, %ld, %lu, %d\r\n", endpoint, cluster, attributeID, data, datalen);
            at_resp_len = sprintf(at_resp, "\r\n+MATTR=%ld,%ld,%lu\r\n", cluster, attributeID, data);
            RM_ATCMD_W_CORE_Write(p_ctrl, (uint8_t *) at_resp, at_resp_len);
        }
    }
    else
    {
        printf("+MATTR Wrong Parameter\n");
    }

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(ATTR)
{
    const char * p_usage =
        "<endpoint-id>,<cluster-id>,<attribute-id>,<attribute-data>,<data-type(w)/data-length(r)>,<write/read>";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(ATTR)
{
    const char * p_descrption = "Set Matter attribute";

    return p_descrption;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_CB(AT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    int    i;
    int    k          = 0;
    char * params[10] = {0, };

    for (i = 0; i < 20; i++)
    {
        if (argv[i] == NULL)
        {
            break;
        }
    }

    if (argc > 2)
    {
        k = 1;
        for (int j = 0; j < i - 1; j++)
        {
            params[j] = argv[k];
            k++;
        }
    }
    else
    {
        params[k] = (char *) strtok(argv[1], " ");

        while (params[k] != NULL)
        {
            k++;
            params[k] = strtok(NULL, " ");
        }
    }

    if (argc > 1)
    {
        if (strncasecmp(params[0], "OK_OTA", 6) == 0)
        {
            BaseType_t xHigherPriorityTaskWoken, xResult;
            xHigherPriorityTaskWoken = pdFALSE;
            xResult = xEventGroupSetBitsFromISR(matter_ota_evt, MATTER_OTA_STATUS_EVT_OK, &xHigherPriorityTaskWoken);
            if (xResult != pdFAIL)
            {
                /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
                 * switch should be requested.  The macro used is port specific and will
                 * be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
                 * the documentation page for the port being used. */
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
        else if (strncasecmp(params[0], "OK_OTA_FAIL", 11) == 0)
        {
            BaseType_t xHigherPriorityTaskWoken, xResult;
            xHigherPriorityTaskWoken = pdFALSE;
            xResult = xEventGroupSetBitsFromISR(matter_ota_evt, MATTER_OTA_STATUS_EVT_NOK, &xHigherPriorityTaskWoken);
            if (xResult != pdFAIL)
            {
                /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
                 * switch should be requested.  The macro used is port specific and will
                 * be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() - refer to
                 * the documentation page for the port being used. */
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(AT)
{
    const char * p_usage = "[<BRDINFO>]";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(AT)
{
    const char * p_descrption = "getting Matter status";

    return p_descrption;
}

RM_ATCMD_W_CORE_MATTER_UNFIXED_ATCMD_CB(CERT)
{
    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              fsp_err = FSP_SUCCESS;

    typedef enum
    {
        READ_CERT_IDX,                 // 0
        READ_CERT_LENGTH,              // 1
        READ_CERT_DATA,                // 2
    } atcmd_esc_cert_cmd_parameter_step;

    char param_atcmd[40] = {0x00, };
    int  param_atcmd_idx = 0;

    int  done = false;
    char ch   = 0;
    atcmd_esc_cert_cmd_parameter_step param_step = READ_CERT_IDX;

    int cert_len = 0;
    int cert_id  = 0;
    int cert_idx = 0;

    unsigned char * p_cert = NULL;

    char resp_str[32] = {0x00, };

    /* AT+MCONFIG=CERT[Index],<length>,<content> */

    /* Input comma(,) */
    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

    if ((FSP_SUCCESS != fsp_err) || ((ch < 0x30) || (ch > 0x39)))
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        goto end;
    }

    cert_id = ch - 0x30;

    while (err == FSP_ERR_AT_CMD_ERR_CMD_OK && !done)
    {
        switch (param_step)
        {
            case READ_CERT_IDX:
            case READ_CERT_LENGTH:
            {
                memset(param_atcmd, 0x00, sizeof(param_atcmd));
                param_atcmd_idx = 0;
                ch              = 0x00;

                while (ch != 0x2C)
                {
                    if (param_atcmd_idx >= sizeof(param_atcmd))
                    {
                        err = FSP_ERR_AT_CMD_ERR_TOO_LONG_RESULT;
                        break;
                    }

                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    param_atcmd[param_atcmd_idx++] = ch;
                }

                /* Update param step */
                if (param_step == READ_CERT_IDX)
                {
                    param_step = READ_CERT_LENGTH;
                }
                else if (param_step == READ_CERT_LENGTH)
                {
                    param_atcmd[param_atcmd_idx - 1] = '\0';

                    if (rm_atcmd_w_core_common_stoi(param_atcmd, &cert_len, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

                    param_step = READ_CERT_DATA;
                }

                break;
            }

            case READ_CERT_DATA:
            {
                p_cert = pvPortMalloc(cert_len + 1);

                if (p_cert == NULL)
                {
                    err = FSP_ERR_AT_CMD_ERR_MEM_ALLOC;
                    break;
                }

                memset(p_cert, 0x00, (cert_len + 1));

                for (int idx = 0; idx < cert_len; idx++)
                {
                    fsp_err = RM_ATCMD_W_CORE_DataRead(p_at_ctrl, (uint8_t *) &ch, sizeof(char));

                    if (fsp_err != FSP_SUCCESS)
                    {
                        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
                        break;
                    }

                    p_cert[cert_idx] = ch;
                    cert_idx++;
                }

                if (err != FSP_ERR_AT_CMD_ERR_CMD_OK)
                {
                    break;
                }

                done = true;
                break;
            }

            default:
            {
                break;
            }
        }
    }

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        /* Store certificate */
        switch (cert_id)
        {
            case 0:
            {
                fsp_err = set_matter_certification_declaration(p_cert, cert_len);

                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                }

                break;
            }

            case 1:
            {
                fsp_err = set_matter_device_attestation_cert(p_cert, cert_len);

                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                }

                break;
            }

            case 2:
            {
                fsp_err = set_matter_product_attestation_intermediate_cert(p_cert, cert_len);

                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                }

                break;
            }

            case 3:
            {
                fsp_err = set_matter_device_attestation_privkey(p_cert, cert_len);

                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                }

                break;
            }

            case 4:
            {
                fsp_err = set_matter_device_attestation_pubkey(p_cert, cert_len);

                if (fsp_err != FSP_SUCCESS)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                }

                break;
            }

            default:
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                break;
            }
        }
    }

end:

    if (err == FSP_ERR_AT_CMD_ERR_CMD_OK)
    {
        bsp_safe_strcpy(resp_str, "\r\nOK\r\n", sizeof(resp_str));
    }
    else
    {
        sprintf(resp_str, "\r\nERROR:%d\r\n", err);
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));

    if (p_cert)
    {
        vPortFree(p_cert);
        p_cert = NULL;
    }

    return err;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_FORMAT_CB(CERT)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_MATTER_ATCMD_BRIEF_CB(CERT)
{
    const char * p_description = "";

    return p_description;
}
#endif                                 /* __SUPPORT_MATTER_IOT__ */
