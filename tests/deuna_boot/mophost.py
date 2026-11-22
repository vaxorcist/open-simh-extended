# open-simh-extended/tests/deuna_boot/mophost.py  v1
#
# A minimal MOP load host (dump/load protocol 60-01) for testing the
# DEUNA/DELUA BOOT port command of SIMH's XU device (known issue 109,
# docs/DEUNA_boot_design.md).  It talks to the simulator over SIMH's UDP
# Ethernet transport: the simulator has  ATTACH XU udp:SIMPORT:127.0.0.1:PORT,
# so every Ethernet frame is one UDP datagram; this host listens on PORT
# and answers to SIMPORT.
#
# Dialogue checked:
#   1. Request Program (code 08), program type 0 (secondary loader), with
#      the expected device type (--devtype: 1 DEUNA, 11 DELUA), format 1,
#      processor 0; answered with one Memory Load with Transfer Address
#      (code 00): the secondary loader image (by default 64 dummy bytes;
#      --secondary FILE: an RSX task image such as DEC's SECUNA.SYS, label
#      blocks 0-1 skipped, loaded at its base address from the label).
#   2. Request Program, program type 1 (tertiary loader), device type 1
#      (UNA), sent to this host; answered with Memory Loads (code 02) of
#      the tertiary loader in pieces of --chunk bytes, each after the
#      board's Request Memory Load (code 0A) for its load number, and a
#      final Memory Load with Transfer Address.  By default the tertiary is
#      a small test program at 002000 that prints TEXT on the console and
#      halts at 002032 (SIMH: "HALT instruction, PC: 002034"); with
#      --tertiary FILE an RSX task image (e.g. DEC's TERUNA.SYS).
#   3. The board's Request Memory Load after the last load (acknowledge).
#   4. --system TEXT: answers the tertiary loader's Request Program for the
#      operating system (program type 2, software ID -1, sent by the
#      tertiary through the board's normal port) with the same test
#      program printing TEXT, in Memory Loads ending with a Parameter Load
#      with Transfer Address (code 14 hex, as RSX load hosts send), each
#      after the tertiary's Request Memory Load.  For DEC's TERUNA.SYS.
#   --watch S: afterwards print every dump/load frame for S seconds (to
#   see what a real tertiary loader such as TERUNA.SYS asks for).
#   Also: --dropfirst ignores the first Request Program (the board must
#   repeat it after 5 s); --resend sends one load twice (the board must
#   ask again for the next number instead of storing twice).
# Prints PASS lines; exits 1 with FAIL on any unexpected frame or timeout.
#
# usage: python3 -I mophost.py PORT SIMPORT [--devtype N] [--swid N]
#            [--text TEXT] [--chunk N] [--secondary FILE] [--tertiary FILE]
#            [--dropfirst] [--resend] [--timeout S] [--watch S] [--system TEXT]

import argparse
import socket
import struct
import sys
import time

HOSTMAC = bytes([0x08, 0x00, 0x2B, 0xAA, 0xBB, 0xCC])


def fail(msg):
    print("FAIL: " + msg)
    sys.stdout.flush()
    sys.exit(1)


def test_tertiary(text):
    """The default tertiary loader: prints text, halts at 002032."""
    words = [0o012702, 0o002036,                # MOV #text,R2
             0o112200,                          # 1$: MOVB (R2)+,R0
             0o001406,                          #     BEQ 3$
             0o105737, 0o177564,                # 2$: TSTB @#177564
             0o100375,                          #     BPL 2$
             0o110037, 0o177566,                #     MOVB R0,@#177566
             0o000770,                          #     BR 1$
             0o105737, 0o177564,                # 3$: TSTB @#177564
             0o100375,                          #     BPL 3$
             0o000000,                          #     HALT (002032)
             0o000000]                          #     (pad)
    code = struct.pack('<%dH' % len(words), *words)
    assert len(code) == 0o36
    code += text.encode('ascii') + b'\r\n\0'
    if len(code) % 2:
        code += b'\0'
    return 0o2000, code, 0o2000


def task_image(path):
    """An RSX task image: base address (label word 4) and the image."""
    d = open(path, 'rb').read()
    base, high = struct.unpack_from('<2H', d, 8)
    image = d[1024:1024 + (high - base + 1)]
    if len(image) % 2:
        image += b'\0'
    return base, image, base


class Host:
    def __init__(self, port, simport):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(('127.0.0.1', port))
        self.sim = ('127.0.0.1', simport)
        self.board = None

    def recv(self, timeout):
        """Next dump/load frame: (source MAC, destination MAC, MOP message)."""
        end = time.time() + timeout
        while time.time() < end:
            self.s.settimeout(max(0.05, end - time.time()))
            try:
                d, _ = self.s.recvfrom(2000)
            except socket.timeout:
                continue
            if len(d) < 17 or d[12:14] != b'\x60\x01':
                continue                        # system ID, loopback, ...
            n = d[14] | (d[15] << 8)
            return d[6:12], d[0:6], d[16:16 + n]
        return None

    def send(self, mop):
        f = self.board + HOSTMAC + b'\x60\x01' + struct.pack('<H', len(mop)) + mop
        if len(f) < 60:
            f += bytes(60 - len(f))
        self.s.sendto(f, self.sim)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', type=int)
    ap.add_argument('simport', type=int)
    ap.add_argument('--devtype', type=int, default=1)
    ap.add_argument('--swid', type=int, default=0)
    ap.add_argument('--text', default='DEUNA BOOT OK')
    ap.add_argument('--chunk', type=int, default=16)
    ap.add_argument('--secondary')
    ap.add_argument('--tertiary')
    ap.add_argument('--dropfirst', action='store_true')
    ap.add_argument('--resend', action='store_true')
    ap.add_argument('--timeout', type=float, default=60)
    ap.add_argument('--watch', type=float, default=0)
    ap.add_argument('--system')
    a = ap.parse_args()
    h = Host(a.port, a.simport)

    # 1. secondary loader
    first = True
    while True:
        r = h.recv(a.timeout)
        if r is None:
            fail("no Request Program for the secondary loader")
        src, dst, mop = r
        if mop[0] != 0x08:
            continue
        print("Request Program from %s to %s: %s" % (src.hex('-'), dst.hex('-'), mop.hex()))
        if mop[3] != 0:
            fail("program type %d, expected 0 (secondary loader)" % mop[3])
        if mop[1] != a.devtype or mop[2] != 1 or mop[4] != a.swid or mop[5] != 0:
            fail("device type %d format %d software ID %d processor %d, expected %d 1 %d 0"
                 % (mop[1], mop[2], mop[4], mop[5], a.devtype, a.swid))
        if dst != bytes([0xAB, 0, 0, 1, 0, 0]):
            fail("not sent to the load assistant multicast")
        if first and a.dropfirst:
            first = False
            t0 = time.time()
            print("first request ignored")
            continue
        if not first and a.dropfirst:
            print("PASS: request repeated after %.1f s" % (time.time() - t0))
        break
    h.board = src
    print("PASS: Request Program for the secondary loader")
    if a.secondary:
        base, image, xfer = task_image(a.secondary)
    else:
        base, image, xfer = 0o14000, bytes(range(64)), 0o14000
    h.send(b'\x00\x00' + struct.pack('<I', base) + image + struct.pack('<I', xfer))
    print("secondary loader sent: %d bytes at %06o" % (len(image), base))

    # 2. tertiary loader
    r = h.recv(15)
    if r is None:
        fail("no Request Program for the tertiary loader")
    src, dst, mop = r
    print("Request Program to %s: %s" % (dst.hex('-'), mop.hex()))
    if mop[0] != 0x08 or mop[3] != 1:
        fail("expected Request Program for the tertiary loader (type 1): %s" % mop.hex())
    if mop[1] != 1 or dst != HOSTMAC:
        fail("device type %d or destination %s (expected 1, this host)" % (mop[1], dst.hex('-')))
    print("PASS: Request Program for the tertiary loader")
    if a.tertiary:
        base, code, xfer = task_image(a.tertiary)
    else:
        base, code, xfer = test_tertiary(a.text)
    pieces = [code[i:i + a.chunk] for i in range(0, len(code), a.chunk)]
    resent = False
    load = 0
    for i, piece in enumerate(pieces):
        last = (i == len(pieces) - 1)
        body = bytes([0x00 if last else 0x02, load & 0xFF]) + struct.pack('<I', base + i * a.chunk) + piece
        if last:
            body += struct.pack('<I', xfer)
        h.send(body)
        if a.resend and i == 1 and not resent:      # the same load once more
            resent = True
            h.send(body)
            r = h.recv(5)
            if r is None or r[2][0] != 0x0A:
                fail("no answer to load %d" % load)
            r2 = h.recv(5)
            if r2 is None or r2[2][0] != 0x0A or r2[2][1] != r[2][1]:
                fail("the repeated load was not answered with the same request")
            print("PASS: repeated load %d answered with request %d" % (load, r2[2][1]))
            if r[2][1] != (load + 1) & 0xFF:
                fail("request %d after load %d" % (r[2][1], load))
            load += 1
            continue
        r = h.recv(5)
        if r is None or r[2][0] != 0x0A:
            fail("no Request Memory Load after load %d" % load)
        if r[2][1] != (load + 1) & 0xFF or r[2][2] != 0:
            fail("Request Memory Load %s after load %d" % (r[2].hex(), load))
        load += 1
    print("PASS: tertiary loader sent in %d loads (%d bytes at %06o, start %06o)"
          % (load, len(code), base, xfer))
    sys.stdout.flush()
    if a.system:
        r = h.recv(20)
        if r is None:
            fail("no Request Program for the system from the tertiary loader")
        src, dst, mop = r
        print("Request Program for the system: %s" % mop.hex())
        if mop[0] != 0x08 or mop[3] != 2 or mop[4] != 0xFF or dst != HOSTMAC:
            fail("expected Request Program type 2, software ID -1, to this host")
        print("PASS: Request Program for the operating system")
        base, code, xfer = test_tertiary(a.system)
        # like a system image, it also loads the first vector (0: 0, 2: PS 340);
        # TERUNA checks location 2 for 340 before it starts the system
        loads = [(0, struct.pack('<2H', 0, 0o340))]
        loads += [(base + i, code[i:i + a.chunk]) for i in range(0, len(code), a.chunk)]
        pieces = loads
        for i, (addr, piece) in enumerate(loads):
            h.send(bytes([0x02, i & 0xFF]) + struct.pack('<I', addr) + piece)
            r = h.recv(10)
            if r is None or r[2][0] != 0x0A or r[2][1] != (i + 1) & 0xFF:
                fail("no Request Memory Load %d from the tertiary loader: %s" % (i + 1, r and r[2].hex()))
        h.send(bytes([0x14, len(pieces) & 0xFF, 0x00]) + struct.pack('<I', xfer))
        print("PASS: system sent in %d loads and a parameter load with transfer address %06o"
              % (len(pieces), xfer))
        sys.stdout.flush()
    end = time.time() + a.watch
    while time.time() < end:
        r = h.recv(max(0.1, end - time.time()))
        if r is not None:
            print("after the boot: %s -> %s: %s" % (r[0].hex('-'), r[1].hex('-'), r[2].hex()))
            sys.stdout.flush()
    time.sleep(1.0)


if __name__ == '__main__':
    main()
