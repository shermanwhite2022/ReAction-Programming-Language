/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "asm.h"
#include "asmtypes.h"

// Output files for the targets.
//
//   c64       PRG file: the load address (2 bytes, low byte first) and the program
//   appleii   DOS 3.3 binary file: load address (2 bytes), length (2 bytes) and the program
//   prodos    the program's bytes, as a file with no header (the load address goes in
//             the directory of a ProDOS disk image, which is the usual output: .po)
//   atari800  Atari DOS binary load file (.xex): $ff $ff, then segments of
//             start address, end address and data. Each block of memory that
//             was written is its own segment, and the run address (.run) is
//             stored in a segment that sets RUNAD ($02e0).
//
// With no target the output is just the bytes, or with -p the bytes after a
// 2 byte load address.

int targetFromName (const char *name) {

	if (!strcasecmp(name, "c64"))
		return TARGET_C64;

	if (!strcasecmp(name, "appleii") || !strcasecmp(name, "apple2"))
		return TARGET_APPLEII;

	if (!strcasecmp(name, "atari800") || !strcasecmp(name, "atari"))
		return TARGET_ATARI800;

	if (!strcasecmp(name, "prodos"))
		return TARGET_PRODOS;

	return -1;
}

const char *targetName (int target) {
	switch (target) {
	case TARGET_C64:	return "c64";
	case TARGET_APPLEII:	return "appleii";
	case TARGET_ATARI800:	return "atari800";
	case TARGET_PRODOS:	return "prodos";
	}

	return "none";
}

// the usual file name extension for a target

const char *targetExtension (int target) {
	switch (target) {
	case TARGET_C64:	return "prg";
	case TARGET_ATARI800:	return "xex";
	case TARGET_PRODOS:	return "po";
	}

	return "bin";
}

// where the program goes when the source has no .org: somewhere that is
// free on that machine and does not need a BASIC stub or a loader

int defaultOrigin (int target) {
	switch (target) {
	case TARGET_C64:	return 0xc000;		// the free 4K above BASIC, SYS 49152
	case TARGET_APPLEII:
	case TARGET_PRODOS:	return 0x6000;		// above the hi-res pages, below DOS
	case TARGET_ATARI800:	return 0x2000;		// above DOS and the screen
	}

	return 0;
}

// the file is built in memory first, since it may go on a disk image

typedef struct {
	unsigned char	*p;
	size_t		n;
	size_t		cap;
} BUF;

static int put (BUF *b, int byte) {
	if (b->n == b->cap) {
		b->cap = b->cap ? b->cap * 2 : 4096;
		b->p = (unsigned char *) realloc(b->p, b->cap);
	}

	b->p[b->n++] = (unsigned char) byte;
	return 1;
}

static int put16 (BUF *b, int word) {
	put(b, word);
	return put(b, word >> 8) + 1;
}

static int putBlock (BUF *b, int from, int to) {
	int a;

	for (a = from; a <= to; a++)
		put(b, _membuf[a]);

	return to - from + 1;
}

// Write the assembled program. The output file is a disk image when its name
// ends in .d64, .atr, .dsk (or .do) or .po; imageName is the name of the file on
// the disk (-n) or NULL, srcBase the name of the source file without extension,
// which is the name to use then. Returns the size of the file, or -1.

int writeOutput (const char *path, int target, int loadHeader, const char *imageName, const char *srcBase) {

	FILE *f;
	BUF b = { NULL, 0, 0 };
	unsigned char *out;
	int kind = imageKind(path);
	int a;
	int end;
	int size;

	if (_maxaddr < 0) {
		fprintf(stderr, "Nothing was assembled, so there is nothing to write.\n");
		return -1;
	}

	if (target != TARGET_NONE || loadHeader || kind) {
		if (_maxaddr > 0xffff) {
			fprintf(stderr, "The program goes up to $%x, which is past the 64K the %s can address.\n",
					_maxaddr, target == TARGET_NONE ? "output file format" : targetName(target));
			return -1;
		}
	}

	if (target == TARGET_APPLEII && _maxaddr - _minaddr + 1 > 0xffff) {
		fprintf(stderr, "The program is too long for a DOS 3.3 binary file (65535 bytes at most).\n");
		return -1;
	}

	if (kind && target == TARGET_NONE) {
		fprintf(stderr, "A %s needs a target (use -t or .target).\n", imageDescription(kind));
		return -1;
	}

	if (kind && imageTarget(kind) != target) {
		fprintf(stderr, "A %s holds programs for the %s target, but the target is %s.\n",
				imageDescription(kind), targetName(imageTarget(kind)), targetName(target));
		return -1;
	}

	if (imageName && !kind) {
		fprintf(stderr, "-n names the file on a disk image, so the output file has to end in .d64, .atr, .dsk or .po.\n");
		return -1;
	}

	switch (target) {

	case TARGET_C64:
		put16(&b, _minaddr);
		putBlock(&b, _minaddr, _maxaddr);
		break;

	case TARGET_APPLEII:
		put16(&b, _minaddr);
		put16(&b, _maxaddr - _minaddr + 1);
		putBlock(&b, _minaddr, _maxaddr);
		break;

	case TARGET_ATARI800:
		put16(&b, 0xffff);

		// one segment for each block of memory that was written

		for (a = _minaddr; a <= _maxaddr; a++) {
			if (!_written[a])
				continue;

			for (end = a; end < _maxaddr && _written[end + 1]; end++)
				;

			put16(&b, a);
			put16(&b, end);
			putBlock(&b, a, end);
			a = end;
		}

		// RUNAD, the address the program is started at

		if (_entry >= 0) {
			put16(&b, 0x02e0);
			put16(&b, 0x02e1);
			put16(&b, _entry);
		}
		break;

	case TARGET_PRODOS:
		// a ProDOS file has no header: the type and the load address are in the directory
		putBlock(&b, _minaddr, _maxaddr);
		break;

	default:
		if (loadHeader)
			put16(&b, _minaddr);

		putBlock(&b, _minaddr, _maxaddr);
		break;
	}

	if (kind) {
		out = buildImage(kind, imageName, srcBase, b.p, (int) b.n, _minaddr, &size);
		free(b.p);

		if (!out)
			return -1;
	} else {
		out = b.p;
		size = (int) b.n;
	}

	f = fopen(path, "wb");

	if (!f) {
		fprintf(stderr, "Unable to write file %s!\n", path);
		free(out);
		return -1;
	}

	if (fwrite(out, 1, (size_t) size, f) != (size_t) size) {
		fclose(f);
		fprintf(stderr, "Error writing file %s!\n", path);
		free(out);
		return -1;
	}

	free(out);

	if (fclose(f)) {
		fprintf(stderr, "Error writing file %s!\n", path);
		return -1;
	}

	return size;
}
