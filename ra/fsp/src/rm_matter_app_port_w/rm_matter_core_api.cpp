/* ${REA_DISCLAIMER_PLACEHOLDER} */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>

#include "rm_wifi.h"

/* Socket and WiFi interface includes. */

#include "matter_main.h"
#include "rm_matter_wifi_common.h"

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
MATTERReturnCode_t MATTER_On(void)
{
    return (MATTERReturnCode_t) RM_MATTER_WIFI_Open(&g_rm_matter_app_ctrl, &g_rm_matter_app_cfg);
}

MATTERReturnCode_t MATTER_Off(void)
{
    return (MATTERReturnCode_t) RM_MATTER_WIFI_Close(&g_rm_matter_app_ctrl);
}
