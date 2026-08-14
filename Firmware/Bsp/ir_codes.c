#include "ir_codes.h"
#include "bsp_flash.h"
#include <string.h>

#define IR_STORE_MAGIC      ((uint32_t)0x31475249U)  /* "IRG1" */
#define IR_STORE_VERSION    ((uint16_t)1U)

typedef struct {
    uint16_t len;
    uint16_t data[IR_FRAME_MAX];
} ir_frame_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t slot_count;
    uint32_t data_bytes;
    uint32_t checksum;
} ir_store_hdr_t;

static ir_frame_t s_slots[IR_SLOT_COUNT];

static uint32_t ir_store_checksum(const void *data, uint32_t bytes)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t       h = 2166136261UL;
    uint32_t       i;

    for (i = 0; i < bytes; i++) {
        h ^= p[i];
        h *= 16777619UL;
    }
    return h;
}

void ir_codes_init(void)
{
    uint8_t i;

    for (i = 0; i < IR_SLOT_COUNT; i++) {
        s_slots[i].len = 0;
    }
}

uint8_t ir_codes_store(uint8_t slot, const uint16_t *data, uint16_t len)
{
    uint16_t i;

    if (slot >= IR_SLOT_COUNT || data == 0 || len == 0) {
        return 0;
    }
    if (len > IR_FRAME_MAX) {
        len = IR_FRAME_MAX;
    }
    for (i = 0; i < len; i++) {
        s_slots[slot].data[i] = data[i];
    }
    s_slots[slot].len = len;
    return 1;
}

const uint16_t *ir_codes_get(uint8_t slot, uint16_t *len)
{
    if (slot >= IR_SLOT_COUNT || s_slots[slot].len == 0) {
        if (len) {
            *len = 0;
        }
        return 0;
    }
    if (len) {
        *len = s_slots[slot].len;
    }
    return s_slots[slot].data;
}

uint8_t ir_codes_count(void)
{
    uint8_t count = 0;
    uint8_t i;

    for (i = 0; i < IR_SLOT_COUNT; i++) {
        if (s_slots[i].len > 0 && s_slots[i].len <= IR_FRAME_MAX) {
            count++;
        }
    }
    return count;
}

uint8_t ir_codes_save(void)
{
    ir_store_hdr_t hdr;
    uint32_t       data_bytes = (uint32_t)sizeof(s_slots);

    if (((uint32_t)sizeof(hdr) + data_bytes) > FLASH_STORE_SIZE) {
        return 0;
    }

    hdr.magic      = IR_STORE_MAGIC;
    hdr.version    = IR_STORE_VERSION;
    hdr.slot_count = IR_SLOT_COUNT;
    hdr.data_bytes = data_bytes;
    hdr.checksum   = ir_store_checksum(s_slots, data_bytes);

    if (!flash_store_erase()) {
        return 0;
    }

    /* Commit data first and header last, so reset during save leaves invalid magic. */
    if (!flash_store_write((uint32_t)sizeof(hdr), s_slots, data_bytes)) {
        return 0;
    }
    return flash_store_write(0U, &hdr, (uint32_t)sizeof(hdr));
}

uint8_t ir_codes_load(void)
{
    const uint8_t        *base       = flash_store_ptr();
    const ir_store_hdr_t *hdr        = (const ir_store_hdr_t *)base;
    uint32_t             data_bytes  = (uint32_t)sizeof(s_slots);
    uint8_t              i;

    if (hdr->magic != IR_STORE_MAGIC ||
        hdr->version != IR_STORE_VERSION ||
        hdr->slot_count != IR_SLOT_COUNT ||
        hdr->data_bytes != data_bytes) {
        return 0;
    }

    if (ir_store_checksum(base + sizeof(ir_store_hdr_t), data_bytes) != hdr->checksum) {
        return 0;
    }

    memcpy(s_slots, base + sizeof(ir_store_hdr_t), data_bytes);

    for (i = 0; i < IR_SLOT_COUNT; i++) {
        if (s_slots[i].len > IR_FRAME_MAX) {
            s_slots[i].len = 0;
        }
    }

    return ir_codes_count();
}

void ir_codes_clear(uint8_t slot)
{
    if (slot < IR_SLOT_COUNT) {
        s_slots[slot].len = 0;
    }
}
