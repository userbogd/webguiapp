 /*! Copyright 2022 Bogdan Pilyugin
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
 *  	 \file webguiapp.h
 *    \version 1.0
 * 		 \date 2022-08-21
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEBGUIAPPCOMPONENT_INCLUDE_WEBGUIAPP_H_
#define COMPONENTS_WEBGUIAPPCOMPONENT_INCLUDE_WEBGUIAPP_H_

#include "HTTPServer.h"
#include "MQTT.h"
#include "LoRaWAN.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "SystemApplication.h"
#include "CommandProcSys.h"
#include "ShiftRegisterSPI.h"

esp_err_t spi_device_polling_transmit_synchronized(spi_device_handle_t handle, spi_transaction_t *trans_desc);
void SetAppVars( rest_var_t* appvars, int size);
bool GetUserAppNeedReset(void);
void SetUserAppNeedReset(bool res);
void LogFile(char *fname, char *format, ...);
void SysLog(char *format, ...);

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

#endif /* COMPONENTS_WEBGUIAPPCOMPONENT_INCLUDE_WEBGUIAPP_H_ */
