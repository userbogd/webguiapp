/*! Copyright 2023 Bogdan Pilyugin
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
 *  	 \file RestApiHandler.c
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>
#include "esp_wifi.h"
#include "esp_netif.h"

extern SYS_CONFIG SysConfig;

static void funct_addone(char *argres, int rw)
{
    int arg = atoi(argres);
    arg *= 2;
    itoa(arg, argres, 10);
}

static void funct_time(char *argres, int rw)
{
    time_t now;
    time(&now);
    snprintf(argres, MAX_DYNVAR_LENGTH, "%d", (int) now);
}

static void funct_wifiscan(char *argres, int rw)
{
    if (atoi(argres))
        WiFiScan();
}

static void funct_wifiscanres(char *argres, int rw)
{
    int arg = atoi(argres);
    wifi_ap_record_t *R = GetWiFiAPRecord(arg);
    if (!R)
        return;
    snprintf(argres, MAX_DYNVAR_LENGTH, "{\"ssid\":\"%s\",\"rssi\":%i,\"ch\":%d}", R->ssid, R->rssi,
             R->primary);
}

static void funct_wifimode(char *argres, int rw)
{
    switch (rw)
    {
        case 0:
            SysConfig.wifiSettings.WiFiMode = atoi(argres);
            break;
        case 1:
            itoa(SysConfig.wifiSettings.WiFiMode, argres, 10);
            break;
    }
}

const rest_var_t ConfigVariables[] =
        {
                /*FUNCTIONS*/
                { 0, "time", &funct_time, VAR_FUNCT, 0, 0 },
                { 0, "addone", &funct_addone, VAR_FUNCT, 0, 0 },
                { 0, "wifi_scan", &funct_wifiscan, VAR_FUNCT, 0, 0 },
                { 0, "wifi_scan_res", &funct_wifiscanres, VAR_FUNCT, 0, 0 },

                /*VARIABLES*/
                { 0, "net_bios_name", &SysConfig.NetBIOSName, VAR_STRING, 3, 31 },
                { 0, "sys_name", &SysConfig.SysName, VAR_STRING, 3, 31 },
                { 0, "sys_pass", &SysConfig.SysPass, VAR_STRING, 3, 31 },

                { 0, "ota_url", &SysConfig.OTAURL, VAR_STRING, 3, 128 },
                { 0, "ota_auto_int", &SysConfig.OTAAutoInt, VAR_INT, 0, 65535 },

                { 0, "ser_num", &SysConfig.SN, VAR_STRING, 10, 10 },
                { 0, "dev_id", &SysConfig.ID, VAR_STRING, 8, 8 },
                { 0, "color_scheme", &SysConfig.ColorSheme, VAR_INT, 1, 2 },

                { 0, "ota_enab", &SysConfig.Flags1.bIsOTAEnabled, VAR_BOOL, 0, 1 },
                { 0, "res_ota_enab", &SysConfig.Flags1.bIsResetOTAEnabled, VAR_BOOL, 0, 1 },
                { 0, "led_enab", &SysConfig.Flags1.bIsLedsEnabled, VAR_BOOL, 0, 1 },
                { 0, "lora_confirm", &SysConfig.Flags1.bIsLoRaConfirm, VAR_BOOL, 0, 1 },
                { 0, "tcp_confirm", &SysConfig.Flags1.bIsTCPConfirm, VAR_BOOL, 0, 1 },

                { 0, "sntp_timezone", &SysConfig.sntpClient.TimeZone, VAR_INT, 0, 23 },
                { 0, "sntp_serv1", &SysConfig.sntpClient.SntpServerAdr, VAR_STRING, 3, 32 },
                { 0, "sntp_serv2", &SysConfig.sntpClient.SntpServer2Adr, VAR_STRING, 3, 32 },
                { 0, "sntp_serv3", &SysConfig.sntpClient.SntpServer3Adr, VAR_STRING, 3, 32 },
                { 0, "sntp_enab", &SysConfig.sntpClient.Flags1.bIsGlobalEnabled, VAR_BOOL, 0, 1 },

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
                { 0, "mqtt_1_serv", &SysConfig.mqttStation[0].ServerAddr, VAR_STRING, 3, 63 },
                { 0, "mqtt_1_port", &SysConfig.mqttStation[0].ServerPort, VAR_INT, 1, 65534 },
                { 0, "mqtt_1_syst", &SysConfig.mqttStation[0].SystemName, VAR_STRING, 3, 31 },
                { 0, "mqtt_1_group", &SysConfig.mqttStation[0].GroupName, VAR_STRING, 3, 31 },
                { 0, "mqtt_1_clid", &SysConfig.mqttStation[0].ClientID, VAR_STRING, 3, 31 },
                { 0, "mqtt_1_uname", &SysConfig.mqttStation[0].UserName, VAR_STRING, 3, 31 },
                { 0, "mqtt_1_pass", &SysConfig.mqttStation[0].UserPass, VAR_STRING, 3, 31 },

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
                { 0, "mqtt_2_serv", &SysConfig.mqttStation[1].ServerAddr, VAR_STRING, 3, 63 },
                { 0, "mqtt_2_port", &SysConfig.mqttStation[1].ServerPort, VAR_INT, 1, 65534 },
                { 0, "mqtt_2_syst", &SysConfig.mqttStation[1].SystemName, VAR_STRING, 3, 31 },
                { 0, "mqtt_2_group", &SysConfig.mqttStation[1].GroupName, VAR_STRING, 3, 31 },
                { 0, "mqtt_2_clid", &SysConfig.mqttStation[1].ClientID, VAR_STRING, 3, 31 },
                { 0, "mqtt_2_uname", &SysConfig.mqttStation[1].UserName, VAR_STRING, 3, 31 },
                { 0, "mqtt_2_pass", &SysConfig.mqttStation[1].UserPass, VAR_STRING, 3, 31 },

#endif
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
                { 0, "eth_ip", &SysConfig.wifiSettings.InfIPAddr, VAR_IPADDR, 0, 0 },
                { 0, "eth_mask", &SysConfig.wifiSettings.InfMask, VAR_IPADDR, 0, 0 },
                { 0, "eth_gw", &SysConfig.wifiSettings.InfGateway, VAR_IPADDR, 0, 0 },
                { 0, "eth_dns1", &SysConfig.wifiSettings.DNSAddr1, VAR_IPADDR, 0, 0 },
                { 0, "eth_dns2", &SysConfig.wifiSettings.DNSAddr2, VAR_IPADDR, 0, 0 },
                { 0, "eth_dns3", &SysConfig.wifiSettings.DNSAddr3, VAR_IPADDR, 0, 0 },




#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
                { 0, "wifi_mode", &funct_wifimode, VAR_FUNCT, 1, 3 },
                { 0, "wifi_sta_ip", &SysConfig.wifiSettings.InfIPAddr, VAR_IPADDR, 0, 0 },
                { 0, "wifi_sta_mask", &SysConfig.wifiSettings.InfMask, VAR_IPADDR, 0, 0 },
                { 0, "wifi_sta_gw", &SysConfig.wifiSettings.InfGateway, VAR_IPADDR, 0, 0 },
                { 0, "wifi_ap_ip", &SysConfig.wifiSettings.ApIPAddr, VAR_IPADDR, 0, 0 },
                { 0, "wifi_dns1", &SysConfig.wifiSettings.DNSAddr1, VAR_IPADDR, 0, 0 },
                { 0, "wifi_dns2", &SysConfig.wifiSettings.DNSAddr2, VAR_IPADDR, 0, 0 },
                { 0, "wifi_dns3", &SysConfig.wifiSettings.DNSAddr3, VAR_IPADDR, 0, 0 },
                { 0, "wifi_sta_ssid", &SysConfig.wifiSettings.InfSSID, VAR_STRING, 3, 31 },
                { 0, "wifi_sta_key", &SysConfig.wifiSettings.InfSecurityKey, VAR_STRING, 8, 31 },
                { 0, "wifi_ap_ssid", &SysConfig.wifiSettings.ApSSID, VAR_STRING, 3, 31 },
                { 0, "wifi_ap_key", &SysConfig.wifiSettings.ApSecurityKey, VAR_STRING, 8, 31 },

                { 0, "wifi_enab", &SysConfig.wifiSettings.Flags1.bIsWiFiEnabled, VAR_BOOL, 0, 1 },
                { 0, "wifi_isdhcp", &SysConfig.wifiSettings.Flags1.bIsDHCPEnabled, VAR_BOOL, 0, 1 },
                { 0, "wifi_power", &SysConfig.wifiSettings.MaxPower, VAR_INT, 0, 80 },
        #endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE


#endif

        };

esp_err_t SetConfVar(char *name, char *val, rest_var_types *tp)
{
    rest_var_t *V = NULL;
    for (int i = 0; i < sizeof(ConfigVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, name))
        {
            V = (rest_var_t*) (&ConfigVariables[i]);
            break;
        }
    }
    if (!V)
        return ESP_ERR_NOT_FOUND;
    int constr;
    *tp = V->vartype;
    switch (V->vartype)
    {
        case VAR_BOOL:
            if (!strcmp(val, "true") || !strcmp(val, "1"))
                *((bool*) V->ref) = true;
            else if (!strcmp(val, "false") || !strcmp(val, "0"))
                *((bool*) V->ref) = 0;
            else
                return ESP_ERR_INVALID_ARG;
        break;
        case VAR_INT:
            constr = atoi(val);
            if (constr < V->minlen || constr > V->maxlen)
                return ESP_ERR_INVALID_ARG;
            *((int*) V->ref) = constr;
        break;
        case VAR_STRING:
            constr = strlen(val);
            if (constr < V->minlen || constr > V->maxlen)
                return ESP_ERR_INVALID_ARG;
            strcpy(V->ref, val);
        break;
        case VAR_IPADDR:
            esp_netif_str_to_ip4(val, (esp_ip4_addr_t*) (V->ref));
        break;
        case VAR_FUNCT:
            ((void (*)(char*, int)) (V->ref))(val, 1);
        break;
        case VAR_ERROR:
            break;

    }
    return ESP_OK;
}

esp_err_t GetConfVar(char *name, char *val, rest_var_types *tp)
{
    rest_var_t *V = NULL;
    for (int i = 0; i < sizeof(ConfigVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, name))
        {
            V = (rest_var_t*) (&ConfigVariables[i]);
            break;
        }
    }
    if (!V)
        return ESP_ERR_NOT_FOUND;
    *tp = V->vartype;
    switch (V->vartype)
    {
        case VAR_BOOL:
            strcpy(val, *((bool*) V->ref) ? "true" : "false");
        break;
        case VAR_INT:
            itoa(*((int*) V->ref), val, 10);
        break;
        case VAR_STRING:
            strcpy(val, (char*) V->ref);
        break;
        case VAR_IPADDR:
            esp_ip4addr_ntoa((const esp_ip4_addr_t*) V->ref, val, 16);
        break;
        case VAR_FUNCT:
            ((void (*)(char*, int)) (V->ref))(val, 0);
        break;
        case VAR_ERROR:
            break;
    }

    //val = V->ref;
    return ESP_OK;
}

