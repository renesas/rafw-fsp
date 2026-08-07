/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "rm_atcmd_w_core.h"

#include "r_gpio_w.h"
#include "r_i2c_master_w.h"
#include "r_adc_w.h"
#include "r_tim_w.h"

#define DRIVER_PARSE_USE_STRTOK    (0)

#if DRIVER_PARSE_USE_STRTOK
 #define SPLIT(a, b)          strtok(a, b)
 #define SPLIT_WL(a, b, c)    strtok(a, b)
#else
 #define SPLIT(a, b)          split(a, b)
 #define SPLIT_WL(a, b, c)    split_wl(a, b, c)
#endif

#define I2C_BUFFER_SIZE_BYTES          (32U)

#define AT_I2CREAD_LENGTH_MIN          (5)
#define AT_I2CWRITE_LENGTH_MIN         (7)
#define AT_ADCCHEN_LENGTH_MIN          (3)
#define AT_ADCREAD_LENGTH_MIN          (3)
#define AT_ADCSTART_LENGTH_MIN         (1)
#define AT_PWMSTART_LENGTH_MIN         (7)
#define I2C_TRANSACTION_BUSY_DELAY     (1000U)                            /* 1000 ms */
#define ADC_READ_MAX_NUMBER            ((ATCMD_W_RESP_LEN_MAX - 9U) / 6U) /* 11: "\n[ " + " ]\n" + "OK\n" */
#define NUMBER_OF_CHAR_1BYTE           (2)

#define PWM_TEST_OUTPUT_PIN_FOR_PWM    (BSP_IO_PORT_00_PIN_10)
#define IOPORT_PERIPHERAL_PWM          ((uint32_t) GPIO_W_CFG_PERIPHERAL_PIN | \
                                        (uint32_t) GPIO_W_PERIPHERAL_TIM_PWM)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum e_at_core_driver_prefix_ret
{
    AT_PREFIX_RES_MATCH = 0,
    AT_PREFIX_SHORT_LENGTH,
    AT_PREFIX_RES_NO_MATCH,
    AT_PREFIX_RES_EOL,
} at_core_driver_prefix_ret_t;

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/
static gpio_w_instance_ctrl_t g_atcmd_drv_ioport_ctrl;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
fsp_err_t rm_atcmd_w_core_driver_handle_req(atcmd_w_ctrl_t * const p_api_ctrl,
                                            char                 * at_req_cmd,
                                            char                 * at_resp_msg,
                                            uint8_t                at_cmd_idx);
void rm_atcmd_w_core_driver_init(void);

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
static uint8_t g_i2c_tx_buffer[I2C_BUFFER_SIZE_BYTES];
static uint8_t g_i2c_rx_buffer[I2C_BUFFER_SIZE_BYTES];

static i2c_master_event_t i2c_atcmd_callback_event;

static uint32_t timeout_ms;

static ioport_cfg_t g_unused_pin_cfg =
{
    .number_of_pins = 0,
    .p_pin_cfg_data = NULL,
};

static adc_w_scan_cfg_t g_atcmd_adc_w_scan_cfg =
{
    .scan_mask = (1U << ADC_CHANNEL_0),
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

static const tim_w_extended_cfg_t g_atcmd_gpt_cfg_ext_default =
{
    .free_run     = false,
    .count_source = TIM_W_CLOCK_LP_CLK,
    .direction    = TIMER_DIRECTION_UP,
};

typedef enum e_at_core_driver_cmd_list
{
    AT_WFWMM_CMD,                      ///< AT+WFWMM Set WMM on/off.
    AT_I2CINIT_CMD,                    ///< AT+I2CINIT Configure GPIOA_8 (I2C_SDA), GPIOA_9 (I2C_SCL) pins to I2C pins.
    AT_I2CREAD_CMD,                    ///< AT+I2CREAD Read values from registers of I2C device.
    AT_I2CWRITE_CMD,                   ///< AT+I2CWRITE Write values to I2C register of I2C device
    AT_I2CEND_CMD,                     ///< AT+I2CEND End I2C device procces
    AT_ADCINIT_CMD,                    ///< AT+ADCINIT Configure GPIOA_0, GPIOA_1, GPIOA_2, GPIOA_3 to analog input pins for ADC.
    AT_ADCCHEN_CMD,                    ///< AT+ADCCHEN Enable given ADC channel.
    AT_ADCSTART_CMD,                   ///< AT+ADCSTART Start ADC function
    AT_ADCREAD_CMD,                    ///< AT+ADCREAD Read ADC value.
    AT_ADCSTOP_CMD,                    ///< AT+ADCSTOP Stop ADC function.
    AT_ADCEND_CMD,                     ///< AT+ADCEND End ADC device procces
    AT_SPICONF_CMD,                    ///< AT+SPICONF Configure SPI.
    AT_PWMINIT_CMD,                    ///< AT+PWMINIT Initialize PWM.
    AT_PWMSTART_CMD,                   ///< AT+PWMSTART Start PWM.
    AT_PWMSTOP_CMD,                    ///< AT+PWMSTOP Stop PWM.

    AT_CMD_LIST_MAX
} at_core_driver_cmd_list_t;

static void i2c_atcmd_callback (i2c_master_callback_args_t * p_args)
{
    i2c_atcmd_callback_event = p_args->event;
}

static void wait_for_i2c_read_complete (void)
{
    while ((I2C_MASTER_EVENT_RX_COMPLETE != i2c_atcmd_callback_event) && timeout_ms)
    {
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        timeout_ms--;;
    }
}

static void wait_for_i2c_write_complete (void)
{
    while ((I2C_MASTER_EVENT_TX_COMPLETE != i2c_atcmd_callback_event) && timeout_ms)
    {
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        timeout_ms--;;
    }
}

#if (DRIVER_PARSE_USE_STRTOK == 0)
static char * split_wl (char * str, const char * sep, uint8_t * plength)
{
    static char * next    = NULL;
    static char * ret_str = NULL;
    uint8_t       length  = 0;

    if (NULL != str)
    {
        next = str;
    }

    if (next == NULL)
    {
        return NULL;
    }

    ret_str = next;

    if (*ret_str == '\0')
    {
        next = NULL;

        return NULL;
    }

    while (*ret_str == *sep)
    {
        ret_str++;
    }

    next = ret_str + 1;
    length++;
    while (*next != *sep)
    {
        if (*next == '\0')
        {
            break;
        }

        next++;
        length++;
    }

    if (*next != '\0')
    {
        *next++ = '\0';
    }

    if (NULL != plength)
    {
        *plength = length;
    }

    return ret_str;
}

static char * split (char * str, const char * sep)
{
    return split_wl(str, sep, NULL);
}

#endif

static at_core_driver_prefix_ret_t rm_atcmd_w_core_driver_check_prefix (const char * p_pre, const char * p_str)
{
    if (*p_pre == AT_CMD_END_OF_STR_LF)
    {
        return AT_PREFIX_RES_EOL;
    }

    if (!strlen(p_pre))
    {
        return AT_PREFIX_SHORT_LENGTH;
    }

    if (!p_str)
    {
        return AT_PREFIX_RES_NO_MATCH;
    }

    if (strncmp(p_pre, p_str, strlen(p_pre)))
    {
        return AT_PREFIX_RES_NO_MATCH;
    }

    return AT_PREFIX_RES_MATCH;
}

static fsp_err_t rm_atcmd_w_core_driver_handle_error_resp (const char * p_func,
                                                           fsp_err_t    error_code,
                                                           char       * p_at_resp_msg)
{
    FSP_PARAMETER_NOT_USED(p_func);

    memcpy(p_at_resp_msg, AT_CMD_RESP_TEXT_ERROR, sizeof(AT_CMD_RESP_TEXT_ERROR));

    return error_code;
}

static fsp_err_t parse_i2c_init_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;

    ret = p_ctrl->p_cfg->p_i2c_master->p_api->open(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                   p_ctrl->p_cfg->p_i2c_master->p_cfg);

    if (FSP_SUCCESS == ret)
    {
        ret = p_ctrl->p_cfg->p_i2c_master->p_api->callbackSet(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                              i2c_atcmd_callback,
                                                              NULL,
                                                              NULL);
    }

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_NOT_OPEN, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_i2c_end_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    ret = p_ctrl->p_cfg->p_i2c_master->p_api->close(p_ctrl->p_cfg->p_i2c_master->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_i2c_read_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    uint32_t slave = 0;
    uint32_t bytes = 0;
    uint32_t i;
    char   * trim_cmd = at_req_cmd;
    char   * tp;

    /* trim_cmd[]: "=d0,10,1" */
    trim_cmd = trim_cmd + strlen("+I2CREAD");

    if (strlen(trim_cmd) < (AT_I2CREAD_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* trim_cmd[]: "d0,10,1" */
    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);

    /* Get slave address */
    /* trim_cmd[]="10,1" */
    /* tp: "d0" */
    tp = SPLIT(trim_cmd, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    slave = strtoul(tp, NULL, 16);

    /* Get register */
    /* trim_cmd[]="1" */
    /* tp: "10" */
    tp = SPLIT(NULL, AT_CMD_VAR_MRK);

    /* Get read length */
    /* tp: "1" */
    tp = SPLIT(NULL, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    bytes = strtoul(tp, NULL, 10);

    if ((1 > bytes) || (I2C_BUFFER_SIZE_BYTES < bytes))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    ret = p_ctrl->p_cfg->p_i2c_master->p_api->slaveAddressSet(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                              slave,
                                                              p_ctrl->p_cfg->p_i2c_master->p_cfg->addr_mode);

    memset(g_i2c_rx_buffer, 0x00, I2C_BUFFER_SIZE_BYTES);
    if (FSP_SUCCESS == ret)
    {
        /* Read data back from the I2C slave */
        i2c_atcmd_callback_event = I2C_MASTER_EVENT_ABORTED;
        timeout_ms               = I2C_TRANSACTION_BUSY_DELAY;

        ret = p_ctrl->p_cfg->p_i2c_master->p_api->read(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                       g_i2c_rx_buffer,
                                                       bytes,
                                                       false);
        wait_for_i2c_read_complete();

        if (I2C_MASTER_EVENT_ABORTED == i2c_atcmd_callback_event)
        {
            ret = FSP_ERR_TIMEOUT;
        }

        if (FSP_SUCCESS == ret)
        {
            for (i = 0; i < bytes; i++)
            {
                sprintf(at_resp_msg + i * 2, "%02x", g_i2c_rx_buffer[i]);
            }

            snprintf(at_resp_msg + bytes * 2, ATCMD_W_RESP_LEN_MAX, "\n%s\n", AT_CMD_RESP_TEXT_OK);

            return ret;
        }
    }

    return rm_atcmd_w_core_driver_handle_error_resp(__func__, ret, at_resp_msg);
}

static fsp_err_t parse_i2c_write_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    uint32_t slave    = 0;
    uint32_t bytes    = 0;
    uint8_t  length   = 0;
    uint8_t  ret_size = 0;
    uint8_t  initial  = 0;
    uint8_t  i;
    char     str_1byte[2];
    char   * trim_cmd = at_req_cmd;
    char   * tp;

    /* trim_cmd[]: "=d0,10,3,670292" */
    trim_cmd = trim_cmd + strlen("+I2CWRITE");

    if (strlen(trim_cmd) < (AT_I2CWRITE_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* trim_cmd[]: "d0,10,3,670292" */
    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);

    /* Get slave address */
    /* trim_cmd[]="10,3,670292" */
    /* tp: "d0" */
    tp     = SPLIT_WL(trim_cmd, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    slave = strtoul(tp, NULL, 16);

    /* Get register */
    /* trim_cmd[]="3,670292" */
    /* tp: "10" */
    tp     = SPLIT_WL(NULL, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    /* Get write length */
    /* trim_cmd[]="670292" */
    /* tp: "3" */
    tp     = SPLIT_WL(NULL, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    bytes = strtoul(tp, NULL, 10);

    if ((1 > bytes) || (I2C_BUFFER_SIZE_BYTES < bytes))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* Get write data */
    /* trim_cmd[]="670292" */
    memset(g_i2c_tx_buffer, 0x00, I2C_BUFFER_SIZE_BYTES);
    trim_cmd = trim_cmd + length;

    if (0 != strlen(trim_cmd) % NUMBER_OF_CHAR_1BYTE)
    {
        str_1byte[0] = '0';
        strncpy(&str_1byte[1], trim_cmd, 1);
        trim_cmd++;
        g_i2c_tx_buffer[0] = (uint8_t) strtoul(str_1byte, NULL, 16);
        initial            = 1;
    }

    for (i = initial; i < bytes; i++)
    {
        if (NUMBER_OF_CHAR_1BYTE > strlen(trim_cmd))
        {
            return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
        }

        strncpy(str_1byte, trim_cmd, NUMBER_OF_CHAR_1BYTE);
        trim_cmd           = trim_cmd + NUMBER_OF_CHAR_1BYTE;
        g_i2c_tx_buffer[i] = (uint8_t) strtoul(str_1byte, NULL, 16);
    }

    ret = p_ctrl->p_cfg->p_i2c_master->p_api->slaveAddressSet(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                              slave,
                                                              p_ctrl->p_cfg->p_i2c_master->p_cfg->addr_mode);

    if (FSP_SUCCESS == ret)
    {
        /* Read data back from the I2C slave */
        i2c_atcmd_callback_event = I2C_MASTER_EVENT_ABORTED;
        timeout_ms               = I2C_TRANSACTION_BUSY_DELAY;

        ret = p_ctrl->p_cfg->p_i2c_master->p_api->write(p_ctrl->p_cfg->p_i2c_master->p_ctrl,
                                                        g_i2c_tx_buffer,
                                                        bytes,
                                                        false);
        wait_for_i2c_write_complete();

        if (I2C_MASTER_EVENT_ABORTED == i2c_atcmd_callback_event)
        {
            ret = FSP_ERR_TIMEOUT;
        }

        if (FSP_SUCCESS == ret)
        {
            snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);

            return ret;
        }
    }

    return rm_atcmd_w_core_driver_handle_error_resp(__func__, ret, at_resp_msg);
}

static fsp_err_t parse_adc_init_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;

    memcpy((adc_w_extended_cfg_t *) p_ctrl->p_cfg->p_adc->p_cfg->p_extend, &g_atcmd_adc_cfg_ext_default,
           sizeof(adc_w_extended_cfg_t));
    ret = p_ctrl->p_cfg->p_adc->p_api->open(p_ctrl->p_cfg->p_adc->p_ctrl, p_ctrl->p_cfg->p_adc->p_cfg);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_NOT_OPEN, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_adc_chen_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    char        * trim_cmd                = at_req_cmd;
    char        * tp;
    adc_channel_t channel = ADC_CHANNEL_0;

    trim_cmd = trim_cmd + strlen("+ADCCHEN");

    if (strlen(trim_cmd) < (AT_ADCCHEN_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);

    /* Get channel */
    tp = SPLIT(trim_cmd, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    channel = (adc_channel_t) strtoul(tp, NULL, 10);

    if (BSP_FEATURE_ADC_W_MAX_NUM_CHANNELS <= (uint32_t) channel)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    g_atcmd_adc_w_scan_cfg.scan_mask = 1U << channel;

    ret = p_ctrl->p_cfg->p_adc->p_api->scanCfg(p_ctrl->p_cfg->p_adc->p_ctrl, &g_atcmd_adc_w_scan_cfg);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_adc_start_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    char * trim_cmd = at_req_cmd;
    char * tp;

    /* trim_cmd[]: "=1" */
    trim_cmd = trim_cmd + strlen("+ADCSTART");

    if (strlen(trim_cmd) < (AT_ADCSTART_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* Get divider */
    /* trim_cmd[]="" */
    /* tp: "1" */
    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);
    tp       = SPLIT(trim_cmd, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    adc_w_extended_cfg_t * p_adc_cfg_ext = (adc_w_extended_cfg_t *) p_ctrl->p_cfg->p_adc->p_cfg->p_extend;
    p_adc_cfg_ext->conversion_clockdiv = (uint16_t) strtoul(tp, NULL, 10);

    ret = p_ctrl->p_cfg->p_adc->p_api->scanCfg(p_ctrl->p_cfg->p_adc->p_ctrl, &g_atcmd_adc_w_scan_cfg);

    if (FSP_SUCCESS != ret)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    ret = p_ctrl->p_cfg->p_adc->p_api->scanStart(p_ctrl->p_cfg->p_adc->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_adc_read_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    uint32_t      bytes   = 0;
    adc_channel_t channel = ADC_CHANNEL_0;
    uint32_t      i;
    char        * trim_cmd = at_req_cmd;
    char        * tp;
    uint16_t      adc_data = 0;
    uint8_t       resp_idx = 0;

    /* trim_cmd[]: "=0,10" */
    trim_cmd = trim_cmd + strlen("+ADCREAD");

    if (strlen(trim_cmd) < (AT_ADCREAD_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* trim_cmd[]: "0,10" */
    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);

    /* Get length */
    /* trim_cmd[]="10" */
    /* tp: "0" */
    tp = SPLIT(trim_cmd, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    channel = (adc_channel_t) strtoul(tp, NULL, 10);

    if (BSP_FEATURE_ADC_W_MAX_NUM_CHANNELS <= (uint32_t) channel)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* tp: "10" */
    tp = SPLIT(NULL, AT_CMD_VAR_MRK);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    bytes = strtoul(tp, NULL, 10);

    if ((1 > bytes) || (ADC_READ_MAX_NUMBER < bytes))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    sprintf(at_resp_msg, "\n[ ");
    resp_idx += 3;

    for (i = 0; i < bytes; i++)
    {
        ret = p_ctrl->p_cfg->p_adc->p_api->read(p_ctrl->p_cfg->p_adc->p_ctrl, channel, &adc_data);
        if (FSP_SUCCESS != ret)
        {
            break;
        }

        sprintf(at_resp_msg + resp_idx, "%5d ", adc_data);
        resp_idx = resp_idx + 6;
    }

    if (FSP_SUCCESS == ret)
    {
        sprintf(at_resp_msg + resp_idx, "]\n");
        resp_idx += 2;
        snprintf(at_resp_msg + resp_idx, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_adc_stop_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    ret = p_ctrl->p_cfg->p_adc->p_api->scanStop(p_ctrl->p_cfg->p_adc->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_adc_end_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    ret = p_ctrl->p_cfg->p_adc->p_api->close(p_ctrl->p_cfg->p_adc->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_spi_conf_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    FSP_PARAMETER_NOT_USED(p_api_ctrl);
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_UNSUPPORTED, at_resp_msg);;
}

static fsp_err_t parse_pwm_init_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;

    memcpy((tim_w_extended_cfg_t *) p_ctrl->p_cfg->p_gpt->p_cfg->p_extend, &g_atcmd_gpt_cfg_ext_default,
           sizeof(tim_w_extended_cfg_t));
    ret = p_ctrl->p_cfg->p_gpt->p_api->open(p_ctrl->p_cfg->p_gpt->p_ctrl, p_ctrl->p_cfg->p_gpt->p_cfg);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_NOT_OPEN, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_pwm_start_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    uint32_t     period_ms                = 0;
    uint32_t     period_counts            = 0;
    uint32_t     duty        = 0;
    uint32_t     duty_counts = 0;
    uint8_t      length      = 0;
    uint8_t      ret_size    = 0;
    timer_info_t timer_info  = {0};
    char       * trim_cmd    = at_req_cmd;
    char       * tp;

    /* trim_cmd[]: "=0,40,50,0" */
    trim_cmd = trim_cmd + strlen("+PWMSTART");

    if (strlen(trim_cmd) < (AT_PWMSTART_LENGTH_MIN + strlen(AT_CMD_CLASS_BC_EXT)))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* trim_cmd[]: "0,40,50,0" */
    trim_cmd = trim_cmd + strlen(AT_CMD_CLASS_BC_EXT);

    /* Get channel but discard(channel is fixed as 0)*/
    /* trim_cmd[]: "40,50,0" */
    /* tp: "0" */
    tp     = SPLIT_WL(trim_cmd, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* Get period */
    /* trim_cmd[]: "50,0" */
    /* tp: "40" */
    tp     = SPLIT_WL(NULL, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    period_ms = strtoul(tp, NULL, 10);

    /* Get duty */
    /* trim_cmd[]: "0" */
    /* tp: "50" */
    tp     = SPLIT_WL(NULL, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    duty = strtoul(tp, NULL, 10);

    if ((duty < 1) || (duty > 100))
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* Get mode cycle but discard(mode cycle is fixed as 0) */
    /* trim_cmd[]: "" */
    /* tp: "0" */
    tp     = SPLIT_WL(NULL, AT_CMD_VAR_MRK, &ret_size);
    length = (uint8_t) (length + ret_size + 1);

    if (NULL == tp)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    /* Calcutale actual period_counts from period_ms */
    ret           = p_ctrl->p_cfg->p_gpt->p_api->infoGet(p_ctrl->p_cfg->p_gpt->p_ctrl, &timer_info);
    period_counts = period_ms * (timer_info.clock_frequency / 1000);

    if (period_counts > 0xFFFF)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    ret = p_ctrl->p_cfg->p_gpt->p_api->periodSet(p_ctrl->p_cfg->p_gpt->p_ctrl, period_counts);

    if (FSP_SUCCESS != ret)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    /* Calculate actual dudy_counts from duty */
    duty_counts = period_counts * duty / 100;

    if (0 == duty_counts)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
    }

    ret = p_ctrl->p_cfg->p_gpt->p_api->dutyCycleSet(p_ctrl->p_cfg->p_gpt->p_ctrl, duty_counts, 0);

    if (FSP_SUCCESS != ret)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    ret = p_ctrl->p_cfg->p_gpt->p_api->start(p_ctrl->p_cfg->p_gpt->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static fsp_err_t parse_pwm_stop_cmd (atcmd_w_ctrl_t * const p_api_ctrl, char * at_req_cmd, char * at_resp_msg)
{
    fsp_err_t ret = FSP_SUCCESS;
    atcmd_w_core_instance_ctrl_t * p_ctrl = (atcmd_w_core_instance_ctrl_t *) p_api_ctrl;
    FSP_PARAMETER_NOT_USED(at_req_cmd);

    ret = p_ctrl->p_cfg->p_gpt->p_api->stop(p_ctrl->p_cfg->p_gpt->p_ctrl);

    if (FSP_SUCCESS == ret)
    {
        snprintf(at_resp_msg, ATCMD_W_RESP_LEN_MAX, "%s\n", AT_CMD_RESP_TEXT_OK);
    }
    else
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_STATE, at_resp_msg);
    }

    return ret;
}

static void rm_atcmd_w_core_driver_init_io_pwm (void)
{
    R_GPIO_W_PinCfg(&g_atcmd_drv_ioport_ctrl, PWM_TEST_OUTPUT_PIN_FOR_PWM, IOPORT_PERIPHERAL_PWM);
}

/***********************************************************************************************************************
 * Exported functions
 **********************************************************************************************************************/
void rm_atcmd_w_core_driver_init (void)
{
    R_GPIO_W_Open(&g_atcmd_drv_ioport_ctrl, &g_unused_pin_cfg);
}

fsp_err_t rm_atcmd_w_core_driver_handle_req (atcmd_w_ctrl_t * const p_api_ctrl,
                                             char                 * at_req_cmd,
                                             char                 * at_resp_msg,
                                             uint8_t                at_cmd_idx)
{
    fsp_err_t ret = FSP_SUCCESS;
    char    * trim_cmd;

    /* Check for the prefix marker in the AT+ command request */
    if (rm_atcmd_w_core_driver_check_prefix(AT_CMD_MRK, at_req_cmd) != AT_PREFIX_RES_MATCH)
    {
        return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_WIFI_INV_PARAM_VAL, at_resp_msg);
    }

    /* Trim the AT prefix */
    trim_cmd = at_req_cmd + strlen(AT_CMD_PREFIX);

    /* Check if AT+ command request is valid and process it */
    switch (at_cmd_idx)
    {
        /* I2C Command */
        case AT_I2CINIT_CMD:
        {
            ret = parse_i2c_init_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_I2CREAD_CMD:
        {
            ret = parse_i2c_read_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_I2CWRITE_CMD:
        {
            ret = parse_i2c_write_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_I2CEND_CMD:
        {
            ret = parse_i2c_end_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        /* ADC Command */
        case AT_ADCINIT_CMD:
        {
            ret = parse_adc_init_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_ADCCHEN_CMD:
        {
            ret = parse_adc_chen_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_ADCSTART_CMD:
        {
            ret = parse_adc_start_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_ADCREAD_CMD:
        {
            ret = parse_adc_read_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_ADCSTOP_CMD:
        {
            ret = parse_adc_stop_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_ADCEND_CMD:
        {
            ret = parse_adc_end_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_SPICONF_CMD:
        {
            ret = parse_spi_conf_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_PWMINIT_CMD:
        {
            ret = parse_pwm_init_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            rm_atcmd_w_core_driver_init_io_pwm();
            break;
        }

        case AT_PWMSTART_CMD:
        {
            ret = parse_pwm_start_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        case AT_PWMSTOP_CMD:
        {
            ret = parse_pwm_stop_cmd(p_api_ctrl, trim_cmd, at_resp_msg);
            break;
        }

        default:
        {

            /* AT command invalid or not found */
            return rm_atcmd_w_core_driver_handle_error_resp(__func__, FSP_ERR_INVALID_ARGUMENT, at_resp_msg);
        }
    }

    return ret;
}
