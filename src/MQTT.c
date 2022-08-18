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
#include "jWrite.h"
#include "jRead.h"
#include "NetTransport.h"
#include "HTTPServer.h"
#include "romfs.h"
#include "MQTT.h"

#define CH_MESSAGE_BUFER_LENTH 32  //size of mqtt queue

#define MESSAGE_LENGTH 32          //base message length, mainly depended by radio requirements
#define MAX_JSON_MESSAGE 256       //max size of mqtt message to publish
#define MAX_FILE_PUBLISH    4096   //bufer for mqtt data publish
#define MAX_DYNVAR_LENTH  64
#define MAX_FILENAME_LENTH 15
#define MAX_ERROR_MESSAGE 32
#define MAX_ERROR_JSON  256
#define MAX_MESSAGE_ID 15

#define MAX_POST_DATA_LENTH 512
#define MQTT_RECONNECT_CHANGE_ADAPTER   3

#if CONFIG_WEBGUIAPP_MQTT_ENABLE

static SemaphoreHandle_t xSemaphoreMQTTHandle = NULL;
static StaticSemaphore_t xSemaphoreMQTTBuf;
static StaticQueue_t xStaticMQTT1MessagesQueue;
static StaticQueue_t xStaticMQTT2MessagesQueue;
uint8_t MQTT1MessagesQueueStorageArea[CH_MESSAGE_BUFER_LENTH * sizeof(DATA_SEND_STRUCT)];
uint8_t MQTT2MessagesQueueStorageArea[CH_MESSAGE_BUFER_LENTH * sizeof(DATA_SEND_STRUCT)];

mqtt_client_t mqtt[CONFIG_MQTT_CLIENTS_NUM] = { 0 };



const char apiver[] = "2.0";
const char tagGet[] = "GET";
const char tagPost[] = "POST";

#define TAG "MQTTApp"

const char* mqtt_app_err_descr[] = {
"Operation OK",
"Internal error",
"Wrong json format",
"Key 'idmess' not found",
"Key 'idmess' value too long",
"Key 'api' not found",
"API version not supported",
"Key 'request' not found",
"Unsupported HTTP method",
"Key 'url' not found",
"Key 'url' value too long",
"URL not found",
"Key 'postdata' not found",
"Key 'postdata' too long",
"File size too big",
"File is empty",
"Unknown error"
};

const char* mqtt_app_err_breef[] = {
"OK",
"INTERNAL_ERR",
"WRONG_JSON_ERR",
"NO_ID_ERR",
"ID_OVERSIZE_ERR",
"NO_API_ERR",
"VERSION_ERR",
"NO_REQUEST_ERR",
"UNSUPPORTED_METHOD_ERR",
"NO_URL_ERR",
"URL_OVERSIZE_ERR",
"URL_NOT_FOUND_ERR",
"NO_POSTDATA_ERR",
"POSTDATA_OVERSIZE_ERR",
"FILE_OVERSIZE_ERR",
"FILE_EMPTY_ERR",
"UNKNOWN_ERR"
};

static void ControlDataHandler(char *data, uint32_t len, int idx);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

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

static const char topic_tx[] = "/UPLINK/";  //subtopic for transmit
static const char topic_rx[] = "/DOWNLINK/";  //subtopic to receive

static void ComposeTopic(char *topic, char *system_name, char* direct, char *client_name, char *service_name)
{
    char tmp[4];
    char dev_rom_id[8];
    GetChipId((uint8_t*) tmp);
    BytesToStr((unsigned char*) tmp, (unsigned char*) dev_rom_id, 4);
    strcpy((char*) topic, system_name);    // Global system name
    strcat((char*) topic, "/");
    strcpy((char*) topic, direct);    // Data direction UPLINK or DOWNLINK
    strcat((char*) topic, "/");
    strcat((char*) topic, (const char*) dev_rom_id);   // Unique device ID (based on ROM chip id)
    strcat((char*) topic, "/");
    strcat((char*) topic, client_name);               // Device client name  (for multiclient devices)
    strcat((char*) topic, "/");
    strcat((char*) topic, (const char*) service_name); // Device service name
}


static void ComposeTopicControl(char *topic, char *roottopic, char *ident, uint8_t dir)
{
    char id[4];
    char id2[8];
    strcpy((char*) topic, roottopic);
    if (dir == 0)
        strcat((char*) topic, topic_rx);
    else
        strcat((char*) topic, topic_tx);
    GetChipId((uint8_t*) id);
    BytesToStr((unsigned char*) id, (unsigned char*) id2, 4);
    strcat((char*) topic, (const char*) id2);
    strcat((char*) topic, "/");
    strcat((char*) topic, ident);
    strcat((char*) topic, (const char*) "/CONTROL");
}

static void ComposeTopicScreen(char *topic, char *roottopic, char *ident, uint8_t dir)
{
    char id[4];
    char id2[8];
    strcpy((char*) topic, roottopic);
    if (dir == 0)
        strcat((char*) topic, topic_rx);
    else
        strcat((char*) topic, topic_tx);
    GetChipId((uint8_t*) id);
    BytesToStr((unsigned char*) id, (unsigned char*) id2, 4);
    strcat((char*) topic, (const char*) id2);
    strcat((char*) topic, "/");
    strcat((char*) topic, ident);
    strcat((char*) topic, (const char*) "/SCREEN");
}

typedef enum
{
    UNSUPPORTED = 0,
    GET,
    POST
} mqtt_api_request_t;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    xSemaphoreTake(xSemaphoreMQTTHandle, pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    mqtt_client_t *ctx = (mqtt_client_t*) event->user_context;

    int msg_id;
    static int MQTTReconnectCounter = 0; //Change network adapter every MQTT_RECONNECT_CHANGE_ADAPTER number attempts
    switch ((esp_mqtt_event_id_t) event_id)
    {
        case MQTT_EVENT_CONNECTED:
            ctx->is_connected = true;
            MQTTReconnectCounter = 0;
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED client %d", ctx->mqtt_index);
            char sub[64];

            ComposeTopicControl(sub, GetSysConf()->mqttStation[ctx->mqtt_index].RootTopic,
                                GetSysConf()->mqttStation[ctx->mqtt_index].ClientID,
                                0);
            msg_id = esp_mqtt_client_subscribe(client, (const char*) sub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);



            ComposeTopicScreen(sub, GetSysConf()->mqttStation[ctx->mqtt_index].RootTopic,
                                GetSysConf()->mqttStation[ctx->mqtt_index].ClientID,
                                0);
            msg_id = esp_mqtt_client_subscribe(client, (const char*) sub, 0);
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
            char topic[64];
            //Check if topic is CONTROL and pass data to handler
            ComposeTopicControl(topic, GetSysConf()->mqttStation[ctx->mqtt_index].RootTopic,
                                GetSysConf()->mqttStation[ctx->mqtt_index].ClientID,
                                0);
            if (!memcmp(topic, event->topic, event->topic_len))
            {
                ControlDataHandler(event->data, event->data_len, ctx->mqtt_index);
                ESP_LOGI(TAG, "Control data handler on client %d", ctx->mqtt_index);

            }
            //end of CONTROL

            //Check if topic is SCREEN and pass data to handler
            ComposeTopicScreen(topic, GetSysConf()->mqttStation[ctx->mqtt_index].RootTopic,
                               GetSysConf()->mqttStation[ctx->mqtt_index].ClientID,
                               0);
            if (!memcmp(topic, event->topic, event->topic_len))
            {
              //  ScreenDataHandler(event->data, event->data_len, ctx->mqtt_index);
                ESP_LOGI(TAG, "Screen data handler on client %d", ctx->mqtt_index);
            }

            //end of SCREEN

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
    for (int i = 0; i < CONFIG_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
        {
            esp_mqtt_client_disconnect(mqtt[i].mqtt);
            esp_mqtt_client_reconnect(mqtt[i].mqtt);
        }
    }
}

static mqtt_app_err_t ResponceWithError(int idx,
                                        espfs_file_t *file,
                                        char *id,
                                        char *url,
                                        mqtt_app_err_t api_err)
{
    espfs_fclose(file);
    char JSONErrorMess[MAX_ERROR_JSON];
    jwOpen(JSONErrorMess, MAX_ERROR_JSON, JW_OBJECT, JW_PRETTY);
    if (id[0])
        jwObj_string("messid", (char*) id);
    else
        jwObj_string("messid", "ERROR");
    jwObj_string("api", (char*) apiver);
    jwObj_string("responce", (char*) mqtt_app_err_breef[api_err]);
    jwObj_string("descript", (char*) mqtt_app_err_descr[api_err]);
    if (url[0])
        jwObj_string("url", (char*) url);
    else
        jwObj_string("url", "ERROR");
    jwEnd();
    jwClose();
    char *buf = (char*) malloc(strlen(JSONErrorMess) + 1);
    if (buf)
    {
        memcpy(buf, JSONErrorMess, strlen(JSONErrorMess));
        DATA_SEND_STRUCT DSS;
        DSS.dt = PUBLISH_CONTROL_DATA;
        DSS.raw_data_ptr = buf;
        DSS.data_lenth = strlen(JSONErrorMess);
        if (xQueueSend(mqtt[idx].mqtt_queue, &DSS, pdMS_TO_TICKS(1000)) == pdPASS)
            return API_OK;
        else
        {
            free(buf);
            return API_INTERNAL_ERR;
        }
    }
    else
    {  // ERR internal error on publish error
        return API_INTERNAL_ERR;
    }
}

static mqtt_app_err_t ResponceWithFile(int idx, espfs_file_t *file,
                                       char *id,
                                       char *url)
{
    struct espfs_stat_t stat;
    char *filebuf = NULL;
    char *outbuf = NULL;
    uint32_t fileLenth, filePtr, readBytes;
    espfs_fstat(file, &stat);
    fileLenth = stat.size;
    mqtt_app_err_t api_err = API_UNKNOWN_ERR;
    if (fileLenth > MAX_FILE_PUBLISH)
    {
        ESP_LOGE(TAG, "File is too big");
        api_err = API_FILE_OVERSIZE_ERR;
        goto file_send_err;
    }
    outbuf = (char*) malloc(MAX_FILE_PUBLISH + 256);
    filebuf = (char*) malloc(fileLenth);

    if (!outbuf || !filebuf)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        api_err = API_INTERNAL_ERR;
        goto file_send_err;
    }

    if (espfs_fread(file, filebuf, fileLenth) == 0)
    {
        ESP_LOGE(TAG, "Read no bytes from file");
        api_err = API_FILE_EMPTY_ERR;
        goto file_send_err;
    }
    espfs_fclose(file);

    jwOpen(outbuf, MAX_FILE_PUBLISH, JW_OBJECT, JW_PRETTY);
    jwObj_string("messid", (char*) id);
    jwObj_string("api", (char*) apiver);
    jwObj_string("responce", (char*) mqtt_app_err_breef[API_OK]);
    jwObj_string("descript", (char*) mqtt_app_err_descr[API_OK]);
    jwObj_string("url", (char*) url);
    jwObj_string("body", "bodydata");
    jwEnd();
    jwClose();

    char *fdata = memmem(outbuf, MAX_FILE_PUBLISH, "\"bodydata\"", strlen("\"bodydata\""));
    filePtr = 0;
    readBytes = 0;
    while (filePtr < fileLenth && readBytes < (MAX_FILE_PUBLISH - MAX_DYNVAR_LENTH))
    {
        if (filebuf[filePtr] == '~')
        {
            int k = 0;
            char DynVarName[16];
            while (filePtr < fileLenth && k < 16 && (filebuf[++filePtr] != '~'))
                DynVarName[k++] = filebuf[filePtr];
            if (filebuf[filePtr] == '~')
            {  //got valid dynamic variable name
                DynVarName[k] = 0x00;
                readBytes += HTTPPrint(NULL, &fdata[readBytes], DynVarName);
                filePtr++;
            }
        }
        else
            fdata[readBytes++] = filebuf[filePtr++];
    }
    const char tail[] = "}";
    strcat((fdata + readBytes), tail);
    free(filebuf);
    DATA_SEND_STRUCT DSS;
    DSS.dt = PUBLISH_CONTROL_DATA;
    DSS.raw_data_ptr = outbuf;
    DSS.data_lenth = (fdata - outbuf) + readBytes + strlen(tail);
    if (xQueueSend(mqtt[idx].mqtt_queue, &DSS, pdMS_TO_TICKS(1000)) == pdPASS)
        return API_OK;
    else
    {
        ESP_LOGE(TAG, "Failed to write mqtt queue");
        api_err = API_INTERNAL_ERR;
        goto file_send_err;
    }
file_send_err:
    free(outbuf);
    free(filebuf);
    return api_err;
}

static void ControlDataHandler(char *data, uint32_t len, int idx)
{
    struct jReadElement result;
    char URL[MAX_FILENAME_LENTH + 1];
    char ID[MAX_MESSAGE_ID + 1];
    char POST_DATA[MAX_POST_DATA_LENTH + 1];
    mqtt_api_request_t req = UNSUPPORTED;
    mqtt_app_err_t api_err = API_UNKNOWN_ERR;
    ID[0] = 0x00;
    URL[0] = 0x00;
    espfs_file_t *file = NULL;

    jRead(data, "", &result);
    if (result.dataType == JREAD_OBJECT)
    {
        /*Checking and get ID message*/
        jRead(data, "{'messid'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > MAX_MESSAGE_ID)
            {
                api_err = API_ID_OVERSIZE_ERR;
                goto api_json_err;
            }
            memcpy(ID, result.pValue, result.bytelen);
            ID[result.bytelen] = 0x00;
        }
        else
        {
            api_err = API_NO_ID_ERR;
            goto api_json_err;
        }

        /*Checking and get API version*/
        jRead(data, "{'api'", &result);
        if (result.elements == 1)
        {
            if (memcmp(apiver, result.pValue, result.bytelen))
            {
                api_err = API_VERSION_ERR;
                goto api_json_err;
            }
        }
        else
        {
            api_err = API_NO_API_ERR;
            goto api_json_err;
        }

        /*Checking and get request type POST, GET or UNSUPPORTED*/
        jRead(data, "{'request'", &result);
        if (result.elements == 1)
        {
            if (!memcmp(tagGet, result.pValue, result.bytelen))
                req = GET;
            else if (!memcmp(tagPost, result.pValue, result.bytelen))
                req = POST;
        }
        else
        {
            api_err = API_NO_REQUEST_ERR;
            goto api_json_err;
        }

        /*Checking and get url*/
        jRead(data, "{'url'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > MAX_FILENAME_LENTH)
            {
                api_err = API_URL_OVERSIZE_ERR;
                goto api_json_err;
            }
            memcpy(URL, result.pValue, result.bytelen);
            URL[result.bytelen] = 0x00;
            file = espfs_fopen(fs, URL);
            if (!file)
            {
                api_err = API_URL_NOT_FOUND_ERR;
                goto api_json_err;
            }

        }
        else
        {
            api_err = API_NO_URL_ERR;
            goto api_json_err;
        }

        if (req == POST)
        {
            /*Checking and get POST DATA*/
            jRead(data, "{'postdata'", &result);
            if (result.elements == 1)
            {
                if (result.bytelen > MAX_POST_DATA_LENTH)
                {
                    api_err = API_POSTDATA_OVERSIZE_ERR;
                    goto api_json_err;
                }
                memcpy(POST_DATA, result.pValue, result.bytelen);
                POST_DATA[result.bytelen] = 0x00;
            }
            else
            {
                api_err = API_NO_POSTDATA_ERR;
                goto api_json_err;
            }

            HTTPPostApp(NULL, URL, POST_DATA);

            jRead(data, "{'reload'", &result);
            if (result.elements == 1 && !memcmp("true", result.pValue, result.bytelen))
            {
                if (ResponceWithFile(idx, file, ID, URL) == API_OK)
                    return;
                else
                    goto api_json_err;
            }
            else
            {
                if (ResponceWithError(idx, file, ID, URL, API_OK) != API_OK)
                    ESP_LOGE(TAG, "Failed to allocate memory for file MQTT message");
            }
            return;
        }
        else if (req == GET)
        {
            //Here GET handler, send file wrapped into json
            if (ResponceWithFile(idx, file, ID, URL) == API_OK)
                return;
            else
                goto api_json_err;
        }
        else
        {
            api_err = API_UNSUPPORTED_METHOD_ERR;
            goto api_json_err;
        }
    }
    else
    {  //ERR no json format
        api_err = API_WRONG_JSON_ERR;
        goto api_json_err;
    }

api_json_err:
    ESP_LOGE(TAG, "ERROR:%s:%s", mqtt_app_err_breef[api_err], mqtt_app_err_descr[api_err]);
    if (ResponceWithError(idx, file, ID, URL, api_err) != API_OK)
        ESP_LOGE(TAG, "Failed to allocate memory for file MQTT message");
}

void MQTTStart(void)
{
    for (int i = 0; i < CONFIG_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
            esp_mqtt_client_reconnect(mqtt[i].mqtt);
    }
}

void MQTTStop(void)
{
    for (int i = 0; i < CONFIG_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
            esp_mqtt_client_disconnect(mqtt[i].mqtt);
    }
}

void MQTTReconnect(void)
{
    for (int i = 0; i < CONFIG_MQTT_CLIENTS_NUM; ++i)
    {
        if (mqtt[i].mqtt)
        {
            esp_mqtt_client_disconnect(mqtt[i].mqtt);
            esp_mqtt_client_reconnect(mqtt[i].mqtt);
        }
    }
}

static void MQTTPublish(mqtt_client_t *mqtt, DATA_SEND_STRUCT *DSS)
{
    char top[64];
    switch (DSS->dt)
    {

        case PUBLISH_CONTROL_DATA:
            ComposeTopicControl(top, GetSysConf()->mqttStation[mqtt->mqtt_index].RootTopic,
                                GetSysConf()->mqttStation[mqtt->mqtt_index].ClientID,
                                1);
            if (mqtt)
            {
                esp_mqtt_client_publish(mqtt->mqtt, (const char*) top, (const char*) DSS->raw_data_ptr,
                                        DSS->data_lenth,
                                        0,
                                        0);

                ESP_LOGI(TAG, "TOPIC=%.*s", strlen(top), top);
            }
            else
                ESP_LOGE(TAG, "MQTT client not initialized");

        break;


        case PUBLISH_SCREEN_DATA:
            ComposeTopicScreen(top, GetSysConf()->mqttStation[mqtt->mqtt_index].RootTopic,
                                GetSysConf()->mqttStation[mqtt->mqtt_index].ClientID,
                                1);
            if (mqtt)
            {
                esp_mqtt_client_publish(mqtt->mqtt, (const char*) top, (const char*) DSS->raw_data_ptr,
                                        DSS->data_lenth,
                                        0,
                                        0);

                ESP_LOGI(TAG, "TOPIC=%.*s", strlen(top), top);
            }
            else
                ESP_LOGE(TAG, "MQTT client not initialized");

        break;

    }
}

void MQTTTaskTransmit(void *pvParameter)
{
    DATA_SEND_STRUCT DSS;
    int idx = *(int*) pvParameter;
    while (!mqtt[idx].mqtt_queue)
        vTaskDelay(pdMS_TO_TICKS(300)); //wait for MQTT queue ready
    while (1)
    {
        while (!mqtt[idx].is_connected)
            vTaskDelay(pdMS_TO_TICKS(1000));
        xQueuePeek(mqtt[idx].mqtt_queue, &DSS, portMAX_DELAY);
        MQTTPublish(&mqtt[idx], &DSS);
        switch (DSS.dt)
        {
            case PUBLISH_SCREEN_DATA:
                xQueueReceive(mqtt[idx].mqtt_queue, &DSS, 0);
                free(DSS.raw_data_ptr);
            break;

            case PUBLISH_CONTROL_DATA:
                xQueueReceive(mqtt[idx].mqtt_queue, &DSS, 0);
                free(DSS.raw_data_ptr);
            break;
        }
    }
}

static void start_mqtt()
{
    esp_mqtt_client_config_t mqtt_cfg = { 0 };

    char url[40];
    char tmp[40];

    for (int i = 0; i < CONFIG_MQTT_CLIENTS_NUM; ++i)
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
            strcat(tmp, "_");
            int len = strlen(tmp);
            BytesToStr((unsigned char*) &GetSysConf()->imei, (unsigned char*) &tmp[len], 4);
            mqtt_cfg.client_id = tmp;
            mqtt[i].is_connected = false;
            mqtt[i].mqtt_index = i;
            mqtt_cfg.user_context = (void*) &mqtt[i];
            mqtt[i].mqtt = esp_mqtt_client_init(&mqtt_cfg);
            /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
            esp_mqtt_client_register_event(mqtt[i].mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, &mqtt[i].mqtt);
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
    if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled)
        MQTT1MessagesQueueHandle = xQueueCreateStatic(CH_MESSAGE_BUFER_LENTH,
                                                      sizeof(DATA_SEND_STRUCT),
                                                      MQTT1MessagesQueueStorageArea,
                                                      &xStaticMQTT1MessagesQueue);
    MQTT2MessagesQueueHandle = NULL;
    if (GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
        MQTT2MessagesQueueHandle = xQueueCreateStatic(CH_MESSAGE_BUFER_LENTH,
                                                      sizeof(DATA_SEND_STRUCT),
                                                      MQTT2MessagesQueueStorageArea,
                                                      &xStaticMQTT2MessagesQueue);

    mqtt[0].mqtt_queue = MQTT1MessagesQueueHandle;
    mqtt[1].mqtt_queue = MQTT2MessagesQueueHandle;



    start_mqtt();
}

#endif
