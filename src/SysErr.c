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
 *  	 \file SysErr.c
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "SystemApplication.h"

typedef struct
{
    sys_error_code errcode;
    char *errbreaf;
    char *errdescr;
} sys_error_t;

const sys_error_t SysErrors[] = {
        {SYS_OK_DATA, "SYS_OK_DATA", "Result successful, data attached" },
        {SYS_OK, "SYS_OK", "Result successful" },

        { SYS_ERROR_WRONG_JSON_FORMAT, "SYS_ERROR_WRONG_JSON_FORMAT", "Wrong JSON format or not JSON data" },
        { SYS_ERROR_PARSE_DATA, "SYS_ERROR_PARSE_PAYLOAD", "Payload object not found"},
        { SYS_ERROR_SHA256_DATA, "SYS_ERROR_SHA256_PAYLOAD", "SHA256 signature is not valid"},
        { SYS_ERROR_PARSE_SIGNATURE, "SYS_ERROR_PARSE_SIGNATURE", "Key 'signature' not found"},
        { SYS_ERROR_PARSE_MESSAGEID, "SYS_ERROR_PARSE_MESSAGEID", "Key 'msgid' not found or have illegal value" },
        { SYS_ERROR_PARSE_MSGTYPE, "SYS_ERROR_PARSE_MSGTYPE", "Key 'msgtype' not found or have illegal value"},
        { SYS_ERROR_PARSE_PAYLOADTYPE, "SYS_ERROR_PARSE_PAYLOADTYPE", "Key 'payloadtype' not found or have illegal value"},

        { SYS_ERROR_PARSE_KEY1, "SYS_ERROR_PARSE_KEY1", "Key 'key1' not found or have illegal value"},
        { SYS_ERROR_PARSE_KEY2, "SYS_ERROR_PARSE_KEY2", "Key 'key2' not found or have illegal value"},
        { SYS_ERROR_PARSE_VARIABLES, "SYS_ERROR_PARSE_VARIABLES", "Key 'variables' not found or have illegal value"},

        { SYS_ERROR_NO_MEMORY, "SYS_ERROR_NO_MEMORY", "ERROR allocate memory for JSON parser" },
        { SYS_ERROR_UNKNOWN, "SYS_ERROR_UNKNOWN", "Unknown ERROR" }

};

void GetSysErrorDetales(sys_error_code err, const char **br, const char **ds)
{
    *br = SysErrors[sizeof(SysErrors) / sizeof(sys_error_t) - 1].errbreaf;
    *ds = SysErrors[sizeof(SysErrors) / sizeof(sys_error_t) - 1].errdescr;

    for (int i = 0; i < sizeof(SysErrors) / sizeof(sys_error_t); i++)
    {
        if (err == SysErrors[i].errcode)
        {
            *br = SysErrors[i].errbreaf;
            *ds = SysErrors[i].errdescr;
            return;
        }
    }
    return;
}

#define TAG "SysErr"


