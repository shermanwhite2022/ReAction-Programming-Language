/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "emit.h"

#define MAX_STRING		256
#define MAX_STRING_REFS 	1048576
#define MAX_ADDRESS_SPACE 	1048576

int _debug 	= 0;
int _output = 1;
int _gindex = 0;
int _lineno = 1;

char _out[256];
char *_garbage[MAX_STRING_REFS];
	
FILE *outf;

extern FILE *yyin;
extern int yyparse();
extern void yy_flush_buffer();
extern void lex_set_base(const char *);
extern void lex_add_path(const char *);
extern void prune_add_dir(const char *);
extern int prune_output(FILE *, FILE *, int);
int _prune = 1;
int _stack_top = -1;
int _opt = 1;			/* the peephole pass and the short FOR loops (rc -O0 turns them off) */
extern void peephole(FILE *in, FILE *out);
int _zero_block = 1;		/* rc -Z clears it: zero filled variables are stored in the program file, as before */

int collect_garbage() {
	int i;

	for (i=0; i<_gindex; i++)
		if (_garbage[i] != NULL)
			free(_garbage[i]);
}

static void derive_output(const char *in, char *out, size_t n) {
	// prog.act -> prog.a ; name without extension -> name.a
	const char *slash = strrchr(in, '/');
	const char *base = slash ? slash + 1 : in;
	const char *dot = strrchr(base, '.');
	size_t len = dot && dot != base ? (size_t)(dot - in) : strlen(in);
	if (len > n - 3)
		len = n - 3;
	memcpy(out, in, len);
	strcpy(out + len, ".a");
}

int main(int argc, char *argv[]) {
	const char *inname = NULL, *outname = NULL;
	int i, verbose = 0;
	FILE *finalf;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-o") && i + 1 < argc)
			outname = argv[++i];
		else if (!strcmp(argv[i], "-d"))
			_debug = 1;
		else if (!strcmp(argv[i], "-I") && i + 1 < argc) {
			lex_add_path(argv[i + 1]);
			prune_add_dir(argv[i + 1]);
			i++;
		} else if (!strcmp(argv[i], "-S") && i + 1 < argc) {
			const char *a = argv[++i];
			char *end;
			long v = strtol(a + (a[0] == '$'), &end, a[0] == '$' ? 16 : 0);
			if (*end || v < 0x200 || v > 0xffff) {
				fprintf(stderr, "%s: -S needs an address from $0200 to $ffff, not %s\n", argv[0], a);
				return 1;
			}
			_stack_top = (int)v;
		} else if (!strcmp(argv[i], "-k"))
			_prune = 0;
		else if (!strcmp(argv[i], "-Z"))
			_zero_block = 0;
		else if (!strcmp(argv[i], "-O0"))
			_opt = 0;
		else if (!strcmp(argv[i], "-v"))
			verbose = 1;
		else if (argv[i][0] == '-' && argv[i][1]) {
			fprintf(stderr, "%s: unknown option %s\n", argv[0], argv[i]);
			return 1;
		} else if (!inname)
			inname = argv[i];
		else {
			fprintf(stderr, "%s: more than one input file\n", argv[0]);
			return 1;
		}
	}

	{	// the library files (IO.ACT ...) live in includes/ next to the compiler
		char dir[600];
		const char *sl = strrchr(argv[0], '/');
		if (sl) {
			snprintf(dir, sizeof dir, "%.*s/includes", (int)(sl - argv[0]), argv[0]);
			lex_add_path(dir);
			prune_add_dir(dir);
			snprintf(dir, sizeof dir, "%.*s", (int)(sl - argv[0]), argv[0]);
			prune_add_dir(dir);
		}
		lex_add_path("includes");
		prune_add_dir("includes");
		prune_add_dir(".");
	}

	if (!inname) {
		fprintf(stderr, "usage: %s [-d] [-k] [-v] [-Z] [-O0] [-S stack] [-I dir] [-o output.a] input.act\n", argv[0]);
		return 1;
	}

	FILE *infile = fopen(inname, "r");
	if (!infile) {
		fprintf(stderr, "main: ERR! Unable to open file %s!\n", inname);
		return 1;
	}

	// output file: -o name, else input name with .a extension

	if (outname)
		snprintf(_out, sizeof _out, "%s", outname);
	else
		derive_output(inname, _out, sizeof _out);

	finalf = fopen(_out, "w");
	if (!finalf) {
		fprintf(stderr, "main: ERR! Unable to open file %s for writing!\n", _out);
		return 1;
	}
	outf = tmpfile();		// the program is written here, then copied to the output file (minus the dead code)

	// initiallize emitter

	init_emit();

	// write out header for assembly

	emit_header(_out, 0x2000);

	// parse assembly input file

	if (_debug)
		printf("DBG: running compiler...\n");

	lex_set_base(inname);
	yyin = infile;
	yyparse();
	fclose(infile);

	// end of program, variables, runtime library

	emit_footer();

	// copy to the output file: with the routines and data nothing uses removed (-k keeps all)

	rewind(outf);
	if (_opt) {
		FILE *pf = tmpfile();
		peephole(outf, pf);
		fclose(outf);
		outf = pf;
		rewind(outf);
	}
	if (_prune)
		prune_output(outf, finalf, verbose);
	else {
		int c;
		while ((c = fgetc(outf)) != EOF) fputc(c, finalf);
	}
	fclose(outf);
	fclose(finalf);

	// clean up

	collect_garbage(); 
}
