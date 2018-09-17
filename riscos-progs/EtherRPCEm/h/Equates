; h.Equates
;  This program is free software; you can redistribute it and/or modify it 
;  under the terms of version 2 of the GNU General Public License as 
;  published by the Free Software Foundation;
;
;  This program is distributed in the hope that it will be useful, but WITHOUT 
;  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
;  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
;  more details.
;
;  You should have received a copy of the GNU General Public License along with
;  this program; if not, write to the Free Software Foundation, Inc., 59 
;  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
;  
;  The full GNU General Public License is included in this distribution in the
;  file called LICENSE.
;  The code in this file is (C) 2003 J Ballance as far as
;  the above GPL permits
;


; flags etc...
V_flag          * &10000000
C_flag          * &20000000
I_flag          * &08000000
IRQ_bit         * &08000000
IRQ_OFF         * &0c000000             ; Cancel IRQ & FIRQ bits
IRQIRQoff       * &08000002             ; IRQ mode, IRQs off
IRQIRQon        * &00000002             ; IRQ mode, IRQs on
SerIRQoff       * &08000003             ; service mode, IRQs off
SerIRQon        * &00000003             ; service mode, IRQs on
SerNoIRQ        * &0c000003             ; service mode, no INTs
SerFIQoff       * &04000003             ; service mode, no FIQs
FIQ             * 1                     ; FIQ mode bits
IRQMode         * 2                     ; IRQ mode bits
SVCE            * 3                     ; Service mode bits
UserMode        * 0                     ; User Mode teqp mask
AllFlags        * &fc000003

Illegal         * &e6000010 
IllegalNE       * &16000010 

; 3 macros to provide safe entry to service mode from anywhere..
; ToSVCEIRQOff enters and turns off IRQs
; ToSVCE       enters without changing IRQ mode
; reg 1 < reg 2.. .. 2 work registers, preserved and not passed into code

        MACRO
        ToSVCEIRQOff	$reg1,$reg2   
        STMFD   r13!,{r$reg1,r$reg2,lr} ; remember work regs & current mode
        				; on current mode stack
        MRS     r$reg1, CPSR           	; remember mode & flags
        ORR     r$reg2, r$reg1, #&83    ; IRQ26->SVC26, IRQ32->SVC32 ,IRQ off
        MSR     CPSR_c, r$reg2
        
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        MOV     r$reg2,lr           	; remember svce lr
        STMFD   r13!,{r$reg1,r$reg2}    ; on svce stack
        				; and continue.. reg1 is i/p mode status
        				;		 reg2 is ip mode lr
        MEND

; go to service mode without effecting IRQ mode
; reg 1 < reg 2.. and not passed into code following this
        MACRO
        ToSVCE	$reg1,$reg2   
        STMFD   r13!,{r$reg1,r$reg2,lr} ; remember work regs & current mode
        				; on current mode stack
        MRS     r$reg1, CPSR           	; remember mode & flags
        ORR     r$reg2, r$reg1, #&3    ; IRQ26->SVC26, IRQ32->SVC32 
        MSR     CPSR_c, r$reg2
        
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        MOV     r$reg2,lr           	; remember svce lr
        STMFD   r13!,{r$reg1,r$reg2}    ; on svce stack
        				; and continue.. reg1 is i/p mode status
        				;		 reg2 is ip mode lr
        MEND

; reg1,reg2 are same regs as provided to ToSVCE.. they will be restored on exit
        MACRO
        FromSVCE $reg1,$reg2     
        LDMFD   r13!,{r$reg1,lr}    	; recover i/p mode + flags,  svce lr 
        MSR     CPSR_c, r$reg1
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        LDMFD   r13!,{r$reg1,r$reg2,lr} ; restore r0,r1,lr in old mode
        MEND

; reg1,reg2 are same regs as provided to ToSVCE.. they will be restored on exit
; also returns to address in lr (subroutine exit)
        MACRO
        FromSVCERet $reg1,$reg2     
        LDMFD   r13!,{r$reg1,lr}    	; recover i/p mode + flags,  svce lr 
        MSR     CPSR_c, r$reg1
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        LDMFD   r13!,{r$reg1,r$reg2,pc} ; restore r0,r1,pc in old mode
        MEND


; go to svce mode, preserving all svce regs
; supplied regs are ones not passed in
        MACRO
        ToSVCEAll $reg1,$reg2
        STMFD   r13!,{r0-r12,lr} ; remember work regs & current mode
        				; on current mode stack
        MRS     r$reg1, CPSR           	; remember mode & flags
        ORR     r$reg2, r$reg1, #&3    ; IRQ26->SVC26, IRQ32->SVC32 
        MSR     CPSR_c, r$reg2
        
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        STMFD   r13!,{r0-r12,lr}    ; on svce stack
        				; and continue.. reg1 is i/p mode status
        MEND
        
; reg1,reg2 are same regs as provided to ToSVCEAll..
;  they will be restored on exit
; returns to address in lr to do subroutine exit
        MACRO
        FromSVCEAllRet $reg1,$reg2
        ldmfd   r13!,{r0-r12,lr}  
        MSR     CPSR_c, r$reg1
        MOV     r0,r0
        MOV     r0,r0
        MOV     r0,r0
        LDMFD   r13!,{r0-r12,pc} ; restore r0,r1,pc in old mode
        MEND

; c veneer entry macro
; on entry,ip -> structure with pointer to c module workspace in first word
; on exit a2 -> this structure, and ip -> c module workspace
        MACRO
        EnterCVeneer
        mov     sl,sp,lsr #20
        mov     sl,sl,lsl #20           ; get stack base
        ldmia   sl,{v1-v2}
        mov     a2,ip                   ; a2 = ptr to workspace structure
        ldr     ip,[a2]                 ; module workspace ptr to ip
        ldr     v3,[ip] 
        stmfd   sp!,{v1-v2}             ; remember existing c data offsets
        ldmib   v3,{v1-v2}              ; get this modules C data offsets
        stmia   sl,{v1-v2}              ; to stack base
        mov     fp,#0                   ; stop C backtrace here

; This is equivalent of 'ADD sl, sl, #0' + |_Lib$Reloc$Off$DP|
        DCD     |_Lib$Reloc$Off$DP| + &E28AA000
        MEND


; c veneer exit macro
        MACRO
        ExitCVeneer
        mov     sl,sp,lsr #20
        mov     sl,sl,lsl #20           ; get stack base
        ldmfd   sp!,{v1-v2}             ; restore chunk pointers
        stmia   sl,{v1-v2}
        MEND


        END
