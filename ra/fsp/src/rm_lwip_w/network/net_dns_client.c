/**
 ****************************************************************************************
 *
 * @file net_dns_client.c
 *
 * @brief DNS Client module
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
#include "bsp_api.h"

#if CFG_WIFI /* Compile only with WiFi stack */

#include "net_dns_client.h"
#include "task.h"
#include "semphr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "util_api.h"
#include "ip_addr.h"
#include "rm_lwip_w_helper.h"
#include "rm_vee_flash_w_rrq_nvram.h"
#ifdef RM_MAP_PERSISTANT_W
#include "rm_map_persistant_w.h"
#endif

#if LWIP_DNS

#define CACHE_EXPIRETIME    (3600*24) // Sec.

static int set_iface_dns_addr(int iface, bool Primary_DNS, char *ip_addr)
{
	UINT status = pdFAIL;
#ifdef __SUPPORT_IPV4__
	ip4_addr_t ipaddr;

	ipaddr.addr = ipaddr_addr(ip_addr);

	if (ipaddr.addr == IPADDR_NONE) {
		// converting fails ...
		status = pdFAIL;
	} else {
		// apply to dns immediately
		/* lwIP fails over to backup DNS servers in order, skipping empty
		 * slots (see dns_next_server_idx() in dpm/dns.c), so the secondary
		 * may sit at index 2. Index 1 is left for the IPv6 RDNSS server,
		 * which nd6.c writes there when IPv6 is enabled - avoiding a
		 * collision with the IPv4 secondary. */
		dns_setserver(Primary_DNS ? 0 : 2, (const ip_addr_t *)&ipaddr);

#ifdef RM_MAP_PERSISTANT_W
		// save to nvram
		if (Primary_DNS) {
			RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_0 : WIFI_PROFILE_DNSSVR_1, ip_addr);
		} else {
			// Secondary DNS
			RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_WIFIPROFILE, iface == 0 ? WIFI_PROFILE_DNSSVR_2ND_0 : WIFI_PROFILE_DNSSVR_2ND_1, ip_addr);
		}
#endif /* SET DNS */

		status = pdPASS;
	}
#endif // __SUPPORT_IPV4__
	return status;
}

int get_dns_addr(int iface, char *result_str)
{
	printf("=== [%s] Need to FreeRTOS code for this fucntion ...\n", __func__);

	RA6W1_UNUSED_ARG(iface);
	RA6W1_UNUSED_ARG(result_str);

	return pdPASS;
}

int get_dns_addr_2nd(int iface_flag, char *result_str)
{
	printf("=== [%s] Need to write FreeRTOS code for this function ...\n", __func__);

	RA6W1_UNUSED_ARG(iface_flag);
	RA6W1_UNUSED_ARG(result_str);

	return pdTRUE;
}

int set_dns_addr(int iface, char *ip_addr)
{
	return set_iface_dns_addr(iface, true, ip_addr);
}

int set_dns_addr_2nd(int iface, char *ip_addr)
{
	return set_iface_dns_addr(iface, false, ip_addr);
}

/* Shared lifetime context for an in-flight DNS request.
 *
 * lwIP invokes the resolve callback exactly once per pending request, which may
 * happen after the requester has already timed out. The context is therefore
 * heap-allocated and its ownership is shared between the requester (dns_resolve)
 * and the callback (dns_req_resolved); whichever finishes last frees it. */
typedef struct {
    SemaphoreHandle_t sem;
    ip_addr_t         result;
    volatile bool     success;     /* Set by the callback when a usable address was returned. */
    volatile bool     done;        /* Set by the callback once it has executed.               */
    volatile bool     waiter_gone; /* Set by the requester when it stops waiting (timeout).    */
} dns_req_ctx_t;

void dns_req_resolved(const char* domain, const ip_addr_t *ipaddr, void *arg)
{
	dns_req_ctx_t *ctx = (dns_req_ctx_t *)arg;
	if (!ctx)
	{
		return;
	}

	RA6W1_UNUSED_ARG(domain);

	/* lwIP calls this callback exactly once per pending request. It may run
	 * after the requester has already timed out and torn down its stack frame.
	 * In that case ownership of the context has been handed over to us and the
	 * (now invalid) output pointer must not be dereferenced. */
	bool abandoned;
	abandoned = ctx->waiter_gone;

	if (abandoned)
	{
		vSemaphoreDelete(ctx->sem);
		vPortFree(ctx);
		return;
	}

	if (ipaddr == NULL
#if defined (__SUPPORT_IPV6__) && defined (__SUPPORT_IPV4__)
	    || ip_addr_isany_val(*ipaddr)
#elif defined (__SUPPORT_IPV4__)
	    || ip4_addr_isany_val(*ip_2_ip4(ipaddr))
#elif defined (__SUPPORT_IPV6__)
	    || ip6_addr_isany_val(*ip_2_ip6(ipaddr))
#endif
	   )
	{
		/* The server answered but the address is unusable. Wake the waiter,
		 * which reports the failure based on ctx->success. */
		xSemaphoreGive(ctx->sem);
	} else
	{
		ctx->result = *ipaddr;
		ctx->success = true;

		xSemaphoreGive(ctx->sem);
	}

	bool free_now;
	taskENTER_CRITICAL();
	ctx->done = true;
	free_now = ctx->waiter_gone;
	taskEXIT_CRITICAL();
	if (free_now) {
		vSemaphoreDelete(ctx->sem);
		vPortFree(ctx);
	}
}

static dns_req_ctx_t *dns_req_ctx_create(void)
{
	dns_req_ctx_t *ctx = (dns_req_ctx_t *)pvPortMalloc(sizeof(dns_req_ctx_t));
	if (!ctx)
	{
		return NULL;
	}

	memset(ctx, 0, sizeof(dns_req_ctx_t));
	ctx->sem = xSemaphoreCreateBinary();
	if (!ctx->sem)
	{
		vPortFree(ctx);
		return NULL;
	}

	return ctx;
}

static void dns_req_ctx_finish(dns_req_ctx_t *ctx, bool callback_pending)
{
	if (!ctx)
	{
		return;
	}

	if (!callback_pending)
	{
		/* No lwIP callback is outstanding, so the context can be released now. */
		vSemaphoreDelete(ctx->sem);
		vPortFree(ctx);
		return;
	}

	/* A callback is still pending. Hand the context over to it unless it has
	 * already run, in which case we are responsible for the cleanup. */
	bool free_now;
	taskENTER_CRITICAL();
	if (ctx->done)
	{
		free_now = true;
	} else
	{
		ctx->waiter_gone = true;
		free_now = false;
	}
	taskEXIT_CRITICAL();

	if (free_now)
	{
		vSemaphoreDelete(ctx->sem);
		vPortFree(ctx);
	}
}

dns_resolve_status_t dns_resolve(const char *domain, u8_t addrtype,
                                 unsigned long wait_option, ip_addr_t *p_addr)
{
	if ((NULL == domain) || (NULL == p_addr))
	{
		return DNS_RESOLVE_ERROR;
	}

	dns_req_ctx_t *ctx = dns_req_ctx_create();
	if (!ctx)
	{
		return DNS_RESOLVE_NOMEM;
	}

	ip_addr_t dns_ipaddr;
	ip_addr_set_zero(&dns_ipaddr);

	dns_resolve_status_t status;
	err_t                ret = dns_gethostbyname_addrtype(domain, &dns_ipaddr, dns_req_resolved, ctx, addrtype);

	if (ERR_OK == ret)
	{
		/* Resolved immediately from the cache or a numeric host string; no
		 * callback is scheduled in this case. */
		if (ip_addr_isany_val(dns_ipaddr))
		{
			status = DNS_RESOLVE_INVALID;
		} else
		{
			*p_addr = dns_ipaddr;
			status  = DNS_RESOLVE_OK;
		}
	} else if (ERR_INPROGRESS == ret)
	{
		/* Request is pending; wait for the callback to signal completion. */
		if (xSemaphoreTake(ctx->sem, portCONVERT_MS_2_TICKS(wait_option)) != pdTRUE)
		{
			status = DNS_RESOLVE_TIMEOUT;
		} else if (!ctx->success)
		{
			status = DNS_RESOLVE_INVALID;
		} else
		{
			*p_addr = ctx->result;
			status  = DNS_RESOLVE_OK;
		}
	} else
	{
		status = DNS_RESOLVE_ERROR;
	}

	dns_req_ctx_finish(ctx, (ERR_INPROGRESS == ret));
	return status;
}

bool dns_A_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option)
{
	ip_addr_t addr;

	memset(ipaddr_str, 0, IPADDR_LEN);

	if (dns_resolve(domain_name, LWIP_DNS_ADDRTYPE_IPV4, wait_option, &addr) != DNS_RESOLVE_OK)
	{
		return pdFALSE;
	}

	ipaddr_ntoa_r(&addr, ipaddr_str, IPADDR_LEN);
	return pdTRUE;
}

bool dns_AAAA_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option)
{
	ip_addr_t addr;

	memset(ipaddr_str, 0, IP6ADDR_LEN);

	if (dns_resolve(domain_name, LWIP_DNS_ADDRTYPE_IPV6, wait_option, &addr) != DNS_RESOLVE_OK)
	{
		return pdFALSE;
	}

	ipaddr_ntoa_r(&addr, ipaddr_str, IP6ADDR_LEN);
	return pdTRUE;
}

unsigned int dns_ALL_Query(unsigned char *domain_name,
						unsigned char *record_buffer,
						unsigned int record_buffer_size,
						unsigned int *record_count,
						unsigned long wait_option)	/* GET_MULTI_IP */
{
	RA6W1_UNUSED_ARG(wait_option);
	
#ifdef __SUPPORT_IPV4__
	int status;
	UINT rec_cnt = 0;
	UINT32* rec_buf = NULL;
	struct addrinfo hints; 
	struct addrinfo *addr_list, *cur;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (record_buffer == NULL || record_count == NULL) {
   		printf("[%s] record_buffer(NULL) / record_count(NULL) \n", __func__);
        return 1; // error
	}

    if((status = getaddrinfo((const char *)domain_name, NULL, &hints, &addr_list)) != 0) {
   		printf("[%s] DNS query error(0x%0x). [%s], see netdb.h\n",
			   __func__, (int)status, domain_name);
        return 1; // error
    }

	rec_buf = (UINT32*)record_buffer;
	
    for (cur = addr_list; (cur != NULL) && (rec_cnt <= record_buffer_size); cur = cur->ai_next) {
       *rec_buf = (UINT32)(((struct sockaddr_in*)(cur->ai_addr))->sin_addr.s_addr);
	   rec_cnt++;
	   rec_buf++;
    }
	
	*record_count = rec_cnt;
	
    freeaddrinfo( addr_list );
#endif // __SUPPORT_IPV4__
	return 0;

}

#endif /* LWIP_DNS */
#endif /* CFG_WIFI */

/* EOF */
