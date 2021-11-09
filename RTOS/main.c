

/**
 * main.c
 */

#include <shell_API.h>
#include "gpio.h"
#include "clock.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "helperfunctions.h"
#include "syscalls.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


    //*************************Hardware info********************
    // Pushbuttons: 0-5 PORTC_4-7, PORTD_6-7
    // LEDS: BLUE: PF2(on-board), RED:PA2, ORANGE:PA3, YELLOW:PA4, GREEN:PE0


//#define RED_LED       (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))
//#define BLUE_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4)))
//#define GREEN_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))

//button bit-band aliasses

#define BUTTON0       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4))) //PC4
#define BUTTON1       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4))) //PC4
#define BUTTON2       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4))) //PC5
#define BUTTON3       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 7*4))) //PC6
#define BUTTON4       (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 6*4))) //PD6
#define BUTTON5       (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 7*4))) //PD7

/** Assembly functions. Remaining functions are declared in startup_ccs.c file */
extern void setPSP(uint32_t *StackPointer); //pass in address of the stack. implemented in assembly
extern void setPrivilege(void); //turn the ASP bit on

void inithw()
{



    //initialize clock & uart
    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6);

    //enable ports to use
    enablePort(PORTF); //for LED's and comparator output at PF0
    enablePort(PORTC);
    enablePort(PORTD);
    enablePort(PORTA);
    enablePort(PORTE);

    //unlock PORTD, Pin 7

    setPinCommitControl(PORTD, 7);

    //set PORT F,A,E LED's as outputs
    selectPinPushPullOutput(PORTF,2);
    selectPinPushPullOutput(PORTA,2);
    selectPinPushPullOutput(PORTA,3);
    selectPinPushPullOutput(PORTA,4);
    selectPinPushPullOutput(PORTE,0);

    //enable pullups for LEDS

    enablePinPullup(PORTF,2);
    enablePinPullup(PORTA,2);
    enablePinPullup(PORTA,3);
    enablePinPullup(PORTA,4);
    enablePinPullup(PORTE,0);

    //Configure BUTTONS for PORTS C&D
    selectPinDigitalInput(PORTC,4);
    selectPinDigitalInput(PORTC,5);
    selectPinDigitalInput(PORTC,6);
    selectPinDigitalInput(PORTC,7);
    selectPinDigitalInput(PORTD,6);
    selectPinDigitalInput(PORTD,7);

    //set internal pull up resistors
    enablePinPullup(PORTC,4);
    enablePinPullup(PORTC,5);
    enablePinPullup(PORTC,6);
    enablePinPullup(PORTC,7);
    enablePinPullup(PORTD,6);
    enablePinPullup(PORTD,7);
}


uint8_t readPbs()
{
   uint8_t sum = 0;

   if(BUTTON0 == 0)
   {
       sum+=1;
   }

   if(BUTTON1==0)
   {
       sum+=2;
   }

   if(BUTTON2==0)
   {
       sum+=4;
   }

   if(BUTTON3==0)
   {
       sum+=8;;
   }

   if(BUTTON4==0)
   {
       sum+=16;
   }

   if(BUTTON5==0)
   {
       sum+=32;
   }

   return sum;
}


int main(void)
{
    inithw();
    char buffer[20];
    while(1)
    {
        uint8_t reader = readPbs();
        sprintf(buffer, "%u", reader); //this functiona adds 16KiB to the overall code size.
        putsUart0(buffer);
        putsUart0("\r\n");
        __delay_cycles(40000000); //wait one second after reading. This is for testing purposes only.
        /*
        turnOnLED(PORTA,2,1);
        turnOnLED(PORTA,3,1);
        turnOnLED(PORTA,4,1);
        turnOnLED(PORTE,0,1);
        turnOnLED(BLUE,1);
        */
    }
}

void shell()

{

    USER_DATA data; //instantiate data from user
    while(1)
    {
        getsUart0(&data); //get data from  tty interface
        putsUart0("\r\n");
        putcUart0('>');

        // Parse fields
        parseFields(&data); //parse input data

        if(isCommand(&data, "reboot", 0))
        {
            reboot();

        }

        else if(isCommand(&data, "ps", 0))
        {
            sys_ps();
        }

        else if(isCommand(&data, "ipcs", 0))
        {
            sys_ipcs();
        }

        else if(isCommand(&data, "kill", 1))
        {
            char * pid = getFieldString (&data, 1);
            sys_kill(pid);
        }

        else if(isCommand(&data, "pi", 1))
        {
            //priority on or off
            char * pid = getFieldString (&data, 1);

            if(str_cmp(pid, "off") == 0) { sys_pi(0); }
            else if(str_cmp(pid, "on") == 0) { sys_pi(1); }

        }

        else if(isCommand(&data, "preempt", 1))
        {
            //preempt on or off

            char * pid = getFieldString (&data, 1);

             if( str_cmp(pid, "off") == 0) { sys_preempt(0); }
             else if( str_cmp(pid, "on") == 0) { sys_preempt(1); }

        }

        else if(isCommand(&data, "sched", 1))
        {
             //select priority or round robbin

            char * pid = getFieldString (&data, 1);
            if( str_cmp(pid, "prio") == 0){ sys_sched(0); }
            else if( str_cmp(pid, "rr") == 0 ){ sys_sched(1); }
        }

        else if(isCommand(&data, "pidof", 1))
        {
            char * pid = getFieldString (&data, 1);
            sys_pidof(pid);
        }

        else
        {
            //get second arg to see if its an argument
            uint8_t args = data.fieldCount-1;

            if(args == 1)
            {
                char * pid = getFieldString (&data, 1);
                char * name = getFieldString (&data, 0);
                if(*pid == '&')
                {
                    //run program at argument string 0
                    sys_select(); //for now it will turn on RED
                }
            }
        }
    }

}
