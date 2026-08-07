/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
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
#pragma once

#include <credentials/DeviceAttestationCredsProvider.h>

namespace chip {
namespace Credentials {
namespace Renes {
namespace common {
/**
 * @brief Set implementation of a converting Hex ASCII string to bin form.
 */
CHIP_ERROR HexAsciiToBin(const unsigned char * hex, size_t hexLen, uint8_t * out, size_t & outLen);

}
/**
 * @brief Get implementation of a sample DAC provider to validate device
 *        attestation procedure.
 *
 * @returns a singleton DeviceAttestationCredentialsProvider that relies on no
 *          storage abstractions.
 */
DeviceAttestationCredentialsProvider * GetRenesDacProvider();

/**
 * @brief Set implementation of a certification in flash memory to validate device
 *        attestation procedure.
 */
CHIP_ERROR SetCertificationDeclaration(unsigned char* data, uint32_t val);
/**
 * @brief Set implementation of a device attestation cert in flash memory to validate device
 *        attestation procedure.
 */
CHIP_ERROR SetDeviceAttestationCert(unsigned char* data, uint32_t val);
/**
 * @brief Set implementation of a product attestation in flash memory to validate device
 *        attestation procedure.
 */
CHIP_ERROR SetProductAttestationIntermediateCert(unsigned char* data, uint32_t val);
/**
 * @brief Set implementation of a device private key in flash memory to validate device
 *        attestation procedure.
 */
CHIP_ERROR SetDeviceAttestationPrivKey(unsigned char* data, uint32_t val);
/**
 * @brief Set implementation of a device public key in flash memory to validate device
 *        attestation procedure.
 */
CHIP_ERROR SetDeviceAttestationPubKey(unsigned char* data, uint32_t val);

} // namespace Renes
} // namespace Credentials
} // namespace chip
