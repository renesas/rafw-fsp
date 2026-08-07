/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include "bsp_api.h"

#if CFG_PMGR

#if CFG_WIFI
/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "rm_pmgr_w_dpm_socket_internal.h"
#include "rm_pmgr_w_instance.h"
#include "sleep_mgmt_regs.h"
#include "iface_defs.h"

#include "lwip/priv/sockets_priv.h" //for struct lwip_sock
#include "lwip/api.h"               //for struct netconn
#include "lwip/priv/tcp_priv.h"
#include "lwip/udp.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#undef	ENABLE_RA6WX_DPM_SOCK_DEBUG_INFO
#define	ENABLE_RA6WX_DPM_SOCK_DEBUG_ERR

#define PRINTF(...)	printf(__VA_ARGS__)

#define	RA6WX_DPM_SOCK_PRINTF			PRINTF

#if defined (ENABLE_RA6WX_DPM_SOCK_DEBUG_INFO)
#define	RA6WX_DPM_SOCK_DEBUG_INFO(fmt, ...)	\
	RA6WX_DPM_SOCK_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	RA6WX_DPM_SOCK_DEBUG_INFO(...)		do {} while (0)
#endif	// (ENABLE_RA6WX_DPM_SOCK_DEBUG_INFO)

#if defined (ENABLE_RA6WX_DPM_SOCK_DEBUG_ERR)
#define	RA6WX_DPM_SOCK_DEBUG_ERR(fmt, ...)	\
	RA6WX_DPM_SOCK_PRINTF("[%s:%d]" fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	RA6WX_DPM_SOCK_DEBUG_ERR(...)		do {} while (0)
#endif	// (ENABLE_RA6WX_DPM_SOCK_DEBUG_ERR) 

#define DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT NUM_SOCKETS

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _dpm_tcp_sess_reason_code {
	DPM_TCP_SESS_NONE = 0,
	DPM_TCP_SESS_BSD_REMAINING_LASTDATA = 1,
	DPM_TCP_SESS_BSD_CLOSING = 2,
	DPM_TCP_SESS_BSD_USED = 3,
	DPM_TCP_SESS_BSD_STATE = 4,
	DPM_TCP_SESS_PCB_UNSENT = 5,
	DPM_TCP_SESS_PCB_UNACK = 6,
} dpm_tcp_sess_reason_code;

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
static struct lwip_sock_name *dpm_lwip_sock_names = NULL;
static dpm_tcp_sess_info *dpm_tcp_sess_list = NULL;

/* UDPH */
static SemaphoreHandle_t tcp_sock_alloc_mutex = NULL;
static ptim_udph_conf_data_t g_ptim_udph_conf;
static SemaphoreHandle_t mutex_udph_tbl = NULL;

/***********************************************************************************************************************
 * Global Variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 *********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
 
void RM_PMGR_W_socket_dpm_init(void)
{
	if (dpm_tcp_sess_list != NULL) {
		return ;
	}

	RA6WX_DPM_SOCK_DEBUG_INFO("Start\n");

	dpm_lwip_sock_names = (struct lwip_sock_name *)TCP_SESS_INFO;

	dpm_tcp_sess_list = (dpm_tcp_sess_info *)(TCP_SESS_INFO + (sizeof(struct lwip_sock_name) * DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT));

	RA6WX_DPM_SOCK_DEBUG_INFO("dpm_lwip_sock_names(%p, %d), "
							  "dpm_tcp_sess_list(%p, %d)\n",
							  dpm_lwip_sock_names,
							  sizeof(struct lwip_sock_name) * DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT,
							  dpm_tcp_sess_list,
							  sizeof(dpm_tcp_sess_info) * DPM_SOCK_MAX_TCP_SESS);

	return ;
}

void RM_PMGR_W_socket_dpm_all_tcp_sess_info_clear(void)
{
	if (!RM_PMGR_W_rtm_exist()) {
		/* Unsupport RTM */
		return ;
	}

	RA6WX_DPM_SOCK_DEBUG_INFO("Start\n");

	RM_PMGR_W_socket_dpm_init();

	//TODO: require to calculate TCP_ALLOC_SZ
	if (dpm_lwip_sock_names != NULL) {
		memset((void *)TCP_SESS_INFO, 0x00, TCP_ALLOC_SZ);
	}

	RA6WX_DPM_SOCK_DEBUG_INFO("clear TCP_SESS_INFO(0x%x, %d, actual size:%d)\n",
							  TCP_SESS_INFO, TCP_ALLOC_SZ,
							  (sizeof(struct lwip_sock_name) * DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT)
								+ (sizeof(dpm_tcp_sess_info) * DPM_SOCK_MAX_TCP_SESS));

	return ;
}

void *RM_PMGR_W_socket_dpm_lwip_sock_names_get(void)
{
	if (!dpm_lwip_sock_names) {
		RM_PMGR_W_socket_dpm_init();
	}

	return (void *)dpm_lwip_sock_names;
}

static void ra6w1_dpm_sock_create_mutex()
{
	if (tcp_sock_alloc_mutex == NULL) {
		tcp_sock_alloc_mutex = xSemaphoreCreateRecursiveMutex();
		if (tcp_sock_alloc_mutex == NULL) {
			RA6WX_DPM_SOCK_DEBUG_INFO("failed to create mutex for tcp session\n");
		}
	}

	return ;
}

static unsigned int ra6w1_dpm_sock_take_mutex(TickType_t wait_option)
{
	if (tcp_sock_alloc_mutex) {
		return xSemaphoreTakeRecursive(tcp_sock_alloc_mutex, wait_option);
	}

	return -1;
}

static unsigned int ra6w1_dpm_sock_give_mutex(void)
{
	if (tcp_sock_alloc_mutex) {
		xSemaphoreGiveRecursive(tcp_sock_alloc_mutex);
	}

	return -1;
}

void RM_PMGR_W_socket_dpm_tcp_sess_rtm_content_print(void)
{
	printf("   dpm_lwip_sock_names(%p, %d), dpm_tcp_sess_list(%p, %d:%d*%d)\n",
							dpm_lwip_sock_names,
							sizeof(struct lwip_sock_name) * DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT,
							dpm_tcp_sess_list,
							sizeof(dpm_tcp_sess_info) * DPM_SOCK_MAX_TCP_SESS,
							sizeof(dpm_tcp_sess_info), DPM_SOCK_MAX_TCP_SESS);

	printf("   total_size: %d(%d+%d)\n", 
							(sizeof(struct lwip_sock_name)*DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT)+
							(sizeof(dpm_tcp_sess_info)*DPM_SOCK_MAX_TCP_SESS),
							(sizeof(struct lwip_sock_name)*DPM_SOCK_MAX_LWIP_SOCK_NAME_CNT),
							(sizeof(dpm_tcp_sess_info)*DPM_SOCK_MAX_TCP_SESS));
}

void RM_PMGR_W_socket_dpm_tcp_sess_pcb_print(int index)
{
	int idx = index;
	dpm_tcp_sess_info *sess_info = NULL;
	struct tcp_pcb *ret = NULL;

	if (dpm_tcp_sess_list == NULL) {
		return;
	}

	sess_info = &(dpm_tcp_sess_list[idx]);

	if ((sess_info->used == 1)) {
		ret = &sess_info->pcb;
		PRINTF(CYAN_COLOR "    >>> Idx:%d Address:%p,%p flags: %p - 0x%x\n" CLEAR_COLOR,
							idx, sess_info, &sess_info->pcb, &(ret->flags), ret->flags);
	}
}

struct tcp_pcb *RM_PMGR_W_socket_dpm_tcp_pcb_create(char *name)
{
#if !defined (ENABLE_RA6WX_DPM_SOCK_DEBUG_INFO)
	FSP_PARAMETER_NOT_USED(name);
#endif
	int idx = 0;
	dpm_tcp_sess_info *sess_info = NULL;
	struct tcp_pcb *ret = NULL;

	if (!dpm_tcp_sess_list) {
		RM_PMGR_W_socket_dpm_init();
	}

	ra6w1_dpm_sock_create_mutex();

	ra6w1_dpm_sock_take_mutex(portMAX_DELAY);

	for (idx = 0 ; idx < DPM_SOCK_MAX_TCP_SESS ; idx++) {
		sess_info = &(dpm_tcp_sess_list[idx]);

		if (sess_info->used == 0) {
			memset(sess_info, 0x00, sizeof(dpm_tcp_sess_info));
			sess_info->used++;
			sess_info->sess_idx = idx;
			ret = &sess_info->pcb;

			RA6WX_DPM_SOCK_DEBUG_INFO("Allcated TCP session\n"
									  "* Idx: %d\n"
									  "* Name: %s\n"
									  "* Address: %p,%p\n"
									  "* flags: %p - 0x%x\n"
									  "* Size: %d\n",
									  idx, name, sess_info, &sess_info->pcb, &(ret->flags), ret->flags,
									  sizeof(dpm_tcp_sess_info));
			break;
		}
	}

	ra6w1_dpm_sock_give_mutex();

	return ret;
}

dpm_tcp_sess_info *RM_PMGR_W_socket_dpm_get_sess_info_by_name(char *name)
{
	int idx = 0;
	dpm_tcp_sess_info *sess_info = NULL;
	size_t name_len = 0;

	if (!RM_PMGR_W_dpm_is_enabled()) {
		RA6WX_DPM_SOCK_DEBUG_INFO("WARN! dpm disabled, accessing dpm sock\n\r");
		/* return NULL; */ /* TODO: FIXME: we are creating dpm socket without
		DPM enabled check - but delete has check enabled - this causes,
		resource leakage and socket create failure afer 4 successive creations
		*/
	}

	if (!dpm_tcp_sess_list) {
		RM_PMGR_W_socket_dpm_init();
	}

	name_len = dpm_strlen(name);

	for (idx = 0 ; idx < DPM_SOCK_MAX_TCP_SESS ; idx++) {
		sess_info = &(dpm_tcp_sess_list[idx]);

		if ((sess_info->used > 0)
			&& (dpm_strlen(sess_info->pcb.name) == name_len)
			&& (dpm_strncmp(sess_info->pcb.name, name, name_len) == 0)) {
			RA6WX_DPM_SOCK_DEBUG_INFO("Found session(%p(%d),%p(%s))\n",
									  sess_info, idx,
									  &(sess_info->pcb), sess_info->pcb.name);
			return sess_info;
		}
	}

	return NULL;
}

void RM_PMGR_W_socket_dpm_set_tcp_ka_time_by_name(char *name) {
	dpm_tcp_sess_info * sess_info = NULL;
	sess_info = RM_PMGR_W_socket_dpm_get_sess_info_by_name(name);

	if((sess_info != NULL) && (sess_info->sess_idx < DPM_TCP_KA_MAX)) {
		RTM_DPM_TCP_KA_TIME_PTR->last_ka_time[sess_info->sess_idx] = dpm_get_rtclk();
	}
}

struct tcp_pcb *RM_PMGR_W_socket_dpm_tcp_pcb_get(char *name)
{
	dpm_tcp_sess_info *sess_info = NULL;

	sess_info = RM_PMGR_W_socket_dpm_get_sess_info_by_name(name);
	if (sess_info != NULL) {
		RA6WX_DPM_SOCK_DEBUG_INFO("Found TCP session(%s)\n", name);
		return &(sess_info->pcb);
	}

	RA6WX_DPM_SOCK_DEBUG_INFO("Not found TCP session(%s)\n", name);

	return NULL;
}

int RM_PMGR_W_socket_dpm_tcp_pcb_delete(char *name)
{
	dpm_tcp_sess_info *sess_info = NULL;

	sess_info = RM_PMGR_W_socket_dpm_get_sess_info_by_name(name);
	if (sess_info != NULL) {
		RA6WX_DPM_SOCK_DEBUG_INFO("Clear TCP session(%s)\n", name);
		memset(sess_info, 0x00, sizeof(dpm_tcp_sess_info));
		return 0;
	}

	RA6WX_DPM_SOCK_DEBUG_ERR("Not found TCP session(%s)\n", name);

	return -1;
}

static int ra6w1_dpm_sock_is_available_sleep_tcp_pcb(struct tcp_pcb *pcb, dpm_tcp_sess_reason_code *reason)
{
	RA6WX_DPM_SOCK_DEBUG_INFO("name(%s), pcb(%p)\n", pcb->name, pcb);

	if (pcb->unsent || pcb->unacked || pcb->snd_queuelen) {
		if (pcb->unsent) {
			RA6WX_DPM_SOCK_DEBUG_INFO("There is unsent messages \n");
		}

		if (pcb->unacked) {
			RA6WX_DPM_SOCK_DEBUG_INFO("There is unacked messages \n");
		}

		if (pcb->snd_queuelen) {
			RA6WX_DPM_SOCK_DEBUG_INFO("There is snd_queuelen (qlen:%d) \n",
									  pcb->snd_queuelen);
		}

		if (reason) {
			*reason = DPM_TCP_SESS_PCB_UNSENT;
		}

		return pdFALSE;
	}


	/* tcp_rx case: if tcp_ack not been sent out yet, wait until tcp_ack is sent out  */
	if(((pcb)->state != LISTEN) && ((pcb)->flags & TF_ACK_DELAY)) {
		RA6WX_DPM_SOCK_DEBUG_INFO("Waiting for ACK (%p - flags:0x%x) \n",
								  &((pcb)->flags), (pcb)->flags);
		if (reason) {
			*reason = DPM_TCP_SESS_PCB_UNACK;
		}
		return pdFALSE;
	}

	return pdTRUE;
}

static int ra6w1_dpm_sock_is_available_sleep_bsd(struct lwip_sock *sock, dpm_tcp_sess_reason_code *reason)
{
	const int max_fd_used = 2;

	if (sock->lastdata.pbuf) {
		RA6WX_DPM_SOCK_DEBUG_INFO("there is received data\n");
		if (reason) {
			*reason = DPM_TCP_SESS_BSD_REMAINING_LASTDATA;
		}
		return pdFALSE;
	}

#if LWIP_NETCONN_FULLDUPLEX
	if (sock->fd_free_pending) {
		RA6WX_DPM_SOCK_DEBUG_INFO("socket is closing(%d)\n", sock->fd_free_pending);
		if (reason) {
			*reason = DPM_TCP_SESS_BSD_CLOSING;
		}
		return pdFALSE;
	}

	if (sock->fd_used > max_fd_used) {
		RA6WX_DPM_SOCK_DEBUG_INFO("lwip_sock->fd_used(%d,%d)\n",
								  sock->fd_used, max_fd_used);
		if (reason) {
			*reason = DPM_TCP_SESS_BSD_USED;
		}
		return pdFALSE;
	}
#endif /* LWIP_NETCONN_FULLDUPLEX */

	if (NETCONNTYPE_GROUP(sock->conn->type) == NETCONN_TCP) {
		if ((sock->conn->state != NETCONN_NONE)
			&& (sock->conn->state != NETCONN_LISTEN)) {
			RA6WX_DPM_SOCK_DEBUG_INFO("netconn is busy(%d)\n", sock->conn->state);
			if (reason) {
				*reason = DPM_TCP_SESS_BSD_STATE;
			}
			return pdFALSE;
		}
	} else {
		RA6WX_DPM_SOCK_DEBUG_INFO("socket type(%x) is not TCP\n",
								  NETCONNTYPE_GROUP(sock->conn->type));
		return pdTRUE;
	}

    return pdTRUE;
}

int RM_PMGR_W_socket_dpm_ongoing_transaction_is_finished(void)
{
	extern struct lwip_sock *get_socket_dpm(int fd);
	extern void done_socket_dpm(struct lwip_sock *sock);

	unsigned int ret = pdTRUE;
	unsigned int idx = 0;

	dpm_tcp_sess_reason_code reason = DPM_TCP_SESS_NONE;

	//for bsd
	struct lwip_sock *sock = NULL;

	//for tcp_pcb
	dpm_tcp_sess_info *sess_info = NULL;

	//for bsd socket
	for (idx = 0 ; idx < NUM_SOCKETS ; idx++) {
		reason = DPM_TCP_SESS_NONE;

		sock = get_socket_dpm(idx);
		if ((sock == NULL)
			|| (sock->conn == NULL)
			|| (strlen(sock->conn->pcb.tcp->name) == 0)) {
			continue;
		}

		RA6WX_DPM_SOCK_DEBUG_INFO("Check BSD Socket(%d)\n", idx);

		ret = ra6w1_dpm_sock_is_available_sleep_bsd(sock, &reason);
		if (!ret) {
			RA6WX_DPM_SOCK_DEBUG_INFO("BSD Socket(%d) is progressing(%d)\n", idx, reason);
			done_socket_dpm(sock);

			sess_info = RM_PMGR_W_socket_dpm_get_sess_info_by_name(sock->conn->pcb.tcp->name);
			if (sess_info) {
				RA6WX_DPM_SOCK_DEBUG_INFO("wait_cnt(%d)\n", sess_info->wait_cnt);
				sess_info->wait_cnt++;

				if (sess_info->wait_cnt > DPM_SOCK_MAX_TCP_WAIT_CNT) {
					sess_info->wait_cnt = 0;
					ret = pdTRUE;

					RA6WX_DPM_SOCK_DEBUG_ERR("BSD Socket(%d) is progressing(%d)"
											 "- wait_cnt(%d/%d)\n",
											 idx, reason,
											 DPM_SOCK_MAX_TCP_WAIT_CNT,
											 sess_info->wait_cnt);
					break;
				}
			}

			return pdFALSE;
		}

		done_socket_dpm(sock);
	}

	//for tcp_pcb
	if (!dpm_tcp_sess_list) {
		RM_PMGR_W_socket_dpm_init();
	}

	for (idx = 0 ; idx < DPM_SOCK_MAX_TCP_SESS ; idx++) {
		reason = DPM_TCP_SESS_NONE;

		sess_info = &(dpm_tcp_sess_list[idx]);

		if (sess_info->used > 0) {
			RA6WX_DPM_SOCK_DEBUG_INFO("Check TCP_PCB(%p(%d),%p(%s)) - wait_cnt(%d)\n",
									  sess_info, idx, &(sess_info->pcb),
									  sess_info->pcb.name, sess_info->wait_cnt);

			ret = ra6w1_dpm_sock_is_available_sleep_tcp_pcb(&(sess_info->pcb), &reason);
			if (!ret) {
				RA6WX_DPM_SOCK_DEBUG_INFO("TCP_PCB(%p(%d),%p(%s)) is progressing(%d)."
										  "- wait_cnt(%d)\n",
										  sess_info, idx, &(sess_info->pcb),
										  sess_info->pcb.name, reason,
										  sess_info->wait_cnt);

				sess_info->wait_cnt++;
				if (sess_info->wait_cnt > DPM_SOCK_MAX_TCP_WAIT_CNT) {
					sess_info->wait_cnt = 0;
					ret = pdTRUE;

					RA6WX_DPM_SOCK_DEBUG_ERR("TCP_PCB(%p(%d),%p(%s)) is progressing(%d)."
											 "- wait_cnt(%d/%d)\n",
											 sess_info, idx, &(sess_info->pcb),
											 sess_info->pcb.name, reason,
											 DPM_SOCK_MAX_TCP_WAIT_CNT,
											 sess_info->wait_cnt);
					break;
				}

				return pdFALSE;
			}
		}
	}

	//clear wait_cnt
	if (ret) {
		RA6WX_DPM_SOCK_DEBUG_INFO("Clear wait_cnt\n");
		for (idx = 0 ; idx < DPM_SOCK_MAX_TCP_SESS ; idx++) {
			sess_info = &(dpm_tcp_sess_list[idx]);
			sess_info->wait_cnt = 0;
		}
	}

	return ret;
}

int RM_PMGR_W_socket_dpm_tcp_sess_is_any_connected_one_existing(void)
{
	unsigned int idx = 0;
	dpm_tcp_sess_info *sess_info = NULL;

    //for tcp_pcb
    if (!dpm_tcp_sess_list) {
        RM_PMGR_W_socket_dpm_init();
    }

    for (idx = 0 ; idx < DPM_SOCK_MAX_TCP_SESS ; idx++) {
        sess_info = &(dpm_tcp_sess_list[idx]);
        if (sess_info->used > 0 && sess_info->pcb.state == ESTABLISHED) {
            return pdTRUE;
        }
    }

    return pdFALSE;
}

static void RM_PMGR_W_dpm_tcp_session_info_set(int sessno, struct tcp_pcb *pcb)
{
	uint32_t remote_ip;
	uint8_t dst_mac[6] = {0,};

	if (pcb->state != ESTABLISHED) {
		return;
	}
	
	/* TCP destination IP */
	remote_ip = ip_addr_get_ip4_u32(((ip_addr_t*)(&(pcb->remote_ip))));
	romac4rtos_set_tcp_ipv4_n(sessno, remote_ip);

	/* TCP destination MAC */
	etharp_get_mac_from_ip(WLAN0_IFACE, (ip4_addr_t *)(&remote_ip), &dst_mac[0]);
	romac4rtos_set_tcp_target_mac_n(sessno, dst_mac);

	/* TCP Session info */
	romac4rtos_set_tcp_n(sessno,
			     pcb->local_port,
			     pcb->remote_port,
			     pcb->snd_nxt,
			     pcb->rcv_nxt,
			     TCPWND_MIN16(RCV_WND_SCALE(pcb, pcb->rcv_ann_wnd)));

	/* TCP Session info */
	romac4rtos_set_tcpseq_n(sessno,
				pcb->snd_nxt,
				pcb->rcv_nxt,
				TCPWND_MIN16(RCV_WND_SCALE(pcb, pcb->rcv_ann_wnd)));

	/* TCP Session TCP KA info */
	romac4rtos_set_tcpka_n(sessno,
			       !!(pcb->so_options & SO_KEEPALIVE),
			       pcb->keep_idle,
#if LWIP_TCP_KEEPALIVE
			       pcb->keep_intvl,
			       pcb->keep_cnt
#else
			       TCP_KEEPINTVL_DEFAULT,
			       TCP_KEEPCNT_DEFAULT
#endif
			       );

#ifdef FOR_DEBUG
	printf(YELLOW_COLOR " [%s] Set TCP session %d: dstIP=%d, sport=%lu, dport=%lu, seq=%lu, ack=%lu, win_sz=%lu\n" CLEAR_COLOR,
		__func__,
		sessno,
		remote_ip,
		pcb->local_port,
		pcb->remote_port,
		pcb->snd_nxt,
		pcb->rcv_nxt,
		pcb->rcv_ann_wndm);
#endif	// FOR_DEBUG
}

void RM_PMGR_W_socket_dpm_tcp_sess_ptim_config_for_connected_one(void)
{
	dpm_tcp_sess_info *sess_info = NULL;

	/* Check tcp_pcb init status */
	if (!dpm_tcp_sess_list) {
		RM_PMGR_W_socket_dpm_init();
	}

	romac4rtos_set_tcpacken(true);
	romac4rtos_set_tcpkaen(true);

	for (int i = 0 ; i < DPM_SOCK_MAX_TCP_SESS ; i++) {
		sess_info = &dpm_tcp_sess_list[i];

		if (!sess_info->used) {
			continue;
		}

		RM_PMGR_W_dpm_tcp_session_info_set(i, &sess_info->pcb);
	}
}

static int udph_tbl_mutex_create(void)
{
    if (!mutex_udph_tbl)
    {
        mutex_udph_tbl = xSemaphoreCreateMutex();

        if (mutex_udph_tbl == NULL) {
            printf("[%s] Faild to create a mutex !\n", __func__);
            return pdFAIL;
        }
    }

    return pdPASS;
}

static int udph_tbl_mutex_take(unsigned int timeout)
{
    int ret = 0;

    if (!mutex_udph_tbl) {
        if (!udph_tbl_mutex_create()) {
            return pdFAIL;
        }
    }

    ret = xSemaphoreTake(mutex_udph_tbl, timeout);

    if (ret != pdTRUE) {
        printf("[%s] Failed to take mutex(%d) \n", __func__, ret);
        return pdFAIL;
    }

    return pdPASS;
}

static int udph_tbl_mutex_give(void)
{
    int ret = 0;

    if (!mutex_udph_tbl) {
        if (!udph_tbl_mutex_create()) {
            return pdFAIL;
        }
    }    

    ret = xSemaphoreGive(mutex_udph_tbl);

    if (ret != pdTRUE) {
        printf("[%s] Failed to give mutex(%d)\n", __func__, ret);
        return pdFAIL;
    }

    return pdPASS;
}

/* Print UDP hole-punching table entries */
void ptim_udph_conf_status_print(void)
{
    int i;
    char buf[40] = {0,};

    printf("UDPH table: \n");
    udph_tbl_mutex_take(portMAX_DELAY);
    for (i = 0 ; i < DPM_SOCK_MAX_UDPH ; i++) 
    {
        if (g_ptim_udph_conf.ptim_udph_config[i].in_use == 1)
        {
            if (g_ptim_udph_conf.ptim_udph_config[i].ip_type == IPADDR_TYPE_V4)
            {
                inet_ntop(AF_INET, &g_ptim_udph_conf.ptim_udph_config[i].peer_ip, buf, sizeof(buf));
            }
            else if (g_ptim_udph_conf.ptim_udph_config[i].ip_type == IPADDR_TYPE_V6)
            {
                inet_ntop(AF_INET6, &g_ptim_udph_conf.ptim_udph_config[i].peer_ip6, buf, sizeof(buf));
            }

            printf("idx=%d \n", i);
            printf("peer_ip=%s \n", buf);
            printf("src_port=%u \ndst_port=%u \nperiod=%lu \n\n",
                            g_ptim_udph_conf.ptim_udph_config[i].src_port,
                            g_ptim_udph_conf.ptim_udph_config[i].dst_port,
                            g_ptim_udph_conf.ptim_udph_config[i].period);
        }
    }
    udph_tbl_mutex_give();
}

/* Print active UDP sockets in the system */
void ptim_udph_sock_status_print(void)
{
    struct udp_pcb * pcb;
    int i = 0;
    printf(" UDP sock status : \n");

    for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
        printf("pcb_idx=%d, local_port=%u, remote_port=%u \n", i++, pcb->local_port, pcb->remote_port);
    }
}

/*
    Get an index of a free slot from UDPH config table
    returns a valid index of a free
        -1    : UDPH config table is currently full
        index : a valid index of a free slot
*/
int ptim_udph_conf_free_slot_get(void)
{
    int i;
    
    udph_tbl_mutex_take(portMAX_DELAY);
    for (i = 0 ; i < DPM_SOCK_MAX_UDPH ; i++) {    
        if (g_ptim_udph_conf.ptim_udph_config[i].in_use == 0) {
            udph_tbl_mutex_give();
            return i;
        }
    }
    udph_tbl_mutex_give();
    
    return -1;
}

/*
    Add a UDPH entry to UDPH config table
    returns pdPASS (success) or pdFALSE (pdFAIL)
*/
int ptim_udph_conf_add(uint32_t dst_ip, uint32_t *dst_ip6, uint16_t src_port, uint16_t dst_port, int period)
{
    int i;

    udph_tbl_mutex_take(portMAX_DELAY);    
    for (i = 0 ; i < DPM_SOCK_MAX_UDPH ; i++) {

        if (g_ptim_udph_conf.ptim_udph_config[i].in_use == 0)
        {

            g_ptim_udph_conf.ptim_udph_config[i].dst_port = dst_port;
            g_ptim_udph_conf.ptim_udph_config[i].src_port = src_port;
            g_ptim_udph_conf.ptim_udph_config[i].period = period;

            if (dst_ip != 0)
            {
                g_ptim_udph_conf.ptim_udph_config[i].ip_type = IPADDR_TYPE_V4;
                g_ptim_udph_conf.ptim_udph_config[i].peer_ip = dst_ip;
            }
            else if (dst_ip6 != 0)
            {
                g_ptim_udph_conf.ptim_udph_config[i].ip_type = IPADDR_TYPE_V6;
                g_ptim_udph_conf.ptim_udph_config[i].peer_ip6[0] = dst_ip6[0];
                g_ptim_udph_conf.ptim_udph_config[i].peer_ip6[1] = dst_ip6[1];
                g_ptim_udph_conf.ptim_udph_config[i].peer_ip6[2] = dst_ip6[2];
                g_ptim_udph_conf.ptim_udph_config[i].peer_ip6[3] = dst_ip6[3];
            }

            g_ptim_udph_conf.ptim_udph_config[i].in_use = 1;
            g_ptim_udph_conf.active_udph_count++;

            udph_tbl_mutex_give();
            return pdPASS;
        }
    }
    udph_tbl_mutex_give();

    return pdFAIL;
}

/*
    Check if UDPH config table is empty or not
    returns pdTRUE (empty) or pdFALSE (not empty)
*/
static int ptim_udph_conf_is_empty(void)
{
    int count;
    
    udph_tbl_mutex_take(portMAX_DELAY);
    count = g_ptim_udph_conf.active_udph_count;
    udph_tbl_mutex_give();
    
    return count ? pdFALSE : pdTRUE;
}

/* 
    Get an index of a UDPH entry that matches with local port and destination port given

    Returns 
        idx (0 ~ DPM_SOCK_MAX_UDPH - 1) to an entry to UDPH config table. 
        -1 if not found
*/
static int ptim_udph_conf_find_entry(uint16_t lport, uint16_t dport)
{
    int i, count;
    udph_tbl_mutex_take(portMAX_DELAY);
    count = g_ptim_udph_conf.active_udph_count;
    udph_tbl_mutex_give();

    if (count == 0) {
        return -1;
    }

    udph_tbl_mutex_take(portMAX_DELAY);
    for (i = 0 ; i < DPM_SOCK_MAX_UDPH ; i++) {
        if (g_ptim_udph_conf.ptim_udph_config[i].in_use == 0) {
            continue;
        }

        if (lport == g_ptim_udph_conf.ptim_udph_config[i].src_port &&
            dport == g_ptim_udph_conf.ptim_udph_config[i].dst_port) {
            udph_tbl_mutex_give();
            return i;
        }
    }
    udph_tbl_mutex_give();

    return -1;
}

/*
    Configure pTIM based on UDPH Config table.
    
    return 
        0   : pTIM UDPH not set
      > 0   : pTIM UDPH set (shows how many multi-sessions set pTIM)

*/
int RM_PMGR_W_socket_dpm_ptim_udph_config(void)
{
    int idx = 0;

    uint32_t dst_ip = 0;
    uint32_t dst_ip6[4] = {0,};
    uint8_t dst_mac[6] = {0,};
    uint8_t ip_type = 0;

    int period = 0;
    uint16_t src_port;
    uint16_t dst_port;

    struct udp_pcb * pcb;
    int ptim_config_set_count = 0;

    /* UDPH config */
    if (ptim_udph_conf_is_empty()) 
    {
        return pdFALSE;
    }

    /* pTIM UDPH - erase all UDPH config first */
    for (idx = 0 ; idx < DPM_SOCK_MAX_UDPH ; idx++) 
    {
        romac4rtos_set_udp_ipv4_n(idx, 0);
        romac4rtos_set_udp_ipv6_n(idx, 0);
    }

    for (pcb = udp_pcbs ; pcb != NULL ; pcb = pcb->next) 
    {

        idx = ptim_udph_conf_find_entry(pcb->local_port, pcb->remote_port);
        if (idx == -1) 
        {
            continue;
        }
        else 
        {
            /* Config ptim UDPH */
            ip_type = g_ptim_udph_conf.ptim_udph_config[idx].ip_type;
            period = g_ptim_udph_conf.ptim_udph_config[idx].period;
            src_port = g_ptim_udph_conf.ptim_udph_config[idx].src_port;
            dst_port = g_ptim_udph_conf.ptim_udph_config[idx].dst_port;

            romac4rtos_set_udp_n(idx, src_port, dst_port);

            if (period) 
            {
                romac4rtos_set_udphen(1);
            } else 
            {
                romac4rtos_set_udphen(0);
            }

            romac4rtos_set_udph_period_n(idx, period);

            if (ip_type == IPADDR_TYPE_V4)
            {
#if LWIP_IPV4
                dst_ip = htonl(g_ptim_udph_conf.ptim_udph_config[idx].peer_ip);
                romac4rtos_set_udp_ipv4_n(idx, *((uint32_t *)dst_ip));
                etharp_get_mac_from_ip(WLAN0_IFACE, (ip4_addr_t *)(&dst_ip), &dst_mac[0]);
#endif /* LWIP_IPV4 */
            } 
            else if (ip_type == IPADDR_TYPE_V6)
            {
#if LWIP_IPV6
                dst_ip6[0] = g_ptim_udph_conf.ptim_udph_config[idx].peer_ip6[0];
                dst_ip6[1] = g_ptim_udph_conf.ptim_udph_config[idx].peer_ip6[1];
                dst_ip6[2] = g_ptim_udph_conf.ptim_udph_config[idx].peer_ip6[2];
                dst_ip6[3] = g_ptim_udph_conf.ptim_udph_config[idx].peer_ip6[3];
                romac4rtos_set_udp_ipv6_n(idx, (uint8_t *)dst_ip6);
#if CFG_PMGR
                /* Get IPv6 Neighbor Advertisement Information */
                dpm_supp_ipv6_info_t     *dpm_ipv6_info;
                RM_PMGR_W_rtm_static_get(RTM_STATIC_KEY_IPV6_INFO_PTR, NULL, NULL, (void**)(&dpm_ipv6_info));
                memcpy(&dst_mac[0], dpm_ipv6_info->dpm_neighbor_macaddr[0], DPM_ETH_ALEN);
#endif /* CFG_PMGR */
#endif /* LWIP_IPV6 */
            }

            romac4rtos_set_udp_target_mac_n(idx, dst_mac);
            ptim_config_set_count++;
        }
    }

    return ptim_config_set_count;
}

int RM_PMGR_W_socket_dpm_tcp_sess_is_connected(char *name)
{
	dpm_tcp_sess_info * sess_info = NULL;
	sess_info = RM_PMGR_W_socket_dpm_get_sess_info_by_name(name);
	
    if (sess_info != NULL) {
        return (sess_info->pcb.state == ESTABLISHED ? pdTRUE : pdFALSE);
    }

   return pdFALSE;
}

void RM_PMGR_W_socket_dpm_tcpka_update_pcb(void)
{
    dpm_tcp_sess_info *sess_info = NULL;

    for (int i = 0 ; i < DPM_SOCK_MAX_TCP_SESS ; i++) 
    {
        sess_info = &dpm_tcp_sess_list[i];

        if (!sess_info->used)
        {
            continue;
        }

        sess_info->pcb.keep_cnt_sent = romac4rtos_get_tcpka_probes_sent_n(i);

#if LWIP_TCP_KEEPALIVE
        if (sess_info->pcb.keep_cnt_sent >= sess_info->pcb.keep_cnt)
#else
        if (sess_info->pcb.keep_cnt_sent >= TCP_KEEPCNT_DEFAULT)
#endif
        {
            /* Constrain RAM till TCP keepalive done */
            RM_PMGR_W_add_sleep_constraint(RM_PMGR_W_get_ctrl(), PMGR_CONSTRAINT_POWER_RAM);
        }
    }
}

#endif /* CFG_WIFI */

#endif /* CFG_PMGR */
