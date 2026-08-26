        @
        @ $Id: hostfsfiler.s,v 1.3 2008/12/08 20:30:30 mhowkins Exp $
        @
        @ HostFS Filer
        @

        @ Register naming
        wp .req r12

        @ ARM constants
        VBIT = 1 << 28
        CBIT = 1 << 29
        ZBIT = 1 << 30
        NBIT = 1 << 31

        @ RISC OS constants
        XOS_CLI               = 0x20005
        XOS_Exit              = 0x20011
        XOS_Module            = 0x2001e
        XOS_ReadModeVariable  = 0x20035
        XOS_ReadMonotonicTime = 0x20042
        XWimp_Initialise = 0x600c0
        XWimp_CreateIcon = 0x600c2
        XWimp_CreateMenu = 0x600d4
        XWimp_CloseDown  = 0x600dd
        XWimp_PollIdle   = 0x600e1
        XWimp_SpriteOp   = 0x600e9

        Module_Enter = 2
        Module_Claim = 6
        Module_Free  = 7

        Message_Quit = 0

        Service_Reset             = 0x27
        Service_StartFiler        = 0x4b
        Service_StartedFiler      = 0x4c
        Service_FilerDying        = 0x4f

        SpriteOp_ReadSpriteInfo = 40

        ModeVariable_YEig = 5

        WIMP_VERSION = 300

        WIMP_POLL_MASK = 0x00000031     @ no Null, Pointer Entering or Pointer Leaving events

        WORKSPACE_SIZE = 1024

        WS_MY_TASK_HANDLE         = 0
        WS_FILER_TASK_HANDLE      = 4
        WS_WIMP_VERSION           = 8
        WS_DRIVE_COUNT            = 12
        WS_DRIVE_MASK             = 16
        WS_DRIVE_START            = 20
        WS_DRIVE_END              = 24
        WS_ICON_HEIGHT            = 28
        WS_ICON_BAR_BLOCK_START   = 32
        WS_ICON_BAR_BLOCK_END     = 176
        WS_ICON_HANDLE_START      = 176
        WS_ICON_HANDLE_END        = 192
        WS_DRIVE_NAME_BLOCK_START = 192
        WS_DRIVE_NAME_BLOCK_END   = 244
        WS_COMMAND_BUFFER         = 244
        WS_WIMP_BLOCK             = 372 @ must be last
        
        ICON_BLOCK_SIZE           = 36
        NAME_BLOCK_SIZE           = 13

        @ HostFS SWIs
        XHostFS_Initialise				= 0x76ad0	
        XHostFS_Drives					= 0x76ad1	@ Get drive information
        XHostFS_GetDriveName			= 0x76ad2	@ Get drive name from identifier
        XHostFS_GenerateCommandString	= 0x76ad3	@ Generate command for drive
        XHostFS_FreeOp					= 0x76ad4	@ Free operation
        XHostFS_ValidateDrive			= 0x76ad5	@ Check drive validity
        
        @ Command strings
        HostFSCommand_OpenDir			= 1
        HostFSCommand_Free				= 2
        HostFSCommand_Boot				= 3
        
        .global _start

_start:

module_start:

        .int	start           @ Start
        .int	init            @ Initialisation
        .int	final           @ Finalisation
        .int    service_pre     @ Service Call
        .int	modtitle        @ Title String
        .int	help            @ Help String
        .int	table           @ Help and Command keyword table
        .int	0               @ SWI Chunk base
        .int	0               @ SWI handler code
        .int	0               @ SWI decoding table
        .int	0               @ SWI decoding code
        .int    0               @ Message File
        .int    modflags        @ Module Flags

modflags:
        .int    1               @ 32 bit compatible

modtitle:
        .string	"RPCEmuHostFSFiler"

help:
        .string	"HostFSFiler\t0.06 (15 Aug 2026)"
        .align


        @ Help and Command keyword table
table:
desktop_hostfsfiler:
        .string "Desktop_HostFSFiler"
        .align
        .int    command_desktop_hostfsfiler
        .int    0x00070000
        .int    0
        .int    command_desktop_hostfsfiler_help

        .byte   0       @ Table terminator

command_desktop_hostfsfiler_help:
        .string	"The HostFSFiler provides the HostFS icons on the icon bar, and uses the Filer to display HostFS directories.\rDo not use *Desktop_HostFSFiler, use *Desktop instead."

        .align



init:
	stmfd	sp!, {lr}

	@ See if we need to claim some workspace
	ldr	r0, [r12]
	teq	r0, #0
	bne	1f

	@ Claim some workspace
	mov	r0, #Module_Claim
	mov	r3, #WORKSPACE_SIZE
	swi	XOS_Module
	ldmvsfd	sp!, {pc}       @ no memory claimed then refuse to initialise
	
	str	r2, [r12]
1:
	ldr	wp, [r12]

	@ Initialise the workspace
	mov	r0, #0
	str	r0, [wp, #WS_MY_TASK_HANDLE]

	ldmfd	sp!, {pc}



final:
	stmfd	sp!, {lr}

	ldr	wp, [r12]

	@ Close Wimp task if active
	ldr	r0, [wp, #WS_MY_TASK_HANDLE]
	cmp	r0, #0
	ldrgt	r1, TASK
	swigt	XWimp_CloseDown

	@ Free workspace
	mov	r0, #Module_Free
	mov	r2, r12
	swi	XOS_Module

        @ Clear V flag (26/32 bit safe) so our module will die
        cmp     pc, #0          @ Clears V (also clears N, Z, and sets C)
	ldmfd	sp!, {pc}



         @ RISC OS 4 Service codetable
service_codetable:
        .int    0               @ no special flags enabled
        .int    service_main
        .int    Service_Reset
        .int    Service_StartFiler
        .int    Service_StartedFiler
        .int    Service_FilerDying
        .int    0               @ table terminator
        .int    service_codetable
service_pre:
        mov     r0, r0          @ magic instruction, pointer to service table at service_pre-4
        teq     r1, #Service_Reset
        teqne   r1, #Service_StartFiler
        teqne   r1, #Service_StartedFiler
        teqne   r1, #Service_FilerDying
        movne   pc, lr

service_main:
        stmfd   sp!, {lr}

        ldr     wp, [r12]

        teq     r1, #Service_Reset
        beq     service_reset
        teq     r1, #Service_StartFiler
        beq     service_startfiler
        teq     r1, #Service_StartedFiler
        beq     service_startedfiler
        teq     r1, #Service_FilerDying
        beq     service_filerdying

        @ Should never reach here
        ldmfd   sp!, {pc}



service_reset:
	@ Zero the Task Handle
	mov	r14, #0
	str	r14, [wp, #WS_MY_TASK_HANDLE]
	ldmfd	sp!, {pc}



service_startfiler:
	ldr	r14, [wp, #WS_MY_TASK_HANDLE]
	teq	r14, #0                                 @ Am I already active?
	moveq	r14, #-1                                @ No, so set handle to -1
	streq	r14, [wp, #WS_MY_TASK_HANDLE]
	streq	r1,  [wp, #WS_FILER_TASK_HANDLE]        @ store Filer's task handle
	adreq	r0,  desktop_hostfsfiler                @ r0 points to command to start task
	moveq	r1,  #0                                 @ claim the service
	ldmfd	sp!, {pc}



service_startedfiler:
	@ Zero the Task Handle if it is -1
	ldr	r14, [wp, #WS_MY_TASK_HANDLE]
	cmp	r14, #-1
	moveq	r14, #0
	streq	r14, [wp, #WS_MY_TASK_HANDLE]
	ldmfd	sp!, {pc}



service_filerdying:
        @ Shut down task if active

        stmfd	sp!, {r0-r1}

        ldr	r0, [wp, #WS_MY_TASK_HANDLE]
        cmp	r0, #0

        @ Zero the Task Handle if non-zero
        movne	r14, #0
        strne	r14, [wp, #WS_MY_TASK_HANDLE]

        @ Shut down task if Task Handle was positive
        ldrgt	r1, TASK
        swigt	XWimp_CloseDown

        ldmfd	sp!, {r0-r1}
        ldmfd	sp!, {pc}



command_desktop_hostfsfiler:
	stmfd	sp!, {lr}
	mov	r2, r0
	adr	r1, modtitle
	mov	r0, #Module_Enter
	swi	XOS_Module
	ldmfd	sp!, {pc}



TASK:
	.ascii	"TASK"

task_modtitle:
	.string	"HostFS Filer"
	.align

icon_bar_block:
	.int	-5		@ Left side of icon bar, scan from left (RO3+)
	.int	0		@ Minimum X
	.int	-16		@ Minimum Y
	.int	96		@ Maximum X
	.int	20		@ Maximum Y (excludes Sprite - added later)
	.int	0x1700310b	@ Flags (includes Indirected Text and Sprite)
	.int	0		@ Gap for pointer to Text
	.int	0		@ Gap for pointer to Validation String
	.int	13		@ Length of Text buffer

icon_bar_validation:
	.ascii	"S"		@ Unterminated - continues below...
icon_bar_icon_name:
	.string	"harddisc"

	.align



menu:
	.string	"HostFS"	@ Menu Title, padded to 12 bytes
	.align
	.int	0

	.byte	7, 2, 7, 0	@ Title colours
	.int	16 * 6		@ Width
	.int	44		@ Height
	.int	0		@ Vertical gap
	@ Menu items
	.int	(1 << 7)	@ Flags: last item
	.int	-1		@ Submenu pointer
	.int	0x07000001	@ Menu item icon flags, Text
	.string	"Free"		@ Menu item icon data, padded to 12 bytes
	.align
	.int	0



	@ "Start" entry point
	@ Entered in User Mode
	@ Therefore no need to preserve link register before calling SWIs
start:
        ldr     wp, [r12]               @ Get workspace pointer
        ldr     r0, [wp, #WS_MY_TASK_HANDLE]
        cmp     r0, #0                  @ Am I already active?
        ble     start_skipclosedown     @ No then skip following instructions
        ldr     r1, TASK                @ Yes, so close down first
        swi     XWimp_CloseDown
        mov     r0, #0                  @ Mark as inactive
        str     r0, [wp, #WS_MY_TASK_HANDLE]

start_skipclosedown:
	ldr	r0, = WIMP_VERSION	@ (re)start the task
	ldr	r1, TASK
	adr	r2, task_modtitle
	swi	XWimp_Initialise
	swivs	XOS_Exit		@ Exit if error

	str	r0, [wp, #WS_WIMP_VERSION]	@ store Wimp version
	str	r1, [wp, #WS_MY_TASK_HANDLE]	@ store Task handle
	
	@ Calculate icon height
	mov r0, #SpriteOp_ReadSpriteInfo
	adr r2, icon_bar_icon_name
	swi XWimp_SpriteOp
	
	movvc r0, r6
	movvc r1, #ModeVariable_YEig
	swivc XOS_ReadModeVariable
	
	bvs close_down
	
	mov r0, #0
	add r0, r0, r4, lsl r2
	str r0, [wp, #WS_ICON_HEIGHT]
	
	@ Retrieve drive information
	swi XHostFS_Drives
	
	@ Returns:
	@   R0 = number of drives
	@   R1 = drive mask
	@   R2 = first drive number
	@   R3 = last drive number

	str	r0, [wp, #WS_DRIVE_COUNT]
	str	r1, [wp, #WS_DRIVE_MASK]
	str	r2, [wp, #WS_DRIVE_START]
	str	r3, [wp, #WS_DRIVE_END]

	@ Prepare for the first drive
	add	r8, wp, #WS_ICON_BAR_BLOCK_START
	add	r9, wp, #WS_ICON_HANDLE_START
	add	r10, wp, #WS_DRIVE_NAME_BLOCK_START

	@ Set the current drive
	mov	r11, r2

create_icon_loop:

	mov r0, r11
	swi XHostFS_ValidateDrive
	
	cmp r1, #0
	beq create_icon_next
	
	adr r5, icon_bar_block
	mov r6, r8
	
	ldmia r5!, {r2-r4}
	stmia r6!, {r2-r4} 		@ Position, min x, min y
	
	ldmia r5!, {r2-r4}
	
	@ Apply icon height
	ldr r0, [wp, #WS_ICON_HEIGHT]
	add r3, r3, r0
	
	stmia r6!, {r2-r4}		@ Max x, max y, flags
	
	mov r0, r11
	mov r1, r10
	mov r2, #13
	swi XHostFS_GetDriveName
	
	mov r0, r1
	adr r1, icon_bar_validation
	
	stmia r6, {r0-r2}
	
	@ Create the icon
	mov	r0, #0x71000000
	mov	r1, r8
	
	swi	XWimp_CreateIcon
	bvs	close_down
	
	@ Store the icon handle
	str r0, [r9]
	
create_icon_next:
	
	add r8, r8, #ICON_BLOCK_SIZE
	add r9, r9, #4
	add r10, r10, #NAME_BLOCK_SIZE
	add r11, r11, #1
	
	ldr r0, [wp, #WS_DRIVE_END]
	cmp r11, r0
	ble create_icon_loop

	@ Main poll loop
re_poll:
	swi	XOS_ReadMonotonicTime	@ returns time in r0
	add	r2, r0, #100		@ poll no sooner than 1 sec unless event
	ldr	r0, = WIMP_POLL_MASK
	add	r1, wp, #WS_WIMP_BLOCK	@ point to Wimp block within workspace
	swi	XWimp_PollIdle
	bvs	close_down

	teq	r0, #6			@ 6 = Mouse Click
	beq	mouse_click
	teq	r0, #9			@ 9 = Menu Selection
	beq	menu_selection
	teq	r0, #17			@ 17 = User Message
	teqne	r0, #18			@ 18 = User Message Recorded
	beq	user_message
	b	re_poll


mouse_click:
	ldr	r0, [r1, #12]		@ Icon handle
	cmp	r0, #-2
	bne	re_poll
	
	mov r9, r1
	add r10, wp, #WS_ICON_HANDLE_START
	ldr r11, [wp, #WS_DRIVE_START]
	
mouse_click_loop:

	mov r0, r11
	swi XHostFS_ValidateDrive
	
	cmp r1, #0
	beq mouse_click_next
	
	ldr r7, [r9, #16]
	ldr r8, [r10]
	
	cmp r7, r8
	bne mouse_click_next
	
	ldr	r0, [r9, #8]		@ Buttons

	cmp	r0, #4			@ Select
	cmpne	r0, #1			@ Adjust
	beq open_drive
	
	cmp	r0, #2			@ Menu
	bne	re_poll

	ldr	r2, [r1, #0]		@ X coordinate of click
	sub	r2, r2, #64
	mov	r3, #(96 + 44)
	adr	r1, menu
	swi	XWimp_CreateMenu

	b	re_poll
	
mouse_click_next:
	
	add r11, r11, #1
	ldr r8, [wp, #WS_DRIVE_END]
	cmp r11, r8
	addle r10, r10, #4
	ble mouse_click_loop
	
	b re_poll

menu_selection:

	mov	r0, #HostFSCommand_Free
	mov	r1, r11
	add	r2, wp, #WS_COMMAND_BUFFER
	mov	r3, #128
	swi	XHostFS_GenerateCommandString

	mov	r0, r2
	swi	XOS_CLI

	b	re_poll

open_drive:

	@ r11 = drive number
	
	mov	r0, #HostFSCommand_OpenDir
	mov	r1, r11
	add	r2, wp, #WS_COMMAND_BUFFER
	mov	r3, #128
	swi	XHostFS_GenerateCommandString
	
	mov	r0, r2
	swi	XOS_CLI
	
	b	re_poll

user_message:
	ldr	r0, [r1, #16]		@ Contains message code
	teq	r0, #Message_Quit	@ Is it Quit message...?
	bne	re_poll			@ ...no so re-poll
					@ otherwise continue to...
close_down:
	@ Close down Wimp task
	ldr	r0, [wp, #WS_MY_TASK_HANDLE]
	ldr	r1, TASK
	swi	XWimp_CloseDown

	@ Zero the Task Handle
	mov	r0, #0
	str	r0, [wp, #WS_MY_TASK_HANDLE]

	swi	XOS_Exit
