#pragma once

#include "includes.h"

void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width);
void flip_str(char* str);
bool isdigit(char c);
bool isuppercasealpha(char c);
bool islowercasealpha(char c);
uint32_t char_to_digit(char c);
uint32_t atoi(char* str, uint32_t base);
void cpystr(char* source, char* dst);
int32_t strcmp(char* str1, char* str2);
int32_t strncmp(char* str1, char* str2, uint32_t n);
uint32_t strlen(char* str);