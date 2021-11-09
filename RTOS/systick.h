/*
 * systick.h
 *
 *  Created on: Nov 3, 2021
 *      Author: abiria
 */

#ifndef SYSTICK_H_
#define SYSTICK_H_

#include <stdint.h>

/* programming systick steps
 * 1) program load value in the STRELOAD register
 * 2) Clear the STCURRENT register by writing in with any value
 * 3) Configure the STCTRL register for the required operation
 *
 * * when the processor is halted for debugging the counter does not decrement
 */

#define ONE_KHZ_CLK 39999


void sysTickLoadValue(uint32_t value);
void sysTickClear(uint32_t value);
void sysTickCtrl();
void sysTickConfig();
#endif /* SYSTICK_H_ */
