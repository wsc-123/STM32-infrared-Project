#include "bsp_delay.h"

/* DWT（Data Watchpoint and Trace）在 Cortex-M3 上提供一个 32 位、
 * 以 CPU 主频递增的周期计数器 CYCCNT。72MHz 下 1 微秒 = 72 个周期，
 * 32 位计数器约 59.6 秒才溢出一次，做红外微秒计时绰绰有余。 */

#define DEMCR_TRCENA_BIT     (1UL << 24)  /* CoreDebug->DEMCR 里的 TRCENA */
#define DWT_CTRL_CYCCNTENA   (1UL << 0)   /* DWT->CTRL 里的 CYCCNTENA */

static volatile uint32_t s_ms = 0;

void bsp_delay_init(void)
{
    CoreDebug->DEMCR |= DEMCR_TRCENA_BIT;   /* 打开跟踪单元总开关 */
    DWT->CYCCNT       = 0;                   /* 计数清零 */
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA;  /* 启动周期计数器 */

    s_ms = 0;
    if (SysTick_Config(SystemCoreClock / 1000U) != 0U) {
        while (1) {
            ;
        }
    }
}

uint32_t bsp_get_cycle(void)
{
    return DWT->CYCCNT;
}

uint32_t bsp_cycles_per_us(void)
{
    return SystemCoreClock / 1000000U;
}

uint32_t bsp_millis(void)
{
    return s_ms;
}

void SysTick_Handler(void)
{
    s_ms++;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    /* 无符号相减天然处理计数器溢出回绕 */
    while ((DWT->CYCCNT - start) < ticks) {
        ;
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}
