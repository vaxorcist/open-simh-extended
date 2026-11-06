# open-simh-extended/tools/make_vmb_seek_disk.py  v2
# /home/claude/simh-sound/tools/make_vmb_seek_disk.py  v2
#
# Makes a blank MSCP disk image (raw, LBN order) whose block 0 is a VAX
# boot block for the MicroVAX VMB: it tells VMB to load SIZE blocks
# starting at LBN and to start them.  The loaded blocks are zero, i.e. a
# HALT instruction, so the console comes back with "?06 HLT INST".  Used to
# drive realistic MSCP transfers (seek and transfer times, seek sounds)
# from the console without an operating system.
#
# Boot block layout as documented in 4.3BSD-Reno sys/vax/mdec/rdboot.c:
# bytes 0-1 any value, byte 2 the word offset of the identification area,
# byte 3 must be 1, bytes 4-7 the LBN of the secondary image (word
# swapped: high word first).  Identification area: 0x18 (VAX instruction
# set), controller type (0 = unknown), file structure (any), checksum
# (ones complement of the sum of the three bytes before it), a zero byte,
# 1 or 0x81, two bytes of any value, then the size in blocks, the load
# offset, the entry offset and the sum of these three longwords.
# (v1 had the NetBSD field order shifted by two bytes and was rejected by
# VMB with ?43 FILESTRUCT.)
#
# Usage: python3 -I tools/make_vmb_seek_disk.py <image> <blocks> <lbn> <size>
import struct
import sys

def main():
    if len(sys.argv) != 5:
        sys.exit("usage: make_vmb_seek_disk.py <image> <blocks> <lbn> <size>")
    path, blocks, lbn, size = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    bb = bytearray(512)
    id_off = 8                                     # identification area (bytes)
    bb[2] = id_off // 2                            # its offset in words
    bb[3] = 1                                      # must be one
    struct.pack_into("<HH", bb, 4, (lbn >> 16) & 0xFFFF, lbn & 0xFFFF)   # LBN, high word first
    iset, ctrl, fstr = 0x18, 0, 0
    bb[id_off + 0] = iset                          # VAX instruction set
    bb[id_off + 1] = ctrl                          # controller type: unknown
    bb[id_off + 2] = fstr                          # file structure: any
    bb[id_off + 3] = (~(iset + ctrl + fstr)) & 0xFF    # checksum of the three bytes
    bb[id_off + 4] = 0                             # must be zero
    bb[id_off + 5] = 0x81                          # must be 1 or 0x81
    load, entry = 0, 0
    struct.pack_into("<IIII", bb, id_off + 8, size, load, entry, (size + load + entry) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(bb)
        f.truncate(blocks * 512)                   # the rest stays zero
    print("%s: %d blocks, boot block -> LBN %d, %d blocks" % (path, blocks, lbn, size))

if __name__ == "__main__":
    main()
