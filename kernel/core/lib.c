#include "kernel.h"

size_t kstrlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

void *kmemset(void *dst, int c, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *kmemmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s || d >= s + n) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return (int)*p - (int)*q;
        p++; q++;
    }
    return 0;
}

char *kstrcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char *kstrchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}

int katoi(const char *s) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (*s++ - '0');
    return n * sign;
}

void kitoa(int32_t val, char *buf, int base) {
    char tmp[33];
    int i = 0;
    bool neg = FALSE;
    if (val < 0 && base == 10) { neg = TRUE; val = -val; }
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    uint32_t uval = (uint32_t)val;
    while (uval) {
        uint32_t rem = uval % (uint32_t)base;
        tmp[i++] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
        uval /= (uint32_t)base;
    }
    if (neg) tmp[i++] = '-';
    tmp[i] = '\0';
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}

void kutoa(uint32_t val, char *buf, int base) {
    char tmp[33];
    int i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val) {
        uint32_t rem = val % (uint32_t)base;
        tmp[i++] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
        val /= (uint32_t)base;
    }
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}
