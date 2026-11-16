#!/usr/bin/env python3
# open-simh-extended/tools/vhd_read_blocks.py  v1
# Reads logical blocks (512 bytes) from a dynamic (or fixed) VHD disk image,
# as SIMH writes them, and writes them to a host file.
#
# Usage:  vhd_read_blocks.py <image.vhd> <out file> <lbn>:<count> [<lbn>:<count> ...]
#         (the extents in file order, e.g. from VMS DUMP/HEADER retrieval
#         pointers; the blocks are written one after the other)
#
# VHD format (Microsoft VHD specification): a 512-byte footer at the end of
# the file ("conectix"); for a dynamic disk a dynamic header at the footer's
# data offset ("cxsparse") with the block allocation table (BAT, big-endian
# 32-bit sector numbers, 0xFFFFFFFF = not allocated) and the block size;
# each data block starts with a sector bitmap (rounded up to 512 bytes),
# then the block's sectors.  Unallocated sectors read as zeros.
import struct, sys

def open_vhd(path):
    f = open(path, "rb")
    f.seek(-512, 2)
    foot = f.read(512)
    if foot[0:8] != b"conectix":
        raise SystemExit("no VHD footer")
    dtype = struct.unpack(">I", foot[60:64])[0]          # 2 fixed, 3 dynamic
    if dtype == 2:
        return f, None
    doff = struct.unpack(">Q", foot[16:24])[0]
    f.seek(doff)
    dh = f.read(1024)
    if dh[0:8] != b"cxsparse":
        raise SystemExit("no dynamic header")
    batoff = struct.unpack(">Q", dh[16:24])[0]
    nbat = struct.unpack(">I", dh[28:32])[0]
    bsize = struct.unpack(">I", dh[32:36])[0]
    f.seek(batoff)
    bat = struct.unpack(">%dI" % nbat, f.read(4 * nbat))
    spb = bsize // 512                                    # sectors per block
    bmsec = (spb // 8 + 511) // 512                       # bitmap sectors
    return f, (bat, spb, bmsec)

def read_sector(f, info, lbn):
    if info is None:                                      # fixed disk
        f.seek(lbn * 512)
        return f.read(512)
    bat, spb, bmsec = info
    b, s = divmod(lbn, spb)
    if b >= len(bat) or bat[b] == 0xFFFFFFFF:
        return bytes(512)
    f.seek((bat[b] + bmsec + s) * 512)
    return f.read(512)

if __name__ == "__main__":
    f, info = open_vhd(sys.argv[1])
    with open(sys.argv[2], "wb") as out:
        for ext in sys.argv[3:]:
            lbn, cnt = (int(x) for x in ext.split(":"))
            for i in range(cnt):
                out.write(read_sector(f, info, lbn + i))
