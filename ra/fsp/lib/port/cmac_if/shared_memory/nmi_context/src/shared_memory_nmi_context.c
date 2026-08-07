/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "shared_memory_nmi_context.h"

#if defined(CORTEX_M33)
shared_ram_nmi_context_t BSP_PLACE_IN_SECTION(BSP_SECTION_NOINIT) g_shared_ram_nmi_context;

shared_ram_nmi_context_t * gp_shared_ram_nmi_context_ptr = &g_shared_ram_nmi_context;
#endif
