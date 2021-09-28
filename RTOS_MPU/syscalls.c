
#include "syscalls.h"
#include "gpio.h"
#include "uart0.h"


void reboot()
{
    selectPinPushPullOutput(GREEN_LED);
    setPinValue(GREEN_LED,1);
    NVIC_APINT_R = (0x05FA0000 | NVIC_APINT_SYSRESETREQ);

}
void sys_ps()
{
    putsUart0("ps called\r\n");
}

void sys_ipcs()
{
    putsUart0("ipcs called\r\n");
}
void sys_kill(char * pid)
{
    putsUart0("pid# ");
    putsUart0(pid);
    putsUart0(" killed\r\n");
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
void sys_preempt(_Bool on_off)
{
    //Turns preemption on or off.

    if(on_off)
    {
        putsUart0("Preempt on\r\n");
    }

    else if(!on_off)
    {
        putsUart0("Preempt off\r\n");
    }
}
void sys_sched (_Bool prio_on)
{
    if(prio_on)
    {
        putsUart0("sched rr\r\n");
    }

    else
    {
        putsUart0("sched prio\r\n");
    }
}
void sys_pidof(char name[])
{
    putsUart0(name);
    putsUart0(" launched\r\n");
}
void sys_select()
{
   //for now turn on red LED
    turnOnLED(RED_LED,1);
}
