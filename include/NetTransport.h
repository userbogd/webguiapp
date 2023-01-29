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
 *   File name: NetTransport.h
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#ifndef MAIN_INCLUDE_NETTRANSPORT_H_
#define MAIN_INCLUDE_NETTRANSPORT_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_netif.h"
#include "esp_log.h"

typedef struct
{
    char imei[32];
    char imsi[32];
    char oper[32];
    char model[32];
} MODEM_INFO;

#define ETH_PRIO 100
#define STA_PRIO 50
#define PPP_PRIO 30
#define AP_PRIO 10

//#define DEFAULT_FALLBACK_DNS  "8.8.8.8"

QueueHandle_t MQTT1MessagesQueueHandle;
QueueHandle_t MQTT2MessagesQueueHandle;
EventGroupHandle_t transport_event_group;

wifi_ap_record_t* GetWiFiAPRecord(uint8_t n);

void StartTimeGet(void);

void WiFiAPStart(void);
void WiFiSTAStart(void);
void WiFiScan(void);
void EthStart(void);

void WiFiTransportTask(void *prm);


void PPPModemColdStart(void);
void PPPModemSoftRestart(void);
void PPPModemStart(void);

void PPPModemGetRSSI(void);

void MQTTRun(void);

MODEM_INFO* GetPPPModemInfo(void);
esp_mqtt_client_handle_t* GetMQTTHandle(void);
void MQTTStart(void);
void MQTTStop(void);
void MQTTReconnect(void);

esp_netif_t* GetPPPNetifAdapter(void);
esp_netif_t* GetSTANetifAdapter(void);
esp_netif_t* GetAPNetifAdapter(void);
esp_netif_t* GetETHNetifAdapter(void);

bool isWIFIConnected(void);
bool isETHConnected(void);
bool isPPPConnected(void);
bool isLORAConnected(void);

bool GetMQTT1Connected(void);
bool GetMQTT2Connected(void);


void SetDefaultNetIF(esp_netif_t *IF);
void NextDefaultNetIF(void);
void PrintDefaultNetIF(void);
void GetDefaultNetIFName(char *name);

void PrintNetifs(void);
void GotEthIF(void);

void GetRFC3339Time(char *t);
void StartTimeGet(void);

esp_err_t StartOTA(void);
char* GetAvailVersion();
char* GetUpdateStatus();

void StartSystemTimer(void);
uint32_t GetUpTime(void);

void RegEthReset(void (*eth_rst)(uint8_t level));
void RegGSMReset(void (*gsm_rst)(uint8_t level));

void GenerateSystemSettingsJSONFile(void);

#endif /* MAIN_INCLUDE_NETTRANSPORT_H_ */
