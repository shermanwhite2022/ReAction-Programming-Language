/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "storage.h" 

int init_emit() {
// initialize storage and other needed libraries

	init_compiler_variables();
	init_typedef_defaults();
	_predeclare_sysmem();
}

/* ---- expression types ------------------------------------------------------
 * The parser calls the emit_ routines in postfix order, so a compile time
 * mirror of the run time stack tells each operator what it is working on:
 * 'I' is a 16 bit integer (2 bytes on the run time stack), 'R' a REAL (4 bytes).
 */
#include <math.h>

static char tstack[65536];
static char tstr[65536];		/* a string address: 1 STRING, 2 ZTRING, 8 a plain BYTE ARRAY, +16 the result of a + */
static int  trec[65536];		/* for an 'A' item: the record type it is the address of */
static int  tsp= 0;

/* Expression types on the compile time stack:
 *   'B' BYTE   'C' CARD (unsigned 16 bit)   'I' INT (signed 16 bit)
 *   'K' a constant 256..32767, 'Z' a constant 0..255 (take on the type of what they are combined with)
 * An operation with a BYTE on one side and a BYTE or a constant 0..255 on the other is done
 * in 8 bits: the result wraps at 256 and is a BYTE, as in Action!.
 *   'R' REAL
 * An integer operation is signed if an operand is an INT and none is a CARD (Action!: any CARD makes it a CARD). */
static int vtc(int typeidx) {
	const char *n= _type_name_of_index(typeidx);

	if (!strcasecmp(n, "byte")) return 'B';
	if (!strcasecmp(n, "int"))  return 'I';
	if (!strcasecmp(n, "real")) return 'R';
	return 'C';
}

static int const_type(int v) { return (v < 0)? 'I': (v > 32767)? 'C': (v > 255)? 'K': 'Z'; }

static void tpush(int t) {
	if (tsp >= (int)sizeof(tstack)) { printf("ERR! expression too deep\n"); exit(1); }
	trec[tsp]= -1;
	tstr[tsp]= 0;
	tstack[tsp++]= t;
}

static void tpushrec(int rectype) {
	tpush('A');
	trec[tsp-1]= rectype;
}

/* STRING and ZTRING variables: a BYTE array of capacity+1 bytes that the compiler knows holds a string */
static struct { char label[300]; int kind, cap; } strv[512];
static int nstrv= 0;

static int strkind_of(char *label) {
	int i;
	for (i= 0; i < nstrv; i++) if (!strcmp(strv[i].label, label)) return strv[i].kind;
	return 0;
}

static int strcap_of(char *label) {
	int i;
	for (i= 0; i < nstrv; i++) if (!strcmp(strv[i].label, label)) return strv[i].cap;
	return 255;
}

static int tpeek();

static int tpop() {
	if (tsp <= 0) return 'I';		/* statements that are not emitted yet leave nothing to pop */
	return tstack[--tsp];
}

static int tpeek() { return (tsp > 0)? tstack[tsp-1]: 'C'; }

/* A constant push is held back until the next thing is emitted, so that a
 * following unary minus can fold into it (-5 is the constant -5, not 5 then
 * NEG16S) and an assignment can convert it for free.  Every routine that writes
 * code calls flush_const() first. */
static int    pend= 0;			/* 0 nothing held, 1 integer, 2 real */
static int    pendi= 0;
static double pendf= 0;

void fp_encode(double v, unsigned char b[4]);

static void flush_const() {
	extern FILE *outf;
	unsigned char b[4];

	if (pend == 1) {
		fprintf(outf, "; Push CONSTANT = (%x) %d\n", pendi & 0xffff, pendi);
		fprintf(outf, "\t\t_PUSHI $%04x\n", pendi & 0xffff);
	} else if (pend == 2) {
		fp_encode(pendf, b);
		fprintf(outf, "; Push FLOAT CONSTANT = %.9g\n", pendf);
		fprintf(outf, "\t\t_PUSHFLOAT $%02x,$%02x,$%02x,$%02x\n", b[0], b[1], b[2], b[3]);
	}
	pend= 0;
}

/* 4 byte real: exponent+128, then a 24 bit two's complement mantissa in
 * [1,2) (negative: [-2,-1)) with the binary point after bit 22 */
void fp_encode(double v, unsigned char b[4]) {
	int e;
	long mi;
	double m;

	if (v == 0.0) { b[0]= b[1]= b[2]= b[3]= 0; return; }

	m= frexp(fabs(v), &e) * 2.0;		/* 1 <= m < 2 */
	e-= 1;
	mi= (long)floor(m * 4194304.0 + 0.5);	/* 2^22 */
	if (mi >= 8388608L) { mi>>= 1; e++; }	/* rounded up to 2.0 */
	if (v < 0) {
		if (mi == 4194304L) { mi= 8388608L; e--; }	/* -2^e: canonical form is -2.0 * 2^(e-1) */
		mi= (-mi) & 0xffffff;
	}
	if (e + 128 < 0)   { b[0]= b[1]= b[2]= b[3]= 0; return; }	/* underflow: 0.0 */
	if (e + 128 > 255) { printf("ERR! real constant %g out of range\n", v); exit(1); }
	b[0]= e + 128;
	b[1]= (mi >> 16) & 255;
	b[2]= (mi >> 8) & 255;
	b[3]= mi & 255;
}

/* ---------------------------------------------------------------------------
 * Output layout (6502asm syntax):
 *
 *   header   .org, .title, .map of zero page names, macro include, stack setup
 *   code     everything the grammar emits while parsing
 *   footer   rts, variable storage (collected in datf), runtime library, .run
 *
 * Variable storage is written to a temporary file while parsing and copied in
 * by emit_footer(), so declarations never end up in the middle of the code.
 * ------------------------------------------------------------------------- */

extern int _zero_block;			/* rc -Z: store the zero filled variables in the program file instead of reserving them */
static FILE *datf= NULL;
static FILE *zerf= NULL;		/* variables that start as zero: reserved with .alloc, cleared when the program starts */
static int zero_vars= 0;

/* reserve size bytes under label (a comment says what it is); nothing is stored in the program file for them */
static void zvar(const char *label, int size, const char *fmt, ...) {
	va_list ap;
	FILE *f;
	if (!_zero_block) {
		f= datf;				/* the old way: the zeros are part of the file */
		fprintf(f, "%s\t\t.bzero %d\t\t; ", label, size);
	} else {
		if (!zerf) zerf= tmpfile();
		f= zerf;
		zero_vars++;
		fprintf(f, "%s\t\t.alloc %d\t\t; ", label, size);
	}
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
	fprintf(f, "\n");
}
static char *qual(char *ident);
static void vt_label(char *out, char *tname) {		/* VT_<CLASS>: the class's method table */
	int i;
	strcpy(out, "VT_");
	for (i= 0; tname[i]; i++)
		out[3+i]= (tname[i] >= 'a' && tname[i] <= 'z')? tname[i]-32: tname[i];
	out[3+i]= 0;
}
static int  push_field_addr(char *cname);
static int  data_org_set= 0;
static int  data_org= 0;
static char *entry_label();
static void check_open_blocks();
static void emit_vtables(FILE *out);
struct routine_s;
static void note_call(struct routine_s *to, int virt, int vcls);
static void check_recursion();
static int _unused_proto;
extern int _prune;
extern int _stack_top;			/* rc -S: where the run time stack starts, -1 for the machine's default */
static int need_io= 0;
static int need_gr= 0;			/* ... or from gr.inc (graphics and sound) */
static int need_misc= 0;			/* ... or one from misc.inc */			/* an I/O routine from io.inc is called */

int emit_header(char *title, int origin) {
	extern FILE *outf;

	datf= tmpfile();

	fprintf(outf,"\t\t.title \"%s\"\n", title);
	if (origin == 0x2000) {		/* the default: on the Apple II the hi-res pages are at $2000 and $4000, so the program goes above them */
		fprintf(outf,"\t\t.if TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS\n\t\t.org $6000\n\t\t.else\n\t\t.org $2000\n\t\t.endif\n");
	} else fprintf(outf,"\t\t.org $%04x\n", origin);
	fprintf(outf,"\n");
	fprintf(outf,"; zero page: RUNSTK = run time stack pointer, TMPA..TMPC = scratch,\n");
	fprintf(outf,"; SGNQ/SGNR = sign flags for division, FPZ = the floating point block (55 bytes).\n");
	fprintf(outf,"; Where they go depends on the machine: the C64 KERNAL and the Apple II monitor use\n");
	fprintf(outf,"; parts of the zero page that the Atari leaves free.\n");
	fprintf(outf,"\t\t.if TARGET == TARGET_C64\n");
	fprintf(outf,"\t\t.const FPZ $2b\t\t; $2b-$61 and $62-$6b: BASIC's area, which the KERNAL does not use\n");
	fprintf(outf,"\t\t.map RUNSTK @ $62\n\t\t.map TMPA @ $64\n\t\t.map TMPB @ $66\n\t\t.map TMPC @ $68\n");
	fprintf(outf,"\t\t.map SGNQ @ $6a\n\t\t.map SGNR @ $6b\n");
	fprintf(outf,"\t\t.elif TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS\n");
	fprintf(outf,"\t\t.const FPZ $50\t\t; $50-$86 and $87-$90: Applesoft's area, so no return to BASIC\n");
	fprintf(outf,"\t\t.map RUNSTK @ $87\n\t\t.map TMPA @ $89\n\t\t.map TMPB @ $8b\n\t\t.map TMPC @ $8d\n");
	fprintf(outf,"\t\t.map SGNQ @ $8f\n\t\t.map SGNR @ $90\n");
	fprintf(outf,"\t\t.else\t\t\t\t; Atari 800, or no target\n");
	fprintf(outf,"\t\t.const FPZ $a0\n");
	fprintf(outf,"\t\t.map RUNSTK @ $8e\n\t\t.map TMPA @ $90\n\t\t.map TMPB @ $92\n\t\t.map TMPC @ $94\n");
	fprintf(outf,"\t\t.map SGNQ @ $96\n\t\t.map SGNR @ $97\n");
	fprintf(outf,"\t\t.endif\n");
	fprintf(outf,"\n; run time stack: it starts below STKPTR and grows down.  Where that is depends on the machine (the top of\n");
	fprintf(outf,"; the RAM the program can have), rc -S sets it, and assembling with -D STKPTR=address overrides both.\n");
	fprintf(outf,"\t\t.ifndef STKPTR\n");
	if (_stack_top >= 0) {
		fprintf(outf,"\t\t.const STKPTR $%04x\t\t; rc -S\n", _stack_top);
	} else {
		fprintf(outf,"\t\t.if TARGET == TARGET_C64\n\t\t.const STKPTR $a000\t\t; below BASIC's ROM\n");
		fprintf(outf,"\t\t.elif TARGET == TARGET_APPLEII || TARGET == TARGET_PRODOS\n\t\t.const STKPTR $9600\t\t; below DOS and BASIC.SYSTEM\n");
		fprintf(outf,"\t\t.else\n\t\t.const STKPTR $8000\t\t; Atari 800, or no target\n\t\t.endif\n");
	}
	fprintf(outf,"\t\t.endif\n");
	fprintf(outf,"\t\t.map SYSMEM @ $0000\t\t; SYSMEM(a) is the byte at address a\n");
	fprintf(outf,"\n");
	fprintf(outf,"\t\t.include \"includes/mdefs.inc\"\t; macro definitions (no code)\n");
	fprintf(outf,"\n");
//
// eventually the stack pointer location will be configurable
//
	fprintf(outf,"; initialize stack pointer @$4000\n");
	fprintf(outf,"\n");
	fprintf(outf,"P_INIT0\t\tlda #<STKPTR\n");
	fprintf(outf,"\t\tsta RUNSTK\n");
	fprintf(outf,"\t\tlda #>STKPTR\n");
	fprintf(outf,"\t\tsta RUNSTK+1\n");
	if (_zero_block) {
	fprintf(outf,"\t\t; clear the variables that start as zero: ZBSTART to ZBEND, whole pages first\n");
	fprintf(outf,"\t\tlda #<ZBSTART\n\t\tsta TMPA\n\t\tlda #>ZBSTART\n\t\tsta TMPA+1\n");
	fprintf(outf,"\t\tsec\n\t\tlda #<ZBEND\n\t\tsbc #<ZBSTART\n\t\tsta TMPB\n\t\tlda #>ZBEND\n\t\tsbc #>ZBSTART\n\t\tsta TMPB+1\n");
	fprintf(outf,"\t\tldy #0\n\t\tlda #0\n");
	fprintf(outf,"PZ1\t\tldx TMPB+1\n\t\tbeq PZ3\n");
	fprintf(outf,"PZ2\t\tsta (TMPA),y\n\t\tiny\n\t\tbne PZ2\n\t\tinc TMPA+1\n\t\tdec TMPB+1\n\t\tjmp PZ1\n");
	fprintf(outf,"PZ3\t\tcpy TMPB\n\t\tbeq PZ9\n\t\tsta (TMPA),y\n\t\tiny\n\t\tbne PZ3\n");
	fprintf(outf,"PZ9\n");
	}
	fprintf(outf,"\t\tlda #<HEAPSTART\t\t; NEW allocates from the end of the program\n");
	fprintf(outf,"\t\tsta HEAPPTR\n");
	fprintf(outf,"\t\tlda #>HEAPSTART\n");
	fprintf(outf,"\t\tsta HEAPPTR+1\n");
	fprintf(outf,"\t\tjmp RXENTRY\t\t; execution starts at the last routine\n");
	return (0);
}

/* called once after yyparse(): end of program, then data, then the runtime */
int emit_footer() {
	extern FILE *outf;
	int c;

	flush_const();
	check_open_blocks();
	check_recursion();

	fprintf(outf,"\n; ---- program entry: the last PROC/FUNC in the source\n");
	fprintf(outf,"RXENTRY");
	{
		char *e= entry_label();
		if (e) fprintf(outf,"\t\tjsr %s\n", e);
		else   fprintf(outf,"\t\tnop\n");
	}
	fprintf(outf,"\t\trts\n\n");
	fprintf(outf,";@mode prog\n");
	fprintf(outf,"; ---- variables\n");
	if (data_org_set) fprintf(outf,"\t\t.org $%04x\t\t; SET $491: variable origin\n", data_org);
	emit_vtables(outf);
	if (datf) {
		rewind(datf);
		while ((c= fgetc(datf)) != EOF) fputc(c, outf);
		fclose(datf);
		datf= NULL;
	}
	fprintf(outf,";@mode lib\n");
	fprintf(outf,"\n; ---- runtime library\n");
	fprintf(outf,"\t\t.include \"includes/rtl.inc\"\n");
	fprintf(outf,"\t\t.include \"includes/fp.inc\"\n");
	if (need_io || _prune) fprintf(outf,"\t\t.include \"includes/io.inc\"\n");	/* with dead code removal it costs nothing when unused */
	if (need_misc || _prune) fprintf(outf,"\t\t.include \"includes/misc.inc\"\n");
	if (need_gr || _prune) fprintf(outf,"\t\t.include \"includes/gr.inc\"\n");
	fprintf(outf,";@mode root\n");
	fprintf(outf,"\n; ---- variables that start as zero: reserved here, nothing is stored in the program file;\n");
	fprintf(outf,"; the start of the program clears them (P_INIT0)\nZBSTART\n");
	fprintf(outf,";@mode prog\n");
	if (zerf) {
		rewind(zerf);
		while ((c= fgetc(zerf)) != EOF) fputc(c, outf);
		fclose(zerf);
		zerf= NULL;
	}
	fprintf(outf,";@mode root\n");
	fprintf(outf,"ZBEND\n");
	fprintf(outf,"\n; ---- start of the NEW heap\nHEAPSTART\n");
	fprintf(outf,"\n; the run time stack must not be inside the program (it grows down from STKPTR)\n");
	fprintf(outf,"\t\t.if ((HEAPSTART > STKPTR-64) & (P_INIT0 < STKPTR))\n");
	fprintf(outf,"\t\t.include \"ERROR the program reaches the run time stack: assemble with rc -S address, above the end of the program\"\n");
	fprintf(outf,"\t\t.endif\n");
	fprintf(outf,"\n\t\t.run P_INIT0\n");
	return (0);
}

int emit_data() {
	return (0);
}

int emit_init_assignment(char *ident, void *value, char *constType) {
	return (0);
}

/* Reserve storage (or, for an address, a name) for a variable.
 *   init < 0 && !mapped  : n zero bytes
 *   mapped               : no storage, name = address given in value
 *   otherwise            : initialised with value
 */
static int emit_storage(char *ident, char *type, int has_value, int value, int mapped) {
	int t_size;
	char *cname;

	t_size= _type_sizeof(type);

	if (t_size < 0) {
		printf("ERR! emit_declaration: %s: %s unknown type/storage!\n", ident, type);
		exit(1);
	}

	if (_in_record()) {			/* inside TYPE name=[ ... ]: a field, not a variable */
		if (_type_is_record(_type_index_of(type))) {
			printf("ERR! %s: a record or class can't be a field of another one, use a %s POINTER!\n", ident, type);
			exit(1);
		}
		if (has_value || mapped) {
			printf("ERR! %s: a TYPE field can't have an initial value or an address!\n", ident);
			exit(1);
		}
		{ int rr= _record_add_field(ident, t_size, 0, -1); _record_set_ftype(_type_index_of(type)); return rr; }
	}

	if (_type_is_class(_type_index_of(type))) {		/* a class variable is a reference to an instance */
		static int instcnt= 0;
		int csize= t_size > 0? t_size: 1;

		if (has_value || mapped) {
			printf("ERR! %s: a class variable can't have an initial value or an address!\n", ident);
			exit(1);
		}
		_valloc(ident, "card");
		cname= _get_compiler_var(ident);
		if (!datf) datf= tmpfile();
		{
			char vt[100];
			vt_label(vt, type);
			fprintf(datf, "INST%05d\t\t.word %s\t\t; instance of %s for %s\n", ++instcnt, vt, type, ident);
			if (csize > 2) fprintf(datf, "\t\t.bzero %d\n", csize - 2);
		}
		fprintf(datf, "%s\t\t.word INST%05d\t\t; %s (reference to a %s)\n", cname, instcnt, ident, type);
		_set_deref(ident, csize, _type_index_of(type));
		return (0);
	}

	if (has_value && !mapped && _type_is_record(_type_index_of(type))) {
		printf("ERR! %s: a record variable can't have an initial value!\n", ident);
		exit(1);
	}

	_valloc(ident, type);
	cname= _get_compiler_var(ident);

	if (!datf) datf= tmpfile();

	if (mapped) {
		fprintf(datf, "\t\t.map %s @ $%04x\t; %s (%s) at fixed address\n", cname, value & 0xffff, ident, type);
	} else if (!has_value) {
		zvar(cname, t_size, "%s (%s)", ident, type);
	} else if (t_size == 4) {
		unsigned char b[4];
		fp_encode((double)value, b);
		fprintf(datf, "%s\t\t.byte $%02x,$%02x,$%02x,$%02x\t; %s (%s) = %d\n", cname, b[0], b[1], b[2], b[3], ident, type, value);
	} else if (t_size == 1) {
		fprintf(datf, "%s\t\t.byte %d\t\t; %s (%s)\n", cname, value & 0xff, ident, type);
	} else {
		fprintf(datf, "%s\t\t.word %d\t\t; %s (%s)\n", cname, value & 0xffff, ident, type);
	}
	return (0);
}

int emit_declaration(char *ident, char *type) {
	return emit_storage(ident, type, 0, 0, 0);
}

/* Operators work on the run time stack: each operand is pushed, then the
 * operator routine pops its operands and pushes the result.  INTs are 16 bit
 * signed, comparisons leave 1 (true) or 0 (false) as a 16 bit value.
 * If either operand is a REAL the other one is converted (ITOFS / ITOF2S) and
 * the floating point routine is used; the arithmetic then gives a REAL.
 * The operand arguments are unused: the stack carries the values. */
static int str_binary(char *op);

int emit_arithmetic(int operand1, int operand2, char *op) {
	extern FILE *outf;

	if (tsp >= 2 && str_binary(op)) return (0);
	static const char *itab[][2]= {
		{"**","EXP16S"}, {"n","NEG16S"}, {"*","MUL16S"}, {"/","DIV16S"},
		{"%","MOD16S"},  {"rsh","RSH16S"}, {"lsh","LSH16S"},
		{"+","ADD16S"},  {"-","SUB16S"},
		{">","GT16S"},   {">=","GE16S"},  {"<","LT16S"},  {"<=","LE16S"},
		{"#","NE16S"},   {"=","EQ16S"},
		{"!","XOR16S"},  {"and","AND16S"}, {"or","OR16S"}
	};
	/* REAL versions; last field 1 if the result is an integer (comparisons) */
	static const struct { const char *op, *rtn; int isint; } rtab[]= {
		{"n","FNEGS",0}, {"+","FADDS",0}, {"-","FSUBS",0}, {"*","FMULS",0}, {"/","FDIVS",0},
		{">","FGTS",1}, {">=","FGES",1}, {"<","FLTS",1}, {"<=","FLES",1}, {"#","FNES",1}, {"=","FEQS",1}
	};
	int i, ta, tb;

	if (!strcmp(op, "n") && pend) {			/* -constant: fold */
		if (pend == 1) { pendi= -pendi; if (tsp > 0) tstack[tsp-1]= const_type(pendi); } else pendf= -pendf;
		return (0);
	}
	flush_const();

	if (!strcmp(op, "n")) {
		tb= tpop();
		if (tb == 'R') {
			fprintf(outf,"\t\tjsr FNEGS\n");
			tpush('R');
			return (0);
		}
		fprintf(outf,"\t\tjsr NEG16S\n");
		tpush('I');
		return (0);
	}

	tb= tpop();				/* right operand, on top */
	ta= tpop();				/* left operand */
	if (ta == 'A' || tb == 'A') {
		printf("ERR! emit_arithmetic: a record can't be an operand of %s, use its fields!\n", op);
		exit(1);
	}

	if (ta == 'R' || tb == 'R') {
		if (tb != 'R') fprintf(outf,"\t\tjsr %s\t\t; right operand to REAL\n", (tb == 'I')? "ITOFS": "UTOFS");
		if (ta != 'R') fprintf(outf,"\t\tjsr %s\t\t; left operand to REAL\n", (ta == 'I')? "ITOF2S": "UTOF2S");
		for (i=0; i<(int)(sizeof(rtab)/sizeof(rtab[0])); i++)
			if (!strcmp(op, rtab[i].op)) {
				fprintf(outf,"\t\tjsr %s\n", rtab[i].rtn);
				tpush(rtab[i].isint? 'B': 'R');
				return (0);
			}
		printf("ERR! emit_arithmetic: operator %s can't be used with a REAL operand!\n", op);
		exit(1);
	}

	{
		int sgn= ((ta == 'I' || tb == 'I') && ta != 'C' && tb != 'C');	/* as in Action!: anything with a CARD is a CARD */
		int cmp= (!strcmp(op, ">") || !strcmp(op, ">=") || !strcmp(op, "<") || !strcmp(op, "<="));
		int dv= (!strcmp(op, "/") || !strcmp(op, "%"));
		int b8= ((ta == 'B' && (tb == 'B' || tb == 'Z')) || (tb == 'B' && ta == 'Z'));
		if (!strcmp(op, "**")) b8= (ta == 'B');	/* the width of a power is the width of its base */
		int wrap= (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") ||
			   !strcmp(op, "lsh") || !strcmp(op, "**"));

		for (i=0; i<(int)(sizeof(itab)/sizeof(itab[0])); i++)
			if (!strcmp(op, itab[i][0])) {
				char rn[32];
				strcpy(rn, itab[i][1]);
				if ((cmp || dv) && !sgn) rn[strlen(rn)-1]= 'U';	/* DIV16S -> DIV16U ... */
				if (!strcmp(op, "**") && tb == 'I') strcpy(rn, "EXP16I");	/* signed exponent: negative gives 0 */
				fprintf(outf,"\t\tjsr %s\n", rn);
				if (b8 && wrap) {
					fprintf(outf,"\t\t_TRUNC8\t\t; 8 bit arithmetic wraps at 256\n");
					tpush('B');
					return (0);
				}
				if (b8 && !cmp) {
					tpush('B');
					return (0);
				}
				tpush((cmp || !strcmp(op, "#") || !strcmp(op, "="))? 'B': sgn? 'I': 'C');
				return (0);
			}
	}

	printf("ERR! emit_arithmetic: operation %s not found!\n", op);
	exit(1);
}

/* ---- record fields: "r.f" -------------------------------------------------
 * r is either a record variable (the field is at its label + offset) or a
 * pointer to a record (the field is reached through the pointer, using the
 * offset as the Y index). */
typedef struct {
	int  ptr;			/* 0: record variable, 1: pointer to a record */
	char label[300];		/* label of the variable (or of the pointer) */
	int  off, size, dsize, dtype;
	char at[320];			/* label+offset, for a record variable */
	int  arr, elsize, count;	/* array field (extension) */
	int  ftype;			/* type index of the field */
} fref;

static void resolve_field(char *cname, fref *r) {
	char base[256], *dot, *lab;
	int t, d;

	strncpy(base, cname, 255); base[255]= 0;
	dot= strchr(base, '.');
	*dot++= 0;

	lab= _get_compiler_var(base);
	if (!lab) {
		printf("ERR! %s.%s: %s is not declared!\n", base, dot, base);
		exit(1);
	}
	strcpy(r->label, lab);

	t= _var_type(base);
	if (_type_is_record(t)) {
		r->ptr= 0;
	} else if (_type_is_record(d= _get_deref_type(base)) && _get_deref(base) > 0) {
		r->ptr= 1; t= d;
	} else {
		printf("ERR! %s.%s: %s is not a record or a pointer to a record!\n", base, dot, base);
		exit(1);
	}

	if (_field_lookup(t, dot, &r->off, &r->size, &r->dsize, &r->dtype) < 0) {
		printf("ERR! %s.%s: the type has no field %s!\n", base, dot, dot);
		exit(1);
	}
	r->arr= _field_array(t, dot, &r->elsize, &r->count);
	r->ftype= _field_type(t, dot);
	if (!r->arr && r->ptr && r->off + 3 > 255) {
		printf("ERR! %s.%s: field is too far into the record (offset %d)!\n", base, dot, r->off);
		exit(1);
	}
	if (r->off) sprintf(r->at, "%s+%d", r->label, r->off);
	else strcpy(r->at, r->label);
}

static int push_field(char *cname, int deref) {
	extern FILE *outf;
	fref r;

	resolve_field(cname, &r);
	fprintf(outf, "; Push FIELD = %s%s\n", cname, deref? "^": "");

	if (r.arr) {				/* an array field used as a value is its address */
		if (deref) { printf("ERR! %s^ : an array field is not a pointer!\n", cname); exit(1); }
		fprintf(outf, "\t\t_PUSHFA%c %s,%d\n\n", r.ptr? 'P': 'D', r.label, r.off);
		tpush('C');
		return (0);
	}
	if (deref) {				/* r.p^ : the field is a pointer */
		if (r.dsize <= 0 || r.ptr || _type_is_record(r.dtype) ||
		   (r.dsize != 1 && r.dsize != 2 && r.dsize != 4)) {
			printf("ERR! %s^ : only a pointer field to BYTE, CARD, INT or REAL of a record variable can be dereferenced!\n", cname);
			exit(1);
		}
		switch (r.dsize) {
			case 1: fprintf(outf, "\t\t_PUSHIND8 %s,0\n\n", r.at);  tpush(vtc(r.dtype)); break;
			case 2: fprintf(outf, "\t\t_PUSHIND16 %s,0\n\n", r.at); tpush(vtc(r.dtype)); break;
			default: fprintf(outf, "\t\t_PUSHINDF %s,0\n\n", r.at); tpush('R'); break;
		}
		return (0);
	}

	switch (r.size) {
		case 1: if (r.ptr) fprintf(outf, "\t\t_PUSHIND8 %s,%d\n\n", r.label, r.off);  else fprintf(outf, "\t\t_PUSH8 %s\n\n", r.at);  tpush(vtc(r.ftype)); break;
		case 2: if (r.ptr) fprintf(outf, "\t\t_PUSHIND16 %s,%d\n\n", r.label, r.off); else fprintf(outf, "\t\t_PUSH16 %s\n\n", r.at); tpush(vtc(r.ftype)); break;
		case 4: if (r.ptr) fprintf(outf, "\t\t_PUSHINDF %s,%d\n\n", r.label, r.off);  else fprintf(outf, "\t\t_PUSHF %s\n\n", r.at);  tpush('R'); break;
		default: printf("ERR! %s: a field of %d bytes can't be used as a value!\n", cname, r.size); exit(1);
	}
	return (0);
}

int emit_pushVar(char *ident, char *vartype) {
	int storage;
	char *addr;
	extern FILE *outf;

	ident= qual(ident);
	flush_const();
	if (strchr(ident, '.')) return push_field(ident, !strcmp(vartype, "^"));
	fprintf(outf, "; Push VARIABLE = %s (%s)\n", ident, vartype);

	addr= _get_compiler_var(ident);
	if (!addr) {
		printf("ERR! emit_pushVar: %s is not declared!\n", ident);
		exit(1);
	}

	if (_get_array(ident, NULL, NULL) && _get_array(ident, NULL, NULL) != 3 && strcmp(vartype, "^")) {		/* an array name is its address */
		if (_get_array(ident, NULL, NULL) == 1) fprintf(outf, "\t\t_PUSHI %s\n\n", addr);
		else fprintf(outf, "\t\t_PUSH16 %s\n\n", addr);
		tpush('C');
		{ int es= 0; _get_array(ident, &es, NULL); tstr[tsp-1]= strkind_of(addr)? strkind_of(addr): ((es == 1)? 8: 0); }
		return (0);
	}

	storage= _get_storage(ident);		/* size in bytes */

	if (!strcmp(vartype, "^") && _type_is_record(_get_deref_type(ident)) && !_type_is_class(_get_deref_type(ident)) && _get_deref(ident) > 0) {
		fprintf(outf, "\t\t_PUSH16 %s\t\t; the record %s points to, as an address\n\n", addr, ident);
		tpushrec(_get_deref_type(ident));		/* only a whole record assignment can use it */
		return (0);
	}
	if (!strcmp(vartype, "i") && _type_is_record(_var_type(ident)) && !_type_is_class(_var_type(ident))) {
		fprintf(outf, "\t\t_PUSHI %s\t\t; the record %s, as an address\n\n", addr, ident);
		tpushrec(_var_type(ident));
		return (0);
	}
	if (_type_is_record(_var_type(ident)) ||
	    (!strcmp(vartype, "^") && _type_is_record(_get_deref_type(ident)))) {
		printf("ERR! %s: a record can't be used as a value, use its fields (%s.field)!\n", ident, ident);
		exit(1);
	}

	if (!strcmp(vartype, "^")) {			/* p^ : the value p points to */
		switch (_get_deref(ident)) {
			case  1: fprintf(outf, "\t\t_PUSHIND8 %s,0\n\n", addr);  tpush(vtc(_get_deref_type(ident))); break;
			case  2: fprintf(outf, "\t\t_PUSHIND16 %s,0\n\n", addr); tpush(vtc(_get_deref_type(ident))); break;
			case  4: fprintf(outf, "\t\t_PUSHINDF %s,0\n\n", addr);  tpush('R'); break;
			default: printf("ERR! %s^ : %s is not a pointer to BYTE, CARD, INT or REAL!\n", ident, ident); exit(1);
		}
		return (0);
	}

	switch (storage) {
		case  0: printf("ERR! emit_pushVar: Storage can't be zero!\n");	exit(1);
		case -1: printf("ERR! emit_pushVar: Unknown storage type!\n"); 	exit(1);
		case  1: fprintf(outf, "\t\t_PUSH8 %s\n", addr); 	tpush(vtc(_var_type(ident))); break;
		case  2: fprintf(outf, "\t\t_PUSH16 %s\n", addr); 	tpush(vtc(_var_type(ident))); break;
		case  4: fprintf(outf, "\t\t_PUSHF %s\n", addr); 	tpush('R'); break;	/* REAL */
		default: printf("ERR! emit_pushVar: %s has unsupported storage (%d bytes)!\n", ident, storage); exit(1);
	}

	fprintf(outf, "\n");

	return(0);
}

int emit_pushConst(int value) {
	flush_const();
	pend= 1; pendi= value;
	tpush(const_type(value));
	return(0);
}

/* Two strings with + (joined in the scratch string STRTMP) or a relational operator (compared like
 * SCompare: by character, then by length).  One of them has to be a STRING or ZTRING; a plain BYTE ARRAY
 * is taken as a STRING.  Anything else is address arithmetic, as in Action!. */
static int str_binary(char *op) {
	extern FILE *outf;
	int tb= tstr[tsp-1], ta= tstr[tsp-2];
	int isplus= !strcmp(op, "+");
	int isrel= !strcmp(op, "=") || !strcmp(op, "#") || !strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=");
	int flags;

	if (!isplus && !isrel) return 0;
	if (!ta || !tb || (!(ta & 3) && !(tb & 3))) return 0;
	flags= (((ta & 3) == 2)? 1: 0) | (((tb & 3) == 2)? 2: 0);
	flush_const();
	if (isplus) {
		if (tb & 16) {
			printf("ERR! a + b + (c + d): put the parenthesised part in a string variable first!\n");
			exit(1);
		}
		if (ta & 16) flags |= 4;
		fprintf(outf, "\t\tlda #%d\n\t\tjsr STRCAT2\t\t; string + string\n", flags);
		tpop(); tpop();
		tpush('C');
		tstr[tsp-1]= 1 | 16;
		return 1;
	}
	fprintf(outf, "\t\tlda #%d\n\t\tjsr STRCMP\t\t; compare two strings: -1, 0 or 1\n", flags);
	tpop(); tpop();
	tpush('I');
	emit_pushConst(0);
	emit_arithmetic(0, 0, op);
	return 1;
}

/* s = string: STRSET copies at most the capacity of s and fixes the length or the terminator */
static int assign_string(char *ident, char *addr, int kind) {
	extern FILE *outf;
	int t= tstr[tsp-1];
	int arr= _get_array(ident, NULL, NULL);
	int flags;

	if (!t) {
		printf("ERR! %s is a %s: assign a string to it, not a number!\n", ident, (kind == 2)? "ZTRING": "STRING");
		exit(1);
	}
	flags= (((t & 3) == 2)? 1: 0) | ((kind == 2)? 2: 0);
	tpop();
	flush_const();
	fprintf(outf, "; %s = <string on top of the stack>\n\t\tjsr POPA\n", ident);
	if (arr == 2) fprintf(outf, "\t\tlda %s\n\t\tsta TMPB\n\t\tlda %s+1\n\t\tsta TMPB+1\n", addr, addr);
	else fprintf(outf, "\t\tlda #<%s\n\t\tsta TMPB\n\t\tlda #>%s\n\t\tsta TMPB+1\n", addr, addr);
	fprintf(outf, "\t\tlda #%d\n\t\tldx #%d\n\t\tjsr STRSET\n", strcap_of(addr), flags);
	return (0);
}

/* The value on top of the stack goes into a variable of the given size
 * (1 = BYTE, 2 = CARD/INT, 4 = REAL), converting INT <-> REAL as needed:
 * REAL to INT truncates toward zero. */
static int conv_cu= 0;		/* set just before convert_top: the destination is a CARD */
static void convert_top(int size, char *ident) {
	extern FILE *outf;
	int t= tpop();
	int cu= conv_cu;

	conv_cu= 0;

	if (t == 'A') {
		printf("ERR! a record can't be assigned to %s, which is not a record of that type!\n", ident);
		exit(1);
	}

	if (size == 4 && t != 'R') {
		if (pend == 1) { pend= 2; pendf= (double)pendi; }
		else { flush_const(); fprintf(outf, "\t\tjsr %s\t\t; %s to REAL\n", (t == 'I')? "ITOFS": "UTOFS", (t == 'I')? "INT": "CARD"); }
	} else if (size != 4 && t == 'R') {
		if (pend == 2) { pend= 1; pendi= cu? (int)(unsigned)(long)pendf: (int)pendf; }
		else { flush_const(); fprintf(outf, "\t\tjsr %s\t\t; REAL to %s\n", cu? "FTOUS": "FTOIS", cu? "CARD": "INT"); }
	}
}

static int assign_field(char *cname, int deref) {
	extern FILE *outf;
	fref r;
	int size;

	resolve_field(cname, &r);
	if (r.arr) {
		printf("ERR! %s is an array field, assign its elements %s(i)!\n", cname, cname);
		exit(1);
	}
	if (deref) {
		if (r.dsize <= 0 || r.ptr || _type_is_record(r.dtype) ||
		   (r.dsize != 1 && r.dsize != 2 && r.dsize != 4)) {
			printf("ERR! %s^ : only a pointer field to BYTE, CARD, INT or REAL of a record variable can be assigned through!\n", cname);
			exit(1);
		}
		size= r.dsize;
	} else {
		size= r.size;
		if (size != 1 && size != 2 && size != 4) {
			printf("ERR! %s: a field of %d bytes can't be assigned to!\n", cname, size);
			exit(1);
		}
	}
	conv_cu= (vtc(r.ftype) == 'C');
	convert_top(size, cname);
	flush_const();

	fprintf(outf, "; %s%s = <top of stack>\n", cname, deref? "^": "");
	if (deref || r.ptr) {
		const char *m= (size == 1)? "_POPIND8": (size == 2)? "_POPIND16": "_POPINDF";
		if (deref) fprintf(outf, "\t\t%s %s,0\n", m, r.at);
		else       fprintf(outf, "\t\t%s %s,%d\n", m, r.label, r.off);
	} else {
		fprintf(outf, "\t\t%s %s\n", (size == 1)? "_POP8": (size == 2)? "_POP16": "_POPF", r.at);
	}
	return (0);
}

int emit_assign(char *ident) {
	extern FILE *outf;
	char *addr;
	int size;

	ident= qual(ident);

	if (strchr(ident, '.')) return assign_field(ident, 0);
	addr= _get_compiler_var(ident);
	if (addr && strkind_of(addr)) return assign_string(ident, addr, strkind_of(addr));
	if (addr && _type_is_record(_var_type(ident))) {
		int t, rt;

		if (_type_is_class(_var_type(ident))) {
			printf("ERR! %s: a class variable is a reference, it can't be assigned a record!\n", ident);
			exit(1);
		}
		flush_const();
		rt= (tsp > 0)? trec[tsp-1]: -1;
		t= tpop();
		if (t != 'A' || rt != _var_type(ident)) {
			printf("ERR! %s: a record can only be assigned a record of the same type (%s = other)!\n", ident, ident);
			exit(1);
		}
		fprintf(outf, "; %s = whole record copy (%d bytes)\n", ident, _type_size_of_index(rt));
		fprintf(outf, "\t\t_COPYREC %s,%d\n", addr, _type_size_of_index(rt));
		return (0);
	}

	if (!addr) {
		printf("ERR! assignment to %s, which is not declared!\n", ident);
		exit(1);
	}
	size= _get_storage(ident);
	if (_get_array(ident, NULL, NULL) == 1) {
		printf("ERR! %s is an array with its own storage, assign its elements %s(i)!\n", ident, ident);
		exit(1);
	}
	if (_get_array(ident, NULL, NULL) == 2) size= 2;	/* sets the array's address */
	conv_cu= (vtc(_var_type(ident)) == 'C');
	convert_top(size, ident);
	flush_const();

	fprintf(outf, "; %s = <top of stack>\n", ident);
	switch (size) {
		case  1: fprintf(outf, "\t\t_POP8 %s\n", addr); break;
		case  2: fprintf(outf, "\t\t_POP16 %s\n", addr); break;
		case  4: fprintf(outf, "\t\t_POPF %s\n", addr); break;
		default: printf("ERR! emit_assign: unsupported storage for %s\n", ident); exit(1);
	}
	return (0);
}

/* p^ = value */
int emit_assign_deref(char *ident) {
	extern FILE *outf;
	char *addr;
	int size;

	ident= qual(ident);

	if (strchr(ident, '.')) return assign_field(ident, 1);
	addr= _get_compiler_var(ident);
	if (addr && _type_is_record(_get_deref_type(ident)) && _get_deref(ident) > 0 && !_type_is_class(_get_deref_type(ident))) {
		int t, rt;

		flush_const();
		rt= (tsp > 0)? trec[tsp-1]: -1;
		t= tpop();
		if (t != 'A' || rt != _get_deref_type(ident)) {
			printf("ERR! %s^ : a record can only be assigned a record of the same type!\n", ident);
			exit(1);
		}
		fprintf(outf, "; %s^ = whole record copy (%d bytes)\n", ident, _type_size_of_index(rt));
		fprintf(outf, "\t\t_COPYIND %s,%d\n", addr, _type_size_of_index(rt));
		return (0);
	}
	if (_type_is_record(_get_deref_type(ident))) {
		printf("ERR! %s^ : %s points to a record, assign its fields (%s.field)!\n", ident, ident, ident);
		exit(1);
	}

	if (!addr) {
		printf("ERR! assignment through %s, which is not declared!\n", ident);
		exit(1);
	}
	size= _get_deref(ident);
	if (size != 1 && size != 2 && size != 4) {
		printf("ERR! %s^ : %s is not a pointer to BYTE, CARD, INT or REAL!\n", ident, ident);
		exit(1);
	}
	conv_cu= (vtc(_get_deref_type(ident)) == 'C');
	convert_top(size, ident);
	flush_const();

	fprintf(outf, "; %s^ = <top of stack>\n", ident);
	switch (size) {
		case  1: fprintf(outf, "\t\t_POPIND8 %s,0\n", addr); break;
		case  2: fprintf(outf, "\t\t_POPIND16 %s,0\n", addr); break;
		default: fprintf(outf, "\t\t_POPINDF %s,0\n", addr); break;
	}
	return (0);
}

/* @x : push the address of variable x as a 16 bit value */
int emit_pushAddr(char *ident) {
	extern FILE *outf;
	char *addr;

	ident= qual(ident);
	if (strchr(ident, '.')) return push_field_addr(ident);
	addr= _get_compiler_var(ident);
	flush_const();
	if (!addr) {
		printf("ERR! @%s: %s is not declared!\n", ident, ident);
		exit(1);
	}
	fprintf(outf, "; Push ADDRESS OF %s\n", ident);
	if (_get_array(ident, NULL, NULL) == 2) fprintf(outf, "\t\t_PUSH16 %s\n", addr);
	else fprintf(outf, "\t\t_PUSHI %s\n", addr);
	tpush('C');
	return (0);
}

int emit_unsupported(char *what) {
	printf("ERR! %s is not supported by the emitter yet!\n", what);
	exit(1);
}

int emit_variable_declaration(char *type, char *ident) {
	extern FILE *outf; 

	emit_declaration(ident, type);
}

int emit_variable_declaration_with_init(char *type, char *ident, int value, char *constType) {
	/* "address": CARD c=$600 puts c at $600.  "constant": CARD c=[5] initial value */
	return emit_storage(ident, type, 1, value, !strcmp(constType, "address"));
}

int emit_pointer_declaration(char *type, char *ident) {
	if (_in_record()) {			/* a pointer field: 2 bytes */
		if (_type_sizeof(type) < 0) {
			printf("ERR! %s: %s unknown type!\n", ident, type);
			exit(1);
		}
		{ int rr= _record_add_field(ident, 2, _type_sizeof(type), _type_index_of(type)); _record_set_ftype(_type_index_of("card")); return rr; }
	}
	emit_storage(ident, "card", 0, 0, 0);		/* a pointer is a 16 bit address */
	_set_deref(ident, _type_sizeof(type), _type_index_of(type));
	if (!_type_is_record(_type_index_of(type)) && _in_record() == 0 &&
	    (_type_sizeof(type) == 1 || _type_sizeof(type) == 2 || _type_sizeof(type) == 4))
		_set_array(ident, 3, _type_sizeof(type), 0);	/* p(i) indexes through the pointer */
	return (0);
}

int emit_pointer_declaration_with_init(char *type, char *ident, int value) {
	emit_storage(ident, "card", 1, value, 1);	/* p=$600: the pointer lives at $600 */
	_set_deref(ident, _type_sizeof(type), _type_index_of(type));
	if (!_type_is_record(_type_index_of(type)) &&
	    (_type_sizeof(type) == 1 || _type_sizeof(type) == 2 || _type_sizeof(type) == 4))
		_set_array(ident, 3, _type_sizeof(type), 0);
	return (0);
}

int emit_record_begin(char *name) {
	return _record_begin(name);
}

int emit_record_end() {
	return _record_end();
}

/* ---- floating point ------------------------------------------------------
 * REAL is 4 bytes (format and routines in includes/fp.inc).  A constant is
 * encoded here, at compile time, and pushed with _PUSHFLOAT e,m1,m2,m3.
 * emit_mathfn calls FSIN FCOS FTAN FATAN FLN FLOG2 FLOG10: REAL on the stack in,
 * REAL out; an integer argument is converted first.
 */
int emit_pushFloat(double value) {
	flush_const();
	pend= 2; pendf= value;
	tpush('R');
	return(0);
}

int emit_mathfn(char *name) {
	extern FILE *outf;
	char up[16];
	int i;

	flush_const();
	{
		int t= tpop();
		if (t != 'R') fprintf(outf, "\t\tjsr %s\t\t; argument to REAL\n", (t == 'I')? "ITOFS": "UTOFS");
	}
	for (i=0; name[i] && i<15; i++) up[i]= (name[i]>='a' && name[i]<='z')? name[i]-32: name[i];
	up[i]= 0;
	fprintf(outf, "\t\tjsr F%s\n", up);
	tpush('R');
	return(0);
}

int emit_variable_declaration_with_float_init(char *type, char *ident, double value) {
	unsigned char b[4];
	char *cname;

	if (_in_record()) {
		printf("ERR! %s: a TYPE field can't have an initial value!\n", ident);
		exit(1);
	}
	if (_type_sizeof(type) != 4) {
		printf("ERR! %s: a floating point initial value needs a REAL variable (it is %s)\n", ident, type);
		exit(1);
	}
	_valloc(ident, type);
	cname= _get_compiler_var(ident);
	if (!datf) datf= tmpfile();
	fp_encode(value, b);
	fprintf(datf, "%s\t\t.byte $%02x,$%02x,$%02x,$%02x\t; %s (%s) = %.9g\n", cname, b[0], b[1], b[2], b[3], ident, type, value);
	return(0);
}

/* ---- SYSMEM (STUB) ---------------------------------------------------------
 * SYSMEM(i) is a reserved BYTE array over available memory.  Not implemented:
 * these emit a macro call that includes/mdefs.inc does not define yet, so the
 * assembler stops at the first use instead of producing silently wrong code.
 *   load : index on stack -> byte on stack
 *   store: value and index on stack -> memory
 */
/* ===========================================================================
 * Routines, calls, control flow, arrays, string constants
 * ========================================================================= */

/* ---- labels ------------------------------------------------------------- */

static int lblcnt= 0;

static int newlabel() { return ++lblcnt; }

static void putlabel(int n) {
	extern FILE *outf;

	flush_const();
	fprintf(outf, "L%05d\n", n);
}

static void jump(int n) {
	extern FILE *outf;

	flush_const();
	fprintf(outf, "\t\tjmp L%05d\n", n);
}

/* hidden storage for loop limits, CASE selectors and string constants goes with the variables */
static void hidden(char *label, int size) {
	if (!datf) datf= tmpfile();
	zvar(label, size, "hidden");
}

/* pop an INT condition and jump to label n if it is zero */
static void cond_jz(int n) {
	extern FILE *outf;

	convert_top(2, "condition");
	flush_const();
	fprintf(outf, "\t\t_JZ L%05d\n", n);
}

/* ---- control frames ---------------------------------------------------------- */

#define K_IF   1
#define K_LOOP 2
#define K_CASE 3
#define K_TRY  4

typedef struct {
	int  kind;
	int  a, b, c;			/* labels, see each construct */
	int  hasElse;
	int  vsize;
	int  usgn;			/* FOR: an unsigned loop variable (BYTE, CARD) */
	int  quick;			/* FOR: unsigned variable with the default step of 1 (short code) */
	char v[48], lim[48], stp[48];	/* FOR: variable, limit and step labels; CASE: selector in v */
} cframe;

static cframe cstack[256];
static int    csp= 0;

static cframe *cpush(int kind) {
	if (csp >= 256) { printf("ERR! statements nested too deeply!\n"); exit(1); }
	memset(&cstack[csp], 0, sizeof(cframe));
	cstack[csp].kind= kind;
	return &cstack[csp++];
}

static cframe *ctop(int kind, char *what) {
	if (csp <= 0 || cstack[csp-1].kind != kind) {
		printf("ERR! internal: %s out of place!\n", what);
		exit(1);
	}
	return &cstack[csp-1];
}

/* IF c THEN .. ELSEIF c THEN .. ELSE .. FI */
int emit_if_begin() {
	cframe *f= cpush(K_IF);
	f->a= newlabel(); f->b= newlabel();
	cond_jz(f->b);
	return (0);
}

int emit_elseif_head() {
	cframe *f= ctop(K_IF, "ELSEIF");
	jump(f->a);
	putlabel(f->b);
	return (0);
}

int emit_elseif_test() {
	cframe *f= ctop(K_IF, "ELSEIF");
	f->b= newlabel();
	cond_jz(f->b);
	return (0);
}

int emit_else() {
	cframe *f= ctop(K_IF, "ELSE");
	f->hasElse= 1;
	jump(f->a);
	putlabel(f->b);
	return (0);
}

int emit_fi() {
	cframe *f= ctop(K_IF, "FI");
	if (!f->hasElse) putlabel(f->b);
	putlabel(f->a);
	csp--;
	return (0);
}

/* WHILE c DO .. OD */
int emit_while_begin() {
	cframe *f= cpush(K_LOOP);
	f->a= newlabel(); f->b= newlabel();
	putlabel(f->a);
	return (0);
}

int emit_while_test() {
	cframe *f= ctop(K_LOOP, "WHILE");
	cond_jz(f->b);
	return (0);
}

int emit_while_end() {
	cframe *f= ctop(K_LOOP, "OD");
	jump(f->a);
	putlabel(f->b);
	csp--;
	return (0);
}

/* DO .. OD, DO .. UNTIL c OD */
int emit_do_begin() {
	cframe *f= cpush(K_LOOP);
	f->a= newlabel(); f->b= newlabel();
	putlabel(f->a);
	return (0);
}

int emit_do_end() {
	return emit_while_end();
}

int emit_until_end() {
	extern FILE *outf;
	cframe *f= ctop(K_LOOP, "UNTIL");
	cond_jz(f->a);				/* condition false: go round again */
	putlabel(f->b);
	csp--;
	return (0);
}

/* A TRY body that is still open has a handler on the run time's list: leaving it by
 * RETURN or EXIT has to take the handler off.  Frames from index 'from' up. */
static void try_pops(int from) {
	extern FILE *outf;
	int i;

	for (i= from; i < csp; i++)
		if (cstack[i].kind == K_TRY && !cstack[i].hasElse) fprintf(outf, "\t\tdec TRYSP\t\t; leaving a TRY\n");
}

/* EXIT leaves the innermost loop */
int emit_exit() {
	int i;

	for (i= csp-1; i >= 0; i--)
		if (cstack[i].kind == K_LOOP) { flush_const(); try_pops(i + 1); jump(cstack[i].b); return (0); }
	printf("ERR! EXIT outside a loop!\n");
	exit(1);
}

/* FOR v = start TO limit [STEP s] DO .. OD
 * The limit and step are worked out once and kept in hidden variables.  The test
 * is signed: v <= limit when the step is not negative, v >= limit when it is. */
int emit_for_limit(char *var) {
	extern FILE *outf;
	cframe *f;
	char *lab= _get_compiler_var(var);
	int size;
	int n;

	if (!lab) { printf("ERR! FOR %s: %s is not declared!\n", var, var); exit(1); }
	size= _get_storage(var);
	if ((size != 1 && size != 2) || _get_array(var, NULL, NULL) || _type_is_record(_var_type(var))) {
		printf("ERR! FOR %s: the loop variable must be a BYTE, CARD or INT!\n", var);
		exit(1);
	}
	f= cpush(K_LOOP);
	n= newlabel();
	f->a= newlabel(); f->b= newlabel();
	strcpy(f->v, lab); f->vsize= size;
	f->usgn= (vtc(_var_type(var)) != 'I');
	f->quick= 0;
	sprintf(f->lim, "FLIM%05d", n);
	sprintf(f->stp, "FSTP%05d", n);
	hidden(f->lim, 2);
	hidden(f->stp, 2);

	conv_cu= f->usgn;
	convert_top(2, var);
	flush_const();
	fprintf(outf, "\t\t_POP16 %s\t\t; limit\n", f->lim);
	return (0);
}

int emit_for_step_default() {
	extern FILE *outf;
	cframe *f= ctop(K_LOOP, "FOR");
	flush_const();
	extern int _opt;
	if (_opt && f->usgn) { f->quick= 1; return (0); }
	fprintf(outf, "\t\t_SET16 %s,1\t\t; step\n", f->stp);
	return (0);
}

int emit_for_step() {
	extern FILE *outf;
	cframe *f= ctop(K_LOOP, "FOR");
	convert_top(2, "step");
	flush_const();
	fprintf(outf, "\t\t_POP16 %s\t\t; step\n", f->stp);
	return (0);
}

int emit_for_top() {
	extern FILE *outf;
	cframe *f= ctop(K_LOOP, "FOR");
	putlabel(f->a);
	if (f->quick) {
		fprintf(outf, "\t\t_FORT1%s %s,%s,L%05d\n", f->vsize == 1? "B": "W", f->lim, f->v, f->b);
		return (0);
	}
	fprintf(outf, "\t\tlda %s\n\t\tsta TMPA\n", f->v);
	if (f->vsize == 2) fprintf(outf, "\t\tlda %s+1\n\t\tsta TMPA+1\n", f->v);
	else fprintf(outf, "\t\tlda #0\n\t\tsta TMPA+1\n");
	fprintf(outf, "\t\t_FORTEST%s %s,%s,L%05d\n", f->usgn? "U": "", f->lim, f->stp, f->b);
	return (0);
}

int emit_for_end() {
	extern FILE *outf;
	cframe *f= ctop(K_LOOP, "OD");
	flush_const();
	if (f->quick) {
		fprintf(outf, "\t\t_FORI1%s %s,L%05d\n", f->vsize == 1? "B": "W", f->v, f->b);
		jump(f->a);
		putlabel(f->b);
		csp--;
		return (0);
	}
	fprintf(outf, "\t\tlda %s\n\t\tsta TMPA\n", f->v);
	if (f->vsize == 2) fprintf(outf, "\t\tlda %s+1\n\t\tsta TMPA+1\n", f->v);
	else fprintf(outf, "\t\tlda #0\n\t\tsta TMPA+1\n");
	fprintf(outf, "\t\t_FORADD %s\n\t\tlda TMPA\n\t\tsta %s\n", f->stp, f->v);
	if (f->vsize == 2) fprintf(outf, "\t\tlda TMPA+1\n\t\tsta %s+1\n", f->v);
	if (f->usgn) fprintf(outf, "\t\t_FORCHK%s %s,L%05d\n", f->vsize == 1? "B": "U", f->stp, f->b);
	jump(f->a);
	putlabel(f->b);
	csp--;
	return (0);
}

/* CASE e  v1,v2 THEN .. v3 THEN .. ELSE .. ESAC   (no fall through) */
int emit_case_begin() {
	extern FILE *outf;
	cframe *f= cpush(K_CASE);
	int n= newlabel();

	f->a= newlabel();
	sprintf(f->v, "CSEL%05d", n);
	hidden(f->v, 2);
	convert_top(2, "CASE");
	flush_const();
	fprintf(outf, "\t\t_POP16 %s\t\t; selector\n", f->v);
	return (0);
}

int emit_case_value(int v, int first) {
	extern FILE *outf;
	cframe *f= ctop(K_CASE, "CASE value");

	if (first) { f->b= newlabel(); f->c= newlabel(); }
	flush_const();
	fprintf(outf, "\t\t_CASEEQ %s,%d,L%05d\n", f->v, v & 0xffff, f->b);
	return (0);
}

int emit_case_then() {
	cframe *f= ctop(K_CASE, "THEN");
	jump(f->c);				/* no value matched: next clause */
	putlabel(f->b);
	return (0);
}

int emit_case_clause_end() {
	cframe *f= ctop(K_CASE, "CASE clause");
	jump(f->a);
	putlabel(f->c);
	return (0);
}

int emit_case_end() {
	cframe *f= ctop(K_CASE, "ESAC");
	putlabel(f->a);
	csp--;
	return (0);
}

/* ---- string constants: length byte, then the characters --------------------- */

static char strtmp[1024];

/* "text" as written in the source, with quotes and "" escapes -> plain text */
static char *unquote(char *q) {
	int i, n= 0;
	int len= strlen(q);

	for (i= 1; i < len-1 && n < 1000; i++) {
		if (q[i] == '"' && q[i+1] == '"') i++;
		strtmp[n++]= q[i];
	}
	strtmp[n]= 0;
	return strtmp;
}

static int strcnt= 0;

/* put a string constant in the data area, returns its label number */
static int make_string(char *quoted) {
	char *s= unquote(quoted);
	int i, n= strlen(s);

	if (n > 255) { printf("ERR! string constant longer than 255 characters!\n"); exit(1); }
	if (!datf) datf= tmpfile();
	fprintf(datf, "STR%05d\t\t.byte %d", ++strcnt, n);
	for (i= 0; i < n; i++) fprintf(datf, ",$%02x", (unsigned char)s[i]);
	fprintf(datf, "\n");
	return strcnt;
}

/* ---- routines ---------------------------------------------------------------------- */

#define MAXPARAM 16

typedef struct routine_s {
	char name[64];
	char label[80];
	int  nparams;
	int  psize[MAXPARAM];		/* 1, 2 or 4 bytes each */
	char pcard[MAXPARAM];		/* 1: a CARD parameter (a REAL argument converts unsigned) */
	char pname[MAXPARAM][64];
	int  ret;			/* 0 PROC, 1 BYTE, 2 CARD/INT, 3 REAL */
	int  rtc;			/* the result's expression type: B C I R */
	int  cls;			/* method of this class (type index), -1 for a plain routine */
	int  slot;			/* method: its slot in the class's method table */
	int  inl;			/* machine code in the source: the code is at the label */
	int  fixed;			/* at a fixed address (no body)... */
	int  addr;			/* ... this one */
	int  ext;			/* the body is assembly code in the run time library; called like any routine */
	int  emitted;			/* its code is in the output, between ;@unit and ;@endunit (for prune.c) */
	char hasdef[MAXPARAM];		/* a parameter with a default value: a call may leave it out */
	int  defval[MAXPARAM];
} routine;

static routine rt[512];
static int     nrt= 0;
static routine *cur= NULL;
static int     in_params= 0;
static char    lastlabel[80]= "";

static void add_param(char *name, int size);
static void check_defaults(routine *r);

static routine *find_routine(char *name) {
	int i;
	for (i= 0; i < nrt; i++)
		if (!strcasecmp(name, rt[i].name)) return &rt[i];
	return NULL;
}

static int ret_kind(char *type) {
	if (!strcasecmp(type, "BYTE")) return 1;
	if (!strcasecmp(type, "CARD") || !strcasecmp(type, "INT")) return 2;
	if (!strcasecmp(type, "REAL")) return 3;
	printf("ERR! FUNC can't return %s!\n", type);
	exit(1);
}

/* ---- classes: inheritance and method tables ---------------------------------------
 * Every instance starts with a hidden pointer to its class's method table.  A
 * subclass has its parent's slots first, then its own new ones; a method with the
 * same name as an inherited one overrides it (same slot, same signature). */
#define MAXCLS   64
#define MAXSLOT  64

typedef struct {
	int  type;				/* type table index */
	int  parent;				/* index in clsreg, or -1 */
	int  nchild;
	int  nslots;
	char sname[MAXSLOT][48];
	int  own[MAXSLOT];			/* routine implementing the slot in this class, -1 if inherited */
	int  sigset[MAXSLOT];
	int  sn[MAXSLOT];			/* parameters not counting this */
	int  spsize[MAXSLOT][MAXPARAM];
	int  sret[MAXSLOT];
	int  done[MAXSLOT];			/* resolved at the end: routine index */
} classinfo;

static classinfo clsreg[MAXCLS];
static int       ncls= 0;

static int class_of_type(int t) {
	int i;
	for (i= 0; i < ncls; i++) if (clsreg[i].type == t) return i;
	return -1;
}

static int slot_find(classinfo *ci, char *name) {
	int i;
	for (i= 0; i < ci->nslots; i++)
		if (!strcasecmp(ci->sname[i], name)) return i;
	return -1;
}

static int find_class_index(char *cname) {
	int t= _type_index_of(cname);

	if (!_type_is_class(t)) {
		printf("ERR! %s is not a class!\n", cname);
		exit(1);
	}
	return t;
}

static routine *find_method(int cls, char *method) {
	char full[200];
	int ci= class_of_type(cls);

	while (ci >= 0) {
		routine *r;
		snprintf(full, sizeof(full), "%s.%s", _type_name_of_index(clsreg[ci].type), method);
		if ((r= find_routine(full))) return r;
		ci= clsreg[ci].parent;
	}
	return NULL;
}

/* the slot a method name has in a class (inherited ones included), -1 if none */
static int method_slot(int cls, char *method) {
	int ci= class_of_type(cls);
	return (ci < 0)? -1: slot_find(&clsreg[ci], method);
}

static void end_routine() {
	extern FILE *outf;
	if (cur && cur->emitted) fprintf(outf, ";@endunit\n");
	cur= NULL;
	_scope_leave();
	_record_resume();
}

static routine *begin_routine(char *name, int ret) {
	routine *r;
	int i, cls= -1;
	char full[200];

	if (strchr(name, '.')) {			/* PROC class.method: defined outside the class body */
		char cn[100], *dot;
		strncpy(cn, name, 99); cn[99]= 0;
		dot= strchr(cn, '.'); *dot++= 0;
		cls= find_class_index(cn);
		snprintf(full, sizeof(full), "%s.%s", _type_name_of_index(cls), dot);
	} else if (_class_open() >= 0) {		/* inside CLASS ... SSALC */
		cls= _class_open();
		snprintf(full, sizeof(full), "%s.%s", _type_name_of_index(cls), name);
	} else {
		strncpy(full, name, 199); full[199]= 0;
	}
	if (find_routine(full)) {
		printf("ERR! routine %s declared twice!\n", full);
		exit(1);
	}
	if (cls >= 0 && strchr(name, '.') == NULL && 0) { }
	if (nrt >= 512) { printf("ERR! too many routines!\n"); exit(1); }
	r= &rt[nrt++];
	memset(r, 0, sizeof(*r));
	strncpy(r->name, full, 63);
	r->ret= ret;
	r->cls= cls;
	sprintf(r->label, "R_");
	for (i= 0; full[i] && strlen(r->label) < 70; i++) {
		if (full[i] == '.') strcat(r->label, "__");
		else sprintf(r->label + strlen(r->label), "%c", (full[i] >= 'a' && full[i] <= 'z')? full[i]-32: full[i]);
	}
	cur= r;
	in_params= 1;
	_record_suspend();			/* a method's locals aren't class fields */
	_scope_enter(nrt);
	if (cls >= 0) {
		classinfo *ci= &clsreg[class_of_type(cls)];
		char *mn= strchr(full, '.') + 1;
		int sl= slot_find(ci, mn);

		if (sl < 0) {				/* a new method: a new slot */
			if (ci->nchild > 0) {
				printf("ERR! %s: a new method can't be added to a class that already has subclasses!\n", full);
				exit(1);
			}
			if (ci->nslots >= MAXSLOT) { printf("ERR! class %s has too many methods!\n", ci->nslots? _type_name_of_index(ci->type): ""); exit(1); }
			sl= ci->nslots++;
			strncpy(ci->sname[sl], mn, 47);
			ci->own[sl]= -1; ci->sigset[sl]= 0;
		}
		ci->own[sl]= nrt - 1;
		r->slot= sl;
	}
	if (cls >= 0) {				/* the hidden first parameter: the instance */
		int sz= _type_size_of_index(cls);
		emit_declaration("this", "card");
		_set_deref("this", sz > 0? sz: 1, cls);
		add_param("this", 2);
	}
	return r;
}

int emit_proc_begin(char *name) {
	begin_routine(name, 0);
	return (0);
}

int emit_func_begin(char *name, char *type) {
	begin_routine(name, ret_kind(type));
	cur->rtc= vtc(_type_index_of(type));
	return (0);
}

static void add_param(char *name, int size) {
	if (cur->nparams >= MAXPARAM) {
		printf("ERR! %s has more than %d parameters!\n", cur->name, MAXPARAM);
		exit(1);
	}
	if (size != 1 && size != 2 && size != 4) {
		printf("ERR! %s: parameter %s must be a BYTE, CARD, INT, REAL, POINTER or ARRAY!\n", cur->name, name);
		exit(1);
	}
	cur->psize[cur->nparams]= size;
	strncpy(cur->pname[cur->nparams], name, 63);
	cur->nparams++;
}

/* the parameter list is complete: the routine starts here and takes its arguments
 * off the stack, last one first */
int emit_proc_params_end() {
	extern FILE *outf;
	int k;

	in_params= 0;
	check_defaults(cur);
	flush_const();
	fprintf(outf, ";@unit\n\n; ---- %s\n%s\n", cur->name, cur->label);
	cur->emitted= 1;
	for (k= cur->nparams-1; k >= 0; k--) {
		char *lab= _get_compiler_var(cur->pname[k]);
		switch (cur->psize[k]) {
			case 1: fprintf(outf, "\t\t_POP8 %s\t\t; %s\n", lab, cur->pname[k]); break;
			case 2: fprintf(outf, "\t\t_POP16 %s\t\t; %s\n", lab, cur->pname[k]); break;
			default: fprintf(outf, "\t\t_POPF %s\t\t; %s\n", lab, cur->pname[k]); break;
		}
	}
	strcpy(lastlabel, cur->label);
	if (cur->cls >= 0 && strcasecmp(strrchr(cur->name, '.')? strrchr(cur->name, '.') + 1: cur->name, "init")) {
		/* a method: its signature must match the slot's.  (init is exempt: a constructor
		 * is only ever called for its own class, never through the method table) */
		classinfo *ci= &clsreg[class_of_type(cur->cls)];
		int sl= cur->slot, i;

		if (!ci->sigset[sl]) {
			ci->sigset[sl]= 1;
			ci->sn[sl]= cur->nparams - 1;
			ci->sret[sl]= cur->ret;
			for (i= 1; i < cur->nparams; i++) ci->spsize[sl][i-1]= cur->psize[i];
		} else {
			int ok= (ci->sn[sl] == cur->nparams - 1 && ci->sret[sl] == cur->ret);
			for (i= 1; ok && i < cur->nparams; i++) ok= (ci->spsize[sl][i-1] == cur->psize[i]);
			if (!ok) {
				printf("ERR! %s overrides a method with a different signature (parameters and result must match)!\n", cur->name);
				exit(1);
			}
		}
	}
	return (0);
}

/* PROC/FUNC name=address(params): a routine already in memory.  Its first
 * three parameter bytes are passed in A, X and Y; a FUNC returns its value
 * in A (BYTE) or A,X (CARD/INT). */
int emit_short_begin(char *name, int addr, int isfunc) {
	extern char _type[];
	routine *r= begin_routine(name, isfunc? ret_kind(_type): 0);
	if (isfunc) r->rtc= vtc(_type_index_of(_type));

	if (isfunc && r->ret == 3) {
		printf("ERR! %s: a routine at a fixed address can't return a REAL!\n", name);
		exit(1);
	}
	if (r->cls >= 0) { printf("ERR! %s: a routine at a fixed address can't be a class method!\n", name); exit(1); }
	r->fixed= 1; r->addr= addr;
	return (0);
}

int emit_short_end() {
	int k, bytes= 0;
	extern FILE *outf;

	in_params= 0;
	check_defaults(cur);
	if (cur->ext) { end_routine(); return (0); }
	for (k= 0; k < cur->nparams; k++) {
		if (cur->psize[k] == 4) {
			printf("ERR! %s: a REAL parameter can't be passed to a routine at a fixed address!\n", cur->name);
			exit(1);
		}
		bytes+= cur->psize[k];
	}
	if (bytes > 3) {
		printf("ERR! %s: a routine at a fixed address takes at most 3 bytes of parameters (A, X, Y)!\n", cur->name);
		exit(1);
	}
	if (cur->inl) {				/* the machine code follows, right here */
		flush_const();
		fprintf(outf, "\n; ---- %s (machine code)\n%s\n", cur->name, cur->label);
	}
	end_routine();
	return (0);
}

/* PROC/FUNC name=@LABEL(params): the routine is assembly code at LABEL (in io.inc and the like).  It
 * is called as any routine is: the arguments are pushed on the run time stack in order, a FUNC
 * leaves its value on the stack. */
int emit_short_begin_ext(char *name, char *label, int isfunc) {
	extern char _type[];
	routine *r= begin_routine(name, isfunc? ret_kind(_type): 0);

	if (isfunc) r->rtc= vtc(_type_index_of(_type));
	if (r->cls >= 0) { printf("ERR! %s: a routine in the run time library can't be a class method!\n", name); exit(1); }
	strncpy(r->label, label, 79); r->label[79]= 0;
	r->ext= 1;
	return (0);
}

/* PROC/FUNC name=*(params)[bytes]: the bytes are the routine */
int emit_short_begin_inline(char *name, int isfunc) {
	extern char _type[];
	routine *r= begin_routine(name, isfunc? ret_kind(_type): 0);

	if (isfunc) r->rtc= vtc(_type_index_of(_type));
	if (isfunc && r->ret == 3) {
		printf("ERR! %s: a machine code routine can't return a REAL!\n", name);
		exit(1);
	}
	if (r->cls >= 0) { printf("ERR! %s: a machine code routine can't be a class method!\n", name); exit(1); }
	r->fixed= 1; r->inl= 1;
	return (0);
}

/* RETURN at the end of a PROC */
int emit_return() {
	extern FILE *outf;

	if (!cur) { printf("ERR! RETURN outside a routine!\n"); exit(1); }
	if (cur->ret) { printf("ERR! FUNC %s must return a value, RETURN(value)!\n", cur->name); exit(1); }
	flush_const();
	fprintf(outf, "\t\trts\n");
	end_routine();
	return (0);
}

static void return_value(int final) {
	extern FILE *outf;

	if (!cur) { printf("ERR! RETURN outside a routine!\n"); exit(1); }
	if (!cur->ret) { printf("ERR! PROC %s can't return a value!\n", cur->name); exit(1); }
	conv_cu= (cur->rtc == 'C');
	convert_top((cur->ret == 3)? 4: 2, cur->name);
	flush_const();
	if (cur->ret == 1) fprintf(outf, "\t\t_TRUNC8\n");
	try_pops(0);
	fprintf(outf, "\t\trts\n");
	if (final) end_routine();
}

/* RETURN(value) at the end of a FUNC */
int emit_func_return() {
	return_value(1);
	return (0);
}

/* RETURN inside a loop or IF: leaves the routine now */
int emit_internal_return(int hasvalue) {
	extern FILE *outf;

	if (!cur) { printf("ERR! RETURN outside a routine!\n"); exit(1); }
	if (hasvalue) { return_value(0); return (0); }
	if (cur->ret) { printf("ERR! FUNC %s must return a value, RETURN(value)!\n", cur->name); exit(1); }
	flush_const();
	try_pops(0);
	fprintf(outf, "\t\trts\n");
	return (0);
}

/* ---- TRY, CATCH, THROW -------------------------------------------------------------
 *   TRY body CATCH [var] handler YRT
 * TRYPUSH (rtl.inc) records the catch address, the hardware stack pointer and the run time
 * stack pointer; a throw puts both pointers back and jumps to the catch address.  The variables are
 * all static, so there is nothing else to undo.  The code is in the variable after CATCH. */
int emit_try_begin() {
	extern FILE *outf;
	cframe *f= cpush(K_TRY);

	f->a= newlabel(); f->b= newlabel();
	flush_const();
	fprintf(outf, "\t\tlda #<L%05d\n\t\tldx #>L%05d\n\t\tjsr TRYPUSH\t\t; TRY\n", f->b, f->b);
	return (0);
}

int emit_catch(char *var) {
	extern FILE *outf;
	cframe *f= ctop(K_TRY, "CATCH");

	flush_const();
	fprintf(outf, "\t\tdec TRYSP\t\t; the body is done: no handler\n");
	jump(f->a);
	putlabel(f->b);				/* a throw arrives here, the handler already taken off */
	f->hasElse= 1;
	if (var) {
		fprintf(outf, "\t\tlda TRYCODE\n\t\tsta TMPA\n\t\tlda #0\n\t\tsta TMPA+1\n\t\tjsr PUSHA\n");
		tpush('B');
		emit_assign(var);
	}
	return (0);
}

int emit_try_end() {
	cframe *f= ctop(K_TRY, "YRT");

	putlabel(f->a);
	csp--;
	return (0);
}

/* THROW e: the nearest TRY gets it.  With none, Error(e) is called, as for a library error. */
int emit_throw() {
	extern FILE *outf;
	routine *er= find_routine("Error");

	if (!er) { printf("ERR! THROW needs a PROC Error(BYTE code): INCLUDE \"IO.ACT\" or write one!\n"); exit(1); }
	convert_top(2, "THROW");
	flush_const();
	fprintf(outf, "\t\tjsr TRYTHROW\t\t; THROW\n");
	note_call(er, 0, -1);
	fprintf(outf, "\t\tjsr %s\t\t; no TRY: the error routine\n", er->label);
	return (0);
}

/* ---- calls and array elements: name( ... ) ------------------------------------------- */

typedef struct {
	int kind;			/* 1 routine, 2 array element, 3 array field element */
	fref f;				/* kind 3: the field */
	routine *r;
	char name[64];
	int argn;
	int virt, slot;			/* a method call dispatched through the method table */
	int vcls;			/* ... and the class of the variable it was called through */
} callframe;

static callframe calls[64];
static int ncalls= 0;

/* ---- arrays of records: "rec ARRAY a(10)" and a(i).field ---------------------------------------------------
 * The array is a BYTE array of 10 * the size of rec.  An element's field is reached by working out the element's
 * address into a hidden pointer to the record, and then using the field through that pointer. */
static struct { char label[80]; char type[64]; int size; } recarr[64];
static int nrecarr= 0;

static int recarr_find(char *label) {
	int i;
	for (i= 0; i < nrecarr; i++) if (!strcmp(recarr[i].label, label)) return i;
	return -1;
}

char *emit_recarr_ref(char *field) {
	callframe c= calls[--ncalls];
	char *lab= _get_compiler_var(c.name);
	int k= lab? recarr_find(lab): -1;
	static int hid= 0;
	char name[32], *res;

	if (k < 0) { printf("ERR! %s(...).%s: %s is not an array of records!\n", c.name, field, c.name); exit(1); }
	if (c.argn != 1) { printf("ERR! %s needs one index!\n", c.name); exit(1); }
	flush_const();
	tpop(); tpush('C');				/* the index is a CARD, so the multiplication doesn't wrap at 256 */
	emit_pushConst(recarr[k].size);
	emit_arithmetic(0, 0, "*");
	emit_pushVar(c.name, "i");			/* the array's address */
	emit_arithmetic(0, 0, "+");
	sprintf(name, "_ra%d", ++hid);
	emit_pointer_declaration(recarr[k].type, name);
	emit_assign(name);
	res= malloc(strlen(name) + strlen(field) + 2);
	sprintf(res, "%s.%s", name, field);
	return res;
}

int emit_call_begin(char *name0) {
	callframe *c;
	char name[160];
	char *dot;

	strncpy(name, name0, 159); name[159]= 0;
	if (ncalls >= 64) { printf("ERR! calls nested too deeply!\n"); exit(1); }
	c= &calls[ncalls];
	memset(c, 0, sizeof(*c));

	/* inside a method a bare name may be a member of the instance */
	if (!strchr(name, '.') && cur && cur->cls >= 0 && !_scope_has_local(name)) {
		char *q= qual(name);
		if (strcmp(q, name)) strcpy(name, q);
		else {
			routine *m= find_method(cur->cls, name);
			if (m) {				/* a call to a sibling method: pass this along */
				ncalls++;
				strncpy(c->name, m->name, 63);
				c->kind= 1; c->r= m;
				c->virt= strcasecmp(name, "init") != 0; c->slot= method_slot(cur->cls, name); c->vcls= cur->cls;
				emit_pushVar("this", "i");
				c->argn= 1;
				return (0);
			}
		}
	}
	ncalls++;
	strncpy(c->name, name, 63);

	if ((dot= strchr(name, '.')) && !strncasecmp(name, "super.", 6)) {	/* super.m(): the parent's m, not dispatched */
		int pc= (cur && cur->cls >= 0)? clsreg[class_of_type(cur->cls)].parent: -1;
		routine *m;

		if (pc < 0) { printf("ERR! %s(): SUPER is only valid in a method of a class with a parent!\n", name); exit(1); }
		m= find_method(clsreg[pc].type, dot + 1);
		if (!m) { printf("ERR! %s(): the parent has no method %s!\n", name, dot + 1); exit(1); }
		c->kind= 1; c->r= m;
		strncpy(c->name, m->name, 63);
		emit_pushVar("this", "i");
		c->argn= 1;
		return (0);
	}
	if ((dot= strchr(name, '.'))) {
		char base[100];
		int t, d, cls= -1;

		strncpy(base, name, dot - name); base[dot - name]= 0;
		dot++;
		if (!_get_compiler_var(base)) {
			printf("ERR! %s(): %s is not declared!\n", name, base);
			exit(1);
		}
		t= _var_type(base);
		if (_type_is_record(d= _get_deref_type(base)) && _get_deref(base) > 0) t= d;
		if (!_type_is_record(t)) {
			printf("ERR! %s(): %s is not a record or class!\n", name, base);
			exit(1);
		}
		if (_field_array(t, dot, NULL, NULL)) {
			resolve_field(name, &c->f);
			c->kind= 3;
			return (0);
		}
		if (_type_is_class(t)) cls= t;
		if (cls >= 0 && find_method(cls, dot) && t == d) {
			routine *m= find_method(cls, dot);
			c->kind= 1; c->r= m;
			c->virt= strcasecmp(dot, "init") != 0; c->slot= method_slot(cls, dot); c->vcls= cls;	/* init is called directly */
			strncpy(c->name, m->name, 63);
			emit_pushVar(base, "i");		/* the instance is the first argument */
			c->argn= 1;
			return (0);
		}
		printf("ERR! %s(): %s has no array field or method %s!\n", name, base, dot);
		exit(1);
	}
	if (_get_array(name, NULL, NULL)) {
		c->kind= 2;
	} else if ((c->r= find_routine(name))) {
		c->kind= 1;
		if (c->r->cls >= 0) {
			printf("ERR! %s is a method, call it as instance.%s()!\n", name, name);
			exit(1);
		}
	} else {
		printf("ERR! %s is not a declared routine or array!\n", name);
		exit(1);
	}
	return (0);
}

/* CARD a=0 in a parameter list: the value a call gets if it leaves the argument out */
int emit_param_default(int v) {
	int k;

	if (!cur || !in_params || cur->nparams < 1) { printf("ERR! internal: default value outside a parameter list!\n"); exit(1); }
	k= cur->nparams - 1;
	if (cur->psize[k] == 4) { printf("ERR! %s: a REAL parameter can't have a default value!\n", cur->name); exit(1); }
	cur->hasdef[k]= 1;
	cur->defval[k]= v & 0xffff;
	return (0);
}

/* the parameters with defaults are the last ones */
static void check_defaults(routine *r) {
	int k, seen= 0;
	for (k= 0; k < r->nparams; k++) {
		if (r->hasdef[k]) seen= 1;
		else if (seen) { printf("ERR! %s: a parameter without a default value follows one with a default!\n", r->name); exit(1); }
	}
}

static int required_args(routine *r) {
	int n= r->nparams;
	while (n > 0 && r->hasdef[n-1]) n--;
	return n;
}

/* a call that leaves out trailing arguments gets their default values */
static void fill_defaults(callframe *c) {
	extern FILE *outf;
	routine *r= c->r;

	if (c->kind != 1) return;
	while (c->argn < r->nparams && r->hasdef[c->argn]) {
		flush_const();
		fprintf(outf, "; default argument\n\t\t_PUSHI $%04x\n", r->defval[c->argn]);
		tpush('C');
		c->argn++;
	}
	/* Action! lets a call pass fewer arguments than there are parameters; the ones left out are 0 here (in
	   Action! they keep whatever they held last, which is nothing a program can count on) */
	while (c->argn < r->nparams && r->psize[c->argn] <= 2 && required_args(r) == r->nparams) {
		flush_const();
		fprintf(outf, "; argument left out\n\t\t_PUSHI 0\n");
		tpush('C');
		c->argn++;
	}
}

/* an argument (or the array index) has been evaluated onto the stack */
int emit_call_arg() {
	callframe *c= &calls[ncalls-1];
	int size;

	if (c->kind == 2 || c->kind == 3) {
		if (c->argn >= 1) { printf("ERR! %s: arrays have one index!\n", c->name); exit(1); }
		size= 2;
	} else {
		if (c->argn >= c->r->nparams) {
			printf("ERR! %s takes %d argument(s), too many given!\n", c->name, c->r->nparams);
			exit(1);
		}
		size= c->r->psize[c->argn];
	}
	{
		int t= tpeek();
		conv_cu= (c->kind == 1 && c->argn < c->r->nparams && c->r->pcard[c->argn]);
		convert_top((size == 4)? 4: 2, c->name);
		tpush((size == 4)? 'R': (t == 'R')? 'I': t);
	}
	c->argn++;
	return (0);
}

/* "text" given as an argument: the address of the string */
int emit_call_string(char *quoted) {
	extern FILE *outf;
	int n= make_string(quoted);

	flush_const();
	fprintf(outf, "; Push STRING\n\t\t_PUSHI STR%05d\n", n);
	tpush('C');
	return emit_call_arg();
}

/* "text" in an expression: the address of the string (length first) */
int emit_string_literal(char *quoted) {
	extern FILE *outf;
	int n= make_string(quoted);

	flush_const();
	fprintf(outf, "; Push STRING\n\t\t_PUSHI STR%05d\n", n);
	tpush('C');
	tstr[tsp-1]= 1;
	return (0);
}

static void elem_info(char *name, int *kind, int *elsize) {
	*kind= _get_array(name, elsize, NULL);
}

/* base address of an array field into TMPB */
static void field_base(fref *f) {
	extern FILE *outf;
	fprintf(outf, "\t\t_RBASE%c %s,%d\n", f->ptr? 'P': 'D', f->label, f->off);
}

static void field_load(fref *f) {
	extern FILE *outf;

	flush_const();
	field_base(f);
	fprintf(outf, "\t\t_AGETT%d\n", f->elsize);
	tpop();
	tpush((f->elsize == 4)? 'R': vtc(f->ftype));
}

/* the array element a(i): index on the stack -> value on the stack */
static void array_load(char *name) {
	extern FILE *outf;
	int kind, elsize;
	char *lab= _get_compiler_var(name);

	elem_info(name, &kind, &elsize);
	flush_const();
	fprintf(outf, "\t\t_AGET%c%d %s\n", (kind == 1)? 'D': 'I', elsize, lab);
	tpop();
	tpush((elsize == 4)? 'R': vtc((kind == 3)? _get_deref_type(name): _var_type(name)));
}

/* jsr to the routine, or through the instance's method table for a method */
static void emit_call_target(callframe *c) {
	extern FILE *outf;
	int k, b= 0;

	note_call(c->r, c->virt, c->vcls);
	if (!c->virt) { fprintf(outf, "\t\tjsr %s\n", c->r->label); return; }
	for (k= 1; k < c->r->nparams; k++) b+= (c->r->psize[k] == 4)? 4: 2;
	fprintf(outf, "\t\t_VCALL %d,%d\t\t; %s\n", b, c->slot * 2, c->name);
}

int emit_call_end_expr() {
	extern FILE *outf;
	callframe c= calls[--ncalls];
	int k;

	if (c.kind == 2 || c.kind == 3) {
		if (c.argn != 1) { printf("ERR! %s needs an index!\n", c.name); exit(1); }
		if (c.kind == 3) field_load(&c.f); else array_load(c.name);
		return (0);
	}
	fill_defaults(&c);
	if (c.argn != c.r->nparams) {
		if (required_args(c.r) < c.r->nparams)
			printf("ERR! %s takes %d to %d argument(s), %d given!\n", c.name, required_args(c.r), c.r->nparams, c.argn);
		else
			printf("ERR! %s takes %d argument(s), %d given!\n", c.name, c.r->nparams, c.argn);
		exit(1);
	}
	if (!c.r->ret) { printf("ERR! PROC %s has no value, it can't be used in an expression!\n", c.name); exit(1); }
	flush_const();
	for (k= 0; k < c.argn; k++) tpop();
	if (c.r->fixed) {
		/* parameters off the stack into FA.., then A, X, Y */
		int off[MAXPARAM], b= 0, bytes[3][2], nb= 0, i;
		for (k= c.r->nparams-1; k >= 0; k--) {
			fprintf(outf, "\t\tjsr POPA\n\t\tlda TMPA\n\t\tsta FA+%d\n\t\tlda TMPA+1\n\t\tsta FA+%d\n", 2*k, 2*k+1);
		}
		for (k= 0; k < c.r->nparams; k++) {
			off[k]= b;
			for (i= 0; i < c.r->psize[k] && nb < 3; i++, nb++) { bytes[nb][0]= 2*k+i; }
			b+= c.r->psize[k];
		}
		if (nb > 0) fprintf(outf, "\t\tldy #0\n");
		if (nb > 2) fprintf(outf, "\t\tldy FA+%d\n", bytes[2][0]);
		if (nb > 1) fprintf(outf, "\t\tldx FA+%d\n", bytes[1][0]);
		if (nb > 0) fprintf(outf, "\t\tlda FA+%d\n", bytes[0][0]);
		{ char tg[100]; if (c.r->inl) strcpy(tg, c.r->label); else sprintf(tg, "$%04x", c.r->addr); fprintf(outf, "\t\tjsr %s\t\t; %s\n", tg, c.name); }
		if (c.r->ret == 1) fprintf(outf, "\t\tsta TMPA\n\t\tlda #0\n\t\tsta TMPA+1\n\t\tjsr PUSHA\n");
		else fprintf(outf, "\t\tsta TMPA\n\t\tstx TMPA+1\n\t\tjsr PUSHA\n");
	} else {
		emit_call_target(&c);
	}
	tpush(c.r->rtc);
	return (0);
}

/* a call used as a statement; the value of a FUNC is thrown away */
int emit_call_end_stmt() {
	extern FILE *outf;
	callframe c= calls[--ncalls];
	int k;

	if (c.kind == 2 || c.kind == 3) {
		printf("ERR! %s(...) is an array, it can't be used as a statement!\n", c.name);
		exit(1);
	}
	fill_defaults(&c);
	if (c.argn != c.r->nparams) {
		if (required_args(c.r) < c.r->nparams)
			printf("ERR! %s takes %d to %d argument(s), %d given!\n", c.name, required_args(c.r), c.r->nparams, c.argn);
		else
			printf("ERR! %s takes %d argument(s), %d given!\n", c.name, c.r->nparams, c.argn);
		exit(1);
	}
	flush_const();
	for (k= 0; k < c.argn; k++) tpop();
	if (c.r->fixed) {
		int bytes[3], nb= 0, i;
		for (k= c.r->nparams-1; k >= 0; k--)
			fprintf(outf, "\t\tjsr POPA\n\t\tlda TMPA\n\t\tsta FA+%d\n\t\tlda TMPA+1\n\t\tsta FA+%d\n", 2*k, 2*k+1);
		for (k= 0; k < c.r->nparams; k++)
			for (i= 0; i < c.r->psize[k] && nb < 3; i++) bytes[nb++]= 2*k+i;
		if (nb > 2) fprintf(outf, "\t\tldy FA+%d\n", bytes[2]);
		if (nb > 1) fprintf(outf, "\t\tldx FA+%d\n", bytes[1]);
		if (nb > 0) fprintf(outf, "\t\tlda FA+%d\n", bytes[0]);
		{ char tg[100]; if (c.r->inl) strcpy(tg, c.r->label); else sprintf(tg, "$%04x", c.r->addr); fprintf(outf, "\t\tjsr %s\t\t; %s\n", tg, c.name); }
	} else {
		emit_call_target(&c);
		if (c.r->ret) fprintf(outf, "\t\t_DROP %d\t\t; unused result\n", (c.r->ret == 3)? 4: 2);
	}
	return (0);
}

/* a(i) = value: the stack holds the index, then the value */
int emit_array_store() {
	extern FILE *outf;
	callframe c= calls[--ncalls];
	int kind, elsize;

	if (c.kind != 2 && c.kind != 3) { printf("ERR! %s is not an array!\n", c.name); exit(1); }
	if (c.argn != 1) { printf("ERR! %s needs an index!\n", c.name); exit(1); }
	if (c.kind == 3) {
		conv_cu= (vtc(c.f.ftype) == 'C');
		convert_top((c.f.elsize == 4)? 4: 2, c.name);
		flush_const();
		tpop();
		field_base(&c.f);
		fprintf(outf, "\t\t_ASETT%d\n", c.f.elsize);
		return (0);
	}
	elem_info(c.name, &kind, &elsize);
	conv_cu= (vtc((kind == 3)? _get_deref_type(c.name): _var_type(c.name)) == 'C');
	convert_top((elsize == 4)? 4: 2, c.name);
	flush_const();
	tpop();					/* the index */
	fprintf(outf, "\t\t_ASET%c%d %s\n", (kind == 1)? 'D': 'I', elsize, _get_compiler_var(c.name));
	return (0);
}

/* a(i) ==op value: keep a copy of the index, load the element */
int emit_array_compound_begin() {
	extern FILE *outf;
	callframe *c= &calls[ncalls-1];

	if (c->kind != 2 && c->kind != 3) { printf("ERR! %s is not an array!\n", c->name); exit(1); }
	if (c->argn != 1) { printf("ERR! %s needs an index!\n", c->name); exit(1); }
	flush_const();
	fprintf(outf, "\t\t_DUP16\n");
	tpush('C');
	if (c->kind == 3) field_load(&c->f); else array_load(c->name);
	return (0);
}

/* ---- array declarations ------------------------------------------------------------- */

static int  a_size= -1, a_nvals= 0, a_haveaddr= 0, a_addr= 0, a_havestr= 0;
static int  a_vals[4096];
static char a_str[300];

int emit_array_size(int n) { a_size= n; return (0); }
int emit_array_value(int v) {
	if (a_nvals >= 4096) { printf("ERR! array initializer too long!\n"); exit(1); }
	a_vals[a_nvals++]= v;
	return (0);
}
int emit_array_addr(int a) { a_haveaddr= 1; a_addr= a; return (0); }
int emit_array_string(char *quoted) {
	strncpy(a_str, unquote(quoted), 299);
	a_havestr= 1;
	return (0);
}

/* ident (and its size / initial values, collected above) is complete */
int emit_array_declare(char *type, char *ident) {
	int elsize= _type_sizeof(type);
	int count, i, kind;
	char *cname;
	unsigned char b[4];

	if (!_in_record() && _type_index_of(type) >= 0 && _type_is_record(_type_index_of(type)) && !_type_is_class(_type_index_of(type))) {
		int rs= _type_sizeof(type);
		if (a_havestr || a_nvals > 0) { printf("ERR! %s: an array of records can't have initial values!\n", ident); exit(1); }
		if (!a_haveaddr && a_size < 1) { printf("ERR! %s: an array of records needs a size, %s ARRAY %s(n)!\n", ident, type, ident); exit(1); }
		if (nrecarr >= 64) { printf("ERR! too many arrays of records!\n"); exit(1); }
		_valloc(ident, "BYTE");
		cname= _get_compiler_var(ident);
		if (!datf) datf= tmpfile();
		count= (a_size > 0)? a_size * rs: 0;
		if (a_haveaddr) fprintf(datf, "\t\t.map %s @ $%04x\t; %s (%s ARRAY) at fixed address\n", cname, a_addr & 0xffff, ident, type);
		else zvar(cname, count, "%s (%s ARRAY %d)", ident, type, a_size);
		_set_array(ident, 1, 1, count);
		strncpy(recarr[nrecarr].label, cname, 79); recarr[nrecarr].label[79]= 0;
		strncpy(recarr[nrecarr].type, type, 63); recarr[nrecarr].type[63]= 0;
		recarr[nrecarr].size= rs;
		nrecarr++;
		a_size= -1; a_nvals= 0; a_haveaddr= 0; a_havestr= 0;
		return (0);
	}

	if (elsize != 1 && elsize != 2 && elsize != 4) {
		printf("ERR! %s: arrays of %s are not supported (BYTE, CARD, INT and REAL only)!\n", ident, type);
		exit(1);
	}
	if (_in_record()) {			/* extension: a fixed size array field */
		if (a_haveaddr || a_havestr || a_nvals > 0 || a_size < 1) {
			printf("ERR! %s: an array field needs a size, ARRAY %s(n), and can't have an address or initial values!\n", ident, ident);
			exit(1);
		}
		_record_add_array_field(ident, elsize, a_size);
		_record_set_ftype(_type_index_of(type));
		a_size= -1;
		return (0);
	}

	_valloc(ident, type);
	cname= _get_compiler_var(ident);
	if (!datf) datf= tmpfile();

	if (a_haveaddr) {
		/* a=$600: the data is already there */
		kind= 1; count= (a_size > 0)? a_size: 0;
		fprintf(datf, "\t\t.map %s @ $%04x\t; %s (%s ARRAY) at fixed address\n", cname, a_addr & 0xffff, ident, type);
	} else if (a_havestr) {
		int n= strlen(a_str);
		if (elsize != 1) { printf("ERR! %s: a string can only initialize a BYTE array!\n", ident); exit(1); }
		count= n + 1;
		if (a_size >= 0 && a_size < count) { printf("ERR! %s: string is longer than the array!\n", ident); exit(1); }
		if (a_size > count) count= a_size;
		kind= 1;
		fprintf(datf, "%s\t\t.byte %d", cname, n);
		for (i= 0; i < n; i++) fprintf(datf, ",$%02x", (unsigned char)a_str[i]);
		if (count > n + 1) fprintf(datf, "\n\t\t.bzero %d", count - n - 1);
		fprintf(datf, "\t\t; %s (%s ARRAY)\n", ident, type);
	} else if (a_nvals > 0) {
		if (a_size >= 0 && a_nvals > a_size) { printf("ERR! %s: more initial values than elements!\n", ident); exit(1); }
		count= (a_size > a_nvals)? a_size: a_nvals;
		kind= 1;
		fprintf(datf, "%s", cname);
		for (i= 0; i < a_nvals; i++) {
			if (elsize == 1) fprintf(datf, "\t\t.byte %d\n", a_vals[i] & 0xff);
			else if (elsize == 2) fprintf(datf, "\t\t.word %d\n", a_vals[i] & 0xffff);
			else { fp_encode((double)a_vals[i], b); fprintf(datf, "\t\t.byte $%02x,$%02x,$%02x,$%02x\n", b[0], b[1], b[2], b[3]); }
		}
		if (count > a_nvals) fprintf(datf, "\t\t.bzero %d\n", (count - a_nvals) * elsize);
		fprintf(datf, "\t\t\t\t\t; %s (%s ARRAY)\n", ident, type);
	} else if (a_size >= 0) {
		count= a_size;
		kind= 1;
		if (count <= 0) { printf("ERR! %s: array size must be at least 1!\n", ident); exit(1); }
		zvar(cname, count * elsize, "%s (%s ARRAY %d)", ident, type, count);
	} else {
		/* no size, no address: a 2 byte variable that will hold the address of an array */
		kind= 2; count= 0;
		zvar(cname, 2, "%s (%s ARRAY, address set at run time)", ident, type);
	}
	_set_array(ident, kind, elsize, count);

	a_size= -1; a_nvals= 0; a_haveaddr= 0; a_havestr= 0;
	return (0);
}

/* STRING s(40)  ZTRING z(40)="text"  STRING s="text": a BYTE array of capacity + 1 bytes.  A STRING has its
 * length first; a ZTRING ends in a zero byte.  Without a size the capacity is that of the text, or 40. */
int emit_string_declare(char *kw, char *ident, int cap, char *init) {
	int kind= (toupper((unsigned char)kw[0]) == 'Z')? 2: 1;
	char *cname;
	int i, n= 0;

	if (_in_record()) { printf("ERR! %s: a record can't have a STRING or ZTRING field, use BYTE ARRAY %s(n)!\n", ident, ident); exit(1); }
	if (init) n= strlen(unquote(init));
	if (cap < 0) cap= init? n: 40;
	if (cap < 1) cap= 1;
	if (n > cap) { printf("ERR! %s: the text is longer than %d characters!\n", ident, cap); exit(1); }
	if (kind == 1 && cap > 255) { printf("ERR! %s: a STRING holds at most 255 characters!\n", ident); exit(1); }
	emit_array_size(cap + 1);
	if (init) {
		if (kind == 1) emit_array_string(init);
		else { char *u= unquote(init); for (i= 0; i < n; i++) emit_array_value((unsigned char)u[i]); }
	}
	emit_array_declare("BYTE", ident);
	cname= _get_compiler_var(ident);
	if (nstrv >= 512) { printf("ERR! too many STRING variables!\n"); exit(1); }
	snprintf(strv[nstrv].label, sizeof strv[nstrv].label, "%s", cname);
	strv[nstrv].kind= kind; strv[nstrv].cap= cap;
	nstrv++;
	return (0);
}

/* STRING s as a routine parameter: the address of a string, of unknown capacity */
int emit_string_param(char *kw, char *ident) {
	char *cname;

	emit_array_param("BYTE", ident);
	if (in_params && cur && cur->fixed) return (0);
	cname= _get_compiler_var(ident);
	if (nstrv >= 512) { printf("ERR! too many STRING variables!\n"); exit(1); }
	snprintf(strv[nstrv].label, sizeof strv[nstrv].label, "%s", cname);
	strv[nstrv].kind= (toupper((unsigned char)kw[0]) == 'Z')? 2: 1; strv[nstrv].cap= 255;
	nstrv++;
	return (0);
}

/* BYTE ARRAY a as a routine parameter: the argument is the array's address */
int emit_array_param(char *type, char *ident) {
	int elsize= _type_sizeof(type);

	if (elsize != 1 && elsize != 2 && elsize != 4) {
		printf("ERR! %s: arrays of %s are not supported!\n", ident, type);
		exit(1);
	}
	if (in_params && cur && cur->fixed) { add_param(ident, 2); return (0); }
	_valloc(ident, type);
	if (!datf) datf= tmpfile();
	zvar(_get_compiler_var(ident), 2, "%s (%s ARRAY parameter)", ident, type);
	_set_array(ident, 2, elsize, 0);
	if (in_params) add_param(ident, 2);
	return (0);
}


/* a routine parameter: BYTE a / CARD POINTER p / REAL r (kind 0 var, 1 pointer) */
int emit_param(char *type, char *ident, int kind) {
	extern FILE *outf;
	int size= (kind == 1)? 2: _type_sizeof(type);

	if (!cur || !in_params) { printf("ERR! internal: parameter %s outside a parameter list!\n", ident); exit(1); }
	if (size < 0) { printf("ERR! %s: %s unknown type!\n", ident, type); exit(1); }
	if (kind == 0 && _type_is_class(_type_index_of(type))) { kind= 1; size= 2; }	/* a class is passed by reference */
	if (kind == 0 && _type_is_record(_type_index_of(type))) {
		printf("ERR! %s: records can't be passed as parameters, pass a %s POINTER!\n", ident, type);
		exit(1);
	}
	if (!cur->fixed) {
		if (kind == 1) emit_pointer_declaration(type, ident);
		else emit_declaration(ident, type);
	}
	add_param(ident, size);
	cur->pcard[cur->nparams-1]= (kind == 0 && vtc(_type_index_of(type)) == 'C');
	return (0);
}

static char *entry_label() {
	int i;
	for (i= nrt-1; i >= 0; i--)
		if (!rt[i].fixed && !rt[i].ext && rt[i].cls < 0) return rt[i].label;
	return NULL;
}

static void check_open_blocks() {
	if (csp > 0) { printf("ERR! IF/DO/CASE/TRY block not closed at the end of the program!\n"); exit(1); }
	if (cur) { printf("ERR! routine %s has no RETURN!\n", cur->name); exit(1); }
}


/* ---- members of the current instance ---------------------------------------------- */

static char qbuf[320];

/* inside a method, a name that isn't a local but is a field of the class is this.name */
static char *qual(char *ident) {
	if (!cur || cur->cls < 0 || strchr(ident, '.')) return ident;
	if (_scope_has_local(ident)) return ident;
	{
		int off, size, ds, dt;
		if (_field_lookup(cur->cls, ident, &off, &size, &ds, &dt) == 0) {
			snprintf(qbuf, sizeof(qbuf), "this.%s", ident);
			return qbuf;
		}
	}
	return ident;
}

/* @r.f : address of a field */
static int push_field_addr(char *cname) {
	extern FILE *outf;
	fref r;

	flush_const();
	resolve_field(cname, &r);
	fprintf(outf, "; Push ADDRESS OF %s\n", cname);
	fprintf(outf, "\t\t_PUSHFA%c %s,%d\n", r.ptr? 'P': 'D', r.label, r.off);
	tpush('C');
	return (0);
}

/* ---- DEFINE (text substitution done by the lexer) -------------------------------------- */

typedef struct { char name[64]; char *text; } defrec;
static defrec defs[2048];
static int    ndefs= 0;

char *define_lookup(char *name) {
	int i;
	for (i= 0; i < ndefs; i++)
		if (!strcasecmp(name, defs[i].name)) return defs[i].text;
	return NULL;
}

static void define_set(char *name, char *text) {
	int i;
	for (i= 0; i < ndefs; i++)
		if (!strcasecmp(name, defs[i].name)) { free(defs[i].text); defs[i].text= strdup(text); return; }
	if (ndefs >= 2048) { printf("ERR! too many DEFINEs!\n"); exit(1); }
	strncpy(defs[ndefs].name, name, 63);
	defs[ndefs].text= strdup(text);
	ndefs++;
}

int emit_define(char *name, char *quoted) {
	define_set(name, unquote(quoted));
	return (0);
}

int emit_define_number(char *name, int value) {
	char t[32];
	sprintf(t, "%d", value);
	define_set(name, t);
	return (0);
}

/* ---- SET addr=value ----------------------------------------------------------------------- */

#define CC_NONNUM 0x40000000

/* Action! pokes the compiler's own memory.  Two of those locations are meaningful
 * here: $E, where the code goes, and $491, where the variables go. */
int emit_set(int a, int b) {
	extern FILE *outf;

	flush_const();
	if (!(a & CC_NONNUM) && ((a & 0xffff) == 0x0e || (a & 0xffff) == 0x491)) {
		if (b & CC_NONNUM) {
			printf("ERR! SET $%X needs a number, not a name or *!\n", a & 0xffff);
			exit(1);
		}
		if ((a & 0xffff) == 0x0e) {
			fprintf(outf, "\t\t.org $%04x\t\t; SET $E: code origin\n", b & 0xffff);
		} else {
			data_org= b & 0xffff;
			data_org_set= 1;
		}
		return (0);
	}
	fprintf(outf, "; SET (no effect)\n");
	fprintf(stderr, "WARNING: SET has no effect in this compiler (only $E, the code origin, and $491, the variable origin)\n");
	return (0);
}

/* ---- SIZEOF ----------------------------------------------------------------------------- */

int emit_sizeof_field(char *cname) {
	char base[100], *dot;
	int t, d, off, size, ds, dt;

	strncpy(base, cname, 99); base[99]= 0;
	dot= strchr(base, '.'); *dot++= 0;
	if (_get_compiler_var(base)) {
		t= _var_type(base);
		if (_type_is_record(d= _get_deref_type(base)) && _get_deref(base) > 0) t= d;
	} else {
		t= _type_index_of(base);			/* SIZEOF(rec.field) names the type */
	}
	if (!_type_is_record(t)) {
		printf("ERR! SIZEOF(%s): %s is not a record or class!\n", cname, base);
		exit(1);
	}
	if (_field_lookup(t, dot, &off, &size, &ds, &dt) < 0) {
		printf("ERR! SIZEOF(%s): no field %s!\n", cname, dot);
		exit(1);
	}
	return size;
}

int emit_sizeof_name(char *name) {
	int n;

	name= qual(name);
	if (strchr(name, '.')) return emit_sizeof_field(name);
	if (_get_compiler_var(name)) {
		if (_get_array(name, NULL, NULL) == 2) return 2;	/* an address variable */
		n= _var_total_size(name);
		if (n > 65535) {
			printf("ERR! SIZEOF(%s) is %d, too big for a 16 bit value!\n", name, n);
			exit(1);
		}
	} else {
		n= _type_size_of_index(_type_index_of(name));
	}
	if (n < 0) {
		printf("ERR! SIZEOF(%s): %s is not a type or a variable!\n", name, name);
		exit(1);
	}
	return n;
}

/* ---- NEW type: a zeroed instance from the heap, its address on the stack -------------- */

int emit_new(char *tname) {
	extern FILE *outf;
	int t= _type_index_of(tname);
	int size= _type_size_of_index(t);

	if (!_type_is_record(t)) {
		printf("ERR! NEW %s: %s is not a TYPE record or a CLASS!\n", tname, tname);
		exit(1);
	}
	if (size < 1) size= 1;
	flush_const();
	if (_type_is_class(t)) {
		char vt[100];
		routine *in;

		vt_label(vt, _type_name_of_index(t));
		fprintf(outf, "\t\t_NEWC %d,%s\t\t; NEW %s\n", size, vt, tname);
		tpush('C');
		if ((in= find_method(t, "init"))) {		/* the constructor runs on the new instance */
			if (in->nparams != 1) {
				printf("ERR! NEW %s: the constructor %s takes %d argument(s), write NEW %s(...)!\n", tname, in->name, in->nparams - 1, tname);
				exit(1);
			}
			note_call(in, 0, -1);
			fprintf(outf, "\t\t_DUP16\n\t\tjsr %s\t\t; init\n", in->label);
			if (in->ret) fprintf(outf, "\t\t_DROP %d\n", (in->ret == 3)? 4: 2);
		}
		return (0);
	}
	fprintf(outf, "\t\t_NEW %d\t\t; NEW %s\n", size, tname);
	tpush('C');
	return (0);
}

/* NEW cls(args): the instance is made, then init runs on a copy of its reference with the
 * arguments.  The reference stays on the stack as the value of the NEW. */
int emit_new_begin(char *tname) {
	extern FILE *outf;
	int t= _type_index_of(tname);
	char vt[100];
	callframe *c;
	routine *in;
	int size= _type_size_of_index(t);

	if (!_type_is_class(t)) {
		printf("ERR! NEW %s(...): arguments can only be given to a CLASS (%s is not one)!\n", tname, tname);
		exit(1);
	}
	if (!(in= find_method(t, "init"))) {
		printf("ERR! NEW %s(...): the class has no constructor init!\n", tname);
		exit(1);
	}
	if (ncalls >= 64) { printf("ERR! calls nested too deeply!\n"); exit(1); }
	if (size < 1) size= 1;
	flush_const();
	vt_label(vt, _type_name_of_index(t));
	fprintf(outf, "\t\t_NEWC %d,%s\t\t; NEW %s(...)\n", size, vt, tname);
	tpush('C');
	fprintf(outf, "\t\t_DUP16\n");
	tpush('C');
	c= &calls[ncalls++];
	memset(c, 0, sizeof(*c));
	c->kind= 1; c->r= in; c->virt= 0; c->argn= 1;
	strncpy(c->name, in->name, 63);
	return (0);
}

int emit_new_end() {
	extern FILE *outf;
	callframe c= calls[--ncalls];
	int k;

	fill_defaults(&c);
	if (c.argn != c.r->nparams) {
		printf("ERR! NEW: the constructor %s takes %d argument(s), %d given!\n", c.name, c.r->nparams - 1, c.argn - 1);
		exit(1);
	}
	flush_const();
	for (k= 0; k < c.argn; k++) tpop();		/* the arguments and the copy of the reference */
	note_call(c.r, 0, -1);
	fprintf(outf, "\t\tjsr %s\t\t; init\n", c.r->label);
	if (c.r->ret) fprintf(outf, "\t\t_DROP %d\n", (c.r->ret == 3)? 4: 2);
	return (0);
}

/* ---- recursion check ------------------------------------------------------------------------
 * Every variable and parameter has one fixed address, so a routine that is still running when
 * it is called again would overwrite its own variables.  Each call is recorded as an edge from
 * the routine it is in to the routine it calls.  A call through the method table can reach the
 * method in the class of the variable it was called through, or any override in a subclass.
 * At the end of the program the graph is searched for a cycle and a cycle is an error. */

typedef struct { routine *from, *to; int virt, vcls; } calledge;
static calledge edges[8192];
static int nedges= 0;

static void note_call(routine *to, int virt, int vcls) {
	if (to->ext && !strncmp(to->label, "IO_", 3)) need_io= 1;
	if (to->ext && !strncmp(to->label, "MS_", 3)) need_misc= 1;
	if (to->ext && (!strncmp(to->label, "GR_", 3) || !strncmp(to->label, "SN_", 3))) need_gr= 1;
	if (!cur || to->fixed) return;			/* code outside a routine, or machine code at a fixed address */
	if (nedges >= 8192) { printf("ERR! too many calls in the program!\n"); exit(1); }
	edges[nedges].from= cur; edges[nedges].to= to;
	edges[nedges].virt= virt; edges[nedges].vcls= vcls;
	nedges++;
}

static int class_is_a(int type, int anc) {
	int ci= class_of_type(type);
	while (ci >= 0) {
		if (clsreg[ci].type == anc) return 1;
		ci= clsreg[ci].parent;
	}
	return 0;
}

static char *simple_name(routine *r) {
	char *d= strrchr(r->name, '.');
	return d? d + 1: r->name;
}

/* does the call along edge k reach routine t? */
static int edge_reaches(calledge *k, routine *t) {
	if (!k->virt) return k->to == t;
	if (t == k->to) return 1;				/* the method the variable's own class resolves it to */
	return t->cls >= 0 && !strcasecmp(simple_name(t), simple_name(k->to)) && class_is_a(t->cls, k->vcls);	/* or an override below it */
}

static int rec_state[512];			/* 0 not seen, 1 on the path, 2 finished */
static routine *rec_path[512];
static int rec_depth;

static int rec_visit(routine *r) {
	int k, i;

	rec_state[r - rt]= 1;
	rec_path[rec_depth++]= r;
	for (k= 0; k < nedges; k++) {
		if (edges[k].from != r) continue;
		for (i= 0; i < nrt; i++) {
			if (!edge_reaches(&edges[k], &rt[i])) continue;
			if (rec_state[i] == 1) {		/* back on a routine that is still running: a cycle */
				int j;
				printf("ERR! recursion: ");
				for (j= 0; j < rec_depth && rec_path[j] != &rt[i]; j++) ;
				for (; j < rec_depth; j++) printf("%s -> ", rec_path[j]->name);
				printf("%s\n", rt[i].name);
				printf("     Variables are statically allocated, so a routine can't call itself, directly or\n");
				printf("     through another routine%s.\n", edges[k].virt? " or a method call that may reach an override": "");
				return (1);
			}
			if (rec_state[i] == 0 && rec_visit(&rt[i])) return (1);
		}
	}
	rec_state[r - rt]= 2;
	rec_depth--;
	return (0);
}

static void check_recursion() {
	int i;

	memset(rec_state, 0, sizeof(rec_state));
	rec_depth= 0;
	for (i= 0; i < nrt; i++)
		if (rec_state[i] == 0 && rec_visit(&rt[i])) exit(1);
}

/* ---- CLASS ---------------------------------------------------------------------------------- */

int emit_class_begin(char *name, char *parent) {
	int pt= -1, pc= -1, ci, i;

	if (parent) {
		pt= _type_index_of(parent);
		if (!_type_is_class(pt)) { printf("ERR! CLASS %s EXTENDS %s: %s is not a class!\n", name, parent, parent); exit(1); }
		pc= class_of_type(pt);
		if (pc < 0) { printf("ERR! CLASS %s: parent %s is not a complete class!\n", name, parent); exit(1); }
	}
	if (ncls >= MAXCLS) { printf("ERR! too many classes!\n"); exit(1); }
	_class_begin_ext(name, pt);
	ci= ncls++;
	memset(&clsreg[ci], 0, sizeof(classinfo));
	clsreg[ci].type= _type_index_of(name);
	clsreg[ci].parent= pc;
	for (i= 0; i < MAXSLOT; i++) { clsreg[ci].own[i]= -1; clsreg[ci].done[i]= -1; }
	if (pc >= 0) {					/* inherit the parent's slots and their signatures */
		classinfo *p= &clsreg[pc];
		memcpy(clsreg[ci].sname, p->sname, sizeof(p->sname));
		memcpy(clsreg[ci].sigset, p->sigset, sizeof(p->sigset));
		memcpy(clsreg[ci].sn, p->sn, sizeof(p->sn));
		memcpy(clsreg[ci].spsize, p->spsize, sizeof(p->spsize));
		memcpy(clsreg[ci].sret, p->sret, sizeof(p->sret));
		clsreg[ci].nslots= p->nslots;
		p->nchild++;
	}
	return (0);
}

/* the method tables, emitted with the data */
static void emit_vtables(FILE *out) {
	int c, i;

	for (c= 0; c < ncls; c++) {
		classinfo *ci= &clsreg[c];
		char vt[100];

		vt_label(vt, _type_name_of_index(ci->type));
		fprintf(out, "%s", vt);
		if (ci->nslots == 0) fprintf(out, "\t\t.word 0\t\t; class %s has no methods\n", _type_name_of_index(ci->type));
		for (i= 0; i < ci->nslots; i++) {
			int k= c, impl= -1;
			while (k >= 0 && impl < 0) {		/* nearest class that defines it */
				if (i < clsreg[k].nslots && clsreg[k].own[i] >= 0) impl= clsreg[k].own[i];
				k= clsreg[k].parent;
			}
			if (impl < 0) {
				printf("ERR! method %s of class %s has no implementation!\n", ci->sname[i], _type_name_of_index(ci->type));
				exit(1);
			}
			fprintf(out, "%s\t\t.word %s\t\t; %s\n", i? "": "", rt[impl].label, rt[impl].name);
		}
	}
}

int emit_class_end() {
	if (cur) { printf("ERR! a method is not finished at the end of the class!\n"); exit(1); }
	_record_end();
	return (0);
}


/* ---- inline machine code ------------------------------------------------------------------
 * An item is the text "N<number>" or "L<assembler expression>". */
static char inl_buf[8][200];
static int  inl_n= 0;

static char *inl_new() { return inl_buf[inl_n++ & 7]; }

char *inline_num(int v) {
	char *s= inl_new();
	sprintf(s, "N%d", v);
	return s;
}

char *inline_name(char *id) {
	char *s= inl_new();
	char *lab= _get_compiler_var(id);
	routine *r;

	if (lab) { sprintf(s, "L%s", lab); return s; }
	if ((r= find_routine(id))) {
		if (r->fixed && !r->inl) sprintf(s, "N%d", r->addr);
		else sprintf(s, "L%s", r->label);
		return s;
	}
	printf("ERR! machine code: %s is not a declared variable or routine!\n", id);
	exit(1);
}

char *inline_here() {
	printf("ERR! machine code: * (the current address) is not supported, use a label name!\n");
	exit(1);
}

char *inline_add(char *a, char *b) {
	char *s= inl_new();

	if (a[0] == 'N' && b[0] == 'N') sprintf(s, "N%d", atoi(a+1) + atoi(b+1));
	else if (a[0] == 'L' && b[0] == 'N') sprintf(s, "%s+%d", a, atoi(b+1));
	else if (a[0] == 'N' && b[0] == 'L') sprintf(s, "L%s+%d", b+1, atoi(a+1));
	else { printf("ERR! machine code: two addresses can't be added!\n"); exit(1); }
	return s;
}

int emit_inline_item(char *s) {
	extern FILE *outf;

	flush_const();
	if (s[0] == 'N') {
		int v= atoi(s+1);
		if (v >= -128 && v <= 255) fprintf(outf, "\t\t.byte $%02x\n", v & 0xff);
		else fprintf(outf, "\t\t.word $%04x\n", v & 0xffff);
	} else {
		fprintf(outf, "\t\t.word %s\n", s+1);
	}
	return (0);
}
