# open-simh-extended/tests/dmv11_vmsload/vmsrun.py  v4 (v4: if VMS shows nothing for 60 s after the date was typed, CR is typed again (a CR was lost twice while the other node connected); v3: lines end with CR, as typed on a terminal; v2: answers the date prompt during the boot)
# Runs one simulator with VMS in the foreground (pexpect), logs in and
# types a list of DCL commands, one at a time: each command is followed by
# a marker line (WRITE SYS$OUTPUT "=Sn","END=") and the next command waits
# for "=SnEND=", so the echo of the command line never matches.  Then it
# stops the simulator (Ctrl/E, EXIT).  The whole console output goes to
# the log file.
#
# usage: python3 -I vmsrun.py SIMULATOR INI LOG CMDFILE [TIMEOUT]
#   CMDFILE: one DCL command per line; lines starting with "#" are
#   comments; "!WAIT n" waits n seconds; "!SIMH text" types Ctrl/E, the
#   SIMH command and CONTINUE.
import sys, time, pexpect

def sendline(text):
    """Type a line with CR (VMS and SIMH want CR, not LF)."""
    p.send(text + "\r")

sim, ini, log, cmdfile = sys.argv[1:5]
timeout = int(sys.argv[5]) if len(sys.argv) > 5 else 900
out = open(log, "w", buffering=1)          # line buffered: the log can be watched
p = pexpect.spawn(sim, [ini], encoding="latin-1", timeout=timeout,
                  env={"SDL_AUDIODRIVER": "dummy", "PATH": "/usr/bin:/bin"})
p.logfile_read = out

def login():
    i = p.expect([r"Username:", r"\(DD-MMM-YYYY HH:MM\)", r"HH:MM\)"])
    if i > 0:
        sendline("21-NOV-2026 12:00")
        p.expect(r"Username:")
    sendline("SYSTEM")
    i = p.expect([r"Password:", r"\$ $"])
    if i == 0:
        sendline("MANAGER")
        i = p.expect([r"\$ $", r"User authorization failure", r"Username:"])
        if i != 0:
            p.expect(r"Username:", timeout=30)
            sendline("SYSTEM")
            p.expect(r"Password:")
            sendline("")
            p.expect(r"\$ $")

typed_date = False
# a boot of VMS V4.7 may ask for the date, and ends with "job terminated"
# before the login works
t_end = time.time() + timeout
while True:
    i = p.expect([r"job terminated", r"\(DD-MMM-YYYY +HH:MM\) +", pexpect.TIMEOUT],
                 timeout=60 if typed_date else max(1, t_end - time.time()))
    if i == 0:
        break
    if i == 1:
        time.sleep(1)
        sendline("21-NOV-2026 12:00")
        typed_date = True
    elif typed_date and time.time() < t_end:
        p.send("\r")                       # the CR may have been lost
    else:
        raise SystemExit("boot did not finish")
time.sleep(2)
sendline("")
login()
n = 0
for line in open(cmdfile):
    line = line.rstrip("\n")
    if not line.strip() or line.startswith("#"):
        continue
    if line.startswith("!WAIT"):
        time.sleep(int(line.split()[1]))
        continue
    if line.startswith("!SIMH "):
        p.sendcontrol("e")
        p.expect(r"sim> ")
        sendline(line[6:])
        p.expect(r"sim> ")
        sendline("CONTINUE")
        continue
    n += 1
    sendline(line)
    sendline('WRITE SYS$OUTPUT "=S%d","END="' % n)
    p.expect("=S%dEND=" % n)
    p.expect(r"\$ $")
p.sendcontrol("e")
p.expect(r"sim> ")
sendline("exit")
p.expect(pexpect.EOF, timeout=30)
out.close()
print("done, %d commands" % n)
