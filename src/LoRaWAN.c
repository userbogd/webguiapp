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
 *   File name: LoRaWANTransport.c
 *     Project: MStation2Firmware
 *  Created on: 2022-12-17
 *      Author: bogd
 * Description:	
 */

#include "ttn.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "Helpers.h"
#include "SystemConfiguration.h"
#include "NetTransport.h"
#include "webguiapp.h"
#include "LoRaWAN.h"

// Pins and other resources
/*Defined in global configuration*/

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE

#define TTN_SPI_HOST      CONFIG_SPI_HOST
#define TTN_PIN_NSS       CONFIG_LORA_SPI_CS_GPIO
#define TTN_PIN_RXTX      TTN_NOT_CONNECTED
#define TTN_PIN_RST       TTN_NOT_CONNECTED
#define TTN_PIN_DIO0      CONFIG_LORA_DIO0_GPIO
#define TTN_PIN_DIO1      CONFIG_LORA_DIO1_GPIO

#define LORAWAN_DELIVERY_RETRY_PERIOD 10
#define MESSAGE_LENGTH 32
#define TAG "LoRaWANApp"
#define LORAWAN_APP_LOG_ENABLED 1
#define LORAWAN_MESSAGE_BUFER_LENTH 32

QueueHandle_t LORAMessagesQueueHandle;
static StaticQueue_t xStaticLoRaMessagesQueue;
uint8_t LoRaMessagesQueueStorageArea[LORAWAN_MESSAGE_BUFER_LENTH
        * sizeof(LORA_DATA_SEND_STRUCT)];

void (*LoRaUserReceiveHandler)(const char *message, int length, int port);
void regLoRaUserReceiveHandler(
        void (*user_handler)(const char *message, int length, int port))
{
    LoRaUserReceiveHandler = user_handler;
}


esp_err_t LORASendData(LORA_DATA_SEND_STRUCT *pdss)
{
    char *ptr = (char*) malloc(MESSAGE_LENGTH);
    if (ptr)
    {
        memcpy(ptr, pdss->raw_data_ptr, MESSAGE_LENGTH);
        LORA_DATA_SEND_STRUCT DSS;
        DSS.raw_data_ptr = ptr;
        DSS.data_length = MESSAGE_LENGTH;
        if (xQueueSend(LORAMessagesQueueHandle, &DSS,
                pdMS_TO_TICKS(1000)) == pdPASS)
            return ESP_OK;
        else
        {
            free(ptr);
            return ESP_ERR_TIMEOUT;
        }
    }
    else
        return ESP_ERR_NO_MEM;
}

void messageReceived(const uint8_t *message, size_t length, ttn_port_t port)
{
#if LORAWAN_APP_LOG_ENABLED == 1
    if (length == MESSAGE_LENGTH && port == 1)
    {
        char P[MESSAGE_LENGTH * 2 + 1];
        P[MESSAGE_LENGTH * 2] = 0x00;
        BytesToStr((unsigned char*) message, (unsigned char*) P, length);
        ESP_LOGI(TAG, "Received=%s", P);
    }
#endif
    if(LoRaUserReceiveHandler != NULL)
        LoRaUserReceiveHandler((char*)message, length, (int)port);

}

void LoRaWANTransportTask(void *pvParameter)
{
    LORA_DATA_SEND_STRUCT DSS;
    while (!LORAMessagesQueueHandle)
        vTaskDelay(pdMS_TO_TICKS(300)); //wait for LORA queue ready
    while (1)
    {
        while (!ttn_is_connected())
            vTaskDelay(pdMS_TO_TICKS(300));
        if (ttn_is_provisioned())
        {
            xQueueReceive(LORAMessagesQueueHandle, &DSS, portMAX_DELAY);
            ttn_transmit_message((const uint8_t*) DSS.raw_data_ptr, MESSAGE_LENGTH, 1, true);

#if LORAWAN_APP_LOG_ENABLED == 1
            char P[MESSAGE_LENGTH * 2 + 1];
            BytesToStr((unsigned char*) DSS.raw_data_ptr, (unsigned char*) P, MESSAGE_LENGTH);
            P[MESSAGE_LENGTH * 2] = 0x00;
            ESP_LOGI(TAG, "Sent=%s", P);
#endif
        }
        else
        {
#if LORAWAN_APP_LOG_ENABLED == 1
            ESP_LOGW(TAG, "Transmit fail, transport not ready");
#endif
        }
    }
}


void LoRaWANRejoin(void)
{
    ttn_rejoin();
}

void LoRaWANInitJoinTask(void *pvParameter)
{
    LORAMessagesQueueHandle = NULL;
    if (GetSysConf()->lorawanSettings.Flags1.bIsLoRaWANEnabled)
        LORAMessagesQueueHandle = xQueueCreateStatic(LORAWAN_MESSAGE_BUFER_LENTH,
                                                     sizeof(LORA_DATA_SEND_STRUCT),
                                                     LoRaMessagesQueueStorageArea,
                                                     &xStaticLoRaMessagesQueue);
    ttn_init();
    // Configure the SX127x pins
    ttn_configure_pins(TTN_SPI_HOST, TTN_PIN_NSS, TTN_PIN_RXTX, -1,
    TTN_PIN_DIO0,
                       TTN_PIN_DIO1);

    char devEui[17], appEui[17], appKey[33];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.DevEui,
               (unsigned char*) devEui,
               8);
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppEui,
               (unsigned char*) appEui,
               8);
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppKey,
               (unsigned char*) appKey,
               16);
    // Register callback for received messages
    ttn_on_message(messageReceived);

    while (!ttn_join(devEui, appEui, appKey))
    {
    };

    xTaskCreate(LoRaWANTransportTask, "LoRaWANTransportTask", 1024 * 4,
                (void*) 0,
                3,
                NULL);

    vTaskDelete(NULL);
}
#endif


void LoRaWANStart(void)
{
#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    xTaskCreate(LoRaWANInitJoinTask, "LoRaWANInitJoinTask", 1024 * 4, (void*) 0,
                3,
                NULL);
#endif
}

bool isLORAConnected(void)
{
#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    return ttn_is_connected();
#else
    return false;
#endif
}
