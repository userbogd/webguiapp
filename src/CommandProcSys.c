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

#define OBJECTS_NUMBER_SYS (1)
#define EXEC_ACTIONS_MAX_NUMBER_SYS (2)

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
static int ExecSysCommand(char *cmd);

int (*CustomExecCommand)(char *cmd);
void regCustomExecCommand(int (*custom_exec)(char *cmd))
{
    CustomExecCommand = custom_exec;
}

static void SYSTEM_TEST_handle(char *obj, char *com, char *arg)
{
    ESP_LOGI(TAG, "INPUTS handler command %s with argument %s", com, arg);
}
static void SYSTEM_REBOOT_handle(char *obj, char *com, char *arg)
{
    ESP_LOGI(TAG, "SYSTEM handler command %s with argument %s", com, arg);
}

typedef struct
{
    int index;
    char object_name[EXEC_OBJECT_NAME_MAX_LENGTH];
    char allowed_actions[EXEC_ACTIONS_MAX_NUMBER_SYS][EXEC_ACTION_NAME_MAX_LENGTH];
    void (*command_handlers[EXEC_ACTIONS_MAX_NUMBER_SYS])(char *obj, char *com, char *arg);
} obj_struct_t;

const obj_struct_t com_obj_arr[] = {
        {
                .index = 0,
                .object_name = "SYSTEM",
                .allowed_actions = { "TEST", "REBOOT" },
                .command_handlers = { &SYSTEM_TEST_handle, &SYSTEM_REBOOT_handle }
        }
};

void GetSysObjectsInfo(char *data)
{
    struct jWriteControl jwc;
    jwOpen(&jwc, data, VAR_MAX_VALUE_LENGTH, JW_ARRAY, JW_COMPACT);
    for (int idx = 0; idx < OBJECTS_NUMBER_SYS; idx++)
    {
        jwArr_object(&jwc);
        jwObj_string(&jwc, "object", com_obj_arr[idx].object_name);
        jwObj_array(&jwc, "actions");
        for (int i = 0; i < EXEC_ACTIONS_MAX_NUMBER_SYS; i++)
        {
            if ((com_obj_arr[idx].allowed_actions[i])[0] != NULL)
                jwArr_string(&jwc, com_obj_arr[idx].allowed_actions[i]);
        }
        jwEnd(&jwc);
        jwEnd(&jwc);
    }
    jwClose(&jwc);
}

int ExecCommand(char *cmd)
{
    int err = ExecSysCommand(cmd);
    if(err != 4)
    {
        if (err > 0)
            ESP_LOGW(TAG, "Command execution ERROR: %s",exec_errors[err]);
        return err;
    }

    if (CustomExecCommand != 0)
        err = CustomExecCommand(cmd);

    if (err > 0)
        ESP_LOGW(TAG, "Command execution ERROR: %s",exec_errors[err]);
    return err;
}

static int ExecSysCommand(char *cmd)
{
    return ExecCommandParse(cmd);
}

static int ExecCommandParse(char *cmd)
{
    char *obj = NULL, *com = NULL, *arg = NULL;
    int err = 0;
    int commlen = strlen(cmd);
    if (commlen > EXEC_COMMAND_MAX_LENGTH)
        return 1;
    char comm[EXEC_COMMAND_MAX_LENGTH + 1];
    const char del1 = ',';
    const char del2 = 0x00;
    strcpy(comm, cmd);
    obj = strtok(comm, &del1);
    com = strtok(NULL, &del1);
    arg = strtok(NULL, &del2);
    if (!obj)
        return 2;
    if (!com)
        return 3;

    err = 4;
    for (int idx = 0; idx < OBJECTS_NUMBER_SYS; idx++)
    {
        if (!strcmp(obj, com_obj_arr[idx].object_name))
        {
            err = 5;
            for (int i = 0; i < EXEC_ACTIONS_MAX_NUMBER_SYS; i++)
            {
                if (!strcmp(com, com_obj_arr[idx].allowed_actions[i]))
                {
                    if (com_obj_arr[idx].command_handlers[i] != NULL)
                    {
                        com_obj_arr[idx].command_handlers[i](obj, com, arg);
                        err = 0;
                    }
                    else
                        err = 6;
                }
            }
        }
    }
    return err;
}

