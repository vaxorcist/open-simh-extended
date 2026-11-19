# open-simh-extended/tools/sidriver_v47/sidtype.py  v1 (2026-11-19)
# Classify the base register of every memory operand of SIDRIVER (V5.5) from
# driver traces (DRV_TRACE lines: PC spec# specbyte reg value type) of the
# trace build of vax8200: which VMS structure does each displacement address?
# The structure type is the DYN$C type byte at offset 0A of the base address.
# Output: sid.types.pkl {(file_pc, operand_index): {type: count}} and a summary.
# Usage: python3 -I sidtype.py <base hex> <trace files...>
import sys, pickle, collections
sys.path.insert(0, '/home/claude/work')
import vaxdis

base = int(sys.argv[1], 16)
d = pickle.load(open('/home/claude/reg/bp/sid.code.pkl', 'rb'))
code = {k: v for k, v in d['code'].items() if v}
DYN = {1:'ADP',2:'ACB',3:'AQB',4:'CEB',5:'CRB',6:'DDB',7:'FCB',8:'FRK',9:'IDB',0xA:'IRP',0xB:'LOG',
       0xC:'PCB',0xD:'PQB',0xE:'RVT',0xF:'TQE',0x10:'UCB',0x11:'VCB',0x12:'WCB',0x13:'BUFIO'}
try:
    types = pickle.load(open('/home/claude/reg/bp/sid.types.pkl', 'rb'))
except FileNotFoundError:
    types = {}
vals = collections.defaultdict(set)
for fn in sys.argv[2:]:
    for l in open(fn):
        f = l.split()
        if len(f) < 6: continue
        pc = int(f[0], 16) - base; i = int(f[1]); v = int(f[4], 16); ty = int(f[5])
        if pc not in code: continue
        if i >= 100: i -= 100                      # base of an indexed specifier
        key = (pc, i - 1)                          # operand index (0-based)
        types.setdefault(key, collections.Counter())[ty] += 1
        vals[key].add(v)
pickle.dump(types, open('/home/claude/reg/bp/sid.types.pkl', 'wb'))
# summary: operands with displacement modes, by type
summ = collections.Counter(); mixed = []
for (pc, i), c in types.items():
    ins = code[pc]
    if i >= len(ins.ops): continue
    o = ins.ops[i]
    if o['mode'] not in ('disp', 'dispdef', 'regdef', 'autoinc', 'autoincdef', 'autodec'): continue
    t = [DYN.get(k, '%02X' % k) for k in c]
    summ[",".join(sorted(t))] += 1
    if len(c) > 1: mixed.append((pc, i, dict(c)))
print("operands seen:", len(types))
for k, n in summ.most_common(30): print("  %5d %s" % (n, k))
print("mixed:", len(mixed))
for m in mixed[:30]: print("   %05X op%d %s %s" % (m[0], m[1], m[2], repr(code[m[0]])))
