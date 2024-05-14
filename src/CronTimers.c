/* Copyright 2023 Bogdan Pilyugin
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
 *   File name: CronTimers.c
 *     Project: webguiapp_ref_implement
 *  Created on: 2023-04-15
 *      Author: bogdan
 * Description:	
 */

#include <CronTimers.h>
#include "esp_log.h"
#include "webguiapp.h"
#include "string.h"
#include <math.h>


#define TAG "CRON_TIMER"

static cron_job *JobsList[CONFIG_WEBGUIAPP_CRON_NUMBER];
static char cron_express_error[CRON_EXPRESS_MAX_LENGTH];

char* GetCronError()
{
    return cron_express_error;
}

void custom_cron_job_callback(cron_job *job)
{
    ExecCommand(((cron_timer_t*) job->data)->exec);
}

esp_err_t InitCronSheduler()
{
    esp_err_t res = ESP_OK;
    return res;
}

const char* check_expr(const char *expr)
{
    const char *err = NULL;
    cron_expr test;
    memset(&test, 0, sizeof(test));
    cron_parse_expr(expr, &test, &err);
    return err;
}

static void ExecuteLastAction(obj_struct_t *objarr)
{
    int obj_idx;
    char objname[CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH / 4 + 1];
    for (obj_idx = 0; obj_idx < CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM; obj_idx++)
    {
        int shdl;
        time_t now;
        time(&now);
        time_t delta = now;
        int minimal = -1;

        char *obj = objarr[obj_idx].object_name;
        if (*obj == '\0')
            break;
        for (shdl = 0; shdl < CRON_TIMERS_NUMBER; shdl++)
        {
            memcpy(objname, GetSysConf()->Timers[shdl].exec, CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH / 4);
            objname[CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH / 4] = 0x00;
            char *obj_in_cron = NULL;
            obj_in_cron = strtok(objname, ",");
            if (GetSysConf()->Timers[shdl].enab &&
                    !GetSysConf()->Timers[shdl].del &&
                    GetSysConf()->Timers[shdl].prev &&
                    !strcmp(obj, obj_in_cron))

            {
                ESP_LOGI(TAG, "Find %s:%s", obj, obj_in_cron);
                cron_expr cron_exp = { 0 };
                cron_parse_expr(GetSysConf()->Timers[shdl].cron, &cron_exp, NULL);
                time_t prev = cron_prev(&cron_exp, now);
                if ((now - prev) < delta)
                {
                    delta = (now - prev);
                    minimal = shdl;
                }
            }
        }

        if (minimal != -1)
        {
            ESP_LOGI(TAG, "Run previous CRON \"%s\" with delta %d", GetSysConf()->Timers[minimal].exec, (int )delta);
            ExecCommand(GetSysConf()->Timers[minimal].exec);

        }
    }
}

void TimeObtainHandler(struct timeval *tm)
{
    ESP_LOGI(TAG, "Current time updated with value %d", (unsigned int )tm->tv_sec);
    ReloadCronSheduler();
    ExecuteLastAction(GetSystemObjects());
    ExecuteLastAction(GetCustomObjects());

}

void DebugTimer()
{
    ExecuteLastAction(GetSystemObjects());
    ExecuteLastAction(GetCustomObjects());
}

esp_err_t ReloadCronSheduler()
{
    //remove all jobs
    ESP_LOGI(TAG, "Cron stop call result %d", cron_stop());
    cron_job_clear_all();
    //check if we have jobs to run
    bool isExpressError = false;
    for (int i = 0; i < CRON_TIMERS_NUMBER; i++)
    {
        const char *err = check_expr(GetSysConf()->Timers[i].cron);
        if (err)
        {
            snprintf(cron_express_error, CRON_EXPRESS_MAX_LENGTH - 1, "In timer %d expression error:%s", i + 1, err);
            ESP_LOGE(TAG, "%s", cron_express_error);
            isExpressError = true;
            continue;
        }
        else if (!GetSysConf()->Timers[i].del && GetSysConf()->Timers[i].enab)
        {
            JobsList[i] = cron_job_create(GetSysConf()->Timers[i].cron, custom_cron_job_callback,
                                          (void*) &GetSysConf()->Timers[i]);
        }
    }
    if (!isExpressError)
        cron_express_error[0] = 0x00; //clear last cron expression parse
    int jobs_num = cron_job_node_count();
    ESP_LOGI(TAG, "In config presents %d jobs", jobs_num);

    if (jobs_num > 0)
        ESP_LOGI(TAG, "Cron start call result %d", cron_start());
    return ESP_OK;
}

/************************ ASTRO *******************************/

#define C (3.14159265/180.0)
#define B (180.0/3.14159265)
#define zenith 90.8335f

#define LAT 54.73
#define LON 20.51
#define SUN_ANG 0

static uint16_t srTime = 0;
static uint16_t ssTime = 0;

static double Lat, Lon, Ang;



static double GetSunEvent(uint8_t event, uint16_t day);

uint16_t NumberDayFromUnix(uint32_t t)
{
  time_t clock;
  struct tm * tp;
  clock = t;
  tp = gmtime(&clock);
  return ((uint16_t) tp->tm_yday) + 1;
}

void SetSunConditions(double lat, double lon, double ang)
{
    Lat = lat;
    Lon = lon;
    Ang = ang;
}

void SetSunTimes(uint32_t t)
{
  if (1)
    {
      double tt;
      tt = GetSunEvent(0, NumberDayFromUnix(t));
      if (tt > 0)
        srTime = (uint16_t) (60.0 * tt);
      else
        srTime = 0xffff; //no valid sinrise time
      tt = GetSunEvent(1, NumberDayFromUnix(t));
      if (tt > 0)
        ssTime = (uint16_t) (60.0 * tt);
      else
        ssTime = 0xffff; //no valid sunset time
    }

     ESP_LOGI("ASTRO", "Day number %d", NumberDayFromUnix(t));
     ESP_LOGI("ASTRO", "Sanrise %dh %dm", srTime/60 + 2, srTime - (srTime/60 * 60));
     ESP_LOGI("ASTRO", "Sanset %dh %dm", ssTime/60 + 2 , ssTime - (ssTime/60 * 60));

}

uint16_t GetSunrise(void)
{
  return srTime;
}

uint16_t GetSunset(void)
{
  return ssTime;
}


static double GetSunEvent(uint8_t event, uint16_t day)
{
  double lngHour, t, M, L, RA, sinDec, cosDec, cosH, H, T, UT;
  double Lquadrant, RAquadrant;
  double zen;
  if (SUN_ANG == 0)
    zen = zenith + (double) SUN_ANG; //sunrise/set
  else
    zen = 90.0 + (double) SUN_ANG; //twilight
  lngHour = LON / 15;
  if (event == 0)
    t = day + ((6 - lngHour) / 24);
  else
    t = day + ((18 - lngHour) / 24);

  M = (0.9856 * t) - 3.289;
  L = M + (1.916 * sin(M * C)) + (0.020 * sin(2 * M * C)) + 282.634;
  if (L > 360)
    {
      L = L - 360;
    }
  else if (L < 0)
    {
      L = L + 360;
    }

  RA = B * atan(0.91764 * tan(L * C));
  if (RA > 360)
    {
      RA = RA - 360;
    }
  else if (RA < 0)
    {
      RA = RA + 360;
    }

  Lquadrant = (floor(L / 90)) * 90;
  RAquadrant = (floor(RA / 90)) * 90;
  RA = RA + (Lquadrant - RAquadrant);
  RA = RA / 15;
  sinDec = 0.39782 * sin(L * C);
  cosDec = cos(asin(sinDec));
  cosH = (cos(zen * C) - (sinDec * sin(LAT * C))) / (cosDec * cos(LAT * C));

  if (event == 0)
    { //rise
      if (cosH > 1)
        return -1;
      H = 360 - B * (acos(cosH));
    }
  else
    { //set
      if (cosH < -1)
        return -1;
      H = B * (acos(cosH));
    }

  H = H / 15;
  T = H + RA - (0.06571 * t) - 6.622;

  UT = T - lngHour;

  if (UT >= 24)
    {
      UT = UT - 24;
    }
  else if (UT < 0)
    {
      UT = UT + 24;
    }
  return UT;
}



