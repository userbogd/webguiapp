/*! Copyright 2022 Bogdan Pilyugin
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
 *  	 \file SystemConfiguration.c
 *    \version 1.0
 * 		 \date 2022-08-13
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "SystemConfiguration.h"
#include "stdlib.h"
#include "string.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define STORAGE_NAMESPACE "storage"
#define TAG "SystemConfiguration"

static SYS_CONFIG SysConfig;

static void ResetSysConfig(SYS_CONFIG *Conf)
{
#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    Conf->wifiSettings.Flags1.bIsWiFiEnabled = CONFIG_WEBGUIAPP_WIFI_ON;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfIPAddr);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_MASK_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfMask);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_GATEWAY_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfGateway);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_AP, (esp_ip4_addr_t*) &Conf->wifiSettings.ApIPAddr);
    Conf->wifiSettings.Flags1.bIsAP = true;
    memcpy(Conf->wifiSettings.ApSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_AP, sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_AP));
    memcpy(Conf->wifiSettings.InfSSID, CONFIG_WEBGUIAPP_WIFI_SSID_STA, sizeof(CONFIG_WEBGUIAPP_WIFI_SSID_STA));
    memcpy(Conf->wifiSettings.InfSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_STA, sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_STA));
    Conf->wifiSettings.Flags1.bIsDHCPEnabled = CONFIG_WEBGUIAPP_WIFI_DHCP_ON;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->wifiSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->wifiSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->wifiSettings.DNSAddr3);

#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    Conf->ethSettings.Flags1.bIsETHEnabled = CONFIG_WEBGUIAPP_ETHERNET_ON;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_IP_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.IPAddr);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_MASK_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Mask);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_GATEWAY_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Gateway);
    //Conf->ethSettings.Flags1.bIsDHCPEnabled = CONFIG_WEBGUIAPP_ETHERNET_DHCP_ON ;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->ethSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->ethSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->ethSettings.DNSAddr3);
#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
    Conf->gsmSettings.Flags1.bIsGSMEnabled = true;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->gsmSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->gsmSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*)&Conf->gsmSettings.DNSAddr3);

#endif


#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    Conf->mqttStation[0].Flags1.bIsGlobalEnabled = CONFIG_MQTT_ON;
    memcpy(Conf->mqttStation[0].ServerAddr, CONFIG_MQTT_SERVER_URL, sizeof(CONFIG_MQTT_SERVER_URL));
    Conf->mqttStation[0].ServerPort = CONFIG_MQTT_SERVER_PORT;
    memcpy(Conf->mqttStation[0].ClientID, CONFIG_MQTT_CLIENT_ID_1, sizeof(CONFIG_MQTT_CLIENT_ID_1));
    memcpy(Conf->mqttStation[0].RootTopic, CONFIG_MQTT_ROOT_TOPIC, sizeof(CONFIG_MQTT_ROOT_TOPIC));
    memcpy(Conf->mqttStation[0].UserName, CONFIG_MQTT_USERNAME, sizeof(CONFIG_MQTT_USERNAME));
    memcpy(Conf->mqttStation[0].UserPass, CONFIG_MQTT_PASSWORD, sizeof(CONFIG_MQTT_PASSWORD));
#if CONFIG_MQTT_CLIENTS_NUM == 2
    Conf->mqttStation[1].Flags1.bIsGlobalEnabled = CONFIG_MQTT_ON;
    memcpy(Conf->mqttStation[1].ServerAddr, CONFIG_MQTT_SERVER_URL, sizeof(CONFIG_MQTT_SERVER_URL));
    Conf->mqttStation[1].ServerPort = CONFIG_MQTT_SERVER_PORT;
    memcpy(Conf->mqttStation[1].ClientID, CONFIG_MQTT_CLIENT_ID_2, sizeof(CONFIG_MQTT_CLIENT_ID_2));
    memcpy(Conf->mqttStation[1].RootTopic, CONFIG_MQTT_ROOT_TOPIC, sizeof(CONFIG_MQTT_ROOT_TOPIC));
    memcpy(Conf->mqttStation[1].UserName, CONFIG_MQTT_USERNAME, sizeof(CONFIG_MQTT_USERNAME));
    memcpy(Conf->mqttStation[1].UserPass, CONFIG_MQTT_PASSWORD, sizeof(CONFIG_MQTT_PASSWORD));
#endif
#endif

}

esp_err_t ReadNVSSysConfig(SYS_CONFIG *SysConf)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // obtain required memory space to store blob being read from NVS
    size_t L = (size_t) sizeof(SYS_CONFIG);
    ESP_LOGI(TAG, "Size of structure to read is %d", L);
    err = nvs_get_blob(my_handle, "sys_conf", SysConf, &L);
    if (err != ESP_OK)
        return err;
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t WriteNVSSysConfig(SYS_CONFIG *SysConf)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    size_t L = (size_t) sizeof(SYS_CONFIG);
    ESP_LOGI(TAG, "Size of structure to write is %d", L);
    err = nvs_set_blob(my_handle, "sys_conf", SysConf, L);
    if (err != ESP_OK)
        return err;

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;
    // Close
    nvs_close(my_handle);

    return ESP_OK;
}

SYS_CONFIG* GetSysConf(void)
{
    return &SysConfig;
}

esp_err_t InitSysConfig(void)
{
    esp_err_t err;
    err = ReadNVSSysConfig(&SysConfig);
    if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Read system configuration OK");
        }
        return err;
    }
    else
    {
        ESP_LOGI(TAG, "Reset and write default system configuration");
        ResetSysConfig(&SysConfig);
        err = WriteNVSSysConfig(&SysConfig);
    }
    return err;
}

esp_err_t ResetInitSysConfig(void)
{
    ESP_LOGI(TAG, "Reset and write default system configuration");
    ResetSysConfig(&SysConfig);
    return WriteNVSSysConfig(&SysConfig);
}

