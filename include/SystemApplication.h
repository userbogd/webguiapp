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
#include "SysConfiguration.h"
#include "esp_err.h"
#include "jRead.h"
#include "jWrite.h"

#define REAST_API_DEBUG_MODE 0

#define EXPECTED_MAX_DATA_SIZE (4096 * 2)
#define VAR_MAX_NAME_LENGTH (32)
#define VAR_MAX_VALUE_LENGTH (EXPECTED_MAX_DATA_SIZE - 512)

#define    PAYLOAD_ERROR            0
#define    PAYLOAD_DEFAULT          1
#define    PAYLOAD_IO_STATE         2
#define    PAYLOAD_BUTTON_EVENT     3


typedef enum
{
    SYS_OK_DATA = 0,
    SYS_OK,
    SYS_GOT_RESPONSE_MESSAGE,

    SYS_ERROR_WRONG_JSON_FORMAT = 200,
    SYS_ERROR_PARSE_DATA,
    SYS_ERROR_SHA256_DATA,

    SYS_ERROR_PARSE_SIGNATURE,
    SYS_ERROR_PARSE_MESSAGEID,
    SYS_ERROR_PARSE_MSGTYPE,
    SYS_ERROR_PARSE_PAYLOADTYPE,
    SYS_ERROR_PARSE_APPLYTYPE,

    SYS_ERROR_PARSE_KEY1,
    SYS_ERROR_PARSE_KEY2,
    SYS_ERROR_PARSE_VARIABLES,

    SYS_ERROR_NO_MEMORY = 300,
    SYS_ERROR_HANDLER_NOT_SET,
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

} payload_type_vars;


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
        char srcID[9];
        char dstID[9];
        time_t time;
        int msgType;
        int payloadType;
        char payloadName[32];
        void *payload;
        unsigned char sha256[32];
    } parsedData;
    int err_code;
} data_message_t;

typedef struct
{
    char *raw_data_ptr;
    int data_length;
}UART_DATA_SEND_STRUCT;

void InitSerialPort(void);
void InitPPPSerial();
void InitSysSDCard();
esp_err_t InitUSBnetif(void);
esp_err_t TransmitSerialPort(char *data, int ln);

esp_err_t GetConfVar(char* name, char* val, rest_var_types *tp);
esp_err_t SetConfVar(char* name, char* val, rest_var_types *tp);

esp_err_t ServiceDataHandler(data_message_t *MSG);
sys_error_code SysVarsPayloadHandler(data_message_t *MSG);
void GetSysErrorDetales(sys_error_code err, const char **br, const char **ds);

#ifdef CONFIG_WEBGUIAPP_I2C_ENABLE
esp_err_t eepr_i2c_read(uint16_t addr, uint8_t *data, int length);
esp_err_t eepr_i2c_write(uint16_t addr, uint8_t *data, int length);
#endif

const char *err_to_name(esp_err_t code); //Custom error reports

#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_SYSTEMAPPLICATION_H_ */
