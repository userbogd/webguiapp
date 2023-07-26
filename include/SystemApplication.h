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
 *  	 \file SystemApplication.h
 *    \version 1.0
 * 		 \date 2023-07-26
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_SYSTEMAPPLICATION_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_SYSTEMAPPLICATION_H_
#include "webguiapp.h"
#include "esp_err.h"
#include "jRead.h"
#include "jWrite.h"

#define EXPECTED_MAX_DATA_RESPONSE_SIZE (4096)

typedef enum
{
    SYS_OK_DATA = 0,
    SYS_OK,

    SYS_ERROR_WRONG_JSON_FORMAT = 200,
    SYS_ERROR_PARSE_DATA,
    SYS_ERROR_SHA256_DATA,

    SYS_ERROR_PARSE_SIGNATURE,
    SYS_ERROR_PARSE_MESSAGEID,
    SYS_ERROR_PARSE_MSGTYPE,
    SYS_ERROR_PARSE_PAYLOADTYPE,

    SYS_ERROR_PARSE_KEY1,
    SYS_ERROR_PARSE_KEY2,

    SYS_ERROR_NO_MEMORY = 300,
    SYS_ERROR_UNKNOWN,

} sys_error_code;

typedef enum
{
    MSG_COMMAND = 1,
    MSG_REQUEST,
    MSG_RESPONSE
} msg_type;

typedef struct
{
int key1;
int key2;

} payload_type_50;


#define DATA_MESSAGE_TYPE_COMMAND  (1)
#define DATA_MESSAGE_TYPE_REQUEST  (2)
#define DATA_MESSAGE_TYPE_RESPONSE  (3)

typedef struct
{
    char *inputDataBuffer;
    int inputDataLength;
    int chlidx;
    char *outputDataBuffer;
    int outputDataLength;
    struct
    {
        uint64_t msgID;
        time_t time;
        int msgType;
        int payloadType;
        void *payload;
        unsigned char sha256[32];
    } parsedData;
    int err_code;
} data_message_t;

esp_err_t SysServiceDataHandler(data_message_t *MSG);
void GetSysErrorDetales(sys_error_code err, const char **br, const char **ds);

#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_SYSTEMAPPLICATION_H_ */
