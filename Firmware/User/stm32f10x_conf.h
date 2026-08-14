#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

/* 标准外设库配置：本工程用到的外设模块头文件。
 * 用不到的可以留着，编译器只编进被调用的部分。 */

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "misc.h"          /* NVIC / SysTick 配置 */

/* 断言：调试期打开可帮助定位参数错误，发布可注释掉 */
#ifdef  USE_FULL_ASSERT
    #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
    void assert_failed(uint8_t *file, uint32_t line);
#else
    #define assert_param(expr) ((void)0)
#endif

#endif /* __STM32F10x_CONF_H */
