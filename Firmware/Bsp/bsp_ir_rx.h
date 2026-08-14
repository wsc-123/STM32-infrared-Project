#ifndef __BSP_IR_RX_H
#define __BSP_IR_RX_H

#include "stm32f10x.h"

/* 红外接收 / 学习：1838B 输出接 PA0，走外部中断 EXTI0。
 * 1838B 空闲输出高电平，检测到 38kHz 载波时拉低，故：
 *   下降沿 = 一段 mark(载波) 开始
 *   上升沿 = 一段 space(无载波) 开始
 * 每个边沿用 DWT 打时间戳，相邻边沿之差即为一段持续时间(微秒)，
 * 交替存入缓冲，得到与发送格式一致的原始时序数组。
 *
 * 帧结束判定：距离上一个边沿超过 IR_RX_IDLE_US 仍无新边沿，视为一帧结束。 */

#define IR_RX_MAX_EDGES     600        /* 单帧最多记录多少段（空调整帧较长）*/

void            ir_rx_init(void);

void            ir_rx_enable(void);    /* 使能 EXTI0 中断 */
void            ir_rx_disable(void);   /* 关闭 EXTI0 中断（发送时用）*/
void            ir_rx_reset(void);     /* 清空当前帧与就绪标志 */

/* 主循环轮询：检测帧是否已结束，更新就绪标志 */
void            ir_rx_poll(void);

/* 是否有一帧就绪 */
uint8_t         ir_rx_ready(void);

/* 取就绪帧：返回时序数组指针，*len 写入段数 */
const uint16_t *ir_rx_get(uint16_t *len);

#endif /* __BSP_IR_RX_H */
