# open-simh-extended/tools/sidriver_v47/vms_varrec.py  v1 (2026-11-19)
# Convert an RMS variable-length record file (2-byte count, word aligned),
# e.g. SYS.MAP copied from a VMS disk image, into a host text file.
# Usage: python3 -I vms_varrec.py <in> <out>
import struct, sys
d = open(sys.argv[1], 'rb').read(); out = []; p = 0
while p + 2 <= len(d):
    n = struct.unpack_from('<H', d, p)[0]; p += 2
    if n == 0xFFFF:                       # end of data in this block
        p = (p // 512 + 1) * 512 if p % 512 else p; continue
    out.append(d[p:p+n].decode('latin-1')); p += n + (n & 1)
open(sys.argv[2], 'w').write("\n".join(out) + "\n")
print(len(out), "records")
