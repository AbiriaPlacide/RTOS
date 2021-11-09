
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

int str_cmp(const char * str1, const char *str2) //reference from Ti
{
    int character, result;

   for (;;)
   {
       character  = (unsigned char)*str1++;
       result = character - (unsigned char)*str2++;

       if (character == 0 || result != 0) break;
   }

   return result;
}


void printHex(uint32_t value)
{
    uint8_t remainder;
    int8_t i;
    char hexOut[64];
    uint32_t counter = 0;

    while(value != 0)
    {
        remainder =  value%16;
        if(remainder <= 9)
        {
            remainder += '0'; //convert to char
        }

        else if( remainder > 9 )
        {
            remainder += 'A' - 10; //minus 10 because this is after 10 ASCII values have passed
        }

        value /=16;
        hexOut[counter] = remainder;
        counter++;
    }

    hexOut[counter+1] = '\0'; //terminate end of char array

    putsUart0("0x");
    for(i=counter-1; i>=0; i--)
    {
        putcUart0(hexOut[i]);
    }

    if(counter == 0)
    {
        putsUart0("0");
    }
    putsUart0("\r\n"); //new line after printing all lines
}

