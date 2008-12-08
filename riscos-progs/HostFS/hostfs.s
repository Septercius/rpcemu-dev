        @
        @ $Id: hostfs.s,v 1.5 for RPCEmu 2008/12/08 20:28:21 mhowkins Exp $
        @

        @ Register naming
	wp .req r12

	@ ARM constants
	VBIT = 1 << 28
	CBIT = 1 << 29
	ZBIT = 1 << 30
	NBIT = 1 << 31

	@ RISC OS constants
        XOS_Write0          = 0x20002
        XOS_CLI             = 0x20005
        XOS_FSControl       = 0x20029
        XOS_Module          = 0x2001E
        XOS_ValidateAddress = 0x2003a
        XMessageTrans_ErrorLookup = 0x61506

        @ Reporter SWI for debug
        XReport_Text0     = 0x54C80 | 0x20000
        XReport_TextS     = 0x54C81 | 0x20000
        XReport_Regs      = 0x54C82 | 0x20000
        XReport_Registers = 0x54C83 | 0x20000
        XReport_Where     = 0x54C84 | 0x20000
        XReport_Poll      = 0x54C85 | 0x20000
        XReport_Dump      = 0x54C86 | 0x20000


	FSControl_AddFS    = 12
	FSControl_SelectFS = 14
	FSControl_RemoveFS = 16

	OSModule_Claim     = 6
	OSModule_Free      = 7

	Service_FSRedeclare = 0x40

	@ Workspace setup
	WorkSpace_Size = 4
        WS_Default_drive = 0

	@ ArcEm SWI chunk
	ARCEM_SWI_CHUNK  = 0x56ac0
	ARCEM_SWI_CHUNKX = ARCEM_SWI_CHUNK | 0x20000
	ArcEm_HostFS    = ARCEM_SWI_CHUNKX + 1

	@ Filing system error codes
	FILECORE_ERROR_DISCFULL = 0xc6
	FILECORE_ERROR_BADDISC  = 0xc8
	FILECORE_ERROR_DISCPROT = 0xc9

	@ Filing system properties
	FILING_SYSTEM_NUMBER = 0x99     @ TODO choose unique value
	MAX_OPEN_FILES       = 100      @ TODO choose sensible value



	.global	_start

_start:


module_start:

        .int    0               @ Start
        .int    init            @ Initialisation
        .int    final           @ Finalisation
        .int    service_pre     @ Service Call
        .int    modtitle        @ Title String
        .int    help            @ Help String
        .int    table           @ Help and Command keyword table
        .int    0x46900         @ SWI Chunk base temporily loaned from HostFS
        .int    swi_handler     @ SWI handler code
        .int    swi_table       @ SWI decoding table
        .int    0               @ SWI decoding code
        .int    0               @ Message File
        .int    modflags        @ Module Flags

modflags:
        .int    1               @ 32 bit compatible

modtitle:
        .string "RPCEmuHostFS"

help:
        .string "RPCEmu HostFS\t0.05 (08 Dec 2008)"

        .align


        @ Help and Command keyword table
table:
        .string "HostFS"
        .align
        .int    command_hostfs
        .int    0x00000000
        .int    command_hostfs_syntax
        .int    command_hostfs_help

        .byte   0       @ Table terminator

command_hostfs_help:
        .ascii  "*HostFS selects the HostFS filing system\r"    @ unterminated - continues below...
command_hostfs_syntax:
        .string "Syntax: *HostFS"
        .align


	@ Filing System Information Block
fs_info_block:
        .int    fs_name         @ Filing System name
        .int    fs_text         @ Filing System startup text
        .int    fs_open         @ To Open files (FSEntry_Open)
        .int    fs_getbytes     @ To Get Bytes (FSEntry_GetBytes)
        .int    fs_putbytes     @ To Put Bytes (FSEntry_PutBytes)
        .int    fs_args         @ To Control open files (FSEntry_Args)
        .int    fs_close        @ To Close open files (FSEntry_Close)
        .int    fs_file         @ To perform whole-file ops (FSEntry_File)
        .int    FILING_SYSTEM_NUMBER | (MAX_OPEN_FILES << 8)
                                @ Filing System Information Word
        .int    fs_func         @ To perform various ops (FSEntry_Func)
        .int    fs_gbpb         @ To perform multi-byte ops (FSEntry_GBPB)
        .int    0               @ Extra Filing System Information Word

fs_name:
        .string "HostFS"

fs_text:
        .string "RPCEmu Host Filing System"
        .align

swi_table:
        .string "RPCEmuHostFS"
        .string "Drives"
        .string "DiscInfo"
        .string "FreeSpace"
        .string "FreeSpace64"
        .byte   0       @terminating byte
        .align


        /* Entry:
         *    R0 - R9 are passed from the SWI caller by RISC OS
         *    R11 = SWI number modulo Chunk Size (ie 0 - 63)
         *    R12 = private word pointer
         *    R13 = supervisor stack
         *    R14 contains the flags of the SWI caller
         * Exit:
         *    R0 - R9 are returned to SWI caller by RISC OS
         *    R10 - R12 may be corrupted
         * Processor Mode
         *    Processor is in SVC mode
         */

swi_handler:
        ldr     wp,[r12]                @get workspace pointer from private word
        cmp     r11,#(swi_select_end-swi_select)/4
        addlo   pc, pc, r11, lsl #2     @dispatch if in range
        b       swi_unknown             @unknown SWI
swi_select:
        b       swi_drives
        b       swi_bye
        b       swi_bye
        b       swi_bye
swi_select_end:

swi_unknown:
        stmfd   sp!, {lr}
        adr     r0, err_badswi
        mov     r1, #0
        mov     r2, #0
        adr     r4, modtitle
        swi     XMessageTrans_ErrorLookup       @ V always set when SWI returns
        ldmfd   sp!, {lr}       @restore link register to original caller
        teq     pc, pc          @not equal in 26 bit SVC mode, doesn't modify V flag
        orrnes  pc, lr, #VBIT   @return with V set, other flags preserved (26 bit mode)
        mov     pc, lr          @return with V set, other flags not preserved (32 bit mode)


        /* Entry:
         *   - 
         * Exit:
         *   r0 = default drive
         *   r1 = 0 may change in the future
         *   r2 = number of hostfs discs
         * Processor Flags:
         *   Corrupted  
         */

swi_drives:
        stmfd   sp!, {lr}
        mov     r9, #8
        swi     ArcEm_HostFS
        mov     r2, r1
        mov     r1, #0
        ldmfd   sp!, {pc}

swi_discinfo:
        /* TODO define parameters and interface to RPCEmu */

swi_freespace:

swi_freespace64:

swi_bye:
        stmfd   sp!, {lr}
        teq     pc, pc          @not equal in 26 bit SVC mode
        ldmnefd sp!, {pc}^      @in 26 bit mode return to caller with flags preserved
        ldmfd   sp!, {pc}

        /* Entry:
         *   r10 = pointer to environment string
         *   r11 = I/O base or instantiation number
         *   r12 = pointer to private word for this instantiation
         *   r13 = stack pointer (supervisor)
         * Exit:
         *   r7-r11, r13 preserved
         *   other may be corrupted
         */
init:
        @ init, claim workspace
        stmfd   sp!, {lr}
        mov     r0, #OSModule_Claim
        mov     r3, #WorkSpace_Size
        swi     XOS_Module
        strvc   r2, [wp]        @ store workspace pointer (wp) to pointer in private word register

	@ Declare filing system when no error occured claiming workspace
        movvc   r0, #FSControl_AddFS
        adrvc   r1, module_start
        movvc   r2, #(fs_info_block - module_start)
        movvc   r3, wp
        swivc   XOS_FSControl

        ldmfd   sp!, {pc}

	/* Entry:
	 *   r10 = fatality indication: 0 is non-fatal, 1 is fatal
	 *   r11 = instantiation number
	 *   r12 = pointer to private word for this instantiation of the module.
	 *   r13 = supervisor stack pointer
	 * Exit:
	 *   preserve processor mode and interrupt state
	 *   r7-r11, r13 preserved
	 *   other and flags may be corrupted
	 */

final:
	@ Remove filing system, claimed workspace is automaticaly freed
	stmfd	sp!, {lr}

	mov	r0, #FSControl_RemoveFS
	adr	r1, fs_name
	swi	XOS_FSControl
	cmp	pc, #0		@ Clears V (also clears N, Z, and sets C)

	ldmfd	sp!, {pc}


	/* Entry:
	 *   r1 = service number
	 *   r12 = pointer to private word for this instantiation
	 *   r13 = stack pointer
	 * Exit:
	 *   r1 = can be set to zero if the service is being claimed
	 *   r0,r2-r8 can be altered to pass back a result
	 *   registers must be preserved if not returning a result
	 *   r12 may be corrupted
	 */

         @ RISC OS 4 Service codetable
service_codetable:
	.int	0               @ no special flags enabled
	.int	service_main
	.int	Service_FSRedeclare
	.int	0		@ table terminator
	.int	service_codetable
service_pre:
	mov	r0, r0          @ magic instruction, pointer to service table at service_pre-4
	teq	r1, #Service_FSRedeclare
	movne	pc, lr
service_main:
	teq	r1, #Service_FSRedeclare
	beq	service_fsredeclare

	mov	pc, lr		@ should never reach here

	@ Filing system reinitialise
service_fsredeclare:
	stmfd	sp!, {r0-r3, lr}

	@ Redeclare filing system
	mov	r0, #FSControl_AddFS
	adr	r1, module_start
	mov	r2, #(fs_info_block - module_start)
	mov	r3, r12
	swi	XOS_FSControl

	ldmfd	sp!, {r0-r3, pc}


	/* Entry (for all *Commands):
	 *   r0 = pointer to command tail (read-only)
	 *   r1 = number of parameters (as counted by OSCLI)
	 *   r12 = pointer to private word for this instantiation
	 *   r13 = stack pointer (supervisor)
	 *   r14 = return address
	 * Exit:
	 *   r0 = error pointer (if needed)
	 *   r7-r11 preserved
	 */

	@ *HostFS
command_hostfs:
	@ Select HostFS as the current Filing System
	stmfd	sp!, {lr}

	mov	r0, #FSControl_SelectFS
	adr	r1, fs_name
	swi	XOS_FSControl

	ldmfd	sp!, {pc}



	/* FSEntry_Open (Open a file)
	 */
fs_open:
	stmfd	sp!, {lr}

	mov	r9, #0
	swi	ArcEm_HostFS

	ldmfd	sp!, {pc}


	/* FSEntry_GetBytes (Get bytes from a file)
	 */
fs_getbytes:
	stmfd	sp!, {lr}

	mov	r9, #1
	swi	ArcEm_HostFS

	ldmfd	sp!, {pc}


	/* FSEntry_PutBytes (Put bytes to a file)
	 */
fs_putbytes:
	stmfd	sp!, {lr}

	mov	r9, #2
	swi	ArcEm_HostFS

	ldmfd	sp!, {pc}


	/* FSEntry_Args (Control open files)
	 */
fs_args:
	stmfd	sp!, {lr}

	mov	r9, #3
	swi	ArcEm_HostFS

	ldmfd	sp!, {pc}


	/* FSEntry_Close (Close an open file)
	 */
fs_close:
	stmfd	sp!, {lr}

	mov	r9, #4
	swi	ArcEm_HostFS

	ldmfd	sp!, {pc}


	/* FSEntry_File (Whole-file operations)
	 */
fs_file:
	stmfd	sp!, {lr}

	mov	r9, #5
	swi	ArcEm_HostFS

        cmp     r9, #0xB0
	movhs   r0,r9
	bhs     hostfs_error

	ldmfd	sp!, {pc}


	/* FSEntry_Func (Various operations)
	 */
fs_func:
	stmfd	sp!, {lr}

	@ Test if operation is FSEntry_Func 10 (Boot filing system)...
	teq	r0, #10
	beq	boot

        @teq     r0, #23
        @swi     XReport_Regs

        @ Pass the call to emulator
	mov	r9, #6
	swi	ArcEm_HostFS

	teq	r9, #255
	beq	not_implemented

        cmp	r9, #0xB0
	movhs	r0,r9
	bhs	hostfs_error

	ldmfd	sp!, {pc}


        /* FSEntry_GBPB (Multi-byte operations)
         */
fs_gbpb:
        stmfd   sp!, {lr}

        mov     r9, #7
        swi     ArcEm_HostFS

        ldmfd   sp!, {pc}


boot:
	adr	r0, 1f
	swi	XOS_CLI
	ldmfd	sp!, {pc}	@ Don't preserve flags - return XOS_CLI's error (if any)

1:
	.string	"Run @.!Boot"
	.align

not_implemented:
	adr	r0, err_badfsop
	mov	r1, #0
	mov	r2, #0
	adr	r4, modtitle
	swi	XMessageTrans_ErrorLookup	@ V always set when SWI returns
	ldmfd	sp!, {pc}


	/* Entry: 
	 * R0 = error number
	 * Exit:
	 * Return function with error
	 */
         
hostfs_error:
	teq	r0, #255
	beq	not_implemented

	teq	r0, #FILECORE_ERROR_DISCFULL
	adreq	r0, err_discfull
	beq	hostfs_return_error

	teq	r0, #FILECORE_ERROR_BADDISC
	adreq	r0, err_baddisc
	beq	hostfs_return_error

	teq	r0, #FILECORE_ERROR_DISCPROT
	adreq	r0, err_discprot
	beq	hostfs_return_error

	adr	r0, err_unknown

hostfs_return_error:
	cmp	r0, #NBIT	@ compare r0 with most negative number (r0-1<<31)
	cmnvc	r0, #NBIT	@ no overflow then compare R0 with most non existent positive number (r0+1<<31)
	ldmfd	sp!, {pc}	@ exit error function with V set

err_badswi:
        .int    0x1E6
        .string "BadSWI"
        .align

err_badfsop:
	.int	0x100A0 | (FILING_SYSTEM_NUMBER << 8)
	.string	"BadFSOp"
	.align

err_discfull:
	.int	0x10000 | (FILING_SYSTEM_NUMBER << 8) | FILECORE_ERROR_DISCFULL
	.string	"Disc is full"
	.align

err_baddisc:
	.int	0x10000 | (FILING_SYSTEM_NUMBER << 8) | FILECORE_ERROR_BADDISC
	.string	"Disc not found"
	.align

err_discprot:
	.int	0x10000 | (FILING_SYSTEM_NUMBER << 8) | FILECORE_ERROR_DISCPROT
	.string	"Disc is protected for changes"
	.align

err_unknown:
	.int	0x10000 | (FILING_SYSTEM_NUMBER << 8)
	.string	"An unknown error occured"
	.align
