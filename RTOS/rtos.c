// RTOS Framework - Fall 2021
// J Losh

// Student Name:
// TO DO: Add your name on this line.  Do not include your ID number in the file.

// Please do not change any function name in this code or the thread priorities

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// 6 Pushbuttons and 5 LEDs, UART
// LEDs on these pins:
// Blue:   PF2 (on-board)
// Red:    PA2
// Orange: PA3
// Yellow: PA4
// Green:  PE0
// PBs on these pins
// PB0:    PC4
// PB1:    PC5
// PB2:    PC6
// PB3:    PC7
// PB4:    PD6
// PB5:    PD7
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1
// Memory Protection Unit (MPU):
// Regions to allow 32 1kiB SRAM access (RW or none)
// Region to allow peripheral access (RW)
// Region to allow flash access (XR)

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "helperfunctions.h"
#include "shell_API.h"
#include "syscalls.h"
#include "clock.h"
#include "gpio.h"
#include "uart0.h"
#include "wait.h"
#include "systick.h"

#define DEBUG
#undef DEBUG

//Added bitbands for button presses
#define BUTTON0       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 4*4))) //PC4
#define BUTTON1       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 5*4))) //PC4
#define BUTTON2       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4))) //PC5
#define BUTTON3       (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 7*4))) //PC6
#define BUTTON4       (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 6*4))) //PD6
#define BUTTON5       (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 7*4))) //PD7

// REQUIRED: correct these bit-banding references for the off-board LEDs
#define BLUE_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4))) // on-board blue LED,    PF2
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 2*4))) // off-board red LED,    PA2
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 0*4))) // off-board green LED,  PE0
#define YELLOW_LED   (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 4*4))) // off-board yellow LED, PA4
#define ORANGE_LED   (*((volatile uint32_t *)(0x42000000 + (0x400043FC-0x40000000)*32 + 3*4))) // off-board orange LED, PA3

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

//added head pointer

uint32_t * heap = (uint32_t *)(0x20001800); //6KiB for OS


//custom prototypes & defines
void strcpyChar(char * dest, const char * src);
uint32_t calculateSRD(uint32_t StackSize);
void setPendSV();
//find in fault_main.s file
extern void setPSP(uint32_t *StackPointer);
uint32_t * getPSP(void);
uint32_t * getMSP(void);
extern void setPrivilege(); //turn the ASP bit on
extern void push_R4_to_R11();
extern void pop_R4_to_R11();
extern void pushPSP(uint32_t value);
extern uint32_t * getR0PSP();
extern uint32_t * getProgramCounter();

//custom globals
enum services{YIELD=1, SLEEP=2, WAIT=3, POST=4};
uint16_t global_SizeOfQueue = 0;
// function pointer
typedef void (*_fn)();

// semaphore
#define MAX_SEMAPHORES 5
#define MAX_QUEUE_SIZE 5

typedef struct _semaphore
{
    uint16_t count;
    uint16_t queueSize;
    uint32_t processQueue[MAX_QUEUE_SIZE]; // store task index here
} semaphore;

semaphore semaphores[MAX_SEMAPHORES];
#define keyPressed 1
#define keyReleased 2
#define flashReq 3
#define resource 4

// task
#define STATE_INVALID    0 // no task
#define STATE_UNRUN      1 // task has never been run
#define STATE_READY      2 // has run, can resume at any time
#define STATE_DELAYED    3 // has run, but now awaiting timer
#define STATE_BLOCKED    4 // has run, but now blocked by semaphore

#define MAX_TASKS 12       // maximum number of valid tasks
uint8_t taskCurrent = 0;   // index of last dispatched task
uint8_t taskCount = 0;     // total number of valid tasks

// REQUIRED: add store and management for the memory used by the thread stacks
//           thread stacks must start on 1 kiB boundaries so mpu can work correctly

struct _tcb
{
    uint8_t state;                 // see STATE_ values above
    void *pid;                     // used to uniquely identify thread
    void *spInit;                  // original top of stack
    void *sp;                      // current stack pointer
    int8_t priority;               // 0=highest to 15=lowest
    uint32_t ticks;                // ticks until sleep complete
    uint32_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    void *semaphore;               // pointer to the semaphore that is blocking the thread

} tcb[MAX_TASKS];

//-----------------------------------------------------------------------------
// RTOS Kernel Functions
//-----------------------------------------------------------------------------

// REQUIRED: initialize systick for 1ms system timer
void initRtos()
{
    uint8_t i;
    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++)
    {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
    }

    sysTickConfig(ONE_KHZ_CLK);
}

// REQUIRED: Implement prioritization to 8 levels
int rtosScheduler()
{
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    while (!ok)
    {
        task++;
        if (task >= MAX_TASKS)
            task = 0;
        ok = (tcb[task].state == STATE_READY || tcb[task].state == STATE_UNRUN);
    }
    return task;
}

bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes)
{
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    // REQUIRED:
    // store the thread name
    // allocate stack space and store top of stack in sp and spInit
    // add task if room in task list
    if (taskCount < MAX_TASKS)
    {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS))
        {
            found = (tcb[i++].pid ==  fn);
        }
        if (!found)
        {
            // find first available tcb record
            i = 0;
            uint32_t sp = (uint32_t)heap;
            while (tcb[i].state != STATE_INVALID)
            {
                i++;
            }

            if(i != 0)
            {
                sp = (uint32_t)tcb[i-1].spInit + stackBytes; //use previous stack size to know what to allocate next
            }
            else { sp = sp + stackBytes; }

            strcpyChar(tcb[i].name, name);
            tcb[i].state = STATE_UNRUN;
            tcb[i].pid = fn;         //used to identify which program to run
            tcb[i].sp = (void *) sp;
            tcb[i].spInit = (void *) sp; //stack pointer
            tcb[i].priority = priority;
            tcb[i].srd = stackBytes/1024; //add later tomorrow
            // increment task count
            taskCount++;
            ok = true;
        }
    }
    // REQUIRED: allow tasks switches again
    return ok;
}

// REQUIRED: modify this function to restart a thread
void restartThread(_fn fn)
{

}

// REQUIRED: modify this function to destroy a thread
// REQUIRED: remove any pending semaphore waiting
// NOTE: see notes in class for strategies on whether stack is freed or not
void destroyThread(_fn fn)
{

}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority)
{
}

bool createSemaphore(uint8_t semaphore, uint8_t count)
{
    bool ok = (semaphore < MAX_SEMAPHORES);
    {
        semaphores[semaphore].count = count;
    }
    return ok;
}

// REQUIRED: modify this function to start the operating system
// by calling scheduler, setting PSP, ASP bit, and PC
void startRtos()
{//reminder: local static variable(will not change after changing stack pointer)

    //global variable
    taskCurrent = (uint8_t)rtosScheduler(); //returns available task to run, also task
    uint32_t * pspPointer = tcb[taskCurrent].sp;
    setPSP(pspPointer); //set sp to the process stack pointer
    setPrivilege(); //removes Privileges

    tcb[taskCurrent].state = STATE_READY;
    _fn funcptr_run = (_fn)tcb[taskCurrent].pid; //pid = task being run
    //runs first task
    funcptr_run();
}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield()
{

    __asm(" SVC #1");
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick)
{
    __asm(" SVC #2");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore)
{
    __asm(" SVC #3");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore)
{
    __asm(" SVC #4");
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void systickIsr()
{
    volatile uint8_t i = 0;
    for(i; i < taskCount; i++)
    {
        if(tcb[i].state == STATE_DELAYED)
        {
            if(tcb[i].ticks == 0)
            {
                tcb[i].state = STATE_READY;
                return;
            }
            else
            {
                tcb[i].ticks--;
            }
        }
    }
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
void pendSvIsr()
{    //used for context switching between tasks
    //check pg. 177 for DERR and IERR
    //if DERR and IERR is set print out called from MPU

    if( (NVIC_FAULT_STAT_R & (0x2) ))// ox3 because DERR and IERR are first two bits.
    {
        putsUart0("Called from MPU, Data access violation\r\n");
    }

    if( (NVIC_FAULT_STAT_R & (0x1) ))// ox3 because DERR and IERR are first two bits.
    {
        putsUart0("Called from MPU, Instruction access violation\r\n");
    }

#ifdef DEBUG
    putsUart0("pendSV in process: ");
    printHex((uint32_t)(tcb[taskCurrent].pid) );
    putsUart0("\r\n");
#endif
    //save context
#ifdef DEBUG
    putsUart0("Name of task: ");
    putsUart0(tcb[taskCurrent].name);
    putsUart0("\r\n");
    putsUart0("pendSV PSP before push: ");
    printHex((uint32_t)getPSP());
#endif
    //save context
    push_R4_to_R11();


#ifdef DEBUG
    putsUart0("pendSV psp after push: ");
    printHex((uint32_t)getPSP());
#endif

    //save current psp
    tcb[taskCurrent].sp = (void *)getPSP();

    taskCurrent = (uint8_t)rtosScheduler(); // run scheduler for new task to fun

    if(tcb[taskCurrent].state == STATE_READY)
    {
        //restore psp
        setPSP((uint32_t *)tcb[taskCurrent].sp);
        //restore context.
        pop_R4_to_R11();
        //pop by hardware on exit of exception
#ifdef DEBUG
    putsUart0("pendSV psp after pop: ");
    printHex((uint32_t)getPSP());
#endif
    }

    else //unrun
    {
        //change psp to new task
        setPSP((uint32_t *)tcb[taskCurrent].spInit);

        //push values of xpsr-r0
        pushPSP((uint32_t)0x01000000);              //expsr 0x01000000 because THUMB bit should always be set according to docs
        pushPSP((uint32_t)tcb[taskCurrent].pid); //pc
        pushPSP((uint32_t)0xFFFFFFFD); //LR holds EXEC_RETURN value. check pg.111 of docs.

        //r12 -r0 holds random values
        pushPSP((uint32_t)0x0);
        //r3
        pushPSP((uint32_t)0x0);
        //r2
        pushPSP((uint32_t)0x0);
        //r1
        pushPSP((uint32_t)0x0);
        //r0
        pushPSP((uint32_t)0x0);

        //at the end mark as ready
        tcb[taskCurrent].state = STATE_READY;

    }
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr()
{

#ifdef DEBUG
    putsUart0("Called from SVCIsr-- \r\n");
        //trigger pendSV
#endif

    //get sleep value
    uint32_t *sleep = getR0PSP();

    //extract svc number from psp stack
    uint32_t *SVCnAddr = getProgramCounter(); //*svcnAddr holds address to pc after SVC call
    uint8_t* svc_num = (uint8_t*)(*SVCnAddr);
    svc_num -=2;

    uint32_t index = *sleep;

    switch(*svc_num)
    {
        case YIELD:
            //setPendSV(); //this will set the bit to pending. will then cause a pendSV exception.
            NVIC_INT_CTRL_R |= (1 << 28);
            break;

        case SLEEP:
            //record time
            tcb[taskCurrent].ticks = *sleep;
            //set state to delay
            tcb[taskCurrent].state = STATE_DELAYED;
            //set pendSV
            NVIC_INT_CTRL_R |= (1 << 28);
            break;

        case WAIT: //reminder index variable is a reference to the semaphore, it may be key_pressed, key_release, resource, etc...
            if(semaphores[index].count > 0)
            {
                semaphores[index].count--;
            }
            else
            {
                uint16_t queueCount = semaphores[index].queueSize++;
                //add process to semaphore i queue, increment queue count
                semaphores[index].processQueue[queueCount] = taskCurrent;

                //record semaphore i in tcb.
                tcb[taskCurrent].semaphore = (void *)(semaphores + index);
                //set state to blocked
                tcb[taskCurrent].state = STATE_BLOCKED;
                //set pendSV
                NVIC_INT_CTRL_R |= (1 << 28);
            }
            break;

        case POST:
            //increment semaphore count
            semaphores[index].count++;
            //if it was zero and is now one, then someone has been waiting.
            if(semaphores[index].count == 1)
            {
                if(semaphores[index].queueSize > 0)
                {
                    //make next task in list to ready state
                    tcb[semaphores[index].processQueue[0]].state = STATE_READY;
                    semaphores[index].count--;

                    //shift queue for next available task = deleting from queue
                    uint8_t i = 0;
                    for(i = 0; i < semaphores[index].queueSize - 1; i++)
                    {
                        semaphores[index].processQueue[i] = semaphores[index].processQueue[i+1];
                    }
                    //decrement queque size and semaphore count
                    semaphores[index].queueSize--;
                }
            }
            break;
    }
}

// REQUIRED: code this function
void mpuFaultIsr()
{

}

// REQUIRED: code this function
void hardFaultIsr()
{

/* provide the value of the PSP, MSP, and hard fault
    flags (in hex)     */

    putsUart0("Hard Fault in process ");
    printHex( ((uint32_t) tcb[taskCurrent].pid+'0') );
    putsUart0("\r\n");

    putsUart0("PSP: ");
    printHex((uint32_t)getPSP());
    putsUart0("MSP: ");
    printHex((uint32_t)getMSP());

    putsUart0("HardFault Flags: ");
    printHex(NVIC_HFAULT_STAT_R);     //page.183 for hardfault register

    putsUart0("\r\n Stuck in HardFault loop \r\n");
    while(1)
    {
    }
}

// REQUIRED: code this function
void busFaultIsr()
{
    putsUart0("Bus Fault in process: ");
    printHex((uint32_t) (tcb[taskCurrent].pid) );
}

// REQUIRED: code this function
void usageFaultIsr()
{

    putsUart0(tcb[taskCurrent].name);
    putsUart0(" \r\nUsage Fault in process: ");
    printHex((uint32_t) (tcb[taskCurrent].pid) );
    putsUart0("\n\r");

    uint16_t * usagefault = ((uint16_t *)(0xE000ED2A)); //Offset to usage fault reg
    putsUart0("fault flags: ");
    printHex(*usagefault);
    putsUart0("\r\n fault psp: ");
    printHex((uint32_t )getPSP());

    while(1);
}

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
//           6 pushbuttons
void initHw()
{
    //initialize clock
    initSystemClockTo40Mhz();
    //init systick
    //sysTickConfig(ONE_KHZ_CLK);

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

// REQUIRED: add code to return a value from 0-63 indicating which of 6 PBs are pressed
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

//-----------------------------------------------------------------------------
// YOUR UNIQUE CODE
// REQUIRED: add any custom code in this space
//-----------------------------------------------------------------------------

uint32_t calculateSRD(uint32_t StackSize)
{
    StackSize = ((StackSize)/1024);
    return StackSize;
}

void strcpyChar(char * dest, const char * src)
{
    uint8_t i = 0;
    while(src[i] != '\0')
    {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

void setPendSV()
{

}

// ------------------------------------------------------------------------------
//  Task functions
// ------------------------------------------------------------------------------

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle()
{
    while(true)
    {
        ORANGE_LED = 1;
        waitMicrosecond(1000);
        ORANGE_LED = 0;
        yield();
    }
}

void idle2()
{
    while(true)
    {
        BLUE_LED = 1;
        waitMicrosecond(1000);
        ORANGE_LED = 0;
        yield();
    }
}


void flash4Hz()
{
    while(true)
    {
        GREEN_LED ^= 1;
        sleep(125);
    }
}

void oneshot()
{
    while(true)
    {
        wait(flashReq);
        YELLOW_LED = 1;
        sleep(1000);
        YELLOW_LED = 0;
    }
}


void partOfLengthyFn()
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn()
{
    uint16_t i;
    while(true)
    {
        wait(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
        }
        RED_LED ^= 1;
        post(resource);
    }
}

void readKeys()
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            YELLOW_LED ^= 1;
            RED_LED = 1;
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            RED_LED = 0;
        }
        if ((buttons & 4) != 0)
        {
            restartThread(flash4Hz);
        }
        if ((buttons & 8) != 0)
        {
            destroyThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce()
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--;
            else
                count = 10;
        }
        post(keyReleased);
    }
}

void uncooperative()
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant()
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important()
{
    while(true)
    {
        wait(resource);
        BLUE_LED = 1;
        sleep(1000);
        BLUE_LED = 0;
        post(resource);
    }
}

// REQUIRED: add processing for the shell commands through the UART here
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

        yield();
    }
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    bool ok;

    // Initialize hardware
    initHw();
    initUart0();
    initRtos();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    // Power-up flash
    GREEN_LED = 1;
    waitMicrosecond(250000);
    GREEN_LED = 0;
    waitMicrosecond(250000);


    // Initialize semaphores
    createSemaphore(keyPressed, 1);
    createSemaphore(keyReleased, 0);
    createSemaphore(flashReq, 5);
    createSemaphore(resource, 1);

    // Add required idle process at lowest priority
    ok =  createThread(idle, "Idle", 7, 1024);

    ok &= createThread(lengthyFn, "LengthyFn", 6, 1024);
    ok &= createThread(flash4Hz, "Flash4Hz", 4, 1024);
    ok &= createThread(oneshot, "OneShot", 2, 1024);
    ok &= createThread(readKeys, "ReadKeys", 6, 1024);
    ok &= createThread(debounce, "Debounce", 6, 1024);
    ok &= createThread(important, "Important", 0, 1024);
    ok &= createThread(uncooperative, "Uncoop", 6, 1024);
    ok &= createThread(errant, "Errant", 6, 1024);
    ok &= createThread(shell, "Shell", 6, 2048);



    //enable faults for debugging
    mpu_memfault_enable();
    mpu_busfault_enable();
    mpu_usagefault_enable();

    // Start up RTOS
    if (ok)
        startRtos(); // never returns
    else
        RED_LED = 1;

    return 0;
}
