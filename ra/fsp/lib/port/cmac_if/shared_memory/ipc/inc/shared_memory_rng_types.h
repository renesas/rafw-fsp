/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/**
 * \addtogroup SHARED_MEM
 \{
 * \addtogroup SHARED_MEM_IPC
 \{
 */

/**
 **************************************************************************************
 **
 *
 * \file shared_memory_rng_types.h
 *
 * \brief This file holds the data type definitions for the Random Number Generator (RNG)
 *        component and specifically the data type definitions for the variables placed
 *        in shared memory and used by the RNG.
 *
 **************************************************************************************
 **
 */

#ifndef SHARED_MEMORY_RNG_TYPES_H_
 #define SHARED_MEMORY_RNG_TYPES_H_

 #ifdef __cplusplus
extern "C"
{
 #endif

/****************************************************************************
 *                               Include files
 ****************************************************************************/

/// \cond
 #include <stdint.h>
 #include "sdk_defs.h"

/// \endcond

/****************************************************************************
 *                              Macro definitions
 ****************************************************************************/

/** \addtogroup RNG_CONFIG RNG data configuration
 *  \brief Generator config
 *  @{
 */

/**
 * The size in bytes of the random number generator pool.\n
 * This value should be multiple of 4
 */
 #ifndef RNG_POOL_SIZE
  #define RNG_POOL_SIZE    (256)
 #endif

/** @}*/

/****************************************************************************
 *                     Enumerations/Type definitions/Structs
 ****************************************************************************/

/** \addtogroup RNG_DATA_TYPES RNG data description
 *  \brief Generator data type structures
 *  @{
 */

/*! \brief Return code base values per component */
typedef enum
{
    IID_RETURN_BASE         = 0x01u,   /*!< \brief    Generic return codes base.
                                        *   \details  Value used internally as the base value for all generic return codes */
    IID_IRNG_BASE           = 0xA0u,   /*!< \brief    iRNG return codes base.
                                        *   \details  Value used internally as the base value for all iRNG return codes */
    IID_DRBG_BASE           = 0xB0u,   /*!< \brief    DRBG return codes base.
                                        *   \details  Value used internally as the base value for all DRBG return codes */
    IID_ENTROPYSOURCE_BASE  = 0xC0u,   /*!< \brief    Entropy Source return codes base.
                                        *   \details  Value used internally as the base value for all Entropy Source return codes */
    IID_RESETPROOFNESS_BASE = 0xD0u,   /*!< \brief    Reset Proofness return codes base.
                                        *   \details  Value used internally as the base value for all Reset Proofness return codes */
    IID_AES_BASE            = 0xE0u,   /*!< \brief    AES return codes base.
                                        *   \details  Value used internally as the base value for all AES return codes */
} iid_return_base_t;

/*! \brief Return code values */
typedef enum
{
    /************************** Generic Return Codes **************************/

    /*! \brief     Successful execution.
     *  \details   Value indicating the successful execution of the called function. */
    IID_SUCCESS = 0x00u,

    /*! \brief     Successful execution.
     *  \details   Value indicating that the given function call is not allowed in the current state of the IRNG module. */
    IID_NOT_ALLOWED = ((uint32_t)IID_RETURN_BASE + 0x00u),

    /*! \brief Invalid function parameters
     *  \details Value indicating that at least one of the parameters passed as argument to the function call has an invalid form
     *           and/or content. This also occurs when one of the required output buffers contains a NULL pointer, or when one of the
     *           provided length parameters is not long enough. */
    IID_INVALID_PARAMETERS = ((uint32_t)IID_RETURN_BASE + 0x01u),

    /************************** iRNG Specific Return Codes **************************/

    /*! \brief Hard reset required
     *  \details Value indicating that a full power down / power up cycle of the hardware is required. */
    IID_ERROR_IRNG_HARD_RESET_REQUIRED = ((uint32_t)IID_IRNG_BASE + 0x01u),

    /************************** DRBG Specific Return Codes **************************/

    /*! \brief DRBG reseed required
     *  \details Value indicating that the DRBG requires a reseeding. */
    IID_ERROR_DRBG_RESEED_REQUIRED = ((uint32_t)IID_DRBG_BASE + 0x01u),

    /*! \brief DRBG self test failed
     *  \details Value indicating that the DRBG self test returned an error. */
    IID_ERROR_DRBG_SELF_TEST_FAILED = ((uint32_t)IID_DRBG_BASE + 0x02u),

    /*! \brief DRBG incorrect CRC-16 checksum
     *  \details Value indicating that the CRC-16 checksum in the DRBG context
     *           does not correspond to the content of the DRBG context. */
    IID_ERROR_DRBG_CONTEXT_INCORRECT_CRC = ((uint32_t)IID_DRBG_BASE + 0x03u),

    /************************** Entropy Source Specific Return Codes ****************/

    /*! \brief Entropy Source RCT health test failed
     *  \details Value indicating that the provided entropy source did not pass the repetition count health test. */
    IID_ERROR_ES_RCT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x01u),

    /*! \brief Entropy Source APT health test failed
     *  \details Value indicating that the provided entropy source did not pass the adaptive proportion health test. */
    IID_ERROR_ES_APT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x02u),

    /*! \brief Entropy Source PUF_RCT health test failed
     *  \details Value indicating that the provided entropy source did not pass the PUF Repetition Count Test (PUF_RCT). */
    IID_ERROR_ES_PUF_RCT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x03u),

    /*! \brief Entropy Source PUF_NRCT health test failed
     *  \details Value indicating that the provided entropy source did not pass the PUF Nibble Repetition Count Test (PUF_NRCT). */
    IID_ERROR_ES_PUF_NRCT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x04u),

    /*! \brief Entropy Source PUF_BRCT health test failed
     *  \details Value indicating that the provided entropy source did not pass the PUF Byte Repetition Count Test (PUF_BRCT). */
    IID_ERROR_ES_PUF_BRCT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x05u),

    /*! \brief Entropy Source PUF_WRCT health test failed
     * \details Value indicating that the provided entropy source did not pass the PUF Word Repetition Count Test (PUF_WRCT). */
    IID_ERROR_ES_PUF_WRCT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x06u),

    /*! \brief Entropy Source PUF_NPT health test failed
     *  \details Value indicating that the provided entropy source did not pass the PUF Nibble Proportion Test (PUF_NPT). */
    IID_ERROR_ES_PUF_NPT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x07u),

    /*! \brief Entropy Source PUF_BAPT health test failed
     *  \details Value indicating that the provided entropy source did not pass the PUF Byte Adaptive Proportion Test (PUF_BAPT). */
    IID_ERROR_ES_PUF_BAPT_HEALTH_TEST_FAILED = ((uint32_t)IID_ENTROPYSOURCE_BASE + 0x08u),

    /************************** Others *********************************************/

    /*! \brief Unused value, placed here to force the compiler to use 32 bits to represent values of this type */
    IID_RETURN_ENUM_32BITS = 0x0FFFFFFFU,
} iid_return_t;

/**
 * \brief This union describes the RNG status
 */
typedef DAF_UNION
{
    /**
     * \brief Status bits
     */
    DAF_STRUCT status_bits
    {
        uint32_t                     : 5; /*!< Reserved                                 */
        uint32_t normal_refiling     : 1; /*!< The pool is under normal refilling       */
        uint32_t above_threshold     : 1; /*!< The pool is above threshold              */
        uint32_t warmstart_refilling : 1; /*!< The pool is under refilling (warmstart)  */
        uint32_t warmstart_refilled  : 1; /*!< The pool is full (warmstart)             */
        uint32_t warmstart_test_ok   : 1; /*!< The warmstart test is OK                 */
        uint32_t warmstart_done      : 1; /*!< Warmstart completed                      */
        uint32_t coldstart_refilling : 1; /*!< The pool is under refilling (coldstart)  */
        uint32_t coldstart_refilled  : 1; /*!< The pool is full (coldtart)              */
        uint32_t coldstart_test_ok   : 1; /*!< The coldstart test is OK                 */
        uint32_t coldstart_done      : 1; /*!< Coldstart completed                      */
        uint32_t pool_usable         : 1; /*!< Indicates if the pool is usable or not   */
    } bits;                               /*!< Stucture for the status bits             */

    /**
     * \brief Status raw value
     */
    DAF_FIELD(uint32_t, value);
} rng_status_t;

/**
 * \brief The structure holds the RNG version fields
 */
typedef struct rng_product_info
{
    uint8_t product_id;                /*!< The product ID                             */
    uint8_t major;                     /*!< The major version                          */
    uint8_t minor;                     /*!< The minor version                          */
    uint8_t patch;                     /*!< The patch number                           */
    uint8_t build_number;              /*!< Build number                               */
} rng_product_info_t;

/**
 * \brief This enumeration is used to describe the state of the RNG manager
 */
typedef enum rng_state
{
    RNG_STATE_UNDEFINED,               /*!< State undefined. Set at initialization     */
    RNG_STATE_COLDSTART_HOLDOFF,       /*!< The state for the coldstart holdoff time   */
    RNG_STATE_COLDSTART_FILL,          /*!< Coldstart filling state                    */
    RNG_STATE_TOPUP_THRESHOLD,         /*!< The state is in threshold check            */
    RNG_STATE_TOPUP,                   /*!< State for filling the pool                 */
    RNG_STATE_ENUM_32BITS = 0x0FFFFFFFU,
} rng_state_e;

/**
 * \brief This structure contains the context (internal) of the RNG component
 */
typedef DAF_STRUCT rng_da1487x
{
    DAF_FIELD(unsigned short, pool_size);      /*!< Size of the random number generator pool (in bytes) */
    DAF_FIELD(unsigned short, pool_head);      /*!< Next location to add a random byte         */
    DAF_FIELD(unsigned short, pool_tail);      /*!< Next location to pull a random byte        */
    DAF_FIELD(volatile rng_status_t, status);  /*!< Current status of the RNG and pool         */
    DAF_FIELD(unsigned short, pool_available); /*!< The number of random bytes in the pool     */
} rng_da1487x_t;

/**
 * \brief The RNG data component
 */
typedef DAF_STRUCT RNG
{
    DAF_FIELD(uint8_t, pool[RNG_POOL_SIZE]);   /*!< The pool of the RNG holding the generated random numbers */
    DAF_FIELD(rng_da1487x_t, context);         /*!< The RNG context                                          */
    DAF_ENUM32(rng_state_e, state);            /*!< RNG state                                                */
    DAF_ENUM32(iid_return_t, internal_status); /*!< Internal error form the underlying components            */
    DAF_FIELD(volatile uint32_t, lock);        /*!< Used as a lock mechanism                                 */
} rng_t;

/** @}*/

 #ifdef __cplusplus
}
 #endif

#endif                                 /* SHARED_MEMORY_RNG_TYPES_H_ */

/**
 * \}
 * \}
 */
