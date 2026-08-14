#include "bsp_ir_rx.h"
#include "bsp_delay.h"

/* 帧间空闲判定：一帧红外发完后会静默较长时间，
 * 这里取 15ms 无边沿即认为一帧结束（NEC/空调帧内部间隔远小于此）。 */
#define IR_RX_IDLE_US       15000U

static volatile uint16_t s_buf[IR_RX_MAX_EDGES];  /* 各段时长(us) */
static volatile uint16_t s_cnt      = 0;          /* 已记录段数 */
static volatile uint32_t s_last_cyc = 0;          /* 上一边沿的周期戳 */
static volatile uint8_t  s_active   = 0;          /* 正在接收一帧 */
static volatile uint8_t  s_ready    = 0;          /* 一帧就绪待取 */

void ir_rx_init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    /* PA0 输入。1838B 模块输出已是推挽，用上拉输入更稳妥 */
    gpio.GPIO_Pin  = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);

    /* 双边沿触发：mark 和 space 都要计时 */
    exti.EXTI_Line    = EXTI_Line0;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel                   = EXTI0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;   /* 比串口高一点，保证时序 */
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    ir_rx_reset();
}

void ir_rx_enable(void)
{
    EXTI->IMR |= EXTI_Line0;
}

void ir_rx_disable(void)
{
    EXTI->IMR &= ~EXTI_Line0;
}

void ir_rx_reset(void)
{
    s_cnt    = 0;
    s_active = 0;
    s_ready  = 0;
}

/* EXTI0 中断：每个边沿打时间戳并算出上一段时长 */
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        uint32_t now = DWT->CYCCNT;

        if (s_ready) {
            /* 上一帧还没被取走，忽略新边沿，防止覆盖 */
            EXTI_ClearITPendingBit(EXTI_Line0);
            return;
        }

        if (!s_active) {
            /* 本帧第一个边沿：只记起点，不产生时长 */
            s_active = 1;
            s_cnt    = 0;
        } else {
            uint32_t us = (now - s_last_cyc) / (SystemCoreClock / 1000000U);
            if (us > 65535U) {
                us = 65535U;
            }
            if (s_cnt < IR_RX_MAX_EDGES) {
                s_buf[s_cnt++] = (uint16_t)us;
            }
        }
        s_last_cyc = now;

        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

void ir_rx_poll(void)
{
    if (s_active && !s_ready && s_cnt > 0) {
        uint32_t idle = (DWT->CYCCNT - s_last_cyc) / (SystemCoreClock / 1000000U);
        if (idle > IR_RX_IDLE_US) {
            s_active = 0;
            s_ready  = 1;         /* 一帧结束 */
        }
    }
}

uint8_t ir_rx_ready(void)
{
    return s_ready;
}

const uint16_t *ir_rx_get(uint16_t *len)
{
    if (len) {
        *len = s_cnt;
    }
    return (const uint16_t *)s_buf;
}
