# open-simh-extended/tools/sidriver_v47/vms_isd.py  v1 (2026-11-18)
# List the image section descriptors (ISDs) of a VAX/VMS executable image:
# size, page count, base VPN/address, flags, file VBN.  Used to lay out an
# image (e.g. TMSCP.EXE, SYSGEN.EXE) in SIMH memory for disassembly.
# Usage: python3 -I vms_isd.py <image.exe>
import struct, sys
d = open(sys.argv[1], 'rb').read()
hsize, activoff, symdbgoff, imgidoff, patchoff = struct.unpack_from('<HHHHH', d, 0)
print("header size %d, activoff %X, imgidoff %X" % (hsize, activoff, imgidoff))
if activoff:
    print("transfer addresses:", ["%08X" % x for x in struct.unpack_from('<III', d, activoff)])
off = hsize
while True:
    isize = struct.unpack_from('<H', d, off)[0]
    if isize == 0 or isize == 0xFFFF:
        break
    pagcnt, vpnword, flags = struct.unpack_from('<HIi', d, off + 2)
    vbn = struct.unpack_from('<I', d, off + 12)[0] if isize >= 16 else 0
    vpn = vpnword & 0x1FFFFF
    print("ISD size %2d pages %3d vpn %06X addr %08X flags %08X vbn %d" %
          (isize, pagcnt, vpn, (vpnword & 0xFFE00000) * 0 + (vpn << 9) | (vpnword & 0x00200000) << 10, flags & 0xFFFFFFFF, vbn))
    off += isize
    if off % 512 > 510:
        off = (off // 512 + 1) * 512
