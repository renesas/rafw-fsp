/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef R_OSPI_W_H
#define R_OSPI_W_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

#include "r_ospi_w_cfg.h"
#include "r_spi_flash_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/*******************************************************************************************************************//**
 * @addtogroup OSPI_W
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

 /* Memory mapped address. */
#define OSPI_W_DEVICE_START_ADDRESS         (BSP_FEATURE_OSPI_DEVICE_0_START_ADDRESS)
#define OSPI_W_DEVICE_START_ADDRESS_DATA    (BSP_FEATURE_OSPI_DEVICE_0_START_ADDRESS_DATA)

/* Sector size. */
#define OSPI_W_FLASH_SECTOR_SIZE            (0x1000U)

/* Section where the r_ospi_w const variables are placed. It needs to be placed in RAM. */
#define OSPI_W_SECTION_CONST_DATA           (".data")

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** OSPI_W memory access mode. */
typedef enum e_ospi_w_access_mode
{
    OSPI_W_ACCESS_MODE_MANUAL = 0,     ///< Direct register access using the OSPI_W register file.
    OSPI_W_ACCESS_MODE_AUTO   = 1,     ///< Up to 32 MB memory-mapped access with 3- and 4-byte addressing modes.
} ospi_w_access_mode_t;

/** OSPI_W Bus mode. */
typedef enum e_ospi_w_bus_mode
{
    OSPI_W_BUS_MODE_SINGLE,            ///< Bus mode in single mode.
    OSPI_W_BUS_MODE_DUAL,              ///< Bus mode in dual mode.
    OSPI_W_BUS_MODE_QUAD,              ///< Bus mode in quad mode.
    OSPI_W_BUS_MODE_OCTAL,             ///< Bus mode in octal mode.
} ospi_w_bus_mode_t;

/** OSPI_W CLK divider. */
typedef enum e_ospi_w_clk_div
{
    OSPI_W_CLK_DIV_1 = 0x0,            ///< OSPI CLK = CLK / 1.
    OSPI_W_CLK_DIV_2 = 0x1,            ///< OSPI CLK = CLK / 2.
    OSPI_W_CLK_DIV_4 = 0x2,            ///< OSPI CLK = CLK / 4.
    OSPI_W_CLK_DIV_8 = 0x3,            ///< OSPI CLK = CLK / 8.
} ospi_w_clk_div_t;

/** The progress of sector/block erasing. */
typedef enum e_ospi_w_ers
{
    OSPI_W_ERS_NO        = 0,          ///< No erase.
    OSPI_W_ERS_PENDING   = 1,          ///< Pending erase request.
    OSPI_W_ERS_RUNNING   = 2,          ///< Erase procedure is running.
    OSPI_W_ERS_SUSPENDED = 3,          ///< Suspended erase procedure.
    OSPI_W_ERS_FINISHING = 4,          ///< Finishing the erase procedure.
} ospi_w_ers_t;

/** OSPI clock mode. */
typedef enum e_ospi_w_clk_mode
{
    OSPI_W_CLK_MODE_LOW  = 0,          ///< OSPI_SCK is low when OSPI_CS is high.
    OSPI_W_CLK_MODE_HIGH = 1,          ///< OSPI_SCK is high when OSPI_CS is high.
} ospi_w_clk_mode_t;

/** OSPIF pads drive current strength. The current value depends on the device. */
typedef enum e_ospi_w_drive_current
{
    OSPI_W_DRIVE_CURRENT_0 = 0,
    OSPI_W_DRIVE_CURRENT_1 = 1,
    OSPI_W_DRIVE_CURRENT_2 = 2,
    OSPI_W_DRIVE_CURRENT_3 = 3,
} ospi_w_drive_current_t;

/**
 * OSPIF read pipe setting.
 *
 * When read pipe is disabled the sampling clock is determined by OSPI_W_SAMPLING_EDGE
 * otherwise by OSPI_W_READ_PIPE_DELAY.
 */
typedef enum e_ospi_w_read_pipe
{
    OSPI_W_READ_PIPE_DISABLE = 0,      ///< Disable read pipe delay.
    OSPI_W_READ_PIPE_ENABLE  = 1,      ///< Enable read pipe delay.
} ospi_w_read_pipe_t;

/**
 * OSPIF Read pipe clock delay in relation to the falling edge of OSPI_SCK.
 *
 * The read pipe delay should be set based on the voltage level of the power rail V12.
 */
typedef enum e_ospi_w_read_pipe_delay
{
    OSPI_W_READ_PIPE_DELAY_0 = 0,      ///< Set read pipe delay to 0.
    OSPI_W_READ_PIPE_DELAY_1 = 1,      ///< Set read pipe delay to 1.
    OSPI_W_READ_PIPE_DELAY_2 = 2,      ///< Set read pipe delay to 2.
    OSPI_W_READ_PIPE_DELAY_3 = 3,      ///< Set read pipe delay to 3.
    OSPI_W_READ_PIPE_DELAY_4 = 4,      ///< Set read pipe delay to 4.
    OSPI_W_READ_PIPE_DELAY_5 = 5,      ///< Set read pipe delay to 5.
    OSPI_W_READ_PIPE_DELAY_6 = 6,      ///< Set read pipe delay to 6.
    OSPI_W_READ_PIPE_DELAY_7 = 7,      ///< Set read pipe delay to 7.
} ospi_w_read_pipe_delay_t;

/** OSPIF pads slew rate. */
typedef enum e_ospi_w_slew_rate
{
    OSPI_W_SLEW_RATE_0 = 0,            ///< Rise = 1.7 V/ns, Fall = 1.9 V/ns (weak).
    OSPI_W_SLEW_RATE_1 = 1,            ///< Rise = 2.0 V/ns, Fall = 2.3 V/ns.
    OSPI_W_SLEW_RATE_2 = 2,            ///< Rise = 2.3 V/ns, Fall = 2.6 V/ns.
    OSPI_W_SLEW_RATE_3 = 3,            ///< Rise = 2.4 V/ns, Fall = 2.7 V/ns (strong)
} ospi_w_slew_rate_t;

/**
 * OSPIF HREADY signal mode when accessing the WRITEDATA, READDATA and DUMMYDATA registers.
 *
 * This configuration is useful when the frequency of the OSPI clock is much lower than
 * the clock of the AMBA bus, in order to avoid locking the AMBA bus for a long time.
 * When is set to OSPI_W_HREADY_MODE_WAIT there is no need to check the OSPIC_BUSY
 * for detecting completion of the requested access.
 */
typedef enum e_ospi_w_hready_mode
{
    OSPI_W_HREADY_MODE_WAIT    = 0,    ///< Adds wait states via hready signal when accessing the OQSPIF_WRITEDATA, OQSPIF_READDATA and OQSPIF_DUMMYDATA registers.
    OSPI_W_HREADY_MODE_NO_WAIT = 1,    ///< Don't add wait states via the HREADY signal.
} ospi_w_hready_mode_t;

/** OSPIF instruction mode. */
typedef enum e_ospi_w_instr_md
{
    OSPI_W_INSTR_MD_TX_AT_ANY_BURST_ACCESS  = 0, ///< Transmit instruction at any burst access.
    OSPI_W_INSTR_MD_TX_ONLY_IN_FIRST_ACCESS = 1, ///< Transmit instruction only in the first access after the selection of Auto Mode.
} ospi_w_instr_md_t;

/** OSPIF wrap mode enable bit. */
typedef enum e_ospi_w_wrap_md
{
    OSPI_W_WRAP_MD_SEL_INSTR    = 0,   ///< The OSPIC_INST is the selected instruction at any read access.
    OSPI_W_WRAP_MD_SEL_INSTR_WB = 1,   ///< The OSPIC_INST_WB is the selected instruction at any read wrapping burst access.
} ospi_w_wrap_md_t;

/** OSPIF write wrapping burst will be implemented by using a special write instruction. */
typedef enum e_ospi_w_wrap_wr_en
{
    OSPI_W_WRAP_WR_EN_INST    = 0,     ///< Select OSPIC_WR_INST.
    OSPI_W_WRAP_WR_EN_INST_WB = 1,     ///< Select OSPIC_WR_INST_WB.
} ospi_w_wrap_wr_en_t;

/** OSPIF behavior when a read burst is terminated early in the AMBA bus. */
typedef enum e_ospi_w_rd_bend_md
{
    OSPI_W_RD_BEND_MD_TERMINATE_ASAP      = 0, ///< The corresponding read burst in the memory device will be terminated as soon as possible, based on the pipeline of the controller.
    OSPI_W_RD_BEND_MD_TERMINATE_WHEN_DONE = 1, ///< The read burst in the memory device will be terminated only after filling the last position of the current read buffer.
} ospi_w_rd_bend_md_t;

/** Set the read pipeline in burst mode while retrieving the data of the memory device. */
typedef enum e_ospi_w_rd_rdb_en
{
    OSPI_W_RD_RDB_EN_MIN_CLK_PULSES = 0, ///< The controller will provide the minimum required number of clock pulses in the interface during the reading of the data.
    OSPI_W_RD_RDB_EN_UNTIL_DONE     = 1, ///< Set the read pipeline in burst mode while the data are retrieved of the memory device.
} ospi_w_rd_rdb_en_t;

/** OSPIF extra byte setting in auto access mode. */
typedef enum e_ospi_w_extra_byte
{
    OSPI_W_EXTRA_BYTE_DISABLE = 0,     ///< Disable extra byte phase.
    OSPI_W_EXTRA_BYTE_ENABLE  = 1,     ///< Enable extra byte phase.
} ospi_w_extra_byte_t;

/**
 * OSPIF extra byte half setting in auto access mode.
 *
 * This setting is out of scope if the extra byte is disabled.
 */
typedef enum e_ospi_w_extra_byte_half
{
    OSPI_W_EXTRA_BYTE_HALF_DISABLE = 0, ///< Transmit the complete extra byte.
    OSPI_W_EXTRA_BYTE_HALF_ENABLE  = 1, ///< The output switches to Hi-Z during the transmission of the low nibble of the extra byte.
} ospi_w_extra_byte_half_t;

/** OSPIF break enable/disable. */
typedef enum e_ospi_w_break
{
    OSPI_W_BREAK_DISABLE = 0,          ///< Disable break command.
    OSPI_W_BREAK_ENABLE  = 1,          ///< Enable break command.
} ospi_w_break_t;

/**
 * OSPIF Disable output during the transmission of the second half (OSPIC_BRK_WRD[3:0]).
 *
 * Setting this bit is only useful if OSPIC_BRK_EN =1 and OSPIC_BRK_SZ= 1.
 */
typedef enum e_ospi_w_break_sec_hf
{
    OSPI_W_BREAK_SEC_HF_DRIVE = 0,     ///< The controller drives the SPI bus during the transmission of the OSPIC_BRK_WRD[3:0].
    OSPI_W_BREAK_SEC_HF_HIZ   = 1,     ///< The controller leaves the SPI bus in Hi-Z during the transmission of the OSPIC_BRK_WORD[3:0].
} ospi_w_break_sec_hf_t;

/** OSPIF tCEM enable/disable. */
typedef enum e_ospi_w_t_cem
{
    OSPI_W_T_CEM_DISABLE = 0,          ///< Disable tCEM control.
    OSPI_W_T_CEM_ENABLE  = 1,          ///< Enable tCEM control.
} ospi_w_t_cem_t;

/** Length of a burst operation for a OSPI RAM device. */
typedef enum e_ospi_w_memblen_burst
{
    OSPI_W_MEMBLEN_BURST_INCR_UNSPECIFIED = 0, ///< The external memory device implements incremental burst of unspecified length.
    OSPI_W_MEMBLEN_BURST_WRAP_4B,              ///< The external memory device implements a wrapping burst of length 4 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_8B,              ///< The external memory device implements a wrapping burst of length 8 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_16B,             ///< The external memory device implements a wrapping burst of length 16 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_32B,             ///< The external memory device implements a wrapping burst of length 32 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_64B,             ///< The external memory device implements a wrapping burst of length 64 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_128B,            ///< The external memory device implements a wrapping burst of length 128 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_256B,            ///< The external memory device implements a wrapping burst of length 256 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_512B,            ///< The external memory device implements a wrapping burst of length 512 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_1024B,           ///< The external memory device implements a wrapping burst of length 1024 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_2048B,           ///< The external memory device implements a wrapping burst of length 2048 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_4096B,           ///< The external memory device implements a wrapping burst of length 4096 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_8192B,           ///< The external memory device implements a wrapping burst of length 8192 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_16384B,          ///< The external memory device implements a wrapping burst of length 16384 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_32768B,          ///< The external memory device implements a wrapping burst of length 32768 bytes.
    OSPI_W_MEMBLEN_BURST_WRAP_65536B,          ///< The external memory device implements a wrapping burst of length 65536 bytes.
} ospi_w_memblen_burst_t;

/**
 * OSPI clock edge setting for the sampling of the incoming data when the read pipe is disabled.
 */
typedef enum e_ospi_w_sampling_edge
{
    OSPI_W_SAMPLING_EDGE_POS = 0,      ///< The incoming data sampling is triggered by the positive edge of OSPI clock signal.
    OSPI_W_SAMPLING_EDGE_NEG = 1,      ///< The incoming data sampling is triggered by the negative edge of OSPI clock signal.
} ospi_w_sampling_edge_t;

/** OSPI DDR or SDR mode. */
typedef enum e_ospi_w_mode
{
    OSPI_W_MODE_DDR = 0,               ///< OSPI DDR (double data rate) mode.
    OSPI_W_MODE_SDR = 1,               ///< OSPI SDR (single data rate) mode.
} ospi_w_mode_t;

/** OSPI device busy status setting. */
typedef enum e_ospi_w_busy_level
{
    OSPI_W_BUSY_LEVEL_LOW  = 0,        ///< The OSPI device is busy when the pin level bit is low.
    OSPI_W_BUSY_LEVEL_HIGH = 1,        ///< The OSPI device is busy when the pin level bit is high.
} ospi_w_busy_level_t;

/**
 * OSPIF timer which is used to count the delay that it has to wait before to read the Flash status register,
 * after an erase or an erase resume command.
 */
typedef enum e_ospi_w_stsdly_sel
{
    OSPI_W_STSDLY_SEL_RESSTS_DLY = 0,  ///< The delay is controlled by the OSPIC_RESSTS_DLY which counts on the OSPI_SCK clock.
    OSPI_W_STSDLY_SEL_RESSUS_DLY = 1,  ///< The delay is controlled by the OSPIC_RESSUS_DLY which counts on the 222 kHz clock.
} ospi_w_stsdly_sel_t;

/** OSPIF value that is transferred on the SPI bus during the phase of the dummy bytes. */
typedef enum e_ospi_w_dummy_value
{
    OSPI_W_RSTAT_DMY_ZERO_KEEP_UNCHANGED = 0, ///< The controller keeps the data in the bus unchanged, until to change the bus direction in input mode.
    OSPI_W_RSTAT_DMY_ZERO_FORCE_ZERO     = 1, ///< Forces the dummy bytes to get the zero value (only for the cycles that are not in input mode).
} ospi_w_dummy_value_t;

/* Index of bus mode. */
typedef enum e_ospi_w_idx_bus_mode
{
    OSPI_W_IDX_INSTR_BUS_MODE    = 0,  ///< Index of instruction bus mode.
    OSPI_W_IDX_NONINSTR_BUS_MODE = 1,  ///< Index of non-instruction bus mode (address/extra/dummy/data bus modes).
} ospi_w_idx_bus_mode_t;

/** Read instruction configuration structure (auto access mode). */
typedef struct st_ospi_w_read_instr_cfg
{
    uint8_t           opcode;                         ///< Read command opcode for Incremental Burst or Single read access.
    uint8_t           opcode_wb;                      ///< Read command opcode for Wrapping Burst.
    uint8_t           extra_byte_value;               ///< Extra Byte value.
    ospi_w_bus_mode_t opcode_bus_mode     : 2;        ///< Bus mode of the opcode phase.
    ospi_w_bus_mode_t addr_bus_mode       : 2;        ///< Bus mode of the address phase.
    ospi_w_bus_mode_t extra_byte_bus_mode : 2;        ///< Bus mode of the extra byte phase.
    ospi_w_bus_mode_t dummy_bus_mode      : 2;        ///< Bus mode of the dummy phase.

    ospi_w_bus_mode_t        data_bus_mode       : 2; ///< Bus mode of the data phase.
    ospi_w_extra_byte_t      extra_byte_en       : 1; ///< Enable Extra Byte.
    ospi_w_extra_byte_half_t extra_byte_half_cfg : 1; ///< Enable Extra Byte Half.
    uint8_t             dummy_bytes              : 5; ///< The number of dummy bytes. Valid values are 0..31 i.e. 1..32 dummy bytes.
    bool                dummy_en;                     ///< Enable dummy bytes.
    ospi_w_instr_md_t   instr_md           : 1;       ///< Instruction mode.
    ospi_w_wrap_md_t    wrap_md            : 1;       ///< Wrap mode enable bit.
    uint8_t             wrap_blen          : 3;       ///< It describes the length in number of bytes of the selected wrapping burst(see also the register OSPIC_WRAP_MD).
    uint8_t             wrap_size          : 2;       ///< Selected data size of a wrapping burst.
    ospi_w_wrap_wr_en_t wrap_wr_en         : 1;       ///< By setting (1) this bit, any write wrapping burst will be implemented by using a special write instruction.
    uint8_t             cs_high_min_cycles : 5;       ///< The SPI bus stays in idle state (OSPI_CS high) for at least this number of OSPI_SCK clock cycles between two consecutive read commands.
    ospi_w_rd_bend_md_t rd_bend_md         : 1;       ///< Defines the behavior of the controller when a read burst is terminated early in the AMBA bus.
    ospi_w_rd_rdb_en_t  rd_rdb_en          : 1;       ///< Write 1 to set the read pipeline in burst mode while are retrieved the data of the memory device.
} ospi_w_read_instr_cfg_t;

/** OSPIF read status instruction configuration structure (auto access mode). */
typedef struct st_ospi_w_read_status_instr_cfg
{
    uint8_t              opcode;               ///< Read Status command opcode.
    ospi_w_bus_mode_t    opcode_bus_mode  : 2; ///< The bus mode of the opcode phase.
    ospi_w_bus_mode_t    receive_bus_mode : 2; ///< The bus mode of the receive data phase.
    uint32_t             busy_pos         : 3; ///< The position of the Busy bit in the status register (0 - 7).
    uint8_t              delay_cycles     : 7; ///< The minimum delay in clock cycles between a Read Status command and the previous Erase command. Usually NOT needed thus is set equal to 0.
    ospi_w_stsdly_sel_t  stsdly_sel       : 1; ///< Select the timer which is used to count the delay before reading the flash status register, after an erase or an erase resume command.
    ospi_w_busy_level_t  busy_level       : 1; ///< Busy bit level.
    uint8_t              rstat_dmy_num    : 4; ///< Number of dummy bytes (minus 1).
    ospi_w_bus_mode_t    dummy_bus_mode   : 2; ///< The bus mode of the dummy data phase.
    ospi_w_dummy_value_t dummy_value      : 1; ///< Define the value that is transferred on the SPI bus during the phase of the dummy bytes.
    bool                 rstat_dmy_en;         ///< Enables the transmission of dummy bytes, immediately after the instruction code of the read status command.
    bool                 rstat_rdb_en;         ///< If enabled, sets the read pipeline in burst mode while is retrieved the status of the memory device.
    bool                 rstat_split_en;       ///< If enabled, a long sequence for the continuous reading of the status register will be divided into individual accesses.
    bool                 rstat_req;            ///< If enabled, the value of the status register of the memory device will be retrieved.
} ospi_w_read_status_instr_cfg_t;

/** OSPIF Erase instruction configuration structure (auto access mode). */
typedef struct st_ospi_w_erase_instr_cfg
{
    ospi_w_bus_mode_t opcode_bus_mode : 2;      ///< Bus mode of the opcode phase.
    ospi_w_bus_mode_t addr_bus_mode   : 2;      ///< Bus mode of the address phase.
    uint32_t          hclk_cycles     : 4;      ///< The number of AMBA AHB hclk cycles (0..15) without memory read requests before executing an erase or erase resume command. Use this setting to delay one of the aforementioned commands otherwise keep it 0.
    uint8_t           opcode;                   ///< Erase command opcode.
    uint8_t           cs_idle_delay_cycles : 5; ///< The minimum CS idle delay in clock cycles (< 32) between a Write Enable, Erase, Erase Suspend or Erase Resume command and the next consecutive command.
} ospi_w_erase_instr_cfg_t;

/** OSPIF Erase suspend/resume instruction structure (auto access mode). */
typedef struct st_ospi_w_suspend_resume_instr_cfg
{
    ospi_w_bus_mode_t suspend_bus_mode : 2;           ///< Bus mode during the erase suspend command phase.
    ospi_w_bus_mode_t resume_bus_mode  : 2;           ///< Bus mode during the erase resume command phase.
    uint8_t           suspend_opcode;                 ///< Erase suspend instruction code.
    uint8_t           resume_opcode;                  ///< Erase resume instruction code.
    uint8_t           res_sus_latency_clk_cycles : 6; ///< The minimum required latency (clock cycles) between an erase resume and the next consequent erase suspend command.
    uint8_t           sussts_dly                 : 6; ///< Defines a timer that counts the minimum allowed delay between an erase suspend command and the next read status command.
} ospi_w_suspend_resume_instr_cfg_t;

/** OSPIF Read break sequence structure (auto access mode). */
typedef struct st_ospi_w_break_instr_cfg
{
    uint16_t              break_opcode;     ///< The code value of the read break instruction.
    uint8_t               break_sz     : 4; ///< The size of Burst Break Sequence 1..16 bytes.
    ospi_w_bus_mode_t     break_tx_md  : 2; ///< The mode of the OSPI Bus during the transmission of the burst break sequence.
    ospi_w_break_sec_hf_t break_sec_hf : 1; ///< Disable output during the transmission of the second half.
    ospi_w_break_t        break_en     : 1; ///< Controls the application of a special command (read burst break sequence) that is used in order to force the device to abandon the continuous read mode.
} ospi_w_break_instr_cfg_t;

/** OSPIF External memory burst length configuration. */
typedef struct st_ospi_w_memblen_cfg
{
    ospi_w_memblen_burst_t memblen : 4;  ///< In this register is defined the expected behavior of the external memory device regarding the length of a burst operation.
    uint16_t               tcem_cc : 12; ///< Defines the maximum allowed time tCEM for which the OSPIC_CS can stay active (when tcem_en=1).
    ospi_w_t_cem_t         tcem_en : 1;  ///< This bit enables the controlling of the maximum time tCEM for which the OSPI_CS remains active (auto mode and PSRAM).
    bool    rd_lin_en              : 1;  ///< By setting (1) in this bit the access limits that are set with the OSPIC_MEMBLEN are ignored for the read operation.
    bool    keep_active            : 1;  ///< By setting (1) in this bit the controller will keep the access active in order to serve consecutive accesses, without toggling the chip select of the memory,when this is possible.
    bool    wcmd_hybrid            : 1;  ///< By setting (1) in this bit, the special commands that used during wrap bursts.
    uint8_t dielen                 : 4;  ///< The size of each die from which the memory device is consisted.
    uint8_t active_thr             : 4;  ///< Defines the maximum number of AHB clock cycles for which the memory should stay without access, before the active access to be aborted.
} ospi_w_memblen_cfg_t;

/** OSPIF Write instruction structure (manual & auto access mode). */
typedef struct st_ospi_w_write_instr_cfg
{
    uint8_t           write_opcode;             ///< The code value of the write instruction.
    uint8_t           write_opcode_wb;          ///< The code value of the write instruction in wrapping burst.
    ospi_w_bus_mode_t opcode_bus_mode : 2;      ///< Bus mode of the opcode phase.
    ospi_w_bus_mode_t addr_bus_mode   : 2;      ///< Bus mode of the address phase.
    ospi_w_bus_mode_t data_bus_mode   : 2;      ///< Bus mode of the data phase.
    ospi_w_bus_mode_t dummy_bus_mode  : 2;      ///< Bus mode of the dummy data phase.
    uint8_t           dummy_bytes     : 5;      ///< The number of dummy bytes. Valid values are 0..31 i.e. 1..32 dummy bytes.
    bool              dummy_en;                 ///< Enable dummy bytes.
    bool              wdex_en;                  ///< If enabled wait until the write data to be available before to start sending the write command sequence.
    uint8_t           cs_hi_min_clk_cycles : 5; ///< After the execution of the write command, the OSPI_CS remains high for at least this number of OSPI_SCK clock cycles.
    bool              send_wen_req;             ///< If enabled the controller will apply the write enable command.
} ospi_w_write_instr_cfg_t;

/** OSPIF write enable instruction configuration structure (auto access mode). */
typedef struct st_ospi_w_write_enable_instr_cfg
{
    ospi_w_bus_mode_t opcode_bus_mode : 2; ///< Bus mode of the opcode phase.
    uint8_t           opcode;              ///< Write Enable command opcode.
} ospi_w_write_enable_instr_cfg_t;

/** OSPIF Double Data Rate (DDR) configuration structure. */
typedef struct st_ospi_w_ctrl_ddr_cfg
{
    uint32_t ctrl_ddra;                ///< DDR Control register A.
    uint32_t ctrl_ddrb;                ///< DDR Control register B.
} ospi_w_ctrl_ddr_cfg_t;

/** OSPIF Extra registers configuration structure. */
typedef struct st_ospi_w_extra_registers_cfg
{
    uint32_t ctrl_mr_reg;              ///< Control MR register.
    uint32_t drst_cmd_reg;             ///< DRST command register.
} ospi_w_extra_registers_cfg_t;

/** OSPIF Enter/Exit QPI instruction configuration structure. */
typedef struct st_ospi_w_qpi_instr_cfg
{
    uint8_t opcode_enter;              ///< Enter QPI command opcode.
    uint8_t opcode_exit;               ///< Exit QPI command opcode.
} ospi_w_qpi_instr_cfg_t;

/**
 * OSPI memory configuration structure.
 *
 * This struct is used to define a driver for a specific OSPI memory.
 */
typedef struct st_ospi_w_flash_cfg
{
    ospi_w_clk_mode_t                         clk_mode : 1;               ///< Clock Mode.
    ospi_w_read_instr_cfg_t const           * p_read_instr_cfg;           ///< Read instruction configuration struct.
    ospi_w_erase_instr_cfg_t const          * p_erase_instr_cfg;          ///< Erase instruction configuration struct.
    ospi_w_suspend_resume_instr_cfg_t const * p_suspend_resume_instr_cfg; ///< Program and erase suspend/resume instruction
    ospi_w_write_enable_instr_cfg_t const   * p_write_enable_instr_cfg;   ///< Write enable instruction configuration.
    ospi_w_read_status_instr_cfg_t const    * p_read_status_instr_cfg;    ///< Read status register instruction
    ospi_w_write_instr_cfg_t const          * p_write_instr_cfg;          ///< Write instruction configuration struct.
    ospi_w_break_instr_cfg_t const          * p_break_instr_cfg;          ///< Burst break instruction configuration struct.
    ospi_w_memblen_cfg_t const              * p_memblen_cfg;              ///< External memory burst length configuration struct.
    ospi_w_ctrl_ddr_cfg_t const             * p_ctrl_ddr;                 ///< OSPI DDR Control registers configuration struct.
    ospi_w_extra_registers_cfg_t const      * p_extra_regs;               ///< OSPI Extra registers configuration struct.
    ospi_w_qpi_instr_cfg_t const            * p_qpi_instr_cfg;            ///< Enter/Exit QPI instruction configuration struct.
} ospi_w_flash_cfg_t;

/** Extended configuration for OSPIF. */
typedef struct st_ospi_w_extended_cfg
{
    uint8_t                    channel;            ///< Channel number to be used (0..1).
    ospi_w_mode_t              ospi_mode;          ///< OSPI DDR or SDR mode.
    ospi_w_clk_div_t           ospi_clk_div;       ///< CLK divider.
    ospi_w_drive_current_t     ospi_drive_current; ///< Set the Drive Strength of the OSPI Controller.
    ospi_w_slew_rate_t         ospi_slew_rate;     ///< Set the Slew Rate of the OSPI Controller.
    ospi_w_flash_cfg_t const * p_ospi_flash_cfg;   ///< Pointer to ospi controller configuration struct.
} ospi_w_extended_cfg_t;

/** Instance control block. DO NOT INITIALIZE.  Initialization occurs when spi_flash_api_t::open is called. */
typedef struct st_ospi_w_instance_ctrl
{
    spi_flash_cfg_t const * p_cfg;                     ///< Pointer to initial configuration.
    spi_flash_data_lines_t  data_lines;                ///< Data lines.
    uint32_t                total_size_bytes;          ///< Total size of the flash in bytes.
    uint32_t                open;                      ///< Whether or not driver is open.
    ospi_w_bus_mode_t       flash_opcode_bus_mode;     ///< Opcode Bus mode of flash.
    ospi_w_bus_mode_t       manual_access_bus_mode[2]; ///< Bus mode for mannual access.
    uint8_t                 manual_command_length;     ///< Command length for mannual access mode.
    bool                    xip_mode_is_enabled;       ///< XiP mode is enabled or disabled.
} ospi_w_instance_ctrl_t;

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/** @cond INC_HEADER_DEFS_SEC */
/** Filled in Interface API structure for this Instance. */
extern const spi_flash_api_t g_ospi_w_on_spi_flash;

/** @endcond */

/***********************************************************************************************************************
 * Public APIs
 **********************************************************************************************************************/
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Open(spi_flash_ctrl_t * p_ctrl, spi_flash_cfg_t const * const p_cfg);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Close(spi_flash_ctrl_t * p_ctrl);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectWrite(spi_flash_ctrl_t    * p_ctrl,
                                                     uint8_t const * const p_src,
                                                     uint32_t const        bytes,
                                                     bool const            read_after_write);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectRead(spi_flash_ctrl_t * p_ctrl,
                                                   uint8_t * const    p_dest,
                                                   uint32_t const     bytes);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_SpiProtocolSet(spi_flash_ctrl_t * p_ctrl, spi_flash_protocol_t spi_protocol);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_XipEnter(spi_flash_ctrl_t * p_ctrl);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_XipExit(spi_flash_ctrl_t * p_ctrl);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Write(spi_flash_ctrl_t    * p_ctrl,
                                               uint8_t const * const p_src,
                                               uint8_t * const       p_dest,
                                               uint32_t              byte_count);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_Erase(spi_flash_ctrl_t * p_ctrl,
                                               uint8_t * const    p_device_address,
                                               uint32_t           byte_count);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_StatusGet(spi_flash_ctrl_t * p_ctrl, spi_flash_status_t * const p_status);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_BankSet(spi_flash_ctrl_t * p_ctrl, uint32_t bank);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_DirectTransfer(spi_flash_ctrl_t                  * p_ctrl,
                                                        spi_flash_direct_transfer_t * const p_transfer,
                                                        spi_flash_direct_transfer_dir_t     direction);
BSP_PLACE_CODE_IN_RAM fsp_err_t R_OSPI_W_AutoCalibrate(spi_flash_ctrl_t * p_ctrl);

/*******************************************************************************************************************//**
 * @} (end defgroup OSPI_W)
 **********************************************************************************************************************/

/* Common macro for FSP header files. There is also a corresponding FSP_HEADER macro at the top of this file. */
FSP_FOOTER

#endif                                 /* R_OSPI_W_H */
