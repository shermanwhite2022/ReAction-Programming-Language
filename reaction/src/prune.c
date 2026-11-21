/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
/* prune.c - dead code removal for the reaction compiler
 *
 * The compiler writes the whole program as assembly text; this pass runs over that text and
 * leaves out the routines and data nothing can reach.  It works on the text, so it treats the
 * compiler's output and the run time library files (rtl.inc, fp.inc, io.inc ...) the same way,
 * and the result no longer needs the .include files: they are copied in, minus what is not used.
 *
 * The text is cut into units:
 *   - a routine the compiler wrote, between  ;@unit  and  ;@endunit  (marked by the compiler)
 *   - in the variable section (";@mode prog") one unit per label: a variable, a string, a class
 *     method table
 *   - in the library (";@mode lib") one unit per label too, with two rules: a unit that does not
 *     end in RTS, JMP or RTI runs on into the next one, so the next one stays with it; and
 *     neighbouring data units are kept or dropped together (tables whose entries must stay in
 *     order, parameter blocks...).  A comment line  ;@break  ends such a run.
 *   - everything else stays: the header, the program entry, the assembler directives (.if, .const,
 *     .map, .macro ...)
 * A unit is kept if a label it defines is named by something that is kept, starting from the
 * lines that always stay.  Names are found in the text by looking at the identifiers of each line
 * (comments and quoted characters left out); a macro call (_NAME) counts as naming what the macro's
 * body names.  Labels may be defined several times (the alternatives of a .if): then all of them
 * are kept, which can only keep too much.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- directories searched for .include files -------------------------------------------- */

static char *libdirs[32];
static int   nlibdirs= 0;

void prune_add_dir(const char *d) {
	if (nlibdirs < 32) libdirs[nlibdirs++]= strdup(d);
}

static FILE *lib_open(const char *name) {
	char path[1200];
	const char *base= strrchr(name, '/');
	FILE *f;
	int i;

	base= base? base + 1: name;
	for (i= 0; i < nlibdirs; i++) {
		snprintf(path, sizeof path, "%s/%s", libdirs[i], name);
		if ((f= fopen(path, "r"))) return f;
		snprintf(path, sizeof path, "%s/%s", libdirs[i], base);
		if ((f= fopen(path, "r"))) return f;
	}
	return fopen(name, "r");
}

/* ---- small containers ------------------------------------------------------------------ */

typedef struct { char **v; int n, cap; } strvec;

static void sv_add(strvec *s, const char *t) {
	if (s->n == s->cap) { s->cap= s->cap? s->cap * 2: 8; s->v= realloc(s->v, s->cap * sizeof(char *)); }
	s->v[s->n++]= strdup(t);
}

static void upcase(char *s) { for (; *s; s++) *s= toupper((unsigned char)*s); }

typedef struct { char *name; int *u; int nu; } lab;
typedef struct lnode { lab l; struct lnode *next; } lnode;
#define HSIZE 8192
static lnode *htab[HSIZE];

static unsigned hash(const char *s) { unsigned h= 5381; while (*s) h= h * 33 + (unsigned char)*s++; return h % HSIZE; }

static lab *lab_find(const char *up, int create) {
	unsigned h= hash(up);
	lnode *n;
	for (n= htab[h]; n; n= n->next) if (!strcmp(n->l.name, up)) return &n->l;
	if (!create) return NULL;
	n= calloc(1, sizeof(lnode));
	n->l.name= strdup(up);
	n->next= htab[h]; htab[h]= n;
	return &n->l;
}

static void lab_define(const char *label, int unit) {
	char up[200];
	lab *l;
	snprintf(up, sizeof up, "%s", label); upcase(up);
	l= lab_find(up, 1);
	l->u= realloc(l->u, (l->nu + 1) * sizeof(int));
	l->u[l->nu++]= unit;
}

/* ---- lines and units ------------------------------------------------------------------ */

static strvec lines;				/* the whole text, includes expanded */
static int   *lu;				/* for each line: its unit, -1 always kept, -2 a marker (dropped) */

typedef struct {
	int mode;
	int expl;				/* a routine the compiler marked */
	int has_code, has_data;
	int last_kind;				/* 0 none, 1 code, 2 data */
	char last_op[24];
	int falls;				/* runs on into the next unit */
	int cond_after;				/* an .else/.endif follows its last line */
	int nomerge;
	int group;
	int glast;				/* for the first unit of a group: the last */
	strvec toks, labs;
} unit;

static unit *units;
static int nunits= 0, cunits= 0;

enum { M_ROOT, M_PROG, M_LIB };

typedef struct { char name[64]; strvec toks; int seen; } macro;
static macro *macros;
static int nmacros= 0, cmacros= 0;

static strvec roottoks;

static int new_unit(int mode, int expl) {
	unit *u;
	if (nunits == cunits) { cunits= cunits? cunits * 2: 256; units= realloc(units, cunits * sizeof(unit)); }
	u= &units[nunits];
	memset(u, 0, sizeof(*u));
	u->mode= mode; u->expl= expl; u->group= nunits;
	return nunits++;
}

/* ---- reading a line ------------------------------------------------------------------- */

static int ident_start(int c) { return isalpha(c) || c == '_'; }
static int ident_char(int c) { return isalnum(c) || c == '_'; }

typedef struct {
	int has_label;
	char label[128];
	const char *rest;			/* after the label */
	char op[64];				/* first word of rest, "" for a blank or comment line */
	const char *args;
} pline;

static void parse_line(const char *s, pline *p) {
	int n= 0;
	memset(p, 0, sizeof(*p));
	if (ident_start((unsigned char)s[0])) {
		while (ident_char((unsigned char)s[n]) && n < 127) { p->label[n]= s[n]; n++; }
		p->label[n]= 0;
		p->has_label= 1;
		if (s[n] == ':') n++;
	}
	p->rest= s + n;
	{
		const char *q= p->rest;
		int k= 0;
		while (*q == ' ' || *q == '\t') q++;
		if (*q != ';') {
			while (*q && *q != ' ' && *q != '\t' && *q != ';' && k < 63) p->op[k++]= *q++;
			p->op[k]= 0;
		}
		p->args= q;
	}
}

/* the names in a piece of line: identifiers, not in a comment or a quote, not hex numbers */
static void scan_tokens(const char *p, strvec *out) {
	char tok[128];
	while (*p) {
		if (*p == ';') break;
		if (*p == '\'' || *p == '"') {
			char q= *p++;
			while (*p && *p != q) p++;
			if (*p) p++;
			continue;
		}
		if (*p == '$' || *p == '%') {
			p++;
			while (isxdigit((unsigned char)*p)) p++;
			continue;
		}
		if (isdigit((unsigned char)*p)) {
			while (isalnum((unsigned char)*p)) p++;
			continue;
		}
		if (ident_start((unsigned char)*p)) {
			int n= 0;
			while (ident_char((unsigned char)*p) && n < 127) tok[n++]= *p++;
			tok[n]= 0;
			sv_add(out, tok);
			continue;
		}
		p++;
	}
}

static int op_is(const char *op, const char *a) { return !strcasecmp(op, a); }

static int is_glue(const char *op) {
	static const char *g[]= { ".if", ".ifdef", ".ifndef", ".elif", ".else", ".endif", ".macro", ".endm",
		".const", ".map", ".org", ".title", ".run", ".include", ".end", NULL };
	int i;
	for (i= 0; g[i]; i++) if (op_is(op, g[i])) return 1;
	return 0;
}

static int is_data(const char *op) {
	static const char *d[]= { ".byte", ".word", ".bzero", ".alloc", ".ds", ".fill", ".dw", ".db", ".text", NULL };
	int i;
	for (i= 0; d[i]; i++) if (op_is(op, d[i])) return 1;
	return 0;
}

static int is_terminator(const char *op) {
	return op_is(op, "rts") || op_is(op, "jmp") || op_is(op, "rti") || op_is(op, "brk");
}

/* ---- reading the text, with the includes copied in ------------------------------------------ */

static void load_lines(FILE *f, int depth) {
	char *buf= NULL;
	size_t cap= 0;
	ssize_t n;

	while ((n= getline(&buf, &cap, f)) >= 0) {
		const char *p= buf;
		while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n]= 0;
		while (*p == ' ' || *p == '\t') p++;
		if (!strncasecmp(p, ".include", 8) && (p[8] == ' ' || p[8] == '\t') && depth < 4) {
			const char *q= strchr(p, '"');
			const char *e= q? strchr(q + 1, '"'): NULL;
			if (q && e) {
				char name[512];
				FILE *g;
				snprintf(name, sizeof name, "%.*s", (int)(e - q - 1), q + 1);
				if ((g= lib_open(name))) {
					char c[600];
					snprintf(c, sizeof c, "; ---- %s", name);
					sv_add(&lines, c);
					load_lines(g, depth + 1);
					fclose(g);
					continue;
				}
			}
		}
		sv_add(&lines, buf);
	}
	free(buf);
}

/* ---- the work ---------------------------------------------------------------------------- */

static int *keep;
static int *stack, nstack;

static void process_tokens(strvec *t);

static void mark(int g) {
	if (keep[g]) return;
	keep[g]= 1;
	stack[nstack++]= g;
}

static void process_tokens(strvec *t) {
	int i, m;
	char up[200];
	lab *l;

	for (i= 0; i < t->n; i++) {
		snprintf(up, sizeof up, "%s", t->v[i]); upcase(up);
		if (up[0] == '_') {
			for (m= 0; m < nmacros; m++)
				if (!strcmp(macros[m].name, up + 1)) {
					if (!macros[m].seen) { macros[m].seen= 1; process_tokens(&macros[m].toks); }
					break;
				}
		}
		if ((l= lab_find(up, 0))) {
			int k;
			for (k= 0; k < l->nu; k++) mark(units[l->u[k]].group);
		}
	}
}

int prune_output(FILE *in, FILE *out, int verbose) {
	int i, u, g, ndrop= 0, nunitkept= 0;
	int mode= M_ROOT, cur= -1, expl= 0, inmacro= -1, nomerge= 0;
	pline p;

	load_lines(in, 0);
	lu= malloc((lines.n + 1) * sizeof(int));

	/* cut the text into units */
	for (i= 0; i < lines.n; i++) {
		const char *s= lines.v[i];
		int blank;

		if (s[0] == ';' && s[1] == '@') {
			const char *a= s + 2;
			lu[i]= -2;
			if (!strncmp(a, "unit", 4)) { cur= new_unit(mode, 1); expl= 1; }
			else if (!strncmp(a, "endunit", 7)) { cur= -1; expl= 0; }
			else if (!strncmp(a, "mode", 4)) {
				a+= 4; while (*a == ' ') a++;
				mode= !strncmp(a, "prog", 4)? M_PROG: !strncmp(a, "lib", 3)? M_LIB: M_ROOT;
				cur= -1; expl= 0;
			}
			else if (!strncmp(a, "break", 5)) { cur= -1; nomerge= 1; }
			continue;
		}
		parse_line(s, &p);
		blank= (p.op[0] == 0);

		if (inmacro >= 0) {
			lu[i]= -1;
			if (op_is(p.op, ".endm")) inmacro= -1;
			else scan_tokens(p.has_label? p.rest: s, &macros[inmacro].toks);
			continue;
		}
		if (op_is(p.op, ".macro")) {
			const char *a= p.args;
			int k= 0;
			if (nmacros == cmacros) { cmacros= cmacros? cmacros * 2: 64; macros= realloc(macros, cmacros * sizeof(macro)); }
			memset(&macros[nmacros], 0, sizeof(macro));
			while (*a == ' ' || *a == '\t') a++;
			while (ident_char((unsigned char)*a) && k < 63) macros[nmacros].name[k++]= *a++;
			upcase(macros[nmacros].name);
			inmacro= nmacros++;
			lu[i]= -1;
			continue;
		}
		if (is_glue(p.op)) {
			lu[i]= -1;
			scan_tokens(p.args, &roottoks);
			if (cur >= 0 && (op_is(p.op, ".else") || op_is(p.op, ".elif") || op_is(p.op, ".endif")))
				units[cur].cond_after= 1;
			continue;
		}
		if (expl) {				/* a routine of the compiler: everything up to ;@endunit */
			lu[i]= cur;
			if (p.has_label) { sv_add(&units[cur].labs, p.label); lab_define(p.label, cur); }
			scan_tokens(p.has_label? p.rest: s, &units[cur].toks);
			continue;
		}
		if (mode == M_ROOT) {
			lu[i]= -1;
			scan_tokens(p.has_label? p.rest: s, &roottoks);
			continue;
		}
		if (p.has_label) {
			cur= new_unit(mode, 0);
			units[cur].nomerge= nomerge; nomerge= 0;
			sv_add(&units[cur].labs, p.label);
			lab_define(p.label, cur);
		}
		if (cur < 0) {				/* before the first label of a region, or after a break */
			lu[i]= -1;
			if (!blank) scan_tokens(s, &roottoks);
			continue;
		}
		lu[i]= cur;
		if (blank) continue;
		scan_tokens(p.has_label? p.rest: s, &units[cur].toks);
		units[cur].cond_after= 0;
		if (is_data(p.op)) { units[cur].has_data= 1; units[cur].last_kind= 2; }
		else { units[cur].has_code= 1; units[cur].last_kind= 1; snprintf(units[cur].last_op, sizeof units[cur].last_op, "%s", p.op); }
	}

	/* does a unit run on into the next one? */
	for (u= 0; u < nunits; u++) {
		unit *x= &units[u];
		if (x->expl) { x->falls= 0; continue; }
		if (x->last_kind == 2) x->falls= 0;
		else if (x->last_kind == 0) x->falls= 1;
		else x->falls= !is_terminator(x->last_op);
		if (x->falls && x->cond_after && x->mode == M_LIB)
			fprintf(stderr, "prune: warning: %s runs on across a conditional assembly boundary\n", x->labs.n? x->labs.v[0]: "?");
	}

	/* neighbouring data units of the library stay together */
	for (u= 0; u < nunits; u++) units[u].glast= u;
	for (u= 1; u < nunits; u++) {
		unit *x= &units[u], *y= &units[u-1];
		if (x->mode == M_LIB && y->mode == M_LIB && !x->expl && !y->expl && !x->nomerge &&
		    x->has_data && !x->has_code && y->has_data && !y->has_code) {
			x->group= y->group;
			units[x->group].glast= u;
		}
	}

	/* what can be reached */
	keep= calloc(nunits + 1, sizeof(int));
	stack= malloc((nunits + 1) * sizeof(int));
	nstack= 0;
	process_tokens(&roottoks);
	while (nstack > 0) {
		g= stack[--nstack];
		for (u= g; u <= units[g].glast; u++) {
			process_tokens(&units[u].toks);
			if (units[u].falls && u + 1 < nunits) mark(units[u+1].group);
		}
	}

	/* the output */
	for (i= 0; i < lines.n; i++) {
		if (lu[i] == -2) continue;
		if (lu[i] >= 0 && !keep[units[lu[i]].group]) continue;
		fputs(lines.v[i], out);
		fputc('\n', out);
	}

	for (u= 0; u < nunits; u++) {
		if (keep[units[u].group]) { nunitkept++; continue; }
		ndrop++;
		if (verbose && units[u].labs.n) {
			if (units[u].expl || (units[u].mode == M_PROG && !strncmp(units[u].labs.v[0], "R_", 2)))
				fprintf(stderr, "removed %s\n", units[u].labs.v[0]);
		}
	}
	if (verbose) {
		int dr= 0, kr= 0;
		for (u= 0; u < nunits; u++)
			if (units[u].expl) { if (keep[units[u].group]) kr++; else dr++; }
		fprintf(stderr, "prune: %d of %d routines and %d of %d other units removed\n", dr, dr + kr,
			ndrop - dr, nunits - dr - kr);
	}
	return 0;
}
