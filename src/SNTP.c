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
 *   File name: SNTP.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#include "esp_sntp.h"
#include "esp_timer.h"
#include "NetTransport.h"
#include "UserCallbacks.h"
#include "CronTimers.h"

#define YEAR_BASE (1900) //tm structure base year

static uint32_t UpTime = 0;


//Pointer to extend user on time got callback
static void (*time_sync_notif)(struct timeval *tv) = NULL;

void regTimeSyncCallback(void (*time_sync)(struct timeval *tv))
{
    time_sync_notif = time_sync;
}

static void initialize_sntp(void);

void SecondTickSystem(void *arg);
esp_timer_handle_t system_seconds_timer;
const esp_timer_create_args_t system_seconds_timer_args = {
        .callback = &SecondTickSystem,
        .name = "secondsTimer"
};

static void obtain_time(void *pvParameter)
{
    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void time_sync_notification_cb(struct timeval *tv)
{
    if (time_sync_notif)
        time_sync_notif(tv);
    TimeObtainHandler(tv);
}

static void initialize_sntp(void)
{

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, GetSysConf()->sntpClient.SntpServerAdr);
    esp_sntp_setservername(1, GetSysConf()->sntpClient.SntpServerAdr);
    esp_sntp_setservername(2, GetSysConf()->sntpClient.SntpServerAdr);
#else
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, GetSysConf()->sntpClient.SntpServerAdr);
  sntp_setservername(1, GetSysConf()->sntpClient.SntpServerAdr);
  sntp_setservername(2, GetSysConf()->sntpClient.SntpServerAdr);
#endif

    sntp_set_sync_interval(6 * 3600 * 1000);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

#if ESP_IDF_VERSION_MAJOR >= 5
    esp_sntp_init();
#else
  sntp_init();
#endif
}

void StartTimeGet(void)
{
    xTaskCreate(obtain_time, "ObtainTimeTask", 1024 * 4, (void*) 0, 3, NULL);
}

void GetRFC3339Time(char *t)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(t, "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
            (timeinfo.tm_year) + YEAR_BASE,
            (timeinfo.tm_mon) + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
}

void GetISO8601Time(char *t)
{
    struct tm timeinfo;
    struct timeval tp;
    gettimeofday(&tp, NULL);
    localtime_r(&tp.tv_sec, &timeinfo);
    sprintf(t, "%04d-%02d-%02dT%02d:%02d:%02d.%luZ",
            (timeinfo.tm_year) + YEAR_BASE,
            (timeinfo.tm_mon) + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec, (unsigned long)tp.tv_usec);
}

void StartSystemTimer(void)
{
    ESP_ERROR_CHECK(esp_timer_create(&system_seconds_timer_args, &system_seconds_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(system_seconds_timer, 1000000));
}

void SecondTickSystem(void *param)
{
    ++UpTime;
}

uint32_t GetUpTime(void)
{
    return UpTime;
}
