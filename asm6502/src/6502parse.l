/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
%{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <ctype.h>
	#include <sys/stat.h>

	#include "asm.h"
	#include "utils.h"
	#include "6502parse.tab.h"

	// yylex() is a small wrapper (end of this file) around the scanner that
	// flex generates, which is called rawlex() here. It remembers the last
	// token, which is needed to tell apart two pairs of things that look the
	// same: "(" after an instruction opens an indirect operand, anywhere else it
	// groups an expression; and "&17" is an octal number where a value is
	// expected, but an AND with 17 after a value (and the same for "%101", a
	// binary number or a modulo).

	#define YY_DECL static int rawlex (void)

	static int _prevTok = 0;
	static int _prev2Tok = 0;

	// does the token just before end a value? (The name in ".const name" does not:
	// what follows it is the value, so ".const flags %101" is a binary number.)

	static int endsValue (int tok) {
		if (tok == VARIABLE && _prev2Tok == DIR_CONST)
			return 0;

		return tok == INT || tok == VARIABLE || tok == CHARLIT || tok == ')' || tok == ACC;
	}

	// The line number is bumped when the next token is requested, not when the
	// newline is read. The parser looks one token ahead, so it has often read the
	// newline before it runs the action for the statement, and errors would
	// otherwise be reported one line too late. Lines inside a macro expansion
	// are not counted: they all belong to the line that called the macro.

	static int _nl_pending = 0;

	// the parser wants every statement to end with a newline, so one is supplied
	// at the end of the file if the last line does not have it

	static int _need_nl = 1;

	// macros: the lexer keeps the text of each macro, and a call to one is
	// replaced by the text of the macro with its arguments filled in. The
	// expansion is scanned like a nested input file.

	#define MAX_MACRO_DEPTH		16
	#define MAX_MACRO_ARGS		32
	#define MAX_NAME		64

	static YY_BUFFER_STATE	_saved[MAX_MACRO_DEPTH];	// input to go back to after an expansion
	static const char	*_expName[MAX_MACRO_DEPTH];	// name of the macro being expanded
	static int		_depth = 0;			// number of expansions in progress
	static int		_macro_id = 0;			// number of the latest expansion (for local labels)

	static char		*_defname = NULL;		// macro being defined
	static char		*_defbody = NULL;
	static size_t		_deflen = 0;
	static size_t		_defcap = 0;
	static int		_defbad = 0;

	static void startMacro (char *);
	static void appendBody (const char *, size_t);
	static void endMacro (void);
	static void abandonMacro (void);
	static int isSizeofCall (void);
	static void macroCall (void);
	static void endExpansion (void);

	// conditional assembly. The lexer does all of the work except for working
	// out the value of the condition in ".if expr" and ".elif expr", which the
	// parser does. When a condition is false the lexer throws the lines away
	// (the SKIP state) until the matching .else, .elif or .endif, so the text
	// that is left out is never looked at, and can hold anything.
	//
	// A stack remembers the .if blocks that are open. Blocks can not start
	// in one macro and end in another: each expansion has to be balanced.

	typedef struct {
		int	line;		// line of the .if, for messages
		int	sawElse;	// the .else has been seen
	} COND;

	static COND	*_cond = NULL;
	static int	_condDepth = 0;
	static int	_condCap = 0;
	static int	_condBase[MAX_MACRO_DEPTH];	// _condDepth when each expansion started
	static int	_skipNest = 0;		// .if blocks inside the text being skipped
	static int	_skipToElse = 0;	// 1: .else or .elif ends the skip, 0: only .endif does

	static void startSkip (int toElse);
	static void condBranchEnd (int isElse);
	static void condEndif (void);
	static void condIfdef (int want);
	static void skipNewline (void);
	static void skipElse (void);
	static void skipEndif (void);
	static int skipElif (void);
	static void condEndOfInput (void);

	// .include: the lexer reads the file with its own buffer, as a macro
	// expansion is read from a string. The files that are open (the one
	// being read and the ones that included it) are on a stack, and each
	// entry remembers where to go back to. A macro can not include a file,
	// so the open macro expansions are always above the include stack.
	//
	// A file named in .include is looked for next to the file that includes
	// it, then in the directories given with -I.

	#define MAX_INCLUDE_DEPTH	16
	#define MAX_INCLUDE_DIRS	32

	typedef struct {
		YY_BUFFER_STATE	buf;		// the including file's input
		FILE		*fp;		// this file
		char		*path;		// this file's name, as it was opened
		const char	*prevName;	// _filename of the including file
		int		prevLine;	// the line of the .include in it
		int		condBase;	// _condDepth when the file was opened
	} INC;

	static INC		_inc[MAX_INCLUDE_DEPTH];
	static int		_incDepth = 0;
	static const char	*_incDirs[MAX_INCLUDE_DIRS];
	static int		_nIncDirs = 0;

	static void includeFile (void);
	static void popInclude (void);

	static int number (char *text, int base) {
		int v = atobx(text, base);

		if (v < 0) {
			asmError("bad number '%s'", text);
			v = 0;
		}

		return v;
	}

	// a label at the start of a line, with the optional colon taken off

	static char *labelName (void) {
		char *s = strdup(yytext);
		size_t n = strlen(s);

		if (n && s[n-1] == ':')
			s[n-1] = 0;

		return s;
	}
%}

%option noyywrap nounput noinput case-insensitive

%x MACROBODY
%x SKIP

%%
	if (_nl_pending) {
		_lineno++;
		_nl_pending = 0;
	}

	/* the text of a macro is copied until its .endm line */

<MACROBODY>^[ \t]*\.endm[^\n]*\n?	{ endMacro(); BEGIN(INITIAL); }
<MACROBODY>[^\n]*\n					{ appendBody(yytext, yyleng); _lineno++; }
<MACROBODY>[^\n]+					{ appendBody(yytext, yyleng); appendBody("\n", 1); }
<MACROBODY><<EOF>>					{
										asmError("missing .endm for macro '%s'", _defname ? _defname : "?");
										abandonMacro();
										BEGIN(INITIAL);
									}

	/* text of a false condition is thrown away, apart from the .if, .else,
	   .elif and .endif lines that say where it ends */

<SKIP>^[ \t]+\.if[^\n]*\n?			{ _skipNest++; skipNewline(); }
<SKIP>^[ \t]+\.else[^\n]*\n?			{ skipElse(); }
<SKIP>^[ \t]+\.elif[^\n]*\n?			{
										if (skipElif()) {
											int n = 0;

											while (yytext[n] == ' ' || yytext[n] == '\t')
												n++;

											yyless(n + 5);		// give back all but ".elif"
											BEGIN(INITIAL);
											return DIR_ELIF;
										}
										skipNewline();
									}
<SKIP>^[ \t]+\.endif[^\n]*\n?			{ skipEndif(); }
<SKIP>[^\n]+						;
<SKIP>\n							{ skipNewline(); }

[ \t\r]+							;
\n									{ _nl_pending = (_depth == 0); return '\n'; }
;.*									;
"//".*								;

0x[0-9a-f]+							{ yylval.ival = number(yytext, 16); return INT; }
\$[0-9a-f]+							{ yylval.ival = number(yytext, 16); return INT; }
&[0-7]+								{
										if (endsValue(_prevTok)) {
											yyless(1);			// a & b: the AND operator, then the number
											return '&';
										}

										yylval.ival = number(yytext, 8);
										return INT;
									}
"%"[01]+							{
										if (endsValue(_prevTok)) {
											yyless(1);			// a % b: modulo, then the number
											return '%';
										}

										yylval.ival = number(yytext, 2);
										return INT;
									}
[0-9]+\.[0-9]+						{ yylval.fval = (float) atof(yytext); return FLOAT; }
[0-9]+								{ yylval.ival = atoi(yytext); return INT; }

	/* a name in the first column is a label (a colon after it is optional) */

^[a-z][a-z0-9_]*:?					{ yylval.sval = labelName(); return LABEL; }

adc|and|asl|bcc|bcs|beq|bit|bmi|bne|bpl|brk|bvc|bvs|clc|cld|cli|clv|cmp|cpx|cpy|dec|dex|dey|eor|inc|inx|iny|jmp|jsr|lda|ldx|ldy|lsr|nop|ora|pha|php|pla|plp|rol|ror|rti|rts|sbc|sec|sed|sei|sta|stx|sty|tax|tay|tsx|txa|txs|tya	{ yylval.sval = strlower(strdup(yytext)); return OPCODE; }

a									{ return ACC; }
x									{ return XX; }
y									{ return YY; }

[a-z][a-z0-9_]*						{ yylval.sval = strdup(yytext); return VARIABLE; }

'([^'\\\r\n]|\\[^\r\n]|\\x[0-9a-f][0-9a-f])'		{ yylval.sval = strdup(yytext); return CHARLIT; }
'([^'\\\r\n]|\\[^\r\n])*'				{ yylval.sval = strdup(yytext); return SQSTRING; }
\"([^"\\\r\n]|\\[^\r\n])*\"				{ yylval.sval = strdup(yytext); return DQSTRING; }

\.macro[^\n]*\n?					{ startMacro(yytext); BEGIN(MACROBODY); }
\.endm								{ asmError(".endm without a matching .macro"); }

\.ifdef[ \t]+[a-z][a-z0-9_]*			{ condIfdef(1); }
\.ifndef[ \t]+[a-z][a-z0-9_]*		{ condIfdef(0); }
\.ifdef|\.ifndef					{ asmError("%s needs a symbol name", yytext); condIf(0); }
\.if									{ return DIR_IF; }
\.else[^\n]*							{ condBranchEnd(1); }
\.elif[^\n]*							{ condBranchEnd(0); }
\.endif								{ condEndif(); }

\.alloc								{ return DIR_ALLOC; }
\.byte								{ return DIR_BYTE; }
\.bzero								{ return DIR_BZERO; }
\.const								{ return DIR_CONST; }
\.custom							{ return DIR_CUSTOM; }
\.data								{ return DIR_DATA; }
\.end								{ return DIR_END; }
\.endg								{ return DIR_ENDG; }
\.endu								{ return DIR_ENDU; }
\.float								{ return DIR_FLOAT; }
\.group								{ return DIR_GROUP; }
\.include[^\n]*						{ includeFile(); }
\.int								{ return DIR_INT; }
\.map								{ return DIR_MAP; }
\.mem								{ return DIR_MEMSIZE; }
\.org|\.origin|\.start				{ return DIR_ORIGIN; }
\.proc								{ return DIR_PROC; }
\.run|\.entry						{ return DIR_RUN; }
\.smode|\.strmode					{ return DIR_SMODE; }
\.string							{ return DIR_STRING; }
\.target							{ return DIR_TARGET; }
\.title								{ return DIR_TITLE; }
\.union								{ return DIR_UNION; }
\.word								{ return DIR_WORD; }
\.wzero								{ return DIR_WZERO; }

	/* _name args... calls a macro. The whole rest of the line is taken as the
	   arguments, except for _sizeof( which is given back */

_[a-z][a-z0-9_]*[^\n]*				{
										if (isSizeofCall()) {
											yyless(7);
											return FN_SIZEOF;
										}
										macroCall();
									}

"#"									{ return POUND; }
"<<"								{ return SHL; }
">>"								{ return SHR; }
"=="								{ return EQ; }
"!="								{ return NE; }
"<="								{ return LE; }
">="								{ return GE; }
"&&"								{ return ANDAND; }
"||"								{ return OROR; }

<<EOF>>								{
										condEndOfInput();
										BEGIN(INITIAL);

										if (_depth > 0) {
											endExpansion();
										} else if (_need_nl) {
											_need_nl = 0;
											return '\n';
										} else {
											_need_nl = 1;

											if (_incDepth > 0)
												popInclude();
											else
												return 0;
										}
									}

.									{ return yytext[0]; }

%%

// ---------------------------------------------------------------- macros

typedef struct macro {
	char		*name;
	char		*body;		// the lines between .macro and .endm, each ending in a newline
	char		**params;	// names of the parameters (".macro name a, b")
	int		nparams;
	int		defpass;	// the last pass this macro was defined in (for .ifdef)
	struct macro	*next;
} MACRO;

static MACRO *_macros = NULL;

typedef struct {
	char	*s;
	size_t	n;
	size_t	cap;
} STR;

static void strAdd (STR *b, const char *s, size_t n) {
	if (b->n + n + 1 > b->cap) {
		b->cap = (b->n + n + 1) * 2;
		b->s = (char *) realloc(b->s, b->cap);
	}

	memcpy(b->s + b->n, s, n);
	b->n += n;
	b->s[b->n] = 0;
}

// is there a macro of this name that was defined above this line? (Only what
// is above counts, in both passes, as for symbols.)

static MACRO *findMacro (const char *name);

static int macroDefined (const char *name) {
	char lname[MAX_NAME];
	MACRO *m;
	int i;

	for (i = 0; name[i] && i < MAX_NAME - 1; i++)
		lname[i] = (char) tolower((unsigned char) name[i]);

	lname[i] = 0;
	m = findMacro(lname);

	return m && m->defpass == _pass;
}

static MACRO *findMacro (const char *name) {
	MACRO *m;

	for (m = _macros; m; m = m->next)
		if (!strcmp(m->name, name))
			return m;

	return NULL;
}

// the line that starts a macro: ".macro name"

// the names after the macro name: ".macro name first, second ; comment"

static char	*_defparams[MAX_MACRO_ARGS];
static int	_defnp = 0;

static void freeDefParams (void) {
	while (_defnp > 0)
		free(_defparams[--_defnp]);
}

static void defParams (const char *p) {
	char pname[MAX_NAME];
	int len;
	int i;
	int afterComma = 0;

	freeDefParams();

	for (;;) {
		while (*p == ' ' || *p == '\t' || *p == '\r')
			p++;

		if (!*p || *p == '\n' || *p == ';' || (p[0] == '/' && p[1] == '/')) {
			if (afterComma) {
				asmError(".macro: a parameter name is missing after the last comma");
				_defbad = 1;
			}

			return;
		}

		if (!isalpha((unsigned char) *p)) {
			asmError(".macro: the parameters have to be names, separated by commas");
			_defbad = 1;
			return;
		}

		len = 0;

		while ((isalnum((unsigned char) *p) || *p == '_') && len < MAX_NAME - 1)
			pname[len++] = (char) tolower((unsigned char) *p++);

		pname[len] = 0;

		for (i = 0; i < _defnp; i++)
			if (!strcmp(_defparams[i], pname)) {
				asmError(".macro: the parameter '%s' is used twice", pname);
				_defbad = 1;
			}

		if (_defnp >= MAX_MACRO_ARGS) {
			asmError(".macro: too many parameters (the maximum is %d)", MAX_MACRO_ARGS);
			_defbad = 1;
			return;
		}

		_defparams[_defnp++] = strdup(pname);
		afterComma = 0;

		while (*p == ' ' || *p == '\t' || *p == '\r')
			p++;

		if (*p == ',') {
			p++;
			afterComma = 1;
			continue;
		}

		if (*p && *p != '\n' && *p != ';' && !(p[0] == '/' && p[1] == '/')) {
			asmError(".macro: the parameters have to be names, separated by commas");
			_defbad = 1;
		}

		return;
	}
}

static void startMacro (char *text) {
	char *p = text + 6;		// skip ".macro"
	size_t n = strlen(text);
	char name[MAX_NAME];
	int len = 0;

	while (*p == ' ' || *p == '\t')
		p++;

	while ((isalnum((unsigned char) *p) || *p == '_') && len < MAX_NAME - 1)
		name[len++] = (char) tolower((unsigned char) *p++);

	name[len] = 0;

	_defbad = 0;

	if (!len || !isalpha((unsigned char) name[0])) {
		asmError(".macro needs a name");
		_defbad = 1;
		strcpy(name, "?");
	}

	free(_defname);
	_defname = strdup(name);
	_deflen = 0;

	defParams(p);

	if (n && text[n-1] == '\n')
		_lineno++;
}

static void appendBody (const char *s, size_t n) {
	if (_deflen + n + 1 > _defcap) {
		_defcap = (_deflen + n + 1) * 2;
		_defbody = (char *) realloc(_defbody, _defcap);
	}

	memcpy(_defbody + _deflen, s, n);
	_deflen += n;
	_defbody[_deflen] = 0;
}

static void abandonMacro (void) {
	free(_defname);
	_defname = NULL;
	_deflen = 0;
	freeDefParams();
}

// the .endm line: keep the macro

static void endMacro (void) {
	MACRO *m;

	if (yyleng && yytext[yyleng-1] == '\n')
		_lineno++;

	if (_defbad) {
		abandonMacro();
		return;
	}

	if (!_defbody)
		appendBody("", 0);

	m = findMacro(_defname);

	if (m) {
		// the second pass reads the same definitions again
		if (_pass == 0)
			asmError("macro '%s' is already defined", _defname);
		free(m->body);

		while (m->nparams > 0)
			free(m->params[--m->nparams]);

		free(m->params);
	} else {
		m = (MACRO *) calloc(1, sizeof(MACRO));
		m->name = strdup(_defname);
		m->next = _macros;
		_macros = m;
	}

	m->body = strdup(_defbody);
	m->defpass = _pass;
	m->params = (char **) calloc(_defnp + 1, sizeof(char *));
	m->nparams = _defnp;

	while (_defnp > 0) {
		m->params[_defnp - 1] = _defparams[_defnp - 1];
		_defnp--;
	}

	abandonMacro();
}

static int isSizeofCall (void) {
	char *p = yytext + 1;

	if (strncasecmp(p, "sizeof", 6))
		return 0;

	return !(isalnum((unsigned char) p[6]) || p[6] == '_');
}

static int addArg (char **args, int *count, const char *s, size_t n) {
	while (n && isspace((unsigned char) *s)) {
		s++;
		n--;
	}

	while (n && isspace((unsigned char) s[n-1]))
		n--;

	if (*count >= MAX_MACRO_ARGS) {
		asmError("too many macro arguments (the maximum is %d)", MAX_MACRO_ARGS);
		return -1;
	}

	args[*count] = (char *) malloc(n + 1);
	memcpy(args[*count], s, n);
	args[*count][n] = 0;
	(*count)++;

	return 0;
}

// split the text after a macro name into arguments. Arguments are separated
// by commas, except for commas inside parentheses or quotes. A comment ends
// the arguments. Returns the number of arguments, or -1 for too many.

static int splitArgs (const char *s, char **args) {
	int count = 0;
	int depth = 0;
	int commas = 0;
	char quote = 0;
	const char *start = s;
	const char *p;

	for (p = s; *p; p++) {
		if (quote) {
			if (*p == '\\' && p[1])
				p++;			// an escape: the next character does not end the string
			else if (*p == quote)
				quote = 0;
		} else if (*p == '"' || *p == '\'') {
			quote = *p;
		} else if (*p == ';' || (p[0] == '/' && p[1] == '/')) {
			break;
		} else if (*p == '(') {
			depth++;
		} else if (*p == ')') {
			if (depth > 0)
				depth--;
		} else if (*p == ',' && depth == 0) {
			if (addArg(args, &count, start, p - start))
				return -1;
			start = p + 1;
			commas++;
		}
	}

	if (addArg(args, &count, start, p - start))
		return -1;

	// nothing after the name is no arguments, not one empty argument

	if (count == 1 && !commas && !args[0][0]) {
		free(args[0]);
		count = 0;
	}

	return count;
}

// fill in the macro text:
//   @1 @2 ...   the argument (the text is used as it is written)
//   @0          the number of arguments
//   @name       a label that is local to this expansion (it becomes name__mN)
// Comments and strings in the macro are left alone.

static char *expandMacro (MACRO *m, char **args, int nargs) {
	STR out = {NULL, 0, 0};
	const char *p = m->body;
	int id = ++_macro_id;
	char quote = 0;
	char tmp[32];

	while (*p) {
		if (*p == '\n') {
			quote = 0;
			strAdd(&out, p++, 1);
		} else if (quote) {
			if (*p == '\\' && p[1] && p[1] != '\n') {
				strAdd(&out, p, 2);		// an escape: the next character does not end the string
				p += 2;
				continue;
			}

			if (*p == quote)
				quote = 0;
			strAdd(&out, p++, 1);
		} else if (*p == '"' || *p == '\'') {
			quote = *p;
			strAdd(&out, p++, 1);
		} else if (*p == ';' || (p[0] == '/' && p[1] == '/')) {
			while (*p && *p != '\n')
				strAdd(&out, p++, 1);
		} else if (*p == '@' && isdigit((unsigned char) p[1])) {
			int n = 0;

			p++;
			while (isdigit((unsigned char) *p) && n < 1000)
				n = n * 10 + (*p++ - '0');

			if (n == 0) {
				sprintf(tmp, "%d", nargs);
				strAdd(&out, tmp, strlen(tmp));
			} else if (n > nargs) {
				asmError("macro '%s' uses @%d but was called with %d argument%s",
						m->name, n, nargs, nargs == 1 ? "" : "s");
			} else {
				strAdd(&out, args[n-1], strlen(args[n-1]));
			}
		} else if (*p == '@' && isalpha((unsigned char) p[1])) {
			char word[MAX_NAME];
			char lword[MAX_NAME];
			int wl = 0;
			int k;

			p++;

			while ((isalnum((unsigned char) *p) || *p == '_') && wl < MAX_NAME - 1) {
				lword[wl] = (char) tolower((unsigned char) *p);
				word[wl++] = *p++;
			}

			word[wl] = 0;
			lword[wl] = 0;

			for (k = 0; k < m->nparams && strcmp(lword, m->params[k]); k++)
				;

			if (k < m->nparams) {
				// @name is a parameter: the text of the argument
				if (k < nargs)
					strAdd(&out, args[k], strlen(args[k]));
				else
					asmError("macro '%s' uses @%s but was called with %d argument%s",
							m->name, word, nargs, nargs == 1 ? "" : "s");
			} else {
				// anything else is a label local to this expansion
				strAdd(&out, word, strlen(word));
				sprintf(tmp, "__m%d", id);
				strAdd(&out, tmp, strlen(tmp));
			}
		} else {
			strAdd(&out, p++, 1);
		}
	}

	if (!out.s)
		out.s = strdup("");

	return out.s;
}

// "_name args": replace the call with the text of the macro

static void macroCall (void) {
	char *p = yytext + 1;
	char name[MAX_NAME];
	char *args[MAX_MACRO_ARGS];
	int len = 0;
	int nargs;
	int i;
	MACRO *m;
	char *text;

	while ((isalnum((unsigned char) *p) || *p == '_') && len < MAX_NAME - 1)
		name[len++] = (char) tolower((unsigned char) *p++);

	name[len] = 0;

	m = findMacro(name);

	if (!m) {
		asmError("undefined macro '%s'", name);
		return;
	}

	if (_depth >= MAX_MACRO_DEPTH) {
		asmError("macros are nested too deeply (does '%s' call itself?)", name);
		return;
	}

	nargs = splitArgs(p, args);

	if (nargs < 0)
		return;

	text = expandMacro(m, args, nargs);

	for (i = 0; i < nargs; i++)
		free(args[i]);

	_saved[_depth] = YY_CURRENT_BUFFER;
	_expName[_depth] = m->name;
	_condBase[_depth] = _condDepth;
	_depth++;

	yy_scan_string(text);		// scan the expansion next, then carry on with this line
	free(text);
}

// ------------------------------------------------- conditional assembly

// the .if blocks that are open inside the macro being expanded, or in the
// file when no macro is being expanded, are the ones above this

static int condBase (void) {
	if (_depth > 0)
		return _condBase[_depth-1];

	return _incDepth > 0 ? _inc[_incDepth-1].condBase : 0;
}

static void startSkip (int toElse) {
	_skipNest = 0;
	_skipToElse = toElse;
	BEGIN(SKIP);
}

// .if expr (called by the parser when it has the value of the condition)

void condIf (int value) {
	if (_condDepth >= _condCap) {
		_condCap = _condCap ? _condCap * 2 : 16;
		_cond = (COND *) realloc(_cond, _condCap * sizeof(COND));
	}

	_cond[_condDepth].line = _lineno;
	_cond[_condDepth].sawElse = 0;
	_condDepth++;

	if (!value)
		startSkip(1);
}

// .ifdef name, .ifndef name. The symbol counts as defined if it is defined
// above this line (a symbol defined later is not, or the first pass and the
// second pass would disagree).

static void condIfdef (int want) {
	char *p = yytext;
	int defined;

	while (*p != ' ' && *p != '\t')
		p++;

	while (*p == ' ' || *p == '\t')
		p++;

	defined = symDefined(p) || macroDefined(p);
	condIf(defined == want);
}

// .elif expr, reached after every earlier block of the .if was false: the
// parser has the value of the condition

void condElif (int value) {
	if (!value)
		startSkip(1);
}

// .else or .elif in a block that is being assembled: the .if is done, so
// skip to its .endif

static void condBranchEnd (int isElse) {
	COND *c;

	if (_condDepth <= condBase()) {
		asmError(".%s without a matching .if", isElse ? "else" : "elif");
		return;
	}

	c = &_cond[_condDepth-1];

	if (c->sawElse)
		asmError(".%s after .else", isElse ? "else" : "elif");

	if (isElse)
		c->sawElse = 1;

	startSkip(0);
}

static void condEndif (void) {
	if (_condDepth <= condBase())
		asmError(".endif without a matching .if");
	else
		_condDepth--;
}

// while skipping: the line that was just matched is over, count it

static void skipNewline (void) {
	if (yyleng && yytext[yyleng-1] == '\n' && _depth == 0)
		_lineno++;
}

// an .else line while skipping

static void skipElse (void) {
	COND *c = &_cond[_condDepth-1];

	if (_skipNest == 0) {
		if (c->sawElse)
			asmError(".else after .else");

		c->sawElse = 1;

		if (_skipToElse)
			BEGIN(INITIAL);
	}

	skipNewline();
}

// an .elif line while skipping. Returns 1 if this is the .elif whose
// condition has to be worked out now.

static int skipElif (void) {
	if (_skipNest == 0) {
		if (_cond[_condDepth-1].sawElse)
			asmError(".elif after .else");
		else if (_skipToElse)
			return 1;
	}

	return 0;
}

static void skipEndif (void) {
	if (_skipNest > 0) {
		_skipNest--;
	} else {
		_condDepth--;
		BEGIN(INITIAL);
	}

	skipNewline();
}

// the end of the file or of a macro expansion: every .if has to be closed

static void condEndOfInput (void) {
	int base = condBase();

	if (_condDepth > base && _depth > 0) {
		asmError("missing .endif before the end of the macro");
		_condDepth = base;
	}

	while (_condDepth > base) {
		asmError("missing .endif for the .if on line %d", _cond[_condDepth-1].line);
		_condDepth--;
	}
}

// ------------------------------------------------------------- .include

// file names have to outlive the file they name, because symbols remember
// where they were defined

static const char *internName (const char *name) {
	typedef struct nm { char *s; struct nm *next; } NM;
	static NM *names = NULL;
	NM *n;

	for (n = names; n; n = n->next)
		if (!strcmp(n->s, name))
			return n->s;

	n = (NM *) malloc(sizeof(NM));
	n->s = strdup(name);
	n->next = names;
	names = n;

	return n->s;
}

// -I dir

int addIncludeDir (const char *dir) {
	if (_nIncDirs >= MAX_INCLUDE_DIRS)
		return 1;

	_incDirs[_nIncDirs++] = dir;
	return 0;
}

// try to open dir + name. dir is empty or ends in a slash after this

static FILE *tryOpen (const char *dir, size_t dirlen, const char *name, char **path) {
	size_t n = dirlen + strlen(name) + 2;
	char *p = (char *) malloc(n);
	FILE *fp;

	struct stat st;

	snprintf(p, n, "%.*s%s%s", (int) dirlen, dir, (dirlen && dir[dirlen-1] != '/') ? "/" : "", name);
	fp = fopen(p, "r");

	// fopen() opens a directory too, but it can not be read

	if (fp && fstat(fileno(fp), &st) == 0 && S_ISDIR(st.st_mode)) {
		fclose(fp);
		fp = NULL;
	}

	if (fp)
		*path = p;
	else
		free(p);

	return fp;
}

static FILE *openInclude (const char *name, char **path) {
	const char *cur = _incDepth > 0 ? _inc[_incDepth-1].path : _filename;
	const char *slash = cur ? strrchr(cur, '/') : NULL;
	FILE *fp;
	int i;

	if (name[0] == '/')
		return tryOpen("", 0, name, path);

	// next to the file that has the .include

	fp = tryOpen(cur, slash ? (size_t) (slash - cur + 1) : 0, name, path);

	for (i = 0; !fp && i < _nIncDirs; i++)
		fp = tryOpen(_incDirs[i], strlen(_incDirs[i]), name, path);

	return fp;
}

// .include "file". The rest of the line was taken by the lexer, so the
// parser only sees the newline that follows.

static void includeFile (void) {
	char *p = yytext + 8;		// skip ".include"
	char name[1024];
	size_t len = 0;
	char quote;
	char *path;
	FILE *fp;
	INC *f;

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p != '"' && *p != '\'') {
		asmError(".include needs a file name in quotes");
		return;
	}

	quote = *p++;

	while (*p && *p != quote && len < sizeof(name) - 1)
		name[len++] = *p++;

	name[len] = 0;

	if (*p != quote) {
		asmError("the file name in .include is not closed or is too long");
		return;
	}

	p++;

	while (*p == ' ' || *p == '\t' || *p == '\r')
		p++;

	if (*p && *p != ';' && !(p[0] == '/' && p[1] == '/')) {
		asmError("unexpected text after the file name in .include");
		return;
	}

	if (!len) {
		asmError(".include needs a file name");
		return;
	}

	if (_depth > 0) {
		asmError(".include can not be used inside a macro");
		return;
	}

	if (_incDepth >= MAX_INCLUDE_DEPTH) {
		asmError("files are included too deeply (does '%s' include itself?)", name);
		return;
	}

	fp = openInclude(name, &path);

	if (!fp) {
		asmError("can not open include file '%s'", name);
		return;
	}

	f = &_inc[_incDepth++];
	f->buf = YY_CURRENT_BUFFER;
	f->fp = fp;
	f->path = path;
	f->prevName = _filename;
	f->prevLine = _lineno;
	f->condBase = _condDepth;

	_filename = (char *) internName(path);
	_lineno = 1;
	_nl_pending = 0;
	listEnter(_filename, f->prevLine);

	yy_switch_to_buffer(yy_create_buffer(fp, YY_BUF_SIZE));
}

// the end of an included file: go back to the line after the .include

static int _resetting = 0;		// popping includes to start another pass: not part of the listing

static void popInclude (void) {
	INC *f = &_inc[--_incDepth];

	if (!_resetting)
		listExit();

	yy_delete_buffer(YY_CURRENT_BUFFER);
	fclose(f->fp);
	yy_switch_to_buffer(f->buf);

	_filename = (char *) f->prevName;
	_lineno = f->prevLine;
	_nl_pending = 0;
	free(f->path);
}

// the files that included the one being read, for error messages

void printIncludeChain (FILE *out) {
	int i;

	for (i = _incDepth - 1; i >= 0; i--)
		fprintf(out, "  included from %s:%d\n", _inc[i].prevName ? _inc[i].prevName : "<input>", _inc[i].prevLine);
}

// the end of an expansion: go back to the line that called the macro

static void endExpansion (void) {
	yy_delete_buffer(YY_CURRENT_BUFFER);
	_depth--;
	yy_switch_to_buffer(_saved[_depth]);
}

// the macro being expanded, for error messages

const char *macroContext (void) {
	return _depth > 0 ? _expName[_depth-1] : NULL;
}

// get ready for another pass over the source. Macros stay defined.

void lexReset (void) {
	while (_depth > 0)
		endExpansion();

	_resetting = 1;

	while (_incDepth > 0)
		popInclude();

	_resetting = 0;

	_lineno = 1;

	abandonMacro();
	BEGIN(INITIAL);

	_condDepth = 0;
	_skipNest = 0;
	_nl_pending = 0;
	_need_nl = 1;
	_macro_id = 0;
	_prevTok = 0;
	_prev2Tok = 0;
}

// ------------------------------------------------------------ yylex()

int yylex (void) {
	int tok = rawlex();
	int result = tok;

	if (tok == '(' && _prevTok == OPCODE)
		result = IND_OPEN;

	_prev2Tok = _prevTok;
	_prevTok = tok;
	return result;
}
