#ifndef __STRING_H__
#define __STRING_H__

#include "stdint.h"
#include "stddef.h"
#include <stddef.h>
void *memset(void *, int, uint64_t);

void *memcpy(void *dest, const void *src, uint64_t n);

int memcmp(const void *str1, const void *str2, uint64_t n);

size_t strlen(const char *str);

char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
#endif
