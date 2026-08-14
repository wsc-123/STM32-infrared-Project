#include "midea_ac.h"
#include "bsp_ir_tx.h"

#define MIDEA_HDR_MARK_US       4480U
#define MIDEA_HDR_SPACE_US      4480U
#define MIDEA_BIT_MARK_US       560U
#define MIDEA_ONE_SPACE_US      1680U
#define MIDEA_ZERO_SPACE_US     560U
#define MIDEA_GAP_US            5600U

#define MIDEA_BITS              48U
#define MIDEA_PHASE_EDGES       (2U + (MIDEA_BITS * 2U) + 2U)
#define MIDEA_FRAME_EDGES       (MIDEA_PHASE_EDGES * 2U)
#define MIDEA_STATE_MASK        0xFFFFFFFFFFFFULL

#define MIDEA_STATE_COOL26      0xA18009FFFF37ULL
#define MIDEA_STATE_COOL25      0xA18008FFFF36ULL
#define MIDEA_STATE_HEAT28      0xA1830BFFFF37ULL
#define MIDEA_STATE_OFF         0xA10009FFFFB7ULL

/* Some Midea remotes encode the temperature field differently. These variants
 * are kept as fallback test codes if the normal Celsius presets do not work. */
#define MIDEA_STATE_F_COOL26    0xA18031FFFF0FULL
#define MIDEA_STATE_F_COOL25    0xA1802FFFFF11ULL
#define MIDEA_STATE_F_HEAT28    0xA18334FFFF09ULL
#define MIDEA_STATE_F_OFF       0xA10031FFFF8FULL

static uint16_t s_midea_frame[MIDEA_FRAME_EDGES];

static void append_edge(uint16_t *buf, uint16_t *idx, uint16_t us)
{
    if (*idx < MIDEA_FRAME_EDGES) {
        buf[*idx] = us;
        (*idx)++;
    }
}

static void append_bit(uint16_t *buf, uint16_t *idx, uint8_t one)
{
    append_edge(buf, idx, MIDEA_BIT_MARK_US);
    append_edge(buf, idx, one ? MIDEA_ONE_SPACE_US : MIDEA_ZERO_SPACE_US);
}

static void append_byte_msb(uint16_t *buf, uint16_t *idx, uint8_t v)
{
    int8_t bit;

    for (bit = 7; bit >= 0; bit--) {
        append_bit(buf, idx, (uint8_t)((v >> bit) & 0x01U));
    }
}

static void append_phase(uint16_t *buf, uint16_t *idx, uint64_t state)
{
    int8_t byte;

    append_edge(buf, idx, MIDEA_HDR_MARK_US);
    append_edge(buf, idx, MIDEA_HDR_SPACE_US);

    for (byte = 5; byte >= 0; byte--) {
        append_byte_msb(buf, idx, (uint8_t)((state >> (byte * 8)) & 0xFFU));
    }

    append_edge(buf, idx, MIDEA_BIT_MARK_US);
    append_edge(buf, idx, MIDEA_GAP_US);
}

static uint8_t preset_to_state(midea_ac_preset_t preset, uint64_t *state)
{
    switch (preset) {
    case MIDEA_AC_COOL26:   *state = MIDEA_STATE_COOL26;   return 1;
    case MIDEA_AC_COOL25:   *state = MIDEA_STATE_COOL25;   return 1;
    case MIDEA_AC_HEAT28:   *state = MIDEA_STATE_HEAT28;   return 1;
    case MIDEA_AC_OFF:      *state = MIDEA_STATE_OFF;      return 1;
    case MIDEA_AC_F_COOL26: *state = MIDEA_STATE_F_COOL26; return 1;
    case MIDEA_AC_F_COOL25: *state = MIDEA_STATE_F_COOL25; return 1;
    case MIDEA_AC_F_HEAT28: *state = MIDEA_STATE_F_HEAT28; return 1;
    case MIDEA_AC_F_OFF:    *state = MIDEA_STATE_F_OFF;    return 1;
    default:                                           return 0;
    }
}

uint8_t midea_ac_send_preset(midea_ac_preset_t preset)
{
    uint64_t state;
    uint16_t len = 0;

    if (!preset_to_state(preset, &state)) {
        return 0;
    }

    append_phase(s_midea_frame, &len, state & MIDEA_STATE_MASK);
    append_phase(s_midea_frame, &len, (~state) & MIDEA_STATE_MASK);

    ir_tx_send(s_midea_frame, len);
    return 1;
}
