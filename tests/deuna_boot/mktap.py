# open-simh-extended/tests/deuna_boot/mktap.py  v1
#
# Writes files onto a SIMH tape image (.tap: each record is a 4-byte
# little-endian length, the data padded to an even length, the length
# again; 0 is a tape mark): every file becomes one tape file of 512-byte
# records followed by a tape mark; the tape ends with a second tape mark.
# VMS reads them with MOUNT/FOREIGN/BLOCKSIZE=512/RECORDSIZE=512 and one
# COPY per file, e.g. COPY MSA0: SYS$SYSTEM:SECUNA.SYS.
#
# usage: python3 -I mktap.py OUT.tap FILE...
import struct
import sys

out = open(sys.argv[1], 'wb')
for name in sys.argv[2:]:
    d = open(name, 'rb').read()
    if len(d) % 512:
        d += bytes(512 - len(d) % 512)
    for i in range(0, len(d), 512):
        out.write(struct.pack('<I', 512) + d[i:i + 512] + struct.pack('<I', 512))
    out.write(struct.pack('<I', 0))
    print("%s: %d records" % (name, len(d) // 512))
out.write(struct.pack('<I', 0))
out.close()
