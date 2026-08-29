#ifndef AHK_DRIVERS_H
#define AHK_DRIVERS_H

#include "../kernel/kernel.h"

void keyboard_install(void);
void keyboard_handler(void);
int  keyboard_getchar(void);
int  keyboard_poll(void);

#endif /* AHK_DRIVERS_H */
