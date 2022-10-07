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

#include "SystemConfiguration.h"
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

esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

static const char *TAG = "WiFiTransport";

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       10

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
static bool isWiFiGotIp = false;

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

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        isWiFiGotIp = false;
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        memcpy(&GetSysConf()->wifiSettings.InfIPAddr, &event->ip_info.ip, sizeof(event->ip_info.ip));
        memcpy(&GetSysConf()->wifiSettings.InfMask, &event->ip_info.netmask, sizeof(event->ip_info.netmask));
        memcpy(&GetSysConf()->wifiSettings.InfGateway, &event->ip_info.gw, sizeof(event->ip_info.gw));
        isWiFiGotIp = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac),
                 event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac),
                 event->aid);
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
    if (strlen(DEFAULT_WIFI_SSID_AP_KEY) == 0)
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

    ESP_LOGI(TAG, "wifi_init_softap finished");
    vTaskDelete(NULL);
}

static void wifi_init_sta(void *pvParameter)
{
    s_wifi_event_group = xEventGroupCreate();
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

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
    WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 GetSysConf()->wifiSettings.InfSSID,
                 GetSysConf()->wifiSettings.InfSecurityKey);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 GetSysConf()->wifiSettings.InfSSID,
                 GetSysConf()->wifiSettings.InfSecurityKey);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
   // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
   // ESP_ERROR_CHECK(esp_event_handler_instance_unregister( WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
   // vEventGroupDelete(s_wifi_event_group);
    vTaskDelete(NULL);
}

void WiFiAPStart(void)
{
    xTaskCreate(wifi_init_softap, "InitSoftAPTask", 1024 * 4, (void*) 0, 3, NULL);
}

void WiFiSTAStart(void)
{
    xTaskCreate(wifi_init_sta, "InitStationTask", 1024 * 4, (void*) 0, 3, NULL);
}


