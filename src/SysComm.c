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
 "wifi_sta_gw":"",
 "wifi_ap_ip":"",
 "wifi_dns1":"",
 "wifi_dns2":"",
 "wifi_dns3":"",
 "wifi_sta_ssid":"",
 "wifi_sta_key":"",
 "wifi_ap_ssid":"",
 "wifi_ap_key":"",
 "wifi_enab":"",
 "wifi_isdhcp":"",
 "wifi_power":""
 }
 }},
 "signature":"6a11b872e8f766673eb82e127b6918a0dc96a42c5c9d184604f9787f3d27bcef"}

 //Example of GET [msgtype=2] variables [payloadtype=1]
 {
 "data":{
 "msgid":123456789,
 "time":"2023-06-03T12:25:24+00:00",
 "msgtype":2,
 "payloadtype":1,
 "payload":{
 "variables":[{"name":"netname","val":""},
 {"name":"otaurl","val":""},
 {"name":"ledenab","val":""},
 {"name":"otaint","val":""}]
 }},
 "signature":"3c1254d5b0e7ecc7e662dd6397554f02622ef50edba18d0b30ecb5d53e409bcb"}


 //Example of RESPONSE [msgtype=3] with variables [payloadtype=1]
 {
 "data":{
 "msgid":123456789,
 "time":"2023-06-03T12:25:24+00:00",
 "msgtype": 3,
 "payloadtype":1,
 "payload":{
 "variables":[{"name":"netname","val":"DEVICE_HOSTNAME"},
 {"name":"otaurl","val":"https://iotronic.cloud/firmware/firmware.bin"},
 {"name":"ledenab","val":"0"},
 {"name":"otaint","val":"3600"}]
 },
 "error":"SYS_OK",
 "error_descr":"Result successful"},
 "signature":"0d3b545b7c86274a6bf5a6e606b260f32b1999de40cb7d29d0949ecc9389cd9d"}
 */

#include "webguiapp.h"
#include "SystemApplication.h"
#include "mbedtls/md.h"

#define TAG "SysComm"

#define MAX_JSON_DATA_SIZE 1024

static esp_err_t SHA256hmacHash(unsigned char *data,
                                int datalen,
                                unsigned char *key,
                                int keylen,
                                unsigned char *res)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, key, keylen);
    mbedtls_md_hmac_update(&ctx, (const unsigned char*) data, datalen);
    mbedtls_md_hmac_finish(&ctx, res);
    mbedtls_md_free(&ctx);
    return ESP_OK;
}

static void Timestamp(char *ts)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    unsigned long long ms = (((unsigned long long) tp.tv_sec) * 1000000 + tp.tv_usec);
    sprintf(ts, "%llu", ms);
}

static sys_error_code SysPayloadTypeVarsHandler(data_message_t *MSG)
{
    char VarName[VAR_MAX_NAME_LENGTH];
    char VarValue[VAR_MAX_VALUE_LENGTH];
    struct jReadElement result;
    const char *err_br;
    const char *err_desc;

    if (!(MSG->parsedData.msgType == DATA_MESSAGE_TYPE_COMMAND || MSG->parsedData.msgType == DATA_MESSAGE_TYPE_REQUEST))
        return SYS_ERROR_PARSE_MSGTYPE;

    jwOpen(MSG->outputDataBuffer, MSG->outputDataLength, JW_OBJECT, JW_COMPACT);
    jwObj_object("data");
    jwObj_int("msgid", MSG->parsedData.msgID);
    char time[RFC3339_TIMESTAMP_LENGTH];
    GetRFC3339Time(time);
    jwObj_string("time", time);
    jwObj_int("messtype", DATA_MESSAGE_TYPE_RESPONSE);
    jwObj_int("payloadtype", 1);
    jwObj_object("payload");
    jwObj_object("variables");

    jRead(MSG->inputDataBuffer, "{'data'{'payload'{'variables'", &result);
    if (result.dataType == JREAD_OBJECT)
    { //Write variables
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

            ESP_LOGI(TAG, "Got write variable %s:%s", VarName, VarValue);

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
            if (tp == VAR_STRING || tp == VAR_IPADDR || tp == VAR_ERROR)
                jwObj_string(VarName, VarValue);
            else
                jwObj_raw(VarName, VarValue);

        }
    }
    else
        return SYS_ERROR_PARSE_VARIABLES;

    jwEnd();
    jwEnd();
    GetSysErrorDetales((sys_error_code) MSG->err_code, &err_br, &err_desc);
    jwObj_string("error", (char*) err_br);
    jwObj_string("error_descr", (char*) err_desc);
    jwEnd();

    char *datap = strstr(MSG->outputDataBuffer, "\"data\":");
    if (datap)
    {
        datap += sizeof("\"data\":") - 1;
        SHA256hmacHash((unsigned char*) datap, strlen(datap), (unsigned char*) "mykey", sizeof("mykey"),
                       MSG->parsedData.sha256);
        unsigned char sha_print[32 * 2 + 1];
        BytesToStr(MSG->parsedData.sha256, sha_print, 32);
        sha_print[32 * 2] = 0x00;
        ESP_LOGI(TAG, "SHA256 of DATA object is %s", sha_print);
        jwObj_string("signature", (char*) sha_print);
    }
    else
        return SYS_ERROR_SHA256_DATA;
    jwEnd();
    jwClose();

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
            break;
            case 2:
                WriteNVSSysConfig(GetSysConf());
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

static sys_error_code SysDataParser(data_message_t *MSG)
{
    struct jReadElement result;
    jRead(MSG->inputDataBuffer, "", &result);
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
        ESP_LOGI(TAG, "SHA256 of DATA object is %s", sha_print);
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
        ESP_LOGI(TAG, "Signature is %.*s", 64, (char* )result.pValue);
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
            //MSG->parsedData.payload = malloc(sizeof(payload_type_vars));   Not needed for this case
            return SysPayloadTypeVarsHandler(MSG);
        break;
        default:
            return SYS_ERROR_PARSE_PAYLOADTYPE;
    }

    return SYS_ERROR_UNKNOWN;
}

esp_err_t SysServiceDataHandler(data_message_t *MSG)
{
    MSG->err_code = (int) SysDataParser(MSG);
    if (MSG->err_code)
    {
        jwOpen(MSG->outputDataBuffer, MSG->outputDataLength, JW_OBJECT, JW_PRETTY);
        jwObj_int("msgid", MSG->parsedData.msgID);
        char time[RFC3339_TIMESTAMP_LENGTH];
        GetRFC3339Time(time);
        jwObj_string("time", time);
        jwObj_int("messtype", DATA_MESSAGE_TYPE_RESPONSE);
        const char *err_br;
        const char *err_desc;
        GetSysErrorDetales((sys_error_code) MSG->err_code, &err_br, &err_desc);
        jwObj_string("error", (char*) err_br);
        jwObj_string("error_descr", (char*) err_desc);
        jwEnd();
        jwClose();
    }

    return ESP_OK;
}
