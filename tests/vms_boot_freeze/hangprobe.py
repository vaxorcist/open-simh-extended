# open-simh-extended/tests/vms_boot_freeze/hangprobe.py  v8 (v8: optional 3rd argument: .ini prefix (default nodmv, e.g. nodmvx for the SET NOASYNCH variant); v7: every command is synchronised with an echoed marker line, as the terminal echo made the sim> prompts slip; v6: VMS state from the symbols SDA gave on a healthy node: every process's state, name, wait mask, the system time and the first timer queue entry; v5: map registers read as QBA MAP[n], pending Qbus interrupt requests (QBA IPL14-17); v4: the rings are Qbus addresses: translate them through the Qbus map before examining physical memory; v3: the MSCP rings in memory (both rings, physical, around 3C5A8) and RQ0's unit state; v2: also the device states of RQ, CLK, TTI, TTO, and the asynchronous I/O setting; INI= selects the .ini pair prefix)
# Known issue 97: two MicroVAX II simulators (no SET CPU IDLE) boot VMS V4.7
# at the same time; one of them sometimes freezes right after the date.
# This script boots both nodes (nodmv_a.ini, nodmv_b.ini, no DMV11), types
# the date, and waits for "job terminated".  A node that shows nothing for
# 120 s is stopped with Ctrl/E and inspected at the sim> prompt: clocks,
# event queue, CPU state and devices, then it runs 10 s more and is
# inspected again (does simulated time still advance?).
#   usage: python3 -I hangprobe.py SIMULATOR ROUNDS
import pexpect, sys, time, shutil, subprocess, re

sim, rounds = sys.argv[1], int(sys.argv[2])
prefix = sys.argv[3] if len(sys.argv) > 3 else "nodmv"
PROBE = ["ex QBA IPC,IPL17,IPL16,IPL15,IPL14", "show clocks", "show queue", "show asynch", "ex STATE", "ex RQ STATE",
         "ex CLK STATE", "ex TTI STATE", "ex TTO STATE", "show rq0"]

def probe(p, log, tag):
    log.write("\n===== %s =====\n" % tag); log.flush()
    for c in PROBE:
        cmd(p, c)
    vms(p, log)
    # MSCP rings: Qbus address -> map register -> physical address
    out = cmd(p, "ex RQ CQBA,RQBA,CQIDX,RQIDX,CQLNT,RQLNT")
    regs = dict(re.findall(r"(CQBA|RQBA|CQIDX|RQIDX|CQLNT|RQLNT):\s+([0-9A-F]+)", out))
    for ring in ("CQBA", "RQBA"):
        if ring not in regs:
            log.write("\n(no %s in %r)\n" % (ring, p.before)); continue
        qa = int(regs[ring], 16)
        m = re.search(r"\]:\s+([0-9A-F]{8})", cmd(p, "ex QBA MAP[%d]" % (qa >> 9)))
        if not m:
            continue
        pa = ((int(m.group(1), 16) & 0x7FFF) << 9) | (qa & 0x1FF)
        log.write("\n%s Qbus %X -> map %s -> physical %X\n" % (ring, qa, m.group(1), pa))
        cmd(p, "ex -l %X:%X" % (pa, pa + 4 * 16 - 1))

STATES = {1: "COLPG", 2: "MWAIT", 3: "CEF", 4: "PFW", 5: "LEF", 6: "LEFO", 7: "HIB",
          8: "HIBO", 9: "SUSP", 10: "SUSPO", 11: "FPG", 12: "COM", 13: "COMO", 14: "CUR"}

def cmd(p, c):
    """run one SIMH command, return its output (marker line, not the prompt)"""
    p.sendline(c)
    p.sendline("echo @@MARK@@")
    p.expect(r"\n@@MARK@@", timeout=60)
    out = p.before
    p.expect(r"sim> ", timeout=30)
    return out

def exl(p, addr, n=1):
    """examine n longwords at a VMS system virtual address"""
    out = cmd(p, "ex -v -l %X:%X" % (addr, addr + 4 * n - 1))
    return [int(v, 16) for v in re.findall(r"^[0-9A-F]+:\s+([0-9A-F]{8})", out, re.M)]

def exb(p, addr, n):
    out = cmd(p, "ex -v -b %X:%X" % (addr, addr + n - 1))
    return bytes(int(v, 16) for v in re.findall(r"^[0-9A-F]+:\s+([0-9A-F]{2})", out, re.M))

def vms(p, log):
    """process list, system time and first timer queue entry (VMS V4.7 MicroVMS,
       symbols from SDA: SCH$GL_PCBVEC 80002988, SCH$GL_MAXPIX 80002990,
       EXE$GQ_SYSTIME 80002B40, EXE$GL_TQFL 80002B58; PCB offsets STS 24,
       STATE 2C, EFWM 4C, EFCS 50, PID 60, LNAME 70)"""
    vec, = exl(p, 0x80002988); maxpix, = exl(p, 0x80002990)
    null = None
    out = ["system time %08X%08X" % tuple(reversed(exl(p, 0x80002B40, 2)))]
    slots = exl(p, vec, maxpix + 1)
    for i, pcb in enumerate(slots):
        if null is None:
            null = pcb
        elif pcb == null:
            continue                        # empty slot points to the null PCB
        sts, = exl(p, pcb + 0x24); st, = exl(p, pcb + 0x2C)
        efwm, efcs = exl(p, pcb + 0x4C, 2); pid, = exl(p, pcb + 0x60)
        nm = exb(p, pcb + 0x70, 16); name = nm[1:1 + nm[0]].decode("latin-1", "replace")
        out.append("slot %2d PCB %08X PID %08X %-15s state %-5s STS %08X EFWM %08X EFCS %08X"
                   % (i, pcb, pid, name, STATES.get(st & 0xFFFF, str(st & 0xFFFF)), sts, efwm, efcs))
    tq, = exl(p, 0x80002B58)
    out.append("first TQE %08X: %s" % (tq, " ".join("%08X" % v for v in exl(p, tq, 12))))
    log.write("\n" + "\n".join(out) + "\n"); log.flush()
    print("\n".join(out), flush=True)

def boot(n):
    shutil.copy("disk%s.vhd" % n.upper(), "nodmv_%s.vhd" % n)
    log = open("probe_%s.log" % n, "w")
    p = pexpect.spawn(sim, ["%s_%s.ini" % (prefix, n)], encoding="latin-1", timeout=600,
                      env={"SDL_AUDIODRIVER": "dummy", "PATH": "/usr/bin:/bin"})
    p.logfile_read = log
    return p, log

for r in range(rounds):
    nodes = {}
    for n in "ab":
        nodes[n] = boot(n)
        time.sleep(3)
    state = {n: "boot" for n in nodes}
    t_date = {}
    t0 = time.time()
    while any(s in ("boot", "date") for s in state.values()) and time.time() - t0 < 900:
        for n, (p, log) in nodes.items():
            if state[n] not in ("boot", "date"):
                continue
            i = p.expect([r"\(DD-MMM-YYYY +HH:MM\) +", r"job terminated", pexpect.TIMEOUT],
                         timeout=1)
            if i == 0:
                time.sleep(1)
                p.send("21-NOV-2026 12:00\r")
                state[n] = "date"; t_date[n] = time.time()
            elif i == 1:
                state[n] = "ok"
            elif state[n] == "date" and time.time() - t_date[n] > 120:
                state[n] = "hung"
                p.send("\x05")
                p.expect(r"sim> ", timeout=30)
                probe(p, log, "hung, first look")
                p.sendline("continue")
                time.sleep(10)
                p.send("\x05")
                p.expect(r"sim> ", timeout=30)
                probe(p, log, "10 s later")
    print("round %d: %s" % (r + 1, state), flush=True)
    for n, (p, log) in nodes.items():
        p.close(force=True); log.close()
        if state[n] == "hung":
            shutil.copy("probe_%s.log" % n, "probe_hung_r%d_%s.log" % (r + 1, n))
    if "hung" in state.values() and prefix == "nodmv":
        break
