#ifndef RM_BOARD_H
#define RM_BOARD_H
#include "devices.h"

void board_hold_fixed_outputs_off(void);
void board_clock_init(void);
bool board_debug_init(Transport *pc);
void board_fatal(void);
Devices board_devices(void);
#endif
