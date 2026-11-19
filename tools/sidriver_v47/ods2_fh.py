# open-simh-extended/tools/sidriver_v47/ods2_fh.py  v1 (2026-11-19)
# Read a file from a raw ODS-2 disk image given the LBN of its file header
# (found e.g. by searching the index file for the name): decode the retrieval
# pointers of the header's map area and write the file's blocks.
# Usage: python3 -I ods2_fh.py <raw image> <header LBN> <output file> [blocks]
import struct, sys
img, hdr_lbn, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
f = open(img, 'rb')
f.seek(hdr_lbn * 512); h = f.read(512)
idoff, mpoff, acoff, rsoff = h[0], h[1], h[2], h[3]
inuse = h[0x3A]
# record attributes at 0x14: FAT$L_HIBLK at 0x14+4 (word-swapped), EFBLK at 0x14+8
def swap(x): return ((x & 0xFFFF) << 16) | (x >> 16)
efblk = swap(struct.unpack_from('<I', h, 0x14 + 8)[0])
ffbyte = struct.unpack_from('<H', h, 0x14 + 12)[0]
name = h[idoff*2:idoff*2+20]
print("name", name, "map words", inuse, "eof block", efblk, "ffbyte", ffbyte)
p = mpoff * 2; end = p + inuse * 2; ext = []
while p < end:
    w0 = struct.unpack_from('<H', h, p)[0]; fmt = w0 >> 14
    if fmt == 1:
        cnt = (w0 & 0xFF) + 1; lbn = ((w0 >> 8) & 0x3F) << 16 | struct.unpack_from('<H', h, p+2)[0]; p += 4
    elif fmt == 2:
        cnt = (w0 & 0x3FFF) + 1; lbn = struct.unpack_from('<I', h, p+2)[0]; p += 6
    elif fmt == 3:
        cnt = (((w0 & 0x3FFF) << 16) | struct.unpack_from('<H', h, p+2)[0]) + 1; lbn = struct.unpack_from('<I', h, p+4)[0]; p += 8
    else:
        p += 2; continue
    ext.append((lbn, cnt))
print("extents", ext)
nblk = int(sys.argv[4]) if len(sys.argv) > 4 else (efblk - 1 + (1 if ffbyte else 0))
data = b''
for lbn, cnt in ext:
    f.seek(lbn * 512); data += f.read(cnt * 512)
open(out, 'wb').write(data[:nblk * 512])
print("wrote", nblk, "blocks")
