/*
 *    Copyright (c) 2023 Project CHIP Authors
 *    All rights reserved.
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

#include <crypto/RawKeySessionKeystore.h>

#include <lib/support/BufferReader.h>

#include <cstdint>
#if defined(__USE_MATTER_DPM_APP__)
#include "rnDeviceWrapAPIs.h"
#endif


namespace chip {
namespace Crypto {

using HKDF_sha_crypto = HKDF_sha;

// The underlying representation of the HKDF key handle
struct RawHkdfKeyHandle
{
    ByteSpan Span() const { return ByteSpan(data, size); }

    // Cap the data size so that the entire structure fits in the opaque context of the HKDF key handle.
    static constexpr size_t kMaxDataSize = std::min<size_t>(CHIP_CONFIG_HKDF_KEY_HANDLE_CONTEXT_SIZE - sizeof(uint8_t), UINT8_MAX);

    uint8_t data[kMaxDataSize];
    uint8_t size;
};

CHIP_ERROR RawKeySessionKeystore::CreateKey(const Symmetric128BitsKeyByteArray & keyMaterial, Aes128KeyHandle & key)
{
    memcpy(key.AsMutable<Symmetric128BitsKeyByteArray>(), keyMaterial, sizeof(Symmetric128BitsKeyByteArray));
    return CHIP_NO_ERROR;
}

CHIP_ERROR RawKeySessionKeystore::CreateKey(const Symmetric128BitsKeyByteArray & keyMaterial, Hmac128KeyHandle & key)
{
    memcpy(key.AsMutable<Symmetric128BitsKeyByteArray>(), keyMaterial, sizeof(Symmetric128BitsKeyByteArray));
    return CHIP_NO_ERROR;
}

CHIP_ERROR RawKeySessionKeystore::CreateKey(const ByteSpan & keyMaterial, HkdfKeyHandle & key)
{
    RawHkdfKeyHandle & rawKey = key.AsMutable<RawHkdfKeyHandle>();

    VerifyOrReturnError(keyMaterial.size() <= sizeof(rawKey.data), CHIP_ERROR_BUFFER_TOO_SMALL);
    memcpy(rawKey.data, keyMaterial.data(), keyMaterial.size());
    rawKey.size = static_cast<uint8_t>(keyMaterial.size());

    return CHIP_NO_ERROR;
}

CHIP_ERROR RawKeySessionKeystore::DeriveKey(const P256ECDHDerivedSecret & secret, const ByteSpan & salt, const ByteSpan & info,
                                            Aes128KeyHandle & key)
{
    HKDF_sha hkdf;

    return hkdf.HKDF_SHA256(secret.ConstBytes(), secret.Length(), salt.data(), salt.size(), info.data(), info.size(),
                            key.AsMutable<Symmetric128BitsKeyByteArray>(), sizeof(Symmetric128BitsKeyByteArray));
}

CHIP_ERROR RawKeySessionKeystore::DeriveSessionKeys(const ByteSpan & secret, const ByteSpan & salt, const ByteSpan & info,
                                                    Aes128KeyHandle & i2rKey, Aes128KeyHandle & r2iKey,
                                                    AttestationChallenge & attestationChallenge)
{
    HKDF_sha hkdf;
    uint8_t keyMaterial[2 * sizeof(Symmetric128BitsKeyByteArray) + AttestationChallenge::Capacity()];

    ReturnErrorOnFailure(hkdf.HKDF_SHA256(secret.data(), secret.size(), salt.data(), salt.size(), info.data(), info.size(),
                                          keyMaterial, sizeof(keyMaterial)));

    Encoding::LittleEndian::Reader reader(keyMaterial, sizeof(keyMaterial));
#if defined(__USE_MATTER_DPM_APP__)
    if (RM_PMGR_W_dpm_is_enabled()) {
        static bool flagFirst = false;
        if (gMatRtmPtr) {
            RENES_LOG("[%s:%d] save !!! keyMaterial[%d]", __func__, __LINE__, sizeof(keyMaterial));
            memcpy(gMatRtmPtr->msg_salt, keyMaterial, sizeof(keyMaterial));
        }
        else {
            RENES_LOG("[%s:%d] Matter RTM not allocated", __func__, __LINE__);
        }
    }
#endif

    return reader.ReadBytes(i2rKey.AsMutable<Symmetric128BitsKeyByteArray>(), sizeof(Symmetric128BitsKeyByteArray))
        .ReadBytes(r2iKey.AsMutable<Symmetric128BitsKeyByteArray>(), sizeof(Symmetric128BitsKeyByteArray))
        .ReadBytes(attestationChallenge.Bytes(), AttestationChallenge::Capacity())
        .StatusCode();
}

#if defined(__USE_MATTER_DPM_APP__)
CHIP_ERROR RawKeySessionKeystore::DeriveSessionKeysAlt(const ByteSpan & secret, const ByteSpan & salt, const ByteSpan & info,
                                                    Aes128KeyHandle & i2rKey, Aes128KeyHandle & r2iKey,
                                                    AttestationChallenge & attestationChallenge)
{
    HKDF_sha_crypto hkdf;
    uint8_t keyMaterial[2 *sizeof(Symmetric128BitsKeyByteArray) + AttestationChallenge::Capacity()];

    Encoding::LittleEndian::Reader reader(keyMaterial, sizeof(keyMaterial));

    if (RM_PMGR_W_dpm_is_enabled()) {
        static bool flagFirst = false;
        if (gMatRtmPtr) {
            if (RM_PMGR_W_dpm_is_wakeup() && !flagFirst) {
                //commissioning (rResponder(1)) -> dpm wake up(rInitiator(1))=> need swapped msg_salt
                if (gMatRtmPtr->rResponder && gMatRtmPtr->rInitiator)
                {
                    uint8_t tmpBuf[48] = {0,};
                    memcpy(tmpBuf, gMatRtmPtr->msg_salt, sizeof(gMatRtmPtr->msg_salt));
                    memcpy(&tmpBuf[0], &gMatRtmPtr->msg_salt[16], 16);
                    memcpy(&tmpBuf[16], &gMatRtmPtr->msg_salt[0], 16);
                    memcpy(gMatRtmPtr->msg_salt, tmpBuf, sizeof(tmpBuf));
                    gMatRtmPtr->rInitiator = 0;
                    gMatRtmPtr->rResponder = 0;
                }
                memcpy(keyMaterial, gMatRtmPtr->msg_salt, sizeof(keyMaterial));
                RENES_LOG("[%s:%d] applied !!! keyMaterial[%d] ----> ", __func__, __LINE__, sizeof(keyMaterial));

                flagFirst = true;
            }
            else {
                RENES_LOG("[%s:%d] saving not supported keyMaterial[%d]", __func__, __LINE__, sizeof(keyMaterial));
            }
        }
        else {
            RENES_LOG("[%s:%d] Matter RTM not allocated", __func__, __LINE__);
        }
    }

    return reader.ReadBytes(i2rKey.AsMutable<Symmetric128BitsKeyByteArray>(), sizeof(Symmetric128BitsKeyByteArray))
        .ReadBytes(r2iKey.AsMutable<Symmetric128BitsKeyByteArray>(), sizeof(Symmetric128BitsKeyByteArray))
        .ReadBytes(attestationChallenge.Bytes(), AttestationChallenge::Capacity())
        .StatusCode();
}
#endif

CHIP_ERROR RawKeySessionKeystore::DeriveSessionKeys(const HkdfKeyHandle & hkdfKey, const ByteSpan & salt, const ByteSpan & info,
                                                    Aes128KeyHandle & i2rKey, Aes128KeyHandle & r2iKey,
                                                    AttestationChallenge & attestationChallenge)
{
    return DeriveSessionKeys(hkdfKey.As<RawHkdfKeyHandle>().Span(), salt, info, i2rKey, r2iKey, attestationChallenge);
}

void RawKeySessionKeystore::DestroyKey(Symmetric128BitsKeyHandle & key)
{
    ClearSecretData(key.AsMutable<Symmetric128BitsKeyByteArray>());
}

void RawKeySessionKeystore::DestroyKey(HkdfKeyHandle & key)
{
    RawHkdfKeyHandle & rawKey = key.AsMutable<RawHkdfKeyHandle>();

    ClearSecretData(rawKey.data);
    rawKey.size = 0;
}

} // namespace Crypto
} // namespace chip
