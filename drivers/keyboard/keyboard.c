#include "kernel.h"

#define KBD_DATA    0x60
#define KBD_STATUS  0x64
#define KBD_BUF_SIZE 64

static char  kbd_buf[KBD_BUF_SIZE];
static uint8_t kbd_head = 0, kbd_tail = 0;
static bool   kbd_shift = FALSE;
static bool   kbd_ctrl  = FALSE;
static bool   kbd_caps  = FALSE;

static const char sc_lower[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',
    0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char sc_upper[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',
    0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void kbd_enqueue(char c)
{
    uint8_t next = (uint8_t)((kbd_tail + 1) % KBD_BUF_SIZE);
    if (next != kbd_head) {
        kbd_buf[kbd_tail] = c;
        kbd_tail = next;
    }
}

void keyboard_handler(void)
{
    uint8_t sc = inb(KBD_DATA);
    bool released = !!(sc & 0x80);
    sc &= 0x7F;

    switch (sc) {
    case 0x2A: case 0x36: kbd_shift = !released; return;
    case 0x1D:             kbd_ctrl  = !released; return;
    case 0x3A: if (!released) kbd_caps = !kbd_caps; return;
    default: break;
    }

    if (released) return;

    bool upper = (kbd_shift ^ kbd_caps);
    char c = upper ? sc_upper[sc] : sc_lower[sc];
    if (c) {
        if (kbd_ctrl && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 1);
        kbd_enqueue(c);
    }
}

void keyboard_install(void)
{
    kbd_head = kbd_tail = 0;
}

int keyboard_poll(void)
{
    if (kbd_head == kbd_tail) return -1;
    char c = kbd_buf[kbd_head];
    kbd_head = (uint8_t)((kbd_head + 1) % KBD_BUF_SIZE);
    return (unsigned char)c;
}

int keyboard_getchar(void)
{
    int c;
    while ((c = keyboard_poll()) == -1)
        __asm__ volatile("hlt");
    return c;
}
