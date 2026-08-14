#ifndef __BSP_WATCHDOG_H
#define __BSP_WATCHDOG_H

#include "stm32f10x.h"

/* Independent watchdog. The timeout is about 4 seconds with the typical
 * STM32F1 LSI clock. Call watchdog_feed() regularly from the main loop. */
void watchdog_init(void);
void watchdog_feed(void);

#endif /* __BSP_WATCHDOG_H */
