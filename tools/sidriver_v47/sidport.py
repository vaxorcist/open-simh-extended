# open-simh-extended/tools/sidriver_v47/sidport.py  v3 (2026-11-19; v3: list of the changed field offsets in sidport_offsets.txt; v2: UCB offset in the parameter table at 0x158)
# Backport of VMS V5.5's SIDRIVER.EXE (DMB32 synchronous line, VAX WAN Device
# Drivers V1.2, "SYNC V1.2-089") to VMS V4.7, by binary patching.
#
# The V5.5 driver (image section at 0, sid0.bin) is converted in place:
#   1. system addresses (SYS.STB of V5.5 -> V4.7, from both SYS.MAP files);
#   2. SMP: every "BLBC @#SMP$GL_FLAGS,x" becomes "BRW x" (V4.7 is never SMP),
#      the non-SMP lock paths set the IPL directly: device lock -> IPL 14
#      (the DMB32's device IPL), fork lock -> the fork IPL kept in the UCB/FKB
#      (V4.7 has an IPL in FKB$B_FIPL where V5 has a spinlock index);
#      spinlock indexes written to fork blocks become IPLs;
#   3. per-CPU delay loop counts (V5 CPU database) -> EXE$GL_TENUSEC/UBDELAY;
#   4. EXE$AR_TQENOREPT (V5: "don't requeue this TQE") -> a dummy TQE in the
#      driver whose TQE$B_RQTYPE has no repeat bit (V4.7's EXE$SWTIMINT then
#      does not requeue);
#   5. the V5 byte count quota routines -> small V4-style routines appended
#      (EXE$BUFFRQUOTA/EXE$ALLOCBUF/EXE$BUFQUOPRC plus JIB$L_BYTCNT, as V4.7's
#      own XGDRIVER does);
#   6. structure field offsets: UCB (fixed part 0..A3, V5 -> V4.7 by name;
#      the driver's own UCB extension from A4 stays), CRB/VEC, IDB, PCB;
#      the base structure of every operand comes from the driver traces and
#      the static register-type propagation (sid.flow.pkl);
#   7. DPT header, init/reinit tables and DDT in the V4.7 formats.
# Output: sidriver47.bin (image section) and SIDRIVER.EXE (V4.7 image).
# Usage: python3 -I sidport.py
import sys, pickle, struct, re, collections
sys.path.insert(0, '/home/claude/work')
import vaxdis

BP = '/home/claude/reg/bp/'
src = open(BP + 'sid0.bin', 'rb').read()
SIZE = 0x5E95
mem = bytearray(src[:0x6000])
d = pickle.load(open(BP + 'sid.code.pkl', 'rb'))
code = {k: v for k, v in d['code'].items() if v}
res = pickle.load(open(BP + 'sid.flow.pkl', 'rb'))
log = []
def note(s): log.append(s)

def load_map(fn):
    out = {}
    for l in open(fn):
        m = re.match(r'^([A-Z_$][\w$]*)\s+([0-9A-F]{8})(-R\S*)?\s', l)
        if m: out.setdefault(m.group(1), int(m.group(2), 16))
    return out
s55 = load_map(BP + 'sys55.map.txt'); s47 = load_map(BP + 'sys47.map.txt')
def defs(fn): return {k: int(v, 16) for k, v in (l.split() for l in open(fn))}
d55 = defs(BP + 'sd55.txt'); d47 = defs(BP + 'sd47.txt')

# ---------------------------------------------------------------- helpers
def put(addr, data, expect=None):
    if expect is not None:
        assert bytes(mem[addr:addr+len(expect)]) == expect, "%05X: %s != %s" % (addr, mem[addr:addr+len(expect)].hex(), expect.hex())
    mem[addr:addr+len(data)] = data

def L(x): return struct.pack('<I', x & 0xFFFFFFFF)

# ------------------------------------------------- 1. system addresses
SYSMAP = {}
inv55 = collections.defaultdict(list)
for k, v in s55.items(): inv55[v].append(k)
SPECIAL = {0x800045CC, 0x8000459C, 0x80004D74, 0x80004E70, 0x80003398, 0x800033B8, 0x800033D0,
           0x800031D8, 0x800031E0, 0x800031E8, 0x800031F0, 0x800031F8, 0x80003200, 0x80003258}
for pc, ins in code.items():
    for o in ins.ops:
        if o['mode'] == 'abs' and 0x80000000 <= o['value'] < 0x80010000:
            v = o['value']
            if v in SPECIAL: continue
            names = [n for n in inv55.get(v, []) if n in s47]
            assert names, "no V4.7 symbol for %08X %s at %05X" % (v, inv55.get(v), pc)
            SYSMAP[v] = s47[names[0]]
            off = pc + o['disp_off']
            put(off, L(SYSMAP[v]), L(v))
for v, n in sorted(SYSMAP.items()): note("system address %08X %-22s -> %08X" % (v, inv55[v][0], n))

# -------------------------------------------------------------- 2. SMP
nblbc = ndl = nfl = 0
for pc, ins in sorted(code.items()):
    raw = bytes(mem[pc:pc+ins.length])
    if ins.name == 'BLBC' and ins.ops[0]['mode'] == 'abs' and ins.ops[0]['value'] == 0x800045CC:
        tgt = ins.ops[1]['value']; disp = tgt - (pc + 3)
        put(pc, bytes([0x31]) + struct.pack('<h', disp) + b'\x01' * 4, raw); nblbc += 1
    elif ins.name == 'MTPR' and raw == bytes.fromhex('daa00112'):          # MTPR 1(R0),#IPL  (device lock)
        put(pc, bytes.fromhex('da141201'), raw); ndl += 1                   # MTPR #14,#IPL ; NOP
    elif ins.name == 'MTPR' and raw == bytes.fromhex('da40 9f 744d0080 12'.replace(' ', '')):  # MTPR @#IPLVEC[R0],#IPL
        put(pc, bytes.fromhex('da5012') + b'\x01' * 5, raw); nfl += 1        # MTPR R0,#IPL
note("SMP: %d BLBC @#SMP$GL_FLAGS -> BRW, %d device lock paths -> MTPR #14, %d fork lock paths -> MTPR R0" % (nblbc, ndl, nfl))
for pc, ins in code.items():
    for o in ins.ops:
        if o['mode'] == 'abs' and o['value'] == 0x800045CC: assert ins.name == 'BLBC', repr(ins)
# spinlock index -> IPL for fork blocks / UCB$B_FLCK
SPL2IPL = {0x34: 8, 0x36: 6, 0x23: 0x18, 0x32: 8, 0x2F: 8, 0x31: 8}
for pc, ins in code.items():
    if ins.name == 'MOVB' and ins.ops[1]['mode'] == 'disp' and ins.ops[1]['value'] == 0xB and ins.ops[0]['mode'] == 'lit':
        v = ins.ops[0]['value']
        if v in SPL2IPL and v != 5:
            put(pc + 1, bytes([SPL2IPL[v]]), bytes([v])); note("%05X MOVB #%X,B(Rn): spinlock index -> IPL %d" % (pc, v, SPL2IPL[v]))

# -------------------------------------------- 3. per-CPU delay counts
tenusec = s47['EXE$GL_TENUSEC']; assert s47['EXE$GL_UBDELAY'] == tenusec + 4
n = 0
for pc, ins in sorted(code.items()):
    if ins.name == 'BICL2' and ins.ops[0]['mode'] == 'abs' and ins.ops[0]['value'] == 0x8000459C:
        p0 = pc - 3
        assert bytes(mem[p0:p0+3]) == bytes.fromhex('db0450')                # MFPR #4,R0
        put(p0, bytes([0x9E, 0x9F]) + L(tenusec - 0x270) + bytes([0x50]) + b'\x01' * 3)  # MOVAB @#TENUSEC-270,R0
        n += 1
note("per-CPU delay loops -> EXE$GL_TENUSEC/UBDELAY: %d" % n)

# ------------------------------------- 4/5. appended code and data
end = (SIZE + 3) & ~3
TQE_DUMMY = end; end += 0x34                     # zero block, TQE$B_RQTYPE = 0: no repeat
JIB = d47['PCB$L_JIB']; BYT = d47['JIB$L_BYTCNT']
def asm_shim_alo(at):
    # R1 = size, R4 = PCB -> R0 status, R1 size, R2 buffer; quota debited
    b = bytearray()
    b += bytes([0x16, 0x9F]) + L(s47['EXE$BUFFRQUOTA'])       # JSB @#EXE$BUFFRQUOTA
    b += bytes([0xE9, 0x50, 0x00])                            # BLBC R0,ret (patched below)
    p1 = len(b) - 1
    b += bytes([0x16, 0x9F]) + L(s47['EXE$ALLOCBUF'])         # JSB @#EXE$ALLOCBUF
    b += bytes([0xE9, 0x50, 0x00]); p2 = len(b) - 1           # BLBC R0,ret
    b += bytes([0xDD, 0x50])                                  # PUSHL R0
    b += bytes([0xD0, 0xC4]) + struct.pack('<h', JIB) + bytes([0x50])   # MOVL W^JIB(R4),R0
    b += bytes([0xC2, 0x51, 0xA0, BYT])                       # SUBL2 R1,B^BYTCNT(R0)
    b += bytes([0xD0, 0x8E, 0x50])                            # MOVL (SP)+,R0
    r = len(b); b += bytes([0x05])                            # RSB
    b[p1] = r - (p1 + 1); b[p2] = r - (p2 + 1)
    return bytes(b)
def asm_shim_nw(at):
    # R1 = size (bit 31 = no wait flag in V5), R4 = PCB -> R0 status; quota debited
    b = bytearray()
    b += bytes([0xCA, 0x8F]) + L(0x80000000) + bytes([0x51])  # BICL2 #80000000,R1
    b += bytes([0xDD, 0x51])                                  # PUSHL R1
    b += bytes([0x16, 0x9F]) + L(s47['EXE$BUFQUOPRC'])        # JSB @#EXE$BUFQUOPRC
    b += bytes([0xE9, 0x50, 0x00]); p1 = len(b) - 1           # BLBC R0,1$
    b += bytes([0xD0, 0xC4]) + struct.pack('<h', JIB) + bytes([0x51])   # MOVL W^JIB(R4),R1
    b += bytes([0xC2, 0x6E, 0xA1, BYT])                       # SUBL2 (SP),B^BYTCNT(R1)
    r = len(b); b += bytes([0xD0, 0x8E, 0x51, 0x05])          # 1$: MOVL (SP)+,R1 ; RSB
    b[p1] = r - (p1 + 1)
    return bytes(b)
def asm_shim_credit(at):
    # R0 = byte count, R4 = PCB: give the quota back
    b = bytearray()
    b += bytes([0xDD, 0x51])                                  # PUSHL R1
    b += bytes([0xD0, 0xC4]) + struct.pack('<h', JIB) + bytes([0x51])   # MOVL W^JIB(R4),R1
    b += bytes([0xC0, 0x50, 0xA1, BYT])                       # ADDL2 R0,B^BYTCNT(R1)
    b += bytes([0xD0, 0x8E, 0x51, 0x05])                      # MOVL (SP)+,R1 ; RSB
    return bytes(b)
SHIM = {}
for name, f in (('ALO', asm_shim_alo), ('NW', asm_shim_nw), ('CREDIT', asm_shim_credit)):
    c = f(end); SHIM[name] = end; put(end, c); end += len(c)
assert end <= 0x6000, hex(end)
VMAP = {0x80003398: SHIM['ALO'], 0x800033D0: SHIM['NW'], 0x800033B8: SHIM['CREDIT']}
for pc, ins in code.items():
    if ins.name == 'JSB' and ins.ops[0]['mode'] == 'abs' and ins.ops[0]['value'] in VMAP:
        tgt = VMAP[ins.ops[0]['value']]
        put(pc, bytes([0x16, 0xEF]) + L(tgt - (pc + 6)), bytes([0x16, 0x9F]) + L(ins.ops[0]['value']))
        note("%05X JSB quota routine -> appended routine at %05X" % (pc, tgt))
    if ins.name == 'MOVL' and ins.ops[0]['mode'] == 'abs' and ins.ops[0]['value'] == 0x80004E70:
        rn = ins.ops[1]['reg']
        put(pc, bytes([0x9E, 0xEF]) + L(TQE_DUMMY - (pc + 6)) + bytes([0x50 | rn]))
        note("%05X EXE$AR_TQENOREPT -> dummy TQE %05X" % (pc, TQE_DUMMY))

# -------------------------------------------------- 6. field offsets
um = pickle.load(open(BP + 'ucbmap.pkl', 'rb'))
UCB_FIXED = 0xA4
def crbmap(x):
    if 0x50 <= x < 0x50 + 0x24: return x - 0x50 + 0x24
    if 0x88 <= x < 0x88 + 0x24: return x - 0x88 + 0x48
    names = [k for k, v in d55.items() if k.startswith('CRB$') and v == x and k in d47]
    return d47[names[0]] if names else None
IDBMAP = {0x18: 0x14, 0x1C: 0x18}
def pcbmap(x):
    names = [k for k, v in d55.items() if k.startswith('PCB$') and k[4] in 'BWLQT' and v == x and k in d47]
    return d47[names[0]] if names else x
NOT_UCB = {(0x2AF4, 0)}                          # R5 is a TQE there
cnt = collections.Counter(); unk = []; offlog = []
for pc, ins in sorted(code.items()):
    for k, o in enumerate(ins.ops):
        if o['mode'] not in ('disp', 'dispdef') or o['reg'] in (14, 15): continue
        t = res.get((pc, k))
        if t in (None, 'MIX') and o['reg'] == 5 and (pc, k) not in NOT_UCB: t = 'UCB'
        x = o['value']; new = None
        if t == 'UCB' and 0 <= x < UCB_FIXED:
            if x in (0x28, 0x29, 0x2A, 0x2B): cnt['UCB$L_DLCK (left)'] += 1; continue
            new = um.get(x)
            if new is None: unk.append("%05X op%d UCB+%X has no V4.7 field: %r" % (pc, k, x, ins)); continue
        elif t and t.startswith('UCB+'):
            base = int(t[4:], 16)
            if base + x < UCB_FIXED and base < UCB_FIXED: unk.append("%05X op%d interior UCB+%X+%X: %r" % (pc, k, base, x, ins))
            continue
        elif t == 'CRB': new = crbmap(x)
        elif t == 'IDB': new = IDBMAP.get(x, x)
        elif t == 'PCB': new = pcbmap(x)
        else: continue
        if new is None: unk.append("%05X op%d %s+%X unmapped: %r" % (pc, k, t, x, ins)); continue
        if new == x: cnt[t + ' same'] += 1; continue
        off = pc + o['disp_off']; n = o['disp_len']
        lim = {1: 0x7F, 2: 0x7FFF, 4: 0x7FFFFFFF}[n]
        assert 0 <= new <= lim
        put(off, new.to_bytes(n, 'little'), x.to_bytes(n, 'little', signed=True)); cnt[t + ' changed'] += 1
        offlog.append("%05X op%d %-4s %3X -> %3X  %r" % (pc, k, t, x, new, ins))
note("field offsets: %s" % dict(cnt))
for u in unk: note("CHECK " + u)

# parameter table at 0x158 (DECnet line/circuit parameters stored relative to
# the UCB by routines 44DC/45D3/46F2): entry offsets < A4 are UCB fields;
# only the buffer size (code AF1 -> UCB$W_DEVBUFSIZ, V5 4E) is one
for p in range(0x158, 0x1F4, 2):
    w = struct.unpack_from('<H', mem, p)[0]
    if w == 0x8000 | 0x4E and struct.unpack_from('<H', mem, p - 2)[0] & 0xFFF == 0xAF1:
        struct.pack_into('<H', mem, p, 0x8000 | um[0x4E]); note("param table %X: UCB$W_DEVBUFSIZ 4E -> %X" % (p, um[0x4E]))

# DPT flags (PC-relative BISB2/BICB2 #4,DPT$B_FLAGS): V5 0x10 -> V4.7 0x0D
for pc, ins in code.items():
    for o in ins.ops:
        if o['mode'] == 'rel' and o['value'] == 0x10:
            off = pc + o['disp_off']; n = o['disp_len']
            dd = int.from_bytes(mem[off:off+n], 'little', signed=True) - 3
            put(off, dd.to_bytes(n, 'little', signed=True)); note("%05X DPT$B_FLAGS reference 10 -> 0D" % pc)

# ------------------------------------------- 7. DPT, init tables, DDT
# V5.5 DPT (at 0): FLINK, BLINK, SIZE w(8), TYPE b(A), REFC b(B), ADPTYPE b(C),
#   (D), UCBSIZE w(E), FLAGS l(10), INITTAB w(14), REINITTAB w(16), UNLOAD w(18),
#   MAXUNITS w(1A), VERSION w(1C), DEFUNITS w(1E), DELIVER w(20), VECTOR w(22),
#   NAME t(24), LINKTIME q(30), ECOLEVEL l(38) ...
# V4.7 DPT: SIZE w(8), TYPE b(A), REFC b(B), ADPTYPE b(C), FLAGS b(D),
#   UCBSIZE w(E), INITTAB w(10), REINITTAB w(12), UNLOAD w(14), MAXUNITS w(16),
#   VERSION w(18), DEFUNITS w(1A), DELIVER w(1C), VECTOR w(1E), NAME t(20),
#   LINKTIME q(2C), ECOLEVEL l(34), length 38
o = src
def W(a): return struct.unpack_from('<H', o, a)[0]
flags5 = struct.unpack_from('<I', o, 0x10)[0]
newdpt = bytearray(0x38)
struct.pack_into('<H', newdpt, 8, end)                 # DPT$W_SIZE: driver with the appended code
newdpt[0xA] = o[0xA]; newdpt[0xB] = 0; newdpt[0xC] = o[0xC]
newdpt[0xD] = flags5 & 0xFF & ~0x00                     # SUBCNTRL, SVP, NOUNLOAD (bits 0-2 same)
struct.pack_into('<H', newdpt, 0xE, W(0xE))            # UCB size 21A
struct.pack_into('<HHHHHHHH', newdpt, 0x10, W(0x14), W(0x16), W(0x18), W(0x1A), 4, W(0x1E), W(0x20), W(0x22))
newdpt[0x20:0x2C] = o[0x24:0x30]                       # counted name "SIDRIVER"
newdpt[0x2C:0x34] = o[0x30:0x38]; newdpt[0x34:0x38] = o[0x38:0x3C]
mem[0:0x82] = b'\0' * 0x82
mem[0:0x38] = newdpt
note("DPT: V4.7 layout, size %X, flags %02X, version 4" % (end, newdpt[0xD]))

STRUC = {2: 0x10, 4: 6, 5: 5, 3: 9, 1: None}           # V5 DPT$K_STRUC_* -> V4 DYN$C_*
def conv_table(a):
    p = a
    while mem[p] != 0:
        s = mem[p]; assert not (s & 0x80), "word offset entry at %X" % p
        st = s & 0x0F; off = mem[p+1]; op = mem[p+2]
        size = {0: 1, 1: 2, 2: 2, 3: 4}[op]
        val = int.from_bytes(mem[p+3:p+3+size], 'little')
        newst = STRUC[st]; newoff = off
        if st == 2:
            if off == 0x0B and op == 0: val = SPL2IPL[val]           # UCB$B_FLCK -> FIPL
            newoff = um[off] if off < UCB_FIXED else off
        elif st == 5: newoff = crbmap(off)
        elif st == 4: newoff = off                                  # DDB same
        note("init entry %X: struct %d off %X op %d val %X -> struct %02X off %X" % (p, st, off, op, val, newst, newoff))
        mem[p] = newst; mem[p+1] = newoff
        mem[p+3:p+3+size] = val.to_bytes(size, 'little')
        p += 3 + size
conv_table(W(0x14))                    # the reinit table (0x9E) is the tail of the init table
DDT = 0xB4
for i in range(0, 0x40, 4):
    v = struct.unpack_from('<I', mem, DDT + i)[0]
    if v in (0x80002A28, 0x80002938):
        put(DDT + i, L(s47['IOC$RETURN'] if v == 0x80002A28 else s47['IOC$MNTVER']))
note("DDT: IOC$RETURN/IOC$MNTVER entries mapped")

open(BP + 'sidriver47.bin', 'wb').write(bytes(mem))
# image file: V4.7 image header (from V4.7 XGDRIVER.EXE), one ISD of 48 pages at VBN 2
hdr = bytearray(open(BP + 'xg47.exe', 'rb').read()[:512])
hsize = struct.unpack_from('<H', hdr, 0)[0]
struct.pack_into('<H', hdr, hsize + 2, 0x30)           # ISD$W_PAGCNT
struct.pack_into('<I', hdr, hsize + 12, 2)             # ISD$L_VBN
imgid = struct.unpack_from('<H', hdr, 6)[0]            # IHD$W_IMGIDOFF
nm = b'SIDRIVER'; hdr[imgid] = len(nm); hdr[imgid+1:imgid+1+len(nm)] = nm
open(BP + 'SIDRIVER.EXE', 'wb').write(bytes(hdr) + bytes(mem))
open(BP + 'sidport.log', 'w').write("\n".join(log) + "\n")
open(BP + 'sidport_offsets.txt', 'w').write("\n".join(offlog) + "\n")
print("\n".join(l for l in log if not l.startswith('init entry') and 'quota routine' not in l))
