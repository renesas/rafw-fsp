/**
 ****************************************************************************************
 *
 * @file rm_cli_w_mem.c
 *
 * @brief i2c memory command functions
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

#define SUPPORT_BSP_OTP

#ifdef SUPPORT_BSP_OTP
#include "bsp_otp.h"
#endif

static bool read(int argc, const char **argv)
{
        uint32_t addr;
        uint32_t data;
        uint32_t len = 1;

        if(argc < 2)
                return false;

        if (!parse_u32(argv[1], &addr)) {
                        return false;
        }

        if(argc > 2) {
                if (!parse_u32(argv[2], &len)) {
                        return false;
                }
        }

        if(len == 1) {
                data = *(uint32_t *)(addr);
                printf("read addr 0x%08x data 0x%08x\n",(unsigned int) addr,(unsigned int) data);
        }
        else {
                printf("0x%08X : ",(unsigned int) addr);
                for(int i = 0; i < (int) len; i++) {
                        data = *(uint32_t *)(addr);
                        printf("%08X ",(unsigned int) data);
                        addr+=4;
                        if((i%8)==7)
                                printf("\n0x%08X : ",(unsigned int) addr+4);
                }
                printf("\n");
        }


        return true;
}

static bool write(int argc, const char **argv)
{
        uint32_t addr;
        uint32_t data;

        if(argc < 3)
                return false;

        if (!parse_u32(argv[1], &addr)) {
                        return false;
        }

        if (!parse_u32(argv[2], &data)) {
                        return false;
        }

        *(uint32_t *)(addr) = data;

        printf("write addr 0x%08x, data 0x%08x\n",(unsigned int) addr,(unsigned int) data);

        return true;
}


const struct {
	char 		*name;
	uint32_t	base;
	uint32_t	len;
} reg_base_list[] = {
	{ "CRG_TOP",		CRG_TOP_BASE, sizeof(CRG_TOP_Type) },
	{ "CRG_SYS",		CRG_SYS_BASE, sizeof(CRG_SYS_Type) },
	{ "CRG_COM",		CRG_COM_BASE, sizeof(CRG_COM_Type) },
	{ "CRG_PER",		CRG_PER_BASE, sizeof(CRG_PER_Type) },

	{ "SCB",			SCB_BASE	, sizeof(SCB_Type) },
	{ "SysTick",		SysTick_BASE, sizeof(SysTick_Type) },
	{ "NVIC",			NVIC_BASE	, sizeof(NVIC_Type) },

	{ NULL,			0			, 0 }
};

static bool regdump(int argc, const char **argv)
{
        uint32_t addr;
        uint32_t data;
        uint32_t len = 1, idx;

        if(argc != 2){

			printf("%s <", argv[0]);

			for(idx = 0; reg_base_list[idx].name != NULL; idx++){
				printf("%s%s", ((idx == 0)?"":"|"), reg_base_list[idx].name);
			}

			printf(">\n");

			return false;
        }

		addr = 0;
		len  = 0;

		for(idx = 0; reg_base_list[idx].name != NULL; idx++){
			if( strcmp(argv[1],reg_base_list[idx].name) == 0 ){
				addr = reg_base_list[idx].base;
				len  = (reg_base_list[idx].len);
			}
		}

		if( (addr != 0) && (len != 0) ){
	        printf("%s : 0x%08X - 0x%08x\n", argv[1],(unsigned int) addr,(unsigned int) len);
		}else{
			printf("%s <", argv[0]);

			for(idx = 0; reg_base_list[idx].name != NULL; idx++){
				printf("%s%s", ((idx == 0)?"":"|"), reg_base_list[idx].name);
			}

			printf("\n");

			return false;
		}

		len = len  / sizeof(uint32_t);

        printf("0x%08X : ",(unsigned int) addr);
        for(int i = 0; i < (int) len; i++) {
                data = *(uint32_t *)(addr);
                printf("%08X ",(unsigned int) data);
                addr+=4;
                if((i%8)==7)
                        printf("\n0x%08X : ",(unsigned int) addr+4);
        }
        printf("\n");

        return true;
}

#ifdef SUPPORT_BSP_OTP
static bool cmd_otp_control(int argc, char *argv[])
{
        uint32_t offset, cnt = 1, i;
        uint32_t value, read_value;
        bsp_otp_init();
        if( strcmp(argv[1], "write") == 0)
        {
                if (argc < 4)
                {
                        printf("otp write [offset] [value]\r\n");
                        return false;
                }
                offset = htoi(argv[2]);
                value = htoi(argv[3]);

                bsp_otp_prog(&value, offset, 1);
                read_value = bsp_otp_word_read(offset);
                printf("offset %08x value %08x read_value %08x \r\n",(unsigned int) offset,(unsigned int) value,(unsigned int) read_value);

        }
        else if( strcmp(argv[1], "read") == 0)
        {
                if (argc < 3)
                {
                        printf("otp read [offset] [length]\r\n");
                        return false;
                }
                offset = htoi(argv[2]);

                if (argc == 4)
                        cnt = htoi(argv[3]);


                for (i = 0; i < cnt; i++)
                {
                        value = bsp_otp_word_read(offset + i);
                        printf("0x%08x : 0x%08x\r\n",(unsigned int)(offset+(i)),(unsigned int) value);
                }
        }
        else if ( strcmp((argv[1]), "lock") == 0)
        {
                if (argc < 3)
                {
                        printf("otp lock [otp_lock_region]");
                }

                value = 0x01 << atoi(argv[2]);
                printf("setting otp lock region 0x%08x\r\n", (unsigned int)value);
                bsp_otp_lock((uint8_t)atoi(argv[2]));
                value = bsp_otp_get_lock_region();
                printf("otp lock region 0x%08x\r\n", (unsigned int)value);
        }
        bsp_otp_close();
        return true;
}
#endif

static const debug_handler_t mem_handlers[] = {
        { "read" , "[addr] <length>", read },
        { "write", "[addr] [data]"  , write },
        { "dump" , "<reg_base>"	, regdump },
#ifdef SUPPORT_BSP_OTP
        { "otp" , "read|write [addr] [length|data]" , (debug_callback_t)cmd_otp_control },
#endif
        { NULL },
};

bool mem_command(int argc, const char *argv[], void *user_data)
{
        (void) user_data;

        return debug_handle_message(argc, argv, mem_handlers);
}
