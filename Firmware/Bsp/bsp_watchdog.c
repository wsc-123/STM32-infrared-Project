#include "bsp_watchdog.h"

#define IWDG_KEY_RELOAD         0xAAAAU
#define IWDG_KEY_ENABLE         0xCCCCU
#define IWDG_KEY_WRITE_ACCESS   0x5555U

/* LSI is typically 40kHz. Prescaler 64 gives about 1.6ms/tick, so 2500 ticks
 * is about 4s. LSI tolerance is large, but this is enough for the nonblocking
 * main loop and long enough for IR send/Flash save bursts. */
#define IWDG_PR_DIV64           0x04U
#define IWDG_RELOAD_4S          2500U

void watchdog_feed(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}

void watchdog_init(void)
{
    /* 标准初始化顺序（同 ST 官方库示例）：解锁 -> 配置 -> 喂狗 -> 使能。
     * 不要在使能之前等待 PVU/RVU 清零：该同步回执要等 IWDG 启动后才会
     * 完成，启动前死等会卡死整个系统（2026-07 实测踩坑，Blue Pill 兼容
     * 芯片上必现）。PVU/RVU 的正确用途是"运行中再次修改 PR/RLR 前确认
     * 上次已完成"。LSI 无需手动开启，写 0xCCCC 使能时硬件自动强制开启。 */
    IWDG->KR  = IWDG_KEY_WRITE_ACCESS;
    IWDG->PR  = IWDG_PR_DIV64;
    IWDG->RLR = IWDG_RELOAD_4S;
    watchdog_feed();
    IWDG->KR = IWDG_KEY_ENABLE;
}
