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

#include <webguiapp.h>
#include "stdlib.h"
#include "string.h"
#include "nvs_flash.h"
#include "nvs.h"



#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"


#include "SystemConfiguration.h"
#include "romfs.h"
#include "NetTransport.h"
#include "Helpers.h"
#include "HTTPServer.h"


#define STORAGE_NAMESPACE "storage"
#define TAG "SystemConfiguration"

#define MANUAL_RESET 0

static SYS_CONFIG SysConfig;


#define SPI_LOCK_TIMEOUT_MS (1000)
#define NETWORK_START_TIMEOUT (60)

SemaphoreHandle_t xSemaphoreSPIHandle = NULL;
StaticSemaphore_t xSemaphoreSPIBuf;

static int NetworkStartTimeout = 0;
static bool isUserAppNeedReset = false;

static void InitSysIO(void);
static void InitSysSPI(void);
static void InitSysI2C(void);

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

    esp_err_t err = nvs_flash_init();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
            MANUAL_RESET == 1
#if (CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO >= 0)
            || gpio_get_level(CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO) == 0
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

    //init rom file system
    init_rom_fs("/espfs");

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
    /*Start PPP modem*/
    if (GetSysConf()->gsmSettings.Flags1.bIsGSMEnabled)
        PPPModemStart();
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
        if (GetSysConf()->wifiSettings.Flags1.bIsAP)
            WiFiAPStart();
        else
            WiFiSTAStart();
    }
#endif

    bool WiFiApOnly = false;
#if   !CONFIG_WEBGUIAPP_GPRS_ENABLE && !CONFIG_WEBGUIAPP_ETHERNET_ENABLE && CONFIG_WEBGUIAPP_WIFI_ENABLE
if(GetSysConf()->wifiSettings.Flags1.bIsAP)
    WiFiApOnly = true;
#endif

    /*Start services depends on client connection*/
#if CONFIG_WEBGUIAPP_GPRS_ENABLE || CONFIG_WEBGUIAPP_ETHERNET_ENABLE || CONFIG_WEBGUIAPP_WIFI_ENABLE
    {
        ESP_ERROR_CHECK(start_file_server());
        if (!WiFiApOnly)
        {
            //start all services
            /*Wait for interfaces connected*/
            while (!(isPPPConnected() ||
                    isWIFIConnected() ||
                    isETHConnected() ||
                    ++NetworkStartTimeout >= NETWORK_START_TIMEOUT))
                vTaskDelay(pdMS_TO_TICKS(1000));

            //Start all services needed internet connection
            StartTimeGet();

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
            if (GetSysConf()->mqttStation[0].Flags1.bIsGlobalEnabled
                    || GetSysConf()->mqttStation[1].Flags1.bIsGlobalEnabled)
            {
                MQTTRun();
            }
#endif
        }

#endif

    }
    return ESP_OK;
}

static void InitSysIO(void)
{
#if (CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO >= 0)
    gpio_pad_select_gpio(CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO);
    gpio_set_direction(CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    gpio_pullup_en(CONFIG_MAIN_FUNCTIONAL_BUTTON_GPIO);

#endif

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));
    ESP_LOGI(TAG, "System GPIO's initialized OK");

}

void spi_device_init_custom(void)
{
    xSemaphoreSPIHandle = xSemaphoreCreateBinaryStatic(&xSemaphoreSPIBuf);
    xSemaphoreGive(xSemaphoreSPIHandle);
}

esp_err_t spi_device_polling_transmit_synchronized(spi_device_handle_t handle, spi_transaction_t *trans_desc)
{
    esp_err_t res;
    if (xSemaphoreTake(xSemaphoreSPIHandle,pdMS_TO_TICKS(SPI_LOCK_TIMEOUT_MS)) == pdTRUE)
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

static void InitSysSPI(void)
{
#ifdef CONFIG_WEBGUIAPP_SPI_ENABLE


    spi_device_init_custom();
    spi_bus_config_t buscfg = {
            .miso_io_num = CONFIG_SPI_MISO_GPIO,
            .mosi_io_num = CONFIG_SPI_MOSI_GPIO,
            .sclk_io_num = CONFIG_SPI_SCLK_GPIO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(CONFIG_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI BUS initialize OK");
#else
    ESP_LOGI(TAG, "SPI BUS disabeled in config");
#endif
}

static void InitSysI2C(void)
{
#ifdef CONFIG_WEBGUIAPP_I2C_ENABLE
    i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = CONFIG_I2C_SDA_GPIO,
            .scl_io_num = CONFIG_I2C_SCL_GPIO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = CONFIG_I2C_CLOCK
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_config));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(TAG, "I2C initialized OK");
#else
    ESP_LOGI(TAG, "I2C bus disabeled in config");
#endif
}

static void ResetSysConfig(SYS_CONFIG *Conf)
{
    char id[4]; char id2[9];
    GetChipId((uint8_t*) id);
    BytesToStr((unsigned char*) id, (unsigned char*) id2, 4);
    id2[8] = 0x00;
    memcpy(Conf->ID, id2, 9);

    UINT32_VAL d;
    GetChipId((uint8_t*) d.v);
    snprintf(Conf->SN, 11, "%010u", swap(d.Val));

    memcpy(Conf->NetBIOSName, CONFIG_WEBGUIAPP_HOSTNAME, sizeof(CONFIG_WEBGUIAPP_HOSTNAME));
    memcpy(Conf->SysName, CONFIG_WEBGUIAPP_USERNAME, sizeof(CONFIG_WEBGUIAPP_USERNAME));
    memcpy(Conf->SysPass, CONFIG_WEBGUIAPP_USERPASS, sizeof(CONFIG_WEBGUIAPP_USERPASS));
    //memcpy(Conf->OTAURL, CONFIG_WEBGUIAPP_, sizeof(SYSTEM_DEFAULT_OTAURL));

    memcpy(Conf->OTAURL, SYSTEM_DEFAULT_OTAURL, sizeof(SYSTEM_DEFAULT_OTAURL));
#if CONFIG_WEBGUIAPP_WIFI_ENABLE
    Conf->wifiSettings.Flags1.bIsWiFiEnabled = CONFIG_WEBGUIAPP_WIFI_ON;
    memcpy(Conf->wifiSettings.ApSSID, CONFIG_WEBGUIAPP_WIFI_SSID_AP, sizeof(CONFIG_WEBGUIAPP_WIFI_SSID_AP));
    strcat(Conf->wifiSettings.ApSSID, "_");
    strcat(Conf->wifiSettings.ApSSID, Conf->ID);

    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfIPAddr);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_MASK_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfMask);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_GATEWAY_STA, (esp_ip4_addr_t*) &Conf->wifiSettings.InfGateway);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_WIFI_IP_AP, (esp_ip4_addr_t*) &Conf->wifiSettings.ApIPAddr);
    Conf->wifiSettings.Flags1.bIsAP = true;
    memcpy(Conf->wifiSettings.ApSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_AP, sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_AP));
    memcpy(Conf->wifiSettings.InfSSID, CONFIG_WEBGUIAPP_WIFI_SSID_STA, sizeof(CONFIG_WEBGUIAPP_WIFI_SSID_STA));
    memcpy(Conf->wifiSettings.InfSecurityKey, CONFIG_WEBGUIAPP_WIFI_KEY_STA, sizeof(CONFIG_WEBGUIAPP_WIFI_KEY_STA));

    Conf->wifiSettings.Flags1.bIsDHCPEnabled = CONFIG_WEBGUIAPP_WIFI_DHCP_ON;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->wifiSettings.DNSAddr3);

#endif

#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    Conf->ethSettings.Flags1.bIsETHEnabled = CONFIG_WEBGUIAPP_ETHERNET_ON;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_IP_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.IPAddr);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_MASK_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Mask);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_ETH_GATEWAY_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.Gateway);
    //Conf->ethSettings.Flags1.bIsDHCPEnabled = CONFIG_WEBGUIAPP_ETHERNET_DHCP_ON ;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->ethSettings.DNSAddr3);
#endif

#if CONFIG_WEBGUIAPP_GPRS_ENABLE
    Conf->gsmSettings.Flags1.bIsGSMEnabled = true;
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS1_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr1);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS2_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr2);
    esp_netif_str_to_ip4(CONFIG_WEBGUIAPP_DNS3_ADDRESS_DEFAULT, (esp_ip4_addr_t*) &Conf->gsmSettings.DNSAddr3);

#endif

#if CONFIG_WEBGUIAPP_MQTT_ENABLE
    Conf->mqttStation[0].Flags1.bIsGlobalEnabled = CONFIG_WEBGUIAPP_MQTT_ON;
    memcpy(Conf->mqttStation[0].ServerAddr, CONFIG_WEBGUIAPP_MQTT_SERVER_URL, sizeof(CONFIG_WEBGUIAPP_MQTT_SERVER_URL));
    Conf->mqttStation[0].ServerPort = CONFIG_WEBGUIAPP_MQTT_SERVER_PORT;
    memcpy(Conf->mqttStation[0].ClientID, CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_1, sizeof(CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_1));
    memcpy(Conf->mqttStation[0].RootTopic, CONFIG_WEBGUIAPP_MQTT_ROOT_TOPIC, sizeof(CONFIG_WEBGUIAPP_MQTT_ROOT_TOPIC));
    memcpy(Conf->mqttStation[0].UserName, CONFIG_WEBGUIAPP_MQTT_USERNAME, sizeof(CONFIG_WEBGUIAPP_MQTT_USERNAME));
    memcpy(Conf->mqttStation[0].UserPass, CONFIG_WEBGUIAPP_MQTT_PASSWORD, sizeof(CONFIG_WEBGUIAPP_MQTT_PASSWORD));
#if CONFIG_MQTT_CLIENTS_NUM == 2
    Conf->mqttStation[1].Flags1.bIsGlobalEnabled = CONFIG_WEBGUIAPP_MQTT_ON;
    memcpy(Conf->mqttStation[1].ServerAddr, CONFIG_WEBGUIAPP_MQTT_SERVER_URL, sizeof(CONFIG_WEBGUIAPP_MQTT_SERVER_URL));
    Conf->mqttStation[1].ServerPort = CONFIG_WEBGUIAPP_MQTT_SERVER_PORT;
    memcpy(Conf->mqttStation[1].ClientID, CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_2, sizeof(CONFIG_WEBGUIAPP_MQTT_CLIENT_ID_2));
    memcpy(Conf->mqttStation[1].RootTopic, CONFIG_WEBGUIAPP_MQTT_ROOT_TOPIC, sizeof(CONFIG_WEBGUIAPP_MQTT_ROOT_TOPIC));
    memcpy(Conf->mqttStation[1].UserName, CONFIG_WEBGUIAPP_MQTT_USERNAME, sizeof(CONFIG_WEBGUIAPP_MQTT_USERNAME));
    memcpy(Conf->mqttStation[1].UserPass, CONFIG_WEBGUIAPP_MQTT_PASSWORD, sizeof(CONFIG_WEBGUIAPP_MQTT_PASSWORD));
#endif
#endif
    memcpy(Conf->sntpClient.SntpServerAdr, DEFAULT_SNTP_SERVERNAME, sizeof(DEFAULT_SNTP_SERVERNAME));
    Conf->sntpClient.Flags1.bIsEthEnabled = DEFAULT_SNTP_ETH_IS_ENABLED;
    Conf->sntpClient.Flags1.bIsWifiEnabled = DEFAULT_SNTP_WIFI_IS_ENABLED;
    Conf->sntpClient.Flags1.bIsGlobalEnabled = DEFAULT_SNTP_GLOBAL_ENABLED;
    Conf->sntpClient.TimeZone = DEFAULT_SNTP_TIMEZONE;
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

void DelayedRestartTask(void *pvParameter)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
void DelayedRestart(void)
{
    xTaskCreate(DelayedRestartTask, "RestartTask", 1024 * 4, (void*) 0, 3, NULL);
}

bool GetUserAppNeedReset(void)
{
    return isUserAppNeedReset;
}

void SetUserAppNeedReset(bool res)
{
    isUserAppNeedReset = res;
}
