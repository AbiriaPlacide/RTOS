/*
 * systick.c
 *
 *  Created on: Nov 3, 2021
 *      Author: abiria
 */

#include <stdint.h>
#include "tm4c123gh6pm.h"

void sysTickLoadValue(uint32_t value)
{
    if(value <= 0xFFFFFF)
    {
        NVIC_ST_RELOAD_R = value;
    }
}

void sysTickClear(uint32_t value)
{
    /* writing any value to this register resets it to zero ad count bit in Control and satus register is cleared*/
    NVIC_ST_CURRENT_R = value;
}

void sysTickCtrl()
{
    //BIT 0: enable systick
    //BIT 1: allows interrupts
    //BIT 2: (0): use PIOSC, (1): use sys clock
    NVIC_ST_CTRL_R = 0x7;
}

void sysTickConfig(uint32_t freq)
{
    sysTickLoadValue(freq);
    sysTickClear(21);
    sysTickCtrl();

}
