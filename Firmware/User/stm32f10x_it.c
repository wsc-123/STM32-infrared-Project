#include "stm32f10x_it.h"

/* Cortex-M3 内核异常处理。外设中断放在各自的 bsp 文件里。 */

void NMI_Handler(void)        { }

void HardFault_Handler(void)
{
    /* 硬件错误：死循环，方便用调试器抓现场 */
    while (1) { }
}

void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }
