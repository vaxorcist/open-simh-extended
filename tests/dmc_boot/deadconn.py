# open-simh-extended/tests/dmc_boot/deadconn.py  v1
# Known issue 116 (patch 25, sim_tmxr.c): connection attempts that a peer
# gave up while they waited in a line's listen queue must not delay its
# live connection.  The PDP-11 of d1.ini runs BOOT DMC0 (its DMC11 listens
# on PORT and has a PEER where nobody listens); this script makes NDEAD
# connections to PORT and closes them at once, then a live one, and waits
# for the bootstrap's MOP Request Program on it (DDCMP maintenance message,
# DLE).  With the fix the simulator discards the closed ones in the same
# poll and takes the live one at its next connection poll (every 2 s of
# simulated time); before, it took one connection per poll, so the live one
# came NDEAD polls later.
# usage: python3 -I deadconn.py PORT [NDEAD]     PASS: Request Program within 3.5 s
import socket, sys, time
port = int(sys.argv[1]); ndead = int(sys.argv[2]) if len(sys.argv) > 2 else 3
for i in range(200):                                    # wait until the simulator listens
    try:
        s = socket.create_connection(('127.0.0.1', port), timeout=1)
        s.close()                                       # (this one is given up too)
        break
    except OSError:
        time.sleep(0.1)
else:
    sys.exit("FAIL: nobody listens on %d" % port)
for i in range(ndead):
    socket.create_connection(('127.0.0.1', port), timeout=1).close()
t0 = time.time()
live = socket.create_connection(('127.0.0.1', port), timeout=1)
live.settimeout(0.2)
buf = b''
while time.time() - t0 < 20:
    try:
        d = live.recv(100)
        if not d:
            sys.exit("FAIL: live connection closed by the simulator after %.1f s" % (time.time() - t0))
        buf += d
    except socket.timeout:
        pass
    i = buf.find(b'\x90')                               # DLE: maintenance message
    if i >= 0 and len(buf) >= i + 12:
        dt = time.time() - t0
        print("Request Program after %.1f s: %s" % (dt, buf[i + 8:i + 12].hex()))
        print("PASS" if dt < 3.5 else "FAIL: too late")
        sys.exit(0)
sys.exit("FAIL: no Request Program within 20 s")
