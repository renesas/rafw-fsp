/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdlib.h>
#include "rm_atcmd_w_core_pin_port.h"
#include "rm_atcmd_w_core_err_code.h"
#include "rm_atcmd_w_core.h"
#include "bsp_otp.h"
#include "r_adc_w.h"
#include "rm_atcmd_w_core_utils.h"
#if CFG_WIFI
 #include "r_pm_if.h"
 #include "r_cc312_crypto.h"
#endif                                 /* CFG_WIFI */
#include "r_gpio_w.h"
#include "common_data.h"

#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
 #include dg_configADNVPARAM_PROJ_FILE
#endif

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CODE(atcmd)    "AT+" # atcmd

#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(atcmd) \
    uint32_t RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _cmd_cb(atcmd_w_ctrl_t * const p_at_ctrl, int argc, char * argv[])
#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _format_cb(void)
#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(atcmd) \
    const char * RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _brief_cb(void)

#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB_P(atcmd)           RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _cmd_cb
#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB_P(atcmd)    RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _format_cb
#define RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB_P(atcmd)     RM_ATCMD_W_CORE_PIN_PORT_ ## atcmd ## _brief_cb

#define GPIO_PROD_PIN_CFG      ((uint32_t) GPIO_W_PERIPHERAL_GPIO |          \
                                (uint32_t) IOPORT_CFG_PORT_DIRECTION_INPUT | \
                                (uint32_t) IOPORT_CFG_PULLDOWN_ENABLE)
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
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GPIOSET);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GPIOSET);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GPIOSET);

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GPIOGET);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GPIOGET);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GPIOGET);

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(SETCONFIG);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(SETCONFIG);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(SETCONFIG);

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GETCONFIG);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GETCONFIG);
RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GETCONFIG);

/* Workaround for ADC wakeup settings until AT+ADC is available */
static adc_w_channel_cfg_t g_at_dpm_adc_cfg0 =
{
    .dma_en              = ADC_W_DMA_DISABLED,
    .interrupt_mode_fifo = ADC_W_INTERRUPT_FIFO_NONE,
    .interrupt_mode_thd  = ADC_W_INTERRUPT_THD_NONE,
    .sensorwakeup_en     = ADC_W_SENSOR_WAKEUP_ENABLED,
    .thd_value           = 0x7FF,
    .threshold_mode      = ADC_W_SENSOR_WAKEUP_THD_OVER,
};
static adc_w_extended_cfg_t g_at_dpm_adc_cfg_ext =
{
    .conversion_clockdiv      = 3,
    .upper_bound_limit        = 0xC000,
    .lower_bound_limit        = 0x50,
    .p_channel_cfgs[0]        = NULL,
    .p_channel_cfgs[1]        = NULL,
    .p_channel_cfgs[2]        = &g_at_dpm_adc_cfg0,
    .p_channel_cfgs[3]        = NULL,
    .timer_count_clock_source = ADC_W_TIMER_COUNT_SOURCE_8,
    .timer_value              = 1,
    .sample_average           = ADC_W_SAMPLE_AVERAGE_4,
};
static adc_cfg_t g_at_dpm_adc_cfg =
{
    .p_extend = &g_at_dpm_adc_cfg_ext,
};

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
const atcmd_w_core_module_t at_core_pin_port_module[] =
{
    {
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CODE(GPIOSET),
        ATCMD_W_TYPE_A,
        4,
        0,
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB_P(GPIOSET),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB_P(GPIOSET),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB_P(GPIOSET)
    },
    {
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CODE(GPIOGET),
        ATCMD_W_TYPE_A,
        2,
        0,
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB_P(GPIOGET),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB_P(GPIOGET),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB_P(GPIOGET)
    },
    {
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CODE(SETCONFIG),
        ATCMD_W_TYPE_A,
        6,
        0,
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB_P(SETCONFIG),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB_P(SETCONFIG),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB_P(SETCONFIG)
    },
    {
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CODE(GETCONFIG),
        ATCMD_W_TYPE_A,
        0,
        0,
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB_P(GETCONFIG),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB_P(GETCONFIG),
        RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB_P(GETCONFIG)
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

static adc_w_instance_ctrl_t g_at_dpm_adc_ctrl;

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/* Workaround for ADC wakeup settings until AT+ADC is available */
fsp_err_atcmd_err_code RM_ATCMD_W_CORE_PIN_PORT_config_adc_as_wake_source (void)
{
    fsp_err_t              fsp_err = FSP_SUCCESS;
    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;
    g_at_dpm_adc_cfg0.sensorwakeup_en = ADC_W_SENSOR_WAKEUP_ENABLED;

    g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ADC_WAKEUP_PIN, ADC_WAKEUP_MODE);

    fsp_err = R_ADC_W_Open(&g_at_dpm_adc_ctrl, &g_at_dpm_adc_cfg);
    if (!(FSP_SUCCESS == fsp_err) && !(FSP_ERR_ALREADY_OPEN == fsp_err))
    {
        err = FSP_ERR_AT_CMD_ERR_PERI_XXX;
        goto end;
    }

    fsp_err = R_ADC_W_SensorWakeupcfg(&g_at_dpm_adc_ctrl);
    if (!(FSP_SUCCESS == fsp_err) && !(FSP_ERR_ALREADY_OPEN == fsp_err))
    {
        err = FSP_ERR_AT_CMD_ERR_PERI_XXX;
        goto end;
    }

end:

    return err;
}

fsp_err_atcmd_err_code RM_ATCMD_W_CORE_PIN_PORT_unconfig_adc_as_wake_source (void)
{
    fsp_err_t              fsp_err = FSP_SUCCESS;
    fsp_err_atcmd_err_code err     = FSP_ERR_AT_CMD_ERR_CMD_OK;

    g_gpio_w.p_api->pinCfg(g_gpio_w.p_ctrl, ADC_WAKEUP_PIN,
                           GPIO_W_CFG_PORT_DIRECTION_INPUT | GPIO_W_CFG_PERIPHERAL_PIN);

    fsp_err = R_ADC_W_Close(&g_at_dpm_adc_ctrl);
    if (!(FSP_SUCCESS == fsp_err) && !(FSP_ERR_NOT_OPEN == fsp_err))
    {
        err = FSP_ERR_AT_CMD_ERR_PERI_XXX;
        goto end;
    }

end:

    return err;
}

uint32_t RM_ATCMD_W_CORE_PIN_PORT_register (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif
    if (p_list->module_cnt >= ATCMD_W_LIST_MAX_CNT)
    {
        return FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
    }

    return rm_atcmd_w_core_register_module_node(p_list, at_core_pin_port_module);
}

uint32_t RM_ATCMD_W_CORE_PIN_PORT_deregister (atcmd_w_core_module_list_t * p_list)
{
#if (ATCMD_W_CFG_PARAM_CHECKING_ENABLE)
    FSP_ASSERT(p_list);
#endif

    rm_atcmd_w_core_deregister(p_list, at_core_pin_port_module);

    return FSP_ERR_AT_CMD_ERR_CMD_OK;
}

uint32_t RM_ATCMD_W_CORE_PIN_PORT_open (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

uint32_t RM_ATCMD_W_CORE_PIN_PORT_close (atcmd_w_ctrl_t * const p_at_ctrl)
{
    FSP_PARAMETER_NOT_USED(p_at_ctrl);
    fsp_err_atcmd_err_code err = FSP_ERR_AT_CMD_ERR_CMD_OK;

    return err;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GPIOSET)
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

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GPIOSET)
{
    const char * p_usage = "<Port Num>,<Pin Num>,<mode>,<val>";

    return p_usage;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GPIOSET)
{
    const char * p_description = "Set the GPIO status to High or Low.";

    return p_description;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GPIOGET)
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

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GPIOGET)
{
    const char * p_usage = "<Port Num>,<Pin Num>";

    return p_usage;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GPIOGET)
{
    const char * p_description = "Get the GPIO statu, it should be hight(1) or low(0).";

    return p_description;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(SETCONFIG)
{
    int                    port;
    int                    pin;
    int                    edge_type  = 0;
    uint32_t               wakeup_pin = 0;
    fsp_err_atcmd_err_code err        = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if ((argc >= 5) && (argc <= 6))
    {
        int action, wake_source;

        if ((rm_atcmd_w_core_common_stoi(argv[1], &action, POL_1) != 0) ||
            (rm_atcmd_w_core_common_stoi(argv[2], &wake_source, POL_1) != 0))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if ((rm_atcmd_w_core_common_is_in_valid_range(action, 1, 2) == pdFALSE) ||
            (rm_atcmd_w_core_common_is_in_valid_range(wake_source, 1, 6) == pdFALSE))
        {
            err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
            goto end;
        }

        if (PMGR_WAKE_SOURCE_WIFI == (1UL << wake_source))
        {
            err = FSP_ERR_AT_CMD_ERR_NOT_SUPPORTED;
            goto end;
        }

        /* 0 (RTC), 1 (GPT), 2 (GPIO), 3 (ADC), 4 (WIFI), 5 (BLE) */
        if (wake_source == RM_ATCMD_W_CORE_DPM_WAKE_SRC_GPIO)
        {
            /* AT+SETCONFIG=<action>,<wake_source>,<port>,<pin>[,<edge_type>] */

            if ((rm_atcmd_w_core_common_stoi(argv[3], &port, POL_1) != 0) ||
                (rm_atcmd_w_core_common_stoi(argv[4], &pin, POL_1) != 0))
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }

            if (rm_atcmd_w_core_common_is_in_valid_range(port, 0, 1) == pdFALSE)
            {
                err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                goto end;
            }

            if (port == 0)
            {
                if ((rm_atcmd_w_core_common_is_in_valid_range(pin, 8, 13) == pdFALSE) && (pin != 0))
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    goto end;
                }
                else
                {
                    if (pin == 0)
                    {
                        wakeup_pin = (uint32_t) (0x01 << 0);
                    }
                    else
                    {
                        wakeup_pin = (uint32_t) (0x01 << (pin - RM_ATCMD_W_CORE_DPM_PORT0_GPIO_OFFSET));
                    }
                }
            }
            else
            {
                if (rm_atcmd_w_core_common_is_in_valid_range(pin, 10, 13) == pdFALSE)
                {
                    err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                    goto end;
                }
                else
                {
                    wakeup_pin = (uint32_t) (0x01 << (pin - RM_ATCMD_W_CORE_DPM_PORT1_GPIO_OFFSET));
                }
            }
        }

        if (action == 1 /* SET */)
        {
            /* Workaround for ADC wakeup settings until AT+ADC is available */
            if (PMGR_WAKE_SOURCE_ADC == (1UL << wake_source))
            {
                err = RM_ATCMD_W_CORE_PIN_PORT_config_adc_as_wake_source();

                if (FSP_ERR_AT_CMD_ERR_PERI_XXX == err)
                {
                    goto end;
                }
            }

            if (wake_source == RM_ATCMD_W_CORE_DPM_WAKE_SRC_GPIO)
            {
                /* edge type */
                if (argc == 6)
                {
                    if (rm_atcmd_w_core_common_stoi(argv[5], &edge_type, POL_1) != 0)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }

                    if (rm_atcmd_w_core_common_is_in_valid_range(edge_type, 0, 1) == pdFALSE)
                    {
                        err = FSP_ERR_AT_CMD_ERR_WRONG_ARGUMENTS;
                        goto end;
                    }
                }

                R_BSP_WakeupSourcePinSet(wakeup_pin, (uint32_t) edge_type);
            }
        }
        else                           /* CLR */
        {
            /* Workaround for ADC wakeup settings until AT+ADC is available */
            if (PMGR_WAKE_SOURCE_ADC == (1UL << wake_source))
            {
                err = RM_ATCMD_W_CORE_PIN_PORT_unconfig_adc_as_wake_source();

                if (FSP_ERR_AT_CMD_ERR_PERI_XXX == err)
                {
                    goto end;
                }
            }

            if (wake_source == RM_ATCMD_W_CORE_DPM_WAKE_SRC_GPIO)
            {
                R_BSP_WakeupSourcePinUnSet(wakeup_pin);
            }
        }

#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      "GPIO_WAKEUP_SOURCE_PIN",
                                      wakeup_pin);
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      "GPIO_WAKEUP_SOURCE_EDGE_TYPE",
                                      edge_type);
#endif
    }
    else
    {
        if (argc == 2)
        {
            err = FSP_ERR_AT_CMD_ERR_INSUFFICIENT_ARGS;
        }
        else
        {
            err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
        }
    }

end:

    return err;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(SETCONFIG)
{
    const char * p_usage = "<action>,<wake_source>,<port>,<pin>[,<edge_type>]";

    return p_usage;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(SETCONFIG)
{
    const char * p_description =
        "Set / Clear (1:Set, 2:Clear) wake source (0:RTC, 1:GPT, 2:GPIO, 3:ADC, 4:WIFI, 5:BLE) for port, pin and edge type";

    return p_description;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_CB(GETCONFIG)
{
    uint16_t               wakeup_gpio = (RTC->GPIO_WAKEUP1_REG_b.GPIO_WAKEUP_EN_SEL & 0x7FF);
    uint16_t               wakeup_adc  = (RTC->XADC12_TIMER_SET_REG_b.ASWCH_CTRL & 0x1f);
    char                 * pins;
    char                   resp_str[128] = {0x00, };
    fsp_err_atcmd_err_code err           = FSP_ERR_AT_CMD_ERR_CMD_OK;

    if ((argc == 1) || rm_atcmd_w_core_common_is_query_arg(argc, argv[1]))
    {
        sprintf(&resp_str[strlen(resp_str)], "\r\n");
        pins = &resp_str[strlen(resp_str)];

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_00)
        {
            strcat(pins, "P0_00, ");
            pins += strlen("P0_00, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_08)
        {
            strcat(pins, "P0_08, ");
            pins += strlen("P0_08, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_09)
        {
            strcat(pins, "P0_09, ");
            pins += strlen("P0_09, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_10)
        {
            strcat(pins, "P0_10, ");
            pins += strlen("P0_10, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_11)
        {
            strcat(pins, "P0_11, ");
            pins += strlen("P0_11, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_12)
        {
            strcat(pins, "P0_12, ");
            pins += strlen("P0_12, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P0_13)
        {
            strcat(pins, "P0_13, ");
            pins += strlen("P0_13, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P1_10)
        {
            strcat(pins, "P1_10, ");
            pins += strlen("P1_11, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P1_11)
        {
            strcat(pins, "P1_11, ");
            pins += strlen("P1_11, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P1_12)
        {
            strcat(pins, "P1_12, ");
            pins += strlen("P1_12, ");
        }

        if (wakeup_gpio & BSP_WAKEUP_GPIO_P1_13)
        {
            strcat(pins, "P1_13, ");
            pins += strlen("P1_13, ");
        }

        if (wakeup_adc & (1 << ADC_CHANNEL_0))
        {
            strcat(pins, "ADC0, ");
            pins += strlen("ADC0, ");
        }

        if (wakeup_adc & 1 << (ADC_CHANNEL_1))
        {
            strcat(pins, "ADC1, ");
            pins += strlen("ADC1, ");
        }

        if (wakeup_adc & (1 << ADC_CHANNEL_2))
        {
            strcat(pins, "ADC2, ");
            pins += strlen("ADC2, ");
        }

        if (wakeup_adc & (1 << ADC_CHANNEL_3))
        {
            strcat(pins, "ADC3, ");
            pins += strlen("ADC3, ");
        }

        int len = strlen(resp_str);
        resp_str[len - 2] = '\0';

        RM_ATCMD_W_CORE_Write(p_at_ctrl, (uint8_t *) resp_str, strlen(resp_str));
    }
    else
    {
        err = FSP_ERR_AT_CMD_ERR_TOO_MANY_ARGS;
    }

    return err;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_FORMAT_CB(GETCONFIG)
{
    const char * p_usage = "";

    return p_usage;
}

RM_ATCMD_W_CORE_PIN_PORT_ATCMD_BRIEF_CB(GETCONFIG)
{
    const char * p_description = "Query the status of wake-up pins";

    return p_description;
}
