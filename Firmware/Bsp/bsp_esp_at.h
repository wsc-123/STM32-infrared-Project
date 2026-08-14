#ifndef __BSP_ESP_AT_H
#define __BSP_ESP_AT_H

#include "stm32f10x.h"

/* ESP-01S(AT 固件 v1.5.4.1) 驱动 —— 巴法云 TCP 接入。
 *
 * 角色：STM32 是主控，主动发 AT 指令让 ESP 连 WiFi、连巴法云 TCP、
 *       同时订阅自有 App 和米家主题，然后定时发 ping 保活。云端把
 *       命令通过 TCP 推下来，本驱动解析 topic= 和 msg= 后交给上层。
 *
 * 物理连接：走 USART1(PA9/PA10)，与 bsp_usart 共用那条串口和环形缓冲。
 *   PA9  USART1_TX -> ESP RX
 *   PA10 USART1_RX <- ESP TX      115200
 *
 * 用法：
 *   esp_at_init();                     // 上电初始化（清状态）
 *   while(1){
 *       esp_at_task();                 // 主循环里不停跑：驱动联网状态机 + 收发
 *       if (esp_at_take_message(&src,buf,n)) {
 *           ...dispatch(src,buf)...
 *       }
 *   }
 */

/* 联网状态，供 OLED / 调试显示 */
typedef enum {
    ESP_ST_RESET = 0,   /* 复位/初始 */
    ESP_ST_HWRESET,     /* 硬件复位 ESP 中：PB0 拉低脉冲 + 等 ESP 启动 */
    ESP_ST_WIFI,        /* 正在连 WiFi */
    ESP_ST_WIFI_OK,     /* WiFi 已连上，拿到 IP */
    ESP_ST_TCP,         /* 正在连巴法云 TCP */
    ESP_ST_ONLINE,      /* TCP 通 + 已订阅，正常在线 */
    ESP_ST_ERROR        /* 出错，等待重试 */
} esp_state_t;

/* 最近一次进入 ERROR 时卡在哪一环，供 OLED 细分显示 ERR AT/WIFI/TCP/LOGIN */
typedef enum {
    ESP_ERR_NONE = 0,
    ESP_ERR_AT,         /* 基础 AT / CWMODE 无响应 */
    ESP_ERR_WIFI,       /* 连路由(CWJAP)失败 */
    ESP_ERR_TCP,        /* 连巴法云 TCP / 掉线 */
    ESP_ERR_LOGIN       /* TCP 通了但订阅(登录巴法云)失败 */
} esp_err_t;

/* 下行消息来自哪个巴法云主题。 */
typedef enum {
    ESP_MSG_NONE = 0,
    ESP_MSG_APP,          /* BEMFA_TOPIC：自有 App / 学习调试 */
    ESP_MSG_MIJA          /* BEMFA_MI_TOPIC：米家空调 */
} esp_msg_source_t;

void        esp_at_init(void);

/* 主循环调用：推进联网状态机、发心跳、收解析云端下发。非阻塞。 */
void        esp_at_task(void);

/* 取一条云端下发消息及其主题来源。msg 已解码并以 '\0' 结尾。 */
uint8_t     esp_at_take_message(esp_msg_source_t *source,
                                char *out, uint16_t max_len);

/* 主动往当前主题发布一条消息（回状态给手机 App 用），成功返回 1。 */
uint8_t     esp_at_publish(const char *msg);

/* 用 /up 更新米家空调主题状态，不向订阅端回推。 */
uint8_t     esp_at_publish_mijia(const char *msg);

/* 当前联网状态 / 拿到的 IP 字符串（未连上时为 "0.0.0.0"）。供 OLED 显示。 */
esp_state_t esp_at_state(void);
const char *esp_at_ip(void);

/* 最近一次失败卡在哪一环，供 OLED 显示 ERR AT/WIFI/TCP/LOGIN。ONLINE 正常时为 NONE。 */
esp_err_t   esp_at_last_err(void);

#endif /* __BSP_ESP_AT_H */
