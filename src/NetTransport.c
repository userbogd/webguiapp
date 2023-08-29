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
 *   File name: NetTransport.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include <SysConfiguration.h>
#include "NetTransport.h"
#include "sdkconfig.h"
#include "lwip/netif.h"

#define TAG "NET_TRANSPORT"

extern struct netif *netif_default;

esp_netif_t* GetNetifCurrentDefault()
{
    return netif_default;
}

esp_netif_t* GetNetifByName(char *name)
{
    return esp_netif_get_handle_from_ifkey(name);
}

void SetDefaultNetIF(esp_netif_t *IF)
{
    for (struct netif *pri = netif_list; pri != NULL; pri = pri->next)
    {
        char ifname[4];
        esp_netif_get_netif_impl_name(IF, ifname);
        if ((pri->name[0] == ifname[0]) && (pri->name[1] == ifname[1]))
        {
            ESP_LOGI(TAG, "Interface priority set to %s", ifname);
            netif_set_default(pri);
        }
    }
}

void PrintDefaultNetIF(void)
{
    char ifname[3];
    ifname[0] = netif_default->name[0];
    ifname[1] = netif_default->name[1];
    ifname[2] = 0x00;
    ESP_LOGI(TAG, "Default IF is:%s", ifname);
}

void GetDefaultNetIFName(char *name)
{
    name[0] = netif_default->name[0];
    name[1] = netif_default->name[1];
    name[2] = 0x00;
}

void NextDefaultNetIF(void)
{
    for (struct netif *netif = netif_list; netif != NULL; netif = netif->next)
    {
        if ((netif->name[0] == netif_default->name[0]) && (netif->name[1] == netif_default->name[1]))
        {
            netif = netif->next;
            if (!netif || ((netif->name[0] == 'l') && (netif->name[1] == 'o')))
            {
                netif = netif_list;
            }
            if (netif)
            {
                netif_set_default(netif);
                PrintDefaultNetIF();
                return;
            }
        }
    }
}

void PrintNetifs(void)
{
// Create an esp_netif pointer to store current interface
    esp_netif_t *interface = esp_netif_next(NULL);
    // Stores the unique interface descriptor, such as "PP1", etc
    char ifdesc[7];
    ifdesc[6] = 0;  // Ensure null terminated string
    ESP_LOGI(TAG, "***************************************");
    while (interface != NULL)
    {
        esp_netif_get_netif_impl_name(interface, ifdesc);
        ESP_LOGI(TAG, "IF NAME:%s", ifdesc);
        ESP_LOGI(TAG, "IF KEY:%s", esp_netif_get_ifkey(interface));
        ESP_LOGI(TAG, "IF DESCR:%s", esp_netif_get_desc(interface));
        ESP_LOGI(TAG, "IF ROUTE PRIO:%d", esp_netif_get_route_prio(interface));

        if (esp_netif_is_netif_up(interface))
            ESP_LOGI(TAG, "IF STATE:UP");
        else
            ESP_LOGI(TAG, "IF STATE:DOWN");
        uint8_t mac_addr[6] = { 0 };

        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(interface, &ip_info) == ESP_OK)
        {
            char buf[16];
            ESP_LOGI(TAG, "IP:%s", esp_ip4addr_ntoa(&ip_info.ip, buf, 16));
            ESP_LOGI(TAG, "GW:%s", esp_ip4addr_ntoa(&ip_info.gw, buf, 16));
            ESP_LOGI(TAG, "MASK:%s", esp_ip4addr_ntoa(&ip_info.netmask, buf, 16));
        }

        esp_netif_dns_info_t dns_info;
        if (esp_netif_get_dns_info(interface, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK)
        {
            char buf[16];
            ESP_LOGI(TAG, "DNS MAIN:%s", esp_ip4addr_ntoa((esp_ip4_addr_t* ) &dns_info.ip, buf, 16));
        }
        if (esp_netif_get_dns_info(interface, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK)
        {
            char buf[16];
            ESP_LOGI(TAG, "DNS BACKUP:%s", esp_ip4addr_ntoa((esp_ip4_addr_t* ) &dns_info.ip, buf, 16));
        }

        if (esp_netif_get_mac(interface, mac_addr) == ESP_OK)
        {
            ESP_LOGI(TAG, "MAC:%02x-%02x-%02x-%02x-%02x-%02x",
                     mac_addr[0],
                     mac_addr[1],
                     mac_addr[2],
                     mac_addr[3],
                     mac_addr[4],
                     mac_addr[5]);
        }

        ESP_LOGI(TAG, "-----------------------");

        // Get the next interface
        interface = esp_netif_next(interface);
    }
    ESP_LOGI(TAG, "***************************************");
}

