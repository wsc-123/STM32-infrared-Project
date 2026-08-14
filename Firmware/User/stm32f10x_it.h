#ifndef __STM32F10x_IT_H
#define __STM32F10x_IT_H

#include "stm32f10x.h"

/* 内核异常处理函数声明。
 * 外设中断（EXTI0_IRQHandler / USART1_IRQHandler）分别定义在
 * bsp_ir_rx.c 和 bsp_usart.c 中，本文件不重复声明。 */

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

#endif /* __STM32F10x_IT_H */
