

/**
 * main.c
 * Abiria Placide
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

/*define program stack size*/

#define STACK_SIZE  128 //bytes

/** Assembly functions. Remaining functions are declared in startup_ccs.c file */
extern void setPSP(uint32_t *StackPointer); //pass in address of the stack. implemented in assembly
extern void setPrivilege(void); //turn the ASP bit on

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

void testRun()
{
    uint8_t * ptr =  (uint8_t *) 0x20000400;
    *ptr = 'B';

    while(1);
}

int main(void)
{
    inithw(); //initialize the UART and port F

    //create descending stack
    uint32_t ProgramStack[STACK_SIZE];
    uint32_t * STACK_TOP = &(ProgramStack[STACK_SIZE]);
    setPSP(&(*STACK_TOP)); //Descending stack so set stack pointer to top of array.

    uint32_t flash_base_addr     = 0x00000000;  // 0x0000.0000 - 0x0003.FFFF | 256 KiB
    uint32_t sram_base_addr      = 0x20000000;  // 0x2000.0000 - 0x2000.7FFF | 32 KiB
    uint32_t periph_base_addr    = 0x40000000;  // 0x4000.0000 - 0x5FFF.FFFF | bit-banded address and alias region

    uint32_t sram8kiB_base_addr0 = 0x20000000;  // 0x2000.0000 - 0x2000.1FFF | 8 KiB, will have higher priority
    uint32_t sram8kiB_base_addr1 = 0x20002000;  // 0x2000.2000 - 0x2000.3FFF | 8 KiB, will have higher priority
    uint32_t sram8kiB_base_addr2 = 0x20004000;  // 0x2000.4000 - 0x2000.5FFF | 8 KiB, will have higher priority
    uint32_t sram8kiB_base_addr3 = 0x20006000;  // 0x2000.6000 - 0x2000.7FFF | 8 KiB, will have higher priority

    //*********************************regions definitions
    uint8_t flash_region         = 0x0;
    uint8_t sram_region          = 0x1;
    uint8_t periph_base_region   = 0x2;

    //size of each region = 0x1FFF = 8KiB
    uint8_t sram8kiB_region0     = 0x3; //0x0000 - 0x1FFF
    uint8_t sram8kiB_region1     = 0x4; //0x2000 - 0x3FFF
    uint8_t sram8kiB_region2     = 0x5; //0x4000 - 0x5FFF
    uint8_t sram8kiB_region3     = 0x6; //0x6000 - 0x7FFF


    mpu_memfault_enable(); //if not enabled a hard-fault will be called instead.


    //********************FLASH Region
    mpu_region_base(flash_base_addr, flash_region);
    mpu_region_attr(AP_FULL_ACCESS| FLASH_TXSCB_ENCODING | FLASH_SIZE_256K | MPU_REGION_ENABLE );

    //********************SRAM Region
    //we need 8KiB in each region for a total of 32 KiB so 4 regions in total. each region will have 1 KiB granularity
   mpu_region_base(sram_base_addr,sram_region );
   mpu_region_attr(AP_FULL_ACCESS| SRAM_TXSCB_ENCODING | SRAM_SIZE_32K | MPU_REGION_ENABLE );

   //*********************Peripherals Region
    mpu_region_base(periph_base_addr,periph_base_region );
    mpu_region_attr(AP_FULL_ACCESS| PERIPHERALS_TXSCB_ENCODING | PERIPH_SIZE_M | MPU_REGION_ENABLE );

    //********************32 KiB region with 1 KiB  = 0x400 granularity
    mpu_region_base(sram8kiB_base_addr0, sram8kiB_region0);
    mpu_region_attr(AP_PRIV_ACCESS | SRAM_TXSCB_ENCODING | SRAM_SIZE_8KiB | MPU_REGION_ENABLE |  DISABLE_SUB_REGION_1 | DISABLE_SUB_REGION_0);

    mpu_region_base(sram8kiB_base_addr1, sram8kiB_region1);
    mpu_region_attr(AP_PRIV_ACCESS | SRAM_TXSCB_ENCODING | SRAM_SIZE_8KiB | MPU_REGION_ENABLE  | DISABLE_ALL_SUB_REGIONS );

    mpu_region_base(sram8kiB_base_addr2, sram8kiB_region2);
    mpu_region_attr(AP_PRIV_ACCESS | SRAM_TXSCB_ENCODING | SRAM_SIZE_8KiB | MPU_REGION_ENABLE  | DISABLE_ALL_SUB_REGIONS);

    mpu_region_base(sram8kiB_base_addr3, sram8kiB_region3);
    mpu_region_attr(AP_PRIV_ACCESS | SRAM_TXSCB_ENCODING | SRAM_SIZE_8KiB | MPU_REGION_ENABLE  | DISABLE_ALL_SUB_REGIONS);

    //Enable MPU & set/unset privilege
    mpu_enable();
    setPrivilege();

    testRun();
    //putsUart0("\n\nReady...\r\n");
    //putcUart0('#');
    //shell(); //run shell interface
}
