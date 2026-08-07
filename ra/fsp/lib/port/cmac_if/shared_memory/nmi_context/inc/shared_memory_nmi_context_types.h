/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef SHARED_MEMORY_NMI_CONTEXT_TYPES_H
#define SHARED_MEMORY_NMI_CONTEXT_TYPES_H

/* Notes on CMAC exception handling:
 *
 * The NMI handler will be triggered whenever any of the following lines is asserted:
 * - CM_EXC_STAT_REG->EXC_CPU_ERROR (Hard Fault)
 * - CM_EXC_STAT_REG->EXC_HWAC_ERROR (HW Assertion Checker)
 * - CM_EXC_STAT_REG->EXC_FW_ERROR (Set by software to force NMI)
 *
 * NMI will be also triggered after WDG timeout.
 *
 * Any CPU error that would trigger HardFault asserts CM_EXC_STAT_REG->EXC_CPU_ERROR.
 * When a HardFault occurs an initial stacking is performed.
 * At next CPU cycle CM_EXC_STAT_REG->EXC_CPU_ERROR will be found asserted and an
 * NMI will be triggered. No hard fault code will ever execute.
 *
 * ----------------------------------------------------------------------------
 * EXC_RETURN and Stack Frame location
 * ----------------------------------------------------------------------------
 * When an exception handler is entered, the LR register is set to a special
 * value called EXC_RETURN by the hardware. This value indicates which stack
 * (MSP or PSP) was used to save the exception frame of the interrupted context.
 *
 * EXC_RETURN values (Cortex-M0+):
 *   0xFFFFFFF1 : Return to Handler mode,  exception frame saved on MSP
 *                (nested exception: NMI interrupted another exception handler)
 *   0xFFFFFFF9 : Return to Thread mode,   exception frame saved on MSP
 *                (NMI interrupted a thread using MSP)
 *   0xFFFFFFFD : Return to Thread mode,   exception frame saved on PSP
 *                (NMI interrupted a thread using PSP)
 *
 * This implementation reads LR (EXC_RETURN) saved on the stack by the naked
 * handler, and selects MSP or PSP accordingly to retrieve the correct
 * exception frame (NMI frame) of the interrupted context.
 *
 * ----------------------------------------------------------------------------
 * Stack frame layout when NMI_HandlerC() is called
 * ----------------------------------------------------------------------------
 * The naked handler pushes registers onto MSP in the following order:
 *
 *   MSP --> context[0]  = LR (EXC_RETURN) } pushed by naked handler
 *           context[1]  = R8              } pushed by naked handler
 *           context[2]  = R9              } (Cortex-M0+ cannot PUSH r8-r11
 *           context[3]  = R10             }  directly, so r4-r7 are used
 *           context[4]  = R11             }  as intermediaries)
 *           context[5]  = R4              } pushed by naked handler
 *           context[6]  = R5              }
 *           context[7]  = R6              }
 *           context[8]  = R7              }
 *
 * The NMI exception frame (R0-R3, R12, LR, PC, xPSR) is saved by HW.
 * Its location depends on EXC_RETURN (context[0]):
 *
 *   EXC_RETURN = 0xFFFFFFFD (PSP used):
 *     NMI exception frame is on PSP:
 *       psp[0] = R0, psp[1] = R1, psp[2] = R2,  psp[3] = R3
 *       psp[4] = R12, psp[5] = LR, psp[6] = PC, psp[7] = xPSR
 *
 *   EXC_RETURN = 0xFFFFFFF9 or 0xFFFFFFF1 (MSP used):
 *     NMI exception frame is on MSP, just below the manually pushed registers:
 *       context[9]  = R0
 *       context[10] = R1
 *       context[11] = R2
 *       context[12] = R3
 *       context[13] = R12
 *       context[14] = LR
 *       context[15] = PC  <-- address where NMI occurred
 *       context[16] = xPSR
 *
 * ----------------------------------------------------------------------------
 * HardFault exception frame (present only when EXC_CPU_ERROR is asserted)
 * ----------------------------------------------------------------------------
 * If NMI was caused by HardFault (CM_EXC_STAT_REG->EXC_CPU_ERROR is set),
 * the HardFault exception frame was saved by HW before the NMI frame.
 * Its location depends on the LR saved in the NMI exception frame
 * (nmi_stacked_lr), which reflects the stack used at the time of HardFault:
 *
 *   nmi_stacked_lr = 0xFFFFFFF9 (HardFault frame on MSP):
 *     HardFault exception frame is on MSP, below the NMI frame:
 *       context[17] = R0
 *       context[18] = R1
 *       context[19] = R2
 *       context[20] = R3
 *       context[21] = R12
 *       context[22] = LR
 *       context[23] = PC  <-- address where HardFault occurred
 *       context[24] = xPSR
 *
 *   nmi_stacked_lr != 0xFFFFFFF9 (HardFault frame on PSP):
 *     HardFault exception frame is on PSP:
 *       psp[0] = R0
 *       psp[1] = R1
 *       psp[2] = R2
 *       psp[3] = R3
 *       psp[4] = R12
 *       psp[5] = LR
 *       psp[6] = PC  <-- address where HardFault occurred
 *       psp[7] = xPSR
 */

typedef struct g_shared_ram_nmi_context
{
    uint32_t magic;                    // 0x00
    uint32_t magic_0;                  // 0x04
    uint32_t magic_1;                  // 0x08

    // The SP (MSP) when NMI_HandlerC() was called.
    // Points to the first word pushed by the naked handler (EXC_RETURN).
    uintptr_t stack_ptr;               // 0x0C

    // {r8 - r11} at time of crash (context[1] - context[4])
    uint32_t stacked_r8;               // 0x10
    uint32_t stacked_r9;               // 0x14
    uint32_t stacked_r10;              // 0x18
    uint32_t stacked_r11;              // 0x1C

    // {r4 - r7} at time of crash (context[5] - context[8])
    uint32_t stacked_r4;               // 0x20
    uint32_t stacked_r5;               // 0x24
    uint32_t stacked_r6;               // 0x28
    uint32_t stacked_r7;               // 0x2C

    // NMI exception frame (R0-R3, R12, LR, PC, xPSR)
    uint32_t nmi_stacked_r0;           // 0x30
    uint32_t nmi_stacked_r1;           // 0x34
    uint32_t nmi_stacked_r2;           // 0x38
    uint32_t nmi_stacked_r3;           // 0x3C
    uint32_t nmi_stacked_r12;          // 0x40
    uint32_t nmi_stacked_lr;           // 0x44
    uint32_t nmi_stacked_pc;           // 0x48
    uint32_t nmi_stacked_psr;          // 0x4C

    // HardFault exception frame (Not always present)
    uint32_t hf_stacked_r0;            // 0x50
    uint32_t hf_stacked_r1;            // 0x54
    uint32_t hf_stacked_r2;            // 0x58
    uint32_t hf_stacked_r3;            // 0x5C
    uint32_t hf_stacked_r12;           // 0x60
    uint32_t hf_stacked_lr;            // 0x64
    uint32_t hf_stacked_pc;            // 0x68
    uint32_t hf_stacked_psr;           // 0x6C

    uint32_t _DFSR;                    // 0x70
    uint32_t error_val;                // 0x74
    uint32_t exc_val;                  // 0x78
    uint32_t _BS_SMPL_ST;              // 0x7C
    uint32_t _BS_SMPL_D;               // 0x80

    // Assert information
    uint32_t assert_magic;             // 0x84
    uint32_t assert_lr;                // 0x88
} shared_ram_nmi_context_t;

#endif /* SHARED_MEMORY_NMI_CONTEXT_TYPES_H */
