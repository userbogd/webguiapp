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
 *   File name: UserCallbacks.h
 *     Project: webguiapp_ref_implement
 *  Created on: 2023-04-22
 *      Author: bogdan
 * Description:	
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_USERCALLBACKS_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_USERCALLBACKS_H_

#include "webguiapp.h"

//Callback for current time obtain notification
void regTimeSyncCallback(void (*time_sync)(struct timeval *tv));

//Callback called just before OTA, aimed user can finish all job must finished on update and reboot
void regHookBeforeUpdate(void (*before_update)(void));

//MQTT incoming data callback
void regUserEventHandler(void (*event_handler)(int idx, void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data), void* user_arg);

//User App POST GET handlers register and set URL for this handlers
void regHTTPUserAppHandlers(char *url,
                            esp_err_t (*get)(httpd_req_t *req),
                            esp_err_t (*post)(httpd_req_t *req));


//User handler for various payload types
void regCustomPayloadTypeHandler(sys_error_code (*payload_handler)(data_message_t *MSG));

//User handler for save App configuration
void regCustomSaveConf(void (*custom_saveconf)(void));

#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_USERCALLBACKS_H_ */
