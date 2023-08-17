/* Copyright 2022 Bogdan Pilyugin
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
 *   File name: EthTransport.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include <stdio.h>
#include <string.h>
#include <SysConfiguration.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "NetTransport.h"
#include "sdkconfig.h"
#if CONFIG_ETH_USE_SPI_ETHERNET
#include "driver/spi_master.h"
#endif
#include "esp_mac.h"

static const char *TAG = "EthTransport";
static bool isEthConn = false;

static void (*eth_reset)(uint8_t level) = NULL;
void RegEthReset(void (*eth_rst)(uint8_t level))
{
    eth_reset = eth_rst;
}

#if CONFIG_USE_SPI_ETHERNET
esp_netif_t *eth_netif_spi[CONFIG_SPI_ETHERNETS_NUM] = { NULL };

#define INIT_SPI_ETH_MODULE_CONFIG(eth_module_config, num)                                      \
    do {                                                                                        \
        eth_module_config[num].spi_cs_gpio = CONFIG_ETH_SPI_CS ##num## _GPIO;           \
        eth_module_config[num].int_gpio = CONFIG_ETH_SPI_INT ##num## _GPIO;             \
        eth_module_config[num].phy_reset_gpio = CONFIG_ETH_SPI_PHY_RST ##num## _GPIO;   \
        eth_module_config[num].phy_addr = CONFIG_ETH_SPI_PHY_ADDR ##num;                \
    } while(0)

typedef struct
{
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
} spi_eth_module_config_t;

esp_netif_t* GetETHNetifAdapter(void)
{
    return eth_netif_spi[0];
}

#endif

bool isETHConnected(void)
{
    return isEthConn;
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data)
{
    uint8_t mac_addr[6] = { 0 };
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t*) event_data;

    switch (event_id)
    {
        case ETHERNET_EVENT_CONNECTED:
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "Ethernet Link Up");
            ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0],
                     mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            isEthConn = false;
        break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet Started");
        break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet Stopped");
        break;
        default:
            break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id,
                                 void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
#if CONFIG_WEBGUIAPP_ETHERNET_ENABLE
    memcpy(&GetSysConf()->ethSettings.IPAddr, &event->ip_info.ip, sizeof(event->ip_info.ip));
    memcpy(&GetSysConf()->ethSettings.Mask, &event->ip_info.netmask, sizeof(event->ip_info.netmask));
    memcpy(&GetSysConf()->ethSettings.Gateway, &event->ip_info.gw, sizeof(event->ip_info.gw));
#endif
    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    isEthConn = true;
}

static void eth_init(void *pvParameter)
{
#if CONFIG_USE_INTERNAL_ETHERNET
    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num = CONFIG_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_ETH_PHY_LAN87XX
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);
#elif CONFIG_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#elif CONFIG_ETH_PHY_KSZ8041
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8041(&phy_config);
#elif CONFIG_ETH_PHY_KSZ8081
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8081(&phy_config);
#endif
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
#endif //CONFIG_USE_INTERNAL_ETHERNET

#if CONFIG_USE_SPI_ETHERNET
    //Reset ethernet SPI device
#if CONFIG_ETH_SPI_PHY_RST0_GPIO >=0
    gpio_set_level(CONFIG_ETH_SPI_PHY_RST0_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(CONFIG_ETH_SPI_PHY_RST0_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
#else
    if (eth_reset)
    {
        eth_reset(0);
        vTaskDelay(pdMS_TO_TICKS(10));
        eth_reset(1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
        ESP_LOGE(TAG, "ethernet chip reset pin not defined");
        ESP_ERROR_CHECK(1);
    }
#endif

    // Create instance(s) of esp-netif for SPI Ethernet(s)
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH()
            ;
    esp_netif_config_t cfg_spi = {
            .base = &esp_netif_config,
            .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    char if_key_str[10];
    char if_desc_str[10];
    char num_str[3];
    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++)
    {
        itoa(i, num_str, 10);
        strcat(strcpy(if_key_str, "ETH_SPI_"), num_str);
        strcat(strcpy(if_desc_str, "eth"), num_str);
        esp_netif_config.if_key = if_key_str;
        esp_netif_config.if_desc = if_desc_str;
        esp_netif_config.route_prio = ETH_PRIO - i;
        eth_netif_spi[i] = esp_netif_new(&cfg_spi);

    }

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config_spi = ETH_PHY_DEFAULT_CONFIG();

    // Install GPIO ISR handler to be able to service SPI Eth modlues interrupts
    //gpio_install_isr_service(0);

    // Init SPI bus
    spi_device_handle_t spi_handle[CONFIG_SPI_ETHERNETS_NUM] = { NULL };

    // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
    spi_eth_module_config_t spi_eth_module_config[CONFIG_SPI_ETHERNETS_NUM];
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 0);
#if CONFIG_SPI_ETHERNETS_NUM > 1
    INIT_SPI_ETH_MODULE_CONFIG(spi_eth_module_config, 1);
#endif

    // Configure SPI interface and Ethernet driver for specific SPI module
    esp_eth_mac_t *mac_spi[CONFIG_SPI_ETHERNETS_NUM];
    esp_eth_phy_t *phy_spi[CONFIG_SPI_ETHERNETS_NUM];
    esp_eth_handle_t eth_handle_spi[CONFIG_SPI_ETHERNETS_NUM] = { NULL };
#if CONFIG_EXAMPLE_USE_KSZ8851SNL
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20
    };

    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++) {
        // Set SPI module Chip Select GPIO
        devcfg.spics_io_num = spi_eth_module_config[i].spi_cs_gpio;

        ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_SPI_HOST, &devcfg, &spi_handle[i]));
        // KSZ8851SNL ethernet driver is based on spi driver
        eth_ksz8851snl_config_t ksz8851snl_config = ETH_KSZ8851SNL_DEFAULT_CONFIG(spi_handle[i]);

        // Set remaining GPIO numbers and configuration used by the SPI module
        ksz8851snl_config.int_gpio_num = spi_eth_module_config[i].int_gpio;
        phy_config_spi.phy_addr = spi_eth_module_config[i].phy_addr;
        phy_config_spi.reset_gpio_num = spi_eth_module_config[i].phy_reset_gpio;

        mac_spi[i] = esp_eth_mac_new_ksz8851snl(&ksz8851snl_config, &mac_config_spi);
        phy_spi[i] = esp_eth_phy_new_ksz8851snl(&phy_config_spi);
    }
#elif CONFIG_DM9051
    spi_device_interface_config_t devcfg = {
        .command_bits = 1,
        .address_bits = 7,
        .mode = 0,
        .clock_speed_hz = CONFIG_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size = 20
    };

    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++) {
        // Set SPI module Chip Select GPIO
        devcfg.spics_io_num = spi_eth_module_config[i].spi_cs_gpio;

        ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_SPI_HOST, &devcfg, &spi_handle[i]));
        // dm9051 ethernet driver is based on spi driver
        eth_dm9051_config_t dm9051_config = ETH_DM9051_DEFAULT_CONFIG(spi_handle[i]);

        // Set remaining GPIO numbers and configuration used by the SPI module
        dm9051_config.int_gpio_num = spi_eth_module_config[i].int_gpio;
        phy_config_spi.phy_addr = spi_eth_module_config[i].phy_addr;
        phy_config_spi.reset_gpio_num = spi_eth_module_config[i].phy_reset_gpio;

        mac_spi[i] = esp_eth_mac_new_dm9051(&dm9051_config, &mac_config_spi);
        phy_spi[i] = esp_eth_phy_new_dm9051(&phy_config_spi);
    }
#elif CONFIG_W5500
    spi_device_interface_config_t devcfg = {
            .command_bits = 16, // Actually it's the address phase in W5500 SPI frame
            .address_bits = 8,  // Actually it's the control phase in W5500 SPI frame
            .mode = 0,
            .clock_speed_hz = CONFIG_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
            .queue_size = 20
    };

    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++)
    {
        // Set SPI module Chip Select GPIO
        devcfg.spics_io_num = spi_eth_module_config[i].spi_cs_gpio;

        ESP_ERROR_CHECK(spi_bus_add_device(CONFIG_SPI_HOST, &devcfg, &spi_handle[i]));
        ESP_LOGI(TAG, "ETHERNET SPI device added OK");
        // w5500 ethernet driver is based on spi driver
#if ESP_IDF_VERSION_MAJOR >= 5
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_SPI_HOST, &devcfg);
#else
        eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle[i]);
#endif
        // Set remaining GPIO numbers and configuration used by the SPI module
        w5500_config.int_gpio_num = spi_eth_module_config[i].int_gpio;
        phy_config_spi.phy_addr = spi_eth_module_config[i].phy_addr;
        phy_config_spi.reset_gpio_num = spi_eth_module_config[i].phy_reset_gpio;

        mac_spi[i] = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
        phy_spi[i] = esp_eth_phy_new_w5500(&phy_config_spi);
    }
#endif //CONFIG_EXAMPLE_USE_W5500

    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++)
    {
        esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac_spi[i], phy_spi[i]);
        ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, &eth_handle_spi[i]));

        /* The SPI Ethernet module might not have a burned factory MAC address, we cat to set it manually.
         02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
         */
        uint8_t mac_addr[6] = { 0 };
        if (esp_read_mac(mac_addr, ESP_MAC_ETH) == ESP_OK)
        {
            ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle_spi[i], ETH_CMD_S_MAC_ADDR, mac_addr));
        }
        else
        {
            ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle_spi[i], ETH_CMD_S_MAC_ADDR, (uint8_t[] ) {
                                                  0x02, 0x00, 0x00, 0x12, 0x34, 0x56 + i }));
        }
        // attach Ethernet driver to TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif_spi[i], esp_eth_new_netif_glue(eth_handle_spi[i])));

        //esp_netif_dns_info_t fldns;
        //esp_netif_str_to_ip4(&GetAppConf()->ethSettings.DNSAddr3, (esp_ip4_addr_t*) (&fldns.ip));
        //memcpy(&fldns.ip, &GetSysConf()->ethSettings.DNSAddr3, sizeof(esp_ip4_addr_t));
        //esp_netif_set_dns_info(eth_netif_spi[i], ESP_NETIF_DNS_FALLBACK, &fldns);
        //DHCP & DNS

        esp_netif_ip_info_t ip_info;
        memcpy(&ip_info.ip, &GetSysConf()->ethSettings.IPAddr, 4);
        memcpy(&ip_info.gw, &GetSysConf()->ethSettings.Gateway, 4);
        memcpy(&ip_info.netmask, &GetSysConf()->ethSettings.Mask, 4);
        esp_netif_dns_info_t dns_info;
        memcpy(&dns_info, &GetSysConf()->ethSettings.DNSAddr1, 4);

        esp_netif_dhcpc_stop(eth_netif_spi[i]);
        esp_netif_set_ip_info(eth_netif_spi[i], &ip_info);
        esp_netif_set_dns_info(eth_netif_spi[i], ESP_NETIF_DNS_MAIN, &dns_info);

        //esp_netif_str_to_ip4(&GetSysConf()->wifiSettings.DNSAddr3, (esp_ip4_addr_t*)(&dns_info.ip));
        memcpy(&dns_info.ip, &GetSysConf()->wifiSettings.DNSAddr3, sizeof(esp_ip4_addr_t));
        esp_netif_set_dns_info(eth_netif_spi[i], ESP_NETIF_DNS_FALLBACK, &dns_info);

        if (GetSysConf()->ethSettings.Flags1.bIsDHCPEnabled)
            esp_netif_dhcpc_start(eth_netif_spi[i]);

    }
#endif // CONFIG_ETH_USE_SPI_ETHERNET

    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
#if CONFIG_USE_INTERNAL_ETHERNET
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
#endif // CONFIG_USE_INTERNAL_ETHERNET
#if CONFIG_USE_SPI_ETHERNET
    for (int i = 0; i < CONFIG_SPI_ETHERNETS_NUM; i++)
    {
        ESP_ERROR_CHECK(esp_eth_start(eth_handle_spi[i]));
    }
#endif // CONFIG_USE_SPI_ETHERNET

    vTaskDelete(NULL);
}

void EthStart(void)
{
    xTaskCreate(eth_init, "EthInitTask", 1024 * 4, (void*) 0, 3, NULL);
}

