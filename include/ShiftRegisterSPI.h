/*
 * ShiftRegisterSPI.h
 *
 *  Created on: Apr 18, 2025
 *      Author: bogd
 */

#ifndef COMPONENTS_WEBGUIAPP_INCLUDE_SHIFTREGISTERSPI_H_
#define COMPONENTS_WEBGUIAPP_INCLUDE_SHIFTREGISTERSPI_H_

#include "esp_err.h"

typedef enum {
    VGPIO_NUM_NC = -1,
    VGPIO_NUM_0 = 0,
    VGPIO_NUM_1 = 1,
    VGPIO_NUM_2 = 2,
    VGPIO_NUM_3 = 3,
    VGPIO_NUM_4 = 4,
    VGPIO_NUM_5 = 5,
    VGPIO_NUM_6 = 6,
    VGPIO_NUM_7 = 7,
    VGPIO_NUM_MAX,
/** @endcond */
} virtual_gpio_num_t;

typedef enum {
    VGPIO_MOTOR_IN1 = 0,
    VGPIO_MOTOR_IN2,
    VGPIO_PILOT_RELAY,
    VGPIO_RELAY1,
    VGPIO_RELAY2,
    VGPIO_TRIAC1,
    VGPIO_TRIAC2,
    VGPIO_TRIAC3

} virtual_gpio_funct_t;

esp_err_t ShiftRegInit(void);
esp_err_t vgpio_set_level(virtual_gpio_num_t gpio_num, uint8_t *gpio_level);
esp_err_t vgpio_get_level(virtual_gpio_num_t gpio_num, uint8_t *gpio_level);
esp_err_t vgpi_get_level(int gpio_num, uint8_t *gpio_level);
esp_err_t vgpio_set_reg(uint8_t reg);

void GPIOExtenderTxRx(uint8_t *tx, uint8_t *rx, int bt);
void GPIOInputRead(int num, int *val);
void GPIOInputWrite(int num, int *val);



#endif /* COMPONENTS_WEBGUIAPP_INCLUDE_SHIFTREGISTERSPI_H_ */
