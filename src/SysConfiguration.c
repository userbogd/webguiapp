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
 *  	 \file WebGUIAppMain.c
 *    \version 1.0
 * 		 \date 2022-08-13
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "../include/SysConfiguration.h"

#include "SystemApplication.h"
#include <webguiapp.h>
#include "stdlib.h"
#include "string.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include <driver/uart.h>

#include "romfs.h"
#include "spifs.h"
#include "NetTransport.h"
#include "Helpers.h"
#include "HTTPServer.h"
#include "esp_rom_gpio.h"

#define STORAGE_NAMESPACE "storage"
#define TAG "SystemConfiguration"

#ifdef CONFIG_RESET_MODE_ENABLE
#define MANUAL_RESET 1
#else
#define MANUAL_RESET 0
#endif

#ifdef CONFIG_USERDEFINED_MAIN_FUNCTIONAL_BUTTON_GPIO
#define MAIN_FUNCTIONAL_BUTTON_GPIO CONFIG_USERDEFINED_MAIN_FUNCTIONAL_BUTTON_GPIO
#else
#ifdef CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO
#define MAIN_FUNCTIONAL_BUTTON_GPIO CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO
#endif
#endif

SYS_CONFIG SysConfig;

#define SPI_LOCK_TIMEOUT_MS (1000)
SemaphoreHandle_t xSemaphoreSPIHandle = NULL;
StaticSemaphore_t xSemaphoreSPIBuf;

#define NETWORK_START_TIMEOUT (5)

static bool isUserAppNeedReset = false;

static void InitSysIO(void);
static void InitSysSPI(void);
static void InitSysI2C(void);

esp_err_t spi_device_polling_transmit_synchronized(spi_device_handle_t handle,
                                                   spi_transaction_t *trans_desc)
{
    esp_err_t res;
    if (xSemaphoreTake(xSemaphoreSPIHandle, pdMS_TO_TICKS(SPI_LOCK_TIMEOUT_MS))
            == pdTRUE)
    {
        res = spi_device_polling_transmit(handle, trans_desc);
        xSemaphoreGive(xSemaphoreSPIHandle);
    }
    else
    {
        res = ESP_ERR_TIMEOUT;
    }
    return res;
}

esp_err_t WebGuiAppInit(void)
{
    InitSysIO();
    StartSystemTimer();
#if CONFIG_WEBGUIAPP_SPI_ENABLE
    InitSysSPI();
#endif
#if CONFIG_WEBGUIAPP_I2C_ENABLE
    InitSysI2C();
#endif
#if CONFIG_SDCARD_ENABLE
    InitSysSDCard();
#endif

    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND
            ||
            MANUAL_RESET == 1
                    #if (MAIN_FUNCTIONAL_BUTTON_GPIO >= 0)
            || gpio_get_level(MAIN_FUNCTIONAL_BUTTON_GPIO) == 0
                    #endif
                    )
    {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        isUserAppNeedReset = true;
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(ResetInitSysConfig());
    }
    ESP_ERROR_CHECK(InitSysConfig());

    //init  file systems
    init_rom_fs("/espfs");
    init_spi_fs("/data");

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
    /*Start PPP modem*/
    if (GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled)
        PPPModemStart();
#endif

    /*LoRaWAN start if enabled*/
#if CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    if (GetSysConf()->lorawanSettings.Flags1.bIsLoRaWANEnabled)
    {
        LoRaWANStart();
    }
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    /*Start Ethernet connection*/
    if (GetSysConf()->ethSettings.Flags1.bIsETHEnabled)
        EthStart();
#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    /*Start WiFi connection*/
    if (GetSysConf()->wifiSettings.Flags1.bIsWiFiEnabled)
    {
        WiFiStart();
    }
#endif

    /*Start services depends on client connection*/
#if CONFIG_WEBGUIAPP_GPRS_ENABLE || CONFIG_WEBGUIAPP_ETHERNET_ENABLE || CONFIG_WEBGUIAPP_WIFI_ENABLE
    ESP_ERROR_CHECK(start_file_server());
    if (GetSysConf()->sntpClient.Flags1.bIsGlobalEnabled)
        StartTimeGet();
    //regTimeSyncCallback(&TimeObtainHandler);
    //mDNSServiceStart();

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled
            || GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
    {
        MQTTRun();
    }
#endif
#endif

#if CONFIG_WEBGUIAPP_UART_TRANSPORT_ENABLE
    InitSerialPort();
#endif

    return ESP_OK;
}

static void InitSysIO(void)
{
#if (MAIN_FUNCTIONAL_BUTTON_GPIO >= 0)
    esp_rom_gpio_pad_select_gpio(MAIN_FUNCTIONAL_BUTTON_GPIO);
    gpio_set_direction(MAIN_FUNCTIONAL_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(MAIN_FUNCTIONAL_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_pullup_en(MAIN_FUNCTIONAL_BUTTON_GPIO);
#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
#if  CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN >= 0
gpio_set_direction(CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN, GPIO_MODE_OUTPUT);
gpio_set_level(CONFIG_MODEM_DEVICE_POWER_CONTROL_PIN, 0);
#endif
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
#if CONFIG_ETH_SPI_PHY_RST0_GPIO >=0
gpio_set_direction(CONFIG_ETH_SPI_PHY_RST0_GPIO, GPIO_MODE_OUTPUT);
gpio_set_level(CONFIG_ETH_SPI_PHY_RST0_GPIO, 0);
#endif
#endif

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_LOGI(TAG, "System GPIO's initialized OK");

}

static void InitSysSPI(void)
{
#ifdef CONFIG_WEBGUIAPP_SPI_ENABLE
    xSemaphoreSPIHandle = xSemaphoreCreateBinaryStatic(&xSemaphoreSPIBuf);
    xSemaphoreGive(xSemaphoreSPIHandle);
    spi_bus_config_t buscfg = { .miso_io_num = CONFIG_SPI_MISO_GPIO,
            .mosi_io_num = CONFIG_SPI_MOSI_GPIO, .sclk_io_num =
            CONFIG_SPI_SCLK_GPIO, .quadwp_io_num = -1, .quadhd_io_num =
                    -1, };
    ESP_ERROR_CHECK(
                    spi_bus_initialize(CONFIG_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI BUS initialize OK");
#else
    ESP_LOGI(TAG, "SPI BUS disabeled in config");
#endif
}

static void InitSysI2C(void)
{
#ifdef CONFIG_WEBGUIAPP_I2C_ENABLE
    i2c_config_t i2c_config = { .mode = I2C_MODE_MASTER, .sda_io_num =
    CONFIG_I2C_SDA_GPIO, .scl_io_num = CONFIG_I2C_SCL_GPIO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE, .scl_pullup_en =
                    GPIO_PULLUP_ENABLE, .master.clk_speed = CONFIG_I2C_CLOCK };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(TAG, "I2C initialized OK");
#else
    ESP_LOGI(TAG, "I2C bus disabeled in config");
#endif
}

static void ResetSysConfig(SYS_CONFIG *Conf)
{
    char id[4];
    char id2[9];
    GetChipId((uint8_t*) id);
    BytesToStr((unsigned char*) id, (unsigned char*) id2, 4);
    id2[8] = 0x00;
    memcpy(Conf->ID, id2, 9);

    UINT32_VAL d;
    GetChipId((uint8_t*) d.v);
    snprintf(Conf->SN, 11, "%010u", (unsigned int) swap(d.Val));

    Conf->ColorSheme = CONFIG_WEBGUIAPP_DEFAULT_COLOR_SCHEME;

    memcpy(Conf->NetBIOSName, CONFIG_WEBGUIAPP_HOSTNAME,
           sizeof(CONFIG_WEBGUIAPP_HOSTNAME));
    memcpy(Conf->SysName, CONFIG_WEBGUIAPP_USERNAME,
           sizeof(CONFIG_WEBGUIAPP_USERNAME));
    memcpy(Conf->SysPass, CONFIG_WEBGUIAPP_USERPASS,
           sizeof(CONFIG_WEBGUIAPP_USERPASS));

    memcpy(Conf->OTAURL, CONFIG_WEBGUIAPP_OTA_HOST, sizeof(CONFIG_WEBGUIAPP_OTA_HOST));
    Conf->OTAAutoInt = CONFIG_WEBGUIAPP_OTA_AUTOUPDATE_PERIOD;

#if CONFIG_WEBGUIAPP_OTA_AUTOUPDATE_ENABLE
    Conf->Flags1.bIsOTAEnabled = true;
#else
    Conf->Flags1.bIsOTAEnabled = false;
#endif

#if   CONFIG_WEBGUIAPP_OTA_RESET_ENABLE
    Conf->Flags1.bIsResetOTAEnabled = true;
#else
    Conf->Flags1.bIsResetOTAEnabled = false;
#endif

#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    Conf->wifiSettings.Flags1.bIsWiFiEnabled = CONFIG_WEBGUIAPP_WIFI_ON;
    memcpy(Conf->wifiSettings.ApSSID, CONFIG_WEBGUIAPP_WIFI_SSID_AP,
           sizeof(CONFIG_WEBGUIAPP_WIFI_SSID_AP));
    strcat(Conf->wifiSettings.ApSSID, "_");
    strcat(Conf->wifiSettings.ApSSID, Conf->ID);

    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_STA,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.InfIPAddr);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_MASK_STA,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.InfMask);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_GATEWAY_STA,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.InfGateway);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_AP,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.ApIPAddr);

    Conf->wifiSettings.WiFiMode = 3; //AP+STA mode
    memcpy(Conf->wifiSettings.ApSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_AP,
           sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_AP));
    memcpy(Conf->wifiSettings.InfSSID, CONFIG_WEBGUIAPP_WIFI_SSID_STA,
           sizeof(CONFIG_WEBGUIAPP_WIFI_SSID_STA));
    memcpy(Conf->wifiSettings.InfSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_STA,
           sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_STA));

    Conf->wifiSettings.Flags1.bIsDHCPEnabled = false;
#if CONFIG_WEBGUIAPP_WIFI_DHCP_ON
    Conf->wifiSettings.Flags1.bIsDHCPEnabled = true;
#endif
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT,
                         (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr3);
    Conf->wifiSettings.MaxPower = 80;
    Conf->wifiSettings.AP_disab_time = 10;
#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
Conf->ethSettings.Flags1.bIsETHEnabled = false;
#if CONFIG_WEBGUIAPP_ETHERNET_ON
Conf->ethSettings.Flags1.bIsETHEnabled = true;
#endif
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_IP_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.IPAddr);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_MASK_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Mask);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_GATEWAY_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Gateway);

Conf->ethSettings.Flags1.bIsDHCPEnabled = false;
#if CONFIG_WEBGUIAPP_ETHERNET_DHCP_DEFAULT
Conf->ethSettings.Flags1.bIsDHCPEnabled = true;
#endif


esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr1);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr2);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr3);
#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
Conf->gsmSettings.Flags1.bIsGSMEnabled = false;
#if CONFIG_WEBGUIAPP_GPRS_ON
Conf->gsmSettings.Flags1.bIsGSMEnabled = true;
#endif
memcpy(Conf->gsmSettings.APN, CONFIG_MODEM_PPP_APN,sizeof(CONFIG_MODEM_PPP_APN));
memcpy(Conf->gsmSettings.login, CONFIG_MODEM_PPP_APN,sizeof(CONFIG_MODEM_PPP_APN));
memcpy(Conf->gsmSettings.password, CONFIG_MODEM_PPP_APN,sizeof(CONFIG_MODEM_PPP_APN));

esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr1);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr2);
esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr3);

#endif

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    Conf->mqttStation[0].Flags1.bIsGlobalEnabled = false;
#if CONFIG_WEBGUIAPP_MQTT_ON
    Conf->mqttStation[0].Flags1.bIsGlobalEnabled = true;
#endif
    memcpy(Conf->mqttStation[0].ServerAddr, CONFIG_WEBGUIAPP_MQTT_SERVER_URL,
           sizeof(CONFIG_WEBGUIAPP_MQTT_SERVER_URL));
    Conf->mqttStation[0].ServerPort = CONFIG_WEBGUIAPP_MQTT_SERVER_PORT;

    memcpy(Conf->mqttStation[0].SystemName, CONFIG_WEBGUIAPP_MQTT_SYSTEM_NAME,
           sizeof(CONFIG_WEBGUIAPP_MQTT_SYSTEM_NAME));
    memcpy(Conf->mqttStation[0].GroupName, CONFIG_WEBGUIAPP_MQTT_GROUP_NAME,
           sizeof(CONFIG_WEBGUIAPP_MQTT_GROUP_NAME));

    Conf->mqttStation[0].ClientID[0] = 0x00;
    strcat(Conf->mqttStation[0].ClientID, CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_1);
    strcat(Conf->mqttStation[0].ClientID, "-");
    strcat(Conf->mqttStation[0].ClientID, Conf->ID);

    memcpy(Conf->mqttStation[0].UserName, CONFIG_WEBGUIAPP_MQTT_USERNAME,
           sizeof(CONFIG_WEBGUIAPP_MQTT_USERNAME));
    memcpy(Conf->mqttStation[0].UserPass, CONFIG_WEBGUIAPP_MQTT_PASSWORD,
           sizeof(CONFIG_WEBGUIAPP_MQTT_PASSWORD));
#if CONFIG_WEBGUIAPP_MQTT_CLIENTS_NUM == 2
    Conf->mqttStation[1].Flags1.bIsGlobalEnabled = false;
    memcpy(Conf->mqttStation[1].ServerAddr, CONFIG_WEBGUIAPP_MQTT_SERVER_URL, sizeof(CONFIG_WEBGUIAPP_MQTT_SERVER_URL));
    Conf->mqttStation[1].ServerPort = CONFIG_WEBGUIAPP_MQTT_SERVER_PORT;
    memcpy(Conf->mqttStation[1].SystemName, CONFIG_WEBGUIAPP_MQTT_SYSTEM_NAME,
           sizeof(CONFIG_WEBGUIAPP_MQTT_SYSTEM_NAME));
    memcpy(Conf->mqttStation[1].GroupName, CONFIG_WEBGUIAPP_MQTT_GROUP_NAME, sizeof(CONFIG_WEBGUIAPP_MQTT_GROUP_NAME));

    Conf->mqttStation[1].ClientID[0] = 0x00;
    strcat(Conf->mqttStation[1].ClientID, CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_2);
    strcat(Conf->mqttStation[1].ClientID, "-");
    strcat(Conf->mqttStation[1].ClientID, Conf->ID);

    memcpy(Conf->mqttStation[1].UserName, CONFIG_WEBGUIAPP_MQTT_USERNAME, sizeof(CONFIG_WEBGUIAPP_MQTT_USERNAME));
    memcpy(Conf->mqttStation[1].UserPass, CONFIG_WEBGUIAPP_MQTT_PASSWORD, sizeof(CONFIG_WEBGUIAPP_MQTT_PASSWORD));
#endif
#endif
    memcpy(Conf->sntpClient.SntpServerAdr, CONFIG_WEBGUIAPP_SNTP_HOST_1,
           sizeof(CONFIG_WEBGUIAPP_SNTP_HOST_1));
    memcpy(Conf->sntpClient.SntpServer2Adr, CONFIG_WEBGUIAPP_SNTP_HOST_2,
           sizeof(CONFIG_WEBGUIAPP_SNTP_HOST_2));
    memcpy(Conf->sntpClient.SntpServer3Adr, CONFIG_WEBGUIAPP_SNTP_HOST_3,
           sizeof(CONFIG_WEBGUIAPP_SNTP_HOST_3));
    Conf->sntpClient.Flags1.bIsGlobalEnabled = CONFIG_WEBGUIAPP_SNTP_AUTOUPDATE_ENABLE;
    Conf->sntpClient.TimeZone = CONFIG_WEBGUIAPP_SNTP_TIMEZONE;
    Conf->sntpClient.lat = 0.0;
    Conf->sntpClient.lon = 0.0;

#ifdef CONFIG_WEBGUIAPP_LORAWAN_ENABLE
    Conf->lorawanSettings.Flags1.bIsLoRaWANEnabled = true;
    Conf->Flags1.bIsLoRaConfirm = false;
    unsigned char temp[16] = { 0 };
    GetChipId((uint8_t*) temp + 4);
    memcpy(Conf->lorawanSettings.DevEui, temp, 8);
    StrToBytes((unsigned char*) CONFIG_LORA_APP_KEY, temp);
    memcpy(Conf->lorawanSettings.AppKey, temp, 16);
    StrToBytes((unsigned char*) CONFIG_LORA_APP_ID, temp);
    memcpy(Conf->lorawanSettings.AppEui, temp, 8);
#endif

#ifdef CONFIG_WEBGUIAPP_UART_TRANSPORT_ENABLE
    Conf->serialSettings.Flags.IsSerialEnabled = false;
    Conf->serialSettings.Flags.IsBridgeEnabled = false;
#ifdef CONFIG_WEBGUIAPP_UART_ON
    Conf->serialSettings.Flags.IsSerialEnabled = true;
#endif
#ifdef CONFIG_WEBGUIAPP_UART_TO_MQTT_BRIDGE_ENABLED
    Conf->serialSettings.Flags.IsBridgeEnabled = true;
#endif
    Conf->serialSettings.Serialmode = 1;
    Conf->serialSettings.BaudRate = CONFIG_WEBGUIAPP_UART_BAUD_RATE;
    Conf->serialSettings.DataBits = UART_DATA_8_BITS;
    Conf->serialSettings.Parity = UART_PARITY_DISABLE;
    Conf->serialSettings.StopBits = UART_STOP_BITS_1;
    Conf->serialSettings.InputBrake = 50;
#endif

#ifdef CONFIG_WEBGUIAPP_MBTCP_ENABLED
    Conf->modbusSettings.IsModbusTCPEnabled = false;
#if CONFIG_WEBGUIAPP_MBTCP_ON == 1
    Conf->modbusSettings.IsModbusTCPEnabled = true;
#endif
    Conf->modbusSettings.ModbusTCPPort = CONFIG_WEBGUIAPP_MBTCP_SERVER_PORT;
#endif
    for (int i = 0; i < CONFIG_WEBGUIAPP_CRON_NUMBER; i++)
    {
        Conf->Timers[i].num = i + 1;
        Conf->Timers[i].del = true;
        Conf->Timers[i].enab = false;
        Conf->Timers[i].prev = false;
        Conf->Timers[i].type = 0;
        Conf->Timers[i].sun_angle = 0;
        strcpy(Conf->Timers[i].name, "Timer Name");
        strcpy(Conf->Timers[i].cron, "* * * * * *");
        strcpy(Conf->Timers[i].exec, "OBJECT,ACTION,ARGUMENTS");

    }

#ifdef CONFIG_WEBGUIAPP_ASTRO_ENABLE

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
        goto nvs_operation_err;

    unsigned char sha256_saved[32];
    unsigned char sha256_calculated[32];
    unsigned char sha_print[32 * 2 + 1];
    sha_print[32 * 2] = 0x00;
    L = 32;
    err = nvs_get_blob(my_handle, "sys_conf_sha256", sha256_saved, &L);
    if (err != ESP_OK)
        goto nvs_operation_err;

    SHA256Hash((unsigned char*) SysConf, sizeof(SYS_CONFIG), sha256_calculated);
    BytesToStr(sha256_saved, sha_print, 32);
    ESP_LOGI(TAG, "Saved hash of structure is %s", sha_print);
    BytesToStr(sha256_calculated, sha_print, 32);
    ESP_LOGI(TAG, "Calculated hash of structure is %s", sha_print);

    if (memcmp(sha256_calculated, sha256_saved, L))
    {
        err = ESP_ERR_INVALID_CRC;
        goto nvs_operation_err;
    }

    nvs_close(my_handle);
    return ESP_OK;

nvs_operation_err:
    nvs_close(my_handle);
    return err;
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
        goto nvs_wr_oper_err;

    unsigned char sha256[32];
    unsigned char sha_print[32 * 2 + 1];

    SHA256Hash((unsigned char*) SysConf, sizeof(SYS_CONFIG), sha256);
    BytesToStr(sha256, sha_print, 32);
    sha_print[32 * 2] = 0x00;
    ESP_LOGI(TAG, "SHA256 of structure to write is %s", sha_print);
    L = 32;
    err = nvs_set_blob(my_handle, "sys_conf_sha256", sha256, L);
    if (err != ESP_OK)
        goto nvs_wr_oper_err;

// Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        goto nvs_wr_oper_err;

    nvs_close(my_handle);
    return ESP_OK;

nvs_wr_oper_err:
    nvs_close(my_handle);
    return err;

}

SYS_CONFIG* GetSysConf(void)
{
    return &SysConfig;
}

esp_err_t InitSysConfig(void)
{
    esp_err_t err;
    err = ReadNVSSysConfig(&SysConfig);
    if (err == ESP_ERR_INVALID_CRC || err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Reset and write default system configuration");
        ResetSysConfig(&SysConfig);
        err = WriteNVSSysConfig(&SysConfig);
        return err;
    }
    else if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Read system configuration OK");
    }
    else
        ESP_LOGW(TAG, "Error reading NVS configuration:%s", esp_err_to_name(err));
    return err;
}

esp_err_t ResetInitSysConfig(void)
{
    ESP_LOGI(TAG, "Reset and write default system configuration");
    ResetSysConfig(&SysConfig);
    return WriteNVSSysConfig(&SysConfig);
}

void DelayedRestartTask(void *pvParameter)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
void DelayedRestart(void)
{
    xTaskCreate(DelayedRestartTask, "RestartTask", 1024 * 4, (void*) 0, 3,
    NULL);
}

bool GetUserAppNeedReset(void)
{
    return isUserAppNeedReset;
}

void SetUserAppNeedReset(bool res)
{
    isUserAppNeedReset = res;
}

#define LOG_MAX_CHUNK_SIZE CONFIG_WEBGUIAPP_SYSLOG_CHUNK_SIZE
#define LOG_MAX_CHUNKS CONFIG_WEBGUIAPP_SYSLOG_MAX_CHUNKS
#define DEFAULT_LOG_FILE_NAME "syslog"
#define LOG_PARTITION "/data/"

static void ComposeLogFilename(int chunk, char *filename)
{
    char chunkstr[2];
    strcpy(filename, LOG_PARTITION);
    strcat(filename, DEFAULT_LOG_FILE_NAME);
    itoa(chunk, chunkstr, 10);
    strcat(filename, chunkstr);
    strcat(filename, ".log");
}

void SysLog(char *format, ...)
{
    char tstamp[ISO8601_TIMESTAMP_LENGTH + 2];
    static int cur_chunk = 0, isstart = 1;
    char filename[32];
    struct stat file_stat;
    FILE *f;
    ComposeLogFilename(cur_chunk, filename);

    //If first call after reboot, try to find not full chunk
    if (isstart)
    {
        while (file_stat.st_size > LOG_MAX_CHUNK_SIZE * 1024 && cur_chunk <= LOG_MAX_CHUNKS - 1)
        {
            cur_chunk++;
            ComposeLogFilename(cur_chunk, filename);
        }
        isstart = 0;
    }
    stat(filename, &file_stat);
    //next if full, else append to current
    if (file_stat.st_size > LOG_MAX_CHUNK_SIZE * 1024)
    {
        if (++cur_chunk > LOG_MAX_CHUNKS - 1)
            cur_chunk = 0;
        ComposeLogFilename(cur_chunk, filename);
        f = fopen(filename, "w");
    }
    else
        f = fopen(filename, "a");

    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s for writing", filename);
        return;
    }
    va_list arg;
    va_start(arg, format);
    va_end(arg);
    strcpy(tstamp, "\r\n");
    char ts[ISO8601_TIMESTAMP_LENGTH];
    GetISO8601Time(ts);
    strcat(tstamp, ts);
    strcat(tstamp, " ");
    fwrite(tstamp, 1, strlen(tstamp), f);
    vfprintf(f, format, arg);
    fclose(f);
}

void LogFile(char *fname, char *format, ...)
{
    char filename[32];
    char tstamp[ISO8601_TIMESTAMP_LENGTH + 2];
    strcpy(filename, "/data/");
    strcat(filename, fname);
    FILE *f = fopen(filename, "a");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s for writing", filename);
        return;
    }
    va_list arg;
    va_start(arg, format);
    va_end(arg);
    strcpy(tstamp, "\r\n");
    char ts[ISO8601_TIMESTAMP_LENGTH];
    GetISO8601Time(ts);
    strcat(tstamp, ts);
    strcat(tstamp, " ");
    fwrite(tstamp, 1, strlen(tstamp), f);
    vfprintf(f, format, arg);
    fclose(f);
    ESP_LOGI(TAG, "File written to %s", filename);
}

