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

#define EXEC_ARGUMENT_MAX_LENGTH (CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH/2)

typedef struct
{
    int index;
    char object_name[CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH/4];
    char allowed_actions[CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM][CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH/4];
    void (*command_handlers[CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM])(char *obj, char *com, char *arg);
} obj_struct_t;

int ExecCommand(char *cmd);
void GetObjectsInfo(char *data);
void SetCustomObjects(obj_struct_t *obj);


#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_COMMANDPROCSYS_H_ */
