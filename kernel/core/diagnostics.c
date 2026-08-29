#include "kernel.h"

void ahk_diagnostic_test(void)
{
    /* Test memory */
    uint8_t *test = kmalloc(1024);
    ASSERT(test != NULL);
    kmemset(test, 0xAB, 1024);
    ASSERT(test[512] == 0xAB);
    kfree(test);
    
    /* Test string functions */
    char buf[32];
    kstrcpy(buf, "AHK");
    ASSERT(kstrcmp(buf, "AHK") == 0);
    
    /* Test arithmetic */
    ASSERT(1 + 1 == 2);
    
    LOG_INFO_MSG("[DIAG] All diagnostic tests passed!\n");
}
