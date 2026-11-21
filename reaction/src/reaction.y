/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
%{
	#include <stdio.h>
	#include <string.h>

	#include "emit.h"
	#include "utils.h"
	#include "functions.h"

	extern int _lineno;

	extern int yylex();
	extern int yyparse();
	extern FILE *yyin;

	#define CC_NONNUM 0x40000000	/* a SET operand that isn't a plain number */

	char _type[256];
	char _strkw[16];
	char _info[256];

	/* kind of the most recent typed parameter in a routine's parameter list:
	   0 = plain variable, 1 = POINTER, 2 = ARRAY.  A bare "..., name" in the
	   list continues that kind (the ARRAY kind emits nothing, as before). */
	static int _pk = 0;

	void yyerror(const char *s);
%}

%union {
	int ival;
	double fval;
	char *sval;
}

%start main_program

%token <fval> RW_FLOAT
%token <ival> INT
%token <ival> RW_BYTE
%token <ival> SQ_CHAR
%token <ival> RW_CARD
%token <sval> VARIABLE
%token <sval> RECORD_VARIABLE
%token <sval> RW_CHAR
%token <sval> DQ_STRING

%token RW_COMMENT
%token RW_ADDRESS
%token RW_AND
%token RW_ARRAY
%token RW_BY
%token RW_CLASS
%token RW_EXTENDS
%token RW_SSALC
%token RW_COMMA
%token RW_CONST
%token RW_DEFINE
%token RW_DO
%token RW_ELSE
%token RW_ELSEIF
%token RW_ENUM
%token RW_EXIT
%token RW_FI
%token RW_FOR
%token RW_FUNC
%token RW_FREE
%token RW_IF
%token RW_INCLUDE
%token RW_INLINE
%token RW_INT
%token RW_IRETURN
%token RW_LSH
%token RW_MALLOC
%token RW_MOD
%token RW_MODULE
%token RW_NEW
%token RW_NEXT
%token RW_OD
%token RW_OF
%token RW_OR
%token RW_PERIOD
%token RW_POINTER
%token RW_PROC
%token RW_REAL
%token RW_TRY RW_CATCH RW_YRT RW_THROW
%token RW_REF
%token RW_RETURN
%token RW_RSH
%token RW_SET
%token RW_STEP
%token RW_THEN
%token RW_TO
%token RW_TYPE
%token RW_UNTIL
%token RW_WHILE
%token RW_XOR
%token RW_ZTRING
%token FN_SIZEOF
%token FN_HEAP
%token FN_STACK
%token RW_EXP
%token RW_EXP10
%token RW_PLUS
%token RW_MINUS
%token RW_MUL
%token RW_DIV
%token RW_AMPERSAND
%token RW_PERCENT
%token RW_EXCLAM
%token RW_EQ
%token RW_EQEQ
%token RW_NE
%token RW_POUND
%token RW_GT
%token RW_GE
%token RW_LT
%token RW_LE
%token RW_DS
%token RW_CARAT
%token RW_AT
%token RW_LPAREN
%token RW_RPAREN
%token RW_LBRACK
%token RW_RBRACK
%token RW_STRING
%token RW_CASE
%token RW_ESAC
%token FN_SIN
%token FN_COS
%token FN_TAN
%token FN_ATAN
%token FN_LN
%token FN_LOG2
%token FN_LOG10

/*
 * Operator precedence, lowest to highest.  Per the ACTION! reference manual
 * (section 4.1.4); operators on one line share a level and associate left.
 *
 *	XOR  !
 *	OR   %
 *	AND  &
 *	=  #  <>  >  >=  <  <=		relational
 *	+  -
 *	*  /  MOD  LSH  RSH
 *	unary -  and  @
 *	**				(REACTION extension; binds tightest, as before)
 */
%left  RW_XOR RW_EXCLAM
%left  RW_OR RW_PERCENT
%left  RW_AND RW_AMPERSAND
%left  RW_EQ RW_POUND RW_NE RW_GT RW_GE RW_LT RW_LE
%left  RW_PLUS RW_MINUS
%left  RW_MUL RW_DIV RW_MOD RW_LSH RW_RSH
%right UMINUS UAT
%right RW_EXP

%type <ival> expr
%type <ival> addr
%type <ival> numeric_value
%type <ival> constant
%type <ival> init_info
%type <ival> signed_int
%type <ival> init_value
%type <ival> comp_const
%type <sval> inline_expr
%type <sval> inline_atom
%type <ival> comp_const_atom
%type <ival> sizeof_value
%type <sval> operator
%type <sval> strkw
%type <sval> ident
%type <sval> compound_ident
%type <sval> recarr_ref
%type <sval> math_function

%%

// grammar to parse

constant:
	RW_LBRACK signed_int RW_RBRACK 	{ $$ = $2; }
	;

/* initializers (not expressions, where unary minus already does this) accept a sign */
signed_int:
	  INT
	| RW_MINUS INT		{ $$ = -$2; }
	;

init_value:
	  numeric_value
	| RW_MINUS INT		{ $$ = -$2; }
	;

ident:
	VARIABLE 	{ $$ = $1; }
	;

compound_ident:
	RECORD_VARIABLE { $$ = $1; }
	;

kw_type:
	  RW_BYTE 	{ strcpy(_type, "BYTE"); }
	| RW_CARD	{ strcpy(_type, "CARD"); }
	| RW_REAL	{ strcpy(_type, "REAL"); }
	| RW_INT	{ strcpy(_type, "INT"); }
	;

type:
	  kw_type
	| ident		{ strcpy(_type, $1); }
	;

numeric_value:
	SQ_CHAR
	| INT
	;

/* operators allowed after "==" (e.g. "x ==+ 1", "flags ==& $0F") */
operator:
	  RW_EXP		{ $$ = "**"; }
	| RW_MUL		{ $$ = "*"; }
	| RW_DIV		{ $$ = "/"; }
	| RW_MOD		{ $$ = "%"; }
	| RW_PLUS		{ $$ = "+"; }
	| RW_MINUS		{ $$ = "-"; }
	| RW_LSH		{ $$ = "lsh"; }
	| RW_RSH		{ $$ = "rsh"; }
	| RW_AMPERSAND	{ $$ = "and"; }
	| RW_AND		{ $$ = "and"; }
	| RW_OR			{ $$ = "or"; }
	| RW_PERCENT	{ $$ = "or"; }
	| RW_XOR		{ $$ = "!"; }
	| RW_EXCLAM		{ $$ = "!"; }
	;

statement:
	  conditional_statement
	| looping_statement
	| subroutine_call_statement
	| assignment_statement
	| return_internal_statement
	| exit_statement
	| try_statement
	| throw_statement
	| case_statement
	| inline_block
	;

execution_statement_block:
	  statement
	| execution_statement_block statement
	;

/* zero or more statements */
opt_execution_statement_block:
	  /* empty */
	| execution_statement_block
	;

addr:
	INT
	;

init_info:
	  addr
	{
		strcpy(_info, "address");
	}
	| constant
	{
		strcpy(_info, "constant");
	}
	;

variable_declaration_with_init:
	  ident
	{
		emit_variable_declaration(_type, $1);
	}
	| ident RW_EQ init_info
	{
		emit_variable_declaration_with_init(_type, $1, $3, _info);
	}
	| ident RW_EQ RW_LBRACK RW_FLOAT RW_RBRACK
	{
		emit_variable_declaration_with_float_init(_type, $1, $4);
	}
	| ident RW_EQ RW_LBRACK RW_MINUS RW_FLOAT RW_RBRACK
	{
		emit_variable_declaration_with_float_init(_type, $1, -$5);
	}
	;

variable_declaration_with_init_list:
	  variable_declaration_with_init
	| variable_declaration_with_init_list RW_COMMA variable_declaration_with_init
	;

typed_variable_declaration_with_init_list:
	type variable_declaration_with_init_list
	;

/* one pointer name, optionally with an address:  p  or  p=$600 */
pointer_declaration_with_init:
	  ident
	{
		emit_pointer_declaration(_type, $1);
	}
	| ident RW_EQ addr
	{
		emit_pointer_declaration_with_init(_type, $1, $3);
	}
	;

pointer_declaration_with_init_list:
	  pointer_declaration_with_init
	| pointer_declaration_with_init_list RW_COMMA pointer_declaration_with_init
	;

/* "BYTE POINTER p, q=$600, r" -- the type is given once for the whole list */
typed_pointer_declaration_with_init_list:
	type RW_POINTER pointer_declaration_with_init_list
	;

/* size and initializer are both optional:  a   a(10)   a=$600   a(4)=[1 2 3 4] */
array_size:
	  RW_LPAREN INT RW_RPAREN		{ emit_array_size($2); }
	| RW_LPAREN sizeof_value RW_RPAREN	{ emit_array_size($2); }
	;

array_values:
	  init_value					{ emit_array_value($1); }
	| array_values init_value		{ emit_array_value($2); }
	;

array_init:
	  RW_EQ addr					{ emit_array_addr($2); }
	| RW_EQ RW_LBRACK array_values RW_RBRACK
	| RW_EQ DQ_STRING				{ emit_array_string($2); }
	;

array_declaration:
	  ident								{ emit_array_declare(_type, $1); }
	| ident array_size					{ emit_array_declare(_type, $1); }
	| ident array_init					{ emit_array_declare(_type, $1); }
	| ident array_size array_init		{ emit_array_declare(_type, $1); }
	;

array_declaration_list:
	  array_declaration
	| array_declaration_list RW_COMMA array_declaration
	;

typed_array_declaration_list:
	type RW_ARRAY array_declaration_list
	;

/* STRING s(40)  ZTRING z(20)="text"  STRING t="text" */
strkw:
	  RW_STRING		{ $$ = "STRING"; }
	| RW_ZTRING		{ $$ = "ZTRING"; }
	;

string_declaration:
	  strkw			{ strcpy(_strkw, $1); }
	  string_item_list
	;

string_item_list:
	  string_item
	| string_item_list RW_COMMA string_item
	;

string_item:
	  ident							{ emit_string_declare(_strkw, $1, -1, NULL); }
	| ident RW_LPAREN INT RW_RPAREN			{ emit_string_declare(_strkw, $1, $3, NULL); }
	| ident RW_EQ DQ_STRING				{ emit_string_declare(_strkw, $1, -1, $3); }
	| ident RW_LPAREN INT RW_RPAREN RW_EQ DQ_STRING	{ emit_string_declare(_strkw, $1, $3, $6); }
	;


/* TYPE name=[ BYTE x,y  CARD z  REAL r  CARD POINTER p ... ]: fields of simple types and pointers */
record_member:
	  typed_variable_declaration_with_init_list
	| typed_pointer_declaration_with_init_list
	| typed_array_declaration_list		/* extension: BYTE ARRAY name(10) */
	;

record_variable_declaration_list:
	  record_member
	| record_variable_declaration_list record_member
	;

record_declaration:
	RW_TYPE ident RW_EQ RW_LBRACK		{ emit_record_begin($2); }
	  record_variable_declaration_list RW_RBRACK	{ emit_record_end(); }
	;

define_declaration_atom:
	  ident RW_EQ DQ_STRING		{ emit_define($1, $3); }
	| ident RW_EQ constant		{ emit_define_number($1, $3); }
	;

define_declaration_atom_list:
	  define_declaration_atom
	| define_declaration_atom_list RW_COMMA define_declaration_atom
	;

define_declaration:
	  RW_DEFINE define_declaration_atom_list
	;

declaration_statement:
	  define_declaration
	| typed_array_declaration_list
	| string_declaration
	| typed_variable_declaration_with_init_list
	| typed_pointer_declaration_with_init_list
	| record_declaration
	| class_definition
	| module_definition
	| set_directive
	;

declaration_statement_block:
	  declaration_statement
	| declaration_statement_block declaration_statement
	;

/*
 * Compiler directives (manual: INCLUDE "file", SET addr=value).
 * A compiler constant is a number, a character, an identifier, or * (the
 * current address), combined with +.  Both directives are STUBS for now:
 * they parse, then warn that they were not processed.
 */
comp_const_atom:
	  INT					{ $$ = $1; }
	| SQ_CHAR				{ $$ = $1; }
	| ident					{ $$ = CC_NONNUM; }
	| RW_MUL				{ $$ = CC_NONNUM; }
	;

comp_const:
	  comp_const_atom
	| comp_const RW_PLUS comp_const_atom	{ $$ = (($1 | $3) & CC_NONNUM) | (($1 + $3) & 0xffff); }
	;

set_directive:
	RW_SET comp_const RW_EQ comp_const	{ emit_set($2, $4); }
	;

/* TRY [stmts] CATCH [(var)] [stmts] YRT: an error raised inside the first block, by THROW or by the
 * library's Error, ends it and runs the second block, with the error code in var. */
try_head:
	  RW_TRY						{ emit_try_begin(); }
	;

catch_head:
	  RW_CATCH						{ emit_catch(NULL); }
	| RW_CATCH RW_LPAREN ident RW_RPAREN	{ emit_catch($3); }
	;

try_statement:
	  try_head opt_execution_statement_block
	  catch_head opt_execution_statement_block RW_YRT	{ emit_try_end(); }
	;

throw_statement:
	  RW_THROW expr					{ emit_throw(); }
	;

exit_statement:
	  RW_EXIT						{ emit_exit(); }
	;

return_statement:
	  RW_RETURN						{ emit_return(); }
	;

return_value_statement:
	  RW_RETURN RW_LPAREN RW_RPAREN			{ emit_return(); }
	| RW_RETURN RW_LPAREN expr RW_RPAREN	{ emit_func_return(); }
	;

return_internal_statement:
	  RW_IRETURN								{ emit_internal_return(0); }
	| RW_IRETURN RW_LPAREN expr RW_RPAREN		{ emit_internal_return(1); }
	;

/*
 * "x = e" evaluates e onto the stack and pops it into x.  "x ==op e" means
 * x = x op e: x is pushed before e, so the operator finds its operands in order.
 * p^ = e stores through the pointer p; r.f = e assigns a record field.  Array elements
 * and NEW are not emitted yet and stop the compiler with an error.
 */
simple_assignment:
	  ident RW_EQ expr						{ emit_assign($1); }
	| ident RW_EQ RW_NEW ident				{ emit_new($4); emit_assign($1); }
	| compound_ident RW_EQ RW_NEW ident		{ emit_new($4); emit_assign($1); }
	| ident RW_EQ new_call					{ emit_assign($1); }
	| compound_ident RW_EQ new_call			{ emit_assign($1); }
	| ident RW_EQ RW_LBRACK expr RW_RBRACK	{ emit_assign($1); }
	| ident RW_EQEQ operator				{ emit_pushVar($1, "i"); }
	  expr									{ emit_arithmetic(0, 0, $3); emit_assign($1); }
	| ident RW_CARAT RW_EQ expr				{ emit_assign_deref($1); }
	| ident RW_CARAT RW_EQEQ operator		{ emit_pushVar($1, "^"); }
	  expr									{ emit_arithmetic(0, 0, $4); emit_assign_deref($1); }
	;

new_head:
	  RW_NEW ident RW_LPAREN					{ emit_new_begin($2); }
	;

new_call:
	  new_head RW_RPAREN						{ emit_new_end(); }
	| new_head parameter_list RW_RPAREN		{ emit_new_end(); }
	;

/* a(i).field of an array of records: it works like a field of a record variable */
recarr_ref:
	  call_head parameter_list RW_RPAREN RW_PERIOD ident	{ $$ = emit_recarr_ref($5); }
	;

compound_assignment:
	  recarr_ref RW_EQ expr					{ emit_assign($1); }
	| recarr_ref RW_EQEQ operator			{ emit_pushVar($1, "ci"); }
	  expr									{ emit_arithmetic(0, 0, $3); emit_assign($1); }
	| compound_ident RW_EQ expr				{ emit_assign($1); }
	| compound_ident RW_EQ RW_LBRACK expr RW_RBRACK	{ emit_assign($1); }
	| compound_ident RW_EQEQ operator		{ emit_pushVar($1, "ci"); }
	  expr									{ emit_arithmetic(0, 0, $3); emit_assign($1); }
	| compound_ident RW_CARAT RW_EQ expr	{ emit_assign_deref($1); }
	| compound_ident RW_CARAT RW_EQEQ operator	{ emit_pushVar($1, "^"); }
	  expr									{ emit_arithmetic(0, 0, $4); emit_assign_deref($1); }
	;

array_assignment:
	  call_head parameter_list RW_RPAREN RW_EQ expr	{ emit_array_store(); }
	| call_head parameter_list RW_RPAREN RW_EQEQ operator	{ emit_array_compound_begin(); }
	  expr										{ emit_arithmetic(0, 0, $5); emit_array_store(); }
	| call_head parameter_list RW_RPAREN RW_EQ RW_LBRACK expr RW_RBRACK	{ emit_unsupported("assignment to an array element"); }
	;

/* name( -- a routine call or an array element; the arguments follow */
call_head:
	  ident RW_LPAREN				{ emit_call_begin($1); }
	| compound_ident RW_LPAREN		{ emit_call_begin($1); }
	;

assignment_statement:
	  simple_assignment
	| compound_assignment
	| array_assignment
	;

/*
 * Routine parameters (manual 6.4):  PROC p(BYTE a,b, CARD c)
 * Groups of different types are separated by commas; a group may also follow
 * the previous one with no comma when it starts with a built-in type keyword
 * (BYTE a CARD b), which is what the manual's own examples sometimes do.
 */
param_decl_tail:
	  ident					{ _pk = 0; emit_param(_type, $1, 0); }
	| ident RW_EQ signed_int	{ _pk = 0; emit_param(_type, $1, 0); emit_param_default($3); }
	| RW_POINTER ident		{ _pk = 1; emit_param(_type, $2, 1); }
	| RW_ARRAY ident		{ _pk = 2; emit_array_param(_type, $2); }
	;

typed_param:
	  type param_decl_tail
	;

kw_typed_param:
	  kw_type param_decl_tail
	;

string_param:
	  strkw ident		{ _pk = 2; strcpy(_type, "BYTE"); emit_string_param($1, $2); }
	;

vector_declaration_list:
	  typed_param
	| string_param
	| vector_declaration_list string_param
	| vector_declaration_list RW_COMMA string_param
	| vector_declaration_list kw_typed_param
	| vector_declaration_list RW_COMMA typed_param
	| vector_declaration_list RW_COMMA ident
	{
		if (_pk == 2) emit_array_param(_type, $3);
		else emit_param(_type, $3, _pk);
	}
	| vector_declaration_list RW_COMMA ident RW_EQ signed_int
	{
		if (_pk == 2) emit_array_param(_type, $3);
		else emit_param(_type, $3, _pk);
		emit_param_default($5);
	}
	;

vector_parameter_list:
	  RW_LPAREN vector_declaration_list RW_RPAREN
	| RW_LPAREN RW_RPAREN
	;

/* routines that have a body */
proc_short:
	  RW_PROC ident					{ emit_proc_begin($2); }
	  vector_parameter_list			{ emit_proc_params_end(); }
	| RW_PROC compound_ident		{ emit_proc_begin($2); }
	  vector_parameter_list			{ emit_proc_params_end(); }
	;

proc_definition:
	  proc_short declaration_statement_block execution_statement_block return_statement
	| proc_short declaration_statement_block return_statement
	| proc_short execution_statement_block return_statement
	| proc_short return_statement
	;

func_short:
	  type RW_FUNC ident			{ emit_func_begin($3, _type); }
	  vector_parameter_list			{ emit_proc_params_end(); }
	| type RW_FUNC compound_ident	{ emit_func_begin($3, _type); }
	  vector_parameter_list			{ emit_proc_params_end(); }
	;

func_definition:
	  func_short declaration_statement_block execution_statement_block return_value_statement
	| func_short declaration_statement_block return_value_statement
	| func_short execution_statement_block return_value_statement
	| func_short return_value_statement
	;

/*
 * Inline machine code: [ $A9 $07 $8D var ] puts those bytes in the program.  A number
 * up to 255 is one byte, a larger one two (low, high); a variable or routine name is
 * its 16 bit address; + adds a number.  As a routine's whole body:
 *     BYTE FUNC AddOne=*(BYTE v)[ $18 $69 $01 $60 ]
 * the arguments arrive in A, X, Y and a FUNC returns its value in A (or A,X).
 */
inline_atom:
	  INT					{ $$ = inline_num($1); }
	| SQ_CHAR				{ $$ = inline_num($1); }
	| RW_MINUS INT			{ $$ = inline_num(-$2); }
	| ident					{ $$ = inline_name($1); }
	| RW_MUL				{ $$ = inline_here(); }
	;

inline_expr:
	  inline_atom
	| inline_expr RW_PLUS inline_atom	{ $$ = inline_add($1, $3); }
	;

inline_items:
	  inline_expr					{ emit_inline_item($1); }
	| inline_items inline_expr		{ emit_inline_item($2); }
	;

inline_block:
	  RW_LBRACK inline_items RW_RBRACK
	;

/* routines at a fixed address (e.g. OS entry points); no body */
short:
	  RW_PROC ident RW_EQ addr		{ emit_short_begin($2, $4, 0); }
	  vector_parameter_list			{ emit_short_end(); }
	| type RW_FUNC ident RW_EQ addr	{ emit_short_begin($3, $5, 1); }
	  vector_parameter_list			{ emit_short_end(); }
	| RW_PROC ident RW_EQ RW_AT ident	{ emit_short_begin_ext($2, $5, 0); }
	  vector_parameter_list			{ emit_short_end(); }
	| type RW_FUNC ident RW_EQ RW_AT ident	{ emit_short_begin_ext($3, $6, 1); }
	  vector_parameter_list			{ emit_short_end(); }
	| RW_PROC ident RW_EQ RW_MUL	{ emit_short_begin_inline($2, 0); }
	  vector_parameter_list			{ emit_short_end(); }
	  inline_block
	| type RW_FUNC ident RW_EQ RW_MUL	{ emit_short_begin_inline($3, 1); }
	  vector_parameter_list			{ emit_short_end(); }
	  inline_block
	;

module_definition:
	RW_MODULE
	;

section_definition:
	  proc_definition
	| func_definition
	| short
	;

/* directives may also follow a routine, so libraries can be INCLUDEd between routines */
section_definition_list:
	  section_definition
	| section_definition_list section_definition
	| section_definition_list declaration_statement	/* globals may also follow a routine */
	;

/* a class collects data members and procedures/functions, in any order */
class_member:
	  declaration_statement
	| section_definition
	;

class_member_list:
	  class_member
	| class_member_list class_member
	;

class_head:
	  RW_CLASS ident						{ emit_class_begin($2, NULL); }
	| RW_CLASS ident RW_EXTENDS ident		{ emit_class_begin($2, $4); }
	;

class_definition:
	  class_head RW_SSALC			{ emit_class_end(); }
	| class_head class_member_list RW_SSALC	{ emit_class_end(); }
	;


/* [classes] [MODULE] [declarations] [routines] -- at least one part */
main_program:
	  program_body
	;

program_body:
	  program_tail
	;

program_tail:
	  declaration_statement_block
	| declaration_statement_block section_definition_list
	| section_definition_list
	;

parameter:
	  expr					{ emit_call_arg(); }
	;

parameter_list:
	parameter
	| parameter_list RW_COMMA parameter
	;

expr:
	  call_head RW_RPAREN						{ emit_call_end_expr(); $$ = 0; }
	| call_head parameter_list RW_RPAREN		{ emit_call_end_expr(); $$ = 0; }
	| RW_LPAREN expr RW_RPAREN		{ $$ = $2; }
	| expr RW_EXP expr				{ emit_arithmetic($1, $3, "**"); }
    | RW_MINUS expr %prec UMINUS 	{ emit_arithmetic(-1, $2, "n"); }
    | RW_AT ident %prec UAT     	{ emit_pushAddr($2); }	/* @x: the address of x */
    | RW_AT compound_ident %prec UAT	{ emit_pushAddr($2); }	/* @r.f: the address of a field */
    | expr RW_MUL expr         		{ emit_arithmetic($1, $3, "*"); }
    | expr RW_DIV expr         		{ emit_arithmetic($1, $3, "/"); }
    | expr RW_MOD expr         		{ emit_arithmetic($1, $3, "%"); }
    | expr RW_RSH expr 				{ emit_arithmetic($1, $3, "rsh"); }
    | expr RW_LSH expr 				{ emit_arithmetic($1, $3, "lsh"); }
    | expr RW_PLUS expr        		{ emit_arithmetic($1, $3, "+"); }
    | expr RW_MINUS expr       		{ emit_arithmetic($1, $3, "-"); }
	| expr RW_GT expr				{ emit_arithmetic($1, $3, ">"); }
	| expr RW_GE expr				{ emit_arithmetic($1, $3, ">="); }
	| expr RW_LT expr				{ emit_arithmetic($1, $3, "<"); }
	| expr RW_LE expr				{ emit_arithmetic($1, $3, "<="); }
	| expr RW_POUND expr			{ emit_arithmetic($1, $3, "#"); }
	| expr RW_NE expr				{ emit_arithmetic($1, $3, "#"); }
	| expr RW_EQ expr				{ emit_arithmetic($1, $3, "="); }
	| expr RW_EXCLAM expr			{ emit_arithmetic($1, $3, "!"); }
	| expr RW_XOR expr				{ emit_arithmetic($1, $3, "!"); }
	| expr RW_AMPERSAND expr		{ emit_arithmetic($1, $3, "and"); }
	| expr RW_AND expr				{ emit_arithmetic($1, $3, "and"); }
	| expr RW_OR expr				{ emit_arithmetic($1, $3, "or"); }
	| expr RW_PERCENT expr			{ emit_arithmetic($1, $3, "or"); }
    | recarr_ref					{ emit_pushVar($1, "ci"); }
    | compound_ident RW_CARAT      	{ emit_pushVar($1, "^"); }
    | compound_ident      			{ emit_pushVar($1, "ci"); }
    | ident RW_CARAT       			{ emit_pushVar($1, "^"); }
    | ident 	         			{ emit_pushVar($1, "i"); }
    | numeric_value					{ emit_pushConst($1); }
	| RW_FLOAT						{ emit_pushFloat($1); }
	| DQ_STRING						{ emit_string_literal($1); }
	| sizeof_value						{ emit_pushConst($1); $$ = 0; }
	| math_function RW_LPAREN expr RW_RPAREN	{ emit_mathfn($1); }
    ;

/* SIZEOF(BYTE)  SIZEOF(rec)  SIZEOF(var)  SIZEOF(r.field)  SIZEOF(type POINTER): a compile time constant */
sizeof_value:
	  FN_SIZEOF RW_LPAREN kw_type RW_RPAREN				{ $$ = emit_sizeof_name(_type); }
	| FN_SIZEOF RW_LPAREN kw_type RW_POINTER RW_RPAREN	{ $$ = 2; }
	| FN_SIZEOF RW_LPAREN ident RW_RPAREN				{ $$ = emit_sizeof_name($3); }
	| FN_SIZEOF RW_LPAREN ident RW_POINTER RW_RPAREN	{ $$ = 2; }
	| FN_SIZEOF RW_LPAREN compound_ident RW_RPAREN		{ $$ = emit_sizeof_field($3); }
	;

math_function:
	  FN_SIN	{ $$ = "sin"; }
	| FN_COS	{ $$ = "cos"; }
	| FN_TAN	{ $$ = "tan"; }
	| FN_ATAN	{ $$ = "atan"; }
	| FN_LN		{ $$ = "ln"; }
	| FN_LOG2	{ $$ = "log2"; }
	| FN_LOG10	{ $$ = "log10"; }
	;

/*
 * CASE expr
 *	  1, 2, 'A' THEN stmts
 *	  3         THEN stmts
 *	  ELSE stmts		(optional)
 * ESAC
 * Labels are integer or character constants; no fall-through.
 */
case_value_list:
	  numeric_value							{ emit_case_value($1, 1); }
	| case_value_list RW_COMMA numeric_value	{ emit_case_value($3, 0); }
	;

case_clause:
	  case_value_list RW_THEN				{ emit_case_then(); }
	  opt_execution_statement_block			{ emit_case_clause_end(); }
	;

case_clause_list:
	  case_clause
	| case_clause_list case_clause
	;

case_default:
	  /* empty */
	| RW_ELSE opt_execution_statement_block
	;

case_statement:
	  RW_CASE expr					{ emit_case_begin(); }
	  case_clause_list case_default RW_ESAC	{ emit_case_end(); }
	;

/* IF c THEN [stmts] {ELSEIF c THEN [stmts]} [ELSE [stmts]] FI */
elseif_conditional_statement:
	  RW_ELSEIF					{ emit_elseif_head(); }
	  expr						{ emit_elseif_test(); }
	  RW_THEN opt_execution_statement_block
	;

elseif_conditional_statement_block_list:
	  /* empty */
	| elseif_conditional_statement_block_list elseif_conditional_statement
	;

else_conditional_statement_block:
	  /* empty */
	| RW_ELSE					{ emit_else(); }
	  opt_execution_statement_block
	;

conditional_statement:
	  RW_IF expr RW_THEN		{ emit_if_begin(); }
	  opt_execution_statement_block
		elseif_conditional_statement_block_list
		else_conditional_statement_block RW_FI		{ emit_fi(); }
	;

do_body:
	  RW_DO RW_OD
	| RW_DO execution_statement_block RW_OD
	;

for_step:
	  /* empty */				{ emit_for_step_default(); }
	| RW_STEP expr				{ emit_for_step(); }
	;

for_head:
	  RW_FOR VARIABLE RW_EQ expr	{ emit_assign($2); }
	  RW_TO expr					{ emit_for_limit($2); }
	  for_step						{ emit_for_top(); }
	;

for_loop_statement:
	  for_head do_body				{ emit_for_end(); }
	;

while_head:
	  RW_WHILE						{ emit_while_begin(); }
	  expr							{ emit_while_test(); }
	;

while_loop_statement:
	  while_head do_body			{ emit_while_end(); }
	;

do_head:
	  RW_DO							{ emit_do_begin(); }
	;

do_loop_statement:
	  do_head RW_OD											{ emit_do_end(); }
	| do_head execution_statement_block RW_OD				{ emit_do_end(); }
	| do_head RW_UNTIL expr RW_OD							{ emit_until_end(); }
	| do_head execution_statement_block RW_UNTIL expr RW_OD	{ emit_until_end(); }
	;

looping_statement:
	  for_loop_statement
	| while_loop_statement
	| do_loop_statement
	;

subroutine_call_statement:
	  call_head RW_RPAREN						{ emit_call_end_stmt(); }
	| call_head parameter_list RW_RPAREN		{ emit_call_end_stmt(); }
	;
%%

