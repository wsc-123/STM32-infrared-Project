#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

/* ============================================================
 *  用户配置区  ——  你只需要改这一个文件
 * ============================================================
 *
 *  将本文件复制为 app_config.h 后，填写 WiFi 账号密码、巴法云私钥
 *  和主题。app_config.h 已被 Git 忽略，不能上传到公开仓库。
 *
 *  ★ 必填三项：WIFI_SSID、WIFI_PASS、BEMFA_UID
 *  ★ BEMFA_TOPIC 供自有 App/学习调试使用
 *  ★ BEMFA_MI_TOPIC 必须以 005 结尾，巴法云才会同步成米家空调
 * ============================================================ */


/* -------- 1. WiFi（连你家路由，2.4GHz） -------- */
/* ESP8266 只支持 2.4GHz，别填 5GHz 的 WiFi 名。 */
#define WIFI_SSID           "your_wifi_ssid"
#define WIFI_PASS           "your_wifi_password"


/* -------- 2. 巴法云 TCP 接入 -------- */
/* 私钥获取：登录 https://cloud.bemfa.com -> 右上角/控制台 ->
 * 复制那串私钥（约 32 位十六进制），粘到下面。 */
#define BEMFA_UID           "your_bemfa_uid_here"

/* 服务器地址 / 端口：巴法云 TCP 固定这两个，一般不用改。 */
#define BEMFA_HOST          "bemfa.com"
#define BEMFA_PORT          "8344"

/* 自有 App 控制主题：保留 SEND/LEARN/STATUS 等维护命令。 */
#define BEMFA_TOPIC         "infrared"

/* 米家空调主题：只承载 on/off/on#模式#温度 等标准空调消息。 */
#define BEMFA_MI_TOPIC      "infrared005"


/* -------- 3. 设备名（显示在 OLED 第一行的开机提示） -------- */
#define DEVICE_NAME         "IR AirCon"


/* -------- 4. 时间参数（一般不用改） -------- */
/* 心跳间隔（毫秒）。巴法云要求 <65 秒发一次，这里取 30 秒稳妥。 */
#define BEMFA_PING_MS       30000U

/* 在线健康检查：定时查询 ESP 的 WiFi/TCP 状态。拔掉 ESP 电源后，
 * 最迟约 ESP_HEALTH_CHECK_MS + ESP_HEALTH_TIMEOUT_MS 可发现离线。 */
#define ESP_HEALTH_CHECK_MS 10000U
#define ESP_HEALTH_TIMEOUT_MS 1500U

/* 两次 ESP 硬件复位至少间隔这么久，避免断电时反复拉低 RST。 */
#define ESP_HWRESET_COOLDOWN_MS 60000U

/* 单条 AT 指令等应答的超时（毫秒）。连 WiFi 那条会久一点，
 * 驱动内部对 CWJAP 单独放宽，这里是普通指令的基准值。 */
#define AT_CMD_TIMEOUT_MS   2000U

/* 订阅巴法云（登录）时等 ">" 提示符和 SEND OK 的超时（毫秒）。
 * 服务器偶尔应答慢，放宽到 5 秒，避免一次抖动引发整轮重连。 */
#define BEMFA_SUB_TIMEOUT_MS 5000U

/* 联网任一步失败后，隔多久重头再来一遍（毫秒）。 */
#define NET_RETRY_MS        5000U


#endif /* __APP_CONFIG_H */
