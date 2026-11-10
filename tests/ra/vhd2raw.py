# open-simh-extended/tests/ra/vhd2raw.py  v1
# vhd2raw.py: dynamic (or fixed) VHD -> flat raw image of the virtual size
import sys, struct
src, dst = sys.argv[1], sys.argv[2]
f = open(src, 'rb'); f.seek(0, 2); n = f.tell()
f.seek(n - 512); foot = f.read(512)
assert foot[:8] == b'conectix', foot[:8]
dtype = struct.unpack('>I', foot[60:64])[0]
size = struct.unpack('>Q', foot[48:56])[0]
out = open(dst, 'wb')
if dtype == 2:                                   # fixed
    f.seek(0); left = size
    while left: b = f.read(min(left, 1 << 20)); out.write(b); left -= len(b)
else:
    hoff = struct.unpack('>Q', foot[16:24])[0]
    f.seek(hoff); h = f.read(1024)
    assert h[:8] == b'cxsparse'
    bat_off = struct.unpack('>Q', h[16:24])[0]
    nent = struct.unpack('>I', h[28:32])[0]
    bsz = struct.unpack('>I', h[32:36])[0]
    f.seek(bat_off); bat = struct.unpack('>%dI' % nent, f.read(4 * nent))
    bmap = ((bsz // 512) + 7) // 8; bmap = (bmap + 511) // 512 * 512
    for i, e in enumerate(bat):
        out.seek(i * bsz)
        if e == 0xFFFFFFFF: continue
        f.seek(e * 512 + bmap); out.write(f.read(bsz))
    out.truncate(size)
print(dst, size, 'bytes, type', dtype)
