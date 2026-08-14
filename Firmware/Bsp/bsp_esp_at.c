#include "bsp_esp_at.h"
#include "bsp_usart.h"
#include "bsp_delay.h"
#include "app_config.h"
#include <string.h>
#include <stdio.h>

/* ============================================================
 *  ESP-01S AT 固件(v1.5.4.1) + 巴法云 TCP 驱动
 *
 *  设计要点：
 *  - 不阻塞主循环。联网分若干步，每步发一条 AT 指令后，在后续的
 *    esp_at_task() 调用里轮询 USART1 环形缓冲，看有没有出现期望的
 *    应答关键字（"OK" / "WIFI GOT IP" / "CONNECT" 等），超时就重来。
 *  - 快速启动：上电先用 AT 探测(最多 3 次)，ESP 有响应就不硬复位；
 *    连 WiFi 前先查 AT+CWJAP?，已连上(含 ESP 自动重连)就跳过 CWJAP，
 *    避免每次开机都强制重新关联一遍(省 4~8 秒，也少一次大电流冲击)。
 *  - 收数据：巴法云推送格式  +IPD,<len>:cmd=2&uid=..&topic=..&msg=XXX
 *    同时解析 topic 和 msg，让上层区分自有 App 与米家空调命令。
 *  - 在线监测：每 ESP_HEALTH_CHECK_MS 查询 AT+CIPSTATUS；每次心跳等待
 *    CIPSEND 提示符和 SEND OK。ESP 断电、WiFi/TCP 断线都能主动发现。
 *  - 重连 TCP 前先 AT+CIPCLOSE 清掉可能残留的旧连接，防止 CIPSTART
 *    因 "ALREADY CONNECTED" 连续失败而升级成硬复位。
 * ============================================================ */

/* ---- ESP 硬件复位脚 (PB0 -> ESP RST，开漏，10k 上拉到 3.3V) ---- */
/* 默认释放(浮空高，由外部 10k 拉高)；拉低才让 ESP 复位。
 * 开漏输出：写 0 拉低，写 1 释放(不主动输出高，靠外部上拉)，
 * 这样即便 STM32 与 ESP 电平配合有偏差也不会硬顶。 */
#define ESP_RST_PORT        GPIOB
#define ESP_RST_PIN         GPIO_Pin_0
#define ESP_RST_CLK         RCC_APB2Periph_GPIOB

#define ESP_RST_LOW_MS      200U     /* RST 拉低脉冲宽度 */
#define ESP_RST_BOOT_MS     2500U    /* 释放后等 ESP 启动的时间(2~3s) */
#define ESP_FAIL_LIMIT      3U       /* 连续失败几次后触发硬复位 */

/* 上电 AT 探测：ESP 冷启动到 AT 可用约 0.3~1s，探测间隔 1s、最多 3 次。
 * 全部无响应才说明 ESP 卡死/没上电，此时才值得花 2.7s 硬复位。 */
#define ESP_AT_PROBE_MS     1000U    /* 单次 AT 探测等应答时间 */
#define ESP_AT_PROBE_TRIES  3U       /* 探测次数上限 */

/* 连 WiFi 前先查 AT+CWJAP? 是否已连上(ESP 掉电重启后会用存储的账号
 * 自动重连)。未连上时每 ESP_WIFI_QUERY_MS 再查一次，给自动重连留
 * ESP_WIFI_QUERY_TRIES 轮时间(约 8s)，都没连上才主动发 CWJAP。 */
#define ESP_WIFI_QUERY_MS    2000U   /* 两次查询之间的等待 */
#define ESP_WIFI_QUERY_TRIES 4U      /* 查询轮数上限 */

#define ESP_TCP_CLOSE_MS    1000U    /* AT+CIPCLOSE 等应答上限 */

/* CWJAP? 应答里"已连上本项目路由"的特征串(带引号防止误匹配子串) */
#define ESP_CWJAP_MATCH     "+CWJAP:\"" WIFI_SSID "\""

/* ESP_ST_ONLINE 内部的非阻塞操作。复用 s_substep，不新增对外状态。 */
#define ONLINE_OP_IDLE          0U
#define ONLINE_OP_HEALTH_WAIT   1U
#define ONLINE_OP_PING_PROMPT   2U
#define ONLINE_OP_PING_SEND_OK  3U

/* AT 交互用的行缓冲：累积 USART1 收到的字节，用于关键字匹配和 msg 解析 */
#define ESP_RXBUF_SIZE      512
static char     s_rx[ESP_RXBUF_SIZE];
static uint16_t s_rxlen = 0;

/* 联网状态机 */
static esp_state_t s_state   = ESP_ST_RESET;
static uint8_t     s_substep = 0;         /* 当前状态内的子步骤 */
static uint32_t    s_t_mark  = 0;         /* 计时基准(ms 累加) */
static uint32_t    s_ping_ms = 0;         /* 距上次心跳的毫秒 */
static uint32_t    s_health_ms = 0;       /* 距上次在线健康确认的毫秒 */
static uint32_t    s_hwreset_ms = 0;      /* 上次拉低 RST 的时刻 */
static uint8_t     s_hwreset_seen = 0;    /* 是否已经执行过硬复位 */

/* 失败恢复：记录连续失败次数与最近一次卡在哪一环 */
static uint8_t   s_fail_cnt = 0;          /* 连续联网失败计数 */
static esp_err_t s_last_err = ESP_ERR_NONE;

/* 上电探测 / 等自动重连的轮次计数 */
static uint8_t s_at_tries   = 0;          /* 已发出的 AT 探测次数 */
static uint8_t s_wifi_tries = 0;          /* 已完成的 CWJAP? 查询轮数 */

/* 拿到的 IP */
static char s_ip[20] = "0.0.0.0";

/* 待上层取走的命令 */
static char    s_cmd[64];
static uint8_t s_cmd_ready = 0;
static esp_msg_source_t s_cmd_source = ESP_MSG_NONE;

static uint32_t now_ms(void)
{
    return bsp_millis();
}

/* PB0 配成开漏输出，默认写 1(释放)，由外部 10k 上拉保持 ESP RST 高电平。 */
static void esp_rst_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(ESP_RST_CLK, ENABLE);

    gpio.GPIO_Pin   = ESP_RST_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;   /* 开漏：写1不主动拉高，靠外部上拉 */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(ESP_RST_PORT, &gpio);

    GPIO_SetBits(ESP_RST_PORT, ESP_RST_PIN);   /* 释放，让 ESP 正常运行 */
}

/* 拉低 RST：让 ESP 进入复位 */
static void esp_rst_assert(void)
{
    GPIO_ResetBits(ESP_RST_PORT, ESP_RST_PIN);
}

/* 释放 RST：ESP 开始启动 */
static void esp_rst_release(void)
{
    GPIO_SetBits(ESP_RST_PORT, ESP_RST_PIN);
}

/* 清空本地接收缓冲 + 底层环形缓冲，准备接收下一条应答 */
static void rx_flush(void)
{
    s_rxlen = 0;
    s_rx[0] = '\0';
    esp_rx_clear();               /* 清 bsp_usart 的环形缓冲 */
}

/* 把 USART1 环形缓冲里的新字节抽到本地行缓冲。返回新增字节数。 */
static uint16_t rx_pump(void)
{
    uint8_t  c;
    uint16_t n = 0;
    while (esp_rx_byte(&c)) {
        if (c == 0U) {
            c = '.';   /* 波特率不符的启动乱码可能带 \0，会截断 strstr 匹配 */
        }
        if (s_rxlen < ESP_RXBUF_SIZE - 1) {
            s_rx[s_rxlen++] = (char)c;
            s_rx[s_rxlen]   = '\0';
        } else {
            /* 缓冲满：保留后半段，丢前半段，避免长期收不到关键字卡死 */
            uint16_t keep = ESP_RXBUF_SIZE / 2;
            memmove(s_rx, s_rx + (s_rxlen - keep), keep);
            s_rxlen = keep;
            s_rx[s_rxlen++] = (char)c;
            s_rx[s_rxlen]   = '\0';
        }
        n++;
    }
    return n;
}

/* 发一条 AT 指令（自动补 \r\n），随后清缓冲准备收应答 */
static void at_send(const char *cmd)
{
    esp_send_string(cmd);
    esp_send_string("\r\n");
    rx_flush();
}

/* 发原始数据（不加 \r\n），用于 AT+CIPSEND 之后发协议体 */
static void at_send_raw(const char *data)
{
    esp_send_string(data);
}

/* Clear only this module's local AT match buffer. Do not clear the USART
 * hardware/ring buffer, otherwise a downlink that arrives while we publish a
 * reply can be dropped before the main state machine sees it. */
static void rx_local_clear(void)
{
    s_rxlen = 0;
    s_rx[0] = '\0';
}

/* 在已收到的缓冲里找关键字 */
static uint8_t rx_has(const char *kw)
{
    return (strstr(s_rx, kw) != 0) ? 1 : 0;
}

static uint8_t hex_value(char c, uint8_t *v)
{
    if (c >= '0' && c <= '9') {
        *v = (uint8_t)(c - '0');
        return 1U;
    }
    if (c >= 'A' && c <= 'F') {
        *v = (uint8_t)(c - 'A' + 10);
        return 1U;
    }
    if (c >= 'a' && c <= 'f') {
        *v = (uint8_t)(c - 'a' + 10);
        return 1U;
    }
    return 0U;
}

static void url_decode_inplace(char *s)
{
    char *r = s;
    char *w = s;

    while (*r) {
        if (*r == '%' && r[1] && r[2]) {
            uint8_t hi;
            uint8_t lo;
            if (hex_value(r[1], &hi) && hex_value(r[2], &lo)) {
                *w++ = (char)((hi << 4) | lo);
                r += 3;
                continue;
            }
        }
        *w++ = (*r == '+') ? ' ' : *r;
        r++;
    }
    *w = '\0';
}

static uint8_t is_trim_char(char c)
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '"' || c == '\'') ? 1U : 0U;
}

static void trim_inplace(char *s)
{
    char *p = s;
    uint16_t len;

    while (is_trim_char(*p)) {
        p++;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1U);
    }

    len = (uint16_t)strlen(s);
    while (len > 0U && is_trim_char(s[len - 1U])) {
        s[--len] = '\0';
    }
}

static uint8_t ip_is_valid(const char *ip)
{
    return (ip != 0 && ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0) ? 1U : 0U;
}

/* 从缓冲里解析 IP：AT+CIFSR 返回含 STAIP,"192.168.x.x" */
static uint8_t parse_ip(void)
{
    char tmp[sizeof(s_ip)];
    char *p = strstr(s_rx, "STAIP");
    uint16_t i = 0;

    if (p) {
        p = strchr(p, '"');
    }
    if (p) {
        p++;
        while (*p && *p != '"' && i < sizeof(s_ip) - 1) {
            tmp[i++] = *p++;
        }
        tmp[i] = '\0';
        if (ip_is_valid(tmp)) {
            strcpy(s_ip, tmp);
            return 1U;
        }
    }
    return 0U;
}

/* 找到包含指定字段的完整 +IPD 载荷边界。 */
static uint8_t parse_ipd_payload_bounds(char *field_pos,
                                        char **payload_start,
                                        char **payload_end)
{
    char    *p = s_rx;
    char    *ipd = 0;
    uint32_t len = 0;
    uint32_t avail;
    char    *payload;

    while ((p = strstr(p, "+IPD,")) != 0 && p < field_pos) {
        ipd = p;
        p += 5;
    }
    if (ipd == 0) {
        return 0;
    }

    p = ipd + 5;
    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        len = len * 10U + (uint32_t)(*p - '0');
        p++;
    }
    if (*p != ':') {
        return 0;
    }

    payload = p + 1;
    if (payload < s_rx || payload > (s_rx + s_rxlen)) {
        return 0;
    }

    avail = (uint32_t)((s_rx + s_rxlen) - payload);
    if (avail < len) {
        return 0;
    }

    *payload_start = payload;
    *payload_end   = payload + len;
    return 1;
}

/* 本地缓冲里若有尚未收全的 +IPD，则先等下一轮，避免健康查询清掉半包。 */
static uint8_t has_incomplete_ipd(void)
{
    char *p = s_rx;

    while ((p = strstr(p, "+IPD,")) != 0) {
        char *q = p + 5;
        char *payload;
        uint32_t len = 0;
        uint32_t avail;

        if (*q < '0' || *q > '9') {
            return 1U;
        }
        while (*q >= '0' && *q <= '9') {
            len = len * 10U + (uint32_t)(*q - '0');
            q++;
        }
        if (*q != ':') {
            return 1U;
        }
        payload = q + 1;
        avail = (uint32_t)((s_rx + s_rxlen) - payload);
        if (avail < len) {
            return 1U;
        }
        p = payload + len;
    }
    return 0U;
}

static char *find_payload_field(char *start, char *end, const char *name)
{
    uint16_t name_len = (uint16_t)strlen(name);
    char    *p;

    for (p = start; p + name_len <= end; p++) {
        if ((p == start || p[-1] == '&') &&
            memcmp(p, name, name_len) == 0) {
            return p + name_len;
        }
    }
    return 0;
}

static uint8_t payload_field_equals(const char *field, const char *end,
                                    const char *expected)
{
    uint16_t len = (uint16_t)strlen(expected);

    if (field == 0 || field + len > end || memcmp(field, expected, len) != 0) {
        return 0;
    }
    return (field + len == end || field[len] == '&' ||
            field[len] == '\r' || field[len] == '\n') ? 1U : 0U;
}

static uint8_t parse_downlink(void)
{
    char *p;
    char *topic;
    char *payload_start;
    char *payload_end;
    uint8_t have_ipd;
    uint8_t ended_by_delim = 0;

    if (s_cmd_ready) {
        return 0;
    }

    p = strstr(s_rx, "msg=");
    if (!p) {
        return 0;
    }

    have_ipd = parse_ipd_payload_bounds(p, &payload_start, &payload_end);
    if (!have_ipd) {
        payload_start = s_rx;
        payload_end = s_rx + s_rxlen;
    }

    topic = find_payload_field(payload_start, payload_end, "topic=");
    if (payload_field_equals(topic, payload_end, BEMFA_TOPIC)) {
        s_cmd_source = ESP_MSG_APP;
    } else if (payload_field_equals(topic, payload_end, BEMFA_MI_TOPIC)) {
        s_cmd_source = ESP_MSG_MIJA;
    } else {
        return 0;
    }

    p += 4;
    uint16_t i = 0;
    while (p < payload_end && *p && *p != '&' && *p != '\r' && *p != '\n' && i < sizeof(s_cmd) - 1) {
        s_cmd[i++] = *p++;
    }
    s_cmd[i] = '\0';

    if (p < payload_end && (*p == '&' || *p == '\r' || *p == '\n')) {
        ended_by_delim = 1U;
    }

    /* 必须读到分隔符/行尾，或确认 +IPD 声明的整包已经收齐，才算完整命令。 */
    if (!ended_by_delim && !(have_ipd && p >= payload_end)) {
        s_cmd[0] = '\0';
        s_cmd_source = ESP_MSG_NONE;
        return 0;
    }
    if (i == 0) {
        s_cmd_source = ESP_MSG_NONE;
        return 0;
    }

    url_decode_inplace(s_cmd);
    trim_inplace(s_cmd);
    if (s_cmd[0] == '\0') {
        s_cmd_source = ESP_MSG_NONE;
        return 0;
    }

    rx_flush();                   /* 处理完清掉，等下一条 */
    s_cmd_ready = 1;
    return 1;
}

void esp_at_init(void)
{
    esp_rst_gpio_init();          /* PB0 开漏，默认释放(外部 10k 上拉) */

    s_rxlen     = 0;
    s_rx[0]     = '\0';
    s_cmd_ready = 0;
    s_cmd_source = ESP_MSG_NONE;
    s_fail_cnt  = 0;
    s_last_err  = ESP_ERR_NONE;
    s_at_tries  = 0;
    s_wifi_tries = 0;
    s_t_mark    = now_ms();
    s_ping_ms   = now_ms();
    s_health_ms = now_ms();
    s_hwreset_ms = 0U;
    s_hwreset_seen = 0U;
    strcpy(s_ip, "0.0.0.0");

    /* 上电不再无条件硬复位 ESP(那要白等 2.7s)：先进 RESET 用 AT 探测，
     * ESP 活着就直接沿用它的状态往下走(自动重连的 WiFi 也能复用)；
     * 探测 ESP_AT_PROBE_TRIES 次都没回应，才进 HWRESET 硬复位。 */
    s_state   = ESP_ST_RESET;
    s_substep = 0;
}

/* 到达超时？(now - mark >= ms) */
static uint8_t timeout(uint32_t ms)
{
    return (uint8_t)((now_ms() - s_t_mark) >= ms);
}
static void mark_time(void)
{
    s_t_mark = now_ms();
}

/* 统一的"进入 ERROR"：记录卡在哪一环、累加连续失败次数。
 * ERROR 状态在 NET_RETRY_MS 后决定是软重试(回 RESET)还是硬复位(进 HWRESET)。 */
static void go_error(esp_err_t err)
{
    s_last_err = err;
    if (s_fail_cnt < 0xFFU) {
        s_fail_cnt++;
    }
    s_state   = ESP_ST_ERROR;
    s_substep = 0;
    mark_time();
}

/* ---- 联网状态机 ----
 * 每个 case 先在需要时发指令(进入子步骤)，然后靠 rx_pump + rx_has 判断
 * 应答，推进到下一步；超时则回 ERROR，由 ERROR 定时重启整个流程。 */
static void run_state_machine(void)
{
    rx_pump();
    parse_ip();

    switch (s_state) {

    case ESP_ST_RESET:
        /* 步骤：AT 探测(最多 ESP_AT_PROBE_TRIES 次) -> AT+CWMODE=1
         * -> AT+CWAUTOCONN=1 -> (进入连 WiFi)。
         * 探测有响应说明 ESP 活着，跳过硬复位直接往下走。 */
        if (s_substep == 0) {
            s_at_tries = 1;
            at_send("AT");
            mark_time();
            s_substep = 1;
        } else if (s_substep == 1) {
            if (rx_has("OK")) {
                at_send("AT+CWMODE=1");
                mark_time();
                s_substep = 2;
            } else if (timeout(ESP_AT_PROBE_MS)) {
                if (s_at_tries < ESP_AT_PROBE_TRIES) {
                    s_at_tries++;
                    at_send("AT");
                    mark_time();
                } else if (!s_hwreset_seen ||
                           (now_ms() - s_hwreset_ms) >= ESP_HWRESET_COOLDOWN_MS) {
                    /* ESP 对 AT 毫无反应：直接硬复位，不绕 ERROR 多等 5s */
                    s_state   = ESP_ST_HWRESET;
                    s_substep = 0;
                    mark_time();
                } else {
                    go_error(ESP_ERR_AT);  /* 硬复位冷却中，先报错软重试 */
                }
            }
        } else if (s_substep == 2) {
            if (rx_has("OK")) {
                /* 确保自动重连开启(存 ESP Flash)，这是跳过 CWJAP 的前提 */
                at_send("AT+CWAUTOCONN=1");
                mark_time();
                s_substep = 3;
            } else if (timeout(AT_CMD_TIMEOUT_MS)) {
                go_error(ESP_ERR_AT);
            }
        } else if (s_substep == 3) {
            /* 就算固件不认识这条指令(回 ERROR)也照常继续 */
            if (rx_has("OK") || rx_has("ERROR") || timeout(AT_CMD_TIMEOUT_MS)) {
                s_state = ESP_ST_WIFI; s_substep = 0; mark_time();
            }
        }
        break;

    case ESP_ST_WIFI: {
        /* 先查 AT+CWJAP? 是否已连上(ESP 上电会自动重连存储的路由；
         * STM32 单独重启时 WiFi 本来就没断)。已连上就跳过 CWJAP，
         * 省 4~8 秒重新关联，也避开关联时的大电流峰值。
         * 连续 ESP_WIFI_QUERY_TRIES 轮都没连上，才主动 CWJAP。 */
        if (s_substep == 0) {
            s_wifi_tries = 0;
            at_send("AT+CWJAP?");
            mark_time();
            s_substep = 1;
        } else if (s_substep == 1) {
            /* 等应答收完整(见到终止的 OK)再判断，防止半截应答误判、
             * 也防止残留尾巴泄漏到下一步 */
            if (rx_has(ESP_CWJAP_MATCH) && rx_has("OK")) {
                s_state = ESP_ST_WIFI_OK; s_substep = 0; mark_time();
            } else if (rx_has("WIFI GOT IP")) {
                s_state = ESP_ST_WIFI_OK; s_substep = 0; mark_time();
            } else if (rx_has("OK") || timeout(AT_CMD_TIMEOUT_MS)) {
                /* 应答完整但没连上(或连的不是本项目路由)：等一会再查 */
                s_wifi_tries++;
                if (s_wifi_tries >= ESP_WIFI_QUERY_TRIES) {
                    s_substep = 3;         /* 自动重连指望不上，主动连 */
                } else {
                    mark_time();
                    s_substep = 2;
                }
            }
        } else if (s_substep == 2) {
            /* 等待窗口：自动重连成功会异步打印 WIFI GOT IP */
            if (rx_has("WIFI GOT IP")) {
                s_state = ESP_ST_WIFI_OK; s_substep = 0; mark_time();
            } else if (timeout(ESP_WIFI_QUERY_MS)) {
                at_send("AT+CWJAP?");
                mark_time();
                s_substep = 1;
            }
        } else if (s_substep == 3) {
            /* 连路由：AT+CWJAP="ssid","pass"  这条较慢，给 15 秒 */
            char cmd[96];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
                     WIFI_SSID, WIFI_PASS);
            at_send(cmd);
            mark_time();
            s_substep = 4;
        } else {
            if (rx_has("WIFI GOT IP") || rx_has("OK")) {
                s_state = ESP_ST_WIFI_OK; s_substep = 0; mark_time();
            } else if (rx_has("FAIL") || rx_has("ERROR")) {
                go_error(ESP_ERR_WIFI);
            } else if (timeout(15000U)) {
                go_error(ESP_ERR_WIFI);
            }
        }
        break;
    }

    case ESP_ST_WIFI_OK:
        /* 查 IP：AT+CIFSR。刚关联上时 DHCP 可能还没给 IP，查到
         * 0.0.0.0 就隔 700ms 重查(也修掉 OLED 一直显示 IP 0.0.0.0
         * 的问题)；多轮仍拿不到 IP 不致命，照样去试 TCP。 */
        if (s_substep == 0) {
            s_wifi_tries = 0;
            at_send("AT+CIFSR");
            mark_time();
            s_substep = 1;
        } else if (s_substep == 1) {
            /* 等整包应答(终止 OK)到齐再走，别让 CIFSR 的尾巴泄漏给 TCP 步骤 */
            if (parse_ip() && rx_has("OK")) {
                s_state = ESP_ST_TCP; s_substep = 0; mark_time();
            } else if (rx_has("OK") || timeout(AT_CMD_TIMEOUT_MS)) {
                s_wifi_tries++;
                if (s_wifi_tries >= 7U) {
                    s_state = ESP_ST_TCP; s_substep = 0; mark_time();
                } else {
                    mark_time();
                    s_substep = 2;
                }
            }
        } else if (s_substep == 2) {
            if (timeout(700U)) {
                at_send("AT+CIFSR");
                mark_time();
                s_substep = 1;
            }
        }
        break;

    case ESP_ST_TCP:
        /* 先关掉可能残留的旧连接，再单连接模式 + 连巴法云 + 订阅主题。
         * 不 CIPCLOSE 的话，订阅超时重试时旧 TCP 还在，CIPSTART 会一直
         * 回 ALREADY CONNECTED + ERROR，3 次后被迫硬复位整轮重来。 */
        if (s_substep == 0) {
            at_send("AT+CIPCLOSE");
            mark_time();
            s_substep = 1;
        } else if (s_substep == 1) {
            /* 没有连接时回 ERROR，属正常。必须等终止应答(OK/ERROR)：
             * 若只看到 CLOSED 就前进，它后面还跟着一个 OK 留在半路，
             * 会被 CIPMUX/CIPSTART 接力误当成自己的应答，引发抢跑、
             * 订阅超时、Net:ERR 反复一两轮才上线。 */
            if (rx_has("OK") || rx_has("ERROR") ||
                timeout(ESP_TCP_CLOSE_MS)) {
                at_send("AT+CIPMUX=0");
                mark_time();
                s_substep = 2;
            }
        } else if (s_substep == 2) {
            if (rx_has("OK") || rx_has("ERROR")) {   /* 已是单连接会回 ERROR，忽略 */
                char cmd[80];
                snprintf(cmd, sizeof(cmd),
                         "AT+CIPSTART=\"TCP\",\"%s\",%s",
                         BEMFA_HOST, BEMFA_PORT);
                at_send(cmd);
                mark_time();
                s_substep = 3;
            } else if (timeout(AT_CMD_TIMEOUT_MS)) {
                go_error(ESP_ERR_TCP);
            }
        } else if (s_substep == 3) {
            /* 成功应答是 "CONNECT" + 终止 "OK"，两个都要在场才算连上；
             * 只见 OK 可能是上一步泄漏的尾巴，不能当真 */
            if (rx_has("CONNECT") && rx_has("OK")) {
                /* 准备发订阅字符串：先算长度再 AT+CIPSEND=len */
                s_substep = 4;
                mark_time();
            } else if (rx_has("ERROR") || rx_has("CLOSED")) {
                go_error(ESP_ERR_TCP);
            } else if (timeout(8000U)) {
                go_error(ESP_ERR_TCP);
            }
        } else if (s_substep == 4) {
            /* 一条 TCP 连接同时订阅自有 App 与米家空调主题。 */
            char body[128];
            char cmd[32];
            int  len;
            snprintf(body, sizeof(body),
                     "cmd=1&uid=%s&topic=%s,%s\r\n",
                     BEMFA_UID, BEMFA_TOPIC, BEMFA_MI_TOPIC);
            len = (int)strlen(body);
            snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);
            at_send(cmd);
            /* 等 ">" 提示符再发数据 */
            s_substep = 5;
            mark_time();
        } else if (s_substep == 5) {
            if (rx_has(">")) {
                char body[128];
                snprintf(body, sizeof(body),
                         "cmd=1&uid=%s&topic=%s,%s\r\n",
                         BEMFA_UID, BEMFA_TOPIC, BEMFA_MI_TOPIC);
                rx_flush();
                at_send_raw(body);
                s_substep = 6;
                mark_time();
            } else if (rx_has("ERROR") || rx_has("link is not")) {
                go_error(ESP_ERR_LOGIN);   /* 连接没就绪，别干等 5 秒 */
            } else if (timeout(BEMFA_SUB_TIMEOUT_MS)) {
                go_error(ESP_ERR_LOGIN);
            }
        } else if (s_substep == 6) {
            if (rx_has("SEND OK") || rx_has("cmd=1") || rx_has("res=1")) {
                rx_flush();
                s_state    = ESP_ST_ONLINE;
                s_substep  = 0;
                s_ping_ms  = now_ms();
                s_health_ms = now_ms();
                s_fail_cnt = 0;               /* 成功上线：清零失败计数 */
                s_last_err = ESP_ERR_NONE;
                mark_time();
            } else if (rx_has("SEND FAIL") || rx_has("ERROR")) {
                go_error(ESP_ERR_LOGIN);
            } else if (timeout(BEMFA_SUB_TIMEOUT_MS)) {
                go_error(ESP_ERR_LOGIN);
            }
        }
        break;

    case ESP_ST_ONLINE: {
        uint32_t now = now_ms();

        /* 下行命令本身证明 ESP/WiFi/TCP/云端链路有效。若它与健康查询交错，
         * 优先交付命令并取消本次查询，下一周期再检查。 */
        if (parse_downlink()) {
            s_substep  = ONLINE_OP_IDLE;
            s_health_ms = now;
            s_ping_ms   = now;
            break;
        }

        if (rx_has("WIFI DISCONNECT")) {
            go_error(ESP_ERR_WIFI);
            break;
        }
        if (rx_has("CLOSED") || rx_has("link is not valid")) {
            go_error(ESP_ERR_TCP);
            break;
        }

        switch (s_substep) {
        case ONLINE_OP_IDLE:
            if (s_cmd_ready || has_incomplete_ipd()) {
                break;
            }

            /* 心跳优先；成功的 SEND OK 同时也可作为本轮健康确认。 */
            if ((now - s_ping_ms) >= BEMFA_PING_MS) {
                rx_local_clear();
                esp_send_string("AT+CIPSEND=6\r\n");
                mark_time();
                s_substep = ONLINE_OP_PING_PROMPT;
            } else if ((now - s_health_ms) >= ESP_HEALTH_CHECK_MS) {
                rx_local_clear();
                esp_send_string("AT+CIPSTATUS\r\n");
                mark_time();
                s_substep = ONLINE_OP_HEALTH_WAIT;
            }
            break;

        case ONLINE_OP_HEALTH_WAIT:
            /* ESP8266 AT 状态：3=TCP 已连接，2/4=有 WiFi 但 TCP 未连接，
             * 5=WiFi 已断开。无任何应答则按 ESP 无响应处理。 */
            if (has_incomplete_ipd()) {
                if (timeout(ESP_HEALTH_TIMEOUT_MS)) {
                    go_error(ESP_ERR_TCP);
                }
            } else if (rx_has("STATUS:3")) {
                rx_local_clear();
                s_health_ms = now;
                s_substep = ONLINE_OP_IDLE;
            } else if (rx_has("STATUS:5")) {
                go_error(ESP_ERR_WIFI);
            } else if (rx_has("STATUS:2") || rx_has("STATUS:4")) {
                go_error(ESP_ERR_TCP);
            } else if (rx_has("ERROR") || timeout(ESP_HEALTH_TIMEOUT_MS)) {
                go_error(ESP_ERR_AT);
            }
            break;

        case ONLINE_OP_PING_PROMPT:
            if (has_incomplete_ipd()) {
                if (timeout(ESP_HEALTH_TIMEOUT_MS)) {
                    go_error(ESP_ERR_TCP);
                }
            } else if (rx_has(">")) {
                rx_local_clear();
                at_send_raw("ping\r\n");
                mark_time();
                s_substep = ONLINE_OP_PING_SEND_OK;
            } else if (rx_has("ERROR") || rx_has("busy") ||
                       timeout(ESP_HEALTH_TIMEOUT_MS)) {
                go_error(ESP_ERR_TCP);
            }
            break;

        case ONLINE_OP_PING_SEND_OK:
            if (has_incomplete_ipd()) {
                if (timeout(AT_CMD_TIMEOUT_MS)) {
                    go_error(ESP_ERR_TCP);
                }
            } else if (rx_has("SEND OK")) {
                rx_local_clear();
                s_ping_ms = now;
                s_health_ms = now;
                s_substep = ONLINE_OP_IDLE;
            } else if (rx_has("SEND FAIL") || rx_has("ERROR") ||
                       timeout(AT_CMD_TIMEOUT_MS)) {
                go_error(ESP_ERR_TCP);
            }
            break;

        default:
            s_substep = ONLINE_OP_IDLE;
            break;
        }
        break;
    }

    case ESP_ST_HWRESET:
        /* 非阻塞硬复位：拉低 RST 脉冲 -> 释放 -> 等 ESP 启动 -> 回 RESET。
         * 全程主循环照常喂狗(看门狗 4s > 本状态最长 ~2.7s)，不会自复位。 */
        if (s_substep == 0) {
            esp_rst_assert();          /* PB0 拉低，ESP 进入复位 */
            /* 现在硬复位只由"AT 探测无响应"或"连续失败达上限"触发，
             * 统一记录时刻，供 ESP_HWRESET_COOLDOWN_MS 冷却限频。 */
            s_hwreset_ms = now_ms();
            s_hwreset_seen = 1U;
            strcpy(s_ip, "0.0.0.0");
            mark_time();
            s_substep = 1;
        } else if (s_substep == 1) {
            if (timeout(ESP_RST_LOW_MS)) {
                esp_rst_release();     /* 释放，ESP 开始启动 */
                mark_time();
                s_substep = 2;
            }
        } else if (s_substep == 2) {
            if (timeout(ESP_RST_BOOT_MS)) {
                s_fail_cnt = 0;        /* 硬复位已执行，失败计数清零重新计 */
                rx_flush();            /* 丢掉 ESP 启动打印(ready 等) */
                s_state   = ESP_ST_RESET;
                s_substep = 0;
                mark_time();
            }
        }
        break;

    case ESP_ST_ERROR:
    default:
        /* 隔 NET_RETRY_MS 后再动作：连续失败达到上限就硬复位 ESP，
         * 否则只做软重试(回 RESET 重发 AT 流程)。 */
        if (timeout(NET_RETRY_MS)) {
            if (s_fail_cnt >= ESP_FAIL_LIMIT &&
                (!s_hwreset_seen ||
                 (now_ms() - s_hwreset_ms) >= ESP_HWRESET_COOLDOWN_MS)) {
                s_state   = ESP_ST_HWRESET;   /* 软重试无效，硬复位 ESP */
                s_substep = 0;
                mark_time();
            } else {
                /* 分层恢复：ESP 活着时只重连失败的那一层；AT 无响应才从
                 * 基础 AT 检测重新开始。硬复位冷却期内也继续软重试。 */
                if (s_last_err == ESP_ERR_TCP || s_last_err == ESP_ERR_LOGIN) {
                    s_state = ESP_ST_TCP;
                } else if (s_last_err == ESP_ERR_WIFI) {
                    strcpy(s_ip, "0.0.0.0");
                    s_state = ESP_ST_WIFI;
                } else {
                    strcpy(s_ip, "0.0.0.0");
                    s_state = ESP_ST_RESET;
                }
                s_substep = 0;
                rx_flush();
                mark_time();
            }
        }
        break;
    }
}

void esp_at_task(void)
{
    run_state_machine();
}

uint8_t esp_at_take_message(esp_msg_source_t *source,
                            char *out, uint16_t max_len)
{
    if (!s_cmd_ready || source == 0 || out == 0 || max_len == 0) {
        return 0;
    }
    uint16_t n = (uint16_t)strlen(s_cmd);
    if (n > max_len - 1) {
        n = max_len - 1;
    }
    memcpy(out, s_cmd, n);
    out[n]      = '\0';
    *source     = s_cmd_source;
    s_cmd_ready = 0;
    s_cmd_source = ESP_MSG_NONE;
    return 1;
}

static uint8_t publish_topic_up(const char *topic, const char *msg)
{
    char body[192];
    char cmd[32];
    int  len;
    uint32_t t0;
    uint8_t got_prompt = 0;

    if (s_state != ESP_ST_ONLINE || topic == 0 || msg == 0) {
        return 0;
    }
    /* 用 /up 只更新云端、不回推给自己，避免自触发 */
    len = snprintf(body, sizeof(body),
                   "cmd=2&uid=%s&topic=%s/up&msg=%s\r\n",
                   BEMFA_UID, topic, msg);
    if (len <= 0 || len >= (int)sizeof(body)) {
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", len);

    rx_local_clear();
    esp_send_string(cmd);
    esp_send_string("\r\n");

    t0 = now_ms();
    while ((now_ms() - t0) < 250U) {
        rx_pump();
        if (rx_has(">")) {
            got_prompt = 1U;
            break;
        }
    }

    /* ESP normally accepts the payload shortly after CIPSEND even if the prompt
     * was not pumped in time. Try once, but keep the RX ring intact either way. */
    (void)got_prompt;
    at_send_raw(body);

    t0 = now_ms();
    while ((now_ms() - t0) < 50U) {
        rx_pump();
    }
    return 1;
}

uint8_t esp_at_publish(const char *msg)
{
    return publish_topic_up(BEMFA_TOPIC, msg);
}

uint8_t esp_at_publish_mijia(const char *msg)
{
    return publish_topic_up(BEMFA_MI_TOPIC, msg);
}

esp_state_t esp_at_state(void)
{
    return s_state;
}

const char *esp_at_ip(void)
{
    return s_ip;
}

esp_err_t esp_at_last_err(void)
{
    return s_last_err;
}
