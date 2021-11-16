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

//added heap pointer

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
enum services{YIELD=1, SLEEP=2, WAIT=3, POST=4, SCHED=5,PREEMPT=6,REBOOT=7,PIDOF=8,KILLPID=9,RESTART=10,IPCS=11,PS=12};
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
#define STATE_SUSPENDED     5

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

//enable preemption
bool preemption = true;
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


//Priority scheduler variables
uint8_t prioIndexBylevel[8] = {0};
bool global_roundRobinScheduler = false; //if false use, priority scheduler
uint8_t prio_level = 0; //keep track of current level when returning from task, so every task gets a turn

int priorityScheduler()
{
    //uint8_t prio_level = 0; //keep track of current level when returning from task, so every task gets a turn
    bool found = false;
    uint8_t index = prioIndexBylevel[prio_level];

    while(!found)
    {
        if(tcb[index].priority == prio_level)
        {
            if(tcb[index].state == STATE_UNRUN || tcb[index].state == STATE_READY)
            {
                found = true;
                prioIndexBylevel[prio_level] = index+1; //store next index at that level
                break;
            }
        }

        index++;

        if(index == MAX_TASKS) //if at end of level, go to next level, reset index, store last index of level
        {
            prioIndexBylevel[prio_level] = 0; //start index over in current level
            index = 0;
            prio_level++;
        }

        if(prio_level > 7)
        {
            index = 0;
            prio_level = 0;
        }
    }

    return index;
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
    uint8_t index;
    for(index = 0; index < MAX_TASKS; index++)
    {
        if(tcb[index].pid == fn)
        {
            putsUart0(tcb[index].name);
            putsUart0(" restarted\r\n");
            tcb[index].sp = tcb[index].spInit;
            tcb[index].state = STATE_UNRUN;
            break;

        }
    }

}

// REQUIRED: modify this function to destroy a thread
// REQUIRED: remove any pending semaphore waiting
// NOTE: see notes in class for strategies on whether stack is freed or not
void destroyThread(_fn fn)
{
    uint8_t index;
    for(index = 0; index < MAX_TASKS; index++)
    {
        if( tcb[index].pid == fn) //&& tcb[index].state == STATE_BLOCKED)
        {
            //remove from semaphore only if blocked

            if(tcb[index].state == STATE_BLOCKED) //found that not putting this condition crashes the RTOS
            {
                semaphore * spore = (semaphore *)tcb[index].semaphore;
                uint8_t i;
                for(i = 0; i < spore->queueSize-1; i++)
                {
                  if(spore->processQueue[i] == index)//index is where it was found to be == taskCurrent as well
                  {
                      spore->processQueue[i]= spore->processQueue[i+1];
                      spore->queueSize--;
                      spore->processQueue[i] = 0;
                  }
                  //decrement queque size and semaphore count
                }
            }

            tcb[index].state = STATE_SUSPENDED;

            putsUart0(tcb[index].name);
            putsUart0(" destroyed \r\n");
            break;
        }
    }
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

    if(global_roundRobinScheduler)
    {
        taskCurrent = (uint8_t)rtosScheduler();
    }

    else
    {
        taskCurrent = (uint8_t)priorityScheduler();
    }

    uint32_t * pspPointer = tcb[taskCurrent].sp;
    setPSP(pspPointer); //set sp to the process stack pointer



    //last step
    //set asp bit
    //set SRD() , also it has to be in pendSV(), srds have to be reset when switching tasks
    //mpu on
    //set tmpl bit
    sysTickConfig(ONE_KHZ_CLK);
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
                break;
            }
            else
            {
                tcb[i].ticks--;
            }
        }
    }

    if(preemption == true)
   {
      //set pendSV, every task only gets 1ms to run, if its not done just task switch
      NVIC_INT_CTRL_R |= (1 << 28);
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
    printHexFromUint32((uint32_t)(tcb[taskCurrent].pid) );
    putsUart0("\r\n");
#endif
    //save context
#ifdef DEBUG
    putsUart0("Name of task: ");
    putsUart0(tcb[taskCurrent].name);
    putsUart0("\r\n");
    putsUart0("pendSV PSP before push: ");
    printHexFromUint32((uint32_t)getPSP());
#endif
    //save context
    push_R4_to_R11();


#ifdef DEBUG
    putsUart0("pendSV psp after push: ");
    printHexFromUint32((uint32_t)getPSP());
#endif

    //save current psp
    tcb[taskCurrent].sp = (void *)getPSP();

    if(global_roundRobinScheduler)
    {
        taskCurrent = (uint8_t)rtosScheduler(); // run scheduler for new task to fun
    }

    else
    {
        taskCurrent = (uint8_t)priorityScheduler(); // run scheduler for new task to run
    }

    if(tcb[taskCurrent].state == STATE_READY)
    {
        //restore psp
        setPSP((uint32_t *)tcb[taskCurrent].sp);
        //restore context.
        pop_R4_to_R11();
        //pop by hardware on exit of exception
#ifdef DEBUG
    putsUart0("pendSV psp after pop: ");
    printHexFromUint32((uint32_t)getPSP());
#endif
    }

    else //unrun task
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
    uint32_t *sleep = getPSP();

    uint32_t * sched_RR_PRIO;
    uint32_t * preempt_value;
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
        case SCHED: //round robin or priority sched
            sched_RR_PRIO = getPSP();
            if(*sched_RR_PRIO)
            {
                putsUart0("sched rr on\r\n");
                global_roundRobinScheduler = true;
            }
            else
            {
                putsUart0("sched prio on\r\n");
                global_roundRobinScheduler = false;
            }

            break;
        case PREEMPT: //turn on preemption
            preempt_value = getR0PSP();

            if(*preempt_value)
            {
                putsUart0("preemption on\r\n");
                preemption = true;
            }
            else
            {
                putsUart0("preemption off\r\n");
                preemption = false;
            }
            break;
        case REBOOT: //reboot device
            GREEN_LED = 1;
            NVIC_APINT_R = (0x05FA0000 | NVIC_APINT_SYSRESETREQ);
            break;
        case PIDOF: //process id of
        {
            volatile uint8_t index = 0;
            char * pidname = (char *) *(getPSP()+1);//R1 regiser

            for(index; index < MAX_TASKS; index++)
            {
                if (str_cmp(tcb[index].name,pidname) == 0)
                {
                    printHexFromUint32((uint32_t)tcb[index].pid);
                    break;
                }
            }
        }
            break;
        case KILLPID: //kill process id
        {

            const char * pidname = (char *) *(getPSP());//R0 register
            uint32_t pid = hexToUint32(pidname);

            volatile uint8_t index = 0;
            for(index; index < MAX_TASKS; index++)
            {
                if (tcb[index].pid ==(void *)pid)
                {
                    destroyThread((_fn)tcb[index].pid);
                }
            }
        }
            break;

        case RESTART:
        {
            volatile uint8_t index = 0;
            char * pidname = (char *) *(getPSP()+1);//R0 regiser
            for(index; index < MAX_TASKS; index++)
            {
                if (str_cmp(tcb[index].name,pidname) == 0 && tcb[index].state == STATE_SUSPENDED)
                {
                    restartThread((_fn)tcb[index].pid);
                }
            }

            break;
        }

        case IPCS:

            break;
        case PS:

            break;



    }
}

// REQUIRED: code this function
void mpuFaultIsr()
{

    putsUart0("MPU Fault in process ");
    printHexFromUint32( ((uint32_t) tcb[taskCurrent].pid+'0') );
    putsUart0("\r\n");
    uint32_t * psp_pointer = getPSP(); //points to top of stack === R0;
    uint32_t * msp_pointer = getMSP();
    putsUart0("PSP: ");
    printHexFromUint32(*psp_pointer);
    putsUart0("MSP: ");
    printHexFromUint32(*msp_pointer);

    //access mem fault register. byte access, instead of word access. (note to self: change this to uint32_t if it does not work with byte access.)

    volatile uint8_t * ptr_memfault = (uint8_t *)(NVIC_FAULT_STAT_R); //page.177 for memfault register

    putsUart0("MemFault(pg.177): ");
    printHexFromUint32(*ptr_memfault);

    putsUart0("R0: ");
    printHexFromUint32(*psp_pointer);

    putsUart0("R1: ");
    printHexFromUint32(*(psp_pointer+1));

    putsUart0("R2: ");
    printHexFromUint32(*(psp_pointer+2));

    putsUart0("R3: ");
    printHexFromUint32(*(psp_pointer+3));

    putsUart0("R12: ");
    printHexFromUint32(*(psp_pointer+4));

    putsUart0("LR: ");
    printHexFromUint32(*(psp_pointer+5));

    putsUart0("PC: ");
    printHexFromUint32(*(psp_pointer+6));

    putsUart0("xPSR: ");
    printHexFromUint32(*(psp_pointer+7));

    volatile uint32_t * MemMgmntFaultAddr = (uint32_t *)(NVIC_MM_ADDR_R); //pg 177.
    volatile uint32_t * BusFaultAddr = (uint32_t * )(NVIC_FAULT_ADDR_R);

    putsUart0("\r\nData Address: ");
    printHexFromUint32(*MemMgmntFaultAddr); //print true faulting data adddress
    putsUart0("Instruction Address: ");
    printHexFromUint32(*BusFaultAddr); //print true faulting instruction address
    putsUart0("\r\n");

    //clear mpu fault pending bit and trigger a pendsv isr

    //clear mpu fault pending bit
    volatile uint32_t * ptr_MPUCLEAR = (uint32_t * )(NVIC_SYS_HND_CTRL_R); //pg 172
    *ptr_MPUCLEAR &= ~(1 << 13); //should clear MPU pending bits

    //trigger pendSV
    volatile uint32_t *pend_sv_ptr = (uint32_t *)(NVIC_INT_CTRL_R);
    *pend_sv_ptr |= (1 << 28); //this will set the bit to pending. will then cause a pendSV exception.
}

// REQUIRED: code this function
void hardFaultIsr()
{
/* provide the value of the PSP, MSP, and hard fault
    flags (in hex) */

    putsUart0("Hard Fault in process ");
    printHexFromUint32( ((uint32_t) tcb[taskCurrent].pid+'0') );
    putsUart0("\r\n");

    putsUart0("PSP: ");
    printHexFromUint32((uint32_t)getPSP());
    putsUart0("MSP: ");
    printHexFromUint32((uint32_t)getMSP());

    putsUart0("HardFault Flags: ");
    printHexFromUint32(NVIC_HFAULT_STAT_R);     //page.183 for hardfault register

    putsUart0("\r\n Stuck in HardFault loop \r\n");
    while(1)
    {
    }
}

// REQUIRED: code this function
void busFaultIsr()
{
    putsUart0("Bus Fault in process: ");
    printHexFromUint32((uint32_t) (tcb[taskCurrent].pid) );
}

// REQUIRED: code this function
void usageFaultIsr()
{

    putsUart0(tcb[taskCurrent].name);
    putsUart0(" \r\nUsage Fault in process: ");
    printHexFromUint32((uint32_t) (tcb[taskCurrent].pid) );
    putsUart0("\n\r");

    uint16_t * usagefault = ((uint16_t *)(0xE000ED2A)); //Offset to usage fault reg
    putsUart0("fault flags: ");
    printHexFromUint32(*usagefault);
    putsUart0("\r\n fault psp: ");
    printHexFromUint32((uint32_t )getPSP());

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
        parseFields(&data); //parse uart input data

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
            char * pidname = getFieldString (&data, 1);
            sys_kill(pidname);
        }

        else if(isCommand(&data, "pi", 1))
        {
            //priority inheritance on or off
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
            char * name = getFieldString (&data, 1);
            uint32_t *pid = NULL;
            sys_pidof(pid, name);
        }

        else if(isCommand(&data, "run", 1))
        {
            char * name = getFieldString (&data, 1);
            sys_select(name);
        }

//        else
//        {
//            //get second arg to see if its an argument
//            uint8_t args = data.fieldCount-1;
//
//            if(args == 1)
//            {
//                char * pid = getFieldString (&data, 1);
//                char * name = getFieldString (&data, 0);
//                if(*pid == '&')
//                {
//                    //run program at argument string 0
//                    sys_select(name); //for now it will turn on RED
//                }
//            }
//        }
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
    ok =  createThread(idle, "Idle", 6, 1024);
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
