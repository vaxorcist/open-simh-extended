# open-simh-extended/tools/sidriver_v47/sidflow.py  v1 (2026-11-19)
# Static register-type propagation over SIDRIVER's code (V4.7 backport):
# which VMS structure does each base register point to at each displacement
# operand?  Seeds: the driver traces (sid.types.pkl, observed DYN type of the
# base), routine entry conventions (FDT, start I/O, cancel, ...), and loads
# learned from the traces (a pointer loaded from field D of structure T has
# the type seen when it is used).  Propagation follows fall-through and
# branch edges inside routines and from call sites into internal callees; a
# call clobbers R0-R2 in the caller.
# Output: sid.flow.pkl {(pc, operand): type-set} and a report.
# Usage: python3 -I sidflow.py
import sys, pickle, collections
sys.path.insert(0, '/home/claude/work')
import vaxdis

BP = '/home/claude/reg/bp/'
d = pickle.load(open(BP + 'sid.code.pkl', 'rb'))
code = {k: v for k, v in d['code'].items() if v}
entries = d['entries']
types = pickle.load(open(BP + 'sid.types.pkl', 'rb'))
DYN = {1:'ADP',5:'CRB',6:'DDB',9:'IDB',0xA:'IRP',0xC:'PCB',0xF:'TQE',0x10:'UCB',0x13:'BUFIO',-1:'IO'}

def obs(pc, i):
    c = types.get((pc, i))
    if not c: return None
    t, n = c.most_common(1)[0]
    if n / sum(c.values()) < 0.95: return None
    return DYN.get(t, 'T%02X' % t)

MEMMODES = ('disp', 'dispdef', 'regdef')
# ---- learned loads: MOVL disp(Rb),Rn executed with base type T; type of Rn at its next use
learn = collections.defaultdict(collections.Counter)

def writes(ins):
    """registers written by an instruction (approximate)."""
    w = set()
    n = ins.name
    for k, o in enumerate(ins.ops):
        if o['mode'] == 'reg' and o['access'] in ('w', 'm'):
            w.add(o['reg'])
            if o['size'] == 8: w.add(o['reg'] + 1)
        if o['mode'] in ('autoinc', 'autodec', 'autoincdef') and o['reg'] != 14:
            w.add(o['reg'])
    if n in ('MOVC3', 'MOVC5', 'CMPC3', 'CMPC5', 'LOCC', 'SKPC', 'SCANC', 'SPANC', 'MATCHC', 'MOVTC', 'MOVTUC', 'CRC'):
        w.update({0, 1, 2, 3, 4, 5})
    if n in ('EDIV', 'EMUL'): pass
    if n == 'POPR':
        m = ins.ops[0]['value'] if ins.ops[0]['mode'] in ('lit', 'imm') else 0xFFF
        w.update(r for r in range(14) if m & (1 << r))
    return w

def src_type(state, o, pc, i):
    """type of the value an operand denotes as a pointer (for MOVL/MOVAx sources)."""
    if o['mode'] == 'reg': return state.get(o['reg'])
    return None

# entry conventions
CONV = {
 'FDT': {3: 'IRP', 4: 'PCB', 5: 'UCB'},
 'DDT START': {3: 'IRP', 5: 'UCB'},
 'DDT ALTSTART': {3: 'IRP', 5: 'UCB'},
 'DDT CANCEL': {3: 'IRP', 4: 'PCB', 5: 'UCB'},
 'DDT REGDUMP': {5: 'UCB'},
 'CRB unit init': {5: 'UCB'},
 'CRB controller init': {5: 'IDB'},
}

succ = {}
pcs = sorted(code)
for pc in pcs:
    ins = code[pc]; s = []
    nxt = pc + ins.length
    for o in ins.ops:
        if o['mode'] == 'branch' and ins.name not in ('BSBB', 'BSBW'): s.append(('br', o['value']))
    if hasattr(ins, 'case_targets'):
        s += [('br', t) for t in ins.case_targets]; nxt = ins.case_base + 2 * len(ins.case_targets)
    if ins.name not in vaxdis.ENDS and nxt in code: s.append(('ft', nxt))
    if ins.name in ('BSBB', 'BSBW', 'JSB'):
        tgt = ins.ops[0]['value'] if ins.ops[0]['mode'] in ('branch', 'rel') else None
        if tgt is not None and tgt in code: s.append(('call', tgt))
    succ[pc] = s

# iterate: states at instruction entry: reg -> type (single) ; conflicting -> 'MIX'
state = {}
def merge(a, b):
    out = dict(a); ch = False
    for r, t in b.items():
        if r not in out: out[r] = t; ch = True
        elif out[r] != t and out[r] != 'MIX': out[r] = 'MIX'; ch = True
    return out, ch

work = []
for pc, why in entries.items():
    if pc not in code: continue
    seed = {}
    for k, v in CONV.items():
        if why.startswith(k): seed = dict(v)
    state[pc] = seed; work.append(pc)

# learned load table from observed operand types
def learn_pass():
    for pc in pcs:
        ins = code[pc]
        if ins.name in ('MOVL',) and len(ins.ops) == 2 and ins.ops[1]['mode'] == 'reg' and ins.ops[0]['mode'] == 'disp':
            bt = obs(pc, 0)
            rn = ins.ops[1]['reg']
            # find next use of rn as base in fall-through chain with observation
            q = pc + ins.length; steps = 0
            while q in code and steps < 12:
                i2 = code[q]
                for k, o in enumerate(i2.ops):
                    if o['mode'] in MEMMODES and o['reg'] == rn and obs(q, k):
                        learn[(bt, ins.ops[0]['value'])][obs(q, k)] += 1; steps = 99; break
                if steps == 99 or rn in writes(i2) or i2.name in vaxdis.ENDS: break
                q += i2.length; steps += 1
learn_pass()

def transfer(pc, st):
    ins = code[pc]; st = dict(st)
    # observations refine the state of base registers
    for k, o in enumerate(ins.ops):
        if o['mode'] in MEMMODES and o['reg'] not in (14, 15):
            t = obs(pc, k)
            if t: st[o['reg']] = t
    w = writes(ins)
    newt = {}
    if ins.name in ('MOVL', 'MOVAL', 'MOVAB') and ins.ops[1]['mode'] == 'reg':
        s = ins.ops[0]; rn = ins.ops[1]['reg']
        if ins.name == 'MOVL' and s['mode'] == 'reg': newt[rn] = st.get(s['reg'])
        elif ins.name == 'MOVL' and s['mode'] == 'disp':
            bt = obs(pc, 0) or st.get(s['reg'])
            c = learn.get((bt, s['value']))
            if c: newt[rn] = c.most_common(1)[0][0]
        elif ins.name in ('MOVAL', 'MOVAB') and s['mode'] == 'disp' and s['value'] != 0:
            bt = obs(pc, 0) or st.get(s['reg'])
            if bt and bt not in ('MIX',): newt[rn] = '%s+%X' % (bt, s['value'])
    for r in w: st.pop(r, None)
    for r, t in newt.items():
        if t: st[r] = t
    return st

it = 0
while work and it < 200000:
    it += 1
    pc = work.pop()
    out = transfer(pc, state.get(pc, {}))
    for kind, t in succ[pc]:
        if kind == 'call':
            # into the callee: arguments (registers) of this call site
            ns, ch = merge(state.get(t, {}), {r: v for r, v in out.items() if r not in (14,)})
            if t not in state or ch: state[t] = ns; work.append(t)
            continue
        o2 = out
        if kind == 'ft' and code[pc].name in ('BSBB', 'BSBW', 'JSB'):
            o2 = {r: v for r, v in out.items() if r not in (0, 1, 2)}
        if t not in state:
            state[t] = dict(o2); work.append(t)
        else:
            ns, ch = merge(state[t], o2)
            if ch: state[t] = ns; work.append(t)

res = {}
for pc in pcs:
    st = state.get(pc, {})
    for k, o in enumerate(code[pc].ops):
        if o['mode'] in ('disp', 'dispdef', 'regdef') and o['reg'] not in (14, 15):
            t = obs(pc, k) or st.get(o['reg'])
            res[(pc, k)] = t
pickle.dump(res, open(BP + 'sid.flow.pkl', 'wb'))
# accuracy check: where observed, what did the propagated state alone say?
agree = dis = 0
for pc in pcs:
    st = state.get(pc, {})
    for k, o in enumerate(code[pc].ops):
        if o['mode'] in MEMMODES and o['reg'] not in (14, 15):
            t = obs(pc, k); p = st.get(o['reg'])
            if t and p:
                if t == p: agree += 1
                else: dis += 1
print("state vs observation: agree", agree, "disagree", dis)
need = pickle.load(open(BP + 'need.pkl', 'rb'))
c = collections.Counter(str(res.get(x)) for x in need)
print("operands needing a type:", len(need), c.most_common())
print("learned loads:", {k: dict(v) for k, v in list(learn.items())[:40]})
if '-v' in sys.argv:
    for pc in pcs:
        st = state.get(pc, {})
        for k, o in enumerate(code[pc].ops):
            if o['mode'] in MEMMODES and o['reg'] not in (14, 15):
                t = obs(pc, k); p = st.get(o['reg'])
                if t and p and t != p: print("  %05X op%d obs %s prop %s  %s" % (pc, k, t, p, repr(code[pc])))
