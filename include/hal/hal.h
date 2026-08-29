#ifndef AHK_HAL_H
#define AHK_HAL_H

#include "../kernel/kernel.h"

void pic_remap(int offset1, int offset2);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif /* AHK_HAL_H */
