# open-simh-extended/tools/sidriver_v47/vaxdis.py  v2 (2026-11-19; v2: BUGW/BUGL are FF FE / FF FD with inline code)
# A small VAX instruction decoder for binary analysis (SIDRIVER backport).
#
# decode(mem, pc) -> Insn with: pc, length, opcode name, operand list.
# Each operand is a dict:
#   mode: 'lit' (short literal), 'reg', 'regdef' (Rn), 'autodec' -(Rn),
#         'autoinc' (Rn)+, 'imm' (#x, i.e. (PC)+), 'autoincdef' @(Rn)+,
#         'abs' (@#x), 'disp' d(Rn), 'dispdef' @d(Rn), 'rel' (PC-relative
#         d(PC): target address), 'reldef' (@d(PC)), 'branch' (branch target)
#   reg, value, index (index register or None), size (data size in bytes),
#   spec_off (offset of the specifier byte in the instruction),
#   disp_off / disp_len (where the displacement or immediate is),
#   access ('r','m','w','a','v','b').
# The opcode table gives for each operand access type and data size, e.g.
# 'rl' = read longword, 'ab' = address of byte, 'bb' = byte branch displacement.
import struct

_T = {}
def _op(code, name, *ops):
    _T[code] = (name, ops)

# --- opcode table (VAX architecture, single and FD-prefixed opcodes) ---
for c, n, o in [
 (0x00,'HALT',()),(0x01,'NOP',()),(0x02,'REI',()),(0x03,'BPT',()),(0x04,'RET',()),
 (0x05,'RSB',()),(0x06,'LDPCTX',()),(0x07,'SVPCTX',()),
 (0x08,'CVTPS',('rw','ab','rw','ab')),(0x09,'CVTSP',('rw','ab','rw','ab')),
 (0x0A,'INDEX',('rl','rl','rl','rl','rl','wl')),(0x0B,'CRC',('ab','rl','rw','ab')),
 (0x0C,'PROBER',('rb','rw','ab')),(0x0D,'PROBEW',('rb','rw','ab')),
 (0x0E,'INSQUE',('ab','ab')),(0x0F,'REMQUE',('ab','wl')),
 (0x10,'BSBB',('bb',)),(0x11,'BRB',('bb',)),(0x12,'BNEQ',('bb',)),(0x13,'BEQL',('bb',)),
 (0x14,'BGTR',('bb',)),(0x15,'BLEQ',('bb',)),(0x16,'JSB',('ab',)),(0x17,'JMP',('ab',)),
 (0x18,'BGEQ',('bb',)),(0x19,'BLSS',('bb',)),(0x1A,'BGTRU',('bb',)),(0x1B,'BLEQU',('bb',)),
 (0x1C,'BVC',('bb',)),(0x1D,'BVS',('bb',)),(0x1E,'BCC',('bb',)),(0x1F,'BCS',('bb',)),
 (0x20,'ADDP4',('rw','ab','rw','ab')),(0x21,'ADDP6',('rw','ab','rw','ab','rw','ab')),
 (0x22,'SUBP4',('rw','ab','rw','ab')),(0x23,'SUBP6',('rw','ab','rw','ab','rw','ab')),
 (0x24,'CVTPT',('rw','ab','ab','rw','ab')),(0x25,'MULP',('rw','ab','rw','ab','rw','ab')),
 (0x26,'CVTTP',('rw','ab','ab','rw','ab')),(0x27,'DIVP',('rw','ab','rw','ab','rw','ab')),
 (0x28,'MOVC3',('rw','ab','ab')),(0x29,'CMPC3',('rw','ab','ab')),(0x2A,'SCANC',('rw','ab','ab','rb')),
 (0x2B,'SPANC',('rw','ab','ab','rb')),(0x2C,'MOVC5',('rw','ab','rb','rw','ab')),
 (0x2D,'CMPC5',('rw','ab','rb','rw','ab')),(0x2E,'MOVTC',('rw','ab','rb','ab','rw','ab')),
 (0x2F,'MOVTUC',('rw','ab','rb','ab','rw','ab')),
 (0x30,'BSBW',('bw',)),(0x31,'BRW',('bw',)),(0x32,'CVTWL',('rw','wl')),(0x33,'CVTWB',('rw','wb')),
 (0x34,'MOVP',('rw','ab','ab')),(0x35,'CMPP3',('rw','ab','ab')),(0x36,'CVTPL',('rw','ab','wl')),
 (0x37,'CMPP4',('rw','ab','rw','ab')),(0x38,'EDITPC',('rw','ab','ab','ab')),
 (0x39,'MATCHC',('rw','ab','rw','ab')),(0x3A,'LOCC',('rb','rw','ab')),(0x3B,'SKPC',('rb','rw','ab')),
 (0x3C,'MOVZWL',('rw','wl')),(0x3D,'ACBW',('rw','rw','mw','bw')),(0x3E,'MOVAW',('aw','wl')),
 (0x3F,'PUSHAW',('aw',)),
 (0x40,'ADDF2',('rf','mf')),(0x41,'ADDF3',('rf','rf','wf')),(0x42,'SUBF2',('rf','mf')),
 (0x43,'SUBF3',('rf','rf','wf')),(0x44,'MULF2',('rf','mf')),(0x45,'MULF3',('rf','rf','wf')),
 (0x46,'DIVF2',('rf','mf')),(0x47,'DIVF3',('rf','rf','wf')),(0x48,'CVTFB',('rf','wb')),
 (0x49,'CVTFW',('rf','ww')),(0x4A,'CVTFL',('rf','wl')),(0x4B,'CVTRFL',('rf','wl')),
 (0x4C,'CVTBF',('rb','wf')),(0x4D,'CVTWF',('rw','wf')),(0x4E,'CVTLF',('rl','wf')),
 (0x4F,'ACBF',('rf','rf','mf','bw')),(0x50,'MOVF',('rf','wf')),(0x51,'CMPF',('rf','rf')),
 (0x52,'MNEGF',('rf','wf')),(0x53,'TSTF',('rf',)),(0x54,'EMODF',('rf','rb','rf','wl','wf')),
 (0x55,'POLYF',('rf','rw','ab')),(0x56,'CVTFD',('rf','wd')),
 (0x58,'ADAWI',('rw','mw')),(0x5C,'INSQHI',('ab','aq')),(0x5D,'INSQTI',('ab','aq')),
 (0x5E,'REMQHI',('aq','wl')),(0x5F,'REMQTI',('aq','wl')),
 (0x60,'ADDD2',('rd','md')),(0x61,'ADDD3',('rd','rd','wd')),(0x62,'SUBD2',('rd','md')),
 (0x63,'SUBD3',('rd','rd','wd')),(0x64,'MULD2',('rd','md')),(0x65,'MULD3',('rd','rd','wd')),
 (0x66,'DIVD2',('rd','md')),(0x67,'DIVD3',('rd','rd','wd')),(0x68,'CVTDB',('rd','wb')),
 (0x69,'CVTDW',('rd','ww')),(0x6A,'CVTDL',('rd','wl')),(0x6B,'CVTRDL',('rd','wl')),
 (0x6C,'CVTBD',('rb','wd')),(0x6D,'CVTWD',('rw','wd')),(0x6E,'CVTLD',('rl','wd')),
 (0x6F,'ACBD',('rd','rd','md','bw')),(0x70,'MOVD',('rd','wd')),(0x71,'CMPD',('rd','rd')),
 (0x72,'MNEGD',('rd','wd')),(0x73,'TSTD',('rd',)),(0x74,'EMODD',('rd','rb','rd','wl','wd')),
 (0x75,'POLYD',('rd','rw','ab')),(0x76,'CVTDF',('rd','wf')),
 (0x78,'ASHL',('rb','rl','wl')),(0x79,'ASHQ',('rb','rq','wq')),(0x7A,'EMUL',('rl','rl','rl','wq')),
 (0x7B,'EDIV',('rl','rq','wl','wl')),(0x7C,'CLRQ',('wq',)),(0x7D,'MOVQ',('rq','wq')),
 (0x7E,'MOVAQ',('aq','wl')),(0x7F,'PUSHAQ',('aq',)),
 (0x80,'ADDB2',('rb','mb')),(0x81,'ADDB3',('rb','rb','wb')),(0x82,'SUBB2',('rb','mb')),
 (0x83,'SUBB3',('rb','rb','wb')),(0x84,'MULB2',('rb','mb')),(0x85,'MULB3',('rb','rb','wb')),
 (0x86,'DIVB2',('rb','mb')),(0x87,'DIVB3',('rb','rb','wb')),(0x88,'BISB2',('rb','mb')),
 (0x89,'BISB3',('rb','rb','wb')),(0x8A,'BICB2',('rb','mb')),(0x8B,'BICB3',('rb','rb','wb')),
 (0x8C,'XORB2',('rb','mb')),(0x8D,'XORB3',('rb','rb','wb')),(0x8E,'MNEGB',('rb','wb')),
 (0x8F,'CASEB',('rb','rb','rb')),(0x90,'MOVB',('rb','wb')),(0x91,'CMPB',('rb','rb')),
 (0x92,'MCOMB',('rb','wb')),(0x93,'BITB',('rb','rb')),(0x94,'CLRB',('wb',)),(0x95,'TSTB',('rb',)),
 (0x96,'INCB',('mb',)),(0x97,'DECB',('mb',)),(0x98,'CVTBL',('rb','wl')),(0x99,'CVTBW',('rb','ww')),
 (0x9A,'MOVZBL',('rb','wl')),(0x9B,'MOVZBW',('rb','ww')),(0x9C,'ROTL',('rb','rl','wl')),
 (0x9D,'ACBB',('rb','rb','mb','bw')),(0x9E,'MOVAB',('ab','wl')),(0x9F,'PUSHAB',('ab',)),
 (0xA0,'ADDW2',('rw','mw')),(0xA1,'ADDW3',('rw','rw','ww')),(0xA2,'SUBW2',('rw','mw')),
 (0xA3,'SUBW3',('rw','rw','ww')),(0xA4,'MULW2',('rw','mw')),(0xA5,'MULW3',('rw','rw','ww')),
 (0xA6,'DIVW2',('rw','mw')),(0xA7,'DIVW3',('rw','rw','ww')),(0xA8,'BISW2',('rw','mw')),
 (0xA9,'BISW3',('rw','rw','ww')),(0xAA,'BICW2',('rw','mw')),(0xAB,'BICW3',('rw','rw','ww')),
 (0xAC,'XORW2',('rw','mw')),(0xAD,'XORW3',('rw','rw','ww')),(0xAE,'MNEGW',('rw','ww')),
 (0xAF,'CASEW',('rw','rw','rw')),(0xB0,'MOVW',('rw','ww')),(0xB1,'CMPW',('rw','rw')),
 (0xB2,'MCOMW',('rw','ww')),(0xB3,'BITW',('rw','rw')),(0xB4,'CLRW',('ww',)),(0xB5,'TSTW',('rw',)),
 (0xB6,'INCW',('mw',)),(0xB7,'DECW',('mw',)),(0xB8,'BISPSW',('rw',)),(0xB9,'BICPSW',('rw',)),
 (0xBA,'POPR',('rw',)),(0xBB,'PUSHR',('rw',)),(0xBC,'CHMK',('rw',)),(0xBD,'CHME',('rw',)),
 (0xBE,'CHMS',('rw',)),(0xBF,'CHMU',('rw',)),
 (0xC0,'ADDL2',('rl','ml')),(0xC1,'ADDL3',('rl','rl','wl')),(0xC2,'SUBL2',('rl','ml')),
 (0xC3,'SUBL3',('rl','rl','wl')),(0xC4,'MULL2',('rl','ml')),(0xC5,'MULL3',('rl','rl','wl')),
 (0xC6,'DIVL2',('rl','ml')),(0xC7,'DIVL3',('rl','rl','wl')),(0xC8,'BISL2',('rl','ml')),
 (0xC9,'BISL3',('rl','rl','wl')),(0xCA,'BICL2',('rl','ml')),(0xCB,'BICL3',('rl','rl','wl')),
 (0xCC,'XORL2',('rl','ml')),(0xCD,'XORL3',('rl','rl','wl')),(0xCE,'MNEGL',('rl','wl')),
 (0xCF,'CASEL',('rl','rl','rl')),(0xD0,'MOVL',('rl','wl')),(0xD1,'CMPL',('rl','rl')),
 (0xD2,'MCOML',('rl','wl')),(0xD3,'BITL',('rl','rl')),(0xD4,'CLRL',('wl',)),(0xD5,'TSTL',('rl',)),
 (0xD6,'INCL',('ml',)),(0xD7,'DECL',('ml',)),(0xD8,'ADWC',('rl','ml')),(0xD9,'SBWC',('rl','ml')),
 (0xDA,'MTPR',('rl','rl')),(0xDB,'MFPR',('rl','wl')),(0xDC,'MOVPSL',('wl',)),(0xDD,'PUSHL',('rl',)),
 (0xDE,'MOVAL',('al','wl')),(0xDF,'PUSHAL',('al',)),
 (0xE0,'BBS',('rl','vb','bb')),(0xE1,'BBC',('rl','vb','bb')),(0xE2,'BBSS',('rl','vb','bb')),
 (0xE3,'BBCS',('rl','vb','bb')),(0xE4,'BBSC',('rl','vb','bb')),(0xE5,'BBCC',('rl','vb','bb')),
 (0xE6,'BBSSI',('rl','vb','bb')),(0xE7,'BBCCI',('rl','vb','bb')),(0xE8,'BLBS',('rl','bb')),
 (0xE9,'BLBC',('rl','bb')),(0xEA,'FFS',('rl','rb','vb','wl')),(0xEB,'FFC',('rl','rb','vb','wl')),
 (0xEC,'CMPV',('rl','rb','vb','rl')),(0xED,'CMPZV',('rl','rb','vb','rl')),
 (0xEE,'EXTV',('rl','rb','vb','wl')),(0xEF,'EXTZV',('rl','rb','vb','wl')),
 (0xF0,'INSV',('rl','rl','rb','vb')),(0xF1,'ACBL',('rl','rl','ml','bw')),
 (0xF2,'AOBLSS',('rl','ml','bb')),(0xF3,'AOBLEQ',('rl','ml','bb')),(0xF4,'SOBGEQ',('ml','bb')),
 (0xF5,'SOBGTR',('ml','bb')),(0xF6,'CVTLB',('rl','wb')),(0xF7,'CVTLW',('rl','ww')),
 (0xF8,'ASHP',('rb','rw','ab','rb','rw','ab')),(0xF9,'CVTLP',('rl','rw','ab')),
 (0xFA,'CALLG',('ab','ab')),(0xFB,'CALLS',('rl','ab')),(0xFC,'XFC',()),
]:
    _op(c, n, *o)

_T2 = {}
for c, n, o in [
 (0x32,'CVTDH',('rd','wh')),(0x33,'CVTGF',('rg','wf')),(0x40,'ADDG2',('rg','mg')),
 (0x41,'ADDG3',('rg','rg','wg')),(0x42,'SUBG2',('rg','mg')),(0x43,'SUBG3',('rg','rg','wg')),
 (0x44,'MULG2',('rg','mg')),(0x45,'MULG3',('rg','rg','wg')),(0x46,'DIVG2',('rg','mg')),
 (0x47,'DIVG3',('rg','rg','wg')),(0x50,'MOVG',('rg','wg')),(0x51,'CMPG',('rg','rg')),
 (0x53,'TSTG',('rg',)),(0x56,'CVTGH',('rg','wh')),(0x7C,'CLRO',('wo',)),(0x7D,'MOVO',('ro','wo')),
 (0x7E,'MOVAO',('ao','wl')),(0x7F,'PUSHAO',('ao',)),(0xFD,'CVTFG',('rf','wg')),
]:
    _T2[c] = (n, o)

_SZ = {'b':1,'w':2,'l':4,'q':8,'o':16,'f':4,'d':8,'g':8,'h':16}
REGN = ['R0','R1','R2','R3','R4','R5','R6','R7','R8','R9','R10','R11','AP','FP','SP','PC']

class Insn:
    def __init__(self, pc): self.pc = pc; self.ops = []; self.name = '?'; self.length = 0; self.bad = False
    def __repr__(self): return "%05X: %s %s" % (self.pc, self.name, ",".join(fmt_op(o) for o in self.ops))

def fmt_op(o):
    m = o['mode']; r = REGN[o['reg']] if o.get('reg') is not None else ''
    if m == 'lit': s = "S^#%X" % o['value']
    elif m == 'reg': s = r
    elif m == 'regdef': s = "(%s)" % r
    elif m == 'autodec': s = "-(%s)" % r
    elif m == 'autoinc': s = "(%s)+" % r
    elif m == 'autoincdef': s = "@(%s)+" % r
    elif m == 'imm': s = "#%X" % o['value']
    elif m == 'abs': s = "@#%08X" % o['value']
    elif m == 'disp': s = "%X(%s)" % (o['value'], r)
    elif m == 'dispdef': s = "@%X(%s)" % (o['value'], r)
    elif m == 'rel': s = "%X" % o['value']
    elif m == 'reldef': s = "@%X" % o['value']
    elif m == 'branch': s = "%X" % o['value']
    else: s = m
    if o.get('index') is not None: s += "[%s]" % REGN[o['index']]
    return s

def decode(mem, pc, base=0):
    """Decode the instruction at pc (mem is bytes indexed by pc - base)."""
    ins = Insn(pc)
    def b(a): return mem[a - base]
    p = pc
    try:
        oc = b(p); p += 1
        if oc == 0xFF and b(p) in (0xFD, 0xFE):  # BUGL / BUGW: inline code, no specifier
            n = 4 if b(p) == 0xFD else 2; p += 1
            ins.name = 'BUGL' if n == 4 else 'BUGW'
            ins.ops.append({'mode':'imm','value':int.from_bytes(mem[p - base:p - base + n], 'little'),
                            'disp_off':p - pc,'disp_len':n,'access':'r','size':n,'reg':None,'index':None})
            ins.length = p + n - pc; return ins
        if oc == 0xFD:
            oc2 = b(p); p += 1
            ent = _T2.get(oc2)
        else:
            ent = _T.get(oc)
        if ent is None:
            ins.bad = True; ins.name = '.BYTE %02X' % oc; ins.length = 1; return ins
        ins.name, ops = ent
        for spec in ops:
            acc, dt = spec[0], spec[1]
            if acc == 'b':                       # branch displacement
                n = 1 if dt == 'b' else 2
                raw = mem[p - base:p - base + n]
                d = struct.unpack('<b' if n == 1 else '<h', raw)[0]
                ins.ops.append({'mode':'branch','value':p + n + d,'disp_off':p - pc,'disp_len':n,'access':'b','size':0})
                p += n; continue
            size = _SZ[dt]
            o = {'access':acc,'size':size,'index':None,'reg':None,'spec_off':p - pc}
            sb = b(p); p += 1
            mode, reg = sb >> 4, sb & 15
            if mode == 4:                        # index prefix
                o['index'] = reg; o['spec_off'] = p - pc
                sb = b(p); p += 1; mode, reg = sb >> 4, sb & 15
            if mode < 4:
                o['mode'] = 'lit'; o['value'] = sb & 0x3F
            elif mode == 5: o['mode'] = 'reg'; o['reg'] = reg
            elif mode == 6: o['mode'] = 'regdef'; o['reg'] = reg
            elif mode == 7: o['mode'] = 'autodec'; o['reg'] = reg
            elif mode == 8:
                if reg == 15:
                    o['mode'] = 'imm'; o['disp_off'] = p - pc; o['disp_len'] = size
                    o['value'] = int.from_bytes(mem[p - base:p - base + size], 'little'); p += size
                else: o['mode'] = 'autoinc'; o['reg'] = reg
            elif mode == 9:
                if reg == 15:
                    o['mode'] = 'abs'; o['disp_off'] = p - pc; o['disp_len'] = 4
                    o['value'] = struct.unpack('<I', mem[p - base:p - base + 4])[0]; p += 4
                else: o['mode'] = 'autoincdef'; o['reg'] = reg
            else:                                # displacement modes A-F
                n = {0xA:1,0xB:1,0xC:2,0xD:2,0xE:4,0xF:4}[mode]
                d = int.from_bytes(mem[p - base:p - base + n], 'little', signed=True)
                o['disp_off'] = p - pc; o['disp_len'] = n; p += n
                deferred = mode in (0xB, 0xD, 0xF)
                if reg == 15:
                    o['mode'] = 'reldef' if deferred else 'rel'; o['value'] = (p + d) & 0xFFFFFFFF
                else:
                    o['mode'] = 'dispdef' if deferred else 'disp'; o['reg'] = reg; o['value'] = d
            ins.ops.append(o)
        if ins.name in ('CASEB','CASEW','CASEL'):
            limit = ins.ops[2]
            n = (limit['value'] if limit['mode'] in ('lit','imm') else 0) + 1
            if limit['mode'] in ('lit','imm') and limit['size'] < 4:
                n = (limit['value'] & ((1 << (8*limit['size'])) - 1)) + 1
            ins.case_base = p; ins.case_targets = []
            for i in range(n):
                d = struct.unpack('<h', mem[p - base:p - base + 2])[0]
                ins.case_targets.append(ins.case_base + d); p += 2
        ins.length = p - pc
    except (IndexError, struct.error):
        ins.bad = True; ins.name = '?'; ins.length = 1
    return ins

ENDS = ('RET','RSB','REI','BRB','BRW','JMP','HALT','CASEB','CASEW','CASEL')
