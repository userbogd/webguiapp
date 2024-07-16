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
 *   File name: FileBlockHandler.c
 *     Project: WebguiappTemplate
 *  Created on: 2024-03-11
 *      Author: bogd
 * Description:	
 */

#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>
#include "esp_vfs.h"
#include "mbedtls/base64.h"

#define TAG "RAW_MEM_API"

/*
 {
 "transid": 623787878,
 "opertype" : 1,                [1-READ, 2-DELETE, 3-WRITE]
 "part": 0,                     []
 "parts": 3,                    []
 "mem_object": "testfile.txt",  [Resource name string]
 "size : 4096,                  [Data block size in bytes]
 "dat" : "" ,                   [Data block BASE64 encoded]
 }
 */


#define READ_ORERATION 1
#define DELETE_ORERATION 2
#define WRITE_ORERATION 3

static cb_blockdata_transfer_t FileTransaction = {
        .opertype = 0
};

esp_err_t ParseBlockDataObject(char *argres, cb_blockdata_transfer_t *ft)
{
    struct jReadElement result;
    jRead(argres, "", &result);
    if (result.dataType != JREAD_OBJECT)
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:not an object\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'transid'", &result);
    if (result.elements == 1)
    {
        int trans = atoi((char*) result.pValue);

        if (ft->open_file_timeout != 0)
        {
            if (trans != ft->transid)
            {
                ESP_LOGW(TAG, "Attempt second client access while first is not finished");
                snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"Device is busy, please try later\"");
                return ESP_ERR_NOT_FINISHED;
            }
        }
        else
        {
            ft->transid = trans;
            //ESP_LOGI(TAG, "New transaction with id %d", ft->transid);
        }

    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'transid' not found, frontend update needed\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'opertype'", &result);
    if (result.elements == 1)
    {
        ft->opertype = atoi((char*) result.pValue);
        if (ft->opertype < 1 || ft->opertype > 3)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'operation' value not in [1...3]\"");
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'operation' not found\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'parts'", &result);
    if (result.elements == 1)
    {
        ft->parts = atoi((char*) result.pValue);
        if (ft->parts < 0 || ft->parts > 20000)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'parts' value not in [0...20000]\"");
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'parts' not found\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'part'", &result);
    if (result.elements == 1)
    {
        ft->part = atoi((char*) result.pValue);
        if (ft->parts < 0 || ft->part > ft->parts - 1)
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'part' value not in [0...(parts-1)]\"");
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'part' not found\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'mem_object'", &result);
    if (result.elements == 1)
    {
        if (result.bytelen > 0 && result.bytelen < MEM_OBLECT_MAX_LENGTH)
        {
            memcpy(ft->mem_object, (char*) result.pValue, result.bytelen);
            ft->mem_object[result.bytelen] = 0x00;
        }
        else
        {
            snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:'mem_object' length out of range\"");
            return ESP_ERR_INVALID_ARG;
        }
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'mem_object' not found\"");
        return ESP_ERR_INVALID_ARG;
    }

    jRead(argres, "{'size'", &result);
    if (result.elements == 1)
    {
        ft->size = atoi((char*) result.pValue);
    }
    else
    {
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:key 'size' not found\"");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;

}

void FileBlockHandler(char *argres, int rw, const char *path)
{

    if (ParseBlockDataObject(argres, &FileTransaction) != ESP_OK)
        return;

    FileTransaction.open_file_timeout = BLOCK_OPERATION_TIMEOUT;   //restart timeout on every block

    //Phase of file operation calculate
    FileTransaction.operphase = 0;           //Simple read or write
    if (FileTransaction.parts == 1)
        FileTransaction.operphase = 3;      //Only one block handle (open and close in one iteration)
    else if (FileTransaction.part == 0)
        FileTransaction.operphase = 1;      //First block of multipart data (open file)
    else if (FileTransaction.part == (FileTransaction.parts - 1))
        FileTransaction.operphase = 2;      //Last block of multipart data (close file)

    strcpy(FileTransaction.filepath, path);
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
        else if (FileTransaction.opertype == WRITE_ORERATION)
        {
            if (stat(FileTransaction.filepath, &FileTransaction.file_stat) == 0)
            {
                if (unlink(FileTransaction.filepath) != 0)
                {
                    ESP_LOGE("FILE_API", "Delete file ERROR : %s", FileTransaction.mem_object);
                    snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:File is already exists and can't be deleted\"");
                }
            }
        }
    }

    if (FileTransaction.opertype == DELETE_ORERATION)
    {
        unlink(FileTransaction.filepath);
        FileTransaction.open_file_timeout = 0;
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
            FileTransaction.f = NULL;
            FileTransaction.open_file_timeout = 0;
            ESP_LOGI("FILE_API", "Close file for read : %s", FileTransaction.mem_object);
        }
        dlen = 0;
        mbedtls_base64_encode(NULL, dlen, &olen, scr, slen);
        dst = (unsigned char*) malloc(olen);
        dlen = olen;
        mbedtls_base64_encode(dst, dlen, &olen, scr, slen);

        struct jWriteControl jwc;
        jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_OBJECT, JW_COMPACT);
        jwObj_int(&jwc, "transid", FileTransaction.transid);
        jwObj_int(&jwc, "opertype", FileTransaction.opertype);
        jwObj_int(&jwc, "parts", FileTransaction.parts);
        jwObj_int(&jwc, "part", FileTransaction.part);

        jwObj_string(&jwc, "mem_object", FileTransaction.mem_object);
        jwObj_int(&jwc, "size", read);
        jwObj_string(&jwc, "dat", (char*) dst);
        jwClose(&jwc);
        free(scr);
        free(dst);

    }
    else if (FileTransaction.opertype == WRITE_ORERATION)
    {
        struct jReadElement result;
        jRead(argres, "{'dat'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > 0)
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
                    FileTransaction.f = NULL;
                    FileTransaction.open_file_timeout = 0;
                    ESP_LOGI("FILE_API", "Close file for write : %s", FileTransaction.mem_object);
                }

                free(dst);
                struct jWriteControl jwc;
                jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_OBJECT, JW_COMPACT);
                jwObj_int(&jwc, "transid", FileTransaction.transid);
                jwObj_int(&jwc, "opertype", FileTransaction.opertype);
                jwObj_int(&jwc, "parts", FileTransaction.parts);
                jwObj_int(&jwc, "part", FileTransaction.part);
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

void FileBlockTimeoutCounter()
{
    if (FileTransaction.open_file_timeout)
    {
        if (--FileTransaction.open_file_timeout == 0)
        {
            if (FileTransaction.f != NULL)
                fclose(FileTransaction.f);
        }
    }
    //ESP_LOGI(TAG, "Block timeout %d", FileTransaction.open_file_timeout);
}

void FileListHandler(char *argres, int rw, const char *path)
{
    char entrypath[FILE_PATH_MAX];
    char entrysize[16];
    const char *entrytype = "file";
    struct dirent *entry;
    struct stat entry_stat;
    DIR *dir = opendir(path);
    const size_t dirpath_len = strlen(path);
    strlcpy(entrypath, path, sizeof(entrypath));

    if (!dir)
    {
        ESP_LOGE("FILE_API", "Failed to stat dir : %s", path);
        snprintf(argres, VAR_MAX_VALUE_LENGTH, "\"ERROR:DIR_NOT_FOUND\"");
        return;
    }

    struct jWriteControl jwc;
    jwOpen(&jwc, argres, VAR_MAX_VALUE_LENGTH, JW_ARRAY, JW_COMPACT);
    while ((entry = readdir(dir)) != NULL)
    {
        strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
        entrytype = (entry->d_type == DT_DIR ? "directory" : "file");
        if (stat(entrypath, &entry_stat) == -1)
        {
            ESP_LOGE("FILE_API", "Failed to stat %s : %s", entrytype, entry->d_name);
            continue;
        }

        jwArr_object(&jwc);
        jwObj_raw(&jwc, "sel", "false");
        jwObj_string(&jwc, "name", (char*) entry->d_name);
        jwObj_int(&jwc, "size", entry_stat.st_size);
        jwEnd(&jwc);
    }
    jwClose(&jwc);
}

