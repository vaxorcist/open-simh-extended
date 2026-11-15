# open-simh-extended/tests/dmb32/p2.py  v2
# v2: the password prompt is optional (VMS asked none on the line)
# DMB32 phase P2 test client: logs in on a DMB32 asynchronous line of VMS
# V4.7 (vax8200, p2.ini) through the Telnet port and checks the async
# functions:
#   1. login (receive FIFO, receive interrupts, echo by DMA output)
#   2. SHOW TERMINAL (output by DMA from a process buffer)
#   3. DIRECTORY SYS$SYSTEM (long output, many DMA buffers)
#   4. XOFF/XON flow control: ^S stops the output, ^Q restarts it
#   5. LOGOUT
# Usage: python3 p2.py <port> <simulator output file>
# The simulator console script writes "=READY" when the system is up.
# Exit code 0 = all checks passed; the session is written to p2_line.log.
import socket, sys, time, re

PORT = int(sys.argv[1])
OUTFILE = sys.argv[2]
LOG = open("p2_line.log", "wb")

def strip_telnet(data):
    """Remove Telnet IAC command sequences from the received bytes."""
    out = bytearray(); i = 0
    while i < len(data):
        b = data[i]
        if b == 255 and i + 1 < len(data):
            cmd = data[i + 1]
            if cmd == 255:                      # escaped 0xFF
                out.append(255); i += 2
            elif cmd == 250:                    # subnegotiation: skip to IAC SE
                j = data.find(b"\xff\xf0", i)
                i = len(data) if j < 0 else j + 2
            elif cmd in (251, 252, 253, 254):   # WILL WONT DO DONT option
                i += 3
            else:
                i += 2
        else:
            out.append(b); i += 1
    return bytes(out)

class Line:
    def __init__(self, port):
        self.s = socket.create_connection(("127.0.0.1", port))
        self.s.settimeout(0.5)
        self.buf = b""
    def read_some(self):
        try:
            d = self.s.recv(65536)
        except socket.timeout:
            return False
        if not d:
            raise SystemExit("connection closed")
        d = strip_telnet(d)
        LOG.write(d); LOG.flush()
        self.buf += d
        return True
    def expect(self, pat, timeout=120):
        """Wait for regex pat in the received text; return the text up to it."""
        end = time.time() + timeout
        rx = re.compile(pat.encode() if isinstance(pat, str) else pat)
        while time.time() < end:
            m = rx.search(self.buf)
            if m:
                text = self.buf[:m.end()]
                self.buf = self.buf[m.end():]
                return text
            self.read_some()
        raise SystemExit("timeout waiting for %r; got tail %r" % (pat, self.buf[-300:]))
    def send(self, text, slow=False):
        """Send text; slow = one character at a time (typing)."""
        data = text.encode()
        if slow:
            for ch in data:
                self.s.sendall(bytes([ch])); time.sleep(0.05)
        else:
            self.s.sendall(data)

def wait_ready():
    """Wait until the console script has logged in and written =READY."""
    for _ in range(1800):
        try:
            if b"=READY" in open(OUTFILE, "rb").read():
                return
        except FileNotFoundError:
            pass
        time.sleep(1)
    raise SystemExit("system not ready")

results = []
def check(name, ok, info=""):
    results.append((name, ok))
    print("%-28s %s %s" % (name, "PASS" if ok else "FAIL", info)); sys.stdout.flush()

wait_ready()
ln = Line(PORT)
time.sleep(1)
ln.send("\r")
ln.expect(r"Username: ")
ln.send("SYSTEM\r", slow=True)
t = ln.expect(r"Password: |\$ ", timeout=180)
if t.endswith(b"Password: "):
    ln.send("MANAGER\r", slow=True)
    t = ln.expect(r"\$ ", timeout=180)
check("1 login", b"Welcome" in t or b"Last interactive" in t)

ln.send("SHOW TERMINAL\r", slow=True)
t = ln.expect(r"\$ ")
m = re.search(rb"Terminal: (_TXA\d+:)", t)
check("2 SHOW TERMINAL", m is not None, m.group(1).decode() if m else repr(t[-200:]))

ln.send("DIRECTORY SYS$SYSTEM:*.EXE\r", slow=True)
t = ln.expect(r"Total of \d+ files?\.", timeout=300)
n = len(re.findall(rb"\.EXE;\d+", t))
ln.expect(r"\$ ")
check("3 DIRECTORY (DMA output)", n > 50, "%d file names, %d bytes" % (n, len(t)))

# XOFF/XON: start a long listing, stop it, check nothing arrives, restart.
ln.send("DIRECTORY/FULL SYS$SYSTEM:*.EXE\r", slow=True)
ln.expect(r"File ID:", timeout=120)
ln.send("\x13")                                 # ^S (XOFF)
time.sleep(1.0)                                 # what was in flight arrives
while ln.read_some():
    pass
before = len(ln.buf)
time.sleep(3.0)
while ln.read_some():
    pass
stopped = len(ln.buf) - before
ln.send("\x11")                                 # ^Q (XON)
t = ln.expect(r"Total of \d+ files?", timeout=600)
ln.expect(r"\$ ")
check("4 XOFF/XON flow control", stopped == 0, "%d bytes while stopped" % stopped)

ln.send("LOGOUT\r", slow=True)
t = ln.expect(r"logged out", timeout=60)
check("5 LOGOUT", True)

ok = all(r[1] for r in results)
print("ALL PASS" if ok else "SOME FAILED")
sys.exit(0 if ok else 1)
