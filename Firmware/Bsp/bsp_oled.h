#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include "stm32f10x.h"

/* 0.96" OLED (SSD1306, 128x64) 软件 I2C 驱动。
 * 移植自参考工程(江协风格)，改动：
 *   - 内部延时统一用 bsp_delay，不再自带忙等/不依赖 SysTick；
 *   - ShowString 为纯 ASCII 版本(状态/IP/数字都够用)。
 *
 * 接线(软件 I2C)：
 *   PB8 -> OLED SCL
 *   PB9 -> OLED SDA
 *   VCC -> 3.3V   GND -> GND
 *
 * 坐标：Line 行 1~4，Column 列 1~16（每字符 8x16 像素）。 */

void OLED_Init(void);
void OLED_Clear(void);

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* 清掉某一行(1~4)剩余列，避免上一次显示的长文字留尾巴。 */
void OLED_ClearLine(uint8_t Line);

#endif /* __BSP_OLED_H */
