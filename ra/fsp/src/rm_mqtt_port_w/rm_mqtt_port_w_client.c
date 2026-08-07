/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "sdk_defs.h"                  /* For bsp_api.h, OS_FREERTOS */
#include "rm_mqtt_port_w.h"
#include "mqtt_client.h"
#include "memory_mosq.h"               /* For _mosquitto_calloc */
#include "rm_vee_flash_w_rrq_nvram.h"  /* For delete_nvram_appcfg_env */
#include "net_network_main.h"          /* For ra6w1_network_main_is_wlaninit */
#ifdef RM_MAP_PERSISTANT_W
 #include "rm_map_persistant_w.h"
#endif

/***********************************************************************************************************************
 * Defines
 **********************************************************************************************************************/
#define MQTT_CLIENT_CERT_START        "\x1B"
#define MQTT_CLIENT_CERT_END          "\x03"

/* Predefined timeout values */
#define MQTT_CLIENT_TIMEOUT_100MS     (100)
#define MQTT_CLIENT_TIMEOUT_400MS     (400)
#define MQTT_CLIENT_TIMEOUT_500MS     (500)
#define MQTT_CLIENT_TIMEOUT_1SEC      (1000)

#define MQTT_CLIENT_MAX_CERT_SIZE     (2045)

#define MQTT_CLIENT_RETURN_TEXT_OK    "OK"
#define MQTT_OPEN                     (0x4d515454ULL)
#define MQTT_CLOSED                   (0)

#define MQTT_CLIENT_CMD_ARG_NUM       (5)

/* No Error */
#define CC_STATUS_SUCCESS             0

/* Too long string value input */
#define CC_FAILURE_STRING_LENGTH      1

/* No value input */
#define CC_FAILURE_NO_VALUE           2

/* Range out */
#define CC_FAILURE_RANGE_OUT          3

/* Not Supported input */
#define CC_FAILURE_NOT_SUPPORTED      4

/* Invalid input */
#define CC_FAILURE_INVALID            5

/* Memory Allocation Failure */
#define CC_FAILURE_NO_ALLOCATION      6

/* Not Ready (Wi-Fi connection or Network setting) */
#define CC_FAILURE_NOT_READY          7

/* Unknown Reason */
#define CC_FAILURE_UNKNOWN            9

/* Enable/Disable values */
typedef enum
{
    /* Not used */
    CC_VAL_DISABLE,

    /* Used */
    CC_VAL_ENABLE,
} cc_val_bool;

/***********************************************************************************************************************
 * Extern variables
 **********************************************************************************************************************/
extern int  getMacAddrMswLsw(UINT iface, ULONG * macmsw, ULONG * maclsw);
extern void mqtt_client_delete_tls_alpns(void);
extern void mqtt_client_delete_cipher_suits(void);

/***********************************************************************************************************************
 * Static Globals
 **********************************************************************************************************************/
static mqtt_client_instance_ctrl_t * gp_ctrl;
static char gs_topic[MQTT_CLIENT_MAX_TOPIC_LEN];

static char gs_message[MQTT_CLIENT_MAX_PUBMSG_LEN + 1];
static char gs_alpn_count[4];
static char gs_alpns[MQTT_CLIENT_MAX_ALPN][MQTT_CLIENT_ALPN_MAX_LEN + 1];
static char gs_cipher_suites[MQTT_CLIENT_TLS_CIPHER_MAX_CNT][5];

/***********************************************************************************************************************
 * Local function prototypes
 **********************************************************************************************************************/
static int  rm_mqtt_client_make_message(char * title, char * buf, size_t buflen);
static void rm_mqtt_client_cmd_mqtt_client_help(void);

#ifdef MQTT_NVRAM_ENABLE
static void rm_mqtt_client_id_number_output(char * id_num);
static UINT rm_mqtt_client_mqtt_client_add_pub_topic(const char * topic);
static UINT rm_mqtt_client_mqtt_client_add_sub_topic(const char * topic);
static UINT rm_mqtt_client_mqtt_client_del_sub_topic(const char * topic);
static void rm_mqtt_client_mqtt_client_delete_sub_topics(void);
static int  rm_mqtt_client_mqtt_client_config_initialize(void);
static UINT rm_mqtt_client_mqtt_client_set_broker_info(char * broker_ip, int port);

#endif                                 /* MQTT_NVRAM_ENABLE */
static UINT rm_mqtt_client_mqtt_client_config(mqtt_client_instance_ctrl_t * p_ctrl, int argc, const char * argv[]);
static void mqtt_publish_cb(int mid);
static void mqtt_connected_cb(void);
static void mqtt_disconnected_cb(void);
static void mqtt_message_cb(const char * buf, int len, const char * topic);
static void mqtt_subscribe_cb(void);
static void mqtt_unsubscribe_cb(void);

/***********************************************************************************************************************
 * Public Functions Implementation
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup MQTT_PORT_W
 * @{
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 *  Initialize the MQTT Client service.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  p_cfg                Pointer to MQTT Client configuration structure.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_cfg instance is NULL.
 * @retval FSP_ERR_INVALID_ARGUMENT Data size is too large or NULL.
 * @retval FSP_ERR_ALREADY_OPEN     The instance has already been opened.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Open (mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_cfg_t const * const p_cfg)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_cfg);
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_cfg->p_callback);

    FSP_ERROR_RETURN(MQTT_OPEN != p_ctrl->open, FSP_ERR_ALREADY_OPEN);
#endif

    p_ctrl->p_cfg             = p_cfg;
    p_ctrl->is_mqtt_connected = false;
    gp_ctrl = p_ctrl;
    memset(gs_topic, 0x00, sizeof(gs_topic));
    memset(gs_message, 0x00, sizeof(gs_message));

    /* Print status */
    const char * config_status[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "status"};

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_status);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Set MQTT 3.1.1 settings */
    const char * config_ver311[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "ver311"};

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_ver311);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Check for host name and port */
    FSP_ERROR_RETURN(NULL != p_cfg->p_host_name, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(0 != p_cfg->mqtt_port, FSP_ERR_INVALID_ARGUMENT);

    /* Init mqtt callbacks */
    mqtt_sub_callback_set(mqtt_connected_cb);
    mqtt_pub_callback_set(mqtt_publish_cb);
    mqtt_msg_callback_set(mqtt_message_cb);
    mqtt_sub_disconn_cb_set(mqtt_disconnected_cb);
    mqtt_subscribe_callback_set(mqtt_subscribe_cb);
    mqtt_unsubscribe_callback_set(mqtt_unsubscribe_cb);

    /* Set the host name of the MQTT broker */
    const char * config_broker_ip[MQTT_CLIENT_CMD_ARG_NUM]   = {"mqtt_config", "broker"};
    const char * config_broker_port[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "port"};

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_broker_ip);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_broker_port);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Store the MQTT Username and Password */
    if ((NULL != p_cfg->p_mqtt_user_name) && (NULL != p_cfg->p_mqtt_password))
    {
        const char * config_username[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "username"};
        const char * config_password[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "password"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_username);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_password);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

#ifdef MQTT_TLS

        /* Enable TLS */
        const char * config_tls[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "tls", "0"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_tls);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

        const char * config_authmode[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "tls_authmode", "0"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_authmode);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

        /* Program the root CA Certificate and the private key */
        struct mosq_config set_mqtt_cfg;

        /* Put the RRQ61000 module into certificate/key input mode */
        set_mqtt_cfg.cacert_ptr      = NULL;
        set_mqtt_cfg.cacert_buflen   = 0;
        set_mqtt_cfg.cert_ptr        = NULL;
        set_mqtt_cfg.cert_buflen     = 0;
        set_mqtt_cfg.priv_key_ptr    = NULL;
        set_mqtt_cfg.priv_key_buflen = 0;
        set_mqtt_cfg.dh_param_ptr    = NULL;
        set_mqtt_cfg.dh_param_buflen = 0;

        /* Send certificate/key ascii text */
        err = mqtt_client_cert_write(&set_mqtt_cfg);
        FSP_ERROR_RETURN(pdPASS == err, FSP_ERR_INVALID_ARGUMENT);
#endif                                 /* MQTT_TLS */
    }
    else
    {
        /* Clear the MQTT Username and Password */
        set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, NULL);
        set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, NULL);

        /* Store the TLS certificate/private key */
        if ((NULL != p_cfg->p_root_ca) && (NULL != p_cfg->p_client_cert) &&
            (NULL != p_cfg->p_client_private_key))
        {
            /* Check the certificates/keys provided to ensure they are smaller than the maximum size */
            FSP_ERROR_RETURN((p_cfg->root_ca_size <= MQTT_CLIENT_MAX_CERT_SIZE) ||
                             (p_cfg->client_cert_size <= MQTT_CLIENT_MAX_CERT_SIZE) ||
                             (p_cfg->private_key_size <= MQTT_CLIENT_MAX_CERT_SIZE),
                             FSP_ERR_INVALID_ARGUMENT);

#ifdef MQTT_TLS

            /* Enable TLS */
            const char * config_tls[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "tls", "1"};

            err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_tls);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

            const char * config_authmode[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "tls_authmode", "2"};

            err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_authmode);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

            /* Program the root CA Certificate and the private key */
            struct mosq_config set_mqtt_cfg;

            /* Put the RRQ61000 module into certificate/key input mode */
            set_mqtt_cfg.cacert_ptr      = (char *) p_cfg->p_root_ca;
            set_mqtt_cfg.cacert_buflen   = p_cfg->root_ca_size;
            set_mqtt_cfg.cert_ptr        = (char *) p_cfg->p_client_cert;
            set_mqtt_cfg.cert_buflen     = p_cfg->client_cert_size;
            set_mqtt_cfg.priv_key_ptr    = (char *) p_cfg->p_client_private_key;
            set_mqtt_cfg.priv_key_buflen = p_cfg->private_key_size;
            set_mqtt_cfg.dh_param_ptr    = NULL;
            set_mqtt_cfg.dh_param_buflen = 0;

            /* Send certificate/key ascii text */
            err = mqtt_client_cert_write(&set_mqtt_cfg);
            FSP_ERROR_RETURN(pdPASS == err, FSP_ERR_INVALID_ARGUMENT);
#endif                                 /* MQTT_TLS */
        }
    }

    FSP_ERROR_RETURN(0 != p_cfg->keep_alive_seconds, FSP_ERR_INVALID_ARGUMENT);

    /* Set the MQTT ping period */
    const char * config_ping_period[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "ping_period"};

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_ping_period);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Perform MQTT optional settings */

    /* Set MQTT ALPN settings */
    if (p_cfg->alpn_count > 0)
    {
        const char * config_tls_alpn[3 + MQTT_CLIENT_MAX_ALPN] = {"mqtt_config", "tls_alpn"};

        /* Set up ALPN protocol and count packet */
        sprintf((char *) &gs_alpn_count, "%d", p_cfg->alpn_count);
        config_tls_alpn[2] = (char *) &gs_alpn_count;
        for (int i = 0; i < p_cfg->alpn_count; i++)
        {
            sprintf((char *) &gs_alpns[i], "%s", p_cfg->p_alpns[i]);
            config_tls_alpn[3 + i] = &gs_alpns[i][0];
        }

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3 + p_cfg->alpn_count, config_tls_alpn);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }
    else
    {
        mqtt_client_delete_tls_alpns();
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                      0);
#endif
    }

    /* Set MQTT TLS Cipher Suite settings */
    if (p_cfg->tls_cipher_count > 0)
    {
        const char * config_tls_cipher[2 + MQTT_CLIENT_TLS_CIPHER_MAX_CNT] = {"mqtt_config", "tls_cipher"};

        for (int i = 0; i < p_cfg->tls_cipher_count; i++)
        {
            sprintf((char *) &gs_cipher_suites[i], "%4X", p_cfg->p_tls_cipher_suites[i]);
            config_tls_cipher[2 + i] = &gs_cipher_suites[i][0];
        }

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2 + p_cfg->tls_cipher_count, config_tls_cipher);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }
    else
    {
        mqtt_client_delete_cipher_suits();
#ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM,
                                      0);
#endif
    }

    /* Set MQTT SNI settings */
    if ((NULL != p_cfg->p_sni_name) && (sizeof(p_cfg->p_sni_name) < MQTT_CLIENT_MAX_SNI_LEN))
    {
        const char * config_tls_sni[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "tls_sni"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_tls_sni);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }
    else
    {
        set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, NULL);
    }

    /* Set MQTT Clean Session settings */
    if ((0 == p_cfg->clean_session) || (1 == p_cfg->clean_session))
    {
        const char * config_clean_session[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "clean_session"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_clean_session);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }

    /* Set MQTT Client Identifier settings */
    if (0 != p_cfg->client_identifier_length)
    {
        const char * config_client_id[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "client_id"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_client_id);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }

    /* Set MQTT Last Will settings */
    if (NULL != p_cfg->p_will_topic)
    {
        const char * config_will_topic[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "will_topic"};
        const char * config_will_msg[MQTT_CLIENT_CMD_ARG_NUM]   = {"mqtt_config", "will_message"};
        const char * config_will_qos[MQTT_CLIENT_CMD_ARG_NUM]   = {"mqtt_config", "will_qos"};

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_will_topic);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_will_msg);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, config_will_qos);
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }

    p_ctrl->open = MQTT_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Disconnect from MQTT Client service.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_ctrl instance is NULL.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened or the client is not connected.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Disconnect (mqtt_client_instance_ctrl_t * p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(MQTT_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ERROR_RETURN(true == p_ctrl->is_mqtt_connected, FSP_ERR_NOT_OPEN);
#endif

    /* Disable the MQTT Client */
    const char * client_stop[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_client", "stop"};
    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, client_stop);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    p_ctrl->is_mqtt_connected = false;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * Configure and connect the MQTT Client service.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  timeout_ms           Timeout in milliseconds.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_ctrl is NULL.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened.
 * @retval FSP_ERR_IN_USE           The MQTT client is already connected.
 * @retval FSP_ERR_INVALID_DATA     Response does not contain Connect status.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Connect (mqtt_client_instance_ctrl_t * p_ctrl, uint32_t timeout_ms)
{
    (void) timeout_ms;
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(MQTT_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ERROR_RETURN(false == p_ctrl->is_mqtt_connected, FSP_ERR_IN_USE);
#endif

    /* Enable the MQTT Client */
    const char * client_start[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_client", "start"};

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, client_start);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    p_ctrl->is_mqtt_connected = 1;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Publish a message for a given MQTT topic.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  p_pub_info           MQTT Publish packet parameters.
 *
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_ctrl, p_pub_info is NULL.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened or the client is not connected.
 * @retval FSP_ERR_INVALID_DATA     Data size is too large.
 * @retval FSP_ERR_OUT_OF_MEMORY    Memory allocation error.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Publish (mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_pub_info_t * const p_pub_info)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_pub_info);
    FSP_ERROR_RETURN(p_pub_info->payload_length <= MQTT_CLIENT_MAX_PUBMSG_LEN, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN(p_pub_info->topic_name_Length <= MQTT_CLIENT_MAX_TOPIC_LEN, FSP_ERR_INVALID_DATA);
    FSP_ERROR_RETURN(true == p_ctrl->is_mqtt_connected, FSP_ERR_NOT_OPEN);
#endif

    /* Publish an MQTT message with topic and payload */
    if ((p_pub_info->topic_name_Length + p_pub_info->payload_length) <= MQTT_CLIENT_MAX_PUBTOPICMSG_LEN)
    {
        /* Set MQTT QoS level settings */
        char qos_level[4] = {0};
        const char * config_qos[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "qos", NULL};

        int payload_len_str_max_len = snprintf(NULL, 0, "%u", MQTT_CLIENT_MAX_PUBMSG_LEN);

        if (payload_len_str_max_len < 0)
        {
            FSP_RETURN(FSP_ERR_INVALID_STATE);
        }

        char * payload_len_str = _mosquitto_malloc((size_t) payload_len_str_max_len + 1);
        if (payload_len_str == NULL)
        {
            FSP_RETURN(FSP_ERR_OUT_OF_MEMORY);
        }
        memset(payload_len_str, 0x00, (size_t) payload_len_str_max_len + 1);
        
        sprintf(qos_level, "%d", p_pub_info->qos);
        config_qos[2] = qos_level;

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_qos);
        if (FSP_SUCCESS != err)
        {
            _mosquitto_free(payload_len_str);
            FSP_RETURN(err);
        }

        snprintf(payload_len_str, (size_t) payload_len_str_max_len, "%lu", p_pub_info->payload_length);
        const char * message_topic[MQTT_CLIENT_CMD_ARG_NUM] =
        {
            "mqtt_client", "-m", (const char *) p_pub_info->p_payload, (const char *) p_pub_info->p_topic_name, 
            (const char *) payload_len_str
        };

        err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 5, message_topic);
        
        _mosquitto_free(payload_len_str);
        
        FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    }
    else
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Subscribe to MQTT topics.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  p_sub_info           List of MQTT subscription info.
 * @param[in]  subscription_count   Number of topics to subscribe to.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_ctrl, p_sub_info is NULL or subscription_count is 0.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened.
 * @retval FSP_ERR_INVALID_DATA     Data size is too large.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Subscribe (mqtt_client_instance_ctrl_t  * p_ctrl,
                                    mqtt_client_sub_info_t * const p_sub_info,
                                    size_t                         subscription_count)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_sub_info);
    FSP_ASSERT(0 != subscription_count);
    FSP_ERROR_RETURN(MQTT_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    /* Set the MQTT subscriber topic */
    if (subscription_count < MQTT_CLIENT_SUBTOPIC_MAX_CNT)
    {
        for (int i = 0; i < (int) subscription_count; i++)
        {
            /* Set MQTT QoS level settings */
            char qos_level[4];
            sprintf((char *) &qos_level, "%d", p_sub_info[i].qos);
            const char * config_qos[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_config", "qos", (char *) &qos_level};

            err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_qos);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

            const char * config_add_topic[MQTT_CLIENT_CMD_ARG_NUM] =
            {
                "mqtt_config", "sub_topic_add", (char *) p_sub_info[i].p_topic_filter
            };

            err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, config_add_topic);
            FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
        }
    }
    else
    {
        return FSP_ERR_WIFI_FAILED;
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Unsubscribe from MQTT topics.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  p_sub_info           List of MQTT subscription info.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_WIFI_FAILED      Error occurred with command to Wifi module.
 * @retval FSP_ERR_ASSERTION        The p_ctrl, p_sub_info is NULL.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened or the client is not connected.
 * @retval FSP_ERR_INVALID_DATA     Data size is too large.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_UnSubscribe (mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_sub_info_t * const p_sub_info)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_sub_info);
    FSP_ERROR_RETURN(true == p_ctrl->is_mqtt_connected, FSP_ERR_NOT_OPEN);
#endif

    const char * message_topic_delete[MQTT_CLIENT_CMD_ARG_NUM] =
    {
        "mqtt_client", "unsub", (char *) p_sub_info->p_topic_filter
    };

    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 3, message_topic_delete);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Receive data subscribed to MQTT Client service.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 * @param[in]  p_cfg                Pointer to MQTT Client configuration structure.
 *
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_ASSERTION        The p_ctrl, p_textstring, p_ip_addr is NULL.
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened or the client is not connected.
 * @retval FSP_ERR_INVALID_DATA     Receive function did not receive valid publish data.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Receive (mqtt_client_instance_ctrl_t * p_ctrl, mqtt_client_cfg_t const * const p_cfg)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(true == p_ctrl->is_mqtt_connected, FSP_ERR_NOT_OPEN);
#endif

    if (true == p_ctrl->is_mqtt_connected)
    {
        size_t xReceivedBytes = strlen(gs_message);

        if (xReceivedBytes > 0)
        {
            mqtt_data.p_data      = (uint8_t *) &gs_message;
            mqtt_data.p_topic     = (char *) &gs_topic;
            mqtt_data.data_length = (uint32_t) strlen(gs_message);
            mqtt_context.event    = MQTT_EVENT_MESSAGED;
            mqtt_data.p_context   = &mqtt_context;

            /* Call the user callback with successful data */
            p_cfg->p_callback(&mqtt_data);
        }
        else
        {
            FSP_RETURN(FSP_ERR_INVALID_DATA);
        }
    }

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 *  Close the MQTT Client service.
 *
 * @param[in]  p_ctrl               Pointer to MQTT Client instance control structure.
 *
 * @retval FSP_ERR_NOT_OPEN         The instance has not been opened.
 * @retval FSP_SUCCESS              Function completed successfully.
 * @retval FSP_ERR_ASSERTION        The p_ctrl, p_textstring, p_ip_addr is NULL.
 **********************************************************************************************************************/
fsp_err_t RM_MQTT_CLIENT_Close (mqtt_client_instance_ctrl_t * p_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Do parameter checking */
#if (1 == MQTT_PORT_W_CFG_PARAM_CHECKING_ENABLED)
    FSP_ASSERT(NULL != p_ctrl);
    FSP_ERROR_RETURN(MQTT_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
#endif

    const char * client_stop[MQTT_CLIENT_CMD_ARG_NUM] = {"mqtt_client", "stop"};
    err = rm_mqtt_client_cmd_mqtt_client(p_ctrl, 2, client_stop);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    p_ctrl->open = MQTT_CLOSED;
    gp_ctrl      = NULL;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup MQTT_PORT_W)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

static int rm_mqtt_client_make_message (char * title, char * buf, size_t buflen)
{
    (void) title;

    char         ch;
    unsigned int msglen = 0;
#ifdef  SKIP_DELIMETER
    int skip = FALSE;
#endif                                 /* SKIP_DELIMETER */

    if (buf == NULL)
    {
        MQTT_DBG_INFO("[%s]Invalid buffer size\n", __func__);

        return -1;
    }

    MQTT_DBG_INFO("Typing data: (%s)\n\tCancel - CTRL+D, "
                  "End of Input - CTRL+C or CTRL+Z\n",
                  title);

    while (1)
    {
        ch = (char) getchar();
        if ((0x03 == ch) || (0x04 == ch) || (0x1a == ch)) /* CTRL+C, CTRL+D, CTRL+Z, */
        {
            break;
        }

#ifdef  SKIP_DELIMETER
        if ('[' == ch)
        {
            skip = TRUE;
            continue;
        }
        else if (']' == ch)
        {
            skip = FALSE;
            continue;
        }
        else if (TRUE == skip)
        {
            continue;
        }
#endif                                 /* SKIP_DELIMETER */

        if (0x0D == ch)
        {
            ch = 0x0A;
        }

        putchar(ch);                   /* local echo */

        msglen++;
        if (msglen > (buflen - 1))
        {
            MQTT_DBG_INFO("\nToo long input data (MAX Length : %d byte)\n", buflen - 1);

            return -1;
        }

        buf[(msglen - 1)] = (char) ch;
    }

    if ((0x03 == ch) || (0x1a == ch))  /* CTRL+C, CTRL+Z, */
    {
        buf[msglen] = '\0';
    }
    else /* CTRL+D */
    {
        /* cancel */
        buf[0] = '\0';
        msglen = 0;
    }

    return (int) msglen;
}

static void rm_mqtt_client_cmd_mqtt_client_help (void)
{
    MQTT_DBG_INFO("- mqtt_client\n\n");
    MQTT_DBG_INFO("  Usage : mqtt_client [option]\n");
    MQTT_DBG_INFO("    option\n");
    MQTT_DBG_INFO("    <start>\t\t    : start mqtt_client\n");
    MQTT_DBG_INFO("    <stop>\t\t    : stop mqtt_client\n");
    MQTT_DBG_INFO("    <check>\t\t    : shows mqtt_client connection status\n");
    MQTT_DBG_INFO("    <unsub> <topic>\t    : unsubscribe from the topic \n");
    MQTT_DBG_INFO("    <-m> <msg> [<topic>]    : publish <msg> w/ <topic> if specified \n");
    MQTT_DBG_INFO("    <-l>\t\t    : publish large <msg>\n");
}

#if defined(__MQTT_CLEAN_SESSION_MODE_SUPPORT__)
static const char * c0_cmd[] = {"mqtt_config", "clean_session", "0"};
static const char * c1_cmd[] = {"mqtt_config", "clean_session", "1"};
#endif                                 /* __MQTT_CLEAN_SESSION_MODE_SUPPORT__ */

fsp_err_t rm_mqtt_client_cmd_mqtt_client (mqtt_client_instance_ctrl_t * p_ctrl, int argc, const char * argv[])
{
    int res_val = 0;

    if ((strcmp(argv[0], "mqtt_client") == 0) || (strcmp(argv[0], "mqtt_sub") == 0))
    {
        if (strcmp(argv[1], "start") == 0)
        {
#ifndef MQTT_MOCK
            if (!ra6w1_network_main_is_wlaninit())
            {
                FSP_RETURN(FSP_ERR_WIFI_FAILED);
            }
#endif                                 /* MQTT_MOCK */
#if defined(__MQTT_CLEAN_SESSION_MODE_SUPPORT__)
            if (3 == argc)
            {
                if (strcmp(argv[2], "c0") == 0)
                {
                    res_val = (int) rm_mqtt_client_mqtt_client_config(p_ctrl, 3, c0_cmd);
                    FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_INVALID_STATE);
                }
                else if (strcmp(argv[2], "c1") == 0)
                {
                    res_val = (int) rm_mqtt_client_mqtt_client_config(p_ctrl, 3, c1_cmd);
                    FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_INVALID_STATE);
                }
            }
#endif                                 /* __MQTT_CLEAN_SESSION_MODE_SUPPORT__ */

            if (mqtt_client_is_running() == TRUE)
            {
                mqtt_client_force_stop();
                mqtt_client_stop_sub();
            }

            mqtt_client_start_sub();
        }
        else if (strcmp(argv[1], "stop") == 0)
        {
#ifndef MQTT_MOCK
            if (!ra6w1_network_main_is_wlaninit())
            {
                FSP_RETURN(FSP_ERR_WIFI_FAILED);
            }
#endif                                 /* MQTT_MOCK */
            if (mqtt_client_is_running() == TRUE)
            {
                mqtt_client_force_stop();
                mqtt_client_stop_sub();
            }
        }
        else if (strcmp(argv[1], "sub") == 0)
        {
#ifndef MQTT_MOCK
            if (!ra6w1_network_main_is_wlaninit())
            {
                FSP_RETURN(FSP_ERR_WIFI_FAILED);
            }
#endif                                 /* MQTT_MOCK */
            res_val = (int) mqtt_client_set_sub_topic((char *) argv[2], atoi(argv[3]));
            FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_WIFI_FAILED);
        }
        else if (strcmp(argv[1], "unsub") == 0)
        {
#ifndef MQTT_MOCK
            if (!ra6w1_network_main_is_wlaninit())
            {
                FSP_RETURN(FSP_ERR_WIFI_FAILED);
            }
#endif                                 /* MQTT_MOCK */
            res_val = mqtt_client_unsub_topic((char *) argv[2]);
            FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_WIFI_FAILED);
        }
        else if (strcmp(argv[1], "check") == 0)
        {
            MQTT_DBG_INFO("%s\n", mqtt_client_check_sub_conn() ? "Connected" : "Not Connected");
        }
        else if (strcmp(argv[1], "-m") == 0)
        {
            if (5 == argc)
            {
                const char * topic = argv[3];
                uint16_t     topic_len = strlen(topic);
                const char * payload = argv[2];
                uint32_t     payload_len = 0;
                
                payload_len = strtoul(argv[4], NULL, 10);
                
                if (topic != NULL)
                {
                    if (topic_len <= 0 || topic_len > MQTT_TOPIC_MAX_LEN)
                    {
                        FSP_RETURN(FSP_ERR_INVALID_ARGUMENT);
                    }
                }

                res_val = mqtt_client_send_message_v2(topic, payload, payload_len);
                FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_INVALID_STATE);
            }
            else
            {
                FSP_RETURN(FSP_ERR_INVALID_ARGUMENT);
            }
        }
        else if (strcmp(argv[1], "-l") == 0)
        {
#ifndef MQTT_MOCK
            if (!ra6w1_network_main_is_wlaninit())
            {
                FSP_RETURN(FSP_ERR_WIFI_FAILED);
            }
#endif                                 /* MQTT_MOCK */
            char * buffer = NULL;
            int    ret;

            buffer = _mosquitto_calloc(MQTT_MSG_MAX_LEN + 1, sizeof(char));
            if (NULL == buffer)
            {
                FSP_RETURN(FSP_ERR_OUT_OF_MEMORY);
            }

            ret = rm_mqtt_client_make_message("MQTT Publisher message", buffer, MQTT_MSG_MAX_LEN + 1);

            if (ret > 0)
            {
                res_val = mqtt_client_send_message(NULL, (char *) buffer);
                FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_INVALID_STATE);
            }
            else
            {
                FSP_RETURN(FSP_ERR_INVALID_ARGUMENT);
            }

            vPortFree(buffer);
        }
        else
        {
            rm_mqtt_client_cmd_mqtt_client_help();
        }
    }
    else if (strcmp(argv[0], "mqtt_config") == 0)
    {
        res_val = (int) rm_mqtt_client_mqtt_client_config(p_ctrl, argc, argv);
        FSP_ERROR_RETURN(MOSQ_ERR_SUCCESS == res_val, FSP_ERR_INVALID_STATE);
    }
    else
    {
        FSP_RETURN(FSP_ERR_INVALID_ARGUMENT);
    }

    return FSP_SUCCESS;
}

#ifdef MQTT_NVRAM_ENABLE
static void rm_mqtt_client_id_number_output (char * id_num)
{
    ULONG macmsw, maclsw;
    getMacAddrMswLsw(0, &macmsw, &maclsw);

    sprintf(id_num, "%02lX%02lX", ((maclsw >> 8) & 0x0ff), ((maclsw >> 0) & 0x0ff));
}

static UINT rm_mqtt_client_mqtt_client_add_pub_topic (const char * topic)
{
    if ((NULL == topic) || (strlen(topic) <= 0) || (strlen(topic) > MQTT_TOPIC_MAX_LEN))
    {
        MQTT_DBG_ERR(RED_COLOR "Topic length error (max_len=%d)\n" CLEAR_COLOR, MQTT_TOPIC_MAX_LEN);

        return CC_FAILURE_STRING_LENGTH;
    }

    return (UINT) set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, (char *) topic);
}

static UINT rm_mqtt_client_mqtt_client_add_sub_topic (const char * topic)
{
    UINT status = CC_STATUS_SUCCESS;

    char * nvram_read_topic = NULL;
    char   nvram_tag[25]    = {0, };
    int    sub_topic_num    = 0;

    if ((NULL == topic) || (strlen(topic) <= 0) || (strlen(topic) > MQTT_TOPIC_MAX_LEN))
    {
        MQTT_DBG_ERR(RED_COLOR "Topic length error (max_len=%d)\n" CLEAR_COLOR, MQTT_TOPIC_MAX_LEN);
        status = CC_FAILURE_RANGE_OUT;
        goto finish;
    }

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                 ENV_GROUP_APPCFG,
                                 MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                 &sub_topic_num);
 #endif
    if (-1 != sub_topic_num)
    {
        if (sub_topic_num >= MQTT_MAX_TOPIC)
        {
            MQTT_DBG_ERR(RED_COLOR "Cannot add topics anymore (max=%d)\n" CLEAR_COLOR, MQTT_MAX_TOPIC);
            status = CC_FAILURE_RANGE_OUT;
            goto finish;
        }

        for (int i = 0; i < sub_topic_num; i++)
        {
            memset(nvram_tag, 0x00, 16);
            sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_APPCFG,
                                            nvram_tag,
                                            &nvram_read_topic);
 #endif

            if (nvram_read_topic && (strcmp(nvram_read_topic, topic) == 0))
            {
                MQTT_DBG_ERR(RED_COLOR "Duplicate topic is not allowed.\n" CLEAR_COLOR);
                status = CC_FAILURE_INVALID;
                OS_FREE(nvram_read_topic); /* nvm str free */
                goto finish;
            }

            OS_FREE(nvram_read_topic);     /* nvm str free */
        }

        sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, sub_topic_num);
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        MQTT_NVRAM_CONFIG_SUB_TOPIC,
                                        &nvram_read_topic);
 #endif

        if (nvram_read_topic && strlen(nvram_read_topic))
        {
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_SUB_TOPIC);
 #endif
        }

        OS_FREE(nvram_read_topic);     /* nvm str free */

        sub_topic_num = 0;
        memset(nvram_tag, 0x00, 16);
        sprintf(nvram_tag, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, sub_topic_num);
    }

    sub_topic_num += 1;
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                  ENV_GROUP_APPCFG,
                                  MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                  sub_topic_num);
 #endif
 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, nvram_tag, topic);
 #endif

finish:

    return status;
}

static UINT rm_mqtt_client_mqtt_client_del_sub_topic (const char * topic)
{
    char * checker = NULL;
    int    i, tmp, result = 1;
    int    tmp_result = FALSE;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                 &tmp);
 #endif
    if (-1 != tmp)
    {
        for (i = 0; i < tmp; i++)
        {
            char topics[25] = {0, };
            sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, &checker);
 #endif

            if (checker && (strcmp(topic, checker) == 0))
            {
 #ifdef RM_MAP_PERSISTANT_W
                tmp_result = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics);
 #endif
                if (FSP_SUCCESS != tmp_result)
                {
                    OS_FREE(checker);  /* nvm str free */
                    goto NVRAM_DRV_OPS_ERROR;
                }
                else
                {
                    result = 0;
                    OS_FREE(checker);  /* nvm str free */
                    break;
                }
            }

            OS_FREE(checker);          /* nvm str free */
        }

        if (0 == result)
        {
            /* entry found ... */

            char topics[26] = {0, };

            for (i += 1; i < tmp; i++)
            {
                sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, &checker);
 #endif
                if (checker)
                {
                    sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i - 1);
 #ifdef RM_MAP_PERSISTANT_W
                    tmp_result = RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                                 ENV_GROUP_APPCFG,
                                                                 topics,
                                                                 &checker);
 #endif
                    if (FSP_SUCCESS != tmp_result)
                    {
                        OS_FREE(checker); /* nvm str free */
                        goto NVRAM_DRV_OPS_ERROR;
                    }
                }

                OS_FREE(checker);      /* nvm str free */
                checker = NULL;
            }

            if (tmp > 1)
            {
 #ifdef RM_MAP_PERSISTANT_W
                tmp_result = RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                           ENV_GROUP_APPCFG,
                                                           MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                                           tmp - 1);
 #endif
                if (FSP_SUCCESS != tmp_result)
                {
                    goto NVRAM_DRV_OPS_ERROR;
                }
            }
            else
            {
 #ifdef RM_MAP_PERSISTANT_W
                tmp_result = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                       ENV_GROUP_APPCFG,
                                                       MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM);
 #endif
                if (FSP_SUCCESS != tmp_result)
                {
                    goto NVRAM_DRV_OPS_ERROR;
                }
            }

            sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, tmp - 1);
 #ifdef RM_MAP_PERSISTANT_W
            tmp_result = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics);
 #endif
            if (FSP_SUCCESS != tmp_result)
            {
                goto NVRAM_DRV_OPS_ERROR;
            }
        }
    }
    else
    {
 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                        ENV_GROUP_APPCFG,
                                        MQTT_NVRAM_CONFIG_SUB_TOPIC,
                                        &checker);
 #endif

        if (checker && (strcmp(topic, checker) == 0))
        {
 #ifdef RM_MAP_PERSISTANT_W
            tmp_result = RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(),
                                                   ENV_GROUP_APPCFG,
                                                   MQTT_NVRAM_CONFIG_SUB_TOPIC);
 #endif
            if (FSP_SUCCESS != tmp_result)
            {
                OS_FREE(checker);      /* nvm str free */
                goto NVRAM_DRV_OPS_ERROR;
            }
            else
            {
                result = 0;
            }
        }

        OS_FREE(checker);              /* nvm str free */
        checker = NULL;
    }

    if (result)
    {
        MQTT_DBG_ERR(RED_COLOR "No Topic to remove.\n" CLEAR_COLOR);

        return 100;
    }

    return (UINT) result;

NVRAM_DRV_OPS_ERROR:

    return 101;
}

static void rm_mqtt_client_mqtt_client_delete_sub_topics (void)
{
    int tmp;

 #ifdef RM_MAP_PERSISTANT_W
    RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                 &tmp);
 #endif
    if (-1 != tmp)
    {
        for (int i = 0; i < tmp; i++)
        {
            char topics[25] = {0, };
            sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics);
 #endif
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Erase(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM);
 #endif
    }

    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_TOPIC, NULL);
}

static int rm_mqtt_client_mqtt_client_config_initialize (void)
{
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, NULL);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PORT, MQTT_CONFIG_PORT_DEF);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, NULL);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_QOS, MQTT_CONFIG_QOS_DEF);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS, MQTT_CONFIG_TLS_DEF);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, NULL);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, NULL);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, NULL);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_MSG, NULL);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_WILL_QOS, MQTT_CONFIG_QOS_DEF);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_AUTO, 0);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PING_PERIOD, MQTT_CONFIG_PING_DEF);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_SAMPLE, 0);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_VER311, MQTT_CONFIG_VER311_DEF);
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, NULL);
    set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_VERSION, MQTT_CONFIG_TLS_VERSION_DEF);

    rm_mqtt_client_mqtt_client_delete_sub_topics();

 #if defined(__MQTT_TLS_OPTIONAL_CONFIG__)
    set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, NULL);
    mqtt_client_delete_tls_alpns();
    mqtt_client_delete_cipher_suits();
 #endif                                /* __MQTT_TLS_OPTIONAL_CONFIG__ */
    return 0;
}

static UINT rm_mqtt_client_mqtt_client_set_broker_info (char * broker_ip, int port)
{
    UINT status = CC_FAILURE_NO_VALUE;

    if ((NULL != broker_ip) && (strlen(broker_ip) > 0))
    {
        status = (UINT) set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, broker_ip);

        if (status)
        {
            return status;
        }
    }

    if (port > 0)
    {
        status = (UINT) set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PORT, port);
    }

    return status;
}

#endif                                 /* MQTT_NVRAM_ENABLE */

static UINT rm_mqtt_client_mqtt_client_config (mqtt_client_instance_ctrl_t * p_ctrl, int argc, const char * argv[])
{
#ifdef MQTT_NVRAM_ENABLE
    int status = CC_STATUS_SUCCESS;

    if (1 == argc)
    {
        return MOSQ_ERR_INVAL;
    }

    if ((strcmp(argv[1], "broker") == 0) && (2 == argc))
    {
        status = (int) rm_mqtt_client_mqtt_client_set_broker_info((char *) p_ctrl->p_cfg->p_host_name, 0);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid Broker Address\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "port") == 0) && (2 == argc))
    {
        status = (int) rm_mqtt_client_mqtt_client_set_broker_info(NULL, p_ctrl->p_cfg->mqtt_port);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid Port Number\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "pub_topic") == 0) && (3 == argc))
    {
        status = (int) rm_mqtt_client_mqtt_client_add_pub_topic(argv[2]);
    }
    else if ((strcmp(argv[1], "sub_topic_add") == 0) && (3 == argc))
    {
        status = (int) rm_mqtt_client_mqtt_client_add_sub_topic(argv[2]);
    }
    else if ((strcmp(argv[1], "sub_topic_del") == 0) && (3 == argc))
    {
        status = (int) rm_mqtt_client_mqtt_client_del_sub_topic(argv[2]);
    }
    else if ((strcmp(argv[1], "sub_topics_del") == 0) && (2 == argc))
    {
        rm_mqtt_client_mqtt_client_delete_sub_topics();
    }
    else if ((strcmp(argv[1], "sub_topic") == 0) && (3 < argc))
    {
        if ((atoi(argv[2]) > MQTT_MAX_TOPIC) || (argc - 3 != atoi(argv[2])))
        {
            MQTT_DBG_ERR(RED_COLOR "Invalid Input\n" CLEAR_COLOR);

            return MOSQ_ERR_INVAL;
        }

        for (int i = 0; i < atoi(argv[2]); i++)
        {
            if (strlen(argv[i + 3]) > MQTT_TOPIC_MAX_LEN)
            {
                MQTT_DBG_ERR(RED_COLOR "Topic length error (max_len=%d)\n" CLEAR_COLOR, MQTT_TOPIC_MAX_LEN);

                return MOSQ_ERR_INVAL;
            }
        }

        for (int i = 0; i < atoi(argv[2]) - 1; i++)
        {
            for (int j = 0; j < atoi(argv[2]) - 1; j++)
            {
                if (3 + i + 1 + j > atoi(argv[2]) + 3 - 1)
                {
                    continue;
                }

                if (strcmp(argv[3 + i], argv[3 + i + 1 + j]) == 0)
                {
                    MQTT_DBG_ERR(RED_COLOR "Duplicate topic is not allowed.\n" CLEAR_COLOR);

                    return MOSQ_ERR_INVAL;
                }
            }
        }

        rm_mqtt_client_mqtt_client_delete_sub_topics();

        for (int i = 0; i < atoi(argv[2]); i++)
        {
            char topics[25] = {0, };
            sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, argv[i + 3]);
 #endif
        }

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                      atoi(argv[2]));
 #endif
        status = CC_STATUS_SUCCESS;
    }
    else if ((strcmp(argv[1], "qos") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_QOS, atoi(argv[2]));
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT QoS value (0~2)\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "tls") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS, atoi(argv[2]));
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT TLS value (0|1)\n" CLEAR_COLOR);
        }
    }

 #if defined(__MQTT_TLS_OPTIONAL_CONFIG__)
    else if ((strcmp(argv[1], "tls_alpn") == 0) && (3 < argc))
    {
        if ((atoi(argv[2]) > MQTT_TLS_MAX_ALPN) || (argc - 3 != atoi(argv[2])))
        {
            MQTT_DBG_ERR(RED_COLOR "Invalid Input(max_num=%d)\n" CLEAR_COLOR, MQTT_TLS_MAX_ALPN);

            return MOSQ_ERR_INVAL;
        }

        for (int i = 0; i < atoi(argv[2]); i++)
        {
            if (strlen(argv[i + 3]) > MQTT_TLS_ALPN_MAX_LEN)
            {
                MQTT_DBG_ERR(RED_COLOR "ALPN length error (max_len=%d)\n" CLEAR_COLOR, MQTT_TLS_ALPN_MAX_LEN);

                return MOSQ_ERR_INVAL;
            }
        }

        for (int i = 0; i < atoi(argv[2]) - 1; i++)
        {
            for (int j = 0; j < atoi(argv[2]) - 1; j++)
            {
                if (3 + i + 1 + j > atoi(argv[2]) + 3 - 1)
                {
                    continue;
                }

                if (strcmp(argv[3 + i], argv[3 + i + 1 + j]) == 0)
                {
                    MQTT_DBG_ERR(RED_COLOR "Duplicate ALPN is not allowed.\n" CLEAR_COLOR);

                    return MOSQ_ERR_INVAL;
                }
            }
        }

        mqtt_client_delete_tls_alpns();

        for (int i = 0; i < atoi(argv[2]); i++)
        {
            char items[24] = {0, };
            sprintf(items, "%s%d", MQTT_NVRAM_CONFIG_TLS_ALPN, i);
  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, items, argv[i + 3]);
  #endif
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                      atoi(argv[2]));
  #endif
        status = CC_STATUS_SUCCESS;
    }
    else if ((strcmp(argv[1], "tls_sni") == 0) && (2 == argc))
    {
        status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, (char *) p_ctrl->p_cfg->p_sni_name);
        if (status)
        {
            /* Assume that a length of SNI would be same or like broker name */
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid SNI (max_len=%d)\n" CLEAR_COLOR, MQTT_BROKER_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "tls_cipher") == 0) && (3 < argc))
    {
        char * result_str_pos;
        char * res_str;
        int    num_cipher_suits = 0;
        int    arg_idx, alloc_bytes = 0;

        num_cipher_suits = argc - 2;
        if (num_cipher_suits > MQTT_TLS_MAX_CSUITS)
        {
            MQTT_DBG_ERR(RED_COLOR "Invalid Input(max_num=%d)\n" CLEAR_COLOR, MQTT_TLS_MAX_CSUITS);

            return MOSQ_ERR_INVAL;
        }

        for (int i = 0; i < num_cipher_suits; i++)
        {
            /* Cipher suite value should be hexdecimal that doesn't include the "0x" prefix.
             * And, maximum length of the value should be 4 as an string.
             */
            if (strlen(argv[i + 2]) > 4)
            {
                MQTT_DBG_ERR(RED_COLOR "ALPN length error (max_len=%d)\n" CLEAR_COLOR, 4);

                return MOSQ_ERR_INVAL;
            }
        }

        alloc_bytes = (num_cipher_suits - 1) + (4 * num_cipher_suits);
        res_str     = _mosquitto_malloc((size_t) (alloc_bytes + 1));
        if (NULL == res_str)
        {
            return MOSQ_ERR_NOMEM;
        }

        /* Delete the data stored */
        mqtt_client_delete_cipher_suits();

        result_str_pos = res_str;
        memset(res_str, 0, (size_t) (alloc_bytes + 1));
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                      ENV_GROUP_APPCFG,
                                      MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM,
                                      num_cipher_suits);
  #endif

        arg_idx = 2;
        sprintf(result_str_pos, "%s", argv[arg_idx]);
        result_str_pos += strlen(argv[arg_idx++]);

        for (int i = 0; i < argc - 3; i++, arg_idx++)
        {
            sprintf(result_str_pos, ",%s", argv[arg_idx]);
            result_str_pos += (strlen(argv[arg_idx]) + 1);
        }

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Write_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                         ENV_GROUP_APPCFG,
                                         MQTT_NVRAM_CONFIG_TLS_CSUITS,
                                         res_str);
  #endif
        _mosquitto_free(res_str);
    }
 #endif                                /* __MQTT_TLS_OPTIONAL_CONFIG__ */
    else if ((strcmp(argv[1], "username") == 0) && (2 == argc))
    {
        status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, (char *) p_ctrl->p_cfg->p_mqtt_user_name);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT username (max_len=%d)\n" CLEAR_COLOR, MQTT_USERNAME_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "password") == 0) && (2 == argc))
    {
        status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, (char *) p_ctrl->p_cfg->p_mqtt_password);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT password (max_len=%d)\n" CLEAR_COLOR, MQTT_PASSWORD_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "long_password") == 0) && (2 == argc))
    {
        char * buffer = NULL;
        int    ret;

        buffer = _mosquitto_calloc(MQTT_MSG_MAX_LEN + 1, sizeof(char));
        if (NULL == buffer)
        {
            MQTT_DBG_INFO("[%s] Failed to alloc memory for password\n", __func__);
            status = MOSQ_ERR_NOMEM;
        }

        ret = rm_mqtt_client_make_message("password", buffer, MQTT_PASSWORD_MAX_LEN + 1);

        MQTT_DBG_INFO("\n");
        if (0 < ret)
        {
            status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, buffer);
            if (status)
            {
                MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT password (max_len=%d)\n" CLEAR_COLOR,
                             MQTT_PASSWORD_MAX_LEN);
            }
        }
        else
        {
            MQTT_DBG_INFO("Invalid message input\n");
            status = MOSQ_ERR_INVAL;
        }

        _mosquitto_free(buffer);
    }
    else if ((strcmp(argv[1], "client_id") == 0) && (2 == argc))
    {
        char ret_str[MQTT_CLIENT_ID_MAX_LEN + 1] = {0, };
        get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, ret_str, sizeof(ret_str));
        if (strcmp(p_ctrl->p_cfg->p_client_identifier, ret_str) != 0)
        {
            status =
                set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, (char *) p_ctrl->p_cfg->p_client_identifier);
        }

        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT client_id (max_len=%d)\n" CLEAR_COLOR, MQTT_CLIENT_ID_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "will_topic") == 0) && (2 == argc))
    {
        char ret_str[MQTT_TOPIC_MAX_LEN + 1] = {0, };
        get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, ret_str, sizeof(ret_str));
        if (strcmp(p_ctrl->p_cfg->p_will_topic, ret_str) != 0)
        {
            status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_TOPIC, (char *) p_ctrl->p_cfg->p_will_topic);
        }

        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT Will Topic (max_len=%d)\n" CLEAR_COLOR, MQTT_TOPIC_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "will_message") == 0) && (2 == argc))
    {
        char ret_str[MQTT_WILL_MSG_MAX_LEN + 1] = {0, };
        get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_MSG, ret_str, sizeof(ret_str));
        if (strcmp(p_ctrl->p_cfg->p_will_msg, ret_str) != 0)
        {
            status = set_mqtt_param_str(RRQ61X_CONF_STR_MQTT_WILL_MSG, (char *) p_ctrl->p_cfg->p_will_msg);
        }

        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT Will Message (max_len=%d)\n" CLEAR_COLOR,
                         MQTT_WILL_MSG_MAX_LEN);
        }
    }
    else if ((strcmp(argv[1], "will_qos") == 0) && (2 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_WILL_QOS, p_ctrl->p_cfg->will_qos_level);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT Will QoS value (0~2)\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "auto") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_AUTO, atoi(argv[2]));
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT_AUTO value (0|1)\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "ping_period") == 0) && (2 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PING_PERIOD, p_ctrl->p_cfg->keep_alive_seconds);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid MQTT PING period (0~86400)\n" CLEAR_COLOR);
        }
    }
    else if ((strcmp(argv[1], "ver311") == 0) && (2 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_VER311, p_ctrl->p_cfg->use_mqtt_v311);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid value (0|1)\n" CLEAR_COLOR);
        }
    }
 #if defined(__MQTT_CLEAN_SESSION_MODE_SUPPORT__)
    else if ((strcmp(argv[1], "clean_session") == 0) && (2 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_CLEAN_SESSION, p_ctrl->p_cfg->clean_session);
        if (status)
        {
            MQTT_DBG_ERR(RED_COLOR "Error: Invalid value (0|1)\n" CLEAR_COLOR);
        }
    }
 #endif                                /* __MQTT_CLEAN_SESSION_MODE_SUPPORT__ */
    else if ((strcmp(argv[1], "no_cert_time_chk") == 0) && (3 == argc))
    {
        /* mqtt_config no_cert_time_chk <1|0> */
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_NO_TIME_CHK, atoi(argv[2]));
    }
    else if ((strcmp(argv[1], "reset") == 0) && (2 == argc))
    {
        status = rm_mqtt_client_mqtt_client_config_initialize();
    }
    else if ((strcmp(argv[1], "tls_incoming") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_INCOMING, atoi(argv[2]));
    }
    else if ((strcmp(argv[1], "tls_outgoing") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_OUTGOING, atoi(argv[2]));
    }
    else if ((strcmp(argv[1], "tls_authmode") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE, atoi(argv[2]));
    }
    else if ((strcmp(argv[1], "tls_version") == 0) && (3 == argc))
    {
        status = set_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_VERSION, atoi(argv[2]));
    }
    else if ((strcmp(argv[1], "status") == 0) && (2 == argc))
    {
        char ret_str[MQTT_PASSWORD_MAX_LEN + 1] = {0, };
        int  ret_num     = 0;
        int  tls_version = 0;

        MQTT_DBG_INFO("MQTT Client Information:\n");

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_SUB, &ret_num);
        MQTT_DBG_INFO("  - MQTT Status  : %s\n", ret_num ? "Running" : "Not Running");

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_BROKER_IP, ret_str, sizeof(ret_str)))
        {
            bsp_safe_strcpy(ret_str, "0.0.0.0", sizeof(ret_str));
        }

        MQTT_DBG_INFO("  - Broker IP          : %s\n", ret_str);

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PORT, &ret_num);
        MQTT_DBG_INFO("  - Port               : %d\n", ret_num);

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PUB_TOPIC, ret_str, sizeof(ret_str)))
        {
            ret_str[0] = '\0';
        }

        MQTT_DBG_INFO("  - Pub. Topic         : %s\n", ret_str);

        MQTT_DBG_INFO("  - Sub. Topic         : ");

 #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_SUB_TOPIC_NUM,
                                     &ret_num);
 #endif
        if (-1 == ret_num)
        {
            if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_TOPIC, ret_str, sizeof(ret_str)))
            {
                ret_str[0] = '\0';
            }

            MQTT_DBG_INFO("%s\n", ret_str);
        }
        else
        {
            memset(ret_str, '\0', MQTT_TOPIC_MAX_LEN + 1);

            if (0 == ret_num)
            {
                MQTT_DBG_INFO("\n");
            }

            for (int i = 0; i < ret_num; i++)
            {
                char   topics[25] = {0, };
                char * tmp_str    = NULL;

                sprintf(topics, "%s%d", MQTT_NVRAM_CONFIG_SUB_TOPIC, i);
 #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, topics, &tmp_str);
 #endif
                if (tmp_str)
                {
                    if ((ret_num - 1) != i)
                    {
                        MQTT_DBG_INFO("%s, ", tmp_str);
                    }
                    else
                    {
                        MQTT_DBG_INFO("%s\n", tmp_str);
                    }
                }

                OS_FREE(tmp_str);      /* nvm str free */
                tmp_str = NULL;
            }
        }

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_QOS, &ret_num);
        MQTT_DBG_INFO("  - QoS Level          : %d\n", ret_num);

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS, &ret_num);
        MQTT_DBG_INFO("  - TLS                : %s\n", ret_num ? "Enable" : "Disable");

        if (1 == ret_num)
        {
            get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_VERSION, &ret_num);
            tls_version = ret_num;
            MQTT_DBG_INFO("  - TLS VER            : %s\n",
                          (ret_num == 0) ? "1.2" : ((ret_num == 1) ? "1.3" : "1.2 and 1.3"));
        }

 #if defined(__MQTT_CLEAN_SESSION_MODE_SUPPORT__)
        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_CLEAN_SESSION, &ret_num);
        MQTT_DBG_INFO("  - Clean Session      : %s\n", ret_num ? "Yes" : "No");
 #endif                                /* __MQTT_CLEAN_SESSION_MODE_SUPPORT__ */

 #if defined(__MQTT_TLS_OPTIONAL_CONFIG__)
  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_TLS_ALPN_NUM,
                                     &ret_num);
  #endif
        MQTT_DBG_INFO("  - TLS ALPN           : ");
        if (!((0 == ret_num) || (-1 == ret_num)))
        {
            for (int i = 0; i < ret_num; i++)
            {
                char   items[24] = {0, };
                char * tmp_str   = NULL;

                sprintf(items, "%s%d", MQTT_NVRAM_CONFIG_TLS_ALPN, i);
  #ifdef RM_MAP_PERSISTANT_W
                RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(), ENV_GROUP_APPCFG, items, &tmp_str);
  #endif
                if (tmp_str && strlen(tmp_str))
                {
                    if ((ret_num - 1) != i)
                    {
                        MQTT_DBG_INFO("%s, ", tmp_str);
                    }
                    else
                    {
                        MQTT_DBG_INFO("%s", tmp_str);
                    }
                }

                OS_FREE(tmp_str);      /* nvm str free */
                tmp_str = NULL;
            }
        }
        else
        {
            MQTT_DBG_INFO("(None)");
        }

        MQTT_DBG_INFO("\r\n");

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_TLS_SNI, ret_str, sizeof(ret_str)))
        {
            ret_str[0] = '\0';
        }

        MQTT_DBG_INFO("  - TLS SNI            : %s\r\n", strlen(ret_str) ? ret_str : "(None)");

  #ifdef RM_MAP_PERSISTANT_W
        RM_MAP_PERSISTANT_W_Read_INT(RM_MAP_PERSISTANT_W_get_ctrl(),
                                     ENV_GROUP_APPCFG,
                                     MQTT_NVRAM_CONFIG_TLS_CSUIT_NUM,
                                     &ret_num);
  #endif
        MQTT_DBG_INFO("  - TLS CIPHER SUIT    : ");
        if (!((0 == ret_num) || (-1 == ret_num)))
        {
            char * tmp_str = NULL;

  #ifdef RM_MAP_PERSISTANT_W
            RM_MAP_PERSISTANT_W_Read_STRING(RM_MAP_PERSISTANT_W_get_ctrl(),
                                            ENV_GROUP_APPCFG,
                                            MQTT_NVRAM_CONFIG_TLS_CSUITS,
                                            &tmp_str);
  #endif
            if (tmp_str && strlen(tmp_str))
            {
                MQTT_DBG_INFO("%s", tmp_str);
            }

            OS_FREE(tmp_str);          /* nvm str free */
            tmp_str = NULL;
        }
        else
        {
            MQTT_DBG_INFO("(None)");
        }
        MQTT_DBG_INFO("\r\n");
 #endif                                /* __MQTT_TLS_OPTIONAL_CONFIG__ */
        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_PING_PERIOD, &ret_num);
        MQTT_DBG_INFO("  - Ping Period        : %d\n", ret_num);

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_INCOMING, &ret_num);
        MQTT_DBG_INFO("  - TLS Incoming buf   : %d(bytes)\n", ret_num);

        get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_OUTGOING, &ret_num);
        MQTT_DBG_INFO("  - TLS Outgoing buf   : %d(bytes)\n", ret_num);

        if (1 != tls_version)          /* TLS Auth mode is only meaningful for TLS v1.2 */
        {
            get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_TLS_AUTHMODE, &ret_num);
            MQTT_DBG_INFO("  - TLS Auth mode      : %d\n", ret_num);
        }

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_USERNAME, ret_str, sizeof(ret_str)) == CC_STATUS_SUCCESS)
        {
            MQTT_DBG_INFO("  - User name          : %s\n", ret_str);
        }
        else
        {
            MQTT_DBG_INFO("  - User name          : %s\n", "(None)");
        }

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_PASSWORD, ret_str, sizeof(ret_str)) == CC_STATUS_SUCCESS)
        {
            MQTT_DBG_INFO("  - Password           : %s\n", ret_str);
        }
        else
        {
            MQTT_DBG_INFO("  - Password           : %s\n", "(None)");
        }

        if (get_mqtt_param_str(RRQ61X_CONF_STR_MQTT_SUB_CLIENT_ID, ret_str, sizeof(ret_str)) == CC_STATUS_SUCCESS)
        {
            MQTT_DBG_INFO("  - Client ID          : %s\n", ret_str);
        }
        else
        {
            /* generate default cid if there's no cid stored in NVM */
            char mac_id[5]   = {0, };
            char def_cid[12] = {0, };
            rm_mqtt_client_id_number_output(mac_id);

            sprintf(def_cid, "%s_%s", "ra6w1", mac_id);

            MQTT_DBG_INFO("  - Client ID          : (default: %s)\n", def_cid);
        }

        if (get_mqtt_param_int(RRQ61X_CONF_INT_MQTT_VER311, &ret_num) == CC_STATUS_SUCCESS)
        {
            if (1 == ret_num)
            {
                MQTT_DBG_INFO("  - MQTT VER           : %s\n", MQTT_VER_311);
            }
            else
            {
                MQTT_DBG_INFO("  - MQTT VER           : %s\n", MQTT_VER_31);
            }
        }
        else
        {
            MQTT_DBG_INFO("  - MQTT VER           : %s\n", MQTT_VER_31);
        }

        status = CC_STATUS_SUCCESS;
    }
    else
    {
        return MOSQ_ERR_INVAL;
    }

    if (CC_STATUS_SUCCESS == status)
    {
        return MOSQ_ERR_SUCCESS;
    }
    else
    {
        return MOSQ_ERR_INVAL;
    }

#else                                  /* MQTT_NVRAM_ENABLE */
    return MOSQ_ERR_SUCCESS;
#endif /* MQTT_NVRAM_ENABLE */
}

static void mqtt_publish_cb (int mid)
{
    (void) mid;
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        mqtt_data.p_data      = NULL;
        mqtt_data.p_topic     = NULL;
        mqtt_data.data_length = 0;
        mqtt_context.event    = MQTT_EVENT_PUBLISHED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

static void mqtt_connected_cb (void)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        mqtt_data.p_data      = NULL;
        mqtt_data.p_topic     = NULL;
        mqtt_data.data_length = 0;
        mqtt_context.event    = MQTT_EVENT_CONNECTED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

static void mqtt_disconnected_cb (void)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        mqtt_data.p_data      = NULL;
        mqtt_data.p_topic     = NULL;
        mqtt_data.data_length = 0;
        mqtt_context.event    = MQTT_EVENT_DISCONNECTED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

static void mqtt_message_cb (const char * buf, int len, const char * topic)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        memset(gs_message, 0x00, sizeof(gs_message));
        memset(gs_topic, 0x00, sizeof(gs_topic));
        memcpy(gs_message, buf, (size_t) len);
        memcpy(gs_topic, topic, strlen(topic));

        mqtt_data.p_data      = (uint8_t *) &gs_message;
        mqtt_data.p_topic     = topic;
        mqtt_data.data_length = strlen(gs_message);
        mqtt_context.event    = MQTT_EVENT_MESSAGED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

static void mqtt_subscribe_cb (void)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        mqtt_data.p_data      = NULL;
        mqtt_data.p_topic     = NULL;
        mqtt_data.data_length = 0;
        mqtt_context.event    = MQTT_EVENT_SUBSCRIBED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

static void mqtt_unsubscribe_cb (void)
{
    mqtt_client_callback_args_t    mqtt_data;
    mqtt_client_callback_context_t mqtt_context;

    if ((NULL != gp_ctrl) && (NULL != gp_ctrl->p_cfg) && (NULL != gp_ctrl->p_cfg->p_callback))
    {
        mqtt_data.p_data      = NULL;
        mqtt_data.p_topic     = NULL;
        mqtt_data.data_length = 0;
        mqtt_context.event    = MQTT_EVENT_UNSUBSCRIBED;
        mqtt_data.p_context   = &mqtt_context;
        gp_ctrl->p_cfg->p_callback(&mqtt_data);
    }
}

fsp_err_t rm_mqtt_client_set_atcmd_event_callback (void * const        p_ctrl,
                                                   uint32_t (        * p_callback)(
                                                       void * const    p_ctrl,
                                                       int             index,
                                                       unsigned char * p_in,
                                                       unsigned int    inlen))
{
#if (ATCMD_IF_SUPPORT == 1)
    int ret = 0;

    ret = mqtt_client_set_atcmd_event_callback(p_ctrl, p_callback);

    if (ret)
    {
        return FSP_ERR_WRITE_FAILED;
    }

    return FSP_SUCCESS;
#else
    RA6W1_UNUSED_ARG(p_ctrl);
    RA6W1_UNUSED_ARG(p_callback);
    return FSP_ERR_UNSUPPORTED;
#endif
}
