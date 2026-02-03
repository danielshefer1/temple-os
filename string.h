#pragma once

#include "includes.h"

void itoa(uint64_t value, char* str, uint64_t base, uint64_t min_width);
void flip_str(char* str);
bool isdigit(char c);
bool isuppercasealpha(char c);
bool islowercasealpha(char c);
uint64_t char_to_digit(char c);
uint64_t atoi(char* str, uint64_t base);
void cpystr(char* source, char* dst);
int32_t strcmp(char* str1, char* str2);
int32_t strncmp(char* str1, char* str2, uint64_t n);
uint64_t strlen(char* str);