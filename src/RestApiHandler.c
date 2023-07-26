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
 *  	 \file RestApiHandler.c
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "SystemApplication.h"
#include <SysConfiguration.h>
#include <webguiapp.h>

extern SYS_CONFIG SysConfig;

const rest_var_t ConfigVariables[] =
{
        {0, "netname", &SysConfig.NetBIOSName, VAR_STRING, 3, 31},
        {1, "otaurl", &SysConfig.OTAURL, VAR_STRING, 3, 128}

};

esp_err_t SetConfVar(char* valias, rest_var_types* vtype, void* val)
{
    rest_var_t *V = NULL;
    for (int i = 0; i< sizeof(ConfigVariables)/sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, valias))
        {
            V = &ConfigVariables[i];
            break;
        }
    }
    if (!V) return ESP_ERR_NOT_FOUND;
    int vallen = strlen(val);
    if (vallen < V->minlen || vallen >= V->maxlen)
        return ESP_ERR_INVALID_ARG;

    switch(*vtype)
    {
        case VAR_BOOL:
            V->ref = (val)?true:false;
            break;
        case VAR_INT:
            V->ref = (int*)val;
            break;
        case VAR_STRING:
            strcpy(V->ref, (char*)val);
            break;

    }
    return ESP_OK;
}


esp_err_t GetConfVar(char* valias, rest_var_types* vtype, void* val)
{
    rest_var_types *faketp  = vtype;
    void *fakepointer = val;
    rest_var_t *V = NULL;
    for (int i = 0; i< sizeof(ConfigVariables)/sizeof(rest_var_t); ++i)
    {
        if (!strcmp(ConfigVariables[i].alias, valias))
        {
            V = &ConfigVariables[i];
            break;
        }
    }
    if (!V) return ESP_ERR_NOT_FOUND;
    vtype = &V->vartype;
    val = V->ref;
    return ESP_OK;
}


sys_error_code SysVarsPayloadHandler(data_message_t *MSG)
{
    struct jReadElement result;
    payload_type_50 *payload;
    payload = ((payload_type_50*) (MSG->parsedData.payload));
    if (MSG->parsedData.msgType == DATA_MESSAGE_TYPE_COMMAND)
    {
        //Extract 'key1' or throw exception
        jRead(MSG->inputDataBuffer, "{'data'{'payload'{'key1'", &result);
        if (result.elements == 1)
        {

        }
        else
            return SYS_ERROR_PARSE_KEY1;

        //Extract 'key1' or throw exception
        jRead(MSG->inputDataBuffer, "{'data'{'payload'{'key2'", &result);
        if (result.elements == 1)
        {

        }
        else
            return SYS_ERROR_PARSE_KEY2;
        //return StartTransactionPayloadType1(MSG);

    }

    else if (MSG->parsedData.msgType == DATA_MESSAGE_TYPE_REQUEST)
    {
        PrepareResponsePayloadType50(MSG);
        return SYS_OK_DATA;
    }
    else
        return SYS_ERROR_PARSE_MSGTYPE;

    return SYS_OK;
}


