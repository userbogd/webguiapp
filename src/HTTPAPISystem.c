/* Copyright 2023 Bogdan Pilyugin
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
 *   File name: HTTPAPISystem.c
 *     Project: webguiapp_ref_implement
 *  Created on: 2023-06-10
 *      Author: bogdan
 * Description:	
 */

#include "HTTPServer.h"
#include "jWrite.h"
#include "jRead.h"

#define API_VER "1.00"

HTTP_IO_RESULT HTTPPostSysAPI(httpd_req_t *req, char *PostData)
{
    char data[1024];
    httpd_req_get_hdr_value_str(req, "Content-Type", (char*) data, 31);
    if (!memcmp(data, "application/json", sizeof("application/json")))
    {
        char key[32] = {0};
        struct jReadElement result;
        jRead(PostData, "", &result);
        if (result.dataType == JREAD_OBJECT)
        {
            jRead_string(PostData, "{'key'", key, sizeof(key), NULL);
        }

        //ESP_LOGI(TAG, "JSON data:%s", PostData);
        jwOpen(data, sizeof(data), JW_OBJECT, JW_COMPACT);
        jwObj_string("apiver", API_VER);
        jwObj_string("key", key);;
        jwObj_raw("result", "OK");
        jwEnd();
        jwClose();
        httpd_resp_sendstr(req, data);
        return HTTP_IO_DONE_API;
    }

    httpd_resp_set_status(req, HTTPD_400);
    return HTTP_IO_DONE_API;
}

