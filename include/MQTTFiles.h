 /* Copyright 2024 Bogdan Pilyugin
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
 *   File name: MQTTFiles.h
 *     Project: WebguiappTemplate
 *  Created on: 2024-03-11
 *      Author: bogd
 * Description:	
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_MQTTFILES_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_MQTTFILES_H_



typedef struct
{
  bool isActive;
  int operType;
  int dataLengthTotal;
  int dataLengthReady;


} mqtt_files_cb;







#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_MQTTFILES_H_ */
