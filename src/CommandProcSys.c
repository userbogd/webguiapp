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
 *   File name: CommandProcSys.c
 *     Project: WebguiappTemplate
 *  Created on: 2023-10-09
 *      Author: bogdan
 * Description:	
 */

#include "CommandProcSys.h"
#include "webguiapp.h"

#define TAG "COMMAND_PROC_SYS"

//#define MAX_OBJECTS_NUMBER CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM
//#define MAX_COMMANDS_NUMBER CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM

const char *exec_errors[] = {
        "executed OK",
        "exec string exceeded maximum length",
        "param OBJECT not found",
        "param COMMAND not found",
        "object not exists",
        "command not exists",
        "handler not set"
};


static int ExecCommandParse(char *cmd);

static void SYSTEM_TEST_handle(char *obj, char *com, char *arg)
{
    ESP_LOGI(TAG, "Object:%s, Command:%s, Argument %s", obj, com, arg);
}
static void SYSTEM_REBOOT_handle(char *obj, char *com, char *arg)
{
    ESP_LOGI(TAG, "Object:%s, Command:%s, Argument %s", obj, com, arg);
}

obj_struct_t *custom_com_obj_arr = NULL;
void SetCustomObjects(obj_struct_t *obj)
{
    custom_com_obj_arr = obj;
}

const obj_struct_t com_obj_arr[] = {
        {
                .index = 0,
                .object_name = "SYSTEM",
                .allowed_actions = { "TEST", "REBOOT", "TEST2", "TEST3", "TEST4", "TEST5" },
                .command_handlers = { &SYSTEM_TEST_handle, &SYSTEM_REBOOT_handle }
        },
        {
                .index = 0,
                .object_name = "SYSTEM1",
                .allowed_actions = { "TEST", "REBOOT", "TEST2", "TEST3", "TEST4" },
                .command_handlers = { &SYSTEM_TEST_handle, &SYSTEM_REBOOT_handle }
        },
        {
                .index = 0,
                .object_name = "SYSTEM2",
                .allowed_actions = { "TEST", "REBOOT", "TEST2", "TEST3", "TEST4" },
                .command_handlers = { &SYSTEM_TEST_handle, &SYSTEM_REBOOT_handle }
        },
        {
                .index = 0,
                .object_name = "SYSTEM3",
                .allowed_actions = { "TEST", "REBOOT", "TEST2", "TEST3", "TEST4", "TEST5" },
                .command_handlers = { &SYSTEM_TEST_handle, &SYSTEM_REBOOT_handle }
        },
        { 0 }
};

obj_struct_t* GetSystemObjects()
{
    return &com_obj_arr;
}

obj_struct_t* GetCustomObjects()
{
    return custom_com_obj_arr;
}


void GetObjectsInfo(char *data)
{
    struct jWriteControl jwc;
    jwOpen(&jwc, data, VAR_MAX_VALUE_LENGTH, JW_ARRAY, JW_COMPACT);
    for (int idx = 0; idx < CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM; idx++)
    {
        if (com_obj_arr[idx].object_name[0] == NULL)
            break;
        jwArr_object(&jwc);
        jwObj_string(&jwc, "object", com_obj_arr[idx].object_name);
        jwObj_array(&jwc, "actions");
        for (int i = 0; i < CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM; i++)
        {
            if ((com_obj_arr[idx].allowed_actions[i])[0] != NULL)
                jwArr_string(&jwc, com_obj_arr[idx].allowed_actions[i]);
        }
        jwEnd(&jwc);
        jwEnd(&jwc);
    }
    for (int idx = 0; idx < CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM; idx++)
    {
        if (custom_com_obj_arr[idx].object_name[0] == NULL)
            break;
        jwArr_object(&jwc);
        jwObj_string(&jwc, "object", custom_com_obj_arr[idx].object_name);
        jwObj_array(&jwc, "actions");
        for (int i = 0; i < CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM; i++)
        {
            if ((custom_com_obj_arr[idx].allowed_actions[i])[0] != NULL)
                jwArr_string(&jwc, custom_com_obj_arr[idx].allowed_actions[i]);
        }
        jwEnd(&jwc);
        jwEnd(&jwc);
    }

    jwClose(&jwc);
}

int ExecCommand(char *cmd)
{
    int err = ExecCommandParse(cmd);
    if(err)
        if (err > 0)
            ESP_LOGW(TAG, "Command execution ERROR: %s",exec_errors[err]);
    return err;
}

static int ExecCommandParse(char *cmd)
{
    char *obj = NULL, *com = NULL, *arg = NULL;
    //int err = 0;
    int commlen = strlen(cmd);
    if (commlen > CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH)
        return 1; //ERROR_TOO_LONG_COMMAND
    char comm[CONFIG_WEBGUIAPP_MAX_COMMAND_STRING_LENGTH + 1];

    strcpy(comm, cmd);
    obj = strtok(comm, ",");
    com = strtok(NULL, ",");
    arg = strtok(NULL, "\0");
    if (!obj)
        return 2; //ERROR_OBJECT_NOT_PARSED
    if (!com)
        return 3; //ERROR_ACTION_NOT_PARSED

    for (int idx = 0; idx < CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM; idx++)
    {
        if (!strcmp(obj, com_obj_arr[idx].object_name))
        {
            for (int i = 0; i < CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM; i++)
            {
                if (!strcmp(com, com_obj_arr[idx].allowed_actions[i]))
                {
                    if (com_obj_arr[idx].command_handlers[i] != NULL)
                    {
                        com_obj_arr[idx].command_handlers[i](obj, com, arg);
                        return 0; //EXECUTED_OK
                    }
                    else
                        return 6; //ERROR_HANDLER_NOT_IMPLEMENTED
                }
            }
            return 5; //ERROR_ACTION_NOT_FOUND
        }
    }

    for (int idx = 0; idx < CONFIG_WEBGUIAPP_MAX_OBJECTS_NUM; idx++)
    {
        if (!strcmp(obj, custom_com_obj_arr[idx].object_name))
        {
            for (int i = 0; i < CONFIG_WEBGUIAPP_MAX_COMMANDS_NUM; i++)
            {
                if (!strcmp(com, custom_com_obj_arr[idx].allowed_actions[i]))
                {
                    if (custom_com_obj_arr[idx].command_handlers[i] != NULL)
                    {
                        custom_com_obj_arr[idx].command_handlers[i](obj, com, arg);
                        return 0; //EXECUTED_OK
                    }
                    else
                        return 6; //ERROR_HANDLER_NOT_IMPLEMENTED
                }
            }
            return 5; //ERROR_ACTION_NOT_FOUND
        }
    }
    return 4; //ERROR_OBJECT_NOT_FOUND
}

