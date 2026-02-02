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
 *  	 \file UserCallbacks.c
 *    \version 1.0
 * 		 \date 2026-02-02
 *     \author Bogdan Pilyugin
 * 	    \brief User callbacks register mechanism
 *    \details
 *	\copyright Apache License, Version 2.0
 */

#include "webguiapp.h"
#define MAX_CALLBACKS 0xF

#define MAX_SNTP_CALLBACKS 8
typedef void (*time_sync_notify)(struct timeval *tv);
typedef struct {
    time_sync_notify callback;
    bool active;
} time_sync_notify_cb_t;

static int TimeNotifyCallbackCounter = 0;
time_sync_notify_cb_t TimeSyncNotifyCallbacks[MAX_SNTP_CALLBACKS] = {0};

void regTimeSyncCallback(void (*time_sync)(struct timeval *tv))
{
    for (int i = 0; i < MAX_SNTP_CALLBACKS; i++) {
        if (!TimeSyncNotifyCallbacks[i].active) {
            TimeSyncNotifyCallbacks[i].callback = time_sync;
            TimeSyncNotifyCallbacks[i].active = true;
            if (i >= TimeNotifyCallbackCounter)
                TimeNotifyCallbackCounter = i + 1;
            ESP_LOGI("SNTP", "Registered TIME SYNC NOTIFY with index %d", i);
            return;
        }
    }
}

void CallTimeSyncCallbacks(struct timeval *tm)
{
    for (int i = 0; i < TimeNotifyCallbackCounter; i++) {
        if (TimeSyncNotifyCallbacks[i].active && TimeSyncNotifyCallbacks[i].callback) {
            TimeSyncNotifyCallbacks[i].callback(tm);
        }
    }
}
