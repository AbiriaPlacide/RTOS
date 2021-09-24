
/*
 * userDefinedHashes.c
 *
 *  Created on: Mar 19, 2021
 *      Author: abiria
 */

#include <stdint.h>
#include "uart0.h"

void binarySend(uint32_t value)
{
    uint32_t shift = 0x80000000;
    int8_t i; //has to be signed or it will overflow to 11111111 and cause an infinite loop
    for(i=32-1; i >=0; i--)
    {
        if(value & shift)
        {
            putcUart0('1');
        }
        else
        {
            putcUart0('0');
        }
        shift = shift >> 1;
    }
    putsUart0("\r\n");

}

int str_cmp(const char * string1, const char *string2)
{
    int c1, res;

   for (;;)
   {
       c1  = (unsigned char)*string1++;
       res = c1 - (unsigned char)*string2++;

       if (c1 == 0 || res != 0) break;
   }

   return res;
}

