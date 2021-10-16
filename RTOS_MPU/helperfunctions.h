#ifndef _MAIN_DEF_H
#define _MAIN_DEF_H
#include "gpio.h"

#define RED_LED PORTF,1
#define GREEN_LED PORTF,3
#define BLUE_LED PORTF,2 //PF2

#define MEASURE_LR PORTE,4 //PE4
#define HIGHSIDE_R PORTE,5   //PE5

#define MEASURE_C PORTA,2 //PA2
#define LOWSIDE_R PORTA,3   //PA3
#define INTEGRATE PORTA,4 //PA4

#define DUT2_AINx PORTE,2 //PE 2, connected to AIN1 and DUT2
#define DUT2_C0 PORTC,7   //PC 7, connected to C0-  and DUT2


#define DISCHARGE_T 6600 //time to discharge = 5Time costants = 33 us x 5

void binarySend(uint32_t value);
int str_cmp(const char * string1, const char *string2); //gotten from ti/cc source_code
void printHex(uint32_t value); //print hex values inputs
#endif
