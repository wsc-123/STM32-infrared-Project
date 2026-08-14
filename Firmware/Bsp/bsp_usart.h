#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f10x.h"

/* USART1 (PA9/PA10) : 与 ESP-01S 通信，接收手机透传下来的文本命令。
 *                     开启接收中断 + 环形缓冲，主循环整行取出。
 * USART2 (PA2/PA3)  : 调试串口，printf 重定向到此，接 USB-TTL 看日志。
 *
 * 约定：手机/ESP 发来的命令以 '\n' 或 '\r' 结尾的一行 ASCII，例如 "POWER_ON\n"。 */

void    usart_init(void);

/* 回复给 ESP-01S / 手机（走 USART1） */
void    esp_send_string(const char *s);

/* 从 USART1 环形缓冲里取一整行（不含换行符，自动补 '\0'）。
 * 取到完整一行返回 1，否则返回 0。空行自动跳过。 */
uint8_t esp_read_line(char *out, uint16_t max_len);

/* ---- 供 bsp_esp_at 驱动使用的原始字节接口 ----
 * AT 协议不是按行处理，需要逐字节抽取环形缓冲做关键字匹配。 */

/* 从 USART1 环形缓冲取一个字节。取到写入 *out 返回 1，缓冲空返回 0。 */
uint8_t esp_rx_byte(uint8_t *out);

/* 丢弃环形缓冲里所有未读字节，并复位整行组包状态。
 * 发下一条 AT 指令前调用，避免旧应答残留干扰关键字匹配。 */
void    esp_rx_clear(void);

#endif /* __BSP_USART_H */
