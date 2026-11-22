# open-simh-extended/tests/dmv11_boot/mopctl.py  v8 (v8: --shortreq, the DMC11's Request Program 8,12,1; v7: DMP11 and DMR11: --devtype, --dmrpw, --text, --noload; v6: wrong password: 3 s without a MOP answer, DDCMP messages ignored; v5: BPL offset of the test program fixed; v4: the simulator may close the line after the start; v3: load 0 covers 0-777, so the restored words at 400-402 are checked too; v2: connects to the DMV11's ATTACH port; the
# simulator's outgoing connection to a listening script is reported failed here)
# A minimal DDCMP/MOP control station for testing the DMV11 primary MOP
# boot of the SIMH PDP-11 (pdp11_dmc.c).  It connects to the DMV11's ATTACH
# port (PORT), and plays the control station of a point-to-point line
# or a multipoint line:
#
#   - power-on boot / invoke primary MOP: it polls (sends DDCMP STRT
#     messages) until the DMV11 answers with MOP Request Program,
#   - remote load detect (--trigger PW): it sends Enter MOP Mode with the
#     password PW and expects Request Program,
#
# then loads a small PDP-11 test program in three loads (vectors 0-777
# with a pattern, the program at 1000, its text at 1100 with the transfer
# address 1000) and checks every Request Memory Load on the way.  The test
# program prints "DMV BOOT OK" on the console and halts; the SIMH script
# then checks memory (see run_tests.sh).
#
# The same station boots a DMP11 (--devtype 18) and a DMR11 (--devtype 40
# --dmrpw: its Enter MOP Mode carries the password four times).  With
# --noload it only checks that no MOP message answers the Enter MOP Mode
# (a DMR11 that starts a boot ROM instead).
#
# usage: python3 -I mopctl.py PORT [--addr A] [--mp] [--trigger PW]
#                                  [--wrongfirst] [--ignorefirst] [--dupload]
#                                  [--devtype T] [--dmrpw] [--text S] [--noload]
#                                  [--shortreq]
#   --addr A       DDCMP address of the remote station (multipoint), default 1
#   --mp           multipoint: first poll a different address (must be ignored)
#   --trigger PW   remote load detect with password PW
#   --wrongfirst   with --trigger: first a wrong password (must be ignored)
#   --ignorefirst  do not answer the first Request Program (retransmission)
#   --dupload      send load 1 twice (the duplicate must be answered again)
#   --devtype T    MOP device type the Request Program must carry (default 34, DMV11)
#   --dmrpw        DMR11: Enter MOP Mode with the password in all four bytes
#   --text S       the text the test program prints (default "DMV BOOT OK")
#   --noload       with --trigger: expect no MOP answer (3 s), load nothing
#   --shortreq     DMC11: the Request Program has no program type (8,12,1)
# Exit code 0 when the whole exchange went as expected.
import socket, struct, sys, time, argparse

DLE, ENQ = 0x90, 0x05

def crc16(data, crc=0):
    # DDCMP CRC-16 (polynomial x^16+x^15+x^2+1, reflected, initial value 0)
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

def ctl(mtype, sub=0, flags=3, rcv=0, num=0, addr=1):
    h = bytes([ENQ, mtype, (sub & 0x3F) | (flags << 6), rcv, num, addr])
    return h + struct.pack('<H', crc16(h))

def maint(data, addr=1):
    n = len(data)
    h = bytes([DLE, n & 0xFF, ((n >> 8) & 0x3F) | (3 << 6), 0, 0, addr])
    return h + struct.pack('<H', crc16(h)) + data + struct.pack('<H', crc16(data))

class Line:
    def __init__(self, sock):
        self.s = sock
        self.buf = b''
    def send(self, pkt):
        self.s.sendall(pkt)
    def recv(self, timeout):
        """Next DDCMP message (header CRC checked), or None after timeout."""
        end = time.time() + timeout
        while True:
            m = self.parse()
            if m is not None:
                return m
            left = end - time.time()
            if left <= 0:
                return None
            self.s.settimeout(left)
            try:
                d = self.s.recv(4096)
            except socket.timeout:
                return None
            if not d:
                raise SystemExit("FAIL: connection closed")
            self.buf += d
    def parse(self):
        while self.buf and self.buf[0] not in (DLE, ENQ, 0x81):
            self.buf = self.buf[1:]                     # sync / fill
        if len(self.buf) < 8:
            return None
        h = self.buf[:8]
        if crc16(h) != 0:
            raise SystemExit("FAIL: bad header CRC %s" % h.hex())
        if h[0] == ENQ:
            self.buf = self.buf[8:]
            return ('ctl', h, b'')
        n = h[1] | ((h[2] & 0x3F) << 8)
        if len(self.buf) < 10 + n:
            return None
        data = self.buf[8:8 + n]
        if crc16(self.buf[8:10 + n]) != 0:
            raise SystemExit("FAIL: bad data CRC")
        self.buf = self.buf[10 + n:]
        return ('dle' if h[0] == DLE else 'data', h, data)

def words(ws):
    return b''.join(struct.pack('<H', w) for w in ws)

# The test program: prints its text, halts.
#  1000 MOV #1000,SP / MOV #1100,R1 / 1$: MOVB (R1)+,R0 / BEQ 3$ /
#  2$: TSTB @#177564 / BPL 2$ / MOVB R0,@#177566 / BR 1$ / 3$: HALT
PROGRAM = words([0o012706, 0o001000, 0o012701, 0o001100, 0o112100, 0o001406,
                 0o105737, 0o177564, 0o100375, 0o110037, 0o177566, 0o000770,
                 0o000000])
PAGE0 = words([0o100000 + i for i in range(256)])       # 0-777: a pattern

def mop_load(code, loadnum, addr, data=b'', xfer=None):
    m = bytes([code, loadnum]) + struct.pack('<I', addr) + data
    if xfer is not None:
        m += struct.pack('<I', xfer)
    return m

def expect_mop(line, addr, what, timeout=10.0):
    """Wait for a MOP message from the remote station; skip other messages."""
    end = time.time() + timeout
    while time.time() < end:
        m = line.recv(end - time.time())
        if m is None:
            break
        kind, h, data = m
        if kind != 'dle':
            print("  (ignored %s message type %d)" % (kind, h[1]))
            continue
        if h[5] != addr:
            raise SystemExit("FAIL: %s with DDCMP address %d, expected %d" % (what, h[5], addr))
        return data
    raise SystemExit("FAIL: no %s within %.0f s" % (what, timeout))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', type=int)
    ap.add_argument('--addr', type=int, default=1)
    ap.add_argument('--mp', action='store_true')
    ap.add_argument('--trigger', type=int)
    ap.add_argument('--wrongfirst', action='store_true')
    ap.add_argument('--ignorefirst', action='store_true')
    ap.add_argument('--dupload', action='store_true')
    ap.add_argument('--devtype', type=int, default=34)
    ap.add_argument('--dmrpw', action='store_true')
    ap.add_argument('--text', default='DMV BOOT OK')
    ap.add_argument('--noload', action='store_true')
    ap.add_argument('--shortreq', action='store_true')
    a = ap.parse_args()
    text = a.text.encode() + b"\r\n\0"
    def enter_mop(pw):
        # DMV11/DMP11: password in the first byte, then zeros; DMR11: four times
        return bytes([6] + ([pw] * 4 if a.dmrpw else [pw, 0, 0, 0]))

    s = None
    for i in range(120):                                # the simulator may start later
        try:
            s = socket.create_connection(('127.0.0.1', a.port), timeout=2)
            break
        except OSError:
            time.sleep(0.5)
    if s is None:
        raise SystemExit("FAIL: cannot connect to port %d" % a.port)
    line = Line(s)
    print("connected")

    if a.trigger is not None:
        if a.wrongfirst:
            line.send(maint(enter_mop((a.trigger + 1) & 0xFF), a.addr))
            end = time.time() + 3.0
            while time.time() < end:                    # the running DDCMP may send STRTs
                m = line.recv(max(0.0, end - time.time()))
                if m is not None and m[0] == 'dle':
                    raise SystemExit("FAIL: answer to a wrong password: %s" % m[2].hex())
            print("wrong password ignored")
        line.send(maint(enter_mop(a.trigger), a.addr))
        if a.noload:
            end = time.time() + 3.0
            while time.time() < end:
                try:
                    m = line.recv(max(0.0, end - time.time()))
                except SystemExit:                  # the simulator may stop meanwhile
                    break
                if m is not None and m[0] == 'dle':
                    raise SystemExit("FAIL: MOP answer %s (a boot ROM was expected to take over)" % m[2].hex())
            print("PASS: no MOP answer (the boot ROM took over)")
            return
        req = expect_mop(line, a.addr, "Request Program")
    else:
        if a.mp:                                        # another tributary first
            line.send(ctl(6, addr=(a.addr + 1) & 0xFF))
            m = line.recv(3.0)
            if m is not None and m[0] == 'dle':
                raise SystemExit("FAIL: answer to a poll for another address")
            print("poll for address %d ignored" % ((a.addr + 1) & 0xFF))
        req = None
        for i in range(20):                             # poll: STRT
            line.send(ctl(6, addr=a.addr))
            m = line.recv(1.0)
            if m is not None and m[0] == 'dle':
                if m[1][5] != a.addr:
                    raise SystemExit("FAIL: Request Program with address %d" % m[1][5])
                req = m[2]
                break
        if req is None:
            raise SystemExit("FAIL: no Request Program after 20 polls")
    print("Request Program: %s" % req.hex())
    expected = bytes([8, a.devtype, 1]) if a.shortreq else bytes([8, a.devtype, 1, 0])
    if req != expected:
        raise SystemExit("FAIL: unexpected Request Program")
    if a.ignorefirst:
        t0 = time.time()
        req = expect_mop(line, a.addr, "repeated Request Program", 12.0)
        dt = time.time() - t0
        print("Request Program repeated after %.1f s: %s" % (dt, req.hex()))
        if req != expected or dt < 3.5:
            raise SystemExit("FAIL: bad retransmission")

    line.send(maint(mop_load(2, 0, 0, PAGE0), a.addr))
    r = expect_mop(line, a.addr, "Request Memory Load 1")
    print("after load 0: %s" % r.hex())
    if r != bytes([10, 1, 0]):
        raise SystemExit("FAIL: expected 0a0100")
    line.send(maint(mop_load(2, 1, 0o1000, PROGRAM), a.addr))
    r = expect_mop(line, a.addr, "Request Memory Load 2")
    print("after load 1: %s" % r.hex())
    if r != bytes([10, 2, 0]):
        raise SystemExit("FAIL: expected 0a0200")
    if a.dupload:
        line.send(maint(mop_load(2, 1, 0o1000, PROGRAM), a.addr))
        r = expect_mop(line, a.addr, "Request Memory Load 2 again")
        print("after load 1 again: %s" % r.hex())
        if r != bytes([10, 2, 0]):
            raise SystemExit("FAIL: expected 0a0200 again")
        line.send(maint(mop_load(2, 7, 0o3000, b'xx'), a.addr))     # out of sequence
        m = line.recv(2.0)
        if m is not None and m[0] == 'dle':
            raise SystemExit("FAIL: answer to load 7: %s" % m[2].hex())
        print("load 7 ignored")
    line.send(maint(mop_load(0, 2, 0o1100, text, 0o1000), a.addr))
    try:                                                # the simulator may stop meanwhile
        m = line.recv(3.0)
    except SystemExit:
        m = None
    if m is not None and m[0] == 'dle':
        raise SystemExit("FAIL: answer after the transfer: %s" % m[2].hex())
    print("PASS: program loaded and started")
    time.sleep(1)
    s.close()

main()
