 /*! Copyright 2025 Bogdan Pilyugin
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
 *  	 \file err.c
 *    \version 1.0
 * 		 \date 2025-12-30
 *     \author Bogdan Pilyugin
 * 	    \brief    
 *    \details 
 *	\copyright Apache License, Version 2.0
 */

#include "esp_err.h"

#define RES_OK          0       /*!< esp_err_t value indicating success (no error) */
#define RES_FAIL        -1      /*!< Generic esp_err_t code indicating failure */

#define ERROR_NO_MEM              0x101   /*!< Out of memory */
#define ERROR_INVALID_ARG         0x102   /*!< Invalid argument */
#define ERROR_INVALID_STATE       0x103   /*!< Invalid state */
#define ERROR_INVALID_SIZE        0x104   /*!< Invalid size */
#define ERROR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define ERROR_NOT_SUPPORTED       0x106   /*!< Operation or feature not supported */
#define ERROR_TIMEOUT             0x107   /*!< Operation timed out */
#define ERROR_INVALID_RESPONSE    0x108   /*!< Received response was invalid */
#define ERROR_INVALID_CRC         0x109   /*!< CRC or checksum was invalid */
#define ERROR_INVALID_VERSION     0x10A   /*!< Version was invalid */
#define ERROR_INVALID_MAC         0x10B   /*!< MAC address was invalid */
#define ERROR_NOT_FINISHED        0x10C   /*!< Operation has not fully completed */
#define ERROR_NOT_ALLOWED         0x10D   /*!< Operation is not allowed */

#define ERR_TBL_IT(err)    {err, #err}

typedef struct {
    esp_err_t code;
    const char *msg;
} esp_err_msg_t;

static const esp_err_msg_t err_msg_table[] = {
#   ifdef      RES_FAIL
    ERR_TBL_IT(RES_FAIL),                                       /*    -1 Generic ERROR_t code indicating failure */
#   endif
#   ifdef      RES_OK
    ERR_TBL_IT(RES_OK),                                         /*     0 ERROR_t value indicating success (no error) */
#   endif
#   ifdef      ERROR_NO_MEM
    ERR_TBL_IT(ERROR_NO_MEM),                                 /*   257 0x101 Out of memory */
#   endif
#   ifdef      ERROR_INVALID_ARG
    ERR_TBL_IT(ERROR_INVALID_ARG),                            /*   258 0x102 Invalid argument */
#   endif
#   ifdef      ERROR_INVALID_STATE
    ERR_TBL_IT(ERROR_INVALID_STATE),                          /*   259 0x103 Invalid state */
#   endif
#   ifdef      ERROR_INVALID_SIZE
    ERR_TBL_IT(ERROR_INVALID_SIZE),                           /*   260 0x104 Invalid size */
#   endif
#   ifdef      ERROR_NOT_FOUND
    ERR_TBL_IT(ERROR_NOT_FOUND),                              /*   261 0x105 Requested resource not found */
#   endif
#   ifdef      ERROR_NOT_SUPPORTED
    ERR_TBL_IT(ERROR_NOT_SUPPORTED),                          /*   262 0x106 Operation or feature not supported */
#   endif
#   ifdef      ERROR_TIMEOUT
    ERR_TBL_IT(ERROR_TIMEOUT),                                /*   263 0x107 Operation timed out */
#   endif
#   ifdef      ERROR_INVALID_RESPONSE
    ERR_TBL_IT(ERROR_INVALID_RESPONSE),                       /*   264 0x108 Received response was invalid */
#   endif
#   ifdef      ERROR_INVALID_CRC
    ERR_TBL_IT(ERROR_INVALID_CRC),                            /*   265 0x109 CRC or checksum was invalid */
#   endif
#   ifdef      ERROR_INVALID_VERSION
    ERR_TBL_IT(ERROR_INVALID_VERSION),                        /*   266 0x10a Version was invalid */
#   endif
#   ifdef      ERROR_INVALID_MAC
    ERR_TBL_IT(ERROR_INVALID_MAC),                            /*   267 0x10b MAC address was invalid */
#   endif
#   ifdef      ERROR_NOT_FINISHED
    ERR_TBL_IT(ERROR_NOT_FINISHED),                           /*   268 0x10c Operation has not fully completed */
#   endif
#   ifdef      ERROR_NOT_ALLOWED
    ERR_TBL_IT(ERROR_NOT_ALLOWED),                            /*   269 0x10d Operation is not allowed */
#   endif
};

const char *err_to_name(esp_err_t code)
{
    size_t i;

    for (i = 0; i < sizeof(err_msg_table) / sizeof(err_msg_table[0]); ++i) {
        if (err_msg_table[i].code == code) {
            return err_msg_table[i].msg;
        }
    }
    return esp_err_to_name(code);
}

