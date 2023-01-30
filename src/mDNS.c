/*! Copyright 2023 Bogdan Pilyugin
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
 *  	 \file mDNS.c
 *    \version 1.0
 * 		 \date 2023-01-29
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "mdns.h"
#include "webguiapp.h"
#include "esp_log.h"

static const char *TAG = "mDNS";

void mDNSServiceStart(void)
{
    //char temp_str[32] = { 0 };
    uint8_t sta_mac[6] = { 0 };
    ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
    char *hostname = GetSysConf()->NetBIOSName;
    //initialize mDNS
    ESP_ERROR_CHECK(mdns_init());
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);

    //set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set("testboard"));

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[] = {
            { "board", "your_bord_id" }
    };

    //initialize service
    ESP_ERROR_CHECK(mdns_service_add("WebServer", "_http", "_tcp", 80, serviceTxtData, 1));
    ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/index.html") );
    free(hostname);
}

