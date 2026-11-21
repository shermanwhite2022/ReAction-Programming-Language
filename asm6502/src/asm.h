/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#ifndef ASM_H
#define ASM_H

#include <stdio.h>
#include "asmtypes.h"

// assembler state (defined in main.c)

extern int _debug;		// debug mode
extern int _lineno;		// current line number
extern int _pc;			// program counter
extern int _smode;		// string mode: 0= zero terminated, 1= ucsd (length byte first), 2= unquoted literal
extern int _memsize;		// maximum memory size
extern int _pass;		// assembly pass: 0= collect symbols, 1= generate code
extern int _errors;		// number of errors reported so far
extern int _minaddr;		// lowest address written in this pass (-1 style "none" is _maxaddr < 0)
extern int _maxaddr;		// highest address written in this pass
extern int _target;		// output target (TARGET_...)
extern int _pcSet;		// .org was used (or the program counter was set), so the default origin is not applied
extern int _targetLocked;	// the target was given on the command line, so .target is ignored
extern int _entry;		// run address from .run, -1 if there is none
extern char _title[256];	// title of assembly program
extern char *_filename;		// name of the source file (for messages)

extern BUFFER _membuf;
extern BUFFER _written;		// 1 for every address that was written to

// asm.c: encoding and emitting

extern void asmError (const char *, ...);
extern int getOpcode (char *, int);
extern int genAssembly (char *, int, int, OP_OUT *);
extern char *genAssemblyOut (OP_OUT *);
extern int isBranch (char *);
extern int isAccumulator (char *);
extern int emitAssemblyCode (int, char *, int, int, unsigned char *);
extern int asmOp (char *, int, int, int);
extern int clearMemory (int, int, unsigned char *);
extern int putData (int, int, int, unsigned char *);
extern int putString (int, int, int, STRV *, unsigned char *);
extern STRV *getString (int, char *);
extern int charLiteral (char *);
extern void setOrigin (int);
extern void setMemSize (int);
extern void setTarget (char *);
extern void setEntry (int);

// output.c: writing the output file for a target

extern int targetFromName (const char *);
extern const char *targetName (int);
extern const char *targetExtension (int);
extern int defaultOrigin (int);
extern int writeOutput (const char *, int, int, const char *, const char *);

// disk images (image.c)
#define IMG_NONE	0
#define IMG_D64		1
#define IMG_ATR		2
#define IMG_DOS33	3
#define IMG_PRODOS	4

extern int imageKind (const char *);
extern int imageTarget (int);
extern const char *imageDescription (int);
extern unsigned char *buildImage (int, const char *, const char *, const unsigned char *, int, int, int *);

// symtab.c: symbols and forward reference tracking

extern int getVar (char *);
extern int assignVar (char *, int, int);
extern int getSizeof (char *);
extern int take_fwd (void);
extern void clear_fwd (void);
extern int noForward (const char *);
extern int shiftLeft (int, int);
extern int shiftRight (int, int);
extern void dumpSymbols (FILE *);
extern void defineSymbol (const char *, int, int);
extern void initPredefined (void);
extern int symDefined (const char *);

// list.c: the listing file (-l)

extern void listEnable (void);
extern void listBytes (int, int);
extern void listEnter (const char *, int);
extern void listExit (void);
extern int writeListing (const char *, const char *, int);

// lexer (6502parse.l)

extern void lexReset (void);
extern int addIncludeDir (const char *);	// -I dir, returns 1 if there are too many
extern void printIncludeChain (FILE *);		// "included from" lines for an error message
extern void condIf (int);		// .if: the value of the condition
extern void condElif (int);		// .elif: the value of the condition
extern const char *macroContext (void);

#endif
