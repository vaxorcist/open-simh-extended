open-simh-extended/tests/dmv11_boot/README.txt  v1 (2026-11-20)

DMV11 switch packs and primary MOP boot (patch 18, PDP-11 simulator).

run_tests.sh BIN/pdp11 [T1 ... T6]   runs all tests (about 1 minute); prints
                                     "Tn PASS" or "Tn FAIL" per test.

mopctl.py   a minimal DDCMP/MOP control station in Python: connects to the
            DMV11's ATTACH port, polls (STRT) or sends Enter MOP Mode, checks
            Request Program (08 22 01 00: device type 34, secondary loader)
            and every Request Memory Load, and loads a small test program
            (vectors 0-777 with a pattern, the program at 1000, its text at
            1100, transfer address 1000).  The program prints "DMV BOOT OK"
            and halts at 1030.
mkini.py    writes t3.ini, t4.ini and t6.ini (their PDP-11 programs are
            assembled there, with computed branch offsets).
common_check.ini  memory after the halt: the program's own words at 24-40
            and 400-402 (the DMV11 puts back what its start trap changed).

T1  power-on boot, point-to-point (MODE=FDXPOINT, POWERONBOOT, BOOT DMV0);
    the first Request Program is not answered: repeated after 5 s.
T2  power-on boot, multipoint tributary 5 (MODE=FDXTRIBUTARY, TRIBUTARY=5):
    a poll for address 6 is ignored, a duplicate load is answered again,
    an out-of-sequence load is ignored.
T3  remote load detect, point-to-point, password 123: the "operating system"
    runs with memory management on (kernel page 0 at physical 100000), has
    started DDCMP (DTR on) and loops; a wrong password is ignored.
T4  invoke primary MOP, multipoint tributary, tributary switches 0: the
    program establishes tributary 9 and sets BSEL1 bits 5 and 6; the boot
    uses address 9.
T5  BOOT DMV0 refusals: power-on boot switch off, mode set by software,
    control station, multipoint tributary without a switch address.
T6  switch-assigned mode and address with the host interface: ESTABLISH 9
    with TRIBUTARY=5 establishes 5; a mode definition for another mode gets
    error 104; with the diagnostic mode bit it is accepted.

Ports 21101-21106 are used; after a run, wait about a minute before the
next (a port still in TIME_WAIT makes ATTACH fail: "bind error 98").
The scripts connect to the simulator's ATTACH port: in Claude's workspace
the simulator's own outgoing connection (PEER) to a listening Python
socket was reported as failed (known issues 91).
Results 2026-11-20: T1-T6 PASS (topic branch dmv-boot and integration).
Also: tests/dmv11/mc.ini (DMV11 master clear signature) on microvax1,
microvax2 and microvax3900: PASS.
