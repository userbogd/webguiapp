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

extern espfs_fs_t *fs;

const char GZIP_SIGN[] = { 0x1f, 0x8b, 0x08 };

static esp_err_t GETHandler(httpd_req_t *req);
static esp_err_t CheckAuth(httpd_req_t *req);

struct file_server_data *server_data = NULL;
httpd_handle_t server = NULL;
static const char *TAG = "HTTPServer";

const char url_api[] = "/api";
//Pointer to external user defined rest api handler
static int (*HTTPUserRestAPI)(char *url, char *req, int len, char *resp) = NULL;
void regHTTPUserRestAPI(int (*api_handler)(char *url, char *req, int len, char *resp))
{
    HTTPUserRestAPI = api_handler;
}

char *userappurl = NULL;
static esp_err_t (*HTTPUserAppGETHandler)(httpd_req_t *req) = NULL;
static esp_err_t (*HTTPUserAppPOSTHandler)(httpd_req_t *req) = NULL;

void regHTTPUserAppHandlers(char *url,
                            esp_err_t (*get)(httpd_req_t *req),
                            esp_err_t (*post)(httpd_req_t *req))
{
    userappurl = url;
    HTTPUserAppGETHandler = get;
    HTTPUserAppPOSTHandler = post;

}

#define BASIC_LOGIN_LENGTH 31
#define BASIC_PASS_LENGTH 31
#define BASIC_DECODED_LENGTH (BASIC_LOGIN_LENGTH + BASIC_PASS_LENGTH + 1 + 1)
#define BASIC_ENCODED_LENGTH (BASIC_DECODED_LENGTH * 4 / 3)

static esp_err_t CheckAuth(httpd_req_t *req)
{
    unsigned char pass[BASIC_DECODED_LENGTH] = { 0 }; //max length of login:password decoded string
    unsigned char inp[BASIC_ENCODED_LENGTH];  //max length of login:password coded string plus Basic
    const char keyword1[] = "Basic ";
    const int keyword1len = sizeof(keyword1) - 1;
    if (httpd_req_get_hdr_value_len(req, "Authorization") > BASIC_ENCODED_LENGTH)
    {
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send_err(req, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, "Authorization field value is too large");
        return ESP_FAIL;
    }
    httpd_req_get_hdr_value_str(req, "Authorization", (char*) inp, BASIC_ENCODED_LENGTH);
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
    else if (IS_FILE_EXT(filename, ".svg"))
    {
        return httpd_resp_set_type(req, "image/svg+xml");
    }
    else if (IS_FILE_EXT(filename, ".css"))
    {
        return httpd_resp_set_type(req, "text/css");
    }
    else if (IS_FILE_EXT(filename, ".js"))
    {
        return httpd_resp_set_type(req, "text/javascript");
    }
    else if (IS_FILE_EXT(filename, ".woff"))
    {
        return httpd_resp_set_type(req, "font/woff");
    }
    else if (IS_FILE_EXT(filename, ".woff2"))
    {
        return httpd_resp_set_type(req, "font/woff2");
    }
    else if (IS_FILE_EXT(filename, ".json"))
    {
        return httpd_resp_set_type(req, "application/json");
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
    ESP_LOGI(TAG, "POST request handle URL: %s", req->uri);
#endif

    if(userappurl && HTTPUserAppPOSTHandler)
    {
        if (memmem(req->uri, strlen(req->uri), userappurl, strlen(userappurl)))
            return HTTPUserAppPOSTHandler(req);
    }

    if (memmem(req->uri, strlen(req->uri), "/storage/upload/", sizeof("/storage/upload/") - 1))
        return upload_post_handler(req);
    if (memmem(req->uri, strlen(req->uri), "/storage/delete/", sizeof("/storage/delete/") - 1))
        return delete_post_handler(req);

    char *buf = ((struct file_server_data*) req->user_ctx)->scratch;
    int received;
    int offset = 0;
    int remaining = req->content_len;
    buf[req->content_len] = 0x00;
    while (remaining > 0)
    {
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Remaining size : %d", remaining);
#endif
        /* Receive the file part by part into a buffer */

        if ((received = httpd_req_recv(req, buf + offset,
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
            offset += received;
        {
#if HTTP_SERVER_DEBUG_LEVEL > 0
        ESP_LOGI(TAG, "Received : %d", received);
#endif
        }

        /* Keep track of remaining size of
         * the file left to be uploaded */
        remaining -= received;
    }

    char filepath[FILE_PATH_MAX];
    const char *filename;

    if (CheckAuth(req) != ESP_OK)
        return ESP_FAIL;

    filename = get_path_from_uri(filepath,
                                 ((struct file_server_data*) req->user_ctx)->base_path,
                                 req->uri,
                                 sizeof(filepath));

    if (!memcmp(filename, url_api, sizeof(url_api)))
        HTTPPostSysAPI(req, buf);
    else
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "URL not found");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t GETHandler(httpd_req_t *req)
{
#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "GET request handle URL: %s", req->uri);
#endif

    if(userappurl && HTTPUserAppGETHandler)
    {
        if (memmem(req->uri, strlen(req->uri), userappurl, strlen(userappurl)))
            return HTTPUserAppGETHandler(req);
    }

    //Route to file server GET handler
    if (memmem(req->uri, strlen(req->uri), "/storage/", sizeof("/storage/") - 1))
        return download_get_handler(req);

    char filepath[FILE_PATH_MAX];
    espfs_file_t *file;
    struct espfs_stat_t stat;
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

//check auth for all files
    if (CheckAuth(req) != ESP_OK)
    {
        return ESP_FAIL;
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
    ESP_LOGI(TAG, "BEGIN send file : %s (%d bytes)...", filename,
             stat.size);
#endif

    set_content_type_from_file(req, filename);
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=600");
    httpd_resp_set_hdr(req, "Connection", "close");

    /*Check if content of file is compressed*/
    char file_header[3];
    espfs_fread(file, file_header, 3);
    if (!memcmp(file_header, GZIP_SIGN, 3))
    {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    espfs_fseek(file, 0, SEEK_SET); //return to begin of file

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data*) req->user_ctx)->scratch;
    size_t chunksize;
    do
    {
        /* Read file in chunks into the scratch buffer */
        chunksize = espfs_fread(file, chunk, SCRATCH_BUFSIZE);
        if (chunksize > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
            {
                espfs_fclose(file);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
        /* Keep looping till the whole file is sent */
    }
    while (chunksize != 0);
    /* Close file after sending complete */
    espfs_fclose(file);
#if HTTP_SERVER_DEBUG_LEVEL > 0
    ESP_LOGI(TAG, "END send file : %s (%d bytes)...", filename,
             stat.size);
#endif
    /* Respond with an empty chunk to signal HTTP response completion */
#ifdef CONFIG_EXAMPLE_HTTPD_CONN_CLOSE_HEADER
    httpd_resp_set_hdr(req, "Connection", "close");
#endif
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = false;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = (4096 + 2048);

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
#if CONFIG_SDCARD_ENABLE
    strlcpy(server_data->base_path2, "/sdcard", sizeof("/sdcard"));
#else
    strlcpy(server_data->base_path2, "/data", sizeof("/data"));
#endif
    server = start_webserver();
    return ESP_OK;
}
