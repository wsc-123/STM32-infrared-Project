#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "stm32f10x.h"

/* 板载 LED：STM32F103C8T6 最小系统板 LED 接在 PC13，低电平点亮。 */

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

#endif /* __BSP_LED_H */
