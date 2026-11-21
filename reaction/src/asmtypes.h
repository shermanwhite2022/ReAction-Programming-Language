/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#ifndef ASMTYPES_H
#define ASMTYPES_H

// acc,imm,zp,zp:x,zp:y,abs,abs:x,abs:y,ind:indx_:ind_y:rel:imp

#define ACCUMULATOR		0
#define IMMEDIATE 	 	1
#define ZERO_PAGE 	 	2
#define ZERO_PAGE_X		3
#define ZERO_PAGE_Y		4
#define ABSOLUTE		5
#define ABSOLUTE_X		6
#define ABSOLUTE_Y		7
#define INDIRECT		8
#define INDIRECTX_		9
#define INDIRECT_Y		10	
#define RELATIVE		11
#define IMPLIED			12

#define MAX_OPCODES 		56

#define MAX_BUFFER_SIZE 	1048576

// typedefs for asm table

typedef char STRINGZ3[4];

typedef struct {
			STRINGZ3 	op;
			unsigned char	opcodes[13];
		} OP_REF;

typedef OP_REF OP_TABLE[MAX_OPCODES];

typedef struct {
		int		len;
		unsigned char	codes[4];
	} OP_OUT;

#endif
