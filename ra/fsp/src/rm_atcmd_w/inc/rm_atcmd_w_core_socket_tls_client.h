/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#ifndef RM_ATCMD_W_CORE_SOCKET_TLS_CLIENT_H
#define RM_ATCMD_W_CORE_SOCKET_TLS_CLIENT_H

#include "FreeRTOS.h"
#include "custom_config_sdk.h"

#include "task.h"

#include "rm_atcmd_w_core_socket_parse.h"
#include "rm_atcmd_w_core_socket_cert_mng.h"

#include "common.h"
#include "mbedtls/private_access.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/rsa.h"
#include "mbedtls/net_sockets.h"

/* Common macro for FSP header files.
 * There is also a corresponding FSP_FOOTER
 * macro at the end of this file. */
FSP_HEADER

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define ATCMD_TLSC_PREFIX_TASK_NAME		"atc_tlsc_task"
#define ATCMD_TLSC_PREFIX_SOCKET_NAME	"atc_tlsc_sock"
#define ATCMD_TLSC_PREFIX_TLS_NAME		"atc_tlsc_tls"

#define ATCMD_TLSC_MAX_THREAD_NAME		20
#define ATCMD_TLSC_MAX_QUEUE_NAME		20
#define ATCMD_TLSC_MAX_SOCKET_NAME		20
#define ATCMD_TLSC_MAX_TLS_NAME			20

#define ATCMD_TLSC_TASK_SIZE			(1024)	//word
#define ATCMD_TLSC_TASK_PRIORITY		(tskIDLE_PRIORITY + 8)

#define ATCMD_TLSC_SLEEP_TIMEOUT		10
#define ATCMD_TLSC_HANDSHAKE_TIMEOUT	500
#define ATCMD_TLSC_DEF_TIMEOUT			500
#define ATCMD_TLSC_RECV_TIMEOUT			10
#define ATCMD_TLSC_RECONN_SLEEP_TIMEOUT	100
#define ATCMD_TLSC_HOST_TIMEOUT         1000
#define ATCMD_TLSC_CONN_TIMEOUT			300
#define ATCMD_TLSC_DISCONN_TIMEOUT		100
#define ATCMD_TLSC_MAX_CONN_CNT			5
#define ATCMD_TLSC_MAX_HOSTNAME			64
#define ATCMD_TLSC_WDOG_LATENCY			4

#define ATCMD_TLSC_MAX_ADDRSTRLEN		64
#define ATCMD_TLSC_MAX_PORTSTRLEN		8

#define ATCMD_TLSC_DATA_BUF_SIZE		(1024 * 2)

#define ATCMD_TLSC_MIN_INCOMING_LEN		(1024)
#define ATCMD_TLSC_MAX_INCOMING_LEN		(1024 * 17)
#define ATCMD_TLSC_DEF_INCOMING_LEN		(1024 * 4)

#define ATCMD_TLSC_MIN_OUTGOING_LEN		(1024)
#define ATCMD_TLSC_MAX_OUTGOING_LEN		(1024 * 17)
#define ATCMD_TLSC_DEF_OUTGOING_LEN		(1024 * 4)

#define ATCMD_TLSC_MIN_RETAINED_HEAP_MEMORY (20 * KBYTE)
#define ATCMD_TLSC_SERIAL_WAIT_CNT      (5)
#define ATCMD_TLSC_SERIAL_WAIT_TIME     (100)   // msec
#define ATCMD_TLSC_DATA_RX_PREFIX_LEN   (256)
#define ATCMD_TLSC_DATA_TX_WAIT_CNT     (5)
#define ATCMD_TLSC_DATA_TX_WAIT_TIME    (1000)  // msec

/// Rx TLS Client message Prefix
#define ATCMD_TLSC_DATA_RX_PREFIX		"+TRSSLDTC"

/// TLS Client Disconnection Prefix
#define ATCMD_TLSC_DISCONN_RX_PREFIX	"+TRSSLXTC"

// AT+TRSSLPRT AT-Command
#define ATCMD_TLSC_STATE_CONN           1
#define ATCMD_TLSC_STATE_DISCONN        0

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
typedef enum _atcmd_tlsc_state
{
    ATCMD_TLSC_STATE_TERMINATED     = 0,
    ATCMD_TLSC_STATE_DISCONNECTED   = 1,
    ATCMD_TLSC_STATE_CONNECTED      = 2,
    ATCMD_TLSC_STATE_REQ_TERMINATE  = 3,
} atcmd_tlsc_state;

typedef enum _atcmd_tls_version
{
    ONLY_TLS12 = 0, // default
    ONLY_TLS13 = 1,
    TLS12_13 = 2,
} atcmd_tls_version;

typedef struct _atcmd_tlsc_config
{
    atcmd_tls_version tls_ver;
    char ca_cert_name[ATCMD_CM_MAX_NAME];
    char cert_name[ATCMD_CM_MAX_NAME];

    unsigned int auth_mode;
    unsigned int incoming_buflen;
    unsigned int outgoing_buflen;

    char hostname[ATCMD_TLSC_MAX_HOSTNAME];

    unsigned int local_port;
    char svr_addr[ATCMD_TLSC_MAX_ADDRSTRLEN];
    char svr_port[ATCMD_TLSC_MAX_PORTSTRLEN];
    unsigned int iface;

    size_t recv_buflen;

    unsigned long task_priority;
    size_t task_size;
} atcmd_tlsc_config;

typedef struct _atcmd_tlsc_context
{
    atcmd_tlsc_state state;
    int cid;

    TaskHandle_t task_handler;
    unsigned char task_name[ATCMD_TLSC_MAX_THREAD_NAME];

    unsigned int timeout;
    unsigned char * recv_buf;

    mbedtls_net_context net_ctx;
    char socket_name[ATCMD_TLSC_MAX_SOCKET_NAME];
    unsigned int bind_port;

    mbedtls_ssl_context * ssl_ctx;
    mbedtls_ssl_config * ssl_conf;
    mbedtls_ctr_drbg_context * ctr_drbg_ctx;
    mbedtls_entropy_context * entropy_ctx;

    mbedtls_x509_crt * ca_cert_crt;
    mbedtls_x509_crt * cert_crt;
    mbedtls_pk_context * pkey_ctx;
    mbedtls_pk_context * alt_pkey_ctx;

    mbedtls_x509_crt_profile crt_profile;

    char tls_name[ATCMD_TLSC_MAX_TLS_NAME];

    void * p_at_ctrl;

    atcmd_tlsc_config * conf;
} atcmd_tlsc_context;

/***********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
// External
void atcmd_tlsc_set_malloc_free(void * (*malloc_func)(size_t), void (*free_func)(void *));
int atcmd_tlsc_init_context(atcmd_tlsc_context * ctx);
int atcmd_tlsc_deinit_context(atcmd_tlsc_context * ctx);
int atcmd_tlsc_set_at_ctrl(atcmd_tlsc_context * ctx, void * const p_at_ctrl);
int atcmd_tlsc_setup_config(atcmd_tlsc_context * ctx, atcmd_tlsc_config * conf);
int atcmd_tlsc_set_incoming_buflen(atcmd_tlsc_config * conf, unsigned int buflen);
int atcmd_tlsc_set_outgoing_buflen(atcmd_tlsc_config * conf, unsigned int buflen);
int atcmd_tlsc_set_hostname(atcmd_tlsc_config * conf, char * hostname);
int atcmd_tlsc_run(atcmd_tlsc_context * ctx, int id);
int atcmd_tlsc_stop(atcmd_tlsc_context * ctx, unsigned int wait_option);

int atcmd_tlsc_write_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t * data_len);

// Internal
int atcmd_tlsc_init_socket(atcmd_tlsc_context * ctx);
int atcmd_tlsc_connect_socket(atcmd_tlsc_context * ctx, unsigned int wait_option);
int atcmd_tlsc_disconnect_socket(atcmd_tlsc_context * ctx);
int atcmd_tlsc_send_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t * data_len);
int atcmd_tlsc_recv(atcmd_tlsc_context * ctx, unsigned char * out, size_t outlen,
                    unsigned int wait_option);
int atcmd_tls_rsa_decrypt_func(void * ctx, size_t * olen,
                               const unsigned char * input, unsigned char * output,
                               size_t output_max_len);
int atcmd_tls_rsa_sign_func(void * ctx,
                            int (*f_rng)(void *, unsigned char *, size_t), void * p_rng,
                            mbedtls_md_type_t md_alg, unsigned int hashlen,
                            const unsigned char * hash, unsigned char * sig);
size_t atcmd_tls_rsa_key_len_func(void * ctx);
#if CFG_PMGR
int atcmd_tlsc_store_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_restore_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_clear_ssl(atcmd_tlsc_context * ctx);
#endif /* CFG_PMGR */
int atcmd_tlsc_init_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_setup_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_deinit_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_shutdown_ssl(atcmd_tlsc_context * ctx);
int atcmd_tlsc_do_handshake(atcmd_tlsc_context * ctx, unsigned long wait_option);
void atcmd_tlsc_transfer_data(atcmd_tlsc_context * ctx, unsigned char * data, size_t data_len);
void atcmd_tlsc_transfer_disconn_data(atcmd_tlsc_context * ctx);
void atcmd_tlsc_entry_func(void * pvParamters);
int atcmd_tlsc_get_local_port(atcmd_tlsc_context * ctx, unsigned int * port);

/* Common macro for FSP header files.
 * There is also a corresponding FSP_HEADER
 * macro at the top of this file. */
FSP_FOOTER

#endif // RM_ATCMD_W_CORE_SOCKET_TLS_CLIENT_H


