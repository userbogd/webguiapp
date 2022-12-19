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
 *  	 \file LoRaWAN.h
 *    \version 1.0
 * 		 \date 2022-12-19
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_LORAWAN_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_LORAWAN_H_


typedef struct
{
    char *raw_data_ptr;
    int data_length;
}LORA_DATA_SEND_STRUCT;

void LoRaWANInitJoinTask(void *pvParameter);
void LoRaWANStop(void);
void LoRaWANStart(void);
esp_err_t LORASendData(LORA_DATA_SEND_STRUCT *pdss);
void regLoRaUserReceiveHandler(
        void (*user_handler)(const char *message, int length, int port));

#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_LORAWAN_H_ */
