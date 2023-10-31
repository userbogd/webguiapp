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
#include "romfs.h"
#include "NetTransport.h"

#include <esp_event.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "mbedtls/base64.h"
#include "SystemApplication.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define MAX_FILE_SIZE   (1000*1024) // 200 KB
#define MAX_FILE_SIZE_STR "1MB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  EXPECTED_MAX_DATA_SIZE
#define AUTH_DATA_MAX_LENGTH 16

#define HTTP_SERVER_DEBUG_LEVEL 0

typedef enum
{
    HTTP_IO_DONE = 0u,  // Finished with procedure
    HTTP_IO_NEED_DATA,  // More data needed to continue, call again later
    HTTP_IO_WAITING,     // Waiting for asynchronous process to complete, call again later
    HTTP_IO_REDIRECT,
    HTTP_IO_DONE_NOREFRESH,
    HTTP_IO_DONE_API
} HTTP_IO_RESULT;



struct file_server_data
{
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];
    char base_path2[ESP_VFS_PATH_MAX + 1];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
/* Pointer to external POST handler*/
};

typedef struct
{
    const char tag[16];
    const int taglen;
    void (*HandlerRoutine)(char *VarData, void *arg);
} dyn_var_handler_t;

esp_err_t start_file_server(void);
HTTP_IO_RESULT HTTPPostApp(httpd_req_t *req, const char *filename, char *PostData);
int HTTPPrint(httpd_req_t *req, char* buf, char* var);
HTTP_IO_RESULT HTTPPostSysAPI(httpd_req_t *req, char *PostData);

esp_err_t download_get_handler(httpd_req_t *req);
esp_err_t upload_post_handler(httpd_req_t *req);
esp_err_t delete_post_handler(httpd_req_t *req);


#endif /* COMPONENTS_WEB_GUI_APPLICATION_INCLUDE_HTTPSERVER_H_ */
