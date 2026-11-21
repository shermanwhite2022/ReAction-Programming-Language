/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "asm.h"
#include "asmtypes.h"

int _debug;		// debug mode
int _lineno;		// current line number
int _pc;		// program counter
int _smode;		// string mode 0= zero terminated string, 1= ucsd string 2= unquoted literal
int _memsize;		// maximum memory size
int _pass;		// assembly pass number
int _errors;		// number of errors found
int _target;		// output target
int _pcSet;		// the program counter was set by .org
int _targetLocked;	// target set on the command line
int _entry;		// run address, -1 if none
int _minaddr;		// lowest address written
int _maxaddr;		// highest address written

char  _title[256];	// title of assembly program
char *_filename;	// source file name

BUFFER _membuf;
BUFFER _written;

extern FILE *yyin;
extern int yyparse(void);
extern void yyrestart(FILE *);

// set up the assembly variables at the start of a pass

static void startPass(int pass) {

	_pass = pass;
	_lineno = 1;
	_pcSet = 0;
	_smode = 0;
	_memsize = MAX_BUFFER_SIZE; // 1M
	_minaddr = INT_MAX;
	_maxaddr = -1;

	strcpy (_title, "");

	memset(_membuf, 0, sizeof(BUFFER));
	memset(_written, 0, sizeof(BUFFER));

	_entry = -1;
	if (!_targetLocked)
		_target = TARGET_NONE;

	_pc = defaultOrigin(_target);

	lexReset();
	clear_fwd();
}

// -D name[=value]: the value is a number (decimal, $hex or 0xhex), 1 if left out

static int defineFromOption(const char *def) {
	char name[128];
	const char *eq = strchr(def, '=');
	size_t n = eq ? (size_t) (eq - def) : strlen(def);
	char *end;
	long value = 1;
	size_t k;

	if (!n || n >= sizeof(name) || !isalpha((unsigned char) def[0])) {
		fprintf(stderr, "Bad -D option '%s' (use -D name or -D name=value)\n", def);
		return 1;
	}

	for (k = 0; k < n; k++)
		if (!isalnum((unsigned char) def[k]) && def[k] != '_') {
			fprintf(stderr, "Bad symbol name in -D option '%s'\n", def);
			return 1;
		}

	memcpy(name, def, n);
	name[n] = 0;

	if (eq) {
		if (eq[1] == '$')
			value = strtol(eq + 2, &end, 16);
		else
			value = strtol(eq + 1, &end, 0);

		if (!eq[1] || *end || (eq[1] == '$' && !eq[2])) {
			fprintf(stderr, "Bad value in -D option '%s'\n", def);
			return 1;
		}
	}

	if (symDefined(name)) {
		fprintf(stderr, "'%s' is already defined\n", name);
		return 1;
	}

	defineSymbol(name, (int) value, 0);
	return 0;
}

static void usage(void) {
	fprintf(stderr, "usage: asm6502 [-t target] [-o output] [-D name[=value]] [-I dir] [-l file] [-n name] [-p] [-s] [-d] source.asm\n");
	fprintf(stderr, "  -t target  c64 (.prg), appleii (DOS 3.3 binary), atari800 (.xex) or prodos (.po disk). Can also be set\n");
	fprintf(stderr, "             in the source with .target. With a target the output file is named after\n");
	fprintf(stderr, "             the source file unless -o is given.\n");
	fprintf(stderr, "  -o file    write the output to file (with no target and no -o a hex dump is printed).\n");
	fprintf(stderr, "             A name ending in .d64, .atr, .dsk or .po makes a disk image with the program on it.\n");
	fprintf(stderr, "  -n name    the name of the program on the disk image (default: the name of the source file)\n");
	fprintf(stderr, "  -D name    define a symbol for .if and .ifdef (value 1), or -D name=value\n");
	fprintf(stderr, "  -I dir     where .include looks for files (after the directory of the including file)\n");
	fprintf(stderr, "  -l file    write a listing (source lines with their addresses and bytes, then the symbols)\n");
	fprintf(stderr, "  -p         with no target: start the output file with the 2 byte load address\n");
	fprintf(stderr, "  -s        print the symbol table\n");
	fprintf(stderr, "  -d        debug mode\n");
}

static void hexDump(void) {
	int a;
	int i;

	for (a = _minaddr & ~15; a <= _maxaddr; a += 16) {
		printf("%04x: ", a);

		for (i = 0; i < 16; i++) {
			if (a + i >= _minaddr && a + i <= _maxaddr)
				printf("%02x ", _membuf[a + i]);
			else
				printf("   ");
		}

		printf("\n");
	}
}

int main(int argc, char *argv[]) {

	int i;
	int showSymbols = 0;
	int loadAddress = 0;
	char *outName = NULL;
	char *listName = NULL;
	char *diskFile = NULL;
	char baseName[1024];
	char *inName = NULL;
	char defName[1024];
	char *dot;
	char *slash;
	int size;
	FILE *infile;

	_debug = 0;
	_errors = 0;
	_filename = NULL;

	initPredefined();

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc)
			outName = argv[++i];
		else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
			_target = targetFromName(argv[++i]);

			if (_target < 0) {
				fprintf(stderr, "Unknown target '%s' (use c64, appleii, atari800 or prodos)\n", argv[i]);
				return 2;
			}

			_targetLocked = 1;
		}
		else if (!strncmp(argv[i], "-I", 2)) {
			// -Idir or -I dir

			const char *dir = argv[i] + 2;

			if (!*dir && i + 1 < argc)
				dir = argv[++i];

			if (!*dir || addIncludeDir(dir)) {
				fprintf(stderr, "Bad or too many -I options\n");
				return 2;
			}
		}
		else if (!strncmp(argv[i], "-D", 2)) {
			// -Dname, -Dname=value or -D name[=value]

			const char *def = argv[i] + 2;

			if (!*def && i + 1 < argc)
				def = argv[++i];

			if (defineFromOption(def))
				return 2;
		}
		else if (!strcmp(argv[i], "-n") && i + 1 < argc)
			diskFile = argv[++i];
		else if (!strcmp(argv[i], "-l") && i + 1 < argc) {
			listName = argv[++i];
			listEnable();
		}
		else if (!strcmp(argv[i], "-p"))
			loadAddress = 1;
		else if (!strcmp(argv[i], "-s"))
			showSymbols = 1;
		else if (!strcmp(argv[i], "-d"))
			_debug = 1;
		else if (argv[i][0] == '-' || inName) {
			usage();
			return 2;
		} else
			inName = argv[i];
	}

	if (!inName) {
		usage();
		return 2;
	}

	infile = fopen(inName, "r");

	if (!infile) {
		printf ("Unable to open file %s!\n", inName);
		return -1;
	}

	_filename = inName;
	yyin = infile;

	// pass 0 finds the labels, pass 1 generates the code

	for (i = 0; i < 2; i++) {
		rewind(infile);
		startPass(i);
		yyrestart(infile);
		yyparse();

		if (_errors)
			break;
	}

	fclose(infile);

	if (_errors) {
		fprintf(stderr, "%d error%s, nothing written.\n", _errors, _errors == 1 ? "" : "s");
		return 1;
	}

	if (_title[0])
		printf("Title: %s\n", _title);

	if (_maxaddr < 0) {
		printf("Nothing was assembled.\n");
	} else {
		printf("Assembled %d bytes at $%04x-$%04x\n", _maxaddr - _minaddr + 1, _minaddr, _maxaddr);

		// with a target the file gets a name from the source file

		if (!outName && _target != TARGET_NONE) {
			snprintf(defName, sizeof(defName) - 8, "%s", inName);
			slash = strrchr(defName, '/');
			dot = strrchr(defName, '.');

			if (dot && (!slash || dot > slash))
				*dot = 0;

			strcat(defName, ".");
			strcat(defName, targetExtension(_target));
			outName = defName;
		}

		if (outName) {
			// the name of the source file without the directory and the extension

			slash = strrchr(inName, '/');
			snprintf(baseName, sizeof(baseName), "%s", slash ? slash + 1 : inName);
			dot = strrchr(baseName, '.');

			if (dot)
				*dot = 0;

			size = writeOutput(outName, _target, loadAddress, diskFile, baseName);

			if (size < 0)
				return 1;

			if (imageKind(outName))
				printf("Wrote %s (%s, %d bytes)\n", outName, imageDescription(imageKind(outName)), size);
			else
				printf("Wrote %s (%s, %d bytes)\n", outName, targetName(_target), size);

			if (_target == TARGET_ATARI800 && _entry < 0)
				printf("Note: no .run address, so the program will load but not start.\n");
		} else {
			if (diskFile) {
				fprintf(stderr, "-n names the file on a disk image, so -o has to name one (ending in .d64, .atr, .dsk or .po).\n");
				return 2;
			}

			hexDump();
		}
	}

	if (listName) {
		if (writeListing(listName, inName, 1) < 0) {
			fprintf(stderr, "Unable to write the listing %s\n", listName);
			return 1;
		}

		printf("Wrote listing %s\n", listName);
	}

	if (showSymbols)
		dumpSymbols(stdout);

	return 0;
}
