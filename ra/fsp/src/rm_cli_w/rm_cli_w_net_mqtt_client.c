/**
 ****************************************************************************************
 *
 * @file rm_cli_w_net_mqtt_client.c
 *
 * @brief mqtt_client cli implementation
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
#include "custom_config_sdk.h"

#if defined (__SUPPORT_MQTT__)

#include "rm_cli_w_net_mqtt_client.h"
#include "mqtt_client.h"
#include "net_network_main.h"

static void cmd_mqtt_client_help(void)
{
	printf("- mqtt_client\n\n");
	printf("  Usage : mqtt_client [option]\n");
	printf("    option\n");
	printf("    <start>\t\t    : start mqtt_client\n");
	printf("    <stop>\t\t    : stop mqtt_client\n");
	printf("    <check>\t\t    : shows mqtt_client connection status\n");
	printf("    <unsub> <topic>\t    : unsubscribe from the topic \n");
	printf("    <-m> <msg> [<topic>]    : publish <msg> w/ <topic> if specified \n");
	printf("    <-l>\t\t    : publish large <msg>\n");
}

static char* c0_cmd[] = {"mqtt_config", "clean_session", "0"};
static char* c1_cmd[] = {"mqtt_config", "clean_session", "1"};

bool cmd_mqtt_client(int argc, char *argv[])
{
	int res_val;
	printf("\r\n");
	
	if (strcmp(argv[0], "mqtt_client") == 0 || strcmp(argv[0], "mqtt_sub") == 0) {		
		if (strcmp(argv[1], "start") == 0) {
            if (!ra6w1_network_main_is_wlaninit()) {
                printf("Wi-Fi is not initialized.\n");
                return pdTRUE;
            }

			if (argc == 3) {
				if (strcmp(argv[2], "c0") == 0) {
					mqtt_client_config(3, c0_cmd);
				} else if (strcmp(argv[2], "c1") == 0) {
					mqtt_client_config(3, c1_cmd);
				}
			}

			if (mqtt_client_is_running() == TRUE) {
				mqtt_client_force_stop();
				mqtt_client_stop_sub();
			}
			
			mqtt_client_start_sub();
		} else if (strcmp(argv[1], "stop") == 0) {
            if (!ra6w1_network_main_is_wlaninit()) {
                printf("Wi-Fi is not initialized.\n");
                return pdTRUE;
            }

			if (mqtt_client_is_running() == TRUE) {
				mqtt_client_force_stop();
				mqtt_client_stop_sub();
			} else {
				MQTT_DBG_PRINT("no mqtt_client to stop\n");
			}
		} else if (strcmp(argv[1], "sub") == 0) {
            if (!ra6w1_network_main_is_wlaninit()) {
                printf("Wi-Fi is not initialized.\n");
                return pdTRUE;
            }

			res_val = mqtt_client_set_sub_topic((char *)argv[2], atoi(argv[3]));

			if (res_val == MOSQ_ERR_SUCCESS) {
				MQTT_DBG_PRINT("Subscribing success !\n");
			} else {
				MQTT_DBG_ERR("err!, check ret=%d\n", res_val);
			}
		} else if (strcmp(argv[1], "unsub") == 0) {
            if (!ra6w1_network_main_is_wlaninit()) {
                printf("Wi-Fi is not initialized.\n");
                return pdTRUE;
            }

			res_val = mqtt_client_unsub_topic(argv[2]);

			if (res_val == MOSQ_ERR_SUCCESS) {
				MQTT_DBG_PRINT("Unsubscribing success !\n");
			} else if (res_val == MOSQ_ERR_NO_CONN) {
				MQTT_DBG_PRINT("mqtt_client should be running\n");
			} else {
				MQTT_DBG_ERR("err!, check ret=%d\n", res_val);
			}			
		} else if (strcmp(argv[1], "check") == 0) {
			printf("%s\n", mqtt_client_check_sub_conn() ? "Connected" : "Not Connected");
		} else if (strcmp(argv[1], "-m") == 0) {
			if (argc == 4) {
				if (strlen(argv[3]) <= 0 || strlen(argv[3]) > MQTT_TOPIC_MAX_LEN) {
					printf(RED_COLOR "Topic length error (max_len=%d)\r\n" CLEAR_COLOR, MQTT_TOPIC_MAX_LEN);
				} else {
					res_val = mqtt_client_send_message(argv[3], argv[2]);
					if (res_val != 0) printf("mqtt msg send failed (%d)\n", res_val);
				}			
			} else if (argc == 3) {
				res_val = mqtt_client_send_message(NULL, argv[2]);
				if (res_val != 0) printf("mqtt msg send failed (%d)\n", res_val);
			} else {
				printf("Invalid message input\n");
			}
		} else if (strcmp(argv[1], "-l") == 0) {
            if (!ra6w1_network_main_is_wlaninit()) {
                printf("Wi-Fi is not initialized.\n");
                return pdTRUE;
            }

			char *buffer = NULL;
			int ret;

			extern int make_message(char *title, char *buf, size_t buflen);
			extern void *_mosquitto_calloc(size_t nmemb, size_t size);

			buffer = _mosquitto_calloc(MQTT_MSG_MAX_LEN + 1, sizeof(char));
			if (buffer == NULL) {
				printf("[%s] Failed to alloc memory to write certificate\n", __func__);
				return pdTRUE;
			}

			ret = make_message("MQTT Publisher message", buffer, MQTT_MSG_MAX_LEN + 1);

			printf("\n");

			if (ret > 0) {
				res_val = mqtt_client_send_message(NULL, buffer);
				if (res_val != 0) printf("mqtt msg send failed (%d)\n", res_val);
			} else {
				printf("Invalid message input\n");
			}

			vPortFree(buffer);
		} else {
			cmd_mqtt_client_help();
		}
	} else if (strcmp(argv[0], "mqtt_config") == 0) {
		mqtt_client_config(argc, argv);
	} else {
		printf("Invalid command\n");
	}

    return pdTRUE;
}
#endif /* __SUPPORT_MQTT__ */
