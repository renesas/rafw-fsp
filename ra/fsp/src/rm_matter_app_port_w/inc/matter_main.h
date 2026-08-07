/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __MATTER_MAIN_H__
#define __MATTER_MAIN_H__
#include <AppTask.h>

#include "AppConfig.h"
#include "InitRnPlatform.h"
#include <DeviceInfoProviderImpl.h>
#include <app/server/Server.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <MatterConfig.h>
#define RENES_ATTESTATION_CREDENTIALS //matterwork[[::matterfeatures::

#ifdef RENES_ATTESTATION_CREDENTIALS
#include "../examples/platform/renesas/RnDeviceAttestationCreds.h"
#else
#include <credentials/examples/DeviceAttestationCredsExample.h>
#endif


extern "C" void matter_app_main_start(void *arg);
#endif
