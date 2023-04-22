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
 *  	 \file HTTPPrintSystem.c
 *    \version 1.0
 * 		 \date 2022-08-14
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "HTTPServer.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "NetTransport.h"
#include "esp_ota_ops.h"
#include "romfs.h"
#include "esp_idf_version.h"
#include "jWrite.h"
#include "UserCallbacks.h"

static const char *TAG = "HTTPServerPrint";

extern espfs_fs_t *fs;

typedef enum
{
    IP,
    NETMASK,
    GW
} IP_PRINT_TYPE;

//Pointer to extend user implemented print handler
static int (*HTTPPrintCust)(httpd_req_t *req, char *buf, char *var, int arg);

void regHTTPPrintCustom(int (*print_handler)(httpd_req_t *req, char *buf, char *var, int arg))
{
    HTTPPrintCust = print_handler;
}

static void PrintInterfaceState(char *VarData, void *arg, esp_netif_t *netif)
{
    if (netif != NULL && esp_netif_is_netif_up(netif))
        snprintf(VarData, MAX_DYNVAR_LENGTH, "CONNECTED");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "DISCONNECTED");
}

static void PrintIPFromInterface(char *VarData, void *arg, esp_netif_t *netif, IP_PRINT_TYPE tp)
{
    char buf[16];
    esp_netif_ip_info_t ip_info;
    if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        switch (tp)
        {
            case IP:
                snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", esp_ip4addr_ntoa(&ip_info.ip, buf, 16));
            break;
            case NETMASK:
                snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", esp_ip4addr_ntoa(&ip_info.netmask, buf, 16));
            break;
            case GW:
                snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", esp_ip4addr_ntoa(&ip_info.gw, buf, 16));
            break;
        }
    }
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "-");
}

static void PrintDNSFromInterface(char *VarData, void *arg, esp_netif_t *netif, esp_netif_dns_type_t type)
{
    char buf[16];
    esp_netif_dns_info_t dns_info;
    if (netif != NULL && esp_netif_get_dns_info(netif, type, &dns_info) == ESP_OK)
    {

        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", esp_ip4addr_ntoa((esp_ip4_addr_t*) (&dns_info.ip), buf, 16));
    }
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "-");
}

static void PrintMACFromInterface(char *VarData, void *arg, esp_netif_t *netif)
{
    uint8_t mac_addr[6] = { 0 };
    esp_netif_get_mac(netif, mac_addr);
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%02x-%02x-%02x-%02x-%02x-%02x",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);
}

static void PrintCheckbox(char *VarData, void *arg, bool checked)
{
    if (checked)
        snprintf(VarData, MAX_DYNVAR_LENGTH, "checked");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
}

static void HTTPPrint_name(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", CONFIG_DEVICE_MODEL_NAME);
}

static void HTTPPrint_time(char *VarData, void *arg)
{
    time_t now;
    time(&now);
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", (int) now);
}
static void HTTPPrint_uptime(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", (int)GetUpTime());
}

static void HTTPPrint_status_fail(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "none");
}

static void HTTPPrint_dname(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->NetBIOSName);
}

static void HTTPPrint_login(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->SysName);
}

static void HTTPPrint_pass(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "******");
}

static void HTTPPrint_ota(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->Flags1.bIsOTAEnabled);
}
static void HTTPPrint_otarst(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->Flags1.bIsResetOTAEnabled);
}

static void HTTPPrint_otaint(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->OTAAutoInt);
}

static void HTTPPrint_serial(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->ID);
}
static void HTTPPrint_serial10(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->SN);
}

static void HTTPPrint_fver(char *VarData, void *arg)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
    {
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", cur_app_info.version);
    }
}

static void HTTPPrint_fverav(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetAvailVersion());
}

static void HTTPPrint_updstat(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetUpdateStatus());
}

static void HTTPPrint_idfver(char *VarData, void *arg)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
    {
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", cur_app_info.idf_ver);
    }
}
static void HTTPPrint_builddate(char *VarData, void *arg)
{
    esp_app_desc_t cur_app_info;
    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
    {
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s %s", cur_app_info.date, cur_app_info.time);
    }
}

static void HTTPPrint_otaurl(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->OTAURL);
}

static void HTTPPrint_tshift(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "7200");
}

static void HTTPPrint_tz(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->sntpClient.TimeZone);
}

static void HTTPPrint_wlev(char *VarData, void *arg)
{
    wifi_ap_record_t wifi;
    if (esp_wifi_sta_get_ap_info(&wifi) == ESP_OK)
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%ddBm", wifi.rssi);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "--");
}

static void HTTPPrint_defadp(char *VarData, void *arg)
{
    GetDefaultNetIFName(VarData);
}

#if CONFIG_WEBGUIAPP_WIFI_ENABLE

static void HTTPPrint_wfen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->wifiSettings.Flags1.bIsWiFiEnabled);
}
static void HTTPPrint_wfstat(char *VarData, void *arg)
{
    if (GetSysConf()->wifiSettings.WiFiMode == WIFI_MODE_AP)
        PrintInterfaceState(VarData, arg, GetAPNetifAdapter());
    else
        PrintInterfaceState(VarData, arg, GetSTANetifAdapter());
}
static void HTTPPrint_cln(char *VarData, void *arg)
{
    //PrintCheckbox(VarData, arg, !GetSysConf()->wifiSettings.Flags1.bIsAP);
}
static void HTTPPrint_apn(char *VarData, void *arg)
{
    //PrintCheckbox(VarData, arg, GetSysConf()->wifiSettings.Flags1.bIsAP);
}

static void HTTPPrint_wfmode(char *VarData, void *arg)
{
    if ((*(uint8_t*) arg) == GetSysConf()->wifiSettings.WiFiMode)
        snprintf(VarData, MAX_DYNVAR_LENGTH, "selected");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
}

static void HTTPPrint_ssidap(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->wifiSettings.ApSSID);
}

static void HTTPPrint_wkeyap(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "********");
}
/*AP IP*/
static void HTTPPrint_ipap(char *VarData, void *arg)
{
    if (GetAPNetifAdapter() && esp_netif_is_netif_up(GetAPNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetAPNetifAdapter(), IP);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->wifiSettings.ApIPAddr));
}

static void HTTPPrint_ssid(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->wifiSettings.InfSSID);
}

static void HTTPPrint_wkey(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "********");
}

static void HTTPPrint_cbdh(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->wifiSettings.Flags1.bIsDHCPEnabled);
}
/*STA IP*/
static void HTTPPrint_ip(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetSTANetifAdapter(), IP);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->wifiSettings.InfIPAddr));
}
/*STA NETMASK*/
static void HTTPPrint_msk(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetSTANetifAdapter(), NETMASK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->wifiSettings.InfMask));
}
/*STA GATEWAY*/
static void HTTPPrint_gate(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetSTANetifAdapter(), GW);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->wifiSettings.InfGateway));
}
/*Current DNS*/
static void HTTPPrint_dns(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetSTANetifAdapter(), ESP_NETIF_DNS_MAIN);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}
static void HTTPPrint_dns2(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetSTANetifAdapter(), ESP_NETIF_DNS_BACKUP);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}
static void HTTPPrint_dns3(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetSTANetifAdapter(), ESP_NETIF_DNS_FALLBACK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}

static void HTTPPrint_macadr(char *VarData, void *arg)
{
    PrintMACFromInterface(VarData, arg, GetSTANetifAdapter());
}

static void HTTPPrint_apmacadr(char *VarData, void *arg)
{
    PrintMACFromInterface(VarData, arg, GetAPNetifAdapter());
}
static void HTTPPrint_wifisc(char *VarData, void *arg)
{
    wifi_ap_record_t *R = GetWiFiAPRecord(*(uint8_t*) (arg));
    if (!R)
        return;
    snprintf(VarData, MAX_DYNVAR_LENGTH, "{\"ssid\":\"%s\",\"rssi\":%i,\"ch\":%d}", R->ssid, R->rssi,
             R->primary);
}
static void HTTPPrint_wifipwr(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", (unsigned int)(GetSysConf()->wifiSettings.MaxPower / 4));
}
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
static void HTTPPrint_ethen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->ethSettings.Flags1.bIsETHEnabled);
}
static void HTTPPrint_ecbdh(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->ethSettings.Flags1.bIsDHCPEnabled);
}
static void HTTPPrint_ethstat(char *VarData, void *arg)
{
    PrintInterfaceState(VarData, arg, GetETHNetifAdapter());
}
/*Etherbox IP*/
static void HTTPPrint_eip(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetETHNetifAdapter(), IP);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->ethSettings.IPAddr));
}
/*Etherbox NETMASK*/
static void HTTPPrint_emsk(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetETHNetifAdapter(), NETMASK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->ethSettings.Mask));
}
/*Ethernet GATEWAY*/
static void HTTPPrint_egate(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetETHNetifAdapter(), GW);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa((const ip4_addr_t*)&GetSysConf()->ethSettings.Gateway));
}
/*Current DNS*/
static void HTTPPrint_edns(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetETHNetifAdapter(), ESP_NETIF_DNS_MAIN);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}
static void HTTPPrint_bkedns(char *VarData, void *arg)
{

    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetETHNetifAdapter(), ESP_NETIF_DNS_BACKUP);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}
static void HTTPPrint_fledns(char *VarData, void *arg)
{

    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintDNSFromInterface(VarData, arg, GetETHNetifAdapter(), ESP_NETIF_DNS_FALLBACK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "0.0.0.0");
}

static void HTTPPrint_emacadr(char *VarData, void *arg)
{
    PrintMACFromInterface(VarData, arg, GetETHNetifAdapter());
}

#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
/*GSM MODEM*/
void HTTPPrint_gsmstat(char *VarData, void *arg)
{
    PrintInterfaceState(VarData, arg, GetPPPNetifAdapter());
}
void HTTPPrint_gsmen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled);
}

void HTTPPrint_gsmmod(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, GetPPPModemInfo()->model);
}
void HTTPPrint_gsmopr(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, GetPPPModemInfo()->oper);
}
void HTTPPrint_gimei(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, GetPPPModemInfo()->imei);
}
void HTTPPrint_gimsi(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, GetPPPModemInfo()->imsi);
}

/*PPP IP*/
void HTTPPrint_gsmip(char *VarData, void *arg)
{
    PrintIPFromInterface(VarData, arg, GetPPPNetifAdapter(), IP);
}
/*PPP NETMASK*/
void HTTPPrint_gsmmsk(char *VarData, void *arg)
{
    PrintIPFromInterface(VarData, arg, GetPPPNetifAdapter(), NETMASK);
}
/*PPP GATEWAY*/
void HTTPPrint_gsmgate(char *VarData, void *arg)
{
    PrintIPFromInterface(VarData, arg, GetPPPNetifAdapter(), GW);
}
/*Current DNS*/
void HTTPPrint_gsmdns(char *VarData, void *arg)
{
    PrintDNSFromInterface(VarData, arg, GetPPPNetifAdapter(), ESP_NETIF_DNS_MAIN);
}
void HTTPPrint_bkgsmdns(char *VarData, void *arg)
{
    PrintDNSFromInterface(VarData, arg, GetPPPNetifAdapter(), ESP_NETIF_DNS_BACKUP);
}
void HTTPPrint_flgsmdns(char *VarData, void *arg)
{
    PrintDNSFromInterface(VarData, arg, GetPPPNetifAdapter(), ESP_NETIF_DNS_FALLBACK);
}

void HTTPPrint_gsmmac(char *VarData, void *arg)
{
    PrintMACFromInterface(VarData, arg, GetPPPNetifAdapter());
}
#endif

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
/*LORAWAN settings*/
void HTTPPrint_lren(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->lorawanSettings.Flags1.bIsLoRaWANEnabled);
}
void HTTPPrint_lrstat(char *VarData, void *arg)
{
    if (isLORAConnected())
        snprintf(VarData, MAX_DYNVAR_LENGTH, "CONNECTED");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "DISCONNECTED");
}
void HTTPPrint_lrdvid(char *VarData, void *arg)
{
    uint8_t temp[16];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.DevEui, temp, 8);
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", temp);
}

void HTTPPrint_lrapid(char *VarData, void *arg)
{
    uint8_t temp[16];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppEui, temp, 8);
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", temp);
}
void HTTPPrint_lrapkey(char *VarData, void *arg)
{
    uint8_t temp[32];
    BytesToStr((unsigned char*) &GetSysConf()->lorawanSettings.AppKey, temp, 16);
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", temp);
}
#endif

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
void HTTPPrint_mqen1(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled);
}
void HTTPPrint_mqurl1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].ServerAddr);
}
void HTTPPrint_mqport1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->mqttStation[0].ServerPort);
}
void HTTPPrint_mqid1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].ClientID);
}
void HTTPPrint_mqsys1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].SystemName);
}
void HTTPPrint_mqgrp1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].GroupName);
}
void HTTPPrint_mqname1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].UserName);
}
void HTTPPrint_mqpass1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "******");
}

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
void HTTPPrint_mqen2(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled);
}
void HTTPPrint_mqurl2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].ServerAddr);
}
void HTTPPrint_mqport2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->mqttStation[1].ServerPort);
}
void HTTPPrint_mqid2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].ClientID);
}
void HTTPPrint_mqsys2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].SystemName);
}
void HTTPPrint_mqgrp2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].GroupName);
}
void HTTPPrint_mqname2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].UserName);
}
void HTTPPrint_mqpass2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "******");
}
#endif
#endif

/*SNTP*/
void HTTPPrint_sntpen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->sntpClient.Flags1.bIsGlobalEnabled);
}
void HTTPPrint_tmsrv(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->sntpClient.SntpServerAdr);
}

static void HTTPPrint_freeram(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", (int) esp_get_free_heap_size());
}
static void HTTPPrint_minram(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", (int) esp_get_minimum_free_heap_size());
}
static void HTTPPrint_mqtt1st(char *VarData, void *arg)
{
    if (GetMQTT1Connected())
        snprintf(VarData, MAX_DYNVAR_LENGTH, "CONNECTED");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "DISCONNECTED");
}
static void HTTPPrint_mqtt2st(char *VarData, void *arg)
{
    if (GetMQTT2Connected())
        snprintf(VarData, MAX_DYNVAR_LENGTH, "CONNECTED");
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "DISCONNECTED");
}

/* Pass build configuration to web interface*/
static void HTTPPrint_hide_gprs(char *VarData, void *arg)
{
#if CONFIG_WEBGUIAPP_GPRS_ENABLE
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
#else
    snprintf(VarData, MAX_DYNVAR_LENGTH, "hide");
#endif
}

static void HTTPPrint_hide_mqtt1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
}

static void HTTPPrint_hide_mqtt2(char *VarData, void *arg)
{
#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
#else
    snprintf(VarData, MAX_DYNVAR_LENGTH, "hide");
#endif
}

static void HTTPPrint_hide_lora(char *VarData, void *arg)
{
#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
#else
    snprintf(VarData, MAX_DYNVAR_LENGTH, "hide");
#endif
}

static void HTTPPrint_hide_eth(char *VarData, void *arg)
{
#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
#else
    snprintf(VarData, MAX_DYNVAR_LENGTH, "hide");
#endif
}

static void HTTPPrint_hide_wifi(char *VarData, void *arg)
{
#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    snprintf(VarData, MAX_DYNVAR_LENGTH, " ");
#else
    snprintf(VarData, MAX_DYNVAR_LENGTH, "hide");
#endif
}

static void HTTPPrint_testvariable(char *VarData, void *arg)
{
    static int counter = 1;
    snprintf(VarData, MAX_DYNVAR_LENGTH, "[Long extended dynamic variable number %d]", counter++);
}

//Default string if not found handler
static void HTTPPrint_DEF(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "#DEF");
}

dyn_var_handler_t HANDLERS_ARRAY[] = {
        /*Ststem settings*/
        { "name", sizeof("name") - 1, &HTTPPrint_name },
        { "dname", sizeof("dname") - 1, &HTTPPrint_dname },
        { "login", sizeof("login") - 1, &HTTPPrint_login },
        { "pass", sizeof("pass") - 1, &HTTPPrint_pass },
        { "ota", sizeof("ota") - 1, &HTTPPrint_ota },
        { "otarst", sizeof("otarst") - 1, &HTTPPrint_otarst },
        { "otaint", sizeof("otaint") - 1, &HTTPPrint_otaint },
        { "fver", sizeof("fver") - 1, &HTTPPrint_fver },
        { "fverav", sizeof("fverav") - 1, &HTTPPrint_fverav },
        { "updstat", sizeof("updstat") - 1, &HTTPPrint_updstat },

        { "idfver", sizeof("idfver") - 1, &HTTPPrint_idfver },
        { "builddate", sizeof("builddate") - 1, &HTTPPrint_builddate },
        { "serial", sizeof("serial") - 1, &HTTPPrint_serial },
        { "serial10", sizeof("serial10") - 1, &HTTPPrint_serial10 },
        { "otaurl", sizeof("otaurl") - 1, &HTTPPrint_otaurl },

        { "time", sizeof("time") - 1, &HTTPPrint_time },
        { "uptime", sizeof("uptime") - 1, &HTTPPrint_uptime },
        { "tshift", sizeof("tshift") - 1, &HTTPPrint_tshift },
        { "tz", sizeof("tz") - 1, &HTTPPrint_tz },

        { "defadp", sizeof("defadp") - 1, &HTTPPrint_defadp },
        { "wlev", sizeof("wlev") - 1, &HTTPPrint_wlev },

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
        /*WiFi network*/
        { "wfen", sizeof("wfen") - 1, &HTTPPrint_wfen },
        { "wfstat", sizeof("wfstat") - 1, &HTTPPrint_wfstat },
        { "cln", sizeof("cln") - 1, &HTTPPrint_cln },
        { "apn", sizeof("apn") - 1, &HTTPPrint_apn },
        { "wfmode", sizeof("wfmode") - 1, &HTTPPrint_wfmode },
        { "ssidap", sizeof("ssidap") - 1, &HTTPPrint_ssidap },
        { "wkeyap", sizeof("wkeyap") - 1, &HTTPPrint_wkeyap },
        { "ipap", sizeof("ipap") - 1, &HTTPPrint_ipap },
        { "ssid", sizeof("ssid") - 1, &HTTPPrint_ssid },
        { "wkey", sizeof("wkey") - 1, &HTTPPrint_wkey },
        { "cbdh", sizeof("cbdh") - 1, &HTTPPrint_cbdh },
        { "ip", sizeof("ip") - 1, &HTTPPrint_ip },
        { "msk", sizeof("msk") - 1, &HTTPPrint_msk },
        { "gate", sizeof("gate") - 1, &HTTPPrint_gate },
        { "dns", sizeof("dns") - 1, &HTTPPrint_dns },
        { "dns2", sizeof("dns2") - 1, &HTTPPrint_dns2 },
        { "dns3", sizeof("dns3") - 1, &HTTPPrint_dns3 },
        { "macadr", sizeof("macadr") - 1, &HTTPPrint_macadr },
        { "apmacadr", sizeof("apmacadr") - 1, &HTTPPrint_apmacadr },
        { "wifisc", sizeof("wifisc") - 1, &HTTPPrint_wifisc },
        { "wifipwr", sizeof("wifipwr") - 1, &HTTPPrint_wifipwr },
        #endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
        /*ETHERNET network*/
        { "ethen", sizeof("ethen") - 1, &HTTPPrint_ethen },
        { "ecbdh", sizeof("ecbdh") - 1, &HTTPPrint_ecbdh },
        { "ethstat", sizeof("ethstat") - 1, &HTTPPrint_ethstat },
        { "eip", sizeof("eip") - 1, &HTTPPrint_eip },
        { "emsk", sizeof("emsk") - 1, &HTTPPrint_emsk },
        { "egate", sizeof("egate") - 1, &HTTPPrint_egate },
        { "edns", sizeof("edns") - 1, &HTTPPrint_edns },
        { "bkedns", sizeof("bkedns") - 1, &HTTPPrint_bkedns },
        { "fledns", sizeof("fledns") - 1, &HTTPPrint_fledns },
        { "emacadr", sizeof("emacadr") - 1, &HTTPPrint_emacadr },
        #endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
        /*GSM modem*/
        { "gsmen", sizeof("gsmen") - 1, &HTTPPrint_gsmen },
        { "gsmstat", sizeof("gsmstat") - 1, &HTTPPrint_gsmstat },
        { "gsmmod", sizeof("gsmmod") - 1, &HTTPPrint_gsmmod },
        { "gsmopr", sizeof("gsmopr") - 1, &HTTPPrint_gsmopr },
        { "gimei", sizeof("gimei") - 1, &HTTPPrint_gimei },
        { "gimsi", sizeof("gimsi") - 1, &HTTPPrint_gimsi },
        { "gsmip", sizeof("gsmip") - 1, &HTTPPrint_gsmip },
        { "gsmmsk", sizeof("gsmmsk") - 1, &HTTPPrint_gsmmsk },
        { "gsmgate", sizeof("gsmgate") - 1, &HTTPPrint_gsmgate },
        { "gsmdns", sizeof("gsmdns") - 1, &HTTPPrint_gsmdns },
        { "bkgsmdns", sizeof("bkgsmdns") - 1, &HTTPPrint_bkgsmdns },
        { "flgsmdns", sizeof("flgsmdns") - 1, &HTTPPrint_flgsmdns },
        { "gsmmac", sizeof("gsmmac") - 1, &HTTPPrint_gsmmac },
        #endif

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
        /*LORAWAN settings*/
        { "lren", sizeof("lren") - 1, &HTTPPrint_lren },
        { "lrstat", sizeof("lrstat") - 1, &HTTPPrint_lrstat },
        { "lrdvid", sizeof("lrdvid") - 1, &HTTPPrint_lrdvid },
        { "lrapid", sizeof("lrapid") - 1, &HTTPPrint_lrapid },
        { "lrapkey", sizeof("lrapkey") - 1, &HTTPPrint_lrapkey },
#endif
#if CONFIG_WEBGUIAPP_MQTT_ENABLE
        /*MQTT*/
        { "mqen1", sizeof("mqen1") - 1, &HTTPPrint_mqen1 },
        { "mqurl1", sizeof("mqurl1") - 1, &HTTPPrint_mqurl1 },
        { "mqport1", sizeof("mqport1") - 1, &HTTPPrint_mqport1 },
        { "mqsys1", sizeof("mqsys1") - 1, &HTTPPrint_mqsys1 },
        { "mqgrp1", sizeof("mqgrp1") - 1, &HTTPPrint_mqgrp1 },
        { "mqid1", sizeof("mqid1") - 1, &HTTPPrint_mqid1 },
        { "mqname1", sizeof("mqname1") - 1, &HTTPPrint_mqname1 },
        { "mqpass1", sizeof("mqpass1") - 1, &HTTPPrint_mqpass1 },

        #if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
        { "mqen2", sizeof("mqen2") - 1, &HTTPPrint_mqen2 },
        { "mqurl2", sizeof("mqurl2") - 1, &HTTPPrint_mqurl2 },
        { "mqport2", sizeof("mqport2") - 1, &HTTPPrint_mqport2 },
        { "mqid2", sizeof("mqid2") - 1, &HTTPPrint_mqid2 },
        { "mqsys2", sizeof("mqsys2") - 1, &HTTPPrint_mqsys2 },
        { "mqgrp2", sizeof("mqgrp2") - 1, &HTTPPrint_mqgrp2 },
        { "mqname2", sizeof("mqname2") - 1, &HTTPPrint_mqname2 },
        { "mqpass2", sizeof("mqpass2") - 1, &HTTPPrint_mqpass2 },

        #endif
#endif
        /*SNTP*/
        { "sntpen", sizeof("sntpen") - 1, &HTTPPrint_sntpen },
        { "tmsrv", sizeof("tmsrv") - 1, &HTTPPrint_tmsrv },

        { "freeram", sizeof("freeram") - 1, &HTTPPrint_freeram },
        { "minram", sizeof("minram") - 1, &HTTPPrint_minram },
        { "mqtt1st", sizeof("mqtt1st") - 1, &HTTPPrint_mqtt1st },
        { "mqtt2st", sizeof("mqtt2st") - 1, &HTTPPrint_mqtt2st },

        /*ERROR report*/
        { "status_fail", sizeof("status_fail") - 1, &HTTPPrint_status_fail },

        { "hide_eth", sizeof("hide_eth") - 1, &HTTPPrint_hide_eth },
        { "hide_wifi", sizeof("hide_wifi") - 1, &HTTPPrint_hide_wifi },
        { "hide_lora", sizeof("hide_lora") - 1, &HTTPPrint_hide_lora },
        { "hide_gprs", sizeof("hide_gprs") - 1, &HTTPPrint_hide_gprs },
        { "hide_mqtt1", sizeof("hide_mqtt1") - 1, &HTTPPrint_hide_mqtt1 },
        { "hide_mqtt2", sizeof("hide_mqtt2") - 1, &HTTPPrint_hide_mqtt2 },

        { "testvariable", sizeof("testvariable") - 1, &HTTPPrint_testvariable },

};

int HTTPPrint(httpd_req_t *req, char *buf, char *var)
{
    char VarData[MAX_DYNVAR_LENGTH];
    const char incPat[] = "inc:";
    const int incPatLen = sizeof(incPat) - 1;
    if (!memcmp(var, incPat, incPatLen))
    {
        const char rootFS[] = "/";
        char filename[32];
        filename[0] = 0x00;
        var += incPatLen;
        strcat(filename, rootFS);
        strcat(filename, var);
        espfs_file_t *file = espfs_fopen(fs, filename);
        struct espfs_stat_t stat;
        if (file)
        {
            espfs_fstat(file, &stat);
            int readBytes = espfs_fread(file, buf, stat.size);
            espfs_fclose(file);
            return readBytes;
        }
    }

    bool fnd = false;
    char *p2 = var + strlen(var) - 1; //last var symbol
    int arg = 0;
//searching for tag in handles array
    for (int i = 0; i < (sizeof(HANDLERS_ARRAY) / sizeof(HANDLERS_ARRAY[0])); ++i)
    {
        if (*p2 == ')')
        { //found close brace
            char *p1 = p2;
            while ((*p1 != '(') && (p1 > var))
                --p1;
            if (*p1 == '(')
            { //found open brace
                *p1 = 0x00; //trim variable to name part
                ++p1; //to begin of argument
                *p2 = 0x00; //set end of argument
                arg = atoi(p1);
            }
        }
        if (strcmp(var, HANDLERS_ARRAY[i].tag) == 0
                && HANDLERS_ARRAY[i].HandlerRoutine != NULL)
        {
            HANDLERS_ARRAY[i].HandlerRoutine(VarData, (void*) &arg);
            fnd = true;
            break;
        }
    }
    if (!fnd)
    {
        if (HTTPPrintCust != NULL)
            return HTTPPrintCust(req, buf, var, arg);
        else
            HTTPPrint_DEF(VarData, NULL);

    }
    int dLen = strlen(VarData);
    memcpy(buf, VarData, dLen);
    return dLen;

}


void GenerateSystemSettingsJSONFile(void)
{
    char *buf = malloc(2048);
    if(!buf) return;

    jwOpen(buf, 2048, JW_OBJECT, JW_PRETTY);
    for (int i = 0; i < (sizeof(HANDLERS_ARRAY) / sizeof(HANDLERS_ARRAY[0])); ++i)
    {
        char val[18];
        val[0] = 0x00;
        strcat(val, "~");
        strcat(val, HANDLERS_ARRAY[i].tag);
        strcat(val, "~");
        jwObj_string((char*)HANDLERS_ARRAY[i].tag, val);
    }
    jwEnd();
    jwClose();
    ESP_LOGI(TAG, "%s", buf);
    free(buf);
}

