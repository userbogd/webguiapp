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

#include "Helpers.h"
#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "romfs.h"
#include "esp_idf_version.h"
#include "NetTransport.h"
#include "esp_vfs.h"

extern SYS_CONFIG SysConfig;

rest_var_t *AppVars = NULL;
int AppVarsSize = 0;
void SetAppVars(rest_var_t *appvars, int size)
{
    AppVars = appvars;
    AppVarsSize = size;
}


static void funct_ser_num(char *argres, int rw)
{
    //UINT32_VAL d;
    //GetChipId((uint8_t *)d.v);
    //snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%010u\"", (unsigned int)swap(d.Val));
    char ser[24];
    GetDeviceSerial(ser);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", ser);
}

static void funct_dev_id(char *argres, int rw)
{
    /*
    char id[4];
    char id2[9];
    GetChipId((uint8_t*) id);
    BytesToStr((unsigned char*) id, (unsigned char*) id2, 4);
    id2[8] = 0x00;
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", id2);
    */
    char id[9];
    GetDeviceID(id);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", id);
}

static void PrintInterfaceState(char *argres, int rw, esp_netif_t *netif)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH,
             (netif != NULL && esp_netif_is_netif_up(netif)) ? "\"CONNECTED\"" : "\"DISCONNECTED\"");
}

static void funct_wifi_stat(char *argres, int rw)
{
    if (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_AP)
        PrintInterfaceState(argres, rw, GetAPNetifAdapter());
    else
        PrintInterfaceState(argres, rw, GetSTANetifAdapter());
}

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
static void funct_eth_stat(char *argres, int rw)
{
    PrintInterfaceState(argres, rw, GetETHNetifAdapter());
}
#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
static void funct_gsm_stat(char *argres, int rw)
{
    PrintInterfaceState(argres, rw, GetPPPNetifAdapter());
}
#endif

static void funct_mqtt_1_stat(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, (GetMQTT1Connected()) ? "\"CONNECTED\"" : "\"DISCONNECTED\"");
}
static void funct_mqtt_2_stat(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, (GetMQTT2Connected()) ? "\"CONNECTED\"" : "\"DISCONNECTED\"");
}
static void funct_mqtt_1_test(char *argres, int rw)
{
    if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled)
        PublicTestMQTT(0);
    snprintf(argres, VAR_MAX_VALUE_LENGTH,
             (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled) ? "\"OK\"" : "\"NOT_AVAIL\"");

}
static void funct_mqtt_2_test(char *argres, int rw)
{
    if (GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
        PublicTestMQTT(1);
    snprintf(argres, VAR_MAX_VALUE_LENGTH,
             (GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled) ? "\"OK\"" : "\"NOT_AVAIL\"");
}

static void funct_def_interface(char *argres, int rw)
{
    char interface[3];
    GetDefaultNetIFName(interface);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", interface);
}

static void funct_time(char *argres, int rw)
{
    time_t now;
    time(&now);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%d", (int) now);
}

static void funct_time_set(char *argres, int rw)
{
    time_t unix = atoi(argres);
    if (unix == 0)
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", "\"ERROR_UNIX_TIME_NULL\"");
        return;
    }
    struct timeval tv;
    tv.tv_sec = unix;
    SetSystemTimeVal(&tv, "Time set from user API");
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", "\"TIME_SET_OK\"");
}

static void funct_uptime(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%d", (int) GetUpTime());
}
static void funct_wifi_level(char *argres, int rw)
{
    wifi_ap_record_t wifi;
    if (esp_wifi_sta_get_ap_info(&wifi) == ESP_OK)
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%d dBm\"", wifi.rssi);
    else
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"-\"");
}

static void funct_fram(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%d", (int) esp_get_free_heap_size());
}
static void funct_fram_min(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%d", (int) esp_get_minimum_free_heap_size());
}

static void funct_idf_ver(char *argres, int rw)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", cur_app_info.idf_ver);
    else
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", "ESP_ERR_NOT_SUPPORTED");
}

static void funct_fw_ver(char *argres, int rw)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", cur_app_info.version);
    else
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", "ESP_ERR_NOT_SUPPORTED");
}

static void funct_build_date(char *argres, int rw)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s %s\"", cur_app_info.date, cur_app_info.time);
    else
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", "ESP_ERR_NOT_SUPPORTED");
}

static void PrintMACFromInterface(char *argres, int rw, esp_netif_t *netif)
{
    uint8_t mac_addr[6] = { 0 };
    esp_netif_get_mac(netif, mac_addr);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%02x-%02x-%02x-%02x-%02x-%02x\"",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);
}

static void funct_wifi_ap_mac(char *argres, int rw)
{
    PrintMACFromInterface(argres, rw, GetAPNetifAdapter());
}

static void funct_wifi_sta_mac(char *argres, int rw)
{
    PrintMACFromInterface(argres, rw, GetSTANetifAdapter());
}
static void funct_eth_mac(char *argres, int rw)
{
    PrintMACFromInterface(argres, rw, GetETHNetifAdapter());
}

static void funct_wifiscan(char *argres, int rw)
{
    if (atoi(argres))
        WiFiScan();
}

static void funct_wifiscanres(char *argres, int rw)
{
    int arg = atoi(argres);
    wifi_ap_record_t *Rec;
    struct jWriteControl jwc;
    jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_ARRAY, JW_COMPACT);
    for (int i = 0; i < arg; i++)
    {
        Rec = GetWiFiAPRecord(i);
        if (Rec && strlen((const char*) Rec->ssid) > 0)
        {
            jwArr_object(&jwc);
            jwObj_string(&jwc, "ssid", (char*) Rec->ssid);
            jwObj_int(&jwc, "rssi", Rec->rssi);
            jwObj_int(&jwc, "ch", Rec->primary);
            jwEnd(&jwc);
        }
    }
    int err = jwClose(&jwc);
    if (err == JWRITE_OK)
        return;
    if (err > JWRITE_BUF_FULL)
        strcpy(argres, "\"SYS_ERROR_NO_MEMORY\"");
    else
        strcpy(argres, "\"SYS_ERROR_UNKNOWN\"");
}

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
void funct_gsm_module(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetPPPModemInfo()->model);
}
void funct_gsm_operator(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetPPPModemInfo()->oper);
}
void funct_gsm_imei(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetPPPModemInfo()->imei);
}
void funct_gsm_imsi(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetPPPModemInfo()->imsi);
}
#ifdef CONFIG_WEBGUIAPP_MODEM_AT_ACCESS
void funct_gsm_at(char *argres, int rw)
{
    char resp[1024];
    resp[0] = 0x00;
    ModemSendAT(argres, resp, 1000);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%s", resp);

}
void funct_gsm_at_timeout(char *argres, int rw)
{
    ModemSetATTimeout(atoi(argres));
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"DONE\"");
}
#endif
void funct_gsm_rssi(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%d", PPPModemGetRSSI());
}
#endif

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
void funct_lora_stat(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH,
                 (isLORAConnected()) ? "\"CONNECTED\"" : "\"DISCONNECTED\"");
}
void funct_lora_devid(char *argres, int rw)
{
    uint8_t temp[16];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.DevEui, temp, 8);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", temp);
}

void funct_lora_appid(char *argres, int rw)
{
    uint8_t temp[16];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppEui, temp, 8);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", temp);
}
void funct_lora_appkey(char *argres, int rw)
{
    uint8_t temp[32];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppKey, temp, 16);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", temp);
}
#endif

static void funct_ota_state(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetUpdateStatus());
}

static void funct_ota_start(char *argres, int rw)
{
    StartOTA(true);
}
static void funct_ota_newver(char *argres, int rw)
{
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", GetAvailVersion());
}

//CRON implementation BEGIN
static void funct_cronrecs(char *argres, int rw)
{
    CronRecordsInterface(argres, rw);
}
//CRON implementation END

static void funct_serial_mode(char *argres, int rw)
{

#ifdef CONFIG_WEBGUIAPP_UART_MODE_UART
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"UART\"");
#elif  CONFIG_WEBGUIAPP_UART_MODE_RS485
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"RS485\"");
#endif

}

static void funct_objsinfo(char *argres, int rw)
{
    GetObjectsInfo(argres);
}

const char *EXEC_ERROR[] = {
        "EXECUTED_OK",
        "ERROR_TOO_LONG_COMMAND",
        "ERROR_OBJECT_NOT_PARSED",
        "ERROR_ACTION_NOT_PARSED",
        "ERROR_OBJECT_NOT_FOUND",
        "ERROR_ACTION_NOT_FOUND",
        "ERROR_HANDLER_NOT_IMPLEMENTED",
};

static void funct_exec(char *argres, int rw)
{
    int res = ExecCommand(argres);
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"%s\"", EXEC_ERROR[res]);
}

static void funct_file_list(char *argres, int rw)
{
#if CONFIG_SDCARD_ENABLE
    FileListHandler(argres, rw, "/sdcard/");
#else
    FileListHandler(argres, rw, "/data/");
#endif
}

static void funct_file_block(char *argres, int rw)
{
#if CONFIG_SDCARD_ENABLE
    FileBlockHandler(argres, rw, "/sdcard/");
#else
    FileBlockHandler(argres, rw, "/data/");
#endif
}

#if CONFIG_SDCARD_ENABLE
static void funct_sd_list(char *argres, int rw)
{
    FileListHandler(argres, rw, "/sdcard/");
}

static void funct_sd_block(char *argres, int rw)
{
    FileBlockHandler(argres, rw, "/sdcard/");
}
#endif

static void funct_lat(char *argres, int rw)
{
    if (rw)
    {
        GetSysConf()->sntpClient.lat = atof(argres);
        strcpy(GetSysConf()->sntpClient.latitude, argres);
        
    }
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%f", GetSysConf()->sntpClient.lat);
}

static void funct_lon(char *argres, int rw)
{
    if (rw)
    {
        GetSysConf()->sntpClient.lon = atof(argres);
        strcpy(GetSysConf()->sntpClient.longitude, argres);
    }
    snprintf(argres, VAR_MAX_VALUE_LENGTH, "%f", GetSysConf()->sntpClient.lon);
}

const int hw_rev = CONFIG_BOARD_HARDWARE_REVISION;
const bool VAR_TRUE = true;
const bool VAR_FALSE = false;

const rest_var_t SystemVariables[] =
        {

        { 0, "exec", &funct_exec, VAR_FUNCT, RW, 0, 0 },
                { 0, "time", &funct_time, VAR_FUNCT, R, 0, 0 },
                { 0, "time_set", &funct_time_set, VAR_FUNCT, RW, 0, 0 },
                { 0, "uptime", &funct_uptime, VAR_FUNCT, R, 0, 0 },
                { 0, "free_ram", &funct_fram, VAR_FUNCT, R, 0, 0 },
                { 0, "free_ram_min", &funct_fram_min, VAR_FUNCT, R, 0, 0 },
                { 0, "def_interface", &funct_def_interface, VAR_FUNCT, R, 0, 0 },
                { 0, "fw_rev", &funct_fw_ver, VAR_FUNCT, R, 0, 0 },
                { 0, "idf_rev", &funct_idf_ver, VAR_FUNCT, R, 0, 0 },
                { 0, "build_date", &funct_build_date, VAR_FUNCT, R, 0, 0 },

                { 0, "model_name", CONFIG_DEVICE_MODEL_NAME, VAR_STRING, R, 1, 64 },
                { 0, "hw_rev", ((int*) &hw_rev), VAR_INT, R, 1, 1024 },
                { 0, "hw_opt", CONFIG_BOARD_HARDWARE_OPTION, VAR_STRING, R, 1, 256 },

                { 0, "net_bios_name", &SysConfig.NetBIOSName, VAR_STRING, RW, 3, 31 },
                { 0, "sys_name", &SysConfig.SysName, VAR_STRING, RW, 3, 31 },
                { 0, "sys_pass", &SysConfig.SysPass, VAR_PASS, RW, 3, 31 },
                { 0, "primary_color", CONFIG_WEBGUIAPP_ACCENT_COLOR, VAR_STRING, R, 3, 31 },
                { 0, "dark_theme", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },

                { 0, "ota_url", &SysConfig.OTAURL, VAR_STRING, RW, 3, 128 },
                { 0, "ota_auto_int", &SysConfig.OTAAutoInt, VAR_INT, RW, 0, 65535 },
                { 0, "ota_state", &funct_ota_state, VAR_FUNCT, R, 0, 0 },
                { 0, "ota_start", &funct_ota_start, VAR_FUNCT, R, 0, 0 },
                { 0, "ota_newver", &funct_ota_newver, VAR_FUNCT, R, 0, 0 },

                { 0, "ser_num", &SysConfig.SN, VAR_STRING, R, 10, 10 },
                { 0, "dev_id", &SysConfig.ID, VAR_STRING, R, 8, 8 },
                //{ 0, "ser_num", &funct_ser_num, VAR_FUNCT, R, 0, 0 },
                //{ 0, "dev_id", &funct_dev_id, VAR_FUNCT, R, 0, 0 },
                
                { 0, "color_scheme", &SysConfig.ColorSheme, VAR_INT, RW, 1, 2 },

                { 0, "ota_enab", &SysConfig.Flags1.bIsOTAEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "res_ota_enab", &SysConfig.Flags1.bIsResetOTAEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "led_enab", &SysConfig.Flags1.bIsLedsEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "lora_confirm", &SysConfig.Flags1.bIsLoRaConfirm, VAR_BOOL, RW, 0, 1 },
                { 0, "tcp_confirm", &SysConfig.Flags1.bIsTCPConfirm, VAR_BOOL, RW, 0, 1 },

                { 0, "sntp_timezone", &SysConfig.sntpClient.TimeZone, VAR_INT, RW, 0, 23 },
                { 0, "sntp_serv1", &SysConfig.sntpClient.SntpServerAdr, VAR_STRING, RW, 3, 32 },
                { 0, "sntp_serv2", &SysConfig.sntpClient.SntpServer2Adr, VAR_STRING, RW, 3, 32 },
                { 0, "sntp_serv3", &SysConfig.sntpClient.SntpServer3Adr, VAR_STRING, RW, 3, 32 },
                { 0, "sntp_enab", &SysConfig.sntpClient.Flags1.bIsGlobalEnabled, VAR_BOOL, RW, 0, 1 },

                { 0, "lat", &funct_lat, VAR_FUNCT, RW, 0, 0 },
                { 0, "lon", &funct_lon, VAR_FUNCT, RW, 0, 0 },
                { 0, "latitude", &SysConfig.sntpClient.latitude, VAR_STRING, RW, 0, 16 },
                { 0, "longitude", &SysConfig.sntpClient.longitude, VAR_STRING, RW, 0, 16 },
                
 				{ 0, "cronrecs_enab", &SysConfig.bIsCRONEnabled, VAR_BOOL, RW, 0, 1 },
 				
#if CONFIG_WEBGUIAPP_MQTT_ENABLE
                { 0, "mqtt_1_enab", &SysConfig.mqttStation[0].Flags1.bIsGlobalEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "mqtt_1_serv", &SysConfig.mqttStation[0].ServerAddr, VAR_STRING, RW, 3, 63 },
                { 0, "mqtt_1_port", &SysConfig.mqttStation[0].ServerPort, VAR_INT, RW, 1, 65534 },
                { 0, "mqtt_1_syst", &SysConfig.mqttStation[0].SystemName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_1_group", &SysConfig.mqttStation[0].GroupName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_1_clid", &SysConfig.mqttStation[0].ClientID, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_1_uname", &SysConfig.mqttStation[0].UserName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_1_pass", &SysConfig.mqttStation[0].UserPass, VAR_PASS, RW, 3, 31 },
                { 0, "mqtt_1_stat", &funct_mqtt_1_stat, VAR_FUNCT, R, 0, 0 },
                { 0, "mqtt_1_test", &funct_mqtt_1_test, VAR_FUNCT, RW, 0, 0 },
                { 0, "mqtt_1_hartenab", &SysConfig.mqttStation[0].Flags1.bIsHeartbeatEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "mqtt_1_hartint", &SysConfig.mqttStation[0].HeartbeatInterval, VAR_INT, RW, 1, 65534 },

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
                { 0, "mqtt_2_enab", &SysConfig.mqttStation[1].Flags1.bIsGlobalEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "mqtt_2_serv", &SysConfig.mqttStation[1].ServerAddr, VAR_STRING, RW, 3, 63 },
                { 0, "mqtt_2_port", &SysConfig.mqttStation[1].ServerPort, VAR_INT, RW, 1, 65534 },
                { 0, "mqtt_2_syst", &SysConfig.mqttStation[1].SystemName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_2_group", &SysConfig.mqttStation[1].GroupName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_2_clid", &SysConfig.mqttStation[1].ClientID, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_2_uname", &SysConfig.mqttStation[1].UserName, VAR_STRING, RW, 3, 31 },
                { 0, "mqtt_2_pass", &SysConfig.mqttStation[1].UserPass, VAR_PASS, RW, 3, 31 },
                { 0, "mqtt_2_stat", &funct_mqtt_2_stat, VAR_FUNCT, R, 0, 0 },
                { 0, "mqtt_2_test", &funct_mqtt_2_test, VAR_FUNCT, RW, 0, 0 },
                { 0, "mqtt_2_hartenab", &SysConfig.mqttStation[1].Flags1.bIsHeartbeatEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "mqtt_2_hartint", &SysConfig.mqttStation[1].HeartbeatInterval, VAR_INT, RW, 1, 65534 },

#endif
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
                { 0, "eth_enab", &SysConfig.ethSettings.Flags1.bIsETHEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "eth_isdhcp", &SysConfig.ethSettings.Flags1.bIsDHCPEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "eth_ip", &SysConfig.ethSettings.IPAddr, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_mask", &SysConfig.ethSettings.Mask, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_gw", &SysConfig.ethSettings.Gateway, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_dns1", &SysConfig.ethSettings.DNSAddr1, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_dns2", &SysConfig.ethSettings.DNSAddr2, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_dns3", &SysConfig.ethSettings.DNSAddr3, VAR_IPADDR, RW, 0, 0 },
                { 0, "eth_stat", &funct_eth_stat, VAR_FUNCT, R, 0, 0 },
                { 0, "eth_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },
                { 0, "eth_mac", &funct_eth_mac, VAR_FUNCT, R, 0, 0 },
                #else
                { 0, "eth_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
                #endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
                { 0, "wifi_mode", &SysConfig.wifiSettings.WiFiMode, VAR_INT, RW, 1, 3 },
                { 0, "wifi_sta_ip", &SysConfig.wifiSettings.InfIPAddr, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_sta_mask", &SysConfig.wifiSettings.InfMask, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_sta_gw", &SysConfig.wifiSettings.InfGateway, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_ap_ip", &SysConfig.wifiSettings.ApIPAddr, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_dns1", &SysConfig.wifiSettings.DNSAddr1, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_dns2", &SysConfig.wifiSettings.DNSAddr2, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_dns3", &SysConfig.wifiSettings.DNSAddr3, VAR_IPADDR, RW, 0, 0 },
                { 0, "wifi_sta_ssid", &SysConfig.wifiSettings.InfSSID, VAR_STRING, RW, 3, 31 },
                { 0, "wifi_sta_key", &SysConfig.wifiSettings.InfSecurityKey, VAR_PASS, RW, 8, 31 },
                { 0, "wifi_ap_ssid", &SysConfig.wifiSettings.ApSSID, VAR_STRING, RW, 3, 31 },
                { 0, "wifi_ap_key", &SysConfig.wifiSettings.ApSecurityKey, VAR_PASS, RW, 8, 31 },

                { 0, "wifi_enab", &SysConfig.wifiSettings.Flags1.bIsWiFiEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "wifi_isdhcp", &SysConfig.wifiSettings.Flags1.bIsDHCPEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "wifi_power", &SysConfig.wifiSettings.MaxPower, VAR_INT, RW, 0, 80 },
                { 0, "wifi_disab_time", &SysConfig.wifiSettings.AP_disab_time, VAR_INT, RW, 0, 60 },
                { 0, "wifi_sta_mac", &funct_wifi_sta_mac, VAR_FUNCT, R, 0, 0 },
                { 0, "wifi_ap_mac", &funct_wifi_ap_mac, VAR_FUNCT, R, 0, 0 },
                { 0, "wifi_stat", &funct_wifi_stat, VAR_FUNCT, R, 0, 0 },
                { 0, "wifi_scan", &funct_wifiscan, VAR_FUNCT, R, 0, 0 },
                { 0, "wifi_scan_res", &funct_wifiscanres, VAR_FUNCT, R, 0, 0 },
                { 0, "wifi_level", &funct_wifi_level, VAR_FUNCT, R, 0, 0 },

#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
                { 0, "gsm_enab", &SysConfig.gsmSettings.Flags1.bIsGSMEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "gsm_apn", &SysConfig.gsmSettings.APN, VAR_STRING, RW, 3, 31 },
                { 0, "gsm_apn_login", &SysConfig.gsmSettings.login, VAR_STRING, RW, 3, 31 },
                { 0, "gsm_apn_password", &SysConfig.gsmSettings.password, VAR_STRING, RW, 3, 31 },
                { 0, "gsm_module", &funct_gsm_module, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_operator", &funct_gsm_operator, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_imei", &funct_gsm_imei, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_imsi", &funct_gsm_imsi, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_ip", &SysConfig.gsmSettings.IPAddr, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_mask", &SysConfig.gsmSettings.Mask, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_gw", &SysConfig.gsmSettings.Gateway, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_dns1", &SysConfig.gsmSettings.DNSAddr1, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_dns2", &SysConfig.gsmSettings.DNSAddr2, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_dns3", &SysConfig.gsmSettings.DNSAddr3, VAR_IPADDR, RW, 0, 0 },
                { 0, "gsm_stat", &funct_gsm_stat, VAR_FUNCT, R, 0, 0 },
                #ifdef CONFIG_WEBGUIAPP_MODEM_AT_ACCESS
                { 0, "gsm_at_timeout", &funct_gsm_at_timeout, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_at", &funct_gsm_at, VAR_FUNCT, R, 0, 0 },
#endif
                { 0, "gsm_rssi", &funct_gsm_rssi, VAR_FUNCT, R, 0, 0 },
                { 0, "gsm_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },
                #else
                { 0, "gsm_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
                #endif

#ifdef CONFIG_WEBGUIAPP_UART_TRANSPORT_ENABLE
                { 0, "serial_enab", &SysConfig.serialSettings.Flags.IsSerialEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "serial_bridge", &SysConfig.serialSettings.Flags.IsBridgeEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "serial_mode", &funct_serial_mode, VAR_FUNCT, R, 1, 2 },
                { 0, "serial_baud", &SysConfig.serialSettings.BaudRate, VAR_INT, RW, 1200, 8192000 },

                { 0, "serial_bits", &SysConfig.serialSettings.DataBits, VAR_INT, RW, 0, 3 },
                { 0, "serial_parity", &SysConfig.serialSettings.Parity, VAR_INT, RW, 0, 3 },
                { 0, "serial_stop", &SysConfig.serialSettings.StopBits, VAR_INT, RW, 1, 3 },

                { 0, "serial_break", &SysConfig.serialSettings.InputBrake, VAR_INT, RW, 1, 50 },
                { 0, "serial_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },
                #else
                { 0, "serial_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
                #endif

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
                { 0, "lora_enab", &SysConfig.lorawanSettings.Flags1.bIsLoRaWANEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "lora_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },
                { 0, "lora_devid", &funct_lora_devid, VAR_FUNCT, RW, 0, 0 },
                { 0, "lora_appid", &funct_lora_appid, VAR_FUNCT, RW, 0, 0 },
                { 0, "lora_appkey", &funct_lora_appkey, VAR_FUNCT, R, 0, 0 },


#else
                { 0, "lora_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
                #endif

#ifdef CONFIG_WEBGUIAPP_MBTCP_ENABLED
                { 0, "mbtcp_enab", &SysConfig.modbusSettings.IsModbusTCPEnabled, VAR_BOOL, RW, 0, 1 },
                { 0, "mbtcp_port", &SysConfig.modbusSettings.ModbusTCPPort, VAR_INT, RW, 1, 65534 },
                { 0, "mbtcp_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 },
#else
                { 0, "mbtcp_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
                #endif
                { 0, "cronrecs", &funct_cronrecs, VAR_FUNCT, RW, 0, 0 },
                { 0, "objsinfo", &funct_objsinfo, VAR_FUNCT, R, 0, 0 },

                { 0, "file_list", &funct_file_list, VAR_FUNCT, R, 0, 0 },
                { 0, "file_block", &funct_file_block, VAR_FUNCT, R, 0, 0 },
                #if CONFIG_SDCARD_ENABLE
                { 0, "sd_list", &funct_sd_list, VAR_FUNCT, R, 0, 0 },
                { 0, "sd_block", &funct_sd_block, VAR_FUNCT, R, 0, 0 },
                { 0, "sd_visible", (bool*) (&VAR_TRUE), VAR_BOOL, R, 0, 1 }
#else
                { 0, "sd_visible", (bool*) (&VAR_FALSE), VAR_BOOL, R, 0, 1 },
        #endif

        };

esp_err_t SetConfVar(char *name, char *val, rest_var_types *tp)
{
    rest_var_t *V = NULL;
    //Search for system variables
    for (int i = 0; i < sizeof(SystemVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(SystemVariables[i].alias, name))
        {
            V = (rest_var_t*) (&SystemVariables[i]);
            break;
        }
    }
    //Search for user variables
    if (AppVars)
    {
        for (int i = 0; i < AppVarsSize; ++i)
        {
            if (!strcmp(AppVars[i].alias, name))
            {
                V = (rest_var_t*) (&AppVars[i]);
                break;
            }
        }
    }

    if (!V)
        return ESP_ERR_NOT_FOUND;
    if (V->varattr == R)
        return ESP_OK;
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
        case VAR_CHAR:
            constr = atoi(val);
            if (constr < V->minlen || constr > V->maxlen)
                return ESP_ERR_INVALID_ARG;
            *((uint8_t*) V->ref) = constr;
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
        case VAR_PASS:
            if (val[0] != '*')
            {
                constr = strlen(val);
                if (constr < V->minlen || constr > V->maxlen)
                    return ESP_ERR_INVALID_ARG;
                strcpy(V->ref, val);
            }
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
    for (int i = 0; i < sizeof(SystemVariables) / sizeof(rest_var_t); ++i)
    {
        if (!strcmp(SystemVariables[i].alias, name))
        {
            V = (rest_var_t*) (&SystemVariables[i]);
            break;
        }
    }
    //Search for user variables
    if (AppVars)
    {
        for (int i = 0; i < AppVarsSize; ++i)
        {
            if (!strcmp(AppVars[i].alias, name))
            {
                V = (rest_var_t*) (&AppVars[i]);
                break;
            }
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
        case VAR_CHAR:
            itoa(*((uint8_t*) V->ref), val, 10);
        break;
        case VAR_STRING:
            strcpy(val, (char*) V->ref);
        break;
        case VAR_PASS:
            strcpy(val, "******");
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

