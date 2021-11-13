//*****************************************************************************
//
// Startup code for use with TI's Code Composer Studio.
//
// Copyright (c) 2011-2014 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Software License Agreement
//
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
//*****************************************************************************

#include <stdint.h>
#include "uart0.h"
#include "helperfunctions.h"
//*****************************************************************************
//
// Forward declaration of the default fault handlers.
//
//*****************************************************************************


void ResetISR(void);
static void NmiSR(void);
static void HardFaultISR(void);
static void IntDefaultHandler(void);



//RTOS Fall 2021 PROJECT ADDONS
//find in fault_main.s file
extern uint32_t * getPSP(void); //process stack pointer
extern uint32_t * getMSP(void); //main stack pointer value
//extern uint32_t * getHardFaultFlags(void); //pg. 183 of datasheet

//find in rtos.c
extern void pendSvIsr();
extern void usageFaultIsr();
extern void svCallIsr();
extern void hardFaultIsr();
extern void busFaultIsr();
extern void systickIsr();
extern void mpuFaultIsr();
//*****************************REGISTER DEFINITIONS
//memory management mpu stuff
#define FAULTSTAT_REG       0xE000ED28U //contains MFAULTSTAT, BFAULTSTAT, UFAULTSTAT
#define MEM_MGNT_REG        0xE000ED34U //true faulting address
#define BUS_FAULT_REG       0xE000ED38U //true faulting address
//hardfault
#define HARD_FAULT_STAT_REG 0xE000ED2CU

//exceptions priority and pending stuff.
#define INTCTRL_REG         0xE000ED04U //will be used to change pendSV status to pending which will cause the exception to be called.
#define SYS_HANDLER_CTRL    0xE000ED24U //pg 173. controls context switching, pending exceptions, etc.

//*****************************SETTING BITS DEFINITIONS

#define PENDSV                  (1 << 28) //pg. 160 INTCLR_REG, will cause a PENDSV exception
#define UNPEND_SV               (1 << 27) //INTCLR_REG, will clear a PENDSV exception
#define MEM_MGNT_FAULT_CLEAR    (1 << 13) //SYS_HANDLER_CTR, will clear an MPU fault

//will be used later in place of PID #?
#define VECPEND_MASK            0x000FF000 //INTCLR_REG, contains exceptions number of the highest priority pending enabled exception


uint8_t pid = 1;

//static void HardFaultISR(uint32_t pid){};
/*
static void MPUFaultISRx()
{
    putsUart0("MPU Fault in process ");
    putcUart0( (pid+'0') );
    putsUart0("\r\n");
    uint32_t * psp_pointer = getPSP(); //points to top of stack === R0;
    uint32_t * msp_pointer = getMSP();
    putsUart0("PSP: ");
    printHex(*psp_pointer);
    putsUart0("MSP: ");
    printHex(*msp_pointer);

    //access mem fault register. byte access, instead of word access. (note to self: change this to uint32_t if it does not work with byte access.)

    volatile uint8_t * ptr_memfault = (uint8_t *)(NVIC_FAULT_STAT_R); //page.177 for memfault register

    putsUart0("MemFault(pg.177): ");
    printHex(*ptr_memfault);

    putsUart0("R0: ");
    printHex(*psp_pointer);

    putsUart0("R1: ");
    printHex(*(psp_pointer+1));

    putsUart0("R2: ");
    printHex(*(psp_pointer+2));

    putsUart0("R3: ");
    printHex(*(psp_pointer+3));

    putsUart0("R12: ");
    printHex(*(psp_pointer+4));

    putsUart0("LR: ");
    printHex(*(psp_pointer+5));

    putsUart0("PC: ");
    printHex(*(psp_pointer+6));

    putsUart0("xPSR: ");
    printHex(*(psp_pointer+7));

    volatile uint32_t * MemMgmntFaultAddr = (uint32_t *)(NVIC_MM_ADDR_R); //pg 177.
    volatile uint32_t * BusFaultAddr = (uint32_t * )(NVIC_FAULT_ADDR_R);

    putsUart0("\r\nData Address: ");
    printHex(*MemMgmntFaultAddr); //print true faulting data adddress
    putsUart0("Instruction Address: ");
    printHex(*BusFaultAddr); //print true faulting instruction address
    putsUart0("\r\n");

    //clear mpu fault pending bit and trigger a pendsv isr

    //clear mpu fault pending bit
    volatile uint32_t * ptr_MPUCLEAR = (uint32_t * )(NVIC_SYS_HND_CTRL_R); //pg 172
    *ptr_MPUCLEAR &= ~(1 << 13); //should clear MPU pending bits

    //trigger pendSV
    volatile uint32_t *pend_sv_ptr = (uint32_t *)(NVIC_INT_CTRL_R);
    *pend_sv_ptr |= (1 << 28); //this will set the bit to pending. will then cause a pendSV exception.
};

*/

static void
IntDefaultHandler(void)
{
    //
    // Go into an infinite loop.
    //
    putsUart0("Default handler");
    while(1)
    {
    }
}



//*****************************************************************************
//
// External declaration for the reset handler that is to be called when the
// processor is started
//
//*****************************************************************************
extern void _c_int00(void);

//*****************************************************************************
//
// Linker variable that marks the top of the stack.
//
//*****************************************************************************
extern uint32_t __STACK_TOP;

//*****************************************************************************
//
// External declarations for the interrupt handlers used by the application.
//
//*****************************************************************************
// To be added by user

//*****************************************************************************
//
// The vector table.  Note that the proper constructs must be placed on this to
// ensure that it ends up at physical address 0x0000.0000 or at the start of
// the program if located at a start address other than 0.
//
//*****************************************************************************
#pragma DATA_SECTION(g_pfnVectors, ".intvecs")
void (* const g_pfnVectors[])(void) =
{
    (void (*)(void))((uint32_t)&__STACK_TOP),
                                            // The initial stack pointer
    ResetISR,                               // The reset handler
    NmiSR,                                  // The NMI handler
    hardFaultIsr,                               // The hard fault handler
    mpuFaultIsr,                            // The MPU fault handler
    busFaultIsr,                             // The bus fault handler
    usageFaultIsr,                            // The usage fault handler
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    svCallIsr,                      // SVCall handler
    IntDefaultHandler,                      // Debug monitor handler
    0,                                      // Reserved
    pendSvIsr,                         // The PendSV handler
    systickIsr,                      // The SysTick handler
    IntDefaultHandler,                      // GPIO Port A
    IntDefaultHandler,                      // GPIO Port B
    IntDefaultHandler,                      // GPIO Port C
    IntDefaultHandler,                      // GPIO Port D
    IntDefaultHandler,                      // GPIO Port E
    IntDefaultHandler,                      // UART0 Rx and Tx
    IntDefaultHandler,                      // UART1 Rx and Tx
    IntDefaultHandler,                      // SSI0 Rx and Tx
    IntDefaultHandler,                      // I2C0 Master and Slave
    IntDefaultHandler,                      // PWM Fault
    IntDefaultHandler,                      // PWM Generator 0
    IntDefaultHandler,                      // PWM Generator 1
    IntDefaultHandler,                      // PWM Generator 2
    IntDefaultHandler,                      // Quadrature Encoder 0
    IntDefaultHandler,                      // ADC Sequence 0
    IntDefaultHandler,                      // ADC Sequence 1
    IntDefaultHandler,                      // ADC Sequence 2
    IntDefaultHandler,                      // ADC Sequence 3
    IntDefaultHandler,                      // Watchdog timer
    IntDefaultHandler,                      // Timer 0 subtimer A
    IntDefaultHandler,                      // Timer 0 subtimer B
    IntDefaultHandler,                      // Timer 1 subtimer A
    IntDefaultHandler,                      // Timer 1 subtimer B
    IntDefaultHandler,                      // Timer 2 subtimer A
    IntDefaultHandler,                      // Timer 2 subtimer B
    IntDefaultHandler,                      // Analog Comparator 0
    IntDefaultHandler,                      // Analog Comparator 1
    IntDefaultHandler,                      // Analog Comparator 2
    IntDefaultHandler,                      // System Control (PLL, OSC, BO)
    IntDefaultHandler,                      // FLASH Control
    IntDefaultHandler,                      // GPIO Port F
    IntDefaultHandler,                      // GPIO Port G
    IntDefaultHandler,                      // GPIO Port H
    IntDefaultHandler,                      // UART2 Rx and Tx
    IntDefaultHandler,                      // SSI1 Rx and Tx
    IntDefaultHandler,                      // Timer 3 subtimer A
    IntDefaultHandler,                      // Timer 3 subtimer B
    IntDefaultHandler,                      // I2C1 Master and Slave
    IntDefaultHandler,                      // Quadrature Encoder 1
    IntDefaultHandler,                      // CAN0
    IntDefaultHandler,                      // CAN1
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // Hibernate
    IntDefaultHandler,                      // USB0
    IntDefaultHandler,                      // PWM Generator 3
    IntDefaultHandler,                      // uDMA Software Transfer
    IntDefaultHandler,                      // uDMA Error
    IntDefaultHandler,                      // ADC1 Sequence 0
    IntDefaultHandler,                      // ADC1 Sequence 1
    IntDefaultHandler,                      // ADC1 Sequence 2
    IntDefaultHandler,                      // ADC1 Sequence 3
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // GPIO Port J
    IntDefaultHandler,                      // GPIO Port K
    IntDefaultHandler,                      // GPIO Port L
    IntDefaultHandler,                      // SSI2 Rx and Tx
    IntDefaultHandler,                      // SSI3 Rx and Tx
    IntDefaultHandler,                      // UART3 Rx and Tx
    IntDefaultHandler,                      // UART4 Rx and Tx
    IntDefaultHandler,                      // UART5 Rx and Tx
    IntDefaultHandler,                      // UART6 Rx and Tx
    IntDefaultHandler,                      // UART7 Rx and Tx
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // I2C2 Master and Slave
    IntDefaultHandler,                      // I2C3 Master and Slave
    IntDefaultHandler,                      // Timer 4 subtimer A
    IntDefaultHandler,                      // Timer 4 subtimer B
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // Timer 5 subtimer A
    IntDefaultHandler,                      // Timer 5 subtimer B
    IntDefaultHandler,                      // Wide Timer 0 subtimer A
    IntDefaultHandler,                      // Wide Timer 0 subtimer B
    IntDefaultHandler,                      // Wide Timer 1 subtimer A
    IntDefaultHandler,                      // Wide Timer 1 subtimer B
    IntDefaultHandler,                      // Wide Timer 2 subtimer A
    IntDefaultHandler,                      // Wide Timer 2 subtimer B
    IntDefaultHandler,                      // Wide Timer 3 subtimer A
    IntDefaultHandler,                      // Wide Timer 3 subtimer B
    IntDefaultHandler,                      // Wide Timer 4 subtimer A
    IntDefaultHandler,                      // Wide Timer 4 subtimer B
    IntDefaultHandler,                      // Wide Timer 5 subtimer A
    IntDefaultHandler,                      // Wide Timer 5 subtimer B
    IntDefaultHandler,                      // FPU
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // I2C4 Master and Slave
    IntDefaultHandler,                      // I2C5 Master and Slave
    IntDefaultHandler,                      // GPIO Port M
    IntDefaultHandler,                      // GPIO Port N
    IntDefaultHandler,                      // Quadrature Encoder 2
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // GPIO Port P (Summary or P0)
    IntDefaultHandler,                      // GPIO Port P1
    IntDefaultHandler,                      // GPIO Port P2
    IntDefaultHandler,                      // GPIO Port P3
    IntDefaultHandler,                      // GPIO Port P4
    IntDefaultHandler,                      // GPIO Port P5
    IntDefaultHandler,                      // GPIO Port P6
    IntDefaultHandler,                      // GPIO Port P7
    IntDefaultHandler,                      // GPIO Port Q (Summary or Q0)
    IntDefaultHandler,                      // GPIO Port Q1
    IntDefaultHandler,                      // GPIO Port Q2
    IntDefaultHandler,                      // GPIO Port Q3
    IntDefaultHandler,                      // GPIO Port Q4
    IntDefaultHandler,                      // GPIO Port Q5
    IntDefaultHandler,                      // GPIO Port Q6
    IntDefaultHandler,                      // GPIO Port Q7
    IntDefaultHandler,                      // GPIO Port R
    IntDefaultHandler,                      // GPIO Port S
    IntDefaultHandler,                      // PWM 1 Generator 0
    IntDefaultHandler,                      // PWM 1 Generator 1
    IntDefaultHandler,                      // PWM 1 Generator 2
    IntDefaultHandler,                      // PWM 1 Generator 3
    IntDefaultHandler                       // PWM 1 Fault
};

//*****************************************************************************
//
// This is the code that gets called when the processor first starts execution
// following a reset event.  Only the absolutely necessary set is performed,
// after which the application supplied entry() routine is called.  Any fancy
// actions (such as making decisions based on the reset cause register, and
// resetting the bits in that register) are left solely in the hands of the
// application.
//
//*****************************************************************************
void
ResetISR(void)
{
    //
    // Jump to the CCS C initialization routine.  This will enable the
    // floating-point unit as well, so that does not need to be done here.
    //
    __asm("    .global _c_int00\n"
          "    b.w     _c_int00");
}

//*****************************************************************************
//
// This is the code that gets called when the processor receives a NMI.  This
// simply enters an infinite loop, preserving the system state for examination
// by a debugger.
//
//*****************************************************************************
static void
NmiSR(void) //non-maskable interrupt
{
    //
    // Enter an infinite loop.
    //
    while(1)
    {
    }
}

//*****************************************************************************
//
// This is the code that gets called when the processor receives a fault
// interrupt.  This simply enters an infinite loop, preserving the system state
// for examination by a debugger.
//
//*****************************************************************************

//<copy pasta on top>

//*****************************************************************************
//
// This is the code that gets called when the processor receives an unexpected
// interrupt.  This simply enters an infinite loop, preserving the system state
// for examination by a debugger.
//
//*****************************************************************************

// <copy pasta on top>
