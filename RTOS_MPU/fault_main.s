;tabis needed
	.global setPSP
	.global getPSP
	.global getMSP
	.global getHardFaultFlags
	.global setPrivilege

	.thumb

.text

setPSP: ; r0 holds the adddress of new stack sets the processes stack pointer to the program stack in main
	mov r1, #2 ; used to set ASP
	msr psp, r0 ; mov r0 into the psp to switch to program stack
	isb ; //
	msr CONTROL, r1 ;; set the ASP bit for thread mode
	isb
	bx lr

getPSP:
	mrs r0, psp ;
	bx lr ; return from function

getMSP:
	mrs r0, msp ;
	bx lr ; return from function

getHardFaultFlags: ; return the hardfault flags
	NOP
	bx lr ; return from sub-routine


setPrivilege:
	mov r1, #3 ; three because we still need the ASP bit to still be set.
	msr CONTROL, r1 ; set the TMPL bit(only privileged software can be executed in thread mode)
	isb ; isb must be used after an msr instruction
	bx lr ; return from sub-routine

