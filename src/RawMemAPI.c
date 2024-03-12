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
 *   File name: RawMemAPI.c
 *     Project: WebguiappTemplate
 *  Created on: 2024-03-11
 *      Author: bogd
 * Description:	
 */

#include "RawMemAPI.h"
#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>
#include "esp_vfs.h"
#include "mbedtls/base64.h"

#define TAG "RAW_MEM_API"

/*
 {
 "operation" : 1,
 "mem_object": "testfile.txt",
 "offset" : 2000,
 "size : 4096,
 "data" : "" ,
 }
 */

static const char *dirpath = "/data/";
#define MEM_OBLECT_MAX_LENGTH 32

void RawDataHandler(char *argres, int rw)
{
    int operation = 0;
    int offset;
    int size;
    char mem_object[MEM_OBLECT_MAX_LENGTH];
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;

    struct jReadElement result;
    jRead(argres, "", &result);
    if (result.dataType != JREAD_OBJECT)
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:not an object\"");
        return;
    }

    jRead(argres, "{'operation'", &result);
    if (result.elements == 1)
    {
        operation = atoi((char*) result.pValue);
        if (operation < 1 || operation > 2)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'operation' value not in [1...2]\"");
            return;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'operation' not found\"");
        return;
    }

    jRead(argres, "{'mem_object'", &result);
    if (result.elements == 1)
    {
        if (result.bytelen > 0 && result.bytelen < MEM_OBLECT_MAX_LENGTH)
        {
            memcpy(mem_object, (char*) result.pValue, result.bytelen);
            mem_object[result.bytelen] = 0x00;
        }
        else
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'mem_object' length out of range\"");
            return;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'mem_object' not found\"");
        return;
    }

    jRead(argres, "{'offset'", &result);
    if (result.elements == 1)
    {
        offset = atoi((char*) result.pValue);
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'offset' not found\"");
        return;
    }

    jRead(argres, "{'size'", &result);
    if (result.elements == 1)
    {
        size = atoi((char*) result.pValue);
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'size' not found\"");
        return;
    }

    ESP_LOGI(TAG, "Got memory object %s, with offest %d and size %d", mem_object, offset, size);

    strcpy(filepath, dirpath);
    strcat(filepath, mem_object);

    if (stat(filepath, &file_stat) == -1)
    {
        ESP_LOGE("FILE_API", "File does not exist : %s", mem_object);
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:DIR_NOT_FOUND\"");
        return;
    }

    FILE *f = fopen(filepath, "r");
    if (f == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file %s for writing", mem_object);
        return;
    }

}
