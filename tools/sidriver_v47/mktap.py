# open-simh-extended/tools/sidriver_v47/mktap.py  v1 (2026-11-19)
# Write a file as a SIMH tape image: 512-byte records, then two tape marks.
# In VMS: MOUNT/FOREIGN/BLOCKSIZE=512/RECORDSIZE=512 MUA0: and COPY MUA0: file.
# Usage: python3 -I mktap.py <input file> <output .tap>
import struct, sys
d = open(sys.argv[1], 'rb').read()
d += b'\0' * (-len(d) % 512)
with open(sys.argv[2], 'wb') as f:
    for i in range(0, len(d), 512):
        f.write(struct.pack('<I', 512) + d[i:i+512] + struct.pack('<I', 512))
    f.write(b'\0' * 8)
print(len(d) // 512, "records")
