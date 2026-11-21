/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#ifndef ASMTYPES_H
#define ASMTYPES_H

// defines

#define DEBUG			0

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

// symbol types

#define SYM_LABEL		1
#define SYM_MAP			2
#define SYM_CONST		3

#define MAX_BUFFER_SIZE 	1048576

// output targets

#define TARGET_NONE		0
#define TARGET_C64		1
#define TARGET_APPLEII		2
#define TARGET_ATARI800		3
#define TARGET_PRODOS		4

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

// a string from the source, with the escapes already decoded. It has a
// length because it can hold zero bytes (\0).

typedef struct {
		char		*s;
		int		len;
	} STRV;

typedef unsigned char	BUFFER[MAX_BUFFER_SIZE];

#endif
