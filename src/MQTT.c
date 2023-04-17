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
 *   File name: MQTT.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */
#include <WebGUIAppMain.h>
#include "esp_log.h"
#include "Helpers.h"
#include "NetTransport.h"
#include "MQTT.h"

#define MQTT_DEBUG_MODE  1

#define MQTT_MESSAGE_BUFER_LENTH 5  //size of mqtt queue
#define MQTT_RECONNECT_CHANGE_ADAPTER  3

#define MQTT_RECONNECT_TIMEOUT 20

#if CONFIG_WEBGUIAPP_MQTT_ENABLE

QueueHandle_t MQTT1MessagesQueueHandle;
QueueHandle_t MQTT2MessagesQueueHandle;

static SemaphoreHandle_t xSemaphoreMQTTHandle = NULL;
static StaticSemaphore_t xSemaphoreMQTTBuf;
static StaticQueue_t xStaticMQTT1MessagesQueue;
static StaticQueue_t xStaticMQTT2MessagesQueue;
uint8_t MQTT1MessagesQueueStorageArea[MQTT_MESSAGE_BUFER_LENTH * sizeof(MQTT_DATA_SEND_STRUCT)];
uint8_t MQTT2MessagesQueueStorageArea[MQTT_MESSAGE_BUFER_LENTH * sizeof(MQTT_DATA_SEND_STRUCT)];

mqtt_client_t mqtt[CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM] = { 0 };

#define TAG "MQTTApp"

static void mqtt_system_event_handler(int idx, void *handler_args, esp_event_base_t base, int32_t event_id,
                                      void *event_data);

static void mqtt1_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt2_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt1_user_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt2_user_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void (*UserEventHandler)(int idx, void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void* UserArg;
void regUserEventHandler(
        void (*event_handler)(int idx, void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data), void* user_arg)
{
    UserEventHandler = event_handler;
    UserArg = user_arg;
}

mqtt_client_t* GetMQTTHandlesPool(int idx)
{
    return &mqtt[idx];
}

QueueHandle_t GetMQTTSendQueue(int idx)
{
    return mqtt[idx].mqtt_queue;
}

bool GetMQTT1Connected(void)
{
    return mqtt[0].is_connected;
}

bool GetMQTT2Connected(void)
{
    return mqtt[1].is_connected;
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

void ComposeTopic(char *topic, int idx, char *service_name, char *direct)
{
    strcpy((char*) topic, GetSysConf()->mqttStation[idx].SystemName);                 // Global system name
    strcat((char*) topic, "/");
    strcat((char*) topic, GetSysConf()->mqttStation[idx].GroupName);                // Global system name
    strcat((char*) topic, "/");
    strcat((char*) topic, GetSysConf()->mqttStation[idx].ClientID);     // Device client name  (for multiclient devices)
    strcat((char*) topic, "-");
    strcat((char*) topic, GetSysConf()->ID);                 //
    strcat((char*) topic, "/");
    strcat((char*) topic, (const char*) service_name);  // Device service name
    strcat((char*) topic, "/");
    strcat((char*) topic, direct);  // Data direction UPLINK or DOWNLINK
}

static void mqtt_system_event_handler(int idx, void *handler_args, esp_event_base_t base, int32_t event_id,
                                      void *event_data)
{
    xSemaphoreTake(xSemaphoreMQTTHandle, pdMS_TO_TICKS(1000));
#if MQTT_DEBUG_MODE > 0
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int )event_id);
#endif
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    int msg_id;
    static int MQTTReconnectCounter = 0; //Change network adapter every MQTT_RECONNECT_CHANGE_ADAPTER number attempts
    char topic[CONFIG_WEBGUIAPP_MQTT_MAX_TOPIC_LENGTH]; //TODO need define max topic length
    switch ((esp_mqtt_event_id_t) event_id)
    {
        case MQTT_EVENT_CONNECTED:
            mqtt[idx].is_connected = true;
            MQTTReconnectCounter = 0;
#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED client %d", idx);
#endif
            ComposeTopic(topic, idx, "SYSTEM", "DWLINK");
            msg_id = esp_mqtt_client_subscribe(client, (const char*) topic, 0);
#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "Subscribe to %s", topic);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
#endif
        break;
        case MQTT_EVENT_DISCONNECTED:

#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED client %d", idx);
#endif
            if (++MQTTReconnectCounter > MQTT_RECONNECT_CHANGE_ADAPTER)
            {
                MQTTReconnectCounter = 0;
                NextDefaultNetIF();
            }
            mqtt[idx].is_connected = false;
        break;
        case MQTT_EVENT_SUBSCRIBED:

#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
#endif
        break;
        case MQTT_EVENT_UNSUBSCRIBED:

#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
#endif
        break;
        case MQTT_EVENT_PUBLISHED:
            #if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
#endif
        break;
        case MQTT_EVENT_DATA:

#if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "MQTT_EVENT_DATA, client %d", idx);
#endif
            //Check if topic is SYSTEM and pass data to handler
            ComposeTopic(topic, idx, "SYSTEM", "DWLINK");
            if (!memcmp(topic, event->topic, event->topic_len))
            {
                SystemDataHandler(event->data, event->data_len, idx);
#if MQTT_DEBUG_MODE > 0
                ESP_LOGI(TAG, "Control data handler on client %d", idx);
#endif
            }
        break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR, client %d", idx);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",
                                     event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
        break;
        default:
            #if MQTT_DEBUG_MODE > 0
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
#endif
        break;
    }
    xSemaphoreGive(xSemaphoreMQTTHandle);
}

static void reconnect_MQTT_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data)
{
    if (event_id == IP_EVENT_ETH_GOT_IP ||
            event_id == IP_EVENT_STA_GOT_IP ||
            event_id == IP_EVENT_PPP_GOT_IP)
    {

        for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
        {
            if (mqtt[i].mqtt && mqtt[i].is_connected)
            {
                esp_mqtt_client_disconnect(mqtt[i].mqtt);
                esp_mqtt_client_reconnect(mqtt[i].mqtt);
            }
        }
    }
}

void MQTTStart(void)
{
    for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
            esp_mqtt_client_reconnect(mqtt[i].mqtt);
    }
}

void MQTTStop(void)
{
    for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
            esp_mqtt_client_disconnect(mqtt[i].mqtt);
    }
}

void MQTTReconnect(void)
{
    for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
        {
            esp_mqtt_client_disconnect(mqtt[i].mqtt);
            esp_mqtt_client_reconnect(mqtt[i].mqtt);
        }
    }
}

void MQTTTaskTransmit(void *pvParameter)
{
    MQTT_DATA_SEND_STRUCT DSS;
    int idx = *(int*) pvParameter;
    while (!mqtt[idx].mqtt_queue)
        vTaskDelay(pdMS_TO_TICKS(300)); //wait for MQTT queue ready
    while (1)
    {
        while (!mqtt[idx].is_connected)
            vTaskDelay(pdMS_TO_TICKS(1000));
        xQueueReceive(mqtt[idx].mqtt_queue, &DSS, portMAX_DELAY);
        if (mqtt[idx].mqtt)
        {
#if MQTT_DEBUG_MODE > 1
            ESP_LOGI(TAG, "MQTT client %d data send:%.*s", idx, DSS.data_length, DSS.raw_data_ptr);
#endif
            esp_mqtt_client_publish(mqtt[idx].mqtt,
                                    (const char*) DSS.topic,
                                    (const char*) DSS.raw_data_ptr,
                                    DSS.data_length,
                                    0, 0);
        }
        else
            ESP_LOGE(TAG, "MQTT client not initialized");
        free(DSS.raw_data_ptr);
    }
}

static void start_mqtt()
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

    char url[CONFIG_WEBGUIAPP_MQTT_MAX_TOPIC_LENGTH + 14];
    char tmp[128];

    for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
    {
        if (GetSysConf()->mqttStation[i].Flags1.bIsGlobalEnabled)
        {
            strcpy(url, "mqtt://");
            strcat(url, GetSysConf()->mqttStation[i].ServerAddr);
            itoa(GetSysConf()->mqttStation[i].ServerPort, tmp, 10);
            strcat(url, ":");
            strcat(url, tmp);
#if ESP_IDF_VERSION_MAJOR >= 5
            mqtt_cfg.broker.address.uri = url;
            mqtt_cfg.credentials.username = GetSysConf()->mqttStation[i].UserName;
            mqtt_cfg.credentials.authentication.password = GetSysConf()->mqttStation[i].UserPass;
#else
            mqtt_cfg.uri = url;
            mqtt_cfg.username = GetSysConf()->mqttStation[i].UserName;
            mqtt_cfg.password = GetSysConf()->mqttStation[i].UserPass;
#endif
            strcpy(tmp, GetSysConf()->mqttStation[i].ClientID);
            strcat(tmp, "-");
            strcat(tmp, GetSysConf()->ID);
#if ESP_IDF_VERSION_MAJOR >= 5
            mqtt_cfg.credentials.client_id = tmp;
            mqtt_cfg.network.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT * 1000;
#else
            mqtt_cfg.client_id = tmp;
            mqtt_cfg.reconnect_timeout_ms = MQTT_RECONNECT_TIMEOUT * 1000;
#endif
            mqtt[i].is_connected = false;
            mqtt[i].mqtt_index = i;
            //mqtt_cfg.user_context = (void*) &mqtt[i];
            mqtt[i].mqtt = esp_mqtt_client_init(&mqtt_cfg);
            /* The last argument may be used to pass data to the event handler, in this example mqtt_system_event_handler */
            esp_mqtt_client_register_event(mqtt[i].mqtt, ESP_EVENT_ANY_ID, mqtt[i].system_event_handler, &mqtt[i]);
            esp_mqtt_client_register_event(mqtt[i].mqtt, ESP_EVENT_ANY_ID, mqtt[i].user_event_handler, &mqtt[i]);
            esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &reconnect_MQTT_handler, &mqtt[i].mqtt);
            esp_mqtt_client_start(mqtt[i].mqtt);
            xTaskCreate(MQTTTaskTransmit, "MQTTTaskTransmit", 1024 * 2, (void*) &mqtt[i].mqtt_index, 3, NULL);
        }
    }
}

void MQTTRun(void)
{
    xSemaphoreMQTTHandle = xSemaphoreCreateBinaryStatic(&xSemaphoreMQTTBuf);
    xSemaphoreGive(xSemaphoreMQTTHandle);

    MQTT1MessagesQueueHandle = NULL;
    MQTT2MessagesQueueHandle = NULL;
    if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled)
        MQTT1MessagesQueueHandle = xQueueCreateStatic(MQTT_MESSAGE_BUFER_LENTH,
                                                      sizeof(MQTT_DATA_SEND_STRUCT),
                                                      MQTT1MessagesQueueStorageArea,
                                                      &xStaticMQTT1MessagesQueue);
    mqtt[0].mqtt_queue = MQTT1MessagesQueueHandle;

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
    if (GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
        MQTT2MessagesQueueHandle = xQueueCreateStatic(MQTT_MESSAGE_BUFER_LENTH,
                                                      sizeof(MQTT_DATA_SEND_STRUCT),
                                                      MQTT2MessagesQueueStorageArea,
                                                      &xStaticMQTT2MessagesQueue);
    mqtt[1].mqtt_queue = MQTT2MessagesQueueHandle;
#endif

    mqtt[0].system_event_handler = mqtt1_system_event_handler;
    mqtt[0].user_event_handler = mqtt1_user_event_handler;
    mqtt[0].user_arg = UserArg;
    mqtt[1].system_event_handler = mqtt2_system_event_handler;
    mqtt[1].user_event_handler = mqtt2_user_event_handler;
    mqtt[1].user_arg = UserArg;
    start_mqtt();
}

static void mqtt1_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    mqtt_system_event_handler(0, handler_args, base, event_id, event_data);
}
static void mqtt2_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    mqtt_system_event_handler(1, handler_args, base, event_id, event_data);
}
static void mqtt1_user_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    UserEventHandler(0, handler_args, base, event_id, event_data);
}
static void mqtt2_user_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    UserEventHandler(1, handler_args, base, event_id, event_data);
}



#endif
