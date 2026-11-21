/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "asm.h"
#include "asmtypes.h"

// assembly instruction length (in bytes) for each addressing mode

int _t6502len[13] = {1,2,2,2,2,3,3,3,3,2,2,2,1};

static const char *_modeNames[13] = {
	"accumulator", "immediate", "zero page", "zero page,x", "zero page,y",
	"absolute", "absolute,x", "absolute,y", "indirect", "(indirect,x)",
	"(indirect),y", "relative", "implied"
};

// assembly table. This table must stay in alphabetical order, getOpcode()
// does a binary search on it.

OP_TABLE _t6502opcodes = {

//opc ,  acc   imm   zp    zp,x  zp,y  abs   abs,x abs,y ind   indx_ ind_y rel   imp

{"adc", {0xff, 0x69, 0x65, 0x75, 0xff, 0x6d, 0x7d, 0x79, 0xff, 0x61, 0x71, 0xff, 0xff}},
{"and", {0xff, 0x29, 0x25, 0x35, 0xff, 0x2d, 0x3d, 0x39, 0xff, 0x21, 0x31, 0xff, 0xff}},
{"asl", {0x0a, 0xff, 0x06, 0x16, 0xff, 0x0e, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"bcc", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x90, 0xff}},
{"bcs", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb0, 0xff}},
{"beq", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff}},
{"bit", {0xff, 0xff, 0x24, 0xff, 0xff, 0x2c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"bmi", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x30, 0xff}},
{"bne", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd0, 0xff}},
{"bpl", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x10, 0xff}},
{"brk", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}}, 
{"bvc", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x50, 0xff}},
{"bvs", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x70, 0xff}},
{"clc", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x18}}, 
{"cld", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xd8}},
{"cli", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x58}},
{"clv", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb8}},
{"cmp", {0xff, 0xc9, 0xc5, 0xd5, 0xff, 0xcd, 0xdd, 0xd9, 0xff, 0xc1, 0xd1, 0xff, 0xff}},
{"cpx", {0xff, 0xe0, 0xe4, 0xff, 0xff, 0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"cpy", {0xff, 0xc0, 0xc4, 0xff, 0xff, 0xcc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"dec", {0xff, 0xff, 0xc6, 0xd6, 0xff, 0xce, 0xde, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"dex", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xca}},
{"dey", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x88}},
{"eor", {0xff, 0x49, 0x45, 0x55, 0xff, 0x4d, 0x5d, 0x59, 0xff, 0x41, 0x51, 0xff, 0xff}},
{"inc", {0xff, 0xff, 0xe6, 0xf6, 0xff, 0xee, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"inx", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe8}},
{"iny", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc8}},
{"jmp", {0xff, 0xff, 0xff, 0xff, 0xff, 0x4c, 0xff, 0xff, 0x6c, 0xff, 0xff, 0xff, 0xff}}, 
{"jsr", {0xff, 0xff, 0xff, 0xff, 0xff, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"lda", {0xff, 0xa9, 0xa5, 0xb5, 0xff, 0xad, 0xbd, 0xb9, 0xff, 0xa1, 0xb1, 0xff, 0xff}},
{"ldx", {0xff, 0xa2, 0xa6, 0xff, 0xb6, 0xae, 0xff, 0xbe, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"ldy", {0xff, 0xa0, 0xa4, 0xb4, 0xff, 0xac, 0xbc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"lsr", {0x4a, 0xff, 0x46, 0x56, 0xff, 0x4e, 0x5e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"nop", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xea}},
{"ora", {0xff, 0x09, 0x05, 0x15, 0xff, 0x0d, 0x1d, 0x19, 0xff, 0x01, 0x11, 0xff, 0xff}},
{"pha", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x48}},
{"php", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08}},
{"pla", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x68}},
{"plp", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x28}},
{"rol", {0x2a, 0xff, 0x26, 0x36, 0xff, 0x2e, 0x3e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"ror", {0x6a, 0xff, 0x66, 0x76, 0xff, 0x6e, 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"rti", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x40}},
{"rts", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x60}},
{"sbc", {0xff, 0xe9, 0xe5, 0xf5, 0xff, 0xed, 0xfd, 0xf9, 0xff, 0xe1, 0xf1, 0xff, 0xff}},
{"sec", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x38}},
{"sed", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8}},
{"sei", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78}},
{"sta", {0xff, 0xff, 0x85, 0x95, 0xff, 0x8d, 0x9d, 0x99, 0xff, 0x81, 0x91, 0xff, 0xff}},
{"stx", {0xff, 0xff, 0x86, 0xff, 0x96, 0x8e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"sty", {0xff, 0xff, 0x84, 0x94, 0xff, 0x8c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}},
{"tax", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xaa}},
{"tay", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xa8}},
{"tsx", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xba}},
{"txa", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8a}},
{"txs", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9a}},
{"tya", {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98}}};


// report an error with the current file and line number

void asmError (const char *fmt, ...) {
	va_list ap;

	fprintf(stderr, "%s:%d: error: ", _filename ? _filename : "<input>", _lineno);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (macroContext())
		fprintf(stderr, " (in macro %s)", macroContext());

	fprintf(stderr, "\n");
	printIncludeChain(stderr);

	_errors++;
}

// remember the range of memory written so the output file can be sized

static void noteWrite (int pc, int len) {
	if (len <= 0)
		return;

	if (pc < _minaddr)
		_minaddr = pc;

	if (pc + len - 1 > _maxaddr)
		_maxaddr = pc + len - 1;

	memset(&_written[pc], 1, len);
	listBytes(pc, len);
}

static int outOfRange (int pc, int len) {
	if (pc < 0 || pc + len > _memsize) {
		asmError("address $%x is outside of memory (size is $%x)", pc, _memsize);
		return 1;
	}
	return 0;
}

// look up the opcode for a mnemonic in an addressing mode. Returns 0xff if
// there is no such instruction (0xff is not a 6502 opcode).

int getOpcode (char *opc, int addr_mode) {

	int lo;
	int hi;
	int m;
	int c;
	int i;

	STRINGZ3 needle;

	if (strlen(opc) != 3 || addr_mode < 0 || addr_mode > IMPLIED)
		return (0xff);

	for (i= 0; i < 3; i++)
		needle[i]= (char) tolower((unsigned char) opc[i]);
	needle[3]= 0;

	lo= 0;
	hi= MAX_OPCODES-1;

	while (lo <= hi) {
		m= (lo+hi)/2;
		c= strcmp(needle, _t6502opcodes[m].op);

		if (!c) {
			if (_debug)
				printf ("DBG: found %s (%s) @%d\n", needle, _t6502opcodes[m].op, m);
			return (_t6502opcodes[m].opcodes[addr_mode]);
		}

		if (c < 0)
			hi= m-1;
		else
			lo= m+1;
	}

	return (0xff);
}

int genAssembly (char *opc, int addr_mode, int operand, OP_OUT *iptr) {

	int opcode;
	unsigned char lobyte = operand & 0xff;
	unsigned char hibyte = (operand >> 8) & 0xff;

	if ((addr_mode < 0) || (addr_mode > IMPLIED))
		return -1;

	opcode= getOpcode(opc, addr_mode);

	if (opcode == 0xff)
		return -1;

	iptr->len= _t6502len[addr_mode];
	iptr->codes[0]= opcode;
	iptr->codes[1]= lobyte;
	iptr->codes[2]= hibyte;

	return (iptr->len);
}

char emitStr[16];

char *genAssemblyOut (OP_OUT *iptr) {

	int i;
	char tmpStr[8];

	strcpy(emitStr, "");

	for (i=0; i<iptr->len && i<3; i++) {
		sprintf(tmpStr, "%02x ", iptr->codes[i]);
		strcat(emitStr, tmpStr);
	}

	return emitStr;
}

// encode one instruction at pc and store it in the memory image. The operand
// is used as is (branches must already be converted to an offset).
// Returns the instruction length, or -1.

int emitAssemblyCode(int pc, char *opc, int operand, int addr_mode, unsigned char *membuf) {
	OP_OUT emit;

	if (genAssembly(opc, addr_mode, operand, &emit) == -1)
		return -1;

	if (outOfRange(pc, emit.len))
		return -1;

	memcpy(&membuf[pc], &emit.codes[0], emit.len);
	noteWrite(pc, emit.len);

	return (emit.len);
}

// assemble an instruction at the current pc. This is where the operand is
// checked and the addressing mode is settled:
//
//  - zero page modes are used when the value fits in a byte and was already
//    defined, otherwise the absolute form is used. A forward reference
//    always gets the absolute form, so that the instruction has the same
//    size in both passes.
//  - an instruction with only one of the two forms (jmp, stx zp,y) gets that one.
//  - a branch is given the target address, and the offset is worked out here.
//
// Returns the instruction length, or -1.

int asmOp (char *opc, int val, int mode, int fwd) {

	int zp;
	int ab;
	int want;
	int alt;
	int off;
	int len;

	switch (mode) {

	case ZERO_PAGE:
	case ZERO_PAGE_X:
	case ZERO_PAGE_Y:
		zp= mode;
		ab= mode + (ABSOLUTE - ZERO_PAGE);

		if (!fwd && val >= 0 && val <= 0xff) {
			want= zp;
			alt= ab;
		} else {
			want= ab;
			alt= zp;
		}

		if (getOpcode(opc, want) != 0xff)
			mode= want;
		else if (getOpcode(opc, alt) != 0xff)
			mode= alt;
		else
			mode= want;
		break;

	case RELATIVE:
		off= val - (_pc + 2);

		if (_pass == 1 && (off < -128 || off > 127)) {
			asmError("branch target $%x is out of range (%d bytes)", val, off);
			off= 0;
		}

		val= off & 0xff;
		break;
	}

	if (getOpcode(opc, mode) == 0xff) {
		asmError("%s has no %s mode", opc, _modeNames[mode]);
		return -1;
	}

	if (_pass == 1) {
		switch (mode) {

		case IMMEDIATE:
			if (val < -128 || val > 255)
				asmError("immediate value %d does not fit in a byte", val);
			break;

		case ZERO_PAGE:
		case ZERO_PAGE_X:
		case ZERO_PAGE_Y:
		case INDIRECTX_:
		case INDIRECT_Y:
			if (val < 0 || val > 0xff)
				asmError("zero page address $%x is out of range", val);
			break;

		case ABSOLUTE:
		case ABSOLUTE_X:
		case ABSOLUTE_Y:
		case INDIRECT:
			if (val < 0 || val > 0xffff)
				asmError("address $%x is out of range", val);
			break;
		}
	}

	len= emitAssemblyCode(_pc, opc, val, mode, _membuf);

	return len;
}

int isBranch (char *s) {
	static const char *branches[] = {"beq", "bne", "bcs", "bcc", "bvs", "bvc", "bmi", "bpl"};
	int i;

	for (i= 0; i < 8; i++)
		if (!strcasecmp(s, branches[i]))
			return 1;

	return 0;
}

int isAccumulator (char *s) {
	static const char *shifts[] = {"asl", "lsr", "rol", "ror"};
	int i;

	for (i= 0; i < 4; i++)
		if (!strcasecmp(s, shifts[i]))
			return 1;

	return 0;
}

// Escapes in strings and in 'c' characters: a backslash and
//   \n 10   \r 13   \t 9   \0 0   \a 7   \e 27   \\ \" \'  (the character itself)
//   \xHH   the byte with that hex value
// The assembler does not translate characters, so \n is plain 10, not the
// end of line character of the C64 (13) or the Atari (155): use \r or \x9b.
// p points just after the backslash; the escape is taken off it. Returns the
// value, or -1 (after reporting it) if the escape is wrong.

static int decodeEscape (const char **p) {
	char c = **p;
	int v;

	if (!c) {
		asmError("a string can not end with a backslash");
		return -1;
	}

	(*p)++;

	switch (c) {
	case 'n':	return 10;
	case 'r':	return 13;
	case 't':	return 9;
	case '0':	return 0;
	case 'a':	return 7;
	case 'e':	return 27;
	case '\\':	return '\\';
	case '"':	return '"';
	case '\'':	return '\'';
	case 'x':
	case 'X':
		if (isxdigit((unsigned char) (*p)[0]) && isxdigit((unsigned char) (*p)[1])) {
			char hex[3] = { (*p)[0], (*p)[1], 0 };

			v = (int) strtol(hex, NULL, 16);
			*p += 2;
			return v;
		}

		asmError("\\x in a string needs two hex digits");
		return -1;
	}

	asmError("unknown escape '\\%c' in a string (use \\\\ for a backslash)", c);
	return -1;
}

// get the text out of a quoted string token, with the escapes decoded.
// mode 2 is a "double quoted" string, mode 1 is a 'single quoted' string.

STRV *getString(int mode, char *s) {
	size_t n= strlen(s);
	char quote= (mode == 2) ? '"' : '\'';
	STRV *r= (STRV *) malloc(sizeof(STRV));
	const char *p;
	const char *end;
	int v;

	r->s= (char *) malloc(n + 1);
	r->len= 0;
	r->s[0]= 0;

	if (n < 2 || s[0] != quote || s[n-1] != quote) {
		asmError("unmatched or unterminated string quotes");
		return r;
	}

	p= s + 1;
	end= s + n - 1;

	while (p < end) {
		if (*p == '\\') {
			p++;
			v= decodeEscape(&p);

			if (v < 0)
				continue;
		} else {
			v= (unsigned char) *p++;
		}

		r->s[r->len++]= (char) v;
	}

	r->s[r->len]= 0;
	return r;
}

// the value of a 'c' character in an expression

int charLiteral (char *s) {
	STRV *r= getString(1, s);
	int v= r->len ? (unsigned char) r->s[0] : 0;

	free(r->s);
	free(r);
	return v;
}

int clearMemory(int pc, int len, unsigned char *membuf) {

	if (len < 0) {
		asmError("negative size %d", len);
		return -1;
	}

	if (outOfRange(pc, len))
		return -1;

	memset (&membuf[pc], 0, len);
	noteWrite(pc, len);

	return (len);
}

// store a value of size bytes (1 or 2), low byte first

int putData (int pc, int size, int val, unsigned char *membuf) {
	int i;

	if (outOfRange(pc, size))
		return -1;

	if (_pass == 1) {
		if (size == 1 && (val < -128 || val > 255))
			asmError("value %d does not fit in a byte", val);
		else if (size == 2 && (val < -32768 || val > 65535))
			asmError("value %d does not fit in a word", val);
	}

	for (i= 0; i < size; i++)
		membuf[pc+i]= (unsigned char) ((val >> (8*i)) & 0xff);

	noteWrite(pc, size);

	return (size);
}

// store a string using the current string mode, then pad with zeros to a
// multiple of size bytes. Returns the number of bytes used.

int putString(int pc, int smode, int size, STRV *str, unsigned char *membuf) {
	const char *s= str->s;
	int n= str->len;
	int total;
	int pad;
	int i;
	int pos;

	switch (smode) {
	case 0:
		total= n+1;
		break;
	case 1:
		if (n > 255) {
			asmError("string is too long for a length prefixed string");
			return -1;
		}
		total= n+1;
		break;
	case 2:
		total= n;
		break;
	default:
		asmError("invalid string mode %d", smode);
		return -1;
	}

	if (size < 1)
		size= 1;

	pad= (size - (total % size)) % size;

	if (outOfRange(pc, total+pad))
		return -1;

	pos= pc;

	if (smode == 1)
		membuf[pos++]= (unsigned char) n;

	for (i= 0; i < n; i++)
		membuf[pos++]= (unsigned char) s[i];

	if (smode == 0)
		membuf[pos++]= 0;

	for (i= 0; i < pad; i++)
		membuf[pos++]= 0;

	noteWrite(pc, total+pad);

	return (total+pad);
}

void setOrigin (int addr) {
	if (addr < 0 || addr >= _memsize)
		asmError("origin $%x is outside of memory (size is $%x)", addr, _memsize);
	else {
		_pc= addr;
		_pcSet= 1;
	}
}

void setMemSize (int size) {
	if (size < 1 || size > MAX_BUFFER_SIZE)
		asmError("memory size must be between 1 and %d", MAX_BUFFER_SIZE);
	else
		_memsize= size;
}

// .target name: choose the output format, unless it was set on the command line

void setTarget (char *name) {
	int t = targetFromName(name);

	if (t < 0)
		asmError("unknown target '%s' (use c64, appleii or atari800)", name);
	else if (!_targetLocked) {
		// with no .org yet the program counter goes to the default for the new target
		if (!_pcSet && _pc == defaultOrigin(_target))
			_pc = defaultOrigin(t);

		_target = t;
	}
}

// .run address: where the program starts. Only the pass 1 value counts,
// so the address can be a label that is defined later.

void setEntry (int addr) {
	if (_pass != 1)
		return;

	if (addr < 0 || addr > 0xffff)
		asmError("run address $%x is out of range", addr);
	else
		_entry = addr;
}

// << and >>. The count has to be 0 to 31. A wrong count is only reported in
// the second pass, because in the first a forward reference counts as 0.
// >> keeps the sign (it is an arithmetic shift).

static int shiftCount (int count, const char *op) {
	if (count >= 0 && count <= 31)
		return count;

	if (_pass == 1)
		asmError("the count of %s must be between 0 and 31 (it is %d)", op, count);

	return 0;
}

int shiftLeft (int value, int count) {
	return (int) ((unsigned) value << shiftCount(count, "<<"));
}

int shiftRight (int value, int count) {
	int n = shiftCount(count, ">>");

	return value < 0 ? ~(~value >> n) : value >> n;
}
