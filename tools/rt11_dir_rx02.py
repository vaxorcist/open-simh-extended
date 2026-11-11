# open-simh-extended/tools/rt11_dir_rx02.py  v1
# Lists the RT-11 directory of an RX02 double-density image in SIMH's
# physical layout (track 0..76, sector 1..26, 256 bytes).  RT-11's DY
# handler maps logical 512-byte blocks to physical sectors starting at
# track 1 with a 2:1 interleave and a track-to-track skew of 6 sectors.
import sys, struct
img = open(sys.argv[1], 'rb').read()
SPT, BPS = 26, 256
def phys(lsec):
    # RT-11 DY mapping (as in the DY handler): logical 256-byte sector -> track, sector
    trk = lsec // SPT
    s = lsec % SPT
    s = s * 2
    if s >= SPT: s += 1                       # odd sectors on the second pass
    s = (s + 6 * trk) % SPT                   # skew
    return trk + 1, s + 1
def blk(n):
    out = b''
    for i in range(2):
        t, s = phys(n * 2 + i)
        off = (t * SPT + s - 1) * BPS
        out += img[off:off + BPS]
    return out
def r50(w):
    c = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789"
    return c[w // 1600] + c[(w // 40) % 40] + c[w % 40]
home = blk(1)
print("volume id:", home[0o730:0o744].decode('ascii', 'replace'), "owner:", home[0o744:0o760].decode('ascii','replace'), "sysid:", home[0o760:0o774].decode('ascii','replace'))
seg = 1
blkno = 6 + (seg - 1) * 2
d = blk(blkno) + blk(blkno + 1)
w = struct.unpack('<512H', d)
nseg, nxt, hi, extra, start = w[0:5]
print("segments", nseg, "next", nxt, "extra bytes", extra, "data start", start)
i = 5; pos = start; n = 0
while True:
    st = w[i]
    if st & 0o4000: break
    name = r50(w[i+1]) + r50(w[i+2]) + "." + r50(w[i+3])
    ln = w[i+4]
    if st & 0o2000: print(f"{name:12s} {ln:5d}  at {pos}"); n += 1
    pos += ln; i += 7 + extra // 2
print(n, "files, last block", pos)
