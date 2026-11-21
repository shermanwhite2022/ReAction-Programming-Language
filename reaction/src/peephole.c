/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
/* peephole.c - a pass over the code the compiler wrote, before the dead code removal
 *
 * The emitter is a one pass, stack machine style generator: every operand is pushed on the run time
 * stack (_PUSHI, _PUSH8, _PUSH16), an operator pops two and pushes the result (jsr ADD16S ...), and an
 * assignment pops the result (_POP16, _POP8).  That costs 50 to 60 cycles per push or pop, so x=x+1 took
 * over 300 cycles.  This pass reads the text of each routine (between ;@unit and ;@endunit) and keeps
 * the operands of the current expression "deferred", as a small model of the top of the stack, instead
 * of pushing them:
 *
 *   - _PUSHI, _PUSH8 and _PUSH16 only record the operand (a constant or a variable)
 *   - jsr ADD16S, SUB16S, AND16S, OR16S, XOR16S on two operands record the operation, and nothing is
 *     generated until the value is used
 *   - comparisons followed by _JZ become a compare and a branch
 *   - _POP16 and _POP8 store the value straight into the variable (x=x+1 becomes inc)
 *   - a call whose arguments are all deferred stores them into the parameter variables of the routine
 *     and enters it after its own pops (every routine that begins with pops gets a second entry label
 *     NAME__e there).  This is possible because there is no recursion: parameters are static.
 *   - any other line (a label, a jump, a call, a macro this pass does not know ...) first pushes what is
 *     deferred, in order, and is then copied, so the code means exactly what it meant before.
 *
 * The deferred values are read when they are used, so nothing that could change a variable may be
 * between the push and the use: every line that is not understood flushes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#define MAXD 4

enum { O_IMM, O_V16, O_V8, O_TA };			/* operand: constant, CARD/INT variable, BYTE variable, the value in TMPA */
typedef struct { int k; char s[160]; long v; int num; } Opd;

enum { I_LEAF, I_BIN, I_CMP };
typedef struct {
	int k;
	Opd l, r;			/* l is the value of a leaf; l and r the operands of an operation */
	int op;				/* BIN: + - & | ^   CMP: e n < g (>=) > l (<=) */
	int sg;				/* CMP: signed */
	int t8;				/* BIN: the result is cut to 8 bits */
} Item;

static Item ds[MAXD + 2];
static int  nd;
static FILE *O;
static int  phn;			/* labels PH00001 ... */

typedef struct { char label[64]; int n; int size[8]; char var[8][96]; } Pro;
static Pro  pros[2000];
static int  npro;

/* ---- text helpers -------------------------------------------------------------------------- */

static void E(const char *fmt, ...) {
	va_list ap;
	fputs("\t\t", O);
	va_start(ap, fmt);
	vfprintf(O, fmt, ap);
	va_end(ap);
	fputc('\n', O);
}

static void LAB(int n) { fprintf(O, "PH%05d\n", n); }
static int  newph(void) { return ++phn; }

static int parse_num(const char *s, long *v) {
	char *end;
	if (s[0] == '$') { if (!s[1]) return 0; *v = strtol(s + 1, &end, 16); return *end == 0; }
	if (isdigit((unsigned char) s[0]) || (s[0] == '-' && isdigit((unsigned char) s[1]))) { *v = strtol(s, &end, 10); return *end == 0; }
	return 0;
}

static Opd mk(int k, const char *s) {
	Opd o;
	memset(&o, 0, sizeof o);
	o.k = k;
	snprintf(o.s, sizeof o.s, "%s", s);
	if (k == O_IMM) { o.num = parse_num(s, &o.v); if (o.num) o.v &= 0xffff; }
	return o;
}

static const char *lo(const Opd *o) {
	static char b[8][200]; static int n;
	char *r = b[n++ & 7];
	switch (o->k) {
	case O_IMM: if (o->num) snprintf(r, 200, "#%ld", o->v & 255); else snprintf(r, 200, "#<%s", o->s); break;
	case O_TA:  strcpy(r, "TMPA"); break;
	default:    snprintf(r, 200, "%s", o->s);
	}
	return r;
}

static const char *hi(const Opd *o) {
	static char b[8][200]; static int n;
	char *r = b[n++ & 7];
	switch (o->k) {
	case O_IMM: if (o->num) snprintf(r, 200, "#%ld", (o->v >> 8) & 255); else snprintf(r, 200, "#>%s", o->s); break;
	case O_V8:  strcpy(r, "#0"); break;
	case O_TA:  strcpy(r, "TMPA+1"); break;
	default:    snprintf(r, 200, "%s+1", o->s);
	}
	return r;
}

static int hizero(const Opd *o) { return o->k == O_V8 || (o->k == O_IMM && o->num && (o->v & 0xff00) == 0); }

/* ---- code for the operations --------------------------------------------------------------- */

/* the value of operation it, into dn (a variable or TMPA), 1 or 2 bytes */
static void gen_bin(const Item *it, int dsize, const char *dn) {
	const Opd *L = &it->l, *R = &it->r;
	int op = it->op;
	const char *opc = (op == '+')? "adc": (op == '-')? "sbc": (op == '&')? "and": (op == '|')? "ora": "eor";
	int self = (strcmp(dn, "TMPA") != 0 && (L->k == O_V16 || L->k == O_V8) && !strcmp(L->s, dn));

	if (self && (op == '+' || op == '-') && R->k == O_IMM && R->num && R->v == 1 && (dsize == 1 || L->k == O_V16)) {
		if (dsize == 1) { E("%s %s", op == '+'? "inc": "dec", dn); return; }
		if (op == '+') { int p = newph(); E("inc %s", dn); E("bne PH%05d", p); E("inc %s+1", dn); LAB(p); }
		else { int p = newph(); E("lda %s", dn); E("bne PH%05d", p); E("dec %s+1", dn); LAB(p); E("dec %s", dn); }
		return;
	}
	if (self && dsize == 2 && L->k == O_V16 && (op == '+' || op == '-') && hizero(R)) {
		int p = newph();
		E(op == '+'? "clc": "sec");
		E("lda %s", dn);
		E("%s %s", opc, lo(R));
		E("sta %s", dn);
		E(op == '+'? "bcc PH%05d": "bcs PH%05d", p);
		E(op == '+'? "inc %s+1": "dec %s+1", dn);
		LAB(p);
		return;
	}
	if (op == '+') E("clc"); else if (op == '-') E("sec");
	E("lda %s", lo(L));
	E("%s %s", opc, lo(R));
	E("sta %s", dn);
	if (dsize == 2) {
		if (it->t8 || (op == '&' && (hizero(L) || hizero(R)))) { E("lda #0"); E("sta %s+1", dn); }
		else if (op == '|' && hizero(R)) { E("lda %s", hi(L)); E("sta %s+1", dn); }
		else if (op == '|' && hizero(L)) { E("lda %s", hi(R)); E("sta %s+1", dn); }
		else if (op == '^' && hizero(R)) { E("lda %s", hi(L)); E("sta %s+1", dn); }
		else if (op == '^' && hizero(L)) { E("lda %s", hi(R)); E("sta %s+1", dn); }
		else { E("lda %s", hi(L)); E("%s %s", opc, hi(R)); E("sta %s+1", dn); }
	}
}

/* store a leaf */
static void gen_store(const Opd *o, int dsize, const char *dn) {
	if (o->k != O_IMM && o->k != O_TA && !strcmp(o->s, dn) && (dsize == 1 || o->k == O_V16)) return;	/* x = x */
	E("lda %s", lo(o));
	E("sta %s", dn);
	if (dsize == 2) {
		if (o->k == O_IMM && o->num && (o->v & 0xff) == ((o->v >> 8) & 0xff)) E("sta %s+1", dn);
		else { E("lda %s", hi(o)); E("sta %s+1", dn); }
	}
}

static void gen_item_to(const Item *it, int dsize, const char *dn) {
	if (it->k == I_BIN) gen_bin(it, dsize, dn);
	else gen_store(&it->l, dsize, dn);
}

/* the compare of a CMP item, then a jump to target if the comparison is false */
static void gen_cmp_jz(const Item *it, const char *target) {
	Opd L = it->l, R = it->r, T;
	int op = it->op;
	int ok = newph(), go = newph();
	int needhi = !(hizero(&L) && hizero(&R));

	if (op == '>') { T = L; L = R; R = T; op = '<'; }	/* L > R is R < L */
	else if (op == 'l') { T = L; L = R; R = T; op = 'g'; }	/* L <= R is R >= L */
	needhi = !(hizero(&L) && hizero(&R));

	if (op == 'e') {
		E("lda %s", lo(&L)); E("cmp %s", lo(&R));
		if (needhi) { E("bne PH%05d", go); E("lda %s", hi(&L)); E("cmp %s", hi(&R)); E("beq PH%05d", ok); LAB(go); }
		else E("beq PH%05d", ok);
		E("jmp %s", target); LAB(ok);
	} else if (op == 'n') {
		E("lda %s", lo(&L)); E("cmp %s", lo(&R)); E("bne PH%05d", ok);
		if (needhi) { E("lda %s", hi(&L)); E("cmp %s", hi(&R)); E("bne PH%05d", ok); }
		E("jmp %s", target); LAB(ok);
	} else if (!it->sg) {
		E("lda %s", lo(&L)); E("cmp %s", lo(&R));
		if (needhi) { E("lda %s", hi(&L)); E("sbc %s", hi(&R)); }
		E(op == '<'? "bcc PH%05d": "bcs PH%05d", ok);
		E("jmp %s", target); LAB(ok);
	} else {
		int v = newph();
		E("lda %s", lo(&L)); E("cmp %s", lo(&R)); E("lda %s", hi(&L)); E("sbc %s", hi(&R));
		E("bvc PH%05d", v); E("eor #$80"); LAB(v);
		E(op == '<'? "bmi PH%05d": "bpl PH%05d", ok);
		E("jmp %s", target); LAB(ok);
	}
}

/* ---- the deferred stack -------------------------------------------------------------------- */

static void push_leaf_phys(const Opd *o) {
	switch (o->k) {
	case O_IMM: E("_PUSHI %s", o->s); break;
	case O_V16: E("_PUSH16 %s", o->s); break;
	case O_V8:  E("_PUSH8 %s", o->s); break;
	default:    E("jsr PUSHA");
	}
}

static const char *cmpname(const Item *it) {
	switch (it->op) {
	case 'e': return "EQ16S";
	case 'n': return "NE16S";
	case '<': return it->sg? "LT16S": "LT16U";
	case 'g': return it->sg? "GE16S": "GE16U";
	case '>': return it->sg? "GT16S": "GT16U";
	default:  return it->sg? "LE16S": "LE16U";
	}
}

static void flush_item(const Item *it) {
	if (it->k == I_LEAF) push_leaf_phys(&it->l);
	else if (it->k == I_BIN) { gen_bin(it, 2, "TMPA"); E("jsr PUSHA"); }
	else { push_leaf_phys(&it->l); push_leaf_phys(&it->r); E("jsr %s", cmpname(it)); }
}

/* push the lowest cnt items for real */
static void flush_prefix(int cnt) {
	int i;
	for (i = 0; i < cnt; i++) flush_item(&ds[i]);
	memmove(&ds[0], &ds[cnt], (nd - cnt) * sizeof(Item));
	nd -= cnt;
}

static void flush_all(void) { flush_prefix(nd); }

static int is_tmpa_user(const Item *it) {
	return it->k == I_BIN || it->k == I_CMP || (it->k == I_LEAF && it->l.k == O_TA)
	    || (it->k != I_LEAF && (it->l.k == O_TA || it->r.k == O_TA));
}

/* TMPA is going to hold a value for item idx: everything below it is pushed for real first, because pushing
 * (which goes through TMPA) can not happen while TMPA is holding a value */
static void clean_below(int idx) {
	if (idx > 0) flush_prefix(idx);
}

static void add_leaf(Opd o) {
	if (nd >= MAXD) flush_prefix(1);
	ds[nd].k = I_LEAF; ds[nd].l = o; ds[nd].t8 = 0;
	nd++;
}

/* the line is an operator that makes two operands into one.  Returns 1 if it was taken care of. */
static int binop(int op) {
	Item n;
	Item *a, *b;

	if (nd < 2) return 0;
	a = &ds[nd - 2]; b = &ds[nd - 1];
	if (a->k == I_CMP || b->k == I_CMP) return 0;
	if (a->k == I_BIN && b->k == I_BIN) return 0;
	if (a->k == I_BIN || b->k == I_BIN) {
		/* one of them is an operation: its value goes into TMPA first, so TMPA has to be free for it */
		int idx = (a->k == I_BIN)? nd - 2: nd - 1;
		Item *other = (idx == nd - 2)? b: a;
		Item *x;
		if (other->l.k == O_TA) return 0;
		clean_below(nd - 2);			/* pushes the items below the pair; the pair stays on top */
		x = (ds[nd - 2].k == I_BIN)? &ds[nd - 2]: &ds[nd - 1];
		gen_bin(x, 2, "TMPA");
		x->k = I_LEAF; x->l = mk(O_TA, "TMPA"); x->t8 = 0;
		a = &ds[nd - 2]; b = &ds[nd - 1];
	}
	if (a->l.k == O_TA && b->l.k == O_TA) return 0;
	memset(&n, 0, sizeof n);
	n.k = I_BIN; n.op = op; n.l = a->l; n.r = b->l;
	nd -= 2;
	ds[nd++] = n;
	return 1;
}

static int cmpop(const char *nm, int *op, int *sg) {
	static const struct { const char *n; int op, sg; } t[] = {
		{"EQ16S", 'e', 0}, {"NE16S", 'n', 0},
		{"LT16S", '<', 1}, {"GE16S", 'g', 1}, {"GT16S", '>', 1}, {"LE16S", 'l', 1},
		{"LT16U", '<', 0}, {"GE16U", 'g', 0}, {"GT16U", '>', 0}, {"LE16U", 'l', 0}
	};
	int i;
	for (i = 0; i < 10; i++) if (!strcmp(nm, t[i].n)) { *op = t[i].op; *sg = t[i].sg; return 1; }
	return 0;
}

static int cmpmake(int op, int sg) {
	Item *a, *b, n;
	if (nd < 2) return 0;
	a = &ds[nd - 2]; b = &ds[nd - 1];
	if (a->k != I_LEAF || b->k != I_LEAF) return 0;
	if (b->l.k == O_TA) return 0;
	memset(&n, 0, sizeof n);
	n.k = I_CMP; n.op = op; n.sg = sg; n.l = a->l; n.r = b->l;
	nd -= 2;
	ds[nd++] = n;
	return 1;
}

/* ---- the text ------------------------------------------------------------------------------ */

static char **L; static int nl, capl;

static void addline(const char *s, size_t n) {
	if (nl == capl) { capl = capl? capl * 2: 4096; L = realloc(L, capl * sizeof(char *)); }
	L[nl] = malloc(n + 1); memcpy(L[nl], s, n); L[nl][n] = 0; nl++;
}

/* split a line: *tok the first word, arg the rest without its comment */
static void split(const char *s, char *tok, char *arg) {
	const char *p = s;
	int n = 0;
	char *c;
	while (*p == ' ' || *p == '\t') p++;
	while (*p && !isspace((unsigned char) *p) && n < 63) tok[n++] = *p++;
	tok[n] = 0;
	while (*p == ' ' || *p == '\t') p++;
	strncpy(arg, p, 190); arg[190] = 0;
	if ((c = strchr(arg, ';'))) *c = 0;
	n = (int) strlen(arg);
	while (n > 0 && (arg[n-1] == ' ' || arg[n-1] == '\t' || arg[n-1] == '\r' || arg[n-1] == '\n')) arg[--n] = 0;
}

static int blank_or_comment(const char *s) {
	while (*s == ' ' || *s == '\t') s++;
	return *s == 0 || *s == '\n' || *s == ';';
}

static Pro *find_pro(const char *label) {
	int i;
	for (i = 0; i < npro; i++) if (!strcmp(pros[i].label, label)) return &pros[i];
	return NULL;
}

static void collect_pros(void) {
	int i, j, inunit = 0;
	char tok[64], arg[200];
	for (i = 0; i < nl; i++) {
		if (!strncmp(L[i], ";@unit", 6)) { inunit = 1; continue; }
		if (!strncmp(L[i], ";@endunit", 9)) { inunit = 0; continue; }
		if (!inunit || blank_or_comment(L[i]) || isspace((unsigned char) L[i][0]) || L[i][0] == '.') continue;
		split(L[i], tok, arg);
		if (strncmp(tok, "R_", 2) || npro >= 2000) continue;
		{
			Pro *p = &pros[npro];
			p->n = 0;
			snprintf(p->label, sizeof p->label, "%s", tok);
			for (j = i + 1; j < nl; j++) {
				char t2[64], a2[200];
				if (blank_or_comment(L[j])) continue;
				split(L[j], t2, a2);
				if (p->n < 8 && (!strcmp(t2, "_POP16") || !strcmp(t2, "_POP8")) && a2[0] && !strchr(a2, ' ')) {
					p->size[p->n] = !strcmp(t2, "_POP16")? 2: 1;
					snprintf(p->var[p->n], sizeof p->var[0], "%s", a2);
					p->n++;
				} else break;
			}
			if (p->n > 0) npro++;
		}
	}
}

void peephole(FILE *in, FILE *out) {
	char buf[4096];
	int i, inunit = 0;
	Pro *cur = NULL;

	O = out;
	nl = 0; nd = 0; npro = 0;
	while (fgets(buf, sizeof buf, in)) addline(buf, strlen(buf));
	collect_pros();

	for (i = 0; i < nl; i++) {
		const char *line = L[i];
		char tok[64], arg[200];

		if (!strncmp(line, ";@unit", 6)) { flush_all(); inunit = 1; fputs(line, O); continue; }
		if (!strncmp(line, ";@endunit", 9)) { flush_all(); inunit = 0; cur = NULL; fputs(line, O); continue; }
		if (!inunit || blank_or_comment(line)) { fputs(line, O); continue; }

		split(line, tok, arg);

		if (!isspace((unsigned char) line[0]) && line[0] != '.') {		/* a label */
			flush_all();
			fputs(line, O);
			cur = find_pro(tok);
			continue;
		}

		if (cur) {
			if (!strcmp(tok, "_POP16") || !strcmp(tok, "_POP8")) { fputs(line, O); continue; }
			fprintf(O, "%s__e\n", cur->label);
			cur = NULL;
		}

		if (!strcmp(tok, "_PUSHI") && arg[0]) { add_leaf(mk(O_IMM, arg)); continue; }
		if (!strcmp(tok, "_PUSH16") && arg[0]) { add_leaf(mk(O_V16, arg)); continue; }
		if (!strcmp(tok, "_PUSH8") && arg[0]) { add_leaf(mk(O_V8, arg)); continue; }

		if (!strcmp(tok, "jsr")) {
			int op, sg;
			if (!strcmp(arg, "ADD16S") && binop('+')) continue;
			if (!strcmp(arg, "SUB16S") && binop('-')) continue;
			if (!strcmp(arg, "AND16S") && binop('&')) continue;
			if (!strcmp(arg, "OR16S")  && binop('|')) continue;
			if (!strcmp(arg, "XOR16S") && binop('^')) continue;
			if (cmpop(arg, &op, &sg) && cmpmake(op, sg)) continue;
			if (!strncmp(arg, "R_", 2) && !strchr(arg, '+')) {
				Pro *p = find_pro(arg);
				if (p && nd >= p->n) {
					int k, base = nd - p->n;
					int ok = 1;
					for (k = base; k < nd; k++) if (ds[k].k == I_CMP) ok = 0;
					if (ok) {
						/* store the arguments (the one on top is the first one the routine pops) */
						for (k = 0; k < p->n; k++) gen_item_to(&ds[nd - 1 - k], p->size[k], p->var[k]);
						nd = base;
						flush_all();
						E("jsr %s__e", arg);
						continue;
					}
				}
			}
		}

		if (!strcmp(tok, "_TRUNC8") && nd >= 1) {
			Item *t = &ds[nd - 1];
			if (t->k == I_LEAF) {
				if (t->l.k == O_IMM && t->l.num) { t->l.v &= 0xff; snprintf(t->l.s, sizeof t->l.s, "%ld", t->l.v); }
				else if (t->l.k == O_V16) t->l.k = O_V8;
				else if (t->l.k == O_TA) { E("lda #0"); E("sta TMPA+1"); }
				else if (t->l.k == O_IMM) goto barrier;
				continue;
			}
			if (t->k == I_BIN) { t->t8 = 1; continue; }
			goto barrier;
		}

		if ((!strcmp(tok, "_POP16") || !strcmp(tok, "_POP8")) && nd >= 1 && arg[0] && !strchr(arg, ' ')) {
			Item *t = &ds[nd - 1];
			int size = !strcmp(tok, "_POP16")? 2: 1;
			if (t->k != I_CMP) {
				gen_item_to(t, size, arg);
				nd--;
				continue;
			}
		}

		if (!strcmp(tok, "_JZ") && nd >= 1 && arg[0]) {
			Item *t = &ds[nd - 1];
			if (t->k == I_CMP) { gen_cmp_jz(t, arg); nd--; continue; }
			if (t->k == I_BIN) {
				clean_below(nd - 1);
				t = &ds[nd - 1];
				gen_bin(t, 2, "TMPA");
				t->k = I_LEAF; t->l = mk(O_TA, "TMPA"); t->t8 = 0;
			}
			if (t->k == I_LEAF) {
				int p = newph();
				Opd *o = &t->l;
				if (o->k == O_IMM && o->num) { if ((o->v & 0xffff) == 0) E("jmp %s", arg); nd--; continue; }
				E("lda %s", lo(o));
				if (o->k == O_V16 || o->k == O_TA || o->k == O_IMM) E("ora %s", hi(o));
				E("bne PH%05d", p); E("jmp %s", arg); LAB(p);
				nd--; continue;
			}
		}

barrier:
		flush_all();
		fputs(line, O);
	}
	flush_all();
	while (nl > 0) free(L[--nl]);
}
