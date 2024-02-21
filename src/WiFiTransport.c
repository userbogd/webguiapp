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
 *   File name: WiFiTransport.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include <SysConfiguration.h>
#include "esp_log.h"
#include "Helpers.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "NetTransport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"

esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

static const char *TAG = "WiFiTransport";

#define WIFI_CONNECT_AFTER_FAIL_DELAY   40
#define WIFI_AP_ONBOOT_TIME 300

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       10

static bool isWiFiRunning = false;
static bool isWiFiConnected = false;
static bool isWiFiGotIp = false;
static bool isWiFiFail = false;
static int TempAPCounter = 0;

#define DEFAULT_SCAN_LIST_SIZE 20
static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
static bool isScanExecuting = false;

wifi_ap_record_t* GetWiFiAPRecord(uint8_t n)
{
    if (n < DEFAULT_SCAN_LIST_SIZE)
    {
        return &ap_info[n];
    }
    return NULL;
}

esp_netif_t* GetSTANetifAdapter(void)
{
    return sta_netif;
}

esp_netif_t* GetAPNetifAdapter(void)
{
    return ap_netif;
}

bool isWIFIConnected(void)
{
    return isWiFiGotIp;
}

void resonnectWithDelay(void *agr)
{
    vTaskDelay(pdMS_TO_TICKS(WIFI_CONNECT_AFTER_FAIL_DELAY * 1000));
    esp_wifi_connect();
    vTaskDelete(NULL);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi STA started, connecting to AP...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP)
    {
        isWiFiGotIp = false;
        ESP_LOGI(TAG, "WiFi STA stopped");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected to AP");
        isWiFiConnected = true;
        isWiFiFail = false;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Disconnected from AP");
        isWiFiConnected = false;
        isWiFiFail = true;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_BEACON_TIMEOUT)
    {
        ESP_LOGW(TAG, "STA beacon timeout");

    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        memcpy(&GetSysConf()->wifiSettings.InfIPAddr, &event->ip_info.ip, sizeof(event->ip_info.ip));
        memcpy(&GetSysConf()->wifiSettings.InfMask, &event->ip_info.netmask, sizeof(event->ip_info.netmask));
        memcpy(&GetSysConf()->wifiSettings.InfGateway, &event->ip_info.gw, sizeof(event->ip_info.gw));
        ESP_LOGI(TAG, "WIFI Got IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "WIFIIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "WIFIMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "WIFIGW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        isWiFiGotIp = true;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        memcpy(&GetSysConf()->wifiSettings.InfIPAddr, &event->ip_info.ip, sizeof(event->ip_info.ip));
        memcpy(&GetSysConf()->wifiSettings.InfMask, &event->ip_info.netmask, sizeof(event->ip_info.netmask));
        memcpy(&GetSysConf()->wifiSettings.InfGateway, &event->ip_info.gw, sizeof(event->ip_info.gw));
        ESP_LOGI(TAG, "WIFI Lost IP Address");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "WIFIIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "WIFIMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "WIFIGW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        isWiFiGotIp = false;
    }

    else if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap(void *pvParameter)
{
    char if_key_str[24];
    esp_netif_inherent_config_t esp_netif_conf = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP()
            ;

    strcpy(if_key_str, "WIFI_AP_USER");
    esp_netif_conf.if_key = if_key_str;
    esp_netif_conf.route_prio = AP_PRIO;

    esp_netif_config_t cfg_netif = {
            .base = &esp_netif_conf,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP
    };

    ap_netif = esp_netif_new(&cfg_netif);
    assert(ap_netif);

    esp_netif_ip_info_t ip_info;
    memcpy(&ip_info.ip, &GetSysConf()->wifiSettings.ApIPAddr, 4);
    memcpy(&ip_info.gw, &GetSysConf()->wifiSettings.ApIPAddr, 4);
    memcpy(&ip_info.netmask, &GetSysConf()->wifiSettings.InfMask, 4);

    esp_netif_dns_info_t dns_info;
    memcpy(&dns_info, &GetSysConf()->wifiSettings.ApIPAddr, 4);

    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    esp_netif_dhcps_start(ap_netif);

    esp_netif_attach_wifi_ap(ap_netif);
    esp_wifi_set_default_wifi_ap_handlers();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
            ;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
            .ap = {

            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
                    .max_connection = EXAMPLE_MAX_STA_CONN,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK
            },
    };
    if (strlen(CONFIG_WEBGUIAPP_WIFI_KEY_AP) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    memcpy(wifi_config.ap.ssid, GetSysConf()->wifiSettings.ApSSID, strlen(GetSysConf()->wifiSettings.ApSSID));
    memcpy(wifi_config.ap.password, GetSysConf()->wifiSettings.ApSecurityKey,
           strlen(GetSysConf()->wifiSettings.ApSecurityKey));
    wifi_config.ap.ssid_len = strlen(GetSysConf()->wifiSettings.ApSSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    int max_power = GetSysConf()->wifiSettings.MaxPower;
    if (max_power >= 8 && max_power <= 80)
        esp_wifi_set_max_tx_power(max_power);

    ESP_LOGI(TAG, "wifi_init_softap finished");
    vTaskDelete(NULL);
}

static void wifi_init_sta(void *pvParameter)
{
    //sta_netif = esp_netif_create_default_wifi_sta();
    char if_key_str[24];
    esp_netif_inherent_config_t esp_netif_conf = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    strcpy(if_key_str, "WIFI_STA_USER");
    esp_netif_conf.if_key = if_key_str;
    esp_netif_conf.route_prio = STA_PRIO;
    esp_netif_config_t cfg_netif = {
            .base = &esp_netif_conf,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA
    };
    sta_netif = esp_netif_new(&cfg_netif);
    assert(sta_netif);

    esp_netif_ip_info_t ip_info;
    memcpy(&ip_info.ip, &GetSysConf()->wifiSettings.InfIPAddr, 4);
    memcpy(&ip_info.gw, &GetSysConf()->wifiSettings.InfGateway, 4);
    memcpy(&ip_info.netmask, &GetSysConf()->wifiSettings.InfMask, 4);
    esp_netif_dns_info_t dns_info;
    memcpy(&dns_info, &GetSysConf()->wifiSettings.DNSAddr1, 4);

    esp_netif_dhcpc_stop(sta_netif);
    esp_netif_set_ip_info(sta_netif, &ip_info);
    esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);

    //esp_netif_str_to_ip4(&GetSysConf()->wifiSettings.DNSAddr3, (esp_ip4_addr_t*)(&dns_info.ip));
    memcpy(&dns_info.ip, &GetSysConf()->wifiSettings.DNSAddr3, sizeof(esp_ip4_addr_t));

    esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_FALLBACK, &dns_info);

    if (GetSysConf()->wifiSettings.Flags1.bIsDHCPEnabled)
        esp_netif_dhcpc_start(sta_netif);

    esp_netif_attach_wifi_station(sta_netif);
    esp_wifi_set_default_wifi_sta_handlers();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
            ;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
            .sta = {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                     * However these modes are deprecated and not advisable to be used. Incase your Access point
                     * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,

                    .pmf_cfg = {
                            .capable = true,
                            .required = false
                    },
            },
    };
    memcpy(wifi_config.sta.ssid, GetSysConf()->wifiSettings.InfSSID, strlen(GetSysConf()->wifiSettings.InfSSID));
    memcpy(wifi_config.sta.password, GetSysConf()->wifiSettings.InfSecurityKey,
           strlen(GetSysConf()->wifiSettings.InfSecurityKey));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    int max_power = GetSysConf()->wifiSettings.MaxPower;
    if (max_power >= 8 && max_power <= 80)
        esp_wifi_set_max_tx_power(max_power);

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    vTaskDelete(NULL);
}

static void wifi_init_apsta(void *pvParameter)
{
    //BEGIN AP MODE IF
    char ap_if_key_str[24];
    esp_netif_inherent_config_t ap_esp_netif_conf = ESP_NETIF_INHERENT_DEFAULT_WIFI_AP()
            ;

    strcpy(ap_if_key_str, "WIFI_AP_USER");
    ap_esp_netif_conf.if_key = ap_if_key_str;
    ap_esp_netif_conf.route_prio = AP_PRIO;

    esp_netif_config_t cfg_netif = {
            .base = &ap_esp_netif_conf,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP
    };

    ap_netif = esp_netif_new(&cfg_netif);
    assert(ap_netif);
    //END AP MODE IF

    //BEGIN STA MODE IF
    char sta_if_key_str[24];
    esp_netif_inherent_config_t staesp_netif_conf = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    strcpy(sta_if_key_str, "WIFI_STA_USER");
    staesp_netif_conf.if_key = sta_if_key_str;
    staesp_netif_conf.route_prio = STA_PRIO;
    esp_netif_config_t sta_cfg_netif = {
            .base = &staesp_netif_conf,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_STA
    };
    sta_netif = esp_netif_new(&sta_cfg_netif);
    assert(sta_netif);
    //END STA MODE IF

    //BEGIN AP MODE CONFIGURATION
    esp_netif_ip_info_t ip_info;
    memcpy(&ip_info.ip, &GetSysConf()->wifiSettings.ApIPAddr, 4);
    memcpy(&ip_info.gw, &GetSysConf()->wifiSettings.ApIPAddr, 4);
    memcpy(&ip_info.netmask, &GetSysConf()->wifiSettings.InfMask, 4);

    esp_netif_dns_info_t dns_info;
    memcpy(&dns_info, &GetSysConf()->wifiSettings.ApIPAddr, 4);

    esp_netif_dhcps_stop(ap_netif);
    esp_netif_set_ip_info(ap_netif, &ip_info);
    esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    esp_netif_dhcps_start(ap_netif);

    esp_netif_attach_wifi_ap(ap_netif);
    esp_wifi_set_default_wifi_ap_handlers();

    wifi_init_config_t ap_cfg = WIFI_INIT_CONFIG_DEFAULT()
            ;

    ESP_ERROR_CHECK(esp_wifi_init(&ap_cfg));
    wifi_config_t ap_wifi_config = {
            .ap = {

            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
                    .max_connection = EXAMPLE_MAX_STA_CONN,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            /*
             .pmf_cfg = {
             .capable = false,
             .required = false}
             */
            },
    };
    if (strlen(CONFIG_WEBGUIAPP_WIFI_KEY_AP) == 0)
    {
        ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    memcpy(ap_wifi_config.ap.ssid, GetSysConf()->wifiSettings.ApSSID, strlen(GetSysConf()->wifiSettings.ApSSID));
    memcpy(ap_wifi_config.ap.password, GetSysConf()->wifiSettings.ApSecurityKey,
           strlen(GetSysConf()->wifiSettings.ApSecurityKey));
    ap_wifi_config.ap.ssid_len = strlen(GetSysConf()->wifiSettings.ApSSID);
    //END AP MODE CONFIGURATION

    //BEGIN STA MODE CONFIGURATION
    //esp_netif_ip_info_t ip_info;
    memcpy(&ip_info.ip, &GetSysConf()->wifiSettings.InfIPAddr, 4);
    memcpy(&ip_info.gw, &GetSysConf()->wifiSettings.InfGateway, 4);
    memcpy(&ip_info.netmask, &GetSysConf()->wifiSettings.InfMask, 4);
    esp_netif_dns_info_t sta_dns_info;
    memcpy(&sta_dns_info, &GetSysConf()->wifiSettings.DNSAddr1, 4);

    esp_netif_dhcpc_stop(sta_netif);
    esp_netif_set_ip_info(sta_netif, &ip_info);
    esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &sta_dns_info);

    //esp_netif_str_to_ip4(&GetSysConf()->wifiSettings.DNSAddr3, (esp_ip4_addr_t*)(&dns_info.ip));
    memcpy(&sta_dns_info.ip, &GetSysConf()->wifiSettings.DNSAddr3, sizeof(esp_ip4_addr_t));

    esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_FALLBACK, &sta_dns_info);

    if (GetSysConf()->wifiSettings.Flags1.bIsDHCPEnabled)
        esp_netif_dhcpc_start(sta_netif);

    esp_netif_attach_wifi_station(sta_netif);
    esp_wifi_set_default_wifi_sta_handlers();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT()
            ;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
    ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t sta_wifi_config = {
            .sta = {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                     * However these modes are deprecated and not advisable to be used. Incase your Access point
                     * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            /*
             .pmf_cfg = {
             .capable = false,
             .required = false
             },
             */
            },
    };
    memcpy(sta_wifi_config.sta.ssid, GetSysConf()->wifiSettings.InfSSID, strlen(GetSysConf()->wifiSettings.InfSSID));
    memcpy(sta_wifi_config.sta.password, GetSysConf()->wifiSettings.InfSecurityKey,
           strlen(GetSysConf()->wifiSettings.InfSecurityKey));
    //END STA MODE CONFIGURATION

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
    esp_wifi_disable_pmf_config(WIFI_IF_STA);
    esp_wifi_disable_pmf_config(WIFI_IF_AP);
    ESP_ERROR_CHECK(esp_wifi_start());

    int max_power = GetSysConf()->wifiSettings.MaxPower;
    if (max_power >= 8 && max_power <= 80)
        esp_wifi_set_max_tx_power(max_power);

    wifi_country_t CC;
    esp_wifi_get_country(&CC);
    ESP_LOGI(TAG, "Country code %.*s, start_ch=%d, total_ch=%d, max power %d", 3, CC.cc, CC.schan, CC.nchan,
             CC.max_tx_power);

    ESP_LOGI(TAG, "wifi_init_softap_sta finished");
    vTaskDelete(NULL);
}

void WiFiDisconnect(void)
{
    esp_wifi_disconnect();
}

void WiFiConnect(void)
{
    esp_wifi_connect();
}

#define RECONNECT_INTERVAL_AP 60
#define RECONNECT_INTERVAL_STA 30
#define WAITIP_INTERVAL 10

static void WiFiControlTask(void *arg)
{
    //WiFi init and start block
    static int reconnect_counter;
    reconnect_counter =
            (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_STA) ? RECONNECT_INTERVAL_STA : RECONNECT_INTERVAL_AP;
    static int waitip_counter = WAITIP_INTERVAL;
    //s_wifi_event_group = xEventGroupCreate();
    switch (GetSysConf()->wifiSettings.WiFiMode)
    {
        case WIFI_MODE_STA:
            xTaskCreate(wifi_init_sta, "InitStationTask", 1024 * 4, (void*) 0, 3, NULL);
        break;
        case WIFI_MODE_AP:
            xTaskCreate(wifi_init_softap, "InitSoftAPTask", 1024 * 4, (void*) 0, 3, NULL);
        break;
        case WIFI_MODE_APSTA:
            xTaskCreate(wifi_init_apsta, "InitSoftAPSTATask", 1024 * 4, (void*) 0, 3, NULL);
        break;
    }
    isWiFiRunning = true;
    //WiFi in work service
    TempAPCounter = GetSysConf()->wifiSettings.AP_disab_time * 60;
    while (isWiFiRunning)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (isWiFiConnected)
        {
            reconnect_counter = RECONNECT_INTERVAL_STA;
            if (!isWiFiGotIp)
            {
                if (--waitip_counter <= 0)
                {
                    ESP_LOGW(TAG, "WiFi STA Connected but can't obtain IP...");
                    esp_wifi_disconnect();
                    waitip_counter = WAITIP_INTERVAL;
                }
            }
        }
        if (isWiFiFail)
        {
            if (--reconnect_counter <= 0 && !isScanExecuting)
            {
                ESP_LOGI(TAG, "WiFi STA started, reconnecting to AP...");
                esp_wifi_connect();
                reconnect_counter =
                        (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_STA) ?
                                RECONNECT_INTERVAL_STA : RECONNECT_INTERVAL_AP;
            }
        }
        if (TempAPCounter > 0)
        {
            if (--TempAPCounter <= 0)
            {
                if (GetAPClientsNumber() > 0)
                    TempAPCounter = GetSysConf()->wifiSettings.AP_disab_time * 60;
                else
                {
                    WiFiStopAP();
                    ESP_LOGI(TAG, "WiFi AP stopped after temporarily activity");
                }
            }
        }

    }

    if (isWiFiConnected)
    {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    esp_netif_destroy(ap_netif);
    vTaskDelete(NULL);
}

void WiFiStart(void)
{
    xTaskCreate(WiFiControlTask, "WiFiCtrlTask", 1024 * 4, (void*) 0, 3, NULL);
}

void WiFiStop()
{
    isWiFiRunning = false;
}

void WiFiStopAP()
{
    if (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_APSTA || GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_STA)
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    else
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}
void WiFiStartAP()
{
    if (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_APSTA)
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    else if (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_STA)
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    else
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
}

void WiFiStartAPTemp(int seconds)
{
    TempAPCounter = seconds;
    WiFiStartAP();
    ESP_LOGI(TAG, "WiFi AP started temporarily for %u seconds", seconds);
}

static void wifi_scan(void *arg)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    vTaskDelay(pdMS_TO_TICKS(1000)); //delay for command result get before network break
    memset(ap_info, 0, sizeof(ap_info));
    while (esp_wifi_scan_start(NULL, true) == ESP_ERR_WIFI_STATE)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    isScanExecuting = false;
    vTaskDelete(NULL);
}

void WiFiScan(void)
{
    isScanExecuting = true;
    xTaskCreate(wifi_scan, "ScanWiFiTask", 1024 * 4, (void*) 0, 3, NULL);
}

int GetAPClientsNumber()
{
    wifi_sta_list_t clients;
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_NULL || mode == WIFI_MODE_STA)
        return -1;
    if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK)
    {
        return clients.num;
    }
    return -1;
}

