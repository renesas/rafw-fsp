/**
 ****************************************************************************************
 *
 * @file rm_cli_w_lmac.h
 *
 * @brief LMAC command functions header
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

#ifndef RM_CLI_W_LMAC_H_
#define RM_CLI_W_LMAC_H_

#define LMAC_HW_IDLE 0
#define LMAC_HW_DOZE 2
#define LMAC_HW_ACTIVE 3

bool lmac_command(int argc, const char *argv[], void *user_data);
bool lmac_init(int argc, const char **argv);
bool lmac_start(int argc, const char **argv);
bool lmac_mib(int argc, const char **argv);
bool cmd_lmac_rftx(int argc, const char **argv);
bool lmac_cont_tx_start(int argc, char *argv[]);
bool lmac_cont_tx_stop(int argc, char *argv[]);
void rftx_stop();
bool lmac_rfcw(int argc, const char **argv);
int cmd_lmac_rftxpkt(int argc, const char ** argv);
int cmd_lmac_scale_mode(int argc, const char ** argv);

#endif /* RM_CLI_W_LMAC_H_ */
