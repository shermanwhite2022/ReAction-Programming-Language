/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "asm.h"
#include "asmtypes.h"

// Disk images. The program file is put on a new, empty disk image that has
// that one file on it:
//
//   .d64  Commodore 1541, 35 tracks (174848 bytes). The file is a PRG.
//   .atr  Atari 810 / Atari DOS 2.0S, single density (90K, 720 sectors of 128 bytes),
//         with the ATR header. The file is the .xex load file.
//   .dsk  Apple II DOS 3.3, 35 tracks of 16 sectors (140K), DOS sector order
//   .do   (the same as .dsk). The file is a binary (type B).
//   .po   Apple II ProDOS, 280 blocks of 512 bytes (140K), ProDOS block order.
//         The file is a BIN file ($06) with the load address as its aux type.
//
// The disks are not bootable: they have no boot sectors, DOS or ProDOS on them.
// They are for putting the program on a disk that a drive or an emulator can read
// (or for copying the file off with a disk utility).

int imageKind (const char *path) {
	const char *dot = strrchr(path, '.');
	const char *slash = strrchr(path, '/');

	if (!dot || (slash && dot < slash))
		return IMG_NONE;

	dot++;

	if (!strcasecmp(dot, "d64"))
		return IMG_D64;

	if (!strcasecmp(dot, "atr"))
		return IMG_ATR;

	if (!strcasecmp(dot, "dsk") || !strcasecmp(dot, "do"))
		return IMG_DOS33;

	if (!strcasecmp(dot, "po"))
		return IMG_PRODOS;

	return IMG_NONE;
}

int imageTarget (int kind) {
	switch (kind) {
	case IMG_D64:		return TARGET_C64;
	case IMG_ATR:		return TARGET_ATARI800;
	case IMG_DOS33:		return TARGET_APPLEII;
	case IMG_PRODOS:	return TARGET_PRODOS;
	}

	return TARGET_NONE;
}

const char *imageDescription (int kind) {
	switch (kind) {
	case IMG_D64:		return "D64 disk image";
	case IMG_ATR:		return "ATR disk image (Atari DOS 2)";
	case IMG_DOS33:		return "DOS 3.3 disk image";
	case IMG_PRODOS:	return "ProDOS disk image";
	}

	return "file";
}

// ---------------------------------------------------------------- names

static int isUpperDigit (int c) {
	return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static int isLetter (int c) {
	return c >= 'A' && c <= 'Z';
}

// work out the name of the file on the disk, from -n or else from the name of
// the source file. out gets the name; for an Atari disk it is 11 characters,
// the name padded to 8 and the extension padded to 3.

static int diskName (int kind, const char *given, const char *base, char *out) {
	char tmp[300];
	size_t n = 0;
	size_t i;
	size_t len;

	if (given) {
		for (i = 0; given[i] && n < 250; i++)
			tmp[n++] = (char) toupper((unsigned char) given[i]);
	} else {
		char fill = kind == IMG_D64 ? '-' : kind == IMG_ATR ? '_' : '.';
		size_t max = kind == IMG_D64 ? 16 : kind == IMG_DOS33 ? 30 : kind == IMG_PRODOS ? 15 : 8;

		for (i = 0; base[i] && n < max; i++) {
			int c = toupper((unsigned char) base[i]);

			tmp[n++] = isUpperDigit(c) ? (char) c : fill;
		}

		// the file systems that want a letter first

		if ((kind == IMG_PRODOS || kind == IMG_ATR) && (n == 0 || !isLetter(tmp[0]))) {
			memmove(tmp + 1, tmp, n);
			tmp[0] = 'P';

			if (n < max)
				n++;
		}

		if (n == 0) {
			strcpy(tmp, "PROGRAM");
			n = 7;
		}

		if (kind == IMG_ATR) {
			strcpy(tmp + n, ".XEX");
			n += 4;
		}
	}

	tmp[n] = 0;
	len = n;

	switch (kind) {

	case IMG_D64:
		if (!len || len > 16)
			goto bad;

		for (i = 0; i < len; i++)
			if (tmp[i] < 0x20 || tmp[i] > 0x5f)
				goto bad;

		strcpy(out, tmp);
		return 0;

	case IMG_DOS33:
		if (!len || len > 30 || tmp[0] == ' ')
			goto bad;

		for (i = 0; i < len; i++)
			if (tmp[i] < 0x20 || tmp[i] > 0x5f || tmp[i] == ',' || tmp[i] == ':')
				goto bad;

		strcpy(out, tmp);
		return 0;

	case IMG_PRODOS:
		if (!len || len > 15 || !isLetter(tmp[0]))
			goto bad;

		for (i = 1; i < len; i++)
			if (!isUpperDigit(tmp[i]) && tmp[i] != '.')
				goto bad;

		strcpy(out, tmp);
		return 0;

	case IMG_ATR: {
		char *dot = strrchr(tmp, '.');
		size_t blen = dot ? (size_t) (dot - tmp) : len;
		size_t elen = dot ? len - blen - 1 : 0;

		if (!blen || blen > 8 || elen > 3 || !isLetter(tmp[0]))
			goto bad;

		for (i = 0; i < blen; i++)
			if (!isUpperDigit(tmp[i]) && tmp[i] != '_')
				goto bad;

		for (i = 0; i < elen; i++)
			if (!isUpperDigit(dot[1 + i]) && dot[1 + i] != '_')
				goto bad;

		memset(out, ' ', 11);
		memcpy(out, tmp, blen);

		if (elen)
			memcpy(out + 8, dot + 1, elen);

		out[11] = 0;
		return 0;
	}
	}

bad:
	switch (kind) {
	case IMG_D64:
		fprintf(stderr, "The file name '%s' is not allowed on a D64 disk (1 to 16 letters, digits and punctuation).\n", tmp);
		break;
	case IMG_DOS33:
		fprintf(stderr, "The file name '%s' is not allowed on a DOS 3.3 disk (1 to 30 characters, no commas or colons).\n", tmp);
		break;
	case IMG_PRODOS:
		fprintf(stderr, "The file name '%s' is not allowed by ProDOS (up to 15 letters, digits and periods, starting with a letter).\n", tmp);
		break;
	default:
		fprintf(stderr, "The file name '%s' is not allowed by Atari DOS 2 (NAME.EXT, 8 and 3 letters, digits and _, starting with a letter).\n", tmp);
		break;
	}

	return -1;
}

static void tooBig (int kind) {
	fprintf(stderr, "The program does not fit on a %s.\n", imageDescription(kind));
}

// ---------------------------------------------------------------- D64

static int d64spt (int t) {
	return t <= 17 ? 21 : t <= 24 ? 19 : t <= 30 ? 18 : 17;
}

static int d64off (int t, int s) {
	int o = 0;
	int i;

	for (i = 1; i < t; i++)
		o += d64spt(i);

	return (o + s) * 256;
}

static unsigned char *d64bam (unsigned char *img, int t) {
	return img + d64off(18, 0) + 4 * t;
}

static int d64isFree (unsigned char *img, int t, int s) {
	return d64bam(img, t)[1 + s / 8] & (1 << (s % 8));
}

static void d64use (unsigned char *img, int t, int s) {
	unsigned char *e = d64bam(img, t);

	e[1 + s / 8] &= (unsigned char) ~(1 << (s % 8));
	e[0]--;
}

// the next free sector, 10 sectors on from the last one as the 1541 does it (the
// first block is the first free one on track 1). Track 18 is the directory.

static int d64alloc (unsigned char *img, int *pt, int *ps, int first) {
	int t = *pt;
	int s = *ps;
	int k;
	int n;

	if (!first)
		s += 10;

	for (;;) {
		n = d64spt(t);

		if (t != 18 && d64bam(img, t)[0] > 0) {
			s %= n;

			for (k = 0; k < n; k++, s = (s + 1) % n) {
				if (d64isFree(img, t, s)) {
					d64use(img, t, s);
					*pt = t;
					*ps = s;
					return 0;
				}
			}
		}

		t++;

		if (t == 18)
			t++;

		if (t > 35)
			return -1;

		s = 0;
	}
}

static unsigned char *buildD64 (const char *name, const unsigned char *data, int len, int *size) {
	unsigned char *img = (unsigned char *) calloc(1, 174848);
	unsigned char *bam;
	unsigned char *dir;
	int nblocks = (len + 253) / 254;
	int *bt;
	int *bs;
	int t = 1;
	int s = 0;
	int i;
	int n;

	if (nblocks < 1)
		nblocks = 1;

	bt = (int *) calloc(nblocks, sizeof(int));
	bs = (int *) calloc(nblocks, sizeof(int));

	// the BAM: every sector free, then the BAM and the first directory sector used

	bam = img + d64off(18, 0);
	bam[0] = 18;
	bam[1] = 1;
	bam[2] = 0x41;

	for (t = 1; t <= 35; t++) {
		n = d64spt(t);
		bam[4 * t] = (unsigned char) n;

		for (s = 0; s < n; s++)
			bam[4 * t + 1 + s / 8] |= (unsigned char) (1 << (s % 8));
	}

	d64use(img, 18, 0);
	d64use(img, 18, 1);

	memset(bam + 0x90, 0xa0, 0x1b);
	memcpy(bam + 0x90, "ASM6502", 7);
	bam[0xa2] = '0';
	bam[0xa3] = '1';
	bam[0xa5] = '2';
	bam[0xa6] = 'A';

	// the file's blocks

	t = 1;
	s = 0;

	for (i = 0; i < nblocks; i++) {
		if (d64alloc(img, &t, &s, i == 0)) {
			tooBig(IMG_D64);
			free(img);
			free(bt);
			free(bs);
			return NULL;
		}

		bt[i] = t;
		bs[i] = s;
	}

	for (i = 0; i < nblocks; i++) {
		unsigned char *blk = img + d64off(bt[i], bs[i]);
		int from = i * 254;
		int count = len - from > 254 ? 254 : len - from;

		if (count < 0)
			count = 0;

		memcpy(blk + 2, data + from, (size_t) count);

		if (i + 1 < nblocks) {
			blk[0] = (unsigned char) bt[i + 1];
			blk[1] = (unsigned char) bs[i + 1];
		} else {
			blk[0] = 0;
			blk[1] = (unsigned char) (count + 1);	// the index of the last byte used
		}
	}

	// the directory entry

	dir = img + d64off(18, 1);
	dir[0] = 0;
	dir[1] = 0xff;
	dir[2] = 0x82;					// PRG, closed
	dir[3] = (unsigned char) bt[0];
	dir[4] = (unsigned char) bs[0];
	memset(dir + 5, 0xa0, 16);
	memcpy(dir + 5, name, strlen(name));
	dir[30] = (unsigned char) (nblocks & 0xff);
	dir[31] = (unsigned char) (nblocks >> 8);

	free(bt);
	free(bs);
	*size = 174848;
	return img;
}

// ---------------------------------------------------------------- ATR (Atari DOS 2.0S)

#define ATR_SECTORS	720

static unsigned char *atrSector (unsigned char *img, int n) {
	return img + 16 + (n - 1) * 128;
}

static int atrIsFree (unsigned char *vtoc, int n) {
	return vtoc[10 + n / 8] & (0x80 >> (n % 8));
}

static void atrUse (unsigned char *vtoc, int n) {
	int freeSectors = vtoc[3] | (vtoc[4] << 8);

	vtoc[10 + n / 8] &= (unsigned char) ~(0x80 >> (n % 8));
	freeSectors--;
	vtoc[3] = (unsigned char) (freeSectors & 0xff);
	vtoc[4] = (unsigned char) (freeSectors >> 8);
}

static unsigned char *buildATR (const char *name, const unsigned char *data, int len, int *size) {
	int total = 16 + ATR_SECTORS * 128;
	unsigned char *img = (unsigned char *) calloc(1, (size_t) total);
	unsigned char *vtoc = atrSector(img, 360);
	unsigned char *dir = atrSector(img, 361);
	int nsec = (len + 124) / 125;
	int *sec;
	int n;
	int i;
	int next = 4;

	if (nsec < 1)
		nsec = 1;

	// the ATR header: $0296, the size in 16 byte paragraphs, the sector size

	img[0] = 0x96;
	img[1] = 0x02;
	img[2] = (unsigned char) ((ATR_SECTORS * 128 / 16) & 0xff);
	img[3] = (unsigned char) ((ATR_SECTORS * 128 / 16) >> 8);
	img[4] = 128;
	img[5] = 0;

	// the VTOC: sectors 1 to 719 are in the bitmap (sector 720 never is).
	// The boot sectors 1-3, the VTOC and the directory (360-368) are in use.

	vtoc[0] = 2;
	vtoc[1] = 0xc3;
	vtoc[2] = 0x02;
	vtoc[3] = 0;
	vtoc[4] = 0;

	for (n = 1; n < ATR_SECTORS; n++) {
		vtoc[10 + n / 8] |= (unsigned char) (0x80 >> (n % 8));
		vtoc[3]++;

		if (!vtoc[3])
			vtoc[4]++;
	}

	for (n = 1; n <= 3; n++)
		atrUse(vtoc, n);

	for (n = 360; n <= 368; n++)
		atrUse(vtoc, n);

	// the file's sectors, the lowest free ones

	sec = (int *) calloc(nsec, sizeof(int));

	for (i = 0; i < nsec; i++) {
		while (next < ATR_SECTORS && !atrIsFree(vtoc, next))
			next++;

		if (next >= ATR_SECTORS) {
			tooBig(IMG_ATR);
			free(img);
			free(sec);
			return NULL;
		}

		sec[i] = next;
		atrUse(vtoc, next);
	}

	for (i = 0; i < nsec; i++) {
		unsigned char *s = atrSector(img, sec[i]);
		int from = i * 125;
		int count = len - from > 125 ? 125 : len - from;
		int link = i + 1 < nsec ? sec[i + 1] : 0;

		if (count < 0)
			count = 0;

		memcpy(s, data + from, (size_t) count);
		s[125] = (unsigned char) (0 << 2 | (link >> 8));	// file number 0, the link's high bits
		s[126] = (unsigned char) (link & 0xff);
		s[127] = (unsigned char) count;
	}

	// the directory entry: in use, the number of sectors, the first sector, the name

	dir[0] = 0x42;
	dir[1] = (unsigned char) (nsec & 0xff);
	dir[2] = (unsigned char) (nsec >> 8);
	dir[3] = (unsigned char) (sec[0] & 0xff);
	dir[4] = (unsigned char) (sec[0] >> 8);
	memcpy(dir + 5, name, 11);

	free(sec);
	*size = total;
	return img;
}

// ---------------------------------------------------------------- DOS 3.3

static unsigned char *dosSector (unsigned char *img, int t, int s) {
	return img + (t * 16 + s) * 256;
}

static unsigned char *dosMap (unsigned char *img, int t, int s, int *mask) {
	unsigned char *vtoc = dosSector(img, 17, 0);

	*mask = 1 << (s & 7);
	return vtoc + 0x38 + 4 * t + (s >= 8 ? 0 : 1);
}

static int dosIsFree (unsigned char *img, int t, int s) {
	int mask;

	return *dosMap(img, t, s, &mask) & mask;
}

static void dosUse (unsigned char *img, int t, int s) {
	int mask;
	unsigned char *b = dosMap(img, t, s, &mask);

	*b &= (unsigned char) ~mask;
}

// tracks 0-2 are kept for DOS and 17 is the catalog, so files go on 3-16 and then 18-34

static int dosAlloc (unsigned char *img, int *pt, int *ps) {
	int i;
	int s;

	for (i = 0; i < 31; i++) {
		int t = i < 14 ? 3 + i : 4 + i;

		for (s = 15; s >= 0; s--) {
			if (dosIsFree(img, t, s)) {
				dosUse(img, t, s);
				*pt = t;
				*ps = s;
				return 0;
			}
		}
	}

	return -1;
}

static unsigned char *buildDOS33 (const char *name, const unsigned char *data, int len, int *size) {
	unsigned char *img = (unsigned char *) calloc(1, 143360);
	unsigned char *vtoc = dosSector(img, 17, 0);
	unsigned char *cat;
	int nsec = (len + 255) / 256;
	int nlist;
	int lt[8], ls[8];
	int *dt;
	int *ds;
	int t, s;
	int i, k, d;

	if (nsec < 1)
		nsec = 1;

	nlist = (nsec + 121) / 122;

	if (nlist > 8) {
		tooBig(IMG_DOS33);
		free(img);
		return NULL;
	}

	dt = (int *) calloc(nsec, sizeof(int));
	ds = (int *) calloc(nsec, sizeof(int));

	// the VTOC: catalog at track 17 sector 15, DOS 3.3, volume 254, 35 tracks of 16 sectors

	vtoc[1] = 17;
	vtoc[2] = 15;
	vtoc[3] = 3;
	vtoc[6] = 254;
	vtoc[0x27] = 122;
	vtoc[0x30] = 17;
	vtoc[0x31] = 1;
	vtoc[0x34] = 35;
	vtoc[0x35] = 16;
	vtoc[0x37] = 1;

	for (t = 0; t < 35; t++) {
		int used = t <= 2 || t == 17;

		vtoc[0x38 + 4 * t] = used ? 0 : 0xff;
		vtoc[0x38 + 4 * t + 1] = used ? 0 : 0xff;
	}

	// the catalog sectors are linked from 15 down to 1

	for (s = 15; s >= 1; s--) {
		cat = dosSector(img, 17, s);

		if (s > 1) {
			cat[1] = 17;
			cat[2] = (unsigned char) (s - 1);
		}
	}

	// the track/sector lists and the data sectors

	d = 0;

	for (k = 0; k < nlist; k++) {
		if (dosAlloc(img, &lt[k], &ls[k])) {
			tooBig(IMG_DOS33);
			free(img);
			free(dt);
			free(ds);
			return NULL;
		}

		for (i = 0; i < 122 && d < nsec; i++, d++) {
			if (dosAlloc(img, &dt[d], &ds[d])) {
				tooBig(IMG_DOS33);
				free(img);
				free(dt);
				free(ds);
				return NULL;
			}
		}
	}

	for (d = 0; d < nsec; d++) {
		int from = d * 256;
		int count = len - from > 256 ? 256 : len - from;

		if (count > 0)
			memcpy(dosSector(img, dt[d], ds[d]), data + from, (size_t) count);
	}

	for (k = 0; k < nlist; k++) {
		unsigned char *list = dosSector(img, lt[k], ls[k]);

		if (k + 1 < nlist) {
			list[1] = (unsigned char) lt[k + 1];
			list[2] = (unsigned char) ls[k + 1];
		}

		list[5] = (unsigned char) ((k * 122) & 0xff);
		list[6] = (unsigned char) ((k * 122) >> 8);

		for (i = 0; i < 122 && k * 122 + i < nsec; i++) {
			list[12 + 2 * i] = (unsigned char) dt[k * 122 + i];
			list[13 + 2 * i] = (unsigned char) ds[k * 122 + i];
		}
	}

	// the catalog entry: first list sector, type B, the name (high bit set, padded
	// with spaces), the number of sectors used

	cat = dosSector(img, 17, 15) + 0x0b;
	cat[0] = (unsigned char) lt[0];
	cat[1] = (unsigned char) ls[0];
	cat[2] = 0x04;
	memset(cat + 3, 0xa0, 30);

	for (i = 0; name[i]; i++)
		cat[3 + i] = (unsigned char) (name[i] | 0x80);

	cat[33] = (unsigned char) ((nsec + nlist) & 0xff);
	cat[34] = (unsigned char) ((nsec + nlist) >> 8);

	free(dt);
	free(ds);
	*size = 143360;
	return img;
}

// ---------------------------------------------------------------- ProDOS

#define PD_BLOCKS	280

static unsigned char *pdBlock (unsigned char *img, int n) {
	return img + n * 512;
}

static int pdAlloc (unsigned char *img) {
	unsigned char *map = pdBlock(img, 6);
	int n;

	for (n = 7; n < PD_BLOCKS; n++) {
		if (map[n / 8] & (0x80 >> (n % 8))) {
			map[n / 8] &= (unsigned char) ~(0x80 >> (n % 8));
			return n;
		}
	}

	return -1;
}

static unsigned char *buildProDOS (const char *name, const unsigned char *data, int len, int aux, int *size) {
	unsigned char *img = (unsigned char *) calloc(1, PD_BLOCKS * 512);
	unsigned char *map = pdBlock(img, 6);
	unsigned char *dir = pdBlock(img, 2);
	unsigned char *e;
	int nblocks = (len + 511) / 512;
	int key;
	int i;
	int nlen = (int) strlen(name);

	if (nblocks < 1)
		nblocks = 1;

	if (nblocks > 256) {
		tooBig(IMG_PRODOS);
		free(img);
		return NULL;
	}

	// the bitmap: blocks 0-6 (boot, the volume directory, the bitmap) are in use

	for (i = 0; i < PD_BLOCKS; i++)
		map[i / 8] |= (unsigned char) (0x80 >> (i % 8));

	for (i = 0; i <= 6; i++)
		map[i / 8] &= (unsigned char) ~(0x80 >> (i % 8));

	// the volume directory is blocks 2 to 5, linked both ways

	for (i = 2; i <= 5; i++) {
		unsigned char *b = pdBlock(img, i);

		b[0] = (unsigned char) (i > 2 ? i - 1 : 0);
		b[2] = (unsigned char) (i < 5 ? i + 1 : 0);
	}

	// the volume header

	dir[4] = (unsigned char) (0xf0 | 7);
	memcpy(dir + 5, "ASM6502", 7);
	dir[34] = 0xc3;
	dir[35] = 0x27;
	dir[36] = 0x0d;
	dir[37] = 1;					// one file
	dir[39] = 6;					// the bitmap is block 6
	dir[41] = PD_BLOCKS & 0xff;
	dir[42] = PD_BLOCKS >> 8;

	// the file: one block (a seedling) or an index block with its data blocks (a sapling)

	if (nblocks == 1) {
		key = pdAlloc(img);

		if (key < 0) {
			tooBig(IMG_PRODOS);
			free(img);
			return NULL;
		}

		memcpy(pdBlock(img, key), data, (size_t) len);
	} else {
		unsigned char *index;

		key = pdAlloc(img);

		if (key < 0) {
			tooBig(IMG_PRODOS);
			free(img);
			return NULL;
		}

		index = pdBlock(img, key);

		for (i = 0; i < nblocks; i++) {
			int b = pdAlloc(img);
			int from = i * 512;
			int count = len - from > 512 ? 512 : len - from;

			if (b < 0) {
				tooBig(IMG_PRODOS);
				free(img);
				return NULL;
			}

			index[i] = (unsigned char) (b & 0xff);
			index[256 + i] = (unsigned char) (b >> 8);
			memcpy(pdBlock(img, b), data + from, (size_t) count);
		}
	}

	e = dir + 4 + 39;
	e[0] = (unsigned char) ((nblocks == 1 ? 0x10 : 0x20) | nlen);
	memcpy(e + 1, name, (size_t) nlen);
	e[0x10] = 0x06;					// BIN
	e[0x11] = (unsigned char) (key & 0xff);
	e[0x12] = (unsigned char) (key >> 8);
	e[0x13] = (unsigned char) ((nblocks == 1 ? 1 : nblocks + 1) & 0xff);
	e[0x14] = (unsigned char) ((nblocks == 1 ? 1 : nblocks + 1) >> 8);
	e[0x15] = (unsigned char) (len & 0xff);
	e[0x16] = (unsigned char) ((len >> 8) & 0xff);
	e[0x17] = (unsigned char) (len >> 16);
	e[0x1e] = 0xc3;
	e[0x1f] = (unsigned char) (aux & 0xff);
	e[0x20] = (unsigned char) (aux >> 8);
	e[0x25] = 2;					// the volume directory's key block

	*size = PD_BLOCKS * 512;
	return img;
}

// ---------------------------------------------------------------- the entry point

// Make the disk image of the given kind with the one file. data is the file
// (len bytes), aux is the load address (ProDOS keeps it in the directory), given
// the file name from -n or NULL, base the name of the source file without its
// extension. Returns the image (to free) and its size, or NULL after saying why.

unsigned char *buildImage (int kind, const char *given, const char *base, const unsigned char *data, int len, int aux, int *size) {
	char name[40];

	if (diskName(kind, given, base, name))
		return NULL;

	switch (kind) {
	case IMG_D64:		return buildD64(name, data, len, size);
	case IMG_ATR:		return buildATR(name, data, len, size);
	case IMG_DOS33:		return buildDOS33(name, data, len, size);
	case IMG_PRODOS:	return buildProDOS(name, data, len, aux, size);
	}

	return NULL;
}
