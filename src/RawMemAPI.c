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

#define READ_ORERATION 1
#define DELETE_ORERATION 2
#define WRITE_ORERATION 3

static const char *dirpath = "/data/";
#define MEM_OBLECT_MAX_LENGTH 32

typedef struct
{
    int opertype;
    int operphase;
    //int offset;
    int size;
    char mem_object[MEM_OBLECT_MAX_LENGTH];
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;
    FILE *f;
    int open_file_timeout;
} file_transaction_t;

static file_transaction_t FileTransaction = {
        .opertype = 0
};

void RawDataHandler(char *argres, int rw)
{
    struct jReadElement result;
    jRead(argres, "", &result);
    if (result.dataType != JREAD_OBJECT)
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:not an object\"");
        return;
    }

    jRead(argres, "{'opertype'", &result);
    if (result.elements == 1)
    {
        FileTransaction.opertype = atoi((char*) result.pValue);
        if (FileTransaction.opertype < 1 || FileTransaction.opertype > 3)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'operation' value not in [1...3]\"");
            return;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'operation' not found\"");
        return;
    }

    jRead(argres, "{'operphase'", &result);
    if (result.elements == 1)
    {
        FileTransaction.operphase = atoi((char*) result.pValue);
        if (FileTransaction.operphase < 0 || FileTransaction.operphase > 3)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'file_operation' value not in [0...3]\"");
            return;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'file_operation' not found\"");
        return;
    }

    jRead(argres, "{'mem_object'", &result);
    if (result.elements == 1)
    {
        if (result.bytelen > 0 && result.bytelen < MEM_OBLECT_MAX_LENGTH)
        {
            memcpy(FileTransaction.mem_object, (char*) result.pValue, result.bytelen);
            FileTransaction.mem_object[result.bytelen] = 0x00;
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

    jRead(argres, "{'size'", &result);
    if (result.elements == 1)
    {
        FileTransaction.size = atoi((char*) result.pValue);
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'size' not found\"");
        return;
    }
    //ESP_LOGI(TAG, "Got memory object %s, with offest %d and size %d", mem_object, offset, size);

    strcpy(FileTransaction.filepath, dirpath);
    strcat(FileTransaction.filepath, FileTransaction.mem_object);

    if (FileTransaction.operphase == 1 || FileTransaction.operphase == 3)
    {
        if (FileTransaction.opertype == READ_ORERATION || FileTransaction.opertype == DELETE_ORERATION)
        {
            if (stat(FileTransaction.filepath, &FileTransaction.file_stat) == -1)
            {
                ESP_LOGE("FILE_API", "File does not exist : %s", FileTransaction.mem_object);
                snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:DIR_NOT_FOUND\"");
                return;
            }
        }
    }

    if (FileTransaction.opertype == DELETE_ORERATION)
    {
        unlink(FileTransaction.filepath);
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"DELETED OK\"");
        return;
    }
    else if (FileTransaction.opertype == READ_ORERATION)
    {
        if (FileTransaction.operphase == 1 || FileTransaction.operphase == 3)
        {
            FileTransaction.f = fopen(FileTransaction.filepath, "r");
            ESP_LOGI("FILE_API", "Open file for read : %s", FileTransaction.mem_object);
        }

        unsigned char *scr, *dst;
        size_t dlen, olen, slen;
        if (FileTransaction.f == NULL)
        {
            ESP_LOGE(TAG, "Failed to open file %s for writing", FileTransaction.mem_object);
            return;
        }
        scr = (unsigned char*) malloc(FileTransaction.size + 1);
        if (scr == NULL)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR: no memory to handle request\"");
            return;
        }

        int read = fread(scr, 1, FileTransaction.size, FileTransaction.f);
        scr[read] = 0x00;
        slen = read;
        if (FileTransaction.operphase == 2 || FileTransaction.operphase == 3)
        {
            fclose(FileTransaction.f);
            ESP_LOGI("FILE_API", "Close file for read : %s", FileTransaction.mem_object);
        }
        dlen = 0;
        mbedtls_base64_encode(NULL, dlen, &olen, scr, slen);
        dst = (unsigned char*) malloc(olen);
        dlen = olen;
        mbedtls_base64_encode(dst, dlen, &olen, scr, slen);

        struct jWriteControl jwc;
        jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_OBJECT, JW_COMPACT);
        jwObj_int(&jwc, "operation", FileTransaction.opertype);
        jwObj_string(&jwc, "mem_object", FileTransaction.mem_object);
        jwObj_int(&jwc, "size", read);
        jwObj_string(&jwc, "dat", (char*) dst);
        jwClose(&jwc);
        free(scr);
        free(dst);
    }
    else if (FileTransaction.opertype == WRITE_ORERATION)
    {
        jRead(argres, "{'dat'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > 0 && result.bytelen <= FileTransaction.size)
            {
                unsigned char *dst;
                size_t dlen, olen;
                if (FileTransaction.operphase == 1 || FileTransaction.operphase == 3)
                {
                    FileTransaction.f = fopen(FileTransaction.filepath, "a");
                    ESP_LOGI("FILE_API", "Open file for write : %s", FileTransaction.mem_object);
                }
                if (FileTransaction.f == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file %s for writing", FileTransaction.mem_object);
                    return;
                }
                dlen = 0;
                mbedtls_base64_decode(NULL, dlen, &olen, (unsigned char*) result.pValue, (size_t) result.bytelen);
                dst = (unsigned char*) malloc(olen);
                dlen = olen;
                mbedtls_base64_decode(dst, dlen, &olen, (unsigned char*) result.pValue, (size_t) result.bytelen);
                //ESP_LOGI("FILE_API", "File write operation BEGIN");
                int write = fwrite((char*) dst, olen, 1, FileTransaction.f);
                //ESP_LOGI("FILE_API", "File write operation END");
                if (FileTransaction.operphase == 2 || FileTransaction.operphase == 3)
                {
                    fclose(FileTransaction.f);
                    ESP_LOGI("FILE_API", "Close file for write : %s", FileTransaction.mem_object);
                }

                free(dst);
                struct jWriteControl jwc;
                jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_OBJECT, JW_COMPACT);
                jwObj_int(&jwc, "operation", FileTransaction.opertype);
                jwObj_string(&jwc, "mem_object", FileTransaction.mem_object);
                jwObj_int(&jwc, "size", write);
                jwClose(&jwc);
            }
            else
            {
                snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'dat' length out of range\"");
                return;
            }
        }
        else
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'dat' not found\"");
            return;
        }

    }

}
