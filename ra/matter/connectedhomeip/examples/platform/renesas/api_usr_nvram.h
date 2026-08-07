/**
****************************************************************************************
*
* @file api_usr_nvram.h
*
* @brief User NVRAM API
*
* Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
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
#if defined(__SUPPORT_USR_NVRAM__)
void api_usr_nvram_init(void);
void api_usr_nvram_bank_status(uint8_t all);
void api_usr_nvram_bank_reset(uint8_t all);

int api_usr_nvram_read_int(const char *name, int32_t *_val);
char *api_usr_nvram_read_string(const char *name);
uint8_t *api_usr_nvram_read_binary(const char *name, uint16_t *size);

int api_usr_nvram_write_int(const char *name, int32_t val);
int api_usr_nvram_write_string(const char *name, const char *val);
int api_usr_nvram_write_binary(const char *name, const char *val, uint16_t size);

int32_t api_usr_nvram_delete_item(const char *name);

int api_usr_nvram_delete_item_tmp(const char *name);
int api_usr_nvram_save_tmp(void);

#endif // __SUPPORT_USR_NVRAM__
