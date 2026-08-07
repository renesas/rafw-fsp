/**
 ****************************************************************************************
 *
 * @file net_dns_client.h
 *
 * @brief Define for DNS Client Module
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


#ifndef	__RA6WX_DNS_CLIENT_H__
#define	__RA6WX_DNS_CLIENT_H__

#include "FreeRTOS.h"
#include "lwipopts.h"

#include "sys_feature.h"
#include "iface_defs.h"
#include "common_def.h"

#include "net_common.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/err.h"


/* Do not commit if log's define is changed. */
#undef	DNS_DEBUG_INFO
#undef	DNS_DEBUG_TEMP
#define	DNS_DEBUG_ERROR
#define	DNS_DEBUG_PRINT

#ifdef	DNS_DEBUG_INFO
#define DNS_DBG_INFO		printf
#else
#define DNS_DBG_INFO(...)	do { } while (0);
#endif

#ifdef  DNS_DEBUG_ERROR
#define DNS_DBG_ERR		printf
#else
#define DNS_DBG_ERR(...)	do { } while (0);
#endif

#ifdef  DNS_DEBUG_TEMP
#define DNS_DBG_TEMP		printf
#else
#define DNS_DBG_TEMP(...)	do { } while (0);
#endif

#ifdef  DNS_DEBUG_PRINT
#define DNS_DBG_PRINT		printf
#else
#define DNS_DBG_PRINT(...)	do { } while (0);
#endif


#define NX_DNS_ENABLE_EXTENDED_RR_TYPES

#define DNS_WAIT				500
#define DNS_LOCAL_CACHE_SIZE	128
#define DNS_BUFFER_SIZE			200
#define DNS_RECORD_COUNT		10

#define DNS_QUERY_AAAA			"aaaa"	/* ipv6 network address */
#define DNS_QUERY_A				"a"		/* network address */
#define DNS_QUERY_SRV			"srv"	/* Location of services */
#define DNS_QUERY_ALL			"all"	/* all query */
#define DNS_QUERY_MX			"mx"	/* mail exchanger */
#define DNS_QUERY_NS			"ns"	/* name server */
#define DNS_QUERY_SOA			"soa"	/* Start of authority */
#define DNS_QUERY_HINFO			"hinfo"	/* host info */
#define DNS_QUERY_TXT			"txt"	/* txt 값 */
#define DNS_QUERY_PTR			"ptr"	/* Reverse DNS query */
#define DNS_QUERY_CNAME			"cname"	/* Canonical name */

/* Time lwIP needs to resolve a name across every configured DNS server.
 *
 * lwIP retries each server DNS_MAX_RETRIES times (with an increasing back-off of
 * DNS_TMR_INTERVAL ticks) before switching to the next configured server. The
 * time spent on a single server is therefore:
 *     (1 + 1 + 2 + ... + (DNS_MAX_RETRIES-1)) * DNS_TMR_INTERVAL
 *   = (1 + (DNS_MAX_RETRIES-1)*DNS_MAX_RETRIES/2) * DNS_TMR_INTERVAL
 *
 * A synchronous resolve must wait at least this long for every server so that a
 * dead primary server actually fails over to a working secondary one. One extra
 * interval is added as margin for the final callback dispatch. */
#define DNS_PER_SERVER_WAIT_MS	\
	((1U + (((DNS_MAX_RETRIES) - 1U) * (DNS_MAX_RETRIES)) / 2U) * (DNS_TMR_INTERVAL))
#define DNS_RESOLVE_DEFAULT_TIMEOUT_MS	\
	(((DNS_MAX_SERVERS) * DNS_PER_SERVER_WAIT_MS) + (DNS_TMR_INTERVAL))

#if defined ( __DNS_2ND_CACHE_SUPPORT__ )
#define	MAX_URL_STRING_LEN		128
#define	MAX_IPADDR_LEN			16
#define	MAX_URL_TABLE_CNT		25

/// URL to IP_Address matching table
typedef struct {
	char	domain_str[MAX_URL_STRING_LEN];
	char	ipaddr_str[MAX_IPADDR_LEN];
	uint64_t   last_time; // sec.
} domain_to_ip_addr_table_t;

typedef struct {
	domain_to_ip_addr_table_t table[MAX_URL_TABLE_CNT];
} domain_to_ip_addr_t;
#endif	// __DNS_2ND_CACHE_SUPPORT__

/* Result of a synchronous DNS resolution performed by dns_resolve(). */
typedef enum {
    DNS_RESOLVE_OK = 0,    /* Host resolved; the output address is valid.           */
    DNS_RESOLVE_INVALID,   /* Server answered but the address is unusable ("any").  */
    DNS_RESOLVE_TIMEOUT,   /* No answer was received within the wait timeout.       */
    DNS_RESOLVE_ERROR,     /* The lookup could not be started or failed.            */
    DNS_RESOLVE_NOMEM,     /* The request context could not be allocated.           */
} dns_resolve_status_t;

/* Synchronously resolve "domain" into "p_addr".
 *
 * This wrapper owns the whole request lifecycle: it allocates the lwIP request
 * context, starts the lookup, waits up to "wait_option" milliseconds for the
 * asynchronous callback, and guarantees the context is released safely even if
 * the lookup completes after this call has returned (timeout). Callers only need
 * to inspect the returned status. "addrtype" is an LWIP_DNS_ADDRTYPE_* value. */
dns_resolve_status_t dns_resolve(const char *domain, u8_t addrtype,
                                 unsigned long wait_option, ip_addr_t *p_addr);

int get_dns_addr(int iface, char *result_str);
int set_dns_addr(int iface, char *ip_addr);
int get_dns_addr_2nd(int iface, char *result_str);
int set_dns_addr_2nd(int iface, char *ip_addr);

void dns_req_resolved(const char* domain, const ip_addr_t *ipaddr, void *arg);
int ra6w1_dns_2nd_cache_add(const char* domain, char* ip_addr, unsigned long ip_addr_long);
int ra6w1_dns_2nd_cache_erase_sflash(void);
UINT ra6w1_dns_2nd_cache_find_answer(const char* domain, unsigned long* ip_addr);

bool dns_A_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option);
bool dns_AAAA_Query(char *domain_name, char *ipaddr_str, unsigned long wait_option);

unsigned int dns_ALL_Query(unsigned char *domain_name,				\
							unsigned char *record_buffer,			\
							unsigned int record_buffer_size,		\
							unsigned int *record_count,				\
							unsigned long wait_option);


#endif	/* __RA6WX_DNS_CLIENT_H__ */

/* EOF */
