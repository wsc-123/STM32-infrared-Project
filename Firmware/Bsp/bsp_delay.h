#ifndef __BSP_DELAY_H
#define __BSP_DELAY_H

#include "stm32f10x.h"

/* 基于 Cortex-M3 内核 DWT 计数器的微秒级延时与时间戳。
 * 不占用任何定时器，红外收发都依赖它做精确计时。 */

void     bsp_delay_init(void);        /* 使能 DWT 周期计数器，上电只调一次 */
void     delay_us(uint32_t us);       /* 微秒级阻塞延时 */
void     delay_ms(uint32_t ms);       /* 毫秒级阻塞延时 */
uint32_t bsp_millis(void);            /* 1ms 单调时基，约 49 天回绕 */
uint32_t bsp_get_cycle(void);         /* 读取当前 CPU 周期计数（用于打时间戳） */
uint32_t bsp_cycles_per_us(void);     /* 每微秒的周期数 = 主频/1e6，72MHz 下为 72 */

#endif /* __BSP_DELAY_H */
