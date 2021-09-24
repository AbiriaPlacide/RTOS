#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"

#define TIMEOUT_MS 1000

void initWatchdog0()
{
    // Enable clock
    SYSCTL_RCGCWD_R |= SYSCTL_RCGCWD_R0;
    _delay_cycles(3);

    // Configure WDT0 which is driven by the system clock
    WATCHDOG0_LOAD_R = (uint32_t)TIMEOUT_MS * 40e3;      // convert into fcyc units
    WATCHDOG0_CTL_R |= WDT_CTL_RESEN;                    // enable reset if timeout
    WATCHDOG0_CTL_R |= WDT_CTL_INTEN;                    // enable interrupts
    WATCHDOG0_LOCK_R = 0x1ACCE551;                       // lock-out further changes
    WATCHDOG0_ICR_R = 0;                                 // clear any pending interrupt
    NVIC_EN0_R |= 1 << (INT_WATCHDOG-16);                // turn-on interrupt 34 (WATCHDOG)
}

void resetWatchdog0()
{

    WATCHDOG0_LOAD_R = (uint32_t)TIMEOUT_MS * 40e3;      // convert into PIOOSC units
}

// Watchdog timer ISR
void watchdogIsr()
{
    setPinValue(RED_LED, 0);
    setPinValue(GREEN_LED, 1);
    // This is the last chance to avoid a reset
    // Write to ICR to avoid a reset
//    WATCHDOG0_ICR_R = 0;
}
