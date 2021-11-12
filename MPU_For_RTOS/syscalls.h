#ifndef __SYS_CALLS__
#define __SYS_CALLS__

#include <stdint.h>

//***************** pg. 192 of datasheet. MPUATTR Reg Flags. AP = Access privilege
#define AP_PRIV_ACCESS              (1 << 24) //only priveleged software can rw
#define AP_FULL_ACCESS              (3 << 24) //all software can rw

#define FLASH_TXSCB_ENCODING        (1 << 17) //flash protection
#define SRAM_TXSCB_ENCODING         (3 << 17) //internal sram
#define PERIPHERALS_TXSCB_ENCODING  (5 << 16) //peripherals

#define MPU_REGION_ENABLE           (1)
#define BASE_ADDR_VALID_ON          (1 << 4 )
#define MPU_MEM_FAULT_ON            (1 << 16)
#define MPU_BUS_FAULT_ON            (1 << 17)
#define MPU_USAGE_FAULT_ON          (1 << 18)

#define FLASH_N_SIZE                18 //2^n 256k
#define SRAM_N_SIZE                 29 //2^n 32k   (include bit-banded regions and bit-band alias regions) size: 0x1FFFFFFF
#define SRAM8KiB_N_SIZE             13 //2^n 8Kib
#define PERIPH_N_SIZE               29 //2^n       (include bit-banded regions and bit-band alias regions) size: 0x1FFFFFFF

#define FLASH_SIZE_256K           ((FLASH_N_SIZE-1) << 1) //size of flash = 256K
#define SRAM_SIZE_32K             ((SRAM_N_SIZE-1)  << 1)
#define PERIPH_SIZE_M             ((PERIPH_N_SIZE-1) << 1)
#define SRAM_SIZE_8KiB            ((SRAM8KiB_N_SIZE-1) << 1)

//**************SUB REGION DISABLE

#define DISABLE_ALL_SUB_REGIONS (255 << 8)
#define DISABLE_SUB_REGION_0    (1   << 8)
#define DISABLE_SUB_REGION_1    (2   << 8)
#define DISABLE_SUB_REGION_2    (4   << 8)
#define DISABLE_SUB_REGION_3    (8   << 8)
#define DISABLE_SUB_REGION_4    (16  << 8)
#define DISABLE_SUB_REGION_5    (32  << 8)
#define DISABLE_SUB_REGION_6    (64  << 8)
#define DISABLE_SUB_REGION_7    (128 << 8)

void sys_ps();                   //Displays the process (thread) information.
void sys_ipcs();                 //Displays the inter-process (thread) communication state.
void sys_kill(char * pid);       //Kills the process (thread) with the matching PID.
void sys_pi(_Bool on_off);       //Turns priority inheritance on or off.
void sys_preempt(_Bool on_off);  //Turns preemption on or off.
void sys_sched (_Bool prio_on);  //Selected priority or round-robin scheduling.
void sys_pidof(char pid[]);      //Displays the PID of the process (thread).
void sys_select();               //Runs the selected program in the background
void reboot();                   //reboots whole system

//******************* mpu function definitions

//void mpu_ctrlReg(uint8_t mask);

void mpu_setRegionNumber(uint8_t number);
void mpu_region_base(uint32_t addr, uint8_t region);
void mpu_region_attr(uint32_t attributes);
void mpu_region_number(uint8_t number);
void mpu_memfault_enable(void);
void mpu_busfault_enable(void);
void mpu_usagefault_enable(void);
void mpu_enable();

#endif
