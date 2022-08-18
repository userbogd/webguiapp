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
 *  	 \file SystemConfiguration.h
 *    \version 1.0
 * 		 \date 2022-08-13
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_SYSTEMCONFIGURATION_H_
#define COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_SYSTEMCONFIGURATION_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_netif.h"
#include "sdkconfig.h"

#define DEFAULT_HOST_NAME               "DEVICE_HOSTNAME"     ///<Default host name of device

#define SYSTEM_DEFAULT_USERNAME         "user"
#define SYSTEM_DEFAULT_PASSWORD         "password"
#define SYSTEM_DEFAULT_OTAURL           "https://iotronic.cloud/firmware/HB75ControllerFirmware.bin"

#define DEFAULT_SNTP_SERVERNAME         "ntp5.stratum2.ru"
#define DEFAULT_SNTP_TIMEZONE           2
#define DEFAULT_SNTP_ETH_IS_ENABLED     false
#define DEFAULT_SNTP_WIFI_IS_ENABLED    false
#define DEFAULT_SNTP_GLOBAL_ENABLED     true

#define DEFAULT_MQTT_SERVERNAME             "iotronic.cloud"
#define DEFAULT_MQTT_SERVERPORT              1883
#define DEFAULT_MQTT_CLIENTID               "HB75_DISP1"
#define DEFAULT_MQTT_CLIENTID2              "HB75_DISP2"
#define DEFAULT_MQTT_USERNAME               "hb75_username"
#define DEFAULT_MQTT_USERPASS               "hb75_pass"
#define DEFAULT_MQTT_ROOTTOPIC              "HB75_CONTROLLER"
#define DEFAULT_MQTT_GLOBAL_ENABLED         true

//WIFI interface related constatnts
#define DEFAULT_WIFI_SSID_INF_NAME               "wifiapname"
#define DEFAULT_WIFI_SSID_INF_KEY                "wifikey"
#define DEFAULT_WIFI_SSID_AP_NAME                "HB75"
#define DEFAULT_WIFI_SSID_AP_KEY                 "123456789"

#define DEFAULT_WIFI_IP_ADDR_INF                "192.168.150.1"
#define DEFAULT_WIFI_MASK                       "255.255.255.0"
#define DEFAULT_WIFI_GATE                       "192.168.150.1"
#define DEFAULT_WIFI_IP_ADDR_AP                 "192.168.150.1"

#define DEFAULT_WIFI_FLAG_ISAP                   true
#define DEFAULT_WIFI_FLAG_DHCP_ENABLED           true
#define DEFAULT_WIFI_FLAG_ISWIFI_ENABLED         true

#define DEFAULT_ETH_IP_ADDR                   "192.168.150.2"
#define DEFAULT_ETH_MASK                      "255.255.255.0"
#define DEFAULT_ETH_GATE                      "192.168.150.1"

#define DEFAULT_ETH_FLAG_DHCP_ENABLED     true
#define DEFAULT_ETH_FLAG_ISETH_ENABLED    true

#define DEFAULT_DNS1                "8.8.8.8"
#define DEFAULT_DNS2                "4.4.8.8"
#define DEFAULT_DNS3                "1.1.1.1"

    /*GSM DEFAULT SETTINGS*/
#define DEFAULT_GSM_GLOBAL_ENABLED      true


//#define LOCK_RELAY_ON

// Application-dependent structure used to contain address information

    /**
     * @struct  APP_CONFIG
     * @brief The main configuration structure
     * @details This structure saving to EEPROM and loading from EEPROM\n
     * on device boot. On load the checksumm is calculate and compare to \n
     * saved one. If find difference (due to eeprom memory distortions),\n
     * the default values will be loaded.
     */
    typedef struct
    {
        char NetBIOSName[32];   ///< NetBIOS name
        char SysName[32];       ///< User Name
        char SysPass[32];       ///< User Password
        char OTAURL[64];        ///< OTA URL
        uint8_t imei[4];

        struct
        {
            char bIsOTAEnabled :1;
            char bIsLedsEnabled :1;  ///< Indication LEDs enable
            char bIsLoRaConfirm :1;  ///< Enable send back confirmation in LoRa channel
            char bIsTCPConfirm :1;   ///< Enable send back confirmation in TCP channel
            char bit4 :1;
            char bit5 :1;
            char bit6 :1;
            char bit7 :1;
        } Flags1; // Flag structure

        struct
        {
            int TimeZone;
            uint8_t SntpServerAdr[33];
            struct
            {
                char bIsWifiEnabled :1;
                char bIsEthEnabled :1;
                char b3 :1;
                char b4 :1;
                char b5 :1;
                char b6 :1;
                char b7 :1;
                char bIsGlobalEnabled :1;
            } Flags1;
        } sntpClient;

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
        struct
        {
            char ServerAddr[32];
            uint16_t ServerPort;
            char ClientID[32];
            char RootTopic[32];
            char UserName[32];
            char UserPass[32];
            struct
            {
                char b0 :1;
                char b1 :1;
                char b2 :1;
                char b3 :1;
                char b4 :1;
                char b5 :1;
                char b6 :1;
                char bIsGlobalEnabled :1;
            } Flags1;
        } mqttStation[CONFIG_MQTT_CLIENTS_NUM];
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
        struct
        {
            ip4_addr_t IPAddr; // IP address
            ip4_addr_t Mask; // network mask
            ip4_addr_t Gateway; // gateway
            ip4_addr_t DNSAddr1; //
            ip4_addr_t DNSAddr2; //
            ip4_addr_t DNSAddr3; //
            uint8_t MACAddr[6]; // MAC address

            struct
            {
                char bIsDHCPEnabled :1;
                char b1 :1;
                char b2 :1;
                char b3 :1;
                char b4 :1;
                char b5 :1;
                char b6 :1;
                char bIsETHEnabled :1;
            } Flags1; // Flag structure

        } ethSettings;
#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
        struct
        {
            ip4_addr_t InfIPAddr; // IP address in infrastructure(INF) mode
            ip4_addr_t InfMask; // network mask in INF mode
            ip4_addr_t InfGateway; // gateway IP in INF mode
            ip4_addr_t ApIPAddr; // IP address in Access point(AP) mode
            ip4_addr_t DNSAddr1; // DNS in station mode
            ip4_addr_t DNSAddr2; // DNS in station mode
            ip4_addr_t DNSAddr3; // DNS in station mode

            char InfSSID[32]; // Wireless SSID in INF mode
            char InfSecurityKey[32]; // Network key in INF mode
            char ApSSID[32]; // Wireless SSID in AP mode
            char ApSecurityKey[32]; // Wireless key in AP mode

            char MACAddrInf[6]; // MAC address
            char MACAddrAp[6]; // MAC address

            struct
            {
                char bIsDHCPEnabled :1;
                char bIsAP :1;
                char b2 :1;
                char b3 :1;
                char b4 :1;
                char b5 :1;
                char b6 :1;
                char bIsWiFiEnabled :1;
            } Flags1; // Flag structure

        } wifiSettings;

#endif
#if CONFIG_WEBGUIAPP_GPRS_ENABLE
        struct
        {
            ip4_addr_t IPAddr; // IP address
            ip4_addr_t Mask; // network mask
            ip4_addr_t Gateway; // gateway
            ip4_addr_t DNSAddr1; //
            ip4_addr_t DNSAddr2; //
            ip4_addr_t DNSAddr3; //
            uint8_t MACAddr[6]; // MAC address
            struct
            {
                char b0 :1;
                char b1 :1;
                char b2 :1;
                char b3 :1;
                char b4 :1;
                char b5 :1;
                char b6 :1;
                char bIsGSMEnabled :1;
            } Flags1; // Flag structure

        } gsmSettings;
#endif

    } SYS_CONFIG;

    esp_err_t ReadNVSSysConfig(SYS_CONFIG *SysConf);
    esp_err_t WriteNVSSysConfig(SYS_CONFIG *SysConf);
    esp_err_t InitSysConfig(void);
    esp_err_t ResetInitSysConfig(void);
    SYS_CONFIG* GetSysConf(void);

    esp_err_t WebGuiAppInit(void);


#endif /* COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_SYSTEMCONFIGURATION_H_ */
