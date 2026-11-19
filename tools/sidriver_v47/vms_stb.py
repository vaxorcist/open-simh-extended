# open-simh-extended/tools/sidriver_v47/vms_stb.py  v2 (2026-11-19; v2: GSY definitions carry a psect index)
# Dump the global symbol definitions of a VAX/VMS object or symbol table file
# (.OBJ/.STB: variable-length records, GSD records type 1, entries GSY (1) and
# EPM (2)) as "NAME VALUE" lines.
# Usage: python3 -I vms_stb.py <file.stb>
import struct, sys
d = open(sys.argv[1], 'rb').read(); p = 0; out = {}
while p + 2 <= len(d):
    n = struct.unpack_from('<H', d, p)[0]; p += 2
    if n == 0xFFFF or n == 0:
        if p % 512: p = (p // 512 + 1) * 512
        continue
    r = d[p:p+n]; p += n + (n & 1)
    if r[0] != 1: continue                 # GSD only
    q = 1
    while q < len(r):
        t = r[q]
        if t == 1:   # GSY definition: type, datatyp, flags(2), psect(1), value(4), name
            flags = struct.unpack_from('<H', r, q+2)[0]
            if flags & 2:                  # definition
                val = struct.unpack_from('<I', r, q+5)[0]; ln = r[q+9]
                out[r[q+10:q+10+ln].decode('latin-1')] = val; q += 10 + ln
            else:
                ln = r[q+4]; q += 5 + ln
        elif t == 2: # EPM: type, datatyp, flags(2), psect(1), value(4), mask(2), name
            val = struct.unpack_from('<I', r, q+5)[0]; ln = r[q+11]
            out[r[q+12:q+12+ln].decode('latin-1')] = val; q += 12 + ln
        elif t == 0: # PSC: type, align, flags(2), alloc(4), name
            ln = r[q+8]; q += 9 + ln
        else:
            break
for k in sorted(out): print(k, "%08X" % out[k])
