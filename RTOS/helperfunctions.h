#ifndef _MAIN_DEF_H
#define _MAIN_DEF_H
#include "gpio.h"

void binarySend(uint32_t value);
int str_cmp(const char * string1, const char *string2); //gotten from ti/cc source_code
void printHex(uint32_t value); //print hex values inputs
#endif
