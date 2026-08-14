#ifndef __CMD_PARSER_H
#define __CMD_PARSER_H

#include "stm32f10x.h"

/* 命令解析层：把一条文本命令翻译成动作（点灯 / 学习 / 回放）。
 *
 * 命令来源：巴法云经 ESP-01S(AT) 下发的 msg 文本，由 main 循环按
 *           主题来源交给自有 App 或米家空调解析入口。
 *           （USART1 已被 ESP-AT 驱动独占，不再走整行读取。）
 *
 * 用法（主循环）：
 *   char cmd[64];
 *   if (source == ESP_MSG_APP)  cmd_parser_exec(cmd);
 *   if (source == ESP_MSG_MIJA) cmd_parser_exec_mijia(cmd);
 *   cmd_parser_service_ir();     // 处理红外学习/缓存
 */

void cmd_parser_init(void);

/* 执行一条命令（来自 MQTT 下发或调试）。line 以 '\0' 结尾。 */
void cmd_parser_exec(const char *line);

/* 执行米家/巴法云空调协议命令：on、off、on#模式#温度... */
void cmd_parser_exec_mijia(const char *line);

/* 主循环轮询红外接收：学习中则存槽，否则缓存供 DUMP。非阻塞。 */
void cmd_parser_service_ir(void);

#endif /* __CMD_PARSER_H */
