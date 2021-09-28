;device includes <tab> is needed
	.def setPSP
.text ; start of code
.thumb

setPSP: ; sets the processes stack pointer to the program stack in main
	msr psp, r0 ; PSP = Process stack pointer
