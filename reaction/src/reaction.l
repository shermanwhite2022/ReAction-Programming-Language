/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
%{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <ctype.h>
	#include <limits.h>

	#include "utils.h"
	#include "y.tab.h"

	int _nest= 0;

	extern int _debug;
	extern int _lineno;
	extern int yylex();
	void yyerror(const char *s);

	/* DEFINE table and compile time directives live in the emitter (emit.c) */
	extern char *define_lookup(char *name);

	#define YY_DECL static int yylex_raw(void)

	static int  defstate= 0;		/* 1: the next identifier is being defined, don't expand it */
	static int  expansions= 0;
	static void do_include(char *text);
	static int  lex_pop(void);

	// BY REF
	// POINTER TO
	// ARRAY OF

%}

%option noyywrap

%%

[\n]											{ _lineno++; }
[;].*$											{ if (_debug) printf("Flex: comment encountered: %s\n", yytext); }

[0-9]+											{ yylval.ival = atoi(yytext); return INT; }
[0-9]+\.[0-9]+([eE][-+]?[0-9]+)?				{ yylval.fval = atof(yytext); return RW_FLOAT; }
\$[0-9a-fA-F]+									{ yylval.ival = atobx(yytext,16); return INT; }
0x[0-9a-fA-F]+									{ yylval.ival = atobx(yytext,16); return INT; }

[Aa][Dd][Dd][Rr][Ee][Ss][Ss] 					{ return RW_ADDRESS; }
[Aa][Nn][Dd]									{ return RW_AND; }
[Aa][Rr][Rr][Aa][Yy]							{ return RW_ARRAY; }
[Bb][Yy][Tt][Ee]								{ return RW_BYTE; }
[Bb][Yy]										{ return RW_BY; }
[Cc][Aa][Rr][Dd]								{ return RW_CARD; }
[Uu][Ii][Nn][Tt]								{ return RW_CARD; }	// UINT is a synonym for CARD
[Cc][Hh][Aa][Rr]								{ return RW_BYTE; }
[Cc][Ll][Aa][Ss][Ss]							{ return RW_CLASS; }
[Cc][Aa][Tt][Cc][Hh]								{ return RW_CATCH; }
[Tt][Rr][Yy]									{ _nest++; return RW_TRY; }
[Yy][Rr][Tt]									{ _nest--; return RW_YRT; }
[Tt][Hh][Rr][Oo][Ww]								{ return RW_THROW; }
[Cc][Aa][Ss][Ee]								{ _nest++; return RW_CASE; }
[Ee][Ss][Aa][Cc]								{ _nest--; return RW_ESAC; }
[Ss][Ii][Nn]								{ return FN_SIN; }
[Cc][Oo][Ss]								{ return FN_COS; }
[Tt][Aa][Nn]								{ return FN_TAN; }
[Aa][Tt][Aa][Nn]								{ return FN_ATAN; }
[Ll][Nn]								{ return FN_LN; }
[Ll][Oo][Gg]2								{ return FN_LOG2; }
[Ll][Oo][Gg]10								{ return FN_LOG10; }
[Cc][Oo][Nn][Ss][Tt]							{ return RW_CONST; }
[Dd][Ee][Ff][Ii][Nn][Ee]						{ return RW_DEFINE; }
[Dd][Oo]										{ _nest++; return RW_DO; }
[Ee][Xx][Tt][Ee][Nn][Dd][Ss]						{ return RW_EXTENDS; }
[Ee][Ll][Ss][Ee]								{ return RW_ELSE; }
[Ee][Ll][Ss][Ee][Ii][Ff]						{ return RW_ELSEIF; }
[Ee][Nn][Uu][Mm]								{ return RW_ENUM; }
[Ee][Xx][Ii][Tt]								{ return RW_EXIT; }
[Ff][Ii]									{ _nest--; return RW_FI; }
[Ff][Ll][Oo][Aa][Tt]								{ return RW_REAL; }
[Ff][Oo][Rr]									{ return RW_FOR; }
[Ff][Uu][Nn][Cc]									{ _nest= 0; return RW_FUNC; }
[Ii][Ff]										{ _nest++; return RW_IF; }
[Ii][Nn][Cc][Ll][Uu][Dd][Ee][ \t]*\"[^"\r\n]*\"		{ do_include(yytext); }
[Ii][Nn][Cc][Ll][Uu][Dd][Ee]					{ return RW_INCLUDE; }
[Ii][Nn][Ll][Ii][Nn][Ee]									{ return RW_INLINE; }
[Ii][Nn][Tt]									{ return RW_INT; }
[Ll][Ss][Hh]									{ return RW_LSH; }
[Mm][Aa][Ll][Ll][Oo][Cc]						{ return RW_MALLOC; }	
[Mm][Oo][Dd]									{ return RW_MOD; }	
[Mm][Oo][Dd][Uu][Ll][Ee]						{ return RW_MODULE; }	
[Nn][Ee][Ww]									{ return RW_NEW; }
[Nn][Ee][Xx][Tt]								{ return RW_NEXT; }	
[Oo][Dd]										{ _nest--; return RW_OD; }	
[Oo][Ff]										{ return RW_OF; }	
[Oo][Rr]										{ return RW_OR; }	
[Pp][Oo][Ii][Nn][Tt][Ee][Rr]					{ return RW_POINTER; }
[Pp][Rr][Oo][Cc]								{ _nest= 0; return RW_PROC; }
[Rr][Ee][Ff]									{ return RW_REF; }
[Rr][Ee][Tt][Uu][Rr][Nn]						{ return ((_nest>0)? RW_IRETURN : RW_RETURN); }
[Rr][Ss][Hh]									{ return RW_RSH; }
[Ss][Ee][Tt]									{ return RW_SET; }
[Ss][Ss][Aa][Ll][Cc]									{ return RW_SSALC; }
[Ss][Tt][Ee][Pp]								{ return RW_STEP; }
[Ss][Tt][Rr][Ii][Nn][Gg]						{ return RW_STRING; }
[Tt][Hh][Ee][Nn]								{ return RW_THEN; }
[Tt][Oo]										{ return RW_TO; }
[Tt][Yy][Pp][Ee]								{ return RW_TYPE; }
[Uu][Nn][Tt][Ii][Ll]							{ return RW_UNTIL; }
[Ww][Hh][Ii][Ll][Ee]							{ return RW_WHILE; }
[Xx][Oo][Rr]									{ return RW_XOR; }
[Zz][Tt][Rr][Ii][Nn][Gg]						{ return RW_ZTRING; }

[Ss][Ii][Zz][Ee][Oo][Ff]|_sizeof				{ return FN_SIZEOF; }
_heapaddr										{ return FN_HEAP; }
_stackaddr										{ return FN_STACK; }

"**"											{ return RW_EXP; }
"+"												{ return RW_PLUS; }
"-"												{ return RW_MINUS; }
"*"												{ return RW_MUL; }
"/"												{ return RW_DIV; }
"&"												{ return RW_AMPERSAND; }
"%"												{ return RW_PERCENT; }
"!"												{ return RW_EXCLAM; }
"="												{ return RW_EQ; }
"=="											{ return RW_EQEQ; }
"<>"											{ return RW_NE; }
"#" 											{ return RW_POUND; }
">"												{ return RW_GT; }
">="											{ return RW_GE; }
"<"												{ return RW_LT; }
"<="											{ return RW_LE; }
"$"												{ return RW_DS; }
"^"												{ return RW_CARAT; }
"@"												{ return RW_AT; }
"("												{ return RW_LPAREN; }
")"												{ return RW_RPAREN; }
"["												{ return RW_LBRACK; }
"]"												{ return RW_RBRACK; }
","												{ return RW_COMMA; }

[[:alpha:]_][[:alnum:]_]*\.[[:alpha:]_][[:alnum:]_]*  	{ yylval.sval = strdup(yytext); return RECORD_VARIABLE; }
[[:alpha:]_][[:alnum:]_]*  						{
		char *d;
		if (defstate != 1 && (d= define_lookup(yytext)) != NULL) {
			int i;
			if (++expansions > 5000) yyerror("DEFINE expands without end (does a DEFINE use its own name?)");
			for (i= strlen(d) - 1; i >= 0; i--) unput(d[i]);
		} else {
			yylval.sval = strdup(yytext);
			return VARIABLE;
		}
	}
\"([^"\r\n]|\"\")*\"							{ yylval.sval = strdup(yytext); return DQ_STRING; }
\'.												{ yylval.ival = yytext[1]; return SQ_CHAR; }
[ \t\r]+
[.]												{ return RW_PERIOD; }
<<EOF>>											{ if (!lex_pop()) yyterminate(); }
.													{ char m[64]; sprintf(m, "unexpected character '%c' (0x%02x)", yytext[0], (unsigned char)yytext[0]); yyerror(m); }

%%

/* ---- INCLUDE: a stack of input files; the lexer switches buffers ------------ */

#define MAXINC 16

typedef struct {
	YY_BUFFER_STATE buf;
	FILE *f;
	int   line;
	char  dir[512];
	char  name[512];
} incctx;

static incctx incstack[MAXINC];
static int    incsp= 0;
static char   curdir[512]= "";
static char   basedir[512]= "";
static char   curname[512]= "";

int lex_strict= 1;			/* 0: a missing INCLUDE file is ignored (parser tests) */

static void dir_of(const char *path, char *dir) {
	const char *sl= strrchr(path, '/');

	if (!sl) { dir[0]= 0; return; }
	strncpy(dir, path, sl - path + 1);
	dir[sl - path + 1]= 0;
}

void lex_set_base(const char *path) {
	dir_of(path, basedir);
	strcpy(curdir, basedir);
	strncpy(curname, path, 511);
}

/* more places to look for INCLUDE files: rc -I dir, and the includes directory next to rc */
static char *included[256];			/* the INCLUDE files read so far (real paths) */
static int   nincluded= 0;
static char incpaths[16][512];
static int  nincpaths= 0;
void lex_add_path(const char *d) {
	size_t n;
	if (nincpaths >= 16 || strlen(d) > 500) return;
	strcpy(incpaths[nincpaths], d);
	n= strlen(d);
	if (n && d[n-1] != '/') strcat(incpaths[nincpaths], "/");
	nincpaths++;
}

static void do_include(char *text) {
	char path[512], cand[1100], *q, *e;
	FILE *f= NULL;
	int k, nt;
	const char *try[3 + 16];

	q= strchr(text, '"') + 1;
	e= strrchr(text, '"');
	strncpy(path, q, e - q); path[e - q]= 0;

	/* Action! names carry a device, D:FILE.ACT or D2:FILE.ACT: drop it */
	k= 0;
	if (isalpha((unsigned char)path[0])) {
		k= 1;
		while (isdigit((unsigned char)path[k])) k++;
		if (path[k] == ':') k++; else k= 0;
	}
	memmove(path, path + k, strlen(path + k) + 1);

	try[0]= curdir; try[1]= basedir; try[2]= "";
	nt= 3;
	for (k= 0; k < nincpaths; k++) try[nt++]= incpaths[k];
	for (k= 0; k < nt && !f; k++) {
		snprintf(cand, sizeof(cand), "%s%s", try[k], path);
		f= fopen(cand, "r");
	}
	if (!f) {
		if (!lex_strict) return;
		sprintf(cand, "cannot open INCLUDE file %s", path);
		yyerror(cand);
	}
	/* a file is read once: a library file can INCLUDE what it needs without a second copy
	 * (declaring everything twice would be an error anyway) */
	{
		char full[1100], real[PATH_MAX];
		int j;
		snprintf(full, sizeof(full), "%s%s", try[k-1], path);
		if (!realpath(full, real)) snprintf(real, sizeof(real), "%s", full);
		for (j= 0; j < nincluded; j++)
			if (!strcmp(included[j], real)) { fclose(f); return; }
		if (nincluded < 256) included[nincluded++]= strdup(real);
	}
	if (incsp >= MAXINC) yyerror("INCLUDE files nested too deeply");

	incstack[incsp].buf= YY_CURRENT_BUFFER;
	incstack[incsp].f= f;
	incstack[incsp].line= _lineno;
	strcpy(incstack[incsp].dir, curdir);
	strcpy(incstack[incsp].name, curname);
	incsp++;
	{
		char full[1100];
		snprintf(full, sizeof(full), "%s%s", try[k-1], path);
		dir_of(full, curdir);
		strncpy(curname, full, 511);
	}
	_lineno= 1;
	yy_switch_to_buffer(yy_create_buffer(f, YY_BUF_SIZE));
}

static int lex_pop(void) {
	if (incsp == 0) return 0;
	incsp--;
	yy_delete_buffer(YY_CURRENT_BUFFER);
	fclose(incstack[incsp].f);
	yy_switch_to_buffer(incstack[incsp].buf);
	_lineno= incstack[incsp].line;
	strcpy(curdir, incstack[incsp].dir);
	strcpy(curname, incstack[incsp].name);
	return 1;
}

/* ---- yylex wrapper: tracks "DEFINE name=value, name=value" so the name being
 * defined is not itself substituted ------------------------------------------ */

int yylex() {
	static int st= 0;
	int t= yylex_raw();

	expansions= 0;
	if (t == RW_DEFINE) st= 1;
	else if (st == 1) st= 2;
	else if (st == 2) st= (t == RW_EQ)? 3: 0;
	else if (st == 3) {
		if (t == DQ_STRING || t == INT || t == SQ_CHAR || t == RW_FLOAT) st= 4;
		else if (t == RW_LBRACK) st= 5;
		else if (t != RW_MINUS) st= 0;
	}
	else if (st == 5) { if (t == RW_RBRACK) st= 4; }
	else if (st == 4) st= (t == RW_COMMA)? 1: 0;
	defstate= (st == 1);
	return t;
}

void yyerror (const char *s) {
    printf ("Error! %s at line %d\n", s, _lineno);
    exit (-1);
}

