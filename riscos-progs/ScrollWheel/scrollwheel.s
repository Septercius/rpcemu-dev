@ ScrollWheel support module
@
@ Based in part on code contributed to ArcEm written by Daniel Clarke,
@ and available under the GPL Version 2.

wp	.req	r12

	@ SWIs
	XOS_Exit			= 0x20011
	XOS_IntOn			= 0x20013
	XOS_IntOff			= 0x20014
	XOS_Module			= 0x2001e
	XOS_ClaimDeviceVector		= 0x2004b
	XOS_ReleaseDeviceVector		= 0x2004c

	XWimp_Initialise		= 0x600c0
	XWimp_Poll			= 0x600c7
	XWimp_GetWindowState		= 0x600cb
	XWimp_GetPointerInfo		= 0x600cf
	XWimp_CloseDown			= 0x600dd
	XWimp_SendMessage		= 0x600e7

	XPodule_ReadInfo		= 0x6028d


	@ SWI Options
	Module_Enter	= 2
	Module_Claim	= 6
	Module_Free	= 7

	Message_Quit		= 0

	Service_Reset		= 0x27
	Service_StartWimp	= 0x49
	Service_StartedWimp	= 0x4a

	WIMP_VERSION = 300

	@ Enable poll word, disable Pointer Entering/Leaving and Null events
	WIMP_POLL_MASK = (1 << 22) | (1 << 4) | (1 << 0)

	@ Workspace
	WORKSPACE_SIZE = 512

	BUFFER_SIZE = 16

	WS_IO_BASE		= 0
	WS_POLL_WORD		= 4
	WS_TASK_HANDLE		= 8
	WS_BUFFER_READ		= 12
	WS_BUFFER_WRITE		= 16
	WS_BUFFER		= 20
	WS_BUFFER_END		= WS_BUFFER + BUFFER_SIZE
	WS_WIMP_BLOCK		= WS_BUFFER_END


	DEFAULT_SCROLL_STEP	= 64


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	.int	start		@ Start
	.int	init		@ Initialisation
	.int	final		@ Finalisation
	.int	service		@ Service Call
	.int	title		@ Title String
	.int	help		@ Help String
	.int	table		@ Help and Command keyword table
	.int	0		@ SWI chunk base
	.int	0		@ SWI handler code
	.int	0		@ SWI decoding table
	.int	0		@ SWI decoding code
	.int	0		@ Message File
	.int	modflags	@ Module Flags

modflags:
	.int	1		@ 32-bit compatible

title:
	.asciz	"ScrollWheel"

help:
	.asciz	"ScrollWheel\t0.01 (19 October 2024)"
	.align

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	@ Help and Command keyword table

table:
desktop_scrollwheel:
	.asciz	"Desktop_ScrollWheel"
	.align
	.int	command_desktop_scrollwheel
	.int	0
	.int	0
	.int	command_desktop_scrollwheel_help

	.int	0	@ Table terminator

command_desktop_scrollwheel_help:
	.asciz	"ScrollWheel provides scroll wheel support in the Wimp.\rDo not use *Desktop_ScrollWheel, use *Desktop instead."

	.align

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

command_desktop_scrollwheel:
	push	{lr}

	mov	r2, r0
	adr	r1, title
	mov	r0, #Module_Enter
	swi	XOS_Module

	pop	{pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	/*
	 * Entry:
	 *	r11 = I/O base or instantiation number
	 *	r12 = pointer to private word for this instantiation
	 */

init:
	push	{lr}

	@ See if we need to claim some workspace
	ldr	r0, [r12]
	teq	r0, #0
	bne	1f

	@ Claim some workspace
	mov	r0, #Module_Claim
	mov	r3, #WORKSPACE_SIZE
	swi	XOS_Module
	popvs	{pc}				@ no memory claimed then refuse to initialise

	str	r2, [r12]

	@ Claimed
1:
	ldr	wp, [r12]

	@ Initialise the workspace
	str	r11, [wp, #WS_IO_BASE]
	mov	r0, #0
	str	r0, [wp, #WS_POLL_WORD]
	str	r0, [wp, #WS_TASK_HANDLE]
	add	r0, wp, #WS_BUFFER
	str	r0, [wp, #WS_BUFFER_READ]
	str	r0, [wp, #WS_BUFFER_WRITE]

	@ Claim Device Vector
	mov	r0, #13				@ Device number (13 = Podule IRQ)
	adr	r1, driver
	mov	r2, wp
	mov	r3, r11				@ Address of interrupt status
	mov	r4, #1				@ Interrupt mask
	swi	XOS_ClaimDeviceVector

	@ Enable Podule interrupts if not currently enabled
	sub	sp, sp, #16
	mov	r0, #((1 << 16) | (1 << 15))
	mov	r1, sp
	mov	r2, #16
	mov	r3, r11
	swi	XPodule_ReadInfo
	add	sp, sp, #16
	popvs	{pc}

	ldr	r0, [sp, #-16]			@ Address of Interrupt Mask Register
	ldr	r1, [sp, #-12]			@ Interrupt Mask Bit Value

	@ Turn off interrupts while modifying Interrupt Mask register in I/O controller
	swi	XOS_IntOff
	ldrb	r2, [r0]			@ Current Interrupt Mask
	tst	r2, r1
	orreq	r2, r2, r1
	strbeq	r2, [r0]
	swi	XOS_IntOn

	@ Enable ScrollWheel message interrupts
	ldr	r0, [wp, #WS_IO_BASE]
	mov	r1, #1
	strb	r1, [r0, #4]

	pop	{pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	/*
	 * Entry:
	 *	r11 = instantiation number
	 *	r12 = pointer to private word for this instantiation
	 */

final:
	push	{lr}

	ldr	wp, [r12]

	@ Close Wimp task if active
	ldr	r0, [wp, #WS_TASK_HANDLE]
	cmp	r0, #0
	ldrgt	r1, TASK
	swigt	XWimp_CloseDown

	@ Disable ScrollWheel message interrupts
	ldr	r0, [wp, #WS_IO_BASE]
	mov	r1, #0
	strb	r1, [r0, #4]

	@ Release Device Vector
	mov	r0, #13				@ Device number (13 = Podule IRQ)
	adr	r1, driver
	mov	r2, wp
	ldr	r3, [wp, #WS_IO_BASE]		@ Address of interrupt status
	mov	r4, #1				@ Interrupt mask
	swi	XOS_ReleaseDeviceVector

	@ Free workspace
	mov	r0, #Module_Free
	mov	r2, wp
	swi	XOS_Module

	@ Clear V flag (26/32-bit safe) so our module will die
	cmp	pc, #0				@ Clears V (also clears N, Z, and sets C)

	pop	{pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

service_list:
	.int	0
	.int	service_main
	.int	Service_StartWimp
	.int	0				@ List terminator


	.int	service_list
service:
	mov	r0, r0				@ Magic instruction, pointer to service list above
	teq	r1, #Service_StartWimp
	movne	pc, lr
service_main:
	ldr	wp, [r12]

	teq	r1, #Service_StartWimp
	beq	service_start_wimp

	mov	pc, lr				@ Should never reach here

service_start_wimp:
	push	{lr}
	ldr	lr, [wp, #WS_TASK_HANDLE]
	teq	lr, #0
	moveq	lr, #-1
	streq	lr, [wp, #WS_TASK_HANDLE]
	adreq	r0, desktop_scrollwheel		@ r0 points to command to start task
	moveq	r1, #0				@ claim the service
	pop	{pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

driver:
	push	{r0, r1, lr}

	ldr	r0, [wp, #WS_IO_BASE]

	@ Clear interrupt
	strb	r0, [r0, #0]

	ldrb	r1, [r0, #4]			@ Message type
	teq	r1, #1
	popne	{r0, r1, pc}

	ldrb	r1, [r0, #8]			@ Message value

	@ Store in circular buffer
	ldr	r0, [wp, #WS_BUFFER_WRITE]
	strb	r1, [r0], #1
	add	r1, wp, #WS_BUFFER_END		@ Pointer to (one past) end of circular buffer
	teq	r0, r1
	addeq	r0, wp, #WS_BUFFER
	str	r0, [wp, #WS_BUFFER_WRITE]

	@ Set Poll Word
	mov	r0, #1
	str	r0, [wp, #WS_POLL_WORD]

	pop	{r0, r1, pc}

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

TASK:
	.ascii	"TASK"

task_title:
	.asciz	"Scroll Wheel Support"
	.align

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

	/*
	 * Entry:
	 *	r12 = pointer to private word for this instantiation
	 *
	 *	Processor is in User mode.
	 */

start:
	ldr	wp, [r12]

	ldr	r0, [wp, #WS_TASK_HANDLE]
	cmp	r0, #0				@ Am I already active?
	ble	start_skip_closedown		@ No then skip following instructions
	ldr	r1, TASK			@ Yes, so close down first
	swi	XWimp_CloseDown
	mov	r0, #0				@ Mark as inactive
	str	r0, [wp, #WS_TASK_HANDLE]

start_skip_closedown:
	mov	r0, #WIMP_VERSION		@ (re)start the task
	ldr	r1, TASK
	adr	r2, task_title
	swi	XWimp_Initialise
	swivs	XOS_Exit			@ Exit if error

	str	r1, [wp, #WS_TASK_HANDLE]	@ store Task Handle

	@ Main poll loop
re_poll:
	ldr	r0, =WIMP_POLL_MASK
	add	r1, wp, #WS_WIMP_BLOCK		@ point to Wimp block within workspace
	add	r3, wp, #WS_POLL_WORD		@ point to Poll Word within workspace
	swi	XWimp_Poll
	bvs	close_down

	teq	r0, #13				@ 13 = Poll Word non-zero
	beq	poll_word_non_zero
	teq	r0, #17				@ 17 = User Message
	teqne	r0, #18				@ 18 = User Message Recorded
	beq	user_message

	b	re_poll


poll_word_non_zero:
	@ Clear Poll Word
	mov	r0, #0
	str	r0, [wp, #WS_POLL_WORD]

	ldr	r0, [wp, #WS_BUFFER_READ]
	ldr	r1, [wp, #WS_BUFFER_WRITE]

	teq	r0, r1
	beq	re_poll

	ldrb	r3, [r0], #1
	add	r1, wp, #WS_BUFFER_END		@ Pointer to (one past) end of circular buffer
	teq	r0, r1
	addeq	r0, wp, #WS_BUFFER
	str	r0, [wp, #WS_BUFFER_READ]

	@ Sign-extend scroll amount
	mov	r3, r3, lsl #24
	mov	r3, r3, asr #24

	add	r1, wp, #WS_WIMP_BLOCK
	swi	XWimp_GetPointerInfo
	bvs	re_poll

	add	r1, r1, #12
	swi	XWimp_GetWindowState
	bvs	re_poll

	ldr	r0, [r1, #32]			@ get window flags

	@ Check if the window has a vertical scroll bar
	tst	r0, #(1 << 31)			@ new style flags?
	beq	scroll_check_old
	tst	r0, #(1 << 28)
	beq	re_poll
	b	scroll_check_done
scroll_check_old:
	tst	r0, #(1 << 2)
	beq	re_poll

scroll_check_done:
	tst	r0, #(1 << 8)
	bne	send_scroll_request

	@ Otherwise just need to do an Open Window request
	ldr	r0, [r1, #24]			@ get y scroll offset
	cmp	r3, #0
	addgt	r0, r0, #DEFAULT_SCROLL_STEP
	sublt	r0, r0, #DEFAULT_SCROLL_STEP
	str	r0, [r1, #24]			@ altered y scroll offset

	mov	r0, #2				@ OpenWindow request
	ldr	r2, [r1, #0]
	swi	XWimp_SendMessage
	b	re_poll

send_scroll_request:
	cmp	r3, #0
	movgt	r4, #1
	movlt	r4, #-1
	str	r4, [r1, #36]

	mov	r0, #10				@ Scroll request
	ldr	r2, [r1, #0]
	swi	XWimp_SendMessage
	b	re_poll


user_message:
	ldr	r0, [r1, #16]			@ Get message code
	teq	r0, #Message_Quit		@ Is it Quit message...?
	bne	re_poll				@ ...no so re-poll
	@ fall-through				@ otherwise continue to...
close_down:
	@ Close down Wimp task
	ldr	r0, [wp, #WS_TASK_HANDLE]
	ldr	r1, TASK
	swi	XWimp_CloseDown

	@ Zero the Task Handle
	mov	r0, #0
	str	r0, [wp, #WS_TASK_HANDLE]

	swi	XOS_Exit
