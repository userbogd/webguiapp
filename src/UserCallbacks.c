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

#include "UserCallbacks.h"
#define MAX_CALLBACKS 0xF

typedef void (*callback_func_t)(void* context);

typedef struct {
    callback_func_t callbacks[MAX_CALLBACKS];
    void* contexts[MAX_CALLBACKS];
    int active[MAX_CALLBACKS];  // 1 if active, 0 if inactive
    int count;
} callback_manager_t;

callback_manager_t test1_cb_manager;
callback_manager_t test2_cb_manager;
callback_manager_t test3_cb_manager;

static void initCallbacksManager(callback_manager_t *mgr)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        mgr->callbacks[i] = NULL;
        mgr->contexts[i] = NULL;
        mgr->active[i] = 0;
    }
    mgr->count = 0;
}

int register_callback(callback_manager_t *mgr, callback_func_t func, void *context)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!mgr->active[i]) {
            mgr->callbacks[i] = func;
            mgr->contexts[i] = context;
            mgr->active[i] = 1;
            if (i >= mgr->count)
                mgr->count = i + 1;
            return i;
        }
    }
    return -1; // Full
}

void TriggerCallbacks(callback_manager_t *mgr)
{
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->active[i] && mgr->callbacks[i]) {
            mgr->callbacks[i](mgr->contexts[i]);
        }
    }
}


void RegisterTest1Callback(void (*funct)(int time))
{
	register_callback(&test1_cb_manager, funct , )
}
