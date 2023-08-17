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
 *   File name: Helpers.c
 *     Project: ChargePointMainboard
 *  Created on: 2022-07-21
 *      Author: Bogdan Pilyugin
 * Description:	
 */
#include "Helpers.h"
#include "esp_mac.h"
#include "esp_rom_crc.h"
#include "mbedtls/md.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t crc32(uint32_t crc, uint8_t const *buf, uint32_t len)
{
    return esp_rom_crc32_le(crc, buf, len);
}

void GetChipId(uint8_t *i)
{
uint8_t mac[6];
//esp_base_mac_addr_get(mac);
esp_efuse_mac_get_default(mac);
memcpy(i,&mac[2],4);
}

char val_to_hex_digit(int val)
{
    return "0123456789ABCDEF"[val];
}

void bin_to_hex_str(const uint8_t *buf, int len, char *hex)
{
    for (int i = 0; i < len; i++)
    {
        uint8_t b = buf[i];
        *hex = val_to_hex_digit((b & 0xf0) >> 4);
        hex++;
        *hex = val_to_hex_digit(b & 0x0f);
        hex++;
    }
}

void BytesToStr(unsigned char *StrIn, unsigned char *StrOut, uint8_t N)
{
    uint8_t it, tmp;
    for (it = 0; it < N; ++it)
    {
        tmp = *StrIn;
        tmp >>= 4;
        tmp &= 0x0F;
        if (tmp <= 0x09)
            *StrOut = tmp + 0x30;
        else
            *StrOut = tmp + 0x37;
        ++StrOut;
        tmp = *StrIn;
        tmp &= 0x0F;
        if (tmp <= 0x09)
            *StrOut = tmp + 0x30;
        else
            *StrOut = tmp + 0x37;
        ++StrOut;
        *StrOut = 0x00;
        ++StrIn;
    }
}

static char char2int(char input)
{
  if (input >= '0' && input <= '9')
    return input - '0';
  if (input >= 'A' && input <= 'F')
    return input - 'A' + 10;
  if (input >= 'a' && input <= 'f')
    return input - 'a' + 10;
  return 0x00;
}

bool StrToBytes(unsigned char *StrIn, unsigned char *StrOut)
{
  if ((strlen((const char*) StrIn) % 2))
    return false;
  while (*StrIn && StrIn[1])
  {
      *(StrOut++) = char2int(*StrIn) * 16 + char2int(StrIn[1]);
      StrIn += 2;
  }
  return true;
}

bool StrToBytesLen(unsigned char *StrIn, unsigned char *StrOut, uint16_t InputSymbols)
{
  if (InputSymbols % 2)
    return false;
  for (int i = 0; i < InputSymbols; i+=2)
  {
    *(StrOut++) = char2int(*StrIn) * 16 + char2int(StrIn[1]);
    StrIn += 2;
  }
  return true;
}


uint32_t swap(uint32_t in)
{
    in = ((in & 0xff000000) >> 24) | ((in & 0x00FF0000) >> 8) | ((in & 0x0000FF00) << 8) | ((in & 0xFF) << 24);
    //in = (in >> 16) | (in << 16);
    return in;
}

char btohexa_low(char b)
{
  b &= 0x0F;
  return (b > 9u) ? b + 'A' - 10 : b + '0';
}

BYTE hexatob(TCPIP_UINT16_VAL AsciiChars)
{
  // Convert lowercase to uppercase
  if (AsciiChars.v[1] > 'F')
    AsciiChars.v[1] -= 'a' - 'A';
  if (AsciiChars.v[0] > 'F')
    AsciiChars.v[0] -= 'a' - 'A';

  // Convert 0-9, A-F to 0x0-0xF
  if (AsciiChars.v[1] > '9')
    AsciiChars.v[1] -= 'A' - 10;
  else
    AsciiChars.v[1] -= '0';

  if (AsciiChars.v[0] > '9')
    AsciiChars.v[0] -= 'A' - 10;
  else
    AsciiChars.v[0] -= '0';

  // Concatenate
  return (BYTE) ((AsciiChars.v[1] << 4) | AsciiChars.v[0]);
}

void UnencodeURL(char* URL)
{
  char *Right, *Copy;
  UINT16_VAL Number;

  while ((Right =  strchr((const char*) URL, '%')))
    {
      // Make sure the string is long enough
      if (Right[1] == '\0')
        break;
      if (Right[2] == '\0')
        break;

      // Update the string in place
      Number.v[0] = Right[2];
      Number.v[1] = Right[1];
      *Right++ = hexatob(Number);
      URL = Right;

      // Remove two blank spots by shifting all remaining characters right two
      Copy = Right + 2;
      while ((*Right++ = *Copy++));
    }
}

esp_err_t SHA256Hash(unsigned char *data, int datalen,
                                 unsigned char *res)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*) data, datalen);
    mbedtls_md_finish(&ctx, res);
    mbedtls_md_free(&ctx);
    return ESP_OK;
}

# if(CONFIG_FREERTOS_USE_TRACE_FACILITY == 1)
void vTaskGetRunTimeStatsCustom( char *pcWriteBuffer )
{
TaskStatus_t *pxTaskStatusArray;
volatile UBaseType_t uxArraySize, x;
unsigned long ulTotalRunTime, ulStatsAsPercentage;

   /* Make sure the write buffer does not contain a string. */
   *pcWriteBuffer = 0x00;

   /* Take a snapshot of the number of tasks in case it changes while this
   function is executing. */
   uxArraySize = uxTaskGetNumberOfTasks();

   /* Allocate a TaskStatus_t structure for each task.  An array could be
   allocated statically at compile time. */
   pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

   if( pxTaskStatusArray != NULL )
   {
      /* Generate raw status information about each task. */
      uxArraySize = uxTaskGetSystemState( pxTaskStatusArray,
                                 uxArraySize,
                                 &ulTotalRunTime );

      /* For percentage calculations. */
      ulTotalRunTime /= 100UL;

      /* Avoid divide by zero errors. */
      if( ulTotalRunTime > 0 )
      {
         /* For each populated position in the pxTaskStatusArray array,
         format the raw data as human readable ASCII data. */
         for( x = 0; x < uxArraySize; x++ )
         {
            /* What percentage of the total run time has the task used?
            This will always be rounded down to the nearest integer.
            ulTotalRunTimeDiv100 has already been divided by 100. */
            ulStatsAsPercentage =
                  pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;

            if( ulStatsAsPercentage > 0UL )
            {
               sprintf( pcWriteBuffer, "%-32s %-10llu %-10lu%% %d\r\n",
                                 pxTaskStatusArray[ x ].pcTaskName,
                                 (uint64_t)pxTaskStatusArray[ x ].ulRunTimeCounter,
                                 ulStatsAsPercentage,
                                 (int)pxTaskStatusArray[ x ].usStackHighWaterMark);
            }
            else
            {
               /* If the percentage is zero here then the task has
               consumed less than 1% of the total run time. */
               sprintf( pcWriteBuffer, "%-32s %-10llu <1%%  %d\r\n",
                                 pxTaskStatusArray[ x ].pcTaskName,
                                 (uint64_t)pxTaskStatusArray[ x ].ulRunTimeCounter,
                                 (int)pxTaskStatusArray[ x ].usStackHighWaterMark);
            }

            pcWriteBuffer += strlen( ( char * ) pcWriteBuffer );
         }
      }

      /* The array is no longer needed, free the memory it consumes. */
      vPortFree( pxTaskStatusArray );
   }
}
#endif
