/*
 * stm32main.h
 *
 *  Created on: May 10, 2021
 *      Author: Helena
 */

#ifndef INC_COMPONENTS_STM32MAIN_H_
#define INC_COMPONENTS_STM32MAIN_H_

#include "stm32f4xx_hal.h"

struct Handles_S {
     UART_HandleTypeDef *huart;
     // Aqui añadimos todos los XXX_HandleTypeDef que necesitaremos.
     ADC_HandleTypeDef *hadc1;
     TIM_HandleTypeDef *htim3;
     I2C_HandleTypeDef *hi2c1;
};

void setup(struct Handles_S *handles);
void loop(void);


#endif /* INC_COMPONENTS_STM32MAIN_H_ */
