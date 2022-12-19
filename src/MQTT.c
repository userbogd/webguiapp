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
#include "esp_log.h"
#include "Helpers.h"
#include "SystemConfiguration.h"

#include "NetTransport.h"
#include "MQTT.h"

#define MQTT_MESSAGE_BUFER_LENTH 32  //size of mqtt queue
#define MQTT_RECONNECT_CHANGE_ADAPTER   3

#if CONFIG_WEBGUIAPP_MQTT_ENABLE

static SemaphoreHandle_t xSemaphoreMQTTHandle = NULL;
static StaticSemaphore_t xSemaphoreMQTTBuf;
static StaticQueue_t xStaticMQTT1MessagesQueue;
static StaticQueue_t xStaticMQTT2MessagesQueue;
uint8_t MQTT1MessagesQueueStorageArea[MQTT_MESSAGE_BUFER_LENTH * sizeof(MQTT_DATA_SEND_STRUCT)];
uint8_t MQTT2MessagesQueueStorageArea[MQTT_MESSAGE_BUFER_LENTH * sizeof(MQTT_DATA_SEND_STRUCT)];

mqtt_client_t mqtt[CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM] = { 0 };

#define TAG "MQTTApp"

static void mqtt_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void (*UserEventHandler)(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void regUserEventHandler(
        void (*event_handler)(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data))
{
    UserEventHandler = event_handler;
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

static void mqtt_system_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    xSemaphoreTake(xSemaphoreMQTTHandle, pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    mqtt_client_t *ctx = (mqtt_client_t*) event->user_context;

    int msg_id;
    static int MQTTReconnectCounter = 0; //Change network adapter every MQTT_RECONNECT_CHANGE_ADAPTER number attempts
    char topic[CONFIG_WEBGUIAPP_MQTT_MAX_TOPIC_LENGTH]; //TODO need define max topic length
    switch ((esp_mqtt_event_id_t) event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ctx->is_connected = true;
            MQTTReconnectCounter = 0;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED client %d", ctx->mqtt_index);
            ComposeTopic(topic, ctx->mqtt_index, "SYSTEM", "DWLINK");
            msg_id = esp_mqtt_client_subscribe(client, (const char*) topic, 0);
            ESP_LOGI(TAG, "Subscribe to %s", topic);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED client %d", ctx->mqtt_index);
            if (++MQTTReconnectCounter > MQTT_RECONNECT_CHANGE_ADAPTER)
            {
                MQTTReconnectCounter = 0;
                NextDefaultNetIF();
            }
            ctx->is_connected = false;
        break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA, client %d", ctx->mqtt_index);
            //Check if topic is SYSTEM and pass data to handler
            ComposeTopic(topic, ctx->mqtt_index, "SYSTEM", "DWLINK");
            if (!memcmp(topic, event->topic, event->topic_len))
            {
                SystemDataHandler(event->data, event->data_len, ctx->mqtt_index);
                ESP_LOGI(TAG, "Control data handler on client %d", ctx->mqtt_index);
            }
        break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR, client %d", ctx->mqtt_index);
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
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    xSemaphoreGive(xSemaphoreMQTTHandle);
}

static void reconnect_MQTT_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id,
                                   void *event_data)
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
        xQueuePeek(mqtt[idx].mqtt_queue, &DSS, portMAX_DELAY);
        if (mqtt[idx].mqtt)
        {
            esp_mqtt_client_publish(mqtt[idx].mqtt,
                                    (const char*) DSS.topic,
                                    (const char*) DSS.raw_data_ptr,
                                    DSS.data_length,
                                    0, 0);
        }
        else
            ESP_LOGE(TAG, "MQTT client not initialized");

        //Here, if need, can be added repeat transmission after delivery timeout.
        //In this case, follow code must be skipped here and executed on delivery confirm or on exceeded retry attempts
        xQueueReceive(mqtt[idx].mqtt_queue, &DSS, 0);
        free(DSS.raw_data_ptr);

    }
}

static void start_mqtt()
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

    char url[40];
    char tmp[40];

    for (int i = 0; i < CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM; ++i)
    {
        if (GetSysConf()->mqttStation[i].Flags1.bIsGlobalEnabled)
        {
            esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &reconnect_MQTT_handler, &mqtt[i].mqtt);
            esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &reconnect_MQTT_handler, &mqtt[i].mqtt);
            esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, &reconnect_MQTT_handler, &mqtt[i].mqtt);

            strcpy(url, "mqtt://");
            strcat(url, GetSysConf()->mqttStation[i].ServerAddr);
            itoa(GetSysConf()->mqttStation[i].ServerPort, tmp, 10);
            strcat(url, ":");
            strcat(url, tmp);
            mqtt_cfg.uri = url;
            mqtt_cfg.username = GetSysConf()->mqttStation[i].UserName;
            mqtt_cfg.password = GetSysConf()->mqttStation[i].UserPass;
            strcpy(tmp, GetSysConf()->mqttStation[i].ClientID);
            strcat(tmp, "-");
            strcat(tmp, GetSysConf()->ID);
            mqtt_cfg.client_id = tmp;
            mqtt[i].is_connected = false;
            mqtt[i].mqtt_index = i;
            mqtt_cfg.user_context = (void*) &mqtt[i];
            mqtt[i].mqtt = esp_mqtt_client_init(&mqtt_cfg);
            /* The last argument may be used to pass data to the event handler, in this example mqtt_system_event_handler */
            esp_mqtt_client_register_event(mqtt[i].mqtt, ESP_EVENT_ANY_ID, mqtt_system_event_handler, &mqtt[i].mqtt);
            esp_mqtt_client_register_event(mqtt[i].mqtt, ESP_EVENT_ANY_ID, UserEventHandler, &mqtt[i].mqtt);
            esp_mqtt_client_start(mqtt[i].mqtt);
            xTaskCreate(MQTTTaskTransmit, "MQTTTaskTransmit", 1024 * 4, (void*) &mqtt[i].mqtt_index, 3, NULL);
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

    start_mqtt();
}

#endif
