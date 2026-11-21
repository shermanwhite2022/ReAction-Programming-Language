/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#ifndef STORAGE_H
#define STORAGE_H

extern int init_compiler_variables();
extern int init_typedef_defaults();
extern int _get_storage(char *);
extern char *_get_compiler_var(char *);
extern int _get_type_storage(char *);
extern int _type_sizeof(char *);
extern char *_valloc(char *, char *);
extern int _set_deref(char *, int, int);
extern int _get_deref_type(char *);
extern int _var_type(char *);
extern int _type_is_record(int);
extern int _in_record();
extern int _scope_enter(int);
extern int _scope_leave();
extern int _scope_current();
extern int _set_array(char *, int, int, int);
extern int _get_array(char *, int *, int *);
extern int _record_begin(char *);
extern int _record_add_field(char *, int, int, int);
extern int _record_end();
extern int _record_add_array_field(char *, int, int);
extern int _field_array(int, char *, int *, int *);
extern int _scope_has_local(char *);
extern int _class_begin(char *);
extern int _class_open();
extern int _class_begin_ext(char *, int);
extern int _record_suspend();
extern int _record_resume();
extern int _type_is_class(int);
extern int _type_size_of_index(int);
extern char *_type_name_of_index(int);
extern int _predeclare_sysmem();
extern int _var_total_size(char *);
extern int _record_set_ftype(int);
extern int _field_type(int, char *);
extern int _field_lookup(int, char *, int *, int *, int *, int *);
extern int _type_index_of(char *);
extern int _get_deref(char *);

#endif
