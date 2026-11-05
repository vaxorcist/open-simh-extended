# open-simh-extended/tools/rb730_layout_convert.py  v2
#
# Converts RB730 (VAX-11/730) disk images between the two layouts:
#
#   std  standard layout = DEC logical block order: cylinder by cylinder,
#        within a cylinder head by head, within a track sector by sector:
#            index = (cylinder * heads + head) * sectors + sector
#        Used by the RB730 simulator from 2026-11-04 on, by the RL11 (an
#        RB02 image in this layout is an RL02 image) and by DEC's drivers.
#   old  layout of earlier versions of the RB730 simulator: head by head,
#        all cylinders of surface 0 before surface 1:
#            index = head * cylinders * sectors + cylinder * sectors + sector
#        Usable with SET RBn LAYOUT=OLD until converted.
#
# Drive geometries:
#   rb02  RL02 cartridge: 512 cylinders, 2 heads, 40 sectors of 256 bytes
#         (10,485,760 bytes)
#   rb80  R80 fixed disk: 559 cylinders, 14 heads, sectors of 512 bytes:
#         standard layout 31 sectors per track, as DEC's drivers use it
#         (242,606 blocks = 124,214,272 bytes, like the RM80), old layout 32
#         sectors per track (128,212,992 bytes; VMS never uses sector 31, so
#         old2std drops it and std2old writes it as zeros)
# Shorter input files (SIMH writes only what was used) are padded with zeros;
# the output always has the full size.
#
# Usage: python3 -I rb730_layout_convert.py {old2std|std2old} {rb02|rb80} <input> <output>
# Example: rb730_layout_convert.py old2std rb80 vms.rb80 vms_std.rb80
#
# Replaces rl02_rb02_convert.py v1 (its rb2rl = old2std rb02, rl2rb = std2old rb02).

import sys

# drive: (cylinders, heads, sectors standard, sectors old, bytes per sector)
GEOM = {"rb02": (512, 2, 40, 40, 256), "rb80": (559, 14, 31, 32, 512)}


def main():
    if (len(sys.argv) != 5 or sys.argv[1] not in ("old2std", "std2old")
            or sys.argv[2] not in GEOM):
        sys.exit("usage: rb730_layout_convert.py {old2std|std2old} {rb02|rb80} <input> <output>")
    mode, drive, src, dst = sys.argv[1:]
    cyl, heads, sect_std, sect_old, nbytes = GEOM[drive]
    size_std = cyl * heads * sect_std * nbytes
    size_old = cyl * heads * sect_old * nbytes
    std = lambda c, h, s: (c * heads + h) * sect_std + s
    old = lambda c, h, s: h * cyl * sect_old + c * sect_old + s
    if mode == "old2std":
        frm, to, size_in, size_out = old, std, size_old, size_std
    else:
        frm, to, size_in, size_out = std, old, size_std, size_old
    data = open(src, "rb").read()
    if len(data) > size_in:                              # SIMH metadata footer
        print("%s: ignoring %d bytes after the %s data (SIMH disk metadata)" %
              (src, len(data) - size_in, drive.upper()))
        data = data[:size_in]
    data = data.ljust(size_in, b"\0")
    out = bytearray(size_out)                            # (old sector 31: zeros)
    for c in range(cyl):
        for h in range(heads):
            for s in range(sect_std):                    # sectors used in both layouts
                i, j = frm(c, h, s) * nbytes, to(c, h, s) * nbytes
                out[j:j + nbytes] = data[i:i + nbytes]
    open(dst, "wb").write(out)
    print("%s -> %s (%s, %s)" % (src, dst, drive.upper(), mode))


if __name__ == "__main__":
    main()
