
#include "syscalls.h"
#include "gpio.h"
#include "uart0.h"


#define RED_LED PORTF,1
#define GREEN_LED PORTF,3
#define BLUE_LED PORTF,2 //PF2

void reboot()
{
    __asm(" SVC #7");
}
void sys_ps()
{
    putsUart0("ps called\r\n");
}

void sys_ipcs()
{
    putsUart0("ipcs called\r\n");
}
void sys_kill(char pid[])
{
    __asm(" SVC #9");

    //putsUart0(pid);
    //putsUart0(" killed\r\n");
}
void sys_pi(_Bool on_off)
{
    //Turns priority inheritance on or off.
    if(on_off)
    {
        putsUart0("PI on\r\n");
    }

    else
    {
        putsUart0("PI off\r\n");
    }
}

void sys_sched (_Bool prio_on)
{
    __asm(" SVC #5");
}

void sys_preempt(_Bool on_off)
{
    __asm(" SVC #6");
}
void sys_pidof(uint32_t * pid, char name[16])
{
    __asm (" SVC #8");
}
void sys_select(char name[])
{
    __asm(" SVC #10");

}

//***************************************mpu sys calls**************
//AP bits common 011(RW for all, full access) 001(RW for privilege only)

void mpu_setRegionNumber(uint8_t number)
{
    //8 total regions. bits [2:0]
    if(number <=7)
    {
        NVIC_MPU_NUMBER_R |= number;
    }
}

//void mpu_ctrlReg(uint8_t mask)
//{
//    // bits [2:0]
//    // 0x4 : provide default background region (bit ignored if MPU is off)
//    // 0x2 : MPU enabled during faults (do not set bit 1(HFNMIENA) if MPU is disabled. causes unpredictable.)
//    // 0x1 : Enable MPU
//    //*********************************
//    // 0x7 : 0x1 | 0x2 | 0x3
//    // 0x5 : 0x1 | 0x4
//    // 0x3 : 0x1 | 0x2
//
//    NVIC_MPU_CTRL_R |= mask;
//}

void mpu_region_base(uint32_t addr, uint8_t region)
{
    //addr field is bits 31:N, N = log2(Region Size in bytes). Has to be an integer multiple of SIZE
    //if valid bit is set within addr, mpu_setRegionNumber() does not have to be used.

    addr |= (BASE_ADDR_VALID_ON | region); //valid bit has to be set within the address to also change the region number

    NVIC_MPU_BASE_R = addr; //set address
}

void mpu_region_attr(uint32_t attributes)

{
    //permission access attributes: XN, AP, [TEX, S, C ,B]

    NVIC_MPU_ATTR_R = attributes;

}

void mpu_subregion_disable(uint32_t region)
{
    region = (region << 8);
    NVIC_MPU_ATTR_R |= region;
}

void mpu_region_number(uint8_t number)
{
    NVIC_MPU_NUMBER_R = number;
}

void mpu_enable()
{
    NVIC_MPU_CTRL_R |= 0x1; // 0th bit of reg is enabled, b'101  PRIVDEFEN & ENABLE set to 1. Allow a default background region for now
}

void mpu_disable()
{
    NVIC_MPU_CTRL_R &= ~0x7;
}


void mpu_memfault_enable()
{
    NVIC_SYS_HND_CTRL_R |=  MPU_MEM_FAULT_ON;
}

void mpu_busfault_enable(void)
{
    NVIC_SYS_HND_CTRL_R |=  MPU_BUS_FAULT_ON;
}
void mpu_usagefault_enable(void)
{
    NVIC_SYS_HND_CTRL_R |=  MPU_USAGE_FAULT_ON;
}

//************************************system exceptions**********************************








