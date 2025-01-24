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

#include "webguiapp.h"
#include "SystemApplication.h"
#include "mbedtls/md.h"
#include <string.h>

#define TAG "SysComm"

#define MAX_JSON_DATA_SIZE 1024
sys_error_code (*CustomPayloadTypeHandler)(data_message_t *MSG);
void (*CustomSaveConf)(void);

void regCustomPayloadTypeHandler(sys_error_code (*payload_handler)(data_message_t *MSG))
{
    CustomPayloadTypeHandler = payload_handler;
}
void regCustomSaveConf(void (*custom_saveconf)(void))
{
    CustomSaveConf = custom_saveconf;
}

static sys_error_code PayloadDefaultTypeHandler(data_message_t *MSG)
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
    jwObj_string(&jwc, "srcid", GetSysConf()->ID);
    jwObj_string(&jwc, "dstid", MSG->parsedData.srcID);
    char time[ISO8601_TIMESTAMP_LENGTH];
    GetISO8601Time(time);
    jwObj_string(&jwc, "time", time);
    jwObj_int(&jwc, "msgtype", DATA_MESSAGE_TYPE_RESPONSE);
    jwObj_int(&jwc, "payloadtype", MSG->parsedData.payloadType);
    jwObj_string(&jwc, "payloadname", MSG->parsedData.payloadName);
    jwObj_object(&jwc, "payload");
    jwObj_int(&jwc, "applytype", 0);
    jwObj_object(&jwc, "variables");

    jRead(MSG->inputDataBuffer, "{'data'{'payload'{'variables'", &result);
    if (result.dataType == JREAD_OBJECT)
    { // Write variables
        char VarName[VAR_MAX_NAME_LENGTH];
        char *VarValue = malloc(VAR_MAX_VALUE_LENGTH);
        if (!VarValue)
            return SYS_ERROR_NO_MEMORY;

        for (int i = 0; i < result.elements; ++i)
        {
            jRead_string(MSG->inputDataBuffer, "{'data'{'payload'{'variables'{*", VarName, VAR_MAX_NAME_LENGTH, &i);
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
            { // Write variables
                res = SetConfVar(VarName, VarValue, &tp);
                if (tp != VAR_FUNCT)
                {
                    if (res == ESP_OK)
                        GetConfVar(VarName, VarValue, &tp);
                    else
                    {
                        strcpy(VarValue, esp_err_to_name(res));
                        tp = VAR_ERROR;
                    }
                }
            }
            else
            { // Read variables
                res = GetConfVar(VarName, VarValue, &tp);
                if (res != ESP_OK)
                    strcpy(VarValue, esp_err_to_name(res));
            }
            // Response with actual data
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
    GetSysErrorDetales((sys_error_code)MSG->err_code, &err_br, &err_desc);
    jwObj_string(&jwc, "error", (char *)err_br);
    jwObj_string(&jwc, "error_descr", (char *)err_desc);
    jwEnd(&jwc);

    char *datap = strstr(MSG->outputDataBuffer, "\"data\":");
    if (datap)
    {
        datap += sizeof("\"data\":") - 1;
        SHA256hmacHash((unsigned char *)datap, strlen(datap), (unsigned char *)"mykey", sizeof("mykey"), MSG->parsedData.sha256);
        unsigned char sha_print[32 * 2 + 1];
        BytesToStr(MSG->parsedData.sha256, sha_print, 32);
        sha_print[32 * 2] = 0x00;
#if REAST_API_DEBUG_MODE
        ESP_LOGI(TAG, "SHA256 of DATA object is %s", sha_print);
#endif
        jwObj_string(&jwc, "signature", (char *)sha_print);
    }
    else
        return SYS_ERROR_SHA256_DATA;
    jwEnd(&jwc);
    jwClose(&jwc);

    jRead(MSG->inputDataBuffer, "{'data'{'payload'{'applytype'", &result);
    if (result.elements == 1)
    {
        int atype = atoi((char *)result.pValue);
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
        SHA256hmacHash((unsigned char *)hashbuf, strlen(hashbuf), (unsigned char *)"mykey", sizeof("mykey"), MSG->parsedData.sha256);
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
        ESP_LOGI(TAG, "Signature is %.*s", 64, (char *)result.pValue);
#endif

        // Here compare calculated and received signature;
    }
    else
        return SYS_ERROR_PARSE_SIGNATURE;

    // Extract 'messidx' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'msgid'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.msgID = jRead_long(MSG->inputDataBuffer, "{'data'{'msgid'", 0);
        if (MSG->parsedData.msgID == 0)
            return SYS_ERROR_PARSE_MESSAGEID;
    }
    else
        return SYS_ERROR_PARSE_MESSAGEID;

    jRead(MSG->inputDataBuffer, "{'data'{'srcid'", &result);
    if (result.elements == 1)
    {
        jRead_string(MSG->inputDataBuffer, "{'data'{'srcid'", MSG->parsedData.srcID, 9, 0);
    }
    else
        strcpy(MSG->parsedData.srcID, "FFFFFFFF");

    jRead(MSG->inputDataBuffer, "{'data'{'dstid'", &result);
    if (result.elements == 1)
    {
        jRead_string(MSG->inputDataBuffer, "{'data'{'dstid'", MSG->parsedData.dstID, 9, 0);
    }
    else
        strcpy(MSG->parsedData.dstID, "FFFFFFFF");

    // Extract 'msgtype' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'msgtype'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.msgType = atoi((char *)result.pValue);
        if (MSG->parsedData.msgType > DATA_MESSAGE_TYPE_RESPONSE || MSG->parsedData.msgType < DATA_MESSAGE_TYPE_COMMAND)
            return SYS_ERROR_PARSE_MSGTYPE;
        if (MSG->parsedData.msgType == DATA_MESSAGE_TYPE_RESPONSE)
            return SYS_GOT_RESPONSE_MESSAGE;
    }
    else
        return SYS_ERROR_PARSE_MSGTYPE;

    // Extract 'payloadtype' or throw exception
    jRead(MSG->inputDataBuffer, "{'data'{'payloadtype'", &result);
    if (result.elements == 1)
    {
        MSG->parsedData.payloadType = atoi((char *)result.pValue);
    }
    else
        return SYS_ERROR_PARSE_PAYLOADTYPE;

    jRead(MSG->inputDataBuffer, "{'data'{'payloadname'", &result);
    if (result.elements == 1)
    {
        jRead_string(MSG->inputDataBuffer, "{'data'{'payloadname'", MSG->parsedData.payloadName, 31, 0);
    }
    else
        strcpy(MSG->parsedData.payloadName, "notset");

    sys_error_code err = SYS_ERROR_HANDLER_NOT_SET;
    switch (MSG->parsedData.payloadType)
    {
        case PAYLOAD_DEFAULT:
            err = PayloadDefaultTypeHandler(MSG);
            break;
    }
    if (err != SYS_ERROR_HANDLER_NOT_SET)
        return err;

    if (CustomPayloadTypeHandler)
        err = CustomPayloadTypeHandler(MSG);
    if (err != SYS_ERROR_HANDLER_NOT_SET)
        return err;

    return PayloadDefaultTypeHandler(MSG);
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
    {
        int er = DataHeaderParser(MSG);
        MSG->err_code = er;
    }

    if (MSG->err_code == SYS_GOT_RESPONSE_MESSAGE)
    {
        // ToDo Here handler of received data
#if REAST_API_DEBUG_MODE
        ESP_LOGI(TAG, "Got response message with msgid=%d", (int)MSG->parsedData.msgID);
#endif
        MSG->outputDataBuffer[0] = 0x00;
        MSG->outputDataLength = 0;
        return ESP_OK;
    }

    if (MSG->err_code)
    {
        struct jWriteControl jwc;
        jwOpen(&jwc, MSG->outputDataBuffer, MSG->outputDataLength, JW_OBJECT, JW_PRETTY);
        jwObj_int(&jwc, "msgid", MSG->parsedData.msgID);
        jwObj_string(&jwc, "srcid", GetSysConf()->ID);
        jwObj_string(&jwc, "dstid", MSG->parsedData.srcID);
        char time[ISO8601_TIMESTAMP_LENGTH];
        GetISO8601Time(time);
        jwObj_string(&jwc, "time", time);
        jwObj_int(&jwc, "messtype", DATA_MESSAGE_TYPE_RESPONSE);
        const char *err_br;
        const char *err_desc;
        GetSysErrorDetales((sys_error_code)MSG->err_code, &err_br, &err_desc);
        jwObj_string(&jwc, "error", (char *)err_br);
        jwObj_string(&jwc, "error_descr", (char *)err_desc);
        jwEnd(&jwc);
        jwClose(&jwc);
    }

    return ESP_OK;
}
