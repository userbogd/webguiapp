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
#include <stdlib.h>

#define TAG "CRON_TIMER"

static cron_job *JobsList[CONFIG_WEBGUIAPP_CRON_NUMBER];
static char cron_express_error[CRON_EXPRESS_MAX_LENGTH];

static int GetSunEvent(uint8_t event, uint32_t unixt, float ang);

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

void CronRecordsInterface(char *argres, int rw)
{
    if (rw)
    {
        struct jReadElement result;
        cron_timer_t T = { 0 };
        jRead(argres, "", &result);
        if (result.dataType == JREAD_ARRAY)
        {
            int i;
            for (i = 0; i < result.elements; i++)
            {
                T.num = jRead_int(argres, "[*{'num'", &i);
                T.del = jRead_int(argres, "[*{'del'", &i);
                T.enab = jRead_int(argres, "[*{'enab'", &i);
                T.prev = jRead_int(argres, "[*{'prev'", &i);
                T.type = jRead_int(argres, "[*{'type'", &i);
                T.sun_angle = (float) jRead_double(argres, "[*{'sun_angle'", &i);
                jRead_string(argres, "[*{'name'", T.name, sizeof(T.name), &i);
                jRead_string(argres, "[*{'cron'", T.cron, sizeof(T.cron), &i);
                jRead_string(argres, "[*{'exec'", T.exec, sizeof(T.exec), &i);

                if (T.type > 0)
                {
                    time_t now;
                    time(&now);
                    int min = GetSunEvent((T.type == 1) ? 0 : 1, now, T.sun_angle);
                    sprintf(T.cron, "0 %d %d * * *", min % 60, min / 60);
                    //ESP_LOGI(TAG, "Set CRON to sun event with %dh %dm", min / 60, min % 60);
                }

                memcpy(&GetSysConf()->Timers[T.num - 1], &T, sizeof(cron_timer_t));
            }
            ReloadCronSheduler();
        }
    }

    struct jWriteControl jwc;
    jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_ARRAY, JW_COMPACT);
    for (int idx = 0; idx < CRON_TIMERS_NUMBER; idx++)
    {
        cron_timer_t T;
        memcpy(&T, &GetSysConf()->Timers[idx], sizeof(cron_timer_t));
        jwArr_object(&jwc);
        jwObj_int(&jwc, "num", (unsigned int) T.num);
        jwObj_int(&jwc, "del", (T.del) ? 1 : 0);
        jwObj_int(&jwc, "enab", (T.enab) ? 1 : 0);
        jwObj_int(&jwc, "prev", (T.prev) ? 1 : 0);
        jwObj_int(&jwc, "type", (unsigned int) T.type);
        jwObj_double(&jwc, "sun_angle", (double) T.sun_angle);
        jwObj_string(&jwc, "name", T.name);
        jwObj_string(&jwc, "cron", T.cron);
        jwObj_string(&jwc, "exec", T.exec);
        jwEnd(&jwc);
    }
    jwClose(&jwc);

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

static float Lat, Lon, Ang;

static int GetSunEvent(uint8_t event, uint32_t unixt, float ang);

uint16_t NumberDayFromUnix(uint32_t t)
{
    time_t clock;
    struct tm *tp;
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
    double tt;
    tt = GetSunEvent(0, t, SUN_ANG);
    if (tt > 0)
        srTime = tt;
    else
        srTime = 0xffff; //no valid sinrise time
    tt = GetSunEvent(1, t, SUN_ANG);
    if (tt > 0)
        ssTime = tt;
    else
        ssTime = 0xffff; //no valid sunset time

    ESP_LOGI("ASTRO", "Day number %d", NumberDayFromUnix(t));
    ESP_LOGI("ASTRO", "Sanrise %dh %dm", srTime / 60 + 2, srTime - (srTime / 60 * 60));
    ESP_LOGI("ASTRO", "Sanset %dh %dm", ssTime / 60 + 2, ssTime - (ssTime / 60 * 60));

}

uint16_t GetSunrise(void)
{
    return srTime;
}

uint16_t GetSunset(void)
{
    return ssTime;
}

static int GetSunEvent(uint8_t event, uint32_t unixt, float ang)
{
    float lngHour, t, M, L, RA, sinDec, cosDec, cosH, H, T, UT;
    float Lquadrant, RAquadrant;
    float zen;
    int day = NumberDayFromUnix(unixt);
    if (ang == 0)
        zen = zenith + (float) ang; //sunrise/set
    else
        zen = 90.0 + (float) ang; //twilight
    lngHour = GetSysConf()->sntpClient.lon / 15;
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
    cosH = (cos(zen * C) - (sinDec * sin(GetSysConf()->sntpClient.lat * C)))
            / (cosDec * cos(GetSysConf()->sntpClient.lat * C));

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
    return (int) floor(UT * 60.0);
}

void AstroRecordsInterface(char *argres, int rw)
{
    /*
     if (rw)
     {
     struct jReadElement result;
     struct jReadElement arr;
     jRead(argres, "", &result);
     if (result.dataType == JREAD_OBJECT)
     {
     GetSysConf()->Astro.lat = (float)jRead_double(argres, "{'lat'", 0);
     GetSysConf()->Astro.lon = (float)jRead_double(argres, "{'lon'", 0);

     jRead(argres, "{'records'", &arr);
     char *asto_rec = (char*) arr.pValue;
     for (int i = 0; i < arr.elements; i++)
     {
     astro_timer_t T;
     T.num = jRead_int(asto_rec, "[*{'num'", &i);
     T.del = jRead_int(asto_rec, "[*{'del'", &i);
     T.enab = jRead_int(asto_rec, "[*{'enab'", &i);
     T.rise = jRead_int(asto_rec, "[*{'rise'", &i);

     T.sensor_enab = jRead_int(asto_rec, "[*{'sensor_enab'", &i);
     T.sensor_angle = (float) jRead_double(asto_rec, "[*{'sensor_angle'", &i);
     T.main_angle = (float) jRead_double(asto_rec, "[*{'main_angle'", &i);
     //T.sensor_time = jRead_int(asto_rec, "[*{'sensor_time'", &i);
     //T.main_time = jRead_int(asto_rec, "[*{'main_time'", &i);

     jRead_string(asto_rec, "[*{'name'", T.name, sizeof(T.name), &i);
     jRead_string(asto_rec, "[*{'exec'", T.exec, sizeof(T.exec), &i);

     time_t now;
     time(&now);
     T.sensor_time = GetSunEvent((T.rise) ? 0 : 1, now, T.sensor_angle);
     T.main_time = GetSunEvent((T.rise) ? 0 : 1, now, T.main_angle);
     memcpy(&GetSysConf()->Astro.records[T.num - 1], &T, sizeof(astro_timer_t));
     }
     }
     }

     struct jWriteControl jwc;
     jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_OBJECT, JW_COMPACT);
     jwObj_double(&jwc, "lat", GetSysConf()->Astro.lat);
     jwObj_double(&jwc, "lon", GetSysConf()->Astro.lon);
     jwObj_array(&jwc, "records");
     for (int idx = 0; idx < CRON_TIMERS_NUMBER; idx++)
     {
     astro_timer_t T;
     memcpy(&T, &GetSysConf()->Astro.records[idx], sizeof(astro_timer_t));
     jwArr_object(&jwc);
     jwObj_int(&jwc, "num", (unsigned int) T.num);
     jwObj_int(&jwc, "del", (T.del) ? 1 : 0);
     jwObj_int(&jwc, "enab", (T.enab) ? 1 : 0);
     jwObj_int(&jwc, "rise", (T.rise) ? 1 : 0);
     jwObj_int(&jwc, "sensor_enab", (T.sensor_enab) ? 1 : 0);
     jwObj_double(&jwc, "sensor_angle", (double) T.sensor_angle);
     jwObj_int(&jwc, "sensor_time", T.sensor_time);

     jwObj_double(&jwc, "main_angle", (double) T.main_angle);
     jwObj_int(&jwc, "main_time", T.main_time);

     jwObj_string(&jwc, "name", T.name);
     jwObj_string(&jwc, "exec", T.exec);
     jwEnd(&jwc);
     }
     jwEnd(&jwc);
     jwClose(&jwc);

     }

     void InitAstro()
     {
     */

}

