#ifndef __BSP_IR_TX_H
#define __BSP_IR_TX_H

#include "stm32f10x.h"

/* 红外发射：TIM3_CH1 (PA6) 输出约 38kHz 载波，经 1k 电阻驱动 S8050 基极，
 * S8050 低边开关驱动红外 LED。
 *
 * 发送一帧 = 一串“持续时间”数组（单位微秒），mark/space 交替：
 *   data[0] = 第 1 段载波(mark)时长
 *   data[1] = 第 1 段无载波(space)时长
 *   data[2] = 第 2 段载波(mark)时长 ...
 * 偶数下标发载波，奇数下标关载波。 */

void ir_tx_init(void);

/* 阻塞发送一帧。发送期间会临时关闭红外接收中断，避免自发自收干扰。 */
void ir_tx_send(const uint16_t *data, uint16_t len);

/* 调试用：单纯开/关载波（步骤 4 验证 38kHz 是否发得出去） */
void ir_tx_carrier(uint8_t on);

#endif /* __BSP_IR_TX_H */
