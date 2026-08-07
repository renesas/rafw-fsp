/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 ***********************************************************************************************************************/
#include "rm_ble_abs_gtl_storage.h"
#ifdef CRYPTO_ENABLED
 #include "rm_ble_abs_gtl_sw_aes_api.h"
#endif

#ifdef ENABLE_STORAGE
#include "rm_map_persistant_w.h"
#include "common_data.h"
#include "common_utils.h"
#include "stdio.h"

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/

/* Capture the vee_flash status*/
rm_vee_status_t p_status = {(rm_vee_state_t) ZERO_VALUE, ZERO_VALUE, ZERO_VALUE, ZERO_VALUE};


#ifdef CRYPTO_ENABLED
extern crypto_env_t rm_ble_abs_gtl_aes_env;
uint8_t             usr_key[16] =
{
    0x3d, 0xaf, 0xba, 0x42, 0x9d, 0x9e, 0xb4, 0x30, 0xb4, 0x22, 0xda, 0x80, 0x2c, 0x9f, 0xac, 0x41
};
 #endif

/*******************************************************************************************************************//**
 *  Wrappper for memory allocate function.
 *
 * @param[in]  size     Size of memory to be allocated (in bytes)
 * @retval NULL         Memory not allocated
 * @retval Address      Address of memory allocated
 **********************************************************************************************************************/
static void * rm_ble_gtl_malloc (uint32_t size)
{
 #if (BSP_CFG_RTOS == 2)               /* FreeRTOS */
    return pvPortMalloc(size);
 #elif (BSP_CFG_RTOS == 0)             /* Baremetal */
    void * p_mem = NULL;
    p_mem = malloc(size);
    memset(p_mem, 0, size);

    return p_mem;
 #else
    return NULL;                       // ThreadX is not supported, so this is a placeholder
 #endif
}

/*******************************************************************************************************************//**
 *  Wrapper for free memory allocation function.
 *
 * @param[in]  p_mem    Pointer to memory to be freed
 **********************************************************************************************************************/
static void rm_ble_gtl_free (void * p_mem)
{
 #if (BSP_CFG_RTOS == 2)               /* FreeRTOS */
    vPortFree(p_mem);
 #elif (BSP_CFG_RTOS == 0)             /* Baremetal */
    free(p_mem);
 #else
    return NULL;                       // ThreadX is not supported, so this is a placeholder
 #endif
}
  
/**********************************************************************************************************************
 *  Executed at start of the application.
 *  This function initializes the VEE driver and formats the flash if necessary.
 * @param[in]  none
 * @param[out] num_of_entries  Number of entries in the flash.
 **********************************************************************************************************************/
 fsp_err_t rm_ble_abs_gtl_storage_init (uint8_t * num_of_entries)
{
    fsp_err_t                err = FSP_SUCCESS;
    uint8_t                  dummy_boot_data[sizeof(rm_ble_abs_gtl_strg_boot)] = {0};
    uint8_t                * p_ref_data = NULL;
    rm_ble_abs_gtl_strg_boot boot_data;

    err = RM_MAP_PERSISTANT_W_Open(RM_MAP_PERSISTANT_W_get_ctrl());

    if ((FSP_SUCCESS == err) || (FSP_ERR_ALREADY_OPEN == err))
    {

        // Read the boot sector data from flash
        err = RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, (uint8_t **) &p_ref_data);
        if ((FSP_SUCCESS != err) && (FSP_ERR_NOT_FOUND != err))
        {
            return err;
        }

        memcpy(&boot_data, p_ref_data, sizeof(rm_ble_abs_gtl_strg_boot));

 #ifdef CRYPTO_ENABLED
        rm_ble_abs_gtl_aes_init(NULL, NULL, NULL); // all null use default
        rm_ble_abs_gtl_aes_env.keyset_func(usr_key, NULL, AES_MODE_128_);
 #endif

        // Check if the magic number is valid
        if ((boot_data.sec_data_mng_info.magic_num != BLE_ABS_SECURE_DATA_MAGIC_NUMBER) || (err != FSP_SUCCESS))
        {

            // The flash has Not been formatted yet or the magic number is not valid.
            err = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(),  BLE_SEC_AREA);
            if (FSP_SUCCESS != err)
            {
                return err;
            }

            // Init the reference sector data
            memset(dummy_boot_data, ZERO_VALUE, sizeof(rm_ble_abs_gtl_strg_boot));
            err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, dummy_boot_data);

            if (FSP_SUCCESS != err)
            {
                return err;
            }

            *num_of_entries = 0;       // Set the number of entries to 0 since the flash has been formatted
        }
        else
        {
            *num_of_entries = boot_data.sec_data_mng_info.bond_cnt;
        }

    }

    return err;
}
 
/**********************************************************************************************************************
 * Read bonding information from flash.
 * @param[in]  num_of_entries   Number of entries to read (retrieved from the security data management information).
 * @param[out] addr             Address to store the read data.
 * @retval     fsp_err_t        Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_read_bond_data (uint8_t num_of_entries, uint8_t ** addr, uint16_t * data_size)
{
    fsp_err_t err              = FSP_SUCCESS;
    void    * p_record_data    = NULL;
    uint8_t * p_ref_data       = rm_ble_gtl_malloc(BOND_DATA_TL_SIZE);
    uint8_t * p_ref_data_start = NULL;
    uint32_t  out_len          = BOND_DATA_SINGLE;

    *data_size = 0;
 #ifdef CRYPTO_ENABLED
    uint8_t decrypted[BOND_DATA_SINGLE] = {0}; // Buffer to hold decrypted data
 #endif
    if (NULL == p_ref_data)
    {
        return FSP_ERR_OUT_OF_MEMORY;          // Memory allocation failed
    }

    p_ref_data_start = p_ref_data;             // Get starting address

    // Check if the number of entries exceeds the maximum allowed
    if (num_of_entries > BLE_ABS_CFG_NUMBER_BONDING)
    {
        rm_ble_gtl_free(p_ref_data);
        p_ref_data_start = NULL;

        return FSP_ERR_INVALID_ARGUMENT;
    }

    for (uint32_t rec_id = 1; rec_id <= num_of_entries; rec_id++)
    {
        err = RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, rec_id, (uint8_t **) &p_record_data);
 
        if (FSP_SUCCESS == err)
        {
 #ifdef CRYPTO_ENABLED

            // The bond data len cannot exceed 254 bytes.
            err = rm_ble_abs_gtl_aes_env.decrypt_func(decrypted,
                                                      sizeof(decrypted),
                                                      (uint8_t *) p_record_data,
                                                      out_len,
                                                      usr_key);

            memcpy(p_ref_data, decrypted, out_len); //IV is discarded by the copy
 #else
            memcpy(p_ref_data, p_record_data, out_len);
 #endif
            p_ref_data += out_len;
            *data_size += (uint16_t) out_len; // Update the total length of data read
        }
        else
        {
            rm_ble_gtl_free(p_ref_data);
            p_ref_data_start = NULL;

            return err;
        }
    }

    *addr = p_ref_data_start;          // Update address to point to the start of the data

    return err;
}

/***********************************************************************************************************************
 * This function releases the buffer allocated for bonding information.
 * @param[in]  p_sec_data   Pointer to the security data buffer to be released.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_rel_buf (uint8_t * p_sec_data)
{
    fsp_err_t err = FSP_SUCCESS;
    rm_ble_gtl_free(p_sec_data);

    return err;
}

/***********************************************************************************************************************
 * This function writes the bonding information to a Virtual EEPROM Record.
 * @param[in]  idx         Index of the record to write.
 * @param[in]  data_addr   Pointer to the data to write.
 * @param[in]  data_size   Size of the data to write.
 * @retval     fsp_err_t   Status of the operation.
 * @note        This function waits for the Virtual EEPROM callback to indicate that it has finished writing data
 * and vee flash is in ready state. If the callback event does not occur within a certain time, it returns a timeout error.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_write_bond_data (uint8_t idx, uint8_t * data_addr, uint16_t data_size)
{
    fsp_err_t err = FSP_SUCCESS;
    if (data_size != BOND_DATA_SINGLE)
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

#ifdef CRYPTO_ENABLED
    uint8_t * plain_data      = rm_ble_gtl_malloc(data_size);
    uint8_t * cipher_datatext = rm_ble_gtl_malloc(data_size + AES_BLOCK_SIZE);

    memset(plain_data, ZERO_VALUE, data_size);
    memset(cipher_datatext, ZERO_VALUE, data_size);

    memcpy(plain_data, data_addr, data_size);
    err = rm_ble_abs_gtl_aes_env.encrypt_func(cipher_datatext,
                                              data_size + AES_BLOCK_SIZE,
                                              plain_data,
                                              data_size,
                                              usr_key,
                                              NULL); // NULL = use default IV

    err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, idx, cipher_datatext); //Size for IV is already preallocated in VEE

#else

    /* Write the data to a Virtual EEPROM Record. */
    err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, idx, data_addr); //Size for IV is already preallocated in VEE
#endif
    if (FSP_SUCCESS != err)
    {
#ifdef CRYPTO_ENABLED
        rm_ble_gtl_free(plain_data);
        rm_ble_gtl_free(cipher_datatext);
#endif

        return err;
    }
#ifdef CRYPTO_ENABLED
    // Block until the write operation is complete, else the buffers may be freed too early.
    rm_ble_gtl_free(plain_data);
    rm_ble_gtl_free(cipher_datatext);
#endif
    return err;
}

/***********************************************************************************************************************
 * Update the number of bonds in the reference data.
 * This function updates the bond count in the reference data and writes it back to flash.
 * @param[in]  act_num_bond   The actual number of bonds to update.
 * @retval     fsp_err_t      Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_update_bond_num (uint8_t act_num_bond)
{
    fsp_err_t                err            = FSP_SUCCESS;
    rm_ble_abs_gtl_strg_boot boot_data      = {0};
    uint8_t   ref_data_write[REF_DATA_SIZE] = {0};
    uint8_t * p_ref_data_read               = NULL;

    /* Get the reference data pointer */
    err = RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, (uint8_t **) &p_ref_data_read);

    /* Read the existing boot data */
    memcpy(&boot_data, p_ref_data_read, sizeof(rm_ble_abs_gtl_strg_boot));

    /* Update the bond count */
    boot_data.sec_data_mng_info.bond_cnt = act_num_bond;

    if ((boot_data.sec_data_mng_info.magic_num != BLE_ABS_SECURE_DATA_MAGIC_NUMBER) && (act_num_bond == 1))
    {
        /* If the magic number is not valid, set it to the correct value */
        boot_data.sec_data_mng_info.magic_num = BLE_ABS_SECURE_DATA_MAGIC_NUMBER;
    }
    else
    {
        /* If the magic number is valid, do nothing */
    }

    /* Write the updated boot data back to flash */
    memcpy(&ref_data_write, &boot_data, REF_DATA_SIZE);

    err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, ref_data_write);

    return err;
}
 
/***********************************************************************************************************************
 *  @brief        This function removes all bond data from the flash memory.
 *  @retval       fsp_err_t   Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_rem_all_bond_data (void)
 {
    fsp_err_t err                           = FSP_SUCCESS;
    rm_ble_abs_gtl_strg_boot boot_data      = { 0 };
    uint8_t ref_data_write[REF_DATA_SIZE]   = { 0 };
    uint8_t *p_ref_data_read                = NULL;

    err = RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, (uint8_t**) &p_ref_data_read);

    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        return err;
    }

    /* Read the existing boot data */
    memcpy(&boot_data, p_ref_data_read, sizeof(rm_ble_abs_gtl_strg_boot));

    /* Update the bond count */
    boot_data.sec_data_mng_info.bond_cnt = ZERO_VALUE;

    /* Format the flash */
    err = RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA);

    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        return err;
    }

    /* Write the updated boot data back to flash */
    memcpy(&ref_data_write, &boot_data, REF_DATA_SIZE);

    err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, ref_data_write);

    if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
    {
        return err;
    }

    return err;
}

/***********************************************************************************************************************
 *  @brief        This function removes specific bond data from the flash memory.
 *  @param[in]    idx         Index of the bond data to remove.
 *  @retval       fsp_err_t   Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_rem_specific_bond_data (uint8_t idx)
{
    // Copy new DB from RAM
    // Update the bond count in the reference data
    FSP_PARAMETER_NOT_USED(idx);

    return FSP_SUCCESS;
}

/***********************************************************************************************************************
 *  @brief        This function retrieves the Bluetooth device address from the flash memory.
 *  @param[out]   bd_address  Pointer to store the Bluetooth device address.
 *  @retval       fsp_err_t   Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_get_addr (st_ble_dev_addr_t * bd_address)
{
    fsp_err_t                err             = FSP_SUCCESS;
    rm_ble_abs_gtl_strg_boot boot_data       = {0};
    uint8_t                * p_ref_data_read = NULL;


    RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, (uint8_t **) &p_ref_data_read);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* Read the existing boot data */
    memcpy(&boot_data, p_ref_data_read, sizeof(rm_ble_abs_gtl_strg_boot));
    if (boot_data.sec_data_mng_info.magic_num != BLE_ABS_SECURE_DATA_MAGIC_NUMBER)
    {
        err = FSP_ERR_NOT_FOUND;
        memset(bd_address, 0x00, BLE_BD_ADDR_LEN); // Clear the address
        return err;
    }

    /* Copy the address */
    memcpy(bd_address->addr, &boot_data.loc_dev_sec_data.loc_ident_addr.addr, BLE_BD_ADDR_LEN);
    bd_address->type = boot_data.loc_dev_sec_data.loc_ident_addr.type;

    return err;
}

/***********************************************************************************************************************
 * @brief        This function sets the Bluetooth device address in the flash memory.
 * @param[in]    bd_address  Pointer to the Bluetooth device address to set.
 * @retval       fsp_err_t   Status of the operation.
 **********************************************************************************************************************/

fsp_err_t rm_ble_abs_gtl_storage_set_addr (st_ble_dev_addr_t * bd_address)
{
    fsp_err_t                err            = FSP_SUCCESS;
    rm_ble_abs_gtl_strg_boot boot_data      = {0};
    uint8_t   ref_data_write[REF_DATA_SIZE] = {0};
    uint8_t * p_ref_data_read               = NULL;

    /* Get the reference data pointer */
    RM_MAP_PERSISTANT_W_Read_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, (uint8_t **) &p_ref_data_read);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    /* Read the existing boot data */
    memcpy(&boot_data, p_ref_data_read, sizeof(rm_ble_abs_gtl_strg_boot));
    if (boot_data.sec_data_mng_info.magic_num != BLE_ABS_SECURE_DATA_MAGIC_NUMBER)
    {
        /* If the magic number is not valid, set it to the correct value */
        boot_data.sec_data_mng_info.magic_num          = BLE_ABS_SECURE_DATA_MAGIC_NUMBER; // Set the magic number to indicate valid data
        boot_data.sec_data_mng_info.bond_cnt           = 0;                                // Reset the bond count
        boot_data.loc_dev_sec_data.loc_ident_addr.type = BLE_GAP_ADDR_PUBLIC;              // Set default address type
        memset(boot_data.loc_dev_sec_data.loc_ident_addr.addr, 0x00, BLE_BD_ADDR_LEN);     // Clear the address
    }

    /* Update the Bluetooth device address */
    memcpy(&boot_data.loc_dev_sec_data.loc_ident_addr.addr, bd_address->addr, BLE_BD_ADDR_LEN);
    boot_data.loc_dev_sec_data.loc_ident_addr.type = bd_address->type;

    /* Write the updated boot data back to flash */
    memcpy(&ref_data_write, &boot_data, REF_DATA_SIZE);

    err = RM_MAP_PERSISTANT_W_Write_BIN(RM_MAP_PERSISTANT_W_get_ctrl(), BLE_SEC_AREA, 0, ref_data_write);

    return err;
}
 
/***********************************************************************************************************************
 * @brief        This function updates the crypto key value
 * @param[in]    key        Pointer to the new crypto key.
 * @param[in]    key_size   Size of the crypto key.
 * @retval       fsp_err_t  Status of the operation.
 **********************************************************************************************************************/
fsp_err_t rm_ble_abs_gtl_storage_upd_crypto_key (uint8_t * key, uint8_t key_size)
{
    fsp_err_t err = FSP_SUCCESS;

 #ifdef CRYPTO_ENABLED
    memcpy(usr_key, key, key_size);
 #else
    FSP_PARAMETER_NOT_USED(key);
    FSP_PARAMETER_NOT_USED(key_size);
 #endif

    return err;
}

#endif                                 // ENABLE_STORAGE
