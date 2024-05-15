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
#include "CronTimers.h"

typedef enum
{
    R = 0,
    RW,
}rest_var_attr;

typedef enum{
    VAR_ERROR = -1,
    VAR_BOOL = 0,
    VAR_INT,
    VAR_STRING,
    VAR_PASS,
    VAR_IPADDR,
    VAR_FUNCT
} rest_var_types;



typedef struct
{
  int id;
  char alias[32];
  void* ref;
  rest_var_types vartype;
  rest_var_attr varattr;
  int minlen;
  int maxlen;
} rest_var_t;



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
        char OTAURL[128];        ///< OTA URL
        int  OTAAutoInt;

        char SN[11];        ///< String of serial number (decimal ID)
        char ID[9];        ///< String of ID ( last 4 bytes of MAC)
        int ColorSheme;

        struct
        {
            bool bIsOTAEnabled;
            bool bIsResetOTAEnabled;
            bool bIsLedsEnabled;  ///< Indication LEDs enable
            bool bIsLoRaConfirm;  ///< Enable send back confirmation in LoRa channel
            bool bIsTCPConfirm;   ///< Enable send back confirmation in TCP channel

        } Flags1; // Flag structure

        struct
        {
            int TimeZone;
            char SntpServerAdr[33];
            char SntpServer2Adr[33];
            char SntpServer3Adr[33];
            struct
            {
                bool bIsGlobalEnabled;
            } Flags1;
            float lat;
            float lon;

        } sntpClient;

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
        struct
        {
            char ServerAddr[64];
            int ServerPort;
            char SystemName[32];
            char GroupName[32];
            char ClientID[32];
            char UserName[32];
            char UserPass[32];
            struct
            {
                bool bIsGlobalEnabled;
            } Flags1;
        } mqttStation[CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM];
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
        struct
        {
            esp_ip4_addr_t IPAddr; // IP address
            esp_ip4_addr_t Mask; // network mask
            esp_ip4_addr_t Gateway; // gateway
            esp_ip4_addr_t DNSAddr1; //
            esp_ip4_addr_t DNSAddr2; //
            esp_ip4_addr_t DNSAddr3; //
            uint8_t MACAddr[6]; // MAC address

            struct
            {
                bool bIsDHCPEnabled;
                bool bIsETHEnabled;
            } Flags1; // Flag structure

        } ethSettings;
#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
        struct
        {
            int WiFiMode;
            esp_ip4_addr_t InfIPAddr; // IP address in infrastructure(INF) mode
            esp_ip4_addr_t InfMask; // network mask in INF mode
            esp_ip4_addr_t InfGateway; // gateway IP in INF mode
            esp_ip4_addr_t ApIPAddr; // IP address in Access point(AP) mode
            esp_ip4_addr_t DNSAddr1; // DNS in station mode
            esp_ip4_addr_t DNSAddr2; // DNS in station mode
            esp_ip4_addr_t DNSAddr3; // DNS in station mode

            char InfSSID[32]; // Wireless SSID in INF mode
            char InfSecurityKey[32]; // Network key in INF mode
            char ApSSID[32]; // Wireless SSID in AP mode
            char ApSecurityKey[32]; // Wireless key in AP mode

            char MACAddrInf[6]; // MAC address
            char MACAddrAp[6]; // MAC address

            struct
            {
                bool bIsDHCPEnabled;
                bool bIsWiFiEnabled;
            } Flags1; // Flag structure
            int MaxPower;
            int AP_disab_time;
        } wifiSettings;

#endif
#if CONFIG_WEBGUIAPP_GPRS_ENABLE
        struct
        {
            esp_ip4_addr_t IPAddr; // IP address
            esp_ip4_addr_t Mask; // network mask
            esp_ip4_addr_t Gateway; // gateway
            esp_ip4_addr_t DNSAddr1; //
            esp_ip4_addr_t DNSAddr2; //
            esp_ip4_addr_t DNSAddr3; //
            uint8_t MACAddr[6]; // MAC address
            struct
            {
                bool bIsGSMEnabled;
            } Flags1; // Flag structure

            char APN[32];
            char login[32];
            char password[32];

        } gsmSettings;
#endif


#ifdef  CONFIG_WEBGUIAPP_UART_TRANSPORT_ENABLE
        struct
        {
            int Serialmode;
            int BaudRate;
            int InputBrake;
            struct
            {
              bool IsSerialEnabled;
              bool IsBridgeEnabled;
            } Flags;
        } serialSettings;
#endif

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
        struct
          {
              char DevEui[8];
              char AppEui[8];
              char AppKey[16];

              struct
              {
                  bool bIsLoRaWANEnabled;
              } Flags1; // Flag structure

          } lorawanSettings;
#endif

          struct
          {
            bool IsModbusTCPEnabled;
            int ModbusTCPPort;

          } modbusSettings;


          cron_timer_t Timers[CONFIG_WEBGUIAPP_CRON_NUMBER];

    } SYS_CONFIG;

    esp_err_t ReadNVSSysConfig(SYS_CONFIG *SysConf);
    esp_err_t WriteNVSSysConfig(SYS_CONFIG *SysConf);
    esp_err_t InitSysConfig(void);
    esp_err_t ResetInitSysConfig(void);
    SYS_CONFIG* GetSysConf(void);

    esp_err_t WebGuiAppInit(void);
    void DelayedRestart(void);


#endif /* COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_SYSTEMCONFIGURATION_H_ */
