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
 *  	 \file MQTTSysHandler.c
 *    \version 1.0
 * 		 \date 2022-08-19
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

/*!
 * Structure of JSON data for system MQTT API
 * {
 * "messid":12345,                                         //uint32_t message id for request/response context
 * "api":"2.0",                                            //string of current API version
 * "request":"GET",                                        //string request type -  "GET" or "POST" allowed
 * "url":"status.json",                                    //string url of resource (generally a file name)
 * "postdata":"param1=value&param2=value",                 //string of POST data payload (required if request is POST)
 * "reload":"true"                                         //string "true" or "false" is needed reload page after POST request (required if request is POST)
 * }
 */

#include "MQTT.h"
#include "jWrite.h"
#include "jRead.h"
#include "esp_log.h"
#include "romfs.h"
#include "HTTPServer.h"

#define MESSAGE_LENGTH 32          //base message length, mainly depended by radio requirements
#define MAX_JSON_MESSAGE 256       //max size of mqtt message to publish
#define MAX_FILE_PUBLISH    4096   //bufer for mqtt data publish
#define MAX_DYNVAR_LENTH  64
#define MAX_FILENAME_LENTH 15
#define MAX_ERROR_MESSAGE 32
#define MAX_ERROR_JSON  256
#define MAX_MESSAGE_ID 15

#define MAX_POST_DATA_LENTH 512

#define TAG "MQTTSysHandler"

const char apiver[] = "2.0";
const char tagGet[] = "GET";
const char tagPost[] = "POST";

const char* mqtt_app_err_descr[] = {
"Operation OK",
"Internal error",
"Wrong json format",
"Key 'idmess' not found",
"Key 'idmess' value too long",
"Key 'api' not found",
"API version not supported",
"Key 'request' not found",
"Unsupported HTTP method",
"Key 'url' not found",
"Key 'url' value too long",
"URL not found",
"Key 'postdata' not found",
"Key 'postdata' too long",
"File size too big",
"File is empty",
"Unknown error"
};

const char* mqtt_app_err_breef[] = {
"OK",
"INTERNAL_ERR",
"WRONG_JSON_ERR",
"NO_ID_ERR",
"ID_OVERSIZE_ERR",
"NO_API_ERR",
"VERSION_ERR",
"NO_REQUEST_ERR",
"UNSUPPORTED_METHOD_ERR",
"NO_URL_ERR",
"URL_OVERSIZE_ERR",
"URL_NOT_FOUND_ERR",
"NO_POSTDATA_ERR",
"POSTDATA_OVERSIZE_ERR",
"FILE_OVERSIZE_ERR",
"FILE_EMPTY_ERR",
"UNKNOWN_ERR"
};

typedef enum
{
    UNSUPPORTED = 0,
    GET,
    POST
} mqtt_api_request_t;

static mqtt_app_err_t ResponceWithError(int idx,
                                        espfs_file_t *file,
                                        char *id,
                                        char *url,
                                        mqtt_app_err_t api_err)
{
    espfs_fclose(file);
    char JSONErrorMess[MAX_ERROR_JSON];
    jwOpen(JSONErrorMess, MAX_ERROR_JSON, JW_OBJECT, JW_PRETTY);
    if (id[0])
        jwObj_string("messid", (char*) id);
    else
        jwObj_string("messid", "ERROR");
    jwObj_string("api", (char*) apiver);
    jwObj_string("responce", (char*) mqtt_app_err_breef[api_err]);
    jwObj_string("descript", (char*) mqtt_app_err_descr[api_err]);
    if (url[0])
        jwObj_string("url", (char*) url);
    else
        jwObj_string("url", "ERROR");
    jwEnd();
    jwClose();
    char *buf = (char*) malloc(strlen(JSONErrorMess) + 1);
    if (buf)
    {
        memcpy(buf, JSONErrorMess, strlen(JSONErrorMess));
        DATA_SEND_STRUCT DSS;
        ComposeTopic(DSS.topic,
                     GetSysConf()->mqttStation[idx].RootTopic,
                     "UPLINK",
                     GetSysConf()->mqttStation[idx].ClientID,
                     "SYSTEM");
        DSS.raw_data_ptr = buf;
        DSS.data_length = strlen(JSONErrorMess);
        if (xQueueSend(GetMQTTHandlesPool(idx)->mqtt_queue, &DSS, pdMS_TO_TICKS(1000)) == pdPASS)
            return API_OK;
        else
        {
            free(buf);
            return API_INTERNAL_ERR;
        }
    }
    else
    {  // ERR internal error on publish error
        return API_INTERNAL_ERR;
    }
}

static mqtt_app_err_t ResponceWithFile(int idx, espfs_file_t *file,
                                       char *id,
                                       char *url)
{
    struct espfs_stat_t stat;
    char *filebuf = NULL;
    char *outbuf = NULL;
    uint32_t fileLenth, filePtr, readBytes;
    espfs_fstat(file, &stat);
    fileLenth = stat.size;
    mqtt_app_err_t api_err = API_UNKNOWN_ERR;
    if (fileLenth > MAX_FILE_PUBLISH)
    {
        ESP_LOGE(TAG, "File is too big");
        api_err = API_FILE_OVERSIZE_ERR;
        goto file_send_err;
    }
    outbuf = (char*) malloc(MAX_FILE_PUBLISH + 256);
    filebuf = (char*) malloc(fileLenth);

    if (!outbuf || !filebuf)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        api_err = API_INTERNAL_ERR;
        goto file_send_err;
    }

    if (espfs_fread(file, filebuf, fileLenth) == 0)
    {
        ESP_LOGE(TAG, "Read no bytes from file");
        api_err = API_FILE_EMPTY_ERR;
        goto file_send_err;
    }
    espfs_fclose(file);

    jwOpen(outbuf, MAX_FILE_PUBLISH, JW_OBJECT, JW_PRETTY);
    jwObj_string("messid", (char*) id);
    jwObj_string("api", (char*) apiver);
    jwObj_string("responce", (char*) mqtt_app_err_breef[API_OK]);
    jwObj_string("descript", (char*) mqtt_app_err_descr[API_OK]);
    jwObj_string("url", (char*) url);
    jwObj_string("body", "bodydata");
    jwEnd();
    jwClose();

    char *fdata = memmem(outbuf, MAX_FILE_PUBLISH, "\"bodydata\"", strlen("\"bodydata\""));
    filePtr = 0;
    readBytes = 0;
    while (filePtr < fileLenth && readBytes < (MAX_FILE_PUBLISH - MAX_DYNVAR_LENTH))
    {
        if (filebuf[filePtr] == '~')
        {
            int k = 0;
            char DynVarName[16];
            while (filePtr < fileLenth && k < 16 && (filebuf[++filePtr] != '~'))
                DynVarName[k++] = filebuf[filePtr];
            if (filebuf[filePtr] == '~')
            {  //got valid dynamic variable name
                DynVarName[k] = 0x00;
                readBytes += HTTPPrint(NULL, &fdata[readBytes], DynVarName);
                filePtr++;
            }
        }
        else
            fdata[readBytes++] = filebuf[filePtr++];
    }
    const char tail[] = "}";
    strcat((fdata + readBytes), tail);
    free(filebuf);
    DATA_SEND_STRUCT DSS;
    ComposeTopic(DSS.topic,
                 GetSysConf()->mqttStation[idx].RootTopic,
                 "UPLINK",
                 GetSysConf()->mqttStation[idx].ClientID,
                 "SYSTEM");
    DSS.raw_data_ptr = outbuf;
    DSS.data_length = (fdata - outbuf) + readBytes + strlen(tail);
    if (xQueueSend(GetMQTTHandlesPool(idx)->mqtt_queue, &DSS, pdMS_TO_TICKS(1000)) == pdPASS)
        return API_OK;
    else
    {
        ESP_LOGE(TAG, "Failed to write mqtt queue");
        api_err = API_INTERNAL_ERR;
        goto file_send_err;
    }
file_send_err:
    free(outbuf);
    free(filebuf);
    return api_err;
}

void SystemDataHandler(char *data, uint32_t len, int idx)
{
    struct jReadElement result;
    char URL[MAX_FILENAME_LENTH + 1];
    char ID[MAX_MESSAGE_ID + 1];
    char POST_DATA[MAX_POST_DATA_LENTH + 1];
    mqtt_api_request_t req = UNSUPPORTED;
    mqtt_app_err_t api_err = API_UNKNOWN_ERR;
    ID[0] = 0x00;
    URL[0] = 0x00;
    espfs_file_t *file = NULL;

    jRead(data, "", &result);
    if (result.dataType == JREAD_OBJECT)
    {
        /*Checking and get ID message*/
        jRead(data, "{'messid'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > MAX_MESSAGE_ID)
            {
                api_err = API_ID_OVERSIZE_ERR;
                goto api_json_err;
            }
            memcpy(ID, result.pValue, result.bytelen);
            ID[result.bytelen] = 0x00;
        }
        else
        {
            api_err = API_NO_ID_ERR;
            goto api_json_err;
        }

        /*Checking and get API version*/
        jRead(data, "{'api'", &result);
        if (result.elements == 1)
        {
            if (memcmp(apiver, result.pValue, result.bytelen))
            {
                api_err = API_VERSION_ERR;
                goto api_json_err;
            }
        }
        else
        {
            api_err = API_NO_API_ERR;
            goto api_json_err;
        }

        /*Checking and get request type POST, GET or UNSUPPORTED*/
        jRead(data, "{'request'", &result);
        if (result.elements == 1)
        {
            if (!memcmp(tagGet, result.pValue, result.bytelen))
                req = GET;
            else if (!memcmp(tagPost, result.pValue, result.bytelen))
                req = POST;
        }
        else
        {
            api_err = API_NO_REQUEST_ERR;
            goto api_json_err;
        }

        /*Checking and get url*/
        jRead(data, "{'url'", &result);
        if (result.elements == 1)
        {
            if (result.bytelen > MAX_FILENAME_LENTH)
            {
                api_err = API_URL_OVERSIZE_ERR;
                goto api_json_err;
            }
            memcpy(URL, result.pValue, result.bytelen);
            URL[result.bytelen] = 0x00;
            file = espfs_fopen(fs, URL);
            if (!file)
            {
                api_err = API_URL_NOT_FOUND_ERR;
                goto api_json_err;
            }

        }
        else
        {
            api_err = API_NO_URL_ERR;
            goto api_json_err;
        }

        if (req == POST)
        {
            /*Checking and get POST DATA*/
            jRead(data, "{'postdata'", &result);
            if (result.elements == 1)
            {
                if (result.bytelen > MAX_POST_DATA_LENTH)
                {
                    api_err = API_POSTDATA_OVERSIZE_ERR;
                    goto api_json_err;
                }
                memcpy(POST_DATA, result.pValue, result.bytelen);
                POST_DATA[result.bytelen] = 0x00;
            }
            else
            {
                api_err = API_NO_POSTDATA_ERR;
                goto api_json_err;
            }

            HTTPPostApp(NULL, URL, POST_DATA);

            jRead(data, "{'reload'", &result);
            if (result.elements == 1 && !memcmp("true", result.pValue, result.bytelen))
            {
                if (ResponceWithFile(idx, file, ID, URL) == API_OK)
                    return;
                else
                    goto api_json_err;
            }
            else
            {
                if (ResponceWithError(idx, file, ID, URL, API_OK) != API_OK)
                    ESP_LOGE(TAG, "Failed to allocate memory for file MQTT message");
            }
            return;
        }
        else if (req == GET)
        {
            //Here GET handler, send file wrapped into json
            if (ResponceWithFile(idx, file, ID, URL) == API_OK)
                return;
            else
                goto api_json_err;
        }
        else
        {
            api_err = API_UNSUPPORTED_METHOD_ERR;
            goto api_json_err;
        }
    }
    else
    {  //ERR no json format
        api_err = API_WRONG_JSON_ERR;
        goto api_json_err;
    }

api_json_err:
    ESP_LOGE(TAG, "ERROR:%s:%s", mqtt_app_err_breef[api_err], mqtt_app_err_descr[api_err]);
    if (ResponceWithError(idx, file, ID, URL, api_err) != API_OK)
        ESP_LOGE(TAG, "Failed to allocate memory for file MQTT message");
}
