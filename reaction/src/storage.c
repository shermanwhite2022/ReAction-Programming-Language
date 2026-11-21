/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_VARS 	32768
#define MAX_TYPES 	8192	

// internal structures for creating types and variables

typedef struct field_rec {
		char name[64];
		int offset;
		int size;
		int deref_size;			// for a pointer field: size of what it points to
		int deref_type;			// ... and its type index (-1 if none)
		int ftype;				// type index of the field (of an array field: of its elements)
		int elsize;				// array field: bytes per element (0 if not an array)
		int count;				// array field: number of elements
	} field_record;

typedef struct {
		char type_name[256];
		char *type_defn;		// complex type definition
		unsigned type_size;
		int nfields;			// > 0 (or is_record) for a TYPE record
		int is_record;
		int is_class;			// a CLASS: a record that also owns methods
		struct field_rec *fields;
	} type_record;

typedef struct {
		char var_name[256];
		unsigned var_type;
		char compiler_name[16]; // the form VARXnnnnnn
		int deref_size;			// for a pointer: size in bytes of what it points to (0 if not a pointer)
		int deref_type;			// ... and the type index of what it points to (-1 if none)
		int scope;				// 0 = global, otherwise the id of the routine it is local to
		int arr;				// 0 not an array, 1 data block at the label, 2 the label holds the address
		int elsize;				// array: bytes per element
		int count;				// array: number of elements
	} var_record;

typedef struct {
		int count;
		var_record table[MAX_VARS];
	} cvar_table;

typedef struct {
		int count;
		type_record table[MAX_TYPES];
	} type_table;

// local variables

cvar_table	vt;
type_table	tt;

// scopes: names declared inside a routine are local to it; a lookup tries the
// current routine first, then the globals

static int _cur_scope= 0;

int _scope_enter(int id) { _cur_scope= id; return 0; }
int _scope_leave() { _cur_scope= 0; return 0; }
int _scope_current() { return _cur_scope; }

// is the name declared in the scope being compiled (not a global)?
int _scope_has_local(char *name) {
	int i;

	if (!_cur_scope) return 0;
	for (i=0; i<vt.count; i++)
		if (vt.table[i].scope == _cur_scope && !strcasecmp(name, vt.table[i].var_name))
			return 1;
	return 0;
}

static int _find(char *name) {
	int i;

	if (_cur_scope)
		for (i=0; i<vt.count; i++)
			if (vt.table[i].scope == _cur_scope && !strcasecmp(name, vt.table[i].var_name))
				return i;
	for (i=0; i<vt.count; i++)
		if (vt.table[i].scope == 0 && !strcasecmp(name, vt.table[i].var_name))
			return i;
	return -1;
}
int _valloc_cnt= 0;

// procedures and functions for evaluating types
// and allocating resources

int init_compiler_variables() {
	int i;

	tt.count = 0;
	vt.count = 0;
	_valloc_cnt= 0;
}

int init_typedef_defaults() {
	int i=0;
	type_record tr;

	memset(&tr, 0, sizeof(tr));

	strcpy(tr.type_name, "byte");
	tr.type_defn = NULL;
	tr.type_size = 1;
	tt.table[i++]= tr;
	
	strcpy(tr.type_name, "card");
	tr.type_defn = NULL;
	tr.type_size = 2;
	tt.table[i++]= tr;
	
	strcpy(tr.type_name, "int");
	tr.type_defn = NULL;
	tr.type_size = 2;
	tt.table[i++]= tr;
	
	strcpy(tr.type_name, "real");
	tr.type_defn = NULL;
	tr.type_size = 4;
	tt.table[i++]= tr;
	
	strcpy(tr.type_name, "string"); 	// a string is a pointer to a string constant in ucsd format
	tr.type_defn = NULL;
	tr.type_size = 2;
	tt.table[i++]= tr;
	
	strcpy(tr.type_name, "ztring");		// a ztring is a pointer to a string constant terminated by 0
	tr.type_defn = NULL;
	tr.type_size = 2;
	tt.table[i++]= tr;
	
	tt.count = i;
}

// type allocation
// define a new type in the type allocation table

int _talloc(char *name) {
	int i;

	for (i=0; i<tt.count; i++) 
		if (!strcasecmp(name, tt.table[i].type_name)) {
			printf("ERR! _talloc: attempt to redefine previously defined type (%s)!\n", name);
			exit(1);
		}

	i= tt.count;

	if (i<MAX_TYPES) {
		strncpy(tt.table[i].type_name, name, 255);
		tt.table[i].type_defn= NULL;
		tt.count++;
	} else {
		printf("ERR: _talloc: type definition %s out of storage space!\n", name);
		exit(1);
	}

	return (0);
}

// get the index of a predefined type

int _type_index(char *tname) {
	int i;

	for (i=0; i<tt.count; i++) 
		if (!strcasecmp(tname, tt.table[i].type_name)) {
			return i;
		}

	return -1;
}

// get the size of a simple type

int _type_sizeof(char *tname) {
	int i;

	for (i=0; i<tt.count; i++) 
		if (!strcasecmp(tname, tt.table[i].type_name)) {
			return (tt.table[i].type_size);
		}

	return -1;
}

// get the size of of a variable

int _get_storage(char *vname) {
	int i= _find(vname);

	if (i < 0) return -1;
	return (tt.table[vt.table[i].var_type].type_size);
}

// create a simple type

int create_type(char *defn, int size) {

}

// create a complex_type;

int create_complex_type(char *defn, int size) {


}

// variable allocation
// define a new variable in the variable allocation_table

char *_valloc (char *name, char *type) {
	int i;

	for (i=0; i<vt.count; i++)
		if (vt.table[i].scope == _cur_scope && !strcasecmp(name, vt.table[i].var_name)){
			printf("ERR: _valloc: attempt to redeclare variable %s\n", name);
			exit(1);
		}

	if (vt.count >= MAX_VARS) {
		printf("ERR: _valloc: out of variable space!\n");
		exit(1);
	}

	i= vt.count;
	strcpy(vt.table[i].var_name, name);
	vt.table[i].var_type = _type_index(type);
	vt.table[i].deref_size = 0;
	vt.table[i].deref_type = -1;
	vt.table[i].scope = _cur_scope;
	vt.table[i].arr = 0;
	vt.table[i].elsize = 0;
	vt.table[i].count = 0;
	sprintf(vt.table[i].compiler_name,"VARX%08d", _valloc_cnt++);
	vt.count++;

	return(0);
}

// get compiler label assocated with request variable (needle)

char *_get_compiler_var(char *needle) {
	int i= _find(needle);

	return (i < 0)? NULL: vt.table[i].compiler_name;
}

// remember / look up what a pointer variable points to

int _set_deref(char *name, int size, int type) {
	int i= _find(name);

	if (i < 0) return -1;
	vt.table[i].deref_size = size;
	vt.table[i].deref_type = type;
	return 0;
}

int _get_deref(char *name) {
	int i= _find(name);

	return (i < 0)? -1: vt.table[i].deref_size;
}

int _get_deref_type(char *name) {
	int i= _find(name);

	return (i < 0)? -1: vt.table[i].deref_type;
}

// index of the type of a variable, -1 if there is no such variable

int _var_type(char *name) {
	int i= _find(name);

	return (i < 0)? -1: vt.table[i].var_type;
}

// array information

int _set_array(char *name, int kind, int elsize, int count) {
	int i= _find(name);

	if (i < 0) return -1;
	vt.table[i].arr = kind;
	vt.table[i].elsize = elsize;
	vt.table[i].count = count;
	return 0;
}

int _get_array(char *name, int *elsize, int *count) {
	int i= _find(name);

	if (i < 0) return 0;
	if (elsize) *elsize= vt.table[i].elsize;
	if (count) *count= vt.table[i].count;
	return vt.table[i].arr;
}

int _type_is_record(int t) {
	return (t >= 0 && t < tt.count && tt.table[t].is_record);
}

// ---- TYPE records --------------------------------------------------------
// _record_begin starts a new type; each field declared until _record_end
// is added at the next offset and the type's size is the sum of the fields.

static int _rec_open= -1;
static int _rec_saved= -1;		// the open CLASS, while a method body is being compiled

int _in_record() {
	return (_rec_open >= 0);
}

int _record_begin(char *name) {
	int i= tt.count;

	if (_rec_open >= 0 || _rec_saved >= 0) {
		printf("ERR! TYPE %s: a TYPE can't be declared inside another TYPE or a CLASS!\n", name);
		exit(1);
	}

	if (_type_index(name) >= 0) {
		printf("ERR! TYPE %s: name already used!\n", name);
		exit(1);
	}
	if (i >= MAX_TYPES) {
		printf("ERR! TYPE %s: out of type space!\n", name);
		exit(1);
	}
	memset(&tt.table[i], 0, sizeof(type_record));
	strncpy(tt.table[i].type_name, name, 255);
	tt.table[i].is_record= 1;
	tt.count++;
	_rec_open= i;
	return (0);
}

int _record_add_field(char *fname, int size, int deref_size, int deref_type) {
	type_record *t;
	field_record *f;
	int k;

	if (_rec_open < 0) return (-1);
	t= &tt.table[_rec_open];

	for (k=0; k<t->nfields; k++)
		if (!strcasecmp(fname, t->fields[k].name)) {
			printf("ERR! TYPE %s: field %s declared twice!\n", t->type_name, fname);
			exit(1);
		}

	t->fields= realloc(t->fields, (t->nfields + 1) * sizeof(field_record));
	f= &t->fields[t->nfields++];
	memset(f, 0, sizeof(*f));
	strncpy(f->name, fname, 63);
	f->offset= t->type_size;
	f->size= size;
	f->deref_size= deref_size;
	f->deref_type= deref_type;
	t->type_size+= size;
	return (0);
}

// a fixed size array field (a reaction extension): count elements of elsize bytes

int _record_add_array_field(char *fname, int elsize, int count) {
	int r= _record_add_field(fname, elsize * count, 0, -1);
	type_record *t;

	if (r < 0) return r;
	t= &tt.table[_rec_open];
	t->fields[t->nfields-1].elsize= elsize;
	t->fields[t->nfields-1].count= count;
	return 0;
}

int _field_array(int t, char *fname, int *elsize, int *count) {
	int k;

	if (!_type_is_record(t)) return 0;
	for (k=0; k<tt.table[t].nfields; k++)
		if (!strcasecmp(fname, tt.table[t].fields[k].name)) {
			if (elsize) *elsize= tt.table[t].fields[k].elsize;
			if (count) *count= tt.table[t].fields[k].count;
			return (tt.table[t].fields[k].elsize > 0);
		}
	return 0;
}

// CLASS: a record whose routines are methods.  The record stays open for data
// members; while a method is compiled it is set aside so locals are not fields.

int _class_begin(char *name) {
	int r= _record_begin(name);

	tt.table[_rec_open].is_class= 1;
	return r;
}

// a class with a parent starts with a copy of the parent's fields (same offsets);
// a root class starts with the hidden field __vt, the pointer to its method table

int _class_begin_ext(char *name, int parent) {
	int r= _class_begin(name);
	int k;
	type_record *p;

	if (parent < 0) return _record_add_field("__vt", 2, 0, -1);
	p= &tt.table[parent];
	for (k=0; k<p->nfields; k++) {
		field_record *f= &p->fields[k];
		_record_add_field(f->name, f->size, f->deref_size, f->deref_type);
		tt.table[_rec_open].fields[tt.table[_rec_open].nfields-1].ftype= f->ftype;
		tt.table[_rec_open].fields[tt.table[_rec_open].nfields-1].elsize= f->elsize;
		tt.table[_rec_open].fields[tt.table[_rec_open].nfields-1].count= f->count;
	}
	return r;
}

int _class_open() { return (_rec_open >= 0)? _rec_open: _rec_saved; }

int _record_suspend() {
	if (_rec_open >= 0) { _rec_saved= _rec_open; _rec_open= -1; }
	return 0;
}

int _record_resume() {
	if (_rec_saved >= 0) { _rec_open= _rec_saved; _rec_saved= -1; }
	return 0;
}

int _type_is_class(int t) {
	return (t >= 0 && t < tt.count && tt.table[t].is_class);
}

int _type_size_of_index(int t) {
	return (t >= 0 && t < tt.count)? (int)tt.table[t].type_size: -1;
}

char *_type_name_of_index(int t) {
	return (t >= 0 && t < tt.count)? tt.table[t].type_name: "";
}

int _record_set_ftype(int t) {
	type_record *r;

	if (_rec_open < 0) return -1;
	r= &tt.table[_rec_open];
	if (r->nfields > 0) r->fields[r->nfields-1].ftype= t;
	return 0;
}

int _field_type(int t, char *fname) {
	int k;

	if (!_type_is_record(t)) return -1;
	for (k=0; k<tt.table[t].nfields; k++)
		if (!strcasecmp(fname, tt.table[t].fields[k].name))
			return tt.table[t].fields[k].ftype;
	return -1;
}

int _record_end() {
	_rec_open= -1;
	return (0);
}

// look up a field of record type t; returns 0 and fills the outputs, or -1

int _field_lookup(int t, char *fname, int *offset, int *size, int *deref_size, int *deref_type) {
	int k;

	if (!_type_is_record(t)) return (-1);
	for (k=0; k<tt.table[t].nfields; k++)
		if (!strcasecmp(fname, tt.table[t].fields[k].name)) {
			*offset= tt.table[t].fields[k].offset;
			*size= tt.table[t].fields[k].size;
			*deref_size= tt.table[t].fields[k].deref_size;
			*deref_type= tt.table[t].fields[k].deref_type;
			return (0);
		}
	return (-1);
}

int _type_index_of(char *name) {
	return _type_index(name);
}


// SYSMEM: a predeclared BYTE array over the whole 64K address space (SYSMEM = address 0)
int _predeclare_sysmem() {
	int i;

	_valloc("SYSMEM", "byte");
	i= vt.count - 1;
	strcpy(vt.table[i].compiler_name, "SYSMEM");
	vt.table[i].arr= 1;
	vt.table[i].elsize= 1;
	vt.table[i].count= 65536;
	_valloc_cnt--;					// SYSMEM doesn't use up a VARX number
	return 0;
}

// total size in bytes of a variable (an array: all its elements); -1 if unknown
int _var_total_size(char *name) {
	int i= _find(name);

	if (i < 0) return -1;
	if (vt.table[i].arr == 1) return vt.table[i].elsize * vt.table[i].count;
	return (int)tt.table[vt.table[i].var_type].type_size;
}
