/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "asm.h"
#include "asmtypes.h"

// Symbol table.
//
// Symbol names are not case sensitive. The assembler makes two passes over
// the source:
//
//   pass 0  defines every label and constant. A symbol that is used before it
//           is defined (a forward reference) has no value yet, so it is
//           treated as 0 and the instruction using it gets its absolute form.
//   pass 1  generates the code. Symbols now have their pass 0 value. A symbol
//           that has not been (re)defined yet in this pass is still treated as
//           a forward reference, so every instruction gets the same size it
//           had in pass 0 and the labels do not move. A label that does move
//           is reported as a phase error.

#define MAXNAME		64
#define HASHSIZE	1021

typedef struct sym {
	char		name[MAXNAME];
	int		value;
	int		type;		// SYM_LABEL, SYM_MAP or SYM_CONST
	int		defpass;	// last pass this symbol was defined in (-1= never)
	int		line;		// line of the definition in pass 0
	const char	*file;		// and the file it was in
	int		predef;		// 0= ordinary, 1= predefined value (-D or built in), 2= the current target
	struct sym	*next;
} SYM;

static SYM *_table[HASHSIZE];

static int _fwd;		// set when the expression being parsed used a forward reference

static unsigned hashName (const char *s) {
	unsigned h= 5381;

	while (*s)
		h= h*33 + (unsigned char) *s++;

	return h % HASHSIZE;
}

// make a lower case copy of a name (the table is not case sensitive)

static void lowerName (char *dest, const char *src) {
	size_t i;

	if (strlen(src) >= MAXNAME)
		asmError("symbol name '%s' is too long (maximum is %d characters)", src, MAXNAME-1);

	for (i= 0; i < MAXNAME-1 && src[i]; i++)
		dest[i]= (char) tolower((unsigned char) src[i]);

	dest[i]= 0;
}

static SYM *findSym (const char *lname) {
	SYM *e;

	for (e= _table[hashName(lname)]; e; e= e->next)
		if (!strcmp(e->name, lname))
			return e;

	return NULL;
}

static SYM *addSym (const char *lname) {
	SYM *e= (SYM *) calloc(1, sizeof(SYM));
	unsigned h= hashName(lname);

	strcpy(e->name, lname);
	e->defpass= -1;
	e->next= _table[h];
	_table[h]= e;

	return e;
}

// returns whether the expression just parsed used a forward reference, and
// clears the flag

int take_fwd (void) {
	int f= _fwd;

	_fwd= 0;
	return f;
}

void clear_fwd (void) {
	_fwd= 0;
}

// for directives where the value has to be known right away (.org, .alloc ...)

int noForward (const char *what) {
	int f= take_fwd();

	if (f)
		asmError("forward references are not allowed in %s", what);

	return f;
}

// Predefined symbols have a value from before the first line is read, so they
// are never forward references and can be used by .if and .ifdef.
//   TARGET                      the output target chosen so far (TARGET_NONE ... )
//   TARGET_NONE/_C64/_APPLEII/_ATARI800   the values TARGET can have
// -D name[=value] on the command line makes more of them.

void defineSymbol (const char *name, int value, int current_target) {
	char lname[MAXNAME];
	SYM *e;

	lowerName(lname, name);
	e= findSym(lname);

	if (!e)
		e= addSym(lname);

	e->value= value;
	e->type= SYM_CONST;
	e->predef= current_target ? 2 : 1;
}

void initPredefined (void) {
	defineSymbol("TARGET", 0, 1);
	defineSymbol("TARGET_NONE", TARGET_NONE, 0);
	defineSymbol("TARGET_C64", TARGET_C64, 0);
	defineSymbol("TARGET_APPLEII", TARGET_APPLEII, 0);
	defineSymbol("TARGET_ATARI800", TARGET_ATARI800, 0);
	defineSymbol("TARGET_PRODOS", TARGET_PRODOS, 0);
}

// is the symbol defined at this point of the source? Only what comes before
// counts, in both passes, so .ifdef gives the same answer in each of them

int symDefined (const char *s) {
	char lname[MAXNAME];
	SYM *e;

	lowerName(lname, s);
	e= findSym(lname);

	return e && (e->predef || e->defpass == _pass);
}

// value of a symbol

int getVar (char *s) {

	char lname[MAXNAME];
	SYM *e;

	lowerName(lname, s);
	e= findSym(lname);

	if (e && e->predef)
		return e->predef == 2 ? _target : e->value;

	if (e && e->defpass == _pass)
		return e->value;

	// not defined yet in this pass, so it is a forward reference

	_fwd= 1;

	if (_pass == 0) {
		if (!e)
			addSym(lname);
		return 0;
	}

	if (!e || e->defpass < 0) {
		asmError("undefined symbol '%s'", s);
		return 0;
	}

	return e->value;
}

// define a symbol

int assignVar (char *s, int vartype, int val) {

	char lname[MAXNAME];
	SYM *e;

	lowerName(lname, s);
	e= findSym(lname);

	if (e && e->predef) {
		asmError("'%s' is a predefined symbol and can not be defined again", s);
		return -1;
	}

	if (_pass == 0) {
		if (e && e->defpass == 0) {
			if (e->file && _filename && strcmp(e->file, _filename))
				asmError("symbol '%s' is already defined (%s line %d)", s, e->file, e->line);
			else
				asmError("symbol '%s' is already defined (line %d)", s, e->line);
			return -1;
		}

		if (!e)
			e= addSym(lname);

		e->value= val;
		e->type= vartype;
		e->defpass= 0;
		e->line= _lineno;
		e->file= _filename;
		return val;
	}

	// pass 1: the value has to be what pass 0 found

	if (!e || e->defpass != 0) {
		asmError("internal error: symbol '%s' was not defined in pass 0", s);
		return -1;
	}

	if (e->value != val)
		asmError("phase error: '%s' was $%04x in the first pass but is $%04x now", s, e->value, val);

	e->defpass= 1;
	return val;
}

// _sizeof(name). Needs the union/group types, which are not implemented yet.

int getSizeof (char *s) {
	asmError("_sizeof(%s): types are not supported yet", s);
	return 0;
}

static int compareSyms (const void *a, const void *b) {
	const SYM *x= *(const SYM * const *) a;
	const SYM *y= *(const SYM * const *) b;

	if (x->value != y->value)
		return (x->value < y->value) ? -1 : 1;

	return strcmp(x->name, y->name);
}

// print the symbol table, sorted by value

void dumpSymbols (FILE *out) {
	static const char *typeNames[]= {"?", "label", "map", "const"};
	SYM **list;
	SYM *e;
	int count= 0;
	int i;

	for (i= 0; i < HASHSIZE; i++)
		for (e= _table[i]; e; e= e->next)
			if (e->defpass >= 0)
				count++;

	if (!count)
		return;

	list= (SYM **) malloc(count * sizeof(SYM *));
	count= 0;

	for (i= 0; i < HASHSIZE; i++)
		for (e= _table[i]; e; e= e->next)
			if (e->defpass >= 0)
				list[count++]= e;

	qsort(list, count, sizeof(SYM *), compareSyms);

	fprintf(out, "Symbols:\n");

	for (i= 0; i < count; i++)
		fprintf(out, "  %-24s $%04x  %s\n", list[i]->name, list[i]->value, typeNames[list[i]->type & 3]);

	free(list);
}
