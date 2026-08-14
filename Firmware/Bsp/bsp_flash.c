#include "bsp_flash.h"
#include "stm32f10x_flash.h"

static void flash_store_clear_flags(void)
{
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

uint8_t flash_store_erase(void)
{
    uint32_t addr;

    FLASH_Unlock();
    flash_store_clear_flags();

    for (addr = FLASH_STORE_BASE;
         addr < (FLASH_STORE_BASE + FLASH_STORE_SIZE);
         addr += FLASH_STORE_PAGE_SIZE) {
        if (FLASH_ErasePage(addr) != FLASH_COMPLETE) {
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();
    return 1;
}

uint8_t flash_store_write(uint32_t off, const void *src, uint32_t bytes)
{
    const uint8_t *p = (const uint8_t *)src;
    uint32_t       i;

    if (src == 0 || off > FLASH_STORE_SIZE || bytes > (FLASH_STORE_SIZE - off)) {
        return 0;
    }

    FLASH_Unlock();
    flash_store_clear_flags();

    for (i = 0; i < bytes; i += 2U) {
        uint32_t addr = FLASH_STORE_BASE + off + i;
        uint16_t half = p[i];

        if ((i + 1U) < bytes) {
            half |= (uint16_t)((uint16_t)p[i + 1U] << 8);
        } else {
            half |= 0xFF00U;
        }

        if (FLASH_ProgramHalfWord(addr, half) != FLASH_COMPLETE ||
            (*(volatile uint16_t *)addr) != half) {
            FLASH_Lock();
            return 0;
        }
    }

    FLASH_Lock();
    return 1;
}

const uint8_t *flash_store_ptr(void)
{
    return (const uint8_t *)FLASH_STORE_BASE;
}
