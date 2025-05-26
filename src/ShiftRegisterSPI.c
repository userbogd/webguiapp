/*
 * ShiftRegisterSPI.c
 *
 *  Created on: Apr 18, 2025
 *      Author: bogd
 */

#include "SysConfiguration.h"
#include "webguiapp.h"
#include "sdkconfig.h"
#include "esp_timer.h"

#ifdef CONFIG_WEBGUIAPP_SR_ENABLE


#define TAG "ShiftRegDriver"

#define CONFIG_SHIFTREG_SPI_HOST     (1)
#define CONFIG_SHIFTREG_SPI_CLOCK_HZ (10000000)
#define CONFIG_SHIFTREG_SPI_CS_GPIO  (12)

#define CONFIG_SHIFTREG_SAMPLES_MS (10)

static spi_device_handle_t spi_shift_handle;
static spi_transaction_t spi_shift_transaction;
static uint8_t inputs[] = { 0, 0 }, outputs[] = { 0, 0 };

static const int SHIFTREG_SAMPLE_START_BIT = BIT0;

static EventGroupHandle_t digio_event_group = NULL;
static SemaphoreHandle_t xSemaphoreShiftRegHandle = NULL;
static StaticSemaphore_t xSemaphoreShiftRegBuf;

esp_timer_handle_t shiftreg_timer, inputs_periodical_timer;

void ShiftRegSampleStart(void *arg);

const esp_timer_create_args_t shiftreg_timer_args = { .callback = &ShiftRegSampleStart, .name = "shiftregTimer" };

void ShiftRegSampleStart(void *arg)
{
    xEventGroupSetBits(digio_event_group, SHIFTREG_SAMPLE_START_BIT);
}

static esp_err_t shiftreg_txrx_transaction(uint8_t *tx, uint8_t *rx, int bits)
{
    memset(&spi_shift_transaction, 0, sizeof(spi_shift_transaction));
    spi_shift_transaction.cmd = 0;
    spi_shift_transaction.addr = 0;
    spi_shift_transaction.length = bits;
    spi_shift_transaction.tx_buffer = tx;
    spi_shift_transaction.rx_buffer = rx;
    spi_shift_transaction.rxlength = 0;
    esp_err_t err = spi_device_polling_transmit(spi_shift_handle, &spi_shift_transaction);
    ESP_ERROR_CHECK(err);
    return err;
}

static void shift_reg_task(void *pvParameter)
{
    while (1)
    {
        xEventGroupWaitBits(digio_event_group, SHIFTREG_SAMPLE_START_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        xSemaphoreTake(xSemaphoreShiftRegHandle, portMAX_DELAY);
        shiftreg_txrx_transaction(outputs, inputs, 8);
        xSemaphoreGive(xSemaphoreShiftRegHandle);
    }
}

esp_err_t ShiftRegInit(void)
{
    spi_device_interface_config_t devcfg = { .command_bits = 0, .address_bits = 0, .mode = 0, .clock_speed_hz = CONFIG_SHIFTREG_SPI_CLOCK_HZ, .queue_size = 10, .spics_io_num = 12 };
    esp_err_t ret = spi_bus_add_device(CONFIG_SHIFTREG_SPI_HOST, &devcfg, &spi_shift_handle);
    ESP_ERROR_CHECK(ret);
    outputs[0] = 0b00110000;
    digio_event_group = xEventGroupCreate();
    xSemaphoreShiftRegHandle = xSemaphoreCreateBinaryStatic(&xSemaphoreShiftRegBuf);
    xSemaphoreGive(xSemaphoreShiftRegHandle);
    ESP_ERROR_CHECK(esp_timer_create(&shiftreg_timer_args, &shiftreg_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(shiftreg_timer, CONFIG_SHIFTREG_SAMPLES_MS * 1000));
    ESP_LOGI(TAG, "HC595 SPI device init OK");
    return ESP_OK;
}

esp_err_t vgpio_set_level(virtual_gpio_num_t gpio_num, uint8_t *gpio_level)
{
    if (gpio_num < 0 || gpio_num >= VGPIO_NUM_MAX)
        return ESP_ERR_INVALID_ARG;
    uint8_t lv = *gpio_level & 1;
    xSemaphoreTake(xSemaphoreShiftRegHandle, portMAX_DELAY);
    outputs[0] = (outputs[0] & ~(1 << gpio_num)) | (lv << gpio_num);
    shiftreg_txrx_transaction(outputs, inputs, 8);
    xSemaphoreGive(xSemaphoreShiftRegHandle);
    return ESP_OK;
}

esp_err_t vgpio_get_level(virtual_gpio_num_t gpio_num, uint8_t *gpio_level)
{
    if (gpio_num < 0 || gpio_num >= VGPIO_NUM_MAX)
        return ESP_ERR_INVALID_ARG;
    *gpio_level = (outputs[0] & (1 << gpio_num)) ? 1 : 0;
    return ESP_OK;
}

esp_err_t vgpio_set_reg(uint8_t reg)
{
    if (xSemaphoreTake(xSemaphoreShiftRegHandle, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        outputs[0] = (outputs[0] & 0b11110001) | (reg << 1);
        shiftreg_txrx_transaction(outputs, inputs, 8);
        xSemaphoreGive(xSemaphoreShiftRegHandle);
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "Can't obtain SPI bus");
        return ESP_ERR_NOT_FINISHED;
    }
}



#endif