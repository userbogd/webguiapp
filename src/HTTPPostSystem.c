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

static const char *TAG = "HTTPServerPost";

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

const char pg_11[] = "index.html";
const char pg_20[] = "index20.html";
const char pg_21[] = "index21.html";
const char pg_22[] = "index22.html";
const char pg_23[] = "index23.html";
const char pg_24[] = "index24.html";
const char pg_30[] = "index30.html";
const char pg_31[] = "index31.html";
const char pg_32[] = "index32.html";
const char pg_33[] = "index33.html";
const char pg_34[] = "index34.html";
const char pg_reboot[] = "reboot.html";
const char pg_db[] = "dbg.json";
const char pg_mm[] = "mem.json";

static HTTP_IO_RESULT AfterPostHandler(httpd_req_t *req, const char *filename, char *PostData);

static HTTP_IO_RESULT HTTPPostIndex(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex21(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex20(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex22(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex23(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex24(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex30(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex31(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex32(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex33(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostIndex34(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostReboot(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostDebug(httpd_req_t *req, char *PostData);
static HTTP_IO_RESULT HTTPPostMemJson(httpd_req_t *req, char *PostData);

HTTP_IO_RESULT (*AfterPostHandlerCust)(httpd_req_t *req, const char *filename, char *PostData);

void regAfterPostHandlerCustom(HTTP_IO_RESULT (*post_handler)(httpd_req_t *req, const char *filename, char *PostData))
{
    AfterPostHandlerCust = post_handler;
}


HTTP_IO_RESULT HTTPPostApp(httpd_req_t *req, const char *filename, char *PostData)
{
    const char *pt = filename + 1;

#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "URI for POST processing:%s", req->uri);
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
            strcpy(filename, PostData);
        break;
    }
    return res;
}

static HTTP_IO_RESULT AfterPostHandler(httpd_req_t *req, const char *filename, char *PostData)
{
    if (!memcmp(filename, pg_11, sizeof(pg_11)))
        return HTTPPostIndex(req, PostData);

    if (!memcmp(filename, pg_20, sizeof(pg_20)))
        return HTTPPostIndex20(req, PostData);
    if (!memcmp(filename, pg_21, sizeof(pg_21)))
        return HTTPPostIndex21(req, PostData);
    if (!memcmp(filename, pg_22, sizeof(pg_22)))
        return HTTPPostIndex22(req, PostData);
    if (!memcmp(filename, pg_23, sizeof(pg_23)))
        return HTTPPostIndex23(req, PostData);
    if (!memcmp(filename, pg_24, sizeof(pg_24)))
        return HTTPPostIndex24(req, PostData);

    if (!memcmp(filename, pg_30, sizeof(pg_30)))
        return HTTPPostIndex30(req, PostData);

    if (!memcmp(filename, pg_31, sizeof(pg_31)))
        return HTTPPostIndex31(req, PostData);
    if (!memcmp(filename, pg_32, sizeof(pg_32)))
        return HTTPPostIndex32(req, PostData);
    if (!memcmp(filename, pg_33, sizeof(pg_33)))
        return HTTPPostIndex33(req, PostData);
    if (!memcmp(filename, pg_34, sizeof(pg_34)))
        return HTTPPostIndex34(req, PostData);


    if (!memcmp(filename, pg_reboot, sizeof(pg_reboot)))
        return HTTPPostReboot(req, PostData);

    if (!memcmp(filename, pg_db, sizeof(pg_db)))
        return HTTPPostDebug(req, PostData);

    if (!memcmp(filename, pg_mm, sizeof(pg_mm)))
        return HTTPPostMemJson(req, PostData);

    if (AfterPostHandlerCust != NULL)
        AfterPostHandler(req, filename, PostData);


    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex20(httpd_req_t *req, char *PostData)
{
#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    char tmp[32];
    bool TempIsETHEnabled = false;
    bool TempIsDHCPEnabled = false;
    if (httpd_query_key_value(PostData, "ethen", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsETHEnabled = true;
    }
    if (httpd_query_key_value(PostData, "dhcp", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsDHCPEnabled = true;
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

    if (httpd_query_key_value(PostData, "sav", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            GetSysConf()->ethSettings.Flags1.bIsETHEnabled = TempIsETHEnabled;
            GetSysConf()->ethSettings.Flags1.bIsDHCPEnabled = TempIsDHCPEnabled;
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }
#endif
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex21(httpd_req_t *req, char *PostData)
{
#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    char tmp[32];
    bool TempIsWiFiEnabled = false;
    bool TempIsDHCPEnabled = false;
    if (httpd_query_key_value(PostData, "wifien", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsWiFiEnabled = true;
    }
    if (httpd_query_key_value(PostData, "netm", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            GetSysConf()->wifiSettings.Flags1.bIsAP = true;
        else if (!strcmp((const char*) tmp, (const char*) "2"))
            GetSysConf()->wifiSettings.Flags1.bIsAP = false;
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
            TempIsDHCPEnabled = true;
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

    if (httpd_query_key_value(PostData, "sav", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            GetSysConf()->wifiSettings.Flags1.bIsWiFiEnabled = TempIsWiFiEnabled;
            GetSysConf()->wifiSettings.Flags1.bIsDHCPEnabled = TempIsDHCPEnabled;
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;

        }
    }
#endif
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex22(httpd_req_t *req, char *PostData)
{
#if    CONFIG_WEBGUIAPP_GPRS_ENABLE
    char tmp[32];
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
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex23(httpd_req_t *req, char *PostData)
{

    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex24(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex30(httpd_req_t *req, char *PostData)
{

    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex32(httpd_req_t *req, char *PostData)
{
    char tmp[64];
    bool TempIsTCPConfirm = false;
    bool TempIsLoRaConfirm = false;
    bool TempIsOTAEnabled = false;

    if (httpd_query_key_value(PostData, "nam", tmp, sizeof(tmp)) == ESP_OK)
    {
        UnencodeURL(tmp);
        strcpy(GetSysConf()->NetBIOSName, tmp);
    }
    if (httpd_query_key_value(PostData, "otaurl", tmp, sizeof(tmp)) == ESP_OK)
    {
        UnencodeURL(tmp);
        strcpy(GetSysConf()->OTAURL, tmp);
    }

    httpd_query_key_value(PostData, "lgn", GetSysConf()->SysName, sizeof(GetSysConf()->SysName));

    if (httpd_query_key_value(PostData, "psn", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (strcmp(tmp, (const char*) "******"))
            strcpy(GetSysConf()->SysPass, tmp);
    }

    if (httpd_query_key_value(PostData, "lrdel", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsLoRaConfirm = true;
    }

    if (httpd_query_key_value(PostData, "tcpdel", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsTCPConfirm = true;
    }

    if (httpd_query_key_value(PostData, "ota", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsOTAEnabled = true;
    }

    if (httpd_query_key_value(PostData, "upd", tmp, sizeof(tmp)) == ESP_OK)
    {

        if (!strcmp(tmp, (const char*) "prs"))
        {
            //StartOTA();
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
    if (httpd_query_key_value(PostData, "sav", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            GetSysConf()->Flags1.bIsTCPConfirm = TempIsTCPConfirm;
            GetSysConf()->Flags1.bIsLoRaConfirm = TempIsLoRaConfirm;
            GetSysConf()->Flags1.bIsOTAEnabled = TempIsOTAEnabled;

            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex31(httpd_req_t *req, char *PostData)
{
#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    char tmp[33];
    bool TempIsMQTT1Enabled = false;
    bool TempIsMQTT2Enabled = false;
    httpd_query_key_value(PostData, "cld1", GetSysConf()->mqttStation[0].ServerAddr,
                          sizeof(GetSysConf()->mqttStation[0].ServerAddr));
    httpd_query_key_value(PostData, "idd1", GetSysConf()->mqttStation[0].ClientID,
                          sizeof(GetSysConf()->mqttStation[0].ClientID));
    httpd_query_key_value(PostData, "top1", GetSysConf()->mqttStation[0].RootTopic,
                          sizeof(GetSysConf()->mqttStation[0].RootTopic));
    httpd_query_key_value(PostData, "clnm1", GetSysConf()->mqttStation[0].UserName,
                          sizeof(GetSysConf()->mqttStation[0].UserName));

    httpd_query_key_value(PostData, "cld2", GetSysConf()->mqttStation[1].ServerAddr,
                          sizeof(GetSysConf()->mqttStation[1].ServerAddr));
    httpd_query_key_value(PostData, "idd2", GetSysConf()->mqttStation[1].ClientID,
                          sizeof(GetSysConf()->mqttStation[1].ClientID));
    httpd_query_key_value(PostData, "top2", GetSysConf()->mqttStation[1].RootTopic,
                          sizeof(GetSysConf()->mqttStation[1].RootTopic));
    httpd_query_key_value(PostData, "clnm2", GetSysConf()->mqttStation[1].UserName,
                          sizeof(GetSysConf()->mqttStation[1].UserName));

    if (httpd_query_key_value(PostData, "mqttenb1", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsMQTT1Enabled = true;
    }
    if (httpd_query_key_value(PostData, "mqttenb2", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp((const char*) tmp, (const char*) "1"))
            TempIsMQTT2Enabled = true;
    }

    if (httpd_query_key_value(PostData, "mprt1", tmp, sizeof(tmp)) == ESP_OK)
        if (httpd_query_key_value(PostData, "mprt1", tmp, sizeof(tmp)) == ESP_OK)
        {
            uint16_t tp = atoi((const char*) tmp);
            if (tp < 65535 && tp >= 1000)
                GetSysConf()->mqttStation[0].ServerPort = tp;
        }
    if (httpd_query_key_value(PostData, "mprt2", tmp, sizeof(tmp)) == ESP_OK)
        if (httpd_query_key_value(PostData, "mprt2", tmp, sizeof(tmp)) == ESP_OK)
        {
            uint16_t tp = atoi((const char*) tmp);
            if (tp < 65535 && tp >= 1000)
                GetSysConf()->mqttStation[1].ServerPort = tp;
        }

    if (httpd_query_key_value(PostData, "clps1", tmp, sizeof(tmp)) == ESP_OK &&
            strcmp(tmp, (const char*) "******"))
    {
        strcpy(GetSysConf()->mqttStation[0].UserPass, tmp);
    }
    if (httpd_query_key_value(PostData, "clps2", tmp, sizeof(tmp)) == ESP_OK &&
            strcmp(tmp, (const char*) "******"))
    {
        strcpy(GetSysConf()->mqttStation[1].UserPass, tmp);
    }

    if (httpd_query_key_value(PostData, "sav", tmp, 4) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled = TempIsMQTT1Enabled;
            GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled = TempIsMQTT2Enabled;
            WriteNVSSysConfig(GetSysConf());
            memcpy(PostData, "/reboot.html", sizeof "/reboot.html");
            return HTTP_IO_REDIRECT;
        }
    }
#endif
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex33(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostIndex34(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}



static HTTP_IO_RESULT HTTPPostReboot(httpd_req_t *req, char *PostData)
{
    char tmp[33];
    if (httpd_query_key_value(PostData, "rbt", tmp, sizeof(tmp)) == ESP_OK)
    {
        if (!strcmp(tmp, (const char*) "prs"))
        {
            //DelayedRestart();
        }
    }
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostDebug(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}

static HTTP_IO_RESULT HTTPPostMemJson(httpd_req_t *req, char *PostData)
{
    return HTTP_IO_DONE;
}

