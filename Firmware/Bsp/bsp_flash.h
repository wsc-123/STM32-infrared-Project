#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "stm32f10x.h"

/*
 * Reserve the top 8KB of the nominal 64KB STM32F103C8 flash for small
 * application data. Keep the Keil IROM size in mind if firmware grows.
 */
#define FLASH_STORE_BASE       ((uint32_t)0x0800E000U)
#define FLASH_STORE_SIZE       ((uint32_t)8192U)
#define FLASH_STORE_PAGE_SIZE  ((uint32_t)1024U)

uint8_t        flash_store_erase(void);
uint8_t        flash_store_write(uint32_t off, const void *src, uint32_t bytes);
const uint8_t *flash_store_ptr(void);

#endif /* __BSP_FLASH_H */
