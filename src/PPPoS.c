/*
 * PPPoS.c
 *
 *  Created on: Feb 10, 2025
 *      Author: bogd
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "freertos/projdefs.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/netif.h"
#include "netif/ppp/pppapi.h"
#include "netif/ppp/pppos.h"
#include "driver/uart.h"
#include "esp_netif.h"
#include <esp_log.h>

#define TAG "pppos"

// UART configuration
#define UART_PORT_NUM  UART_NUM_2
#define UART_BAUD_RATE 115200
#define UART_RX_PIN    18
#define UART_TX_PIN    17
#define UART_BUF_SIZE  12000

// PPP configuration
static ppp_pcb *ppp;
static struct netif ppp_netif;

// Function to handle PPP link status changes
static void ppp_link_status_cb(ppp_pcb *pcb, int err_code, void *ctx)
{
    struct netif *pppif = (struct netif *)ctx;

    if (err_code == PPPERR_NONE)
    {
        ESP_LOGI(TAG, "PPP connection established");
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP address: %s", ip4addr_ntoa(netif_ip4_addr(pppif)));
        ESP_LOGI(TAG, "Gateway: %s", ip4addr_ntoa(netif_ip4_gw(pppif)));
        ESP_LOGI(TAG, "Netmask: %s", ip4addr_ntoa(netif_ip4_netmask(pppif)));
        ESP_LOGI(TAG, "~~~~~~~~~~~");
        return;
    }

    ESP_LOGI(TAG, "PPP connection lost");
}

// Function to output PPP data over UART
static u32_t pppos_output_cb(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx)
{
    uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    return len;
}

static void uart_init()
{
    uart_config_t uart_config = { .baud_rate = UART_BAUD_RATE, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE };
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
}

static uint8_t data[UART_BUF_SIZE];

static void pppos_task(void *arg)
{
    
    while (1)
    {
        int len = uart_read_bytes(UART_PORT_NUM, data, UART_BUF_SIZE, pdMS_TO_TICKS(10));
        if (len > 0)
        {
            pppos_input(ppp, data, len);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
        }
}
void InitPPPSerial()
{
    uart_init();
    // Create PPPoS interface
    ppp = pppapi_pppos_create(&ppp_netif, pppos_output_cb, ppp_link_status_cb, &ppp_netif);
    if (ppp == NULL)
    {
        ESP_LOGE(TAG, "Failed to create PPPoS interface");
        return;
    }
    // Set PPP as the default interface
    pppapi_set_default(ppp);
    
    // Connect PPP
    pppapi_connect(ppp, 0);
   
    xTaskCreate(pppos_task, "pppos_task", 4096 , (void *)0, 7, NULL);
}
