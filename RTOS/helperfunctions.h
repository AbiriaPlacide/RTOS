#ifndef _MAIN_DEF_H
#define _MAIN_DEF_H
#include "gpio.h"


void strcpyChar(char * dest, const char * src);
void binarySend(uint32_t value);
int str_cmp(const char * string1, const char *string2); //gotten from ti/cc source_code
void printHex(uint32_t value); //print hex values inputs//no longer being used.
void printHexFromUint32(uint32_t val);
char CharConvertToLowerCase(char s);
uint32_t hexToUint32(const char name[]);
uint32_t charStringToInt(const char name []);
uint8_t str_len(const char name[]);

#endif
