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

#include <WebGUIAppMain.h>
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

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       10

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static bool isWiFiGotIp = false;

#define DEFAULT_SCAN_LIST_SIZE 20
static wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
static bool isScanReady = false;

static TaskHandle_t reconnect_task = NULL;

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
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        isWiFiGotIp = false;
        //esp_wifi_connect();

        /*
        ESP_LOGE(TAG, "Connect to the AP fail");
        if (!reconnect_task)
        {
            xTaskCreate(resonnectWithDelay, "reconnect_delay", 1024, NULL, 3, &reconnect_task);
            ESP_LOGW(TAG, "Pending reconnect in %d seconds", WIFI_CONNECT_AFTER_FAIL_DELAY);
        }
        */
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

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

    if (event_id == WIFI_EVENT_AP_STACONNECTED)
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
    if (max_power >= 8 && max_power <= 84)
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
    if (max_power >= 8 && max_power <= 84)
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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &event_handler,
                    NULL,
                    NULL));

    wifi_config_t ap_wifi_config = {
            .ap = {

            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
                    .max_connection = EXAMPLE_MAX_STA_CONN,
                    .authmode = WIFI_AUTH_WPA_WPA2_PSK
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

                    .pmf_cfg = {
                            .capable = true,
                            .required = false
                    },
            },
    };
    memcpy(sta_wifi_config.sta.ssid, GetSysConf()->wifiSettings.InfSSID, strlen(GetSysConf()->wifiSettings.InfSSID));
    memcpy(sta_wifi_config.sta.password, GetSysConf()->wifiSettings.InfSecurityKey,
           strlen(GetSysConf()->wifiSettings.InfSecurityKey));
    //END STA MODE CONFIGURATION

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    int max_power = GetSysConf()->wifiSettings.MaxPower;
    if (max_power >= 8 && max_power <= 84)
        esp_wifi_set_max_tx_power(max_power);

    wifi_country_t CC;
    esp_wifi_get_country(&CC);
    ESP_LOGW(TAG, "Country code %.*s, start_ch=%d, total_ch=%d, max power %d", 3, CC.cc, CC.schan, CC.nchan,
             CC.max_tx_power);

    ESP_LOGI(TAG, "wifi_init_softap_sta finished");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */

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

#define BASE_RECONNECT_INTERVAL 6

static void WiFiControlTask(void *arg)
{
    //WiFi init and start block
    static int reconnect_interval = BASE_RECONNECT_INTERVAL;
    static int reconnect_counter = BASE_RECONNECT_INTERVAL;
    s_wifi_event_group = xEventGroupCreate();
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
    //WiFi in work service
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & WIFI_CONNECTED_BIT)
        {
            reconnect_interval = BASE_RECONNECT_INTERVAL;
            reconnect_counter = BASE_RECONNECT_INTERVAL;
        }
        else if (bits & WIFI_FAIL_BIT)
        {
            if (--reconnect_counter <= 0)
            {
                ESP_LOGI(TAG, "WiFi STA started, reconnecting to AP...");
                esp_wifi_connect();
                reconnect_interval += BASE_RECONNECT_INTERVAL;
                if (reconnect_interval >= BASE_RECONNECT_INTERVAL * 10)
                    reconnect_interval = BASE_RECONNECT_INTERVAL * 10;
                reconnect_counter = reconnect_interval;
            }
        }

    }
}

void WiFiStart(void)
{
    xTaskCreate(WiFiControlTask, "WiFiCtrlTask", 1024 * 4, (void*) 0, 3, NULL);
}

static void wifi_scan(void *arg)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    isScanReady = true;
    vTaskDelete(NULL);
}

void WiFiScan(void)
{
    isScanReady = false;
    xTaskCreate(wifi_scan, "ScanWiFiTask", 1024 * 4, (void*) 0, 3, NULL);
}
