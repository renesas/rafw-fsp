/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Nest Labs, Inc.
 *    Copyright (c) 2023 Modified by Renesas Electronics Corporation
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Utilities for accessing persisted device configuration on
 *          platforms based on the Renesas SDK.
 */
/* this file behaves like a config.h, comes first */
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <lib/core/CHIPEncoding.h>
#include "platform/renesas/RnConfig.h"

#include "FreeRTOS.h"
#include "rnDeviceWrapAPIs.h"

#define MAX_MKEY_LEN	16

namespace chip {
namespace DeviceLayer {
namespace Internal {

#if defined(USE_LVL_VALUE_RAM)
uint8_t gTmplvlBuf[15];
#endif

CHIP_ERROR RnConfig::Init()
{
    return CHIP_NO_ERROR;
}

void RnConfig::DeInit()
{

}

CHIP_ERROR RnConfig::ReadConfigValue(Key key, bool & val)
{
    CHIP_ERROR err;
    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc, storedVal;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    rc = matter_read_nvram_int(tmpKey, &storedVal);
    if (rc == 0) {
        val = (bool)storedVal;
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValue(Key key, uint32_t & val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc, storedVal;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    rc = matter_read_nvram_int(tmpKey, &storedVal);
    if (rc == 0) {
        val = (uint32_t)storedVal;
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValue(Key key, uint64_t & val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc, storedVal;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
//checking more
    uint8_t tmpBuf[10];
    size_t dataLen = 0;
    memset(tmpBuf, 0x00, 10);
    dataLen = matter_read_nvram_binary((const char*)tmpKey, tmpBuf, 8);
    if (dataLen > 0) {
        if (sizeof(uint64_t) < dataLen) {
            err = CHIP_ERROR_BUFFER_TOO_SMALL;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }

        if (dataLen == 8) {
            val = ((uint64_t)tmpBuf[7] << 56) | ((uint64_t)tmpBuf[6] << 48) | ((uint64_t)tmpBuf[5] << 40) | ((uint64_t)tmpBuf[4] << 32) |
                tmpBuf[3] << 24 | tmpBuf[2] << 16 | tmpBuf[1] << 8 | tmpBuf[0];
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            err = CHIP_NO_ERROR;
        } else {
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; //fix this
        }
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
        ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
    }

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    char *tmpBuf = NULL;
    size_t dataLen;
    outLen = 0;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    tmpBuf = matter_read_nvram_string(tmpKey);
    if (tmpBuf != NULL) {
        dataLen = strlen(tmpBuf);
        if (dataLen <= 0) {
            err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
            return err;
        }
        if (bufSize <= dataLen) {
            err = CHIP_ERROR_BUFFER_TOO_SMALL;
            return err;
        }
        outLen = ((dataLen == 1) && (tmpBuf[0] == 0)) ? 0 : dataLen;
        memcpy(buf, tmpBuf, outLen);
        buf[outLen] = 0; // Add the terminator char.
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    uint8_t *tmpBuf = NULL;
    int rc;
    size_t dataLen = 0;
    outLen = 0;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    tmpBuf = (uint8_t *)pvPortMalloc(bufSize);
    dataLen = matter_read_nvram_binary((const char*)tmpKey, tmpBuf, bufSize);
    if (dataLen > 0) {
        if (bufSize < dataLen) {
            outLen = bufSize;
        } else {
            outLen = dataLen;
        }

        memcpy(buf, tmpBuf, outLen);
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }
    vPortFree(tmpBuf);

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen, size_t offset)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    uint8_t *tmpBuf = NULL;
    int rc;
    size_t dataLen = 0;
    outLen = 0;

#if defined(USE_LVL_VALUE_RAM)
    if(key == 0x3ab7505) {
        memcpy(buf, gTmplvlBuf, bufSize);
        return CHIP_NO_ERROR;
    }
#endif

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    tmpBuf = (uint8_t *)pvPortMalloc(bufSize + offset);
    dataLen = matter_read_nvram_binary((const char*)tmpKey, tmpBuf, bufSize + offset);
    if (dataLen > 0) {
        if (bufSize < dataLen - offset) {
            outLen = bufSize;
        } else {
            outLen = dataLen - offset;
        }
        memcpy(buf, &tmpBuf[offset], outLen);
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }
    vPortFree(tmpBuf);

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen, size_t keyLen, size_t offset)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    uint8_t *tmpBuf = NULL;
    int rc;
    size_t dataLen = 0;
    outLen = 0;

#if defined(USE_LVL_VALUE_RAM)
    if(key == 0x3ab7505)
    {
        memcpy(buf, gTmplvlBuf, bufSize);
        return CHIP_NO_ERROR;
    }
#endif

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    tmpBuf = (uint8_t*)pvPortMalloc(bufSize+keyLen);
    dataLen = matter_read_nvram_binary((char *)tmpKey, tmpBuf, bufSize+keyLen);
    if (tmpBuf != NULL)
    {
        if (dataLen <= 0) {
            //err = CHIP_ERROR_INVALID_STRING_LENGTH;
            err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
            vPortFree(tmpBuf);
            return err;
        }

        if (bufSize < dataLen - (offset+keyLen)) {
            outLen = bufSize;
        }
        else {
            outLen = dataLen - (offset+keyLen);
        }
        memcpy(buf, &tmpBuf[offset+keyLen], outLen);
        err = CHIP_NO_ERROR;
    }
    else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }
    vPortFree(tmpBuf);

    return err;
}

CHIP_ERROR RnConfig::ReadConfigValueCounter(uint8_t counterIdx, uint32_t & val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc, storedVal;
    Key key = kMinConfigKey_MatterCounter + counterIdx;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    rc = matter_read_nvram_int(tmpKey, &storedVal);
    if (rc == 0) {
        val = (uint32_t)storedVal;
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValue(Key key, bool val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int tmpVal = (bool)val;

    if (ValidConfigKey(key) == false) {
        err = CHIP_ERROR_INVALID_ARGUMENT; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    int rc = matter_write_nvram_int(tmpKey, tmpVal);
    if (rc != 0) {
        err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    } else {
        err = CHIP_NO_ERROR;
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValue(Key key, uint32_t val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int tmpVal = (int)val;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    int rc = matter_write_nvram_int(tmpKey, tmpVal);
    if (rc != 0) {
        err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    } else {
        err = CHIP_NO_ERROR;
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValue(Key key, uint64_t val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int tmpVal = (int)val;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    sprintf(tmpKey, "%07lx", key);
    //checking more
    int rc;
    uint8_t data[8] = {0,};
    data[0] = val & 0xff;
    data[1] = (val >> 8) & 0xff;
    data[2] = (val >> 16) & 0xff;
    data[3] = (val >> 24) & 0xff;
    data[4] = (val >> 32) & 0xff;
    data[5] = (val >> 40) & 0xff;
    data[6] = (val >> 48) & 0xff;
    data[7] = (val >> 56) & 0xff;
    rc = matter_write_nvram_binary((const char *)tmpKey, (const char*)data, sizeof(data));
    if (rc == 0) {
        RENES_LOG("[%s:%s:%d] val = 0x%llx", __FILENAME__, __func__, __LINE__, val);
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
        ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValueStr(Key key, const char * str)
{
    return WriteConfigValueStr(key, str, (str != NULL) ? strlen(str) : 0);
}

CHIP_ERROR RnConfig::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    CHIP_ERROR err = CHIP_ERROR_INVALID_ARGUMENT;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    char *tmpBuf;

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    if (str != NULL) {
        sprintf(tmpKey, "%07lx", key);
        tmpBuf = static_cast<char *>(chip::Platform::MemoryCalloc(strLen + 1, sizeof(char)));

        if (tmpBuf == NULL) {
            err = CHIP_ERROR_BUFFER_TOO_SMALL;
            ChipLogError(DeviceLayer, "[%s:%s:%d] err=0x%lx", __FILENAME__, __func__, __LINE__, err.Format());
            return err;
        }

        memcpy(tmpBuf, str, strLen);

        if (sizeof(tmpBuf) > strLen) {
            tmpBuf[strLen] = '\0';
        }

        int rc = matter_write_nvram_string(tmpKey, tmpBuf);
        if (rc != 0) {
            err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
        } else {
            err = CHIP_NO_ERROR;
        }
        chip::Platform::MemoryFree(tmpBuf);
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    CHIP_ERROR err = CHIP_ERROR_INVALID_ARGUMENT;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc;

#if defined(USE_LVL_VALUE_RAM)
    if(key == 0x3ab7505) {
        memset(gTmplvlBuf, 0x00, sizeof(gTmplvlBuf));
        memcpy(gTmplvlBuf, data, dataLen);
        return CHIP_NO_ERROR;
    }
#endif

    if (ValidConfigKey(key) == false) {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND; // Verify key id.
        return err;
    }

    if (data != NULL && dataLen > 0) {
        sprintf(tmpKey, "%07lx", key);
        rc = matter_write_nvram_binary((const char *)tmpKey, (const char*)data, dataLen);
        if (rc == 0) {
            err = CHIP_NO_ERROR;
        } else {
            err = CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
        }
    } else {
        err = CHIP_ERROR_INVALID_ARGUMENT;
    }

    return err;
}

CHIP_ERROR RnConfig::WriteConfigValueCounter(uint8_t counterIdx, uint32_t val)
{
    CHIP_ERROR err;

    char tmpKey[MAX_MKEY_LEN] = {0,};
    int tmpVal = (int)val;
    Key key = kMinConfigKey_MatterCounter + counterIdx;
    //VerifyOrExit(ValidConfigKey(key), err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND); // Verify key id.
    sprintf(tmpKey, "%07lx", key);
    int rc = matter_write_nvram_int(tmpKey, tmpVal);
    if (rc != 0) {
        err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    } else {
        err = CHIP_NO_ERROR;
    }

    return err;
}

CHIP_ERROR RnConfig::ClearConfigValue(Key key)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR RnConfig::ClearConfigValueTemp(Key key)
{
    return CHIP_NO_ERROR;
}

CHIP_ERROR RnConfig::SaveConfigValueTemp(void)
{
    CHIP_ERROR err;

    int rc = matter_save_tmp_nvram();
    if (rc == 0) {
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }

    return err;
}

bool RnConfig::ConfigValueExists(Key key)
{
    char tmpKey[MAX_MKEY_LEN] = {0,};
    int rc, storedVal;
    CHIP_ERROR err;

    sprintf(tmpKey, "%07lx", key);
    rc = matter_read_nvram_int(tmpKey, &storedVal);
    if (rc == 0) {
        err = CHIP_NO_ERROR;
    } else {
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return (err == CHIP_NO_ERROR);
}

bool RnConfig::ConfigValueExists(Key key, size_t & dataLen)
{
    CHIP_ERROR err;
    char tmpKey[MAX_MKEY_LEN] = {0,};
    char *buf = NULL;
    size_t readLen = 0;

    sprintf(tmpKey, "%07lx", key);
    buf = matter_read_nvram_string(tmpKey);
    if (buf != NULL) {
        readLen = strlen(buf);
        dataLen      = ((readLen == 1) && (buf[0] == 0)) ? 0 : readLen;
        buf[dataLen] = 0; // Add the terminator char.
        err = CHIP_NO_ERROR;
    } else {
        dataLen = readLen;
        err = CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
    }

    return (err == CHIP_NO_ERROR);
}

CHIP_ERROR RnConfig::FactoryResetConfig(void)
{
    CHIP_ERROR err;

//matterwork[[::nvram work
    RENES_LOG("[%s:%s:%d] factory reset...", __FILENAME__, __func__, __LINE__);
    err = CHIP_NO_ERROR;
//]]matterwork

    return err;
}

bool RnConfig::ValidConfigKey(Key key)
{
    // Returns true if the key is in the Matter NVRAM reserved key range.
    // or if the key is in the User Domain key range
    // Additional check validates that the user consciously defined the expected key range
    if (((key >= kMatterNvramKeyLoLimit) && (key <= kMatterNvramKeyHiLimit) && (key >= kMinConfigKey_MatterFactory) &&
         (key <= kMaxConfigKey_MatterKvs)) ||
        ((key >= kUserNvramKeyDomainLoLimit) && (key <= kUserNvramKeyDomainHiLimit))) {
        return true;
    }

    return false;
}

void RnConfig::RunConfigUnitTest()
{
    // Run common unit test.
//matterwork[[::nvram work
    RENES_LOG("[%s:%s:%d] TBD", __FILENAME__, __func__, __LINE__);
//]]matterwork
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
