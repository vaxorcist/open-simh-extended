# open-simh-extended/tests/tu58/mkdd_prep.py  v1
#
# Prepares a working copy of the RT-11 V5.3 RL02 kit (rtv53_rl.dsk, from the
# SIMH software kits: bitsavers simh.trailing-edge.com/kits/rtv53swre.tar.Z)
# so that it boots to the RT-11 monitor instead of the automatic
# installation: the kit boots RT11AI.SYS, which runs STARTA.COM (the
# installation procedure, which ignores Ctrl/C).  This script replaces the
# text of STARTA.COM in the COPY with an empty line, so the monitor comes up
# at the "." prompt.  The original kit is not changed.
#
# Usage:  python3 -I mkdd_prep.py rtv53_rl.dsk work_rl.dsk
#         then:  BIN/pdp11 mkdd.ini   (builds rt11v53_dd.dsk, a bootable
#         RT-11 V5.3 SJ TU58 cartridge, and boots it from DD0)

import struct
import sys


def r50(w):
    c = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789"
    return c[w // 1600] + c[(w // 40) % 40] + c[w % 40]


def find(d, want):
    """Start block and length of a file in an RT-11 directory (block 6 on)."""
    seg_no = 1
    while seg_no:
        seg = d[(6 + (seg_no - 1) * 2) * 512:(8 + (seg_no - 1) * 2) * 512]
        tot, nxt, hi, extra, start = struct.unpack('<5H', seg[:10])
        p, ent, blk = 10, 14 + extra, start
        while p + ent <= 1024:
            st, n1, n2, n3, ln = struct.unpack('<5H', seg[p:p + 10])
            if st & 0o4000:                     # end of segment
                break
            name = (r50(n1) + r50(n2)).strip() + "." + r50(n3).strip()
            if (st & 0o2000) and name == want:  # permanent file
                return blk, ln
            blk += ln
            p += ent
        seg_no = nxt
    return None


def main(argv):
    if len(argv) != 3:
        print("usage: python3 -I mkdd_prep.py <kit rtv53_rl.dsk> <working copy>")
        return 2
    d = bytearray(open(argv[1], 'rb').read())
    f = find(d, "STARTA.COM")
    if f is None:
        print("STARTA.COM not found: not the RT-11 V5.3 kit?")
        return 1
    blk, ln = f
    d[blk * 512:(blk + ln) * 512] = b"\r\n\x1a" + bytes(ln * 512 - 3)   # an empty line, ^Z
    open(argv[2], 'wb').write(d)
    print("STARTA.COM (block %d, %d blocks) emptied in %s" % (blk, ln, argv[2]))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
