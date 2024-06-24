/* Copyright 2024 Bogdan Pilyugin
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
 *   File name: EEPROM.c
 *     Project: EVCHS_Stick
 *  Created on: 2024-06-20
 *      Author: bogd
 * Description:	
 */

#include "webguiapp.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

#define TAG "EEPROMDriver"

#define I2C_MASTER_TIMEOUT_MS       1000
#define I2C_MASTER_NUM              CONFIG_I2C_HOST                           /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_SDA_GPIO         CONFIG_I2C_SDA_GPIO         /*!< I2C master i2c SDA pin*/
#define I2C_MASTER_SCL_GPIO         CONFIG_I2C_SCL_GPIO                          /*!< I2C master i2c SCL pin*/

#define EEPR24CXX_ADDR                  0xA0                        /*!< Slave address of 24Cxx devices*/

#define EEPROM_WRITE_PAGE_SIZE 32
#define EEPROM_WRITE_MAX_ATTEMPTS 20

typedef struct
{
    i2c_port_t port;            // I2C port number
    uint8_t addr;               // I2C address
    uint32_t clk_speed;             // I2C clock frequency for master mode
} i2c_dev_t;

i2c_dev_t eepr_24c32 = {
        .port = I2C_MASTER_NUM,
        .addr = EEPR24CXX_ADDR >> 1,
        .clk_speed = CONFIG_I2C_CLOCK
};

static esp_err_t i2c_dev_read(const i2c_dev_t *dev, const void *out_data, size_t out_size, void *in_data,
                              size_t in_size)
{
    if (!dev || !in_data || !in_size)
        return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (out_data && out_size)
    {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write(cmd, (void*) out_data, out_size, true);
    }
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | 1, true);
    i2c_master_read(cmd, in_data, in_size, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(dev->port, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /*
     if (res != ESP_OK)
     ESP_LOGE(TAG, "Could not read from device [0x%02x at %d]: %d", dev->addr, dev->port, res);
     */
    i2c_cmd_link_delete(cmd);
    return res;
}

static esp_err_t i2c_dev_write(const i2c_dev_t *dev, const void *out_reg, size_t out_reg_size, const void *out_data,
                               size_t out_size)
{
    if (!dev || !out_data || !out_size)
        return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true);
    if (out_reg && out_reg_size)
        i2c_master_write(cmd, (void*) out_reg, out_reg_size, true);
    i2c_master_write(cmd, (void*) out_data, out_size, true);
    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(dev->port, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    /*
     if (res != ESP_OK)
     ESP_LOGE(TAG, "Could not write to device [0x%02x at %d]: %d", dev->addr, dev->port, res);
     */
    i2c_cmd_link_delete(cmd);
    return res;
}



esp_err_t eepr_i2c_read(uint16_t addr, uint8_t *data, int length)
{

    int attempts = 0;
    esp_err_t err;
    uint8_t adr[] = { (uint8_t)(addr >> 8), (uint8_t)(addr & 0xff) };
    while ((err = i2c_dev_read(&eepr_24c32, adr, 2, data, length)) != ESP_OK)
    {
        //ESP_LOGW(TAG, "EEPROM not ready attempt %d", attempts);
        if (++attempts >= EEPROM_WRITE_MAX_ATTEMPTS) //Critical error
        {
            ESP_LOGE(TAG, "EEPROM read critical error!");
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}


esp_err_t eepr_i2c_write(uint16_t addr, uint8_t *data, int length)
{
    int written = 0, attempts = 0;
    uint16_t block_addr = addr;
    esp_err_t err;
    while ((length - written) > 0)
    {
        int block_len = MIN(length - written, EEPROM_WRITE_PAGE_SIZE);
        uint8_t adr[] = { (uint8_t)(block_addr >> 8), (uint8_t)(block_addr & 0xff) };
        while ((err = i2c_dev_write(&eepr_24c32, adr, 2, data + written, block_len)) != ESP_OK)
        {
            //ESP_LOGW(TAG, "EEPROM not ready attempt %d", attempts);
            if (++attempts >= EEPROM_WRITE_MAX_ATTEMPTS) //Critical error
            {
                ESP_LOGE(TAG, "EEPROM write critical error!");
                return err;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        attempts = 0;
        written += block_len;
        //ESP_LOGI(TAG, "written %d byte from addr %d", written, block_addr);
        block_addr += EEPROM_WRITE_PAGE_SIZE;

    }
    return ESP_OK;
}

