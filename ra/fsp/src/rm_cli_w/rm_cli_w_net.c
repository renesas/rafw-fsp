/**
 ****************************************************************************************
 *
 * @file rm_cli_w_net.c
 *
 * @brief RTOS command functions
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

#include "FreeRTOS.h"
#include "rm_wifi.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#if CFG_WIFI
#include "rm_cli_w_utils.h"
#include "rm_cli_w_debug_utils.h"
#include "sdk_defs.h"
#include "task.h"
#include "rm_lwiperf.h"
#include "iface_defs.h"
#if LWIP_ARP
#include "lwip/etharp.h"
#endif /*LWIP_ARP*/
#include "net_arp.h"
#include "rm_cli_w_net_mqtt_client.h"
#include "lwipopts.h"
#include "lwip/stats.h"

#include "sys_feature.h"
#include "common_def.h"
#include "lwip/err.h"
#include "lwip/stats.h"
#include "rm_cert.h"
#if defined (__SUPPORT_ATCMD_TLS__)
#include "rm_atcmd_w_core_socket_cert_mng.h"
#endif
#include "dhcpserver.h"
#include "net_network_main.h"
#include "lwip/dhcp.h"
#include "net_dns_client.h"
#include "net_ip_handler.h"

#include "net_sntp_client.h"
#include "rm_cli_w.h"
#include "rm_cli_w_net.h"
#include "net_dhcp_server.h"
#include "rm_sntp.h"

#if defined (__SUPPORT_OTA__)
#include "ota_update.h"
#include "ota_update_common.h"
#include "ota_update_http.h"
#endif 

#ifdef SIGMA_TEST_ENABLE
#include "traffic_generator.h"
#endif

#include "rm_lwip_w_helper.h"
#include "lwip/ip_addr.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#include "rm_wifi_helper.h"

#if defined ( __SUPPORT_IPERF3__ )
#include "lwiperf3.h"
#endif //(__SUPPORT_IPERF3__)

#if (TCP_CLIENT_APP_START == 1)
#include "tcp_client.h"
#endif
#include "rm_dhcp.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

/* CMD LIST */
#define CMD_GETWLANMAC       "getwlanmac"
#define CMD_SETWLANMAC       "setwlanmac"
#define CMD_SETOTPMAC        "setotpmac"
#define CMD_MACSPOOFING      "macspoofing"

extern fsp_err_t rm_stdio_w_locked_write(const char * p_str);
extern int get_ent_cert_verify_flags(void);
extern bool cmd_dns2cache(int argc, char *argv[]);
#if (SUPPORT_FSP_RM_OTA_W == 1)
#if defined (__SUPPORT_OTA__)
extern bool cmd_ota_update(int argc, char *argv[]);
#endif /*__SUPPORT_OTA__*/
#endif
bool cmd_network_config_wrapper(int argc, char *argv[]);
bool cmd_supp_start(int argc, char *argv[]);
bool cmd_supp_stop(int argc, char *argv[]);

/**
 * ex)
 * print_separate_bar("=", 10, 2);
 *
 * "==========\n\n"
 *
 */
void BSP_WEAK_REFERENCE print_separate_bar(unsigned char text, unsigned char loop_count, unsigned char CR_loop_count)
{
    unsigned char prt_str[260];

    memset(prt_str, 0, 256);

    if ((loop_count + CR_loop_count) + 1 > 260) {
        loop_count = (unsigned char)(260 - (CR_loop_count - 1));
    }

    memset(prt_str, text, loop_count);

    if (CR_loop_count > 0) {
        memset(prt_str + loop_count, '\n', CR_loop_count);
    }
    printf("%s", prt_str);
#if defined ( SIGMA_TEST_ENABLE )
    PRINT_SIGMA_CMD("%s", prt_str);
#endif  // SIGMA_TEST_ENABLE
}

UINT isVaildDomain(char *domain)
{
    if (strchr(domain, '.') != NULL) {
        return TRUE;
    }

    return FALSE;
}

static bool cmd_get_wlaninit(int argc, const char **argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    ra6w1_network_main_print_wlaninit_mode();

    return pdTRUE;
}

static bool cmd_set_wlaninit(int argc, char *argv[])
{
    if (argc != 2) {
        goto usage_msg;
    }

    if (   argc == 2
        && strlen(argv[1]) == 1
        && (ctoi(argv[1]) == 1 || ctoi(argv[1]) == 0)) {
        ra6w1_network_main_set_wlaninit_mode(ctoi(argv[1]));
        ra6w1_network_main_print_wlaninit_mode();
        return pdTRUE;
    }

usage_msg:
    printf("\nUsage : setwlaninit [0|1]\n"
           "\t0 : Disable WLAN init.\n"
           "\t1 : Enable WLAN init.\n");

    return pdTRUE;
}

static bool cmd_run_wlaninit(int argc, char *argv[])
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    if (argc == 1) {
        ra6w1_network_main_init_wlan_with_task();
    } else if (argc == 2 && strcasecmp(argv[1], "status") == 0) {
        if (ra6w1_network_main_is_wlaninit()) {
            printf("Wi-Fi initialization has been done.\n");
        } else {
            printf("Wi-Fi initialization is not doen.\n");
        }
    } else {
        return pdFALSE;
    }

    return pdTRUE;
}

static bool cmd_get_sys_mode(int argc, const char **argv)
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    print_sys_mode((UINT)(get_sys_mode()));
    return pdTRUE;
}


#ifndef ISDIGIT
#define ISDIGIT(c)( (c < '0' || c > '9') ? 0 : 1)
#endif
static bool cmd_set_sys_mode(int argc, char *argv[])
{
    if (argc == 1) {
        goto usage_msg;
    }

    if (   argc == 2
            && ISDIGIT(argv[1][0])
            && (strlen(argv[1]) == 1)
            && atoi(argv[1]) >= WIFI_DEVICE_MODE_EXT_STATION
#ifdef __SUPPORT_P2P__
            && atoi(argv[1]) <= WIFI_DEVICE_MODE_EXT_P2P_STATION
#else
            && atoi(argv[1]) <= WIFI_DEVICE_MODE_EXT_AP_STATION
#endif /* __SUPPORT_P2P__ */
       ) {

        if (set_sys_mode(atoi(argv[1])) == 0) {
            print_sys_mode(ctoi(argv[1]));
            return pdTRUE;
        }
    }

usage_msg :
    print_sys_mode(get_sys_mode());
#ifdef __SUPPORT_P2P__
    printf("\nUsage : setsysmode [0|1|2|3|4|5]\n"
           "\t0 : Station\n"
           "\t1 : Soft-AP\n"
           "\t2 : WiFi Direct Mode\n"
           "\t3 : WiFi Direct P2P GO Fixed Mode\n"
           "\t4 : Station & Soft-AP\n"
           "\t5 : Station & WiFi Direct\n");
#else
    printf("\nUsage : setsysmode [0|1|2]\n"
           "\t0 : Station\n"
           "\t1 : Soft-AP\n"
           "\t2 : Station & Soft-AP\n");
#endif /* __SUPPORT_P2P__ */
    return pdTRUE;
}

static bool cmd_wpa_cli(int argc, const char **argv)
{
    if (argc < 2) {
        return pdFALSE;
    }

    ra6w1_wpa_cli(argc, (char **)argv);

    return pdTRUE;
}

#ifdef SIGMA_TEST_ENABLE
bool cmd_wpa_cli_wrapper(int argc, const char **argv)
{
    cmd_wpa_cli(argc, argv);

    return pdTRUE;
}
#endif
/* debug cmd */
static bool cmd_debug(int argc, char *argv[])
{
    if (argc == 1 || (strcasecmp(argv[1], "HELP") == 0)) {
        printf("debug [option]\n");
        printf("\tarp [on|off]\n");

#ifdef __CMD_DHCPD_RX_MSG_DROP__
#ifdef __SUPPORT_DHCP_SVR__
        printf("\tdhcpd_drop [option]\n");
#endif /* __SUPPORT_DHCP_SVR__ */
#endif /* __CMD_DHCPD_RX_MSG_DROP__ */

        printf("\tdhcpc [level] level=0~6 defaut 1\n");
        printf("\tumac on|off mask\n"
               "\t\te.g. debug umac 1 0x4\n");

        printf("\twpa_cli [on|off]\n");
        printf("\tsupp_level [level]\n");
#ifdef DEBUG_SAMPLE
        printf("\tsample [on|off]\n");
#endif /* DEBUG_SAMPLE */
    } else if (strcasecmp(argv[1], "WPA_CLI") == 0 && (argc == 3)) {
        extern char dbg_wpa_cli;

        if (strcasecmp(argv[2], "ON") == 0) {
            dbg_wpa_cli = 1;
        } else {
            dbg_wpa_cli = 0;
        }

        printf("Debug WPA_CLI : %s\n", dbg_wpa_cli ? "On" : "Off");
    } else if (strcasecmp(argv[1], "ARP") == 0 && (argc == 3)) {
#ifdef DEBUG_ARP

        if (strcasecmp(argv[2], "ON") == 0) {
            set_debug_arp(1);
        } else {
            set_debug_arp(0);
        }

        printf("Debug ARP : %s\n", get_debug_arp() ? "On" : "Off");
#endif    /* DEBUG_ARP */
#ifdef __CMD_DHCPD_RX_MSG_DROP__
    } else if (strcasecmp(argv[1], "DHCPD_DROP") == 0) {
        if ( argc == 3
                && (argv[2][0] == '1'
                    || argv[2][0] == '3'
                    || argv[2][0] == '4'
                    || argv[2][0] == '7'
                    || argv[2][0] == '8'
                    || argv[2][0] == '0')) {
            set_debug_dhcpd_msg_drop((UCHAR)ctoi(argv[2]));
            printf("DHCPD_DROP status = %u\n", get_debug_dhcpd_msg_drop());
        } else {
            printf("DHCPD_DROP status = %u\n", get_debug_dhcpd_msg_drop());
            /* Define the DHCP Message Types.  */
            printf("\nDHCPD_DROP [option]\n"
                   "\t[option]\n"
                   "\tDHCP_DISCOVER 1\n"
                   "\tDHCP_REQUEST    3\n"
                   "\tDHCP_DECLINE    4\n"
                   "\tDHCP_RELEASE    7\n"
                   "\tDHCP_INFORM    8\n"
                   "\tDisable        0\n");
        }

#endif /* __CMD_DHCPD_RX_MSG_DROP__ */
#ifdef __SUPPORT_IPV4__
#if LWIP_DHCP
    } else if (strcasecmp(argv[1], "DHCPC") == 0
               && argc == 3
               && argv[2][0] >= '0'
               && argv[2][0] <= '6') {
        set_debug_dhcpc((u8_t)ctoi(argv[2]));
        printf("\nDHCP CLIENT Debug Level : %d\n", get_debug_dhcpc());
#endif /* LWIP_DHCP */
#endif // __SUPPORT_IPV4__
    } else if (strcasecmp(argv[1], "UMAC") == 0) {
        if (argc == 4) {
            umac_set_debug( (u8)ctoi(argv[2]),            // d_on
                            (u16)htoi(argv[3]), (u8)1 );        // d_msk
            return pdTRUE;
        } else if ( argc == 3 && ctoi(argv[2]) ==  0 ) {
            umac_set_debug(0, 0, 1);
            return pdTRUE;
        } else {
            printf(
                "\n"
                "Usage: debug umac is_on dbg_mask(Hexa)\n"
                "e.g.    debug umac 1 0x4 /* UM_RX */\n"
                "        debug umac 0     /* Off   */\n"
                "dbg_mask info:\n"
                "  UM_TX      0x01\n"
                "  UM_TX2LM   0x02\n"
                "  UM_RX      0x04\n"
                "  UM_RX_NI   0x08\n"
                "  UM_SC      0x10\n"
                "  UM_CN      0x20\n"
                "  UM_MF      0x40\n"
                "\n");
            return pdTRUE;
        }

#ifdef FEATURE_P2P_M_ABS_CTW_VAL
    } else if (strcasecmp(argv[1], "P2P_GO_PS") == 0) {

        if (strcasecmp(argv[2], "SET") == 0) {
            int r;
            //check absence value.
            r = umac_set_p2p_go_noa_ctw(ctoi(argv[3]));

            if (!r) {
                return pdTRUE;
            } else {
                //print error #
                printf("Failure of set_p2p_go_noa_ctw :Error#%d\n", r);
                return pdFALSE;
            }
        } else if (strcasecmp(argv[2], "GET") == 0) {
            int a_noa_ctw[2];

            //read values from mode params and p2p_env.
            umac_get_p2p_go_noa_ctw(a_noa_ctw);
            printf("Current absent period:%d ms & CT window:%d ms\n"
                   , a_noa_ctw[0], a_noa_ctw[1] );
            return pdTRUE;
        } else {
            //print usage and inform 3rd arg is wrong.
            printf(
                "\n"
                "Usage: debug p2p_go_ps set|get #\n"
                "e.g.    debug p2p_go_ps set 40    <= Set absent period to 40ms\n"
                "        debug p2p_go_ps get \n"
                "\n");
            return pdTRUE;
        }

#endif //FEATURE_P2P_M_ABS_CTW_VAL
    } else if (strcasecmp(argv[1], "supp_level") == 0) {
        extern int wpa_debug_level;

        if (argc == 3) {
            wpa_debug_level = ctoi(argv[2]);
        } else {
            printf("\tcurrent supp_level=%d\n", wpa_debug_level);
            printf("\t  0:EXCESSIVE\n\t  1:MSGDUMP\n\t  2:DEBUG\n\t  3:USER_DEBUG\n\t  4:INFO\n\t  5:WARNING\n\t  6:ERROR\n");

        }
#ifdef DEBUG_SAMPLE
    } else if (strcasecmp(argv[1], "SAMPLE") == 0) {

        if (strcasecmp(argv[2], "ON") == 0) {
            ; /* ON */
        } else {
            ; /* OFF */
        }

        printf("Debug SAMPLE : %s\n", get_debug_arp() ? "On" : "Off");
#endif /* DEBUG_SAMPLE */
    } else {
        printf("Error: Debug option\n");
    }

    return pdTRUE;
}

#if defined(SIGMA_TEST_ENABLE)
static void print_tg_setprofile()
{
    printf(
        "Usage:\n"
        "tg_setprofile [profile] [stream_id] <direction> <dst-ip> <dst-port> <src-ip>\n\r"
        "\t\t<src-port> <rate> <duration> <pkt-size> <class>\n\r"
        "\t\t<start-delay> <max-count> <dscp> <hti> <protocol> <burst-frag>\n\r"
        "\t\t<burst-period>\n\r"
        "\tprofile     : Profile to be used\n\r"
        "\t\t[File_Transfer, Multicast, IPTV, Transaction, Start_Sync, UAPSD, Burst]\n\r"
        "\tstream_id   : A unique ID for traffic\n\r"
        "\tdirection   : traffic agent direction, [Send(Tx), Receive(Rx)]\n\r"
        "\tdst-ip      : IP of receive agent\n\r"
        "\tdst-port    : Port of receiver agent\n\r"
        "\tsrc-ip      : IP of source agent\n\r"
        "\tsrc-port    : Port of source agent\n\r"
        "\trate        : Frames per second, [0=maximum-possible]\n\r"
        "\tduration    : Number of seconds to send packets\n\r"
        "\tpkt-size    : Transport payload length in bytes, [0-1450]\n\r"
        "\tclass       : Traffic class, [Voice, Video, Background, Besteffort]\n\r"
        "\tstart-delay : Delay after send command to start stream\n\r"
        "\tmax-count   : Maximum count before stopping stream\n\r"
        "\tdscp        : Differentiated Services Code Point\n\r"
        "\thti         : HT throughput flag, [On, Off]\n\r"
        "\tprotocol    : Protocol to be used, [0=UDP(default), 1=TCP]\n\r"
        "\tburst-frag  : Number of packets to be send in a burst\n\r"
        "\tburst-period: Periodicity of the burst in milli seconds\n\r"
        "\n"
    );
}

/**
 tg_setprofile [profile] [stream_id] <direction> <dst-ip> <dst-port> <src-ip>
 <src-port> <rate> <duration> <pkt-size> <class>
 <start-delay> <max-count> <dscp> <hti> <protocol> <burst-frag>
 <burst-period>
*/
static bool cmd_tg_setprofile(int argc, char *argv[])
{
    int i, stream_id;
    struct tg_session *session = NULL;
    ip4_addr_t tmp_addr;

    if (argc != 19)
        goto pexit;

    stream_id = atoi(argv[2]);

    for (i = 0; i < MAX_PROF; i++) {
        if (g_tg_info.tg_stream.session[i].config.profile.stream_id == stream_id ||
            g_tg_info.tg_stream.session[i].task_info.run_state == TG_TEST_STOPPED) {
            session = tg_get_stream_session(i);
            break;
        }
    }

    if (!session) {
        printf("Invalid stream ID\n\r");
        return pdFALSE;
    }

    session->config.profile.stream_id = stream_id;
    session->config.profile.profile = tg_get_profile_idx(argv[1]);
    if (!(session->config.profile.profile)) {
        fprintf(stderr, "STREAM: Invalid profile spec!\n\r");
        return pdFALSE;
    }

    session->config.profile.direction = atoi(argv[3]);

    ipaddr_aton(argv[4], (ip_addr_t *)&tmp_addr);
    if (is_in_valid_ip_class(argv[4]) || ip4_addr_ismulticast(&tmp_addr)) {    /* IP */
        ip4addr_aton(argv[4], (ip4_addr_t *)&session->config.profile.d_ipaddr);
    } else {
        fprintf(stderr, "STREAM: Invalid Dest IP provided!\n");
        return pdFALSE;
    }

    session->config.profile.d_port = atoi(argv[5]);
    if (is_in_valid_ip_class(argv[6])) {    /* IP */
        ip4addr_aton(argv[6], (ip4_addr_t *)&session->config.profile.s_ipaddr);
    } else {
        fprintf(stderr, "STREAM: Invalid Src IP provided!\n");
        //return pdFALSE;
    }

    session->config.profile.s_port = atoi(argv[7]);
    session->config.profile.rate = atoi(argv[8]);
    if (session->config.profile.rate < 1)
        session->config.profile.rate = TG_MAX_RATE;
    session->config.profile.duration = atoi(argv[9]);
    if (session->config.profile.duration < 1)
        session->config.profile.duration = TG_MAX_DURATION;

    session->config.profile.pkt_sz = atoi(argv[10]);
    if (!(session->config.profile.pkt_sz >= 0 &&
            session->config.profile.pkt_sz <= TG_MAX_MTU_SIZE)) {
        fprintf(stderr, "STREAM: Invalid MTU Size! !(%d < %d < %d)\n",
                0, session->config.profile.pkt_sz, (TG_MAX_MTU_SIZE + 1));
        return pdFALSE;
    }

    if (!session->config.profile.pkt_sz)
        session->config.profile.pkt_sz = TG_MAX_MTU_SIZE;

    session->config.profile.traffic_class = atoi(argv[11]);
    if (!(session->config.profile.traffic_class)) {
        fprintf(stderr, "STREAM: Invalid QOS type!\n");
        return pdFALSE;
    }

    session->config.profile.start_delay = atoi(argv[12]);
    session->config.profile.max_cnt = atoi(argv[13]);
    session->config.profile.dscp = atoi(argv[14]);
    session->config.profile.hti = atoi(argv[15]);
    if (!(session->config.profile.hti)) {
        fprintf(stderr, "STREAM: Invalid hti value!\n");
        return pdFALSE;
    }

    session->config.profile.proto = atoi(argv[16]);
    session->config.profile.burst_frag = atoi(argv[17]);
    session->config.profile.burst_period = atoi(argv[18]);

    session->task_info.run_state = TG_TEST_READY;

    tg_throughput_eval_and_print_config(session);

    return pdTRUE;

pexit:
    print_tg_setprofile();

    return pdTRUE;
}

static bool cmd_tg_test(int argc, char *argv[])
{
    int i, j, action = TG_TEST_STOP;
    int p_idx;

    if (argc > 2 && !strcmp("start", argv[1])) {
        action = TG_TEST_RUN;
    } else if (argc > 2 && !strcmp("stop", argv[1])) {
        action = TG_TEST_STOP;
    } else if (argc > 1 && !strcmp("reset", argv[1])) {
        tg_throughput_test(TG_TEST_RESET, 0);
    } else {
        PRINTF("Invalid option %s\n", argv[1]);
        PRINTF("tg_test [start|stop|reset] <pid>\n");
        return pdFALSE;
    }

    // start from argv[2]
    for(j = 2; j < argc ; j++) {
        p_idx = -1;
        for (i = 0; i < MAX_PROF; i++) {
            if (g_tg_info.tg_stream.session[i].config.profile.stream_id ==
                    atoi(argv[j]))
                p_idx = i;
        }

        tg_throughput_test(action, p_idx);
    }

    return pdTRUE;
}
bool tg_setprofile(int argc, char *argv[])
{
    return cmd_tg_setprofile(argc, argv);
}

bool tg_test(int argc, char *argv[])
{
    return cmd_tg_test(argc, argv);
}

static void print_tg_ping_help_text( void )
{
#if defined(SIGMA_TEST_ENABLE)
    PRINT_SIGMA_CMD(
        "Usage:\n"
        "tg_ping start <ip> <dscp> <duration> <frame-rate> <ip-type> "
        "<tos> <destmac> <cvid> <svid>\n"
        "tg_ping stop <stream_id>\n"
        "\tip        : IP(v4/v6) address to ping\n"
        "\tdscp      : Differentiated Services Code Point value for IP package priority\n"
        "\tduration  : Number of seconds to ping [0-inf, >0]\n"
        "\tframe-rate: Number of ping to send each second [1-100]\n"
        "\tframe-size: Length of ping data [1-1024]\n"
        "\tip-type   : IP protocol version. 1=IPv4 2=IPv6\n"
        "\ttos       : Type of Service value for IP packet priority\n"
        "\tdestmac   : MAC address to send the ping to\n"
        "\tcvid      : VLAN identifier\n"
        "\tsvid      : VLAN identifier\n\n");
#endif
}

static bool cmd_tg_ping(int argc, char *argv[])
{

    struct tg_session *session;

    if (argc == 12 && !strcmp("start", argv[1])) { 	/* start */
        if (g_tg_info.tg_ping.active_count < MAX_PROF + 1) {
            session = tg_get_ping_session(g_tg_info.tg_ping.active_count);
            memset(session, 0, sizeof(*session));

            if (is_in_valid_ip_class(argv[2])) { 	/* IP */
                ip4addr_aton(argv[2], (ip4_addr_t *)&session->config.ping.param.ipaddr);
            } else {
                fprintf(stderr, "%s: PING: Invalid IP provided!\n", __func__);
                return pdFALSE;
            }

            if (atoi(argv[3]) > 0) {	/* DSCP */
                session->config.ping.dscp = atoi(argv[3]);
            } else {
                fprintf(stderr, "%s: PING: Invalid DSCP provided! [%d]\n",
                        __func__, atoi(argv[3]));
                return pdFALSE;
            }

            if (atoi(argv[4]) >= 0) {		/* Duration */
                session->config.ping.duration = atoi(argv[4]);
                if (!session->config.ping.duration)
                    session->config.ping.duration = 60 * 60 * 24; /* 1 day - max */
            } else {
                fprintf(stderr, "%s: PING: Invalid Duration provided! [%d]\n",
                                        __func__, atoi(argv[4]));
                return pdFALSE;
            }

            if (atof(argv[5]) > 0.0 && atof(argv[5]) <=
                    (float)(DEFAULT_INTERVAL / MIN_INTERVAL)) {		/* frame rate */
                float frame_rate = atof(argv[5]);
                session->config.ping.param.interval = (int)((1.0f * 1000.0f) / frame_rate);
            } else {
                fprintf(stderr, "%s: PING: Invalid frame rate provided! [%d]\n",
                                        __func__, atoi(argv[5]));
                return pdFALSE;
            }

            if (atoi(argv[6]) > 0 && atoi(argv[6]) <= TG_MAX_PING_SIZE) { /* frame len */
                session->config.ping.param.len = atoi(argv[6]);
            } else {
                fprintf(stderr, "%s: PING: Invalid frame size provided! [%d]\n",
                                        __func__, atoi(argv[6]));
                return pdFALSE;
            }

            session->config.ping.param.max_count =
                    session->config.ping.duration * 1000 / session->config.ping.param.interval;
            session->config.ping.param.wait = DEFAULT_PING_WAIT;
            session->config.ping.param.ping_interface = WLAN0_IFACE;

            TG_PRINTF("PING: STREAM_ID=%d\n", g_tg_info.tg_ping.active_count);
            tg_ping_test(TG_TEST_RUN, g_tg_info.tg_ping.active_count);
            g_tg_info.tg_ping.active_count++;
        } else {
            fprintf(stderr, "%s: PING: Max session already running\n", __func__);
            return pdFALSE;;
        }
    } else if (argc == 3 && !strcmp("stop", argv[1])) {
        int index = atoi(argv[2]);

        if (index >= 0 && index < MAX_PROF) {		/* Id */
            if (g_tg_info.tg_ping.active_count > 0) {
                tg_ping_test(TG_TEST_STOP, index);
                g_tg_info.tg_ping.active_count--;
            } else {
                fprintf(stderr, "%s: PING: No session running! [%d]\n",
                        __func__, index);
            }
        } else {
            fprintf(stderr, "%s: PING: Invalid index provided! [%d]\n",
                                    __func__, index);
            return pdFALSE;
        }
    } else {
        goto pexit;
    }

    return pdTRUE;

pexit:
    print_tg_ping_help_text();

    return pdFALSE;
}

bool tg_ping(int argc, char *argv[])
{
    return cmd_tg_ping(argc, argv);
}
#endif //# defined(SIGMA_TEST_ENABLE)

static bool cmd_getWLANMac(int argc, char *argv[])
{
    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    int type;
    char macstr[18];

    for (int iface = 0; iface <= WLAN1_IFACE; iface++) {
        memset(macstr, 0, 18);

        type = rm_wifi_get_mac_address_string(iface, macstr, 1);

        if (type < 0) {
            printf("Read Error : WLAN MAC\n");
        } else {
            printf("WLAN%d - %s ", iface, macstr);
            switch (type) {
            case MAC_SPOOFING:
                printf("(MAC Spoofing)\n");
                break;

            case NVRAM_MAC:
                printf("(NVRAM MAC)\n");
                break;

            case OTP_MAC:
                printf("(OTP MAC idx=%d)\n", bsp_tcs_otp_mac_cnt());
                break;
            }
        }
    }
    return pdTRUE;
}


/*
setwlanmac
setotpmac
macspoofing
*/

extern int getStr(char *get_data, int get_len);
static int compare_macaddr(char* macaddr1, char* macaddr2)
{
    int len1, len2;
    char tmp_macstr1[13];
    char tmp_macstr2[13];

    len1 = strlen(macaddr1);
    len2 = strlen(macaddr2);



    if (len1 != 12 && len1 != 17 && len2 != 12 && len2 != 17) {
        return 0;
    }

    if (len1 == 17) {
        memset(tmp_macstr1, 0, 13);
        sprintf(tmp_macstr1, "%c%c%c%c%c%c%c%c%c%c%c%c",
            macaddr1[0],  macaddr1[1],
            macaddr1[3],  macaddr1[4],
            macaddr1[6],  macaddr1[7],
            macaddr1[9],  macaddr1[10],
            macaddr1[12], macaddr1[13],
            macaddr1[15], macaddr1[16]);
    } else {
        bsp_safe_strcpy(tmp_macstr1, macaddr1, sizeof(tmp_macstr1));
    }

    if (len2 == 17) {
        memset(tmp_macstr2, 0, 13);
        sprintf(tmp_macstr2, "%c%c%c%c%c%c%c%c%c%c%c%c",
            macaddr2[0],  macaddr2[1],
            macaddr2[3],  macaddr2[4],
            macaddr2[6],  macaddr2[7],
            macaddr2[9],  macaddr2[10],
            macaddr2[12], macaddr2[13],
            macaddr2[15], macaddr2[16]);
    } else {
        bsp_safe_strcpy(tmp_macstr2, macaddr2, sizeof(tmp_macstr2));
    }

    if (strncasecmp(tmp_macstr1, tmp_macstr2, 12) == 0) { /* Same Mac Address */
        return 1;
    }

    return 0;
}


#define OTP_MAC_AVAILABLE 4
static bool cmd_setWLANMac(int argc, char *argv[])
{
    UINT status = E_WRITE_ERROR;

    if (argc == 1) {
        printf("Usage: %s [xx:xx:xx:xx:xx:xx | xx-xx-xx-xx-xx-xx | xxxxxxxxxxxx%s]\n\t(for Station Only)\n",
               argv[0], strcmp(argv[0], "setotpmac") != 0 ? " | erase":"");
        return pdTRUE;
    }

    if (argc == 2) {
    	size_t len = strlen(argv[1]);

        if (strcmp(argv[0], CMD_SETWLANMAC) == 0) {
            status = rm_wifi_write_mac_address(argv[1], NVRAM_MAC);
        } else if (strcmp(argv[0], CMD_MACSPOOFING) == 0) {
            status = rm_wifi_write_mac_address(argv[1], MAC_SPOOFING);
        }
        else if (strcmp(argv[0], CMD_SETOTPMAC) == 0) {
            int     otp_mac_index = -1;
            char    macaddr_otp_str[13];
            char    tmp_macstr[18];

            memset(macaddr_otp_str, 0, 13);
            memset(tmp_macstr, 0, 18);

            if (len != 17 && len != 12) {
                status = E_UNKNOW;
                goto msg_error;
            }

            otp_mac_index = bsp_tcs_otp_mac_cnt();

            uint32_t mac_addr[2] = {0, };

            if (bsp_tcs_otp_read_mac(mac_addr)) {
                sprintf(macaddr_otp_str, "%04x%08x", (unsigned int)(mac_addr[1] & 0xffff), (unsigned int)mac_addr[0]);
            }

            /* Read current OTP MAC */
            if (otp_mac_index != -1 && otp_mac_index < OTP_MAC_AVAILABLE) {
                printf("\n[Current OTP MAC]\n\tOTP MAC Index: %d of %d\n", otp_mac_index, OTP_MAC_AVAILABLE);

                if (otp_mac_index > 0) {
                    printf("\tMAC Addr: %c%c:%c%c:%c%c:%c%c:%c%c:%c%c\n",
                           macaddr_otp_str[0], macaddr_otp_str[1],
                           macaddr_otp_str[2], macaddr_otp_str[3],
                           macaddr_otp_str[4], macaddr_otp_str[5],
                           macaddr_otp_str[6], macaddr_otp_str[7],
                           macaddr_otp_str[8], macaddr_otp_str[9],
                           macaddr_otp_str[10], macaddr_otp_str[11]);
                } else {
                    printf("\tMAC Addr: <Empty>\n");
                }
            } else if (otp_mac_index >= OTP_MAC_AVAILABLE) {
                printf("Could not change OTP MAC address.(%d of %d)\n", otp_mac_index, OTP_MAC_AVAILABLE);
                return pdTRUE;
            }

            printf(ANSI_BOLD ANSI_COLOR_GREEN "Input MAC Addres: ");

            if (len == 12) {
                sprintf(tmp_macstr, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
                        tolower(argv[1][0]), tolower(argv[1][1]),
                        tolower(argv[1][2]), tolower(argv[1][3]),
                        tolower(argv[1][4]), tolower(argv[1][5]),
                        tolower(argv[1][6]), tolower(argv[1][7]),
                        tolower(argv[1][8]), tolower(argv[1][9]),
                        tolower(argv[1][10]), tolower(argv[1][11]));
            } else if (len == 17) {
                sprintf(tmp_macstr, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
                        tolower(argv[1][0]), tolower(argv[1][1]),
                        tolower(argv[1][3]), tolower(argv[1][4]),
                        tolower(argv[1][6]), tolower(argv[1][7]),
                        tolower(argv[1][9]), tolower(argv[1][10]),
                        tolower(argv[1][12]), tolower(argv[1][13]),
                        tolower(argv[1][15]), tolower(argv[1][16]));
            }

            printf("%s" ANSI_NORMAL "\n\n", tmp_macstr);

            if (compare_macaddr(macaddr_otp_str, argv[1])) {
                status = E_SAME_ERROR;
                goto msg_error;
            }

            {
#if defined ( __SUPPORT_APP_CONSOLE_INPUT__ )
                char input_str[2];
                printf(" Are you sure ? [" ANSI_BOLD "N" ANSI_NORMAL "o/"  ANSI_BOLD "Y" ANSI_NORMAL "es] : ");
                getStr(input_str, 1);
                if (toupper(input_str[0]) != 'Y') {
                    status = E_CANCELED;
                } else 
#endif // __SUPPORT_APP_CONSOLE_INPUT__
                {
                    status = rm_wifi_write_mac_address(argv[1], OTP_MAC);
                }
            }
        }
    }

msg_error:

    switch (status) {
        case E_WRITE_OK:
            printf("Write OK\n");
            break;
    
        case E_WRITE_ERROR:
            printf("Write Error\n");
            break;
    
        case E_ERASE_OK:
            printf("Erase OK\n");
            break;
    
        case E_ERASE_ERROR:
            printf("Erase ERROR\n");
            break;
    
        case E_DIGIT_ERROR:
#if SUPPORT_WLAN1_LOCAL_MACADDRESS
            printf("ERR: Digit ( 0 ~ F )\n");
#else
            printf("ERR: Last Digit - Need to Even-number ( 0, 2, 4, 6 , 8, A, C, or E )\n");
#endif /* SUPPORT_WLAN1_LOCAL_MACADDRESS */
            break;

        case E_MCAST_ERROR:
            printf("Mulicast MAC Address!!\n");
            printf("Canceled\n");
            break;

        case E_LOCAL_ERROR:
            printf("locally MAC Address!!\n");
            printf("Canceled\n");
            break;
    
        case E_SAME_ERROR:
            printf("Same MAC Address!!\n");
            printf("Canceled\n");
            break;

        case E_CANCELED:
            printf("Canceled\n");
            break;
            
        case E_INVALID_ERROR:
            printf("ERR: Invalid MAC Address\n");
            break;
    
        default:
            printf("ERR: option %s\nex) %s [xx:xx:xx:xx:xx:xx |"
                   " xx-xx-xx-xx-xx-xx | xxxxxxxxxxxx%s]\n",
                   argc > 1 ? argv[1] : "", argv[0],
                   strcmp(argv[0], CMD_SETOTPMAC) == 0 ? "" :
                   " | erase");
            break;
    }

    return pdTRUE;
}


#ifdef __SUPPORT_IPV4__
/* Validation check the local subnet of defined interface */
int isvalidIPsubnetInterface(long ip, int iface)
{
    int     status;
    char    ipstr[16];
    char    smstr[16];
    ip_addr_t tmp_addr;
    unsigned long ip_val, sm_val;
    
    
    /* IP Address */
    if (!get_ip_info(iface, GET_IPADDR, ipstr)) {
        return pdFAIL;
    }

    /* Subnet */
    if (!get_ip_info(iface, GET_SUBNET, smstr)) {
        return pdFAIL;
    }

    ipaddr_aton(ipstr, &tmp_addr);
    ip_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
    memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

    ipaddr_aton(smstr, &tmp_addr);
    sm_val = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

    status = isvalidIPsubnetRange(ip, ip_val, sm_val);
    return status;
}
#endif // __SUPPORT_IPV4__

#if defined (__SUPPORT_IPERF__)
static bool  cmd_iperf_cli(int argc, char *argv[])
{
    UCHAR   protocol_mode = IPERF_TCP;
    UCHAR   iperf_mode = IPERF_SERVER;
    UCHAR   iperf_interface = NONE_IFACE;
    struct IPERF_CONFIG config = { 0, };

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    config.ip_ver = 4;
    config.TestTime = 10 * configTICK_RATE_HZ;              /* 10 Sec. */
    config.RxTimeOut = 0;
    config.Interval = 0;
    config.PacketSize = 0;
    config.WMM_Tos = 255;
    config.bandwidth = 0;
    config.bandwidth_format = 'A';
    config.port = CLIENT_DEST_PORT;
    config.sendnum = 0;
    config.pair = 0;
    config.network_pool = 0;                                                /* 0: iperf_pool  1: main pool, */
    config.window_size = 0;                                                 /* 0: use default */
#ifdef __IPERF_PRINT_MIB__
    config.mib_flag = 0;                                                    /* 0: use default */
#endif /* __IPERF_PRINT_MIB__ */
    config.term = 0;
    config.tcp_api_mode = TCP_API_MODE_DEFAULT;             /* 0: tcp api mode */

    if (   argc == 1
            || (argc == 2 && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "help", 4) == 0))) {
        printf("Usage:\t%s "
#ifndef __SUPPORT_MULTI_IP_IF__
               "-I [WLAN0|WLAN1] "
#endif // __SUPPORT_MULTI_IP_IF__
               "[-s|-c host][options]\n"
               "\t%s [-h] [-v]\n"
               "\nClient/Server:\n"
#ifndef __SUPPORT_MULTI_IP_IF__
               "\t-I      Interface [WLAN0|WLAN1]\n"
#endif // __SUPPORT_MULTI_IP_IF__
               "\t-i      seconds between periodic bandwidth reports \n"
               "\t-u      use UDP rather than TCP\n"
               "\t-p, #   server port to listen on/connect to\n"
               "\t-f, [kmKM]   format to report: Kbits, Mbits, KBytes, MBytes\n"
               "\t-d      finsh service\n"
               "\t\tex) %s -I [wlan0|wlan1] -d -c -u : udp clinet\n"
               "\t\t    %s -I [wlan0|wlan1] -d -c    : tcp clinet\n"
               "\t\t    %s -I [wlan0|wlan1] -d -u    : udp server\n"
               "\t\t    %s -I [wlan0|wlan1] -d       : tcp server\n",
               argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);

#ifdef  __SUPPORT_IPV6__
        printf("\t-V Set the domain to IPv6\n");
#endif  /* __SUPPORT_IPV6__ */

        printf( "\nServer specific:\n"
                "\t-s      run in server mode\n"
                "\t-T  #   Rx Time Out Min:1 sec. 'F' Forever\n");

        printf( "\nClient specific:\n"
                "\t-c      <host>   run in client mode, connecting to <host>\n"
                "\t-t  #   time in seconds to transmit for (default 10 secs)\n"
                "\t-x  #   tcp API mode default:basic tcp(API) 1:Altcp 2:Socket \n"
#if UNUSE
                "\t-y  #   Transmit delay, tick 1 ~ 100\n"
#endif /* UNUSE */
                "\t-l  #   PacketSize option (UDP default %d, IPv6 %d TCP 1000)\n"
                "\t-n  #   UDP Tx packet number\n"
                "\t-S, #   set the socket's IP_TOS (byte) field\n"
                "\t-P, #   Pair Index (0,1,2)\n"
#ifdef __IPERF_BANDWIDTH__
                "\t-b  #M  Bandwidth to send at in Mbits/sec (debug only)\n"
#endif /* __IPERF_BANDWIDTH__ */
                "\t        (default Max, Step 1~100 Mbps)\n"
                "\t-O      use Main Packet Pool\n"
                ,
                DEFAULT_IPV4_PACKET_SIZE, DEFAULT_IPV6_PACKET_SIZE);

        printf( "\nMiscellaneous:\n"
#ifdef __IPERF_PRINT_MIB__
                "\t-m #    Print MIB info(debug only)\n"
                "\t\t1 counter reset\n"
                "\t\t2 counter retention\n"

#endif /* __IPERF_PRINT_MIB__ */
                "\t-h      print this message\n"

                "\t-v      print version\n");
        return pdTRUE;
    }

    /*          "\t -V     Set the domain to IPv6\n" \ */

    /* Parse the command.  */
    for (int n = 0; n < argc; n++) {

        /* -d Server/Client */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'd') {
            config.term = pdTRUE;
            n++;
        }

        if (*argv[n] == '-' && *(argv[n] + 1) == 'v') {
            printf("\niPerf2 Version  %s\n\n", IPERF_VERSION);
            return pdTRUE;
        }

#ifdef  __SUPPORT_IPV6__
        if (*argv[n] == '-' && *(argv[n] + 1) == 'V') {
            /* IPv6 */
            config.ip_ver = 6;
            continue;
        }
#endif  /* __SUPPORT_IPV6__ */

        /* -u UDP */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'u') {
            protocol_mode = IPERF_UDP; /* UDP */
        }

        /* -s Server */
        if (*argv[n] == '-' && *(argv[n] + 1) == 's') {
            iperf_mode = IPERF_SERVER;
        }

        /* -c Client */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'c') {
            iperf_mode = IPERF_CLIENT;

            if (config.term == pdTRUE) {
                continue;
            }

            /* DEST IP Address */
            if (config.ip_ver == 4) {   /* check ip */
#ifdef __SUPPORT_IPV4__
                ip_addr_t tmp_addr;
                if (is_in_valid_ip_class(argv[n + 1])) {
                    ipaddr_aton(argv[n + 1], &tmp_addr);
                    config.ipaddr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
                    n++;
                    continue;
                }
#endif // __SUPPORT_IPV4__
#ifdef  __SUPPORT_IPV6__
            } else if (config.ip_ver == 6) {
                int nPort;
                int bSuccess;

                bSuccess = parse_IPv6_to_long(argv[n + 1], config.ipv6addr, &nPort);

                if (!bSuccess) {
                    printf("Invalid IPv6 Address\n");
                    return pdFALSE;
                }

                n++;
                continue;
#endif  /* __SUPPORT_IPV6__ */
            } else {
                printf("\tERR: -c IPAddress\n");
                return pdTRUE;
            }
        }

        /* -n Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'n') {
            config.sendnum = ctoi(argv[n + 1]);

            if (config.sendnum >= 1) {
                n++;
                continue;
            } else {
                printf("\tERR: -n Number\n");
                return pdTRUE;
            }
        }

        /* -b bandwidth */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'b') {
#ifdef __IPERF_BANDWIDTH__

            for (int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                    if (toupper(*(argv[n + 1] + tt)) == 'M') {
                        *(argv[n + 1] + tt) = '\0';
                    } else {
                        printf("\tERR: -b Number\n");
                        return pdTRUE;
                    }
                }
            }

            config.bandwidth = ctoi(argv[n + 1]);
            n++;
            continue;
#else
            printf("\t'-b' option is not supported.\n");
            return pdTRUE;
#endif /* __IPERF_BANDWIDTH__ */
        }

        /* -f bandwidth */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'f') {

            char bformat = *argv[n + 1];

            if (   strlen(argv[n + 1]) == 1
                    && (bformat == 'M' || bformat == 'K' || bformat == 'm' || bformat == 'k')) {

                config.bandwidth_format = (UCHAR)bformat;
            } else {
                printf("\tERR: -f [M|K|m|k]\n");
                return pdTRUE;
            }

            n++;
            continue;
        }

        /* -w Number : TCP window size */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'w') {
            config.window_size = ctoi(argv[n + 1]);

            if (config.window_size >= 4 && config.window_size <= 64) {
                n++;
                continue;
            } else {
                printf("\tERR: -w Number\n");
                return pdTRUE;
            }
        }

        /* -P Pair Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'P') {
            config.pair = (UCHAR)ctoi(argv[n + 1]);

            if (config.pair < IPERF_TCP_TX_MAX_PAIR) {      /* 0, 1, 2 */
                n++;
                continue;
            } else {
                printf("\tERR: -P Pair Number\n");
                return pdTRUE;
            }
        }

        /* -p Port Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'p') {
            config.port = ctoi(argv[n + 1]);

            if (config.port >= 5001 && config.port < 32768) {       /* Local Port Range 32768~61000 */
                n++;
                continue;
            } else {
                printf("\tERR: -p Port (5001~32767)\n");
                return pdTRUE;
            }
        }

        /* -t Time */
        if (*argv[n] == '-' && *(argv[n] + 1) == 't') {
            if (config.TestTime > 0) {
                config.TestTime = (ctoi(argv[n + 1]) * configTICK_RATE_HZ);
                n++;
                continue;
            } else {
                printf("\tERR: -t time\n");
                return pdTRUE;
            }
        }

        /* -t Rx Time out */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'T') {
            if (*argv[n + 1]  == 'f' || *argv[n + 1]  == 'F') {
                config.RxTimeOut = portMAX_DELAY;
                n++;
                continue;
            } else {
                int check_num = 0;

                for (unsigned int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                    if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                        check_num = 1;
                        break;
                    }
                }

                if (check_num == 0) {
                    config.RxTimeOut = (ctoi(argv[n + 1]) * configTICK_RATE_HZ);
                    n++;
                    continue;
                } else {
                    printf("\tERR: -T time(sec.) or F\n");
                    return pdTRUE;
                }
            }
        }

        /* -l PacketSize option */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'l') {
            config.PacketSize = ctoi(argv[n + 1]);

            if (config.PacketSize > 0) {
                n++;
                continue;
            } else {
                printf("\tERR: -l size\n");
                return pdTRUE;
            }
        }

        /* -i Interval */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'i') {
            if (ctoi(argv[n + 1]) > 0) {
                config.Interval = ctoi(argv[n + 1]) * configTICK_RATE_HZ;
                n++;
                continue;
            } else {
                printf("\tERR: -i interval\n");
                return pdTRUE;
            }
        }

        /* -I Interface */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'I') {
            if (strcasecmp(argv[n + 1], "WLAN0") == 0) {
                iperf_interface = WLAN0_IFACE;
            } else if (strcasecmp(argv[n + 1], "WLAN1") == 0) {
                iperf_interface = WLAN1_IFACE;
            } else {
                printf("\tERR: -I interface\n");
                return pdTRUE;
            }

            n++;
            continue;
        }

        /* -S WMM TOS option */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'S') {
            config.WMM_Tos = (UCHAR)ctoi(argv[n + 1]);
            n++;
            continue;
        }

        /* -O iPerf network packet pool */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'O') {
            config.network_pool = 1;
        }

#ifdef __IPERF_PRINT_MIB__

        /* -m : mib */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'm') {
            for (int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                    printf("\tERR: -m %s\n", argv[n + 1]);
                    return pdTRUE;
                }
            }

            config.mib_flag = ctoi(argv[n + 1]);

            n++;
            continue;
        }

#endif /* __IPERF_PRINT_MIB__ */

#ifdef UNUSE
        if (*argv[n] == '-' && *(argv[n] + 1) == 'y') {
            config.transmit_rate = ctoi(argv[n + 1]);

            if (config.transmit_rate > 0) {
                n++;
                continue;
            } else {
                printf("\tERR: -y #\n");
                return pdTRUE;
            }
        }
#endif /* UNUSE */

        if (*argv[n] == '-' && *(argv[n] + 1) == 'x') {
            config.tcp_api_mode = ctoi(argv[n + 1]);

            if (config.tcp_api_mode < TCP_API_MODE_MAX) {
                n++;
                continue;
            } else {
                printf("\tERR: -x mode(0|1|2)\n");
                return pdTRUE;
            }
        }

    }

    if (iperf_interface == NONE_IFACE) {
        if (iperf_mode == IPERF_CLIENT) {
#if defined (__SUPPORT_IPV4__)            
            if (isvalidIPsubnetInterface((long)config.ipaddr, WLAN1_IFACE)) {
                iperf_interface = WLAN1_IFACE;
            } else {
                iperf_interface = WLAN0_IFACE;
            }
#endif // __SUPPORT_IPV4__
        } else {        /* IPERF_SERVER Mode Inteface */
            printf("\tERR: -I Interface\n");
            return pdTRUE;
        }
    }

    switch (protocol_mode) {
        case IPERF_TCP:
            if (iperf_mode == IPERF_CLIENT) {
                iperf_mode = IPERF_CLIENT_TCP;
            } else {
                iperf_mode = IPERF_SERVER_TCP;
            }

            break;

        case IPERF_UDP:
            if (iperf_mode == IPERF_CLIENT) {
                iperf_mode = IPERF_CLIENT_UDP;
            } else {
                iperf_mode = IPERF_SERVER_UDP;
            }

            break;
    }

    return iperf_cli(iperf_interface, iperf_mode, &config);
}
#endif  //(__SUPPORT_IPERF__)

#if defined (__SUPPORT_IPERF3__)
static bool  cmd_iperf3_cli(int argc, char *argv[])
{
    UCHAR   protocol_mode = IPERF_TCP;
    UCHAR   iperf_mode = IPERF_SERVER;
    UCHAR   iperf_interface = NONE_IFACE;
    struct IPERF_CONFIG config = { 0, };

    config.ip_ver = 4;
    config.TestTime = 10 * configTICK_RATE_HZ;              /* 10 Sec. */
    config.RxTimeOut = 0;
    config.Interval = 0;
    config.PacketSize = 0;
    config.WMM_Tos = 255;
    config.bandwidth = 0;
    config.bandwidth_format = 'A';
    config.port = LWIPERF3_TCP_PORT_DEFAULT;
    config.sendnum = 0;
    config.pair = 0;
    config.network_pool = 0;                                                /* 0: iperf_pool  1: main pool, */
    config.window_size = 0;                                                 /* 0: use default */
#ifdef __IPERF_PRINT_MIB__
    config.mib_flag = 0;                                                    /* 0: use default */
#endif /* __IPERF_PRINT_MIB__ */
    config.term = 0;
    config.tcp_api_mode = TCP_API_MODE_DEFAULT;             /* 0: tcp api mode */

    if (   argc == 1
            || (argc == 2 && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "help", 4) == 0))) {
        printf("Usage:\t%s "
#ifndef __SUPPORT_MULTI_IP_IF__
               "-I [WLAN0|WLAN1] "
#endif // __SUPPORT_MULTI_IP_IF__
               "[-s|-c host][options]\n"
               "\t%s [-h] [-v]\n"
               "\nClient/Server:\n"
#ifndef __SUPPORT_MULTI_IP_IF__
               "\t-I      Interface [WLAN0|WLAN1]\n"
#endif // __SUPPORT_MULTI_IP_IF__
               "\t-i      seconds between periodic bandwidth reports \n"
               "\t-u      use UDP rather than TCP\n"
               "\t-p, #   server port to listen on/connect to\n"
               "\t-f, [kmKM]   format to report: Kbits, Mbits, KBytes, MBytes\n"
               "\t-d      finsh service\n"
               "\t\tex) %s -I [wlan0|wlan1] -d -c -u : udp clinet\n"
               "\t\t    %s -I [wlan0|wlan1] -d -c    : tcp clinet\n"
               "\t\t    %s -I [wlan0|wlan1] -d -u    : udp server\n"
               "\t\t    %s -I [wlan0|wlan1] -d       : tcp server\n",
               argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);

#ifdef  __SUPPORT_IPV6__
        printf("\t-V Set the domain to IPv6\n");
#endif  /* __SUPPORT_IPV6__ */

        printf( "\nServer specific:\n"
                "\t-s      run in server mode\n"
                "\t-T  #   Rx Time Out Min:1 sec. 'F' Forever\n");

        printf( "\nClient specific:\n"
                "\t-c      <host>   run in client mode, connecting to <host>\n"
                "\t-t  #   time in seconds to transmit for (default 10 secs)\n"
                "\t-x  #   tcp API mode default:basic tcp(API) 1:Altcp 2:Socket \n"
                "\t-y  #   Transmit delay, tick 1 ~ 100\n"
                "\t-l  #   PacketSize option (UDP default %d, IPv6 %d TCP 1000)\n"
                "\t-n  #   UDP Tx packet number\n"
                "\t-P, #   Pair Index (0,1,2)\n"
#ifdef __IPERF_BANDWIDTH__
                "\t-b  #M  Bandwidth to send at in Mbits/sec (debug only)\n"
#endif /* __IPERF_BANDWIDTH__ */
                "\t        (default Max, Step 1~100 Mbps)\n"
                "\t-O      use Main Packet Pool\n"
                ,
                DEFAULT_IPV4_PACKET_SIZE, DEFAULT_IPV6_PACKET_SIZE);

        printf( "\nMiscellaneous:\n"
#ifdef __IPERF_PRINT_MIB__
                "\t-m #    Print MIB info(debug only)\n"
                "\t\t1 counter reset\n"
                "\t\t2 counter retention\n"

#endif /* __IPERF_PRINT_MIB__ */
                "\t-h      print this message\n"

                "\t-v      print version\n");
        return pdTRUE;
    }

    /*          "\t -V     Set the domain to IPv6\n" \ */

    /* Parse the command.  */
    for (int n = 0; n < argc; n++) {

        /* -d Server/Client */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'd') {
            config.term = pdTRUE;
            n++;
        }

        if (*argv[n] == '-' && *(argv[n] + 1) == 'v') {
            printf("\niPerf3 Version %s\n\n", IPERF_VERSION);
            return pdTRUE;
        }

#ifdef  __SUPPORT_IPV6__
        if (*argv[n] == '-' && *(argv[n] + 1) == 'V') {
            /* IPv6 */
            config.ip_ver = 6;
            continue;
        }
#endif  /* __SUPPORT_IPV6__ */

        /* -u UDP */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'u') {
            protocol_mode = IPERF_UDP; /* UDP */
        }

        /* -s Server */
        if (*argv[n] == '-' && *(argv[n] + 1) == 's') {
            iperf_mode = IPERF_SERVER;
        }

        /* -c Client */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'c') {
            iperf_mode = IPERF_CLIENT;

            if (config.term == pdTRUE) {
                continue;
            }

            /* DEST IP Address */
            if (config.ip_ver == 4) {   /* check ip */
#ifdef __SUPPORT_IPV4__                
                ip_addr_t tmp_addr;
                if (is_in_valid_ip_class(argv[n + 1])) {
                    ipaddr_aton(argv[n + 1], &tmp_addr);
                    config.ipaddr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
                    n++;
                    continue;
                }
#endif // __SUPPORT_IPV4__
#ifdef  __SUPPORT_IPV6__
            } else if (config.ip_ver == 6) {
                int nPort;
                int bSuccess;

                bSuccess = parse_IPv6_to_long(argv[n + 1], config.ipv6addr, &nPort);

                if (!bSuccess) {
                    printf("Invalid IPv6 Address\n");
                    return pdFALSE;
                }

                n++;
                continue;
#endif  /* __SUPPORT_IPV6__ */
            } else {
                printf("\tERR: -c IPAddress\n");
                return pdTRUE;
            }
        }

        /* -n Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'n') {
            config.sendnum = ctoi(argv[n + 1]);

            if (config.sendnum >= 1) {
                n++;
                continue;
            } else {
                printf("\tERR: -n Number\n");
                return pdTRUE;
            }
        }

        /* -b bandwidth */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'b') {
#ifdef __IPERF_BANDWIDTH__

            for (int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                    if (toupper(*(argv[n + 1] + tt)) == 'M') {
                        *(argv[n + 1] + tt) = '\0';
                    } else {
                        printf("\tERR: -b Number\n");
                        return;
                    }
                }
            }

            config.bandwidth = ctoi(argv[n + 1]);
            n++;
            continue;
#else
            printf("\t'-b' option is not supported.\n");
            return pdTRUE;
#endif /* __IPERF_BANDWIDTH__ */
        }

        /* -f bandwidth */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'f') {

            char bformat = *argv[n + 1];

            if (   strlen(argv[n + 1]) == 1
                    && (bformat == 'M' || bformat == 'K' || bformat == 'm' || bformat == 'k')) {

                config.bandwidth_format = (UCHAR)bformat;
            } else {
                printf("\tERR: -f [M|K|m|k]\n");
                return pdTRUE;
            }

            n++;
            continue;
        }

        /* -w Number : TCP window size */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'w') {
            config.window_size = ctoi(argv[n + 1]);

            if (config.window_size >= 4 && config.window_size <= 64) {
                n++;
                continue;
            } else {
                printf("\tERR: -w Number\n");
                return pdTRUE;
            }
        }

        /* -P Pair Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'P') {
            config.pair = (UCHAR)ctoi(argv[n + 1]);

            if (config.pair < IPERF_TCP_TX_MAX_PAIR) {      /* 0, 1, 2 */
                n++;
                continue;
            } else {
                printf("\tERR: -P Pair Number\n");
                return pdTRUE;
            }
        }

        /* -p Port Number */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'p') {
            config.port = ctoi(argv[n + 1]);

            if (config.port >= 5001 && config.port < 32768) {       /* Local Port Range 32768~61000 */
                n++;
                continue;
            } else {
                printf("\tERR: -p Port (5001~32767)\n");
                return pdTRUE;
            }
        }

        /* -t Time */
        if (*argv[n] == '-' && *(argv[n] + 1) == 't') {
            if (config.TestTime > 0) {
                config.TestTime = (ctoi(argv[n + 1]) * configTICK_RATE_HZ);
                n++;
                continue;
            } else {
                printf("\tERR: -t time\n");
                return pdTRUE;
            }
        }

        /* -t Rx Time out */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'T') {
            if (*argv[n + 1]  == 'f' || *argv[n + 1]  == 'F') {
                config.RxTimeOut = portMAX_DELAY;
                n++;
                continue;
            } else {
                int check_num = 0;

                for (unsigned int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                    if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                        check_num = 1;
                        break;
                    }
                }

                if (check_num == 0) {
                    config.RxTimeOut = (ctoi(argv[n + 1]) * configTICK_RATE_HZ);
                    n++;
                    continue;
                } else {
                    printf("\tERR: -T time(sec.) or F\n");
                    return pdTRUE;
                }
            }
        }

        /* -l PacketSize option */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'l') {
            config.PacketSize = ctoi(argv[n + 1]);

            if (config.PacketSize > 0) {
                n++;
                continue;
            } else {
                printf("\tERR: -l size\n");
                return pdTRUE;
            }
        }

        /* -i Interval */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'i') {
            if (ctoi(argv[n + 1]) > 0) {
                config.Interval = ctoi(argv[n + 1]) * configTICK_RATE_HZ;
                n++;
                continue;
            } else {
                printf("\tERR: -i interval\n");
                return pdTRUE;
            }
        }

        /* -I Interface */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'I') {
            if (strcasecmp(argv[n + 1], "WLAN0") == 0) {
                iperf_interface = WLAN0_IFACE;
            } else if (strcasecmp(argv[n + 1], "WLAN1") == 0) {
                iperf_interface = WLAN1_IFACE;
            } else {
                printf("\tERR: -I interface\n");
                return pdTRUE;
            }

            n++;
            continue;
        }

        /* -S WMM TOS option */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'S') {
            config.WMM_Tos = (UCHAR)ctoi(argv[n + 1]);
            n++;
            continue;
        }

        /* -O iPerf network packet pool */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'O') {
            config.network_pool = 1;
        }

#ifdef __IPERF_PRINT_MIB__

        /* -m : mib */
        if (*argv[n] == '-' && *(argv[n] + 1) == 'm') {
            for (int tt = 0; tt < strlen(argv[n + 1]); tt++) {
                if (isdigit((int)(*(argv[n + 1] + tt))) == 0) {
                    printf("\tERR: -m %s\n", argv[n + 1]);
                    return;
                }
            }

            config.mib_flag = ctoi(argv[n + 1]);

            n++;
            continue;
        }

#endif /* __IPERF_PRINT_MIB__ */

        if (*argv[n] == '-' && *(argv[n] + 1) == 'y') {
            config.transmit_rate = ctoi(argv[n + 1]);

            if (config.transmit_rate > 0) {
                n++;
                continue;
            } else {
                printf("\tERR: -y size\n");
                return pdTRUE;
            }
        }

        if (*argv[n] == '-' && *(argv[n] + 1) == 'x') {
            config.tcp_api_mode = ctoi(argv[n + 1]);

            if (config.tcp_api_mode < TCP_API_MODE_MAX) {
                n++;
                continue;
            } else {
                printf("\tERR: -x mode(0|1|2)\n");
                return pdTRUE;
            }
        }

    }

    if (iperf_interface == NONE_IFACE) {
        if (iperf_mode == IPERF_CLIENT) {
#if defined (__SUPPORT_IPV4__)
            if (isvalidIPsubnetInterface((long)config.ipaddr, WLAN1_IFACE)) {
                iperf_interface = WLAN1_IFACE;
            } else {
                iperf_interface = WLAN0_IFACE;
            }
#endif // __SUPPORT_IPV4__
        } else {        /* IPERF_SERVER Mode Inteface */
            printf("\tERR: -I Interface\n");
            return pdTRUE;
        }
    }

    switch (protocol_mode) {
        case IPERF_TCP:
            if (iperf_mode == IPERF_CLIENT) {
                iperf_mode = IPERF_CLIENT_TCP;
            } else {
                iperf_mode = IPERF_SERVER_TCP;
            }

            break;

        case IPERF_UDP:
            if (iperf_mode == IPERF_CLIENT) {
                iperf_mode = IPERF_CLIENT_UDP;
            } else {
                iperf_mode = IPERF_SERVER_UDP;
            }

            break;
    }

    return iperf3_cli(iperf_interface, iperf_mode, &config);

}
#endif  //(__SUPPORT_IPERF3__)


#if LWIP_ARP
static bool cmd_arp_table(int argc, char *argv[])
{
    UINT status;
    UINT iface = 0;
    UCHAR tmp_str[ARP_TABLE_SIZE];

    memset(tmp_str, 0, 10);

    //If wlan is not initialized 
    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    if (argc == 1 || strcasecmp(argv[1], "HELP") == 0) {
arp_help:
        printf("    Usage : arp [option] [interface]\n");
        printf("     option\n");
        printf("\t [-a]\t : Print all of ARP table\n");
        printf("\t [-d]\t : Delete all of ARP table\n");
        printf("     interface\n");
        printf("\t [WLAN0 or WLAN1]\n");
        printf("\n");
        return pdTRUE;
    }

    if (strcasecmp(argv[1], "WLAN0") == 0 && argc == 2) { /* WLAN0 */
        iface = WLAN0_IFACE;
        status = print_arp_table(iface);
    } else if (strcasecmp(argv[1], "WLAN1") == 0 && argc == 2) {    /* WLAN1 */
        iface = WLAN1_IFACE;
        status = print_arp_table(iface);
    } else if (strcasecmp(argv[1], "-A") == 0) {
        for (int i = 0; i < 2; i++) {
            print_arp_table((UINT)i);
        }

        status = pdPASS;
    } else if (strcasecmp(argv[1], "-D") == 0) {
        for (int i = 0; i < 2; i++) {
            arp_entry_delete(i);
        }

        status = pdPASS;
    } else if (strcasecmp(argv[1], "-F") == 0) {
        /* Internal command for debugging
           e.g.) arp -f wlan0 192.168.1.1
           : find mac address (resolved, stable) for 192.168.1.1 from arp table */
        int is_err = 0;
        
        if (argc == 4) {
            int find_result = 0;
            char * input_ip_str = argv[3];
            ip_addr_t input_ip;
            uint8_t _mac_addr[6] = {0,};
            
            if (strcasecmp(argv[1], "WLAN0") == 0) {
                iface = WLAN0_IFACE;
            } else if (strcasecmp(argv[1], "WLAN1") == 0) {
                iface = WLAN1_IFACE;
            } else {
                is_err = 1;
            }

            ipaddr_aton(input_ip_str, &input_ip);

            find_result = etharp_get_mac_from_ip(iface, (ip4_addr_t *)(&input_ip), &_mac_addr[0]);
            
            if (find_result) {
                printf("Search result = %d, ip_addr = %s, mac_addr = %u:%u:%u:%u:%u:%u\n", 
                    find_result, input_ip_str,
                    _mac_addr[0], _mac_addr[1], _mac_addr[2], _mac_addr[3], _mac_addr[4], _mac_addr[5]);
            }
        }
        
        if (is_err == 1) {
            goto arp_help;
        }
    
        status = pdPASS;
    } else {
        goto arp_help;
    }

    if (status != pdPASS) {
        goto arp_help;
    }

    return pdTRUE;
}
extern bool cmd_arping(int argc, char *argv[]);
#endif //LWIP_ARP

bool cmd_nslookup(int argc, char *argv[])
{
    LWIP_UNUSED_ARG(argv);
    bool result = pdFALSE;

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    if (argc == 1) {
        printf("\r\nUsage: nslookup <domain>\n");
        return pdTRUE;
    }

#ifdef LWIP_DNS
    char * domain_url = argv[1];
#if defined (__SUPPORT_IPV4__)
    char ip4addr_str[IPADDR_LEN] = {0,};
#endif // __SUPPORT_IPV4__

#if defined (__SUPPORT_IPV6__)
    char ip6addr_str[IP6ADDR_LEN] = {0,};
#endif // __SUPPORT_IPV6

    printf("Domain: %s\n\r", domain_url);

#if defined (__SUPPORT_IPV4__)
    result = dns_A_Query(domain_url, ip4addr_str, 4000);

    /* Fail checking ... */
    if (result == pdFALSE) {
        printf("\nDNS(A)query failed. Domain url is invalid.\n");
    } else {
        printf("\nIPv4 : %s\n", ip4addr_str);
    }

#endif // __SUPPORT_IPV4__

#if defined (__SUPPORT_IPV6__)
    result = dns_AAAA_Query(domain_url, ip6addr_str, 4000);

    /* Fail checking ... */
    if (result == pdFALSE) {
        printf("\nDNS(AAAA)query failed. Domain url is invalid.\n");
    } else {
        printf("\nIPv6 : %s\n", ip6addr_str);
    }

    return pdTRUE;
#endif // __SUPPORT_IPV6__
#endif    /*LWIP_DNS*/
}

#if LWIP_ARP
/* arprequset, arpresponse, garpsend */
static bool cmd_arp_send(int argc, char *argv[])
{
    int status = pdPASS;

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

#ifdef __SUPPORT_MULTI_IP_IF__

    if (((*argv[0] == 'a' && (argc == 2 || argc == 3)) || (*argv[0] == 'g')))
#else
    if (   *argv[1] == 'w'
            && *(argv[1] + 1) == 'l'
            && *(argv[1] + 2) == 'a'
            && *(argv[1] + 3) == 'n'
            && (*(argv[1] + 4) == '0' || *(argv[1] + 4) == '1')
            && ((*argv[0] == 'a' && (argc == 3 || argc == 4)) || (*argv[0] == 'g') || (*argv[0] == 'd')))
#endif /* __SUPPORT_MULTI_IP_IF__ */
    {
#ifndef __SUPPORT_MULTI_IP_IF__

#endif /* __SUPPORT_MULTI_IP_IF__ */

        if (*argv[0] == 'g')
#ifdef __SUPPORT_MULTI_IP_IF__
        {
            int ch_ip = 0;

            if (argc == 2) {
                /* check ip confilt */
                ch_ip = ctoi(argv[1]);
            }

            /* GARP */
            status = garp_request(WLAN0_IFACE, ch_ip);
        } else if (strncmp(argv[0], "arpsend", 6) == 0 &&  argc == 2 && is_in_valid_ip_class(argv[1])) {
            /* ARP Request */
            ip4_addr_t ipaddr;
            ipaddr_aton(argv[1], &ipaddr);
            status = arp_request(ipaddr, WLAN0_IFACE);
        } else if (strncmp(argv[0], "dhcp_arp", 8) == 0 && ( argc == 1 || (argc == 2 && is_in_valid_ip_class(argv[1])))) {
            /* DHCP ARP Request */
            ip_addr_t tmp_addr;
            ipaddr_aton(argv[1], &tmp_addr);
            status = dhcp_arp_request(WLAN0_IFACE, argc == 2 ? lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr))) : 0);
        } else if (strncmp(argv[0], "arpres", 6) == 0 && argc == 3 && is_in_valid_ip_class(argv[1])) {
            /* ARP Response*/
            status = arp_response(WLAN0_IFACE, argv[1],  argv[2]);
        }
#else
        {
            int ch_ip = 0;

            if (argc == 3) {
                /* check ip confilt */
                ch_ip = (int)ctoi(argv[2]);
            }

            /* GARP */
            status = garp_request(*(argv[1] + 4) - 0x30, ch_ip);
        } else if (strncmp(argv[0], "arpsend", 6) == 0 &&  argc == 3 && is_in_valid_ip_class(argv[2])) {
            /* ARP Request */
            ip4_addr_t ipaddr;
            ipaddr_aton(argv[2], (ip_addr_t *)&ipaddr);
            status = arp_request(ipaddr, *(argv[1] + 4) - 0x30);
        } else if (strncmp(argv[0], "dhcp_arp", 8) == 0 && (argc == 2 || (argc == 3 && is_in_valid_ip_class(argv[2])))) {
            /* DHCP ARP Request */
            ip_addr_t tmp_addr;
            ipaddr_aton(argv[2], &tmp_addr);
            status = dhcp_arp_request(*(argv[1] + 4) - 0x30, argc == 3 ? lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr))) : 0);
        } else if (strncmp(argv[0], "arpres", 6) == 0 && argc == 4 && is_in_valid_ip_class(argv[2])) {
            /* ARP Response*/
            status = arp_response(*(argv[1] + 4) - 0x30, argv[2],  argv[3]);
        }

#endif /* __SUPPORT_MULTI_IP_IF__ */
    } else { /* help */
        printf("\n%s "
#ifndef __SUPPORT_MULTI_IP_IF__
               "[wlan0|wlan1]"
#endif /* __SUPPORT_MULTI_IP_IF__ */
               " %s %s\n",
               argv[0],                                                     /* cmd */
               *argv[0] == 'g' ? "[Option]" : "[Dst IP Address]",         /* arp resquest */
               strncmp(argv[0], "arpres", 6) == 0 ? "[Dst MAC Address]" : "" );     /* arp response */

        if (*argv[0] == 'g') { /* garp */
            printf("\tOption = 0 : Normal garp\n"
                   "\tOption = 1 : Check IP conflict\n");
        }

        return pdTRUE;
    }

    if (status == pdPASS) {
        printf("\n%s sent%s%s\n",
               argv[0],
               *argv[0] == ('g') ? "" : " : ",
               *argv[0] == ('g') ? "" :
#ifdef __SUPPORT_MULTI_IP_IF__
               argv[1]
#else
               argv[2]
#endif /* __SUPPORT_MULTI_IP_IF__ */
              );
    } else {
        printf("\nERR : %s send\n", argv[0]);
    }

    return pdTRUE;
}
#endif // LWIP_ARP

#if defined ( __SUPPORT_SNTP_CLIENT__ )
bool cmd_network_sntp(int argc, char *argv[])
{
    unsigned int status;

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    if (argc == 3) {
        unsigned int svr_addr_idx = 99;

        if (strcasecmp("ADDR", argv[1]) == 0) {
            svr_addr_idx = 0;
        } else if (strcasecmp("ADDR_2", argv[1]) == 0) {
            svr_addr_idx = 1;
        } else if (strcasecmp("ADDR_3", argv[1]) == 0) {
            svr_addr_idx = 2;
        }

        if (svr_addr_idx < 3) {
            if (strchr(argv[2], '.') != NULL) {        /* IPv4 */
#if defined ( __SUPPORT_IPV4__ )
                if (isVaildDomain(argv[2]) == 0 && is_in_valid_ip_class(argv[2]) == 0) {
                    goto sntp_help;
                }
#else // __SUPPORT_IPV6__
                if (isVaildDomain(argv[2]) == 0) {
                    goto sntp_help;
                }
#endif // __SUPPORT_IPV4__
            }
#if defined ( __SUPPORT_IPV6__ )
            else if (strchr(argv[2], ':') != NULL) {     /* IPv6 */
                ULONG   ipv6_dst[4];

                if (parse_IPv6_to_long(argv[2], ipv6_dst, NULL) == 0) {
                    goto sntp_help;
                }
            }
#endif    /* __SUPPORT_IPV6__ */
            else {
                goto sntp_help;
            }

            status = (UINT)set_sntp_server((UCHAR *)argv[2], svr_addr_idx);
            printf("%sSNTP Server %d: %s\n", status ? "":"Error: ", svr_addr_idx+1, (UCHAR *)argv[2]);

            return pdTRUE;
        } else if (strcasecmp("PERIOD", argv[1]) == 0) {
            unsigned int i = 0;

            for (i = 0 ; i < strlen(argv[2]) ; i++) {
                if (!ISDIGIT(argv[2][i])) {
                    printf("Error: SNTP period %s\n", argv[2]);
                    return pdTRUE;
                }
            }

            i = ctoi(argv[2]);

            status = set_sntp_period((int)i);
            printf("%sSNTP period %d\n", status ? "":"Error: ", i);
            return pdTRUE;
        } else {
            goto sntp_help;
        }
    } else if (argc == 2) {
        if (strcasecmp("ENABLE", argv[1]) == 0) {
            if (sntp_get_use()) {
                printf("SNTP is Already Enabled\n");
            } else {
                sntp_set_use(pdTRUE);
                printf("SNTP use: Enabled\n");
            }

            return pdTRUE;
        } else if (strcasecmp("SYNC", argv[1]) == 0) {
            if (sntp_get_use()) {
                sntp_sync_now();
            } else {
                printf("SNTP is not Enabled\n");
            }

            return pdTRUE;
        } else if (strcasecmp("DISABLE", argv[1]) == 0) {
            sntp_set_use(pdFALSE);
            printf("SNTP use: Disabled\n");
            return pdTRUE;
        } else if (strcasecmp("STATUS", argv[1]) == 0) {
            char sntp_addr_str[32] = {0, };

            printf("SNTP Status : %s\n", get_sntp_use() ? "Enabled" : "Disabled");

            if (get_sntp_use() == pdTRUE) {
                get_sntp_server(sntp_addr_str, 0);
                printf("\tServer_1 : %s\n", sntp_addr_str);

                get_sntp_server(sntp_addr_str, 1);
                printf("\tServer_2 : %s\n", sntp_addr_str);

                get_sntp_server(sntp_addr_str, 2);
                printf("\tServer_3 : %s\n", sntp_addr_str);

                printf("\tPeriod   : %d sec.\n", get_sntp_period());
            }

            return pdTRUE;
        } else if (strcasecmp("HELP", argv[1]) == 0) {
            goto sntp_help;
        } else {
            goto sntp_help;
        }
    } else {
        /* HELP */
sntp_help:
        printf("Uasge: sntp [option]\n"
               "\t\t: status\n"
               "\t\t: enable | disable\n"
               "\t\t: addr [server]\n"
               "\t\t: addr_2 [server]\n"
               "\t\t: addr_3 [server]\n"
               "\t\t: period [second]\n"
               "\t\t: sync\n"
               "\t\t: help\n");
    }

    return pdTRUE;
}
#endif    /*__SUPPORT_SNTP_CLIENT__*/


#if defined ( __USER_DHCP_HOSTNAME__ )
static bool cmd_dhcp_hostname(int argc, char *argv[])
{
    char    name_str[DHCP_HOSTNAME_MAX_LEN + 1];
    char *result_ptr = NULL;
    int status;

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    if (argc == 1) {
dhcp_hostname_help :
        printf("- dhcp_hostname\n\n");
        printf("  Usage : dhcp_hostname [get|set|del] [hostname]\n");
        printf("    option\n");
        printf("    <get>\t\t    : get saved user DHCP-client hostname\n");
        printf("    <set> <hostname>\t    : set new user DHCP-client hostname (len <= 32)\n");
        printf("    <del>\t\t    : delete saved DHCP-client hostname\n");
        return pdTRUE;
    } else if (argc == 2) {
        if (strcmp(argv[1], "get") == 0) {
            memset(name_str, 0, (DHCP_HOSTNAME_MAX_LEN+1));
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME, &result_ptr);
#endif

	        if (result_ptr && strlen(result_ptr) > 0) {
	            bsp_safe_strcpy(name_str, result_ptr, DHCP_HOSTNAME_MAX_LEN + 1);
	            printf(" > User DHCP-client hostname = [%s]\n", name_str);
	        } else {
	            printf("No saved User DHCP-client hostname(%d)...\n", CC_FAILURE_NO_VALUE);
	        }
        } else if (strcmp(argv[1], "del") == 0) {

            // Delete hostname to NVRAM
#ifdef RM_MAP_PERSISTANT_W
            status = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME);
#endif

            if (status != FSP_SUCCESS) {
                printf("Failed to delete User DHCP-client hostname (%d) !!!\n", status);
            }

        } else {
            goto dhcp_hostname_help;
        }
    } else if (argc == 3) {
        if (strcmp(argv[1], "set") == 0) {
            if (strlen(argv[2]) > DHCP_HOSTNAME_MAX_LEN) {
                printf("Too long user DHCP-client hostname <%s> !!!\n", argv[2]);
                goto dhcp_hostname_help;
            }

            int    str_len = strlen(argv[2]);
            if (str_len > 0) {
                char    ch;

                // Check DHCP hostname validity : 0 .. 9, a .. z, A .. Z, -
                for (int i = 0; i < str_len; i++) {
                    ch = argv[2][i];
                    if (   (ch == '-')
                        || (ch >= '0' && ch <= '9')
                        || (ch >= 'a' && ch <= 'z')
                        || (ch >= 'A' && ch <= 'Z') ) {
                        // Okay,,, next character...
                        continue;
                    } else {
                        printf("Failed to save user DHCP-client hostname (%d) !!!\n", CC_FAILURE_INVALID);
                    }
                }

                // Save hostname to NVRAM
#ifdef RM_MAP_PERSISTANT_W
                status = RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME, argv[2]);
#endif
            } else {
                // Delete hostname to NVRAM
#ifdef RM_MAP_PERSISTANT_W
                status = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, NVR_DHCPC_HOSTNAME);
#endif
            }

            if (status != FSP_SUCCESS) {
                printf("Failed to save user DHCP-client hostname (%d) !!!\n", status);
            }

        } else {
            goto dhcp_hostname_help;
        }
    } else {
        goto dhcp_hostname_help;
    }

    return pdTRUE;
}
#endif    // __USER_DHCP_HOSTNAME__

#if defined (__SUPPORT_IPV4__)
extern struct netif   *dhcps_netif;
static bool cmd_network_dhcpd(int argc, char *argv[])
{
    int status = ERR_OK;

    extern UINT is_supplicant_done();

    dhcps_cmd_param *param = NULL;
    param = pvPortMalloc(sizeof(dhcps_cmd_param));
    memset(param, 0, sizeof(dhcps_cmd_param));

    ULONG start_ip, end_ip, ip_addr, net_mask, gw_addr = 0;

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        if (param) {
            vPortFree(param);
            param = NULL;
        }
        return pdTRUE;
    }

    switch (get_run_mode()) {
        case WIFI_DEVICE_MODE_EXT_STATION:
            goto not_support;

#ifdef __SUPPORT_P2P__
        case WIFI_DEVICE_MODE_EXT_P2P:
        case WIFI_DEVICE_MODE_EXT_P2P_GO:
        case WIFI_DEVICE_MODE_EXT_P2P_STATION:
            if (get_netmode(WLAN1_IFACE) == DHCPCLIENT){
                goto not_support;
            }
            break;
#endif /* __SUPPORT_P2P__ */

        default:
            /* Don't support "dhcpd command" when no connection state */
            if (is_supplicant_done() == 0){
not_support :
                printf("Notice : Doesn't support 'dhcpd' command in no connection state\n");
                return pdTRUE;
            }
    }

    if (argc == 2 && strcasecmp(argv[1], "HELP") == 0) {
        usage_dhcpd();
        return pdTRUE;
    }
#ifndef __SUPPORT_DHCP_SVR__
    else {
        printf(">> Doesn't support DHCP server !!!\n");
        return pdTRUE;
    }
#endif

    if (argc < 2) {
        status = ERR_ARG;
    }
#if defined (__UNUSED_CODE__)
#ifdef  __SUPPORT_IPV6__
    for (int n = 0; n < argc; n++) {
        if (strcmp(argv[n], "-6") == 0) {
            if (argc < 3) {
                usage_dhcpd();
                return pdTRUE;
            }
            // ver = 6;
        }
    }
#endif    /* __SUPPORT_IPV6__ */
#endif /* __UNUSED_CODE__ */


    for (int n = 1; n < argc; n++) {
        //Parse & execute dhcp server
        if (strcasecmp(argv[n], "STATUS") == 0) {
            dhcps_get_info();
            break;
        } else if (strcasecmp(argv[n], "RANGE") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            if ( argc < 4 ) {
                status = ERR_ARG;
            } else {
                ip_addr_t tmp_addr;
                
                ipaddr_aton(argv[n+1], &tmp_addr);
                start_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));
                memset(&tmp_addr, 0x00, sizeof(ip_addr_t));

                ipaddr_aton(argv[n+2], &tmp_addr);
                end_ip = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&tmp_addr)));

                if (is_dhcp_server_running() == 1 /* DHCPD is in running state */) {
                    ip_addr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&dhcps_netif->ip_addr)));
                    net_mask = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&dhcps_netif->netmask)));
                    gw_addr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&dhcps_netif->gw)));

                } else {
                    /* DHCPD is in stopped state */
                    struct netif *netif = netif_get_by_index(WLAN1_IFACE + 2); // assume Softap mode
                    ip_addr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&netif->ip_addr)));
                    net_mask = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&netif->netmask)));
                    gw_addr = lwip_htonl(ip4_addr_get_u32(ip_2_ip4(&netif->gw)));
                }
                
                if (is_in_valid_ip_class(argv[n+1]) && is_in_valid_ip_class(argv[n+2])) {
                    if( ((ip_addr >> 8) != (start_ip >> 8)) || ((ip_addr >> 8) != (end_ip >> 8)) ) {
                        PRINTF("ERR: Failed to set range of IP_addr list.\n");
                        status = ERR_ARG;
                        break;
                    }

                    if (isvalidIPsubnetRange(start_ip,  gw_addr, net_mask) == pdFALSE) {
                        PRINTF("ERR: Start IP_addr is out of range. \n");
                        status = ERR_ARG;
                        break;
                    }

                    if (isvalidIPsubnetRange(end_ip,  gw_addr, net_mask) == pdFALSE) {
                        PRINTF("ERR: End IP_addr is out of range. \n");
                        status = ERR_ARG;
                        break;
                    }
                    if (isvalidIPrange(ip_addr, start_ip, end_ip)) {
                        PRINTF("ERR: IP_addr is out of DHCP range. \n");
                        status = ERR_ARG;
                        break;
                    }

                    if((end_ip - start_ip + 1) > DHCPS_MAX_LEASE) {
                        PRINTF("ERR: Failed to set range of IP_addr list(Max:%d).\n", DHCPS_MAX_LEASE);
                        status = ERR_ARG;
                        break;
                    }

                    /* Lease range */
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_START_IP, argv[n+1]);
#endif
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_END_IP, argv[n+2]);
#endif

                    dhcps_set_ip_range(argv[n+1], argv[n+2]);
                } else {
                    printf(">> DHCP IP RANGE invalid.\n");
                    status = ERR_ARG;
                }
            }
            break;
        } else if (strcasecmp(argv[n], "LEASE_TIME") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            if ((atoi(argv[n + 1]) < MIN_DHCP_SERVER_LEASE_TIME) || 
                (atoi(argv[n + 1]) > MAX_DHCP_SERVER_LEASE_TIME)) {
                goto dhcpd_help;
            }

            dhcps_time_t lease_time = (u32_t)atoi(argv[n + 1]);

            if (lease_time > 0) {
                /* Lease time */
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                            DHCP_SERVER_LEASE_TIME, (int)lease_time);
#endif

                dhcps_set_lease_time(lease_time);
            } else {
                PRINTF(">> DHCP Lease Time invalid.\n");
                status = ERR_ARG;
            }
            break;
        } else if (strcasecmp(argv[n], "LEASE") == 0) {
            /* Lease List */
            u8_t is_assigned = (u8_t)atoi(argv[n+1]);
            dhcps_print_lease_pool(is_assigned);
            break;
        }
#if defined ( __SUPPORT_MESH_PORTAL__ ) || defined (__SUPPORT_MESH__)
        else if (strcasecmp(argv[n], "DNS") == 0) {
            if (argc == 3
                && (get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_PORTAL || get_run_mode() == WIFI_DEVICE_MODE_EXT_MESH_POINT)) {
                if (is_in_valid_ip_class(argv[n+1])) {
                    ip4_addr_t dns_addr;
                    ip4addr_aton(argv[n+1], &dns_addr);
#ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, DHCP_SERVER_DNS, argv[n+1]);
#endif

                    dhcps_dns_setserver(&dns_addr);
                } else {
                    PRINTF("DHCP DNS address invalid.\n");
                    status = ERR_ARG;
                }
                break;
            }
            goto dhcpd_help;
        }
#endif // __SUPPORT_MESH_PORTAL__ || __SUPPORT_MESH__
        else if (strcasecmp(argv[n], "RESPONSE_DELAY") == 0) {
            continue;
        } else if (strcasecmp(argv[n + 1], "WLAN0") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            param->dhcps_interface = WLAN0_IFACE;
            continue;
        } else if (strcasecmp(argv[n + 1], "WLAN1") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            param->dhcps_interface = WLAN1_IFACE;
            continue;
        } else if (strcasecmp(argv[n], "BOOT") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            if (strcasecmp(argv[n + 1], "ON") == 0) {
#if (LWIP_DHCPS && LWIP_IPV4)
                param = pvPortMalloc(sizeof(dhcps_cmd_param));
                memset(param, 0, sizeof(dhcps_cmd_param));

                param->cmd = DHCP_SERVER_STATE_STOP;
                dhcps_run(param);

                vTaskDelay(100*5);
                
                param = pvPortMalloc(sizeof(dhcps_cmd_param));
                memset(param, 0, sizeof(dhcps_cmd_param));

                param->cmd = DHCP_SERVER_STATE_START;
                param->dhcps_interface = WLAN1_IFACE;
                dhcps_run(param);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                            NVR_KEY_DHCPD, pdTRUE);
#endif
#endif /*LWIP_DHCPS*/

            } else {
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, "USEDHCPD");
#endif
            }
            
            break;
        } else if (strcasecmp(argv[n], "START") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            param->cmd = DHCP_SERVER_STATE_START;
            param->dhcps_interface = WLAN1_IFACE;
            dhcps_run(param);
            return pdTRUE;
        } else if (strcasecmp(argv[n], "STOP") == 0) {
#ifdef __SUPPORT_P2P__
            switch (get_run_mode()) {
                case WIFI_DEVICE_MODE_EXT_P2P:
                case WIFI_DEVICE_MODE_EXT_P2P_GO:
                case WIFI_DEVICE_MODE_EXT_P2P_STATION:
                    goto dhcpd_help;
            }
#endif /* __SUPPORT_P2P__ */

            param->cmd = DHCP_SERVER_STATE_STOP;
            dhcps_run(param);
            return pdTRUE;
        } else {
            goto dhcpd_help;
        }
    }

    if (status != ERR_OK) {
dhcpd_help:
        usage_dhcpd();
    }

    if (param != NULL) {
        vPortFree(param);
    }

    return pdTRUE;
}
#endif // __SUPPORT_IPV4__

static bool cmd_network_config(int argc, char *argv[])
{
    int iface = WLAN0_IFACE;
    struct netif *netif;
#if LWIP_DHCP
    err_t status;
#endif /* LWIP_DHCP */

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    if ((argc > 1) && (argv[1][0] == '?')) {
help:

        printf( "\nUsage:\n"
                "\tifconfig [interface] [ipaddress] [subnet] [gateway]\t- set Netmode: Static IP\n"
                "\tifconfig [interface] [dhcp]\t\t- set Netmode: DHCP Client\n"
              );
        printf( "\tifconfig [interface] [up|down]\t\t- Network interface up/down\n"
                "\tifconfig [interface] [start|stop|renew|release] \t- DHCP Clinet cmd\n");
        printf( "\tifconfig [interface] [dns] [ipaddress]\t- set DNS IP\n"
                "\tifconfig [interface] [dns2] [ipaddress]\t- set 2nd DNS IP\n"
                "\tifconfig [interface]    \t\t- Display config [interface]\n");

        printf( "\tifconfig -a\t\t\t\t- Display full configuration information.\n"
                "\tifconfig help or ?\t\t\t- This message\n\n");
        return pdTRUE;
    }

    if (argc >= 2) {
        /* ifconfig [wlan0|wlan1|eth0|all|-a]    */
        if (strcasecmp(argv[1], "WLAN0") == 0) {        /* WLAN0 */
            iface = WLAN0_IFACE;
        } else if (strcasecmp(argv[1], "WLAN1") == 0) {    /* WLAN1 */
            iface = WLAN1_IFACE;
        } else if (strcasecmp(argv[1], "-A") == 0 || strcasecmp(argv[1], "ALL") == 0) {
            for (int i = WLAN0_IFACE; i <= WLAN1_IFACE; i++) {
                ra6w1_net_check(i, pdFALSE);
            }
            return pdTRUE;
        } else {
            goto help;
        }
    }

    switch (argc) {
        case 1: {
            /* ifconfig (All interfaces simplied) */
            for (int i = WLAN0_IFACE; i <= WLAN1_IFACE; i++) {
                ra6w1_net_check(i, pdTRUE);
            }

            break;
        }

        case 2: {
            ra6w1_net_check((int)iface, 0);
        }
        break;

        case 3: {
            /* ifconfig [wlan0|wlan1|eth0] [up|down]    */
            /* ifconfig [wlan0|wlan1|eth0] [dhcp]    */
            /* ifconfig [wlan0|wlan1|eth0] [renew]    */

            netif = netif_get_by_index((u8_t)(iface + 2));

            if (netif == NULL) {
                printf("WLAN%d interface = NULL\n", iface);
                return pdFAIL;
            }

            if (strcasecmp(argv[2], "UP") == 0) {            /* UP */
                wifi_netif_control(iface, IFACE_UP);
                iface_updown((UINT)iface, IFACE_UP);
            } else if (strcasecmp(argv[2], "DOWN") == 0) {    /* DOWN */
                wifi_netif_control(iface, IFACE_DOWN);
#if LWIP_DHCP
#ifdef __SUPPORT_IPV4__
            } else if (get_netmode(iface) == DHCPCLIENT) {
                /* DHCP Client  Renew */
                if (strcasecmp(argv[2], "RENEW") == 0) {
#if CFG_PMGR
                    if (RM_PMGR_W_dpm_is_wakeup() == pdTRUE) {
                        status = dhcp_start(netif);
                    } 
                    else 
#endif /* CFG_PMGR */
                    {
                        status = dhcp_renew(netif);
                    }
                } else if (strcasecmp(argv[2], "RELEASE") == 0) {
                    status = dhcp_release(netif);
                    printf("\nRelease DHCP Client :");

                    if (status != ERR_OK) {
                        printf(" Error WLAN%u.\n", iface);
                    } else {
                        printf(" Success WLAN%d.\n", iface);
                    }
                } else  if (strcasecmp(argv[2], "START") == 0) {
                    status = dhcp_start(netif);

                    if (status == ERR_OK) {
                        printf("\nDHCP Client Started WLAN%u.\n", iface);
                    } else {
                        printf("\nDHCP Client Start Error(%d) WLAN%u.\n", status, iface);
                    }
                } else if (strcasecmp(argv[2], "STOP") == 0) {
                    dhcp_stop(netif);
                } else if (strcasecmp(argv[2], "DHCP") == 0) {
                    printf("\nAlready DHCP Client Mode WLAN%u.\n", iface);
                } else {
                    goto help;
                }
            } else if (strncasecmp(argv[2], "DHCP", 4) == 0 && (iface == WLAN0_IFACE)) {
                /* Set DHCP Client */
                set_netmode((UCHAR)iface, DHCPCLIENT, TRUE);
                printf("\nDHCP Client Mode(WLAN0)\n");

                if (strcasecmp(argv[2], "DHCP_OFF") != 0) {
                    status = dhcp_start(netif);/* DHCP Client : START */
                }
#endif // __SUPPORT_IPV4__            
#endif /* LWIP_DHCP */
            } else {
                goto help;
            }

            break;
        }
#ifdef __SUPPORT_IPV4__
        case 4: {
            /* ifconfig [wlan0|wlan1|eth0] [dns] [DNS Server IP]     */
            ip4_addr_t ipaddr;

            if (strcasecmp(argv[2], "DNS") == 0) {
                ipaddr_aton(argv[3], (ip_addr_t *)&ipaddr);
                dns_setserver(0, (ip_addr_t *)&ipaddr);

#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_0 : WIFI_PROFILE_DNSSVR_1 , argv[3]);
#endif
                printf("DNS:%s \n", argv[3]);
            } else if (strcasecmp(argv[2], "DNS2") == 0) {
                ipaddr_aton(argv[3], (ip_addr_t *)&ipaddr);
                dns_setserver(1, (ip_addr_t *)&ipaddr);
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_2ND_0 : WIFI_PROFILE_DNSSVR_2ND_1, argv[3]);
#endif /* SET DNS */

                printf("DNS2:%s \n", argv[3]);
            } else {
                goto help;
            }

            break;
        }
#endif // __SUPPORT_IPV4__

        case 5: {
#ifdef    __SUPPORT_IPV6__
            /* ifconfig [wlan0|wlan1|eth0] [-6] dhcp [on|off] */
            if (strcasecmp(argv[3], "DHCP") == 0) {
                if (strcasecmp(argv[4], "ON") == 0) {
#ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                                "DHCPv6", 1);
#endif
                } else if (strcasecmp(argv[4], "OFF") == 0) {
#ifdef RM_MAP_PERSISTANT_W
                    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG,
                                                "DHCPv6", 0);
#endif
                }

                return pdTRUE;
            }
#endif    /* __SUPPORT_IPV6__ */

#ifdef __SUPPORT_IPV4__
            if (!is_in_valid_ip_class(argv[2]) || !isvalidip(argv[3]) || !is_in_valid_ip_class(argv[4])) {
                printf("\nInvalid Address!!\n");
                goto help;
                break;
            }

            if (ip_change(iface, argv[2], argv[3], argv[4], pdTRUE) == pdFAIL) {
                printf("\nInvalid Network Address!!\n");
                goto help;
                break;
            }

            printf("\n[WLAN%d]\n" \
                   "NetMode\t\t:Static IP\n" \
                   "IP Address\t:%s\n" \
                   "Mask\t\t:%s\n" \
                   "Gateway\t\t:%s\n",
                   iface, argv[2], argv[3], argv[4]);
#endif // __SUPPORT_IPV4__

#ifdef __SUPPORT_DHCPC_IP_TO_STATIC_IP__
#ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_SYSCFG, ENV_KEY_TEMP_STATIC_IP);
#endif
#endif /* __SUPPORT_DHCPC_IP_TO_STATIC_IP__ */
            break;
        }

        default: {
            goto help;
        }
    }

    return pdTRUE;
}

bool cmd_network_config_wrapper(int argc, char *argv[])
{
    cmd_network_config(argc, argv);

    return true;
}

static bool cmd_netstats(int argc, char *argv[])
{
    stats_opt option = stats_all;

    if (argc == 1) {
        option = stats_ipv4;
    } else if (argc == 2) {
        if (strcmp(argv[1], "all") == 0) {
            option = stats_all;
        } else if (strcmp(argv[1], "ipv4") == 0) {
            option = stats_ipv4;
        } else if (strcmp(argv[1], "ip") == 0) {
            option = stats_ip;
        } else if (strcmp(argv[1], "udp") == 0) {
            option = stats_udp;
        } else if (strcmp(argv[1], "tcp") == 0) {
            option = stats_tcp;
        } else if (strcmp(argv[1], "icmp") == 0) {
            option = stats_icmp;
        } else if (strcmp(argv[1], "arp") == 0) {
            option = stats_arp;
        } else if (strcmp(argv[1], "ipfrag") == 0) {
            option = stats_ipfrag;
#ifdef __SUPPORT_IPV6__
        } else if (strcmp(argv[1], "ipv6") == 0) {
            option = stats_ipv6;
        } else if (strcmp(argv[1], "ip6") == 0) {
            option = stats_ip;
        } else if (strcmp(argv[1], "nd6") == 0) {
            option = stats_nd6;
        } else if (strcmp(argv[1], "mld6") == 0) {
            option = stats_mld6;
        } else if (strcmp(argv[1], "icmp6") == 0) {
            option = stats_icmp6;
        } else if (strcmp(argv[1], "ip6frag") == 0) {
            option = stats_ip6frag;
#endif /* __SUPPORT_IPV6__ */
        } else if (strcmp(argv[1], "link") == 0) {
            option = stats_link;
        } else if (strcmp(argv[1], "mem") == 0) {
            option = stats_memory;
        } else if (strcmp(argv[1], "sys") == 0) {
            option = stats_system;
        } else if (strcmp(argv[1], "help") == 0) {
            goto help;
        } else {
            goto help;
        }
    } 

    if (argc == 1 || argc == 2) {
        rm_lwip_w_stats_display(option);
    }

    if (argc == 3 && strcmp(argv[2], "reset") == 0) {
        if (strcmp(argv[1], "all") == 0) {
            option = stats_all;
        } else if (strcmp(argv[1], "ipv4") == 0) {
            option = stats_ipv4;
        } else if (strcmp(argv[1], "ip") == 0) {
            option = stats_ip;
        } else if (strcmp(argv[1], "udp") == 0) {
            option = stats_udp;
        } else if (strcmp(argv[1], "tcp") == 0) {
            option = stats_tcp;
        } else if (strcmp(argv[1], "icmp") == 0) {
            option = stats_icmp;
        } else if (strcmp(argv[1], "arp") == 0) {
            option = stats_arp;
        } else if (strcmp(argv[1], "ipfrag") == 0) {
            option = stats_ipfrag;
#ifdef __SUPPORT_IPV6__
        } else if (strcmp(argv[1], "ipv6") == 0) {
            option = stats_ipv6;
        } else if (strcmp(argv[1], "ip6") == 0) {
            option = stats_ip;
        } else if (strcmp(argv[1], "nd6") == 0) {
            option = stats_nd6;
        } else if (strcmp(argv[1], "mld6") == 0) {
            option = stats_mld6;
        } else if (strcmp(argv[1], "icmp6") == 0) {
            option = stats_icmp6;
        } else if (strcmp(argv[1], "ip6frag") == 0) {
            option = stats_ip6frag;
#endif /* __SUPPORT_IPV6__ */
        } else if (strcmp(argv[1], "link") == 0) {
            option = stats_link;
        } else if (strcmp(argv[1], "mem") == 0 || strcmp(argv[1], "sys") == 0) {
            printf("\nReset is not supported for %s\n", argv[1]);
            goto help;
        } else {
            goto help;
        }

        rm_lwip_w_stats_reset(option);
    }

    if (   ( (argc > 2) && (argv[1][0] == '?') )
        || ( (argc == 2) && (strcmp(argv[1], "help") == 0) ) ) {
help:
        printf("\nUsage:\n"
                "\t%s [option1] [option2]\n"
                "\t   option1: [all|ipv4|ip|udp|tcp|icmp|arp|ipfrag"
#ifdef __SUPPORT_IPV6__
                "|ipv6|ip6|nd6|mld6|icmp6|ip6frag"
#endif /* __SUPPORT_IPV6__ */
                "|link|mem|sys|help|?]\n"
                "\t   option2: [reset|<none>]", argv[0]);
    }

    return pdTRUE;
}


#ifdef __SUPPORT_IPV6__
extern void nd6_display_neighbor_cache_netif(struct netif *netif);
extern void nd6_cleanup_netif(struct netif *netif);
static bool cmd_ipv6_ndp(int argc, char *argv[])
{
    struct netif *wlan0_netif = netif_get_by_index(WLAN0_IFACE + 2);
    struct netif *wlan1_netif = netif_get_by_index(WLAN1_IFACE + 2);

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdFALSE;
    }

    if (argc == 2 && strcmp(argv[1], "delete") == 0) {
        nd6_cleanup_netif(wlan0_netif);
        printf("Delete neighbor cache\n");
    }
    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        printf("ndp [option]\n"
               "\toption: [delete|help]\n");
    } else if(argc == 1) {
        if (wlan0_netif) {
            printf("\nWLAN0\n");
            nd6_display_neighbor_cache_netif(wlan0_netif);
        }

        if (wlan1_netif) {
            printf("\nWLAN1\n");
            nd6_display_neighbor_cache_netif(wlan1_netif);
        }
    }

    return pdTRUE;
}
#endif // __SUPPORT_IPV6__

bool cmd_supp_stop(int argc, char *argv[])
{
    int    ret;

    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    ret = request_stop_supplicant();
    if (!ret) {
        printf(" Request the supplicant to stop \n");
    }

    return pdTRUE;
}

bool cmd_supp_start(int argc, char *argv[])
{
    int    ret;

    RA6W1_UNUSED_ARG(argc);
    RA6W1_UNUSED_ARG(argv);

    if (!ra6w1_network_main_is_wlaninit()) {
        printf("Wi-Fi is not initialized.\n");
        return pdTRUE;
    }

    ret = request_start_supplicant();
    if (!ret) {
        printf(" Request the supplicant to start \n");
    }

    return pdTRUE;
}

#undef  SKIP_DELIMETER
int make_message(char *title, char *buf, size_t buflen)
{
    char ch;
    unsigned int msglen = 0;
#ifdef  SKIP_DELIMETER
    int skip = FALSE;
#endif  //SKIP_DELIMETER
#if defined(__SUPPORT_APP_CONSOLE_INPUT__)
    int app_ch;
#endif
    TaskStatus_t xTaskDetails;

    if (buf == NULL)
    {
        printf("[%s]Invalid buffer size\n", __func__);
        return -1;
    }

    printf("Typing data: (%s)\n\tCancel - CTRL+D, "
           "End of Input - CTRL+C or CTRL+Z\n" , title);

    vTaskGetInfo(cli_handle_task, &xTaskDetails, pdTRUE, eInvalid);
    if (xTaskDetails.eCurrentState != eSuspended)
    {
        app_request_console_input_access();
    }

    while (1)
    {
        if (xTaskDetails.eCurrentState != eSuspended)
        {
            app_ch = getchar_nowait();
            if (app_ch == -1)
            {
                vTaskDelay(portCONVERT_MS_2_TICKS(5));
                continue;
            }

            ch = (char) app_ch;
        }
        else
        {
            ch = (char) getchar();
        }

        /* Check CTRL+C, CTRL+D, CTRL+Z. */
        if (ch == 0x03 || ch == 0x04 || ch == 0x1a)
        { 
            break;
        }

#ifdef SKIP_DELIMETER
        if (ch == '[')
        {
            skip = TRUE;
            continue;
        }
        else if (ch == ']')
        {
            skip = FALSE;
            continue;
        }
        else if (skip == TRUE)
        {
            continue;
        }
#endif //SKIP_DELIMETER

        if (ch == 0x0D)
        {
            ch = 0x0A;
        }

        /* Local Echo */
        putchar(ch);

        msglen++;

        if (msglen > (buflen - 1))
        {
            printf("\nToo long input data (MAX Length : %d byte)\n", buflen - 1);

            if (xTaskDetails.eCurrentState != eSuspended)
            {
                app_release_console_input_access();
            }
            return -1;
        }

        buf[(msglen-1)] = (char) ch;
    }

    /* Check CTRL+C, CTRL+Z */
    if (ch == 0x03 || ch == 0x1a)
    { 
        buf[msglen] = '\0';
    }
    else
    {
        /* Check CTRL+D and Cancel input*/
        buf[0] = '\0';
        msglen = 0;
    }

#if defined(__SUPPORT_APP_CONSOLE_INPUT__)
    if (xTaskDetails.eCurrentState != eSuspended)
    {
        app_release_console_input_access();
    }
#endif // __SUPPORT_APP_CONSOLE_INPUT__

    return (int) msglen;
}

enum
{
    CA_CERT0 = 0,   // For MQTTs CLI
    CLIENT_CERT0,
    CLIENT_KEY0,
    DH_PARAM0,
    CA_CERT1,       // For HTTPs CLI
    CLIENT_CERT1,
    CLIENT_KEY1,
    DH_PARAM1,
    CA_CERT2,       // For Enterprise(802.1x)
    CLIENT_CERT2,
    CLIENT_KEY2,
    DH_PARAM2,
    CA_CERT3,        // For OTA
    CLIENT_CERT3,
    CLIENT_KEY3,
    DH_PARAM3,
    CA_CERT4,        // For HTTPs SVR
    CLIENT_CERT4,
    CLIENT_KEY4,
    DH_PARAM4, 
    CA_CERT6,        // For AWS
    INITIAL_CERT6,
    INITIAL_KEY6,
    UNIQUE_CERT6,
    UNIQUE_KEY6,
    CD7,             // For Matter
    DAC_CERT7,
    PAI_CERT7,
    DAC_KEY7,
    DAC_PUB7,
    CA_CERT8,        // For Miscellaneous Application 1
    CLIENT_CERT8,
    CLIENT_KEY8,
    DH_PARAM8,
    EXCH_PARAM8,
    CA_CERT9,        // For Miscellaneous Application 2
    CLIENT_CERT9,
    CLIENT_KEY9,
    DH_PARAM9,
    EXCH_PARAM9,
    CA_CERT10,        // For Miscellaneous Application 3
    CLIENT_CERT10,
    CLIENT_KEY10,
    DH_PARAM10,
    EXCH_PARAM10,
    CA_CERT11,        // For Miscellaneous Application 4
    CLIENT_CERT11,
    CLIENT_KEY11,
    DH_PARAM11,
    EXCH_PARAM11,
    CA_CERT12,        // For Miscellaneous Application 5
    CLIENT_CERT12,
    CLIENT_KEY12,
    DH_PARAM12,
    EXCH_PARAM12,
    CA_CERT13,        // For Miscellaneous Application 6
    CLIENT_CERT13,
    CLIENT_KEY13,
    DH_PARAM13,
    EXCH_PARAM13,
    CA_CERT14,        // For Miscellaneous Application 7
    CLIENT_CERT14,
    CLIENT_KEY14,
    DH_PARAM14,
    EXCH_PARAM14,
    CA_CERT15,        // For Miscellaneous Application 8
    CLIENT_CERT15,
    CLIENT_KEY15,
    DH_PARAM15,
    EXCH_PARAM15,
    TLS_CERT_01 = 100,
    TLS_CERT_02,
    TLS_CERT_03,
    TLS_CERT_04,
    TLS_CERT_05,
    TLS_CERT_06,
    TLS_CERT_07,
    TLS_CERT_08,
    TLS_CERT_09,
    TLS_CERT_10,
    TLS_CERT_11,
    TLS_CERT_12,
    TLS_CERT_13,
    TLS_CERT_14,
    TLS_CERT_15,
    TLS_CERT_16,    
    CERT_ALL,
    CERT_END,
};

enum
{
    ACT_NONE = 0,
    ACT_WRITE,
    ACT_READ,
    ACT_DELETE,
    ACT_STATUS
};

/*
certificate read/write/delete/status
*/

int cert_rwds(char action, char dest)
{
    extern int make_message(char *title, char *buf, size_t buflen);

    int status = 0;
    char *buffer = NULL;
    unsigned int sflash_addr[] = {
        SF_TLS_CERT_MQTT_CLI_CA_ADDR,               // #0
        SF_TLS_CERT_MQTT_CLI_CERTIFICATE_ADDR,
        SF_TLS_CERT_MQTT_CLI_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MQTT_CLI_DH_PARAMETER_ADDR,        
        SF_TLS_CERT_HTTPS_CLI_CA_ADDR,              // #1
        SF_TLS_CERT_HTTPS_CLI_CERTIFICATE_ADDR,
        SF_TLS_CERT_HTTPS_CLI_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_HTTPS_CLI_DH_PARAMETER_ADDR,
        SF_TLS_CERT_WPA_ENT_CA_ADDR,                // #2
        SF_TLS_CERT_WPA_ENT_CERTIFICATE_ADDR,
        SF_TLS_CERT_WPA_ENT_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_WPA_ENT_DH_PARAMETER_ADDR,
        SF_TLS_CERT_OTA_CA_ADDR,                    // #3
        SF_TLS_CERT_OTA_CERTIFICATE_ADDR,
        SF_TLS_CERT_OTA_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_OTA_DH_PARAMETER_ADDR,
        SF_TLS_CERT_HTTPS_SVR_CA_ADDR,              // #4
        SF_TLS_CERT_HTTPS_SVR_CERTIFICATE_ADDR,
        SF_TLS_CERT_HTTPS_SVR_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_HTTPS_SVR_DH_PARAMETER_ADDR,
        SF_TLS_CERT_AWS_CA_ADDR,                    // #6
        SF_TLS_CERT_AWS_INITIAL_CERT_ADDR,
        SF_TLS_CERT_AWS_INITIAL_PRIV_KEY_ADDR,
        SF_TLS_CERT_AWS_UNIQUE_CERT_ADDR,
        SF_TLS_CERT_AWS_UNIQUE_PRIV_KEY_ADDR,
        SF_MATTER_CERT_CD_ADDR,                     // #7
        SF_MATTER_CERT_DAC_CERTIFICATE_ADDR,
        SF_MATTER_CERT_PAI_CERTIFICATE_ADDR,
        SF_MATTER_CERT_DAC_PRIVATE_KEY_ADDR,
        SF_MATTER_CERT_DAC_PUBLIC_KEY_ADDR,
        SF_TLS_CERT_MISC1_CA_ADDR,                   // #8
        SF_TLS_CERT_MISC1_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC1_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC1_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC1_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC2_CA_ADDR,                   // #9
        SF_TLS_CERT_MISC2_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC2_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC2_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC2_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC3_CA_ADDR,                   // #10
        SF_TLS_CERT_MISC3_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC3_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC3_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC3_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC4_CA_ADDR,                   // #11
        SF_TLS_CERT_MISC4_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC4_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC4_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC4_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC5_CA_ADDR,                   // #12
        SF_TLS_CERT_MISC5_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC5_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC5_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC5_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC6_CA_ADDR,                   // #13
        SF_TLS_CERT_MISC6_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC6_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC6_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC6_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC7_CA_ADDR,                   // #14
        SF_TLS_CERT_MISC7_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC7_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC7_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC7_EXCHANGE_ADDR,
        SF_TLS_CERT_MISC8_CA_ADDR,                   // #15
        SF_TLS_CERT_MISC8_CERTIFICATE_ADDR,
        SF_TLS_CERT_MISC8_PRIVATE_KEY_ADDR,
        SF_TLS_CERT_MISC8_DH_PARAMETER_ADDR,
        SF_TLS_CERT_MISC8_EXCHANGE_ADDR
    };

    // Status
    if (action == ACT_STATUS || action == ACT_READ) {
        rm_cert_format_t format = RM_CERT_FORMAT_NONE;
        size_t outlen = CERT_MAX_LENGTH;
        buffer = pvPortMalloc(CERT_MAX_LENGTH);
        if (buffer == NULL) {
            printf("[%s] Failed to allocate memory to read certificate\n", __func__);
            return pdFALSE;
        }
        memset(buffer, 0xff, CERT_MAX_LENGTH);

        status = RM_CERT_Read(RM_CERT_GetModule(sflash_addr[(int)dest]), 
                         RM_CERT_GetType(sflash_addr[(int)dest]), 
                         &format, (unsigned char *)buffer, &outlen);
        
        if ((UCHAR)(buffer[0]) != 0xFF && status == 0) {
            if (action == ACT_READ) {
#if defined(__SUPPORT_APP_CONSOLE_INPUT__) & CFG_CLI
                app_request_console_input_access();
#endif

                rm_stdio_w_locked_write(buffer);

#if defined(__SUPPORT_APP_CONSOLE_INPUT__) & CFG_CLI
                app_release_console_input_access();
#endif
            }

            memset(buffer, 0, CERT_MAX_LENGTH);
            vPortFree(buffer);
            buffer = NULL;
            return pdPASS;
        } else {
            if (action == ACT_READ) {
                printf("Empty\n");
            }

            memset(buffer, 0, CERT_MAX_LENGTH);
            vPortFree(buffer);
            buffer = NULL;
            return pdFAIL;
        }
    } else if (action == ACT_WRITE) {
        int cert_len = 0;
        rm_cert_format_t format = RM_CERT_FORMAT_PEM;
        
        buffer = pvPortCalloc(FLASH_WRITE_LENGTH, sizeof(char));

        if (buffer == NULL) {
            printf("[%s] Failed to allocate memory to write certificate\n", __func__);
            return pdFALSE;
        }

        cert_len = make_message("Certificate value", buffer, FLASH_WRITE_LENGTH);

        if (cert_len > 0) {

            if(RM_CERT_IsPemFormat(buffer) == pdTRUE)
            {
                format = RM_CERT_FORMAT_PEM;
            } else if(RM_CERT_IsDerFormat(buffer) == pdTRUE)
            {
                format = RM_CERT_FORMAT_DER;
            }

            if ((format != RM_CERT_FORMAT_PEM) && (format != RM_CERT_FORMAT_DER)){
                printf("\n\nNot pem or DER format\n");
                status = -1;
            } else {
                status = RM_CERT_Write(RM_CERT_GetModule(sflash_addr[(int)dest]), 
                                       RM_CERT_GetType(sflash_addr[(int)dest]),
                                       format, (unsigned char *)buffer, (size_t)cert_len);
            }
        } else {
            printf("\n\nCanceled\n");
            status = -1;
        }

        if (buffer) {
            vPortFree(buffer);
        }
        
        buffer = NULL;

        if (status == 0) {
            return pdPASS;
        }

        return pdFAIL;
    } else if (action == ACT_DELETE) {
            if (dest == CERT_ALL) {
                /* for EAP-FAST */
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "fast_pac");
#endif
#ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFICFG, "fast_pac_len");
#endif

                status = RM_CERT_DeleteAll();
            } else {
                status = RM_CERT_Delete(RM_CERT_GetModule(sflash_addr[(int)dest]), 
                                            RM_CERT_GetType(sflash_addr[(int)dest]));
            }
        if (status == 0) {
            return pdPASS;
        } else {
            return pdFAIL;
        }
    }

    return pdFAIL;
}

static bool cmd_certificate(int argc, char *argv[])
{
    int dest = CERT_END;
    char action = ACT_NONE;
    int status = 0;

#if defined (__SUPPORT_ATCMD_TLS__)
    atcmd_cm_cert_t *atcmd_cert = NULL;
    int atcmd_cert_idx = 0;
#endif // __SUPPORT_ATCMD_TLS__

    // Help
    if (argc == 1 || argc > 3 || strcasecmp(argv[1], "HELP") == 0) {
help:
        printf("\ncert [action] [dest]\n\n");

        printf("  action=[status|read|write|del|enable_verify_ent|disable_verify_ent|status_verify_ent|help]\n");
        printf("  dest  =[ca#|cert#|key#|dh#|all]\n");
        
        printf("\n\t[action]\n" \
                "\tstatus\tCertificate Status for <dest>\n" \
                "\tread\tCertificate Read\n" \
                "\twrite\tCertificate Write\n" \
                "\tdel\tCertificate Delete\n");
        printf("\tenable_verify_ent\tEnable enterprise validation flag (stateless)\n");
        printf("\tdisable_verify_ent\tDisable enterprise validation flag (stateless)\n");
        printf("\tstatus_verify_ent\tEnterprise validation flag status\n");

        printf("\thelp\tThis Message\n\n");

        printf("\t[dest]\n" \
                "\tca#\tRoot CA (#0~15, except 5,7)\n" \
                "\tcert#\tServer/Client Certif,icate (#0~15, except 5,6,7)\n" \
                "\tkey#\tPrivate Key (#0~15, except 5,6,7)\n" \
                "\tdh#\tDH Parameter (#0~15, except 5,6,7)\n" \
                "\texc#\tExchange parameter (#15)\n");

        if ( (CERT_AWS_USED == 1) )
        {
            printf("\tinitcert#\tInitial Certificate(#6)\n" \
                    "\tinitkey#\tInitial Key (#6)\n" \
                    "\tuniqcert#\tUnique Certificate(#6)\n" \
                    "\tuniqkey#\tUnique Key (#6)\n");
        }

        if ( (CERT_MATTER_USED == 1) )
        {
            printf("\tcd#\tCertificate Declaration (#7)\n" \
                    "\tdac#\tDAC Certificate (#7)\n" \
                    "\tpai#\tPAI Certificate (#7)\n" \
                    "\tpriv#\tDAC Private Key (#7)\n" \
                    "\tpub#\tDAC Public Key (#7)\n");
        }

        printf("\tall\tAll(Certificate #0~#15); status/del only\n\n");
        printf("\tAvailable #:\n");

        if (CERT_MQTTS_CLI_USED == 1) {
            printf("\t \t0: MQTT Client\n");
        }        
        if (CERT_HTTPS_CLI_USED == 1) {
            printf("\t \t1: HTTPs Client\n");
        }
        if (CERT_WPA_ENT_USED == 1) {
            printf("\t \t2: Enterprise (802.1x)\n");
        }
        if (CERT_OTA_USED == 1) {
            printf("\t \t3: OTA\n");
        }
        if (CERT_HTTPS_SVR_USED == 1) {
            printf("\t \t4: HTTPs Server\n");
        }
        if (CERT_AWS_USED == 1) {
            printf("\t \t6: AWS\n");
        }
        if (CERT_MATTER_USED == 1) {
            printf("\t \t7: Matter\n");
        }
        if (CERT_MISC1_USED == 1) {
            printf("\t \t8: Miscellaneous Application 1\n");
        }
        if (CERT_MISC2_USED == 1) {
            printf("\t \t9: Miscellaneous Application 2\n");
        }
        if (CERT_MISC3_USED == 1) {
            printf("\t \t10: Miscellaneous Application 3\n");
        }
        if (CERT_MISC4_USED == 1) {
            printf("\t \t11: Miscellaneous Application 4\n");
        }
        if (CERT_MISC5_USED == 1) {
            printf("\t \t12: Miscellaneous Application 5\n");
        }
        if (CERT_MISC6_USED == 1) {
            printf("\t \t13: Miscellaneous Application 6\n");
        }
        if (CERT_MISC7_USED == 1) {
            printf("\t \t14: Miscellaneous Application 7\n");
        }
        if (CERT_MISC8_USED == 1) {
            printf("\t \t15: Miscellaneous Application 8\n");
        }

        printf("\tverify value (For Enterprise certificate Only)\n" \
              "\t  0x1   Validation expiration time check\n" \
              "\t  0x200 Validation start time check\n\n");

        return pdTRUE;
    }

    if (argc == 2 && strcasecmp(argv[1], "STATUS_VERIFY_ENT") == 0) {
        printf("0x%0x\n" \
          "  0x1   Validation Expiration Time Check\n" \
          "  0x200 Validation start time check item\n", get_ent_cert_verify_flags());
        return pdTRUE;
    } else if (argc == 3 && (strstr(argv[1], "VERIFY_ENT") != 0 || strstr(argv[1], "verify_ent") != 0)) {
        // ENABLE_VERIFY_ENT, DISABLE_VERIFY_ENT
        char flag = pdTRUE;

        dest = (int)htoi(argv[2]);

        if (!(dest == 0x1 || dest == 0x200 || dest == 0x201)) {
            goto help;
        }

        if (strcasecmp(argv[1], "DISABLE_VERIFY_ENT") == 0) {
            flag = pdFALSE;        
        }

        set_ent_cert_verify_flags(flag, dest);
        printf("0x%0x\n", get_ent_cert_verify_flags());
        return pdTRUE;
    }

    // Action
    if (strcasecmp(argv[1], "DEL") == 0) {
        action = ACT_DELETE;
    } else if (strcasecmp(argv[1], "STATUS") == 0) {
        action = ACT_STATUS;
        dest = CERT_ALL;
    } else if (strcasecmp(argv[1], "WRITE") == 0) {
        action = ACT_WRITE;
    } else if (strcasecmp(argv[1], "READ") == 0) {
        action = ACT_READ;
    } else {
        printf("\nwrong [action]\n\n");
        goto help;
    }

    if (argc != 3) {
        printf("\n [dest] missing !! \n\n");
        goto help;        
    }

    // Dest
    // cert0 MQTT TLS Client
    if (strcasecmp(argv[2], "CA0") == 0 && CERT_MQTTS_CLI_USED == 1) {
        dest = CA_CERT0;
    } else if (strcasecmp(argv[2], "CERT0") == 0 && CERT_MQTTS_CLI_USED == 1) {
        dest = CLIENT_CERT0;
    } else if (strcasecmp(argv[2], "KEY0") == 0 && CERT_MQTTS_CLI_USED == 1) {
        dest = CLIENT_KEY0;
    } else if (strcasecmp(argv[2], "DH0") == 0 && CERT_MQTTS_CLI_USED == 1) {
        dest = DH_PARAM0;
    }
    // cert1 HTTPs Client
    else if (strcasecmp(argv[2], "CA1") == 0 && CERT_HTTPS_CLI_USED == 1) {
        dest = CA_CERT1;
    } else if (strcasecmp(argv[2], "CERT1") == 0 && CERT_HTTPS_CLI_USED == 1) {
        dest = CLIENT_CERT1;
    } else if (strcasecmp(argv[2], "KEY1") == 0 && CERT_HTTPS_CLI_USED == 1) {
        dest = CLIENT_KEY1;
    } else if (strcasecmp(argv[2], "DH1") == 0 && CERT_HTTPS_CLI_USED == 1) {
        dest = DH_PARAM1;
    }
    // cert2 Enterprise(802.1x)
    else if (strcasecmp(argv[2], "CA2") == 0 && CERT_WPA_ENT_USED == 1) {
        dest = CA_CERT2;
    } else if (strcasecmp(argv[2], "CERT2") == 0 && CERT_WPA_ENT_USED == 1) {
        dest = CLIENT_CERT2;
    } else if (strcasecmp(argv[2], "KEY2") == 0 && CERT_WPA_ENT_USED == 1) {
        dest = CLIENT_KEY2;
    } else if (strcasecmp(argv[2], "DH2") == 0 && CERT_WPA_ENT_USED == 1) {
        dest = DH_PARAM2;
    }
    // cert3 OTA
    else if (strcasecmp(argv[2], "CA3") == 0 && CERT_OTA_USED == 1) {
        dest = CA_CERT3;
    } else if (strcasecmp(argv[2], "CERT3") == 0 && CERT_OTA_USED == 1) {
        dest = CLIENT_CERT3;
    } else if (strcasecmp(argv[2], "KEY3") == 0 && CERT_OTA_USED == 1) {
        dest = CLIENT_KEY3;
    } else if (strcasecmp(argv[2], "DH3") == 0 && CERT_OTA_USED == 1) {
        dest = DH_PARAM3;
    }
    // cert4 HTTPs Server
    else if (strcasecmp(argv[2], "CA4") == 0 && CERT_HTTPS_SVR_USED == 1) {
        dest = CA_CERT4;
    } else if (strcasecmp(argv[2], "CERT4") == 0 && CERT_HTTPS_SVR_USED == 1) {
        dest = CLIENT_CERT4;
    } else if (strcasecmp(argv[2], "KEY4") == 0 && CERT_HTTPS_SVR_USED == 1) {
        dest = CLIENT_KEY4;
    } else if (strcasecmp(argv[2], "DH4") == 0 && CERT_HTTPS_SVR_USED == 1) {
        dest = DH_PARAM4;
    }
    // cert6 AWS
    else if (strcasecmp(argv[2], "CA6") == 0 && CERT_AWS_USED == 1) {
        dest = CA_CERT6;
    } else if (strcasecmp(argv[2], "INITCERT6") == 0 && CERT_AWS_USED == 1) {
        dest = INITIAL_CERT6;
    } else if (strcasecmp(argv[2], "INITKEY6") == 0 && CERT_AWS_USED == 1) {
        dest = INITIAL_KEY6;
    } else if (strcasecmp(argv[2], "UNIQCERT6") == 0 && CERT_AWS_USED == 1) {
        dest = INITIAL_CERT6;
    } else if (strcasecmp(argv[2], "UNIQKEY6") == 0 && CERT_AWS_USED == 1) {
        dest = INITIAL_KEY6;
    }
    // cert7 MATTER
    else if (strcasecmp(argv[2], "CD7") == 0 && CERT_MATTER_USED == 1) {
        dest = CD7;
    } else if (strcasecmp(argv[2], "DAC7") == 0 && CERT_MATTER_USED == 1) {
        dest = DAC_CERT7;
    } else if (strcasecmp(argv[2], "PAI7") == 0 && CERT_MATTER_USED == 1) {
        dest = PAI_CERT7;
    } else if (strcasecmp(argv[2], "PRIV7") == 0 && CERT_MATTER_USED == 1) {
        dest = DAC_KEY7;
    } else if (strcasecmp(argv[2], "PUB7") == 0 && CERT_MATTER_USED == 1) {
        dest = DAC_PUB7;
    }
    // cert8 MISCELLLANEOUS APPLICATION 1
    else if (strcasecmp(argv[2], "CA8") == 0 && CERT_MISC1_USED == 1) {
        dest = CA_CERT8;
    } else if (strcasecmp(argv[2], "CERT8") == 0 && CERT_MISC1_USED == 1) {
        dest = CLIENT_CERT8;
    } else if (strcasecmp(argv[2], "KEY8") == 0 && CERT_MISC1_USED == 1) {
        dest = CLIENT_KEY8;
    } else if (strcasecmp(argv[2], "DH8") == 0 && CERT_MISC1_USED == 1) {
        dest = DH_PARAM8;
    } else if (strcasecmp(argv[2], "EXC8") == 0 && CERT_MISC1_USED == 1) {
        dest = EXCH_PARAM8;
    }
    // cert8 MISCELLLANEOUS APPLICATION 2
    else if (strcasecmp(argv[2], "CA9") == 0 && CERT_MISC2_USED == 1) {
        dest = CA_CERT9;
    } else if (strcasecmp(argv[2], "CERT9") == 0 && CERT_MISC2_USED == 1) {
        dest = CLIENT_CERT9;
    } else if (strcasecmp(argv[2], "KEY9") == 0 && CERT_MISC2_USED == 1) {
        dest = CLIENT_KEY9;
    } else if (strcasecmp(argv[2], "DH9") == 0 && CERT_MISC2_USED == 1) {
        dest = DH_PARAM9;
    } else if (strcasecmp(argv[2], "EXC9") == 0 && CERT_MISC2_USED == 1) {
        dest = EXCH_PARAM9;
    }
    // cert8 MISCELLLANEOUS APPLICATION 3
    else if (strcasecmp(argv[2], "CA10") == 0 && CERT_MISC3_USED == 1) {
        dest = CA_CERT10;
    } else if (strcasecmp(argv[2], "CERT10") == 0 && CERT_MISC3_USED == 1) {
        dest = CLIENT_CERT10;
    } else if (strcasecmp(argv[2], "KEY10") == 0 && CERT_MISC3_USED == 1) {
        dest = CLIENT_KEY10;
    } else if (strcasecmp(argv[2], "DH10") == 0 && CERT_MISC3_USED == 1) {
        dest = DH_PARAM10;
    } else if (strcasecmp(argv[2], "EXC10") == 0 && CERT_MISC3_USED == 1) {
        dest = EXCH_PARAM10;
    }
    // cert11 MISCELLLANEOUS APPLICATION 4
    else if (strcasecmp(argv[2], "CA11") == 0 && CERT_MISC4_USED == 1) {
        dest = CA_CERT11;
    } else if (strcasecmp(argv[2], "CERT11") == 0 && CERT_MISC4_USED == 1) {
        dest = CLIENT_CERT11;
    } else if (strcasecmp(argv[2], "KEY11") == 0 && CERT_MISC4_USED == 1) {
        dest = CLIENT_KEY11;
    } else if (strcasecmp(argv[2], "DH11") == 0 && CERT_MISC4_USED == 1) {
        dest = DH_PARAM11;
    } else if (strcasecmp(argv[2], "EXC11") == 0 && CERT_MISC4_USED == 1) {
        dest = EXCH_PARAM11;
    }
    // cert12 MISCELLLANEOUS APPLICATION 5
    else if (strcasecmp(argv[2], "CA12") == 0 && CERT_MISC5_USED == 1) {
        dest = CA_CERT12;
    } else if (strcasecmp(argv[2], "CERT12") == 0 && CERT_MISC5_USED == 1) {
        dest = CLIENT_CERT12;
    } else if (strcasecmp(argv[2], "KEY12") == 0 && CERT_MISC5_USED == 1) {
        dest = CLIENT_KEY12;
    } else if (strcasecmp(argv[2], "DH12") == 0 && CERT_MISC5_USED == 1) {
        dest = DH_PARAM12;
    } else if (strcasecmp(argv[2], "EXC12") == 0 && CERT_MISC5_USED == 1) {
        dest = EXCH_PARAM12;
    }
    // cert13 MISCELLLANEOUS APPLICATION 6
    else if (strcasecmp(argv[2], "CA13") == 0 && CERT_MISC6_USED == 1) {
        dest = CA_CERT13;
    } else if (strcasecmp(argv[2], "CERT13") == 0 && CERT_MISC6_USED == 1) {
        dest = CLIENT_CERT13;
    } else if (strcasecmp(argv[2], "KEY13") == 0 && CERT_MISC6_USED == 1) {
        dest = CLIENT_KEY13;
    } else if (strcasecmp(argv[2], "DH13") == 0 && CERT_MISC6_USED == 1) {
        dest = DH_PARAM13;
    } else if (strcasecmp(argv[2], "EXC13") == 0 && CERT_MISC6_USED == 1) {
        dest = EXCH_PARAM13;
    }
    // cert14 MISCELLLANEOUS APPLICATION 7
    else if (strcasecmp(argv[2], "CA14") == 0 && CERT_MISC7_USED == 1) {
        dest = CA_CERT14;
    } else if (strcasecmp(argv[2], "CERT14") == 0 && CERT_MISC7_USED == 1) {
        dest = CLIENT_CERT14;
    } else if (strcasecmp(argv[2], "KEY14") == 0 && CERT_MISC7_USED == 1) {
        dest = CLIENT_KEY14;
    } else if (strcasecmp(argv[2], "DH14") == 0 && CERT_MISC7_USED == 1) {
        dest = DH_PARAM14;
    } else if (strcasecmp(argv[2], "EXC14") == 0 && CERT_MISC7_USED == 1) {
        dest = EXCH_PARAM14;
    }
    // cert15 MISCELLLANEOUS APPLICATION 8
    else if (strcasecmp(argv[2], "CA15") == 0 && CERT_MISC8_USED == 1) {
        dest = CA_CERT15;
    } else if (strcasecmp(argv[2], "CERT15") == 0 && CERT_MISC8_USED == 1) {
        dest = CLIENT_CERT15;
    } else if (strcasecmp(argv[2], "KEY15") == 0 && CERT_MISC8_USED == 1) {
        dest = CLIENT_KEY15;
    } else if (strcasecmp(argv[2], "DH15") == 0 && CERT_MISC8_USED == 1) {
        dest = DH_PARAM15;
    } else if (strcasecmp(argv[2], "EXC15") == 0 && CERT_MISC8_USED == 1) {
        dest = EXCH_PARAM15;
    }
    // all
    else if (strcasecmp(argv[2], "ALL") == 0) {
        dest = CERT_ALL;
    } else if (action != ACT_STATUS && dest != CERT_ALL) {
        printf("\n Wrong [dest] or [dest] is not used !! \n\n");
        goto help;
    }

    // status
    if (dest == CERT_ALL && action == ACT_STATUS) {

        if (CERT_MQTTS_CLI_USED == 1) {
            printf("\n#0:\n  For MQTT Client \n");
            printf("  - Root CA     : %s\n", cert_rwds(ACT_STATUS, CA_CERT0) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT0) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY0) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter: %s\n", cert_rwds(ACT_STATUS, DH_PARAM0) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_HTTPS_CLI_USED == 1) {
            printf("\n#1:\n  For HTTPs Client\n");
            printf("  - Root CA     : %s\n", cert_rwds(ACT_STATUS, CA_CERT1) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT1) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY1) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter: %s\n", cert_rwds(ACT_STATUS, DH_PARAM1) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_WPA_ENT_USED == 1) {
            printf("\n#2:\n  For Enterprise (802.1x)\n");
            printf("  - Root CA     : %s\n", cert_rwds(ACT_STATUS, CA_CERT2) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT2) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY2) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter: %s\n", cert_rwds(ACT_STATUS, DH_PARAM2) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_OTA_USED == 1) {
            printf("\n#3:\n  For OTA \n");
            printf("  - Root CA     : %s\n", cert_rwds(ACT_STATUS, CA_CERT3) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT3) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY3) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter: %s\n", cert_rwds(ACT_STATUS, DH_PARAM3) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_HTTPS_SVR_USED == 1) {
            printf("\n#4:\n  For HTTPs Server \n");
            printf("  - Root CA     : %s\n", cert_rwds(ACT_STATUS, CA_CERT4) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT4) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY4) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter: %s\n", cert_rwds(ACT_STATUS, DH_PARAM4) == pdTRUE ? "Found" : "Empty");
        }
   
#if defined (__SUPPORT_ATCMD_TLS__)
        printf("\n#5:TLS_CERT for ATCMD\n");

        {
            atcmd_cert = pvPortMalloc(sizeof(atcmd_cm_cert_t));
            if (atcmd_cert == NULL) {
                printf("Failed to allocate memory to get certificates(%d)\n",
                       sizeof(atcmd_cm_cert_t));
            } else {
                for (atcmd_cert_idx = 0 ; atcmd_cert_idx < ATCMD_CM_MAX_CERT_NUM ; atcmd_cert_idx++) {
                    memset(atcmd_cert, 0x00, sizeof(atcmd_cm_cert_t));

                    status = atcmd_cm_read_cert_by_idx((unsigned int)atcmd_cert_idx, atcmd_cert);
                    if (status) {
                        printf("Failed to read certificate(%d,0x%x)\n", atcmd_cert_idx, status);
                        continue;
                    }

                    if ((atcmd_cert->info.flag == ATCMD_CM_INIT_FLAG)
                            && (strlen(atcmd_cert->info.name) > 0)
                            && (atcmd_cert->info.cert_len > 0)) {
                        printf("  - TLS_CERT_%02d : %s(%s/%d/%d/%d/%d)\n",
                               atcmd_cert_idx + 1, "Found",
                               atcmd_cert->info.name, atcmd_cert->info.type,
                               atcmd_cert->info.seq, atcmd_cert->info.format,
                               atcmd_cert->info.cert_len);
                    } else {
                        printf("  - TLS_CERT_%02d : %s\n", atcmd_cert_idx + 1, "Empty");
                    }
                }

                vPortFree(atcmd_cert);
                atcmd_cert = NULL;
            }
        }
        printf("\n");
#endif // __SUPPORT_ATCMD_TLS__

        if (CERT_AWS_USED == 1) {
            printf("\n#6:\n  For AWS \n");
            printf("  - Root CA             : %s\n", cert_rwds(ACT_STATUS, CA_CERT6) == pdTRUE ? "Found" : "Empty");
            printf("  - Initial Certificate : %s\n", cert_rwds(ACT_STATUS, INITIAL_CERT6) == pdTRUE ? "Found" : "Empty");
            printf("  - Initial Private Key : %s\n", cert_rwds(ACT_STATUS, INITIAL_KEY6) == pdTRUE ? "Found" : "Empty");
            printf("  - Unique Certificate  : %s\n", cert_rwds(ACT_STATUS, UNIQUE_CERT6) == pdTRUE ? "Found" : "Empty");
            printf("  - Unique Private Key  : %s\n", cert_rwds(ACT_STATUS, UNIQUE_KEY6) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MATTER_USED == 1) {
            printf("\n#7:\n  For Matter \n");
            printf("  - Certificate Declaration : %s\n", cert_rwds(ACT_STATUS, CD7) == pdTRUE ? "Found" : "Empty");
            printf("  - DAC Certificate         : %s\n", cert_rwds(ACT_STATUS, DAC_CERT7) == pdTRUE ? "Found" : "Empty");
            printf("  - PAI Certificate         : %s\n", cert_rwds(ACT_STATUS, PAI_CERT7) == pdTRUE ? "Found" : "Empty");
            printf("  - DAC Key                 : %s\n", cert_rwds(ACT_STATUS, DAC_KEY7) == pdTRUE ? "Found" : "Empty");
            printf("  - DAC Public Key          : %s\n", cert_rwds(ACT_STATUS, DAC_PUB7) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC1_USED == 1) {
            printf("\n#8:\n  For Miscellaneous Application 1\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT8) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT8) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY8) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM8) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM8) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC2_USED == 1) {
            printf("\n#9:\n  For Miscellaneous Application 2\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT9) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT9) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY9) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM9) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM9) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC3_USED == 1) {
            printf("\n#9:\n  For Miscellaneous Application 3\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT10) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT10) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY10) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM10) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM10) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC4_USED == 1) {
            printf("\n#10:\n  For Miscellaneous Application 4\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT11) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT11) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY11) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM11) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM11) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC5_USED == 1) {
            printf("\n#11:\n  For Miscellaneous Application 5\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT12) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT12) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY12) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM12) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM12) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC6_USED == 1) {
            printf("\n#12:\n  For Miscellaneous Application 6\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT13) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT13) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY13) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM13) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM13) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC7_USED == 1) {
            printf("\n#13:\n  For Miscellaneous Application 7\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT14) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT14) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY14) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM14) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM14) == pdTRUE ? "Found" : "Empty");
        }

        if (CERT_MISC8_USED == 1) {
            printf("\n#14:\n  For Miscellaneous Application 8\n");
            printf("  - Root CA       : %s\n", cert_rwds(ACT_STATUS, CA_CERT15) == pdTRUE ? "Found" : "Empty");
            printf("  - Certificate   : %s\n", cert_rwds(ACT_STATUS, CLIENT_CERT15) == pdTRUE ? "Found" : "Empty");
            printf("  - Private Key   : %s\n", cert_rwds(ACT_STATUS, CLIENT_KEY15) == pdTRUE ? "Found" : "Empty");
            printf("  - DH Parameter  : %s\n", cert_rwds(ACT_STATUS, DH_PARAM15) == pdTRUE ? "Found" : "Empty");
            printf("  - Exch Parameter: %s\n", cert_rwds(ACT_STATUS, EXCH_PARAM15) == pdTRUE ? "Found" : "Empty");
        }
#if 0 // Reserved - For User requirement
        printf("\n#4:\n  For Reserved\n");
        printf("  - Root CA     : %sound\n", status = cert_rwds(ACT_STATUS, CA_CERT4) == pdTRUE ? "F" : "Not f");
        printf("  - Certificate : %sound\n", status = cert_rwds(ACT_STATUS, CLIENT_CERT4) == pdTRUE ? "F" : "Not f");
        printf("  - Private Key : %sound\n", status = cert_rwds(ACT_STATUS, CLIENT_KEY4) == pdTRUE ? "F" : "Not f");
        printf("  - DH Parameter: %sound\n", status = cert_rwds(ACT_STATUS, DH_PARAM4) == pdTRUE ? "F" : "Not f");
#endif // Reserved

        return pdTRUE;
    } else if (dest == CERT_ALL && action != ACT_STATUS && action != ACT_DELETE) {
        goto help;
    }

    if (action == ACT_READ) {
        printf("  - %-10s : ", argv[2]);
    }
    
    status = cert_rwds(action, (char)dest);

    switch (action) {
        case ACT_STATUS:
            printf("  - %-10s : %s\n\n", argv[2], (status == pdPASS) ? "Found" : "Empty");
            break;

        case ACT_WRITE:
            printf("\n%s Write %s.\n", argv[2], (status == pdPASS) ? "success":"failed");
            break;

        case ACT_DELETE:
            printf("\n%s Delete %s.\n", argv[2], (status == pdPASS) ? "success":"failed");

            break;
    }

    return pdTRUE;
}

#if defined ( __SUPPORT_MQTT__ )
#if defined ( MQTT_TEST_SERVER )
extern bool MQTT_server_start(uint16_t portno, bool tls_insecure, uint16_t max_subscriptions, uint16_t max_retained_topics);
bool cmd_mqtt_server(int argc, char *argv[])
{
    if (strcmp(argv[0], "mqtt_server") == 0)
    {
        if (strcmp(argv[1], "start") == 0)
        {
            if (atoi(argv[2]) == 1)
            {
                MQTT_server_start(8883, true, 4, 0);
            }
            else
            {
                MQTT_server_start(1883, false, 4, 0);
            }
        }
    }

    return pdTRUE;
}
#endif /* MQTT_TEST_SERVER */
#endif /* __SUPPORT_MQTT__ */

extern bool cmd_ping_client(int argc, char *argv[]);

extern int get_sta_signal_poll(void);

#ifdef SIGMA_TEST_ENABLE
bool get_rssi(void)
{
    int rssi = 0;
    rssi = get_sta_signal_poll();

    PRINT_SIGMA_CMD("\r\n\n--------------------------\n"
                 "| CURRENT RSSI: %d      |\n"
                 "--------------------------"
                 "\r\n",
                 rssi);
                 
    return pdTRUE;
}
#endif

static const debug_handler_t net_handlers[] = {
    { "getwlaninit",    "",                                             (debug_callback_t)cmd_get_wlaninit    },
    { "setwlaninit",    "",                                             (debug_callback_t)cmd_set_wlaninit    },
    { "runwlaninit",    "",                                             (debug_callback_t)cmd_run_wlaninit    },
    { CMD_GETWLANMAC,   "Show Wi-Fi MAC address",                       (debug_callback_t)cmd_getWLANMac      },
    { CMD_SETWLANMAC,   "Set Wi-Fi MAC address",                        (debug_callback_t)cmd_setWLANMac      },
    { CMD_SETOTPMAC,    "OTP Write MAC_addr",                           (debug_callback_t)cmd_setWLANMac      },
    { CMD_MACSPOOFING,  "MAC Spoofing for Station",                     (debug_callback_t)cmd_setWLANMac      },

#if LWIP_ARP
#if defined ( __SUPPORT_IPV4__ )
    { "arp",            "arp [wlan0|wlan1|-a]",                         (debug_callback_t)cmd_arp_table       },
    { "arpsend",        "arpsend [wlan0|wlan1] [dst IP]",               (debug_callback_t)cmd_arp_send        },
    { "arpresponse",    "arpresponse [wlan0|wlan1] [dst IP] [dst MAC]", (debug_callback_t)cmd_arp_send        },
    { "garpsend",       "garpsend [wlan0|wlan1] [option]",              (debug_callback_t)cmd_arp_send        },
    { "dhcp_arp",       "dhcp_arp [wlan0|wlan1] [dst IP_addr]",         (debug_callback_t)cmd_arp_send        },
    { "arping",         "arping help",                                  (debug_callback_t)cmd_arping          },
#endif // __SUPPORT_IPV4__
#if defined ( __SUPPORT_IPV6__ )
    { "ndp",            "ndp IPv6 neighbors",                           (debug_callback_t)cmd_ipv6_ndp        },
#endif // __SUPPORT_IPV6__        
#endif //LWIP_ARP

#if defined ( __SUPPORT_IPV4__ )
    { "dhcpd",          " help",                                        (debug_callback_t)cmd_network_dhcpd   },
#endif // __SUPPORT_IPV4__
    { "ifconfig",       "?",                                            (debug_callback_t)cmd_network_config  },
    { "stats",          "network statistics",                           (debug_callback_t)cmd_netstats        },
    
    { "ping",           "-help",                                        (debug_callback_t)cmd_ping_client     },
#ifdef    __SUPPORT_SNTP_CLIENT__
    { "sntp",           "sntp help",                                    (debug_callback_t)cmd_network_sntp    },
#endif    /* __SUPPORT_SNTP_CLIENT__ */
#if defined (__SUPPORT_IPERF__)
    { "iperf",          "-help",                                        (debug_callback_t)cmd_iperf_cli       },
#endif  //(__SUPPORT_IPERF__)
#if defined (__SUPPORT_IPERF3__)
    { "iperf3",          "-help",                                       (debug_callback_t)cmd_iperf3_cli      },
#endif  //(__SUPPORT_IPERF3__)

#if defined ( __USER_DHCP_HOSTNAME__ )
    { "dhcp_hostname",    "dhcp_hostname [get|set|del] [hostname]",     (debug_callback_t)cmd_dhcp_hostname   },
#endif    // __USER_DHCP_HOSTNAME__    
#ifdef    __SUPPORT_NSLOOKUP__
#ifdef LWIP_DNS
    { "nslookup",       "nslookup [domain]",                            (debug_callback_t)cmd_nslookup        },
#endif    /*LWIP_DNS*/
#endif    /* __SUPPORT_NSLOOKUP__ */
#if defined ( __SUPPORT_MQTT__ )
    { "mqtt_config",    "MQTT Configuration command",                   (debug_callback_t)cmd_mqtt_client     },
    { "mqtt_client",    "MQTT Client operation command",                (debug_callback_t)cmd_mqtt_client     },
#if defined ( MQTT_TEST_SERVER )
    { "mqtt_server",    "MQTT Server operation command",                (debug_callback_t)cmd_mqtt_server     },
#endif /* MQTT_TEST_SERVER */
#if defined (__MQTT_EMUL_CMD__)
    { "mq_emul",        "MQTT emulation command",                       (debug_callback_t)cmd_mq_msg_tbl_test },
#endif /* __MQTT_EMUL_CMD__ */
#if defined (__MQTT_DBG_TBL_CMD__)
    { "mq_tbl",         "MQTT preserved message table command",         (debug_callback_t)cmd_mq_msg_tbl_test },
#endif /* __MQTT_DBG_TBL_CMD__ */
#endif    /* __SUPPORT_MQTT__ */

#if (SUPPORT_FSP_RM_OTA_W == 1)
#ifdef  __SUPPORT_OTA__
    { "ota_update",     "ota_update help",                              (debug_callback_t)cmd_ota_update      },
#endif /*  __SUPPORT_OTA__ */
#endif

    { "getsysmode",     "Get current Wi-Fi mode",                       (debug_callback_t)cmd_get_sys_mode    },
    { "setsysmode",     "Set Wi-Fi running mode",                       (debug_callback_t)cmd_set_sys_mode    },
    { "supp_stop",      "Stop wpa_supplicant",                          (debug_callback_t)cmd_supp_stop       },
    { "supp_start",     "Start wpa_supplicant",                         (debug_callback_t)cmd_supp_start      },
    { "wpa_cli",        "wpa_supplicant cli command",                   (debug_callback_t)cmd_wpa_cli         },

    { "cert",           "manage Certificate for TLS",                   (debug_callback_t)cmd_certificate     },

    { "debug",          "debug help",                                   (debug_callback_t)cmd_debug           },
#if defined(SIGMA_TEST_ENABLE)
    { "tg_setprofile",  "traffic generator set profile cli: see help",  (debug_callback_t)cmd_tg_setprofile   },
    { "tg_test",        "traffic generator start/stop command",         (debug_callback_t)cmd_tg_test         },
    { "tg_ping",        "traffic generator for ping",                   (debug_callback_t)cmd_tg_ping         },
    { "get_rssi",        "Read current rssi",                           (debug_callback_t)get_rssi         },
#endif
#if (TCP_CLIENT_APP_START == 1)
    { "tcpc_conf",      "TCP client's configuration",                   (debug_callback_t)cmd_tcpc_conf       },
#endif
    { NULL },
};
#endif /* CFG_WIFI */
bool net_command(int argc, const char *argv[], void *user_data)
{
    RA6W1_UNUSED_ARG(user_data);
#if CFG_WIFI
    return debug_handle_message(argc, argv, net_handlers);
#endif

    printf("\nnot supported\n");

    return false;
}
