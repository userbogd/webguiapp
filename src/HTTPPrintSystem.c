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




static const char *TAG = "HTTPServerPrint";



typedef enum
{
    IP,
    NETMASK,
    GW
} IP_PRINT_TYPE;

//Pointer to extend user implemented print handler
static int (*HTTPPrintCust)(httpd_req_t *req, char *buf, char *var);

void regHTTPPrintCustom(int (*print_handler)(httpd_req_t *req, char *buf, char *var))
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

//Default string if not found handler
static void HTTPPrint_DEF(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "#DEF");
}

#if CONFIG_WEBGUIAPP_WIFI_ENABLE

static void HTTPPrint_wfen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->wifiSettings.Flags1.bIsWiFiEnabled);
}
static void HTTPPrint_wfstat(char *VarData, void *arg)
{
    if (GetSysConf()->wifiSettings.Flags1.bIsAP)
        PrintInterfaceState(VarData, arg, GetAPNetifAdapter());
    else
        PrintInterfaceState(VarData, arg, GetSTANetifAdapter());
}
static void HTTPPrint_cln(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, !GetSysConf()->wifiSettings.Flags1.bIsAP);
}
static void HTTPPrint_apn(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->wifiSettings.Flags1.bIsAP);
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
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->wifiSettings.ApIPAddr));
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
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->wifiSettings.InfIPAddr));
}
/*STA NETMASK*/
static void HTTPPrint_msk(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetSTANetifAdapter(), NETMASK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->wifiSettings.InfMask));
}
/*STA GATEWAY*/
static void HTTPPrint_gate(char *VarData, void *arg)
{
    if (GetSTANetifAdapter() && esp_netif_is_netif_up(GetSTANetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetSTANetifAdapter(), GW);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->wifiSettings.InfGateway));
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
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->ethSettings.IPAddr));
}
/*Etherbox NETMASK*/
static void HTTPPrint_emsk(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetETHNetifAdapter(), NETMASK);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->ethSettings.Mask));
}
/*Ethernet GATEWAY*/
static void HTTPPrint_egate(char *VarData, void *arg)
{
    if (GetETHNetifAdapter() && esp_netif_is_netif_up(GetETHNetifAdapter()))
        PrintIPFromInterface(VarData, arg, GetETHNetifAdapter(), GW);
    else
        snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", ip4addr_ntoa(&GetSysConf()->ethSettings.Gateway));
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
void HTTPPrint_gsmen(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled);
}
void HTTPPrint_gsmstat(char *VarData, void *arg)
{
    PrintInterfaceState(VarData, arg, GetPPPNetifAdapter());
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

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
void HTTPPrint_mqtten1(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled);
}
void HTTPPrint_ipcld1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].ServerAddr);
}
void HTTPPrint_mport1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->mqttStation[0].ServerPort);
}
void HTTPPrint_idcld1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].ClientID);
}
void HTTPPrint_topic1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].RootTopic);
}
void HTTPPrint_clname1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[0].UserName);
}
void HTTPPrint_clpass1(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "******");
}

#if CONFIG_MQTT_CLIENTS_NUM == 2
void HTTPPrint_mqtten2(char *VarData, void *arg)
{
    PrintCheckbox(VarData, arg, GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled);
}
void HTTPPrint_ipcld2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].ServerAddr);
}
void HTTPPrint_mport2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%d", GetSysConf()->mqttStation[1].ServerPort);
}
void HTTPPrint_idcld2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].ClientID);
}
void HTTPPrint_topic2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].RootTopic);
}
void HTTPPrint_clname2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", GetSysConf()->mqttStation[1].UserName);
}
void HTTPPrint_clpass2(char *VarData, void *arg)
{
    snprintf(VarData, MAX_DYNVAR_LENGTH, "%s", "******");
}
#endif
#endif

dyn_var_handler_t HANDLERS_ARRAY[] = {
        /*Ststem settings*/
        { "dname", sizeof("dname") - 1, &HTTPPrint_dname },
        { "login", sizeof("login") - 1, &HTTPPrint_login },
        { "pass", sizeof("pass") - 1, &HTTPPrint_pass },

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
        /*WiFi network*/
        { "wfen", sizeof("wfen") - 1, &HTTPPrint_wfen },
        { "wfstat", sizeof("wfstat") - 1, &HTTPPrint_wfstat },
        { "cln", sizeof("cln") - 1, &HTTPPrint_cln },
        { "apn", sizeof("apn") - 1, &HTTPPrint_apn },
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

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
        /*MQTT*/
        { "mqtten1", sizeof("mqtten1") - 1, &HTTPPrint_mqtten1 },
        { "ipcld1", sizeof("ipcld1") - 1, &HTTPPrint_ipcld1 },
        { "mport1", sizeof("mport1") - 1, &HTTPPrint_mport1 },
        { "idcld1", sizeof("idcld1") - 1, &HTTPPrint_idcld1 },
        { "topic1", sizeof("topic1") - 1, &HTTPPrint_topic1 },
        { "clname1", sizeof("clname1") - 1, &HTTPPrint_clname1 },
        { "clpass1", sizeof("clpass1") - 1, &HTTPPrint_clpass1 },
        #if CONFIG_MQTT_CLIENTS_NUM == 2
        { "mqtten2", sizeof("mqtten2") - 1, &HTTPPrint_mqtten2 },
        { "ipcld2", sizeof("ipcld2") - 1, &HTTPPrint_ipcld2 },
        { "mport2", sizeof("mport2") - 1, &HTTPPrint_mport2 },
        { "idcld2", sizeof("idcld2") - 1, &HTTPPrint_idcld2 },
        { "topic2", sizeof("topic2") - 1, &HTTPPrint_topic2 },
        { "clname2", sizeof("clname2") - 1, &HTTPPrint_clname2 },
        { "clpass2", sizeof("clpass2") - 1, &HTTPPrint_clpass2 },
#endif
#endif
        /*ERROR report*/
        { "status_fail", sizeof("status_fail") - 1, &HTTPPrint_status_fail },

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
            HTTPPrintCust(req, buf, var);
        else
            HTTPPrint_DEF(VarData, NULL);

    }
    int dLen = strlen(VarData);
    memcpy(buf, VarData, dLen);
    return dLen;

}
