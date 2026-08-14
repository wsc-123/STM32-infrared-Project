#ifndef __IR_CODES_H
#define __IR_CODES_H

#include "stm32f10x.h"

/* 学习到的红外整帧存储。
 * 空调遥控发的是“完整状态帧”（开关+模式+温度+风速一起），
 * 所以每个槽位存一整帧原始时序，回放即可复现某个固定状态。
 *
 * 学习结果保存在 RAM，并可同步保存到内部 Flash，掉电后可恢复。
 * 槽位与命令的映射见 cmd_parser.c。 */

#define IR_SLOT_COUNT       6
#define IR_FRAME_MAX        600        /* 与 bsp_ir_rx 的 IR_RX_MAX_EDGES 对齐 */

void            ir_codes_init(void);

/* 把一帧时序存入槽位 slot；成功返回 1 */
uint8_t         ir_codes_store(uint8_t slot, const uint16_t *data, uint16_t len);

/* 取槽位 slot 的帧；*len 写入段数；空槽返回 0 */
const uint16_t *ir_codes_get(uint8_t slot, uint16_t *len);

/* Flash persistence. load returns restored non-empty slot count. */
uint8_t         ir_codes_load(void);
uint8_t         ir_codes_save(void);
void            ir_codes_clear(uint8_t slot);
uint8_t         ir_codes_count(void);

#endif /* __IR_CODES_H */
