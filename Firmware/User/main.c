#include "stm32f10x.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_usart.h"
#include "bsp_ir_tx.h"
#include "bsp_ir_rx.h"
#include "bsp_oled.h"
#include "bsp_esp_at.h"
#include "bsp_watchdog.h"
#include "ir_codes.h"
#include "cmd_parser.h"
#include "app_config.h"
#include <stdio.h>
#include <string.h>

/*
 * STM32 红外空调网关 —— 主程序（巴法云 TCP 版）
 *
 *   手机 App -> 巴法云(TCP) -> ESP-01S(AT) -> STM32(USART1)
 *        STM32 解析命令 -> 学习: 1838B(PA0) 捕获原始遥控帧
 *                       -> 回放: TIM3(PA6) 38kHz 载波驱动红外 LED -> 空调
 *        OLED(PB8/PB9) 实时显示 WiFi/网络状态、IP、学习/回放结果
 *
 * 引脚：
 *   PC13  板载状态灯（低电平点亮）
 *   PA9   USART1_TX -> ESP-01S RX
 *   PA10  USART1_RX <- ESP-01S TX      (115200, AT 协议，驱动独占)
 *   PA2   USART2_TX -> USB-TTL RX      (调试 printf, 115200)
 *   PA3   USART2_RX <- USB-TTL TX
 *   PA0   1838B OUT  (红外接收, EXTI0)
 *   PA6   TIM3_CH1   (红外发射 38kHz)
 *   PB8   OLED SCL   (软件 I2C)
 *   PB9   OLED SDA
 */

/* ERROR 状态按最近失败环节细分文本，方便定位卡在哪一步 */
static const char *err_text(esp_err_t e)
{
    switch (e) {
    case ESP_ERR_AT:    return "ESP:ERR Net:--";    /* ESP 无响应/断电 */
    case ESP_ERR_WIFI:  return "WiFi:ERR Net:--";  /* 连路由失败 */
    case ESP_ERR_TCP:   return "WiFi:OK Net:ERR";  /* TCP/云端掉线 */
    case ESP_ERR_LOGIN: return "WiFi:OK Sub:ERR";  /* TCP 通但订阅失败 */
    default:            return "WiFi:ERR retry";
    }
}

/* 把 ESP 联网状态映射成 OLED 第2行文本 */
static const char *net_text(esp_state_t st)
{
    switch (st) {
    case ESP_ST_RESET:   return "WiFi:.. Net:..";
    case ESP_ST_HWRESET: return "ESP reset...";
    case ESP_ST_WIFI:    return "WiFi:...";
    case ESP_ST_WIFI_OK: return "WiFi:OK Net:..";
    case ESP_ST_TCP:     return "WiFi:OK Net:...";
    case ESP_ST_ONLINE:  return "WiFi:OK Net:OK";
    case ESP_ST_ERROR:   return err_text(esp_at_last_err());
    default:             return "WiFi:?  Net:?";
    }
}

int main(void)
{
    esp_state_t last_st = (esp_state_t)0xFF;   /* 强制第一次刷新 */
    char        ip_line[20];
    char        last_ip[20] = "";
    char        cmd[64];
    esp_msg_source_t cmd_source;
    uint8_t     restored_slots;

    /* SystemInit() 已在启动文件中把主频配到 72MHz */

    /* 中断优先级分组：2 位抢占 + 2 位子优先级 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    bsp_delay_init();       /* DWT 微秒计时 + SysTick 毫秒时基，务必最先 */
    led_init();
    usart_init();
    OLED_Init();
    ir_tx_init();
    ir_rx_init();
    ir_codes_init();
    restored_slots = ir_codes_load();
    cmd_parser_init();
    esp_at_init();

    ir_rx_enable();

    printf("\r\n==== STM32 IR Gateway (bemfa TCP) ====\r\n");
    printf("clk=%luHz, connecting WiFi...\r\n", (unsigned long)SystemCoreClock);
    printf("[store] restored %u IR slots\r\n", restored_slots);

    /* OLED 开机界面。若上次复位由独立看门狗触发，第一行显示 WDG!
     * —— 用于肉眼确认“看门狗反复复位”是否真的发生 */
    OLED_Clear();
    OLED_ShowString(1, 1, DEVICE_NAME);
    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET) {
        OLED_ShowString(1, (uint8_t)(strlen(DEVICE_NAME) + 2), "WDG!");
        printf("[boot] reset caused by IWDG watchdog!\r\n");
    } else {
        OLED_ShowString(1, (uint8_t)(strlen(DEVICE_NAME) + 2), "boot");
    }
    RCC_ClearFlag();
    OLED_ShowString(2, 1, net_text(ESP_ST_RESET));
    OLED_ShowString(3, 1, "IP 0.0.0.0");
    snprintf(cmd, sizeof(cmd), "slots:%u", restored_slots);
    OLED_ShowString(4, 1, cmd);

    /* 上电闪灯表示活着 */
    for (int i = 0; i < 3; i++) {
        led_on();  delay_ms(80);
        led_off(); delay_ms(80);
    }

    watchdog_init();

    while (1) {
        watchdog_feed();

        /* 1) 推进 ESP 联网状态机 + 收发（非阻塞） */
        esp_at_task();

        /* 2) 状态有变化时刷新 OLED 第2、3行 */
        {
            esp_state_t st = esp_at_state();
            const char *ip = esp_at_ip();
            if (st != last_st || strcmp(ip, last_ip) != 0) {
                last_st = st;
                strncpy(last_ip, ip, sizeof(last_ip) - 1);
                last_ip[sizeof(last_ip) - 1] = '\0';
                OLED_ClearLine(2);
                OLED_ShowString(2, 1, net_text(st));

                snprintf(ip_line, sizeof(ip_line), "IP %s", ip);
                OLED_ClearLine(3);
                OLED_ShowString(3, 1, ip_line);

                printf("[net] %s  ip=%s\r\n", net_text(st), ip);

                if (st == ESP_ST_ONLINE) {
                    OLED_ClearLine(1);
                    OLED_ShowString(1, 1, DEVICE_NAME);
                    OLED_ShowString(1, (uint8_t)(strlen(DEVICE_NAME) + 2), "OK");
                }
            }
        }

        /* 3) 有云端下发命令则执行 */
        if (esp_at_take_message(&cmd_source, cmd, sizeof(cmd))) {
            printf("[cmd:%s] %s\r\n",
                   (cmd_source == ESP_MSG_MIJA) ? "mijia" : "app", cmd);
            if (cmd_source == ESP_MSG_MIJA) {
                cmd_parser_exec_mijia(cmd);
            } else {
                cmd_parser_exec(cmd);
            }
        }

        /* 4) 处理红外学习/接收 */
        cmd_parser_service_ir();
    }
}
