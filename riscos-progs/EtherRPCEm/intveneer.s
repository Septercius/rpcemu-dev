; > s.intveneer

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
;


        AREA    |C$$data|,DATA

errbuf	DCD	&804d38
errmess	%	256

        AREA    |C$$code|,CODE,READONLY

	EXPORT	|networktxswi|
networktxswi
	STMFD	sp!,{v1, v2, lr}
	MOV	v2, a4
	MOV	v1, a3
	MOV	a4, a2
	MOV	a3, a1
	LDR	a2, =errmess
	MOV	a1, #0
	SWI	&56ac4
	TST	a1, #0
	LDRNE	a1, =errbuf
	LDMFD	sp!, {v1, v2, pc}

	EXPORT	|networkrxswi|
networkrxswi
	STMFD	sp!,{v1, lr}
	MOV	v1, a3
	MOV	a4, a2
	MOV	a3, a1
	LDR	a2, =errmess
	MOV	a1, #1
	SWI	&56ac4
	TEQ	a1, #0
	LDRNE	a1, =errbuf
	TEQ	v1, #0
	STRNE	a2, [v1]
	LDMFD	sp!, {v1, pc}

	EXPORT	|networkirqswi|
networkirqswi
	STMFD	sp!,{lr}
	MOV	a3, a1
	LDR	a2, =errmess
	MOV	a1, #2
	SWI	&56ac4
	TST	a1, #0
	LDRNE	a1, =errbuf
	LDMFD	sp!, {pc}

	EXPORT	|callrx|
callrx
	MOV	r12, r3
	MOV	pc, r2

        GET     h.Equates
        IMPORT  |_Lib$Reloc$Off$DP|
        IMPORT  |callback|
        EXPORT  |CallBkVeneer|

ggggx    DCB     "CallBkVeneer",0
        ALIGN
ggggy    DCD     &ff000000+ggggy-ggggx
; set up correct c veneer to call c routine for callback handling
; on entry, IRQ mode, IRQs off, r12 = pointer to workspace structure
CallBkVeneer
        ToSVCEAll  2,3
        EnterCVeneer
        mov     a1, a2              ; a1 = base device pointer
        bl      |callback|
        ExitCVeneer
        FromSVCEAllRet 2,3

        EXPORT  |CallEveryVeneer|
        IMPORT  |callevery_handler|
        IMPORT  |irqstatus|


gggga    DCB     "CallEveryVeneer",0
        ALIGN
ggggb    DCD     &ff000000+ggggb-gggga
CallEveryVeneer
        ToSVCEAll  2,3
        EnterCVeneer
        stmfd   sp!, {a1-a2}

        ldr     a1, =|irqstatus|
        mov     a2, #0
        str     a2, [a1]

        MOV     a3, #0
        LDR     a2, =errmess
        MOV     a1, #3
        SWI     &56ac4

        ldmfd   sp!, {a1-a2}
        mov     a1, a2              ; a1 = base device pointer
        bl      |callevery_handler|
        ExitCVeneer
        FromSVCEAllRet 2,3

        END
