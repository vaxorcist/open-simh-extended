#!/usr/bin/env python3
# open-simh-extended/tools/m9312_lst2bin.py  v2
#
# Builds a ROM image for SIMH's PDP-11 ROM device (ATTACH ROMn) from the
# MACRO-11 listings of DEC boot PROMs, e.g. the M9312 boot ROMs whose
# listings Don North (AK6DN) published at https://ak6dn.github.io/PDP-11/M9312/
# (sources: github.com/ak6dn/ak6dn.github.io, directory PDP-11/M9312).
#
# Why from the listings: the PROM programmer files on that page (Intel HEX)
# hold the raw contents of the 4-bit PROM chips, in the M9312's own bit
# order, while SIMH's ROM device wants a flat image of 16-bit PDP-11 words,
# low byte first (as in PDP-11 memory), the first word at the unit's ADDRESS
# and the file size being the ROM size.  A listing has the assembled words
# with their addresses, which is exactly that.
#
# Console/diagnostic PROMs (23-248F1 11/04-34, 23-446F1 11/44, 23-616F1
# 11/60-70, 23-774F1 11/24) are 256 words at 165000-165777 (SET ROMn
# ADDRESS=17765000); their listings are assembled there, so they need no
# @ADDRESS.
#
# Placement: each M9312 boot ROM occupies one socket of 64 words: socket 1
# at 173000, 2 at 173200, 3 at 173400, 4 at 173600.  The listings are
# assembled at 173000 whatever socket the ROM is meant for (the code is
# position independent and finds its neighbours by relative offsets), so
# every listing gets its socket address: LISTING@ADDRESS (octal).  Without
# @ADDRESS a listing stays where it was assembled.  The image runs from the
# lowest to the highest address used; gaps are 0.
#
# Check: --hex LISTING=HEXFILE compares a listing with the PROM programmer
# file of the same chip.  The M9312 boot PROM (512x4, the code in the low
# 256 cells) holds word i in the cells 4i..4i+3; found by comparing all 64
# words of 23-862A9 (2026-11-22): cell 4i+0 bits 0-3 = word bits 8,1,2,3;
# 4i+1 = 4,5,6,7; 4i+2 = 0,9,/10,/11; 4i+3 = /12,13,14,15 (/ = stored
# inverted).  The console PROM (1024x4) uses the same order for its 256
# words (cells 0..1023), as AK6DN's obj2hex.pl (tools.tar.gz) confirms; the
# chip size comes from the HEX file: 512 cells = boot PROM, 64 words;
# 1024 cells = console PROM, 256 words.  Every word of a listing must match
# its chip.
#
# Relative words: in the console listings a PC-relative operand is printed
# as its target with a trailing apostrophe (e.g. "165322'"); the word
# stored is the displacement target - (address of the word + 2).  Found by
# comparing with the HEX files (2026-11-22, v2).
#
# History: v1 boot PROMs only.  v2 (2026-11-22) console PROMs (chip size
# from the HEX file) and relative words.
#
# usage:
#   python3 -I m9312_lst2bin.py OUT.bin LISTING[@ADDRESS] ... [--hex LISTING=HEXFILE] ...
# example (the M9312 'XM' DECnet ROM set for the DMC11/DMR11, sockets 1-3):
#   python3 -I m9312_lst2bin.py m9312_xm.bin \
#       23-862A9/23-862A9.lst@173000 23-863A9/23-863A9.lst@173200 \
#       23-864A9/23-864A9.lst@173400 \
#       --hex 23-862A9/23-862A9.lst=23-862A9/23-862A9-0625.hex \
#       --hex 23-863A9/23-863A9.lst=23-863A9/23-863A9-063F.hex \
#       --hex 23-864A9/23-864A9.lst=23-864A9/23-864A9-0551.hex
# then in SIMH:  SET ROM0 ADDRESS=17773000  and  ATTACH ROM0 m9312_xm.bin

import re
import sys

# M9312 boot PROM cell bits -> word bits: CELL_MAP[j][b] = (word bit, inverted)
CELL_MAP = [[(8, 0), (1, 0), (2, 0), (3, 0)],
            [(4, 0), (5, 0), (6, 0), (7, 0)],
            [(0, 0), (9, 0), (10, 1), (11, 1)],
            [(12, 1), (13, 0), (14, 0), (15, 0)]]
WORDS_PER_ROM = 64                  # one boot PROM socket; padding unit of the image


def read_listing(path):
    """The bytes a MACRO-11 listing assembles: {address: byte}.

    A code line has the line number in columns 1-8, the address in columns
    10-15 and up to three values after it: 6 octal digits a word, 3 digits
    a byte.  Lines without an address (equates, comments) carry no code."""
    data = {}
    with open(path, errors='replace') as f:
        for line in f:
            if len(line) < 16 or not re.fullmatch(r'[0-7]{6}', line[9:15]):
                continue
            addr = int(line[9:15], 8)
            for tok in line[15:40].split():
                if re.fullmatch(r"[0-7]{6}'?", tok):        # a word, low byte first
                    value = int(tok[:6], 8)
                    if tok.endswith("'"):                   # PC-relative: displacement
                        value = (value - (addr + 2)) & 0o177777
                    data[addr] = value & 0o377
                    data[addr + 1] = value >> 8
                    addr += 2
                elif re.fullmatch(r'[0-7]{3}', tok):        # a byte
                    data[addr] = int(tok, 8)
                    addr += 1
                else:                                       # the source text begins
                    break
    if not data:
        raise SystemExit("%s: no code found (not a MACRO-11 listing?)" % path)
    return data


def read_intel_hex(path):
    """The data records of an Intel HEX file: {address: byte}."""
    mem = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line.startswith(':'):
                continue
            count, addr, rtype = int(line[1:3], 16), int(line[3:7], 16), int(line[7:9], 16)
            if rtype == 0:
                for i in range(count):
                    mem[addr + i] = int(line[9 + 2 * i:11 + 2 * i], 16)
    return mem


def check_hex(listing, hexfile):
    """Compare the words of a listing (one ROM, assembled at its lowest
    address) with the cells of the M9312 PROM in a HEX file."""
    data = read_listing(listing)
    base = min(data) & ~1
    cells = read_intel_hex(hexfile)
    words = 64 if len(cells) <= 512 else 256                # boot 512x4 / console 1024x4
    bad = 0
    for i in range(words):
        word = data.get(base + 2 * i, 0) | (data.get(base + 2 * i + 1, 0) << 8)
        for j in range(4):
            cell = 0
            for b, (wbit, inv) in enumerate(CELL_MAP[j]):
                cell |= (((word >> wbit) & 1) ^ inv) << b
            if cells.get(4 * i + j) != cell:
                bad += 1
    print("%s vs %s: %s" % (listing, hexfile, "all %d words match" % words if bad == 0
                                                else "%d cells differ" % bad))
    return bad == 0


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__ if __doc__ else "usage: m9312_lst2bin.py OUT.bin LISTING[@ADDRESS] ...")
    out = argv[1]
    image = {}
    ok = True
    args = argv[2:]
    i = 0
    while i < len(args):
        arg = args[i]
        if arg == '--hex':                                  # --hex LISTING=HEXFILE
            listing, hexfile = args[i + 1].split('=', 1)
            ok = check_hex(listing, hexfile) and ok
            i += 2
            continue
        path, _, at = arg.partition('@')
        data = read_listing(path)
        origin = min(data) & ~1
        shift = (int(at, 8) - origin) if at else 0
        for addr, byte in data.items():
            if (addr + shift) in image:
                raise SystemExit("%s: address %06o used twice" % (path, addr + shift))
            image[addr + shift] = byte
        print("%s: %d bytes, %06o-%06o" % (path, len(data), origin + shift, max(data) + shift))
        i += 1
    if not ok:
        raise SystemExit("HEX check failed: no image written")
    lo = min(image) & ~1
    hi = (max(image) | 1) + 1
    # a whole number of 64-word sockets from the first one used
    hi = lo + ((hi - lo + 2 * WORDS_PER_ROM - 1) // (2 * WORDS_PER_ROM)) * 2 * WORDS_PER_ROM
    with open(out, 'wb') as f:
        f.write(bytes(image.get(a, 0) for a in range(lo, hi)))
    print("%s: %d bytes, %06o-%06o (SET ROMn ADDRESS=%08o)" % (out, hi - lo, lo, hi - 1, 0o17600000 + lo))


if __name__ == '__main__':
    main(sys.argv)
