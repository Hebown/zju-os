#include "string.h"
#include "stdint.h"
#include "stddef.h"
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

void *memset(void *dest, int c, uint64_t n) {
    char *s = (char *)dest;
    for (uint64_t i = 0; i < n; ++i) {
        s[i] = c;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, uint64_t n) {
    const uint8_t *source = (const uint8_t *)src;
    uint8_t *target = (uint8_t *)dest;
    for (uint64_t i = 0; i < n; i++) {
        target[i] = source[i];
    }
    return dest;
}

int memcmp(const void *str1, const void *str2, uint64_t n) {
    unsigned char *p1 = (unsigned char *)str1;
    unsigned char *p2 = (unsigned char *)str2;
    
    for (uint64_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

// 复制字符串
char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++) != '\0');
    return dest;
}

// 复制字符串（带长度限制）
char* strncpy(char* dest, const char* src, size_t n) {
    char* d = dest;
    while (n-- > 0) {
        if ((*d = *src) != '\0') {
            src++;
        }
        d++;
    }
    return dest;
}

// 比较两个字符串
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 比较两个字符串（带长度限制）
int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    
    while (n-- > 0 && *s1 && *s2) {
        if (*s1 != *s2) {
            return *(const unsigned char*)s1 - *(const unsigned char*)s2;
        }
        s1++;
        s2++;
    }
    
    if (n == (size_t)-1) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
