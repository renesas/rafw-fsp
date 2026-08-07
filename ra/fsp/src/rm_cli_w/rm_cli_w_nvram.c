/**
 ****************************************************************************************
 *
 * @file rm_cli_w_nvram.c
 *
 * @brief NVRAM command functions
 *
 * Copyright (c) 2023 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "sdk_defs.h"
#include "FreeRTOS.h"
#include "task.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#define SUPPORT_CLI_NVRAM

#if (dg_configNVPARAM_ADAPTER == 1)
#if !(dg_configFLASH_ADAPTER == 1 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !(dg_configNVMS_ADAPTER == 1 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !(dg_configNVMS_VES == 1 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !(dg_configNVPARAM_ADAPTER == 1 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !(dg_configNVPARAM_ADAPTERv2 == 1 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !(dg_configNVPARAM_APP_AREA == 2 )
#undef  SUPPORT_CLI_NVRAM
#endif
#if !defined(dg_configADNVPARAM_PROJ_FILE)
#undef  SUPPORT_CLI_NVRAM
#endif
#endif /* dg_configNVPARAM_ADAPTER */

extern void find_group_list(void);
extern bool chk_duplicates_env(void);

#if defined(SUPPORT_CLI_NVRAM) && defined(RM_MAP_PERSISTANT_W)

#if (dg_configNVPARAM_ADAPTERv2 == 1)
static bool cmd_getenvauto(int argc, const char **argv)
{
    uint8_t * data_ptr;
    int8_t data_length = 0;

    if (argc != 3) {
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_Auto(RM_MAP_PERSISTANT_W_get_ctrl(), (char *) argv[1], (char *) argv[2],
                           (int8_t *) &data_length, &data_ptr);
#endif

    if (data_ptr == NULL)
    {
        printf("%s=<Empty>\n", argv[2]);
        return true;
    }

    switch ((int)data_length) {
        case sizeof(uint8_t):
            printf("%s .... (int8)%d 0x%02x", argv[2], *(int8_t *) data_ptr, *(int8_t *) data_ptr);
            break;

        case sizeof(uint16_t):
            printf("%s ... (int16) %d 0x%04x", argv[2],  *(int16_t *) data_ptr, *(int16_t *) data_ptr);
            break;

        case sizeof(uint32_t):
            printf("%s ... (int32) %d 0x%08x", argv[2], *(int *) data_ptr, *(unsigned int *) data_ptr);
            break;

        case -1:
            printf("%s.%s is not matched", argv[1], argv[2]);
            break;

        default:
            printf("%s=", argv[2]);
            puts((char *) data_ptr);
            break;
        }

    puts("\n");

    return true;
}

static bool cmd_getenvbin(int argc, const char **argv)
{
    int data_len;
    union {
        uint32_t value32;
        uint16_t value16;
        uint8_t  value8;
    } value;

    uint32_t len = 0;

    if (argc == 3) {
        len = 0;
    } else if (argc == 4) {
        len = atoi(argv[3]);
    } else {
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1], (char *)argv[2], &value, (uint16_t *)&data_len);
#endif

    if(argc == 3)
    {
        len = data_len;
    }

    if (data_len > 0) {
        switch (len) {
            case sizeof(uint8_t):
                printf("\n%s=>(u8) %02x, %d", argv[2], value.value8, value.value8);
                break;

            case sizeof(uint16_t):
                printf("\n%s=>(u16) %04x, %d", argv[2], value.value16, value.value16);
                break;

            default:
                printf("\n%s=>(u32) %04x, %d", argv[2], (unsigned int)value.value32, (int)value.value32);
                break;
        }

        puts("\n");
    } else {
        printf("\n%s.%s is not matched or illegal (len?)\n", argv[1], argv[2]);
    }

    return true;
}

// setenvbin partition tagname, len, value
static bool cmd_setenvbin(int argc, const char **argv)
{
    union {
        uint32_t value32;
        uint16_t value16;
        uint8_t value8;
    } value;
    uint32_t len;

    if (argc != 5) {
        return false;
    }

    len = atoi(argv[3]);
    switch (len) {
        case sizeof(uint8_t) :
            value.value8 = atoi(argv[4]);
            printf("\n%s:=(u8) %02x, %d", argv[2], value.value8, value.value8);
            break;

        case sizeof(uint16_t) :
            value.value16 = atoi(argv[4]);
            printf("\n%s:=(u16) %04x, %d", argv[2], value.value16, value.value16);
            break;

        default :
            value.value32 = atoi(argv[4]);
            printf("\n%s:=(u32) %04x, %d", argv[2], (unsigned int)value.value32, (int)value.value32);
            break;
    }
    puts("\n");
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_UINT(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1],
			                         (char *)argv[2], &value, len) == FSP_SUCCESS) {
#endif
        puts("ra6w1_setenv_bin : ok");
    } else {
        puts("ra6w1_setenv_bin : err");
    }

    return true;
}

static bool cmd_setenvint(int argc, const char **argv)
{
    if (argc != 4) {
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1],
			                         (char *)argv[2], atoi(argv[3])) == FSP_SUCCESS) {
#endif
        puts("setenvint : ok");
    } else {
        puts("setenvint : err");
    }

    return true;
}

#endif

// getenv partition tagname
static bool cmd_getenv(int argc, const char **argv)
{
    char *datastr = NULL;

    if (argc != 3) {
        return false;
    }

#ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1], (char *)argv[2], &datastr);
#endif

    if ( datastr != NULL) {
        printf("\n%s=", argv[2]);
        puts(datastr);
        puts("\n");
    } else {
        printf("\n%s.%s is not matched\n", argv[1], argv[2]);
    }

    return true;
}

// setenv partition tagname value
static bool cmd_setenv(int argc, const char **argv)
{
    if (argc != 4) {
        return false;
    }
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                       (char *)argv[1], (char *)argv[2],
                                       (char *)argv[3]) == FSP_SUCCESS) {
        printf("setenv %s->%s='%s' OK\n", argv[1], argv[2], argv[3]);
        return true;
    } else {
        printf("setenv %s->%s='%s' FAILED\n", argv[1], argv[2], argv[3]);
        return false;
    }
#endif
}

static bool cmd_setenvauto(int argc, const char **argv)
{
    if (argc != 4) {
        return false;
    }
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_Auto(RM_MAP_PERSISTANT_W_get_ctrl(),
                                       (char *) argv[1], (char *) argv[2],
                                       (char *) argv[3]) == FSP_SUCCESS) {
        return true;
    } else {
        return false;
    }
#endif
}

#ifdef SIGMA_TEST_ENABLE
bool cmd_setenv_wrapper(int argc, const char **argv)
{
    cmd_setenv(argc, argv);

    return pdTRUE;
}
#endif


// unsetenv partition tagname
static bool cmd_unsetenv(int argc, const char **argv)
{
    if (argc != 3) {
        return false;
    }
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1],
                                (char *)argv[2]) == FSP_SUCCESS) {
        return true;
    } else {
        return false;
    }
#endif
}

// puttenv partition tagname
static bool cmd_putenv(int argc, const char **argv)
{
    if (argc != 3) {
        return false;
    }
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), (char *)argv[1],
                                       (char *)argv[2], (char *)argv[3]) == FSP_SUCCESS) {
        return true;
    } else {
        return false;
    }
#endif
}

static bool cmd_clearenv(int argc, const char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "format") == 0) {
            printf("Start nvram format.\n");

            /*
            * !!! caution !!!
            *   rm_vee_flash_w will erase all data.
            */
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), NULL);
#endif
            printf("Format completed\n");
            return pdPASS;
        } else if (strncmp(argv[1], ENV_GROUP_BOOTCFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_DEVCFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_WIFICFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_SYSCFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_APPCFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_TESTCFG, 3) == 0
            || strncmp(argv[1], ENV_GROUP_WIFIPROFILE, 3) == 0
            || strncmp(argv[1], ENV_GROUP_BLECFG, 3) == 0) {

            /*
            * !!! caution !!!
            *   cpu clock is erased w/ the option ENV_GROUP_BOOTCFG ( "bootcfg" )
            */
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), (char*)argv[1]);
#endif
        } else if (strcmp(argv[1], ENV_GROUP_ALL) == 0) {
            /*
            * !!! caution !!!
            *   MACAddress is erased w/ the option ENV_GROUP_ALL ( "all" )
            */
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_BOOTCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_DEVCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG);
#endif

#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_TESTCFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE);
#endif
        } else {
            return pdFAIL;
        }
        printf("Erase completed %s\n", argv[1]);
        return pdPASS;
    }
    return pdFAIL;
}

static bool cmd_refreshenv(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    /* To improve the search speed of rm_vee_flash_w,
     * we need to refresh it to remove duplicated records.
     */
    printf("Start nvram reflash.\n");
#ifdef RM_MAP_PERSISTANT_W
    if (RM_MAP_PERSISTANT_W_reflash(RM_MAP_PERSISTANT_W_get_ctrl()) == FSP_SUCCESS) {
#endif
        printf("Completed.\n");
    } else {
        printf("Failure.\n");
    }

    return true;
}

static bool cmd_group_list(int argc, const char **argv)
{
    (void) argc;
    (void) argv;

    find_group_list();
    return true;
}

static bool cmd_printenv(int argc, const char **argv)
{
    int printall = 0;

    if (argc >= 1 && argc <= 3) {
        if ((argc == 2 && strncmp(argv[1], ENV_GROUP_ALL, 3) == 0)
            || (argc == 3 && strncmp(argv[2], ENV_GROUP_ALL, 3) == 0)) {

            printall = 1;
        }
        printf("\n");
        if (argc >= 2 &&
            (strncmp(argv[1], ENV_GROUP_BOOTCFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_DEVCFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_WIFICFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_SYSCFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_APPCFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_TESTCFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_BLECFG, 3) == 0
             || strncmp(argv[1], ENV_GROUP_WIFIPROFILE, 3) == 0)) {

#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      (char*)argv[1], printall);
#endif
        } else {
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_BOOTCFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_DEVCFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFICFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_SYSCFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase_GROUP(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_BLECFG);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_TESTCFG, printall);
#endif
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Display(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_WIFIPROFILE, printall);
#endif
        }
    } else {
        return pdFAIL;
    }

    chk_duplicates_env();

    return pdPASS;
}

static const debug_handler_t nvram_handlers[] = {
#if (dg_configNVPARAM_ADAPTERv2 == 1)
    { "getenvauto", "getenvauto [<group>][<name>]",                     cmd_getenvauto  },
    { "getenvbin",  "getenvbin [<group>][<name>][<data_len>|4|2|1]",    cmd_getenvbin   },
    { "setenvbin",  "setenvbin [<group>][<name>][<data_len>|4|2|1][<value32>|<value16>|<value8>]",  cmd_setenvbin },
    { "setenvint",  "setenvint [<group>][<name>][<value>]",             cmd_setenvint },
#endif /* dg_configNVPARAM_ADAPTERv2 */
    { "getenv",     "getenv [<group>][<name>]",                         cmd_getenv      },
    { "setenv",     "setenv [<group>][<name>][string value]",           cmd_setenv      },
    { "setenvauto", "setenvauto [<group>][<name>][value]",              cmd_setenvauto  },
    { "unsetenv",   "unsetenv [<group>][<name>]",                       cmd_unsetenv    },
    { "putenv",     "putenv [<group>][<name>]",                         cmd_putenv      },
    { "printenv",   "printenv [<group>|all][all|<none>]",               cmd_printenv    },
    { "group",      "print group list",                                 cmd_group_list  },
    { "clearenv",   "clearenv [<group>|format|all]",                    cmd_clearenv    },
    { "refreshenv", "refresh nvram",                                    cmd_refreshenv  },
    { NULL },
};

#endif //SUPPORT_CLI_NVRAM


bool nvram_command(int argc, const char *argv[], void *user_data)
{
    (void) user_data;

#if defined(SUPPORT_CLI_NVRAM) && defined(RM_MAP_PERSISTANT_W)
    return debug_handle_message(argc, argv, nvram_handlers);
#else
    printf("\nnot supported");
    return false;
#endif //SUPPORT_CLI_NVRAM
}
