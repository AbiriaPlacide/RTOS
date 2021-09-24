

/**
 * main.c
 * Abiria Placide
 */

#include "gpio.h"
#include "shell.h"
#include "watchdog.h"
#include "clock.h"
#include "uart0.h"
#include "tm4c123gh6pm.h"
#include "syscalls.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


void inithw()
{
    //initialize clock
    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6);

    //enable port F for debugging

    enablePort(PORTF); //for LED's and comparator output at PF0

    //set PORT F LED's as outputs
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);

}

int main(void)
{
    inithw();
    putsUart0("\n\nReady...\r\n");
    putcUart0('#');

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
            //use watchdogTimer to reset hardware.
            selectPinPushPullOutput(GREEN_LED);
            setPinValue(GREEN_LED,1);
            initWatchdog0(); //Time to reset is set by TIMEOUT_MS in watchdog.c
            while(true);
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
