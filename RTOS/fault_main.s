;tab is needed
	.global setPSP
	.global getPSP
	.global getMSP
	.global setPrivilege
	.global push_R4_to_R11
	.global pop_R4_to_R11
	.global pushPSP
	.global getR0PSP
	.global getProgramCounter
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

setPrivilege:
	mov r1, #3 ; three because we still need the ASP bit to still be set.
	msr CONTROL, r1 ; set the TMPL bit(only privileged software can be executed in thread mode)
	isb ; isb must be used after an msr instruction
	bx lr ; return from sub-routine

;manual push pop because, regular push and pop instructions use the main stack pointer and threads are using PSP

push_R4_to_R11: ; save state after push func, stack pointer should indicate lowest address in stack frame
	mrs r0, psp ; get address of stack
	sub r0, #4  ; descending stack so decrement first, then store value
	str r11, [r0]
	sub r0, #4
	str r10, [r0]
	sub r0, #4
	str r9, [r0]
	sub r0, #4
	str r8, [r0]
	sub r0, #4
	str r7, [r0]
	sub r0, #4
	str r6, [r0]
	sub r0, #4
	str r5, [r0]
	sub r0, #4
	str r4, [r0]
	msr psp, r0 ;save current location of psp for to pop later
	bx lr ; return

pop_R4_to_R11: ; restore state
	mrs r0, psp
	ldr r4, [r0]
	add r0, #4
	ldr r5, [r0]
	add r0, #4
	ldr r6, [r0]
	add r0, #4
	ldr r7, [r0]
	add r0, #4
	ldr r8, [r0]
	add r0, #4
	ldr r9, [r0]
	add r0, #4
	ldr r10, [r0]
	add r0, #4
	ldr r11, [r0]
	add r0, #4
	msr psp, r0 ;address = original psp before the push happenned.
	bx lr ; return

pushPSP: ; takes arg value pushes on to stack of task
	mrs r1, psp
	sub r1, #4
	str r0, [r1]
	msr psp, r1
	bx lr

getR0PSP:
	mrs r0, psp
	bx lr
getProgramCounter: ;
	mrs r0, psp
	add r0, #24
	bx lr



