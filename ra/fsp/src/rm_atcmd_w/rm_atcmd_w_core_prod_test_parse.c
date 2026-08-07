/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#if CFG_WIFI
 #include <stdlib.h>
 #include "rm_atcmd_w_core_prod_test_parse.h"
 #include "rm_atcmd_w_core_err_code.h"
 #include "rm_atcmd_w_core.h"
 #include "r_ospi_w.h"
 #include "bsp_otp.h"
 #include "r_adc_w.h"

 #include "rm_pmgr_w_instance.h"
 #include "r_pm_if.h"

 #include "r_gpio_w.h"
 #include "common_data.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(atcmd)    "AT+" # atcmd

 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _format_cb(void)
 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _brief_cb(void)

 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _cmd_cb
 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _format_cb
 #define RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_PROD_TEST_ ## atcmd ## _brief_cb

 #define GPIO_PROD_PIN_CFG      ((uint32_t) GPIO_W_PERIPHERAL_GPIO |          \
                                 (uint32_t) GPIO_W_CFG_PORT_DIRECTION_INPUT | \
                                 (uint32_t) GPIO_W_CFG_PULLDOWN_ENABLE)
 #define PORT_MASK              (0xFF00)
 #define PIN_MASK               (0x00FF)

 #define FLASH_READ_ID_CMD      (0x9F)
 #define FLASH_ID_LENGTH        (0x03)
 #define FLASH_MANUFACTURE      (0x1f)
 #define FLASH_MEMORY_TYPE      (0x43)
 #define FLASH_CAPACITY_TYPE    (0x17)

 #define BOOT_APP_ADDRESS       0x28600000

 #define PROD_UNSUPPORT         1

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODPWRAIL);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODPWRAIL);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODPWRAIL);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODSLEEP);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODSLEEP);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODSLEEP);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(UOTPWRASC);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(UOTPWRASC);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(UOTPWRASC);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(UOTPRDASC);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(UOTPRDASC);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(UOTPRDASC);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSMACWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSMACWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSMACWR);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSMACRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSMACRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSMACRD);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSXTALWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSXTALWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSXTALWR);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSXTALRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSXTALRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSXTALRD);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTALWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTALWR);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTALWR);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTALRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTALRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTALRD);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSWAFERRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSWAFERRD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSWAFERRD);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSADD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSADD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSADD);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(CHIPTEMP);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(CHIPTEMP);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(CHIPTEMP);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODGPIOSET);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODGPIOSET);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODGPIOSET);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODGPIOGET);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODGPIOGET);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODGPIOGET);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODQSPI);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODQSPI);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODQSPI);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PROD_GPIO_RUN);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PROD_GPIO_RUN);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PROD_GPIO_RUN);

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTAL32K_OUT);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTAL32K_OUT);
RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTAL32K_OUT);

static uint32_t custom_gpio_test(void);

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_prod_test_module[] =
{
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PRODGPIOSET),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PRODGPIOSET),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PRODGPIOSET),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PRODGPIOSET)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PRODGPIOGET),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PRODGPIOGET),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PRODGPIOGET),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PRODGPIOGET)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PROD_GPIO_RUN),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PROD_GPIO_RUN),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PROD_GPIO_RUN),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PROD_GPIO_RUN),
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(XTAL32K_OUT),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(XTAL32K_OUT),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(XTAL32K_OUT),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(XTAL32K_OUT),
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PRODQSPI),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PRODQSPI),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PRODQSPI),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PRODQSPI)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PRODPWRAIL),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PRODPWRAIL),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PRODPWRAIL),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PRODPWRAIL)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(PRODSLEEP),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(PRODSLEEP),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(PRODSLEEP),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(PRODSLEEP)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(UOTPWRASC),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(UOTPWRASC),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(UOTPWRASC),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(UOTPWRASC)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(UOTPRDASC),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(UOTPRDASC),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(UOTPRDASC),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(UOTPRDASC)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSMACWR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSMACWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSMACWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSMACWR)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSMACRD),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSMACRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSMACRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSMACRD)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSXTALWR),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSXTALWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSXTALWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSXTALWR)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSXTALRD),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSXTALRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSXTALRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSXTALRD)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSWAFERRD),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSWAFERRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSWAFERRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSWAFERRD)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(XTALWR),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(XTALWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(XTALWR),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(XTALWR)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(XTALRD),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(XTALRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(XTALRD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(XTALRD)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(TCSADD),
        ATCMD_W_TYPE_A,
        1,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(TCSADD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(TCSADD),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(TCSADD)
    },
    {
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CODE(CHIPTEMP),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB_P(CHIPTEMP),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB_P(CHIPTEMP),
        RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB_P(CHIPTEMP)
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

static const adc_w_channel_cfg_t g_atcmd_adc_w_channel_cfg0_default =
{
    .dma_en              = ADC_W_DMA_DISABLED,
    .interrupt_mode_fifo = ADC_W_INTERRUPT_FIFO_NONE,
    .interrupt_mode_thd  = ADC_W_INTERRUPT_THD_NONE,
    .sensorwakeup_en     = ADC_W_SENSOR_WAKEUP_DISABLED,
    .thd_value           = 0,
    .threshold_mode      = ADC_W_SENSOR_WAKEUP_THD_OVER,
};

static const adc_w_channel_cfg_t g_atcmd_adc_w_channel_cfg1_default =
{
    .dma_en              = ADC_W_DMA_DISABLED,
    .interrupt_mode_fifo = ADC_W_INTERRUPT_FIFO_NONE,
    .interrupt_mode_thd  = ADC_W_INTERRUPT_THD_NONE,
    .sensorwakeup_en     = ADC_W_SENSOR_WAKEUP_DISABLED,
    .thd_value           = 0,
    .threshold_mode      = ADC_W_SENSOR_WAKEUP_THD_OVER,
};

static const adc_w_channel_cfg_t g_atcmd_adc_w_channel_cfg2_default =
{
    .dma_en              = ADC_W_DMA_DISABLED,
    .interrupt_mode_fifo = ADC_W_INTERRUPT_FIFO_NONE,
    .interrupt_mode_thd  = ADC_W_INTERRUPT_THD_NONE,
    .sensorwakeup_en     = ADC_W_SENSOR_WAKEUP_DISABLED,
    .thd_value           = 0,
    .threshold_mode      = ADC_W_SENSOR_WAKEUP_THD_OVER,
};

static const adc_w_channel_cfg_t g_atcmd_adc_w_channel_cfg3_default =
{
    .dma_en              = ADC_W_DMA_DISABLED,
    .interrupt_mode_fifo = ADC_W_INTERRUPT_FIFO_NONE,
    .interrupt_mode_thd  = ADC_W_INTERRUPT_THD_NONE,
    .sensorwakeup_en     = ADC_W_SENSOR_WAKEUP_DISABLED,
    .thd_value           = 0,
    .threshold_mode      = ADC_W_SENSOR_WAKEUP_THD_OVER,
};

static const adc_w_extended_cfg_t g_atcmd_adc_cfg_ext_default =
{
    .conversion_clockdiv      = 3,
    .upper_bound_limit        = 0xC000,
    .lower_bound_limit        = 0x50,
    .p_channel_cfgs[0]        = &g_atcmd_adc_w_channel_cfg0_default,
    .p_channel_cfgs[1]        = &g_atcmd_adc_w_channel_cfg1_default,
    .p_channel_cfgs[2]        = &g_atcmd_adc_w_channel_cfg2_default,
    .p_channel_cfgs[3]        = &g_atcmd_adc_w_channel_cfg3_default,
    .timer_count_clock_source = ADC_W_TIMER_COUNT_SOURCE_8,
    .timer_value              = 0,
    .sample_average           = ADC_W_SAMPLE_AVERAGE_4,
};

const ioport_pin_cfg_t g_pin_cfg_data_prod[] =
{
    {
        .pin     = BSP_IO_PORT_00_PIN_05,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PULLUP_ENABLE),
    },
    {
        .pin     = BSP_IO_PORT_00_PIN_08,
        .pin_cfg = (uint32_t) (GPIO_W_CFG_PORT_DIRECTION_OUTPUT),
    },
};

const ioport_cfg_t g_bsp_pin_cfg_prod =
{
    .number_of_pins = (sizeof(g_pin_cfg_data_prod)) / (sizeof(ioport_pin_cfg_t)),
    .p_pin_cfg_data = &g_pin_cfg_data_prod[0],
};

#define RRQ61X_OSPI_W_ATCMD_SUPPORT

/***********************************************************************************************************************
 * Extern variables
 **********************************************************************************************************************/
extern atcmd_w_core_instance_t g_at_core_instance;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
uint32_t RM_ATCMD_W_CORE_PROD_TEST_register (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_prod_test_module);
}

uint32_t RM_ATCMD_W_CORE_PROD_TEST_deregister (atcmd_w_core_module_list_t * p_list)
{
 #if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
 #endif

    rm_atcmd_w_core_deregister(p_list, at_core_prod_test_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_PROD_TEST_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

uint32_t RM_ATCMD_W_CORE_PROD_TEST_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODPWRAIL)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    fsp_err_t              ret = FSP_SUCCESS;

    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_at_ctrl;
    adc_w_scan_cfg_t               g_atcmd_adc_w_scan_cfg;

    uint32_t channel = ADC_CHANNEL_0, i;
    uint32_t adc_avg = 0;
    uint16_t adc_data;
    uint8_t  power;
    char     result_str[64] = {0, };

    power = (uint8_t) rm_atcmd_w_core_common_atoi(argv[1]);

 #define PROD_GET_ADC_VAL(x)    ((x >> 4) & 0xfff)
 #define PROD_GPADC_SAMPLE_LENGTH        (32 * 5) // For decreasing values deviation.
 #define IOPORT_PERIPHERAL_ADC_ENABLE    ((uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT | \
                                          (uint32_t) GPIO_W_PERIPHERAL_ADC)

    if ((argc < 2) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        return err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 2)
    {
        return err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    /* Conversion from port, pin to channel */
    switch (power)
    {
        case 0:
        {
            if (RTC->READ_STATUS_REG_b.RF_LDO_RDY_VBAT_RD_STATUS)
            {
                return FSP_ERR_AT_CMD_ERR_CMD_OK;
            }
            else
            {
                return FSP_ERR_AT_CMD_ERR_NO_RESULT;
            }

            break;
        }

        case 1:
        {
            if (RTC->READ_STATUS_REG_b.DCDC_PA_OK_RD_STATUS)
            {
                return FSP_ERR_AT_CMD_ERR_CMD_OK;
            }
            else
            {
                return FSP_ERR_AT_CMD_ERR_NO_RESULT;
            }

            break;
        }

        case 2:
        {
            channel = ADC_CHANNEL_2;
            g_atcmd_adc_w_scan_cfg.scan_mask = (1U << ADC_CHANNEL_2);
            break;
        }

        case 3:
        {
            channel = ADC_CHANNEL_3;
            g_atcmd_adc_w_scan_cfg.scan_mask = (1U << ADC_CHANNEL_3);
            break;
        }

        default:
        {
            break;
        }
    }

    memcpy((adc_w_extended_cfg_t *) p_ctrl->p_cfg->p_adc->p_cfg->p_extend, &g_atcmd_adc_cfg_ext_default,
           sizeof(adc_w_extended_cfg_t));

    ret  = p_ctrl->p_cfg->p_adc->p_api->open(p_ctrl->p_cfg->p_adc->p_ctrl, p_ctrl->p_cfg->p_adc->p_cfg);
    ret |= p_ctrl->p_cfg->p_adc->p_api->scanCfg(p_ctrl->p_cfg->p_adc->p_ctrl, &g_atcmd_adc_w_scan_cfg);
    ret |= p_ctrl->p_cfg->p_adc->p_api->scanStart(p_ctrl->p_cfg->p_adc->p_ctrl);

    if (power == 2)
    {
        RTC->CNT_TESTI_REG_b.AUXADC12_CS            = 0x2;
        AUXADC->SWITCHING_MODE_REG_b.SWITCHING_MODE = 0x140;
        AUXADC->XADC12B_CTRL_REG_b.ADC12B_CS        = 0x6;
    }
    else if (power == 3)
    {
        RTC->CNT_TESTI_REG_b.AUXADC12_CS            = 0x3;
        AUXADC->SWITCHING_MODE_REG_b.SWITCHING_MODE = 0x180;
        AUXADC->XADC12B_CTRL_REG_b.ADC12B_CS        = 0x7;
    }

    if (ret)
    {
        return FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    for (i = 0; i < PROD_GPADC_SAMPLE_LENGTH; i++)
    {
        p_ctrl->p_cfg->p_adc->p_api->read(p_ctrl->p_cfg->p_adc->p_ctrl, channel, &adc_data);
        adc_avg += PROD_GET_ADC_VAL(adc_data);
        R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MICROSECONDS);
    }

    adc_avg = (adc_avg / PROD_GPADC_SAMPLE_LENGTH) & 0xfff;
    ret     = p_ctrl->p_cfg->p_adc->p_api->close(p_ctrl->p_cfg->p_adc->p_ctrl);

    sprintf(result_str, "\r\n+PRODPWRAIL:%d,%ld\r\n", power, adc_avg);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODPWRAIL)
{
    const char * p_usage = "<Select Power Rail>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODPWRAIL)
{
    const char * p_description = "Select power rail";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODSLEEP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
 #if CFG_PMGR
    char     result_str[64] = {0, };
    uint32_t mode;
    uint32_t sleep_time;

  #if defined(PRODTEST_DELAY_FOR_OK)

    // It need to take some delay to get the OK sent to Host.
    vTaskDelay(pdMS_TO_TICKS(PROD_PRINT_DELAY_TIME * 5));
  #endif

    if ((argc < 3) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 3)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        mode       = (uint32_t) rm_atcmd_w_core_common_atoi(argv[1]);
        sleep_time = (uint32_t) rm_atcmd_w_core_common_atoi(argv[2]);

        sprintf(result_str, "\r\nOK\r\n");
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

        R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
        if (mode == 2)
        {
            R_PM_LowPowerModeEnter(PMGR_LLD_POWER_MODE_SLEEP2, SEC_TO_RTC_TICKS(sleep_time));
        }
        else if (mode == 3)
        {
            ((uint32_t *) BOOT_APP_ADDRESS)[1] = 0;
            R_PM_LowPowerModeEnter(PMGR_LLD_POWER_MODE_SLEEP3, SEC_TO_RTC_TICKS(sleep_time));
        }
    }
 #endif

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODSLEEP)
{
    const char * p_usage = "<mode>,<time>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODSLEEP)
{
    const char * p_description = "Set into sleep mode.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(UOTPWRASC)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);

    /* Write raw data on OTP */
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               offset, data, ret_data;

    bsp_otp_init();
    offset = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[1]);
    data   = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[2]);

    if ((offset < 0x2C) || (offset > 0x1fe))
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto OTP_WRITE_END;
    }

    ret_data = bsp_otp_word_prog(data, offset);
    if (ret_data != data)
    {
        err = FSP_ERR_AT_CMD_ERR_PERI_XXX;
    }

OTP_WRITE_END:
    bsp_otp_close();

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(UOTPWRASC)
{
    const char * p_usage = "<offset>,<data>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(UOTPWRASC)
{
    const char * p_description = "Write OTP data.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(UOTPRDASC)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);

    /* Read raw data on OTP */
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               offset, rdata[64], cnt, i;
    char result_str[64] = {0, };

    bsp_otp_init();
    offset = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[1]);
    cnt    = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[2]);

    if (cnt > 64)
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        goto OTP_READ_END;
    }

    bsp_otp_read(rdata, offset, cnt);
    sprintf(result_str, "\r\n");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    for (i = 0; i < cnt; i++)
    {
        sprintf(result_str, "0x%08x ", (unsigned int) rdata[i]);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    }

    sprintf(result_str, "\r");
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

OTP_READ_END:
    bsp_otp_close();

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(UOTPRDASC)
{
    const char * p_usage = "<address>,<cnt>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(UOTPRDASC)
{
    const char * p_description = "Read OTP data.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTALWR)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);

    fsp_err_atcmd_err_code err  = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               trim = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[1]);
    CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_CCTRL = (trim & 0x7f);

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTALWR)
{
    const char * p_usage = "<X-TAL_trim>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTALWR)
{
    const char * p_description = "Write xtal trim";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTALRD)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char result_str[64]        = {0, };

    sprintf(result_str, "\r\n%02x\r\n", CRG_COM->XTAL40M_CTRL_REG_b.XTAL40M_CCTRL);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTALRD)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTALRD)
{
    const char * p_description = "read xtal trim.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSMACWR)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               i;
    uint32_t               option_check = 0;
    uint32_t               mac_addr[2]  = {0, };
    uint8_t                tmp_mac[6]   = {0, };

    if ((argc < 3) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 3)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        option_check = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[2]);

        for (i = 0; i < 6; i++)
        {
            tmp_mac[i] = (uint8_t) (rm_atcmd_w_core_common_htoi_char(argv[1][i * 2]) << 4 |
                                    rm_atcmd_w_core_common_htoi_char(argv[1][i * 2 + 1]));
        }

        /* Convert form MAC_TYPE of AT+COMMAND to it of TCS */
        memcpy(&mac_addr[0], tmp_mac + 2, 4);
        memcpy(((uint8_t *) &mac_addr[1]) + 2, tmp_mac, 2);

        mac_addr[0] = (tmp_mac[2] << 24 & 0xff000000) |
                      (tmp_mac[3] << 16 & 0xff0000) | (tmp_mac[4] << 8 & 0xff00) | (tmp_mac[5] & 0xff);
        mac_addr[1] = (tmp_mac[0] << 8 & 0xff00) | (tmp_mac[1] & 0xff);

        if (bsp_tcs_otp_write_mac(mac_addr, option_check))
        {
            err = FSP_ERR_AT_CMD_ERR_CMD_OK;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
        }
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSMACWR)
{
    const char * p_usage = "<mac>,<check_option>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSMACWR)
{
    const char * p_description = "Write MaRc address.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSMACRD)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err         = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               mac_addr[2] = {0, };
    char result_str[64]                = {0, };

    if (bsp_tcs_otp_read_mac(mac_addr))
    {
        sprintf(result_str, "\r\n%04lx%08lx\r\n", mac_addr[1] & 0xffff, mac_addr[0]);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

        err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSMACRD)
{
    const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSMACRD)
{
    const char * p_description = "Read Mac Address.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSXTALWR)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);

    fsp_err_atcmd_err_code err          = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               option_check = 0;
    uint32_t               value        = 0;

    option_check = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[2]);
    value        = (uint32_t) rm_atcmd_w_core_common_htoi_custom(argv[1]);

    value = (value & 0x7f0000) | 0x076f;

    if (bsp_tcs_otp_write_xtal(&value, option_check))
    {
        err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSXTALWR)
{
    const char * p_usage = "<X-TAL_trim>,<check_option>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSXTALWR)
{
    const char * p_description = "Write XTAL TRIM value.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSXTALRD)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err   = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               value = 0;
    char result_str[64]          = {0, };

    if (bsp_tcs_otp_read_xtal(&value))
    {
        sprintf(result_str, "\r\n0x%08lx\r\n", value);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSXTALRD)
{
    const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSXTALRD)
{
    const char * p_description = "Read X-TAL trim value.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSWAFERRD)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    fsp_err_atcmd_err_code err   = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               value = 0;
    char result_str[64]          = {0, };

    value = bsp_tcs_read_wafer();
    if (value)
    {
        sprintf(result_str, "\r\n0x%08lx\r\n", value);
        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
        err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSWAFERRD)
{
    const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSWAFERRD)
{
    const char * p_description = "Read WAFER value on TCS.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(TCSADD)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    FSP_PARAMETER_NOT_USED(argc);

    fsp_err_atcmd_err_code err      = FSP_ERR_AT_CMD_ERR_CMD_OK;
    uint32_t               tmp_len  = 0;
    uint32_t             * tmp_dest = NULL;

    /* Parameter validation */
    if ((argc < 2) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (argc > 2)
    {
        return FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    tmp_len = strlen(argv[1]);
    if ((tmp_len == 0) || ((tmp_len % 8) != 0))
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    /* Allocate memory with proper size check */
    size_t alloc_size = (tmp_len / 8);

    tmp_dest = (uint32_t *) malloc(alloc_size);
    if (tmp_dest == NULL)
    {
        return FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    /* Process data */
    for (uint32_t i = 0; i < alloc_size; i++)
    {
        uint32_t value = 0;
        for (uint32_t j = 0; j < 8; j++)
        {
            value |= rm_atcmd_w_core_common_htoi_char(argv[1][i * 8 + j]) << (28 - j * 4);
        }

        tmp_dest[i] = value;
    }

    if (!bsp_tcs_otp_add(tmp_dest, alloc_size))
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    free(tmp_dest);

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(TCSADD)
{
    const char * p_usage = "<tcs_data>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(TCSADD)
{
    const char * p_description = "Add tcs data";

    return p_description;
}

/* CHIPTEMP - Read chip temperature from WIFI_RFHPI TEMP_SENSOR_CAL register */
 #define WIFI_RFHPI_TEMP_SENSOR_CAL_ADDR    (0x60d0006cu)
 #define WIFI_RFHPI_TEMP_VALUE_MASK         (0x7Fu) /* bits [6:0]  */

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(CHIPTEMP)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

    uint32_t reg      = *(volatile uint32_t *) WIFI_RFHPI_TEMP_SENSOR_CAL_ADDR;
    uint32_t temp_val = reg & WIFI_RFHPI_TEMP_VALUE_MASK;

    /* Temperature (degC) = TEMP_VALUE * (-1.667) + 153
     * Computed in millidegrees using integer arithmetic to avoid float printf. */
    int32_t      temp_mdeg = (int32_t) temp_val * (-1667) + 153000;
    int32_t      abs_mdeg  = (temp_mdeg < 0) ? -temp_mdeg : temp_mdeg;
    int32_t      whole     = abs_mdeg / 1000;
    int32_t      frac2     = (abs_mdeg % 1000) / 10; /* two decimal digits */
    const char * p_sign    = (temp_mdeg < 0) ? "-" : "";

    char result_str[64] = {0, };
    snprintf(result_str, sizeof(result_str), "\r\n+CHIPTEMP:0x%02x,%s%d.%02d\r\n", (unsigned int) temp_val, p_sign,
             (int) whole, (int) frac2);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(CHIPTEMP)
{
    static const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(CHIPTEMP)
{
    static const char * p_description = "Read chip temperature (TEMP_VALUE_AFTER_PROCESS_CAL)";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PROD_GPIO_RUN)
{
    FSP_PARAMETER_NOT_USED(argc);

    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    char     result_str[48]    = {0, };
    uint32_t res               = custom_gpio_test();

    sprintf(result_str, AT_CMD_ENTER_NEW_LINE "%s PROD_GPIO_RUN: %u", rm_atcmd_w_core_common_strupr(argv[0] + 2),
            (unsigned int) res);

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));
    if (res != 0)
    {
        err = FSP_ERR_AT_CMD_ERR_NO_RESULT;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PROD_GPIO_RUN)
{
    const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PROD_GPIO_RUN)
{
    const char * p_description = "Display GPIO RUN Test";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODGPIOSET)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    gpio_w_cfg_options_t   cfg;
    gpio_w_peripheral_t    mode;

    uint8_t port;
    uint8_t pin;
    uint8_t cfg_num;
    uint8_t level;
    char    result_str[32] = {0x00, };
    int8_t  result;

    /* Argument check */
    if ((argc < 5) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 5)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        port    = (uint8_t) rm_atcmd_w_core_common_atoi(argv[1]);
        pin     = (uint8_t) rm_atcmd_w_core_common_atoi(argv[2]);
        cfg_num = (uint8_t) rm_atcmd_w_core_common_atoi(argv[3]);
        level   = (uint8_t) rm_atcmd_w_core_common_atoi(argv[4]);

        /* Check paramtater */
        if (BSP_FEATURE_IO_PORT_COUNT <= port)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
        }
        else if (BSP_IO_PORT_00 == port)
        {
            if (BSP_FEATURE_IO_PORT0_GPIO_COUNT <= pin)
            {
                err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            }
        }
        else if (BSP_IO_PORT_01 == port)
        {
            if (BSP_FEATURE_IO_PORT1_GPIO_COUNT <= pin)
            {
                err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            }
        }

        if (IOPORT_CFG_INVALID <= cfg_num)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
        }

        if (BSP_IO_LEVEL_HIGH < level)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }

        /* Check the Pin used */
        mode = (gpio_w_peripheral_t) (BSP_IO_PXX_MODE_REG(port, pin) & GPIO_P0_00_MODE_REG_PID_Msk);

        if (GPIO_W_PERIPHERAL_GPIO != mode)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
    }

    /* Normal processing */
    if (FSP_ERR_AT_CMD_ERR_CMD_OK == err)
    {
        /* Conversion from numeric to config parameters */
        switch (cfg_num)
        {
            case IOPORT_CFG_INPUT:
            {
                cfg = GPIO_W_CFG_PORT_DIRECTION_INPUT;
                break;
            }

            case IOPORT_CFG_INPUT_PULLUP:
            {
                cfg = GPIO_W_CFG_PULLUP_ENABLE;
                break;
            }

            case IOPORT_CFG_INPUT_PULLDOWN:
            {
                cfg = GPIO_W_CFG_PULLDOWN_ENABLE;
                break;
            }

            case IOPORT_CFG_OUTPUT:
            {
                cfg = GPIO_W_CFG_PORT_DIRECTION_OUTPUT;
                break;
            }

            case IOPORT_CFG_OUTPUT_PUSH_PULL:
            {
                cfg = GPIO_W_CFG_PORT_DIRECTION_OUTPUT;
                break;
            }

            case IOPORT_CFG_OUTPUT_OPEN_DRAIN:
            default:
            {
                cfg = GPIO_W_CFG_OPEN_DRAIN_ENABLE;
                break;
            }
        }

        g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, (bsp_io_port_pin_t) ((port << BSP_IO_PORT_OFFSET) | pin),
                               (uint32_t) cfg);
        g_gpio_w.p_api->pinWrite(g_gpio_w.p_ctrl, (bsp_io_port_pin_t) ((port << BSP_IO_PORT_OFFSET) | pin),
                                 (bsp_io_level_t) level);
    }

    /* Conversion from error code to number */
    switch (err)
    {
        case FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED:
        {
            result = -5;
            break;
        }

        case FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS:
        {
            result = -4;
            break;
        }

        case FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS:
        {
            result = -3;
            break;
        }

        case FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS:
        {
            result = -2;
            break;
        }

        case FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD:
        {
            result = -1;
            break;
        }

        case FSP_ERR_AT_CMD_ERR_CMD_OK:
        default:
        {
            result = 0;
            break;
        }
    }

    sprintf(result_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), result);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODGPIOSET)
{
    const char * p_usage = "<Port Num>,<Pin Num>,<mode>,<val>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODGPIOSET)
{
    const char * p_description = "Set the GPIO status to High or Low.";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(PRODGPIOGET)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    gpio_w_peripheral_t    mode;

    uint8_t port;
    uint8_t pin;
    bool    level;
    char    result_str[32] = {0x00, };
    int8_t  result;

    if ((argc < 3) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }
    else if (argc > 3)
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }
    else
    {
        port = (uint8_t) rm_atcmd_w_core_common_atoi(argv[1]);
        pin  = (uint8_t) rm_atcmd_w_core_common_atoi(argv[2]);

        /* Check parameter */
        if (BSP_FEATURE_IO_PORT_COUNT <= port)
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
        }
        else if (BSP_IO_PORT_00 == port)
        {
            if (BSP_FEATURE_IO_PORT0_GPIO_COUNT <= pin)
            {
                err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            }
        }
        else if (BSP_IO_PORT_01 == port)
        {
            if (BSP_FEATURE_IO_PORT1_GPIO_COUNT <= pin)
            {
                err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            }
        }

        /* Check the Pin used */
        mode = (gpio_w_peripheral_t) (BSP_IO_PXX_MODE_REG(port, pin) & GPIO_P0_00_MODE_REG_PID_Msk);

        if (GPIO_W_PERIPHERAL_GPIO != mode)
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
        }
    }

    /* Normal processing */
    if (FSP_ERR_AT_CMD_ERR_CMD_OK == err)
    {
        g_gpio_w.p_api->pinRead(g_gpio_w.p_ctrl, (bsp_io_port_pin_t) ((port << BSP_IO_PORT_OFFSET) | pin),
                                (bsp_io_level_t *) &level);

        sprintf(result_str, "\r\n%s:%d,%d,%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), port, pin, level);
    }
    else
    {
        /* Conversion from error code to number */
        switch (err)
        {
            case FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED:
            {
                result = -5;
                break;
            }

            case FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS:
            {
                result = -4;
                break;
            }

            case FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS:
            {
                result = -3;
                break;
            }

            case FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS:
            {
                result = -2;
                break;
            }

            case FSP_ERR_AT_CMD_ERR_UNKNOWN_CMD:
            {
                result = -1;
                break;
            }

            case FSP_ERR_AT_CMD_ERR_CMD_OK:
            default:
            {
                result = 0;
                break;
            }
        }

        sprintf(result_str, "\r\n%s:%d", rm_atcmd_w_core_common_strupr(argv[0] + 2), result);
    }

    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODGPIOGET)
{
    const char * p_usage = "<Port Num>,<Pin Num>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODGPIOGET)
{
    const char * p_description = "Get the GPIO statu, it should be hight(1) or low(0).";

    return p_description;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB(XTAL32K_OUT)
{
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if (argc < 2)
    {
        return FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
    }

    if (0 == strcmp(argv[1], "on"))
    {
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, FUNC_CLOCK_SEL, 0x3);
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, XTAL32K_OUTPUT_EN, 0x1);
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, FUNC_CLOCK_EN, 0x1);

        REG_SETF(GPIO, P0_08_MODE_REG, PUPD, 0x3);
        REG_SETF(GPIO, P0_08_MODE_REG, PID, 0x36);
    }
    else if (0 == strcmp(argv[1], "off"))
    {
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, FUNC_CLOCK_SEL, 0x0);
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, XTAL32K_OUTPUT_EN, 0x0);
        REG_SETF(GPIO, GPIO_CLK_SEL_REG, FUNC_CLOCK_EN, 0x0);

        REG_SETF(GPIO, P0_08_MODE_REG, PUPD, 0x0);
        REG_SETF(GPIO, P0_08_MODE_REG, PID, 0x0);
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
    }

    return err;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(XTAL32K_OUT)
{
    const char * p_usage = "<on|off>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(XTAL32K_OUT)
{
    const char * p_description = "Set XTAL32K CLK OUT to GPIO";

    return p_description;
}

BSP_PLACE_CODE_IN_RAM RM_ATCMD_W_CORE_PROD_TEST_ATCMD_CB (PRODQSPI)
{
    FSP_PARAMETER_NOT_USED(argc);
    FSP_PARAMETER_NOT_USED(argv);

 #if defined(RRQ61X_OSPI_W_ENABLED) || defined(RRQ61X_OSPI_W_ATCMD_SUPPORT)
    fsp_err_atcmd_err_code          err = FSP_ERR_AT_CMD_ERR_CMD_OK;
    spi_flash_direct_transfer_t     transfer;
    spi_flash_direct_transfer_dir_t direction;
    fsp_err_t ospi_err = 0;
    uint8_t   id[3]    = {0x00, 0x00, 0x00};
    uint8_t * RBuff;
    uint8_t * WBuff;
    char      result_str[64] = {0, };

    const spi_flash_instance_t * p_spi_flash = ((const spi_flash_instance_t *) g_at_core_instance.at_conf.conf.p_spi_flash);

    printf("\n### QSPI Flash test\n");
 #ifndef OSPI_W_CFG_XIP_SUPPORT_ENABLE
    ospi_err = p_spi_flash->p_api->open(p_spi_flash->p_ctrl, p_spi_flash->p_cfg);
 #endif

    if (ospi_err)
    {
        err = FSP_ERR_AT_CMD_ERR_SFLASH_INIT;
        goto PRODQSPI_END;
    }

    transfer.command        = 0x9F;
    transfer.command_length = 0x01;
    transfer.address        = 0x00;
    transfer.dummy_cycles   = 0;
    transfer.address_length = 0;

    transfer.data_u64    = 0;          // clean data buffer
    transfer.data_length = FLASH_ID_LENGTH;
    direction            = SPI_FLASH_DIRECT_TRANSFER_DIR_READ;

    p_spi_flash->p_api->directTransfer(p_spi_flash->p_ctrl, &transfer, direction);

    memcpy((void *) id, (void *) (&transfer.data), 3);

    printf("### manufacturer_id=0x%x, type=0x%x, density=0x%x\n", id[0], id[1], id[2]);

    sprintf(result_str, "\r\n+PRODQSPI:JEDEC, %x %x %x\r\n", id[0], id[1], id[2]);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    if (id[0] == 0xFF)
    {
        /* There is no flash */
        err = FSP_ERR_AT_CMD_ERR_SFLASH_INIT;
        goto PRODQSPI_END;
    }

    /* Check QE bit */
    transfer.command        = 0x35;
    transfer.command_length = 0x01;
    transfer.address        = 0x00;
    transfer.dummy_cycles   = 0;
    transfer.address_length = 0;

    transfer.data_u64    = 0x00;
    transfer.data_length = 1;
    direction            = SPI_FLASH_DIRECT_TRANSFER_DIR_READ;
    p_spi_flash->p_api->directTransfer(p_spi_flash->p_ctrl, &transfer, direction);
    memcpy((void *) id, (void *) (&transfer.data), 1); // Reuse id buffer

    if ((id[0] & 0x02) != 0x02)
    {
        printf("### Setting QE bit for flash verify\n");

        /* Enable QE bit */
        transfer.command        = 0x31;
        transfer.command_length = 0x01;
        transfer.address        = 0x00;
        transfer.dummy_cycles   = 0;
        transfer.address_length = 0;

        transfer.data_u64    = 0x02;
        transfer.data_length = 1;
        direction            = SPI_FLASH_DIRECT_TRANSFER_DIR_WRITE;
        p_spi_flash->p_api->directTransfer(p_spi_flash->p_ctrl, &transfer, direction);
    }

    /* Erase Flash */
    ospi_err = p_spi_flash->p_api->erase(p_spi_flash->p_ctrl, (uint8_t *) (PRODMEM_ADDR | OSPI_W_DEVICE_START_ADDRESS_DATA), 0x1000);
    if (ospi_err)
    {
        err = FSP_ERR_AT_CMD_ERR_SFLASH_ERASE;
        goto PRODQSPI_END;
    }

    /* Write the serial numbers data to Sflash. */
    WBuff = (unsigned char *) malloc(PRODMEM_SIZE);
    memset(WBuff, 0, PRODMEM_SIZE);

    for (int i = 0; i < PRODMEM_SIZE; i++)
    {
        WBuff[i] = (unsigned char) i & 0xFF;
    }

    if (p_spi_flash->p_api->write(p_spi_flash->p_ctrl, WBuff, (uint8_t *) (PRODMEM_ADDR | OSPI_W_DEVICE_START_ADDRESS_DATA), PRODMEM_SIZE) != FSP_SUCCESS)
    {
        printf("### Flash write failed.\n");
        err = FSP_ERR_AT_CMD_ERR_SFLASH_WRITE;
        goto ret_write_failed;
    }

  #ifdef DUMP_USER_ATCMD
    USER_AT_LOG("\n\n### Flash write data----------------------\n");
    data_dump_print(PRODMEM_ADDR, WBuff, PRODMEM_SIZE);
  #endif

    RBuff = (unsigned char *) malloc(PRODMEM_SIZE);
    memset(RBuff, 0, PRODMEM_SIZE);
    memcpy((void *) RBuff, (void *) (PRODMEM_ADDR | OSPI_W_DEVICE_START_ADDRESS_DATA), PRODMEM_SIZE);

  #ifdef DUMP_USER_ATCMD
    USER_AT_LOG("\n\n### Flash Read data----------------------\n");
    data_dump_print(PRODMEM_ADDR, RBuff, PRODMEM_SIZE);
  #endif

    if (memcmp(WBuff, RBuff, PRODMEM_SIZE))
    {
        printf("### Write / Read verify NG, Test failed\n");
        err = FSP_ERR_AT_CMD_ERR_SFLASH_VERIFY;
        goto ret_read_failed;
    }

    printf("Write / Read verify OK\n");

ret_read_failed:
    free(RBuff);
ret_write_failed:
    free(WBuff);

    sprintf(result_str, "\r\n+PRODQSPI:%d\r\n", err);
    RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) result_str, strlen(result_str));

    /* Erase the section wrriten for test */
    ospi_err = p_spi_flash->p_api->erase(p_spi_flash->p_ctrl, (uint8_t *) (PRODMEM_ADDR | OSPI_W_DEVICE_START_ADDRESS_DATA), 0x1000);
    if (ospi_err)
    {
        err = FSP_ERR_AT_CMD_ERR_SFLASH_ERASE;
        goto PRODQSPI_END;
    }

PRODQSPI_END:
 #ifndef OSPI_W_CFG_XIP_SUPPORT_ENABLE
    p_spi_flash->p_api->close(p_spi_flash->p_ctrl);
 #endif

    return err;
 #else
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_SFLASH_INIT;

    return err;
 #endif
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_FORMAT_CB(PRODQSPI)
{
    const char * p_usage = "<none>";

    return p_usage;
}

RM_ATCMD_W_CORE_PROD_TEST_ATCMD_BRIEF_CB(PRODQSPI)
{
    const char * p_description = "Check if the QSPI flash is initialized, "
                                 "then try to erase/write/read/compare some data. "
                                 "If all tests are done, then erase the written data and send OK (result).";

    return p_description;
}

 #ifdef RRQ61X_OSPI_W_ATCMD_SUPPORT
  #undef RRQ61X_OSPI_W_ATCMD_SUPPORT
 #endif

/**
 * @brief GPIO pair test
 *
 * @param [in] force: io_port_pin for force
 * @param [in] sense: io_port_pin for sense
 * @return uint32_t : 1 if successful, 0 if failed
 *
 * checks connection between 2 GPIOs externally connected via 1K resistor.
 * One of the pins forces the output, the other senses it. The output is toggled and all GPIOs, except
 * masked ones for UART and XTAL32k, and is checked (XOR) to detect if only the expected bits
 * toggled during the test.
 */
static uint32_t test_pin_pair (bsp_io_port_pin_t force, bsp_io_port_pin_t sense)
{
    uint32_t       p0_val         = 0;
    uint32_t       p1_val         = 0;
    uint32_t       p0_result_mask = 0;
    uint32_t       p1_result_mask = 0;
    ioport_size_t  temp_val       = 0;
    const uint32_t unused_p0_mask = 0x0003FFC;  // mask out unused pins (0,1,14+)
    const uint32_t unused_p1_mask = 0x0000FC0F; // mask out unused pins (4-9, 16+)

    /* Set the corresponding bits for the expected result for P0 and P1 */
    if (!(force & PORT_MASK))
    {
        p0_result_mask |= (1 << (force & PIN_MASK));
    }
    else
    {
        p1_result_mask |= (1 << (force & PIN_MASK));
    }

    if (!(sense & PORT_MASK))
    {
        p0_result_mask |= (1 << (sense & PIN_MASK));
    }
    else
    {
        p1_result_mask |= (1 << (sense & PIN_MASK));
    }

    /* Set initial state */
    R_GPIO_W_PinCfg(&g_gpio_w_ctrl, force, (uint32_t) BSP_IO_DIRECTION_OUTPUT);
    R_GPIO_W_PinWrite(&g_gpio_w_ctrl, force, BSP_IO_LEVEL_LOW);
    R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MICROSECONDS);

    /* Save initial state */
    R_GPIO_W_PortRead(&g_gpio_w_ctrl, BSP_IO_PORT_00, &temp_val);
    p0_val = temp_val;
    R_GPIO_W_PortRead(&g_gpio_w_ctrl, BSP_IO_PORT_01, &temp_val);
    p1_val = temp_val;

    /* Toggle the force pin */
    R_GPIO_W_PinWrite(&g_gpio_w_ctrl, force, BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(200, BSP_DELAY_UNITS_MICROSECONDS);

    /* Read new state and check only expected pins have changed */
    R_GPIO_W_PortRead(&g_gpio_w_ctrl, BSP_IO_PORT_00, &temp_val);
    p0_val ^= temp_val;
    p0_val &= unused_p0_mask;

    R_GPIO_W_PortRead(&g_gpio_w_ctrl, BSP_IO_PORT_01, &temp_val);
    p1_val ^= temp_val;
    p1_val &= unused_p1_mask;

    /* Restore force pin state */
    R_GPIO_W_PinCfg(&g_gpio_w_ctrl, force, (uint32_t) GPIO_PROD_PIN_CFG);

    return (p0_val == p0_result_mask) && (p1_val == p1_result_mask);
}

static uint32_t custom_gpio_test (void)
{
    uint32_t  result = 0;
    uint8_t   i      = 0;
    fsp_err_t err;
    uint32_t  temp_OQSPI_GPIO_MODE = CRG_TOP->CLK_AMBA_REG_b.OQSPI_GPIO_MODE;

    CRG_TOP->CLK_AMBA_REG_b.OQSPI_GPIO_MODE = 1;

    /* GPIOs used for the tests */
    const uint8_t GPIOs_P0[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    const uint8_t GPIOs_P1[] = {0, 1, 2, 3, 10, 11, 12, 13, 14, 15};

    err = R_GPIO_W_Open(&g_gpio_w_ctrl, &g_bsp_pin_cfg_prod);
    if ((err != FSP_SUCCESS) && (err != FSP_ERR_ALREADY_OPEN))
    {
        return 0x7FF;
    }

    /* Initialize GPIOs */
    for (i = 0; i < sizeof(GPIOs_P0); i++)
    {
        R_GPIO_W_PinCfg(&g_gpio_w_ctrl, BSP_IO_PORT_00_PIN_00 + GPIOs_P0[i], (uint32_t) GPIO_PROD_PIN_CFG);
    }

    for (i = 0; i < sizeof(GPIOs_P1); i++)
    {
        R_GPIO_W_PinCfg(&g_gpio_w_ctrl, BSP_IO_PORT_01_PIN_00 + GPIOs_P1[i], (uint32_t) GPIO_PROD_PIN_CFG);
    }

    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_05, BSP_IO_PORT_00_PIN_08);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_04, BSP_IO_PORT_00_PIN_10);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_09, BSP_IO_PORT_00_PIN_12);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_11, BSP_IO_PORT_00_PIN_13);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_02, BSP_IO_PORT_00_PIN_03);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_00_PIN_06, BSP_IO_PORT_00_PIN_07);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_01_PIN_03, BSP_IO_PORT_01_PIN_00);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_01_PIN_11, BSP_IO_PORT_01_PIN_13);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_01_PIN_01, BSP_IO_PORT_01_PIN_02);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_01_PIN_10, BSP_IO_PORT_01_PIN_12);
    result <<= 1;
    result  |= test_pin_pair(BSP_IO_PORT_01_PIN_14, BSP_IO_PORT_01_PIN_15);

    CRG_TOP->CLK_AMBA_REG_b.OQSPI_GPIO_MODE = temp_OQSPI_GPIO_MODE & 0x1;

    /*
     * Return zero if successful
     * else return the positions of all the failed tests - LSB indicates the last test
     * for example resualt = 0....0 0010 means the test before the last one failed.
     */
    result ^= 0x7FF;

    return result;
}

#endif                                 /* CFG_WIFI */
