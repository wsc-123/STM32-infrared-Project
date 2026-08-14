#ifndef __MIDEA_AC_H
#define __MIDEA_AC_H

#include "stm32f10x.h"

typedef enum {
    MIDEA_AC_COOL26 = 0,
    MIDEA_AC_COOL25,
    MIDEA_AC_HEAT28,
    MIDEA_AC_OFF,
    MIDEA_AC_F_COOL26,
    MIDEA_AC_F_COOL25,
    MIDEA_AC_F_HEAT28,
    MIDEA_AC_F_OFF
} midea_ac_preset_t;

uint8_t midea_ac_send_preset(midea_ac_preset_t preset);

#endif /* __MIDEA_AC_H */
