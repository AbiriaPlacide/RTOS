
/*
 * userDefinedHashes.c
 *
 *  Created on: Mar 19, 2021
 *      Author: abiria
 */

#include <stdint.h>
#include "uart0.h"

char CharConvertToLowerCase(char s)
{
    if(s >= 'A' && s <= 'Z')
    {
       s += 32;
    }
    return s;
}

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


void strcpyChar(char * dest, const char * src)
{
    uint8_t i = 0;
    while(src[i] != '\0')
    {
        dest[i] = CharConvertToLowerCase(src[i]);
        i++;
    }

    dest[i] = '\0';
}

uint8_t str_len(const char name[])
{
    uint8_t i;
    for(i = 0; name[i] != '\0'; i++);

    return i;
}

uint32_t charStringToInt(const char name [])
{
    uint32_t convert = 0;
    uint32_t stringLen = str_len(name);
    uint8_t i;

    for(i = 0; i < stringLen; i++)
    {
        convert = convert * 10 + ( name[i] - '0');
    }
    return convert;
}

uint32_t hexToUint32(const char name[])
{
    uint32_t i;
    uint32_t convert = 0;
    char errant = 0;
    uint32_t stringLen = str_len(name);

    for(i = 0; i < stringLen; i++)
    {
        if(name[i] - '0' > 9)
        {
            errant = CharConvertToLowerCase(name[i]);

            errant = errant - 'a' + 10;
        }
        else
        {
            errant = (name[i] - '0');
        }

        if(name[1] == 'x' || name[1] == 'X')
        {
            if(i > 1)
            {
                convert = (convert << 4) | ((uint8_t)errant & 0xF); //add lowest 4 bits to final value
            }
        }

        else
        {
            convert = (convert << 4) | ((uint8_t)errant & 0xF); //add lowest 4 bits to final value
        }

    }

    return convert;
}


void printHexFromUint32(uint32_t val)
{
    uint32_t i;
    uint32_t errant = 0;
    uint32_t loop = 8;
    putsUart0("0x");
    for(i = 0; i < loop; i++)
    {
        errant = val & 0xF0000000;
        errant = errant >> 28; //shift by 4 * 7 half-bytes
        if(errant > 9)
        {
            switch(errant)
            {
                case 15:
                    putsUart0("F");
                    break;
                case 14:
                    putsUart0("E");
                    break;
                case 13:
                    putsUart0("D");
                    break;
                case 12:
                    putsUart0("C");
                    break;
                case 11:
                    putsUart0("B");
                    break;
                case 10:
                    putsUart0("A");
                    break;
            }
        }
        else
        {
            putcUart0(errant + '0');
        }

        val = (val << 4);
    }

    putsUart0("\r\n");
}



