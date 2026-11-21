/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#ifndef EMIT_H
#define EMIT_H

#include <stdio.h>

extern int init_emit();
extern int emit_data();
extern int emit_header(char *, int);
extern int emit_footer();
extern int emit_assign(char *);
extern int emit_assign_deref(char *);
extern int emit_unsupported(char *);
extern int emit_record_begin(char *);
extern int emit_record_end();
extern int emit_pushAddr(char *);
extern int emit_arithmetic(int, int, char *);
extern int emit_pushVar(char * , char *);
extern int emit_pushConst(int);
extern int emit_pushFloat(double);
extern int emit_mathfn(char *);
extern int emit_set(int, int);
extern int emit_variable_declaration_with_float_init(char *, char *, double);
extern int emit_variable_declaration(char *, char *);
extern int emit_variable_declaration_with_init(char *, char *, int, char *);
extern int emit_pointer_declaration(char *, char *);
extern int emit_pointer_declaration_with_init(char *, char *, int);

extern int emit_if_begin(); extern int emit_elseif_head(); extern int emit_elseif_test();
extern int emit_else(); extern int emit_fi();
extern int emit_while_begin(); extern int emit_while_test(); extern int emit_while_end();
extern int emit_do_begin(); extern int emit_do_end(); extern int emit_until_end(); extern int emit_exit();
extern int emit_for_limit(char *); extern int emit_for_step_default(); extern int emit_for_step();
extern int emit_for_top(); extern int emit_for_end();
extern int emit_case_begin(); extern int emit_case_value(int, int); extern int emit_case_then();
extern int emit_case_clause_end(); extern int emit_case_end();
extern int emit_proc_begin(char *); extern int emit_func_begin(char *, char *);
extern int emit_proc_params_end(); extern int emit_short_begin(char *, int, int); extern int emit_short_end();
extern int emit_param(char *, char *, int);
extern int emit_string_literal(char *); extern int emit_string_declare(char *, char *, int, char *); extern int emit_string_param(char *, char *);
extern int emit_try_begin(); extern int emit_catch(char *); extern int emit_try_end(); extern int emit_throw();
extern int emit_return(); extern int emit_func_return(); extern int emit_internal_return(int);
extern int emit_call_begin(char *); extern int emit_call_arg(); extern int emit_call_string(char *);
extern int emit_call_end_expr(); extern int emit_call_end_stmt();
extern int emit_array_store(); extern int emit_array_compound_begin();
extern int emit_array_size(int); extern int emit_array_value(int); extern int emit_array_addr(int);
extern char *emit_recarr_ref(char *); extern int emit_array_string(char *); extern int emit_array_declare(char *, char *);
extern int emit_array_param(char *, char *);

extern int emit_define(char *, char *); extern int emit_define_number(char *, int);
extern int emit_sizeof_name(char *); extern int emit_sizeof_field(char *);
extern int emit_new(char *);
extern int emit_new_begin(char *);
extern int emit_short_begin_ext(char *, char *, int);
extern int emit_new_end();
extern int emit_class_begin(char *, char *); extern int emit_class_end();
extern char *define_lookup(char *);

extern int emit_short_begin_inline(char *, int); extern int emit_inline_item(char *);
extern char *inline_num(int); extern char *inline_name(char *); extern char *inline_here();
extern char *inline_add(char *, char *);

#endif
