#include "cmd_parser.h"
#include "bsp_led.h"
#include "bsp_ir_tx.h"
#include "bsp_ir_rx.h"
#include "bsp_esp_at.h"
#include "bsp_oled.h"
#include "bsp_delay.h"
#include "ir_codes.h"
#include "midea_ac.h"
#include <string.h>
#include <stdio.h>

#define SLOT_POWER_ON       0
#define SLOT_POWER_OFF      1
#define SLOT_COOL_26        2
#define SLOT_COOL_25        3
#define SLOT_HEAT_28        4
#define SLOT_ECO            5
#define MI_FIELD_COUNT      5

static int8_t s_learn_slot = -1;

static uint16_t s_last_frame[IR_FRAME_MAX];
static uint16_t s_last_len = 0;
static const char *s_mijia_state = 0;

static void reply(const char *s)
{
    char    line[20];
    uint8_t i = 0;

    printf("%s", s);

    while (s[i] && s[i] != '\r' && s[i] != '\n' && i < sizeof(line) - 1) {
        line[i] = s[i];
        i++;
    }
    line[i] = '\0';
    OLED_ClearLine(4);
    OLED_ShowString(4, 1, line);
}

static void reply_mqtt(const char *s)
{
    esp_at_publish(s);
}

static const char *esp_state_text(esp_state_t st)
{
    switch (st) {
    case ESP_ST_RESET:   return "RESET";
    case ESP_ST_HWRESET: return "HWRESET";
    case ESP_ST_WIFI:    return "WIFI";
    case ESP_ST_WIFI_OK: return "WIFI_OK";
    case ESP_ST_TCP:     return "TCP";
    case ESP_ST_ONLINE:  return "ONLINE";
    case ESP_ST_ERROR:   return "ERROR";
    default:             return "UNKNOWN";
    }
}

static uint8_t parse_slot_arg(const char *p, uint8_t *slot)
{
    uint8_t v;

    while (*p == ' ') {
        p++;
    }
    if (*p < '0' || *p > ('0' + IR_SLOT_COUNT - 1)) {
        return 0;
    }

    v = (uint8_t)(*p - '0');
    p++;

    while (*p == ' ') {
        p++;
    }
    if (*p != '\0') {
        return 0;
    }

    *slot = v;
    return 1;
}

static void build_slot_map(char *out, uint8_t out_len)
{
    uint8_t i;

    if (out_len < (IR_SLOT_COUNT + 1U)) {
        if (out_len > 0U) {
            out[0] = '\0';
        }
        return;
    }

    for (i = 0; i < IR_SLOT_COUNT; i++) {
        uint16_t len = 0;
        out[i] = (ir_codes_get(i, &len) != 0) ? (char)('0' + i) : '-';
    }
    out[IR_SLOT_COUNT] = '\0';
}

static void do_status(void)
{
    char slots[IR_SLOT_COUNT + 1U];
    char msg[96];

    build_slot_map(slots, sizeof(slots));
    snprintf(msg, sizeof(msg), "NET %s IP %s SLOTS %s\r\n",
             esp_state_text(esp_at_state()), esp_at_ip(), slots);
    reply(msg);
    reply_mqtt(msg);
}

static void do_save(void)
{
    char msg[40];

    if (ir_codes_save()) {
        snprintf(msg, sizeof(msg), "OK saved %u slots\r\n", ir_codes_count());
    } else {
        snprintf(msg, sizeof(msg), "ERR save failed\r\n");
    }
    reply(msg);
    reply_mqtt(msg);
}

static void do_clear(uint8_t slot)
{
    char msg[48];

    ir_codes_clear(slot);
    if (ir_codes_save()) {
        snprintf(msg, sizeof(msg), "OK cleared slot %u\r\n", slot);
    } else {
        snprintf(msg, sizeof(msg), "ERR clear slot %u save failed\r\n", slot);
    }
    reply(msg);
    reply_mqtt(msg);
}

static const char *mijia_state_for_slot(uint8_t slot)
{
    switch (slot) {
    case SLOT_POWER_ON:  return "on";
    case SLOT_POWER_OFF: return "off";
    case SLOT_COOL_26:   return "on#2#26";
    case SLOT_COOL_25:   return "on#2#23"; /* 槽3当前实际学习的是制冷23度 */
    case SLOT_ECO:       return "on#7";
    default:             return 0;          /* 槽4屏显切换不改变空调状态 */
    }
}

static void sync_mijia_slot(uint8_t slot)
{
    const char *state = mijia_state_for_slot(slot);

    if (state != 0) {
        s_mijia_state = state;
        esp_at_publish_mijia(state);
    }
}

static void restore_mijia_state(void)
{
    if (s_mijia_state != 0) {
        esp_at_publish_mijia(s_mijia_state);
    }
}

static uint8_t do_send_slot(uint8_t slot, const char *name)
{
    uint16_t        len = 0;
    const uint16_t *d   = ir_codes_get(slot, &len);
    char            msg[48];

    if (d == 0) {
        snprintf(msg, sizeof(msg), "ERR %s empty(slot %u)\r\n", name, slot);
        reply(msg);
        reply_mqtt(msg);
        return 0;
    }
    led_on();
    ir_tx_send(d, len);
    led_off();
    snprintf(msg, sizeof(msg), "OK sent %s (%u edges)\r\n", name, len);
    reply(msg);
    reply_mqtt(msg);
    sync_mijia_slot(slot);
    return 1;
}

/* 解析 on#模式#温度#风速#左右扫风#上下扫风，空字段记为 -1。 */
static uint8_t parse_mijia_on(const char *line, int16_t fields[MI_FIELD_COUNT],
                              uint8_t *field_count)
{
    const char *p;
    uint8_t     count = 0;
    uint8_t     i;

    for (i = 0; i < MI_FIELD_COUNT; i++) {
        fields[i] = -1;
    }
    if (line[0] != 'o' || line[1] != 'n') {
        return 0;
    }

    p = line + 2;
    while (*p != '\0') {
        int16_t value = 0;

        if (*p != '#' || count >= MI_FIELD_COUNT) {
            return 0;
        }
        p++;

        if (*p == '#' || *p == '\0') {
            fields[count++] = -1;
            continue;
        }
        if (*p < '0' || *p > '9') {
            return 0;
        }
        while (*p >= '0' && *p <= '9') {
            value = (int16_t)(value * 10 + (*p - '0'));
            if (value > 100) {
                return 0;
            }
            p++;
        }
        if (*p != '\0' && *p != '#') {
            return 0;
        }
        fields[count++] = value;
    }

    *field_count = count;
    return 1;
}

static void mijia_error(void)
{
    reply("ERR MI unsupported\r\n");
    reply_mqtt("ERR MI unsupported\r\n");
    restore_mijia_state();
}

static void dispatch_mijia(const char *line)
{
    int16_t fields[MI_FIELD_COUNT];
    int16_t mode;
    int16_t temp;
    uint8_t count;
    uint8_t i;

    if (!strcmp(line, "off")) {
        if (!do_send_slot(SLOT_POWER_OFF, "MI off")) {
            restore_mijia_state();
        }
        return;
    }
    if (!parse_mijia_on(line, fields, &count)) {
        mijia_error();
        return;
    }
    if (count == 0U) {
        if (!do_send_slot(SLOT_POWER_ON, "MI on")) {
            restore_mijia_state();
        }
        return;
    }

    mode = fields[0];
    temp = (count > 1U) ? fields[1] : -1;

    /* 当前学习码没有独立风速/扫风状态，只接受缺省值或自动/关闭(0)。 */
    for (i = 2U; i < count; i++) {
        if (fields[i] > 0) {
            mijia_error();
            return;
        }
    }

    if (mode == 7 && (temp <= 0 || temp == 26)) {
        if (!do_send_slot(SLOT_ECO, "MI ECO")) {
            restore_mijia_state();
        }
    } else if ((mode == 2 || mode < 0) && (temp <= 0 || temp == 26)) {
        if (!do_send_slot(SLOT_COOL_26, "MI cool26")) {
            restore_mijia_state();
        }
    } else if ((mode == 2 || mode < 0) && temp == 23) {
        if (!do_send_slot(SLOT_COOL_25, "MI cool23")) {
            restore_mijia_state();
        }
    } else {
        mijia_error();
    }
}

static void do_ir_test(void)
{
    uint8_t i;

    ir_rx_disable();
    led_on();
    for (i = 0; i < 3; i++) {
        ir_tx_carrier(1);
        delay_ms(120);
        ir_tx_carrier(0);
        delay_ms(120);
    }
    led_off();
    ir_rx_reset();
    ir_rx_enable();

    reply("OK IR_TEST\r\n");
    reply_mqtt("OK IR_TEST\r\n");
}

static void do_midea(midea_ac_preset_t preset, const char *name)
{
    char msg[48];

    led_on();
    if (midea_ac_send_preset(preset)) {
        snprintf(msg, sizeof(msg), "OK sent %s\r\n", name);
    } else {
        snprintf(msg, sizeof(msg), "ERR %s\r\n", name);
    }
    led_off();

    reply(msg);
    reply_mqtt(msg);
}

static void print_help(void)
{
    reply("Commands:\r\n");
    reply("  LED_ON / LED_OFF / LED_TOGGLE\r\n");
    reply("  LEARN <0-5>   arm learning into slot\r\n");
    reply("  SEND <0-5>    replay a slot\r\n");
    reply("  DUMP          print last captured timings\r\n");
    reply("  STATUS        show net/ip/learned slots\r\n");
    reply("  SAVE          save RAM slots to Flash\r\n");
    reply("  CLEAR <0-5>   clear a slot and save\r\n");
    reply("  IR_TEST       blink IR LED carrier\r\n");
    reply("  MIDEA_COOL26 MIDEA_COOL25 MIDEA_HEAT28 MIDEA_OFF\r\n");
    reply("  MIDEA_F_COOL26 MIDEA_F_COOL25 MIDEA_F_HEAT28 MIDEA_F_OFF\r\n");
    reply("  POWER_ON POWER_OFF COOL_26 COOL_25 HEAT_28\r\n");
}

static void dump_last(void)
{
    char     line[16];
    uint16_t i;

    if (s_last_len == 0) {
        reply("no frame captured\r\n");
        return;
    }
    printf("last frame %u edges (us):\r\n", s_last_len);
    for (i = 0; i < s_last_len; i++) {
        snprintf(line, sizeof(line), "%u%s", s_last_frame[i],
                 (i + 1 < s_last_len) ? "," : "\r\n");
        printf("%s", line);
    }
}

static void dispatch(const char *line)
{
    uint8_t slot = 0;

    if      (!strcmp(line, "LED_ON"))     { led_on();     reply("OK LED_ON\r\n");     }
    else if (!strcmp(line, "LED_OFF"))    { led_off();    reply("OK LED_OFF\r\n");    }
    else if (!strcmp(line, "LED_TOGGLE")) { led_toggle(); reply("OK LED_TOGGLE\r\n"); }
    else if (!strcmp(line, "HELP"))       { print_help(); }
    else if (!strcmp(line, "DUMP"))       { dump_last(); }
    else if (!strcmp(line, "STATUS"))     { do_status(); }
    else if (!strcmp(line, "SAVE"))       { do_save(); }
    else if (!strcmp(line, "POWER_ON"))   { do_send_slot(SLOT_POWER_ON,  "POWER_ON");  }
    else if (!strcmp(line, "POWER_OFF"))  { do_send_slot(SLOT_POWER_OFF, "POWER_OFF"); }
    else if (!strcmp(line, "COOL_26"))    { do_send_slot(SLOT_COOL_26,   "COOL_26");   }
    else if (!strcmp(line, "COOL_25"))    { do_send_slot(SLOT_COOL_25,   "COOL_25");   }
    else if (!strcmp(line, "HEAT_28"))    { do_send_slot(SLOT_HEAT_28,   "HEAT_28");   }
    else if (!strcmp(line, "IR_TEST"))    { do_ir_test(); }
    else if (!strcmp(line, "WDG_TEST")) {
        /* 故意锁死主循环：约 4 秒后 IWDG 应复位系统，
         * 重启后 OLED 第一行显示 WDG! —— 用于验证看门狗在岗 */
        reply("OK WDG_TEST freeze, reboot in ~4s\r\n");
        reply_mqtt("OK WDG_TEST freeze, reboot in ~4s\r\n");
        for (;;) { ; }
    }
    else if (!strcmp(line, "MIDEA_COOL26"))   { do_midea(MIDEA_AC_COOL26,   "MIDEA_COOL26");   }
    else if (!strcmp(line, "MIDEA_COOL25"))   { do_midea(MIDEA_AC_COOL25,   "MIDEA_COOL25");   }
    else if (!strcmp(line, "MIDEA_HEAT28"))   { do_midea(MIDEA_AC_HEAT28,   "MIDEA_HEAT28");   }
    else if (!strcmp(line, "MIDEA_OFF"))      { do_midea(MIDEA_AC_OFF,      "MIDEA_OFF");      }
    else if (!strcmp(line, "MIDEA_F_COOL26")) { do_midea(MIDEA_AC_F_COOL26, "MIDEA_F_COOL26"); }
    else if (!strcmp(line, "MIDEA_F_COOL25")) { do_midea(MIDEA_AC_F_COOL25, "MIDEA_F_COOL25"); }
    else if (!strcmp(line, "MIDEA_F_HEAT28")) { do_midea(MIDEA_AC_F_HEAT28, "MIDEA_F_HEAT28"); }
    else if (!strcmp(line, "MIDEA_F_OFF"))    { do_midea(MIDEA_AC_F_OFF,    "MIDEA_F_OFF");    }
    else if (!strncmp(line, "LEARN", 5)) {
        if (parse_slot_arg(line + 5, &slot)) {
            char msg[40];
            s_learn_slot = (int8_t)slot;
            ir_rx_reset();
            snprintf(msg, sizeof(msg), "OK learning slot %u, press remote\r\n", slot);
            reply(msg);
        } else {
            reply("ERR slot 0-5\r\n");
        }
    }
    else if (!strncmp(line, "SEND", 4)) {
        if (parse_slot_arg(line + 4, &slot)) {
            char name[8];
            snprintf(name, sizeof(name), "slot%u", slot);
            do_send_slot(slot, name);
        } else {
            reply("ERR slot 0-5\r\n");
        }
    }
    else if (!strncmp(line, "CLEAR", 5)) {
        if (parse_slot_arg(line + 5, &slot)) {
            do_clear(slot);
        } else {
            reply("ERR slot 0-5\r\n");
        }
    }
    else {
        char msg[40];
        snprintf(msg, sizeof(msg), "ERR cmd %.18s\r\n", line);
        reply(msg);
        reply_mqtt(msg);
    }
}

static uint8_t trim_char(char c)
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '"' || c == '\'') ? 1U : 0U;
}

static void normalize_cmd(const char *in, char *out, uint16_t out_len)
{
    uint16_t n = 0;
    uint16_t len;

    if (out_len == 0U) {
        return;
    }

    while (*in && trim_char(*in)) {
        in++;
    }

    while (*in && n < out_len - 1U) {
        out[n++] = *in++;
    }
    out[n] = '\0';

    len = (uint16_t)strlen(out);
    while (len > 0U && trim_char(out[len - 1U])) {
        out[--len] = '\0';
    }
}

void cmd_parser_init(void)
{
    s_learn_slot = -1;
    s_last_len   = 0;
    s_mijia_state = 0;
}

void cmd_parser_exec(const char *line)
{
    char clean[64];

    normalize_cmd(line, clean, sizeof(clean));
    dispatch(clean);
}

void cmd_parser_exec_mijia(const char *line)
{
    char     clean[64];
    uint16_t i;

    normalize_cmd(line, clean, sizeof(clean));
    for (i = 0; clean[i] != '\0'; i++) {
        if (clean[i] >= 'A' && clean[i] <= 'Z') {
            clean[i] = (char)(clean[i] - 'A' + 'a');
        }
    }
    dispatch_mijia(clean);
}

void cmd_parser_service_ir(void)
{
    uint16_t        len = 0;
    const uint16_t *d;
    uint16_t        i;

    ir_rx_poll();
    if (!ir_rx_ready()) {
        return;
    }

    d = ir_rx_get(&len);

    s_last_len = (len > IR_FRAME_MAX) ? IR_FRAME_MAX : len;
    for (i = 0; i < s_last_len; i++) {
        s_last_frame[i] = d[i];
    }

    if (s_learn_slot >= 0) {
        char msg[56];
        if (ir_codes_store((uint8_t)s_learn_slot, d, len)) {
            if (ir_codes_save()) {
                snprintf(msg, sizeof(msg), "OK learned slot %d, %u edges (saved)\r\n",
                         s_learn_slot, len);
            } else {
                snprintf(msg, sizeof(msg), "WARN learned slot %d, %u edges save FAIL\r\n",
                         s_learn_slot, len);
            }
        } else {
            snprintf(msg, sizeof(msg), "ERR store slot %d failed\r\n", s_learn_slot);
        }
        reply(msg);
        reply_mqtt(msg);
        s_learn_slot = -1;
    }

    ir_rx_reset();
}
