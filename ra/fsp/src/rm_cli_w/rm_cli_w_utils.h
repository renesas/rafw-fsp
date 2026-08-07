/**
 * \{
 *
 * \brief Command Line Interface Utilities
 *
 * \{
 */

/**
 ****************************************************************************************
 *
 * @file rm_cli_w_utils.h
 *
 * @brief Declarations for CLI service utilities
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


#ifndef RM_CLI_W_UTILS_H
#define RM_CLI_W_UTILS_H

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "util_api.h"
#if CFG_PMGR
#include "rm_pmgr_w_instance.h"
#include "rm_cli_w_dpm.h"
#endif 

#ifndef   __STATIC_INLINE
 #define __STATIC_INLINE                        static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define dg_configSYS_TCS                           ( 1 )

// atoi error policy
// default leading "0" / "+" / "-0" are not allowed
#define POL_1   1 
// leading "+" / "-0" are not allowed
#define POL_2   2
// clear the terminal screen
#define CLI_CLR_SCREEN  "\e[1;1H\e[2J"

/*
 * \brief Verify if given argument is a number or not
 *
 * \param [in] arg      Argument to check
 * \param [out] v       Argument parsed to a number
 *
 * \return true if argument was properly parsed to a number, false otherwise
 */
__STATIC_INLINE bool verify_num(const char *arg, long *v)
{
    char *check_ptr;
    errno = 0;

    *v = strtol(arg, &check_ptr, 0);

    if (errno == ERANGE) {
        return false;
    }

    return (*arg != '\0' && *check_ptr == '\0');
}

/*
 * \brief Verify if given argument is a non-negative number or not
 *
 * \param [in] arg      Argument to check
 * \param [out] v       Argument parsed to a number
 *
 * \return true if argument was properly parsed to a non-negative number, false otherwise
 */
__STATIC_INLINE bool verify_non_neg_num(const char *arg, unsigned long long *v)
{
    char *check_ptr;
    errno = 0;

    /*
     * Check if the argument doesn't include '-' character at the first position, that informs
     * that this is a negative number
     */
    if (arg[0] == '-') {
            return false;
    }

    *v = strtoull(arg, &check_ptr, 0);

    if (errno == ERANGE) {
        return false;
    }

    return (*arg != '\0' && *check_ptr == '\0');
}

/*
 * \brief Parse argument to uint64_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_u64(const char *arg, uint64_t *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    *val = (uint64_t)buf;
    return true;
}

/*
 * \brief Parse argument to uint32_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_u32(const char *arg, uint32_t *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    if (buf > ULONG_MAX) {
            return false;
    }

    *val = (uint32_t)buf;
    return true;
}

/*
 * \brief Parse argument to uint16_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_u16(const char *arg, uint16_t *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    if (buf > USHRT_MAX) {
            return false;
    }

    *val = (uint16_t)buf;
    return true;
}

/*
 * \brief Parse argument to uint8_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_u8(const char *arg, uint8_t *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    if (buf > UCHAR_MAX) {
            return false;
    }

    *val = (uint8_t)buf;
    return true;
}

/*
 * \brief Parse argument to int16_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_16(const char *arg, int16_t *val)
{
    long buf = 0;

    if (!verify_num(arg, &buf)) {
            return false;
    }

    if (buf > SHRT_MAX) {
            return false;
    }

    *val = (int16_t)buf;
    return true;
}

/*
 * \brief Parse argument to bool
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a boolean value
 *
 * \return true if parsed correctly and number has proper value, false otherwise
 */
__STATIC_INLINE bool parse_bool(const char *arg, bool *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    /* Valid values are only 0 (false) or 1 (true) */
    if (buf > 1) {
            return false;
    }

    *val = (bool)buf;
    return true;
}

/*
 * \brief Parse argument to size_t
 *
 * \param [in] arg      Argument to parse
 * \param [out] val     Argument parsed to a number
 *
 * \return true if parsed correctly, false otherwise
 */
__STATIC_INLINE bool parse_size_t(const char *arg, size_t *val)
{
    unsigned long long buf;

    if (!verify_non_neg_num(arg, &buf)) {
            return false;
    }

    if (sizeof(size_t) == 4 && buf > ULONG_MAX) {
            return false;
    }

    *val = (size_t)buf;
    return true;
}

__STATIC_INLINE  unsigned char toint(char c)
{
    unsigned char rslt;

    if ( (c >= '0') && (c <= '9') ){
            rslt = (c-'0');
    } else if ( (c >= 'a') && (c <= 'f') ) {
            rslt = (c-'a'+(unsigned char)10);
    } else if ( (c >= 'A') && (c <= 'F') ) {
            rslt = (c-'A'+(unsigned char)10);
    } else {
            rslt =  (unsigned char)0;
    }

    return rslt;
}

__STATIC_INLINE  unsigned int htoi(char *s)
{
    unsigned int sum = 0;

    while ((*s >= '0') && (*s <= 'f')){
            sum = sum * 16 + toint(*s++);
    }

    return(sum);
}

__STATIC_INLINE unsigned int ctoi(char *s)
{
	unsigned int sum = 0;

	while((*s >= '0') && (*s <= '9'))
		sum = sum * 10 + toint(*s++);

	return sum;
}

#ifdef __cplusplus
}
#endif

/*
 ****************************************************************************************
 * @brief      Show FreeRTOS task lists
 * @param[in]  None
 * @return     None
 ****************************************************************************************
 */
void show_task_list(void);
extern bool reset(void);

int  cli_atoi_custom (char *str);
int  cli_get_int_val_from_str(char *param, int *int_val, int policy);
int  cli_get_int_val_from_date_time_str(char *param, int *int_val);
int  cli_is_date_time_valid(struct tm *t);

#if CMD_DPMD
bool dpmd_command(int argc, const char *argv[], void *user_data);
#endif // CMD_DPMD
bool mem_command(int argc, const char *argv[], void *user_data);
bool os_command(int argc, const char *argv[], void *user_data);
#if CFG_PMGR
bool pmgr_command(int argc, const char *argv[], void *user_data);
#endif /* CFG_PMGR */

void print_version(void);
void clear_wakeup_src_all(void);
void reboot_func(unsigned int flag);
int cmd_set_time(char *date_format, char *time_format, int daylight);
bool root_command_handlers(int argc, const char *argv[], void *user_data);
void help_root_cmd(void);

bool nvram_command(int argc, const char *argv[], void *user_data);

/*
 * External global variables
 */

/*
 * External global functions
 */

#if CFG_PMGR
#if (dg_configUSE_RETENTION_MEM_INFO == 1)
extern bool cmd_rtm_info(int argc, char *argv[]);
#endif	// dg_configUSE_RETENTION_MEM_INFO
#endif /* CFG_PMGR */

extern bool cmd_regdb(int argc, char *argv[]);
extern void easy_setup_task(void * pvPraram);
extern void hal_machw_stop(void);


#endif /* RM_CLI_W_UTILS_H */

/**
 * \}
 * \}
 */



/* EOF */
