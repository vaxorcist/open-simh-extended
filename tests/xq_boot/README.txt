open-simh-extended/tests/xq_boot/README.txt  v1 (2026-11-24)

Tests of patch 27 (known issues 118, 119, 120): booting a PDP-11/83 from a
DEQNA, DELQA or DELQA-T (device XQ/XQB) with SIMH's BOOT XQ.

How BOOT XQ works: SIMH puts a small host bootstrap at 020000; it asks the
board for its boot ROM (CSR bits BD and EL), the board copies the ROM into
host memory at 0 (SIMH has the ROM contents of the real boards in
PDP11/pdp11_xq_bootrom.h), and the bootstrap starts it.  The ROM code runs
its self-test (setup packets, address filter, internal loopback, a
non-existent memory test) and then sends MOP Request Program frames to the
load assistant multicast AB-00-00-01-00-00 until a load host answers.

No load host is used here: a passing boot ROM keeps sending its requests,
which watch.py counts; a failing one halts.  The simulator's Ethernet goes
over SIMH's UDP transport (ATTACH XQ udp:PORT:127.0.0.1:WATCHPORT).

    cd tests/xq_boot
    ./run_tests.sh ../../BIN/pdp11

Test  What                                                 Expected
Q1    BOOT XQ, TYPE=DEQNA                                  no HALT in 20 s,
                                                           >= 3 MOP requests
Q2    BOOT XQ, TYPE=DELQA                                  the same
Q3    BOOT XQ, TYPE=DELQA-T                                the same
Q4    BOOT XQB (second board, CSR 174460), DELQA           the same
Q5    BOOT XQB, DEQNA, realism mode                        the same
Q6    BOOT XQ (DELQA, nobody on the other UDP port, so     no "double free",
      writes fail), stopped by a breakpoint at the ROM's   5 of 5 runs
      request loop, DETACH XQ, EXIT

Results (2026-11-24): patch 27: Q1-Q6 PASS.  Before (integration 6dc70135):
Q1 and Q5 FAIL (DEQNA: HALT at 002332, R0 011102: the setup packet came back
about 17,000 instructions after its transmission, the ROM polls for about
2,000), Q4 FAIL (HALT at 020212: the host bootstrap used CSR 177460), Q6
FAIL (SIMH aborted with "double free or corruption" in every run).

Files: run_tests.sh, watch.py (prints every frame the simulator sends),
README.txt.  The generated Q*.ini, Q*.out and Q*.frames are not kept.

The device types seen in the MOP requests: DEQNA 5, DELQA 37 (hex 25),
DELQA-T 75 (hex 4B); program type 1, data link buffer size 1514.  A real load needs a load host that answers them (VMS
NCP with SERVICE DEVICE QNA, or an RSX host).
