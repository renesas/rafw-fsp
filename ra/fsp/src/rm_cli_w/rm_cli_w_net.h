/**
 ****************************************************************************************
 *
 * @file rm_cli_w_net.h
 *
 * @brief net command functions header
 *
 * Copyright (c) 2023-2024 Renesas Electronics. All rights reserved.
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

#ifndef RM_CLI_W_NET_H_
#define RM_CLI_W_NET_H_

/* for ra6w1_wpa_cli(). */
#if CFG_WIFI
//#include "rm_wifi_helper.h"

bool net_command(int argc, const char *argv[], void *user_data);
void print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count);

UINT isVaildDomain(char *domain);
#ifdef __SUPPORT_IPV4__
int isvalidIPsubnetInterface(long ip, int iface);
#endif /* __SUPPORT_IPV4__ */
#if defined ( __SUPPORT_SNTP_CLIENT__ )
bool cmd_network_sntp(int argc, char *argv[]);
#endif /* __SUPPORT_SNTP_CLIENT__ */
int get_current_rssi(void);
int make_message(char *title, char *buf, size_t buflen);
int cert_rwds(char action, char dest);

#if defined ( __SUPPORT_NSLOOKUP__ )
extern bool	cmd_nslookup(int argc, char *argv[]);
#endif	// __SUPPORT_NSLOOKUP__

extern int request_start_supplicant(void);
extern int request_stop_supplicant(void);
extern void set_ent_cert_verify_flags(char flag, int value);
extern int set_sys_mode(int mode);
extern void umac_set_debug(u8 d_on, u16 d_msk, u8 flag);
extern void wifi_netif_control(int intf, int flag);
extern int getStr(char *get_data, int get_len);
#endif
#endif /* RM_CLI_W_NET_H_ */
