/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
%{
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>

	#include "asm.h"
	#include "asmtypes.h"

	extern int yylex();
	extern int yyparse();
	extern FILE *yyin;

	void yyerror(const char *s);

	static int _status;
	static int _dsize;		// element size of the .byte/.word list being parsed

	// emit something at the program counter and move the program counter past it

	#define ADVANCE(call)	do { _status = (call); if (_status != -1) _pc += _status; } while (0)

%}

%define parse.error verbose

%union {
	int ival;
	float fval;
	char *sval;
	void *pval;
}

%start program

%token <ival> INT
%token <fval> FLOAT
%token <sval> LABEL
%token <sval> VARIABLE
%token <sval> OPCODE
%token <sval> SQSTRING
%token <sval> DQSTRING
%token <sval> CHARLIT
%token IND_OPEN
%token XX
%token YY
%token ACC

%token DIR_ALLOC
%token DIR_BYTE
%token DIR_BZERO
%token DIR_CONST
%token DIR_CUSTOM
%token DIR_DATA
%token DIR_END
%token DIR_ENDG
%token DIR_ELIF
%token DIR_ENDU
%token DIR_FLOAT
%token DIR_GROUP
%token DIR_IF
%token DIR_INT
%token DIR_MAP
%token DIR_MEMSIZE
%token DIR_ORIGIN
%token DIR_PROC
%token DIR_RUN
%token DIR_SMODE
%token DIR_STRING
%token DIR_TARGET
%token DIR_TITLE
%token DIR_UNION
%token DIR_WORD
%token DIR_WZERO
%token POUND
%token EQ NE LE GE ANDAND OROR SHL SHR
%token FN_SIZEOF

// operators, from the loosest to the tightest

%left OROR
%left ANDAND
%left EQ NE
%left '<' '>' LE GE
%left '|'
%left '^'
%left '&'
%left SHL SHR
%left '+' '-'
%left '*' '/' '%'
%left UMINUS
%left BYTEL
%left BYTEH
%left NOT

%type <ival> expr
%type <pval> string

%%

// grammar to parse. Comments are dropped by the lexer, so a statement is
// just an optional label followed by an instruction or a directive.

program:
	/* empty */
	| program line
	;

line:
	'\n'
	| statement '\n'
	{ clear_fwd(); }
	| error '\n'
	{ yyerrok; clear_fwd(); }
	;

statement:
	label
	| label body
	| body
	;

label:
	LABEL
	{ assignVar($1, SYM_LABEL, _pc); }
	;

body:
	asm
	| directive
	| definition
	;

// instructions. An address that fits in a byte is zero page, anything else is
// absolute. asmOp() makes the final choice, see asm.c.

asm:
	OPCODE POUND expr
	{ ADVANCE(asmOp($1, $3, IMMEDIATE, take_fwd())); }
	|

	OPCODE IND_OPEN expr ')' ',' YY
	{ ADVANCE(asmOp($1, $3, INDIRECT_Y, take_fwd())); }
	|

	OPCODE IND_OPEN expr ',' XX ')'
	{ ADVANCE(asmOp($1, $3, INDIRECTX_, take_fwd())); }
	|

	OPCODE IND_OPEN expr ')'
	{ ADVANCE(asmOp($1, $3, INDIRECT, take_fwd())); }
	|

	OPCODE expr ',' XX
	{ ADVANCE(asmOp($1, $2, ZERO_PAGE_X, take_fwd())); }
	|

	OPCODE expr ',' YY
	{ ADVANCE(asmOp($1, $2, ZERO_PAGE_Y, take_fwd())); }
	|

	OPCODE expr
	{ ADVANCE(asmOp($1, $2, isBranch($1) ? RELATIVE : ZERO_PAGE, take_fwd())); }
	|

	OPCODE ACC
	{ ADVANCE(asmOp($1, 0, ACCUMULATOR, 0)); }
	|

	OPCODE
	{ ADVANCE(asmOp($1, 0, isAccumulator($1) ? ACCUMULATOR : IMPLIED, 0)); }
	;

// directives

directive:
	data_dir operand_list

	| DIR_FLOAT float_list
	{ asmError(".float is not supported yet"); }
	|

	DIR_STRING string
	{ ADVANCE(putString(_pc, _smode, 1, (STRV *) $2, _membuf)); }
	|

	DIR_STRING CHARLIT
	{ ADVANCE(putString(_pc, _smode, 1, getString(1, $2), _membuf)); }
	|

	DIR_ALLOC expr
	{ noForward(".alloc");
	  if ($2 < 0)
		asmError("negative size %d", $2);
	  else
		_pc += $2;
	}
	|

	DIR_BZERO expr
	{ noForward(".bzero");
	  ADVANCE(clearMemory(_pc, $2, _membuf));
	}
	|

	DIR_WZERO expr
	{ noForward(".wzero");
	  ADVANCE(clearMemory(_pc, $2 * 2, _membuf));
	}
	|

	DIR_CONST VARIABLE expr
	{ noForward(".const");
	  assignVar($2, SYM_CONST, $3);
	}
	|

	DIR_MEMSIZE expr
	{ noForward(".mem");
	  setMemSize($2);
	}
	|

	DIR_SMODE INT
	{ if ($2 < 0 || $2 > 2)
		asmError("string mode must be 0, 1 or 2");
	  else
		_smode = $2;
	}
	|

	DIR_ORIGIN expr
	{ noForward(".org");
	  setOrigin($2);
	}
	|

	DIR_MAP VARIABLE '@' expr
	{ noForward(".map");
	  assignVar($2, SYM_MAP, $4);
	}
	|

	DIR_MAP VARIABLE '@' nested_list
	{ asmError(".map with a structure is not supported yet"); }
	|

	DIR_TITLE string
	{ strncpy(_title, ((STRV *) $2)->s, sizeof(_title) - 1);
	  _title[sizeof(_title) - 1] = 0;
	}
	|

	DIR_TITLE CHARLIT
	{ strncpy(_title, getString(1, $2)->s, sizeof(_title) - 1);
	  _title[sizeof(_title) - 1] = 0;
	}
	|

	DIR_TARGET VARIABLE
	{ setTarget($2); }
	|

	DIR_RUN expr
	{ take_fwd();
	  setEntry($2);
	}
	|

	DIR_END
	{ YYACCEPT; }
	|

	DIR_IF expr
	{ noForward(".if");
	  condIf($2);
	}
	|

	DIR_ELIF expr
	{ noForward(".elif");
	  condElif($2);
	}
	|

	dir_custom custom_list
	{ asmError("custom directives are not supported yet"); }
	;

data_dir:
	DIR_BYTE	{ _dsize = 1; }
	| DIR_DATA	{ _dsize = 1; }
	| DIR_WORD	{ _dsize = 2; }
	| DIR_INT	{ _dsize = 2; }
	;

// .byte, .data, .word and .int take numbers and strings. Strings are padded
// to the element size.

operand_list:
	operand
	| operand_list ',' operand
	;

operand:
	expr
	{ take_fwd();
	  ADVANCE(putData(_pc, _dsize, $1, _membuf));
	}
	|

	string
	{ ADVANCE(putString(_pc, _smode, _dsize, (STRV *) $1, _membuf)); }
	;

// Macros and .include are handled entirely by the lexer (see 6502parse.l).
// So are .else, .endif, .ifdef and .ifndef, and so is throwing away the text
// of a false condition: only the value of the condition in .if and .elif comes here. The rest of the
// directives are parsed, but nothing is done with them yet: unions, groups,
// custom types and custom directives.

definition:
	union_def
	{ asmError("unions are not supported yet"); }
	|

	group_def
	{ asmError("groups are not supported yet"); }
	|

	DIR_CUSTOM variable_type VARIABLE
	{ asmError("custom types are not supported yet"); }
	;

union_def:
	DIR_UNION VARIABLE '\n' declaration_block DIR_ENDU
	;

group_def:
	DIR_GROUP VARIABLE '\n' declaration_block DIR_ENDG
	;

dir_custom:
	'.' VARIABLE
	;

declaration_block:
	/* empty */
	| declaration_block declaration_line '\n'
	;

declaration_line:
	/* empty */
	| declaration
	;

declaration:
	variable_type ':' VARIABLE
	| variable_type ':' VARIABLE '[' INT ']'
	| union_def
	| group_def
	;

variable_type:
	VARIABLE
	;

float_list:
	FLOAT
	| float_list ',' FLOAT
	;

custom_type:
	expr
	| string
	;

custom_list:
	custom_type
	| custom_list ',' custom_type
	;

nested_list:
	'{' '}'
	| '{' custom_list '}'
	| '{' nested_list ',' custom_list '}'
	;

expr:
	INT
	| VARIABLE			{ $$ = getVar($1); }
	| CHARLIT			{ $$ = charLiteral($1); }
	| '*'				{ $$ = _pc; }
	| '(' expr ')'			{ $$ = $2; }
	| '+' expr %prec UMINUS		{ $$ = $2; }
	| expr '&' expr			{ $$ = $1 & $3; }
	| expr '%' expr			{ if ($3 == 0) {
						if (_pass == 1)
							asmError("division by zero");
						$$ = 0;
					  } else
						$$ = $1 % $3;
					}
	| expr SHL expr			{ $$ = shiftLeft($1, $3); }
	| expr SHR expr			{ $$ = shiftRight($1, $3); }
	| expr '|' expr			{ $$ = $1 | $3; }
	| expr '^' expr			{ $$ = $1 ^ $3; }
	| expr '*' expr 		{ $$ = $1 * $3; }
	| expr '/' expr 		{ if ($3 == 0) {
						if (_pass == 1)
							asmError("division by zero");
						$$ = 0;
					  } else
						$$ = $1 / $3;
					}
	| expr '+' expr 		{ $$ = $1 + $3; }
	| expr '-' expr 		{ $$ = $1 - $3; }
	| '-' expr %prec UMINUS  	{ $$ = -$2; }
	| '<' expr %prec BYTEL 		{ $$ = $2 & 0xff; }
	| '>' expr %prec BYTEH 		{ $$ = ($2 >> 8) & 0xff; }
	| '!' expr %prec NOT		{ $$ = !$2; }
	| expr EQ expr			{ $$ = $1 == $3; }
	| expr NE expr			{ $$ = $1 != $3; }
	| expr '<' expr			{ $$ = $1 < $3; }
	| expr '>' expr			{ $$ = $1 > $3; }
	| expr LE expr			{ $$ = $1 <= $3; }
	| expr GE expr			{ $$ = $1 >= $3; }
	| expr ANDAND expr		{ $$ = $1 && $3; }
	| expr OROR expr		{ $$ = $1 || $3; }
	| FN_SIZEOF '(' VARIABLE ')'	{ $$ = getSizeof($3); }
	;

string:
	DQSTRING
	{ $$ = getString(2, $1); }
	|

	SQSTRING
	{ $$ = getString(1, $1); }
	;


%%

// main program

// other c code.

void yyerror (const char *s) {
	asmError("%s", s);
}
