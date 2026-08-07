#ifndef __SDK_DEFS_H__
 #define __SDK_DEFS_H__

 #include <stddef.h>

 #ifdef __cplusplus
extern "C" {
 #endif

 #if defined(CMAC_CPU)
  #include "renesas_cmac.h"
 #elif defined(CORTEX_M33)
  #include "bsp_cmac.h"
 #endif

/* TODO: Stub Segger SystemView Macros since it's not yet available */
 #define SEGGER_SYSVIEW_LW_EnterISR(a)
 #define SEGGER_SYSVIEW_LW_ExitISR()
 #define LW_SYSVIEW_SYS2CMAC_IRQ    0xDEADC0DE

 #if defined(CORTEX_M33)
  #define ASSERT_ERROR(a)    BSP_CHECK_FATAL(a)
 #endif

/**
 * \brief Access register field mask.
 *
 * Returns a register field mask (aimed to be used with local variables).
 * e.g.
 * \code
 * uint16_t tmp;
 *
 * tmp = CRG_TOP->SYS_STAT_REG;
 *
 * if (tmp & REG_MSK(CRG_TOP, SYS_STAT_REG, XTAL16_TRIM_READY)) {
 * ...
 * \endcode
 */
 #define REG_MSK(base, reg, field) \
    (base ## _ ## reg ## _ ## field ## _Msk)

/**
 * \brief Definition of alignment macros for inter-processor structure passing
 *
 */
 #define DAF_MIN(A, B)          (A) < (B) ? (A) : (B)
 #define ROUND_UP_TO_POW2(A)    (A) < 2 ? 1 : (A) < 3 ? 2 : (A) < 5 ? 4 : 8
 #define DAF_MAX_ALIGN    8
 #define DAF_STRUCT       struct __attribute__((packed, aligned(DAF_MAX_ALIGN)))
 #define DAF_UNION        union __attribute__((packed, aligned(DAF_MAX_ALIGN)))

 #define DAF_FIELD(TYPE,                                                                            \
                   NAME)        TYPE __attribute__((aligned(ROUND_UP_TO_POW2(DAF_MIN(DAF_MAX_ALIGN, \
                                                                                     sizeof(TYPE)))))) NAME
 #define DAF_FIELD_VOID(TYPE,                                                                          \
                        NAME)      TYPE __attribute__((aligned(ROUND_UP_TO_POW2(DAF_MIN(DAF_MAX_ALIGN, \
                                                                                        sizeof(char)))))) NAME

 #define DAF_ENUM32(TYPE, NAME)    union { TYPE NAME; int32_t NAME ## _int32; }

 #ifdef __cplusplus
}
 #endif

#endif                                 /* __SDK_DEFS_H__ */
