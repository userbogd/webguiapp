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

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "driver/spi_master.h"

esp_err_t spi_device_polling_transmit_synchronized(spi_device_handle_t handle, spi_transaction_t *trans_desc);

bool GetUserAppNeedReset(void);
void SetUserAppNeedReset(bool res);

#endif /* COMPONENTS_WEBGUIAPPCOMPONENT_INCLUDE_WEBGUIAPP_H_ */
