/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *    Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *           http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <crypto/CHIPCryptoPAL.h>
#include <rnDeviceWrapAPIs.h>
#include <rnDeviceDataProvider.h>
#include <lib/support/Base64.h>
#include <setup_payload/Base38Encode.h>
#include <setup_payload/SetupPayload.h>
#include "platform/renesas/RnConfig.h"

uint32_t t_passcode = 20202021;
namespace chip {
namespace DeviceLayer {
namespace Rn {

// using namespace chip::Credentials;
using namespace chip::DeviceLayer::Internal;

CHIP_ERROR rnDeviceDataProvider::GetSetupDiscriminator(uint16_t & setupDiscriminator)
{
    CHIP_ERROR err;
    uint32_t setupDiscriminator32;

    err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_SetupDiscriminator, setupDiscriminator32);
#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
#if defined(ENABLE_CHIP_APP_EXT)
        setupDiscriminator32 = 0;
#else
        setupDiscriminator32 = CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR;
#endif // ENABLE_CHIP_APP_EXT
        //in case of first time, write it on NVRAM
        err = RnConfig::WriteConfigValue(RnConfig::kConfigKey_SetupDiscriminator, setupDiscriminator32);
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR

    VerifyOrReturnLogError(setupDiscriminator32 <= kMaxDiscriminatorValue, CHIP_ERROR_INVALID_ARGUMENT);
    setupDiscriminator = static_cast<uint16_t>(setupDiscriminator32);
    return CHIP_NO_ERROR;
}

CHIP_ERROR rnDeviceDataProvider::SetSetupDiscriminator(uint16_t setupDiscriminator)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_SetupDiscriminator,
                                                  static_cast<uint32_t>(setupDiscriminator));
}

CHIP_ERROR rnDeviceDataProvider::GetSpake2pIterationCount(uint32_t & iterationCount)
{
    CHIP_ERROR err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_Spake2pIterationCount, iterationCount);

#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT) && CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        iterationCount = CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT;
        err            = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        RnConfig::WriteConfigValue(RnConfig::kConfigKey_Spake2pIterationCount, iterationCount);
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT) && CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_ITERATION_COUNT
    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetSpake2pIterationCount(uint32_t iterationCount)
{
    CHIP_ERROR err = RnConfig::WriteConfigValue(RnConfig::kConfigKey_Spake2pIterationCount, iterationCount);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetSpake2pSalt(MutableByteSpan & saltBuf)
{
    static constexpr size_t kSpake2pSalt_MaxBase64Len = BASE64_ENCODED_LEN(chip::Crypto::kSpake2p_Max_PBKDF_Salt_Length) + 1;

    CHIP_ERROR err                          = CHIP_NO_ERROR;
    char saltB64[kSpake2pSalt_MaxBase64Len] = { 0 };
    size_t saltB64Len                       = 0;

    err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_Spake2pSalt, saltB64, sizeof(saltB64), saltB64Len);

#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT)
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        saltB64Len = strlen(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT);
        ReturnErrorCodeIf(saltB64Len > sizeof(saltB64), CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(saltB64, CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT, saltB64Len);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_Spake2pSalt, saltB64, saltB64Len);
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_SALT)

    ReturnErrorOnFailure(err);

    uint8_t saltByteArray[kSpake2pSalt_MaxBase64Len] = { 0 };
    size_t saltLen                                   = chip::Base64Decode32(saltB64, saltB64Len, saltByteArray);
    ReturnErrorCodeIf(saltLen > saltBuf.size(), CHIP_ERROR_BUFFER_TOO_SMALL);

    memcpy(saltBuf.data(), saltByteArray, saltLen);
    saltBuf.reduce_size(saltLen);

    return CHIP_NO_ERROR;
}

CHIP_ERROR rnDeviceDataProvider::SetSpake2pSalt(char * saltBuf, size_t saltLen)
{
    CHIP_ERROR err = RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_Spake2pSalt, saltBuf, saltLen);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetSpake2pVerifier(MutableByteSpan & verifierBuf, size_t & verifierLen)
{
    static constexpr size_t kSpake2pSerializedVerifier_MaxBase64Len =
        BASE64_ENCODED_LEN(chip::Crypto::kSpake2p_VerifierSerialized_Length) + 1;

    CHIP_ERROR err                                            = CHIP_NO_ERROR;
    char verifierB64[kSpake2pSerializedVerifier_MaxBase64Len] = { 0 };
    size_t verifierB64Len                                     = 0;

    err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_Spake2pVerifier, verifierB64, sizeof(verifierB64),
                                           verifierB64Len);

#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER)
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        verifierB64Len = strlen(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER);
        ReturnErrorCodeIf(verifierB64Len > sizeof(verifierB64), CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(verifierB64, CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER, verifierB64Len);
        err = CHIP_NO_ERROR;
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER)

    ReturnErrorOnFailure(err);

    verifierLen = chip::Base64Decode32(verifierB64, verifierB64Len, reinterpret_cast<uint8_t *>(verifierB64));
    ReturnErrorCodeIf(verifierLen > verifierBuf.size(), CHIP_ERROR_BUFFER_TOO_SMALL);

    memcpy(verifierBuf.data(), verifierB64, verifierLen);
    verifierBuf.reduce_size(verifierLen);

    return CHIP_NO_ERROR;
}

CHIP_ERROR rnDeviceDataProvider::SetSpake2pVerifier(char * verifierBuf, size_t verifierLen)
{
    CHIP_ERROR err = RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_Spake2pVerifier, verifierBuf, verifierLen);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetSetupPayload(MutableCharSpan & payloadBuf)
{
    CHIP_ERROR err                                      = CHIP_NO_ERROR;
    uint8_t payloadBitSet[kTotalPayloadDataSizeInBytes] = { 0 };
    size_t bitSetLen                                    = 0;

    err = RnConfig::ReadConfigValueBin(RnConfig::kConfigKey_SetupPayloadBitSet, payloadBitSet, kTotalPayloadDataSizeInBytes,
                                           bitSetLen);

#if defined(CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE) && CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
//matterwork[[::kOnNetwork::change LSB to MSB each 1byte
        static constexpr uint8_t kTestSetupPayloadBitset[] = { 0x88, 0xFF, 0x2F, 0x00, 0x84, 0x00, 0xE0, 0x4B, 0x84, 0x68, 0x02 };
        //10001000 11111111 00101111 00000000 10000100 00000000 11100000 01001011 10000100 01101000 00000010
        //00010001 11111111 11110100 00000000 00100001 00000000 00000111 11010010 00100001 00010110 01000000

        //000 1000 1111 1111 1111 1010 0000 0000 0001 00 0010 0000 0000 0000 1111 1010 0100 0100 0010 0010 1100 100 0000
        //0	  1	   F	F    F	  5	   0	0    8	  0  4	       0	0    F	  5	   2	2    4	  2	   3    1   0
        //0	0xFFF1 0x8005 0  4 0xF00 0x134.4225(20202021) 0

        //product id: 0x8000
        //static constexpr uint8_t kTestSetupPayloadBitset[] = { 0x88, 0xFF, 0x07, 0x00, 0x84, 0x00, 0xE0, 0x4B, 0x84, 0x68, 0x02 };
//]]matterwork
        bitSetLen                                          = sizeof(kTestSetupPayloadBitset);
        ReturnErrorCodeIf(bitSetLen > kTotalPayloadDataSizeInBytes, CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(payloadBitSet, kTestSetupPayloadBitset, bitSetLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueBin(RnConfig::kConfigKey_SetupPayloadBitSet, payloadBitSet, bitSetLen);
    }
#endif // defined(CHIP_DEVICE_CONFIG_USE_TEST_SPAKE2P_VERIFIER)

    ReturnErrorOnFailure(err);

    size_t prefixLen = strlen(kQRCodePrefix);

    if (payloadBuf.size() < prefixLen + 1)
    {
        err = CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    else
    {
        MutableCharSpan subSpan = payloadBuf.SubSpan(prefixLen, payloadBuf.size() - prefixLen);
        memcpy(payloadBuf.data(), kQRCodePrefix, prefixLen);
        err = base38Encode(MutableByteSpan(payloadBitSet), subSpan);
        // Reduce output span size to be the size of written data
        payloadBuf.reduce_size(subSpan.size() + prefixLen);
    }

    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetSetupPasscode(uint32_t & setupPasscode)
{
    setupPasscode = t_passcode;

    return CHIP_NO_ERROR;
}

CHIP_ERROR rnDeviceDataProvider::SetSetupPasscode(uint32_t setupPasscode)
{
    t_passcode = setupPasscode;
    return CHIP_NO_ERROR;
}

CHIP_ERROR rnDeviceDataProvider::GetVendorName(char * buf, size_t bufSize)
{
    size_t vendorNameLen = 0; // without counting null-terminator
    CHIP_ERROR err;
    err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_VendorName, buf, bufSize, vendorNameLen);
#ifdef CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        vendorNameLen = strlen(CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME);
        if (bufSize < vendorNameLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME, vendorNameLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_VendorName, buf, vendorNameLen);
    }
    return err;
#endif
}

CHIP_ERROR rnDeviceDataProvider::GetVendorId(uint16_t & vendorId)
{
    ChipError err       = CHIP_NO_ERROR;
    uint32_t vendorId32 = 0;

    err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_VendorId, vendorId32);

#if defined(CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID) && CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
#if defined(ENABLE_CHIP_APP_EXT)
        vendorId32 = 0;
#else
        vendorId32 = CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID;
#endif // ENABLE_CHIP_APP_EXT
        //in case of first time, write it on NVRAM
        err = RnConfig::WriteConfigValue(RnConfig::kConfigKey_VendorId, vendorId32);
    }
#endif // defined(CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID) && CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID

    ReturnErrorOnFailure(err);
    vendorId = static_cast<uint16_t>(vendorId32);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetVendorId(uint32_t vendorId)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_VendorId, vendorId);
}

CHIP_ERROR rnDeviceDataProvider::GetProductName(char * buf, size_t bufSize)
{
    size_t productNameLen = 0; // without counting null-terminator
#ifdef CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME
    CHIP_ERROR err;
    err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_ProductName, buf, bufSize, productNameLen);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        productNameLen = strlen(CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME);
        if (bufSize < productNameLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME, productNameLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_ProductName, buf, productNameLen);
    }
    return err;
#endif
//]]mattterwork

}

CHIP_ERROR rnDeviceDataProvider::GetProductId(uint16_t & productId)
{
    ChipError err        = CHIP_NO_ERROR;
    uint32_t productId32 = 0;

    err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_ProductId, productId32);

#if defined(CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID) && CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
#if defined(ENABLE_CHIP_APP_EXT)
        productId32 = 0;
#else
        productId32 = CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID;
#endif // ENABLE_CHIP_APP_EXT
        //in case of first time, write it on NVRAM
        err = RnConfig::WriteConfigValue(RnConfig::kConfigKey_ProductId, productId32);
    }
#endif // defined(CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID) && CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID
    ReturnErrorOnFailure(err);

    productId = static_cast<uint16_t>(productId32);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetProductId(uint16_t productId)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_ProductId, static_cast<uint32_t>(productId));
}

CHIP_ERROR rnDeviceDataProvider::GetHardwareVersionString(char * buf, size_t bufSize)
{
    size_t hardwareVersionStringLen = 0; // without counting null-terminator
    CHIP_ERROR err =
        RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_HardwareVersionString, buf, bufSize, hardwareVersionStringLen);
#if defined(CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING)
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        err = CHIP_NO_ERROR;
        memset(buf, 0, bufSize);
        hardwareVersionStringLen = strlen(CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING);
        if (bufSize < hardwareVersionStringLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING, hardwareVersionStringLen);
        //in case of first time, write it on NVRAM
        RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_HardwareVersionString, buf, hardwareVersionStringLen);
    }
#endif // CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION_STRING
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetHardwareVersion(uint16_t & hardwareVersion)
{
    CHIP_ERROR err;
    uint32_t hardwareVersion32;

    err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_HardwareVersion, hardwareVersion32);
#if defined(CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION)
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
#if defined(ENABLE_CHIP_APP_EXT)
        hardwareVersion32 = 0;
#else
        hardwareVersion32 = CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION;
#endif // ENABLE_CHIP_APP_EXT
        err = RnConfig::WriteConfigValue(RnConfig::kConfigKey_HardwareVersion, (uint32_t)hardwareVersion32);
    }
#endif // defined(CHIP_DEVICE_CONFIG_DEVICE_HARDWARE_VERSION)

    hardwareVersion = static_cast<uint16_t>(hardwareVersion32);
    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetHardwareVersion(uint16_t hardwareVersion)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_HardwareVersion, static_cast<uint32_t>(hardwareVersion));
}

CHIP_ERROR rnDeviceDataProvider::GetDeviceTypeIdEp1(uint32_t & deviceId)
{
    CHIP_ERROR err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Id, deviceId);

    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        deviceId = 0x000A; // lock
        err      = CHIP_NO_ERROR;
        RnConfig::WriteConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Id, deviceId);
    }

    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetDeviceTypeIdEp1(uint32_t deviceId)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Id, deviceId);
}  

CHIP_ERROR rnDeviceDataProvider::GetDeviceTypeVersionEp1(uint8_t & deviceVer)
{
    uint32_t ver_32;
    CHIP_ERROR err = RnConfig::ReadConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Ver, ver_32);

    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        ver_32 = 0x0001;
        err = CHIP_NO_ERROR;
        RnConfig::WriteConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Ver, ver_32);
    }

    deviceVer = static_cast<uint8_t>(ver_32);

    return err;
}

CHIP_ERROR rnDeviceDataProvider::SetDeviceTypeVersionEp1(uint8_t deviceVer)
{
    return RnConfig::WriteConfigValue(RnConfig::kConfigKey_DeviceTypeEp1Ver, static_cast<uint32_t>(deviceVer));
}

CHIP_ERROR rnDeviceDataProvider::GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan)
{
    ChipError err = CHIP_ERROR_WRONG_KEY_TYPE;
#if CHIP_ENABLE_ROTATING_DEVICE_ID
    static_assert(ConfigurationManager::kRotatingDeviceIDUniqueIDLength >= ConfigurationManager::kMinRotatingDeviceIDUniqueIDLength,
                  "Length of unique ID for rotating device ID is smaller than minimum.");

    size_t uniqueIdLen = 0;
    err =
        RnConfig::ReadConfigValueBin(RnConfig::kConfigKey_UniqueId, uniqueIdSpan.data(), uniqueIdSpan.size(), uniqueIdLen);
#ifdef CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        constexpr uint8_t uniqueId[] = CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID;

        ReturnErrorCodeIf(sizeof(uniqueId) > uniqueIdSpan.size(), CHIP_ERROR_BUFFER_TOO_SMALL);
        memcpy(uniqueIdSpan.data(), uniqueId, sizeof(uniqueId));
        uniqueIdLen = sizeof(uniqueId);
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueBin(RnConfig::kConfigKey_UniqueId, uniqueIdSpan.data(), uniqueIdLen);
        //err = CHIP_NO_ERROR;
    }
#endif // CHIP_DEVICE_CONFIG_ROTATING_DEVICE_ID_UNIQUE_ID

    ReturnErrorOnFailure(err);
    uniqueIdSpan.reduce_size(uniqueIdLen);

#endif // CHIP_ENABLE_ROTATING_DEVICE_ID
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetSerialNumber(char * buf, size_t bufSize)
{
    size_t serialNumberLen = 0; // without counting null-terminator
    CHIP_ERROR err;
    err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_SerialNum, buf, bufSize, serialNumberLen);
#ifdef CHIP_DEVICE_CONFIG_TEST_SERIAL_NUMBER
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        serialNumberLen = strlen (getSerialNumber());
        if (bufSize < serialNumberLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, getSerialNumber(), serialNumberLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_SerialNum, buf, serialNumberLen);
    }
    return err;
#endif
}

CHIP_ERROR rnDeviceDataProvider::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    CHIP_ERROR err;
    constexpr uint8_t kDateStringLength = 10; // YYYY-MM-DD
    char dateStr[kDateStringLength + 1];
    size_t dateLen;
    char * parseEnd;

    err = RnConfig::ReadConfigValueBin(RnConfig::kConfigKey_ManufacturingDate, reinterpret_cast<uint8_t *>(dateStr),
                                           sizeof(dateStr), dateLen);
    SuccessOrExit(err);

    VerifyOrExit(dateLen == kDateStringLength, err = CHIP_ERROR_INVALID_ARGUMENT);

    // Cast does not lose information, because we then check that we only parsed
    // 4 digits, so our number can't be bigger than 9999.
    year = static_cast<uint16_t>(strtoul(dateStr, &parseEnd, 10));
    VerifyOrExit(parseEnd == dateStr + 4, err = CHIP_ERROR_INVALID_ARGUMENT);

    // Cast does not lose information, because we then check that we only parsed
    // 2 digits, so our number can't be bigger than 99.
    month = static_cast<uint8_t>(strtoul(dateStr + 5, &parseEnd, 10));
    VerifyOrExit(parseEnd == dateStr + 7, err = CHIP_ERROR_INVALID_ARGUMENT);

    // Cast does not lose information, because we then check that we only parsed
    // 2 digits, so our number can't be bigger than 99.
    day = static_cast<uint8_t>(strtoul(dateStr + 8, &parseEnd, 10));
    VerifyOrExit(parseEnd == dateStr + 10, err = CHIP_ERROR_INVALID_ARGUMENT);

exit:
    if (err != CHIP_NO_ERROR && err != CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND)
    {
        ChipLogError(DeviceLayer, "Invalid manufacturing date: %s", dateStr);
    }
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        year        = 2023;
        month       = 1;
        day         = 1;
        err         = CHIP_NO_ERROR;
    }
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetPartNumber(char * buf, size_t bufSize)
{
    size_t partNumberLen = 0; // without counting null-terminator
    CHIP_ERROR err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_PartNumber, buf, bufSize, partNumberLen);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        partNumberLen = strlen(CHIP_DEVICE_CONFIG_TEST_PART_NUMBER);
        if (bufSize < partNumberLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_TEST_PART_NUMBER, partNumberLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_PartNumber, buf, partNumberLen);
    }
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetProductURL(char * buf, size_t bufSize)
{
    size_t productUrlLen = 0; // without counting null-terminator
    CHIP_ERROR err = RnConfig::ReadConfigValueStr(RnConfig::kConfigKey_ProductURL, buf, bufSize, productUrlLen);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        productUrlLen = strlen(CHIP_DEVICE_CONFIG_TEST_PRODUCT_URL);
        if (bufSize < productUrlLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_TEST_PRODUCT_URL, productUrlLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueStr(RnConfig::kConfigKey_ProductURL, buf, productUrlLen);
    }
    return err;
}

CHIP_ERROR rnDeviceDataProvider::GetProductLabel(char * buf, size_t bufSize)
{
    size_t productLabelLen = 0; // without counting null-terminator
    CHIP_ERROR err = RnConfig::ReadConfigValueStr(RnConfig::KConfigKey_ProductLabel, buf, bufSize, productLabelLen);
    if (err == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND) {
        memset(buf, 0, bufSize);
        productLabelLen = strlen(CHIP_DEVICE_CONFIG_TEST_PRODUCT_LABLE);
        if (bufSize < productLabelLen) {
            err = CHIP_ERROR_INVALID_ARGUMENT;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }
        memcpy(buf, CHIP_DEVICE_CONFIG_TEST_PRODUCT_LABLE, productLabelLen);
        err = CHIP_NO_ERROR;
        //in case of first time, write it on NVRAM
        //RnConfig::WriteConfigValueStr(RnConfig::KConfigKey_ProductLabel, buf, productLabelLen);
    }
    return err;
}

rnDeviceDataProvider & rnDeviceDataProvider::GetDeviceDataProvider()
{
    static rnDeviceDataProvider sDataProvider;
    return sDataProvider;
}

} // namespace Rn
} // namespace DeviceLayer
} // namespace chip
