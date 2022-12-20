 /* Copyright 2022 Bogdan Pilyugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *   File name: MQTT.h
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#ifndef MAIN_INCLUDE_MQTT_H_
#define MAIN_INCLUDE_MQTT_H_

#include "mqtt_client.h"


typedef int mqtt_app_err_t;

#define API_OK                          0       /*!< success (no error) */
#define API_INTERNAL_ERR                1
#define API_WRONG_JSON_ERR              2
#define API_NO_ID_ERR                   3
#define API_ID_OVERSIZE_ERR             4
#define API_NO_API_ERR                  5
#define API_VERSION_ERR                 6
#define API_NO_REQUEST_ERR              7
#define API_UNSUPPORTED_METHOD_ERR      8
#define API_NO_URL_ERR                  9
#define API_URL_OVERSIZE_ERR            10

#define API_URL_NOT_FOUND_ERR           11

#define API_NO_POSTDATA_ERR             12
#define API_POSTDATA_OVERSIZE_ERR       13
#define API_FILE_OVERSIZE_ERR           14
#define API_FILE_EMPTY_ERR              15
#define API_UNKNOWN_ERR                 16

typedef struct
{
    char topic[CONFIG_WEBGUIAPP_MQTT_MAX_TOPIC_LENGTH];
    char *raw_data_ptr;
    int data_length;
}MQTT_DATA_SEND_STRUCT;

/**
 * @brief wrapper around esp_mqtt_client_handle_t with additional info
 *
 */
typedef struct
{
    int mqtt_index;  /// numerical index of mqtt client
    esp_mqtt_client_handle_t mqtt; /// mqtt client handle
    QueueHandle_t mqtt_queue;   /// queue for data sending over current mqtt client
    int wait_delivery_bit;  /// is message delivered before timeout flag
    bool is_connected; /// is client connected flag
    void (*system_event_handler)(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    void (*user_event_handler)(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
} mqtt_client_t;

mqtt_client_t* GetMQTTHandlesPool(int idx);
QueueHandle_t GetMQTTSendQueue(int idx);
void ComposeTopic(char *topic, int idx, char *service_name, char *direct);
void regUserEventHandler(void (*event_handler)(int idx, void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data));
void SystemDataHandler(char *data, uint32_t len, int idx);

#endif /* MAIN_INCLUDE_MQTT_H_ */
