#include "bsp_ir_tx.h"
#include "bsp_delay.h"
#include "bsp_ir_rx.h"      /* 发送时临时关闭接收 */

/* TIM3 时钟 = 72MHz。载波 38kHz：
 *   周期计数 = 72e6 / 38e3 ≈ 1895  -> ARR = 1894, PSC = 0
 *   实际频率 = 72e6 / 1895 ≈ 37.99kHz
 * 占空比取 1/2（约 50%）：接收头解调只看包络，1/3~1/2 都兼容，
 * 取 1/2 时同样的峰值电流下平均辐射功率比 1/3 高 50%，射得更远。
 * LED 平均电流 = 峰值×占空比×码型占空(约一半)，100mA 峰值下依然安全。 */
#define IR_TIM              TIM3
#define IR_TIM_ARR          1894
#define IR_TIM_DUTY         947        /* mark 时的 CCR 值(1894/2) */

void ir_tx_init(void)
{
    GPIO_InitTypeDef       gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef      oc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* PA6 = TIM3_CH1，复用推挽输出 */
    gpio.GPIO_Pin   = GPIO_Pin_6;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* 时基 */
    tim.TIM_Prescaler     = 0;
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_Period        = IR_TIM_ARR;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(IR_TIM, &tim);

    /* 通道 1 PWM 模式：CCR=0 时输出恒低（无载波），CCR=DUTY 时发载波 */
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = 0;                    /* 上电默认关载波 */
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(IR_TIM, &oc);
    TIM_OC1PreloadConfig(IR_TIM, TIM_OCPreload_Enable);

    TIM_Cmd(IR_TIM, ENABLE);
}

void ir_tx_carrier(uint8_t on)
{
    TIM_SetCompare1(IR_TIM, on ? IR_TIM_DUTY : 0);
}

void ir_tx_send(const uint16_t *data, uint16_t len)
{
    if (data == 0 || len == 0) {
        return;
    }

    ir_rx_disable();                 /* 关掉接收，避免发射被自己收到 */

    for (uint16_t i = 0; i < len; i++) {
        ir_tx_carrier((i & 1) ? 0 : 1);   /* 偶数段 mark，奇数段 space */
        delay_us(data[i]);
    }

    ir_tx_carrier(0);                /* 收尾确保关灯 */

    ir_rx_reset();                   /* 清掉发送期间的残留状态 */
    ir_rx_enable();
}
