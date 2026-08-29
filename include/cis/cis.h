#ifndef AHK_CIS_H
#define AHK_CIS_H

#include "../kernel/kernel.h"

void cis_init(void);
void cis_handle_fault(registers_t *regs);

#endif /* AHK_CIS_H */
