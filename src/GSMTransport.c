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
 *   File name: GSMTransport.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include <string.h>

#include "../include/SysConfiguration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "esp_event.h"
#include "esp_modem_api.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "NetTransport.h"

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const char *TAG = "GSMTransport";
static int ResetType = 0;
static bool isPPPinitializing = false;
#endif

static bool isPPPConn = false;
TaskHandle_t initTaskhandle;

#define PPP_MODEM_TIMEOUT 40

MODEM_INFO mod_info = { "-", "-", "-", "-" };
esp_netif_t *ppp_netif;
esp_modem_dce_t *dce;
TaskHandle_t trasporttask;

static void (*gsm_reset)(uint8_t level) = NULL;
void RegGSMReset(void (*gsm_rst)(uint8_t level))
{
    gsm_reset = gsm_rst;
}

esp_netif_t* GetPPPNetifAdapter(void)
{
    return ppp_netif;
}

MODEM_INFO* GetPPPModemInfo(void)
{
    return &mod_info;
}

bool isPPPConnected(void)
{
    return isPPPConn;
}

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id,
                           void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %u", (unsigned int)event_id);
    if (event_id == NETIF_PPP_ERRORUSER)
    {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id,
                        void *event_data)
{
    ESP_LOGD(TAG, "IP event! %u", (unsigned int)event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP)
    {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);
        isPPPConn = true;
        ESP_LOGI(TAG, "GOT ip event!!!");
    }
    else if (event_id == IP_EVENT_PPP_LOST_IP)
    {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
        isPPPConn = false;
    }
    else if (event_id == IP_EVENT_GOT_IP6)
    {
        ESP_LOGI(TAG, "GOT IPv6 event!");
        ip_event_got_ip6_t *event = (ip_event_got_ip6_t*) event_data;
        ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
    }
}

void ModemNotReady(void)
{

}

static void GSMInitTask(void *pvParameter)
{
    isPPPinitializing = true;
    int starttype = *((int*) pvParameter);
    esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event);
    esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed);

    if (dce)
    {
        esp_modem_destroy(dce);
    }

    if (ppp_netif != NULL)
    {
        esp_netif_destroy(ppp_netif);
    }

    if (starttype == 0)
    {
#if CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN >= 0
    gpio_set_level(CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(5000));
#else
        if (gsm_reset)
        {
            gsm_reset(0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            gsm_reset(1);
        }
        else
        {
            ESP_LOGE(TAG, "ethernet chip reset pin not defined");
            ESP_ERROR_CHECK(1);
        }
#endif

    }

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
    event_group = xEventGroupCreate();
    /* Configure the DTE */
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */
    //dte_config.uart_config.port_num = CONFIG_MODEM_UART_PORT_NUM;
    dte_config.uart_config.tx_io_num = CONFIG_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = CONFIG_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = CONFIG_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = CONFIG_MODEM_UART_CTS_PIN;
    dte_config.uart_config.rx_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE;
    dte_config.uart_config.tx_buffer_size = CONFIG_MODEM_UART_TX_BUFFER_SIZE;
    dte_config.uart_config.event_queue_size = CONFIG_MODEM_UART_EVENT_QUEUE_SIZE;
    dte_config.task_stack_size = CONFIG_MODEM_UART_EVENT_TASK_STACK_SIZE;
    dte_config.task_priority = CONFIG_MODEM_UART_EVENT_TASK_PRIORITY;
    dte_config.dte_buffer_size = CONFIG_MODEM_UART_RX_BUFFER_SIZE / 2;
    /* Configure the DCE */
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_MODEM_PPP_APN);
    /* Configure the PPP netif */
    esp_netif_inherent_config_t esp_netif_conf = ESP_NETIF_INHERENT_DEFAULT_PPP();

    esp_netif_conf.route_prio = PPP_PRIO;
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();

    netif_ppp_config.base = &esp_netif_conf;
    /* Init netif object */
    ppp_netif = esp_netif_new(&netif_ppp_config);
    assert(ppp_netif);
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM800, &dte_config, &dce_config, ppp_netif);
    assert(dce);

    mod_info.model[0] = 0x00;
    int GSMConnectTimeout = 0;
    while (esp_modem_get_module_name(dce, mod_info.model) != ESP_OK)
    {
        if (++GSMConnectTimeout >= PPP_MODEM_TIMEOUT)
            goto modem_init_fail;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Module type:%s", mod_info.model);

    mod_info.imsi[0] = 0x00;
    while (esp_modem_get_imsi(dce, mod_info.imsi) != ESP_OK)
    {
        if (++GSMConnectTimeout >= PPP_MODEM_TIMEOUT)
            goto modem_init_fail;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "IMSI:%s", mod_info.imsi);

    mod_info.oper[0] = 0x00;
    int tech = 0;
    while (esp_modem_get_operator_name(dce, mod_info.oper, &tech) != ESP_OK)
    {
        if (++GSMConnectTimeout >= PPP_MODEM_TIMEOUT)
            goto modem_init_fail;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Operator:%s", mod_info.oper);

    mod_info.imei[0] = 0x00;
    while (esp_modem_get_imei(dce, mod_info.imei) != ESP_OK)
    {
        if (++GSMConnectTimeout >= PPP_MODEM_TIMEOUT)
            goto modem_init_fail;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "IMEI:%s", mod_info.imei);

    while (esp_modem_set_mode(dce, ESP_MODEM_MODE_CMUX) != ESP_OK)
    {
        if (++GSMConnectTimeout >= PPP_MODEM_TIMEOUT)
            goto modem_init_fail;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "PPP data mode OK");

    xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    isPPPinitializing = false;
    vTaskDelete(NULL);

    return;
modem_init_fail:
    ESP_LOGE(TAG, "PPP modem init error");
    isPPPinitializing = false;
    vTaskDelete(NULL);
}

void PPPModemColdStart(void)
{
    ResetType = 0;
    xTaskCreate(GSMInitTask, "GSMInitTask", 1024 * 6, &ResetType, 3, &initTaskhandle);
}

void PPPModemSoftRestart(void)
{
    ResetType = 1;
    xTaskCreate(GSMInitTask, "GSMInitTask", 1024 * 6, &ResetType, 3, &initTaskhandle);
}

static void GSMRunTask(void *pvParameter)
{
    while (1)
    {
        if (!isPPPConn && !isPPPinitializing)
        { //try to reconnect modem
            ESP_LOGI(TAG, "PPP modem restart");
            PPPModemColdStart();
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void PPPModemStart(void)
{
    xTaskCreate(GSMRunTask, "GSMRunTask", 1024 * 4, &ResetType, 3, NULL);
}

int PPPModemGetRSSI(void)
{
    int rssi = -1, ber;
    esp_modem_get_signal_quality(dce, &rssi, &ber);
    //ESP_LOGW(TAG, "Signal %d, ber %d", rssi, ber);
    return rssi;
}

void ModemSendAT(char *cmd, char *resp, int timeout)
{
    esp_modem_at(dce, cmd, resp, timeout);
    ESP_LOGI(TAG, "Command:%s", cmd);
    ESP_LOGW(TAG, "%s", resp);
}

void ModemSendSMS(void)
{

}

#endif

