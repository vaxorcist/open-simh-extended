open-simh-extended/tests/dmv11_boot/README.txt  v7 (2026-11-23; v7: R8-R10, SIMH's BOOT DMCn bootstrap (patch 24); mopctl.py v11 --listen, --progtype, --mingap; .gitignore; v6: mopctl.py --rom also without --trigger (tests/m9312_roms); v5: R6, R7 boot through DEC's M9312 'XM' DECnet ROM; v4: DMC11 tests R4, R5, R3 for the DMC11's switch packs; v3: DMP11 tests P1-P4 and DMR11 tests R1-R3 (patch 21); v2: the start trap words are at 774-776)

DMV11, DMP11, DMR11 and DMC11 switch packs and boot (patches 18 and 21, PDP-11
simulator).

run_tests.sh BIN/pdp11 [T1 ... R10]  runs all tests (about 3 minutes); prints
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
            message answers (a DMR11 that starts a boot ROM); --shortreq:
            the DMC11's Request Program 8,12,1 (no program type); --rom:
            the load host of DEC's M9312 'XM' ROM (one Memory Load with
            Transfer Address of a position-independent program, stored
            by the ROM at 0 and started at 6; reconnects after the ROM's
            RESET); with --trigger after the remote load detect, without
            it for a ROM started by a console or the switch register,
            which asks by itself (tests/m9312_roms, mopctl.py v10);
            --listen: no polling, the PDP-11's bootstrap (BOOT DMCn) asks
            by itself; --progtype P: the program type the Request Program
            must carry; --mingap S: with --ignorefirst, the shortest gap
            accepted before the repeated request (mopctl.py v11).
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
R3  DMC11 switch packs (TYPE=DMC): defaults 377/377, word offset 010 =
    173020; DMR11 value checks.
R4  DMC11 remote load detect without a boot ROM: the DMC11 loads the program
    itself (Request Program 8,12,1, device type 12); wrong password first.
R5  DMC11 remote load detect with a boot ROM (r2rom.bin at 173000,
    BOOTOFFSET 10 words): 24 = 173020, 26 = 0, restart at 173020.
R6  DMR11 remote load detect into DEC's M9312 'XM' DECnet boot ROM
    (BOOTOFFSET 4 = entry 173004, unit 0 without diagnostics): the ROM
    resets the bus, finds the DMR11 at its floating CSR (160070), sends
    Request Program 8,12,1,0 in maintenance mode, takes one Memory Load
    with Transfer Address and starts it at 6: "XM ROM BOOT OK", HALT at 34.
R7  the same with a DMC11 (BOOTOFFSET 2 words = 173004).
R8  BOOT DMC0 on a DMC11 (patch 24): SIMH's MOP bootstrap at 140000 asks
    with Request Program 8,12,1,0; the first is not answered (repeated
    after about 5 s of simulated time); three loads; at the start (a
    breakpoint at 1000) R1 = CSR 160070 and SP = 1000; "DMC BOOT OK".
R9  BOOT DMC0 on a DMR11 with PROGRAM=TERTIARY and 64 KB: Request Program
    8,40,1,1; "DMR BOOT OK".
R10 BOOT DMCn refusals (not attached, no such unit), HELP DMC BOOTING
    (BOOT command) and HELP BOOT.

R6 and R7 need the ROM image m9312_xm.bin in this directory (DEC firmware,
not in the repository; run_tests.sh says SKIP without it).  Build it from
Don North's (AK6DN) M9312 listings:

    git clone --depth 1 --filter=blob:none --sparse https://github.com/ak6dn/ak6dn.github.io ak6dn
    cd ak6dn
    git sparse-checkout set PDP-11/M9312
    cd PDP-11/M9312
    python3 -I <notes>/tools/m9312_lst2bin.py <this directory>/m9312_xm.bin \
        23-862A9/23-862A9.lst@173000 23-863A9/23-863A9.lst@173200 \
        23-864A9/23-864A9.lst@173400 \
        --hex 23-862A9/23-862A9.lst=23-862A9/23-862A9-0625.hex \
        --hex 23-863A9/23-863A9.lst=23-863A9/23-863A9-063F.hex \
        --hex 23-864A9/23-864A9.lst=23-864A9/23-864A9-0551.hex

(384 bytes, 173000-173577; the --hex checks compare every word with the
PROM programmer files.)

The test "operating systems" (P2, P3, R1, R2, R4-R7) set SP 1000: the takeover
pushes PC and PSW, and an 11/70 with SP 0 stops with a red stack trap.

Ports 21101-21106, 21111-21114 and 21121-21126 are used; after a run, wait about a minute before the
next (a port still in TIME_WAIT makes ATTACH fail: "bind error 98").
The scripts connect to the simulator's ATTACH port: in Claude's workspace
the simulator's own outgoing connection (PEER) to a listening Python
socket was reported as failed (known issues 91).
Results 2026-11-20: T1-T6 PASS (topic branch dmv-boot and integration).
Results 2026-11-22: T1-T6, P1-P4, R1-R3 PASS (topic branch dmp-dmr-boot).
Results 2026-11-22 (patch 21 v2, DMC11): T1-T6, P1-P4, R1-R5 PASS.
Results 2026-11-22 (patch 21 v3, M9312 ROM): T1-T6, P1-P4, R1-R7 PASS.
Also: tests/dmv11/mc.ini (DMV11 master clear signature) on microvax1,
microvax2 and microvax3900: PASS.
