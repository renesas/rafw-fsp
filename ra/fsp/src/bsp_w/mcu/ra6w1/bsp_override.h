/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU_RA6W1
 * @{
 **********************************************************************************************************************/

/** @} (end addtogroup BSP_MCU_RA6W1) */

#ifndef BSP_OVERRIDE_H
#define BSP_OVERRIDE_H

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/* Define overrides required for this MCU. */
#define BSP_OVERRIDE_ADC_EVENT_T
#define BSP_OVERRIDE_ADC_INCLUDE
#define BSP_OVERRIDE_ADC_INFO_T
#define BSP_OVERRIDE_ADC_RESOLUTION_T

#define BSP_OVERRIDE_TIMER_MODE_T
#define BSP_OVERRIDE_TIMER_EVENT_T
#define BSP_OVERRIDE_TIMER_SOURCE_DIV_T
#define BSP_OVERRIDE_TRANSFER_ADDR_MODE_T
#define BSP_OVERRIDE_TRANSFER_REPEAT_AREA_T
#define BSP_OVERRIDE_TRANSFER_CHAIN_MODE_T
#define BSP_OVERRIDE_TRANSFER_INFO_T
#define BSP_OVERRIDE_TRANSFER_IRQ_T
#define BSP_OVERRIDE_TRANSFER_SIZE_T
#define BSP_OVERRIDE_TRANSFER_MODE_T

#define BSP_OVERRIDE_WDT_TIMEOUT_T

/** ADC callback event definitions  */
typedef enum e_adc_event
{
    ADC_EVENT_SCAN_COMPLETE,           ///< Normal/Group A scan complete
    ADC_EVENT_SCAN_COMPLETE_GROUP_B,   ///< Group B scan complete
    ADC_EVENT_SCAN_COMPLETE_GROUP_C,   ///< Group C scan complete
    ADC_EVENT_CALIBRATION_COMPLETE,    ///< Calibration complete
    ADC_EVENT_CONVERSION_COMPLETE,     ///< Conversion complete
    ADC_EVENT_CALIBRATION_REQUEST,     ///< Calibration requested
    ADC_EVENT_CONVERSION_ERROR,        ///< Scan error
    ADC_EVENT_OVERFLOW,                ///< Overflow occurred
    ADC_EVENT_LIMIT_CLIP,              ///< Limiter clipping occurred
    ADC_EVENT_FIFO_READ_REQUEST,       ///< FIFO read requested
    ADC_EVENT_FIFO_OVERFLOW,           ///< FIFO overflow occurred
    ADC_EVENT_THD_UNDER,               ///< Conversion result was below the threshold
    ADC_EVENT_THD_OVER,                ///< Conversion result exceeded the threshold
} adc_event_t;

/** ADC data resolution definitions */
typedef enum e_adc_resolution
{
    ADC_RESOLUTION_12_BIT = 0,         ///< 12 bit resolution
    ADC_RESOLUTION_10_BIT = 1,         ///< 10 bit resolution
    ADC_RESOLUTION_7_BIT  = 2,         ///< 7 bit resolution
    ADC_RESOLUTION_4_BIT  = 3,         ///< 4 bit resolution
} adc_resolution_t;

/** ADC Information Structure for Transfer Interface */
typedef struct st_adc_info
{
    void   * p_address;                ///< The address to start reading the data from
    uint32_t length;                   ///< The total number of transfers to read
} adc_info_t;

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** Options to configure pin functions  */
typedef enum e_gpio_w_cfg_options
{
    /* PUPD */
    GPIO_W_CFG_PERIPHERAL_PIN        = 0x00000000, ///< Enables pin to operate as a peripheral pin
    GPIO_W_CFG_PULLDOWN_ENABLE       = 0x00000200, ///< Enables pull down
    GPIO_W_CFG_PULLUP_ENABLE         = 0x00000100, ///< Enables pull up
    GPIO_W_CFG_PORT_DIRECTION_INPUT  = 0x00000000, ///< Sets the pin direction to input
    GPIO_W_CFG_PORT_DIRECTION_OUTPUT = 0x00000300, ///< Sets the pin direction to output

    /* PPOD */
    GPIO_W_CFG_OPEN_DRAIN_ENABLE = 0x00000700,     ///< Enables open-drain output

    /* PAD input selection */
    GPIO_W_CFG_SCHMITT_TRIG_ENABLE = 0x00000800,   ///< Enables Schmitt trigger for input pin

    /* Drive Strength */
    GPIO_W_CFG_DRV_2mA  = 0x00000000,              ///< 2mA
    GPIO_W_CFG_DRV_4mA  = 0x00001000,              ///< 4mA
    GPIO_W_CFG_DRV_8mA  = 0x00002000,              ///< 8mA
    GPIO_W_CFG_DRV_14mA = 0x00003000,              ///< 14mA

    /* Standard PAD slew rate control */
    GPIO_W_CFG_SLW_FAST = 0x00000000,              ///< Fast
    GPIO_W_CFG_SLW_SLOW = 0x00004000,              ///< Slow

    /* Standard PAD Parametric Output control, parametric inverted data */

    // TIN-TODO: Most probably the name of this field was copy pasted from the corresponding RA enum & needs to be changed to match the actual functionality of the corresponding register field.
    GPIO_W_CFG_ANALOG_ENABLE = 0x00008000,    ///< Enabled

    /* PIN LEVEL */
    GPIO_W_CFG_PORT_OUTPUT_LOW  = 0x00000000, ///< Sets the pin level to low
    GPIO_W_CFG_PORT_OUTPUT_HIGH = 0x00400000, ///< Sets the pin level to high

    /* GPIO CFG */
    GPIO_W_CFG_RETENTION  = 0x20000000,       ///< The pin should not float during sleep
    GPIO_W_CFG_IRQ_ENABLE = 0x40000000,       ///< Enables IRQ
} gpio_w_cfg_options_t;

typedef uint32_t wdt_timeout_t;

/** Timer operational modes */
typedef enum e_timer_mode
{
    TIMER_MODE_PERIODIC,               ///< Timer restarts after period elapses.
    TIMER_MODE_ONE_SHOT,               ///< Timer stops after period elapses.
    TIMER_MODE_PWM,                    ///< Timer generates square-wave PWM output.
    TIMER_MODE_EDGE_DETECT,            ///< Timer asynchronously counting up edges.
} timer_mode_t;

/** Events that can trigger a callback function */
typedef enum e_timer_event
{
    TIMER_EVENT_CYCLE_END,                    ///< Requested timer delay has expired or timer has wrapped around
    TIMER_EVENT_PULSE_CNT_CYCLE_END,          ///< Requested number of pulses were triggered
    TIMER_EVENT_SEQUENTIAL_CAPTURE_CYCLE_END, ///< Requested number of captures were triggered
    TIMER_EVENT_OVERFLOW,                     ///< Timer overflow event (counter reached 32-bit value)
    TIMER_EVENT_UNDERFLOW,                    ///< Timer underflow event (counter went under zero (0) value)
    TIMER_EVENT_CAPTURE_A,                    ///< A capture has occurred on signal A
    TIMER_EVENT_CAPTURE_B,                    ///< A capture has occurred on signal B
    TIMER_EVENT_CAPTURE_C,                    ///< A capture has occurred on signal C
    TIMER_EVENT_CAPTURE_D,                    ///< A capture has occurred on signal D
    TIMER_EVENT_CAPTURE_E,                    ///< A capture has occurred on signal E
    TIMER_EVENT_CAPTURE_F,                    ///< A capture has occurred on signal F
    TIMER_EVENT_CAPTURE_G,                    ///< A capture has occurred on signal G
    TIMER_EVENT_CAPTURE_H,                    ///< A capture has occurred on signal H
    TIMER_EVENT_COMPARE_A,                    ///< A compare has occurred on signal A
    TIMER_EVENT_COMPARE_B,                    ///< A compare has occurred on signal B
    TIMER_EVENT_COMPARE_C,                    ///< A compare has occurred on signal C
    TIMER_EVENT_COMPARE_D,                    ///< A compare has occurred on signal D
    TIMER_EVENT_COMPARE_E,                    ///< A compare has occurred on signal E
    TIMER_EVENT_COMPARE_F,                    ///< A compare has occurred on signal F
    TIMER_EVENT_COMPARE_G,                    ///< A compare has occurred on signal G
    TIMER_EVENT_COMPARE_H,                    ///< A compare has occurred on signal H
} timer_event_t;

typedef enum e_timer_source_div
{
    TIMER_SOURCE_DIV_1  = 0,           ///< Timer clock source divided by 1
    TIMER_SOURCE_DIV_2  = 1,           ///< Timer clock source divided by 2
    TIMER_SOURCE_DIV_3  = 2,           ///< Timer clock source divided by 3
    TIMER_SOURCE_DIV_4  = 3,           ///< Timer clock source divided by 4
    TIMER_SOURCE_DIV_5  = 4,           ///< Timer clock source divided by 5
    TIMER_SOURCE_DIV_6  = 5,           ///< Timer clock source divided by 6
    TIMER_SOURCE_DIV_7  = 6,           ///< Timer clock source divided by 7
    TIMER_SOURCE_DIV_8  = 7,           ///< Timer clock source divided by 8
    TIMER_SOURCE_DIV_9  = 8,           ///< Timer clock source divided by 9
    TIMER_SOURCE_DIV_10 = 9,           ///< Timer clock source divided by 10
    TIMER_SOURCE_DIV_11 = 10,          ///< Timer clock source divided by 11
    TIMER_SOURCE_DIV_12 = 11,          ///< Timer clock source divided by 12
    TIMER_SOURCE_DIV_13 = 12,          ///< Timer clock source divided by 13
    TIMER_SOURCE_DIV_14 = 13,          ///< Timer clock source divided by 14
    TIMER_SOURCE_DIV_15 = 14,          ///< Timer clock source divided by 15
    TIMER_SOURCE_DIV_16 = 15,          ///< Timer clock source divided by 16
    TIMER_SOURCE_DIV_17 = 16,          ///< Timer clock source divided by 17
    TIMER_SOURCE_DIV_18 = 17,          ///< Timer clock source divided by 18
    TIMER_SOURCE_DIV_19 = 18,          ///< Timer clock source divided by 19
    TIMER_SOURCE_DIV_20 = 19,          ///< Timer clock source divided by 20
    TIMER_SOURCE_DIV_21 = 20,          ///< Timer clock source divided by 21
    TIMER_SOURCE_DIV_22 = 21,          ///< Timer clock source divided by 22
    TIMER_SOURCE_DIV_23 = 22,          ///< Timer clock source divided by 23
    TIMER_SOURCE_DIV_24 = 23,          ///< Timer clock source divided by 24
    TIMER_SOURCE_DIV_25 = 24,          ///< Timer clock source divided by 25
    TIMER_SOURCE_DIV_26 = 25,          ///< Timer clock source divided by 26
    TIMER_SOURCE_DIV_27 = 26,          ///< Timer clock source divided by 27
    TIMER_SOURCE_DIV_28 = 27,          ///< Timer clock source divided by 28
    TIMER_SOURCE_DIV_29 = 28,          ///< Timer clock source divided by 29
    TIMER_SOURCE_DIV_30 = 29,          ///< Timer clock source divided by 30
    TIMER_SOURCE_DIV_31 = 30,          ///< Timer clock source divided by 31
    TIMER_SOURCE_DIV_32 = 31,          ///< Timer clock source divided by 32
} timer_source_div_t;

/** Address mode specifies whether to modify (increment or decrement) pointer after each transfer. */
typedef enum e_transfer_addr_mode
{
    /** Address pointer remains fixed after each transfer. */
    TRANSFER_ADDR_MODE_FIXED = 0,

    /** Offset is added to the address pointer after each transfer. */
    TRANSFER_ADDR_MODE_OFFSET = 1,

    /** Address pointer is incremented by associated @ref transfer_size_t after each transfer. */
    TRANSFER_ADDR_MODE_INCREMENTED = 2,

    /** Address pointer is decremented by associated @ref transfer_size_t after each transfer. */
    TRANSFER_ADDR_MODE_DECREMENTED = 3
} transfer_addr_mode_t;

/** Repeat area options (source or destination).  In @ref TRANSFER_MODE_REPEAT, the selected pointer returns to its
 *  original value after transfer_info_t::length transfers.  In @ref TRANSFER_MODE_BLOCK and @ref TRANSFER_MODE_REPEAT_BLOCK,
 *  the selected pointer returns to its original value after each transfer. */
typedef enum e_transfer_repeat_area
{
    /** Destination area repeated in @ref TRANSFER_MODE_REPEAT or @ref TRANSFER_MODE_BLOCK or @ref TRANSFER_MODE_REPEAT_BLOCK. */
    TRANSFER_REPEAT_AREA_DESTINATION = 0,

    /** Source area repeated in @ref TRANSFER_MODE_REPEAT or @ref TRANSFER_MODE_BLOCK or @ref TRANSFER_MODE_REPEAT_BLOCK. */
    TRANSFER_REPEAT_AREA_SOURCE = 1
} transfer_repeat_area_t;

/** Chain transfer mode options.
 *  @note Only applies for DTC. */
typedef enum e_transfer_chain_mode
{
    /** Chain mode not used. */
    TRANSFER_CHAIN_MODE_DISABLED = 0,

    /** Switch to next transfer after a single transfer from this @ref transfer_info_t. */
    TRANSFER_CHAIN_MODE_EACH = 2,

    /** Complete the entire transfer defined in this @ref transfer_info_t before chaining to next transfer. */
    TRANSFER_CHAIN_MODE_END = 3
} transfer_chain_mode_t;

/** Interrupt options. */
typedef enum e_transfer_irq
{
    /** Interrupt occurs only after last transfer. If this transfer is chained to a subsequent transfer,
     *  the interrupt will occur only after subsequent chained transfer(s) are complete.
     *  @warning  DTC triggers the interrupt of the activation source.  Choosing TRANSFER_IRQ_END with DTC will
     *            prevent activation source interrupts until the transfer is complete. */
    TRANSFER_IRQ_END = 0,

    /** Interrupt occurs after each transfer.
     *  @note     Not available in all HAL drivers.  See HAL driver for details. */
    TRANSFER_IRQ_EACH = 1
} transfer_irq_t;

/** Transfer size specifies the size of each individual transfer.
 *  Total transfer length = transfer_size_t * transfer_length_t
 */
typedef enum e_transfer_size
{
    TRANSFER_SIZE_1_BYTE = 0,          ///< Each transfer transfers a 8-bit value
    TRANSFER_SIZE_2_BYTE = 1,          ///< Each transfer transfers a 16-bit value
    TRANSFER_SIZE_4_BYTE = 2,          ///< Each transfer transfers a 32-bit value
    TRANSFER_SIZE_8_BYTE = 3           ///< Each transfer transfers a 64-bit value
} transfer_size_t;

/** Transfer burst mode specifies if burst mode is enabled and which is the burst size.
 *  @note Only applies for DMAC. */
typedef enum e_transfer_burst_mode
{
    TRANSFER_BURST_MODE_DISABLED = 0,  ///< Burst mode is disabled.
    TRANSFER_BURST_MODE_4X       = 1,  ///< Burst mode is enabled, burst size of 4 data units is used.
    TRANSFER_BURST_MODE_8X       = 2,  ///< Burst mode is enabled, burst size of 8 data units is used.
} transfer_burst_mode_t;

/** Transfer mode describes what will happen when a transfer request occurs. */
typedef enum e_transfer_mode
{
    /** In normal mode, each transfer request causes a transfer of @ref transfer_size_t from the source pointer to
     *  the destination pointer.  The transfer length is decremented and the source and address pointers are
     *  updated according to @ref transfer_addr_mode_t.  After the transfer length reaches 0, transfer requests
     *  will not cause any further transfers. */
    TRANSFER_MODE_NORMAL = 0,

    /** Repeat mode is like normal mode, except that when the transfer length reaches 0, the pointer to the
     *  repeat area and the transfer length will be reset to their initial values.  If DMAC is used, the
     *  transfer repeats only transfer_info_t::num_blocks times.  After the transfer repeats
     *  transfer_info_t::num_blocks times, transfer requests will not cause any further transfers.  If DTC is
     *  used, the transfer repeats continuously (no limit to the number of repeat transfers). */
    TRANSFER_MODE_REPEAT = 1,

    /** In block mode, each transfer request causes transfer_info_t::length transfers of @ref transfer_size_t.
     *  After each individual transfer, the source and destination pointers are updated according to
     *  @ref transfer_addr_mode_t.  After the block transfer is complete, transfer_info_t::num_blocks is
     *  decremented.  After the transfer_info_t::num_blocks reaches 0, transfer requests will not cause any
     *  further transfers. */
    TRANSFER_MODE_BLOCK = 2,

    /** In addition to block mode features, repeat-block mode supports a ring buffer of blocks and offsets
     *  within a block (to split blocks into arrays of their first data, second data, etc.) */
    TRANSFER_MODE_REPEAT_BLOCK = 3
} transfer_mode_t;

/** This structure specifies the properties of the transfer.
 *  @warning  When using DTC, this structure corresponds to the descriptor block registers required by the DTC.
 *            The following components may be modified by the driver: p_src, p_dest, num_blocks, and length.
 *  @warning  When using DTC, do NOT reuse this structure to configure multiple transfers.  Each transfer must
 *            have a unique transfer_info_t.
 *  @warning  When using DTC, this structure must not be allocated in a temporary location.  Any instance of this
 *            structure must remain in scope until the transfer it is used for is closed.
 *  @note     When using DTC, consider placing instances of this structure in a protected section of memory. */
typedef struct st_transfer_info
{
    union
    {
        struct
        {
            uint32_t : 16;

            /** Select if burst mode is enable and what the burst length will be. */
            transfer_burst_mode_t burst_mode : 2;

            /** Select what happens to destination pointer after each transfer. */
            transfer_addr_mode_t dest_addr_mode : 2;

            /** Select to repeat source or destination area, unused in @ref TRANSFER_MODE_NORMAL. */
            transfer_repeat_area_t repeat_area : 1;

            /** Select if interrupts should occur after each individual transfer or after the completion of all planned
             *  transfers. */
            transfer_irq_t irq : 1;

            /** Select when the chain transfer ends. */
            transfer_chain_mode_t chain_mode : 2;

            uint32_t : 2;

            /** Select what happens to source pointer after each transfer. */
            transfer_addr_mode_t src_addr_mode : 2;

            /** Select number of bytes to transfer at once. @see transfer_info_t::length. */
            transfer_size_t size : 2;

            /** Select mode from @ref transfer_mode_t. */
            transfer_mode_t mode : 2;
        } transfer_settings_word_b;

        uint32_t transfer_settings_word;
    };

    void const * volatile p_src;       ///< Source pointer
    void * volatile       p_dest;      ///< Destination pointer

    /** Number of blocks to transfer when using @ref TRANSFER_MODE_BLOCK (both DTC an DMAC) or
     * @ref TRANSFER_MODE_REPEAT (DMAC only) or
     * @ref TRANSFER_MODE_REPEAT_BLOCK (DMAC only), unused in other modes. */
    volatile uint16_t num_blocks;

    /** Length of each transfer.  Range limited for @ref TRANSFER_MODE_BLOCK, @ref TRANSFER_MODE_REPEAT,
     *  and @ref TRANSFER_MODE_REPEAT_BLOCK
     *  see HAL driver for details. */
    volatile uint16_t length;
} transfer_info_t;
#endif
