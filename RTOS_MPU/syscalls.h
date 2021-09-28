#ifndef __SYS_CALLS__
#define __SYS_CALLS__


void sys_ps();                   //Displays the process (thread) information.
void sys_ipcs();                 //Displays the inter-process (thread) communication state.
void sys_kill(char * pid);       //Kills the process (thread) with the matching PID.
void sys_pi(_Bool on_off);       //Turns priority inheritance on or off.
void sys_preempt(_Bool on_off);  //Turns preemption on or off.
void sys_sched (_Bool prio_on);  //Selected priority or round-robin scheduling.
void sys_pidof(char pid[]);      //Displays the PID of the process (thread).
void sys_select();    //Runs the selected program in the background
void reboot();

#endif
