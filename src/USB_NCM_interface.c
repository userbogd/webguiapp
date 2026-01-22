/*! Copyright 2026 Bogdan Pilyugin
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
 *  	 \file USB_NCM_interface.c
 *    \version 1.0
 * 		 \date 2026-01-21
 *     \author Bogdan Pilyugin
 * 	    \brief
 *    \details
 *	\copyright Apache License, Version 2.0
 */

#include <stdio.h>
#include "SysConfiguration.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_net.h"
#include <esp_mac.h>

#include "lwip/esp_netif_net_stack.h"
#include <webguiapp.h>

static const char *TAG = "USB_NCM";
static esp_netif_t *s_netif = NULL;

esp_err_t wired_send(void *buffer, uint16_t len, void *buff_free_arg)
{
    return tinyusb_net_send_sync(buffer, len, buff_free_arg, pdMS_TO_TICKS(100));
}

static void l2_free(void *h, void *buffer)
{
    free(buffer);
}

static esp_err_t netif_transmit(void *h, void *buffer, size_t len)
{
    if (wired_send(buffer, len, NULL) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send buffer to USB!");
    }
    return ESP_OK;
}

static esp_err_t netif_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    if (s_netif)
    {
        void *buf_copy = malloc(len);
        if (!buf_copy)
        {
            return ESP_ERR_NO_MEM;
        }
        memcpy(buf_copy, buffer, len);
        return esp_netif_receive(s_netif, buf_copy, len, NULL);
    }
    return ESP_OK;
}

/**
 *  In this scenario of configuring WiFi, we setup USB-Ethernet to create a virtual network and run DHCP server,
 *  so it could assign an IP address to the PC
 *
 *           ESP32               PC
 *      |    lwip MAC=...01   |                        eth NIC MAC=...02
 *      | <DHCP server>   usb | <->  [ USB-NCM device acting as eth-NIC ]
 *      | <HTTP server>       |
 *      | (wifi-provisioning) |
 *
 *  From the PC's NIC perspective the board acts as a separate network with it's own IP and MAC address,
 *  but the virtual ethernet NIC has also it's own IP and MAC address (configured via tinyusb_net_init()).
 *  That's why we need to create the virtual network with *different* MAC address.
 *  Here, we use two different OUI range MAC addresses.
 */
esp_err_t InitUSBnetif(void)
{
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_net_config_t net_config = {
        // locally administrated address for the ncm device as it's going to be used internally
        // for configuration only
        //.mac_addr = { 0x02, 0x02, 0x11, 0x22, 0x33, 0x01 },
        .on_recv_callback = netif_recv_callback,
    };
    memcpy(net_config.mac_addr, GetSysConf()->usbnetSettings.MACAddrRemote, 6);

    esp_err_t ret = tinyusb_net_init(&net_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Cannot initialize USB Net device");
        return ret;
    }

    // with OUI range MAC to create a virtual netif running http server
    // this needs to be different to usb_interface_mac (==client)
    uint8_t lwip_addr[6] = { 0x02, 0x02, 0x11, 0x22, 0x33, 0x02 };

    esp_netif_ip_info_t _g_esp_netif_usb_ip;
    //    = { .ip = { .addr = ESP_IP4TOADDR(192, 168, 70, 1) }, .gw = { .addr = ESP_IP4TOADDR(192, 168, 70, 2) }, .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) } };

    memcpy(&_g_esp_netif_usb_ip.ip, &GetSysConf()->usbnetSettings.IPAddr, 4);
    memcpy(&_g_esp_netif_usb_ip.gw, &GetSysConf()->usbnetSettings.Gateway, 4);
    memcpy(&_g_esp_netif_usb_ip.netmask, &GetSysConf()->usbnetSettings.Mask, 4);

    // Definition of
    // 1) Derive the base config (very similar to IDF's default WiFi AP with DHCP server)
    esp_netif_inherent_config_t base_cfg = { .flags = ESP_NETIF_FLAG_AUTOUP, // Run DHCP server; set the netif "ip" immediately
        .ip_info = &_g_esp_netif_usb_ip,                                     // Use the same IP ranges as IDF's soft AP
        .if_key = "wired",                                                   // Set mame, key, priority
        .if_desc = "usb ncm config device",
        .route_prio = 100 };
    // 2) Use static config for driver's config pointing only to static transmit and free functions
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = (void *)1,             // not using an instance, USB-NCM is a static singleton (must be != NULL)
        .transmit = netif_transmit,      // point to static Tx function
        .driver_free_rx_buffer = l2_free // point to Free Rx buffer function
    };

    // 3) USB-NCM is an Ethernet netif from lwip perspective, we already have IO definitions for that:
    struct esp_netif_netstack_config lwip_netif_config = { .lwip = { .init_fn = ethernetif_init, .input_fn = ethernetif_input } };

    // Config the esp-netif with:
    //   1) inherent config (behavioural settings of an interface)
    //   2) driver's config (connection to IO functions -- usb)
    //   3) stack config (using lwip IO functions -- derive from eth)
    esp_netif_config_t cfg = { .base = &base_cfg, .driver = &driver_cfg, .stack = &lwip_netif_config };

    s_netif = esp_netif_new(&cfg);
    if (s_netif == NULL)
    {
        return ESP_FAIL;
    }
    //esp_netif_set_mac(s_netif, lwip_addr);
	esp_netif_set_mac(s_netif, GetSysConf()->usbnetSettings.MACAddrLocal);

    esp_netif_dns_info_t dns_info;
    memcpy(&dns_info, &GetSysConf()->wifiSettings.DNSAddr1, 4);
    esp_netif_set_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info);

    // set the minimum lease time
    // uint32_t lease_opt = 1;
    // esp_netif_dhcps_option(s_netif, ESP_NETIF_OP_SET, IP_ADDRESS_LEASE_TIME, &lease_opt, sizeof(lease_opt));

    // start the interface manually (as the driver has been started already)
    esp_netif_action_start(s_netif, 0, 0, 0);
    return ESP_OK;
}
