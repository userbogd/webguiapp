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
 *  	 \file SerialPort.c
 *    \version 1.0
 * 		 \date 2023-10-15
 *     \author Bogdan Pilyugin
 * 	    \brief
 *    \details
 *	\copyright Apache License, Version 2.0
 */

#include <driver/uart.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/projdefs.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <hal/uart_types.h>
#include <MQTT.h>
#include <sdkconfig.h>
#include <stdlib.h>
#include <string.h>
#include "SysConfiguration.h"
#include "webguiapp.h"
#include "driver/gpio.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include "esp_netif.h"

#define TAG             "serial_port"
#define UART_READ_TOUT  (80) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks
#define PATTERN_CHR_NUM (3)   /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define UART_TX_QUEUE_SIZE (5)
#define UART_RX_QUEUE_SIZE (5)
#define UART_DEBUG_MODE    0

#if CONFIG_WEBGUIAPP_UART_TRANSPORT_ENABLE

QueueHandle_t UARTtxQueueHandle;
static StaticQueue_t xStaticUARTtxQueue;
uint8_t UARTtxQueueStorageArea[UART_TX_QUEUE_SIZE * sizeof(UART_DATA_SEND_STRUCT)];

static QueueHandle_t uart_event_queue;
static char rxbuf[CONFIG_WEBGUIAPP_UART_BUF_SIZE];


esp_err_t TransmitSerialPort(char *data, int ln)
{
    if(!GetSysConf()->serialSettings.Flags.IsSerialEnabled)
    	return ESP_ERR_NOT_ALLOWED;
    UART_DATA_SEND_STRUCT DSS;
    char *buf = malloc(ln);
    if (!buf)
        return ESP_ERR_NO_MEM;

    memcpy(buf, data, ln);
    DSS.raw_data_ptr = buf;
    DSS.data_length = ln;
    if (xQueueSend(UARTtxQueueHandle, &DSS, 0) != pdPASS)
    {
        free(buf);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void ReceiveHandlerAPI()
{
    char *respbuf = malloc(EXPECTED_MAX_DATA_SIZE);
    if (respbuf)
    {
        data_message_t M = { 0 };
        M.inputDataBuffer = rxbuf;
        M.inputDataLength = strlen(rxbuf);
        M.chlidx = 100;
        M.outputDataBuffer = respbuf;
        M.outputDataLength = EXPECTED_MAX_DATA_SIZE;
        ServiceDataHandler(&M);
        if (M.outputDataBuffer[0] != 0x00 && M.outputDataLength > 0)
        {
            TransmitSerialPort(M.outputDataBuffer, MIN(EXPECTED_MAX_DATA_SIZE, strlen(M.outputDataBuffer)));
        }
        free(respbuf);
    }
    else
    {
        ESP_LOGE(TAG, "Out of free RAM for HTTP API handle");
    }
}

void serial_RX_task(void *arg)
{
    uart_event_t event;
    int buffered_size;

    for (;;)
    {
        // Waiting for UART event.
        if (xQueueReceive(uart_event_queue, (void *)&event, portMAX_DELAY))
        {
#if UART_DEBUG_MODE == 1
            ESP_LOGI(TAG, "uart[%d] event:%d", CONFIG_WEBGUIAPP_UART_PORT_NUM, event.type);
#endif
            switch (event.type)
            {
                // Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                 other types of events. If we take too much time on data event, the queue might
                 be full.*/
                case UART_DATA:
                    if (event.timeout_flag)
                    {
                        bzero(rxbuf, CONFIG_WEBGUIAPP_UART_BUF_SIZE);
                        uart_get_buffered_data_len(CONFIG_WEBGUIAPP_UART_PORT_NUM, (size_t *)&buffered_size);
                        if (buffered_size)
                        {
                            uart_read_bytes(CONFIG_WEBGUIAPP_UART_PORT_NUM, rxbuf, buffered_size, 100);
#if UART_DEBUG_MODE == 1
                            ESP_LOGI(TAG, "read of %d bytes: %s", buffered_size, rxbuf);
#endif
                            
                            if (GetSysConf()->serialSettings.Flags.IsBridgeEnabled)
                            {
                                ExternalServiceMQTTSend(EXTERNAL_SERVICE_NAME, rxbuf, buffered_size, 0);
                                ExternalServiceMQTTSend(EXTERNAL_SERVICE_NAME, rxbuf, buffered_size, 1);
                            }
                            else
                                ReceiveHandlerAPI();

                        }
                    }
                    break;
                    // Event of HW FIFO overflow detected
                case UART_FIFO_OVF:

#if UART_DEBUG_MODE == 1
                    ESP_LOGI(TAG, "hw fifo overflow");
#endif
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(CONFIG_WEBGUIAPP_UART_PORT_NUM);
                    xQueueReset(uart_event_queue);
                    break;
                    // Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGE(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(CONFIG_WEBGUIAPP_UART_PORT_NUM);
                    xQueueReset(uart_event_queue);
                    break;
                    // Event of UART RX break detected
                case UART_BREAK:
#if UART_DEBUG_MODE == 1
                    ESP_LOGI(TAG, "uart rx break");
#endif
                    break;
                    // Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "uart parity error");
                    break;
                    // Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "uart frame error");
                    break;
                    // UART_PATTERN_DET
                case UART_PATTERN_DET:

                    uart_get_buffered_data_len(CONFIG_WEBGUIAPP_UART_PORT_NUM, (size_t *)&buffered_size);
                    int pos = uart_pattern_pop_pos(CONFIG_WEBGUIAPP_UART_PORT_NUM);
#if UART_DEBUG_MODE == 1
                    ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", (int)pos, (int)buffered_size);
#endif
                    if (pos == -1)
                    {
                        // There used to be a UART_PATTERN_DET event, but the pattern position queue is full so that it can not
                        // record the position. We should set a larger queue size.
                        // As an example, we directly flush the rx buffer here.
                        uart_flush_input(CONFIG_WEBGUIAPP_UART_PORT_NUM);
                    }
                    else
                    {
                        uart_read_bytes(CONFIG_WEBGUIAPP_UART_PORT_NUM, rxbuf, pos, 100 / portTICK_PERIOD_MS);
                        uint8_t pat[PATTERN_CHR_NUM + 1];
                        memset(pat, 0, sizeof(pat));
                        uart_read_bytes(CONFIG_WEBGUIAPP_UART_PORT_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
#if UART_DEBUG_MODE == 1
                        ESP_LOGI(TAG, "read data: %s", rxbuf);
                        ESP_LOGI(TAG, "read pat : %s", pat);
#endif
                    }

                    break;
                    // Others
                default:
                    ESP_LOGW(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

void static serial_TX_task(void *arg)
{
    UART_DATA_SEND_STRUCT DSS;
    while (1)
    {
        xQueueReceive(UARTtxQueueHandle, &DSS, portMAX_DELAY);
        if (uart_write_bytes(CONFIG_WEBGUIAPP_UART_PORT_NUM, DSS.raw_data_ptr, DSS.data_length) != DSS.data_length)
            ESP_LOGE(TAG, "RS485 write data failure");
        free(DSS.raw_data_ptr);
        if (uart_wait_tx_done(CONFIG_WEBGUIAPP_UART_PORT_NUM, pdMS_TO_TICKS(1000)) != ESP_OK)
            ESP_LOGE(TAG, "RS485 transmit data failure");
		vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void InitSerialPort(void)
{
    uart_config_t uart_config = {
        .baud_rate = GetSysConf()->serialSettings.BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    //uart_config.data_bits = (uint8_t)GetSysConf()->serialSettings.DataBits;

    switch (GetSysConf()->serialSettings.DataBits)
    {
        case 0:
            uart_config.data_bits = UART_DATA_5_BITS;
            break;
        case 1:
            uart_config.data_bits = UART_DATA_6_BITS;
            break;
        case 2:
            uart_config.data_bits = UART_DATA_7_BITS;
            break;
        case 3:
            uart_config.data_bits = UART_DATA_8_BITS;
            break;
    }


    switch (GetSysConf()->serialSettings.Parity)
    {
        case 0:
            uart_config.parity = UART_PARITY_DISABLE;
            break;
        case 2:
            uart_config.parity = UART_PARITY_EVEN;
            break;
        case 3:
            uart_config.parity = UART_PARITY_ODD;
            break;
    }

    
    switch (GetSysConf()->serialSettings.StopBits)
    {
        case 1:
            uart_config.stop_bits = UART_STOP_BITS_1;
            break;
        case 2:
            uart_config.stop_bits = UART_STOP_BITS_1_5;
            break;
        case 3:
            uart_config.stop_bits = UART_STOP_BITS_2;
            break;
    }
	

    // uart_config.stop_bits = (uint8_t) GetSysConf()->serialSettings.StopBits;

    ESP_LOGI(TAG, "UART data_bits:%d parity:%d stop_bits:%d", GetSysConf()->serialSettings.DataBits, GetSysConf()->serialSettings.Parity, GetSysConf()->serialSettings.StopBits);

    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_install(CONFIG_WEBGUIAPP_UART_PORT_NUM, CONFIG_WEBGUIAPP_UART_BUF_SIZE * 2, 0, 20, &uart_event_queue, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_param_config(CONFIG_WEBGUIAPP_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_pin(CONFIG_WEBGUIAPP_UART_PORT_NUM, CONFIG_WEBGUIAPP_UART_TXD, CONFIG_WEBGUIAPP_UART_RXD, CONFIG_WEBGUIAPP_UART_RTS, -1));

#ifdef CONFIG_WEBGUIAPP_UART_MODE_UART
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_mode(CONFIG_WEBGUIAPP_UART_PORT_NUM, UART_MODE_UART));
#elif CONFIG_WEBGUIAPP_UART_MODE_RS485
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_mode(CONFIG_WEBGUIAPP_UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));
#endif

    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_enable_rx_intr(CONFIG_WEBGUIAPP_UART_PORT_NUM));
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_rx_timeout(CONFIG_WEBGUIAPP_UART_PORT_NUM, UART_READ_TOUT));
    // ESP_ERROR_CHECK_WITHOUT_ABORT(uart_enable_pattern_det_baud_intr(CONFIG_UART_PORT_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0));
    uart_pattern_queue_reset(CONFIG_WEBGUIAPP_UART_PORT_NUM, 20);

    UARTtxQueueHandle = NULL;
    UARTtxQueueHandle = xQueueCreateStatic(UART_TX_QUEUE_SIZE, sizeof(UART_DATA_SEND_STRUCT), UARTtxQueueStorageArea, &xStaticUARTtxQueue);




    xTaskCreate(serial_TX_task, "serial_tx", 4096 * 2, (void *)0, 7, NULL);
    xTaskCreate(serial_RX_task, "serial_rx", 4096 * 4, (void *)0, 12, NULL);
    ESP_LOGI(TAG, "Serial port initialized on UART%d with baudrate %d", CONFIG_WEBGUIAPP_UART_PORT_NUM, GetSysConf()->serialSettings.BaudRate);
}

#endif
