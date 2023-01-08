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
 *   File name: OTA.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <string.h>
#include "sdkconfig.h"
#include "romfs.h"

#include <sys/socket.h>
#include <WebGUIAppMain.h>
#include "NetTransport.h"

TaskHandle_t ota_task_handle;
static const char *TAG = "OTAmodule";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define HASH_LEN 32
//#define CONFIG_FIRMWARE_UPGRADE_URL "https://iotronic.cloud:443/firmware/MStation2.bin"
#define REPORT_PACKETS_EVERY 100

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i)
    {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static int GetBuildNumber(char *version)
{
    char *p = version;
    int C = 0, P = 0;
    while (*(p++) != 0x00 && ++C < 32)
    {
        if (*p == '.')
            ++P;
        if (P == 3)
            return atoi(++p);
    }
    return 0;
}

esp_err_t my_esp_https_ota(const esp_http_client_config_t *config)
{
    esp_app_desc_t cur_app_info;
    esp_app_desc_t new_app_info;

    if (esp_ota_get_partition_description(esp_ota_get_running_partition(), &cur_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Current firmware");
        ESP_LOGI(TAG, "********************************");
        print_sha256(cur_app_info.app_elf_sha256, "SHA-256 for current firmware: ");
        ESP_LOGI(TAG, "IDF ver %s", cur_app_info.idf_ver);
        ESP_LOGI(TAG, "Version %s", cur_app_info.version);
        ESP_LOGI(TAG, "Build date %s", cur_app_info.date);
        ESP_LOGI(TAG, "Build time %s", cur_app_info.time);
        ESP_LOGI(TAG, "********************************");
    }
    else
        return ESP_FAIL;

    if (!config)
    {
        ESP_LOGE(TAG, "esp_http_client config not found");
        return ESP_ERR_INVALID_ARG;
    }

    esp_https_ota_config_t ota_config = {
            .http_config = config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (https_ota_handle == NULL)
    {
        return ESP_FAIL;
    }

    int size = esp_https_ota_get_image_size(https_ota_handle);
    if (size < 0)
    {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Image size %d bytes", size);

    err = esp_https_ota_get_img_desc(https_ota_handle, &new_app_info);
    if (err != ESP_OK)
    {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "********************************");
    ESP_LOGI(TAG, "New firmware");
    print_sha256(new_app_info.app_elf_sha256, "SHA-256 for new firmware: ");
    ESP_LOGI(TAG, "IDF ver %s", new_app_info.idf_ver);
    ESP_LOGI(TAG, "Version %s", new_app_info.version);
    ESP_LOGI(TAG, "Build date %s", new_app_info.date);
    ESP_LOGI(TAG, "Build time %s", new_app_info.time);
    ESP_LOGI(TAG, "********************************");

    //Here compare new and old firmware and make decision of update needed
    ESP_LOGI(TAG, "Compare versions: current build is %d, update build is :%d",
             GetBuildNumber(cur_app_info.version),
             GetBuildNumber(new_app_info.version));
    bool need_to_update = GetBuildNumber(new_app_info.version) > GetBuildNumber(cur_app_info.version);

    if (need_to_update)
    {
        ESP_LOGW(TAG, "New firmware has newer build, START update firmware");
        int countPackets = 0;
        while (1)
        {
            err = esp_https_ota_perform(https_ota_handle);
            if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
            {
                break;
            }
            if (++countPackets >= REPORT_PACKETS_EVERY)
            {
                ESP_LOGI(TAG, "Updated %d bytes", esp_https_ota_get_image_len_read(https_ota_handle));
                countPackets = 0;
            }
        }

        if (err != ESP_OK)
        {
            esp_https_ota_abort(https_ota_handle);
            return err;
        }
    }
    else
    {
        ESP_LOGI(TAG, "New firmware has NOT newer build, SKIP update firmware");
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err != ESP_OK)
    {
        return ota_finish_err;
    }
    if (need_to_update)
    {
        ESP_LOGI(TAG, "Firmware updated and now restarting...");
        esp_restart();
    }
    return ESP_OK;
}

static void OTATask(void *pvParameter)
{

    espfs_file_t *file;
    struct espfs_stat_t stat;

    //open file
    file = espfs_fopen(fs, "res/ca_cert.pem");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to read certificate file");
        goto update_error;
    }
    //get file info
    espfs_stat(fs, "res/ca_cert.pem", &stat);
    uint32_t  fileSize;
    fileSize = stat.size;
    char *certbuf = (char*) malloc(fileSize);
    if (certbuf)
    {
        espfs_fread(file, certbuf, fileSize);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        espfs_fclose(file);
        goto update_error;
    }

    esp_http_client_config_t config = {
            .url = GetSysConf()->OTAURL,
            .cert_pem = (char*) certbuf,
            .event_handler = _http_event_handler,
            .keep_alive_enable = true,
            .skip_cert_common_name_check = true,
    };

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif
    esp_err_t ret = my_esp_https_ota(&config);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Firmware upgrade completed");
    }
    else
    {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
    free(certbuf);
    espfs_fclose(file);
    update_error:


    vTaskDelete(NULL);
}

/**
 * @file
 * @brief  Start OTA update task
 *
 * @pre   Network is ready
 * @post  None
 * @return ESP_OK on success
 *         ESP_ERR_NOT_FINISHED on attempt rerun existing thread
 */

esp_err_t StartOTA(void)
{
    if (xTaskGetHandle("OTATask") != NULL)
    {
        ESP_LOGW(TAG, "OTA task is already running");
        return ESP_ERR_NOT_FINISHED;
    }
    ESP_LOGI(TAG, "Starting OTA Task");
    xTaskCreate(OTATask, "OTATask", 1024 * 8, (void*) 0, 3, NULL);
    return ESP_OK;
}


