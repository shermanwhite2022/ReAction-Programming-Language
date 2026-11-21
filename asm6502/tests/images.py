# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
"""Readers for the disk images asm6502 writes. They follow the on-disk structures
from the specification (directory chains, sector links, free sector maps) and check
that the bookkeeping is consistent, so a wrong link or a wrong free count is found
and not only wrong file contents. Nothing here is shared with the assembler.

Each reader returns a list of files: dicts with name, data and the fields of the
directory entry. A broken disk raises BadDisk."""


class BadDisk(Exception):
    pass


def need(cond, msg):
    if not cond:
        raise BadDisk(msg)


# ------------------------------------------------------------------ D64

D64_SPT = [21] * 17 + [19] * 7 + [18] * 6 + [17] * 5          # tracks 1..35


def d64_off(t, s):
    need(1 <= t <= 35 and 0 <= s < D64_SPT[t - 1], 'bad track/sector %d/%d' % (t, s))
    return (sum(D64_SPT[:t - 1]) + s) * 256


def d64_read(img):
    need(len(img) == 174848, 'size %d' % len(img))
    bam = img[d64_off(18, 0):d64_off(18, 0) + 256]
    need(bam[0] == 18 and bam[1] == 1, 'BAM does not point at the directory')
    need(bam[2] == 0x41, 'DOS version')

    used = {(18, 0)}
    files = []
    t, s = 18, 1
    seen = set()

    while t:
        need((t, s) not in seen, 'directory loop')
        seen.add((t, s))
        used.add((t, s))
        sec = img[d64_off(t, s):d64_off(t, s) + 256]
        for e in range(8):
            ent = sec[e * 32:(e + 1) * 32]
            if ent[2] == 0:
                continue
            need(ent[2] == 0x82, 'file type %02x' % ent[2])
            name = bytes(ent[5:21]).rstrip(b'\xa0').decode('ascii')
            ft, fs = ent[3], ent[4]
            data = bytearray()
            nblocks = 0
            while ft:
                need((ft, fs) not in used, 'sector %d/%d used twice' % (ft, fs))
                used.add((ft, fs))
                blk = img[d64_off(ft, fs):d64_off(ft, fs) + 256]
                nblocks += 1
                if blk[0]:
                    data += blk[2:256]
                    ft, fs = blk[0], blk[1]
                else:
                    need(2 <= blk[1] <= 255, 'last block byte index %d' % blk[1])
                    data += blk[2:blk[1] + 1]
                    ft = 0
            need(ent[30] | ent[31] << 8 == nblocks, 'block count %d vs %d' % (ent[30] | ent[31] << 8, nblocks))
            files.append(dict(name=name, data=bytes(data), blocks=nblocks))
        t, s = sec[0], sec[1]

    # the BAM has to say exactly this: every sector in use is clear, the rest free, and the counts match
    for t in range(1, 36):
        e = bam[4 * t:4 * t + 4]
        free = 0
        for s in range(D64_SPT[t - 1]):
            bit = (e[1 + s // 8] >> (s % 8)) & 1
            if (t, s) in used:
                need(not bit, 'sector %d/%d is in use but free in the BAM' % (t, s))
            elif t != 18:
                need(bit, 'sector %d/%d is free but in use in the BAM' % (t, s))
            free += bit
        need(e[0] == free, 'track %d: BAM count %d but %d free' % (t, e[0], free))
        for s in range(D64_SPT[t - 1], 24):
            need(not (e[1 + s // 8] >> (s % 8)) & 1, 'track %d: bit %d past the last sector is set' % (t, s))
    return files


# ------------------------------------------------------------------ ATR

def atr_read(img):
    need(img[0] == 0x96 and img[1] == 0x02, 'ATR magic')
    paras = img[2] | img[3] << 8 | img[6] << 16
    secsize = img[4] | img[5] << 8
    need(secsize == 128, 'sector size %d' % secsize)
    need(len(img) == 16 + paras * 16, 'size %d vs header %d' % (len(img), 16 + paras * 16))
    nsec = paras * 16 // 128
    need(nsec == 720, 'sectors %d' % nsec)

    def sec(n):
        need(1 <= n <= 720, 'sector %d' % n)
        return img[16 + (n - 1) * 128:16 + n * 128]

    vtoc = sec(360)
    need(vtoc[0] == 2, 'DOS code')
    total = vtoc[1] | vtoc[2] << 8
    freecount = vtoc[3] | vtoc[4] << 8
    need(total == 707, 'total sectors %d' % total)

    used = {1, 2, 3} | set(range(360, 369))
    files = []
    for i in range(64):
        ent = sec(361 + i // 8)[(i % 8) * 16:(i % 8) * 16 + 16]
        if ent[0] == 0:
            continue
        need(ent[0] == 0x42, 'directory flag %02x' % ent[0])
        count = ent[1] | ent[2] << 8
        n = ent[3] | ent[4] << 8
        name = bytes(ent[5:13]).decode('ascii').rstrip(' ')
        ext = bytes(ent[13:16]).decode('ascii').rstrip(' ')
        data = bytearray()
        walked = 0
        while n:
            need(n not in used, 'sector %d used twice' % n)
            used.add(n)
            s = sec(n)
            need(s[125] >> 2 == i, 'file number %d in the sector link, expected %d' % (s[125] >> 2, i))
            nxt = (s[125] & 3) << 8 | s[126]
            nbytes = s[127]
            need(nbytes <= 125, 'byte count %d' % nbytes)
            if nxt:
                need(nbytes == 125, 'a full sector is expected before the next one')
            data += s[:nbytes]
            walked += 1
            n = nxt
        need(walked == count, 'sector count %d vs %d' % (count, walked))
        files.append(dict(name=name + ('.' + ext if ext else ''), data=bytes(data), sectors=count))

    for n in range(1, 720):
        bit = (vtoc[10 + n // 8] >> (7 - n % 8)) & 1
        need(bool(bit) == (n not in used), 'sector %d: bitmap says %s, but it is %s' % (n, 'free' if bit else 'used', 'used' if n in used else 'free'))
    need(freecount == 719 - len(used), 'free count %d vs %d' % (freecount, 719 - len(used)))
    return files


# ------------------------------------------------------------------ DOS 3.3 (.dsk, DOS order)

def dos_read(img):
    need(len(img) == 143360, 'size %d' % len(img))

    def sec(t, s):
        need(0 <= t < 35 and 0 <= s < 16, 'bad track/sector %d/%d' % (t, s))
        return img[(t * 16 + s) * 256:(t * 16 + s + 1) * 256]

    vtoc = sec(17, 0)
    need(vtoc[3] == 3, 'DOS release')
    need(vtoc[0x34] == 35 and vtoc[0x35] == 16, 'geometry')
    need(vtoc[0x36] | vtoc[0x37] << 8 == 256, 'bytes per sector')
    need(vtoc[0x27] == 122, 'pairs per list')

    used = {(t, s) for t in (0, 1, 2, 17) for s in range(16)}
    files = []
    t, s = vtoc[1], vtoc[2]
    need((t, s) == (17, 15), 'catalog starts at %d/%d' % (t, s))
    seen = set()
    while t:
        need((t, s) not in seen, 'catalog loop')
        seen.add((t, s))
        cat = sec(t, s)
        for e in range(7):
            ent = cat[0x0b + e * 35:0x0b + (e + 1) * 35]
            if ent[0] == 0 or ent[0] == 255:
                continue
            ftype = ent[2]
            name = bytes(b & 0x7f for b in ent[3:33]).decode('ascii').rstrip(' ')
            need(all(b & 0x80 for b in ent[3:33]), 'the name has to have the high bit set')
            nsec = ent[33] | ent[34] << 8
            data = bytearray()
            lt, ls = ent[0], ent[1]
            counted = 0
            index = 0
            while lt:
                need((lt, ls) not in used, 'list sector %d/%d used twice' % (lt, ls))
                used.add((lt, ls))
                counted += 1
                lst = sec(lt, ls)
                need(lst[5] | lst[6] << 8 == index, 'sector offset %d, expected %d' % (lst[5] | lst[6] << 8, index))
                for i in range(122):
                    dt, ds = lst[12 + 2 * i], lst[13 + 2 * i]
                    if dt == 0 and ds == 0:
                        continue
                    need((dt, ds) not in used, 'sector %d/%d used twice' % (dt, ds))
                    used.add((dt, ds))
                    counted += 1
                    data += sec(dt, ds)
                index += 122
                lt, ls = lst[1], lst[2]
            need(counted == nsec, 'sector count %d vs %d' % (nsec, counted))
            files.append(dict(name=name, type=ftype & 0x7f, data=bytes(data), sectors=nsec))
        t, s = cat[1], cat[2]
    need(len(seen) == 15, 'the catalog has %d sectors' % len(seen))

    for t in range(35):
        e = vtoc[0x38 + 4 * t:0x38 + 4 * t + 4]
        need(e[2] == 0 and e[3] == 0, 'track %d bitmap tail' % t)
        for s in range(16):
            bit = ((e[0] if s >= 8 else e[1]) >> (s % 8)) & 1
            need(bool(bit) == ((t, s) not in used), 'track %d sector %d: bitmap %d but %s' % (t, s, bit, 'used' if (t, s) in used else 'free'))
    return files


def dos_binary(file):
    """A DOS 3.3 binary file: address, length, the bytes (and zero padding after)."""
    d = file['data']
    addr = d[0] | d[1] << 8
    n = d[2] | d[3] << 8
    need(len(d) >= 4 + n, 'binary shorter than its length')
    need(not any(d[4 + n:]), 'padding is not zero')
    return addr, bytes(d[4:4 + n])


# ------------------------------------------------------------------ ProDOS (.po)

def prodos_read(img):
    need(len(img) == 280 * 512, 'size %d' % len(img))

    def blk(n):
        need(0 <= n < 280, 'bad block %d' % n)
        return img[n * 512:(n + 1) * 512]

    used = set(range(0, 7))
    d = blk(2)
    need(d[0] | d[1] << 8 == 0, 'volume directory has a previous block')
    hdr = d[4:43]
    need(hdr[0] >> 4 == 0xf, 'volume header storage type')
    vname = bytes(hdr[1:1 + (hdr[0] & 15)]).decode('ascii')
    need(hdr[0x1f] == 0x27 and hdr[0x20] == 0x0d, 'entry length / entries per block')
    need(hdr[0x23] | hdr[0x24] << 8 == 6, 'bitmap pointer')
    need(hdr[0x25] | hdr[0x26] << 8 == 280, 'total blocks')
    count = hdr[0x21] | hdr[0x22] << 8

    # the directory blocks, in order
    chain = [2]
    while True:
        b = blk(chain[-1])
        nxt = b[2] | b[3] << 8
        if not nxt:
            break
        need(nxt not in chain, 'directory loop')
        need(blk(nxt)[0] | blk(nxt)[1] << 8 == chain[-1], 'directory back link')
        chain.append(nxt)
    need(len(chain) == 4, 'directory has %d blocks' % len(chain))
    used |= set(chain)

    files = []
    for bi, bn in enumerate(chain):
        b = blk(bn)
        for e in range(13):
            if bi == 0 and e == 0:
                continue
            ent = b[4 + e * 39:4 + (e + 1) * 39]
            st = ent[0] >> 4
            if st == 0:
                continue
            name = bytes(ent[1:1 + (ent[0] & 15)]).decode('ascii')
            ftype = ent[0x10]
            key = ent[0x11] | ent[0x12] << 8
            nblocks = ent[0x13] | ent[0x14] << 8
            eof = ent[0x15] | ent[0x16] << 8 | ent[0x17] << 16
            aux = ent[0x1f] | ent[0x20] << 8
            need(ent[0x25] | ent[0x26] << 8 == 2, 'header pointer')
            need(ent[0x1e] & 0xc3 == 0xc3, 'access')
            counted = 0
            if st == 1:
                data_blocks = [key]
            elif st == 2:
                idx = blk(key)
                counted = 1
                data_blocks = []
                used.add(key)
                for i in range(256):
                    p = idx[i] | idx[256 + i] << 8
                    if p:
                        data_blocks.append(p)
            else:
                raise BadDisk('storage type %d' % st)
            data = bytearray()
            for p in data_blocks:
                need(p not in used, 'block %d used twice' % p)
                used.add(p)
                counted += 1
                data += blk(p)
            need(counted == nblocks, 'blocks used %d vs %d' % (nblocks, counted))
            need(len(data_blocks) == (eof + 511) // 512, '%d data blocks for %d bytes' % (len(data_blocks), eof))
            need(not any(data[eof:]), 'bytes after the end of the file are not zero')
            files.append(dict(name=name, type=ftype, aux=aux, data=bytes(data[:eof]), blocks=nblocks, storage=st))
    need(count == len(files), 'file count %d vs %d' % (count, len(files)))

    bm = blk(6)
    for n in range(280):
        bit = (bm[n // 8] >> (7 - n % 8)) & 1
        need(bool(bit) == (n not in used), 'block %d: bitmap %d but %s' % (n, bit, 'used' if n in used else 'free'))
    need(not any(bm[35:]), 'bitmap padding')
    return files
