/*! Copyright 2023 Bogdan Pilyugin
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
 *  	 \file SysComm.c
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

/*
 //Example of SET [msgtype=1] variables [payloadtype=1]
 {
 "data":{
 "msgid":123456789,
 "time":"2023-06-03T12:25:24+00:00",
 "msgtype":1,
 "payloadtype":1,
 "payload":{
 "applytype":1,
 "variables":{
 "wifi_mode":"",
 "wifi_sta_ip":"",
 "wifi_sta_mask":"",
 }
 }},
 "signature":"6a11b872e8f766673eb82e127b6918a0dc96a42c5c9d184604f9787f3d27bcef"}
 */

#include "webguiapp.h"
#include "SystemApplication.h"
#include "mbedtls/md.h"

#define TAG "SysComm"

#define MAX_JSON_DATA_SIZE 1024
sys_error_code (*CustomPayloadTypeHandler)(data_message_t *MSG);
esp_err_t (*CustomSaveConf)(void);

void regCustomPayloadTypeHandler(sys_error_code (*payload_handler)(data_message_t *MSG))
{
    CustomPayloadTypeHandler = payload_handler;
}
void regCustomSaveConf(esp_err_t (custom_saveconf)(void))
{
    CustomSaveConf = custom_saveconf;
}

static sys_error_code PayloadType_1_Handler(data_message_t *MSG)
{
    struct jReadElement result;
    const char *err_br;
    const char *err_desc;

    if (!(MSG->parsedData.msgType == DATA_MESSAGE_TYPE_COMMAND || MSG->parsedData.msgType == DATA_MESSAGE_TYPE_REQUEST))
        return SYS_ERROR_PARSE_MSGTYPE;
    struct jWriteControl jwc;
    jwOpen(&jwc, MSG->outputDataBuffer, MSG->outputDataLength, JW_OBJECT, JW_COMPACT);
    jwObj_object(&jwc, "data");
    jwObj_int(&jwc, "msgid", MSG->parsedData.msgID);
    char time[RFC3339_TIMESTAMP_LENGTH];
    GetRFC3339Time(time);
    jwObj_string(&jwc, "time", time);
    jwObj_int(&jwc, "msgtype", DATA_MESSAGE_TYPE_RESPONSE);
    jwObj_int(&jwc, "payloadtype", 1);
    jwObj_object(&jwc, "payload");
    jwObj_int(&jwc, "applytype", 0);
    jwObj_object(&jwc, "variables");

    jRead(MSG->inputDataBuffer, "{'data'{'payload'{'variables'", &result);
    if (result.dataType == JREAD_OBJECT)
    { //Write variables
        char VarName[VAR_MAX_NAME_LENGTH];
        char *VarValue = malloc(VAR_MAX_VALUE_LENGTH);
        if (!VarValue)
            return SYS_ERROR_NO_MEMORY;

        for (int i = 0; i < result.elements; ++i)
        {
            jRead_string(MSG->inputDataBuffer, "{'data'{'payload'{'variables'{*", VarName,
            VAR_MAX_NAME_LENGTH,
                         &i);
            const char parsevar[] = "{'data'{'payload'{'variables'{'";
            char expr[sizeof(parsevar) + VAR_MAX_NAME_LENGTH];
            strcpy(expr, parsevar);
            strcat(expr, VarName);
            strcat(expr, "'");

            jRead_string(MSG->inputDataBuffer, expr, VarValue, VAR_MAX_VALUE_LENGTH, &i);
#if REAST_API_DEBUG_MODE
            ESP_LOGI(TAG, "Got write variable %s:%s", VarName, VarValue);
#endif
            esp_err_t res = ESP_ERR_INVALID_ARG;
            rest_var_types tp = VAR_ERROR;
            if (MSG->parsedData.msgType == DATA_MESSAGE_TYPE_COMMAND)
            { //Write variables
                res = SetConfVar(VarName, VarValue, &tp);
                if (res == ESP_OK)
                    GetConfVar(VarName, VarValue, &tp);
                else
                    strcpy(VarValue, esp_err_to_name(res));
            }
            else
            { //Read variables
                res = GetConfVar(VarName, VarValue, &tp);
                if (res != ESP_OK)
                    strcpy(VarValue, esp_err_to_name(res));
            }
            //Response with actual data
            if (tp == VAR_STRING || tp == VAR_IPADDR || tp == VAR_ERROR || tp == VAR_PASS)
                jwObj_string(&jwc, VarName, VarValue);
            else
                jwObj_raw(&jwc, VarName, VarValue);

        }
        free(VarValue);
    }
    else
        return SYS_ERROR_PARSE_VARIABLES;

    jwEnd(&jwc);
    jwEnd(&jwc);
    GetSysErrorDetales((sys_error_code) MSG->err_code, &err_br, &err_desc);
    jwObj_string(&jwc, "error", (char*) err_br);
    jwObj_string(&jwc, "error_descr", (char*) err_desc);
    jwEnd(&jwc);

    char *datap = strstr(MSG->outputDataBuffer, "\"data\":");
    if (datap)
    {
        datap += sizeof("\"data\":") - 1;
        SHA256hmacHash((unsigned char*) datap, strlen(datap), (unsigned char*) "mykey", sizeof("mykey"),
                       MSG->parsedData.sha256);
        unsigned char sha_print[32 * 2 + 1];
        BytesToStr(MSG->parsedData.sha256, sha_print, 32);
        sha_print[32 * 2] = 0x00;
#if REAST_API_DEBUG_MODE
        ESP_LOGI(TAG, "SHA256 of DATA object is %s", sha_print);
#endif
        jwObj_string(&jwc, "signature", (char*) sha_print);
    }
    else
        return SYS_ERROR_SHA256_DATA;
    jwEnd(&jwc);
    jwClose(&jwc);

    jRead(MSG->inputDataBuffer, "{'data'{'payload'{'applytype'", &result);
    if (result.elements == 1)
    {
        int atype = atoi((char*) result.pValue);
        switch (atype)
        {
            case 0:
                break;
            case 1:
                WriteNVSSysConfig(GetSysConf());
                if (CustomSaveConf != NULL)
                    CustomSaveConf();
            break;
            case 2:
                WriteNVSSysConfig(GetSysConf());
                if (CustomSaveConf != NULL)
                    CustomSaveConf();
                DelayedRestart();
            break;
            default:
                return SYS_ERROR_PARSE_APPLYTYPE;
        }

    }
    else
    {
        if (MSG->parsedData.msgType == DATA_MESSAGE_TYPE_COMMAND)
            return SYS_ERROR_PARSE_APPLYTYPE;
    }

    return SYS_OK_DATA;
}

static sys_error_code DataHeaderParser(data_message_t *MSG)
{
    struct jReadElement result;
    jRead(MSG->inputDataBuffer, "{", &result);
    if (result.dataType != JREAD_OBJECT)
        return SYS_ERROR_WRONG_JSON_FORMAT;
    MSG->parsedData.msgID = 0;

    char *hashbuf = malloc(MSG->inputDataLength);
    if (hashbuf == NULL)
        return SYS_ERROR_NO_MEMORY;
    jRead_string(MSG->inputDataBuffer, "{'data'", hashbuf, MSG->inputDataLength, 0);
    if (strlen(hashbuf) > 0)
    {
        SHA256hmacHash((unsigned char*) hashbuf, strlen(hashbuf), (unsigned char*) "mykey", sizeof("mykey"),
                       MSG->parsedData.sha256);
        unsigned char sha_print[32 * 2 + 1];
        BytesToStr(MSG->parsedData.sha256, sha_print, 32);
        sha_print[32 * 2] = 0x00;
#if REAST_API_DEBUG_MODE
        ESP_LOGI(TAG, "SHA256 of DATA object is %s", sha_print);
#endif
        free(hashbuf);
    }
    else
    {
        free(hashbuf);
        return SYS_ERROR_PARSE_DATA;
    }

    jRead(MSG->inputDataBuffer, "{'signature'", &result);
    if (result.elements == 1)
    {
#if REAST_API_DEBUG_MODE
        ESP_LOGI(TAG, "Signature is %.*s", 64, (char* )result.pValue);
#endif

        //Here compare calculated and received signature;
    }
    else
        return SYS_ERROR_PARSE_SIGNATURE;

    //Extract 'messidx' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'msgid'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.msgID = jRead_long(MSG->inputDataBuffer, "{'data'{'msgid'", 0);
        if (MSG->parsedData.msgID == 0)
            return SYS_ERROR_PARSE_MESSAGEID;
    }
    else
        return SYS_ERROR_PARSE_MESSAGEID;

    //Extract 'msgtype' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'msgtype'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.msgType = atoi((char*) result.pValue);
        if (MSG->parsedData.msgType > 2 || MSG->parsedData.msgType < 1)
            return SYS_ERROR_PARSE_MSGTYPE;
    }
    else
        return SYS_ERROR_PARSE_MSGTYPE;

    //Extract 'payloadtype' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'payloadtype'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.payloadType = atoi((char*) result.pValue);
        if (MSG->parsedData.payloadType < 1 && MSG->parsedData.payloadType > 100)
            return SYS_ERROR_PARSE_PAYLOADTYPE;
    }
    else
        return SYS_ERROR_PARSE_PAYLOADTYPE;

    switch (MSG->parsedData.payloadType)
    {
        case 1:
            return PayloadType_1_Handler(MSG);
        break;

        default:
            if (CustomPayloadTypeHandler)
                CustomPayloadTypeHandler(MSG);
            else
                return SYS_ERROR_PARSE_PAYLOADTYPE;
    }

    return SYS_ERROR_UNKNOWN;
}

esp_err_t ServiceDataHandler(data_message_t *MSG)
{
    if (MSG == NULL)
    {
        ESP_LOGE(TAG, "MSG object is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (MSG->inputDataLength == 0)
    {
        ESP_LOGE(TAG, "Data for parser is 0 length");
        if (MSG != NULL)
            MSG->err_code = SYS_ERROR_UNKNOWN;
    }
    else
        MSG->err_code = (int) DataHeaderParser(MSG);

    if (MSG->err_code)
    {
        struct jWriteControl jwc;
        jwOpen(&jwc, MSG->outputDataBuffer, MSG->outputDataLength, JW_OBJECT, JW_PRETTY);
        jwObj_int(&jwc, "msgid", MSG->parsedData.msgID);
        char time[RFC3339_TIMESTAMP_LENGTH];
        GetRFC3339Time(time);
        jwObj_string(&jwc, "time", time);
        jwObj_int(&jwc, "messtype", DATA_MESSAGE_TYPE_RESPONSE);
        const char *err_br;
        const char *err_desc;
        GetSysErrorDetales((sys_error_code) MSG->err_code, &err_br, &err_desc);
        jwObj_string(&jwc, "error", (char*) err_br);
        jwObj_string(&jwc, "error_descr", (char*) err_desc);
        jwEnd(&jwc);
        jwClose(&jwc);
    }

    return ESP_OK;
}
