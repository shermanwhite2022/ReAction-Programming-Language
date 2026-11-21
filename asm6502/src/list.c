/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm.h"

// Listing file (-l file).
//
// During the second pass every write to memory is noted, together with the
// file and line it came from, and so is every .include. After a successful
// assembly the listing is made by reading the source files again and putting
// the bytes next to the line that produced them. Lines that were skipped
// (a false .if) are shown without any bytes, and a file that was included is
// shown where the .include line is.

#define EV_BYTES	0
#define EV_ENTER	1
#define EV_EXIT		2

#define KEEP		16		// bytes kept for one write (a long .bzero is cut short)
#define PER_ROW		4		// bytes shown on one row of the listing

typedef struct {
	int		type;
	const char	*file;		// the file of the line (EV_ENTER: the file that was entered)
	int		line;			// the line (EV_ENTER: the line of the .include)
	int		pc;
	int		len;			// bytes written
	int		kept;			// bytes stored in data
	unsigned char	data[KEEP];
} EVENT;

static EVENT	*_ev = NULL;
static int	_nev = 0;
static int	_capev = 0;
static int	_on = 0;

void listEnable (void) {
	_on = 1;
}

static EVENT *newEvent (int type) {
	if (_nev >= _capev) {
		_capev = _capev ? _capev * 2 : 256;
		_ev = (EVENT *) realloc(_ev, _capev * sizeof(EVENT));
	}

	memset(&_ev[_nev], 0, sizeof(EVENT));
	_ev[_nev].type = type;
	return &_ev[_nev++];
}

// only the second pass is listed, and only when a listing was asked for

static int recording (void) {
	return _on && _pass == 1;
}

void listBytes (int pc, int len) {
	EVENT *e;

	if (!recording() || len <= 0)
		return;

	// bytes that follow the previous write of the same line share its rows
	if (_nev > 0) {
		EVENT *p = &_ev[_nev-1];

		if (p->type == EV_BYTES && p->file == _filename && p->line == _lineno &&
		    p->pc + p->len == pc && p->kept == p->len && p->len < KEEP) {
			int n = len < KEEP - p->len ? len : KEEP - p->len;

			memcpy(p->data + p->kept, &_membuf[pc], n);
			p->kept += n;
			p->len += len;
			return;
		}
	}

	e = newEvent(EV_BYTES);
	e->file = _filename;
	e->line = _lineno;
	e->pc = pc;
	e->len = len;
	e->kept = len < KEEP ? len : KEEP;
	memcpy(e->data, &_membuf[pc], e->kept);
}

void listEnter (const char *file, int includeLine) {
	EVENT *e;

	if (!recording())
		return;

	e = newEvent(EV_ENTER);
	e->file = file;
	e->line = includeLine;
}

void listExit (void) {
	if (recording())
		newEvent(EV_EXIT);
}

// ----------------------------------------------------------------- writing

typedef struct {
	const char	*name;
	char		**line;		// the lines of the file, without their newlines
	int		count;
	int		next;			// the next line that has not been written yet (0 based)
} FRAME;

static FRAME	_stack[64];
static int	_top = -1;

static int readFile (FRAME *f, const char *name) {
	FILE *fp = fopen(name, "rb");
	char *text;
	long size;
	long i;
	int n = 0;

	memset(f, 0, sizeof(*f));
	f->name = name;

	if (!fp)
		return -1;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	text = (char *) malloc(size + 2);
	size = (long) fread(text, 1, size, fp);
	fclose(fp);
	text[size] = 0;

	for (i = 0; i < size; i++)
		if (text[i] == '\n')
			n++;

	if (size && text[size-1] != '\n')
		n++;

	f->line = (char **) calloc(n + 1, sizeof(char *));

	for (i = 0, n = 0; i < size; n++) {
		char *start = text + i;

		while (i < size && text[i] != '\n')
			i++;

		text[i] = 0;

		if (i > 0 && text[i-1] == '\r')
			text[i-1] = 0;

		f->line[n] = start;
		i++;
	}

	f->count = n;
	return 0;
}

// one row of the listing: line number (0 for none), address and bytes (rows
// with no bytes leave them blank), then the text of the line

static void row (FILE *out, int lineno, int havePc, int pc, const unsigned char *b, int nb, const char *text) {
	char bytes[PER_ROW * 3 + 1];
	int i;

	bytes[0] = 0;

	for (i = 0; i < nb; i++)
		sprintf(bytes + i * 3, "%02x ", b[i]);

	if (lineno)
		fprintf(out, "%5d  ", lineno);
	else
		fprintf(out, "       ");

	if (havePc)
		fprintf(out, "%04x  ", pc);
	else
		fprintf(out, "      ");

	fprintf(out, "%-*s", PER_ROW * 3, bytes);

	if (text)
		fprintf(out, " %s", text);

	fprintf(out, "\n");
}

// write the source lines of the file on top of the stack, up to but not
// including line number "upto" (1 based)

static void catchUp (FILE *out, int upto) {
	FRAME *f = &_stack[_top];

	while (f->next < f->count && f->next + 1 < upto) {
		row(out, f->next + 1, 0, 0, NULL, 0, f->line[f->next]);
		f->next++;
	}
}

static void finishFrame (FILE *out) {
	FRAME *f = &_stack[_top];

	while (f->next < f->count) {
		row(out, f->next + 1, 0, 0, NULL, 0, f->line[f->next]);
		f->next++;
	}
}

static void pushFile (FILE *out, const char *name) {
	if (_top + 1 >= (int) (sizeof(_stack) / sizeof(_stack[0])))
		return;

	_top++;

	if (readFile(&_stack[_top], name)) {
		fprintf(out, "       ; (could not read %s again)\n", name);
	}
}

// the bytes of one write, as rows of PER_ROW. The first row goes with the
// text of the line if it has not been written yet.

static void writeBytes (FILE *out, EVENT *e, int *textDone, int lineno, const char *text) {
	int i;
	int n;

	for (i = 0; i < e->kept; i += PER_ROW) {
		n = e->kept - i < PER_ROW ? e->kept - i : PER_ROW;

		if (!*textDone) {
			row(out, lineno, 1, e->pc + i, e->data + i, n, text);
			*textDone = 1;
		} else {
			row(out, 0, 1, e->pc + i, e->data + i, n, NULL);
		}
	}

	if (e->len > e->kept)
		fprintf(out, "       %04x  ... %d more bytes\n", e->pc + e->kept, e->len - e->kept);
}

int writeListing (const char *path, const char *mainFile, int showSymbols) {
	FILE *out = fopen(path, "w");
	int i;
	FRAME *f;

	if (!out)
		return -1;

	fprintf(out, "%s\n", mainFile);
	fprintf(out, "line  addr  bytes        source\n");

	_top = -1;
	pushFile(out, mainFile);

	for (i = 0; i < _nev; i++) {
		EVENT *e = &_ev[i];

		if (_top < 0)
			break;

		f = &_stack[_top];

		if (e->type == EV_ENTER) {
			catchUp(out, e->line);

			if (f->next + 1 == e->line && e->line <= f->count) {
				row(out, e->line, 0, 0, NULL, 0, f->line[f->next]);
				f->next++;
			}

			fprintf(out, "       ; ---- %s\n", e->file);
			pushFile(out, e->file);
		} else if (e->type == EV_EXIT) {
			finishFrame(out);
			fprintf(out, "       ; ---- end of %s\n", f->name);
			_top--;
		} else {
			const char *text = e->line >= 1 && e->line <= f->count ? f->line[e->line-1] : "";
			int textDone;

			catchUp(out, e->line);

			// the first write from a line also writes the text of the line
			textDone = f->next + 1 != e->line;
			writeBytes(out, e, &textDone, e->line, text);

			if (f->next + 1 == e->line)
				f->next = e->line;
		}
	}

	while (_top >= 0) {
		finishFrame(out);

		if (_top > 0)
			fprintf(out, "       ; ---- end of %s\n", _stack[_top].name);

		_top--;
	}

	if (showSymbols) {
		fprintf(out, "\n");
		dumpSymbols(out);
	}

	fclose(out);
	return 0;
}
