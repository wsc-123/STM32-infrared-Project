#include "bsp_usart.h"
#include <stdio.h>

/* ---------------- USART1：ESP-01S ---------------- */
#define ESP_USARTx          USART1
#define ESP_BAUD            115200
/* PA9  = USART1_TX -> ESP-01S RX
 * PA10 = USART1_RX <- ESP-01S TX */

/* ---------------- USART2：调试串口 ---------------- */
#define DBG_USARTx          USART2
#define DBG_BAUD            115200
/* PA2 = USART2_TX -> USB-TTL RX
 * PA3 = USART2_RX <- USB-TTL TX */

/* ---------------- USART1 接收环形缓冲 ---------------- */
#define RX_RING_SIZE        256                 /* 必须为 2 的幂 */
#define RX_RING_MASK        (RX_RING_SIZE - 1)

static volatile uint8_t  s_ring[RX_RING_SIZE];
static volatile uint16_t s_head = 0;            /* 中断写入位置 */
static volatile uint16_t s_tail = 0;            /* 主循环读取位置 */

/* 跨多次调用累积一行的组包缓冲 */
static char     s_line[128];
static uint16_t s_line_len = 0;

/* ---------------- 底层发送 ---------------- */
static void usart_send_char(USART_TypeDef *u, uint8_t c)
{
    while (USART_GetFlagStatus(u, USART_FLAG_TXE) == RESET) {
        ;
    }
    USART_SendData(u, c);
}

void esp_send_string(const char *s)
{
    while (*s) {
        usart_send_char(ESP_USARTx, (uint8_t)*s++);
    }
}

/* printf 重定向到调试串口 USART2（Keil 需勾选 Use MicroLIB） */
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_send_char(DBG_USARTx, (uint8_t)ch);
    return ch;
}

/* ---------------- 初始化 ---------------- */
void usart_init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;

    /* 时钟：GPIOA + AFIO 在 APB2；USART1 在 APB2；USART2 在 APB1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO   |
                           RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* --- USART1 引脚：PA9 复用推挽输出，PA10 浮空输入 --- */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* --- USART2 引脚：PA2 复用推挽输出，PA3 浮空输入 --- */
    gpio.GPIO_Pin   = GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin  = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* --- USART1 参数 --- */
    usart.USART_BaudRate            = ESP_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ESP_USARTx, &usart);

    /* --- USART2 参数（同上，波特率不同）--- */
    usart.USART_BaudRate = DBG_BAUD;
    USART_Init(DBG_USARTx, &usart);

    /* USART1 接收中断（ESP 命令异步到达）*/
    USART_ITConfig(ESP_USARTx, USART_IT_RXNE, ENABLE);
    nvic.NVIC_IRQChannel                   = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(ESP_USARTx, ENABLE);
    USART_Cmd(DBG_USARTx, ENABLE);
}

/* ---------------- USART1 接收中断服务 ----------------
 * 只做一件事：把收到的字节塞进环形缓冲，尽量短。 */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(ESP_USARTx, USART_IT_RXNE) != RESET) {
        uint8_t  c    = (uint8_t)USART_ReceiveData(ESP_USARTx);
        uint16_t next = (s_head + 1) & RX_RING_MASK;
        if (next != s_tail) {          /* 未满才写，满了丢弃防覆盖 */
            s_ring[s_head] = c;
            s_head         = next;
        }
        /* 读 DR 已自动清 RXNE，无需手动清 */
    }
}

/* ---------------- 整行读取 ---------------- */
static uint8_t ring_pop(uint8_t *out)
{
    if (s_tail == s_head) {
        return 0;                      /* 空 */
    }
    *out   = s_ring[s_tail];
    s_tail = (s_tail + 1) & RX_RING_MASK;
    return 1;
}

/* 从环形缓冲取一个字节（供 ESP-AT 驱动逐字节解析 AT 应答 / +IPD）。
 * 取到返回 1，缓冲空返回 0。 */
uint8_t esp_rx_byte(uint8_t *out)
{
    return ring_pop(out);
}

/* 丢弃环形缓冲里所有未读字节，并复位整行组包状态。
 * ESP-AT 驱动在发下一条 AT 指令前调用，避免上一条的残留应答干扰匹配。 */
void esp_rx_clear(void)
{
    s_tail     = s_head;
    s_line_len = 0;
}

uint8_t esp_read_line(char *out, uint16_t max_len)
{
    uint8_t c;

    while (ring_pop(&c)) {
        if (c == '\n' || c == '\r') {
            if (s_line_len == 0) {
                continue;              /* 跳过空行 / CRLF 的第二个字符 */
            }
            /* 拷贝到调用者缓冲 */
            uint16_t n = s_line_len;
            if (n > (uint16_t)(max_len - 1)) {
                n = max_len - 1;
            }
            for (uint16_t i = 0; i < n; i++) {
                out[i] = s_line[i];
            }
            out[n]     = '\0';
            s_line_len = 0;
            return 1;
        } else {
            if (s_line_len < sizeof(s_line) - 1) {
                s_line[s_line_len++] = (char)c;
            }
            /* 超长则丢弃多余字符，等换行 */
        }
    }
    return 0;
}
