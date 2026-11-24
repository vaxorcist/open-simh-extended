# open-simh-extended/tests/wait_interrupt/w.py  v2 (v2: W5 TS LOAD)
# Tests of patch 26 (prompt waits interruptible) through a pseudo terminal:
#   W1 Ctrl-E during SET RL0 ONLINE (spin-up 40 s): the wait ends at once,
#      message, prompt; SHOW RL0 SPINDLE: still spinning up
#   W2 Ctrl-C during SET RL0 OFFLINE (spin-down 30 s): the same
#   W3 a DO script with SET RL0 ONLINE: Ctrl-E ends the wait and the script
#      (its next line, ECHO W3-NEXT, is not executed)
#   W4 without a key the wait ends by itself (SPINUP 2000 ms)
#   W5 Ctrl-E during SET TS LOAD (30 s): the reel wait ends at once
# usage (from this directory): python3 -I w.py ../../BIN/pdp11
# Creates w.rl01, t.tap and w3.do here (gitignored).
import os, pty, sys, time, select
sim = sys.argv[1]
open('w3.do', 'w').write('set rl0 online\necho W3-NEXT\n')

def run(cmds):
    pid, fd = pty.fork()
    if pid == 0:
        os.environ['SDL_AUDIODRIVER'] = 'dummy'
        os.execv(sim, [sim])
    out = b''
    def readfor(t, until=None):
        nonlocal out
        end = time.time() + t
        while time.time() < end:
            r, _, _ = select.select([fd], [], [], 0.1)
            if r:
                try:
                    d = os.read(fd, 4096)
                except OSError:
                    return
                out += d
                if until and until in out[-400:]:
                    return
    readfor(3, b'sim> ')
    for c, wait, until in cmds:
        os.write(fd, c)
        readfor(wait, until)
    os.write(fd, b'exit\r'); readfor(3)
    try: os.kill(pid, 9)
    except OSError: pass
    return out.decode('latin-1').replace('\r', '')

base = [(b'set realism enabled\r', 3, b'sim> '),
        (b'attach -n rl0 w.rl01\r', 3, b'sim> ')]
res = {}
t0 = time.time()
o = run(base + [(b'set rl0 online\r', 2, b'spinning up'), (b'\x05', 3, b'sim> '),
                (b'show rl0 spindle\r', 3, b'sim> ')])
res['W1'] = ('wait interrupted, still spinning up' in o and 'spinning up,' in o.split('show rl0 spindle')[-1]
             and time.time() - t0 < 20, o)
open('w.rl01', 'a').close()
t0 = time.time()
o = run(base + [(b'dep rl spinup 1000\r', 2, b'sim> '), (b'set rl0 online\r', 6, b'ready\nsim> '),
                (b'set rl0 offline\r', 2, b'spinning down'), (b'\x03', 3, b'sim> '),
                (b'show rl0 spindle\r', 3, b'sim> ')])
res['W2'] = ('wait interrupted, still spinning down' in o and time.time() - t0 < 20, o)
t0 = time.time()
o = run(base + [(b'do w3.do\r', 2, b'spinning up'), (b'\x05', 3, b'sim> '), (b'echo W3-PROMPT\r', 2, b'sim> ')])
res['W3'] = ('wait interrupted' in o and 'W3-NEXT' not in o.replace('echo W3-NEXT', '') and 'W3-PROMPT' in o
             and time.time() - t0 < 20, o)
o = run(base + [(b'dep rl spinup 2000\r', 2, b'sim> '), (b'set rl0 online\r', 10, b'ready\nsim> ')])
res['W4'] = ('up to speed, ready' in o and 'interrupted' not in o, o)
o = run([(b'set realism enabled\r', 3, b'sim> '), (b'set ts enabled\r', 2, b'sim> '),
         (b'attach -n ts t.tap\r', 3, b'sim> '), (b'set ts load\r', 2, b'in 30 s'),
         (b'\x05', 3, b'sim> '), (b'show ts transport\r', 3, b'sim> ')])
res['W5'] = ('wait interrupted, still loading' in o and 'transport: loading,' in o.split('show ts transport')[-1], o)
for k, (ok, o) in res.items():
    print(k, 'PASS' if ok else 'FAIL')
    if not ok:
        print(o[-1500:])
