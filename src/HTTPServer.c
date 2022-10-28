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
 *  	 \file HTTPServer.c
 *    \version 1.0
 * 		 \date 2022-08-14
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "HTTPServer.h"
#include "sdkconfig.h"

const char GZIP_SIGN[] = { 0x1f, 0x8b, 0x08 };

static esp_err_t GETHandler(httpd_req_t *req);
static esp_err_t CheckAuth(httpd_req_t *req);

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  4096
#define AUTH_DATA_MAX_LENGTH 16

struct file_server_data
{
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];
    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
/* Pointer to external POST handler*/
};
struct file_server_data *server_data = NULL;
httpd_handle_t server = NULL;
static const char *TAG = "HTTPServer";

static esp_err_t CheckAuth(httpd_req_t *req)
{
    unsigned char pass[18] = { 0 }; //max length of login:password decoded string
    unsigned char inp[31];  //max length of login:password coded string plus Basic
    const char keyword1[] = "Basic ";
    const int keyword1len = sizeof(keyword1) - 1;
    if (httpd_req_get_hdr_value_len(req, "Authorization") > 31)
    {
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send_err(req, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, "Authorization field value is too large");
        return ESP_FAIL;
    }
    httpd_req_get_hdr_value_str(req, "Authorization", (char*) inp, 31);
    unsigned char *pt = memmem(inp, sizeof(inp), keyword1, keyword1len);
    if (pt)
    {
        pt += keyword1len;
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Authorization string is:%s", pt);
#endif
        size_t l;
        mbedtls_base64_decode(pass, (size_t) sizeof(pass), &l, pt, strlen((const char*) pt));
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Authorization decoded string is:%s", pass);
#endif

        strcpy((char*) inp, GetSysConf()->SysName); //buffer inp reused for login:pass check
        strcat((char*) inp, (char*) ":");
        strcat((char*) inp, GetSysConf()->SysPass);
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Reference auth data is %s", inp);
#endif
    }
    if (pt == NULL || strcmp((const char*) inp, (char*) pass))
    {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic");
        //httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "This page requires authorization");
        return ESP_FAIL;
    }
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req,
                                            const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf"))
    {
        return httpd_resp_set_type(req, "application/pdf");
    }
    else if (IS_FILE_EXT(filename, ".html"))
    {
        return httpd_resp_set_type(req, "text/html");
    }
    else if (IS_FILE_EXT(filename, ".jpeg"))
    {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    else if (IS_FILE_EXT(filename, ".png"))
    {
        return httpd_resp_set_type(req, "image/png");
    }
    else if (IS_FILE_EXT(filename, ".ico"))
    {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    else if (IS_FILE_EXT(filename, ".css"))
    {
        return httpd_resp_set_type(req, "text/css");
    }
    else if (IS_FILE_EXT(filename, ".woff2"))
    {
        return httpd_resp_set_type(req, "font/woff2");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path,
                                     const char *uri,
                                     size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest)
    {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash)
    {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize)
    {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

static esp_err_t POSTHandler(httpd_req_t *req)
{
#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "POST request handle");
#endif
    char *buf = ((struct file_server_data*) req->user_ctx)->scratch;
    int received;
    int remaining = req->content_len;
    buf[req->content_len] = 0x00;
    HTTP_IO_RESULT http_res;
    while (remaining > 0)
    {
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
#endif
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf,
                                       MIN(remaining, SCRATCH_BUFSIZE))) <= 0)
        {
            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry if timeout occurred */
                continue;
            }

            /* In case of unrecoverable error*/
            ESP_LOGE(TAG, "File reception failed!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to receive file");
            return ESP_FAIL;
        }

        /* Write buffer content to file on storage */
        if (received)
        {
            char filepath[FILE_PATH_MAX];
            const char *filename = get_path_from_uri(filepath,
                                                     ((struct file_server_data*) req->user_ctx)->base_path,
                                                     req->uri,
                                                     sizeof(filepath));
            http_res = HTTPPostApp(req, filename, buf);

            if (http_res == HTTP_IO_DONE)
                return GETHandler(req);

            else if (http_res == HTTP_IO_REDIRECT)
            {
                httpd_resp_set_status(req, "307 Temporary Redirect");
                httpd_resp_set_hdr(req, "Location", filename);
                httpd_resp_send(req, NULL, 0);  // Response body can be empty
#if HTTP_SERVER_DEBUG_LEVEL > 0
             ESP_LOGI(TAG, "Redirect request from POST");
     #endif
                return ESP_OK;
            }

            else if (http_res == HTTP_IO_DONE_NOREFRESH)
            {
                httpd_resp_set_status(req, HTTPD_204);
                httpd_resp_send(req, NULL, 0);  // Response body can be empty
                return ESP_OK;
            }

        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    return ESP_OK;
}

static esp_err_t GETHandler(httpd_req_t *req)
{
#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "GET request handle");
#endif
    char filepath[FILE_PATH_MAX];
    espfs_file_t *file;
    struct espfs_stat_t stat;
    bool isDynamicVars = false;
    uint32_t fileSize;      //length of file in bytes
    uint32_t bufSize;
    uint32_t readBytes;     //number of bytes, already read from file

    const char *filename = get_path_from_uri(filepath,
                                             ((struct file_server_data*) req->user_ctx)->base_path,
                                             req->uri,
                                             sizeof(filepath));
    if (!filename)
    {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Filename too long");
        return ESP_FAIL;
    }

    /* Redirect request to /index.html */
    if (filename[strlen(filename) - 1] == '/')
    {
        httpd_resp_set_status(req, "307 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/index.html");
        httpd_resp_send(req, NULL, 0);  // Response body can be empty
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Redirect request to /index.html");
#endif
        return ESP_OK;
    }

    //check auth for all files except status.json
    if (strcmp(filename, "/status.json"))
    {
        if (CheckAuth(req) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    //open file
    file = espfs_fopen(fs, filepath);
    if (!file)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }
    //get file info
    espfs_stat(fs, filepath, &stat);

#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "Sending file : %s (%d bytes)...", filename,
             stat.size);
#endif
    //OutputDisplay((char*) filepath);
    set_content_type_from_file(req, filename);
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data*) req->user_ctx)->scratch;

    fileSize = stat.size;
    bufSize = MIN(fileSize, SCRATCH_BUFSIZE - MAX_DYNVAR_LENGTH);  //
    readBytes = 0;
    //allocate buffer for file data
    char *buf = (char*) malloc(bufSize);
    if (!buf)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        espfs_fclose(file);
        return ESP_FAIL;
    }
    //read first portion of data from file
    readBytes = espfs_fread(file, buf, bufSize);
    //check if file is compressed by GZIP and add correspondent header
    if (memmem(buf, 3, GZIP_SIGN, 3))
    {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=600");
    }

    //check if the file can contains dynamic variables
    if (IS_FILE_EXT(filename, ".html") || IS_FILE_EXT(filename, ".json"))
        isDynamicVars = true;

    bool transfComplete = false;
    do
    {
        int pt = 0;
        int prcdBytes = 0;
        while (pt < bufSize && prcdBytes < (SCRATCH_BUFSIZE))
        {
            if (buf[pt] == '~' && isDynamicVars) //open tag
            {
                int k = 0;
                char ch = 0x00;
                char DynVarName[MAX_DYNVAR_NAME_LENGTH];
                while (k < MAX_DYNVAR_NAME_LENGTH)
                {
                    if (pt < bufSize)
                    {
                        if (buf[++pt] != '~')
                            DynVarName[k++] = buf[pt]; //continue extract variable name from buf
                        else
                        {
                            break; //found close tag
                        }
                    }
                    else //need read more characters directly from file
                    {
                        if (espfs_fread(file, &ch, 1))
                        {
                            readBytes++;
                            if (ch != '~')
                                DynVarName[k++] = ch; //continue extract variable name from file
                            else
                                break; //found close tag
                        }
                        else // end of file
                        {
                            break;  //error end of file
                        }
                    }
                }


                if (buf[pt] == '~' || ch == '~')   //close tag, got valid dynamic variable name
                {
                    DynVarName[k] = 0x00;
                    prcdBytes += HTTPPrint(req, &chunk[prcdBytes], DynVarName);
                    pt++;
                }
                else  //not found close tag, exit by overflow max variable size or file end
                {

                }
            }
            else
                chunk[prcdBytes++] = buf[pt++]; //ordinary character
        }
        if (prcdBytes != 0)
        {
            /* Send the buffer contents as HTTP response chunk */
#if HTTP_SERVER_DEBUG_LEVEL > 0
            ESP_LOGI(TAG, "Write to HTTPserv resp %d", prcdBytes);
#endif
            if (httpd_resp_send_chunk(req, chunk, prcdBytes) != ESP_OK)
            {
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "Failed to send file");
                free(buf);
                espfs_fclose(file);
                return ESP_FAIL;
            }
        }
        /* Keep looping till the whole file is sent */
         int nextPortion;
         if((nextPortion = espfs_fread(file, buf, bufSize)))
         {
             bufSize = nextPortion;
             readBytes += nextPortion;
         }
         else
           transfComplete = true;
    }
    while (!transfComplete);

    /* Close file after sending complete */
    espfs_fclose(file);
#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "File sending complete, read from file %d", readBytes);
#endif
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    free(buf);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = 3;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        /* URI handler for GET request */
        httpd_uri_t get = { .uri = "/*",
                .method = HTTP_GET,
                .handler = GETHandler,
                .user_ctx = server_data // Pass server data as context
                };
        httpd_register_uri_handler(server, &get);

        /* URI handler for POST request */
        httpd_uri_t post = { .uri = "/*",
                .method = HTTP_POST,
                .handler = POSTHandler,
                .user_ctx = server_data // Pass server data as context
                };
        httpd_register_uri_handler(server, &post);
        return server;
    }
    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void reconnect_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t*) arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Any adapter got new IP. Restart web server.");
        stop_webserver(*server);
        *server = start_webserver();
    }
}

/* Function to start the file server */
esp_err_t start_file_server(void)
{
    if (server_data)
    {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &reconnect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &reconnect_handler, &server));

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, "/", sizeof("/"));
    server = start_webserver();
    return ESP_OK;
}
