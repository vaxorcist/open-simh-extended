open-simh-extended/tests/dmv11_boot/README.txt  v3 (2026-11-22; v3: DMP11 tests P1-P4 and DMR11 tests R1-R3 (patch 21); v2: the start trap words are at 774-776)

DMV11, DMP11 and DMR11 switch packs and boot (patches 18 and 21, PDP-11
simulator).

run_tests.sh BIN/pdp11 [T1 ... R3]   runs all tests (about 2 minutes); prints
                                     "PASS" or "FAIL" per test.

mopctl.py   a minimal DDCMP/MOP control station in Python: connects to the
            DMV11's ATTACH port, polls (STRT) or sends Enter MOP Mode, checks
            Request Program (08 22 01 00: device type 34, secondary loader)
            and every Request Memory Load, and loads a small test program
            (vectors 0-777 with a pattern, the program at 1000, its text at
            1100, transfer address 1000).  The program prints "DMV BOOT OK"
            (--text: another text) and halts at 1030.  --devtype 18 or 40
            for a DMP11 or DMR11, --dmrpw: the DMR11's Enter MOP Mode (the
            password in all four bytes), --noload: only check that no MOP
            message answers (a DMR11 that starts a boot ROM).
mkini.py    writes t3.ini, t4.ini and t6.ini (their PDP-11 programs are
            assembled there, with computed branch offsets).
common_check.ini  memory after the halt: the program's own words at 24-40
            and 774-776 (the board puts back what its start trap changed);
            argument: the unit (do common_check.ini dmv0).

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

P1  DMP11 power-on boot, point-to-point (PDP-11/70, BOOT DMP0), device
    type 18; the first Request Program is not answered.
P2  DMP11 remote load detect, multipoint tributary 5, while the CPU runs a
    loop; a wrong password is ignored.
P3  DMP11 invoke primary MOP, point-to-point (MOV #60000 to SEL0).
P4  BOOT DMP0 refusals (power-on boot switch E121 SW10 off by default,
    mode set by software, control station) and BOOTUNIT 16.
R1  DMR11 remote load detect without a boot ROM (DMC0 TYPE=DMR, PASSWORD
    123): the DMR11 loads the program itself (device type 40); a wrong
    password is ignored.
R2  DMR11 remote load detect with a boot ROM (r2rom.bin at 173000,
    BOOTOFFSET 20): 24 = 173020, 26 = 0, the CPU restarts at 173020
    (MOV #123456,R0; HALT), no MOP message.
R3  DMC11 refusal (TYPE=DMC), DMR11 value checks.

The test "operating systems" (P2, P3, R1, R2) set SP 1000: the takeover
pushes PC and PSW, and an 11/70 with SP 0 stops with a red stack trap.

Ports 21101-21106, 21111-21114 and 21121-21122 are used; after a run, wait about a minute before the
next (a port still in TIME_WAIT makes ATTACH fail: "bind error 98").
The scripts connect to the simulator's ATTACH port: in Claude's workspace
the simulator's own outgoing connection (PEER) to a listening Python
socket was reported as failed (known issues 91).
Results 2026-11-20: T1-T6 PASS (topic branch dmv-boot and integration).
Results 2026-11-22: T1-T6, P1-P4, R1-R3 PASS (topic branch dmp-dmr-boot).
Also: tests/dmv11/mc.ini (DMV11 master clear signature) on microvax1,
microvax2 and microvax3900: PASS.
