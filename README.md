Real-time Operating Systems Course
1. Shell Interface over UART
2. Memory Protection Unit 
3. RTOS Design
	1) initHW() and ReadPushButtons() functions
	2) created head area aligned to a 1024 byte boundary by modifying size of sram
	3) modified createThread() to set process stack size and store thread name, pid, etc.
	4) use startRTOS() to call the scheduler and lauch first task as unprivileged
	5) implemented yield which will call SVC instruction which will call pendSVisr to handle task Switching
	6) implemented pendSVisr to save context of task when switching between tasks. (switching between 1 task for now)
			*(push/pop reg 4-11, since they are not saved by hardware)
	7) implemented pendSVisr to switch between multiple tasks
	8) added support for sleep and systick timer/isr
	9) added semaphore functionality for  wait
	10) added semaphore functionality for post
	11) added priority scheduler 
	12) add kernal functions to get cpu time and priviledged information over utart (...in progress)
	13) added preemption
