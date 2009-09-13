#ifndef CODEGEN_X86_COMMON_H
#define CODEGEN_X86_COMMON_H

/* x86 registers (bitfields for instruction encoding) */
#define EAX	0x00
#define ECX	0x08
#define EDX	0x10
#define EBX	0x18
#define ESP	0x20
#define EBP	0x28
#define ESI	0x30
#define EDI	0x38

/* Basic x86 operations (bitfields for instruction encoding) */
#define X86_OP_ADD	0x00
#define X86_OP_OR	0x08
#define X86_OP_ADC	0x10	/* Add with carry */
#define X86_OP_SBB	0x18	/* Subtract with borrow */
#define X86_OP_AND	0x20
#define X86_OP_SUB	0x28
#define X86_OP_XOR	0x30
#define X86_OP_CMP	0x38

#endif /* CODEGEN_X86_COMMON_H */

