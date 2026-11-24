open-simh-extended/tests/wait_interrupt/README.txt  v1 (2026-11-24)

Patch 26, known issue 117: in the realism mode the waits at the sim> prompt
(SET <unit> ONLINE/OFFLINE/LOAD/UNLOAD/REWIND wait until the drive is
ready or at rest) end early on the WRU character (Ctrl-E) or Ctrl-C.

w.py drives the PDP-11 simulator through a pseudo terminal, so the keys
arrive as from a real terminal; no audio output or image is needed:

    cd tests/wait_interrupt
    python3 -I w.py ../../BIN/pdp11

Test  What                                                  Expected
W1    Ctrl-E during SET RL0 ONLINE (spin-up 40 s)            "wait interrupted, still spinning
                                                             up"; SHOW RL0 SPINDLE: spinning up
W2    Ctrl-C during SET RL0 OFFLINE (spin-down 30 s)         "wait interrupted, still spinning
                                                             down"
W3    DO w3.do (SET RL0 ONLINE, ECHO W3-NEXT), Ctrl-E        the wait and the script end; W3-NEXT
                                                             not printed; the prompt works
W4    no key, SPINUP 2000 ms                                 "up to speed, ready" as before
W5    Ctrl-E during SET TS LOAD (30 s)                       "wait interrupted, still loading";
                                                             SHOW TS TRANSPORT: loading

Results (2026-11-24): patch 26 W1-W5 PASS; before (integration 7caf9efd)
W1-W3 FAIL (the wait ran to its end), W4 PASS.  tests/rl02_8600/c1.ini
(vax8600, stdin /dev/null) unchanged.
