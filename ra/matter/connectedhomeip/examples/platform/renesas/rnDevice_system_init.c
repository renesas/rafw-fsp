 /*
  *
  *    Copyright (c) 2023 Renesas Electronics Corporation and/or its  affiliates
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
 
 /*
  *
  * @file rnDevice_system_init.c
  *
  * @brief System Intialization.
  *
  */
#include "common_def.h"
#if 1 //[rrq61000 matter work]
#include "net_common.h"
#else
#include "da16x_network_common.h"
#endif
#include "iface_defs.h"
#include <wifi_rn_events.h>

extern int get_run_mode(void);

void rnDevice_system_init(void)
{
    PRINTF("[%s:%d] initialization system if needed\n", __func__, __LINE__);
    if (get_run_mode() == WIFI_DEVICE_MODE_EXT_STATION)
    {
        static int chkCount = 1;
        while ((chk_network_ready(WLAN0_IFACE) == 0) || (!wifi_have_ipv6_addr(RN_WIFI_STA_INTERFACE)))
        {
            vTaskDelay(10);
            if (chkCount++ % 20 == 0)
            {
                PRINTF("Network status: waiting for valid IPv6\n");
            }
        }
    }
    return;
}
//]]matterwork
