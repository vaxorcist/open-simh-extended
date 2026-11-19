# open-simh-extended/tools/sidriver_v47/sidmap.py  v6 (2026-11-19; v6: data symbols excluded; v5: DDT entries DDT-relative, SI_FATAL inline byte, fatal BUGW; v4: FDT routines are entry-relative; v3: the driver's STB symbols as entry candidates; v2: address-taken candidates are code only if they decode cleanly)
# Code map of VMS V5.5 SIDRIVER.EXE (image section loaded at 0, sid0.bin) for
# the V4.7 backport: recursive traversal from the driver's entry points
# (DDT, FDT, CRB init table entries, DPT deliver routine) and from code
# addresses taken by MOVAB/PUSHAB/MOVAL/PUSHAL/JSB/BSB; writes the
# disassembly of all code found (sid.map.dis) and a code/data byte map
# (pickle sid.code.pkl: {pc: Insn}).
# Usage: python3 -I sidmap.py <sid0.bin> <outdir>
import sys, pickle, struct
sys.path.insert(0, '/home/claude/work')
import vaxdis

mem = open(sys.argv[1], 'rb').read()
out = sys.argv[2]
SIZE = 0x5E95                                 # DPT$W_SIZE

def L(a): return struct.unpack_from('<I', mem, a)[0]
def Q(a): return struct.unpack_from('<Q', mem, a)[0]

entries = {}                                  # pc -> reason
def add(pc, why):
    if 0x100 <= pc < SIZE and pc not in entries: entries[pc] = why

DDT = 0xB4
names = ['START','UNSOLINT','FDT','CANCEL','REGDUMP','DIAG/ERRBUF','UNITINIT','ALTSTART',
         'MNTVER','CLONEDUCB','FDTSIZE','MNTV_SSSC','MNTV_FOR','MNTV_SQD','AUX_STORAGE','AUX_ROUTINE']
for i, n in enumerate(names):
    v = L(DDT + 4*i)
    if n not in ('FDT','DIAG/ERRBUF','FDTSIZE') and v < SIZE: add(DDT + v, 'DDT ' + n)   # DDT-relative
# FDT: at DDT+40 (DDT-relative), two quadword masks, then (mask quad, routine) entries
fdt = DDT + L(DDT + 8); fdtsize = L(DDT + 0x28)
p = fdt + 16
while p < fdt + fdtsize:
    add(p + L(p + 8), 'FDT mask %016X' % Q(p)); p += 12   # routine is relative to the entry
add(0x312E, 'CRB ISR'); add(0x061E, 'CRB controller init'); add(0x0714, 'CRB unit init')
add(0x05D5, 'DPT deliver')

code = {}
NORETURN = {0x4AFB: 1}
DATA_SYMS = {'$$$DTYPE','$$$SNH','BUG$_NOBUFPCKT','DEF_TRIB_PARAM','DEF_LINE_PARAM','MODEM_STATE_INFO_TABLE',
             'MODEM_TIMEOUT_TABLE','SI$OPTIONS','RELOAD_COUNT','DDCMP$L_VERSION','SI_END','DDCMP$AB_STTAB',
             'POLY','CRCTABLE'}          # SI_FATAL: bugcheck, one inline byte

def traverse(starts, code, strict):
    """Decode from starts into a new dict; returns (dict, ok)."""
    new = {}; work = list(starts); taken = {}; ok = True
    while work:
        pc = work.pop()
        while pc not in code and pc not in new and 0 <= pc < SIZE:
            ins = vaxdis.decode(mem, pc)
            if ins.bad:
                ok = False
                if strict: return new, False, taken
                print("bad decode at %05X" % pc); break
            new[pc] = ins
            for o in ins.ops:
                if o['mode'] == 'branch':
                    work.append(o['value'])
                elif o['mode'] == 'rel' and ins.name in ('JSB','JMP'):
                    work.append(o['value'])
                elif o['mode'] == 'rel' and ins.name in ('MOVAB','PUSHAB','MOVAL','PUSHAL','MOVAW','PUSHAW') and o['value'] < SIZE:
                    taken.setdefault(o['value'], []).append(pc)
            if hasattr(ins, 'case_targets'):
                work.extend(ins.case_targets)
                nxt = ins.case_base + 2 * len(ins.case_targets)
                for a in range(ins.case_base, nxt): new.setdefault(a, None)
                pc = nxt; continue
            if ins.name in vaxdis.ENDS: break
            if ins.name in ('BUGW','BUGL') and ins.ops[0]['value'] & 4: break      # fatal bugcheck
            if ins.name in ('BSBB','BSBW') and ins.ops[0]['value'] in NORETURN:
                for a in range(pc + ins.length, pc + ins.length + NORETURN[ins.ops[0]['value']]): new.setdefault(a, None)
                break                                                         # inline argument, no return
            pc += ins.length
    return new, ok, taken

new, ok, taken = traverse(sorted(entries), code, False)
code.update(new)
rejected = set()
# global symbols of the driver (SIDRIVER.STB) inside the image: code if they decode cleanly
for l in open(out + '/sidstb.txt'):
    nm, v = l.split(); v = int(v, 16)
    if nm in DATA_SYMS: continue
    if 0x100 <= v < SIZE and v not in code:
        taken.setdefault(v, []).append(-1); symname = nm
while True:
    cands = [a for a in sorted(taken) if a not in code and a not in rejected]
    if not cands: break
    for a in cands:
        n2, ok2, t2 = traverse([a], code, True)
        if ok2:
            entries[a] = 'address taken at ' + ",".join("%X" % x for x in taken[a][:3])
            code.update(n2)
            for k, v in t2.items(): taken.setdefault(k, []).extend(v)
        else:
            rejected.add(a)
print("address-taken candidates rejected as data:", " ".join("%X" % a for a in sorted(rejected)))

pickle.dump({'code': code, 'entries': entries}, open(out + '/sid.code.pkl', 'wb'))
covered = set()
for pc, ins in code.items():
    if ins: covered.update(range(pc, pc + ins.length))
    else: covered.add(pc)
with open(out + '/sid.map.dis', 'w') as f:
    for pc in sorted(k for k, v in code.items() if v):
        if pc in entries: f.write("\n; ---- %s\n" % entries[pc])
        f.write(repr(code[pc]) + "\n")
print("instructions", sum(1 for v in code.values() if v), "bytes covered", len(covered), "of", SIZE)
# uncovered ranges
gaps = []; a = 0x100
while a < SIZE:
    if a not in covered:
        b = a
        while b < SIZE and b not in covered: b += 1
        gaps.append((a, b)); a = b
    else: a += 1
print("gaps:", len(gaps), ["%X-%X" % g for g in gaps if g[1] - g[0] >= 8][:60])
