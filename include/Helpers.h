 /* Copyright 2022 Bogdan Pilyugin
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
 *   File name: Helpers.h
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */

#ifndef MAIN_INCLUDE_HELPERS_H_
#define MAIN_INCLUDE_HELPERS_H_

#include "common_types.h"
#include "esp_err.h"

uint32_t crc32(uint32_t crc, uint8_t const *buf, uint32_t len);
void GetChipId(uint8_t *i);
uint32_t swap(uint32_t in);
void BytesToStr(unsigned char *StrIn, unsigned char *StrOut, uint8_t N);
bool StrToBytes(unsigned char *StrIn, unsigned char *StrOut);
bool StrToBytesLen(unsigned char *StrIn, unsigned char *StrOut, uint16_t InputSymbols);
void bin_to_hex_str(const uint8_t *buf, int len, char *hex);
void UnencodeURL(char* URL);
esp_err_t SHA256Hash(unsigned char *data, int datalen,
                                 unsigned char *res);
esp_err_t SHA256hmacHash(unsigned char *data,
                                int datalen,
                                unsigned char *key,
                                int keylen,
                                unsigned char *res);
#if (CONFIG_FREERTOS_USE_TRACE_FACILITY == 1)
void vTaskGetRunTimeStatsCustom( char *pcWriteBuffer );
#endif
#endif /* MAIN_INCLUDE_HELPERS_H_ */
