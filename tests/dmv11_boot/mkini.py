# open-simh-extended/tests/dmv11_boot/mkini.py  v5 (v5: common_check.ini gets the unit; v4: SEL0 after a command can keep bits, e.g. the diagnostic mode bit; v3: a command waits for RDI and meanwhile takes responses (clears RDO); v2: t6.ini, the switch-assigned mode and address
# with the host interface)
# Writes t3.ini and t4.ini, the DMV11 boot tests with a running "operating
# system": a small PDP-11 program that master-clears DMV0 (CSR 160360, the
# only floating device), waits for RUN, gives control commands through the
# RQI/RDI handshake and then either loops (T3, remote load detect) or sets
# BSEL1 bits 5 and 6 (T4, invoke primary MOP).  The program words are
# assembled here by hand-made helpers, so that branch offsets are computed,
# not typed.
CSR = 0o160360

def prologue(a):
    """Master clear, wait for RUN (SEL0 bit 15)."""
    return [0o012737, 0o040000, CSR,            # MOV #40000,@#CSR
            0o005737, CSR,                      # 1$: TST @#CSR
            br(0o100000, a + 10, a + 6)]        # BPL 1$

def br(op, at, target):
    """Branch instruction at address 'at' to 'target'."""
    off = (target - (at + 2)) // 2
    assert -128 <= off <= 127
    return op | (off & 0o377)

def command(a, sel2, sel6, sel0=0o000200, after=0):
    """One command: RQI (with other SEL0 bits), wait RDI, SEL2/SEL6, clear
    RDI, clear RQI, delay."""
    return [0o012737, sel0, CSR,                # MOV #sel0,@#CSR     RQI
            0o032737, 0o000020, CSR + 2,        # 1$: BIT #20,@#CSR+2 RDI?
            br(0o001000, a + 0o14, a + 0o34),   # BNE 3$
            0o105737, CSR + 2,                  # TSTB @#CSR+2        RDO?
            br(0o100000, a + 0o22, a + 6),      # BPL 1$
            0o042737, 0o000200, CSR + 2,        # BIC #200,@#CSR+2    response taken
            br(0o000400, a + 0o32, a + 6),      # BR 1$
            0o012737, sel2, CSR + 2,            # 3$: MOV #sel2,@#CSR+2
            0o012737, sel6, CSR + 6,            # MOV #sel6,@#CSR+6
            0o042737, 0o000020, CSR + 2,        # BIC #20,@#CSR+2     command given
            ] + ([0o005037, CSR] if after == 0 else     # CLR @#CSR           RQI off
                 [0o012737, after, CSR]) + [            # (or keep other SEL0 bits)
            0o012700, 0o010000,                 # MOV #10000,R0
            0o077001]                           # 2$: SOB R0,2$

def program(base, cmds, tail):
    words = prologue(base)
    for c in cmds:
        words += command(base + 2 * len(words), *c)
    words += tail(base + 2 * len(words))
    return words

def deposits(phys, words):
    return ["dep %o %06o" % (phys + 2 * i, w) for i, w in enumerate(words)]

# T3: establish tributary 1 and START it (DTR on, DDCMP sends STRT), then
# loop with priority 0.  Memory management maps kernel page 0 to physical
# 100000, so the program at virtual 2000 is at physical 102000.
t3 = program(0o2000, [(0o000421, 1), (0o000421, 3)], lambda a: [0o000777])
# T4: establish tributary 9 (multipoint tributary from the switches), then
# invoke primary MOP: MOV #60000,@#CSR, BR .
t4 = program(0o2000, [(0o004421, 1)], lambda a: [0o012737, 0o060000, CSR, 0o000777])

T3_HEAD = """; open-simh-extended/tests/dmv11_boot/t3.ini  v3 (written by mkini.py v5)
; T3 remote load detect, point-to-point (FDXPOINT), password 123.  The
; "operating system" is a program at virtual 2000 with memory management
; on (kernel page 0 mapped to physical 100000, so a vector fetched through
; the MMU would be wrong), priority 0: it master-clears DMV0, establishes
; and starts tributary 1 (DTR on) and loops.  A wrong password is ignored
; first.
set cpu 11/73
set cpu 256k
set dmv enable
set dmv0 mode=fdxpoint
set dmv0 tributary=123
set dmv0 remoteload
set dmv0 peer=127.0.0.1:21003
attach dmv0 21103
set debug t3.dbg
set dmv debug=INFO;WARN
dep KIPAR0 1000
dep KIPDR0 77406
dep KIPAR1 200
dep KIPDR1 77406
dep KIPAR7 177600
dep KIPDR7 77406
"""
T3_TAIL = """dep MMR0 1
dep PSW 0
dep SP 1000
go 2000
do common_check.ini dmv0
"""
T4_HEAD = """; open-simh-extended/tests/dmv11_boot/t4.ini  v3 (written by mkini.py v5)
; T4 invoke primary MOP, multipoint tributary (FDXTRIBUTARY) with a
; software-assigned address: tributary switches 0; the program master-
; clears DMV0, establishes tributary 9 and sets BSEL1 bits 5 and 6.
set cpu 11/73
set cpu 256k
set dmv enable
set dmv0 mode=fdxtributary
set dmv0 peer=127.0.0.1:21004
attach dmv0 21104
set debug t4.dbg
set dmv debug=INFO;WARN
"""
T4_TAIL = """dep PSW 0
dep SP 1000
go 2000
do common_check.ini dmv0
"""
# T6: mode switches FDXTRIBUTARY, tributary switches 5.  Establish 9 (is
# made 5), mode definition 3 (error 104), mode definition 3 with the
# diagnostic mode bit (accepted), then HALT.
t6 = program(0o2000, [(0o004421, 1), (0o000022, 3), (0o000022, 3, 0o004200, 0o004000)], lambda a: [0o000000])
T6_HEAD = """; open-simh-extended/tests/dmv11_boot/t6.ini  v2 (written by mkini.py v5)
; T6 switch-assigned mode and tributary address with the host interface:
; MODE=FDXTRIBUTARY, TRIBUTARY=5.  The program master-clears DMV0,
; establishes tributary 9 (overridden: established as 5), gives a mode
; definition for full-duplex point-to-point (error 104: the switches set
; the mode), the same with the diagnostic mode bit BSEL1 bit 3 (accepted),
; and halts.  The checks are in the debug log (run_tests.sh).
set cpu 11/73
set dmv enable
set dmv0 mode=fdxtributary
set dmv0 tributary=5
attach dmv0 21106
set debug t6.dbg
set dmv debug=INFO;WARN
"""
T6_TAIL = """dep PSW 0
dep SP 1000
go 2000
"""
open("t6.ini", "w").write(T6_HEAD + "\n".join(deposits(0o2000, t6)) + "\n" + T6_TAIL)
open("t3.ini", "w").write(T3_HEAD + "\n".join(deposits(0o102000, t3)) + "\n" + T3_TAIL)
open("t4.ini", "w").write(T4_HEAD + "\n".join(deposits(0o2000, t4)) + "\n" + T4_TAIL)
print("t3.ini, t4.ini, t6.ini written")
