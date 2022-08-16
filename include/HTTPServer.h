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
 *  	 \file HTTPServer.h
 *    \version 1.0
 * 		 \date 2022-08-14
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#ifndef COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_HTTPSERVER_H_
#define COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_HTTPSERVER_H_

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include <esp_http_server.h>

#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#include "Helpers.h"
#include "SystemConfiguration.h"
#include "romfs.h"
#include "NetTransport.h"

#include <esp_event.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "mbedtls/base64.h"

typedef enum
{
    HTTP_IO_DONE = 0u,  // Finished with procedure
    HTTP_IO_NEED_DATA,  // More data needed to continue, call again later
    HTTP_IO_WAITING,     // Waiting for asynchronous process to complete, call again later
    HTTP_IO_REDIRECT
} HTTP_IO_RESULT;

esp_err_t start_file_server(void);
HTTP_IO_RESULT HTTPPostApp(httpd_req_t *req, const char *filename, char *PostData);
int HTTPPrint(httpd_req_t *req, char* buf, char* var);

#endif /* COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_HTTPSERVER_H_ */
