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
 *   File name: CommandProcSys.h
 *     Project: WebguiappTemplate
 *  Created on: 2023-10-09
 *      Author: bogdan
 * Description:	
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_COMMANDPROCSYS_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_COMMANDPROCSYS_H_


#include "esp_err.h"

#define EXEC_COMMAND_MAX_LENGTH (64)
#define EXEC_OBJECT_NAME_MAX_LENGTH (EXEC_COMMAND_MAX_LENGTH/4)
#define EXEC_ACTION_NAME_MAX_LENGTH (EXEC_COMMAND_MAX_LENGTH/4)
#define EXEC_ARGUMENT_MAX_LENGTH (EXEC_COMMAND_MAX_LENGTH/2)



int ExecCommand(char *cmd);
void GetSysObjectsInfo(char *data);


#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_COMMANDPROCSYS_H_ */
