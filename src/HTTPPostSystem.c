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
 *  	 \file HTTPPostSystem.c
 *    \version 1.0
 * 		 \date 2022-08-14
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "HTTPServer.h"
#include "LoRaWAN.h"
#include "Helpers.h"
#include "MQTT.h"
#include "UserCallbacks.h"

static const char *TAG = "HTTPServerPost";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

const char url_adapters[] = "adapters.html";
const char url_services[] = "services.html";
const char url_system[] = "system.html";
const char url_reboot[] = "reboot.html";
const char url_sysapi[] = "sysapi";

static HTTP_IO_RESULT AfterPostHandler(httpd_req_t *req, const char *filename, char *PostData);
static HTTP_IO_RESULT HTTPPostAdaptersSettings(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostServicesSettings(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostSystemSettings(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostReboot(httpd_req_t *req, char *PostData);

HTTP_IO_RESULT (*AfterPostHandlerCust)(httpd_req_t *req, const char *filename, char *PostData);

void regAfterPostHandlerCustom(HTTP_IO_RESULT (*post_handler)(httpd_req_t *req, const char *filename, char *PostData))
{
    AfterPostHandlerCust = post_handler;
}


HTTP_IO_RESULT HTTPPostApp(httpd_req_t *req, const char *filename, char *PostData)
{
    const char *pt = filename + 1;

#if HTTP_SERVER_DEBUG_LEVEL > 0
    //ESP_LOGI(TAG, "URI for POST processing:%s", req->uri);
    ESP_LOGI(TAG, "Filename:%s", pt);
    ESP_LOGI(TAG, "DATA for POST processing:%s", PostData);
#endif

    HTTP_IO_RESULT res = AfterPostHandler(req, pt, PostData);

    switch (res)
    {
        case HTTP_IO_DONE:
            break;
        case HTTP_IO_WAITING:
            break;
        case HTTP_IO_NEED_DATA:
            break;
        case HTTP_IO_REDIRECT:
            strcpy((char*) filename, PostData);
        break;
        case HTTP_IO_DONE_NOREFRESH:
            break;
        case HTTP_IO_DONE_API:
            break;
        break;
    }
    return res;
}

static HTTP_IO_RESULT AfterPostHandler(httpd_req_t *req, const char *filename, char *PostData)
{
    if (!memcmp(filename, url_sysapi, sizeof(url_sysapi)))
        return HTTPPostSysAPI(req, PostData);
    if (!memcmp(filename, url_adapters, sizeof(url_adapters)))
        return HTTPPostAdaptersSettings(req, PostData);
    if (!memcmp(filename, url_services, sizeof(url_services)))
        return HTTPPostServicesSettings(req, PostData);
    if (!memcmp(filename, url_system, sizeof(url_system)))
        return HTTPPostSystemSettings(req, PostData);

    if (!memcmp(filename, url_reboot, sizeof(url_reboot)))
        return HTTPPostReboot(req, PostData);


    // If not found target URL here, try to call custom code
    if (AfterPostHandlerCust != NULL)
        return AfterPostHandlerCust(req, filename, PostData);

    return HTTP_IO_DONE;
}




static HTTP_IO_RESULT HTTPPostAdaptersSettings(httpd_req_t *req, char *PostData)
{
    char tmp[33];
#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE

    bool TempIsETHEnabled = false;
    bool TempIsETHDHCPEnabled = false;
    if (httpd_query_key_value(PostData, "ethen", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
        TempIsETHEnabled = true;
    }
    if (httpd_query_key_value(PostData, "dhcp", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
        TempIsETHDHCPEnabled = true;
    }

    if (httpd_query_key_value(PostData, "ipa", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.IPAddr);
    if (httpd_query_key_value(PostData, "mas", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.Mask);
    if (httpd_query_key_value(PostData, "gte", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.Gateway);
    if (httpd_query_key_value(PostData, "dns1", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.DNSAddr1);
    if (httpd_query_key_value(PostData, "dns2", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.DNSAddr2);
    if (httpd_query_key_value(PostData, "dns3", tmp, 15) == ESP_OK)
    esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->ethSettings.DNSAddr3);

#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE

    bool TempIsWiFiEnabled = false;
    bool TempIsWIFIDHCPEnabled = false;
    if (httpd_query_key_value(PostData, "wifien", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsWiFiEnabled = true;
    }
    if (httpd_query_key_value(PostData, "wfmode", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "sta"))
            GetSysConf()->wifiSettings.WiFiMode = WIFI_MODE_STA;
        else if (!strcmp((const char*) tmp, (const char*) "ap"))
            GetSysConf()->wifiSettings.WiFiMode = WIFI_MODE_AP;
        else if (!strcmp((const char*) tmp, (const char*) "apsta"))
            GetSysConf()->wifiSettings.WiFiMode = WIFI_MODE_APSTA;
    }
    if (httpd_query_key_value(PostData, "wifipwr", tmp, sizeof(tmp)) == ESP_OK)
    {
        uint8_t pwr = atoi((const char*) tmp);
        if (pwr >= 1 && pwr <= 20)
            GetSysConf()->wifiSettings.MaxPower = pwr * 4;
    }

    /*AP section*/
    httpd_query_key_value(PostData, "wfiap", GetSysConf()->wifiSettings.ApSSID,
                          sizeof(GetSysConf()->wifiSettings.ApSSID));
    if (httpd_query_key_value(PostData, "wfpap", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strcmp(tmp, (const char*) "********"))
            strcpy(GetSysConf()->wifiSettings.ApSecurityKey, tmp);
    }
    if (httpd_query_key_value(PostData, "ipaap", tmp, sizeof(tmp)) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.ApIPAddr);

    httpd_query_key_value(PostData, "wfi", GetSysConf()->wifiSettings.InfSSID,
                          sizeof(GetSysConf()->wifiSettings.InfSSID));
    if (httpd_query_key_value(PostData, "wfp", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strcmp(tmp, (const char*) "********"))
            strcpy(GetSysConf()->wifiSettings.InfSecurityKey, tmp);
    }
    /*STATION section*/
    if (httpd_query_key_value(PostData, "dhcp", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsWIFIDHCPEnabled = true;
    }
    if (httpd_query_key_value(PostData, "ipa", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.InfIPAddr);
    if (httpd_query_key_value(PostData, "mas", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.InfMask);
    if (httpd_query_key_value(PostData, "gte", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.InfGateway);
    if (httpd_query_key_value(PostData, "dns", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.DNSAddr1);
    if (httpd_query_key_value(PostData, "dns2", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.DNSAddr2);
    if (httpd_query_key_value(PostData, "dns3", tmp, 15) == ESP_OK)
        esp_netif_str_to_ip4(tmp, (esp_ip4_addr_t*) &GetSysConf()->wifiSettings.DNSAddr3);

#endif
#if    CONFIG_WEBGUIAPP_GPRS_ENABLE
    bool TempIsGSMEnabled = false;
    if (httpd_query_key_value(PostData, "gsmen", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsGSMEnabled = true;
    }

    if (httpd_query_key_value(PostData, "sav", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled = TempIsGSMEnabled;
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }
    if (httpd_query_key_value(PostData, "restart", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            //PPPModemSoftRestart();
        }
    }
    if (httpd_query_key_value(PostData, "hdrst", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            //PPPModemColdStart();
        }
    }
#endif

#if   CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    bool TempIsLoRaEnabled = false;
    if (httpd_query_key_value(PostData, "lren", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsLoRaEnabled = true;
    }
    if (httpd_query_key_value(PostData, "lrdvid", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strlen(tmp) == 16)
            StrToBytesLen((unsigned char*) tmp, (unsigned char*) GetSysConf()->lorawanSettings.DevEui, 16);
    }
    if (httpd_query_key_value(PostData, "lrapid", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strlen(tmp) == 16)
            StrToBytesLen((unsigned char*) tmp, (unsigned char*) GetSysConf()->lorawanSettings.AppEui, 16);
    }
    if (httpd_query_key_value(PostData, "lrapkey", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strlen(tmp) == 32)
            StrToBytesLen((unsigned char*) tmp, (unsigned char*) GetSysConf()->lorawanSettings.AppKey, 32);
    }
#endif

    if (httpd_query_key_value(PostData, "wifisc", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            WiFiScan();
        }
    }

    if (httpd_query_key_value(PostData, "wifistart", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            WiFiStartAP();
        }
    }
    if (httpd_query_key_value(PostData, "wifistop", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            WiFiStopAP();
        }
    }


    if (httpd_query_key_value(PostData, "wifisave", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }

    if (httpd_query_key_value(PostData, "save", tmp, 5) == ESP_OK ||
            httpd_query_key_value(PostData, "apply", tmp, 5) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "eth"))
        {
#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
            GetSysConf()->ethSettings.Flags1.bIsETHEnabled = TempIsETHEnabled;
            GetSysConf()->ethSettings.Flags1.bIsDHCPEnabled = TempIsETHDHCPEnabled;
#endif
        }
        else if (!strcmp(tmp, (const char*) "wifi"))
        {
#if CONFIG_WEBGUIAPP_WIFI_ENABLE
            GetSysConf()->wifiSettings.Flags1.bIsWiFiEnabled = TempIsWiFiEnabled;
            GetSysConf()->wifiSettings.Flags1.bIsDHCPEnabled = TempIsWIFIDHCPEnabled;
#endif
        }
        else if (!strcmp(tmp, (const char*) "gprs"))
        {
#if    CONFIG_WEBGUIAPP_GPRS_ENABLE
            GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled = TempIsGSMEnabled;
#endif
        }
        else if (!strcmp(tmp, (const char*) "lora"))
        {
#if    CONFIG_WEBGUIAPP_LORAWAN_ENABLE
            GetSysConf()->lorawanSettings.Flags1.bIsLoRaWANEnabled = TempIsLoRaEnabled;
#endif
        }

        if (httpd_query_key_value(PostData, "apply", tmp, 5) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            return HTTP_IO_DONE;
        }
        else if (httpd_query_key_value(PostData, "save", tmp, 5) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }

    }

    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostServicesSettings(httpd_req_t *req, char *PostData)
{
    char tmp[64];
#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    bool TempIsMQTT1Enabled = false;

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
    bool TempIsMQTT2Enabled = false;
#endif
    httpd_query_key_value(PostData, "mqurl1", GetSysConf()->mqttStation[0].ServerAddr,
                          sizeof(GetSysConf()->mqttStation[0].ServerAddr));
    httpd_query_key_value(PostData, "mqid1", GetSysConf()->mqttStation[0].ClientID,
                          sizeof(GetSysConf()->mqttStation[0].ClientID));
    httpd_query_key_value(PostData, "mqsys1", GetSysConf()->mqttStation[0].SystemName,
                          sizeof(GetSysConf()->mqttStation[0].SystemName));
    httpd_query_key_value(PostData, "mqgrp1", GetSysConf()->mqttStation[0].GroupName,
                          sizeof(GetSysConf()->mqttStation[0].GroupName));
    httpd_query_key_value(PostData, "mqname1", GetSysConf()->mqttStation[0].UserName,
                          sizeof(GetSysConf()->mqttStation[0].UserName));

    if (httpd_query_key_value(PostData, "mqen1", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsMQTT1Enabled = true;
    }
    if (httpd_query_key_value(PostData, "mqport1", tmp, sizeof(tmp)) == ESP_OK)
    {
        uint16_t tp = atoi((const char*) tmp);
        if (tp < 65535 && tp >= 1000)
            GetSysConf()->mqttStation[0].ServerPort = tp;
    }

    if (httpd_query_key_value(PostData, "mqpass1", tmp, sizeof(tmp)) == ESP_OK &&
            strcmp(tmp, (const char*) "******"))
    {
        strcpy(GetSysConf()->mqttStation[0].UserPass, tmp);
    }

#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
    httpd_query_key_value(PostData, "mqurl2", GetSysConf()->mqttStation[1].ServerAddr,
                          sizeof(GetSysConf()->mqttStation[1].ServerAddr));
    httpd_query_key_value(PostData, "mqid2", GetSysConf()->mqttStation[1].ClientID,
                          sizeof(GetSysConf()->mqttStation[1].ClientID));
    httpd_query_key_value(PostData, "mqsys2", GetSysConf()->mqttStation[1].SystemName,
                          sizeof(GetSysConf()->mqttStation[1].SystemName));
    httpd_query_key_value(PostData, "mqgrp2", GetSysConf()->mqttStation[1].GroupName,
                          sizeof(GetSysConf()->mqttStation[1].GroupName));
    httpd_query_key_value(PostData, "mqname2", GetSysConf()->mqttStation[1].UserName,
                          sizeof(GetSysConf()->mqttStation[1].UserName));

    if (httpd_query_key_value(PostData, "mqen2", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsMQTT2Enabled = true;
    }

    if (httpd_query_key_value(PostData, "mqport2", tmp, sizeof(tmp)) == ESP_OK)
    {
        uint16_t tp = atoi((const char*) tmp);
        if (tp < 65535 && tp >= 1000)
            GetSysConf()->mqttStation[1].ServerPort = tp;
    }

    if (httpd_query_key_value(PostData, "mqpass2", tmp, sizeof(tmp)) == ESP_OK &&
            strcmp(tmp, (const char*) "******"))
    {
        strcpy(GetSysConf()->mqttStation[1].UserPass, tmp);
    }

#endif

    /*SNTP*/
    bool TempIsSNTPEnabled = false;
    if (httpd_query_key_value(PostData, "sntpen", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsSNTPEnabled = true;
    }
    httpd_query_key_value(PostData, "tsr", GetSysConf()->sntpClient.SntpServerAdr,
                          sizeof(GetSysConf()->sntpClient.SntpServerAdr));
    httpd_query_key_value(PostData, "tsr2", GetSysConf()->sntpClient.SntpServer2Adr,
                          sizeof(GetSysConf()->sntpClient.SntpServer2Adr));
    httpd_query_key_value(PostData, "tsr3", GetSysConf()->sntpClient.SntpServer3Adr,
                          sizeof(GetSysConf()->sntpClient.SntpServer3Adr));

    /*MQTT Test button handlers*/
    if (httpd_query_key_value(PostData, "mqtttest1", tmp, 6) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled)
                PublicTestMQTT(0);
            return HTTP_IO_DONE;
        }
    }

    if (httpd_query_key_value(PostData, "mqtttest2", tmp, 6) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            if (GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
                PublicTestMQTT(1);
            return HTTP_IO_DONE;
        }
    }

    if (httpd_query_key_value(PostData, "save", tmp, 6) == ESP_OK ||
            httpd_query_key_value(PostData, "apply", tmp, 6) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "mqtt1"))
        {
            GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled = TempIsMQTT1Enabled;
        }

        else if (!strcmp(tmp, (const char*) "mqtt2"))
        {
#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
            GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled = TempIsMQTT2Enabled;
#endif
        }

        else if (!strcmp(tmp, (const char*) "sntp"))
        {
            GetSysConf()->sntpClient.Flags1.bIsGlobalEnabled = TempIsSNTPEnabled;
        }

        if (httpd_query_key_value(PostData, "apply", tmp, 6) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            return HTTP_IO_DONE;
        }
        else if (httpd_query_key_value(PostData, "save", tmp, 6) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }

#endif
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostSystemSettings(httpd_req_t *req, char *PostData)
{
    char tmp[128];
    bool TempIsOTAEnabled = false;
    bool TempIsRstEnabled = false;
    if (httpd_query_key_value(PostData, "nam", tmp, sizeof(tmp)) == ESP_OK)
    {
        UnencodeURL(tmp);
        strcpy(GetSysConf()->NetBIOSName, tmp);
    }

    httpd_query_key_value(PostData, "lgn", GetSysConf()->SysName, sizeof(GetSysConf()->SysName));

    if (httpd_query_key_value(PostData, "psn", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strcmp(tmp, (const char*) "******"))
            strcpy(GetSysConf()->SysPass, tmp);
    }

    if (httpd_query_key_value(PostData, "ota", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsOTAEnabled = true;
    }

    if (httpd_query_key_value(PostData, "otarst", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsRstEnabled = true;
    }

    if (httpd_query_key_value(PostData, "otaint", tmp, sizeof(tmp)) == ESP_OK)
    {
        uint16_t tp = atoi((const char*) tmp);
        if (tp < 65535 && tp >= 1)
            GetSysConf()->OTAAutoInt = tp;
    }

    if (httpd_query_key_value(PostData, "otaurl", tmp, sizeof(tmp)) == ESP_OK)
    {
        UnencodeURL(tmp);
        strcpy(GetSysConf()->OTAURL, tmp);
    }

    if (httpd_query_key_value(PostData, "colscheme", tmp, sizeof(tmp)) == ESP_OK)
    {
        uint16_t chm = atoi((const char*) tmp);
        if (chm < 3 && chm >= 1)
            GetSysConf()->ColorSheme = chm;
    }

    if (httpd_query_key_value(PostData, "upd", tmp, sizeof(tmp)) == ESP_OK)
    {

        if (!strcmp(tmp, (const char*) "prs"))
        {
            StartOTA();
        }

    }
    if (httpd_query_key_value(PostData, "rst", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }

    if (httpd_query_key_value(PostData, "save", tmp, sizeof(tmp)) == ESP_OK ||
            httpd_query_key_value(PostData, "apply", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "syst"))
        {
            GetSysConf()->Flags1.bIsOTAEnabled = TempIsOTAEnabled;
            GetSysConf()->Flags1.bIsResetOTAEnabled = TempIsRstEnabled;
        }

        if (httpd_query_key_value(PostData, "apply", tmp, 5) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            return HTTP_IO_DONE;
        }
        else if (httpd_query_key_value(PostData, "save", tmp, 5) == ESP_OK)
        {
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }

    }

    if (httpd_query_key_value(PostData, "cmd", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "1"))
        {

            return HTTP_IO_DONE_NOREFRESH;
        }
#if CONFIG_WEBGUIAPP_GPRS_ENABLE
        else if (!strcmp(tmp, (const char*) "2"))
        {
            char resp[256] = {0};
            ModemSendAT("AT+CCLK?\r", resp, 200);
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "3"))
        {
            char resp[256] = {0};
            ModemSendAT("ATD+79022518532;\r", resp, 200);
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "4"))
        {
            char resp[256] = {0};
            ModemSendAT("ATH\r", resp, 200);
            return HTTP_IO_DONE_NOREFRESH;
        }
#endif
        else if (!strcmp(tmp, (const char*) "5"))
        {
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "6"))
        {
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "7"))
        {
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "8"))
        {
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "9"))
        {
            return HTTP_IO_DONE_NOREFRESH;
        }
        else if (!strcmp(tmp, (const char*) "10"))
        {
            GenerateSystemSettingsJSONFile();
            return HTTP_IO_DONE_NOREFRESH;
        }

    }

    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostReboot(httpd_req_t *req, char *PostData)
{
    char tmp[33];
    if (httpd_query_key_value(PostData, "rbt", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            DelayedRestart();
        }
    }
    return HTTP_IO_DONE;
}

