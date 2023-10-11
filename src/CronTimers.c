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

//{
// "data":{
// "msgid":123456789,
// "time":"2023-06-03T12:25:24+00:00",
// "msgtype":1,"payloadtype":1, "payload":{"applytype":1,
// "variables":{
// "cronrecs":[{ "num": 1, "del": 0, "enab": 1, "prev": 0, "name": "Timer Name", "obj": 0, "act": 0,
//              "cron": "*/3 * * * * *",
//              "exec": "OUTPUTS,TEST,ARGUMENTS"
//                }]
// }}},"signature":"6a11b872e8f766673eb82e127b6918a0dc96a42c5c9d184604f9787f3d27bcef"}
#include <CronTimers.h>
#include "esp_log.h"
#include "webguiapp.h"
#include "string.h"

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

        //ESP_LOGI(TAG, "Check object %s", obj);

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
            ESP_LOGW(TAG, "Run previous CRON \"%s\" with delta %d", GetSysConf()->Timers[minimal].exec, (int )delta);
            ExecCommand(GetSysConf()->Timers[minimal].exec);

        }
    }
}

void TimeObtainHandler(struct timeval *tm)
{
    ESP_LOGW(TAG, "Current time received with value %d", (unsigned int )tm->tv_sec);
    ReloadCronSheduler();
    ExecuteLastAction(GetSystemObjects());
    ExecuteLastAction(GetCustomObjects());
    LogFile("cron.log", "Cron service started");
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

