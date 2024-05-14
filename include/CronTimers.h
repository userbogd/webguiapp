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
 *   File name: CronTimers.h
 *     Project: webguiapp_ref_implement
 *  Created on: 2023-04-15
 *      Author: bogdan
 * Description:	
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_CRONTIMERS_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_CRONTIMERS_H_
#include "esp_err.h"
#include "cron.h"
#include "jobs.h"
#include "ccronexpr.h"

#define CRON_TIMERS_NUMBER (16)
#define TIMER_NAME_LENGTH (16)
#define TIMER_CRONSTRING_LENGTH (32)
#define CRON_EXPRESS_MAX_LENGTH (128)
#define TIMER_EXECSTRING_LENGTH (64)

#define CRON_OBJECTS_NUMBER (16)
#define CRON_OBJECT_NAME_LENGTH (16)

typedef struct
{
    int idx;
    char objname[CRON_OBJECT_NAME_LENGTH];
} cron_obj_t;

/**
 * Cron scheduler configuration structure
 */
typedef struct
{
    int num; /*!< Index of sheduler */
    bool del; /*!< Flag of non valid record, free for future overwrite */
    bool enab; /*!< Enable scheduler */
    bool prev; /*!< Enable to execute nearest in the past sheduled action */
    char name[TIMER_NAME_LENGTH]; /*!< Human readable name of scheduler */
    char cron[TIMER_CRONSTRING_LENGTH]; /*!< Cron expression */
    char exec[TIMER_EXECSTRING_LENGTH]; /*!< Cron command string */

} cron_timer_t;

typedef struct
{
    int num; /*!< Index of sheduler */
    bool del; /*!< Flag of non valid record, free for future overwrite */
    bool enab; /*!< Enable scheduler */
    char name[TIMER_NAME_LENGTH]; /*!< Human readable name of scheduler */

    bool rise; /*!<If event is sunrise*/
    bool sensor_enab; /*!< Enable light sensor handle */
    float sensor_angle; /*!<Sun angle start sensor checking*/
    float main_angle;/*!<Sun angle unconditional event issue*/
    int sensor_time;
    int main_time;

    char exec[TIMER_EXECSTRING_LENGTH]; /*!< Cron command string */
} astro_timer_t;

typedef struct
{
    float lat;
    float lon;
    astro_timer_t timers[CONFIG_WEBGUIAPP_CRON_NUMBER]

} astro_handle_t;

esp_err_t InitCronSheduler();
esp_err_t ReloadCronSheduler();
char* GetCronError();
void DebugTimer();

char* GetCronObjectNameDef(int idx);
char* GetCronObjectName(int idx);
char* GetCronActionName(int idx);
char* GetCronActAvail(int idx);

void TimeObtainHandler(struct timeval *tm);

/**
 * \brief Handle all actions under all objects
 * \param obj  Index of the object
 * \param act  Index of the action
 */
void custom_cron_execute(int obj, int act);

void SetSunTimes(uint32_t t);

#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_CRONTIMERS_H_ */
