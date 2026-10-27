#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

#define DHT11_GPIO_PORT GPIOA
#define DHT11_GPIO_PIN  GPIO_Pin_5
#define DHT11_GPIO_CLK  RCC_APB2Periph_GPIOA

void DHT11_Init(void);
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);

#endif